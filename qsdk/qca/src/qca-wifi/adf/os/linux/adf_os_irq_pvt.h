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

#ifndef __ADF_CMN_OS_IRQ_PVT_H
#define __ADF_CMN_OS_IRQ_PVT_H

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <adf_os_types.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
extern irqreturn_t __adf_os_interrupt(int irq, void *dev,
                                     struct pt_regs *regs);
#else
extern irqreturn_t __adf_os_interrupt(int irq, void *dev);
#endif

/**
 * @brief register the IRQ handler
 * 
 * @param[in] pdev
 * @param[out] osdev (store IRQ)
 * 
 * @return int
 */
static inline a_status_t
__adf_os_setup_intr(__adf_os_device_t osdev, adf_os_drv_intr fn)
{
    int err = 0, irq_type;

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
    irq_type = SA_SHIRQ;
#else
    irq_type = IRQF_SHARED;
#endif
    osdev->func  = (__adf_os_intr)fn;

    err = request_irq(osdev->irq,
                      __adf_os_interrupt, 
                      irq_type, 
                      osdev->drv_name,
                      osdev); 
    if (err < 0){
        printk("ADF_NET:Interrupt setup failed\n");
        err = A_STATUS_EIO;
    }
    
    return err;
}

/**
 * @brief release the IRQ
 * 
 * @param irq (irq number)
 * @param sc (cookie)
 */
static inline void
__adf_os_free_intr(__adf_os_device_t osdev)
{
     free_irq(osdev->irq, osdev);
}
#endif
