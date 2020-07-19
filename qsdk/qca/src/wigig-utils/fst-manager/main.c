/*
 * FST CLI based main
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

#include "utils/common.h"
#include "utils/os.h"
#include "utils/eloop.h"
#include "common/defs.h"

#define FST_MGR_COMPONENT "MAINCLI"
#include "fst_manager.h"
#include "fst_ctrl.h"
#include "fst_cfgmgr.h"

#define DEFAULT_FST_INIT_RETRY_PERIOD_SEC 1
#define MAX_CTRL_IFACE_SIZE 256

extern Boolean fst_ctrl_create(const char *ctrl_iface,
	unsigned int ping_interval);
extern void fst_ctrl_free(void);

/* globals */
unsigned int fst_debug_level = MSG_INFO;
unsigned int fst_num_of_retries = 20;
unsigned int fst_ping_interval = 1;
Boolean      fst_force_nc = FALSE;
char         *fst_l2da_default_slave = NULL;
static Boolean fst_main_do_loop = FALSE;
static Boolean fst_continuous_loop = FALSE;
static Boolean terminate_signalled = FALSE;

static void fst_manager_signal_terminate(int sig)
{
	fst_mgr_printf(MSG_INFO, "termination signal arrived (%d)",
			sig);
	terminate_signalled = TRUE;
}

static void fst_manager_eloop_terminate(int sig, void *signal_ctx)
{
	eloop_terminate();
	fst_manager_signal_terminate(sig);
}

static void usage(const char *prog)
{
	printf("Usage: %s [options] [<ctrl_interace_name>]\n", prog);
	printf(", where options are:\n"
	       "\t--version, -V       - show version.\n"
	       "\t--daemon, -B        - run in daemon mode. Exit after "
			"wpa_supplicant/hostapd quits\n"
	       "\t--daemonex, -b      - continuous daemon mode. Wait for a next "
			"wpa_supplicant/hostapd\n"
	       "\t--config, -c <file> - read the FST configuration from the file\n"
	       "\t--default-slave, -s <interface name> - set default slave interface. relevant only in L2DA mode\n"
	       "\t--retries -r <int>  - number of session setup retries.\n"
	       "\t--ping-int -p <int> - CLI ping interval in sec, 0 to disable\n"
	       "\t--force-nc -n       - force non-compliant mode.\n"
	       "\t--debug, -d         - increase debugging verbosity (-dd - more, "
			"-ddd - even more)\n"
	       "\t--logfile, -f <file>- log output to specified file\n"
	       "\t--usage, -u         - this message\n"
	       "\t--help, -h          - this message\n");
	exit(2);

}

void main_loop(const char *ctrl_iface)
{
	if (!fst_ctrl_create(ctrl_iface, fst_ping_interval)) {
		fst_mgr_printf(MSG_ERROR, "cannot create fst_ctrl");
		goto error_fst_ctrl_create;
	}

	if (fst_manager_init()) {
		fst_mgr_printf(MSG_ERROR, "cannot init fst manager");
		goto error_fst_manager_init;
	}

	if (eloop_register_signal_terminate(fst_manager_eloop_terminate, NULL)) {
		fst_mgr_printf(MSG_ERROR, "eloop_register_signal_terminate");
		goto error_eloop_register_signal_terminate;
	}

	eloop_run();

	fst_mgr_printf(MSG_INFO, "eloop finished");
	if (!fst_continuous_loop)
		fst_main_do_loop = FALSE;

error_eloop_register_signal_terminate:
	fst_manager_deinit();
error_fst_manager_init:
	fst_ctrl_free();
error_fst_ctrl_create:
	return;
}

int main(int argc, char *argv[])
{
	const struct option long_opts[] = {
		{"version",  no_argument, NULL, 'V'},
		{"daemon",   no_argument, NULL, 'B'},
		{"daemonex", no_argument, NULL, 'b'},
		{"config",   required_argument, NULL, 'c'},
		{"default-slave", required_argument, NULL, 's'},
		{"retries",  required_argument, NULL, 'r'},
		{"ping-int",  required_argument, NULL, 'p'},
		{"force-nc", no_argument, NULL, 'n'},
		{"debug",    optional_argument, NULL, 'd'},
		{"logfile",  required_argument, NULL, 'f'},
		{"usage",    no_argument, NULL, 'u'},
		{"help",     no_argument, NULL, 'h'},
		{NULL}
	};
	int res = -1;
	char short_opts[] = "VBbc:s:r:nd::f:uh";
	const char *ctrl_iface = NULL;
	char *fstman_config_file = NULL;
	int opt, i;
	char buf[MAX_CTRL_IFACE_SIZE];

	wpa_debug_open_syslog();
	while ((opt = getopt_long_only(argc, argv, short_opts, long_opts, NULL))
	       != -1) {
		switch (opt) {
		case 'V':
			printf("FST Manager, version "
				FST_MANAGER_VERSION "\n");
			exit(0);
			break;
		case 'B':
			fst_main_do_loop = TRUE;
			break;
		case 'b':
			fst_main_do_loop = TRUE;
			fst_continuous_loop = TRUE;
			break;
		case 'c':
			if (fstman_config_file != NULL) {
				fst_mgr_printf(MSG_ERROR,
					"Multiple configurations not allowed\n");
				goto error_cfmgr_params;
			}
			if (optarg != NULL)
				fstman_config_file=os_strdup(optarg);
			if (fstman_config_file == NULL) {
				fst_mgr_printf(MSG_ERROR,
					"Filename memory allocation error\n");
				goto error_cfmgr_params;
			}
			break;
		case 's':
			if (optarg != NULL)
				fst_l2da_default_slave = os_strdup(optarg);
			break;
		case 'r':
			if (optarg != NULL)
				fst_num_of_retries = strtoul(optarg, NULL, 0);
			break;
		case 'p':
			if (optarg != NULL)
				fst_ping_interval = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			fst_force_nc = TRUE;
			fst_mgr_printf(MSG_INFO, "Non-compliant FST mode forced\n");
			break;
		case 'd':
			fst_debug_level = MSG_DEBUG;
			if (optarg && optarg[0] == 'd') {
				fst_debug_level = MSG_MSGDUMP;
				if (optarg[1] == 'd')
					fst_debug_level = MSG_EXCESSIVE;
			}
			break;
		case 'f':
			if (wpa_debug_open_file(optarg))  {
				fst_mgr_printf(MSG_ERROR,
					"Cannot open log file: %s", optarg);
				goto error_cfmgr_params;
			}
			break;
		case 'u':
		case 'h':
		case '?':
		default:
			usage(argv[0]);
			break;
		}
	}

	if (!fstman_config_file && argc - optind != 1) {
		fst_mgr_printf(MSG_ERROR,
			"either ctrl_interace_name or config has to be specified");
		usage(argv[0]);
		goto error_cfmgr_params;
	}

	if (fstman_config_file)
		i = fst_cfgmgr_init(FST_CONFIG_INI, (void*)fstman_config_file);
	else
		i = fst_cfgmgr_init(FST_CONFIG_CLI, NULL);
	if (i != 0) {
		fst_mgr_printf(MSG_ERROR, "FST Configuration error");
		goto error_cfmgr_init;
	}

	if (argc - optind == 1)
		ctrl_iface = argv[optind];
	else if (!fst_cfgmgr_get_ctrl_iface(buf, sizeof(buf)))
		ctrl_iface = buf;
	else {
		fst_mgr_printf(MSG_ERROR, "cannot get ctrl_iface");
		goto error_ctrl_iface;
	}

	if (eloop_init() != 0)  {
		fst_mgr_printf(MSG_ERROR, "cannot init eloop");
		goto error_eloop_init;
	}

	signal(SIGINT, fst_manager_signal_terminate);
	signal(SIGTERM, fst_manager_signal_terminate);
	while (TRUE) {
		main_loop(ctrl_iface);
		if (!fst_main_do_loop || terminate_signalled)
			break;
		signal(SIGINT, fst_manager_signal_terminate);
		signal(SIGTERM, fst_manager_signal_terminate);
		os_sleep(DEFAULT_FST_INIT_RETRY_PERIOD_SEC, 0);
	}

	eloop_destroy();
	res = 0;

error_eloop_init:
error_ctrl_iface:
	fst_cfgmgr_deinit();
error_cfmgr_init:
error_cfmgr_params:
	wpa_debug_close_syslog();
	wpa_debug_close_file();
	os_free(fstman_config_file);
	os_free(fst_l2da_default_slave);
	return res;
}
