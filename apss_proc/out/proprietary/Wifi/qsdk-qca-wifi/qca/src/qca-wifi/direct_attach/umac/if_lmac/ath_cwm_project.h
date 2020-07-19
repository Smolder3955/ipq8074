/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
#ifndef _ATH_CWM_PROJECT_H_
#define _ATH_CWM_PROJECT_H_
#include "if_athvar.h"
#include "wlan_opts.h"
#include "project.h"
/*
 * Channel Width Management (CWM)
 */

/* printf */
#define CWM_DBG_PRINT(dev, fmt, ...) DPRINTF((struct ath_softc_net80211 *)dev, ATH_DEBUG_CWM, fmt, __VA_ARGS__)

/* Memory related functions */
#define CWM_ALLOC_STATE_STRUCT(dev, size) OS_MALLOC(dev, size, GFP_KERNEL)
#define CWM_ALLOC_INDIR_TBL(dev, size) OS_MALLOC(dev, size, GFP_KERNEL)
#define CWM_ALLOC_Q_ENTRY(dev, data_type, pData) data_type *pData = (data_type *)OS_MALLOC(dev, sizeof(data_type), GFP_ATOMIC)

#define CWM_HANDLE(dev) ATH_SOFTC_NET80211(dev)->sc_cwm

#define CWM_IEEE80211_EXTPROTMODE enum ieee80211_cwm_extprotmode
#define CWM_TO_IEEE80211_EXTPROTMODE(cw_val, ieee_val)\
    switch(cw_val)                                  \
    {                                               \
        case CWM_EXTPROTNONE:                       \
            ieee_val = IEEE80211_CWM_EXTPROTNONE;         \
            break;                                  \
        case CWM_EXTPROTCTSONLY:                    \
            ieee_val = IEEE80211_CWM_EXTPROTCTSONLY;      \
            break;                                  \
        case CWM_EXTPROTRTSCTS:                     \
            ieee_val = IEEE80211_CWM_EXTPROTRTSCTS;	    \
            break;                                  \
        default:                                    \
            ieee_val = IEEE80211_CWM_EXTPROTMAX;\
    }
#define IEEE80211_TO_CWM_EXTPROTMODE(ieee_val, cw_val)\
    switch(ieee_val)                        \
    {                                       \
        case IEEE80211_CWM_EXTPROTNONE:           \
            cw_val = CWM_EXTPROTNONE;       \
            break;                          \
        case IEEE80211_CWM_EXTPROTCTSONLY:        \
            cw_val = CWM_EXTPROTCTSONLY;    \
            break;                          \
        case IEEE80211_CWM_EXTPROTRTSCTS:         \
            cw_val = CWM_EXTPROTRTSCTS;	    \
            break;                          \
        default:                            \
            cw_val = CWM_EXTPROTMAX;        \
            break;                          \
    }


#define CWM_IEEE80211_EXTPROTSPACING enum ieee80211_cwm_extprotspacing
#define CWM_TO_IEEE80211_EXTPROTSPACING(cw_val, ieee_val)\
    switch(cw_val)                                      \
    {                                                   \
        case CWM_HT_EXTPROTSPACING_20:                  \
            ieee_val = IEEE80211_CWM_EXTPROTSPACING20;        \
            break;                                      \
        case CWM_HT_EXTPROTSPACING_25:                  \
            ieee_val = IEEE80211_CWM_EXTPROTSPACING25;        \
            break;                                      \
        default:                                        \
            ieee_val = IEEE80211_CWM_EXTPROTSPACINGMAX;       \
    }
#define IEEE80211_TO_CWM_EXTPROTSPACING(ieee_val, cw_val)\
    switch(ieee_val)                                \
    {                                               \
        case IEEE80211_CWM_EXTPROTSPACING20:              \
            cw_val = CWM_HT_EXTPROTSPACING_20;      \
            break;                                  \
        case IEEE80211_CWM_EXTPROTSPACING25:              \
            cw_val = CWM_HT_EXTPROTSPACING_25;      \
            break;                                  \
        default:                                    \
            cw_val = CWM_HT_EXTPROTSPACING_MAX;     \
            break;                                  \
    }


#define CWM_IEEE80211_MODE enum ieee80211_cwm_mode
#define CWM_TO_IEEE80211_MODE(cw_val, ieee_val)\
    switch(cw_val){                             \
        case CWM_MODE20:                        \
            ieee_val = IEEE80211_CWM_MODE20;          \
            break;                              \
        case CWM_MODE2040:                      \
            ieee_val = IEEE80211_CWM_MODE2040;        \
            break;                              \
        case CWM_MODE40:                        \
            ieee_val = IEEE80211_CWM_MODE40;          \
            break;                              \
        default:                                \
            ieee_val = IEEE80211_CWM_MODEMAX;         \
    }       
#define IEEE80211_TO_CWM_MODE(ieee_val, cw_val)\
     switch(ieee_val){                  \
        case IEEE80211_CWM_MODE20:            \
            cw_val = CWM_MODE20;        \
            break;                      \
        case IEEE80211_CWM_MODE2040:          \
            cw_val = CWM_MODE2040;      \
            break;                      \
        case IEEE80211_CWM_MODE40:            \
            cw_val = CWM_MODE40;        \
            break;                      \
        default:                        \
            cw_val = CWM_MODEMAX;       \
            break;                      \
    }
   

#define CWM_IEEE80211_WIDTH enum ieee80211_cwm_width
#define CWM_TO_IEEE80211_WIDTH(cw_val, ieee_val)\
    ((ieee_val) = (((cw_val) == CWM_WIDTH20) ? IEEE80211_CWM_WIDTH20 : IEEE80211_CWM_WIDTH40))
#define IEEE80211_TO_CWM_WIDTH(ieee_val, cw_val)\
    ((cw_val) = ((ieee_val) == IEEE80211_CWM_WIDTH20) ? CWM_WIDTH20 : CWM_WIDTH40)


int  ath_cwm_attach(struct ath_softc_net80211 *scn, struct ath_reg_parm *ath_conf_parm);
void ath_cwm_detach(struct ath_softc_net80211 *scn);
int  ath_cwm_ioctl(struct ath_softc_net80211 *scn, int cmd, caddr_t data);
u_int32_t ath_cwm_getextbusy(struct ath_softc_net80211 *scn);
enum ieee80211_cwm_width ath_cwm_getlocalchwidth(struct ieee80211com *ic);
HAL_HT_MACMODE ath_cwm_macmode(ieee80211_handle_t ieee);

/* Notifications */
void ath_cwm_up(void *arg, struct ieee80211vap *vap);
void ath_cwm_down(struct ieee80211vap *vap);
void ath_cwm_join(void *arg, struct ieee80211vap *vap);
void ath_cwm_newchwidth(struct ieee80211_node *ni);
void ath_cwm_txtimeout(struct ath_softc_net80211 *scn);

void ath_cwm_scan_start(struct ieee80211com *ic);
void ath_cwm_scan_end(struct ieee80211com *ic);

void ath_cwm_radio_disable(struct ath_softc_net80211 *scn);
void ath_cwm_radio_enable(struct ath_softc_net80211 *scn);

void ath_cwm_chwidth_change(struct ieee80211_node *ni);
void ath_cwm_switch_mode_static20(struct ieee80211com *ic);
void ath_cwm_switch_mode_dynamic2040(struct ieee80211com *ic);
void ath_cwm_switch_to40(struct ieee80211com *ic);
void ath_cwm_switch_to20(struct ieee80211com *ic);

#endif /* _ATH_CWM_PROJECT_H_ */
