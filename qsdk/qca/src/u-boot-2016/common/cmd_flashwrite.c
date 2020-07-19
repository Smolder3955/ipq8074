/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * FlashWrite command support
 */
#include <common.h>
#include <command.h>
#include <asm/arch-qca-common/smem.h>
#include <part.h>
#include <linux/mtd/mtd.h>
#include <nand.h>
#include <mmc.h>
#include <sdhci.h>

DECLARE_GLOBAL_DATA_PTR;
#ifndef CONFIG_SDHCI_SUPPORT
extern qca_mmc mmc_host;
#else
extern struct sdhci_host mmc_host;
#endif

#define SMEM_PTN_NAME_MAX     16

static int write_to_flash(int flash_type, uint32_t address, uint32_t offset,
uint32_t part_size, uint32_t file_size, char *layout)
{

	char runcmd[256];
	int nand_dev = CONFIG_NAND_FLASH_INFO_IDX;

	if (flash_type == SMEM_BOOT_NAND_FLASH) {

		snprintf(runcmd, sizeof(runcmd), "nand device %d && ", nand_dev);

		if (strcmp(layout, "default") != 0) {

			snprintf(runcmd + strlen(runcmd), sizeof(runcmd),
						"ipq_nand %s && ", layout);
		}

		snprintf(runcmd + strlen(runcmd), sizeof(runcmd),
			"nand erase 0x%x 0x%x && "
			"nand write 0x%x 0x%x 0x%x && ",
			offset, part_size,
			address, offset, file_size);

	} else if (flash_type == SMEM_BOOT_MMC_FLASH) {

		snprintf(runcmd, sizeof(runcmd),
			"mmc erase 0x%x 0x%x && "
			"mmc write 0x%x 0x%x 0x%x && ",
			offset, part_size,
			address, offset, file_size);

	} else if (flash_type == SMEM_BOOT_SPI_FLASH) {

		snprintf(runcmd, sizeof(runcmd),
			"sf probe && "
			"sf erase 0x%x 0x%x && "
			"sf write 0x%x 0x%x 0x%x && ",
			offset, part_size,
			address, offset, file_size);
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int fl_erase(int flash_type, uint32_t offset, uint32_t part_size,
							 char *layout)
{
	char runcmd[256];
	int nand_dev = CONFIG_NAND_FLASH_INFO_IDX;

	if (flash_type == SMEM_BOOT_NAND_FLASH) {

		snprintf(runcmd, sizeof(runcmd), "nand device %d && ", nand_dev);
		if (strcmp(layout, "default") != 0) {
			snprintf(runcmd + strlen(runcmd), sizeof(runcmd),
						"ipq_nand %s && ", layout);
		}

		snprintf(runcmd + strlen(runcmd), sizeof(runcmd),
					"nand erase 0x%x 0x%x ",
					 offset, part_size);

	} else if (flash_type == SMEM_BOOT_MMC_FLASH) {

		snprintf(runcmd, sizeof(runcmd),
				"mmc erase 0x%x 0x%x ",
				 offset, part_size);

	} else if (flash_type == SMEM_BOOT_SPI_FLASH) {

		snprintf(runcmd, sizeof(runcmd),
				"sf probe && "
				"sf erase 0x%x 0x%x ",
				 offset, part_size);
	}

	if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_flash(cmd_tbl_t *cmdtp, int flag, int argc,
char * const argv[])
{
	int flash_cmd = 0;
	uint32_t load_addr, offset, part_size, file_size, adj_size;
	uint32_t size_block, start_block, file_size_cpy;
	char *part_name = NULL, *filesize, *loadaddr;
	int flash_type, ret, retn;
	unsigned int active_part = 0;
	char *layout = NULL;
#ifdef CONFIG_IPQ806X
	char* layout_linux[] = {"rootfs", "0:BOOTCONFIG", "0:BOOTCONFIG1"};
	int len, i;
#endif
	offset = 0;
	part_size = 0;
	layout = "default";
	retn = CMD_RET_FAILURE;

	block_dev_desc_t *blk_dev;
	disk_partition_t disk_info = {0};
	qca_smem_flash_info_t *sfi = &qca_smem_flash_info;
	nand_info_t *nand = &nand_info[CONFIG_NAND_FLASH_INFO_IDX];

	if (strcmp(argv[0], "flash") == 0)
		flash_cmd = 1;

	if (flash_cmd) {
		if ((argc < 2) || (argc > 4))
			return CMD_RET_USAGE;

		if (argc == 2) {
			loadaddr = getenv("fileaddr");
			if (loadaddr != NULL)
				load_addr = simple_strtoul(loadaddr, NULL, 16);
			else
				return CMD_RET_USAGE;

			filesize = getenv("filesize");
			if (filesize != NULL)
				file_size = simple_strtoul(filesize, NULL, 16);
			else
				return CMD_RET_USAGE;

		} else if (argc == 4) {
			load_addr = simple_strtoul(argv[2], NULL, 16);
			file_size = simple_strtoul(argv[3], NULL, 16);

		} else
			return CMD_RET_USAGE;

		file_size_cpy = file_size;
	}
	else {
		if (argc != 2)
			return CMD_RET_USAGE;
	}

	flash_type = sfi->flash_type;
	part_name = argv[1];

	if (sfi->flash_type == SMEM_BOOT_NAND_FLASH) {

		ret = smem_getpart(part_name, &start_block, &size_block);
		if (ret)
			return retn;

		offset = sfi->flash_block_size * start_block;
		part_size = sfi->flash_block_size * size_block;
#ifdef CONFIG_IPQ806X
		len = sizeof(layout_linux)/sizeof(layout_linux[0]);

		for (i = 0; i < len; i++) {

			if (!strncmp(layout_linux[i], part_name,
						SMEM_PTN_NAME_MAX)) {
				layout = "linux";
				break;
			}
		}

		if (i == len )
			layout = "sbl";
#endif

	} else if (sfi->flash_type == SMEM_BOOT_MMC_FLASH) {

		blk_dev = mmc_get_dev(mmc_host.dev_num);
		if (blk_dev != NULL) {

			ret = get_partition_info_efi_by_name(blk_dev,
			part_name, &disk_info);
			if (ret)
				return retn;

			offset = (ulong)disk_info.start;
			part_size = (ulong)disk_info.size;
		}

	} else if (sfi->flash_type == SMEM_BOOT_SPI_FLASH) {

		if (get_which_flash_param(part_name)) {

			/* NOR + NAND*/
			flash_type = SMEM_BOOT_NAND_FLASH;
			ret = getpart_offset_size(part_name, &offset, &part_size);
			if (ret)
				return retn;

		} else if ((sfi->flash_secondary_type == SMEM_BOOT_NAND_FLASH)
				&& (strncmp(part_name, "rootfs", 6) == 0)) {

			/* IPQ806X - NOR + NAND */
			flash_type = SMEM_BOOT_NAND_FLASH;

			if (sfi->rootfs.offset == 0xBAD0FF5E) {
				if (smem_bootconfig_info() == 0)
					active_part = get_rootfs_active_partition();

				offset = (ulong) active_part * IPQ_NAND_ROOTFS_SIZE;
				part_size = (ulong) IPQ_NAND_ROOTFS_SIZE;
			}

		}else if ((smem_getpart(part_name, &start_block, &size_block)
				== -ENOENT) && (sfi->rootfs.offset == 0xBAD0FF5E)){

			/* NOR + EMMC */
			flash_type = SMEM_BOOT_MMC_FLASH;

			blk_dev = mmc_get_dev(mmc_host.dev_num);
			if (blk_dev != NULL) {

				ret = get_partition_info_efi_by_name(blk_dev,
							part_name, &disk_info);
				if (ret)
					return retn;

				offset = (ulong)disk_info.start;
				part_size = (ulong)disk_info.size;
			}
		} else {

			ret = smem_getpart(part_name, &start_block,
							&size_block);
			if (ret)
				return retn;

			offset = sfi->flash_block_size * start_block;
			part_size = sfi->flash_block_size * size_block;
		}
	}

	if (flash_cmd) {
		if (flash_type == SMEM_BOOT_NAND_FLASH) {

			adj_size = file_size % nand->writesize;
			if (adj_size)
				file_size = file_size + (nand->writesize - adj_size);
		}

		if (flash_type == SMEM_BOOT_MMC_FLASH) {

			if (disk_info.blksz) {
				file_size = file_size / disk_info.blksz;
				adj_size = file_size_cpy % disk_info.blksz;
				if (adj_size)
					file_size = file_size + 1;
			}
		}

		ret = write_to_flash(flash_type, load_addr, offset, part_size,
							file_size, layout);
	} else
		ret = fl_erase(flash_type, offset, part_size, layout);

return ret;
}

U_BOOT_CMD(
	flash,       4,      0,      do_flash,
	"flash part_name \n"
	"\tflash part_name load_addr file_size \n",
	"flash the image at load_addr, given file_size in hex\n"
);

U_BOOT_CMD(
	flasherase,       4,      0,      do_flash,
	"flerase part_name \n",
	"erases on flash the given partition \n"
);
