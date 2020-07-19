/*
* Copyright (c) 2012, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2012, Qualcomm Atheros Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ADF_OS_PERF_H
#define __ADF_OS_PERF_H

#ifdef QCA_PERF_PROFILING 

#if (QCA_MIPS74K_PERF_PROFILING || QCA_MIPS24KK_PERF_PROFILING)
#include <adf_os_mips_perf_pvt.h>
#endif

 
/* Structures */

/* #defines required for structures */
#define MAX_SAMPLES_SHIFT   5   /* change this only*/
#define MAX_SAMPLES         (1 << MAX_SAMPLES_SHIFT)
#define INC_SAMPLES(x)      ((x + 1) & (MAX_SAMPLES - 1))
#define MAX_SAMPLE_SZ       (sizeof(uint32_t) * MAX_SAMPLES)
#define PER_SAMPLE_SZ       sizeof(uint32_t)

typedef struct adf_os_perf_entry {
    struct list_head            list;   /**< Next */
    struct list_head            child;  /**< Child */ 

    struct adf_os_perf_entry   *parent; /**< Top */

    adf_os_perf_cntr_t          type;
    uint8_t                    *name;

    struct proc_dir_entry      *proc;

    uint64_t                    start_tsc[MAX_SAMPLES];
    uint64_t                    end_tsc[MAX_SAMPLES];

    uint32_t                    samples[MAX_SAMPLES];
    uint32_t                    sample_idx;

    spinlock_t                  lock_irq;

} adf_os_perf_entry_t;

/* Typedefs */
typedef __adf_os_perf_id_t  adf_os_perf_id_t;

typedef int     ( *proc_read_t)(char *page, char **start, off_t off, int count, 
	                        int *eof, void *data);
typedef int     ( *proc_write_t)(struct file *file, const char *buf, 
                             unsigned long count, void *data);
typedef void    ( *perf_sample_t)(struct adf_os_perf_entry  *entry, 
                                  uint8_t  done);

typedef void    ( *perf_init_t)(struct adf_os_perf_entry   *entry, 
                                uint32_t                    def_val);

/* Structure using typedefs, hence after typedefs */
typedef struct proc_api_tbl {
    proc_read_t     proc_read;
    proc_write_t    proc_write;
    perf_sample_t   sample;
    perf_init_t     init;
    uint32_t        def_val;
} proc_api_tbl_t;

/* Macros */
#define INIT_API(name, val)    {   \
    .proc_read  = read_##name,     \
    .proc_write = write_##name,    \
    .sample     = sample_event,    \
    .init       = init_##name,     \
    .def_val    = val,             \
}

#define PERF_ENTRY(hdl)     (adf_os_perf_entry_t *)hdl

#define adf_os_perf_init(_parent, _id, _ctr_type)   \
    __adf_os_perf_init((_parent), (_id), (_ctr_type))

#define adf_os_perf_destroy(_id)    __adf_os_perf_destroy((_id))

#define adf_os_perf_start(_id)      __adf_os_perf_start((_id))

#define adf_os_perf_end(_id)        __adf_os_perf_end((_id))

/* Extern declarations */
extern int
adf_os_perfmod_init(void);
extern void
adf_os_perfmod_exit(void);

#else /* !QCA_PERF_PROFILING */

#define adf_os_perfmod_init()
#define adf_os_perfmod_exit()
#define DECLARE_N_EXPORT_PERF_CNTR(id)
#define START_PERF_CNTR(_id, _name)
#define END_PERF_CNTR(_id) 

#endif /* QCA_PERF_PROFILING */


#endif  /* __ADF_OS_PERF_H */
