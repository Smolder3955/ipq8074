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

#include <linux/module.h>
#include <adf_os_perf.h>

MODULE_AUTHOR("QUalcomm Atheros Inc.");
MODULE_DESCRIPTION("Qualcomm Atheros Device Framework Module");
MODULE_LICENSE("Dual BSD/GPL");

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

static int __init 
adf_os_mod_init(void)
{
    adf_os_perfmod_init();
    return 0;
}

static void __exit 
adf_os_mod_exit(void)
{
    adf_os_perfmod_exit();
}

module_init(adf_os_mod_init);
module_exit(adf_os_mod_exit);

