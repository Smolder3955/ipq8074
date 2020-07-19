/*
* Copyright (c) 2013, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
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
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <stdarg.h> /* va_list */

/* ASF_PRINT_INLINE -
 * Should the print-category check performed by asf_print be done
 * inline (with asf_print being a macro), or inside the asf_print
 * function?
 * The latter minimizes the code size.  The former keeps print
 * arguments from being evaluated when the print category in question
 * is disabled - this can be significant if print arguments include
 * register-read results, which can take a long time to obtain.
 */
#ifndef ASF_PRINT_INLINE
/*
 * By default, make asf_print a function, to minimize code size.
 */
#define ASF_PRINT_INLINE 0
#endif


/* ASF_PRINT_MASK_BITS -
 * This limits the number of distinct categories
 * within a print mask.  (One bit per category.)
 */
#define ASF_PRINT_MASK_BITS 64

#define ASF_PRINT_MASK_BYTES \
    ((ASF_PRINT_MASK_BITS + 7) / 8)

/* asf_print_bit_spec -
 * Mapping of print-category names to their bit-numbers.
 * This allows the user-friendly feature of specifying print categories
 * to be turned on/off by name rather than by bit number.
 * In addition, it allows the user to enable / disable print categories
 * in multiple print masks that use the same name, even if they have
 * different bit positions.
 */
struct asf_print_bit_spec {
    int bit_num; /* 0 to ASF_PRINT_MASK_BITS-1 */
    const char *bit_name;
};

/* asf_print_ctrl -
 * Information to control printout filtering.
 * This struct contains both fields that are directly used to determine
 * whether to allow or filter out printouts (the category_mask and
 * verb_threshold) fields, and also fields that facilitate manipulation
 * of the direct-control fields.
 */
struct asf_print_ctrl {
    unsigned char category_mask[ASF_PRINT_MASK_BYTES];
    unsigned verb_threshold;
    const char *name; /* optional name */

    /* bit_specs - optional map of bit names -> bit numbers */
    int num_bit_specs;
    const struct asf_print_bit_spec *bit_specs;

    /* next - allow print control structs to be linked into a list */
    struct asf_print_ctrl *next;

    /*
     * custom - allow a print-control object to specify its own
     * printout function, including a "context" argument.
     * If this function pointer is non-NULL, this custom printout
     * function will be used instead of the global printout
     * registered via asf_print_setup's vp arg.
     * The custom_ctxt pointer will be used as the ctxt arg when
     * the custom_print function is called.
     */
    void (*custom_print)(void *ctxt, const char *fmt, va_list args);
    void *custom_ctxt;
};

/* asf_vprint_fp -
 * Function pointer provided to asf_print to actually do the print, once
 * asf_print determines a print should be kept rather than filtered out.
 */
typedef void (*asf_vprint_fp)(const char *fmt, va_list args);
typedef void (*asf_print_lock_fp)(void *lock);
typedef void (*asf_print_unlock_fp)(void *lock);

/* asf_print_setup -
 * Register a vprintf function for asf_print to use in the cases where,
 * based on the verbosity and category, asf_print actually does print.
 * This registered function could either be a kernel function, a la
 * vprintk, or a user-space function, a la vprintf.
 *
 * If a print function pointer has already been registered through a
 * prior call to asf_print_setup, the new value will be ignored,
 * unless it is NULL.
 * Thus, a 2nd call to asf_print_setup will have no effect, unless it
 * sets the function pointer to NULL, so a 3rd call to asf_print_setup
 * can reprogram the function pointer.
 * The asf_print_reset macro can be used to undo an initial call to
 * asf_print_setup
 */
extern void asf_print_setup(
    asf_vprint_fp vp,
    asf_print_lock_fp lock_func,
    asf_print_unlock_fp unlock_func,
    void *lock);

/* asf_print_reset:
 * Erase the function pointers registered by a prior call to
 * asf_print_setup, so a subsequent call to asf_print_setup can
 * register different function pointers.
 */
#define asf_print_reset() \
    asf_print_setup(NULL, NULL, NULL, NULL, NULL, NULL)

extern struct asf_print_ctrl AsfPrintCtrlShared;

/* asf_print -
 * Do the printout specified by the format string argument ("fmt") and
 * variable args, but only if the specified category is enabled in the
 * print control struct and the verbosity threshold in the print control
 * struct is >= the specified verbosity of this printout.
 * If no print control struct is provided (handle == NULL), a shared
 * global print control struct is used.
 */
#if ASF_PRINT_INLINE /*------------------------------------------------------*/
/*
 * Implement asf_print and asf_print_category as macros, so the
 * print condition check can be done inline, and the function call
 * (and in particular, the function arg evaluation can be skipped
 * if the print category in question is disabled.
 * This inlining can result in a substantial code size increase.
 */
extern void asf_print_private(
    struct asf_print_ctrl *handle,
    const char *fmt,
    ...);

#define asf_print(handle, category, verb, fmt, ...)                 \
    /* sanity check */                                              \
    if ((category) < ASF_PRINT_MASK_BITS) {                         \
        int byte, bit;                                              \
        struct asf_print_ctrl *private_handle;                      \
                                                                    \
        byte = (category) >> 3; /* category / 8 */                  \
        bit = (category) - byte * 8;                                \
        private_handle = (handle) ?                                 \
            (handle) :                                              \
            &AsfPrintCtrlShared;                                    \
        if (private_handle->category_mask[byte] & (1 << bit) &&     \
            (verb) <= private_handle->verb_threshold )              \
        {                                                           \
            asf_print_private(private_handle, fmt, ## __VA_ARGS__); \
        }                                                           \
    }

#define asf_print_category(handle, category, fmt, ...)              \
    /* sanity check */                                              \
    if ((category) < ASF_PRINT_MASK_BITS) {                         \
        int byte, bit;                                              \
        struct asf_print_ctrl *private_handle;                      \
                                                                    \
        byte = (category) >> 3; /* category / 8 */                  \
        bit = (category) - byte * 8;                                \
        private_handle = (handle) ?                                 \
            (handle) :                                              \
            &AsfPrintCtrlShared;                                    \
        if (private_handle->category_mask[byte] & (1 << bit)) {     \
            asf_print_private(private_handle, fmt, ## __VA_ARGS__); \
        }                                                           \
    }

extern void asf_vprint_private(
    struct asf_print_ctrl *handle,
    const char *fmt,
    va_list args);

/* asf_vprint -
 * Same as asf_print, but takes its variable args as a va_list.
 */
#define asf_vprint(handle, category, verb, fmt, args)            \
    /* sanity check */                                           \
    if ((category) < ASF_PRINT_MASK_BITS) {                      \
        int byte, bit;                                           \
        struct asf_print_ctrl *private_handle;                   \
                                                                 \
        byte = (category) >> 3; /* category / 8 */               \
        bit = (category) - byte * 8;                             \
        private_handle = (handle) ?                              \
            (handle) :                                           \
            &AsfPrintCtrlShared;                                 \
        if (private_handle->category_mask[byte] & (1 << bit) &&  \
            (verb) <= private_handle->verb_threshold )           \
        {                                                        \
            asf_vprint_private(private_handle, fmt, args);       \
        }                                                        \
    }

#define asf_vprint_category(handle, category, fmt, args)         \
    /* sanity check */                                           \
    if ((category) < ASF_PRINT_MASK_BITS) {                      \
        int byte, bit;                                           \
        struct asf_print_ctrl *private_handle;                   \
                                                                 \
        byte = (category) >> 3; /* category / 8 */               \
        bit = (category) - byte * 8;                             \
        private_handle = (handle) ?                              \
            (handle) :                                           \
            &AsfPrintCtrlShared;                                 \
        if (private_handle->category_mask[byte] & (1 << bit)) {  \
            asf_vprint_private(private_handle, fmt, args);       \
        }                                                        \
    }

#else /*---------------------------------------------------------------------*/
/*
 * Implement asf_print and asf_print_category as functions,
 * i.e. make asf_print and asf_print_category into pass-through macros
 * and do all filtering inside the asf_print_private function.
 * This reduces code size, at the expense of always evaluating
 * the function arguments, even if the print message ends up
 * being discarded because its print category is disabled.
 */
extern void asf_print_private(
    struct asf_print_ctrl *handle,
    unsigned category,
    unsigned verb,
    const char *fmt,
    ...);

#define asf_print(handle, category, verb, fmt, ...) \
    asf_print_private(handle, category, verb, fmt, ## __VA_ARGS__)

extern void asf_print_category_private(
    struct asf_print_ctrl *handle,
    unsigned category,
    const char *fmt,
    ...);

#define asf_print_category(handle, category, fmt, ...) \
    asf_print_category_private(handle, category, fmt, ## __VA_ARGS__)

extern void asf_vprint_private(
    struct asf_print_ctrl *handle,
    unsigned category,
    unsigned verb,
    const char *fmt,
    va_list args);

#define asf_vprint(handle, category, verb, fmt, args) \
    asf_vprint_private(handle, category, verb, fmt, args)

extern void asf_vprint_category_private(
    struct asf_print_ctrl *handle,
    unsigned category,
    const char *fmt,
    va_list args);

#define asf_vprint_category(handle, category, fmt, args) \
    asf_vprint_category_private(handle, category, fmt, args)

#endif /* ASF_PRINT_INLINE */ /*---------------------------------------------*/

struct asf_print_instance; /* forward declaration */

/* asf_print_ctrl_register -
 * Add a print-control object into a list within the default shared
 * asf_print_instance, to allow print-control category / verbosity
 * settings to be easily applied to multiple print-control structs.
 * It is not necessary to register a print-control struct, unless it
 * is desired to use the category mask / verbosity level adjustment
 * functions below to affect the print-control struct in question.
 */
#define asf_print_ctrl_register(print_ctrl_p) \
    asf_print_ctrl_register_private(NULL, print_ctrl_p)

/* asf_print_ctrl_unregister -
 * Remove a print-control object from the default shared
 * asf_print_instance's list.
 * Returns 0 if the specified object was found and removed, or 1 on failure.
 */
#define asf_print_ctrl_unregister(print_ctrl_p) \
    asf_print_ctrl_unregister_private(NULL, print_ctrl_p)

/* asf_print_mask_set -
 * Enable/disable the specified category in the provided print-control struct.
 */
extern void asf_print_mask_set(
    struct asf_print_ctrl *handle,
    int category,
    int enable);

/* asf_print_mask_set_by_name -
 * Enable/disable the specified category in the specified print-control
 * struct.
 * This function searches its list of registered print-control structs
 * and operates on all print-control structs with names that match the
 * specification.
 * If no name is specified (name == NULL), then this function operates on
 * all print-control structs in its registered list.
 * If no match is found, the function silently returns.
 */
#define asf_print_mask_set_by_name(name, category, enable) \
    asf_print_mask_set_by_name_private(NULL, name, category, enable)

/* asf_print_match_type -
 * Enumeration of the different types of matching for bit names that
 * asf_print supports.
 *   EXACT - matches if the specified string exactly matches a bit name
 *   START - matches if the spec string is an initial substring of a bit name
 *   CONTAINS - matches if the specified string is a substring of a bit name
 */
typedef enum {
    ASF_PRINT_MATCH_EXACT,
    ASF_PRINT_MATCH_START,
    ASF_PRINT_MATCH_CONTAINS,

    ASF_PRINT_MATCH_NUM_TYPES
} asf_print_match_type;

/* asf_print_mask_set_by_bit_name -
 * Enable/disable the category specified by name in the print-control
 * struct specified by name.
 * This function searches its list of registered print-control structs
 * and operates on all print-control structs with names that match the
 * specification.
 * Within each print-control struct having the specified name, the
 * function searches for a category bit specification whose name matches
 * the specified bit-name.  Any matching categories are enabled or
 * disabled, according to the indicated action.
 * If no name is specified (name == NULL), then this function operates on
 * all print-control structs in its registered list.
 * If no match is found, the function silently returns.
 */
#define asf_print_mask_set_by_bit_name(name, bit_name, enable, match) \
    asf_print_mask_set_by_bit_name_private(NULL, name, bit_name, enable, match)

/* asf_print_verb_set_by_name -
 * Adjust the verbosity threshold to the specified level in the specified
 * print-control struct.
 * This function searches its list of registered print-control structs
 * and operates on all print-control structs with names that match the
 * specification.
 * If no name is specified (name == NULL), then this function operates on
 * all print-control structs in its registered list.
 * If no match is found, the function silently returns.
 */
#define asf_print_verb_set_by_name(name, threshold) \
    asf_print_verb_set_by_name_private(NULL, name, threshold);

/* asf_print_get_namespaces -
 * Copy the names of the print-control objects registered with the default
 * shared asf_print_instance into a buffer provided by the caller.
 * The caller indicates how long the buffer is.
 * If the caller either provides NULL for the buffer, or a length of 0,
 * asf_print_get_namespaces will not copy print-control struct names
 * into the buffer, but will instead return the number of print-control
 * structs that are currently registered.
 *
 * Note that the strings themselves are not copied into the buffer;
 * rather, just the char* pointers are copied into the buffer.
 * This is sufficient, since in general the print-control object names
 * will be string literals, rather than dynamically-constructed strings
 * in non-permanent buffers.
 *
 * If the buffer does not have room to store the names of all print-control
 * objects, asf_print_get_namespaces will copy as many as it can, and return
 * a negative result.  The absolute value of this negative value will
 * indicate how many print-control object's names were not copied.
 * For example, if there are 10 registered print-control structs, but
 * the caller to asf_print_get_namespaces only provides a *char[8] buffer,
 * asf_print_get_namespaces will return -2.
 * Otherwise, asf_print_get_namespaces will return the number of print
 * control object's names that it copied.
 */
#define asf_print_get_namespaces(names, length) \
    asf_print_get_namespaces_private(NULL, names, length)

/* asf_print_get_bit_specs -
 * Return the names and the bit-numbers for each print category within
 * the print control object specified by name.
 * Fill in the call-by-reference "length" parameter with the number of
 * print categories in the returned bit_spec array.
 * The print control object list in the default shared asf_print_instance
 * is searched for the specified print control object.
 *
 * Returns NULL on error (e.g. if the specified print control object
 * is not found in the list)
 */
#define asf_print_get_bit_specs(name, length) \
    asf_print_get_bit_specs_private(NULL, name, length)


/*--- advanced asf_print functions -------------------------------------------
 * These functions create/delete new asf_print_instances that can be
 * used to create exclusive groups of asf_print_ctrl structs,
 * or take a handle to an asf_print_instance to specify which group
 * of registered asf_print_ctrl structs to operate on, rather than
 * using the default shared asf_print_instance.
 */

/* asf_print_new -
 * Allocate, initialize, and return a new asf_print_instance object.
 * This allows print control objects to be registered to different
 * asf_print objects, resulting in groups of print control objects
 * belonging to associated modules, rather than a single flat group
 * of all print control objects.
 * Returns a handle to a new asf_print_instance object on success,
 * or NULL on failure.
 */
extern struct asf_print_instance *asf_print_new(void/*lock*/);

/* asf_print_destroy -
 * Delete an asf_print object.
 * It is necessary to unregister all print control structs from the
 * asf_print_instance object before destroying it.
 * Returns 0 on success, non-zero on failure.
 */
extern int asf_print_destroy(struct asf_print_instance *handle);

#define asf_print_ctrl_register_adv(handle, print_ctrl_p) \
    asf_print_ctrl_register_private(handle, print_ctrl_p)

#define asf_print_ctrl_unregister_adv(handle, print_ctrl_p) \
    asf_print_ctrl_unregister_private(handle, print_ctrl_p)

#define asf_print_mask_set_by_name_adv(handle, name, category, enable) \
    asf_print_mask_set_by_name_private(handle, name, category, enable)

#define asf_print_mask_set_by_bit_name_adv(h, name, bit_name, enable, match) \
    asf_print_mask_set_by_bit_name_private(h, name, bit_name, enable, match)

#define asf_print_verb_set_by_name_adv(handle, name, threshold) \
    asf_print_verb_set_by_name_private(handle, name, threshold)

#define asf_print_get_namespaces_adv(handle, names, length) \
    asf_print_get_namespaces_private(handle, names, length)

#define asf_print_get_bit_specs_adv(handle, name, length) \
    asf_print_get_bit_specs_private(handle, name, length)


/*=== private asf_print functions ============================================
 * These functions should not be used directly.
 * Instead, use the API macros that call these functions.
 */

/*
 * Don't call asf_print_ctrl_register_private directly.
 * Use asf_print_ctrl_register or asf_print_ctrl_register_adv instead.
 */
extern void asf_print_ctrl_register_private(
    struct asf_print_instance *handle,
    struct asf_print_ctrl *p);

/*
 * Don't call asf_print_ctrl_unregister_private directly.
 * Use asf_print_ctrl_unregister or asf_print_ctrl_unregister_adv instead.
 */
extern int asf_print_ctrl_unregister_private(
    struct asf_print_instance *handle,
    struct asf_print_ctrl *p);

/*
 * Don't call asf_print_mask_set_by_name_private directly.
 * Use asf_print_mask_set_by_name or asf_print_mask_set_by_name_adv instead.
 */
extern void asf_print_mask_set_by_name_private(
    struct asf_print_instance *handle,
    const char *name,
    int category,
    int enable);

/*
 * Don't call asf_print_mask_set_by_bit_name_private directly.
 * Use asf_print_mask_set_by_bit_name or asf_print_mask_set_by_bit_name_adv
 * instead.
 */
extern void asf_print_mask_set_by_bit_name_private(
    struct asf_print_instance *handle,
    const char *name,
    const char *bit_name,
    int enable,
    asf_print_match_type match);

/*
 * Don't call asf_print_verb_set_by_name_private directly.
 * Use asf_print_verb_set_by_name or asf_print_verb_set_by_name_adv instead.
 */
extern void asf_print_verb_set_by_name_private(
    struct asf_print_instance *handle,
    const char *name,
    unsigned threshold);

/*
 * Don't call asf_print_get_namespaces_private directly.
 * Use asf_print_get_namespaces or asf_print_get_namespaces_adv instead.
 */
int asf_print_get_namespaces_private(
    struct asf_print_instance *handle,
    const char *names[],
    int length);

/*
 * Don't call asf_print_get_bit_specs_private directly.
 * Use asf_print_get_bit_specs or asf_print_get_bit_specs_adv instead.
 */
const struct asf_print_bit_spec *asf_print_get_bit_specs_private(
    struct asf_print_instance *handle,
    char *name,
    int *length);
