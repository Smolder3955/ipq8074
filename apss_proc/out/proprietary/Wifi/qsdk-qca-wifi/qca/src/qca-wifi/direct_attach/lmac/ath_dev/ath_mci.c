/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ath_internal.h"
#include "ath_mci.h"
#include "ath_bt.h"
#include "qdf_mem.h"

#if ATH_SUPPORT_MCI

static int ath_coex_mci_bt_cal_do(struct ath_softc *sc, int tx_timeout, int rx_timeout);

static void ath_coex_mci_print_msg(struct ath_softc *sc, bool send, u_int8_t hdr, int len, u_int32_t *pl)
{
#if DBG
    char s[128];
    char *p = s;
    int i;
    u_int8_t *p_data = (u_int8_t *) pl;

    if (send) {
        p += snprintf(s, 60,
                "(MCI) INTR >>>>> Hdr: %02X, Len: %d, Payload:", hdr, len);
    }
    else {
        p += snprintf(s, 60,
                "(MCI) INTR <<<<< Hdr: %02X, Len: %d, Payload:", hdr, len);
    }
    for ( i=0; i<len; i++)
    {
        p += snprintf(p, 60, " %02x", *(p_data + i));
    }
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", s);
/*
    for ( i=0; i<(len + 3)/4; i++)
    {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)   0x%08x\n", *(pl + i));
    }
*/
#endif
}

static void ath_coex_mci_print_INT(struct ath_softc *sc, u_int32_t mciInt, u_int32_t rxMsgInt)
{
#if DBG
    if (rxMsgInt & ~HAL_MCI_INTERRUPT_RX_MSG_MONITOR) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX,
            "(MCI) mciInt = 0x%x, mciIntRxMsg = 0x%x\n",
            mciInt, rxMsgInt);
    }
    if (mciInt & HAL_MCI_INTERRUPT_SW_MSG_DONE) {
        //DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) MCI INT SW msg done\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_CPU_INT_MSG) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT CPU INT\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_RX_CHKSUM_FAIL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT rx ChkSum fail\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_RX_HW_MSG_FAIL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT rx HW msg fail\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_RX_SW_MSG_FAIL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT rx SW msg fail\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_TX_HW_MSG_FAIL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT tx HW msg fail\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_TX_SW_MSG_FAIL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT tx SW msg fail\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_REMOTE_SLEEP_UPDATE) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) MCI INT remote sleep update: %d\n",
            ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_REMOTE_SLEEP, NULL));
    }
    if (mciInt & HAL_MCI_INTERRUPT_RX_MSG) {
        //DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) MCI INT Rx MSG\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_RX_INVALID_HDR) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT rx INVALID_HDR\n");
    }
    if (mciInt & HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: MCI INT rx CONT_INFO_TIMEOUT\n");
    }

    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_GPM) {
        //DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg GPM\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_SCHD_INFO) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg scheduling info\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO) {
        //DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg LNA Info\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_SYS_SLEEPING) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg Sys sleeping\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_SYS_WAKING) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg Sys waking\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_REMOTE_RESET) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg Remote Reset\n");
    }
    if (rxMsgInt & HAL_MCI_INTERRUPT_RX_MSG_REQ_WAKE) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Rx Msg Req wake\n");
    }
#endif
}

int
ath_coex_mci_buf_alloc(struct ath_softc *sc, struct ath_mci_buf *buf)
{
    int error = 0;

    buf->bf_addr = (void *)OS_MALLOC_CONSISTENT(sc->sc_osdev,
                                       buf->bf_len, &buf->bf_paddr,
                                       OS_GET_DMA_MEM_CONTEXT(buf, bf_dmacontext),
                                       sc->sc_reg_parm.shMemAllocRetry);

    if (buf->bf_addr == NULL) {
        error = -ENOMEM;
    }
    return error;
}

void
ath_coex_mci_buf_free(struct ath_softc *sc, struct ath_mci_buf *buf)
{
    if (buf->bf_addr) {
        OS_FREE_CONSISTENT(sc->sc_osdev, buf->bf_len,
                           buf->bf_addr, buf->bf_paddr,
                           OS_GET_DMA_MEM_CONTEXT(buf, bf_dmacontext));
    }
}

int
ath_coex_mci_setup(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_mci_coex *mci = &sc->sc_mci;
    int error = 0;
    u_int8_t *pByte;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s\n", __func__);

    /* allocate buffers to hold both scheduling messages and GPM. */
    mci->mci_sched_buf.bf_len = ATH_MCI_SCHED_BUF_SIZE + ATH_MCI_GPM_BUF_SIZE;
    if (ath_coex_mci_buf_alloc(sc, &mci->mci_sched_buf) != 0) {
        error = -ENOMEM;
        DPRINTF(sc, ATH_DEBUG_FATAL, "(MCI) %s: MCI buffer allocation failed.\n", __func__);
        goto fail;
    }
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s: SCHED buffer addr = 0x%x, len = 0x%x, phy addr = 0x%x\n",
            __func__, mci->mci_sched_buf.bf_addr, mci->mci_sched_buf.bf_len, mci->mci_sched_buf.bf_paddr);

    /* Part of the buffer is for sched_buf */
    mci->mci_sched_buf.bf_len = ATH_MCI_SCHED_BUF_SIZE;

    /* Initialize the buffer */
    qdf_mem_set(mci->mci_sched_buf.bf_addr, mci->mci_sched_buf.bf_len, MCI_GPM_RSVD_PATTERN);

    /* Other part of the buffer is for GPM. */
    mci->mci_gpm_buf.bf_len = ATH_MCI_GPM_BUF_SIZE;
    mci->mci_gpm_buf.bf_addr = (u_int8_t *)mci->mci_sched_buf.bf_addr + mci->mci_sched_buf.bf_len;
    mci->mci_gpm_buf.bf_paddr = mci->mci_sched_buf.bf_paddr + mci->mci_sched_buf.bf_len;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s: GPM buffer addr = 0x%x, len = 0x%x, phy addr = 0x%x\n",
            __func__, mci->mci_gpm_buf.bf_addr, mci->mci_gpm_buf.bf_len, mci->mci_gpm_buf.bf_paddr);

    /* Initialize the buffer */
    qdf_mem_set(mci->mci_gpm_buf.bf_addr, mci->mci_gpm_buf.bf_len, MCI_GPM_RSVD_PATTERN);

    /*
     * For OS's that need to allocate dma context to be used to
     * send down to hw, do that here. (BSD is the only one that needs
     * it currently.)
     */
    ALLOC_DMA_CONTEXT_POOL(sc->sc_osdev, name, nbuf);

    ath_hal_mci_setup(sc->sc_ah,
                       mci->mci_gpm_buf.bf_paddr,
                       mci->mci_gpm_buf.bf_addr,
                       (mci->mci_gpm_buf.bf_len >> 4),
                       mci->mci_sched_buf.bf_paddr);

    return 0;
fail:
    return error;
}

int
ath_coex_mci_attach(struct ath_softc *sc)
{
    struct ath_mci_coex *mci = &sc->sc_mci;

    mci->mci_support = AH_TRUE;
    return 0;
}

void
ath_coex_mci_detach(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_mci_coex *mci = &sc->sc_mci;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s\n", __func__);

    /* Both schedule and gpm buffers will be released. */
    ath_coex_mci_buf_free(sc, &mci->mci_sched_buf);

    ath_hal_mci_detach(ah);
}

static void
ath_coex_mci_debug_print(struct ath_softc *sc, u_int8_t type, u_int8_t *pData)
{
    u_int32_t *pData32 = (u_int32_t *)pData;
    u_int32_t data = *(pData32 + 3);

    *(pData + 12) = 0;
    DPRINTF(sc, ATH_DEBUG_BTCOEX,
        "(MCI) BT_DEBUG: %7s - 0x%08x, (%02x, %02x, %02x, %02x)\n",
        pData + 5, data,
        data & 0xff, *(pData + 13), *(pData + 14), *(pData + 15));
    if (( (*(pData + 5) == 'I') || (*(pData + 8) == 'N') ) && (data != 0) ) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR: in BT\n");
    }
}

static void ath_coex_mci_cal_msg(struct ath_softc *sc, u_int8_t opcode,
                                 u_int8_t *rx_payload)
{
    u_int32_t payload[4] = {0, 0, 0, 0};

    switch (opcode)
    {
        case MCI_GPM_BT_CAL_REQ:
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) receive BT_CAL_REQ\n");
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL) == MCI_BT_AWAKE)
            {
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_CAL_START, NULL);
                ath_coex_mci_bt_cal_do(sc, 1000, 1000);
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) State mismatches: %d\n",
                        ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL));
            }
            break;
        case MCI_GPM_BT_CAL_DONE:
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) receive BT_CAL_DONE\n");
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL) == MCI_BT_CAL)
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) ERROR ILLEGAL!\n");
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) BT not in CAL state.\n");
            }
            break;
        case MCI_GPM_BT_CAL_GRANT:
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) receive BT_CAL_GRANT\n");

            /* Send WLAN_CAL_DONE for now */
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Send WLAN_CAL_DONE\n");
            MCI_GPM_SET_CAL_TYPE(payload, MCI_GPM_WLAN_CAL_DONE);
            ath_coex_mci_send_gpm(sc, &payload[0]);
            break;
        default:
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) Unknown GPM CAL message.\n");
            break;
    }
}

void
ath_coex_mci_update_wlan_channels(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    HAL_CHANNEL *chan = &sc->sc_curchan;
    u_int32_t channel_info[4] =
        { 0x00000000, 0xffffffff, 0xffffffff, 0x7fffffff };
    int32_t wl_chan, bt_chan, bt_start = 0, bt_end = 79;

    /* BT channel frequency is 2402 + k, k = 0 ~ 78 */
    if ((btinfo->wlan_state == ATH_WLAN_STATE_CONNECT) &&
        (chan->channel_flags & CHANNEL_2GHZ) &&
        (chan->channel >= 2402))
    {
        wl_chan = chan->channel - 2402;
        if (chan->channel_flags & CHANNEL_HT20) {
            bt_start = wl_chan - 10;
            bt_end = wl_chan + 10;
        }
        else if (chan->channel_flags & CHANNEL_HT40PLUS) {
            bt_start = wl_chan - 10;
            bt_end = wl_chan + 30;
        }
        else if (chan->channel_flags & CHANNEL_HT40MINUS) {
            bt_start = wl_chan - 30;
            bt_end = wl_chan + 10;
        }
        else {
            bt_start = wl_chan - 10;
            bt_end = wl_chan + 10;
        }
        /* TODO btCoexAFHSideBand */
        bt_start -= 7;
        bt_end += 7;

        if (bt_start < 0) {
            bt_start = 0;
        }
        if (bt_end > MCI_NUM_BT_CHANNELS) {
            bt_end = MCI_NUM_BT_CHANNELS;
        }
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) WLAN use channel %d\n", chan->channel);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) mask BT channel %d - %d\n", bt_start, bt_end);
        for ( bt_chan = bt_start; bt_chan < bt_end; bt_chan++) {
            MCI_GPM_CLR_CHANNEL_BIT(&channel_info[0], bt_chan);
        }
    }
    else {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) WLAN not use any 2G channel, unmask all for BT\n");
    }
    ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SEND_WLAN_CHANNELS, &channel_info[0]);
}

#if DBG
static char *ath_coex_mci_string_profile(int value)
{
    switch(value) {
        case MCI_GPM_COEX_PROFILE_UNKNOWN:
            return "Unknown";
        case MCI_GPM_COEX_PROFILE_RFCOMM:
            return "RFCOMM";
        case MCI_GPM_COEX_PROFILE_A2DP:
            return "A2DP";
        case MCI_GPM_COEX_PROFILE_HID:
            return "HID";
        case MCI_GPM_COEX_PROFILE_BNEP:
            return "BNEP";
        case MCI_GPM_COEX_PROFILE_VOICE:
            return "Voice";
        default:
            break;
    }
    return "--";
}

static char *ath_coex_mci_string_state(int value)
{
    if(value == MCI_GPM_COEX_PROFILE_STATE_START) {
        return "Start";
    }
    if(value == MCI_GPM_COEX_PROFILE_STATE_END) {
        return "End";
    }
    return "--";
}

static char *ath_coex_mci_string_role(int value)
{
    if(value == MCI_GPM_COEX_PROFILE_MASTER) {
        return "Master";
    }
    if(value == MCI_GPM_COEX_PROFILE_SLAVE) {
        return "Slave";
    }
    return "--";
}

static char *ath_coex_mci_string_rate(int value)
{
    if(value == ATH_COEX_BT_RATE_BR) {
        return "BR";
    }
    if(value == ATH_COEX_BT_RATE_EDR) {
        return "EDR";
    }
    return "--";
}
#endif /* DBG */

static void ath_coex_mci_dump_profile_info(struct ath_softc *sc,
                                        ATH_COEX_MCI_PROFILE_INFO *pInfo)
{
#if DBG
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)  profileType    = %s\n",
        ath_coex_mci_string_profile(pInfo->profileType));
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)  LinkId         = %d\n",
        pInfo->connHandle);
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)  profileState   = %s\n",
        ath_coex_mci_string_state(pInfo->profileState));
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)  connectionRole = %s\n",
        ath_coex_mci_string_role(pInfo->connectionRole));
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI)  btRate         = %s\n",
        ath_coex_mci_string_rate(pInfo->btRate));
    DPRINTF(sc, ATH_DEBUG_BTCOEX,
        "(MCI)  voiceType      = %d\n", pInfo->voiceType);
    DPRINTF(sc, ATH_DEBUG_BTCOEX,
        "(MCI)  T              = %d\n", pInfo->T);
    DPRINTF(sc, ATH_DEBUG_BTCOEX,
        "(MCI)  W              = %d\n", pInfo->W);
    DPRINTF(sc, ATH_DEBUG_BTCOEX,
        "(MCI)  A              = %d\n", pInfo->A);
#endif /* DBG */
}

static void ath_coex_mci_fill_profile_info(struct ath_softc *sc,
                                            ATH_COEX_MCI_PROFILE_INFO *pInfo,
                                            u_int8_t *rx_payload)
{
    struct ath_mci_coex *mci = &sc->sc_mci;

    pInfo->profileType      = *(rx_payload + MCI_GPM_COEX_B_PROFILE_TYPE);;
    pInfo->connHandle       = *(rx_payload + MCI_GPM_COEX_B_PROFILE_LINKID);
    pInfo->profileState     = *(rx_payload + MCI_GPM_COEX_B_PROFILE_STATE);
    pInfo->connectionRole   = *(rx_payload + MCI_GPM_COEX_B_PROFILE_ROLE);
    pInfo->btRate           = *(rx_payload + MCI_GPM_COEX_B_PROFILE_RATE);
    pInfo->voiceType        = *(rx_payload + MCI_GPM_COEX_B_PROFILE_VOTYPE);
    pInfo->T = *((u_int16_t *) (rx_payload + MCI_GPM_COEX_H_PROFILE_T));
    pInfo->W =                *(rx_payload + MCI_GPM_COEX_B_PROFILE_W);
    pInfo->A =                *(rx_payload + MCI_GPM_COEX_B_PROFILE_A);


    /* Get the latest voice priority */
    if (pInfo->profileType == MCI_GPM_COEX_PROFILE_VOICE) {
        if (pInfo->voiceType == ATH_MCI_VOICE_2EV3) {
            mci->mci_voice_priority = ATH_MCI_VOICE_2EV3_PRI;
        }
        else if (pInfo->voiceType == ATH_MCI_VOICE_EV3) {
            mci->mci_voice_priority = ATH_MCI_VOICE_EV3_PRI;
        }
        else if (pInfo->voiceType == ATH_MCI_VOICE_2EV5) {
            mci->mci_voice_priority = ATH_MCI_VOICE_2EV5_PRI;
        }
        else if (pInfo->voiceType == ATH_MCI_VOICE_3EV3) {
            mci->mci_voice_priority = ATH_MCI_VOICE_3EV3_PRI;
        }
        else {
            mci->mci_voice_priority = ATH_MCI_VOICE_SCO_PRI;
        }
    }

    ath_coex_mci_dump_profile_info(sc, pInfo);

    if (pInfo->connHandle == MCI_GPM_INVALID_PROFILE_HANDLE) {
        pInfo->connHandle = INVALID_PROFILE_TYPE_HANDLE;
    }

    if (*(rx_payload + MCI_GPM_COEX_B_PROFILE_STATE) == MCI_GPM_COEX_PROFILE_STATE_START) {
        pInfo->profileState = ATH_COEX_BT_PROFILE_START;
    }
    else {
        pInfo->profileState = ATH_COEX_BT_PROFILE_END;
    }

    if (*(rx_payload + MCI_GPM_COEX_B_PROFILE_ROLE) == MCI_GPM_COEX_PROFILE_MASTER) {
        pInfo->connectionRole = ATH_COEX_BT_ROLE_MASTER;
    }
    else {
        pInfo->connectionRole = ATH_COEX_BT_ROLE_SLAVE;
    }
}

static void ath_coex_mci_coex_msg(struct ath_softc *sc, u_int8_t opcode,
                                  u_int8_t *rx_payload)
{
    u_int32_t payload[4] = {0, 0, 0, 0};
    u_int32_t version;
    u_int8_t major;
    u_int8_t minor;
    ATH_COEX_MCI_PROFILE_INFO profileInfo;
    ATH_COEX_MCI_BT_STATUS_UPDATE_INFO statusInfo;
    u_int32_t seq_num;

    switch (opcode)
    {
        case MCI_GPM_COEX_VERSION_QUERY:
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Recv GPM COEX Version Query.\n");
            version = ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SEND_WLAN_COEX_VERSION, NULL);
            break;

        case MCI_GPM_COEX_VERSION_RESPONSE:
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Recv GPM COEX Version Response.\n");
            major = *(rx_payload + MCI_GPM_COEX_B_MAJOR_VERSION);
            minor = *(rx_payload + MCI_GPM_COEX_B_MINOR_VERSION);
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) BT Coex version: %d.%d\n", major, minor);
            version = (major << 8) + minor;
            version = ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_COEX_VERSION, &version);
            break;

        case MCI_GPM_COEX_STATUS_QUERY:
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Recv GPM COEX Status Query = 0x%02x.\n",
                *(rx_payload + MCI_GPM_COEX_B_WLAN_BITMAP));
            /*if ((*(rx_payload + MCI_GPM_COEX_B_WLAN_BITMAP)) & MCI_GPM_COEX_QUERY_WLAN_ALL_INFO)*/
            {
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SEND_WLAN_CHANNELS, NULL);
            }
            break;

        case MCI_GPM_COEX_BT_PROFILE_INFO:
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Recv GPM COEX BT_Profile_Info.\n");
            ath_coex_mci_fill_profile_info(sc, &profileInfo, rx_payload);
            if ((profileInfo.profileType == MCI_GPM_COEX_PROFILE_UNKNOWN) ||
                (profileInfo.profileType >= MCI_GPM_COEX_PROFILE_MAX))
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX,
                        "%s: Illegal profile type = %d, state = %d\n",
                        __func__, profileInfo.profileType, profileInfo.profileState);
                break;
            }
            ath_bt_coex_event(sc, ATH_COEX_EVENT_BT_MCI_PROFILE_INFO, (void *) &profileInfo);
            break;

        case MCI_GPM_COEX_BT_STATUS_UPDATE:
            statusInfo.type       = *(rx_payload + MCI_GPM_COEX_B_STATUS_TYPE);
            statusInfo.connHandle = *(rx_payload + MCI_GPM_COEX_B_STATUS_LINKID);
            statusInfo.state      = *(rx_payload + MCI_GPM_COEX_B_STATUS_STATE);

            seq_num = *((u_int32_t *)(rx_payload + 12));
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Recv GPM COEX BT_Status_Update: "
                "type=%d, linkId=%d, state=%d, SEQ=%d\n",
                statusInfo.type,
                statusInfo.connHandle,
                statusInfo.state,
                seq_num);
            ath_bt_coex_event(sc, ATH_COEX_EVENT_BT_MCI_STATUS_UPDATE, (void *) &statusInfo);
            break;

        default:
            DPRINTF(sc, ATH_DEBUG_BTCOEX,
                "(MCI) Unknown GPM COEX message = 0x%02x\n", opcode);
            break;
    }
}

void
ath_coex_mci_intr(struct ath_softc *sc)
{
    u_int32_t mciInt, mciIntRxMsg;
    u_int32_t offset, subtype, opcode;
    struct ath_mci_coex *mci = &sc->sc_mci;
    u_int32_t *pGpm;
    u_int8_t *pByteData;
    u_int32_t more_data = HAL_MCI_GPM_MORE;
    bool skip_gpm = false;

    ath_hal_mci_get_int(sc->sc_ah, &mciInt, &mciIntRxMsg);

    if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_ENABLE, NULL) == 0) {
        ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_INIT_GPM_OFFSET, NULL);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) INTR but MCI_disabled\n");

        DPRINTF(sc, ATH_DEBUG_BTCOEX,
            "(MCI) MCI interrupt: mciInt = 0x%x, mciIntRxMsg = 0x%x\n",
            mciInt, mciIntRxMsg);

        return;
    }
    ath_coex_mci_print_INT(sc, mciInt, mciIntRxMsg);

#if 0
    /* Remote reset check should be done before req_wake. */
    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_REMOTE_RESET) {
        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_REMOTE_RESET;
    }
#endif

    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_REQ_WAKE) {
        u_int32_t payload4[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffff00};

        /*
         * The following REMOTE_RESET and SYS_WAKING used to sent only when BT
         * wake up. Now they are always sent, as a recovery method to reset BT
         * MCI's RX alignment.
         */
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 1. INTR Send REMOTE_RESET\n");
        ath_hal_mci_send_message(sc->sc_ah, MCI_REMOTE_RESET, 0, payload4, 16, AH_TRUE, AH_FALSE);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 1. INTR Send SYS_WAKING\n");
        ath_hal_mci_send_message(sc->sc_ah, MCI_SYS_WAKING, 0, NULL, 0, AH_TRUE, AH_FALSE);

        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_REQ_WAKE;
        ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_RESET_REQ_WAKE, NULL);

        /* always do this for recovery and 2G/5G toggling and LNA_TRANS */
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 1. Set BT state to AWAKE.\n");
        ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_AWAKE, NULL);
    }

#if 0
    /* process REMOTE_SLEEP_UPDATE */
    if (mciInt & HAL_MCI_INTERRUPT_REMOTE_SLEEP_UPDATE) {
        mciInt &= ~HAL_MCI_INTERRUPT_REMOTE_SLEEP_UPDATE;

        if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL) == MCI_BT_SLEEP)
        {
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_REMOTE_SLEEP, NULL) == MCI_BT_SLEEP)
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 4. BT stays in SLEEP mode.\n");
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 4. Set BT state to AWAKE.\n");
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_AWAKE, NULL);
            }
        }
        else {
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_REMOTE_SLEEP, NULL) == MCI_BT_AWAKE)
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 4. BT stays in AWAKE mode.\n");
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 4. Set BT state to SLEEP.\n");
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_SLEEP, NULL);
            }
        }
    }
#endif

    /* Processing SYS_WAKING/SYS_SLEEPING */
    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_SYS_WAKING) {
        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_SYS_WAKING;

        if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL) == MCI_BT_SLEEP)
        {
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_REMOTE_SLEEP, NULL) == MCI_BT_SLEEP)
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 2. BT stays in SLEEP mode.\n");
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 2. Set BT state to AWAKE.\n");
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_AWAKE, NULL);
            }
        }
        else {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 2. BT stays in AWAKE mode.\n");
        }
    }

    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_SYS_SLEEPING) {
        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_SYS_SLEEPING;

        if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_BT, NULL) == MCI_BT_AWAKE)
        {
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_REMOTE_SLEEP, NULL) == MCI_BT_AWAKE)
            {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 3. BT stays in AWAKE mode.\n");
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 3. Set BT state to SLEEP.\n");
                ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_SET_BT_SLEEP, NULL);
            }
        }
        else {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) 3. BT stays in SLEEP mode.\n");
        }
    }

    if ((mciInt & HAL_MCI_INTERRUPT_RX_INVALID_HDR) ||
        (mciInt & HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT))
    {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) MCI RX broken, skip GPM messages\n");
        ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_RECOVER_RX, NULL);
        skip_gpm = true;
    }

    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_SCHD_INFO) {
        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_SCHD_INFO;

        offset = ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_LAST_SCHD_MSG_OFFSET, NULL);
    }

    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_GPM) {
        mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_GPM;

        while (more_data == HAL_MCI_GPM_MORE) {
            pGpm = mci->mci_gpm_buf.bf_addr;
            offset = ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_NEXT_GPM_OFFSET, &more_data);

            if (offset == HAL_MCI_GPM_INVALID) {
                break;
            }
            pGpm += (offset >> 2);
            ath_coex_mci_print_msg(sc, false, MCI_GPM, 16, pGpm);
            /* The first dword is timer. The real data starts from 2nd dword. */
            subtype = MCI_GPM_TYPE(pGpm);
            opcode = MCI_GPM_OPCODE(pGpm);

            if (!skip_gpm) {
                if (MCI_GPM_IS_CAL_TYPE(subtype)) {
                    ath_coex_mci_cal_msg(sc, subtype, (u_int8_t*) pGpm);
                }
                else {
                    switch (subtype) {
                    case MCI_GPM_COEX_AGENT:
                        ath_coex_mci_coex_msg(sc, opcode, (u_int8_t*) pGpm);
                        break;
                    case MCI_GPM_BT_DEBUG:
                        ath_coex_mci_debug_print(sc, opcode, (u_int8_t *) pGpm);
                        break;
                    default:
                        DPRINTF(sc, ATH_DEBUG_BTCOEX,
                            "(MCI) Unknown GPM message.\n");
                        break;
                    }
                }
            }
            MCI_GPM_RECYCLE(pGpm);
        }
    }

    if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_MONITOR) {
        if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL) {
            mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL;
            //DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) LNA_CONTROL\n");
        }
        if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO) {
            mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO;
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) LNA_INFO\n");
        }
        if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO) {
            int8_t value_dbm =
                    ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_CONT_RSSI_POWER, NULL);

            mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO;
            if (ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_CONT_TXRX, NULL)) {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) CONT_INFO: (tx) pri = %d, pwr = %d dBm\n",
                    ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_CONT_PRIORITY, NULL),
                    value_dbm);
            }
            else {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) CONT_INFO: (rx) pri = %d, rssi = %d dBm\n",
                    ath_hal_mci_state(sc->sc_ah, HAL_MCI_STATE_CONT_PRIORITY, NULL),
                    value_dbm);
            }
        }
        if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK) {
            mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK;
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) CONT_NACK\n");
        }
        if (mciIntRxMsg & HAL_MCI_INTERRUPT_RX_MSG_CONT_RST) {
            mciIntRxMsg &= ~HAL_MCI_INTERRUPT_RX_MSG_CONT_RST;
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) CONT_RST\n");
        }
    }

    if ((mciInt & HAL_MCI_INTERRUPT_RX_INVALID_HDR) ||
        (mciInt & HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT))
    {
        ath_bt_coex_event(sc, ATH_COEX_EVENT_BT_NOOP, NULL);
        mciInt &= ~(HAL_MCI_INTERRUPT_RX_INVALID_HDR |
                    HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT);
    }

    if (mciIntRxMsg & 0xfffffffe) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s: Not processed IntRxMsg = 0x%x\n", __func__, mciIntRxMsg);
    }
}

void
ath_coex_mci_send_gpm(struct ath_softc *sc, u_int32_t *payload)
{
    ath_hal_mci_send_message(sc->sc_ah, MCI_GPM, 0, payload, 16,
        AH_FALSE, AH_TRUE);
}

static int ath_coex_mci_bt_cal_do(struct ath_softc *sc, int tx_timeout, int rx_timeout)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
    HAL_STATUS status;

    ATH_PS_WAKEUP(sc);

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s\n", __func__);

    /* Set the flag so that other functions will not try to restart tx/rx DMA */
    /* Note: we should re-organized the fellowing code to use ath_reset_start(),
       ath_reset(), and ath_reset_end() */
    atomic_inc(&sc->sc_in_reset);
    ATH_USB_TX_STOP(sc->sc_osdev);
    ATH_INTR_DISABLE(sc);
    ATH_STOPRECV_EX(sc, rx_timeout);            /* turn off frame recv */

    /* XXX: do not flush receive queue here. We don't want
     * to flush data frames already in queue because of
     * changing channel. */

    /* Set coverage class */
    ath_hal_setcoverageclass(ah, sc->sc_coverageclass, 0);

    ATH_HTC_TXRX_STOP(sc, AH_TRUE);

    ATH_RESET_LOCK(sc);
    sc->sc_paprd_done_chain_mask = 0;

    sc->sc_mci.mci_bt_cal_start = ath_hal_gettsf32(sc->sc_ah);

#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan,
                       ht_macmode, sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_TRUE, &status,
                       AH_FALSE))
    {
        printk("(MCI) %s: unable to stop rx dma hal status %u\n",
               __func__, status);
    }
    else {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "(MCI) %s: BT calibration takes %d us\n",
                __func__,
                (ath_hal_gettsf32(sc->sc_ah) - sc->sc_mci.mci_bt_cal_start));
    }

    /* Clear the "In Reset" flag. */
    atomic_dec(&sc->sc_in_reset);
    ASSERT(atomic_read(&sc->sc_in_reset) >= 0);

    ATH_HTC_ABORTTXQ(sc);
    ATH_RESET_UNLOCK(sc);

    /*
     * Re-enable rx framework.
     */
    ath_wmi_start_recv(sc);

    if (ATH_STARTRECV(sc) != 0) {
        printk("%s: unable to restart recv logic\n", __func__);
    }

#ifdef ATH_GEN_TIMER
    /* Inform the generic timer module that a reset just happened. */
    ath_gen_timer_post_reset(sc);
#endif

    /*
     * Re-enable interrupts.
     */
    ATH_INTR_DISABLE(sc);
    ATH_INTR_ENABLE(sc);
    ATH_USB_TX_START(sc->sc_osdev);

    ATH_PS_SLEEP(sc);

    return 0;
}

#endif /* ATH_SUPPORT_MCI */
