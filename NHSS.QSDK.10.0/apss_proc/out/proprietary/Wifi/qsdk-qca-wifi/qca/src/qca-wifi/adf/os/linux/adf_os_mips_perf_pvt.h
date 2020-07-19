#ifndef __ADF_OS_MIPS_PERF_PVT_H
#define __ADF_OS_MIPS_PERF_PVT_H

#include <adf_os_types.h>

#define PROCFS_PERF_DIRNAME "mips_perf"

/** 
 * @brief Perf counter entry type
 */
typedef enum adf_os_perf_cntr {
    /* CPU specific counters */

    /* MIPS 24K Counters, pb9x */
    CNTR_MIPS24K_CPU,   /**< Count CPU Cycles */
    CNTR_MIPS24K_PERF0, /**< Count Perf0 events */
    CNTR_MIPS24K_PERF1, /**< Count Perf1 events */
    CNTR_MIPS24K_MAX,
    
    /* MIPS 74K Counters, db12x */
    CNTR_MIPS74K_CPU,   /**< Count CPU Cycles */
    CNTR_MIPS74K_PERF0, /**< Count Perf0 events */
    CNTR_MIPS74K_PERF1, /**< Count Perf1 events */
    CNTR_MIPS74K_PERF2, /**< Count Perf2 events */
    CNTR_MIPS74K_PERF3, /**< Count Perf3 events */
    CNTR_MIPS74K_MAX,

    /* Generic counters */
    CNTR_GROUP,         /**< Counter group*/
    CNTR_CHKPOINT,      /**< Count checkpoint */

    CNTR_LAST,          /**< Counter Last */
} adf_os_perf_cntr_t;

/* typedefs */
typedef void *  __adf_os_perf_id_t;

/* Extern declarations */
extern __adf_os_perf_id_t   __adf_os_perf_init(__adf_os_perf_id_t    parent, 
                                               uint8_t     *id_name, 
                                               uint32_t   type);
extern a_bool_t             __adf_os_perf_destroy(__adf_os_perf_id_t     id);

extern void     __adf_os_perf_start(__adf_os_perf_id_t       id);
extern void     __adf_os_perf_end(__adf_os_perf_id_t         id);

/* Macros */
    
#define destroy_perf_cntr(id, cpu)   ({      \
    int __cnt__ = CNTR_##cpu##_CPU;    \
    for (; __cnt__ < CNTR_##cpu##_MAX; __cnt__++)            \
        __adf_os_perf_destroy(perf_##id[(__cnt__ - CNTR_##cpu##_CPU)]);    \
})


#define start_perf_cntr(id, cpu) ({       \
    int __cnt__ = CNTR_##cpu##_CPU;    \
    for (; __cnt__ < CNTR_##cpu##_MAX; __cnt__++)            \
        __adf_os_perf_start(perf_##id[(__cnt__ - CNTR_##cpu##_CPU)]);    \
})

#define end_perf_cntr(id, cpu) ({       \
    int __cnt__ = CNTR_##cpu##_CPU;    \
    for (; __cnt__ < CNTR_##cpu##_MAX; __cnt__++)            \
        __adf_os_perf_end(perf_##id[(__cnt__ - CNTR_##cpu##_CPU)]);    \
})



#if defined QCA_MIPS74K_PERF_PROFILING  /* MIPS74K */

#define init_mips74k_perf(id, name)   ({    \
    root_##id = __adf_os_perf_init(0, #name, CNTR_GROUP);    \
    perf_##id[0]  = __adf_os_perf_init(root_##id, "cpu", CNTR_MIPS74K_CPU); \
    perf_##id[1]  = __adf_os_perf_init(root_##id, "perf0", CNTR_MIPS74K_PERF0); \
    perf_##id[2]  = __adf_os_perf_init(root_##id, "perf1", CNTR_MIPS74K_PERF1); \
    perf_##id[3]  = __adf_os_perf_init(root_##id, "perf2", CNTR_MIPS74K_PERF2); \
    perf_##id[4]  = __adf_os_perf_init(root_##id, "perf3", CNTR_MIPS74K_PERF3); \
})

#define destroy_mips74k_perf(id)        destroy_perf_cntr(id, MIPS74K)

#define START_MIPS74K_PERF_CNTR(_id, _name)                 \
{                                                           \
    static int _id##_perf_init = 0;                         \
    if (unlikely(!_id##_perf_init++)) {                     \
        printk("Initializing Perf Counters for %s *********\n", __func__);  \
        init_mips74k_perf(_id, _name);                     \
    }                                                       \
    start_perf_cntr(_id, MIPS74K);                          \
}

#define END_MIPS74K_PERF_CNTR(_id)                           \
    end_perf_cntr(_id, MIPS74K);

#define DECLARE_N_EXPORT_PERF_CNTR(id)      \
    __adf_os_perf_id_t  root_##id   = NULL, \
                        perf_##id[CNTR_MIPS74K_MAX - CNTR_MIPS74K_CPU] = {0}; \
    EXPORT_SYMBOL(perf_##id);

#define START_PERF_CNTR START_MIPS74K_PERF_CNTR

#define END_PERF_CNTR END_MIPS74K_PERF_CNTR

#elif defined QCA_MIPS24K_PERF_PROFILING    /* MIPS24K */

#define init_mips24k_perf(id, name)   ({    \
    root_##id = __adf_os_perf_init(0, #name, CNTR_GROUP);    \
    perf_##id[0]  = __adf_os_perf_init(root_##id, "cpu", CNTR_MIPS24K_CPU); \
    perf_##id[1]  = __adf_os_perf_init(root_##id, "perf0", CNTR_MIPS24K_PERF0); \
    perf_##id[2]  = __adf_os_perf_init(root_##id, "perf1", CNTR_MIPS24K_PERF1); \
})

#define destroy_mips24k_perf(id)        destroy_perf_cntr(id, MIPS24K)

#define DECLARE_N_EXPORT_PERF_CNTR(id)      \
    __adf_os_perf_id_t  root_##id   = NULL, \
                        perf_##id[CNTR_MIPS24K_MAX - CNTR_MIPS24K_CPU] = {0}; \
    EXPORT_SYMBOL(perf_##id);

#define START_PERF_CNTR START_MIPS24K_PERF_CNTR

#define END_PERF_CNTR END_MIPS24K_PERF_CNTR

#else

#error "CONFIGURATION ERROR :   Either QCA_MIPS74K_PERF_PROFILING or    \
                                QCA_MIPS24K_PERF_PROFILING  should be defined"

#endif

#endif  /* __ADF_OS_MIPS_PERF_PVT_H */
