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
 *
 *  Implementation of Defer module.
 */

#include "ath_internal.h"
#include "ath_defer.h"

void
ath_defer_callback_func(ath_defer_call_t *defer_call)
{
    defer_call->callback(defer_call->context);
}

int
ath_init_defer_call(osdev_t osdev, ath_defer_call_t *defer_call,
                            ath_defer_callback func, void *arg)
{
    int ret=0;

    ret = OS_INIT_DEFER_CALL(osdev, &defer_call->os_defer_call,
            ath_defer_callback_func, defer_call);
    if (ret != 0)
        return 1;

    defer_call->callback = func;
    defer_call->context = arg;
    defer_call->signature = ATH_DEFER_CALL_ACTIVE;
    return 0;
}

void
ath_free_defer_call(ath_defer_call_t *defer_call)
{
    OS_FREE_DEFER_CALL(&defer_call->os_defer_call);

    defer_call->callback = NULL;
    defer_call->context = NULL;
    defer_call->signature = ATH_DEFER_CALL_DEAD;
}

void
ath_schedule_defer_call(ath_defer_call_t *defer_call)
{
    OS_SCHE_DEFER_CALL(&defer_call->os_defer_call);
}


