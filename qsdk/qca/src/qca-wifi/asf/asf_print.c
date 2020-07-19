/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include <stdarg.h>       /* va_list */

#include "asf_print.h"    /* asf_print_ctrl, etc. */

#include "qdf_mem.h"   /* qdf_mem_malloc, qdf_mem_free */
#include "qdf_str.h"   /* qdf_str_cmp */

#ifndef NULL
#define NULL ((void *) 0) 
#endif

/* ASF_PRINT_SUPPORT -
 * Control how many of the asf_print features are included in the compiled SW.
 * ASF_PRINT_SUPPORT == 1 -> support regular print filtering
 * ASF_PRINT_SUPPORT == 2 -> also support extended bit name matching methods
 *                           (in asf_print_mask_set_by_bit_name)
 */
#ifndef ASF_PRINT_SUPPORT
#define ASF_PRINT_SUPPORT 2
#endif

/* ASF_PRINT_VALID_MAGIC_NUM -
 * Special pattern that in-use asf_print_instance objects are marked with
 * to indicate that they are valid.
 */
#define ASF_PRINT_VALID_MAGIC_NUM 0xC001Babe
#define ASF_PRINT_IS_VALID(h) ((h)->valid == ASF_PRINT_VALID_MAGIC_NUM)

/* asf_print_instance -
 * Store a list of print-control objects that are registered with
 * this asf_print_instance, and keep a mutex lock for safe traversal
 * and modification of this list.
 */
struct asf_print_instance {
    int num_list_elems;
    struct asf_print_ctrl *print_ctrl_list;
    unsigned valid;
};

/*
 * Provide a single shared print-control struct for users who
 * don't care to set up their own private print-control struct.
 */
struct asf_print_ctrl AsfPrintCtrlShared = {
    {0},      /* category_mask - all categories disabled */
    0,        /* verb_threshold - lowest verbosity */
    "shared", /* name */
    0,        /* num_bit_specs */
    NULL,     /* bit_specs */
    NULL,     /* next */
    NULL,     /* custom_print */
    NULL,     /* custom_ctxt */
};

/* asf_print_default -
 * Provide a shared default asf_print_instance, for callers who don't
 * want to create and maintain their own asf_print_instance.
 * Put the shared print-control object in this shared asf_print_instance.
 */
static struct asf_print_instance asf_print_default = {
    /* initialize list with shared print-control object */
    1, &AsfPrintCtrlShared,
    ASF_PRINT_VALID_MAGIC_NUM,
};

static asf_vprint_fp vprint = NULL;
static asf_print_lock_fp Lock_func = NULL;
static asf_print_unlock_fp Unlock_func = NULL;
static void *Lock = NULL;

void asf_print_setup(
    asf_vprint_fp vp,
    asf_print_lock_fp lock_func,
    asf_print_unlock_fp unlock_func,
    void *lock)
{
    vprint = vp;
    if (!lock_func || !unlock_func || !lock) {
        Lock_func = NULL;
        Unlock_func = NULL;
        Lock = NULL;
    } else {
        Lock_func = lock_func;
        Unlock_func = unlock_func;
        Lock = lock;
    }
}

#if ASF_PRINT_INLINE
/* category and verbosity filtering has already been done by the cover macro */
#define CAT_VAR
#define CAT_TYPE
#define VERB_VAR
#define VERB_TYPE
#else
/*
 * Category and verbosity filtering still needs to be done,
 * so category and verbosity argument are needed.
 */
#define CAT_VAR category,
#define CAT_TYPE unsigned
#define VERB_VAR verb,
#define VERB_TYPE unsigned
#endif /* ASF_PRINT_INLINE */

#if ASF_PRINT_INLINE
/*
 * If ASF_PRINT_INLINE is not enabled, then asf_vprint_category_private
 * is exported for use by the asf_vprint_category macro, and a prototype
 * for asf_vprint_category_private will be provided by asf_print.h.
 * If ASF_PRINT_INLINE is enabled, then asf_vprint_category_private is not
 * referenced outside this file by macros from asf_print.h, and has no
 * prototype in asf_print.h.  So, declare asf_vprint_category_private
 * as static.
 */
static
#endif
void asf_vprint_category_private(
    struct asf_print_ctrl *handle,
    CAT_TYPE CAT_VAR
    const char *fmt,
    va_list args)
{
    /*
     * Check if print filtering has already been done by a cover macro.
     * If not, do it here.
     */
#if ! ASF_PRINT_INLINE
    int byte, bit;

    if (category >= ASF_PRINT_MASK_BITS) return; /* sanity check */

    byte = category >> 3;
    bit = category - byte * 8;

    if (!handle) handle = &AsfPrintCtrlShared;
    if (!(handle->category_mask[byte] & (1 << bit))) {
        return; /* filter out this print message */
    }
#endif /* ! ASF_PRINT_INLINE */

    if (handle->custom_print) {
        handle->custom_print(handle->custom_ctxt, fmt, args);
    } else if (vprint) {
        vprint(fmt, args);
    }
}

void asf_vprint_private(
    struct asf_print_ctrl *handle,
    CAT_TYPE CAT_VAR
    VERB_TYPE VERB_VAR
    const char *fmt,
    va_list args)
{
    /*
     * Check if print filtering has already been done by a cover macro.
     * If not, do it here.
     */
#if ! ASF_PRINT_INLINE
    if (!handle) handle = &AsfPrintCtrlShared;
    if (verb > handle->verb_threshold) {
        /* filter out this print message */
        return;
    }
#endif /* ! ASF_PRINT_INLINE */
    asf_vprint_category_private(handle, CAT_VAR fmt, args);
}

void asf_print_private(
    struct asf_print_ctrl *handle,
    CAT_TYPE CAT_VAR
    VERB_TYPE VERB_VAR
    const char *fmt,
    ...)
{
    va_list args;

    va_start(args, fmt);
    asf_vprint_private(handle, CAT_VAR VERB_VAR fmt, args);
    va_end(args);
}

#if ! ASF_PRINT_INLINE
void asf_print_category_private(
    struct asf_print_ctrl *handle,
    unsigned category,
    const char *fmt,
    ...)
{
    va_list args;

    va_start(args, fmt);
    asf_vprint_category_private(handle, category, fmt, args);
    va_end(args);
}
#endif /* ! ASF_PRINT_INLINE */

#undef CAT_VAR
#undef CAT_TYPE
#undef VERB_VAR
#undef VERB_TYPE

struct asf_print_instance *asf_print_new(void/*lock*/)
{
    struct asf_print_instance *handle;
    handle = qdf_mem_malloc(sizeof(struct asf_print_instance));
    if (handle) {
        handle->print_ctrl_list = NULL;
        handle->valid = ASF_PRINT_VALID_MAGIC_NUM;
    }
    return handle;
}

int asf_print_destroy(struct asf_print_instance *handle)
{
    if (!handle) return 1; /* fail - invalid handle */
    if (handle->print_ctrl_list) return 1; /* fail - list is not empty */

    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return 1; /* failure - invalid asf_print_instance */
    }
    handle->valid = 0; /* mark this object as being no longer valid */
    qdf_mem_free(handle);
    if (Lock) Unlock_func(Lock);
    return 0; /* success */
}

void asf_print_ctrl_register_private(
    struct asf_print_instance *handle,
    struct asf_print_ctrl *p)
{
    if (!p) return;
    if (!handle) handle = &asf_print_default;

    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return; /* failure - invalid asf_print_instance */
    }
    /* insert the new element at the head of the linked list */
    p->next = handle->print_ctrl_list;
    handle->print_ctrl_list = p;
    handle->num_list_elems++;
    if (Lock) Unlock_func(Lock);
}

int asf_print_ctrl_unregister_private(
    struct asf_print_instance *handle,
    struct asf_print_ctrl *p)
{
    struct asf_print_ctrl *list_elem;
    int status = 0; /* assume success */

    /*
     * Traverse the list, checking if the next element is the specified one.
     * The anchor pointer at the head of the list needs to be handled as a
     * special case.
     */
    if (!p) return 1; /* failure - invalid print control object */
    if (!handle) handle = &asf_print_default;

    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return 1; /* failure - invalid asf_print_instance */
    }

    list_elem = handle->print_ctrl_list;
    if (!list_elem) {
        status = 1; /* failure - list is empty */
        goto done;
    }
    if (list_elem == p) {
        handle->print_ctrl_list = p->next;
        goto success;
    }
    while (list_elem->next) {
        if (list_elem->next == p) {
            list_elem->next = p->next; /* splice out p from the list */
            goto success; /* success */
        }
        list_elem = list_elem->next;
    }
    status = 1; /* failure - print control object not found */
    goto done;
success:
    handle->num_list_elems--;
done:
    if (Lock) Unlock_func(Lock);
    return status;
}

void asf_print_mask_set(struct asf_print_ctrl *handle, int category, int enable)
{
    int byte, bit;

    if (category >= ASF_PRINT_MASK_BITS) return; /* sanity check */

    byte = category >> 3; /* category / 8 */
    bit = category - byte * 8;

    if (enable) {
        handle->category_mask[byte] |= (1 << bit);
    } else {
        handle->category_mask[byte] &= ~(1 << bit);
    }
}

void asf_print_mask_set_by_name_private(
    struct asf_print_instance *handle,
    const char *name,
    int category,
    int enable)
{
    struct asf_print_ctrl *p;

    if (category > ASF_PRINT_MASK_BITS) return; /* sanity check */
    if (!handle) handle = &asf_print_default;
        
    /*
     * set/clear the specified bit for the print-control struct with the
     * matching name, or if no name is specified, for all print-control
     * structs.
     */
    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return;
    }
    p = handle->print_ctrl_list;
    do {
        if (! name || (p->name && ! qdf_str_cmp(name, p->name))) {
            asf_print_mask_set(p, category, enable);
        }
        p = p->next;
    } while (p);
    if (Lock) Unlock_func(Lock);
}

static int asf_print_strings_match(
    const char *pattern,
    const char *string,
    asf_print_match_type match_type)
{
    if (match_type == ASF_PRINT_MATCH_EXACT &&
        !qdf_str_cmp(pattern, string)) {
        return 1;
    }
#if ASF_PRINT_SUPPORT >= 2
    if (match_type == ASF_PRINT_MATCH_START) {
        /* the caller has guaranteed that pattern and string are non-NULL */
        while (*pattern != '\0' && *string != '\0') {
            if (*pattern != *string) return 0; /* strings differ */
            pattern++; string++;
        }
        if (*pattern == '\0') return 1; /* "string" starts with "pattern" */
        return 0;
    }
    if (match_type == ASF_PRINT_MATCH_CONTAINS) {
        if (*pattern == '\0') return 1; /* empty pattern matches any string */
        /* the caller has guaranteed that pattern and string are non-NULL */
        /* Repeat while "string" contains characters:
         * 1.  Remove the initial chars from "string" until a match is
         *     found for the first char of "pattern".
         * 2.  See if "pattern" occurs at the start of "string".
         */
        while (1) {
            const char *tmp_str, *tmp_pat;
            /* advance string until its 1st char matches pattern's 1st char */
            while (*string != '\0' && *string != *pattern) {
                string++;
            }
            if (*string == '\0') return 0;
            /* 
             * Use temporary pointers to see if "pattern" occurs
             * at the start of "string".
             */
            tmp_str = string;
            tmp_pat = pattern;
            while (*tmp_pat != '\0' && *tmp_str != '\0') {
                if (*tmp_pat != *tmp_str) break; /* strings differ */
                tmp_pat++; tmp_str++;
            }
            /* if the entire pattern was found in string, it's a match */
            if (*tmp_pat == '\0') return 1;
            string++; /* no match at this char - try the next char */
        } 
    }
#endif
    return 0;
}

void asf_print_mask_set_by_bit_name_private(
    struct asf_print_instance *handle,
    const char *name,
    const char *bit_name,
    int enable,
    asf_print_match_type match_type)
{
    struct asf_print_ctrl *p;

    if (!bit_name) return; /* sanity checks */
    if (!handle) handle = &asf_print_default;

#if ASF_PRINT_SUPPORT < 2
    /* silently downgrade the bit name match method to the basic method (exact match) */
    match_type = ASF_PRINT_MATCH_EXACT;
#endif

    /*
     * set/clear the specified bit for the print-control struct with the
     * matching name, or if no name is specified, for all print-control
     * structs.
     */
    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return;
    }
    p = handle->print_ctrl_list;
    do {
        if (! name || (p->name && ! qdf_str_cmp(name, p->name))) {
            int i;
            for (i = 0; i < p->num_bit_specs; i++) {
                if (p->bit_specs[i].bit_name &&
                    asf_print_strings_match(
                        bit_name, p->bit_specs[i].bit_name, match_type))
                {
                    int category = p->bit_specs[i].bit_num;
                    asf_print_mask_set(p, category, enable);
                }
            }
        }
        p = p->next;
    } while (p);
    if (Lock) Unlock_func(Lock);
}

void asf_print_verb_set_by_name_private(
    struct asf_print_instance *handle,
    const char *name,
    unsigned threshold)
{
    struct asf_print_ctrl *p;

    if (!handle) handle = &asf_print_default;
    /*
     * set the verbosity level for the print-control struct with the
     * matching name, or if no name is specified, for all print-control
     * structs.
     */
    if (Lock) Lock_func(Lock);
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return;
    }
    p = handle->print_ctrl_list;
    do {
        if (! name || (p->name && ! qdf_str_cmp(name, p->name))) {
            p->verb_threshold = threshold;
        }
        p = p->next;
    } while (p);
    if (Lock) Unlock_func(Lock);
}

int asf_print_get_namespaces_private(
    struct asf_print_instance *handle,
    const char *names[],
    int length)
{
    struct asf_print_ctrl *p;
    int status;

    if (!handle) handle = &asf_print_default;

    if (Lock) Lock_func(Lock);
    /* make sure the handle points to a valid asf_print_instance */
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return 0;
    }
    /*
     * If the caller has not provided a buffer to copy the names of
     * print-control objects into, then just return the number of
     * print-control objects in the list.
     */
    if (!names || length == 0) {
        int num_list_elems = handle->num_list_elems;
        if (Lock) Unlock_func(Lock);
        return num_list_elems;
    }

    status = (handle->num_list_elems > length) ?
        /* a negative return value indicates how many names weren't copied */
        length - handle->num_list_elems :
        /* a positive return value indicates how many names were copied */
        handle->num_list_elems;

    p = handle->print_ctrl_list;
    while (p && length) {
        *names = p->name;
        p = p->next;
        names++;
        length--;
    }
    if (Lock) Unlock_func(Lock);
    return status;
}

const struct asf_print_bit_spec *asf_print_get_bit_specs_private(
    struct asf_print_instance *handle,
    char *name,
    int *length)
{
    struct asf_print_ctrl *p;
    const struct asf_print_bit_spec *bit_specs = NULL;

    if (!name || !length) return NULL; /* sanity checks */
    if (!handle) handle = &asf_print_default;

    if (Lock) Lock_func(Lock);
    /* make sure the handle points to a valid asf_print_instance */
    if (!ASF_PRINT_IS_VALID(handle)) {
        if (Lock) Unlock_func(Lock);
        return NULL;
    }
    p = handle->print_ctrl_list;
    do {
        if (p->name && ! qdf_str_cmp(name, p->name)) {
            *length = p->num_bit_specs;
            bit_specs = p->bit_specs;
            break;
        }
        p = p->next;
    } while (p);
    if (Lock) Unlock_func(Lock);
    return bit_specs;
}
