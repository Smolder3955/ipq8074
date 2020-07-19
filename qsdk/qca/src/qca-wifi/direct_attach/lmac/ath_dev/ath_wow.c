/*
 * Copyright (c) 2005, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 *  ath_wow.c:  implementation of WakeonWireless in ath layer 
 */
 
#include "ath_internal.h" 

#if ATH_WOW

/* Forward declarations*/
int ath_wow_create_pattern(ath_dev_t dev);

int
ath_get_wow_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_hasWow && (ATH_DEV_TO_SC(dev))->sc_wowenable);
}

#if ATH_WOW_OFFLOAD
int ath_get_wowoffload_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_wowSupportOffload);
}
int ath_get_wowoffload_gtk_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_hasWowGtkOffload);
}
int ath_get_wowoffload_arp_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_hasWowArpOffload);
}
int ath_get_wowoffload_max_arp_offloads(ath_dev_t dev)
{
    return WOW_OFFLOAD_ARP_INFO_MAX;
}
int ath_get_wowoffload_ns_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_hasWowNsOffload);
}
int ath_get_wowoffload_max_ns_offloads(ath_dev_t dev)
{
    return WOW_OFFLOAD_NS_INFO_MAX;
}
int ath_get_wowoffload_4way_hs_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_wowSupport4wayHs);
}
int ath_get_wowoffload_acer_magic_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_wowSupportAcerMagic);
}
int ath_get_wowoffload_acer_swka_support(ath_dev_t dev, u_int32_t id)
{
    return ((ATH_DEV_TO_SC(dev))->sc_wowSupportAcerSwKa) &&
            (id < WOW_OFFLOAD_ACER_SWKA_MAX);
}
bool ath_wowoffload_acer_magic_enable(ath_dev_t dev)
{
    return (ath_get_wowoffload_acer_magic_support(dev) &&
            ((ATH_DEV_TO_SC(dev))->sc_wowInfo->acer_magic.valid));
}
#endif /* ATH_WOW_OFFLOAD */

int
ath_set_wow_enable(ath_dev_t dev, int clearbssid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t wakeUpEvents = sc->sc_wowInfo->wakeUpEvents;
	HAL_INT status;
#if ATH_WOW_OFFLOAD
    bool wowOffload = ath_get_wowoffload_support(dev);
#else
    bool wowOffload = FALSE;
#endif /* ATH_WOW_OFFLOAD */

    if (sc->sc_invalid)
        return -EIO;
    
    ATH_PS_WAKEUP(sc);

    // Set SCR to force wake
    ath_pwrsave_awake(sc);

    /* Enable the pattern matching registers */
    if (wakeUpEvents & ATH_WAKE_UP_PATTERN_MATCH) {
        ath_wow_create_pattern(sc);
    }

	// Save the current interrupt mask so that we can restore it after system wakes up 
    sc->sc_wowInfo->intrMaskBeforeSleep = ath_hal_intrget(ah);
    // Disable interrupts and poll on isr exiting.
    sc->sc_invalid = 1;
    ath_hal_intrset(ah, 0);
    ath_wait_sc_inuse_cnt(sc, 1000);

    /*
     * disable interrupt and clear pending isr again in case "ath_handle_intr"
     * was running at the same time.
     */
    ath_hal_intrset(ah, 0);
    if (ath_hal_intrpend(ah)) {
        ath_hal_getisr(ah, &status, HAL_INT_LINE, HAL_MSIVEC_MISC);
    }
    /*
     * To avoid false wake, we enable beacon miss interrupt only when system sleep.
     */
    sc->sc_imask = HAL_INT_GLOBAL;
    if (wakeUpEvents & ATH_WAKE_UP_BEACON_MISS) {
        sc->sc_imask |= HAL_INT_BMISS;
    }    
    sc->sc_invalid = 0;
    ath_hal_intrset(ah, sc->sc_imask);
    /*
     * Call ath_hal_intrset(ah, sc->sc_imask) again since ath_hal_intrset(ah, 0)
     * was called twice previously. ah_ier_refcount has been bump up to 2.
     * Without the second call, AR_IER won't be enabled when compile flag
     * HAL_INTR_REFCOUNT_DISABLE is set to 1. EV 75289.
     */
    ath_hal_intrset(ah, sc->sc_imask);

#if ATH_WOW_OFFLOAD
    if (wowOffload) {
        /* TX and RX have to be stopped before WoW offload handshake
         * since the offload information is downloaded using the MAC PCU
         * buffer */
        ath_stoprecv(sc, 0);
        ath_tx_flush(sc);
        if (sc->sc_wowInfo->rekeyInfo->valid == FALSE) {
            /*
             * There is no key and replay_counter passed down for WPA.
             * Clear to zero to avoid replay_counter error.
             */
            ath_wowoffload_remove_gtk_rekey_info(dev);
        }
        ath_hal_wowOffloadPrep(ah);
        ath_hal_wowOffloadDownloadRekeyData(ah, (u_int32_t *)&sc->sc_wowInfo->rekeyInfo->dl_data, 
                                        sizeof(struct wow_offload_rekey_download_data));
        ath_hal_wowOffloadDownloadAcerMagic(ah,
                                            sc->sc_wowInfo->acer_magic.valid,
                                            &sc->sc_wowInfo->acer_magic.data[0],
                                            sizeof(sc->sc_wowInfo->acer_magic.data));
        ath_hal_wowOffloadDownloadAcerSWKA( ah, 0,
                                            sc->sc_wowInfo->acer_swka[0].valid,
                                            sc->sc_wowInfo->acer_swka[0].period,
                                            sc->sc_wowInfo->acer_swka[0].size,
                                            &sc->sc_wowInfo->acer_swka[0].data[0]);
        ath_hal_wowOffloadDownloadAcerSWKA( ah, 1,
                                            sc->sc_wowInfo->acer_swka[1].valid,
                                            sc->sc_wowInfo->acer_swka[1].period,
                                            sc->sc_wowInfo->acer_swka[1].size,
                                            &sc->sc_wowInfo->acer_swka[1].data[0]);
        ath_hal_wowOffloadDownloadArpOffloadData(ah, 0, (u_int32_t *)&sc->sc_wowInfo->arp_info[0]);
        ath_hal_wowOffloadDownloadArpOffloadData(ah, 1, (u_int32_t *)&sc->sc_wowInfo->arp_info[1]);
        ath_hal_wowOffloadDownloadNsOffloadData(ah, 0, (u_int32_t *)&sc->sc_wowInfo->ns_info[0]);
        ath_hal_wowOffloadDownloadNsOffloadData(ah, 1, (u_int32_t *)&sc->sc_wowInfo->ns_info[1]);

        if (ath_wowoffload_acer_magic_enable(dev) &&
            (wakeUpEvents & ATH_WAKE_UP_MAGIC_PACKET))
        {
            wakeUpEvents |= ATH_WAKE_UP_ACER_MAGIC_PACKET;
            wakeUpEvents &= ~ATH_WAKE_UP_MAGIC_PACKET;
        }

        if (!ath_get_wowoffload_4way_hs_support(dev)) {
            wakeUpEvents &= ~ATH_WAKE_UP_4WAY_HANDSHAKE;
        }

        if (ath_get_wowoffload_acer_swka_support(dev, 0) &&
            (sc->sc_wowInfo->acer_swka[0].valid || sc->sc_wowInfo->acer_swka[1].valid))
        {
            wakeUpEvents |= ATH_WAKE_UP_ACER_KEEP_ALIVE;
        }
    }
#endif /* ATH_WOW_OFFLOAD */

    /* TBD: should not pass wakeUpEvents if "ath_hal_enable_wow_xxx_events" is implemented */
    /*
            The clearbssid parameter is set only for the case where the wake needs to be
            on a timeout. The timeout mechanism, at this time, is specific to Maverick. For a
            manuf test, the system is put into sleep with a timer. On timer expiry, the chip
            interrupts(timer) and wakes the system. Clearbssid is specified as TRUE since
            we do not want a spurious beacon miss. If specified as true, we clear the bssid regs.
           The timeout value is specified out of band (via ath_set_wow_timeout) and is held in the 
           wow info struct.
         */

    if(ath_hal_wowEnable(ah, wakeUpEvents, sc->sc_wowInfo->timeoutinsecs, clearbssid, wowOffload) == -1) {
        printk("%s[%d]: Error entering wow mode\n", __func__, __LINE__);
        return (-1);
    }
    else
    {
        USHORT word = PCIE_PM_CSR_STATUS | PCIE_PM_CSR_PME_ENABLE | PCIE_PM_CSR_D3;
        OS_PCI_WRITE_CONFIG(sc->sc_osdev, PCIE_PM_CSR, &word, sizeof(USHORT));
    }

    sc->sc_wow_sleep = 1;

    ATH_PS_SLEEP(sc);

    return 0;
}

int
ath_wow_wakeup(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct wow_info  *wowInfo;
    u_int16_t word;
    u_int32_t wowStatus;
#if ATH_WOW_OFFLOAD
    bool wowOffload = ath_get_wowoffload_support(dev);
#else
    bool wowOffload = FALSE;
#endif /* ATH_WOW_OFFLOAD */

    DPRINTF(sc, ATH_DEBUG_ANY, "%s: Wakingup due to wow signal\n", __func__);

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    sc->sc_wow_sleep = 0;

    ATH_PS_WAKEUP(sc);

    // Set SCR to force wake
    ath_pwrsave_awake(sc);

    /* restore system interrupt after wake up */
    ath_hal_intrset(ah, wowInfo->intrMaskBeforeSleep);
    sc->sc_imask = wowInfo->intrMaskBeforeSleep;

#if ATH_WOW_OFFLOAD
    if (wowOffload) {
        /* These parameters would have changed since the host went to
         * sleep. Get them from the embedded CPU on wake up */
        ath_hal_wowOffloadRetrieveParam(ah, &wowInfo->rekeyInfo->ul_data.replay_counter, WOW_PARAM_REPLAY_CNTR);
        ath_hal_wowOffloadRetrieveParam(ah, &wowInfo->rekeyInfo->ul_data.keytsc, WOW_PARAM_KEY_TSC);
        ath_hal_wowOffloadRetrieveParam(ah, &wowInfo->rekeyInfo->ul_data.tx_seqnum, WOW_PARAM_TX_SEQNUM);
    }
#endif /* ATH_WOW_OFFLOAD */
    
    wowStatus = ath_hal_wowWakeUp(ah, wowOffload);
    //wowStatus = ath_hal_wowWakeUp(ah, wowInfo->chipPatternBytes);

#if ATH_WOW_OFFLOAD
    if (wowOffload) {
        ath_hal_wowOffloadPost(ah);
    }
#endif /* ATH_WOW_OFFLOAD */

    /* Make sure PM_CSR is cleared only after tying back the reset */
    word = PCIE_PM_CSR_STATUS;

    OS_PCI_WRITE_CONFIG(sc->sc_osdev, PCIE_PM_CSR, &word, sizeof(USHORT));

    if (sc->sc_wow_bmiss_intr) {
        /*
         * Workaround for the hw that have problems detecting Beacon Miss
         * during WOW sleep.
         */
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: add beacon miss to wowStatus.\n", __func__);
        wowStatus |= AH_WOW_BEACON_MISS;
        sc->sc_wow_bmiss_intr = 0;
    }
    wowInfo->wowWakeupReason =  wowStatus & 0x0FFF;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_wow_wakeups_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_wow_wakeups++;
#endif
    if (wowStatus) {
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_wow_wakeupsok_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_wow_wakeupsok++;
#endif
    }

    ATH_PS_SLEEP(sc);
    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_create_pattern(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int         pattern_num;
    int         pattern_count = 0;
    u_int8_t*   bssid;
    u_int32_t   i, byte_cnt;
    u_int8_t    dis_deauth_pattern[MAX_PATTERN_SIZE];
    u_int8_t    dis_deauth_mask[MAX_PATTERN_SIZE];
    WOW_PATTERN *pattern;

/*
 * FIXME: Is it possible to move this out of common code ?
 * Maverick wants to set std patterns - deauth &/or deassoc
 * selectively.
 */
    struct wow_info  *wowInfo = sc->sc_wowInfo;

    OS_MEMZERO(dis_deauth_pattern, MAX_PATTERN_SIZE);
    OS_MEMZERO(dis_deauth_mask, MAX_PATTERN_SIZE);


    /* Create Dissassociate / Deauthenticate packet filter
     *     2 bytes        2 byte    6 bytes   6 bytes  6 bytes
     *  +--------------+----------+---------+--------+--------+----
     *  + Frame Control+ Duration +   DA    +  SA    +  BSSID +
     *  +--------------+----------+---------+--------+--------+----
     *
     * The above is the management frame format for disassociate/deauthenticate
     * pattern, from this we need to match the first byte of 'Frame Control'
     * and DA, SA, and BSSID fields (skipping 2nd byte of FC and Duration feild. 
     * 
     * Disassociate pattern
     * --------------------
     * Frame control = 00 00 1010
     * DA, SA, BSSID = x:x:x:x:x:x
     * Pattern will be A0000000 | x:x:x:x:x:x | x:x:x:x:x:x | x:x:x:x:x:x  -- 22 bytes
     * Deauthenticate pattern
     * ----------------------
     * Frame control = 00 00 1100
     * DA, SA, BSSID = x:x:x:x:x:x
     * Pattern will be C0000000 | x:x:x:x:x:x | x:x:x:x:x:x | x:x:x:x:x:x  -- 22 bytes
     */

    /* Create Disassociate Pattern first */
    byte_cnt = 0;
    OS_MEMZERO((char*)&dis_deauth_pattern[0], MAX_PATTERN_SIZE);

    /* Fill out the mask with all FF's */
    for (i = 0; i < MAX_PATTERN_MASK_SIZE; i++) {
        dis_deauth_mask[i] = 0xff;
    }

    /* Copy the 1st byte of frame control field */
    dis_deauth_pattern[byte_cnt] = 0xA0;
    byte_cnt++;

    /* Skip 2nd byte of frame control and Duration field */
    byte_cnt += 3;

    /* Need not match the destination mac address, it can be a broadcast
     * mac address or an unicast to this staion 
     */
    byte_cnt += 6;

    /* Copy the Source mac address */
    bssid = sc->sc_curbssid;
    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        dis_deauth_pattern[byte_cnt] = bssid[i];
        byte_cnt++;
    }

    /* Copy the BSSID , it's same as the source mac address */
    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        dis_deauth_pattern[byte_cnt] = bssid[i];
        byte_cnt++;
    }

    /* Create Disassociate pattern mask */
    if (ath_hal_wowMatchPatternExact(ah)) {
        if (ath_hal_wowMatchPatternDword(ah)) {
            /*
             * This is a WAR for Bug 36529. For Merlin, because of hardware bug 31424 
             * that the first 4 bytes have to be matched for all patterns. The mask 
             * for disassociation and de-auth pattern matching need to enable the
             * first 4 bytes. Also the duration field needs to be filled. We assume
             * the duration of de-auth/disassoc frames and association resp are the same.
             */
            dis_deauth_mask[0] = 0xf0;

            /* Fill in duration field */
            dis_deauth_pattern[2] = wowInfo->wowDuration & 0xff;
            dis_deauth_pattern[3] = (wowInfo->wowDuration >> 8) & 0xff;
        }
        else {
            dis_deauth_mask[0] = 0xfe;
        }
        dis_deauth_mask[1] = 0x03;
        dis_deauth_mask[2] = 0xc0;
    } else {
        dis_deauth_mask[0] = 0xef;
        dis_deauth_mask[1] = 0x3f;
        dis_deauth_mask[2] = 0x00;
        dis_deauth_mask[3] = 0xfc;
    }

    ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
    pattern_count++;

    /* For de-authenticate pattern, only the 1st byte of the frame control
     * feild gets changed from 0xA0 to 0xC0
     */
    dis_deauth_pattern[0] = 0xC0;

    /* Now configure the Deauthenticate pattern to the pattern 1 registries */
    ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
    pattern_count++;

    for (pattern_num = 0; pattern_num < MAX_NUM_USER_PATTERN; pattern_num++) {
        pattern = &sc->sc_wowInfo->patterns[pattern_num];
        if (pattern->valid) {
            ath_hal_wowApplyPattern(ah, pattern->patternBytes, pattern->maskBytes, pattern_count, pattern->patternLen);
            pattern_count++;
        }
    }

    /* 
     * For the WOW Merlin WAR(But 36529), the first 4 bytes all need to be matched. We could
     * receive a de-auth frame that has been retransmitted. Add another de-auth pattern with 
     * retry bit set in frame control if there is a space available. 
     */
    if (ath_hal_wowMatchPatternDword(ah) && (pattern_count < MAX_NUM_PATTERN)) {
        dis_deauth_pattern[1] = 0x08;
        ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
        pattern_count++;
    }
    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
void 
ath_set_wow_events(ath_dev_t dev, u_int32_t wowEvents)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    sc->sc_wowInfo->wakeUpEvents = wowEvents;

    /* TBD: Need to call ath_hal_enable_wow_xxx_events instead of passing this flag */
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_get_wow_events(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    return sc->sc_wowInfo->wakeUpEvents;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_add_wakeup_pattern(ath_dev_t dev,u_int32_t patternId, u_int8_t *patternBytes, u_int8_t *maskBytes, u_int32_t patternLen)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t i;
    struct wow_info  *wowInfo;
    WOW_PATTERN *pattern;

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    /* check for duplicate patterns */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!OS_MEMCMP(patternBytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                (!OS_MEMCMP(maskBytes, pattern->maskBytes, MAX_PATTERN_SIZE)))
            {
                printk("Pattern added already \n");
                return 0;
            }
        }
    }

    /* find a empty pattern entry */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (!pattern->valid) {
            break;
        }
    }

    if (i == MAX_NUM_USER_PATTERN) {
        printk("Error : All the %d pattern are in use. Cannot add a new pattern \n", MAX_NUM_PATTERN);
        return (-1);
    }

    DPRINTF(sc, ATH_DEBUG_ANY, "Pattern added to entry %d \n",i);

    // add the pattern
    pattern = &wowInfo->patterns[i];
    OS_MEMCPY(pattern->maskBytes, maskBytes, MAX_PATTERN_SIZE);
    OS_MEMCPY(pattern->patternBytes, patternBytes, MAX_PATTERN_SIZE);
    pattern->patternId  = patternId;
    pattern->patternLen = patternLen;
    pattern->valid = 1;

    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_remove_wakeup_pattern(ath_dev_t dev, u_int8_t *patternBytes, u_int8_t *maskBytes)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t i;
    struct wow_info  *wowInfo;
    WOW_PATTERN *pattern;

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    DPRINTF(sc, ATH_DEBUG_ANY,"%s: Remove wake up pattern \n", __func__);
    printk("mask = %x pat = %x \n",(u_int32_t)maskBytes, (u_int32_t)patternBytes);

    /* remove the pattern and return if present */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!OS_MEMCMP(patternBytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                !OS_MEMCMP(maskBytes, pattern->maskBytes, MAX_PATTERN_SIZE))
            {
                pattern->valid = 0;
                OS_MEMZERO(pattern->patternBytes, MAX_PATTERN_SIZE);
                OS_MEMZERO(pattern->maskBytes, MAX_PATTERN_SIZE);
                DPRINTF(sc, ATH_DEBUG_ANY, "Pattern Removed from entry %d \n",i); 
                return 0;
            }
        }
    }

    // pattern not found
    DPRINTF(sc, ATH_DEBUG_ANY, "%s : Error : Pattern not found \n", __func__); 

    return (-1);
}

int ath_wow_get_wakeup_pattern(ath_dev_t dev, u_int8_t *wakePatternBytes,u_int32_t *wakePatternSize, u_int32_t *patternId)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct wow_info  *wowInfo;

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    DPRINTF(sc, ATH_DEBUG_ANY,"%s: get saved wake up pattern \n", __func__);

    /* Here do the match operation and copy the matched pattern*/
    {
        //*patternId = 0;
        //OS_MEMCPY();
        //return 0;
    }

    // pattern not found
    DPRINTF(sc, ATH_DEBUG_ANY, "%s : Error : Pattern not found \n", __func__); 

    return (-1);
}

int
ath_get_wow_wakeup_reason(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    return sc->sc_wowInfo->wowWakeupReason;
}    

int
ath_wow_matchpattern_exact(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    return ((ath_hal_wowMatchPatternExact(ah)) ? TRUE : FALSE);
}   

/*
 * This is a WAR for Bug 36529. For Merlin, because of hardware bug 31424 
 * that the first 4 bytes have to be matched for all patterns. The duration 
 * field needs to be filled. We assume the duration of de-auth/disassoc 
 * frames and association resp are the same.
 *
 * This function is called by net80211 layer to set the duration field for 
 * pattern matching.
 */
void 
ath_wow_set_duration(ath_dev_t dev, u_int16_t duration)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_hasWow && (sc->sc_wowInfo != NULL)) {
        sc->sc_wowInfo->wowDuration = duration;
    }
}


void
ath_set_wow_timeout(ath_dev_t dev, u_int32_t timeoutinsecs)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc && sc->sc_wowInfo) {
        if (sc->sc_hasWow) {
            sc->sc_wowInfo->timeoutinsecs = timeoutinsecs;
        }
        else {
            sc->sc_wowInfo->timeoutinsecs = 0;
        }             
    }
}

u_int32_t
ath_get_wow_timeout(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if (sc && sc->sc_wowInfo) {
        if (sc->sc_hasWow) {
            return sc->sc_wowInfo->timeoutinsecs;
        }
    }
    return 0;
}

#if ATH_WOW_OFFLOAD
void ath_set_wow_enabled_events(ath_dev_t dev,
                                u_int32_t EnabledWoLPacketPatterns,
                                u_int32_t EnabledProtocolOffloads,
                                u_int32_t MediaSpecificWakeUpEvents)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    sc->sc_wowInfo->EnabledWoLPacketPatterns = EnabledWoLPacketPatterns;
    sc->sc_wowInfo->EnabledProtocolOffloads = EnabledProtocolOffloads;
    sc->sc_wowInfo->MediaSpecificWakeUpEvents = MediaSpecificWakeUpEvents;
}

void ath_get_wow_enabled_events(ath_dev_t dev,
                                u_int32_t *pEnabledWoLPacketPatterns,
                                u_int32_t *pEnabledProtocolOffloads,
                                u_int32_t *pMediaSpecificWakeUpEvents)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (pEnabledWoLPacketPatterns) {
        *pEnabledWoLPacketPatterns = sc->sc_wowInfo->EnabledWoLPacketPatterns;
    }
    if (pEnabledProtocolOffloads) {
        *pEnabledProtocolOffloads = sc->sc_wowInfo->EnabledProtocolOffloads;
    }
    if (pMediaSpecificWakeUpEvents) {
        *pMediaSpecificWakeUpEvents = sc->sc_wowInfo->MediaSpecificWakeUpEvents;
    }
}

void ath_wowoffload_remove_gtk_rekey_info(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct wow_offload_rekey_info *rekey_info = sc->sc_wowInfo->rekeyInfo;
    struct wow_offload_rekey_download_data *dl_data = &rekey_info->dl_data;

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) clear GTK key info\n");
    dl_data->kck_0 = 0;
    dl_data->kck_1 = 0;
    dl_data->kck_2 = 0;
    dl_data->kck_3 = 0;

    dl_data->kek_0 = 0;
    dl_data->kek_1 = 0;
    dl_data->kek_2 = 0;
    dl_data->kek_3 = 0;

    dl_data->replay_counter_lower = 0;
    dl_data->replay_counter_upper = 0;

    rekey_info->id = 0;
    rekey_info->valid = FALSE;
}


u_int32_t
ath_wowoffload_remove_offload_info(ath_dev_t dev, u_int32_t protocol_offload_id)
{
    int i;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct wow_offload_rekey_info *rekey_info = sc->sc_wowInfo->rekeyInfo;
    struct wow_offload_rekey_download_data *dl_data = &rekey_info->dl_data;

    for (i = 0; i < WOW_OFFLOAD_ARP_INFO_MAX; i++) {
        if (sc->sc_wowInfo->arp_info[i].valid &&
            (sc->sc_wowInfo->arp_info[i].id == protocol_offload_id))
        {
            memset(&sc->sc_wowInfo->arp_info[i], 0, sizeof(struct wow_offload_arp_info));
            sc->sc_wowInfo->arp_info[i].valid = FALSE;
            DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Remove ARP offload id:%08X\n", protocol_offload_id);
            return 1;
        }
    }

    for (i = 0; i < WOW_OFFLOAD_NS_INFO_MAX; i++) {
        if (sc->sc_wowInfo->ns_info[i].valid &&
            (sc->sc_wowInfo->ns_info[i].id == protocol_offload_id))
        {
            memset(&sc->sc_wowInfo->ns_info[i], 0, sizeof(struct wow_offload_ns_info));
            sc->sc_wowInfo->ns_info[i].valid = FALSE;
            DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Remove NS offload id:%08X\n", protocol_offload_id);
            return 1;
        }
    }

    if (rekey_info->id == protocol_offload_id) {
        ath_wowoffload_remove_gtk_rekey_info(dev);
        DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Remove GTK offload id:%08X\n", protocol_offload_id);
        return 1;
    }

    return 0;
}

u_int32_t 
ath_wowoffload_get_rekey_info(ath_dev_t dev, void *buf, u_int32_t param)
{
    /* Get updated key TSC, TX sequence number and Replay counter
     * values from the embedded CPU on wake up. These values would 
     * have changed since the host went to sleep */
    struct wow_offload_rekey_info *rekey_info = ATH_DEV_TO_SC(dev)->sc_wowInfo->rekeyInfo;
    switch (param) {
        case WOW_OFFLOAD_KEY_TSC:
            *(u_int64_t *)buf = rekey_info->ul_data.keytsc;
            break;
        case WOW_OFFLOAD_TX_SEQNUM:
            *(u_int32_t *)buf = rekey_info->ul_data.tx_seqnum;
            break;
        case WOW_OFFLOAD_REPLAY_CNTR:
            *(u_int64_t *)buf = rekey_info->ul_data.replay_counter;
            break;
    }

    return 0;
}

u_int32_t
ath_wowoffload_update_txseqnum(ath_dev_t dev, struct ath_node *an, u_int32_t tidno, u_int16_t seqnum)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_atx_tid *tid;

    tid = ATH_AN_2_TID(an, tidno);
    tid->seq_next = seqnum;

    return 0;
}

u_int32_t
ath_wowoffload_set_rekey_misc_info(ath_dev_t dev, struct wow_offload_misc_info *wow)
{
    /* This miscellaneous information will be provided by the WLAN
     * driver */
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct wow_offload_rekey_info *rekey_info = sc->sc_wowInfo->rekeyInfo;
    struct wow_offload_rekey_download_data *rekey_data = &rekey_info->dl_data;

    rekey_data->flags         = wow->flags;
    rekey_data->myaddr_lower  = *((u_int32_t *)&wow->myaddr[0]);
    rekey_data->myaddr_upper = *((u_int32_t *)&wow->myaddr[4]);
    rekey_data->bssid_lower   = *((u_int32_t *)&wow->bssid[0]);
    rekey_data->bssid_upper  = *((u_int32_t *)&wow->bssid[4]);
    rekey_data->tx_seqnum     = wow->tx_seqnum;
    rekey_data->ucast_keyix   = wow->ucast_keyix;
    rekey_data->cipher        = wow->cipher;
    rekey_data->keytsc_lower  = wow->keytsc & 0xFFFFFFFF;
    rekey_data->keytsc_upper  = (wow->keytsc >> 32) & 0xFFFFFFFF;

    return 0;
}

u_int32_t 
ath_wowoffload_set_rekey_suppl_info(ath_dev_t dev, u_int32_t id, u_int8_t *kck, u_int8_t *kek,
                        u_int64_t *replay_counter)
{
    /* This information is provided by the supplicant */

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct wow_offload_rekey_info *rekey_info = sc->sc_wowInfo->rekeyInfo;
    struct wow_offload_rekey_download_data *dl_data = &rekey_info->dl_data;

    ath_wowoffload_remove_offload_info(dev, id);
    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Add GTK offload id:%08X\n", id);

    /*
     * The hardware registers are native little-endian byte order.
     * Big-endian hosts are handled by enabling hardware byte-swap
     * of register reads and writes at reset.
     */
    dl_data->kck_0 = *(u_int32_t *)&kck[0];
    dl_data->kck_1 = *(u_int32_t *)&kck[4];
    dl_data->kck_2 = *(u_int32_t *)&kck[8];
    dl_data->kck_3 = *(u_int32_t *)&kck[12];

    dl_data->kek_0 = *(u_int32_t *)&kek[0];
    dl_data->kek_1 = *(u_int32_t *)&kek[4];
    dl_data->kek_2 = *(u_int32_t *)&kek[8];
    dl_data->kek_3 = *(u_int32_t *)&kek[12];

    dl_data->replay_counter_lower = (*replay_counter) & 0xFFFFFFFF;
    dl_data->replay_counter_upper = ((*replay_counter) >> 32) & 0xFFFFFFFF;

    rekey_info->id = id;
    rekey_info->valid = TRUE;

    return 1;
}

int ath_acer_keepalive(ath_dev_t dev, u_int32_t id, u_int32_t msec, u_int32_t size, u_int8_t* packet)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) keepalive: id=%d, masec=%d, size=%d\n", id, msec, size);
    if (size >= 6) {
        DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) keepalive: data=%02x:%02x:%02x:%02x:%02x:%02x\n",
            packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
    }
    if (ath_get_wowoffload_acer_swka_support(dev, id) &&
        (size <= WOW_OFFLOAD_ACER_SWKA_DATA_SIZE)) {
        sc->sc_wowInfo->acer_swka[id].id = id;
        sc->sc_wowInfo->acer_swka[id].period = msec;
        sc->sc_wowInfo->acer_swka[id].size = size;
        if (msec && size) {
            sc->sc_wowInfo->acer_swka[id].valid = TRUE;
        } else {
            sc->sc_wowInfo->acer_swka[id].valid = FALSE;
        }
        memcpy((u_int8_t *) &sc->sc_wowInfo->acer_swka[id].data[0], packet, size);
        return 1;
    }
    return 0;
}

int ath_acer_keepalive_query(ath_dev_t dev, u_int32_t id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) keepqalive query: id=%d %s\n",
        id, sc->sc_wowInfo->acer_swka[id].valid?"enabled":"disabled");
    if (ath_get_wowoffload_acer_swka_support(dev, id)) {
        return sc->sc_wowInfo->acer_swka[id].valid?1:0;
    }
    return 0;
}

int ath_acer_wakeup_match(ath_dev_t dev, u_int8_t* pattern)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) magic password enable: %02x:%02x:%02x:%02x:%02x:%02x\n",
        pattern[0], pattern[1], pattern[2], pattern[3], pattern[4], pattern[5]);
    if (ath_get_wowoffload_acer_magic_support(dev)) {
        sc->sc_wowInfo->acer_magic.valid = TRUE;
        memcpy(sc->sc_wowInfo->acer_magic.data, pattern, WOW_OFFLOAD_ACER_MAGIC_DATA_SIZE);
        return 1;
    }

    return 0;
}

int ath_acer_wakeup_disable(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) magic password disable\n");
    if (ath_get_wowoffload_acer_magic_support(dev)) {
        sc->sc_wowInfo->acer_magic.valid = FALSE;
        return 1;
    }
    return 0;
}

int ath_acer_wakeup_query(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) magic password query: %s\n",
        ath_wowoffload_acer_magic_enable(dev)?"enabled":"disabled");
    return ath_wowoffload_acer_magic_enable(dev)?1:0;
}

struct ndis_IPV4_ARP_PARAMETERS
{
    ULONG   Flags;                  
    UCHAR   RemoteIPv4Address[4];   // source IPv4 address (optional)
    UCHAR   HostIPv4Address[4];     // destination IPv4 address
    UCHAR   MacAddress[6];          // MAC address                        
};

u_int32_t ath_wowoffload_set_arp_info(ath_dev_t dev,
                                    u_int32_t   protocolOffloadId,
                                    void       *pParams)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ndis_IPV4_ARP_PARAMETERS *pArp = (struct ndis_IPV4_ARP_PARAMETERS *) pParams;
    int iArp;

    ath_wowoffload_remove_offload_info(dev, protocolOffloadId);

    for (iArp = 0; iArp < WOW_OFFLOAD_ARP_INFO_MAX; iArp++) {
        if (sc->sc_wowInfo->arp_info[iArp].valid == FALSE) {
            break;
        }
    }
    if (iArp == WOW_OFFLOAD_ARP_INFO_MAX) {
        return 0;
    }

    if (ath_get_wowoffload_arp_support(dev)) {
        DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Add ARP offload id:%08X\n", protocolOffloadId);
        sc->sc_wowInfo->arp_info[iArp].valid = TRUE;
        sc->sc_wowInfo->arp_info[iArp].id = protocolOffloadId;
        
        memcpy( sc->sc_wowInfo->arp_info[iArp].RemoteIPv4Address.u8,
                pArp->RemoteIPv4Address,
                4);
        memcpy( sc->sc_wowInfo->arp_info[iArp].HostIPv4Address.u8,
                pArp->HostIPv4Address,
                4);
        sc->sc_wowInfo->arp_info[iArp].MacAddress.u32[1] = 0;
        memcpy( sc->sc_wowInfo->arp_info[iArp].MacAddress.u8,
                pArp->MacAddress,
                6);
        return 1;
    }
    
    return 0;
}

struct ndis_IPV6_NS_PARAMETERS
{
    ULONG   Flags;                              
    UCHAR   RemoteIPv6Address[16];              // source IPv6 address (optional)
    UCHAR   SolicitedNodeIPv6Address[16];       // solicited node IPv6 address
    UCHAR   MacAddress[6];                      // MAC address     
    UCHAR   TargetIPv6Addresses[2][16];         // An array of local IPv6 addesses           
};

u_int32_t ath_wowoffload_set_ns_info(ath_dev_t  dev,
                                    u_int32_t   protocolOffloadId,
                                    void       *pParams)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ndis_IPV6_NS_PARAMETERS *pNS = (struct ndis_IPV6_NS_PARAMETERS *) pParams;
    int iNS;

    ath_wowoffload_remove_offload_info(dev, protocolOffloadId);

    for (iNS = 0; iNS < WOW_OFFLOAD_NS_INFO_MAX; iNS++) {
        if (sc->sc_wowInfo->ns_info[iNS].valid == FALSE) {
            break;
        }
    }
    if (iNS == WOW_OFFLOAD_NS_INFO_MAX) {
        return 0;
    }

    if (ath_get_wowoffload_ns_support(dev)) {
        DPRINTF(sc, ATH_DEBUG_ANY, "(WOW) Add NS offload id:%08X\n", protocolOffloadId);
        sc->sc_wowInfo->ns_info[iNS].valid = TRUE;
        sc->sc_wowInfo->ns_info[iNS].id = protocolOffloadId;
        
        memcpy( sc->sc_wowInfo->ns_info[iNS].RemoteIPv6Address.u8,
                pNS->RemoteIPv6Address,
                16);
        memcpy( sc->sc_wowInfo->ns_info[iNS].SolicitedNodeIPv6Address.u8,
                pNS->SolicitedNodeIPv6Address,
                16);
        sc->sc_wowInfo->ns_info[iNS].MacAddress.u32[1] = 0;
        memcpy( sc->sc_wowInfo->ns_info[iNS].MacAddress.u8,
                pNS->MacAddress,
                6);
        memcpy( sc->sc_wowInfo->ns_info[iNS].TargetIPv6Addresses[0].u8,
                pNS->TargetIPv6Addresses[0],
                16);
        memcpy( sc->sc_wowInfo->ns_info[iNS].TargetIPv6Addresses[1].u8,
                pNS->TargetIPv6Addresses[1],
                16);
        return 1;
    }
    
    return 0;
}

#endif /* ATH_WOW_OFFLOAD */

#endif
