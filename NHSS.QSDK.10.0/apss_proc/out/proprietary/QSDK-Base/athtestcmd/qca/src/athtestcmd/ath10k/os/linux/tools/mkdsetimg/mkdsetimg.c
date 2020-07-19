/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>

#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>

#include "athdefs.h"
#include "athtypes_linux.h"
#include "a_osapi.h"

#include "dsetid.h"
#include "dset_internal.h"

#include "lz.h"

/*
 * Overview:
 * This application is used to create a byte-for-byte image of memory
 * that contains all DataSets that should be loaded into a given type
 * of memory (ROM, Flash, or RAM).  The DataSet memory image that is
 * produced is stored in a file and includes both DataSet Data and
 * DataSet Metadata.  The file might be included in a ROM firmware
 * image, or programmed into flash (via flashup.sh) or downloaded
 * into Target RAM during BMI phase (via bmiloader).
 *
 * DataSet Description file:
 * Operation of this tool is guided by command line arguments and
 * by a DataSet Description File (e.g. dsets.txt).
 * Line formats:
 *    dsetid filename [dset_flag] [dset_flag]
 * where dset_flag is "standard", "compress" or "bpatch".
 * (Note: Actually only the first letter of the dset_flag matters.)
 * 
 * If no dset_flag is specified, then the DataSet is a standard
 * uncompressed DataSet.  If "bpatch" is specified, then the file
 * specified by filename contains bdiff data for an existing
 * DataSet with id=dsetid.  If "compress" is specified then the
 * standard data or bdiff data will be compressed before it is
 * put into the DataSet image.
 *
 * DataSet Map file:
 * This tool optionally accepts -- and generates -- a "DataSet Map"
 * file.  This file provides a list of DataSets along with the Target
 * address where each DataSet can be found.  The map file that was
 * generated when the ROM dset image was created is used, for
 * example, as the map input file for generation of the Flash DataSet
 * image.
 *
 * Map File Format:
 *     DescriptorAddress dsetid size DataPtr type AuxPtr NextDesc
 *
 * [Note that the only information that is really required for a
 * subsequent run is the DescriptorAddress of the first descriptor;
 * the other information in a map file is included because it's
 * easy to generate and may be useful for debug.]
 *
 * Internals:
 * This tool constructs an image in memory and then commits that in-core
 * image to a file.  It also maintains an oldmap table which contains
 * one entry for each pre-existing DataSet (specified via the mapin
 * parameter).  For example, if we're creating the RAM DataSet image,
 * then pre-existing DataSets are those in ROM and/or Flash.  The oldmap
 * table is initialized from the supplied mapin file.  This tool also
 * maintains a newmap table which contains one entry for each DataSet
 * that is added (with new entries for lines in the DataSet Description
 * file).  Both the oldmap and the newmap entries track 1) where in
 * Target memory the descriptor belongs and 2) complete contents of
 * the descriptor.
 *
 * The in-core DataSet image can be built so that it starts or
 * ends at a specified address.  This allows for some flexibility
 * in placing the image in Target memory.  In either case, the
 * end result puts metadata (descriptors) first followed by data.
 *
 * DataSets are padded, if needed so that every DataSet begins on
 * a word boundary; but the size stored in a descriptor represents
 * the actual number of bytes of data (does not reflect padding).
 *
 * This tool includes a few features added just in order to ease
 * integration with the build process.  First, it accepts a pad
 * argument which adds a specified number of bytes of padding at
 * the end of the image.  Second, the tool reserves the last
 * word of the image to hold a Target memory pointer to the start
 * of the image, which is also the first descriptor.
 */

#if 0
#define DEBUG /* Enable debugging within this application */
#endif

/* Maximum length of a filename */
#define MAX_FILENAME 1023

/* Maximum size of a DataSet Image (arbitrary limit) */
#define MAX_DSETIMG_SZ (16*1024)

/* Maximum number of DataSets supported (arbitrary limit) */
#define MAX_NUM_DSET 64

/* Maximum number of characters in a line of input (arbitrary limit) */
#define MAX_LINE_BUF 256

/* Flags, specified in DataSet Descriptor file */
#define FLAG_STANDARD   1
#define FLAG_BPATCH     2
#define FLAG_COMPRESS   4

/* DataSet Description File (e.g. dset.desc.txt) */
char desc_filename[MAX_FILENAME+1];
FILE *desc_file;

/* 
 * Old DataSet Map File, used as input to
 * this application ("mapin").
 */
char oldmap_filename[MAX_FILENAME+1];
FILE *oldmap_file;

/*
 * New DataSet Map File, produced as output by
 * this application ("mapout").
 */
char newmap_filename[MAX_FILENAME+1];
FILE *newmap_file;

/*
 * Space to hold the memory image that's being prepared.
 * This is where the in-core DataSet image resides.
 */
char dsetimg_space[MAX_DSETIMG_SZ];
int nbytes_remaining;

/*
 * These point to the start/end of the dset image in memory.
 * At first, both pointers point to the end of the in-core
 * space.  When a new dset is added, the start pointer is
 * adjusted downward accordingly.
 */
char *dsetimg_start, *dsetimg_end;

/*
 * User-specified start OR end Target address.  Exactly one can
 * be specified. This allows the user to choose placement within
 * Target memory so that the image starts or ends at a certain
 * Target address.
 */
A_UINT32 start_memaddr, end_memaddr;

/* Output file to contain the DataSet image. */
char out_filename[MAX_FILENAME+1];
FILE *out_file;

/*
 * Number of bytes of padding desired at tail of the output file.
 * This is appended to the file after all other processing has
 * completed and just helps this tool fit into the build flow
 * more easily.
 */
int tailpad;    
char zeroes[4]={0,0,0,0}; /* Used for zero padding */

A_BOOL verbose;

/*
 * One of these holds information for each line in the
 * mapin file (pre-existing DataSets) and for each new
 * generated DataSet.
 */
typedef struct dset_map_entry_s {
    A_UINT32 targaddr;          /* location in Target memory */
    dset_descriptor_t desc;     /* descriptor information */
} dset_map_entry_t;

/*
 * This defines a collection of descriptors in a
 * single type of memory (e.g. ROM descriptors).
 */
typedef struct dset_map_s {
    int                 map_count;
    dset_map_entry_t    map_entry[MAX_NUM_DSET];
} dset_map_t;

/* Original DataSet Map, from the mapin file */
dset_map_t oldmap;

/* New DataSet Map created by this tool in the mapout file */
dset_map_t newmap;

/* Holds each line of an input file while it is processed. */
char line_buf[MAX_LINE_BUF+1];
int cursor;
#define is_line_empty() (line_buf[cursor] == '\0')
#define is_white_space() \
        (line_buf[cursor] ==' ' || \
         line_buf[cursor] == '\t' || \
         line_buf[cursor] == '\n')

/*
 * Line number of input file that is currently being processed.
 * Input files are processed sequentially, and line_num is reset
 * as needed.
 */
int line_num;

char *prog_name;        /* This tool's name, for error msgs */

char command[1024];     /* This tool's cmd line, which is   */
                        /* echo'ed into the mapout file     */

char cmd_args[] = 
"\
  --desc=<DataSet description filename>\n\
  --out=<DataSet output filename>\n\
  --start=target address  OR  --end=target address\n\
  --mapin=<DataSet map input filename>\n\
  --mapout=<DataSet map output filename>\n\
  --pad=<nbytes> (add 0 padding at tail)\n\
  --verbose\n\
\n";

#define ERROR(args...)                          \
do {                                            \
    fprintf(stderr, "ERROR %s: ", prog_name);   \
    fprintf(stderr, args);                      \
    exit(1);                                    \
} while (0)

#define WARNING(args...)                        \
do {                                            \
    fprintf(stderr, "WARNING %s: ", prog_name); \
    fprintf(stderr, args);                      \
} while (0)

void
usage(void)
{
    fprintf(stderr, "%s arguments\n", prog_name);
    fprintf(stderr, "%s\n", cmd_args);
    exit(1);
}

void
open_output_file(FILE **out_file, char *filename)
{
    FILE *file = fopen(filename, "w+");
    if (!file) {
        ERROR("Cannot create/write output file, %s\n", filename);
    }

    *out_file = file;
}

void
open_input_file(FILE **in_file, char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        ERROR("Cannot open input file, %s for reading.\n", filename);
    }

    *in_file = file;
}


/*
 * Read a line from a file into line_buf, expanding environment variables
 * and convert blank lines and comment lines into empty lines.
 * We do this in a lazy way, by leveraging the echo command to do
 * the work.
 */
int
read_line(FILE *myfile)
{
#define ECHO_CMD "/bin/echo -n "
#define ECHO_SZ (sizeof(ECHO_CMD)-1)
    static char echo_line[ECHO_SZ+MAX_LINE_BUF+1];
    FILE *linein;

    if (line_num == 0) {
        /* Needed first time only */
        strcpy(echo_line, ECHO_CMD);
    }

    line_num++;

    if (!fgets(&echo_line[ECHO_SZ], MAX_LINE_BUF, myfile)) {
        return -1; /* EOF */
    }

#if defined(DEBUG)
    printf("DEBUG: Expanding (%s)\n", echo_line);
#endif
    linein = popen(echo_line, "r");
    memset(line_buf, '\0', MAX_LINE_BUF+1); /* sanity */
    if (!fgets(line_buf, MAX_LINE_BUF, linein)) {
        line_buf[0]='\0';
    }
    pclose(linein);

    cursor = 0;

    return 0; /* A line was read */
}

char *current_filename;

void
change_input_file(char *filename)
{
    current_filename = filename;
    line_num = 0;
}

/*
 * Skip over blanks and tabs in the line buffer.
 * After skipping, if more data was expected and
 * no more data is available in the line buffer
 * then print an error message and abort.
 */
void
skip_blanks(int more_expected)
{
    while (is_white_space()) {
        cursor++;
    }

    if (more_expected && is_line_empty()) {
        ERROR("Missing information in file %s, line %d\n",
                current_filename, line_num);
    }
}

/*
 * Parse a non-comment line of the DataSet Description file.
 * This function decomposes information from line_buf
 * and returns what it found as "out" parameters.
 *
 * Return value is 0 on success, or -1 for blank lines and
 * comment lines.
 */
int
parse_desc_line(A_UINT16 *pdsetid,
                int *pdset_flags,
                char **pfilename)
{
    char *remainder;
    int new_cursor;
    A_UINT16 dsetid;
    int dset_flags;
    char *dset_filename;

#if defined(DEBUG)
    printf("Parse DataSet description file line: %s\n", line_buf);
#endif
    skip_blanks(0);
    if (is_line_empty()) {
        return -1; /* blank line or comment line */
    }

    /* Read dsetid from a line in the DataSet Description file */
    {
        dsetid = strtoul(&line_buf[cursor], &remainder, 0);
        new_cursor = remainder - line_buf;
        if (cursor == new_cursor) {
            ERROR("Could not determine DataSet ID in %s, line %d: '%s'\n",
                    desc_filename, line_num, &line_buf[cursor]);
        } else {
            cursor = new_cursor;
        }
        skip_blanks(1);
    }

    /* Read dset_filename */
    {
        dset_filename = &line_buf[cursor];
        while (!is_white_space() && !is_line_empty()) {
            cursor++;
        }
        line_buf[cursor] = '\0'; /* null-terminate filename */
        cursor++;
        skip_blanks(0);
    } 

    /* Read dset_flags */
    dset_flags = 0;
    while (!is_line_empty()) {
        switch (line_buf[cursor]) {
            case 's':
            case 'S':
                dset_flags |= FLAG_STANDARD;
                break;
            case 'b':
            case 'B':
                dset_flags |= FLAG_BPATCH;
                break;
            case 'c':
            case 'C':
                dset_flags |= FLAG_COMPRESS;
                break;
            default:
                ERROR("Unknown DataSet Type in %s, line %d: '%s'\n",
                        desc_filename, line_num, line_buf);
                break;
        }
        while (!is_white_space() && !is_line_empty()) {
            cursor++;
        }
        skip_blanks(0);
    }

    /*
     * If no flags or just compression were specified,
     * assume FLAG_STANDARD.
     */
    if (!(dset_flags & FLAG_BPATCH)) {
        dset_flags |= FLAG_STANDARD;
    }

    /* Sanity check flag combinations */
    if ((dset_flags & FLAG_STANDARD) &&
        (dset_flags & FLAG_BPATCH))
    {
        ERROR("Bad combination of flags in %s, line %d\n",
                desc_filename, line_num);
    }


    *pdsetid = dsetid;
    *pdset_flags = dset_flags;
    *pfilename = (void *)dset_filename;

    return 0;
}

/* Read FIELD from a line in the DataSet Map file */
#define READ_FIELD_FROM_MAPFILE(addr, FIELD, fieldtype, more)           \
{                                                                       \
    *(addr) = (fieldtype)strtoul(&line_buf[cursor], &remainder, 0);     \
    new_cursor = remainder - line_buf;                                  \
    if (cursor == new_cursor) {                                         \
        ERROR("Could not read '%s' from %s, line %d: '%s'\n",           \
                "FIELD", oldmap_filename, line_num, &line_buf[cursor]); \
    } else {                                                            \
        cursor = new_cursor;                                            \
    }                                                                   \
                                                                        \
    skip_blanks(more);                                                  \
}

/*
 * Parse a non-comment line of a DataSet Map file.
 * This function decomposes information from line_buf
 * and returns what it found in map_entry.
 * Format:  DescriptorAddress dsetid size DataPtr type AuxPtr NextDesc
 */
int
parse_map_line(dset_map_entry_t *map_entry)
{
    char *remainder;
    int new_cursor;
    A_UINT32 junk;

    skip_blanks(0);
    if (is_line_empty()) {
        return -1; /* blank line or comment line */
    }

    READ_FIELD_FROM_MAPFILE(&map_entry->targaddr, DescriptorAddress, A_UINT32, 1);
    READ_FIELD_FROM_MAPFILE(&map_entry->desc.id, dsetid, A_UINT16, 1);
    READ_FIELD_FROM_MAPFILE(&map_entry->desc.size, size, A_UINT16, 1);
    READ_FIELD_FROM_MAPFILE(&map_entry->desc.DataPtr, DataPtr, void *, 1);
    READ_FIELD_FROM_MAPFILE(&map_entry->desc.data_type, type, int, 1);
    READ_FIELD_FROM_MAPFILE(&map_entry->desc.AuxPtr, AuxPtr, void *, 1);
    READ_FIELD_FROM_MAPFILE(&junk, next, A_UINT32, 0);

    if (!is_line_empty()) {
        ERROR("Unexpected characters detected at end of %s, line %d: '%s'\n",
                oldmap_filename, line_num, &line_buf[cursor]);
    }

    return 0;
}

/*
 * Convert an in-core address to an appropriate address
 * in Target memory.
 */
A_UINT32
TO_MEM(void *addr)
{
    int offset;
    A_UINT32 memaddr;

    if ((addr < (void *)dsetimg_start) ||
        (addr >= (void *)dsetimg_end))
    {
        ERROR("Internal error -- TO_MEM address out of range (%p)\n", addr);
    }

    if (start_memaddr) {
        offset = (char *)addr - (char *)dsetimg_start;
        memaddr = start_memaddr + offset;
    } else /* end_memaddr */ {
        offset = (char *)dsetimg_end - (char *)addr;
        memaddr = end_memaddr - offset;
    }

    return memaddr;
}

void
parse_oldmap_file(void)
{
    int rv;

    if (!oldmap_file) {
        return;
    }

#if defined(DEBUG)
    printf("parse oldmap file\n");
#endif

    if (verbose) {
        printf("Read mapin file, '%s'\n", oldmap_filename);
    }

    change_input_file(oldmap_filename);

    for (;;) {
        rv = read_line(oldmap_file);
        if (rv) {
            break;
        }
        rv = parse_map_line(&oldmap.map_entry[oldmap.map_count]);
        if (rv) {
            continue;
        }

#if defined(DEBUG)
        printf("DEBUG: oldmap: dsetid=%d targaddr=0x%x\n",
                oldmap.map_entry[oldmap.map_count].desc.id,
                oldmap.map_entry[oldmap.map_count].targaddr);
#endif

        oldmap.map_count++;
    }

    if (oldmap.map_count == 0) {
        ERROR("No DataSets specified in %s.\n", oldmap_filename);
    }
}

/*
 * Search a descriptor map for a specified dsetid and sets
 * ptargaddr to the corresponding descriptor address in
 * target memory, if found.  Returns 1 if an entry is found
 * and 0 if an entry is not found.
 */
int
search_map(dset_map_t *map, A_UINT16 dsetid, A_UINT32 *ptargaddr)
{
    int i;
    dset_map_entry_t *map_entry;

    for (i=0; i<map->map_count; i++) {
        map_entry = &map->map_entry[i];

#if defined(DEBUG)
        printf("DEBUG: search compare %d with %d\n", map_entry->desc.id, dsetid);
#endif
        if (map_entry->desc.id == dsetid) {
#if defined(DEBUG)
        printf("DEBUG: search for %d found 0x%x\n", dsetid, map_entry->targaddr);
#endif
            if (ptargaddr) {
                if (map_entry->targaddr == 0) {
                    ERROR("Probable internal error -- search for targaddr too early\n");
                }
                *ptargaddr = map_entry->targaddr;
            }
            return 1;
        }
    }

#if defined(DEBUG)
    printf("DEBUG: search for %d failed\n", dsetid);
#endif
    return 0;
}

/*
 * Determine the size of (uncompressed) BPatched DataSet.
 * The bdiff tool appends this information to the end of
 * the BPatch data (so that we don't need to have the
 * unpatched data and actually apply the patch in order
 * to know how big the resulting data is).
 */
A_UINT16
patched_dset_size(char *dset_data, int dsetsz)
{
    A_UINT16 patched_sz;

    if (dsetsz < 2) {
        ERROR("In patched_dset_size, dsetsz is too small (%d)\n", dsetsz);
    }

    patched_sz = (((unsigned char)(dset_data[dsetsz-1]) << 8) +
                  ((unsigned char)(dset_data[dsetsz-2]) << 0));

    return patched_sz;
}

/*
 * Add the specified DataSet data to the growing in-core image.
 * Store metadata (dset_descriptor_t) in newmap for later inclusion
 * in the in-core image.
 */
void
add_dset_to_image(A_UINT16 dsetid, int dset_flags, char *dset_filename)
{
    int dset_fd;
    int orig_dsetsz, dsetsz, patched_dsetsz, rounded_dsetsz;
    dset_descriptor_t *desc;
    char *buffer;
    static char compress_buffer[MAX_DSETIMG_SZ+(MAX_DSETIMG_SZ>>4)];
    static char file_buffer[MAX_DSETIMG_SZ];

    struct stat filestat;

    if ((dset_fd = open(dset_filename, O_RDONLY)) < 0) {
        ERROR("Cannot open DataSet: %s.\n", dset_filename);
    }

    if (fstat(dset_fd, &filestat) < 0) {
        ERROR("Cannot determine length of DataSet: %s.\n", dset_filename);
    }

    orig_dsetsz = filestat.st_size;

    if (orig_dsetsz > MAX_DSETIMG_SZ) {
        ERROR("DataSet (%s) too large (%d)\n", dset_filename, orig_dsetsz);
    }

    if (orig_dsetsz == 0) {
        WARNING("Empty DataSet, %s\n", dset_filename);
    }

    if (orig_dsetsz && (read(dset_fd, file_buffer, orig_dsetsz) != orig_dsetsz)) {
        ERROR("Failed to read %d bytes from %s\n", orig_dsetsz, dset_filename);
    }

    /*
     * Print a warning if we're adding the same dsetid twice
     * within a given memory type.  That's not strictly illegal;
     * but it's a strange thing to do unless we're testing.
     */
    {
        if (search_map(&newmap, dsetid, NULL)) {
            WARNING("Adding dsetid (%d) multiple times.\n", dsetid);
        }
    } 

    if (dset_flags & FLAG_BPATCH) {
        /* Extract and strip off trailing 2 bytes of "patched size" info */
        patched_dsetsz = patched_dset_size(file_buffer, orig_dsetsz);
        orig_dsetsz -= 2;
    }

    /* If compression was requested, deal with that now */
    if ((dset_flags & FLAG_COMPRESS) &&
        (orig_dsetsz != 0))
    {
        int compress_ratio;

        dsetsz = LZ_Compress((unsigned char *)file_buffer, (unsigned char *)compress_buffer, orig_dsetsz);
        compress_ratio = (dsetsz*100)/orig_dsetsz;
        if (verbose) {
            printf("Compress ratio %d%% (from %d to %d): %s\n",
                    compress_ratio, orig_dsetsz, dsetsz, dset_filename);
        }

        /*
         * If the file does not compress well, then ignore
         * the request for compression.
         */
        if (compress_ratio > 90) {
            buffer = file_buffer;
            dsetsz = orig_dsetsz;

            dset_flags &= ~FLAG_COMPRESS;
            if (!(dset_flags & FLAG_BPATCH)) {
                dset_flags |= FLAG_STANDARD;
            }

            if (verbose) {
                printf("SKIP compression on %s\n", dset_filename);
            }
        } else {
            buffer = compress_buffer;
        }
    } else {
        dsetsz = orig_dsetsz;
        buffer = file_buffer;
    }

    /* 
     * This is the space actually consumed for data,
     * possibly compressed, in the image.
     */
    rounded_dsetsz = ((dsetsz+3) / 4) * 4;

    nbytes_remaining -= (rounded_dsetsz + sizeof(dset_descriptor_t));
    if (nbytes_remaining < 0) {
        ERROR("Remaining space (%d) insufficient to add DataSet %s\n",
                nbytes_remaining, dset_filename);
    }

    dsetimg_start -= rounded_dsetsz;

    /*
     * Copy data into in-core image.  If this is a compressed
     * DataSet, we copy the compressed image.
     */
    memcpy(dsetimg_start, buffer, dsetsz);

    /* Allocate a new descriptor */
    desc = &newmap.map_entry[newmap.map_count].desc;
    newmap.map_count++;

    /*
     * We won't know addresses in Target memory until after
     * the entire Dataset Description file has been parsed
     * and all data has been placed.  At that time, we add
     * descriptors to the image, set the next field, fix
     * the DataPtr field, and set the AuxPtr field or BPatch
     * descriptors.
     */
    desc->id = dsetid;
    if (dset_flags & FLAG_BPATCH) {
        dset_descriptor_t *bdiff_desc; /* bdiff data */

        if (!search_map(&oldmap, dsetid, (A_UINT32 *)&desc->DataPtr)) {
            ERROR("Attempt to BPatch non-existent DataSet (%d) in %s\n",
                    dsetid, desc_filename);
        }
        desc->data_type = DSET_TYPE_BPATCHED;
        desc->size = patched_dsetsz;
        /* desc->AuxPtr is backpatched after descriptors are placed. */

        /*
         * Add a second dset descriptor for this line of the
         * dset description file.  The original descriptor, desc,
         * is a "bpatch" descriptor which points to the original
         * descriptor via DataPtr and to the bdiff descriptor
         * via AuxPtr.  The new bdiff descriptor points to the
         * bdiff data via DataPtr.  If the bdiff data may be
         * compressed, in which case it's handled just like any
         * other compressed DataSet.
         */
        bdiff_desc = &newmap.map_entry[newmap.map_count].desc;
        newmap.map_count++;

        bdiff_desc->id = DSETID_UNUSED;
        bdiff_desc->DataPtr = (void *)dsetimg_start; /* adjust later */

        if (dset_flags & FLAG_COMPRESS) {
            bdiff_desc->data_type = DSET_TYPE_COMPRESSED;
            bdiff_desc->size = orig_dsetsz;
            desc->AuxPtr = (void *)dsetsz;
        } else {
            bdiff_desc->data_type = DSET_TYPE_STANDARD;
            bdiff_desc->size = dsetsz;
        }
    } else if (dset_flags & FLAG_COMPRESS) {
            desc->data_type = DSET_TYPE_COMPRESSED;
            desc->DataPtr = (void *)dsetimg_start; /* adjust later */
            desc->size = orig_dsetsz;
            desc->AuxPtr = (void *)dsetsz;
    } else {
            desc->data_type = DSET_TYPE_STANDARD;
            desc->size = dsetsz;
            desc->DataPtr = (void *)dsetimg_start; /* adjust later */
    }
}

/*
 * Parse the DataSet Description file (e.g. dsets.rom.*.txt).
 *
 * Skip over comment lines and blank line, decompose
 * lines with data on them into various components, and
 * cause the resulting DataSets to be included in the
 * in-core DataSet memory image.
 */
void
parse_dset_desc_file(void)
{
    int rv;
    A_UINT16 dsetid;
    char *dset_filename;
    int dset_flags;
    dset_descriptor_t *dset_metadata;
    int i;

    /*
     * Initialize in-core start, end, and nbytes_remaining.
     * We reserve the top word to point to the start of the
     * first descriptor.  This makes it easy for Target
     * software to locate ROM/flash DataSets, since this
     * final word is located at ROM_DATASET_INDEX_ADDR or
     * at FLASH_DATASET_INDEX_ADDR.  It also makes it easy
     * to see where in Target memory a DataSet image should
     * be loaded.
     */
    dsetimg_end    = &dsetimg_space[MAX_DSETIMG_SZ];
    dsetimg_start  = dsetimg_end - sizeof(A_UINT32);
    nbytes_remaining = MAX_DSETIMG_SZ - sizeof(A_UINT32);

#if defined(DEBUG)
    printf("Parse dset desc_file\n");
#endif

    change_input_file(desc_filename);

    for (;;) {
        rv = read_line(desc_file);
        if (rv) {
            break;
        }
        rv = parse_desc_line(&dsetid, &dset_flags, &dset_filename);
        if (rv) {
            continue;
        }

#if defined(DEBUG)
        printf("DEBUG: newmap: dsetid=%d type=%d filename=%s",
            dsetid, dset_flags, dset_filename);
#endif

        /*
         * This adds data -- but not metadata -- to the
         * incore image.  Metadata is stored in newmap.
         */
        add_dset_to_image(dsetid, dset_flags, dset_filename);
    }

    if (newmap.map_count == 0) {
        ERROR("No DataSets specified in %s.\n", desc_filename);
    }

    /*
     * Now that we know how much metadata there is, we can determine
     * where each descriptor should reside and fix descriptor linkage
     * and other Target memory pointers.
     */
    {
        int metadata_sz = sizeof(dset_descriptor_t) * newmap.map_count;

        /* Place metadata just before data */
        dsetimg_start -= metadata_sz;
        dset_metadata = (dset_descriptor_t *)dsetimg_start;

#if defined(DEBUG)
        printf("Fix descriptor linkage.\n");
#endif

        for (i=0; i<newmap.map_count; i++) {
            newmap.map_entry[i].targaddr = TO_MEM(&dset_metadata[i]);
            newmap.map_entry[i].desc.next =
                ((i == newmap.map_count-1) ?
                    (struct dset_descriptor_s *)oldmap.map_entry[0].targaddr :
                    (struct dset_descriptor_s *)TO_MEM(&(dset_metadata[i+1])));
                    /* Note: We could cause linkage to skip over bdiff dsets,
                       since we never need to search for them. */
        }
    }

    /*
     * Now that descriptors are assigned fixed locations in Target
     * memory, we can backpatch the AuxPtr field for BPatch DataSets
     * and fix DataPtr fields for non-BPatch Datasets.
     *
     * NB: Due to the way the image is constructed, we know that the
     * bdiff descriptor immediately follows the BPatch descriptor.
     */
    {
#if defined(DEBUG)
        printf("DEBUG: Backpatch BPatch AuxPtr\n");
#endif
        for (i=0; i<newmap.map_count; i++) {
            if (newmap.map_entry[i].desc.data_type == DSET_TYPE_BPATCHED) {
                newmap.map_entry[i].desc.AuxPtr = (void *)(newmap.map_entry[i+1].targaddr);
            } else {
                /* Adjust DataPtr to make it Target-Memory relative */
                newmap.map_entry[i].desc.DataPtr =
                    (void *)TO_MEM(newmap.map_entry[i].desc.DataPtr);
            }
        }
    }

    /* Copy completed descriptors to in-core image. */
    for (i=0; i<newmap.map_count; i++) {
        memcpy(&dset_metadata[i], &newmap.map_entry[i].desc, sizeof(dset_descriptor_t));
    }

    /*
     * Store Target address of first descriptor in the last word
     * of the image (e.g. ROM_DATASET_INDEX_ADDR).
     */
    ((A_UINT32 *)dsetimg_end)[-1] = newmap.map_entry[0].targaddr;
}

/* Print a map entry to the mapout file */
#define PRINT_TO_MAPOUT(map_entry)                              \
        fprintf(newmap_file, "0x%x %d %d 0x%x %d 0x%x 0x%x\n",  \
                (A_UINT32)((map_entry)->targaddr),              \
                (map_entry)->desc.id,                           \
                (map_entry)->desc.size,                         \
                (A_UINT32)((map_entry)->desc.DataPtr),          \
                (map_entry)->desc.data_type,                    \
                (A_UINT32)((map_entry)->desc.AuxPtr),           \
                (A_UINT32)((map_entry)->desc.next));

/*
 * Commit the new map information along with old map information to
 * the mapout file.
 */
void
write_map_to_file(void)
{
    int i;

    if (!newmap_file) {
        return;
    }

    if (verbose) {
        printf("Write mapout file, '%s'\n", newmap_filename);
    }

    fprintf(newmap_file, "# %s\n", command);
    fprintf(newmap_file, "# Load address: 0x%x\n", TO_MEM(dsetimg_start));

    for (i=0; i<newmap.map_count; i++) {
        PRINT_TO_MAPOUT(&newmap.map_entry[i]);
    }

    for (i=0; i<oldmap.map_count; i++) {
        PRINT_TO_MAPOUT(&oldmap.map_entry[i]);
    }
}

/*
 * Push the in-core DataSet image to the output file.
 * This is called one time once the in-core DataSet image
 * is completely finished.
 */
void
write_dsetimg_to_outfile(void)
{
    if (verbose) {
        printf("Using descriptor file '%s'\n", desc_filename);
        printf("Write %d bytes to DataSet image file '%s'\n",
                dsetimg_end - dsetimg_start, out_filename);
    }

    if (dsetimg_start != dsetimg_end) {
        if (!fwrite(dsetimg_start, 1, dsetimg_end - dsetimg_start, out_file)) {
            ERROR("Cannot write DataSet image to output file (%s)\n",
                        out_filename);
        }
    }
}

/* Add tailpad bytes of padding at the end of the output file. */
void
pad_output_file(void)
{
    int remaining = tailpad;

    while (remaining) {
        if (!fwrite(&zeroes, 1, 4, out_file)) {
            ERROR("Cannot zero-pad (%d) output file (%s)\n",
                remaining, out_filename);
        }
        remaining -= 4;
    }

    if (verbose) {
        printf("Zero pad at end: %d bytes\n", tailpad);
    }
}

/* Process command-line arguments and do basic sanity checks. */
void
scan_args(int argc, char **argv)
{
    int desc_specified = 0;
    int out_specified = 0;
    int oldmap_specified = 0;
    int newmap_specified = 0;
    int start_memaddr_specified = 0;
    int end_memaddr_specified = 0;
    int c;
    int i;

    /* Save the command that was used for inclusion in the mapout file */
    for (i=0; i<argc; i++) {
        strcat(command, argv[i]);
        strcat(command, " ");
    }
    
#if defined(DEBUG)
    printf("scan arguments\n");
#endif
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"desc", 1, NULL, 'd'},
            {"out", 1, NULL, 'o'},
            {"mapin", 1, NULL, 'I'},
            {"mapout", 1, NULL, 'O'},
            {"end", 1, NULL, 'e'},
            {"start", 1, NULL, 's'},
            {"pad", 1, NULL, 'p'},
            {"verbose", 0, NULL, 'v'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "d:o:I:O:e:s:p:v",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd': /* description file */
            strncpy(desc_filename, optarg, MAX_FILENAME);
            desc_specified = 1;
            break;

        case 'o': /* output file */
            strncpy(out_filename, optarg, MAX_FILENAME);
            out_specified = 1;
            break;

        case 'I': /* oldmap input file */
            strncpy(oldmap_filename, optarg, MAX_FILENAME);
            oldmap_specified = 1;
            break;

        case 'O': /* newmap output file */
            strncpy(newmap_filename, optarg, MAX_FILENAME);
            newmap_specified = 1;
            break;

        case 'e': /* end address */
            end_memaddr = strtoul(optarg, NULL, 0);
            end_memaddr_specified = 1;
            break;

        case 's': /* start address */
            start_memaddr = strtoul(optarg, NULL, 0);
            start_memaddr_specified = 1;
            break;

        case 'p':
            tailpad = strtoul(optarg, NULL, 0);
            if (tailpad % 4) {
                ERROR("padding (%d) should be a multiple of 4 bytes\n", tailpad);
            }
            break;

        case 'v': /* verbose */
            verbose = TRUE;
            break;

        default:
            usage();
        }
    }

    if (!desc_specified ||
        !out_specified ||
        (!start_memaddr_specified && !end_memaddr_specified))
    {
        fprintf(stderr, "%s: Missing required arguments\n", prog_name);
        usage();
    }

    if (start_memaddr_specified && end_memaddr_specified) {
        ERROR("Conflicting arguments -- specify start OR end, not both\n");
    }

    if ((start_memaddr & 0x3) ||
        (end_memaddr & 0x3))
    {
        ERROR("Start/End address must be 4-byte aligned\n");
    }

#if defined(DEBUG)
    printf("DEBUG: desc=%s out=%s end_memaddr=0x%x\n", 
        desc_filename, out_filename, end_memaddr);
#endif

    /* Verify that we can open specified files. */

    open_input_file(&desc_file, desc_filename);

    open_output_file(&out_file, out_filename);

    if (oldmap_specified) {
        open_input_file(&oldmap_file, oldmap_filename);
    }

    if (newmap_specified) {
        open_output_file(&newmap_file, newmap_filename);
    }
}

int
main(int argc, char *argv[], char *arge[])
{
    prog_name = basename(argv[0]);

    scan_args(argc, argv);
    parse_oldmap_file();
    parse_dset_desc_file();
    write_dsetimg_to_outfile();
    write_map_to_file();
    pad_output_file();

    printf("Load address: 0x%x\n", TO_MEM(dsetimg_start));

    exit(0); /* success */
}
