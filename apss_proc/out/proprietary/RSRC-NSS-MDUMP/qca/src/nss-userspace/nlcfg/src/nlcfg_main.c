/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "nlcfg_hlos.h"
#include "nlcfg_param.h"
#include "nlcfg_family.h"

/* Family handler table */
static struct nlcfg_param family_params[] = {
	NLCFG_PARAMLIST_INIT("family=ipv4", nlcfg_ipv4_params, nlcfg_param_iter_tbl),
	NLCFG_PARAMLIST_INIT("family=ipv6", nlcfg_ipv6_params, nlcfg_param_iter_tbl)
};

/* Family handler table */
static struct nlcfg_param root = NLCFG_PARAMLIST_INIT("nlcfg", family_params, NULL);

/*
 * main()
 */
int main(int argc, char *argv[])
{
	struct nlcfg_param_in match = {0};
	int error;

	match.total = argc;
	match.args = (char **)argv;

	if (argc < 2) {
		nlcfg_log_arg_error((struct nlcfg_param *)&root);
		match.cur_param = &root;
		error = -EINVAL;
		goto help;
	}

	error = nlcfg_param_iter_tbl(&root, &match);
	if (error < 0) {
		goto help;
	}

	return 0;
help:
	nlcfg_param_help(match.cur_param);

	return error;
}
