/*==================================================================
 *
 * FILE:        deviceprogrammer_bsp.h
 *
 * DESCRIPTION: This file provides SDCC abstraction for dependent drivers for Non-OS(BOOT).
 *   
 *
 *        Copyright (c) 2008-2013,2017 Qualcomm Technologies, Inc.
 *        All rights reserved.
 *        Confidential and Proprietary - Qualcomm Technologies, Inc.
 *==================================================================*/

/*===================================================================
 *
 *                       EDIT HISTORY FOR FILE
 *
 *   This section contains comments describing changes made to the
 *   module. Notice that changes are listed in reverse chronological
 *   order.
 *
 *   $Header: //components/rel/boot.bf/3.3.1/boot_images/core/storage/tools/deviceprogrammer_ddr/src/bsp/deviceprogrammer_bsp.h#5 $
 *   $DateTime: 2017/05/16 09:41:44 $ $Author: pwbldsvc $
 *
 *   YYYY-MM-DD   who     what, where, why
 *   ----------   ---     ----------------------------------------------
 *   08/05/17   an      Fixed header markings.
 *   2013-06-03   ah      Added legal header
 *   2013-05-31   ab      Initial checkin
 */

#ifndef __DEVICEPROGRAMMER_BSP_H
#define __DEVICEPROGRAMMER_BSP_H

/******************************************************************************
*
* Description:
*    Disable the watchdog timer
*      
******************************************************************************/
void bsp_freeze_wdog(void);

/******************************************************************************
*
* Description:
*    Kick the watchdog timer
*      
******************************************************************************/
void bsp_kick_wdog(void);

/******************************************************************************
*
* Description:
*    This function Resets the target
*
******************************************************************************/
void bsp_target_reset(void);

/******************************************************************************
*
* Description:
*    This function powers down the target
*
******************************************************************************/
void bsp_target_poweroff(void);


/******************************************************************************
*
* Description:
*    This function performs the HW Init for eHOSTDL
*    The HW init includes enabling SDRAM, Clocks etc...
*      
******************************************************************************/
void bsp_hw_init(void);

/******************************************************************************
*
* Description:
*    This function provides a busy wait for the caller.
*
* Arguments:
*    us              [IN] : time in micro seconds
*      
******************************************************************************/
void clk_busy_wait(uint32 us);


#endif /* __DEVICEPROGRAMMER_BSP_H */



