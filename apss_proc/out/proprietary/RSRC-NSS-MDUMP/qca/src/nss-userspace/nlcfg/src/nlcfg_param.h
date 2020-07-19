/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NLCFG_PARAM_H
#define __NLCFG_PARAM_H

#define NLCFG_PARAM_PROTO_LEN 20

#define NLCFG_PARAM_NUM(param_array) (sizeof(param_array) / sizeof(param_array[0]))

#define NLCFG_PARAM_INIT(_idx, _param) 	\
[_idx] = {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = 1,	\
}

#define NLCFG_PARAMLIST_INIT(_param, _sub_param_tbl, _match_cb) {	\
	.name = (_param),	\
	.len = sizeof(_param) - 1,	\
	.num_params = NLCFG_PARAM_NUM(_sub_param_tbl),	\
	.sub_params = (_sub_param_tbl),	\
	.match_cb = (_match_cb),	\
}

struct nlcfg_param;

/**
 * @brief match list provided matching the paramters
 */
struct nlcfg_param_in {
	uint32_t total;			/**< total number of parameters */
	struct nlcfg_param *cur_param;	/**< current used param */
	char **args;			/**< list of arguments */
};

/**
 * @brief callback function when the match succeeds
 *
 * @param param[IN] parameter that matched
 * @param match[IN] match list used
 * @param data[OUT] store data extracted from match list; caller should provide valid memory
 *
 * @return 0 for success and -ve for failure
 */
typedef int ( *nlcfg_param_match_t)(struct nlcfg_param *param, struct nlcfg_param_in *match);

/**
 * @brief parameter definition
 */
struct nlcfg_param {
	char *name;				/**< name of the parameter */
	char *data;				/**< pointer to data portion */

	uint16_t len;				/**< string length of the parameter */
	uint16_t num_params;			/**< number of sub-parameters present */

	struct nlcfg_param *sub_params;		/**< sub-parameter list */
	nlcfg_param_match_t match_cb;		/**< match callback function upon match */
};

/**
 * @brief iterate through a table of sub parameters and match it with the corresponding of arguments
 *
 * @param param[IN] paramter
 * @param match[IN] argument list
 *
 * @return 0 on sucess and -ve on failure
 */
int nlcfg_param_iter_tbl(struct nlcfg_param *param, struct nlcfg_param_in *match);

/**
 * @brief dump help options for the corresponding param table
 *
 * @param param[IN] parent parameter
 *
 * @return 0
 */
int nlcfg_param_help(struct nlcfg_param *param);
/**
 * @brief extract the string from the argument
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the string
 * @param data[OUT] storage location where the string needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int nlcfg_param_get_str(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the integer from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the integer {1, 2, 4, 8}
 * @param data[OUT] storage location where the integer needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int nlcfg_param_get_int(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the hex number from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size of the hex number
 * @param data[OUT] storage location where the hex number needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int nlcfg_param_get_hex(const char *arg, uint16_t data_sz, uint8_t *data);

/**
 * @brief extract the IPv4 or IPv6 address from the argument string
 *
 * @param arg[IN] argument string
 * @param data_sz[IN] maximum accepted size for IP address {IPv4 = 4 & IPv6 = 16}
 * @param data[OUT] storage location where the IP address needs to be copied
 *
 * @return 0 on success or -ve for failure
 */
int nlcfg_param_get_ipaddr(const char *arg, uint16_t data_sz, void *data);

/**
 * @brief extract the protocol number from the protocol string
 *
 * @param str protocol string
 *
 * @param protocol_num[OUT] protocol number for matched protocol name
 *
 * @return 0 on success or -ve on failure
 */
int nlcfg_param_get_protocol(char *str, uint8_t *protocol_num);

#endif /* __NLCFG_PARAM_H*/

