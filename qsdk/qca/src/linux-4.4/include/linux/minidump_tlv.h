/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
typedef struct qcom_wdt_scm_tlv_msg {
	unsigned char *msg_buffer;
	unsigned char *cur_msg_buffer_pos;
	unsigned int len;
} qcom_wdt_scm_tlv_msg_t;

struct minidump_tlv_info {
    uint64_t start;
    uint64_t size;
};

#define QCOM_WDT_SCM_TLV_TYPE_SIZE	1
#define QCOM_WDT_SCM_TLV_LEN_SIZE	2
#define QCOM_WDT_SCM_TLV_TYPE_LEN_SIZE (QCOM_WDT_SCM_TLV_TYPE_SIZE + QCOM_WDT_SCM_TLV_LEN_SIZE)

#define BUFLEN 4096
#define MOD_LOG_LEN 256
int fill_minidump_segments(uint64_t start_addr, uint64_t size, unsigned char type, char *name);
int store_module_info(char *name ,unsigned long address, unsigned char type);

struct module_sect_attr {
	struct module_attribute mattr;
	char *name;
	unsigned long address;
};

struct module_sect_attrs {
	struct attribute_group grp;
	unsigned int nsections;
	struct module_sect_attr attrs[0];
};

/* TLV_Types */
enum {
	QCA_WDT_LOG_DUMP_TYPE_INVALID,
	QCA_WDT_LOG_DUMP_TYPE_UNAME,
	QCA_WDT_LOG_DUMP_TYPE_DMESG,
	QCA_WDT_LOG_DUMP_TYPE_LEVEL1_PT,
	QCA_WDT_LOG_DUMP_TYPE_WLAN_MOD,
	QCA_WDT_LOG_DUMP_TYPE_WLAN_MOD_INFO,
};
