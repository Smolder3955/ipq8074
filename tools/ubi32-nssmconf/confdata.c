/*
 **************************************************************************
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LKC_DIRECT_LINK
#include "lkc.h"

#define LOCAL_BUILD_SETTINGS "/defconfig"

#define PRIVATE_HDR_PATH   "./build/include/"
#define CONFIG_ORDER_PATH  "./config/config.order"
#define CONFIG_H_PATH 	   "./config/config.h"
#define CONFIG_MK_PATH 	   "./config/config.mk"
#define CONFIG_INC_PATH    "./config/config.inc"

#define ENABLE_HOME_DEFCONFIG_CHECK 0

/*
 *  Structure that holds the details of the chosen packages
 */
struct dep_list{
	struct dep_list *next;
	struct symbol *sym;
	char pkg_name[MAX_NAME_LENGTH];
};

/*
 * Head pointer to the ordered and disordered list, respectively.
 */
struct dep_list *head_ordered_list, *head_disordered_list, *tail_disordered_list;

static void conf_warning(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

static const char *conf_filename;
static int conf_lineno, conf_warnings, conf_unsaved;

const char conf_def_filename[] = ".config";

const char conf_defname[] = "scripts/config/defconfig";

bool flag_global_debug = false;

const char *conf_confnames[] = {
	".config",
	conf_defname,
	NULL,
};

static void conf_warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:warning: ", conf_filename, conf_lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	conf_warnings++;
}

static char *conf_expand_value(const char *in)
{
	struct symbol *sym;
	const char *src;
	static char res_value[SYMBOL_MAXLENGTH];
	char *dst, name[SYMBOL_MAXLENGTH];

	res_value[0] = 0;
	dst = name;
	while ((src = strchr(in, '$'))) {
		strncat(res_value, in, src - in);
		src++;
		dst = name;
		while (isalnum(*src) || *src == '_')
			*dst++ = *src++;
		*dst = 0;
		sym = sym_lookup(name, 0);
		sym_calc_value(sym);
		strcat(res_value, sym_get_string_value(sym));
		in = src;
	}
	strcat(res_value, in);

	return res_value;
}

char *conf_get_default_confname(void)
{
	struct stat buf;
	static char fullname[PATH_MAX+1];
	char *env, *name;

	name = conf_expand_value(conf_defname);
	env = getenv(SRCTREE);
	if (env) {
		sprintf(fullname, "%s/%s", env, name);
		if (!stat(fullname, &buf))
			return fullname;
	}
	return name;
}

void conf_reset(void)
{
	struct symbol *sym;
	int i;

	for_all_symbols(i, sym) {
		sym->flags |= SYMBOL_NEW | SYMBOL_CHANGED;
		if (sym_is_choice(sym))
			sym->flags &= ~SYMBOL_NEW;
		sym->flags &= ~SYMBOL_VALID;
		switch (sym->type) {
		case S_INT:
		case S_HEX:
		case S_STRING:
		case S_UQSTRING:
			if (sym->user.val)
				free(sym->user.val);
		default:
			sym->user.val = NULL;
			sym->user.tri = no;
		}
	}
	conf_read_simple(NULL, 0);
}

int conf_read_file(FILE *in, struct symbol *sym){
	char line[1024];
	char *p, *p2;

	while (fgets(line, sizeof(line), in)) {
		conf_lineno++;
		sym = NULL;
		switch (line[0]) {
		case '#':
			if (memcmp(line + 2, "CONFIG_", 7))
				continue;
			p = strchr(line + 9, ' ');
			if (!p)
				continue;
			*p++ = 0;
			if (strncmp(p, "is not set", 10))
				continue;
			sym = sym_find(line + 9);
			if (!sym) {
				//conf_warning("trying to assign nonexistent symbol %s", line + 9);
				break;
			} /*else if (!(sym->flags & SYMBOL_NEW)) {
				//conf_warning("trying to reassign symbol %s", sym->name);
				break;
			}*/
			switch (sym->type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				sym->user.tri = no;
				sym->flags &= ~SYMBOL_NEW;
				break;
			default:
				;
			}
			break;
		case 'C':
			if (memcmp(line, "CONFIG_", 7)) {
				conf_warning("unexpected data");
				continue;
			}
			p = strchr(line + 7, '=');
			if (!p)
				continue;
			*p++ = 0;
			p2 = strchr(p, '\n');
			if (p2)
				*p2 = 0;
			sym = sym_find(line + 7);
			if (!sym) {
				//conf_warning("trying to assign nonexistent symbol %s", line + 7);
				break;
			} /*else if (!(sym->flags & SYMBOL_NEW)) {
				conf_warning("trying to reassign symbol %s", sym->name);
				break;
			}*/
			switch (sym->type) {
			case S_TRISTATE:
				if (p[0] == 'm') {
					sym->user.tri = mod;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
			case S_BOOLEAN:
				if (p[0] == 'y') {
					sym->user.tri = yes;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
				if (p[0] == 'n') {
					sym->user.tri = no;
					sym->flags &= ~SYMBOL_NEW;
					break;
				}
				conf_warning("symbol value '%s' invalid for %s", p, sym->name);
				break;
			case S_STRING:
			case S_UQSTRING:
				if (*p++ != '"')
					break;
				for (p2 = p; (p2 = strpbrk(p2, "\"\\")); p2++) {
					if (*p2 == '"') {
						*p2 = 0;
						break;
					}
					memmove(p2, p2 + 1, strlen(p2));
				}
				if (!p2) {
					conf_warning("invalid string found");
					continue;
				}
			case S_INT:
			case S_HEX:
				if (sym_string_valid(sym, p)) {
					sym->user.val = strdup(p);
					sym->flags &= ~SYMBOL_NEW;
				} else {
					conf_warning("symbol value '%s' invalid for %s", p, sym->name);
					continue;
				}
				break;
			default:
				;
			}
			break;
		case '\n':
			break;
		default:
			conf_warning("unexpected data");
			continue;
		}
		if (sym && sym_is_choice_value(sym)) {
			struct symbol *cs = prop_get_symbol(sym_get_choice_prop(sym));
			switch (sym->user.tri) {
			case no:
				break;
			case mod:
				if (cs->user.tri == yes) {
					conf_warning("%s creates inconsistent choice state", sym->name);
					cs->flags |= SYMBOL_NEW;
				}
				break;
			case yes:
				cs->user.val = sym;
				break;
			}
			cs->user.tri = E_OR(cs->user.tri, sym->user.tri);
		}
	}
	fclose(in);

	return 0;
}

int conf_read_simple(const char *name, int load_config)
{
	FILE *in = NULL;
	FILE *defaults = NULL;
	struct symbol *sym;
	int i;
#if (ENABLE_HOME_DEFCONFIG_CHECK)
	char *home_dir = getenv("HOME");
	char *default_config_path = NULL;

	if(home_dir){
			default_config_path = malloc(strlen(home_dir) + sizeof(LOCAL_BUILD_SETTINGS) + 1);
			if (!default_config_path) {
				printf("Memory allocation failed!\n");
				exit(1);
			}
			sprintf(default_config_path, "%s%s", home_dir, LOCAL_BUILD_SETTINGS);
			defaults = zconf_fopen(default_config_path);
			if(defaults)
					printf("# using buildsystem predefines from %s\n", default_config_path);
			free(default_config_path);
	}
#endif

	if(load_config){
		if (name) {
			in = zconf_fopen(name);
		} else {
			const char **names = conf_confnames;
			while ((name = *names++)) {
				name = conf_expand_value(name);
				in = zconf_fopen(name);
				if (in) {
					printf(_("#\n"
					         "# using defaults found in %s\n"
					         "#\n"), name);
					break;
				}
			}
		}
	}

	if (!in && !defaults)
		return 1;

	conf_filename = name;
	conf_lineno = 0;
	conf_warnings = 0;
	conf_unsaved = 0;

	for_all_symbols(i, sym) {
		sym->flags |= SYMBOL_NEW | SYMBOL_CHANGED;
		if (sym_is_choice(sym))
			sym->flags &= ~SYMBOL_NEW;
		sym->flags &= ~SYMBOL_VALID;
		switch (sym->type) {
		case S_INT:
		case S_HEX:
		case S_STRING:
		case S_UQSTRING:
			if (sym->user.val)
				free(sym->user.val);
		default:
			sym->user.val = NULL;
			sym->user.tri = no;
		}
	}

	if(defaults)
		conf_read_file(defaults, sym);

	if(in)
		conf_read_file(in, sym);

	if (modules_sym)
		sym_calc_value(modules_sym);

	return 0;
}

int conf_read(const char *name)
{
	struct symbol *sym;
	struct property *prop;
	struct expr *e;
	int i;

	if (conf_read_simple(name, 1))
		return 1;

	for_all_symbols(i, sym) {
		sym_calc_value(sym);
		if (sym_is_choice(sym) || (sym->flags & SYMBOL_AUTO))
			goto sym_ok;
		if (sym_has_value(sym) && (sym->flags & SYMBOL_WRITE)) {
			/* check that calculated value agrees with saved value */
			switch (sym->type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				if (sym->user.tri != sym_get_tristate_value(sym))
					break;
				if (!sym_is_choice(sym))
					goto sym_ok;
			default:
				if (!strcmp(sym->curr.val, sym->user.val))
					goto sym_ok;
				break;
			}
		} else if (!sym_has_value(sym) && !(sym->flags & SYMBOL_WRITE))
			/* no previous value and not saved */
			goto sym_ok;
		conf_unsaved++;
		/* maybe print value in verbose mode... */
	sym_ok:
		if (sym_has_value(sym) && !sym_is_choice_value(sym)) {
			if (sym->visible == no)
				sym->flags |= SYMBOL_NEW;
			switch (sym->type) {
			case S_STRING:
			case S_INT:
			case S_HEX:
			case S_UQSTRING:
				if (!sym_string_within_range(sym, sym->user.val)) {
					sym->flags |= SYMBOL_NEW;
					sym->flags &= ~SYMBOL_VALID;
				}
			default:
				break;
			}
		}
		if (!sym_is_choice(sym))
			continue;
		prop = sym_get_choice_prop(sym);
		for (e = prop->expr; e; e = e->left.expr)
			if (e->right.sym->visible != no)
				sym->flags |= e->right.sym->flags & SYMBOL_NEW;
	}

	sym_change_count = conf_warnings && conf_unsaved;

	return 0;
}

/*
 * find_num_dep()
 *	Returns the number of packages on which a pacakge depends upon.
 */
int find_num_dep(struct symbol *sym)
{
	int count = 0;
	struct property *prop;
	struct symbol *prop_sym;

	for_all_properties(sym, prop, P_SELECT) {
		if ((prop->expr) && (prop->expr->type == E_SYMBOL)) {
			prop_sym = sym_find(prop->expr->left.sym->name);
			if ((prop_sym) && (*sym_get_string_value(prop_sym) != 'y')) {
				continue;
			}
		}
		count++;
	}

	return count;
}

/* ordered_list_insert()
 *	Inserts a given dep_list node into the ordered list (if all its dependent nodes are present)
 *	Returns 1 when successful, else returns 0.
 */
bool ordered_list_insert(struct dep_list *curr)
{
	struct symbol *sym = curr->sym;
	const int num_dep = find_num_dep(sym);
	int satisfied_count = 0;
	struct property *prop;

	if (num_dep == 0) {
		curr->next = head_ordered_list;
		head_ordered_list = curr;
		return 1;
	}

	struct dep_list *o_ptr;
	for (o_ptr = head_ordered_list; o_ptr != NULL; o_ptr = o_ptr->next) {

		/*
		 * This loop goes through the dependency list of the current element in the disordered list.
		 */
		for_all_properties (sym, prop, P_SELECT) {
			struct gstr r = str_new();
			expr_gstr_print(prop->expr, &r);
			if (strcmp(r.s, o_ptr->sym->name) == 0) {
				satisfied_count++;
			}

			/*
			 * When all its dependencies are present in the ordered list, this equality will be achieved.
			 */
			if (satisfied_count == num_dep) {
				curr->next = o_ptr->next;
				o_ptr->next = curr;
				return 1;
			}
		}
	}
	return 0;
}

/*
 * order_list()
 *	Takes the list of symbols in the disordered list and created an ordered list according to the dependency.
 *
 * ORDER: Package with the least number of dependencies is at the top, and the package with the highest
 * 	  dependency goes at the bottom.
 */
void order_list(struct dep_list *prev, struct dep_list *curr)
{
	if (curr == NULL && prev == NULL) {
		return;
	} else if (curr == NULL) {
		order_list(NULL, head_disordered_list);
		return;
	}

	struct dep_list *next = curr->next;
	bool flag = ordered_list_insert(curr);

	if (flag == 1) {
		if (prev != NULL) {
			prev->next = next;
		} else {
			head_disordered_list = next;
			prev = NULL;
		}
		order_list(prev, next);
	} else {
		order_list(curr, next);
	}
	return;
}

/*
 * create_private_header()
 *	Creates the <pkgName>_config.h files inside 'build/include/'
 *
 * This function will also convert the public defines into private defines.
 * i.e. The <PKGNAME>_DEBUG_LVL will become DEBUG_LVL.
 */
void create_private_header(char *pkgDefine, const char *pkgName)
{
	char symbol_name[MAX_NAME_LENGTH];
	const char *debug_types[] = {"ERROR", "WARNING", "INFO", "TRACE"};
	const int len_debug_types = sizeof(debug_types)/sizeof(char *);
	struct symbol *sym_subType;

	char *path_to_file = malloc((strlen(PRIVATE_HDR_PATH) + strlen(pkgName) + strlen("_config.h") + 1));
	if (!path_to_file) {
		fprintf(stderr, _("Memory allocation failed!\n"));
		exit(1);
	}

	/*
	 * Open the private header file.
	 */
	sprintf(path_to_file, "%s%s%s", PRIVATE_HDR_PATH, pkgName, "_config.h");
	FILE *fp = fopen(path_to_file, "w");
	if (fp == NULL) {
		fprintf(stderr, _("Error: Unable to create private header file : %s\n"), path_to_file);
		exit(1);
	}

	/*
	 * Auto generated comment on top of each file.
	 */
	fprintf(fp, "/*\n * Auto generated by mconf. DO NOT EDIT this FILE\n */\n");

	/*
	 * Print the private defines, deriving values from its corresponding public defines.
	 */
	snprintf(symbol_name, MAX_NAME_LENGTH, "%s_DEBUG_PKG_LVL_MESSAGES", pkgDefine);
	struct symbol *sym_lvl	=  sym_find(symbol_name);
	if (sym_lvl != NULL) {
		const char *c_lvl = sym_get_string_value(sym_lvl);
		int debug_level;
		if (strlen(c_lvl) != 0 && flag_global_debug == true) {
			debug_level = *c_lvl - '0';
			fprintf(fp, "\n#define DEBUG_PKG");
		} else {
			debug_level = 0;
		}
		fprintf(fp, "\n#define DEBUG_PKG_LVL_MESSAGES\t%d\n#define %s_dbg_lvl\t%d\n", debug_level, pkgName, debug_level);
		int i;
		for (i=0; i < len_debug_types; i++) {
			if (i < (debug_level)) {
				fprintf(fp, "\n#define DEBUG_MESSAGE_L%d_CTRL\t%d", i+1, 1);
			} else {
				fprintf(fp, "\n#define DEBUG_MESSAGE_L%d_CTRL\t%d", i+1, 0);
			}
		}
	}

	fclose(fp);
}

/*
 *  process_symbol()
 *		Extracts the required details when a symbol is passed to it and inserts them into the disordered list.
 */
void process_symbol(struct menu *menu)
{
	bool hit, dir_flag;
	char *str_append = "_PKG_DIR", *sym_name;
	struct property *prop;
	struct symbol *sym = menu->sym, *pkgName_sym;
	struct menu *menuptr;
	struct dep_list *new_node = (struct dep_list *) malloc(sizeof(struct dep_list));
	if (!new_node) {
		fprintf(stderr, "Error: Memory allocation failed!\n");
		exit(1);
	}

	/*
	 * Return, if the symbol is not selected.
	 */
	if (*sym_get_string_value(sym) != 'y') {
		return;
	}

	/*
	 *  This block extracts the package directory name.
	 */
	sym_name = malloc((strlen(sym->name) + strlen(str_append) + 1));
	if (!sym_name) {
		fprintf(stderr, _("Memory allocation failed!\n"));
		exit(1);
	}
	sprintf(sym_name, "%s%s", sym->name, str_append);
	pkgName_sym = sym_find(sym_name);
	if (pkgName_sym) {
		create_private_header(sym->name, sym_get_string_value(pkgName_sym));
		strcpy(new_node->pkg_name, sym_get_string_value(pkgName_sym));
	} else {
		fprintf(stderr, _("Error: Package \"%s\" does not have %s_PKG_DIR defined\n"), sym->name, sym->name);
		exit(1);
	}
	free(sym_name);

	/*
	 * The code below inserts the new block at the end of the disordered list.
	 */
	new_node->sym = sym;
	if (head_disordered_list == NULL) {
		new_node->next = NULL;
		head_disordered_list = new_node;
		tail_disordered_list = new_node;
	} else {
		new_node->next = NULL;
		tail_disordered_list->next = new_node;
		tail_disordered_list = new_node;
	}
}


/*
 * create_dep_list()
 *	This is the function creates the config.order file.
 */
void create_dep_list(struct menu *rootmenu)
{
	struct menu *menu;
	struct symbol *sym;
	bool flag_found_pkg_menu = false;
	struct dep_list *p, *free_ptr;
	FILE *fp = fopen(CONFIG_ORDER_PATH, "w");

	if (fp == NULL) {
		fprintf(stderr, _("Error: Failed to create %s - check if path exists."), CONFIG_ORDER_PATH);
		exit(1);
	}

	menu = rootmenu->list;
	head_disordered_list = NULL;
	head_ordered_list = NULL;

	/*
	 * This loop goes through all the symbols inside Packages.
	 */
	while (menu) {
		sym = menu->sym;
		if (flag_found_pkg_menu == false && !sym) {
			if (strncmp(menu->prompt->text, "Package", 7) == 0 ) {
				flag_found_pkg_menu = true;
				if (menu->list) {
					menu = menu->list; /* Get into the package list. */
					continue;
				} else {
					fprintf(stderr, _("Error: No packages found inside menu Package menu\n"));
					exit(1);
				}
			} else {
				menu = menu->next; /* Keep searching for the 'Package' menu */
				continue;
			}
		}

		if (!sym) { /* Skip if not a symbol */
			menu = menu->next;
		} else if (!(sym->flags & SYMBOL_CHOICE)) {
			process_symbol(menu);
			if (menu->next) {
				menu = menu->next; /* Parse through the list of menus. */
			} else {
				break;
			}
		}
	}

	/*
	 *  Throws an error message when it fails to find a menu named 'Packages'.
	 */
	if (flag_found_pkg_menu == false) {
		fprintf(stderr, _("Error: Failed to create the 'config.order' file, please make sure that the packages are put into a root menu named 'Package(s)'.\n"));
		exit(1);
	}
	order_list(NULL, head_disordered_list); /* This call will produce the ordered list, which can later be accessed using 'head_ordered_list' pointer */

	/*
	 * Save the list into "config.order" file.
	 */
	p = head_ordered_list;
	while (p != NULL) {
		fprintf(fp, "%s\n", p->pkg_name);
		free_ptr = p;
		p = p->next;
		free(free_ptr); /* This is freeing up memory allocated to the ordered_list. */
	}
	head_ordered_list = NULL;
	if (sym_find("APPLICATION") != NULL) {
		create_private_header("APPLICATION", "Application");
		fprintf(fp, "Application");
	} else {
		fprintf(stderr, "Error: Application Missing!");
		exit(1);
	}
	fclose(fp);
}

/*
 * conf_write()
 *	con_write() function has been 'modified' to generate config.h config.inc and config.mk
 */
int conf_write(const char *name, bool genSF)
{
	FILE *out,*h,*inc,*mk;
	struct symbol *sym;
	struct menu *menu;
	const char *basename;
	char dirname[128], tmpname[128], newname[128];
	int type, l, index;
	const char *str, *uq_str;
	time_t now;
	int use_timestamp = 1;
	char *env;
	bool genSideFiles = genSF; /* genSideFiles takes care of optional printing */

	dirname[0] = 0;
	if (name && name[0]) {
		struct stat st;
		char *slash;

		if (!stat(name, &st) && S_ISDIR(st.st_mode)) {
			strcpy(dirname, name);
			strcat(dirname, "/");
			basename = conf_def_filename;
		} else if ((slash = strrchr(name, '/'))) {
			int size = slash - name + 1;
			memcpy(dirname, name, size);
			dirname[size] = 0;
			if (slash[1])
				basename = slash + 1;
			else
				basename = conf_def_filename;
		} else
			basename = name;
	} else
		basename = conf_def_filename;

	/*
	 * Figure out the directory in which the config outputs need to be placed.
	 */
	sprintf(newname, "%s.tmpconfig.%d", dirname, (int)getpid());
	out = fopen(newname, "w");
	if(out == NULL) {
		fprintf(stderr, _("Error: Failed to create %s - check if path exists.\n"), newname);
		exit(1);
	}
	if (genSF) {
		printf("*** Creating configuration files (.config config.order config.h config.mk and config.inc)\n");
		h = fopen(CONFIG_H_PATH, "w");
		if(h == NULL) {
			fprintf(stderr, _("Error: Failed to create %s - check if path exists.\n"), CONFIG_H_PATH);
			exit(1);
		}

		inc = fopen(CONFIG_INC_PATH, "w");
		if(inc == NULL) {
			fprintf(stderr, _("Error: Failed to create %s - check path if path exists.\n"), CONFIG_INC_PATH);
			exit(1);
		}

		mk = fopen(CONFIG_MK_PATH, "w");
		if(mk == NULL) {
			fprintf(stderr, _("Error: Failed to create %s - check if path exists.\n"), CONFIG_MK_PATH);
			exit(1);
		}
	}

	if (!out)
		return 1;
	sym = sym_lookup("NSSDISTROVERSION", 0);
	sym_calc_value(sym);
	time(&now);
	env = getenv("KCONFIG_NOTIMESTAMP");
	if (env && *env)
		use_timestamp = 0;

	fprintf(out, _("#\n"
		       "# Automatically generated make config: don't edit\n"
		       "# Ultra Distro version: %s\n"
		       "%s%s"
		       "#\n"),
		     sym_get_string_value(sym),
		     use_timestamp ? "# " : "",
		     use_timestamp ? ctime(&now) : "");

	if (genSF) {
		fprintf(mk, _("#\n"
			       "# Automatically generated configuration file: don't edit\n"
			       "%s%s"
			       "#\n"),
			     use_timestamp ? "# " : "",
			     use_timestamp ? ctime(&now) : "");
		fprintf(h, _("/*\n"
			     " * Automatically generated configuration file: don't edit\n"
			     " %s%s"
			     " */\n"),
			     use_timestamp ? "* " : "",
			     use_timestamp ? ctime(&now) : "");
		fprintf(inc, _("/*\n"
			       " * Automatically generated configuration file: don't edit\n"
			       " %s%s"
			       " */\n"),
			     use_timestamp ? "* " : "",
			     use_timestamp ? ctime(&now) : "");
		fprintf(inc, _("\n#ifndef _CONFIG_INC_\n"
			    "#define _CONFIG_INC_\n"));
	}

	if (!sym_change_count)
		sym_clear_all_valid();

	menu = rootmenu.list;
	while (menu) {
		sym = menu->sym;

		/*
		 * Excludes unnecessary defines in the header files (side files).
		 */
		if (sym && sym->name && (strncmp(sym->name, "FORCE_", 6) == 0 || strncmp(sym->name, "LIST_", 5) == 0)) {
			genSideFiles = false;
		} else {
			genSideFiles = genSF;
		}

		if (!sym) {
			if (!menu_is_visible(menu))
				goto next;
			str = menu_get_prompt(menu);
			fprintf(out, "\n"
				     "#\n"
				     "# %s\n"
				     "#\n", str);

			if (genSideFiles) {
				fprintf(mk, "\n"
					     "#\n"
					     "# %s\n"
					     "#\n", str);
				fprintf(h, "\n"
					   "/*\n"
					   " * %s\n"
					   " */\n", str);
				fprintf(inc, "\n"
					   "/*\n"
					   " * %s\n"
					   " */\n", str);
			}
		} else if (!(sym->flags & SYMBOL_CHOICE)) {
			sym_calc_value(sym);
			if (!(sym->flags & SYMBOL_WRITE))
				goto next;
			sym->flags &= ~SYMBOL_WRITE;
			type = sym->type;
			if (type == S_TRISTATE) {
				sym_calc_value(modules_sym);
/* tristate always enabled */
#if 0
				if (modules_sym->curr.tri == no)
					type = S_BOOLEAN;
#endif
			}
			switch (type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				switch (sym_get_tristate_value(sym)) {
				case no:
					fprintf(out, "# CONFIG_%s is not set\n", sym->name);
					break;
				case mod:
					fprintf(out, "CONFIG_%s=m\n", sym->name);
					if (genSideFiles && sym->prop) {
						fprintf(mk, "\n# %s \n%s = m\n", sym->prop->text, sym->name);
						fprintf(h, "\n/* %s */\n#define %s m\n", sym->prop->text, sym->name);
						fprintf(inc, "\n/* %s */\n#define %s m\n",  sym->prop->text, sym->name);
					}
					break;
				case yes:
					fprintf(out, "CONFIG_%s=y\n", sym->name);

					/* Check and save if global debug is chosen */
					if (strncmp(sym->name,"DEBUG\0",6) == 0) {
						flag_global_debug = true;
					}

					if (genSideFiles && sym->prop) {
						fprintf(mk, "\n# %s \n%s = 1\n", sym->prop->text, sym->name);
						fprintf(h, "\n/* %s */\n#define %s 1\n", sym->prop->text, sym->name);
						fprintf(inc, "\n/* %s */\n#define %s 1\n",  sym->prop->text, sym->name);
					}
					break;
				}

				/*
				 * Special case in creating config.mk
				 *	This handles the Multiple Instance of ipEthernet.
				 */
				if (genSideFiles && strcmp(sym->name, "IPETHERNET_MI_ENABLED") == 0) {
					if (sym_get_tristate_value(sym) != yes) {
						break;
					}
					char* eth_type[] = {"eth_wan", "eth_lan"};
					const int len_eth_type = sizeof(eth_type)/sizeof(char *);
					char symbol_name[MAX_NAME_LENGTH];
					for (index = 0; index < len_eth_type; index++) {
						snprintf(symbol_name, MAX_NAME_LENGTH, "%s_IPETHERNET_INSTANCE_ENABLED", eth_type[index]);
						struct symbol *sym_t =  sym_find(symbol_name);
        					if (sym_t == NULL) {
							continue;
						}
						if (sym_get_tristate_value(sym_t) == yes) {
			        			fprintf(mk, "\nIPETHERNET_MI_ENABLED_INSTANCES += %s_\n", eth_type[index]);
						}
					}
				}
				break;
			case S_STRING:
				// fix me
				str = sym_get_string_value(sym);
				fprintf(out, "CONFIG_%s=\"", sym->name);
				if (genSideFiles) {
					fprintf(mk, "\n# %s \n%s = ", sym->prop->text, sym->name);
					fprintf(h, "\n/* %s */\n#define %s \"", sym->prop->text, sym->name);
					fprintf(inc, "\n/* %s */\n#define %s \"",  sym->prop->text, sym->name);
				}
				do {
					l = strcspn(str, "\"\\");
					if (l) {
						fwrite(str, l, 1, out);
						if (genSideFiles) {
							fwrite(str, l, 1, mk);
							fwrite(str, l, 1, h);
							fwrite(str, l, 1, inc);
						}
					}
					str += l;
					while (*str == '\\' || *str == '"') {
						fprintf(out, "\\%c", *str);
						if (genSideFiles) {
							fprintf(mk, "\\%c", *str);
							fprintf(h, "\\%c", *str);
							fprintf(inc, "\\%c", *str);
						}
						str++;
					}
				} while (*str);
				fputs("\"\n", out);
				if (genSideFiles) {
					fputs("\n", mk);
					fputs("\"\n", h);
					fputs("\"\n", inc);
				}

				/*
				 * Special case in creating config.mk
				 */
				if (genSideFiles && strstr(sym->name, "_PKG_DIR") != NULL) {
					fprintf(mk, "\n# Package sub-directories\nPKG_SUBDIRS += $(%s)\n", sym->name);
				}
				break;
			case S_UQSTRING:
				str = sym_get_string_value(sym);
				uq_str = str; /* Requires a duplicate copy */
				fprintf(out, "\nCONFIG_%s=\"", sym->name);
				do {
					l = strcspn(str, "\"\\");
					if (l) {
						fwrite(str, l, 1, out);
					}
					str += l;
					while (*str == '\\' || *str == '"') {
						fprintf(out, "\\%c", *str);
						str++;
					}
				} while (*str);
				fputs("\"\n", out);

				if (genSideFiles) {
					fprintf(mk, "\n# %s\n%s = %s\n", sym->prop->text, sym->name, uq_str);
					fprintf(h, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, uq_str);
					fprintf(inc, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, uq_str);

					if ((strncmp(sym->name, "ARCH_EXT", 8) == 0 ||
					     strncmp(sym->name, "AP_BOARD_NAME", 13) == 0 ||
					     strncmp(sym->name, "AP_SOC_NAME", 11) == 0)
					    && strlen(uq_str) != 0) {
						fprintf(mk, "\n# %s\n%s = 1\n", sym->prop->text, uq_str);
						fprintf(h, "\n/* %s */\n#define %s 1\n", sym->prop->text, uq_str);
						fprintf(inc, "\n/* %s */\n#define %s 1\n", sym->prop->text, uq_str);
					}
				}
				break;
			case S_HEX:
				str = sym_get_string_value(sym);
				if (str[0] != '0' || (str[1] != 'x' && str[1] != 'X')) {
					fprintf(out, "CONFIG_%s=%s\n", sym->name, str);
					if (genSideFiles) {
						fprintf(mk, "\n# %s\n%s = %s\n", sym->prop->text, sym->name, str);
						fprintf(h, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, str);
						fprintf(inc, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, str);
					}
					break;
				}
			case S_INT:
				str = sym_get_string_value(sym);
				fprintf(out, "CONFIG_%s=%s\n", sym->name, str);
				if (genSideFiles) {
					fprintf(mk, "\n# %s\n%s = %s\n", sym->prop->text, sym->name, str);
					fprintf(h, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, str);
					fprintf(inc, "\n/* %s */\n#define %s %s\n", sym->prop->text, sym->name, str);
				}
				break;
			}
		}

	next:
		if (menu->list) {
			menu = menu->list;
			continue;
		}
		if (menu->next)
			menu = menu->next;
		else while ((menu = menu->parent)) {
			if (menu->next) {
				menu = menu->next;
				break;
			}
		}
	}
	fclose(out);

	if (genSF) {
		fprintf(inc,"\n#endif /* _CONFIG_INC_ */\n");

		/*
		 * This dummy printing at the end needs to be removed once Ultra chucks HRT tables
		 */
		fprintf(h,"/* HRT table definition */\n\n#define HRT_TABLE_DATA ARCH_THREAD_NULL | ARCH_THREAD_TABLE_END\n \
			  /* Thread allocation bitmaps */\n#define THREAD_HRT_RESERVED 0x000\n#define THREAD_NRT_RESERVED \
			  0xc00\n#define THREAD_UNASSIGNED   0x7ff\n");
		fclose(mk);
		fclose(h);
		fclose(inc);
		create_dep_list(&rootmenu);
	}


	if (!name || basename != conf_def_filename) {
		if (!name)
			name = conf_def_filename;
		sprintf(tmpname, "%s.old", name);
		rename(name, tmpname);
	}
	sprintf(tmpname, "%s%s", dirname, basename);
	if (rename(newname, tmpname))
		return 1;

	sym_change_count = 0;

	return 0;
}
