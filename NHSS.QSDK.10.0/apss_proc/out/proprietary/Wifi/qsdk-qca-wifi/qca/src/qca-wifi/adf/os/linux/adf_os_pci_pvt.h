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

#ifndef __ADF_CMN_OS_PCI_PVT_H
#define __ADF_CMN_OS_PCI_PVT_H

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <adf_os_types.h>
#include <adf_os_util.h>


#define __ADF_PCI_BAR0              0x10

struct adf_os_pci_drvinfo;

a_status_t       __adf_os_pci_drv_reg(struct adf_os_pci_drvinfo *drv);
void             __adf_os_pci_drv_unreg(uint8_t *name);

#define __adf_os_pci_module_init(_fn)  \
    static int _fn##_mod(void) \
    {                   \
        a_status_t st;  \
        st = (_fn)();         \
        if (st != A_STATUS_OK)  \
          return -1;            \
        else                    \
          return 0;             \
    }                           \
    module_init(_fn##_mod);

#define __adf_os_pci_module_exit(_x)   module_exit(_x)

/**
 * Initiallize the PCI driver structure
 */
#define __adf_os_pci_set_drv(_name,     \
                             _pci_ids,  \
                             _attach,   \
                             _detach,   \
                             _suspend,  \
                             _resume)   \
{                                   \
    .drv_attach = (_attach),        \
    .drv_detach = (_detach),        \
    .drv_suspend = (_suspend),      \
    .drv_resume = (_resume),        \
    .pci_id = (_pci_ids),           \
    .mod_name = #_name,             \
};                                  \
MODULE_LICENSE("BSD");



/**
 * @brief Read 1 byte, 2 byte & 4 byte config 
 * @param osdev
 * @param offset
 * @param val
 * 
 * @return int
 */
static inline int 
__adf_os_pci_config_read8(__adf_os_device_t osdev, int offset, 
                          uint8_t *val)
{
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    if(pci_read_config_byte(pdev, offset, val))
        return A_STATUS_FAILED;

    return 0;
}

static inline int 
__adf_os_pci_config_read16(__adf_os_device_t osdev, int offset, 
                          uint16_t *val)
{
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    if(pci_read_config_word(pdev, offset, val))
        return A_STATUS_FAILED;

    return 0;
}

static inline int 
__adf_os_pci_config_read32(__adf_os_device_t osdev, int offset, 
                          uint32_t *val)
{
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    if(pci_read_config_dword(pdev, offset, val))
        return A_STATUS_FAILED;

    return 0;
}

/**
 * @brief Write 1 byte, 2 byte & 4 byte config 
 * @param osdev
 * @param offset
 * @param val
 * 
 * @return int
 */

static inline int 
__adf_os_pci_config_write8(adf_os_device_t osdev, int offset, a_uint8_t val)
{
    uint8_t tmp;
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    pci_write_config_byte(pdev, offset, val);
    /**
     * generally the read should flush the bus with pending writes
     */
    pci_read_config_byte(pdev, offset, &tmp);

    if(tmp == val)
        return 0;
    return A_STATUS_FAILED;
}


static inline int
__adf_os_pci_config_write16(adf_os_device_t osdev, int offset, 
                            uint16_t val)
{
    uint16_t tmp;
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    pci_write_config_word(pdev, offset, val);
    /**
     * generally the read should flush the bus with pending writes
     */
    pci_read_config_word(pdev, offset, &tmp);

    if(tmp == val)
        return 0;
    return A_STATUS_FAILED;
}


static inline int
__adf_os_pci_config_write32(adf_os_device_t osdev, int offset, 
                            uint32_t val)
{
    uint32_t tmp;
    struct pci_dev *pdev = NULL;

    adf_os_assert(osdev);

    pdev = to_pci_dev(osdev->dev);
    pci_write_config_dword(pdev, offset, val);
    /**
     * generally the read should flush the bus with pending writes
     */
    pci_read_config_dword(pdev, offset, &tmp);

    if(tmp == val)
        return 0;
    return A_STATUS_FAILED;
}

static inline adf_os_handle_t
__adf_os_pcidev_to_os(__adf_os_device_t osdev)
{
        return ((adf_os_handle_t)osdev);
}


#endif/* __ADF_CMN_OS_PCI_PVT_H*/
