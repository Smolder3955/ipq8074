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
 */
#ifndef _ATH_CWM_H_
#define _ATH_CWM_H_

#include "ath_cwm_project.h"
/*
 *----------------------------------------------------------------------
 * local definitions/macros
 *----------------------------------------------------------------------
 */

#ifndef ATH_CWM_MAC_DISABLE_REQUEUE
/* default - disable requeuing for MAC 40=>20 transition */
#define ATH_CWM_MAC_DISABLE_REQUEUE 1
#endif

#ifndef ATH_CWM_PHY_DISABLE_TRANSITIONS 
/* default - disable requeuing for PHY transitions */
#define ATH_CWM_PHY_DISABLE_TRANSITIONS 1 
#endif

#define ATH_CWM_EXTCH_BUSY_THRESHOLD  30  /* Extension Channel Busy Threshold (0-100%) */

#define ATH_CWM_TIMER_INTERVAL_MS   1000    /* timer interval (milliseconds) */
#define ATH_CWM_TIMER_EXTCHSENSING_MS (10*1000)	/* ext channel sensing enable/disable (milliseconds) */

struct ath_cwm;

enum cwm_opmode {
    CWM_M_STA,    /* infrastructure station */
    CWM_M_HOSTAP,    /* Software Access Point */
    CWM_M_OTHER
};


enum cwm_phymode {
    CWM_PHYMODE_HT40PLUS,    /* 5Ghz, HT40 (ext ch +1) */
    CWM_PHYMODE_HT40MINUS,   /* 2Ghz, HT40 (ext ch -1) */
    CWM_PHYMODE_OTHER
};

enum cwm_mode {
    CWM_MODE20,
    CWM_MODE2040,         
    CWM_MODE40,
    CWM_MODEMAX

};

enum cwm_width {
    CWM_WIDTH20,
    CWM_WIDTH40
};

enum cwm_extprotmode {
    CWM_EXTPROTNONE,      /* no protection */
    CWM_EXTPROTCTSONLY,   /* CTS to self */
    CWM_EXTPROTRTSCTS,    /* RTS-CTS */
    CWM_EXTPROTMAX
};

enum cwm_ht_macmode {
    CWM_HT_MACMODE_20       = 0,            /* 20 MHz operation */
    CWM_HT_MACMODE_2040     = 1,            /* 20/40 MHz operation */
};

enum cwm_ht_extprotspacing {
    CWM_HT_EXTPROTSPACING_20    = 0,            /* 20 MHZ spacing */
    CWM_HT_EXTPROTSPACING_25    = 1,            /* 25 MHZ spacing */
    CWM_HT_EXTPROTSPACING_MAX
};

/* CWM States */
enum ath_cwm_state {
    ATH_CWM_STATE_EXT_CLEAR,
    ATH_CWM_STATE_EXT_BUSY,
    ATH_CWM_STATE_EXT_UNAVAILABLE,
    ATH_CWM_STATE_MAX
};

/* CWM Events */
enum ath_cwm_event {
    ATH_CWM_EVENT_TXTIMEOUT,    /* tx timeout interrupt */
    ATH_CWM_EVENT_EXTCHCLEAR,   /* ext channel sensing - clear */
    ATH_CWM_EVENT_EXTCHBUSY,    /* ext channel sensing - busy */
    ATH_CWM_EVENT_EXTCHSTOP,    /* ext channel sensing - stop */
    ATH_CWM_EVENT_EXTCHRESUME,  /* ext channel sensing - resume */
    ATH_CWM_EVENT_DESTCW20,     /* dest channel width changed to 20  */
    ATH_CWM_EVENT_DESTCW40,     /* dest channel width changed to 40  */
    ATH_CWM_EVENT_MAX,
};


/* State transition */
struct ath_cwm_statetransition {
    enum ath_cwm_state      ct_newstate;                         /* Output: new state */
    void (*ct_action)(struct ath_cwm *cw_struct, void *arg);   /* Output: action */
};

/* event queue entry */
struct ath_cwm_eventq_entry {
    TAILQ_ENTRY(ath_cwm_eventq_entry) ce_entry;

    enum ath_cwm_event 		ce_event;
    void *			ce_arg;
#if ATH_RESET_SERIAL
    void *          ce_extra_arg;
#endif
};


struct ath_cwm_conf {
    u_int32_t cw_enable;
};

/* CWM hardware state */
struct ath_cwm_hwstate {
    enum cwm_ht_macmode              ht_prev_macmode;             /* MAC - 20/40 mode */
    enum cwm_ht_macmode              ht_macmode;             /* MAC - 20/40 mode */
    enum cwm_ht_extprotspacing       ht_extprotspacing;      /* ext channel protection spacing */
};


/* CWM resources/state */
struct ath_cwm {

    osdev_t osdev; /* OS handle for allocating memory, timers, etc. */
    void *wlandev; /* Wlan device handle used for callback functions */

    /* Configuration */
    enum cwm_mode           cw_mode;              /* CWM mode */
    int8_t                  cw_extoffset;         /* CWM Extension Channel Offset */
    int8_t                  cw_ht40allowed;       /* CWM width allowed to change */
    enum cwm_extprotmode    cw_extprotmode;       /* CWM Extension Channel Protection Mode */
    enum cwm_ht_extprotspacing cw_extprotspacing;    /* CWM Extension Channel Protection Spacing */
    u_int32_t               cw_enable;            /* CWM State Machine Enabled */
    u_int32_t               cw_extbusythreshold;  /* CWM Extension Channel Busy Threshold */

    /* State */
    enum cwm_width          cw_width;             /* CWM channel width */

    timer_struct_t          ac_timer;               /* CWM timer - monitor the extension channel */
    enum ath_cwm_state      ac_timer_prevstate;     /* CWM timer - prev state of last timer call */
    u_int32_t               ac_timer_statetime;     /* CWM timer - time (sec) elapsed in current state */
    u_int8_t                ac_vextch;              /* DBG virtual ext channel sensing enabled? */
    u_int8_t                ac_vextchbusy;          /* DBG virtual ext channel state */
    u_int32_t               ac_extchbusyper;        /* Last measurement of ext ch busy (percent) */
    spinlock_t              ac_lock;                /* CWM spinlock */

    /* event queue (used to serialize events from DPC/timer contexts) */
    TAILQ_HEAD(, ath_cwm_eventq_entry) ac_eventq;   /* CWM event queue */
    atomic_t                ac_eventq_owner;        /* CWM event queue ownership flag */

    /* State Machine */
    u_int8_t                ac_running;             /* CWM running */
    enum ath_cwm_state      ac_state;               /* CWM state */
    struct ath_cwm_hwstate  ac_hwstate;             /* CWM hardware state */

    /* Debug Information */

};

#ifndef DISABLE_FUNCTION_INDIRECTION
typedef struct cwm_int_fn_s {
    /* internal APIs for patching */
    void (* __cwm_inithwstate)(struct ath_cwm *cw_struct);
    void (* __cwm_init)(COM_DEV *dev);
    void (* __cwm_start)(COM_DEV *dev);
    void (* __cwm_stop)(COM_DEV *dev);
    void (* __cwm_queueevent)(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg);
    void (* __cwm_statetransition)(struct ath_cwm *cw_struct, enum ath_cwm_event event, void *arg);
    timer_func_t __cwm_timer;
    int  (* __cwm_extchbusy)(struct ath_cwm *cw_struct);
    void (* __cwm_action_invalid)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_action_mac40to20)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_action_mac20to40)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_action_phy40to20)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_action_phy20to40)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_action_requeue)(struct ath_cwm *cw_struct, void *arg);
    void (* __cwm_debuginfo)(struct ath_cwm *cw_struct);
} CWM_INTERNAL_FN;

extern CWM_INTERNAL_FN _cwm_int_fn;

#define CWM_INT_API_FN(fn) _cwm_int_fn.__##fn
#else /* DISABLE_FUNCTION_INDIRECTION */
#define CWM_INT_API_FN(fn) fn
#endif /* DISABLE_FUNCTION_INDIRECTION */


#define CWM_inithwstate(cw) CWM_INT_API_FN(cwm_inithwstate((cw)))
#define CWM_init(cw) CWM_INT_API_FN(cwm_init(cw))
#define CWM_start(cw) CWM_INT_API_FN(cwm_start(cw))
#define CWM_stop(cw) CWM_INT_API_FN(cwm_stop(cw))
#define CWM_queueevent(cw, ev, arg) CWM_INT_API_FN(cwm_queueevent((cw),(ev),(arg)))
#define CWM_statetransition(cw, ev, arg) CWM_INT_API_FN(cwm_statetransition((cw),(ev),(arg)))
#define CWM_timer(arg) CWM_INT_API_FN(cwm_timer(arg))
#define CWM_extchbusy(cw) CWM_INT_API_FN(cwm_extchbusy(cw))
#define CWM_action_invalid(cw,arg) CWM_INT_API_FN(cwm_action_invalid((cw),(arg)))
#define CWM_action_mac40to20(cw,arg) CWM_INT_API_FN(cwm_action_mac40to20((cw),(arg)))
#define CWM_action_mac20to40(cw,arg) CWM_INT_API_FN(cwm_action_mac20to40((cw),(arg)))
#define CWM_action_phy40to20(cw,arg) CWM_INT_API_FN(cwm_action_phy40to20((cw),(arg)))
#define CWM_action_phy20to40(cw,arg) CWM_INT_API_FN(cwm_action_phy20to40((cw),(arg)))
#define CWM_action_requeue(cw,arg) CWM_INT_API_FN(cwm_action_requeue((cw),(arg)))
#define CWM_debuginfo(cw) CWM_INT_API_FN(cwm_debuginfo(cw))
#define CWM_init_stt(cw) CWM_INT_API_FN(cwm_init_stt(cw))
    
#define CWM_timer_fn_ptr CWM_INT_API_FN(cwm_timer)    

/*
 *----------------------------------------------------------------------
 * local function declarations
 *----------------------------------------------------------------------
 */

struct ath_cwm *cwm_attach(void *wlandev, osdev_t osdev, struct ath_cwm_conf *cwm_conf_parm);
void cwm_detach(COM_DEV *dev);
void cwm_init(COM_DEV *dev);
void cwm_stop(COM_DEV *dev);
void cwm_scan(COM_DEV *dev, enum cwm_opmode opmode);
void cwm_up(COM_DEV *dev, enum cwm_opmode opmode);
void cwm_join(COM_DEV *dev);
void cwm_down(COM_DEV *dev, enum cwm_opmode opmode);
void cwm_radio_disable(COM_DEV *dev);
void cwm_switch_mode_static20(struct ath_cwm *cw_struct);
void cwm_switch_mode_dynamic2040(struct ath_cwm *cw_struct);
void cwm_chwidth_change(struct ath_cwm *cw_struct, void *ni, enum cwm_width chwidth, enum cwm_opmode opmode);
void cwm_txtimeout(COM_DEV *dev);
void
cwm_gethwstate(struct ath_cwm *cw_struct, struct ath_cwm_hwstate *cwm_hwstate);
u_int32_t  cwm_getextbusy(struct ath_cwm *cw_struct);
enum cwm_width cwm_getlocalchwidth(struct ath_cwm *cw_struct);
enum cwm_ht_macmode cwm_macmode(struct ath_cwm *cw_struct);
void cwm_switch_to20(COM_DEV *dev);
void cwm_switch_to40(COM_DEV *dev);

void cwm_set_mode(COM_DEV *dev, CWM_IEEE80211_MODE cw_mode);
CWM_IEEE80211_MODE cwm_get_mode(COM_DEV *dev);
CWM_IEEE80211_EXTPROTMODE cwm_get_extprotmode(COM_DEV *dev);
CWM_IEEE80211_EXTPROTSPACING cwm_get_extprotspacing(COM_DEV *dev);
CWM_IEEE80211_WIDTH cwm_get_width(COM_DEV *dev);
u_int32_t cwm_get_enable(COM_DEV *dev);
u_int32_t cwm_get_extbusythreshold(COM_DEV *dev);
int8_t cwm_get_extoffset(COM_DEV *dev);
void cwm_set_extprotmode(COM_DEV *dev, CWM_IEEE80211_EXTPROTMODE val);
void cwm_set_extprotspacing(COM_DEV *dev, CWM_IEEE80211_EXTPROTSPACING val);
void cwm_set_enable(COM_DEV *dev, u_int32_t val);
void cwm_set_extbusythreshold(COM_DEV *dev, u_int32_t val);
void cwm_set_width(COM_DEV *dev, CWM_IEEE80211_WIDTH val);
void cwm_set_extoffset(COM_DEV *dev, int8_t val);


#endif /* _ATH_CWM_INTERNAL_H_ */
