
/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <debug.h>
#include <lib/ptable.h>
#include <smem.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <platform/gpio.h>
#include <reg.h>
#include <app.h>
#include <gsbi.h>
#include <target.h>
#include <platform.h>
#include <uart_dm.h>
#include <crypto_hash.h>
#include <board.h>
#include <target/board.h>
#include <partition_parser.h>
#include <stdlib.h>
#include <libfdt.h>
#include <mmc_wrapper.h>
#include <err.h>

board_ipq807x_params_t *gboard_param;

gpio_func_data_t uart4_gpio_hk01[] = {
	{
		.gpio = 23,
		.func = 2,
		.pull = GPIO_PULL_DOWN,
		.oe = GPIO_OE_ENABLE
	},
	{
		.gpio = 24,
		.func = 2,
		.pull = GPIO_NO_PULL,
		.oe = GPIO_OE_ENABLE
	},
};

uart_cfg_t uart4_console_uart_hk01 = {
	.base           = GSBI_1,
	.gsbi_base      = 0,
	.uart_dm_base = UART4_DM_BASE,
	.dbg_uart_gpio = uart4_gpio_hk01,
};

/* Board specific parameter Array */
board_ipq807x_params_t board_params[] = {
	{
		.machid = MACH_TYPE_IPQ807X_AP_HK01_1_C1,
		.console_uart_cfg = &uart4_console_uart_hk01,
		.dtb_config_name = { "config@hk01" },
	},
};

#define NUM_IPQ807X_BOARDS	ARRAY_SIZE(board_params)

board_ipq807x_params_t *get_board_param(unsigned int machid)
{
	unsigned int index = 0;

	if (gboard_param)
		return gboard_param;

	for (index = 0; index < NUM_IPQ807X_BOARDS; index++) {
		if (machid == board_params[index].machid) {
			gboard_param = &board_params[index];
			return &board_params[index];
		}
	}

	printf("cdp: Invalid machine id 0x%x\n", machid);

	for (;;);

	return NULL;
}

static inline int
valid_mac_addr(const unsigned char *mac)
{
	if (!mac ||
            (mac[0] & 1) ||	/* broadcast/multicast addresses */
	    ((mac[0] == 0) && (mac[1] == 0) && (mac[2] == 0) &&
             (mac[3] == 0) && (mac[4] == 0) && (mac[5] == 0)))
		return 0;	/* Invalid */

	return 1;		/* Valid */
}

#define IPQ_GMAC_COUNT		2

#define MAC_ADDR_FMT		"%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ADDR_ARG(x)		(x)[0], (x)[1], (x)[2], (x)[3], (x)[4], (x)[5]

void update_mac_addrs(void *fdt)
{
	int i, j, index;
	unsigned long long off;
	unsigned char *mac;
	char eth[] = { "ethernetX" };

	index = partition_get_index("0:ART");
	if (index == INVALID_PTN) {
		critical("ART partition not found, can't get MAC addresses\n");
		return;
	}

	off = partition_get_offset(index);
	if (off == 0ull) {
		critical("ART partition offset invalid\n");
		return;
	}

	mac = memalign(BLOCK_SIZE, BLOCK_SIZE);
	if (mac == NULL) {
		critical("Could not allocate sufficient memory to read MAC information\n");
		return;
	}
	if (mmc_read(off, (unsigned int *)mac, BLOCK_SIZE)) {
		critical("Could not read ART partition\n");
		return;
	}

	for (i = j = 0; i < IPQ_GMAC_COUNT; i++) {
		unsigned char *p = &mac[j * 6];

		snprintf(eth, sizeof(eth), "ethernet%d", i);

		if (!valid_mac_addr(p)) {
			critical("Ignoring " MAC_ADDR_FMT " for %s\n",
					MAC_ADDR_ARG(p), eth);
			j++;
			continue;
		}

		index = fdt_path_offset(fdt, eth);
		if (index < 0) {
			info("Skipping %s\n", eth);
			continue;
		}

		info("Setting " MAC_ADDR_FMT " for %s\n", MAC_ADDR_ARG(p), eth);

		if (fdt_setprop(fdt, index, "local-mac-address", p, 6) < 0) {
			critical("DT update [" MAC_ADDR_FMT "] failed for %s\n",
					MAC_ADDR_ARG(p), eth);
			continue;
		}
		j++;
	}

	free(mac);
}

void fdt_fixup_version(void *fdt)
{
	int offset, ret;
	char ver[OEM_VERSION_STRING_LENGTH + VERSION_STRING_LENGTH + 1];

	offset = fdt_path_offset(fdt, "/");

	if (!smem_get_build_version(ver, sizeof(ver), BOOT_VERSION)) {
		ret = fdt_setprop((void *)fdt, offset, "boot_version", ver, strlen(ver));
		if (ret)
			dprintf(CRITICAL, "fdt-fixup: Unable to set Boot version\n");
	}

	if (!smem_get_build_version(ver, sizeof(ver), TZ_VERSION)) {
		ret = fdt_setprop((void *)fdt, offset, "tz_version", ver, strlen(ver));
		if (ret)
			dprintf(CRITICAL, "fdt-fixup: Unable to set TZ version\n");
	}

	if (!smem_get_build_version(ver, sizeof(ver), RPM_VERSION)) {
		ret = fdt_setprop((void *)fdt, offset, "rpm_version", ver, strlen(ver));
		if (ret)
			dprintf(CRITICAL, "fdt-fixup: Unable to set rpm version\n");
	}

	return;
}
int set_uuid_bootargs(char *boot_args, char *part_name, int buflen, bool gpt_flag)
{
	disk_partition_t disk_info;
	int ret;
	int len;

	if (!boot_args || !part_name || buflen <=0 || buflen > MAX_BOOT_ARGS_SIZE)
		return ERR_INVALID_ARGS;

	ret = get_partition_info_efi_by_name(part_name, &disk_info);
	if (ret) {
		dprintf(INFO, "%s : name not found in gpt table.\n", part_name);
		return ERR_INVALID_ARGS;
	}

	if ((len = strlcpy(boot_args, "root=PARTUUID=", buflen)) >= buflen)
		return ERR_INVALID_ARGS;

	boot_args += len;
	buflen -= len;

	if ((len = strlcpy(boot_args, disk_info.uuid, buflen)) >= buflen)
		return ERR_INVALID_ARGS;

	boot_args += len;
	buflen -= len;

	if (gpt_flag && (len = strlcpy(boot_args, " gpt rootwait nosmp", buflen)) >= buflen)
		return ERR_INVALID_ARGS;

	return 0;
}

int update_uuid(char *bootargs)
{
	int ret;

	if (smem_bootconfig_info() == 0) {
		ret = get_rootfs_active_partition();
		if (ret) {
			strlcpy(bootargs, "rootfsname=rootfs_1 gpt", MAX_BOOT_ARGS_SIZE);
			ret  = set_uuid_bootargs(bootargs, "rootfs_1", MAX_BOOT_ARGS_SIZE, true);
		} else {
			strlcpy(bootargs, "rootfsname=rootfs gpt", MAX_BOOT_ARGS_SIZE);
			ret  = set_uuid_bootargs(bootargs, "rootfs", MAX_BOOT_ARGS_SIZE, true);
		}
	} else {
		strlcpy(bootargs, "rootfsname=rootfs gpt", MAX_BOOT_ARGS_SIZE);
		ret  = set_uuid_bootargs(bootargs, "rootfs", MAX_BOOT_ARGS_SIZE, true);
	}

	if (ret) {
		dprintf(INFO, "Error in updating UUID. using device name to mountrootfs\n");
		return 0;
	}

	return 1;
}
