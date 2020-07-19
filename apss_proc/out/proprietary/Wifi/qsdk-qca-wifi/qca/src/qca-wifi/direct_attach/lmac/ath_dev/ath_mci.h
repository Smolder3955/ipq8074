/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * Public Interface for MCI module
 */

#ifndef _DEV_ATH_MCI_H
#define _DEV_ATH_MCI_H

#if ATH_SUPPORT_MCI

struct ath_mci_buf {
    TAILQ_ENTRY(ath_buf)    bf_list;

    void                   *bf_addr;     /* virtual addr of desc */
    dma_addr_t              bf_paddr;    /* physical addr of buffer */
    u_int32_t               bf_len;  /* len of data */

    OS_DMA_MEM_CONTEXT(bf_dmacontext)    /* OS Specific DMA context */
};


typedef enum gen_state {
	LNA_REQ,
	LNA_MASTER,
	LNA_SLAVE
} GEN_STATE_T;

typedef enum gpm_dbg_msg_type {
    MCI_GPM_DEBUG_MSG_TYPE_TEXT,
    MCI_GPM_DEBUG_MSG_TYPE_BYTE,
    MCI_GPM_DEBUG_MSG_TYPE_WORD
} MCI_GPM_DEBUG_MSG_TYPE_T;

typedef struct ath_mci_coex {
    u_int8_t            mci_support;
	MCI_BT_STATE_T      mci_bt_state;
	GEN_STATE_T         mci_gen_state;
	u_int32_t           mci_gpm_body[4];
    atomic_t            mci_cal_flag;
    struct ath_mci_buf  mci_sched_buf;
    struct ath_mci_buf  mci_gpm_buf;
    u_int32_t           mci_bt_cal_start;
    u_int8_t            mci_voice_priority;
} MCI_COEX_T;

#define ATH_MCI_SCHED_BUF_SIZE      (16 * 16)                       /* 16 entries, 4 dword each */
#define ATH_MCI_GPM_MAX_ENTRY       16
#define ATH_MCI_GPM_BUF_SIZE        (ATH_MCI_GPM_MAX_ENTRY * 16)    /* Each entry is 4 dword */

#define ATH_MCI_VOICE_EV3           3
#define ATH_MCI_VOICE_2EV3          6
#define ATH_MCI_VOICE_2EV5          7
#define ATH_MCI_VOICE_3EV3          8

#define ATH_MCI_VOICE_SCO_PRI       110
#define ATH_MCI_VOICE_EV3_PRI       112
#define ATH_MCI_VOICE_2EV3_PRI      114
#define ATH_MCI_VOICE_2EV5_PRI      116
#define ATH_MCI_VOICE_3EV3_PRI      118
#define ATH_MCI_HID_PRIORITY        94
#define ATH_MCI_PAN_PRIORITY        52
#define ATH_MCI_A2DP_PRIORITY       90
#define ATH_MCI_RFCOMM_PRIORITY     50
#define ATH_MCI_INQUIRY_PRIORITY    62
#define ATH_MCI_HI_PRIORITY         60

/* Exported functions */
int ath_coex_mci_attach(struct ath_softc *sc);
void ath_coex_mci_detach(struct ath_softc *sc);
void ath_coex_mci_update_wlan_channels(struct ath_softc *sc);
void ath_coex_mci_intr(struct ath_softc *sc);
int ath_coex_mci_setup(struct ath_softc *sc);
void ath_coex_mci_send_gpm(struct ath_softc *sc, u_int32_t *payload);
u_int16_t ath_coex_mci_get_sco_priority(struct ath_softc *sc);
#endif /* ATH_SUPPORT_MCI */

#endif /* end of _DEV_ATH_MCI_H */
