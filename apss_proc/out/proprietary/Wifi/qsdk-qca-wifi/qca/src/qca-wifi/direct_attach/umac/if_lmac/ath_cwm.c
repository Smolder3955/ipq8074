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
 
/*
 * CWM (Channel Width Management) 
 *
 *
 * Synchronization strategy:
 *	A single spinlock (ac_lock) is used to protect access to shared data structures:
 *		- CWM event queue
 *		- CWM state 
 *
 *	An event queue is used to serialize events from 
 *		- DPC context (CST interrupt, channel width notification)
 *		- CWM timer context (extension channel sensing)
 *		- OID/ioctl context  
 *
 *	All 3 contexts (DPC, timer, OID) can run concurrently on SMP machines.
 *	An ownership flag is used to make sure only one thread context
 *      can process the event queue.  No lock is held while processing events.
 */
#include "ath_cwm.h"
#include "ath_cwm_project.h"
#include "ath_cwm_cb.h"

/*
 *----------------------------------------------------------------------
 * local function declarations
 *----------------------------------------------------------------------
 */
static void cwm_inithwstate(struct ath_cwm *cw_struct);
static void cwm_start(COM_DEV *dev);

static void cwm_queueevent(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg);
static void cwm_statetransition(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg);
static TIMER_FUNC(cwm_timer);
static int  cwm_extchbusy(struct ath_cwm *cw_struct);

static void cwm_action_invalid(struct ath_cwm *cw_struct, void *arg);
static void cwm_action_mac40to20(struct ath_cwm *cw_struct, void *arg);
static void cwm_action_mac20to40(struct ath_cwm *cw_struct, void *arg);
static void cwm_action_phy40to20(struct ath_cwm *cw_struct, void *arg);
static void cwm_action_phy20to40(struct ath_cwm *cw_struct, void *arg);
static void cwm_action_requeue(struct ath_cwm *cw_struct, void *arg);

static void cwm_debuginfo(struct ath_cwm *cw_struct);

/* Global variable */
struct ath_cwm_statetransition ath_cwm_stt[ATH_CWM_STATE_MAX][ATH_CWM_EVENT_MAX] = {

    /* ATH_CWM_STATE_EXT_CLEAR */
    {
    /* ATH_CWM_EVENT_TXTIMEOUT  ==> */ {ATH_CWM_STATE_EXT_BUSY,     cwm_action_mac40to20},
    /* ATH_CWM_EVENT_EXTCHCLEAR ==> */ {ATH_CWM_STATE_EXT_CLEAR,    NULL},
    /* ATH_CWM_EVENT_EXTCHBUSY  ==> */ {ATH_CWM_STATE_EXT_BUSY,     cwm_action_mac40to20},
    /* ATH_CWM_EVENT_EXTCHSTOP  ==> */ {ATH_CWM_STATE_EXT_CLEAR,    cwm_action_invalid},
    /* ATH_CWM_EVENT_EXTCHRESUME==> */ {ATH_CWM_STATE_EXT_CLEAR,    cwm_action_invalid},
    /* ATH_CWM_EVENT_DESTCW20   ==> */ {ATH_CWM_STATE_EXT_CLEAR,    cwm_action_requeue},
    /* ATH_CWM_EVENT_DESTCW40   ==> */ {ATH_CWM_STATE_EXT_CLEAR,    NULL},
    },

    /* ATH_CWM_STATE_EXT_BUSY */
    {
    /* ATH_CWM_EVENT_TXTIMEOUT  ==> */ {ATH_CWM_STATE_EXT_BUSY,     NULL},
    /* ATH_CWM_EVENT_EXTCHCLEAR ==> */ {ATH_CWM_STATE_EXT_CLEAR,    cwm_action_mac20to40},
    /* ATH_CWM_EVENT_EXTCHBUSY  ==> */ {ATH_CWM_STATE_EXT_BUSY,     NULL},
#ifdef ATH_CWM_PHY_DISABLE_TRANSITIONS
    /* ATH_CWM_EVENT_EXTCHSTOP  ==> */ {ATH_CWM_STATE_EXT_BUSY, NULL},
#else
    /* ATH_CWM_EVENT_EXTCHSTOP  ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE, cwm_action_phy40to20},
#endif    
    /* ATH_CWM_EVENT_EXTCHRESUME==> */ {ATH_CWM_STATE_EXT_BUSY,     cwm_action_invalid},
    /* ATH_CWM_EVENT_DESTCW20   ==> */ {ATH_CWM_STATE_EXT_BUSY,     NULL},
    /* ATH_CWM_EVENT_DESTCW40   ==> */ {ATH_CWM_STATE_EXT_BUSY,     NULL},
    },

    /* ATH_CWM_STATE_EXT_UNAVAILABLE */
    {
    /* ATH_CWM_EVENT_TXTIMEOUT  ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE,  NULL},
    /* ATH_CWM_EVENT_EXTCHCLEAR ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE,  cwm_action_invalid},
    /* ATH_CWM_EVENT_EXTCHBUSY  ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE,  cwm_action_invalid},
    /* ATH_CWM_EVENT_EXTCHSTOP  ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE,  cwm_action_invalid},
    /* ATH_CWM_EVENT_EXTCHRESUME==> */ {ATH_CWM_STATE_EXT_BUSY,         cwm_action_phy20to40},
    /* ATH_CWM_EVENT_DESTCW20   ==> */ {ATH_CWM_STATE_EXT_UNAVAILABLE,  NULL},
    /* ATH_CWM_EVENT_DESTCW40   ==> */ {ATH_CWM_STATE_EXT_BUSY,         cwm_action_phy20to40},
    }

};


/*
 *----------------------------------------------------------------------
 * static variables
 *----------------------------------------------------------------------
 */
#if ATH_DEBUG
static const char *ath_cwm_statename[ATH_CWM_STATE_MAX] = {
	"EXT CLEAR",		/* ATH_CWM_STATE_EXT_CLEAR */
	"EXT BUSY",		/* ATH_CWM_STATE_EXT_BUSY */
	"EXT UNAVAIL",		/* ATH_CWM_STATE_EXT_UNAVAILABLE */
};
   
static const char *ath_cwm_eventname[ATH_CWM_EVENT_MAX] = {
	"TXTIMEOUT", 		/* ATH_CWM_EVENT_TXTIMEOUT */
	"EXTCHCLEAR", 		/* ATH_CWM_EVENT_EXTCHCLEAR */
	"EXTCHBUSY", 		/* ATH_CWM_EVENT_EXTCHBUSY */
	"EXTCHSTOP", 		/* ATH_CWM_EVENT_EXTCHSTOP */
	"EXTCHRESUME", 		/* ATH_CWM_EVENT_EXTCHRESUME */
	"DESTCW20", 		/* ATH_CWM_EVENT_DESTCW20 */
	"DESTCW40", 		/* ATH_CWM_EVENT_DESTCW40 */
};
#endif


/*
 *----------------------------------------------------------------------
 * public functions
 *----------------------------------------------------------------------
 */
#ifndef DISABLE_FUNCTION_INDIRECTION
CWM_INTERNAL_FN _cwm_int_fn = {
    cwm_inithwstate,
    cwm_init,
    cwm_start,
    cwm_stop,
    cwm_queueevent,
    cwm_statetransition,
    cwm_timer,
    cwm_extchbusy,
    cwm_action_invalid,
    cwm_action_mac40to20,
    cwm_action_mac20to40,
    cwm_action_phy40to20,
    cwm_action_phy20to40,
    cwm_action_requeue,
    cwm_debuginfo,
};
#endif

/*
 * Device Attach
 */
struct ath_cwm *
cwm_attach(void *wlandev, osdev_t osdev, struct ath_cwm_conf *cwm_conf_parm)
{
    struct ath_cwm *cw_struct;

	CWM_DBG_PRINT(wlandev, "%s\n", __func__);

	cw_struct = (struct ath_cwm *)CWM_ALLOC_STATE_STRUCT(osdev, 
                                      sizeof(struct ath_cwm));
	if (cw_struct == NULL) {
		CWM_DBG_PRINT(wlandev, "%s: no memory for cwm attach\n", __func__);
		return NULL;
	}

	OS_MEMZERO(cw_struct, sizeof(struct ath_cwm));

    /* Init OS hdl and WLAN dev hdl */
    cw_struct->osdev = osdev;
    cw_struct->wlandev = wlandev;

	/* ieee80211 Layer - Default Configuration */
	cw_struct->cw_mode 		= CWM_MODE20;
	cw_struct->cw_extoffset 	= 0;
	cw_struct->cw_extprotmode	= CWM_EXTPROTNONE;
	cw_struct->cw_extprotspacing 	= CWM_HT_EXTPROTSPACING_25;
	cw_struct->cw_enable      	= cwm_conf_parm->cw_enable;
	cw_struct->cw_extbusythreshold = ATH_CWM_EXTCH_BUSY_THRESHOLD;
	cw_struct->cw_width = CWM_WIDTH20;
    cw_struct->cw_ht40allowed = 1;

	/* Allocate resources */
	TIMER_INIT(osdev, &cw_struct->ac_timer,
			     CWM_timer_fn_ptr, cw_struct);
	spin_lock_init(&cw_struct->ac_lock);
	TAILQ_INIT(&cw_struct->ac_eventq);

	/* Atheros layer initialization */
	cw_struct->ac_running 	= 0;
	cw_struct->ac_timer_statetime = 0;
	cw_struct->ac_timer_prevstate = ATH_CWM_STATE_EXT_CLEAR;
	cw_struct->ac_vextch 		= 0;
	cw_struct->ac_vextchbusy 	= 0;
	cw_struct->ac_extchbusyper 	= 0;

	/* default state */
	cw_struct->ac_state  		= ATH_CWM_STATE_EXT_CLEAR;
	CWM_inithwstate(cw_struct);
			       
	return cw_struct;
}


void cwm_set_mode(COM_DEV *dev, CWM_IEEE80211_MODE ieee_mode)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    IEEE80211_TO_CWM_MODE(ieee_mode, cw_struct->cw_mode);
    CWM_DBG_PRINT(cw_struct->wlandev, "cwmode = %d", cw_struct->cw_mode);
}

CWM_IEEE80211_MODE cwm_get_mode(COM_DEV *dev)
{
    CWM_IEEE80211_MODE mode;
    CWM_TO_IEEE80211_MODE(CWM_HANDLE(dev)->cw_mode, mode);
    return mode;
}

CWM_IEEE80211_EXTPROTMODE cwm_get_extprotmode(COM_DEV *dev)
{ 
    CWM_IEEE80211_EXTPROTMODE extprotmode;
    CWM_TO_IEEE80211_EXTPROTMODE(CWM_HANDLE(dev)->cw_extprotmode, extprotmode);
    return extprotmode;
}

CWM_IEEE80211_EXTPROTSPACING cwm_get_extprotspacing(COM_DEV *dev)
{ 
    CWM_IEEE80211_EXTPROTSPACING extprotsp;
    CWM_TO_IEEE80211_EXTPROTSPACING(CWM_HANDLE(dev)->cw_extprotspacing, extprotsp);
    return extprotsp;
}

u_int32_t cwm_get_enable(COM_DEV *dev)
{
    return CWM_HANDLE(dev)->cw_enable;
}

u_int32_t cwm_get_extbusythreshold(COM_DEV *dev)
{ 
    return CWM_HANDLE(dev)->cw_extbusythreshold;
}

CWM_IEEE80211_WIDTH cwm_get_width(COM_DEV *dev)
{
    CWM_IEEE80211_WIDTH width;
    CWM_TO_IEEE80211_WIDTH(CWM_HANDLE(dev)->cw_width, width);
    return width;
}

int8_t cwm_get_extoffset(COM_DEV *dev)
{ 
    return CWM_HANDLE(dev)->cw_extoffset;
}

void cwm_set_extprotmode(COM_DEV *dev, CWM_IEEE80211_EXTPROTMODE val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    IEEE80211_TO_CWM_EXTPROTMODE(val, cw_struct->cw_extprotmode);
    CWM_DBG_PRINT(cw_struct->wlandev, "cwextprotmode = %d", cw_struct->cw_extprotmode);
}

void cwm_set_extprotspacing(COM_DEV *dev, CWM_IEEE80211_EXTPROTSPACING val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    IEEE80211_TO_CWM_EXTPROTSPACING(val, cw_struct->cw_extprotspacing);
    CWM_DBG_PRINT(cw_struct->wlandev, "cwextprotspacing = %d", cw_struct->cw_extprotspacing);
}

void cwm_set_enable(COM_DEV *dev, u_int32_t val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    cw_struct->cw_enable = val;
    CWM_DBG_PRINT(cw_struct->wlandev, "cwenable = %d", cw_struct->cw_enable);
}

void cwm_set_extbusythreshold(COM_DEV *dev, u_int32_t val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    cw_struct->cw_extbusythreshold = val;
    CWM_DBG_PRINT(cw_struct->wlandev, "cwextbusythreshold= %d", cw_struct->cw_extbusythreshold);
}

void cwm_set_width(COM_DEV *dev, CWM_IEEE80211_WIDTH val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    IEEE80211_TO_CWM_WIDTH(val, cw_struct->cw_width);
    CWM_DBG_PRINT(cw_struct->wlandev, "cwwidth = %d", cw_struct->cw_width);
}

void cwm_set_extoffset(COM_DEV *dev, int8_t val)
{ 
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    cw_struct->cw_extoffset = val;
    CWM_DBG_PRINT(cw_struct->wlandev, "cwextoffset = %d", cw_struct->cw_extoffset);
}

/*
 * Device Detach
 */
void
cwm_detach(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    struct ath_cwm_eventq_entry *eventqentry;

    if (cw_struct == NULL) {
        return;
    }

    CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

    /* Cleanup resources */
    TIMER_CANCEL(&cw_struct->ac_timer, CANCEL_NO_SLEEP);
    TIMER_FREE(&cw_struct->ac_timer);
    spin_lock_destroy(&cw_struct->ac_lock);

    while (!TAILQ_EMPTY(&cw_struct->ac_eventq)) {
        eventqentry = TAILQ_FIRST(&cw_struct->ac_eventq);
        TAILQ_REMOVE(&cw_struct->ac_eventq, eventqentry, ce_entry);
        OS_FREE(eventqentry);
        eventqentry = NULL;
	}

    /* free CWM memory */
    OS_FREE(cw_struct);
}


void
cwm_scan(COM_DEV *dev, enum cwm_opmode opmode)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    switch (opmode){
        case CWM_M_HOSTAP:
        case CWM_M_STA:
            /* 20 MHz mode when scanning */
            /* XXX: Why is there no call to a HAL fn to set MAC to
             * HT20?
             *          * */
            cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_20;
            /* stop CWM state machine */
            CWM_stop(dev);
            break;
        default:
            break;
    }
}
void
cwm_up(COM_DEV *dev, enum cwm_opmode opmode)
{
    switch (opmode) {
    case CWM_M_HOSTAP:
    case CWM_M_STA:
        CWM_start(dev);
        break;
    default:
        break;
    }
}

void
cwm_join(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    /* channel information is now valid */
    if ((cw_struct->cw_mode == CWM_MODE2040) ||
            (cw_struct->cw_mode == CWM_MODE40)) {
        if (CWM_HT40_ALLOWED_CB(cw_struct)) {
            /* set channel width to 40 MHz. 
             * extension offset is assumed to be set correctly
             * by config/auto channel select
             */
            cw_struct->cw_width = CWM_WIDTH40;
        } else {
            CWM_DBG_PRINT(cw_struct->wlandev, "%s: BSS/Regulatory does not allow 40MHz.\n", __func__);
            cw_struct->cw_width = CWM_WIDTH20;
            cw_struct->cw_extoffset = 0;
        }
        /* init hardware state */
        CWM_inithwstate(cw_struct);
    }

}

void
cwm_down(COM_DEV *dev, enum cwm_opmode opmode)
{
    switch (opmode) {
        case CWM_M_HOSTAP:
        case CWM_M_STA:
            CWM_stop(dev);
            CWM_init(dev);

            break;
        default:
            break;

    }
}

/*
 * Radio disable notification
 *
 */
void
cwm_radio_disable(COM_DEV *dev)
{
    //Skip radio-disable operation if cwm handle is NULL to avoid BSOD
    //ToDo - Implement CWM module
    if (!CWM_HANDLE(dev))
    {
        KASSERT(FALSE, ("%s: CWM not implemented yet, radio-disable operation is skipped \n", __func__));
        return;
    }
    
	CWM_DBG_PRINT(CWM_HANDLE(dev)->wlandev, "%s\n", __func__);

	/* stop CWM state machine and timer */
	CWM_stop(dev);
}

void cwm_switch_mode_static20(struct ath_cwm *cw_struct)
{
    if (cw_struct->cw_width!= CWM_WIDTH20) {
        cw_struct->cw_width = CWM_WIDTH20;
        cw_struct->cw_mode = CWM_MODE20;
        CWM_action_phy40to20(cw_struct, NULL);
        CWM_action_mac40to20(cw_struct, NULL);
    } 
}

void cwm_switch_mode_dynamic2040(struct ath_cwm *cw_struct)
{
    if (cw_struct->cw_width == CWM_WIDTH20) {
        cw_struct->cw_width = CWM_WIDTH40;
        cw_struct->cw_mode = CWM_MODE2040;
        CWM_action_phy20to40(cw_struct, NULL);
        CWM_action_mac20to40(cw_struct, NULL);
    }
}

/*
 * Node has changed channel width 
 *
 */
void
cwm_chwidth_change(struct ath_cwm *cw_struct, void *pNode, enum cwm_width chwidth, 
        enum cwm_opmode opmode)
{
	enum ath_cwm_event event;
	
    if (cw_struct == NULL) {
		return;
	}

	CWM_DBG_PRINT(cw_struct->wlandev, "%s: chwidth = %d\n", __func__, chwidth);

        if((chwidth == CWM_WIDTH40) &&
	   (cw_struct->ac_hwstate.ht_macmode == CWM_HT_MACMODE_20)) {
            CWM_DBG_PRINT(cw_struct->wlandev,
			"%s: chwidth mismatch, mac mode=%s, chwidth=%s\n",__func__,
                        cw_struct->ac_hwstate.ht_macmode == CWM_HT_MACMODE_20 ?
			"HT20" : "HT40", chwidth == CWM_WIDTH40 ? "HT40" : "HT20");
	    return; 
        }


    cw_struct->cw_width = chwidth;

    if(opmode == CWM_M_STA){
        if(chwidth == CWM_WIDTH40) {
            cw_struct->cw_ht40allowed = 1;
        }
        else {
            cw_struct->cw_ht40allowed = 0;
        }
    }
        
    if (cw_struct->ac_running) {
        event = (chwidth == CWM_WIDTH40) ?
            ATH_CWM_EVENT_DESTCW40 : ATH_CWM_EVENT_DESTCW20;
        CWM_queueevent(cw_struct, event, pNode);
    }

    /* Fix for EV126352: Notify rate control of width change (select new rate table)
     * Replaced the call to CWM_RATE_UPDATENODE_CB with CWM_RATE_UPDATEALLNODES_CB.
     * The former updates a single VAP and may not do the correct thing where there
     * are multiple VAPs on a single hardware. There could be a corner case where
     * there is inconsistency between a VAP configuration and the HW state.
     */
    CWM_RATE_UPDATEALLNODES_CB(cw_struct);

    /*
    * If channel width change is issued by station then Send Action Frame for all AP Vap
    */
    if(CWM_M_STA == opmode){
       CWM_SENDACTIONMGMT_APONLY_CB(cw_struct);
    }
}

 /*
  * get ext channel busy (percentage) 
  */
 u_int32_t
 cwm_getextbusy(struct ath_cwm *cw_struct)
 {
	if (cw_struct == NULL) {
		return 0;
	}
 
	return cw_struct->ac_extchbusyper;
 }

/*
 * Tx timeout interrupt 
 *
 */
void
cwm_txtimeout(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);

    if (cw_struct == NULL) {
		return;
	}

	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

	if (!cw_struct->ac_running) {
	      return;
	}

	CWM_queueevent(cw_struct, ATH_CWM_EVENT_TXTIMEOUT, NULL);
}

/*
 * get hw state 
 *
 */
void
cwm_gethwstate(struct ath_cwm *cw_struct, struct ath_cwm_hwstate *cwm_hwstate)
{
    if (cw_struct == NULL) {
		return;
	}

	spin_lock(&cw_struct->ac_lock);

	*cwm_hwstate = cw_struct->ac_hwstate;

	spin_unlock(&cw_struct->ac_lock);
}


enum cwm_ht_macmode
cwm_macmode(struct ath_cwm *cw_struct)
{
    /* Check if valid CWM HW state exists */
    if (cw_struct && CWM_HT40_ALLOWED_CB(cw_struct)) {
        return (cw_struct->ac_hwstate.ht_macmode);
    } else {
        return CWM_HT_MACMODE_20;
    }   
}


/*
 *----------------------------------------------------------------------
 * local functions
 *----------------------------------------------------------------------
 */

/*
 * Acquire event queue ownership
 */
static INLINE u_int8_t
ATH_CWM_ACQUIRE_EVENT_QUEUE_OWNERSHIP(struct ath_cwm *ac) 
{
	return (OS_ATOMIC_CMPXCHG(&ac->ac_eventq_owner, 0, 1) == 0);
}

/*
 * Release event queue ownership
 */
static INLINE int32_t
ATH_CWM_RELEASE_EVENT_QUEUE_OWNERSHIP(struct ath_cwm *ac) 
{
	return (OS_ATOMIC_CMPXCHG(&ac->ac_eventq_owner, 1, 0));
}

/*
 * CWM init hardware state based on current configuration
 *
 */
static void 
cwm_inithwstate(struct ath_cwm *cw_struct)
{
	spin_lock(&cw_struct->ac_lock);

	/* mac and phy mode */
	switch (cw_struct->cw_width) {
	case CWM_WIDTH40:
		cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_2040;
		break;

	case CWM_WIDTH20:
	default:
		cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_20;
		break;
	}

	/* extension channel protection spacing */				   
    /* XXX: Where is ac_hwstate.ht_extprotspacing used? */
	switch (cw_struct->cw_extprotspacing) {
	case CWM_HT_EXTPROTSPACING_20:
		cw_struct->ac_hwstate.ht_extprotspacing = CWM_HT_EXTPROTSPACING_20;
		break;
	case CWM_HT_EXTPROTSPACING_25:
	default:
		cw_struct->ac_hwstate.ht_extprotspacing = CWM_HT_EXTPROTSPACING_25;
		break;
	}

    spin_unlock(&cw_struct->ac_lock);
}

/*
 * CWM Init
 *
 * - initialize state machine based on current configuration
 * - called by ath_init()
 *
 */
void cwm_init(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    enum cwm_phymode chanmode;

    if (cw_struct == NULL) {
        return;
    }

    CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

    chanmode = CWM_GET_CURCH_MODE_CB(cw_struct);

    spin_lock(&cw_struct->ac_lock);

    /* Validate configuration */
    switch(chanmode) {
        case CWM_PHYMODE_HT40PLUS:
            if (cw_struct->cw_enable) {
                cw_struct->cw_mode = CWM_MODE2040;
            }
            else {
                cw_struct->cw_mode = CWM_MODE40;
            }
            cw_struct->cw_extoffset = 1;
            cw_struct->cw_width = CWM_WIDTH40;
            break;
        case CWM_PHYMODE_HT40MINUS:
            if (cw_struct->cw_enable) {
                cw_struct->cw_mode = CWM_MODE2040;
            }
            else {
                cw_struct->cw_mode = CWM_MODE40;
            }
            cw_struct->cw_extoffset = -1;
            cw_struct->cw_width = CWM_WIDTH40;
            break;
        default:
            cw_struct->cw_mode = CWM_MODE20;
            cw_struct->cw_extoffset = 0;
            cw_struct->cw_width = CWM_WIDTH20;
    }
    spin_unlock(&cw_struct->ac_lock);
    CWM_inithwstate(cw_struct);

    spin_lock(&cw_struct->ac_lock);

    /* Display configuration */
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: cw_mode %d\n", __func__, cw_struct->cw_mode);
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: cw_extoffset %d\n", __func__, cw_struct->cw_extoffset);
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: cw_extprotmode %d\n", __func__, cw_struct->cw_extprotmode);
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: cw_extprotspacing %d\n", __func__, cw_struct->cw_extprotspacing);
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: cw_enable %d\n", __func__, cw_struct->cw_enable);

    spin_unlock(&cw_struct->ac_lock);
}

/*
 * CWM Start
 */
static void
cwm_start(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);

	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

	if (cw_struct->ac_running) {
		return;
	}

	if (cw_struct->cw_mode != CWM_MODE2040) {
		CWM_DBG_PRINT(cw_struct->wlandev, "%s: CWM is not in dynamic 2040mode \n", __func__);
		return;
	}

	if(!CWM_HT40_ALLOWED_CB(cw_struct)) {
		CWM_DBG_PRINT(cw_struct->wlandev, "%s: CWM state machine disabled because of BSS/regulatory restrictions\n", __func__);
		return;
	}

    spin_lock(&cw_struct->ac_lock);
    /* Start CWM */
	cw_struct->ac_running = 1;

	/* Initial state */
	if (cw_struct->cw_width == CWM_WIDTH40) {
		cw_struct->ac_state = ATH_CWM_STATE_EXT_CLEAR;
	} else {
		cw_struct->ac_state = ATH_CWM_STATE_EXT_BUSY;
	}

	CWM_debuginfo(cw_struct);

    spin_unlock(&cw_struct->ac_lock);

	TIMER_START(&cw_struct->ac_timer, ATH_CWM_TIMER_INTERVAL_MS);
}


/*
 * CWM Stop
 */
void cwm_stop(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
	
    if (cw_struct == NULL) {
		return;
	}

	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);
    
	if (cw_struct->ac_running) {
		cw_struct->ac_running = 0;
		TIMER_CANCEL(&cw_struct->ac_timer, CANCEL_NO_SLEEP);
	}
}

#if ATH_RESET_SERIAL
typedef void (*cwmCallback)(void *) ;
int ath_cwm_queue_event(osdev_t sc_osdev, cwmCallback func, void *arg);

static void
cwm_callback(void *arg)
{
    struct ath_cwm_eventq_entry *eventqentry = arg;
    
     CWM_statetransition(eventqentry->ce_extra_arg,
            eventqentry->ce_event , eventqentry->ce_arg);

            /* Free event queue entry */
     OS_FREE(eventqentry);
}
static void
cwm_queueevent(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg)
{
    int ret;
    /* Allocate event queue entry */
    CWM_ALLOC_Q_ENTRY(cw_struct->osdev, struct ath_cwm_eventq_entry, eventqentry);

    if (eventqentry == NULL) {
        return;
    }

#if ATH_DEBUG
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: %s\n", __func__, ath_cwm_eventname[event]);
#endif

    eventqentry->ce_event = event;
    eventqentry->ce_arg   = arg;
    eventqentry->ce_extra_arg = cw_struct;

    ret = ath_cwm_queue_event(((struct ath_softc_net80211 *)cw_struct->wlandev)->sc_osdev,
                cwm_callback, eventqentry);
    if (ret) {//queue failed
        OS_FREE(eventqentry);
#if ATH_DEBUG
        CWM_DBG_PRINT(cw_struct->wlandev, "queue %s: %s failed\n",
        __func__, ath_cwm_eventname[event]);
    
#endif
    }
  
}

#else
/*
 * CWM queue event
 *
 * - queue and process events. This will serialize all CWM events without
 *   holding a spinlock.
 * - function may called from DPC/timer/passive contexts
 *
 */
static void
cwm_queueevent(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg)
{
    /* Allocate event queue entry */
    CWM_ALLOC_Q_ENTRY(cw_struct->osdev, struct ath_cwm_eventq_entry, eventqentry);

    if (eventqentry == NULL) {
        return;
    }

#if ATH_DEBUG
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: %s\n", __func__, ath_cwm_eventname[event]);
#endif

    eventqentry->ce_event = event;
    eventqentry->ce_arg   = arg;

    /* Queue event */
    spin_lock(&cw_struct->ac_lock);
    TAILQ_INSERT_TAIL(&cw_struct->ac_eventq, eventqentry, ce_entry);
    spin_unlock(&cw_struct->ac_lock);

    /* Try to claim event queue ownership */
    if (ATH_CWM_ACQUIRE_EVENT_QUEUE_OWNERSHIP(cw_struct)) {

        /* 
         * We own the event queue => process all events. 
         * This enforces serialization of events 
         */
        while (!TAILQ_EMPTY(&cw_struct->ac_eventq)) {

            /* Get event queue entry */
            spin_lock(&cw_struct->ac_lock);
            eventqentry = TAILQ_FIRST(&cw_struct->ac_eventq);
            TAILQ_REMOVE(&cw_struct->ac_eventq, eventqentry, ce_entry);
            spin_unlock(&cw_struct->ac_lock);
            /* Process event queue entry (lock is _NOT_ held) */
            CWM_statetransition(cw_struct, eventqentry->ce_event , eventqentry->ce_arg);

            /* Free event queue entry */
            OS_FREE(eventqentry);
            eventqentry = NULL;
        }

        ATH_CWM_RELEASE_EVENT_QUEUE_OWNERSHIP(cw_struct);
    }
}

#endif
/*
 * CWM State Transition
 *
 * Assumptions: serialized by caller
 * 
 */
static void
cwm_statetransition(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg)
{
    const struct ath_cwm_statetransition 	*st;
    static const struct ath_cwm_statetransition apst = {ATH_CWM_STATE_EXT_UNAVAILABLE,
        NULL}; /* special case for AP */

    KASSERT(cw_struct->ac_running, ("%s: event received but cwm fsm not running\n", __func__));

    KASSERT(event < ATH_CWM_EVENT_MAX, ("%s: invalid event %d\n", __func__, event));

    /* Input:  current state, event 
     * Output: new state, action    
     */
    st =  &ath_cwm_stt[cw_struct->ac_state][event];

    /* A little hacky, but avoids having a separate state transition table for the AP */
    if ((CWM_GET_IC_OPMODE_CB(cw_struct) == CWM_M_HOSTAP) && (cw_struct->ac_state == ATH_CWM_STATE_EXT_UNAVAILABLE) 
            && (event == ATH_CWM_EVENT_DESTCW40)) {
        st = &apst;
    }

#if ATH_DEBUG
    CWM_DBG_PRINT(cw_struct->wlandev, "%s: Event %s. State %s => %s\n", __func__,
            ath_cwm_eventname[event], ath_cwm_statename[cw_struct->ac_state], ath_cwm_statename[st->ct_newstate]);
#endif

    /* update state */
    cw_struct->ac_state = st->ct_newstate;

    /* associated action (if any) */
    if (st->ct_action != NULL) {
        st->ct_action(cw_struct, arg);
    } else {
        CWM_DBG_PRINT(cw_struct->wlandev, "%s: no associated action\n", __func__);
    }
}



/*
 * CWM Timer
 * 
 * - monitor the extension channel 
 * - generate CWM events based on extension channel activity
 *
 * Return 0 to reschedule timer
 */
static TIMER_FUNC(cwm_timer)
{
    int			extchbusy = 0, persistentstate = 0;
    struct ath_cwm *cw_struct = context;

    CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

    if((!cw_struct->ac_running) || (cw_struct->cw_mode != CWM_MODE2040) || (!cw_struct->cw_ht40allowed)
        || (CWM_GET_WLAN_SCAN_IN_PROGRESS_CB(cw_struct))) {
        TIMER_RETURN(1);
    }

    /* monitor extension channel */
    if (cw_struct->ac_state != ATH_CWM_STATE_EXT_UNAVAILABLE) {
        extchbusy = CWM_extchbusy(cw_struct);
    }

    if ( extchbusy == -1 ){
        /* "invalid extch " */
        TIMER_RETURN(0);
    } 

    /* check for same state for a period of time */
    if (cw_struct->ac_state == cw_struct->ac_timer_prevstate) {
        cw_struct->ac_timer_statetime += ATH_CWM_TIMER_INTERVAL_MS;
        if (cw_struct->ac_timer_statetime >= ATH_CWM_TIMER_EXTCHSENSING_MS) {
            persistentstate = 1;
        }
    } else {
        /* reset counter */
        cw_struct->ac_timer_statetime = 0;
        cw_struct->ac_timer_prevstate = cw_struct->ac_state;
    }

    switch (cw_struct->ac_state) {
        case ATH_CWM_STATE_EXT_CLEAR:
            if (extchbusy) {
                CWM_queueevent(cw_struct, ATH_CWM_EVENT_EXTCHBUSY, NULL);
            }
            break;
        case ATH_CWM_STATE_EXT_BUSY:
            if (extchbusy) {
                if (persistentstate) {
                    CWM_queueevent(cw_struct, ATH_CWM_EVENT_EXTCHSTOP, NULL);
                }
            } else {
                CWM_queueevent(cw_struct, ATH_CWM_EVENT_EXTCHCLEAR, NULL);
            }
            break;
        case ATH_CWM_STATE_EXT_UNAVAILABLE:
            if (persistentstate) {
                CWM_queueevent(cw_struct, ATH_CWM_EVENT_EXTCHRESUME, NULL);
            }
            break;
        default:
            CWM_DBG_PRINT(cw_struct->wlandev, "%s: invalid state, %d\n", __func__, cw_struct->ac_state);
            break;

    }

    TIMER_RETURN(0);

}

/*
 * CWM Extension Channel Busy Check
 * 
 * - return 1  if extension channel busy,
 *		    0  if extension channel clear,
 *          -1 if invalid extension channel estimate.
 */
static int
cwm_extchbusy(struct ath_cwm *cw_struct)
{
    int busy 	= 0;
    int extchbusy;
    /* debugging - virtual extension channel sensing */
    if (cw_struct->ac_vextch) {
        return cw_struct->ac_vextchbusy;
    }

    extchbusy = CWM_GET_EXTCHBUSYPER_CB(cw_struct);
    if ( extchbusy >= 0) {
        /* Extension Channel busy (0-100%) */
        cw_struct->ac_extchbusyper = extchbusy;
        if (cw_struct->ac_extchbusyper > cw_struct->cw_extbusythreshold) {
            busy = 1;
        }
    } else {
        /* invalid extch estimate */
        busy = -1;
    } 

    return busy;
}

/*
 * Actions
 *
 * Note: All actions are serialized by caller
 *
 */

/*
 * Action: Invalid state transition
 */
static void
cwm_action_invalid(struct ath_cwm *cw_struct, void *arg)
{
	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);
}

void
cwm_switch_to20(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    CWM_DBG_PRINT(cw_struct->wlandev,
            "%s: Switching MAC from 20/40 to 20\n", __func__);
    CWM_stop(dev); /* stop the cwm timer */
    CWM_action_mac40to20(cw_struct, (void *)NULL);
}

void cwm_switch_to40(COM_DEV *dev)
{
    struct ath_cwm *cw_struct = CWM_HANDLE(dev);
    CWM_DBG_PRINT(cw_struct->wlandev,
            "%s: Switching MAC from 20 to 20/40\n", __func__);
    CWM_action_mac20to40(cw_struct, (void *)NULL);
    CWM_start(dev); /* start the cwm timer */
}

/*
 * Action: switch MAC from 40 to 20
 */
static void
cwm_action_mac40to20(struct ath_cwm *cw_struct, void *arg)
{

	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

#if ATH_CWM_MAC_DISABLE_REQUEUE
	/* set channel width */
	cw_struct->cw_width = CWM_WIDTH20;

	/* set MAC to 20 MHz */
	cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_20;
    CWM_SET_MACMODE_CB(cw_struct, CWM_HT_MACMODE_20);

	/* notify rate control of new mode (select new rate table) */
    CWM_RATE_UPDATEALLNODES_CB(cw_struct);
    CWM_SMART_ANT_ACTION(cw_struct);
#else
	/* stop MAC */
    CWM_STOPTXDMA_CB(cw_struct);

    CWM_PROCESS_TX_PKTS_CB(cw_struct);

	/* set channel width */
	cw_struct->cw_width = CWM_WIDTH20;

	/* set MAC to 20 MHz */
	cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_20;
    CWM_SET_MACMODE_CB(cw_struct, CWM_HT_MACMODE_20);

	/* notify rate control of new mode (select new rate table) */
    CWM_RATE_UPDATEALLNODES_CB(cw_struct);
    CWM_SMART_ANT_ACTION(cw_struct);

	/* 
	 * Re-queue/re-aggregate packets as 20 MHz 
	 */
    CWM_REQUEUE_TX_PKTS_CB(cw_struct);
	/* Resume MAC */
    CWM_RESUMETXDMA_CB(cw_struct);
#endif //ATH_CWM_MAC_DISABLE_REQUEUE

	/* all virtual APs - send ch width action management frame */
	CWM_SENDACTIONMGMT_CB(cw_struct);
   
}

/*
 * Action: switch MAC from 20 to 40
 */
static void
cwm_action_mac20to40(struct ath_cwm *cw_struct, void *arg)
{

    CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

    /* No need to requeue existing frames on hardware queue
     * This avoids stopping the hardware queue and re-queuing 
     */

    /* set channel width */
    cw_struct->cw_width = CWM_WIDTH40;

    /* set MAC to 40 MHz */
    cw_struct->ac_hwstate.ht_macmode = CWM_HT_MACMODE_2040;
    CWM_SET_MACMODE_CB(cw_struct, CWM_HT_MACMODE_2040);

    /* notify rate control of new mode (select new rate table) */
    CWM_RATE_UPDATEALLNODES_CB(cw_struct);
    CWM_SMART_ANT_ACTION(cw_struct);

    /* all virtual APs - send ch width action management frame */
    CWM_SENDACTIONMGMT_CB(cw_struct);
}

/*
 * Action: switch PHY from 40 to 20 
 */
static void
cwm_action_phy40to20(struct ath_cwm *cw_struct, void *arg)
{
    CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

    /* Set current channge flag to PHY 20 MHz */
    CWM_SET_CURCHAN_FLAGS_HT20_CB(cw_struct);

    CWM_CHANGE_CHANNEL_CB(cw_struct);

    /* all virtual APs - re-send ch width action management frame */
    CWM_SENDACTIONMGMT_CB(cw_struct);

}

/*
 * Action: switch PHY from 20 to 40 
 */
static void
cwm_action_phy20to40(struct ath_cwm *cw_struct, void *arg)
{
    /* Set current channge flag to PHY 40 MHz */
    CWM_SET_CURCHAN_FLAGS_HT40_CB(cw_struct);

    /* all virtual APs - re-send ch width action management frame */
    CWM_SENDACTIONMGMT_CB(cw_struct);

    CWM_CHANGE_CHANNEL_CB(cw_struct);
}
       
/*
 * Action: filter destination and requeue
 */
static void
cwm_action_requeue(struct ath_cwm *cw_struct, void *arg)
{
	CWM_DBG_PRINT(cw_struct->wlandev, "%s\n", __func__);

	/* destination node channel width changed
	 * requeue tx frames with updated node's channel width setting
	 */

#if ATH_CWM_MAC_DISABLE_REQUEUE
	/* update node's rate table */
	CWM_RATE_UPDATENODE_CB(cw_struct, arg);
#else
    CWM_REQUEUE_CB(cw_struct, arg);
#endif  //ATH_CWM_MAC_DISABLE_REQUEUE
}


/*
 * Helper functions
 */



/*
 * Debug - Display CWM information 
 */

static void
cwm_debuginfo(struct ath_cwm *cw_struct)
{
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_running %d\n",
        __func__, cw_struct->ac_running);
#if ATH_DEBUG
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_state %s\n",
        __func__, ath_cwm_statename[cw_struct->ac_state]);
#endif
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_hwstate.ht_macmode %d\n",
        __func__, cw_struct->ac_hwstate.ht_macmode);
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_hwstate.ht_extprotspacing %d\n",
        __func__, cw_struct->ac_hwstate.ht_extprotspacing);
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_vextch %d\n",
        __func__, cw_struct->ac_vextch);
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_vextchbusy %d\n",
        __func__, cw_struct->ac_vextchbusy);
#if ATH_DEBUG
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_timer_prevstate %s\n",
        __func__, ath_cwm_statename[cw_struct->ac_timer_prevstate]);
#endif
	CWM_DBG_PRINT(cw_struct->wlandev, "%s: ac_timer_statetime %d\n",
        __func__, cw_struct->ac_timer_statetime);

}

