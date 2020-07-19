/*
 * mdump.c
 *	memory dump utility
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/*
 * In normal Unix (BSD/Linux), /dev/mem is directly accessable.
 * In embedded Linux system, /dev/mem can only be accessed via mmap due to
 * read_mem in drivers/char/mem.c is calling range_is_allowed() ->
 * devmem_is_allowed:
 *
 * devmem_is_allowed() checks to see if /dev/mem access to a certain
 * memory region is valid. The argument is a physical page number.
 * They mimic x86 here by disallowing access to system RAM as well as
 * device-exclusive MMIO regions. This effectively disable read()/write()
 * on /dev/mem.
 *
 * #ifdef CONFIG_STRICT_DEVMEM
 *
 * #include <linux/ioport.h>
 *
 * int devmem_is_allowed(unsigned long pfn)
 * {
 * 	if (iomem_is_exclusive(pfn << PAGE_SHIFT))
 * 		return 0;
 * 	if (!page_is_ram(pfn))
 * 		return 1;
 * 	return 0;
 * }
 *
 * #endif
 * "linux-4.4/arch/arm/mm/mmap.c" 240
 *
 * However, this explored a hole on mmap system call that does not check
 * uid; thus, this app can be used by any user to access entire CPU address
 * spacing.
 * This hole seems to be on every Unix platform. Therefore, do not let
 * regular users to access this application, which can cause system crash
 * besides unauthorized memory access.	"chmod 500" is recommended.
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define mdump_align(s, as)	((s + as - 1) & ~(as))
#define MDUMP_BYTES_TO_DUMP	(16 << 20)	/* mini 16MB for NSS FW DDR dump */
#define MDUMP_NSS_DDR_START	0x40000000

static void mdump_usage(int ac, char **av)
{
	fprintf(stderr, "bad parameter %d: %s\n", ac, av[ac-1]);
	fprintf(stderr, "	[-a address <%x> [-b bytes <%d>]\n",
		MDUMP_NSS_DDR_START, MDUMP_BYTES_TO_DUMP);

	exit(127);
}

int main(int argc, char **argv)
{
	uint32_t bytes = MDUMP_BYTES_TO_DUMP;
	size_t	addr = MDUMP_NSS_DDR_START;
	size_t	offset, map_base, map_size;
	int pagesize, pagemask;
	int8_t *mem;
	int ch, fd;

	while ((ch = getopt(argc, argv, "a:b:")) != -1) {
		switch (ch) {
		case 'a':
			addr = strtoul(argv[optind-1], NULL, 0);
			break;
		case 'b':
			bytes = strtoul(argv[optind-1], NULL, 0);
			break;
		default:
			mdump_usage(optind, argv);
		}
	}

	fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}

	pagesize = sysconf(_SC_PAGESIZE);
	fprintf(stderr, "page size %d\n", pagesize);

	pagemask = pagesize - 1;
	map_base = addr & ~pagemask;
	offset = addr & pagemask;
	map_size = mdump_align(bytes + offset, pagesize);
	mem = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, map_base);
	if (mem == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return 2;
	}

	if (write(1, mem + offset, bytes) < 0)
		perror("write");

	munmap(mem, map_size);
	close(fd);

	return 0;
}
