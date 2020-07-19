/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*! \file ah_wmi.c
**  \brief Linux support for WMI function in HAL
**    
**  Copyright (c) 2002-2004 Sam Leffler, Errno Consulting.
**  Copyright (c) 2005-2007 Atheros Communications Inc.
**
**  All rights reserved.
**
** Redistribution and use in source and binary forms are permitted
** provided that the following conditions are met:
** 1. The materials contained herein are unmodified and are used
**    unmodified.
** 2. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following NO
**    ''WARRANTY'' disclaimer below (''Disclaimer''), without
**    modification.
** 3. Redistributions in binary form must reproduce at minimum a
**    disclaimer similar to the Disclaimer below and any redistribution
**    must be conditioned upon including a substantially similar
**    Disclaimer requirement for further binary redistribution.
** 4. Neither the names of the above-listed copyright holders nor the
**    names of any contributors may be used to endorse or promote
**    product derived from this software without specific prior written
**    permission.
**
** NO WARRANTY
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
** MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
** FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
** OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGES.
**
*/

/* 
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */ 

#include "opt_ah.h"
#include "ah.h"
#include "ah_internal.h"



#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

/*
** Linux Includes
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>

/*
** HTC Includes
*/
#include "../../htc/inc/wmi_cmd.h"

/*
** HAL Includes
*/

#include <ar5416/ar5416.h>


#define AR5416_MAX_CHAINS 3

/*
 * TxPower per rate is given by EEPROM and is
 * sent to the target
 */
struct rate_table_txpow
{
    u_int32_t rateCount;
    u_int32_t mode;
    /* adjusted power for descriptor-based TPC for 1, 2, or 3 chains */
    int txPower[37/*Ar5416RateSize*/][AR5416_MAX_CHAINS];
};




/* printf interfaces */
extern  void __ahdecl ath_hal_printf(struct ath_hal *, const char*, ...)
        __printflike(2,3);
extern QDF_STATUS __ahdecl 
wmi_cmd(void *wmi_handle, WMI_COMMAND_ID cmdId, u_int8_t *pCmdBuffer, u_int32_t length, u_int8_t *pRspBuffer, u_int32_t rspLen, u_int32_t timeout);

extern void __ahdecl
wmi_reg_write_single(void *wmi_handle, u_int reg, u_int32_t val);
extern void __ahdecl
wmi_reg_write_delay(void *wmi_handle, u_int reg, u_int32_t val);
extern void __ahdecl
wmi_reg_write_flush(void *wmi_handle);
extern void __ahdecl
wmi_reg_rmw(void *wmi_handle, u_int reg, u_int32_t clr, u_int32_t set);

void __ahdecl 
ath_hal_wmi_reg_write_single(struct ath_hal *ah, u_int reg, u_int32_t val);

#define KEYTABLE_0               0x8800  /* MAC Key Cache */
#define KEYTABLE(_n)             (KEYTABLE_0 + ((_n)*32))
#define KEY_CACHE_SIZE           128

struct registerWrite {
    u_int32_t reg;
    u_int32_t val;
} /*data*/;

void __ahdecl 
ath_hal_wmi_reg_write_delay(struct ath_hal *ah, u_int reg, u_int32_t val)
{
    /* We need to keep register write in keycache single */
    if (reg >= KEYTABLE_0 && reg < KEYTABLE(KEY_CACHE_SIZE))
        ath_hal_wmi_reg_write_single(ah, reg, val);
    else
        wmi_reg_write_delay(AH_PRIVATE(ah)->hal_wmi_handle, reg, val);
}

void __ahdecl 
ath_hal_wmi_reg_write_flush(struct ath_hal *ah)
{
    wmi_reg_write_flush(AH_PRIVATE(ah)->hal_wmi_handle);
}

void __ahdecl 
ath_hal_wmi_reg_write_single(struct ath_hal *ah, u_int reg, u_int32_t val)
{
    wmi_reg_write_single(AH_PRIVATE(ah)->hal_wmi_handle, reg,val);
}

u_int32_t __ahdecl 
ath_hal_wmi_reg_read(struct ath_hal *ah, u_int reg)
{
    u_int32_t val;
    u_int32_t cmd_status = -1;
    u_int32_t tmp = htonl(reg);

    cmd_status = wmi_cmd(AH_PRIVATE(ah)->hal_wmi_handle, 
        WMI_REG_READ_CMDID, 
        (u_int8_t *)&tmp, 
        sizeof(tmp),
        (u_int8_t *)&val,
        sizeof(val),
        100 /* timeout unit? */);

    if (cmd_status) {
        if (cmd_status != -1)
            ath_hal_printf(NULL, "Host : WMI COMMAND READ FAILURE stat = %x\n", cmd_status);

        return EIO;
    }

    return ntohl(val);
}

void __ahdecl 
ath_hal_wmi_reg_write(struct ath_hal *ah, u_int reg, u_int32_t val)
{
    if (AH_PRIVATE(ah)->ah_reg_write_buffer_flag)
        ath_hal_wmi_reg_write_delay(ah, reg, val); 
    else
        ath_hal_wmi_reg_write_single(ah, reg, val);
}


void __ahdecl 
ath_hal_wmi_reg_rmw(struct ath_hal *ah, u_int reg, u_int32_t clr, u_int32_t set)
{  

    wmi_reg_rmw(AH_PRIVATE(ah)->hal_wmi_handle, reg, clr, set);

}


void __ahdecl 
ath_hal_wmi_ps_set_state(struct ath_hal *ah, u_int16_t mode)
{
    u_int32_t val;
    u_int32_t cmd_status = -1;

    mode = htons(mode);
        
    cmd_status = wmi_cmd(AH_PRIVATE(ah)->hal_wmi_handle, 
        WMI_PS_SET_STATE_CMD, 
        (u_int8_t *)&mode, 
        sizeof(u_int16_t),
        (u_int8_t *) &val,
        sizeof(val),
        100 /* timeout unit? */);

    if (cmd_status) {
        if (cmd_status != -1) {
            ath_hal_printf(NULL, "Host : WMI COMMAND SET PS STATE FAILURE stat = %x\n", cmd_status);
        }
    }
    return;
}


