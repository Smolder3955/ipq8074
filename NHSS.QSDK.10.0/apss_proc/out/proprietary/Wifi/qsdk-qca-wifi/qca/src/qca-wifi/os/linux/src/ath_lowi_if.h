/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <osdep.h>
#include "osif_private.h"

#ifndef _ATH_LOWI_IF_H_
#define _ATH_LOWI_IF_H__

#if ATH_SUPPORT_LOWI
#include "ol_if_athvar.h"

extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;

/* umac functions to send action frames */
extern void ieee80211_lowi_send_wru_frame (u_int8_t *data);
extern void ieee80211_lowi_send_ftmrr_frame (u_int8_t *data);
extern u_int32_t osif_get_num_active_vaps( wlan_dev_t  comhandle);

/* offload function to send lowi frames to FW */

#define WLAN_DEFAULT_LOWI_IF_NETLINK_PID 0xffffff
#define NETLINK_LOWI_IF_EVENT 22

/* ANI (LOWI) MESSAGE TYPES */
#define ANI_MSG_APP_REG_REQ       0x1
#define ANI_MSG_APP_REG_RSP       0x2
#define ANI_MSG_OEM_DATA_REQ      0x3
#define ANI_MSG_OEM_DATA_RSP      0x4
#define ANI_MSG_CHANNEL_INFO_REQ  0x5
#define ANI_MSG_CHANNEL_INFO_RSP  0x6
#define ANI_MSG_OEM_ERROR         0x7
#define ANI_MSG_PEER_STATUS_IND   0x8

#define TARGET_OEM_CAPABILITY_REQ       0x01
#define TARGET_OEM_CAPABILITY_RSP       0x02
#define TARGET_OEM_MEASUREMENT_REQ      0x03
#define TARGET_OEM_MEASUREMENT_RSP      0x04
#define TARGET_OEM_ERROR_REPORT_RSP     0x05
#define TARGET_OEM_NAN_MEAS_REQ         0x06
#define TARGET_OEM_NAN_MEAS_RSP         0x07
#define TARGET_OEM_NAN_PEER_INFO        0x08
#define TARGET_OEM_CONFIGURE_LCR        0x09
#define TARGET_OEM_CONFIGURE_LCI        0x0A
#define TARGET_OEM_CONFIGURE_WRU	0x80
#define TARGET_OEM_CONFIGURE_FTMRR	0x81

#define LOWI_LCI_REQ_WRU_OK	           0x12
#define LOWI_FTMRR_OK	                   0x13
#define LOWI_MESSAGE_CHANNEL_INFO_POSITION 12
#define LOWI_MESSAGE_WRU_POSITION          8
#define LOWI_MESSAGE_FTMRR_POSITION        8
#define LOWI_MESSAGE_REQ_ID_POSITION       4
#define LOWI_MESSAGE_MSGSUBTYPE_LEN        4
#define LOWI_MESSAGE_SUBIE_AND_LEN_OCTETS  2


struct lowi_if_netlink {
       struct sock      *lowi_if_sock;
       u_int32_t        lowi_if_pid;
       atomic_t         lowi_if_refcnt;
   };

struct ani_chinfo_resp {
    u_int8_t               num_chids;
    /* Stores ani_ch_info for each ch */
}__attribute__((packed));

struct ani_ch_info {
    u_int32_t ch_id;
    u_int32_t reserved0;
    wmi_host_channel wmi_chan;
}__attribute__((packed));

struct ani_header {
    u_int16_t msg_type;
    u_int16_t msg_length;
}__attribute__((packed));

struct ani_header_rsp {
    u_int16_t msg_type;
    u_int16_t msg_length;
    u_int32_t msg_subtype;
}__attribute__((packed));

struct ani_intf_info {
    u_int8_t intf_id;
    u_int8_t vdev_id;
}__attribute__((packed));

struct ani_reg_resp {
    u_int8_t               num_intf;
    /* Stores ani_intf_info for each intf */
}__attribute__((packed));

struct ani_err_msg_report {
    u_int16_t msg_type;
    u_int16_t msg_length;
    u_int32_t msg_subtype;
    u_int32_t req_id;
    u_int8_t dest_mac[IEEE80211_ADDR_LEN + 2]; //2 extea bytes for alignment
    u_int32_t err_code;
}__attribute__((packed));

#if 0

/* Used for Where are you: LCI measurement request */
struct wru_lci_request {
    u_int8_t sta_mac[IEEE80211_ADDR_LEN];
    u_int8_t dialogtoken;
    u_int16_t num_repetitions;
    u_int8_t id;
    u_int8_t len;
    u_int8_t meas_token;
    u_int8_t meas_req_mode;
    u_int8_t meas_type;
    u_int8_t loc_subject;
}__attribute__((packed));


#define MAX_NEIGHBOR_NUM 15
#define MAX_NEIGHBOR_LEN 50
struct ftmrr_request {
    u_int8_t sta_mac[IEEE80211_ADDR_LEN];
    u_int8_t dialogtoken;
    u_int16_t num_repetitions;
    u_int8_t id;
    u_int8_t len;
    u_int8_t meas_token;
    u_int8_t meas_req_mode;
    u_int8_t meas_type;
    u_int16_t rand_inter;
    u_int8_t min_ap_count;
    u_int8_t elem[MAX_NEIGHBOR_NUM*MAX_NEIGHBOR_LEN];
}__attribute__((packed));
#endif

#define IEEE80211_CHINFO_MAX_CHANNELS 40
#define MAX_NUM_INTERFACES        5

#define INTF_ID_FOR_STA           0
#define INTF_ID_FOR_P2P_GO        1
#define INTF_ID_FOR_P2P_CLI       2
#define INTF_ID_FOR_SOFT_AP       3
#define ANI_HDR_LEN (sizeof(struct ani_header))
#define ANI_HDR_LEN_RSP (sizeof(struct ani_header_rsp))
#define ANI_REG_RESP_LEN (sizeof(struct ani_reg_resp))
#define ANI_INTF_INFO_LEN (sizeof(struct ani_intf_info))
#define ANI_CHINFO_RESP_LEN (sizeof(struct ani_chinfo_resp))
#define ANI_CH_INFO_LEN (sizeof(struct ani_ch_info))
#define ANI_ERR_REP_RESP_LEN   20

int ath_lowi_if_netlink_init(void);
int ath_lowi_if_netlink_delete(void);
void ath_lowi_if_netlink_send(void *buf, int len);
void ath_lowi_if_nlsend_response(struct ieee80211com *ic, u_int8_t *data, u_int16_t datalen, u_int8_t msgtype, u_int32_t msg_subtype, u_int8_t error_code);
void ath_lowi_if_processmsg(void *data);
void ath_lowi_if_nlsend_reg_resp(struct ieee80211com *ic);
void ath_lowi_if_nlsend_chinfo_resp(struct ieee80211com *ic);
void ath_lowi_if_send_report_resp(struct ieee80211com *ic, int req_id, u_int8_t* dest_mac, int err_code);
struct ieee80211com* ath_lowi_if_get_ic(void *any, int* channel_mhz, char* sta_mac);

#else
#define ath_lowi_if_netlink_init() do {} while (0)
#define ath_lowi_if_netlink_delete() do {} while (0)
#endif

#endif /* _ATH_LOWI_IF_H__*/
