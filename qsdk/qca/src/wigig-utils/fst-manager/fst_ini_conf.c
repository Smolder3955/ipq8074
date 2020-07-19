/*
 * FST Manager: INI configuration file parser
 *
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdlib.h>
#include "fst_ini_conf.h"
#include "ini.h"

#define FST_MGR_COMPONENT "INI_CONF"
#include "fst_manager.h"

struct fst_ini_config
{
	char *filename;
};

struct cfg_ctx {
	const char *section;
	const char *name;
	char       *buffer;
	int        buflen;
	Boolean    is_found;
};

static int ini_handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct cfg_ctx *ctx = (struct cfg_ctx*)user;
	if (!ctx->is_found) {
		if (strncmp(section, ctx->section, strlen(section)) == 0 &&
			strncmp(name, ctx->name, strlen(name)) == 0) {
			ctx->is_found = TRUE;
			if (ctx->buffer && ctx->buflen) {
				os_strlcpy(ctx->buffer, value, ctx->buflen);
			}
		}
	}
	return 1;
}

static int parse_csv(char *string, char **tokens, int tokenslen)
{
	const char *delim = " ,\t";
	char *tokbuf;
	int count = 0;
	char *token = strtok_r(string, delim, &tokbuf);
	while (token != NULL && count < tokenslen) {
		count++;
		*tokens++ = token;
		token = strtok_r(NULL, delim, &tokbuf);
	}
	return count;
}

static Boolean fst_ini_config_read(struct fst_ini_config *h, const char *s,
	const char *k, char *buf, int buflen)
{
	struct cfg_ctx ctx;
	ctx.section = s;
	ctx.name = k;
	ctx.buffer = buf;
	ctx.buflen = buflen;
	ctx.is_found = FALSE;

	if (ini_parse(h->filename, ini_handler, &ctx) < 0) {
		fst_mgr_printf(MSG_ERROR, "Error parsing configuration file %s",
			h->filename);
	}
	return ctx.is_found;
}

struct fst_ini_config *fst_ini_config_init(const char *filename)
{
	struct fst_ini_config *h = (struct fst_ini_config*)malloc(
		sizeof(struct fst_ini_config));
	if (h == NULL) {
		fst_mgr_printf(MSG_ERROR, "Cannot allocate memory for config");
		return NULL;
	}
	h->filename = strdup(filename);
	if (h->filename == NULL) {
		free(h);
		fst_mgr_printf(MSG_ERROR, "Cannot allocate memory for filename");
		return NULL;
	}
	return h;
}

void fst_ini_config_deinit(struct fst_ini_config *h)
{
	if (h)
		free(h->filename);
	free(h);
}

/*
 * FST Manager standalone configuration
 */
#define INI_MAX_STRING 80
#define INI_MAX_TOKENS 16

int fst_ini_config_get_ctrl_iface(struct fst_ini_config *h, char *buf, int size)
{
	if (!fst_ini_config_read(h, "fst_manager", "ctrl_iface",
		buf, size)) {
		fst_mgr_printf(MSG_ERROR,
			"No ctrl_iface found in fst_manager config");
		return -1;
	}

	return 0;
}

int fst_ini_config_get_group_ifaces(struct fst_ini_config *h,
	const struct fst_group_info *group, struct fst_iface_info **ifaces)
{
	char buf[INI_MAX_STRING+1], buf_ifaces[INI_MAX_STRING+1];
	char *tokens[INI_MAX_TOKENS];
	int i, cnt;

	if (!fst_ini_config_read(h, (const char*)group->id, "interfaces",
		buf_ifaces, INI_MAX_STRING)) {
		fst_mgr_printf(MSG_ERROR,
			"No interfaces key for group %s", group->id);
		return -1;
	}

	cnt = parse_csv(buf_ifaces, tokens, INI_MAX_TOKENS);
	if (cnt == 0) {
		fst_mgr_printf(MSG_ERROR,
			"No interfaces found for group %s", group->id);
		return -1;
	}
	fst_mgr_printf(MSG_INFO,"%d interfaces found", cnt);

	*ifaces = (struct fst_iface_info*)os_zalloc(
		sizeof(struct fst_iface_info) * cnt);
	if (*ifaces == NULL) {
		fst_mgr_printf(MSG_ERROR,
			"Cannot allocate memory for interface");
		return -1;
	}
	for (i = 0; i < cnt; i++) {
		os_strlcpy((*ifaces)[i].name, tokens[i], sizeof((*ifaces)[i].name));
		if (fst_ini_config_read(h, (const char*)tokens[i], "priority",
			buf, INI_MAX_STRING))
			(*ifaces)[i].priority = strtoul(buf, NULL, 0);
		if (fst_ini_config_read(h, (const char*)tokens[i], "default_llt",
			buf, INI_MAX_STRING))
			(*ifaces)[i].llt = strtoul(buf, NULL, 0);
		fst_mgr_printf(MSG_DEBUG,
			"iface %s (pri=%d, llt=%d) has been parsed",
			(*ifaces)[i].name, (*ifaces)[i].priority, (*ifaces)[i].llt);
	}
	return cnt;
}

int fst_ini_config_get_groups(struct fst_ini_config *h,
	struct fst_group_info **groups)
{
	char buf[INI_MAX_STRING+1];
	char *tokens[INI_MAX_TOKENS];
	int i, cnt;

	*groups = NULL;
	if (!fst_ini_config_read(h, "fst_manager", "groups",
		buf, INI_MAX_STRING)) {
		fst_mgr_printf(MSG_ERROR,
			"No groups key found in fst_manager config");
		return -1;
	}

	cnt = parse_csv(buf, tokens, INI_MAX_TOKENS);
	if (cnt == 0) {
		fst_mgr_printf(MSG_ERROR,
			"No groups defined in config");
		return -1;
	}
	*groups = (struct fst_group_info*)malloc(
		sizeof(struct fst_group_info) * cnt);
	if (*groups == NULL) {
		fst_mgr_printf(MSG_ERROR,"Cannot allocate memory for groups");
		return -1;
	}

	for (i = 0; i < cnt; i++) {
		os_strlcpy((*groups)[i].id, tokens[i], sizeof((*groups)[i].id));
		fst_mgr_printf(MSG_DEBUG,"Config: group %s has been parsed", tokens[i]);
	}
	return cnt;
}

char *fst_ini_config_get_rate_upgrade_master(struct fst_ini_config *h,
	const char *groupname)
{
	char buf[INI_MAX_STRING+1];
	if(!fst_ini_config_read(h, groupname, "rate_upgrade_master", buf,
	   INI_MAX_STRING))
		return NULL;
	return strdup(buf);
}

char *fst_ini_config_get_rate_upgrade_acl_fname(struct fst_ini_config *h,
	const char *groupname)
{
	char buf[INI_MAX_STRING+1];
	if(!fst_ini_config_read(h, groupname, "rate_upgrade_acl_file", buf,
	   INI_MAX_STRING))
		return NULL;
	return strdup(buf);
}

int fst_ini_config_get_group_slave_ifaces(struct fst_ini_config *h,
	const struct fst_group_info *group, const char *master,
	struct fst_iface_info **ifaces)
{
	int i, cnt;

	fst_mgr_printf(MSG_INFO,"group %s master %s", group->id, master);

	cnt = fst_ini_config_get_group_ifaces(h, group, ifaces);
	if (cnt < 0)
		return cnt;

	for (i=0; i < cnt; i++) {
		if (!strncmp((*ifaces)[i].name, master, strlen(master))) {
			/* Skip the master interface */
			cnt--;
			if (i < cnt)
				os_memmove(&(*ifaces)[i], &(*ifaces)[i+1],
					(cnt-i) * sizeof(struct fst_iface_info));
			return cnt;
		}
	}
	return cnt;
}

int fst_ini_config_get_mux_type(struct fst_ini_config *h,
	const char *gname, char *buf, int buflen)
{
	if(!fst_ini_config_read(h, gname, "mux_type", buf, buflen))
		return 0;
	return strlen(buf);
}


int fst_ini_config_get_mux_ifname(struct fst_ini_config *h,
	const char *gname, char *buf, int buflen)
{
	if(!fst_ini_config_read(h, gname, "mux_ifname", buf, buflen))
		return 0;
	return strlen(buf);
}

int fst_ini_config_get_l2da_ap_default_ifname(struct fst_ini_config *h,
	const char *gname, char *buf, int buflen)
{
	if(!fst_ini_config_read(h, gname, "l2da_ap_default_ifname", buf, buflen))
		return 0;
	return strlen(buf);
}

Boolean fst_ini_config_is_mux_managed(struct fst_ini_config *h, const char *gname)
{
	char buf[INI_MAX_STRING+1];
	if(!fst_ini_config_read(h, gname, "mux_managed", buf,
	   INI_MAX_STRING))
		return FALSE;
	if (buf[0] == '1' || buf[0] == 'y' || buf[0] == 'Y')
		return TRUE;
	else if (buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N')
		return FALSE;
	else {
		fst_mgr_printf(MSG_ERROR,
			"mux_managed wrong boolean %s, assuming false", buf);
		return FALSE;
	}
}

int fst_ini_config_get_iface_group_cipher(struct fst_ini_config *h,
	const struct fst_iface_info *iface, char *buf, int len)
{
	if(!fst_ini_config_read(h, iface->name, "wpa_group", buf, len))
		return 0;
	return strlen(buf);
}

int fst_ini_config_get_iface_pairwise_cipher(struct fst_ini_config *h,
	const struct fst_iface_info *iface, char *buf, int len)
{
	if(!fst_ini_config_read(h, iface->name, "wpa_pairwise", buf, len))
		return 0;
	return strlen(buf);
}

int fst_ini_config_get_iface_hw_mode(struct fst_ini_config *h,
	const struct fst_iface_info *iface, char *buf, int len)
{
	if(!fst_ini_config_read(h, iface->name, "hw_mode", buf, len))
		return 0;
	return strlen(buf);
}

int fst_ini_config_get_iface_channel(struct fst_ini_config *h,
	const struct fst_iface_info *iface, char *buf, int len)
{
	if(!fst_ini_config_read(h, iface->name, "channel", buf, len))
		return 0;
	return strlen(buf);
}

int fst_ini_config_get_txqueuelen(struct fst_ini_config *h, const char *gname)
{
	char buf[INI_MAX_STRING + 1];

	if (!fst_ini_config_read(h, gname, "txqueuelen", buf, INI_MAX_STRING))
		return -1;

	return atoi(buf);
}

int fst_ini_config_get_slave_ctrl_interface(struct fst_ini_config *h, char *buf, int len)
{
	if(!fst_ini_config_read(h, "fst_manager", "slave_ctrl_iface_dir", buf, len))
		return 0;
	return strlen(buf);
}
