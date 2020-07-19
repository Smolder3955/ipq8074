/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "nlcfg_hlos.h"
#include "nlcfg_param.h"

/**
 * @brief protocol info
 */
struct proto_info {
	char *proto_name;
	uint8_t proto_num;
};

/* IP protocol array */
struct proto_info protocol_array[] = {
	{"ipinip", 4},
	{"tcp", 6},
	{"udp", 17},
	{"ipv6", 41},
	{"ipv6-frag", 44},
	{"gre", 47},
	{"esp", 50},
	{"ah", 51},
	{"ipip", 94},
	{"etherip", 97},
	{"sctp", 132},
	{"udplite", 136},
	{NULL, 0}
};

/*
 * ******************
 * Internal Functions
 * ******************
 */

/*
 * nlcfg_param_char2num()
 * 	converts a character to a hexadecimal number
 */
static inline uint8_t nlcfg_param_char2hex(char c)
{
	switch(c) {
	case '0' ... '9': /* numbers */
		return c - '0';

	case 'a' ... 'f': /* hex */
		return c - 'a' + 10;

	case 'A' ... 'F': /* hex */
		return c - 'A' + 10;

	default:
		return 0;
	}
}

/*
 * ******************
 * Exported Functions
 * ******************
 */
/*
 * nlcfg_param_search()
 * 	returns a parameter from the table that matches with the name
 */
static struct nlcfg_param *nlcfg_param_search(char *name, struct nlcfg_param param_tbl[], uint32_t num_params)
{
	struct nlcfg_param *param;

	if (!name || !param_tbl) {
		return NULL;
	}

	for (param = &param_tbl[0]; num_params; num_params--, param++) {
		/* match the param with the input name */
		if (strncmp(name, param->name, param->len) == 0) {
			return param;
		}
	}

	return NULL;
}

/*
 * nlcfg_param_help()
 * 	iterate through the table and dump help
 */
int nlcfg_param_help(struct nlcfg_param *param)
{
	struct nlcfg_param *sub_param = NULL;
	int i;

	if (!param) {
		return 0;
	}

	/* walk the parameter table to print the options */
	for (i = 0, sub_param = param->sub_params; i < param->num_params; i++, sub_param++) {
		nlcfg_log_arg_options((i + 1), sub_param);
	}

	return 0;
}

/*
 * nlcfg_param_iter_tbl()
 * 	iterate through the table and call the match callback for thee matching entries
 */
int nlcfg_param_iter_tbl(struct nlcfg_param *param, struct nlcfg_param_in *match)
{
	int error = -EINVAL;
	int i;

	if (!param || !match) {
		return error;
	}


	/* walk the parameter table  to find the matching command */
	for (i = 0; i < match->total; i++) {
		char *name = match->args[i];

		match->cur_param = param;

		/* search for the parameter in its sub_param table */
		struct nlcfg_param *sub_param = nlcfg_param_search(name, param->sub_params, param->num_params);
		if (!sub_param) {
			continue;
		}

		error = 0;

		sub_param->data = name + sub_param->len;

		/* parameter found, call the found handler if present */
		if (!sub_param->match_cb) {
			continue;
		}

		error = sub_param->match_cb(sub_param, match);
		if (error) {
			break;
		}
	}

	return error;
}

/*
 * nlcfg_param_get_str()
 * 	extract the string from the incoming data
 */
int nlcfg_param_get_str(const char *arg, uint16_t data_sz, void *data)
{
	if (!arg || !data) {
		nlcfg_log_error("string arg or data is NULL\n");
		return -EINVAL;
	}

	strncpy(data, arg, data_sz);

	return 0;
}

/*
 * nlcfg_param_get_int()
 * 	extract the integer from the incoming data
 */
int nlcfg_param_get_int(const char *arg, uint16_t data_sz, void *data)
{
	long int_val;
	char *end;

	if (!arg || !data) {
		return -EINVAL;
	}

	int_val = strtol(arg, &end, 10);

	if ((int_val == LONG_MIN) || (int_val == LONG_MAX)) {
		return -E2BIG;
	}

	memcpy(data, &int_val, data_sz);

	return 0;
}

/*
 * nlcfg_param_get_hex()
 * 	extract the hexadecimal from the incoming data
 */
int nlcfg_param_get_hex(const char *arg, uint16_t data_sz, uint8_t *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	/*
	 * 2 bytes in the string represents 1 byte in the data;
	 * Also, we want to ensure not going beyond the internal
	 * storage size
	 */
	size_t arg_len = strnlen(arg, data_sz * 2);

	/*
	 * align the argument length to 2 bytes
	 */
	size_t data_len = (NLCFG_ALIGN(arg_len, 2) / 2);

	int i = arg_len - 1;
	int j = data_len - 1;

	/*
	 * Read input stream and extract 2 bytes and store them
	 * in 1 byte unsigned array
	 */
	for (; i >= 0; i--, j--) {
		data[j] = nlcfg_param_char2hex(arg[i]);

		if (--i < 0) {
			break;
		}

		data[j] |= (nlcfg_param_char2hex(arg[i]) << 4);

	}

	return 0;
}

/*
 * nlcfg_param_get_ipaddr()
 * 	extract the IPv4 or IPv6 address from the incoming data
 */
int nlcfg_param_get_ipaddr(const char *arg, uint16_t data_sz, void *data)
{
	if (!arg || !data) {
		return -EINVAL;
	}

	switch(data_sz) {
	case sizeof(struct in_addr): /* IPv4 */
		if (inet_pton(AF_INET, arg, data) == 0) {
			return -EINVAL;
		}

		break;

	case sizeof(struct in6_addr): /* IPv6 */
		if (inet_pton(AF_INET6, arg, data) == 0) {
			return -EINVAL;
		}

		break;

	default:
		nlcfg_log_error("IP address storage incorrect:%d\n", data_sz);
		return -E2BIG;
	}

	return 0;
}

/*
 * nlcfg_param_get_protocol()
 * 	Get the protocol number from the string.
 */
int nlcfg_param_get_protocol(char *str, uint8_t *protocol_num)
{
	uint32_t i = 0;

	while (protocol_array[i].proto_name != NULL) {
		if (!(strcmp(protocol_array[i].proto_name, str))) {
			protocol_num = &(protocol_array[i].proto_num);
			return 0;
		}

		i++;
	}
	return -EINVAL;
}
