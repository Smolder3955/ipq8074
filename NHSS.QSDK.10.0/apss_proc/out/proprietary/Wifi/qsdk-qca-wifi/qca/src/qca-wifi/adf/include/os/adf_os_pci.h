/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/**
 * @ingroup adf_os_public
 * @file adf_os_pci.h
 * This file abstracts the PCI subsystem.
 */
#ifndef __ADF_OS_PCI_H
#define __ADF_OS_PCI_H

#include <adf_os_types.h>
#include <adf_os_pci_pvt.h>

/**
 * @brief An ecore needs to provide a table of all pci device/vendor id's it 
 * supports
 *
 * This table should be terminated by a NULL entry , i.e. {0}
 */
typedef struct adf_os_pci_devid{
    a_uint32_t vendor;
    a_uint32_t device;
    a_uint32_t subvendor;
    a_uint32_t subdevice;
}adf_os_pci_devid_t;

typedef struct adf_os_pci_data{
    adf_os_pci_devid_t pci_id;
    a_uint32_t extra;
}adf_os_pci_data_t;
        
/**
 * @brief Setup the following driver information: name, PCI IDs of devices
 * supported and some device handlers.
 */
#define adf_os_pci_set_drv(_name, _pci_ids, _attach, \
                           _detach, _suspend, _resume) \
    __adf_os_pci_set_drv(_name, _pci_ids, _attach, _detach, _suspend, _resume)



typedef struct adf_os_pci_drvinfo{
    /**
     * @brief Attach function to be called when the OS detects the device
     */
    adf_drv_handle_t (*drv_attach)  (adf_os_pci_data_t *data, 
                                     adf_os_device_t osdev);

    /** 
     * @brief  detach function to be called when the OS removes the device
     */
    void       (*drv_detach)  (adf_drv_handle_t hdl);
    /** 
     * @brief  suspend function to be called when the OS suspends the device
     */
    void       (*drv_suspend) (adf_drv_handle_t hdl, adf_os_pm_t pm);
    /** 
     * @brief  resume function to be called when the OS resumes the device
     */
    void       (*drv_resume)  (adf_drv_handle_t hdl);
    /**
     * @brief driver specific data
     */
    adf_os_pci_devid_t         *pci_id; /**< PCI ID table pointer */
    unsigned char              *mod_name; /**< module name */
    unsigned char              *ifname; /**< Interface name for this device */
}adf_os_pci_drvinfo_t;


#define ADF_OS_MAX_PCI_IDS          256
#define ADF_OS_PCI_BAR_0            0
#define ADF_OS_PCI_ANY_ID           (~0)

/**
 * @brief Typically can be used to create a table of various 
 *        device ID's
 */
#define ADF_OS_PCI_DEVICE(_vendor, _device)   \
    (_vendor), (_device), ADF_OS_PCI_ANY_ID, ADF_OS_PCI_ANY_ID

/**
 * @brief Define the entry point for the PCI module.
 */ 
#define adf_os_pci_module_init(_fn)     __adf_os_pci_module_init(_fn)

/**
 * @brief Define the exit point for the PCI module.
 */ 
#define adf_os_pci_module_exit(_fn)     __adf_os_pci_module_exit(_fn)

/**
 * @brief Setup the following driver information: name, PCI IDs of devices
 * supported and some device handlers.
 */ 
#define adf_os_pci_set_drv_info(_name, _pci_ids, _attach, _detach, _suspend, _resume) \
    __adf_os_pci_set_drv_info(_name, _pci_ids, _attach, _detach, _suspend, _resume)

/**
 * @brief Read a byte of PCI config space.
 *
 * @param[in]  osdev    platform device instance
 * @param[in]  offset   offset to read
 * @param[out] val      value read
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_read8(adf_os_device_t osdev, int offset, a_uint8_t *val)
{
    return __adf_os_pci_config_read8(osdev, offset, val);
}

/**
 * @brief Write a byte to PCI config space.
 *
 * @param[in] osdev    platform device instance
 * @param[in] offset   offset to write
 * @param[in] val      value to write
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_write8(adf_os_device_t osdev, int offset, a_uint8_t val)
{
    return __adf_os_pci_config_write8(osdev, offset, val);
}

/**
 * @brief Read 2 bytes of PCI config space.
 *
 * @param[in]  osdev    platform device instance
 * @param[in]  offset   offset to read
 * @param[out] val      value read
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_read16(adf_os_device_t osdev, int offset, a_uint16_t *val)
{
    return __adf_os_pci_config_read16(osdev, offset, val);
}

/**
 * @brief Write 2 bytes to PCI config space.
 *
 * @param[in] osdev    platform device instance
 * @param[in] offset   offset to write
 * @param[in] val      value to write
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_write16(adf_os_device_t osdev, int offset, a_uint16_t val)
{
    return __adf_os_pci_config_write16(osdev, offset, val);
}

/**
 * @brief Read 4 bytes of PCI config space.
 *
 * @param[in]  osdev    platform device instance
 * @param[in]  offset   offset to read
 * @param[out] val      value read
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_read32(adf_os_device_t osdev, int offset, a_uint32_t *val)
{
    return __adf_os_pci_config_read32(osdev, offset, val);
}

/**
 * @brief Write 4 bytes to PCI config space.
 *
 * @param[in] osdev    platform device instance
 * @param[in] offset   offset to write
 * @param[in] val      value to write
 *
 * @return status of operation
 */ 
static inline int 
adf_os_pci_config_write32(adf_os_device_t osdev, int offset, a_uint32_t val)
{
    return __adf_os_pci_config_write32(osdev, offset, val);
}
/**
 * @brief Get OS Handle from OS device object.
 *
 *@param[in] osdev OS device object
 * 
 *@return OS handle
 */

static adf_os_inline adf_os_handle_t
adf_os_pcidev_to_os(adf_os_device_t osdev)
{
        return __adf_os_pcidev_to_os(osdev);
}


/**
 * @brief this register the driver to the shim, but won't get
 *        any handle until create device is called.
 * 
 * @param[in] drv_info driver info structure
 * 
 * @return status of operation
 */
static adf_os_inline a_status_t
adf_os_pci_drv_reg(adf_os_pci_drvinfo_t *drv_info)
{
    return(__adf_os_pci_drv_reg(drv_info));
}


/**
 * @brief deregister the driver from the shim
 *
 * @param[in] drv_name driver name passed in adf_drv_info_t
 *
 * @see adf_os_pci_drv_reg()
 */
static adf_os_inline void
adf_os_pci_drv_unreg(a_uint8_t *drv_name)
{
    __adf_os_pci_drv_unreg(drv_name);
}


#endif

