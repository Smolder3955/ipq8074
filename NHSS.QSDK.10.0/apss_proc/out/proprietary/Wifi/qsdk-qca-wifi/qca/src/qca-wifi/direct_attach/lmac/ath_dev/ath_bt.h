/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Public Interface for Bluetooth coexistence module
 */

#ifndef _DEV_ATH_BT_H
#define _DEV_ATH_BT_H

#include "ath_bt_interface.h"
#define USE_ATH_TIMER 1

#ifdef ATH_BT_COEX

enum {
    ATH_COEX_WLAN_SCAN_NONE = 0,
    ATH_COEX_WLAN_SCAN_START,
    ATH_COEX_WLAN_SCAN_END,
    ATH_COEX_WLAN_SCAN_RESET
};

typedef enum {
    ATH_BT_COEX_PTA_ENABLE = 0,         /* Default PTA mode is enabled. */
    ATH_BT_COEX_PTA_DISABLE,
} ATH_BT_COEX_PTA_STATE;

typedef enum {
    ATH_BT_COEX_LOW_RSSI = 0,
    ATH_BT_COEX_HIGH_RSSI,
    ATH_BT_COEX_LOW_RCV_XPUT,
    ATH_BT_COEX_HIGH_RCV_XPUT,
    ATH_BT_COEX_HID_ON,
    ATH_BT_COEX_HID_OFF,
    ATH_BT_COEX_SCO_ON,
    ATH_BT_COEX_SCO_OFF,
    ATH_BT_COEX_A2DP_ON,
    ATH_BT_COEX_A2DP_OFF,
} ATH_BT_COEX_LOW_ACK_FLAG;

struct ath_bt_info;

struct ath_bt_coex_info {
    int     (*init)(struct ath_softc *sc, struct ath_bt_info *btinfo);
    int     (*resume)(struct ath_softc *sc, struct ath_bt_info *btinfo);
    int     (*pause)(struct ath_softc *sc, struct ath_bt_info *btinfo);
    void    (*free)(struct ath_softc *sc, struct ath_bt_info *btinfo);
};


typedef struct coex_bt_rcv_rate {
    u_int32_t           rateHandle[4];      /* rcv rate and link handle. bit 0-15: rate, bit 16-31: handle. */
} ATH_COEX_BT_RCV_RATE;

typedef struct coex_disable_PTA {
    HAL_BOOL                enable;
    u_int8_t                rssiThreshold;
    ATH_BT_COEX_PTA_STATE   state;
    u_int8_t                enableCnt;
    u_int8_t                disableCnt;
} ATH_COEX_DISABLE_PTA;

/*
 * PS poll scheme definitions
 */
struct ath_bt_coex_ps_poll_info {
    struct ath_bt_coex_info     coex_ps_poll;       /* PS-poll scheme info */
    struct ath_gen_timer        *hwtimer;           /* HW timer for PS-Poll scheme */
    struct ath_gen_timer        *hwtimer2;          /* HW timer2 for PS-Poll scheme */

    u_int8_t                    sync_cnt;           /* Number of SCO BT_ACTIVE since last sync */
    u_int8_t                    overflow_cnt;       /* Number for SCO timer overflow */
    u_int8_t                    stomp_cnt;          /* Number for SCO BT_ACTIVE being stomped */
    u_int32_t                   tstamp;             /* Time stamp */
    u_int32_t                   lastpolltstamp;     /* Time stamp for last PS-Poll */
    u_int8_t                    sample : 1,         /* Start SCO BT_ACTIVE sampling */
                                resync : 1,         /* SCO BT_ACTIVE resync */
                                in_reset : 1;       /* In reset */
};

#define SCO_RESET_SYNC_STATE(_sco)  do {    \
    _sco->sync_cnt = 0;                     \
    _sco->overflow_cnt = 0;                 \
    _sco->sample = 0;                       \
} while (AH_FALSE);

/*
 * PS enable/disable scheme definitions
 */
struct ath_bt_coex_ps_info {
    struct ath_bt_coex_info     coex_ps;        /* PS scheme info */
    struct ath_gen_timer        *a2dp_period_timer; /* Timer for BT period */
    struct ath_gen_timer        *a2dp_dcycle_timer; /* Timer for BT duty cycle */

    u_int32_t                   a2dp_period;    /* in msec */
    u_int32_t                   a2dp_dcycle;    /* in percentage of a period */
    u_int8_t                    numStompDcycle; /* Number of stomped BT duty cycles */
    u_int8_t                    defer_dcycle : 1, /* BT duty cycle is being deferred */
                                wait_for_complete : 1; /* A PS frame is waiting for completion */
};

/*
 * HW3wire scheme definitions
 */
struct ath_bt_coex_hw3wire_info {
    struct ath_bt_coex_info     coex_hw3wire;       /* Hardware 3-wire scheme info */
#ifdef USE_ATH_TIMER
    struct ath_timer            period_timer;       /* Timer for BT period */
#else
    struct ath_gen_timer        *period_timer;      /* Timer for BT period */
#endif
    struct ath_gen_timer        *no_stomp_timer;    /* Timer for no BT stomping */

    u_int32_t                   hw3wire_period;     /* in usec */
    u_int32_t                   hw3wire_no_stomp;   /* in usec */
    u_int16_t                   orgAggrSubframes;   /* Original copy of aggrSubFrames setting */
    u_int8_t                    numSkipNullFrame;   /* Number of skipped null frames */
    int8_t                      rssiProfileThreshold;  /* rssi threshold for profile change */
    u_int16_t                   rssiProfileDisableCnt; /* number of times rssi is high */
    u_int16_t                   rssiProfileEnableCnt;  /* number of times rssi is low */
    u_int8_t                    rssiProfile11g;     /* DutyCycle value for 11g mode */
    u_int8_t                    rssiProfile11n;     /* DutyCycle value for 11n mode */
    u_int8_t                    psPending : 1,      /* A null frame is pending */
                                timerState : 2,     /* No stomp timer state */
                                rssiProfile : 1,    /* Change Profile based on RSSI */
                                rssiSpecialCase : 1;/* Change Profile flag */
};

#define MAX_NUM_BT_ACL_PROFILE      7
#define MAX_NUM_BT_SCO_PROFILE      1
#define MAX_NUM_BT_COEX_PROFILE     (MAX_NUM_BT_ACL_PROFILE + MAX_NUM_BT_SCO_PROFILE)
#define INVALID_PROFILE_TYPE_HANDLE 0xffffffff

#define BT_SLAVE_PAN_DUTYCYCLE_ADJUSTMENT 20

struct ath_bt_info {
    /* Common variables */
    int                 bt_enable;          /* whether Bluetooth module is enable */
    HAL_BOOL            bt_on;              /* whether Bluetooth device is on */
    spinlock_t          bt_lock;            /* btinfo level lock */
    int                 bt_bmissThresh;     /* BMISS threshold */
    HAL_BOOL            bt_gpioIntEnabled;  /* GPIO interrupt is enabled*/
    HAL_BT_COEX_INFO    bt_hwinfo;          /* BT coex hardware configuration info */

    /* 2-wire specific variables */
    u_int8_t            bt_protectMode;     /* BT protection mode */
    u_int32_t           bt_activeCount;     /* BT_ACTIVE count */
    struct ath_timer    bt_activeTimer;     /* Timer for BT_ACTIVE measurement */
    u_int8_t            bt_preGpioState;    /* Previous GPIO state */
    u_int8_t            bt_timeOverThre;    /* Time that BT_ACTIVE count is over threshold*/
    u_int8_t            bt_timeOverThre2;   /* Time that BT_ACTIVE count is over threshold2*/
    HAL_BOOL            bt_initStateDone;   /* End of init state*/
    u_int8_t            bt_initStateTime;   /* Time in init state */
    u_int8_t            bt_activeChainMask; /* Chain mask to use when force to single chain */

    /* 3-wire specific variables */
    struct ath_bt_coex_info   *bt_coex;     /* Scheme object */
    ATH_BT_STATE        bt_state;           /* Bluetooth device state */
    ATH_WLAN_STATE      wlan_state;         /* wlan device state */
    ATH_WLAN_STATE      wlan_preState;      /* Previous wlan device state */
#ifdef ATH_USB
    ATH_USB_WLAN_TX     wlan_tx_state;
#endif
    u_int8_t            wlan_scanState;     /* wlan scan state */
    u_int8_t            bt_mgtPending;      /* Number of pending BT management actions */
    u_int8_t            bt_mgtExtendTime;   /* BT management actions extend time in seconds */
    ATH_COEX_SCHEME_INFO coexScheme;        /* BT coex scheme */
    HAL_BOOL            bt_hwtimerEnabled;  /* used for synchronization with timer interrupt */
    HAL_BOOL            bt_hwtimerEnabled2; /* used for synchronization with timer interrupt */
    struct ath_timer    watchdog_timer;     /* Watchdog timer */
    u_int32_t           watchdog_cnt;       /* Watchdog timer counter */
    u_int8_t            bt_sco_pspoll;      /* PS-poll for SCO/eSCO profile */
    u_int16_t           wlan_channel;       /* WLAN home channel */
    int8_t              wlan_rssi;          /* WLAN beacon RSSI */
    u_int8_t            wlan_aspm;          /* WLAN PCI CFG ASPM setting */
    u_int32_t           btBDRHandle;        /* Handle of BDR device */
    u_int32_t           btForcedWeight;     /* Forced weight table from registry */
    u_int16_t           btRcvThreshold;     /* BT receiving throughput threshold for low ACK power in kbps */
    ATH_COEX_DISABLE_PTA disablePTA;        /* Disable PTA mode */
    u_int32_t           activeProfile;      /* Active BT profile */
    u_int8_t            mciHWBcnProcOff;    /* Time to turn of HW beacon proc for MCI */
    u_int32_t           bt_stomping : 1,    /* BT is being stomped */
                        numACLProfile : 3,  /* Number of ACL profiles */
                        numSCOProfile : 1,  /* number of SCO/eSCO profiles */
                        bt_coexAgent : 1,   /* Coex agent is enabled */
                        detaching : 1,      /* Device is in detach */
                        lowerTxPower : 5,   /* Need to lower Tx Power */
                        antDivEnable : 1,   /* Enable Antenna Diversity or not */
                        ant_div_wlan_noconn_enable : 1, /* Enable Antenna Diversity when wlan is not connected */
                        antDivState : 1,    /* Current State of Antenna Diversity ON or OFF */
                        forceStomp : 2,     /* Force to stomp BT even WLAN is idle */
                        lowACKPower:1,      /* Low ACK power */
                        slavePan:1,         /* Pan profile and slave role */
                        mciSharedConcurTxEn:1,/* Concurrent tx for MCI with shared chain. */
                        mciSharedConcurTx:1,/* Concurrent tx for MCI with shared chain. */
                        mciFTPStompEn:1,    /* Need to use special stomp low weight for FTP */
                        mciFTPStomp: 1,     /* Current state of FTPStomp */
                        mciTuning: 1;       /* Coex tuning for MCI */

    /* SCO/eSCO parameters in usec */
    u_int16_t           bt_sco_tm_low;      /* Low limit of SCO BT_ACTIVE time */
    u_int16_t           bt_sco_tm_high;     /* Hight limit of SCO BT_ACTIVE time */
    u_int16_t           bt_sco_tm_intrvl;   /* Interval time for SCO BT_ACTIVE */
    u_int16_t           bt_sco_tm_idle;     /* Idle time between SCO BT_ACTIVE */
    u_int16_t           bt_sco_delay;       /* PS-poll delay time after BT_ACTIVE goes low */
    u_int16_t           bt_sco_last_poll;   /* Last PS-Poll allowed to send before next BT_ACTIVE comes */
    u_int32_t           wlanRxPktNum;       /* Number of WLAN rx packets */
    u_int16_t           waitTime;           /* Waiting time */

#if ATH_SUPPORT_MCI
    ATH_COEX_MCI_PROFILE_INFO pMciList[MAX_NUM_BT_COEX_PROFILE];
    ATH_COEX_MCI_BT_STATUS_UPDATE_INFO pMciMgmtList[MAX_NUM_BT_COEX_PROFILE];
    u_int32_t           mciProfileFlag;     /* Profile flag for MCI */
#endif
    ATH_COEX_PROFILE_TYPE_INFO  pList[MAX_NUM_BT_ACL_PROFILE];

    u_int8_t            lowACKDisableCnt;
    u_int8_t            lowACKEnableCnt;
    u_int8_t            concurTxEnableCnt;
    u_int8_t            concurTxDisableCnt;
    int8_t              rssiTxLimit;        /* rssi Threshold for Tx power Limit hack */

    union {
        struct ath_bt_coex_hw3wire_info  hw3wire_info; /* HW3wire scheme definitions */
        struct ath_bt_coex_ps_poll_info	 ps_poll_info; /* PS poll scheme definitions */
        struct ath_bt_coex_ps_info       ps_info;      /* PS enable/disable scheme definitions*/
    } bt_scheme_info;
};

#define BT_COEX_BT_ACTIVE_MEASURE       1000    /* 1 second */
#define BT_COEX_BT_ACTIVE_THRESHOLD     20      /* Threshold for BT_ACTIVE state change */
#define BT_COEX_BT_ACTIVE_THRESHOLD2    400     /* Threshold2 for BT_ACTIVE state change */
#define BT_COEX_PROT_MODE_ON_TIME       2       /* 2 seconds over threshold to turn on protection mode */
#define BT_COEX_INIT_STATE_TIME         30      /* First 30 seconds after driver loaded is init state */
#define Bt_COEX_INIT_STATE_SCAN_TIME    4       /* Wait for 4 seconds before enable protection mode */

#define ATH_BT_TO_PSPOLL(_btinfo)      ((struct ath_bt_coex_ps_poll_info *)((_btinfo)->bt_coex))
#define ATH_BT_TO_PS(_btinfo)          ((struct ath_bt_coex_ps_info *)((_btinfo)->bt_coex))
#define ATH_BT_TO_HW3WIRE(_btinfo)     ((struct ath_bt_coex_hw3wire_info *)((_btinfo)->bt_coex))

#define ATH_BT_LOCK_INIT(_btinfo)   spin_lock_init(&(_btinfo)->bt_lock);
#define ATH_BT_LOCK_DESTROY(_btinfo)
#ifdef ATH_USB
#define ATH_BT_LOCK(_btinfo)
#define ATH_BT_UNLOCK(_btinfo)
#else
#define ATH_BT_LOCK(_btinfo)        spin_lock(&(_btinfo)->bt_lock);
#define ATH_BT_UNLOCK(_btinfo)      spin_unlock(&(_btinfo)->bt_lock);
#endif

#define BT_SCO_CYCLES_PER_SYNC     52
#define BT_SCO_OVERFLOW_LIMIT      10

/* BT ISM band */
#define BT_LOW_FREQ                 2400
#define BT_HIGH_FREQ                2484

#define MAX_SCHEME_PARAMETERS       4

#define MGMT_EXTEND_TIME            6 

/* RSSI related */
#define ATH_COEX_RSSI_SWITCH_CNT    20
#define ATH_COEX_RSSI_SWITCH_CNT_HW 3           /* Switch count when HW beacon processing is on */
#define ATH_COEX_SWITCH_EXTRA_RSSI  2           /* Extra RSSI threshold to avoid fluctuation */

/* RSSI and Mode for Dutycycle change */
#define RSSI_MODE_PROFILE_ENABLE    1
#define RSSI_PROFILE_THRESHOLD      0xff

/* Force stomp */
#define BT_COEX_FORCE_STOMP_ASSOC       0x01
#define BT_COEX_FORCE_STOMP_RX_TRAFFIC  0x02
#define BT_COEX_FORCE_STOMP_THRESHOLD   12      // Number of rx packets per 200 ms

#define BT_COEX_RX_TRAFFIC_CHECK_TIME   100     // 100 ms
#define BT_COEX_MCI_FTP_STOMP_THRESH    5

/* Lower ACK power */
#define BT_COEX_LOWER_ACK_PWR_MASK      0x1f
#define BT_COEX_LOWER_ACK_PWR_RSSI      0x10
#define BT_COEX_LOWER_ACK_PWR_SCO       0x01
#define BT_COEX_LOWER_ACK_PWR_XPUT      0x02
#define BT_COEX_LOWER_ACK_PWR_HID       0x04
#define BT_COEX_LOWER_ACK_PWR_A2DP      0x08
#define BT_COEX_NEED_LOWER_ACK_PWR(_btinfo)  (((_btinfo)->lowerTxPower & BT_COEX_LOWER_ACK_PWR_MASK) > BT_COEX_LOWER_ACK_PWR_RSSI)
#define BT_COEX_LOWER_ACK_PWR_FLAG_CHECK(_btinfo, _flag)     ((_btinfo)->lowerTxPower & (_flag))
#define BT_COEX_ACTIVE_PROFILE(_btinfo, _flag)   ((_btinfo)->activeProfile & (_flag))

#define BT_COEX_IS_11G_MODE(_sc) (((_sc)->sc_curchan.channel_flags & CHANNEL_G) == CHANNEL_G)
#define BT_COEX_IS_11B_MODE(_sc) (((_sc)->sc_curchan.channel_flags & CHANNEL_B) == CHANNEL_B)
#define BT_COEX_IS_HELIUS(_btinfo) (_btinfo->bt_hwinfo.bt_module == HAL_BT_MODULE_HELIUS)

/* Profile flag for MCI */
#define ATH_COEX_MCI_MULTIPLE_ATMPT     0x01
#define ATH_COEX_MCI_SLAVE_PAN_FTP      0x02
#define ATH_COEX_MCI_MULTIPLE_TOUT      0x04
#define BT_COEX_MCI_FLAG(_btinfo, _flag)   ((_btinfo)->mciProfileFlag & (_flag))

/*
 * Profiles to scheme mapping table
 * Max number of ACL profiles allowed is 3.
 *
 * ATH_BT_COEX_SCHEME_PS      param: [period in ms] [bt dcycle %] [stomp type]
 * ATH_BT_COEX_SCHEME_HW3WIRE param: [period in ms] [bt dcycle %] [stomp type]
 */
#define ATH_BT_COEX_BDR_DUTYCYCLE_ADJUST    20
#define ATH_BT_COEX_MAX_DUTYCYCLE           90
#define GET_SCHEME_INDEX(_btinfo)   (((_btinfo)->numACLProfile << 1) + (_btinfo)->numSCOProfile)
#define BT_COEX_NUM_OF_PROFILE(_btinfo) ((_btinfo)->numACLProfile + (_btinfo)->numSCOProfile)

static const ATH_COEX_SCHEME_INFO btProfileToScheme[(MAX_NUM_BT_ACL_PROFILE + 1)*(MAX_NUM_BT_SCO_PROFILE + 1)] = {
    {    ATH_BT_COEX_SCHEME_NONE,    0,    0,    HAL_BT_COEX_NO_STOMP,    0}, // No profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   50,   HAL_BT_COEX_STOMP_LOW,    6}, // 1 SCO/eSCO profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   50,   HAL_BT_COEX_STOMP_LOW,    0}, // 1 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   60,   HAL_BT_COEX_STOMP_LOW,    6}, // 1 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   60,   HAL_BT_COEX_STOMP_LOW,    0}, // 2 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   70,   HAL_BT_COEX_STOMP_LOW,    6}, // 2 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   70,   HAL_BT_COEX_STOMP_LOW,    0}, // 3 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   80,   HAL_BT_COEX_STOMP_LOW,    6}, // 3 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   80,   HAL_BT_COEX_STOMP_LOW,    0}, // 4 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   85,   HAL_BT_COEX_STOMP_LOW,    6}, // 4 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   85,   HAL_BT_COEX_STOMP_LOW,    0}, // 5 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   90,   HAL_BT_COEX_STOMP_LOW,    6}, // 5 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   90,   HAL_BT_COEX_STOMP_LOW,    0}, // 6 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   95,   HAL_BT_COEX_STOMP_LOW,    6}, // 6 ACL + 1 SCO/eSCO profiles
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   95,   HAL_BT_COEX_STOMP_LOW,    0}, // 7 ACL profile
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   98,   HAL_BT_COEX_STOMP_LOW,    6}  // 7 ACL + 1 SCO/eSCO profiles
};

/*
 * BT/WLAN states to scheme mapping table
 *
 * BT state:
 *          0: OFF
 *          1: ON
 *          2: MGT
 * WLAN state:
 *          0: DISCONNECT
 *          1: CONNECT
 *          2: SCAN
 *          3: ASSOC
 * ATH_BT_COEX_SCHEME_PS      param: [period in ms] [bt dcycle %] [stomp type]
 * ATH_BT_COEX_SCHEME_HW3WIRE param: [period in ms] [bt dcycle %] [stomp type]
 */
#define MAX_NUM_BT_STATE        3
#define MAX_NUM_WLAN_STATE      4
static const ATH_COEX_SCHEME_INFO stateToScheme[MAX_NUM_BT_STATE][MAX_NUM_WLAN_STATE] = {
   {{    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}, // BT_STATE_OFF   WLAN_STATE_DISCONNECT
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   40, HAL_BT_COEX_STOMP_LOW,    0}, // BT_STATE_OFF   WLAN_STATE_CONNECT
    {    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}, // BT_STATE_OFF   WLAN_STATE_SCAN
    {    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}},// BT_STATE_OFF   WLAN_STATE_ASSOC
   {{    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}, // BT_STATE_ON    WLAN_STATE_DISCONNECT
    {    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}, // BT_STATE_ON    WLAN_STATE_CONNECT **table lookup
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   60, HAL_BT_COEX_STOMP_LOW,    0}, // BT_STATE_ON    WLAN_STATE_SCAN
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   50, HAL_BT_COEX_STOMP_ALL,    0}},// BT_STATE_ON    WLAN_STATE_ASSOC
   {{    ATH_BT_COEX_SCHEME_NONE,    0,    0,  HAL_BT_COEX_NO_STOMP,    0}, // BT_STATE_MGT   WLAN_STATE_DISCONNECT
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   60, HAL_BT_COEX_STOMP_ALL,    0}, // BT_STATE_MGT   WLAN_STATE_CONNECT
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   60, HAL_BT_COEX_STOMP_ALL,    0}, // BT_STATE_MGT   WLAN_STATE_SCAN
    { ATH_BT_COEX_SCHEME_HW3WIRE,   40,   50, HAL_BT_COEX_STOMP_ALL,    0}} // BT_STATE_MGT   WLAN_STATE_ASSOC
};

/*
 * 
 */
int ath_bt_coex_attach(struct ath_softc *sc);
void ath_bt_coex_detach(struct ath_softc *sc);
void ath_bt_coex_gpio_intr(struct ath_softc *sc);
int ath_bt_coex_event(ath_dev_t dev, u_int32_t nevent, void *param);
uint8_t ath_bt_coex_mci_tx_chainmask(struct ath_softc *sc, uint8_t chainmask, uint8_t rate);
u_int32_t ath_btcoexinfo(ath_dev_t dev, u_int32_t reg);
u_int32_t ath_coexwlaninfo(ath_dev_t dev, u_int32_t reg,u_int32_t bOn);
/*
 * Functions for net80211
 */
u_int32_t ath_bt_coex_ps_complete(ath_dev_t dev, u_int32_t type);
int ath_bt_coex_get_info(ath_dev_t dev, u_int32_t infoType, void *info);
#endif /* ATH_BT_COEX */

#endif /* end of _DEV_ATH_BT_H */
