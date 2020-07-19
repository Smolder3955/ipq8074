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
 * Public Interface for defer module
 */

/*
 * Definitions for the Atheros Defer module.
 */
#ifndef _DEV_ATH_DEFER_H
#define _DEV_ATH_DEFER_H

/* 
 * defer handler function. 
 */

typedef int (*ath_defer_callback) (void *context);

typedef struct ath_defer_call_s {
    os_defer_call_t        os_defer_call;            // timer object
    void*                  context;             // execution context
    int (*callback) (void*);               // handler function
    u_int32_t              signature;           // contains a signature indicating object has been initialized
} ath_defer_call_t;

int ath_init_defer_call(osdev_t osdev,  ath_defer_call_t *defer_call,
                            ath_defer_callback func, void *arg);
void ath_free_defer_call(ath_defer_call_t *defer_call);
void ath_schedule_defer_call (ath_defer_call_t *defer_call);

#define ATH_DEFER_CALL_ACTIVE     0xfe00dc88
#define ATH_DEFER_CALL_DEAD       0xfe00dc44

#endif /* _DEV_ATH_DEFER_H */


