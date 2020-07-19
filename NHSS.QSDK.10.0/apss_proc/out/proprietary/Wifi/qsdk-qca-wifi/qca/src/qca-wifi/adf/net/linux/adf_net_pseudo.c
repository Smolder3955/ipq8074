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

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <adf_os_types.h>
#include <adf_net.h>
#include <asf_queue.h>

#define NUM_OF_DEVICES              2

static int               num = 0;


/**
 * Prototypes
 */
int             __adf_net_pseudo_attach(const char *mod_name);
int             __adf_net_pseudo_detach(const char *mod_name);

extern __adf_net_mod_t *    __adf_net_find_mod(const char *mod_name);


__adf_os_device_t osdev[NUM_OF_DEVICES];

EXPORT_SYMBOL(__adf_net_pseudo_attach);
EXPORT_SYMBOL(__adf_net_pseudo_detach);

int 
__adf_net_pseudo_attach(const char *mod_name)
{
    __adf_net_mod_t         *elem = NULL;
    adf_os_resource_t       drv_res = {0};
    adf_os_attach_data_t    drv_data = {{0}};   
    a_status_t          status;

    printk("ADF_NET:Attaching the pseudo driver\n");
    /**
     * Allocate the sc & zero down
     */
    osdev[num] = kzalloc(sizeof(struct __adf_device), GFP_KERNEL);
    if (!osdev[num]) {
        printk("Cannot malloc softc\n");
        status = A_STATUS_ENOMEM;
        goto mem_fail;
    }
    /**
     * set the references for sc & dev
     */
    osdev[num]->dev      = NULL;

    elem = __adf_net_find_mod(mod_name);
    /**
     * we should always find a drv
     */
    adf_os_assert(elem);

    osdev[num]->drv_hdl = adf_drv_attach(elem, &drv_res, 1, &drv_data, osdev[num]);

    /**
     * We expect the at the driver_attach the create_dev has happened
     */
    if(osdev[num]->drv_hdl == NULL) {
        printk("ADF_NET:Pseudo device is not created, asserting\n");
        adf_os_assert(0);
        goto attach_fail;
    }
    num++;
    return 0;

attach_fail:
    kfree(osdev[num]);
mem_fail:
    return status;
}

int  
__adf_net_pseudo_detach(const char *mod_name)
{
    __adf_net_mod_t *elem;

    num--;

    printk("ADF_NET:detaching the pseudo driver\n");
    
    elem = __adf_net_find_mod(mod_name);
    /**
     * only detach for attached devices
     */
    adf_os_assert(elem);

    adf_drv_detach(elem, osdev[num]->drv_hdl);

    kfree(osdev[num]);
    
    return 0;
}

