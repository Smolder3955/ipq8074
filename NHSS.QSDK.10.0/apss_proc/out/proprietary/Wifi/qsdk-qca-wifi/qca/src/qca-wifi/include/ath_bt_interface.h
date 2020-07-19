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

/*
 * Public Interface for Bluetooth coexistence module
 */

#ifndef _ATH_BT_INTERFACE_H_
#define _ATH_BT_INTERFACE_H_

#include "ath_bt_registry.h"

typedef enum {
    /* Internal to the WLAN driver */
    ATH_COEX_EVENT_WLAN_CONNECT = 0,
    ATH_COEX_EVENT_WLAN_DISCONNECT,
    ATH_COEX_EVENT_WLAN_RESET_START,
    ATH_COEX_EVENT_WLAN_RESET_END,
    ATH_COEX_EVENT_WLAN_SCAN,
    ATH_COEX_EVENT_WLAN_RSSI_UPDATE,
    ATH_COEX_EVENT_WLAN_NETWORK_SLEEP,
    ATH_COEX_EVENT_WLAN_FULL_SLEEP,
    ATH_COEX_EVENT_WLAN_ASSOC,
    ATH_COEX_EVENT_WLAN_AGGR_FRAME_LEN,
    /* Expose to coex agent */
    ATH_COEX_EVENT_BT_NOOP,
    ATH_COEX_EVENT_BT_INQUIRY,
    ATH_COEX_EVENT_BT_CONNECT,
    ATH_COEX_EVENT_BT_ROLE_SWITCH, /* Not implemented */
    ATH_COEX_EVENT_BT_PROFILE,
    ATH_COEX_EVENT_BT_SCO_PKTINFO,
    ATH_COEX_EVENT_BT_PROFILE_INFO,
    ATH_COEX_EVENT_BT_RCV_RATE,
#if ATH_SUPPORT_MCI
    ATH_COEX_EVENT_BT_MCI_PROFILE_INFO,
    ATH_COEX_EVENT_BT_MCI_STATUS_UPDATE,
#endif
    ATH_COEX_EVENT_WLAN_TRAFFIC
}ATH_BT_COEX_EVENT;

/* Uses SIOCGATHDIAG to send these BT-COEX IOCTL */
#define ATH_BT_IOCTL_BEGIN                50
#define ATH_BT_IOCTL_ENABLE               ( ATH_BT_IOCTL_BEGIN + 1 )
#define ATH_BT_IOCTL_DISABLE              ( ATH_BT_IOCTL_BEGIN + 2 )
#define ATH_BT_IOCTL_GET_REGISTRY         ( ATH_BT_IOCTL_BEGIN + 3 )
#define ATH_BT_IOCTL_SET_REGISTRY         ( ATH_BT_IOCTL_BEGIN + 4 )
#define ATH_BT_IOCTL_GETINFO              ( ATH_BT_IOCTL_BEGIN + 5 )
#define ATH_BT_IOCTL_EVENT_START          ( ATH_BT_IOCTL_GETINFO + ATH_COEX_EVENT_WLAN_CONNECT + 1 )
#define ATH_BT_IOCTL_EVENT_END            ( ATH_BT_IOCTL_EVENT_START + ATH_COEX_EVENT_BT_RCV_RATE )
#define ATH_BT_IOCTL_END                  ( ATH_BT_IOCTL_EVENT_END )

enum {
    ATH_COEX_WLAN_ASSOC_START = 0,
    ATH_COEX_WLAN_ASSOC_END_SUCCESS,
    ATH_COEX_WLAN_ASSOC_END_FAIL
};

typedef enum {
    ATH_WLAN_STATE_DISCONNECT = 0,
    ATH_WLAN_STATE_CONNECT,
    ATH_WLAN_STATE_SCAN,
    ATH_WLAN_STATE_ASSOC
} ATH_WLAN_STATE;

enum {
    ATH_COEX_BT_INQUIRY_START = 0,
    ATH_COEX_BT_INQUIRY_END
};

enum {
    ATH_COEX_BT_CONNECT_START = 0,
    ATH_COEX_BT_CONNECT_END
};

enum {
    ATH_COEX_BT_PROFILE_TYPE_ACL = 0,
    ATH_COEX_BT_PROFILE_TYPE_SCO_ESCO,
    ATH_COEX_BT_PROFILE_TYPE_UNKNOWN
};

enum {
    ATH_COEX_BT_PROFILE_START = 0,
    ATH_COEX_BT_PROFILE_END,
    ATH_COEX_BT_PROFILE_UNKNOWN
};

enum {
    ATH_COEX_BT_RATE_BR = 0,
    ATH_COEX_BT_RATE_EDR,
};

#define ATH_COEX_BT_PROFILE_ACL     0x1
#define ATH_COEX_BT_PROFILE_A2DP    0x2
#define ATH_COEX_BT_PROFILE_HID     0x4
#define ATH_COEX_BT_PROFILE_PAN     0x8
#define ATH_COEX_BT_PROFILE_SCO_ESCO   0x10

enum {
    ATH_COEX_BT_ROLE_MASTER = 0,
    ATH_COEX_BT_ROLE_SLAVE,
    ATH_COEX_BT_ROLE_UNKNOWN
	};

typedef enum {
    ATH_BT_STATE_OFF = 0,          /* Bluetooth device is off/idle */
    ATH_BT_STATE_ON,               /* Bluetooth device is on, active profile > 0 */
    ATH_BT_STATE_MGT,              /* Bluetooth device is send management frames */
    ATH_BT_MAX_STATE
} ATH_BT_STATE;

#ifdef ATH_USB
typedef enum {
    ATH_USB_WLAN_TX_LOW,               /* Tx traffic low  */
    ATH_USB_WLAN_TX_HIGH               /* Tx traffic high */
} ATH_USB_WLAN_TX;
#endif

typedef enum {
    ATH_BT_COEX_SCHEME_NONE = 0,      /* No Bluetooth coexistence */
    ATH_BT_COEX_SCHEME_PS_POLL,       /* 3-wire plus PS poll. Mainly for SCO/eSCO */
    ATH_BT_COEX_SCHEME_PS,            /* 3-wire plus ps enable/disable. Mainly for ACL */
    ATH_BT_COEX_SCHEME_HW3WIRE,       /* HW 3-wire with software time sharing */
    ATH_BT_COEX_SCHEME_MAX
} ATH_BT_COEX_SCHEME;

typedef enum {
    ATH_BT_COEX_CFG_NONE,          /* No bt coex enabled */
    ATH_BT_COEX_CFG_2WIRE_2CH,     /* 2-wire with 2 chains */
    ATH_BT_COEX_CFG_2WIRE_CH1,     /* 2-wire with ch1 */
    ATH_BT_COEX_CFG_2WIRE_CH0,     /* 2-wire with ch0 */
    ATH_BT_COEX_CFG_3WIRE,         /* 3-wire */
    ATH_BT_COEX_CFG_MCI            /* MCI */
} ATH_BT_COEX_CFG;

typedef enum {
    ATH_BT_PROT_MODE_NONE,
    ATH_BT_PROT_MODE_ON,
    ATH_BT_PROT_MODE_DEFER,
} ATH_BT_PROTECT_MODE;

typedef enum {
    ATH_BT_COEX_PS_POLL,
    ATH_BT_COEX_PS_POLL_DATA,
    ATH_BT_COEX_PS_POLL_ABORT,
    ATH_BT_COEX_PS_ENABLE,
    ATH_BT_COEX_PS_DISABLE,
    ATH_BT_COEX_PS_FORCE_DISABLE
} ATH_BT_PS_COMPLETE_TYPE;

typedef enum {
    ATH_BT_COEX_INFO_SCHEME,
    ATH_BT_COEX_INFO_BTBUSY,
    ATH_BT_COEX_INFO_ALL,
} ATH_COEX_INFO_TYPE;

#ifdef WIN32
#pragma pack(push, spectral_data, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif  /* __ATTRIB_PACK */
#endif /* WIN32 */

typedef struct {
    u_int32_t   profileType;
    u_int32_t   profileState;
    u_int32_t   connectionRole;
    u_int32_t   btRate;
    u_int32_t   connHandle;
} __ATTRIB_PACK ATH_COEX_PROFILE_INFO, *PATH_COEX_PROFILE_INFO;

typedef struct {
    u_int32_t   profileType;
    u_int32_t   connHandle;
    u_int32_t   connectionRole;
} ATH_COEX_PROFILE_TYPE_INFO, *PATH_COEX_PROFILE_TYPE_INFO;

typedef struct {
    u_int32_t   connectionRole;
    u_int32_t   connHandle;
} ATH_COEX_ROLE_INFO, *PATH_COEX_ROLE_INFO;

#if ATH_SUPPORT_MCI
typedef struct {
    u_int32_t   profileType;
    u_int32_t   profileState;
    u_int32_t   connectionRole;
    u_int32_t   btRate;
    u_int32_t   connHandle;
    u_int32_t   voiceType;
    u_int32_t   T;      /* Voice: Tvoice, HID: Tsniff,        in slots */
    u_int32_t   W;      /* Voice: Wvoice, HID: Sniff timeout, in slots */
    u_int32_t   A;      /*                HID: Sniff attempt, in slots */
} __ATTRIB_PACK ATH_COEX_MCI_PROFILE_INFO, *PATH_COEX_MCI_PROFILE_INFO;

typedef struct {
    u_int32_t   type;
    u_int32_t   connHandle;
    u_int32_t   state;
} __ATTRIB_PACK ATH_COEX_MCI_BT_STATUS_UPDATE_INFO, *PATH_COEX_MCI_BT_STATUS_UPDATE_INFO;
#endif

typedef struct coex_scheme_detail {
    u_int8_t                scheme;
    u_int8_t                bt_period;          /* BT traffic pattern period in ms */
    u_int8_t                bt_dutyCycle;       /* BT duty cycle in percentage */
    int                     bt_stompType;       /* Types of BT stomping : HAL_BT_COEX_STOMP_TYPE */
    u_int8_t               wlan_aggrLimit;     /* WLAN aggr limit in 0.25 ms */
} __ATTRIB_PACK ATH_COEX_SCHEME_INFO;

/* Struct to get BT info. Exported to Apps. since struct ath_bt_info is already defined, named this as 
    ath_bt_coex_comp_info
 */
struct ath_bt_coex_comp_info {
    /* Common variables */
    int                 bt_enable;              /* whether Bluetooth module is enable */
    int                 bt_on;              /* whether Bluetooth device is on */
    int                 bt_bmissThresh;     /* BMISS threshold */
    int                 bt_gpioIntEnabled;  /* GPIO interrupt is enabled*/
    // will get later if needed : HAL_BT_COEX_INFO    bt_hwinfo;          /* BT coex hardware configuration info */

    ATH_BT_STATE        bt_state;           /* Bluetooth device state */
    ATH_WLAN_STATE      wlan_state;         /* wlan device state */
    ATH_WLAN_STATE      wlan_preState;      /* Previous wlan device state */
    u_int8_t            wlan_scanState;     /* wlan scan state */
    u_int8_t            bt_mgtPending;      /* Number of pending BT management actions */
    u_int8_t            bt_mgtExtendTime;   /* BT management actions extend time in seconds */
    ATH_COEX_SCHEME_INFO coexScheme;        /* BT coex scheme */
    u_int32_t           watchdog_cnt;       /* Watchdog timer counter */
    u_int8_t            bt_sco_pspoll;      /* PS-poll for SCO/eSCO profile */
    u_int16_t           wlan_channel;       /* WLAN home channel */
    int8_t              wlan_rssi;          /* WLAN beacon RSSI */
    u_int8_t            wlan_aspm;          /* WLAN PCI CFG ASPM setting */
    u_int32_t           btBDRHandle;        /* Handle of BDR device */
    u_int32_t           btForcedWeight;     /* Forced weight table from registry */
    u_int16_t           btRcvThreshold;     /* BT receiving throughput threshold for low ACK power in kbps */
    u_int8_t            bt_stomping : 1,    /* BT is being stomped */
                        numACLProfile : 3,  /* Number of ACL profiles */
                        numSCOProfile : 1,  /* number of SCO/eSCO profiles */
                        bt_coexAgent : 1,   /* Coex agent is enabled */
                        detaching : 1;      /* Device is in detach */

    /* SCO/eSCO parameters in usec */
    u_int16_t           bt_sco_tm_low;      /* Low limit of SCO BT_ACTIVE time */
    u_int16_t           bt_sco_tm_high;     /* Hight limit of SCO BT_ACTIVE time */
    u_int16_t           bt_sco_tm_intrvl;   /* Interval time for SCO BT_ACTIVE */
    u_int16_t           bt_sco_tm_idle;     /* Idle time between SCO BT_ACTIVE */
    u_int16_t           bt_sco_delay;       /* PS-poll delay time after BT_ACTIVE goes low */
    u_int16_t           bt_sco_last_poll;   /* Last PS-Poll allowed to send before next BT_ACTIVE comes */

}__ATTRIB_PACK;

#ifdef WIN32
#pragma pack(pop, spectral_data)
#endif /* WIN32 */
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif  /* __ATTRIB_PACK */

#endif /* end of _ATH_BT_INTERFACE_H_ */
