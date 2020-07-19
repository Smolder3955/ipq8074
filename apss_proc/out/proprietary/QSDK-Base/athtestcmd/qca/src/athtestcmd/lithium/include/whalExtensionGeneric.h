/* 
 * Copyright (c) 2016, 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2016 Qualcomm Atheros, Inc. 
 *
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
*/ 

#if !defined(_WHAL_EXTENSION_GENERIC_H)
#define  _WHAL_EXTENSION_GENERIC_H

#include "wal.h"
#include "utf_dev.h"

// defines
#define MAX_UTF_CAL_WRITE_REG 128
#define TX_PWR_DEFAULT        63   // 31.5 dBm. HW limit

extern void tcmd_main(void);
#define utfMain      tcmd_main
#define utfTxCmd     tcmd_cont_tx_cmd 
#define utfRxCmd     tcmd_cont_rx_cmd 
#define utfPmCmd     tcmd_pm_cmd 
#define _utfRxStat_t tcmd_rx_stat_t

typedef struct _postWhalResetSettings {
    A_UINT32  addrMode[MAX_UTF_CAL_WRITE_REG/*16*/];
    A_UINT32  value[MAX_UTF_CAL_WRITE_REG/*16*/];
    A_UINT32  mask[MAX_UTF_CAL_WRITE_REG/*16*/];
    void      (*pPostProc)(void *pPhyHandle, void* pParms);                   // a different func can be installed for sweeping power in bench characterization, e.g.
    A_UINT32  numOfRegs_AlsoMeantPostProcInstalledIfNonZero;
    void      (*pPostSetPwrProc)(void *pPhyHandle, A_BOOL is2GHz, A_UINT16 pcDac);                   // a different func can be installed for sweeping power in bench characterization, e.g.
} _POST_WHAL_RESET_SETTINGS;

// Public data
extern _POST_WHAL_RESET_SETTINGS _PostWhalResetSettings;

// Exported APIs
extern A_UINT16 ftmSetTxPowerLimitPlus(wlan_tcmd_dev_t *dev, A_UINT16 pwrGain); 

#endif //#if !defined(_WHAL_EXTENSION_GENERIC_H)

