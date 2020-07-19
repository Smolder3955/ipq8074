/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212desc.h"

/*
 * Get the RXDP.
 */
u_int32_t
ar5212GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype)
{
    HALASSERT(qtype == 0);
    return OS_REG_READ(ath, AR_RXDP);
}

/*
 * Set the RxDP.
 */
void
ar5212SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype)
{
    OS_REG_WRITE(ah, AR_RXDP, rxdp);
    HALASSERT(OS_REG_READ(ah, AR_RXDP) == rxdp);
    HALASSERT(qtype == 0);
}

/*
 * Set Receive Enable bits.
 */
void
ar5212EnableReceive(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXE);
}

/*
 * Set the RX abort bit.
 */
HAL_BOOL 
ar5212SetRxAbort(struct ath_hal *ah, HAL_BOOL set)
{
    // No-op. Used only on 5416 MAC.
    return AH_TRUE;
}

/*
 * Stop Receive at the DMA engine
 */
HAL_BOOL
ar5212StopDmaReceive(struct ath_hal *ah, u_int timeout)
{
#define AH_RX_STOP_DMA_TIMEOUT 10000   /* usec */  

    if (timeout == 0) {
        timeout = AH_RX_STOP_DMA_TIMEOUT;
    }

    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);    /* Set receive disable bit */
    if (!ath_hal_wait(ah, AR_CR, AR_CR_RXE, 0, timeout)) {
#ifdef AH_DEBUG
        HDPRINTF(ah, HAL_DBG_DMA, "%s: dma failed to stop in 10ms\n"
            "AR_CR=0x%08x\nAR_DIAG_SW=0x%08x\n",
            __func__,
            OS_REG_READ(ah, AR_CR),
            OS_REG_READ(ah, AR_DIAG_SW));
#endif
        return AH_FALSE;
    } else {
        return AH_TRUE;
    }

#undef AH_RX_STOP_DMA_TIMEOUT
}

/*
 * Start Transmit at the PCU engine (unpause receive)
 */
void
ar5212StartPcuReceive(struct ath_hal *ah, HAL_BOOL is_scanning)
{
    OS_REG_WRITE(ah, AR_DIAG_SW,
        OS_REG_READ(ah, AR_DIAG_SW) &~ AR_DIAG_RX_DIS);
    ar5212EnableMIBCounters(ah);
    ar5212AniReset(ah, 1);
}

/*
 * Stop Transmit at the PCU engine (pause receive)
 */
void
ar5212StopPcuReceive(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_DIAG_SW,
        OS_REG_READ(ah, AR_DIAG_SW) | AR_DIAG_RX_DIS);
    ar5212DisableMIBCounters(ah);
}

/*
 * Set multicast filter 0 (lower 32-bits)
 *               filter 1 (upper 32-bits)
 */
void
ar5212SetMulticastFilter(struct ath_hal *ah, u_int32_t filter0, u_int32_t filter1)
{
    OS_REG_WRITE(ah, AR_MCAST_FIL0, filter0);
    OS_REG_WRITE(ah, AR_MCAST_FIL1, filter1);
}

/*
 * Get the receive filter.
 */
u_int32_t
ar5212GetRxFilter(struct ath_hal *ah)
{
    u_int32_t bits = OS_REG_READ(ah, AR_RX_FILTER);
    u_int32_t phybits = OS_REG_READ(ah, AR_PHY_ERR);
    if (phybits & AR_PHY_ERR_RADAR)
        bits |= HAL_RX_FILTER_PHYRADAR;
    if (phybits & (AR_PHY_ERR_OFDM_TIMING|AR_PHY_ERR_CCK_TIMING))
        bits |= HAL_RX_FILTER_PHYERR;
    return bits;
}

/*
 * Set the receive filter.
 */
void
ar5212SetRxFilter(struct ath_hal *ah, u_int32_t bits)
{
    u_int32_t phybits;

    if (bits & HAL_RX_FILTER_MYBEACON) {
        OS_REG_WRITE(ah, AR_RX_FILTER, ((bits & 0xff) | AR_RX_BEACON));
    } else {
        OS_REG_WRITE(ah, AR_RX_FILTER, bits & 0xff);
    }
    phybits = 0;
    if (bits & HAL_RX_FILTER_PHYRADAR)
        phybits |= AR_PHY_ERR_RADAR;
    if (bits & HAL_RX_FILTER_PHYERR)
        phybits |= AR_PHY_ERR_OFDM_TIMING | AR_PHY_ERR_CCK_TIMING;
    OS_REG_WRITE(ah, AR_PHY_ERR, phybits);
    if (phybits) {
        OS_REG_WRITE(ah, AR_RXCFG,
            OS_REG_READ(ah, AR_RXCFG) | AR_RXCFG_ZLFDMA);
    } else {
        OS_REG_WRITE(ah, AR_RXCFG,
            OS_REG_READ(ah, AR_RXCFG) &~ AR_RXCFG_ZLFDMA);
    }
}

/*
 * Initialize RX descriptor, by clearing the status and setting
 * the size (and any other flags).
 */
HAL_BOOL
ar5212SetupRxDesc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t size, u_int flags)
{
    struct ar5212_desc *ads = AR5212DESC(ds);

    HALASSERT((size &~ AR_BufLen) == 0);

    ads->ds_ctl0 = 0;
    ads->ds_ctl1 = size & AR_BufLen;

    if (flags & HAL_RXDESC_INTREQ)
        ads->ds_ctl1 |= AR_RxInterReq;
    ads->ds_rxstatus0 = ads->ds_rxstatus1 = 0;

    return AH_TRUE;
}

/*
 * Process an RX descriptor.  Depending on the parameters, this
 * may operate directly on the descriptor (ar5212ProcRxDesc), or it
 * may operate on a stack copy of the descriptor (ar5212ProcRxDescFast).
 */
static HAL_STATUS
ar5212ProcRxDescBase(struct ath_hal *ah, struct ath_desc *ds,
    struct ath_rx_status *rx_stats, const struct ar5212_desc *stats_ds)
{
    rx_stats->rs_datalen = stats_ds->ds_rxstatus0 & AR_DataLen;
    rx_stats->rs_tstamp = MS(stats_ds->ds_rxstatus1, AR_RcvTimestamp);
    rx_stats->rs_status = 0;
    /* XXX what about KeyCacheMiss? */
    rx_stats->rs_rssi = MS(stats_ds->ds_rxstatus0, AR_RcvSigStrength);
    if (stats_ds->ds_rxstatus1 & AR_KeyIdxValid)
        rx_stats->rs_keyix = MS(stats_ds->ds_rxstatus1, AR_KeyIdx);
    else
        rx_stats->rs_keyix = HAL_RXKEYIX_INVALID;
    /* NB: caller expected to do rate table mapping */
    rx_stats->rs_rate = MS(stats_ds->ds_rxstatus0, AR_RcvRate);
    rx_stats->rs_antenna  = MS(stats_ds->ds_rxstatus0, AR_RcvAntenna);
    rx_stats->rs_more = (stats_ds->ds_rxstatus0 & AR_More) ? 1 : 0;

    if (stats_ds->ds_rxstatus1 & AR_KeyCacheMiss) {
        rx_stats->rs_status |= HAL_RXERR_KEYMISS;
    }

    if ((stats_ds->ds_rxstatus1 & AR_FrmRcvOK) == 0) {
        /*
         * These four bits should not be set together.  The
         * 5212 spec states a Michael error can only occur if
         * DecryptCRCErr not set (and TKIP is used).  Experience
         * indicates however that you can also get Michael errors
         * when a CRC error is detected, but these are specious.
         * Consequently we filter them out here so we don't
         * confuse and/or complicate drivers.
         */
        if (stats_ds->ds_rxstatus1 & AR_CRCErr)
            rx_stats->rs_status |= HAL_RXERR_CRC;
        else if (stats_ds->ds_rxstatus1 & AR_PHYErr) {
            u_int phyerr;

            rx_stats->rs_status |= HAL_RXERR_PHY;
            phyerr = MS(stats_ds->ds_rxstatus1, AR_PHYErrCode);
            rx_stats->rs_phyerr = phyerr;
            if ((!AH5212(ah)->ah_hasHwPhyCounters) &&
                (phyerr != HAL_PHYERR_RADAR))
                ar5212AniPhyErrReport(ah, &ds->ds_rxstat);
        } else if (stats_ds->ds_rxstatus1 & AR_DecryptCRCErr)
            rx_stats->rs_status |= HAL_RXERR_DECRYPT;
        else if (stats_ds->ds_rxstatus1 & AR_MichaelErr)
            rx_stats->rs_status |= HAL_RXERR_MIC;
    }
    return HAL_OK;
}

/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into the software portion
 * of the descriptor.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar5212ProcRxDescFast(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, struct ath_rx_status *rx_stats, void *buf_addr)
{
    struct ar5212_desc ads;
    const struct ar5212_desc *adsp = AR5212DESC(ds);
#ifdef RXDESC_SELF_LINKED
    struct ar5212_desc *ands = AR5212DESC(nds);
#endif

    if ((adsp->ds_rxstatus1 & AR_Done) == 0)
        return HAL_EINPROGRESS;

#ifdef RXDESC_SELF_LINKED
    /*
     * Given the use of a self-linked tail be very sure that the hw is
     * done with this descriptor; the hw may have done this descriptor
     * once and picked it up again...make sure the hw has moved on.
     */
    if ((ands->ds_rxstatus1&AR_Done) == 0 && OS_REG_READ(ah, AR_RXDP) == pa)
        return HAL_EINPROGRESS;
#endif

    /*
     * Now we need to get the stats from the descriptor. Since desc are
     * uncached, lets make a copy of the stats first into a cached memory
     * location on our stack.
     * Note that, since we touch most of the rx stats, a memcpy would
     * always be more efficient
     * This reduces the number of uncached accesses.
     * Do this copy here, after the check so that when the checks fail, we
     * dont end up copying the entire stats uselessly.
     * This causes some redundant checks in ar5416ProcRxDescBase, but since
     * the checks are fast, the cost is small.
     */
    ads.u.rx = adsp->u.rx;
    return ar5212ProcRxDescBase(ah, ds, rx_stats, &ads);
}

/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into the software portion
 * of the descriptor.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar5212ProcRxDesc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, u_int64_t tsf, struct ath_rx_status *rxs)
{
    const struct ar5212_desc *ads = AR5212DESC(ds);
#ifdef RXDESC_SELF_LINKED
    struct ar5212_desc *ands = AR5212DESC(nds);
#endif

    if ((ads->ds_rxstatus1 & AR_Done) == 0)
        return HAL_EINPROGRESS;

#ifdef RXDESC_SELF_LINKED
    /*
     * Given the use of a self-linked tail be very sure that the hw is
     * done with this descriptor; the hw may have done this descriptor
     * once and picked it up again...make sure the hw has moved on.
     */
    if ((ands->ds_rxstatus1&AR_Done) == 0 && OS_REG_READ(ah, AR_RXDP) == pa)
        return HAL_EINPROGRESS;
#endif

    return ar5212ProcRxDescBase(ah, ds, rxs, AR5212DESC(ds));
}

HAL_STATUS
ar5212GetRxKeyIdx(struct ath_hal *ah, struct ath_desc *ds, u_int8_t *keyix, u_int8_t *status)
{
    u_int32_t status1;    
    const struct ar5212_desc *ads = AR5212DESC(ds);

    status1 = ads->ds_rxstatus1;
    if ((status1 & AR_Done) == 0)
        return HAL_EINPROGRESS;

    *status = 0;
    if (status1 & AR_KeyIdxValid)
        *keyix = MS(status1, AR_KeyIdx);
    else
        *keyix = HAL_RXKEYIX_INVALID;
    return HAL_OK;
}

void ar5212PromiscMode(struct ath_hal *ah, HAL_BOOL enable)
{
	u_int32_t regVal = 0;
	regVal =  OS_REG_READ(ah, AR_RX_FILTER);
	if(enable){
		regVal |= AR_RX_PROM;
	}
	else{ /*Disable promisc mode */
		regVal &= ~AR_RX_PROM;
	}	
	OS_REG_WRITE(ah, AR_RX_FILTER, regVal);
}

void 
ar5212ReadPktlogReg(struct ath_hal *ah, u_int32_t *rxfilterVal, u_int32_t *rxcfgVal, u_int32_t *phyErrMaskVal, u_int32_t *macPcuPhyErrRegval)
{

    *rxfilterVal = OS_REG_READ(ah, AR_RX_FILTER);
    *rxcfgVal    = OS_REG_READ(ah, AR_RXCFG);
    *phyErrMaskVal = OS_REG_READ(ah, AR_PHY_ERR);
    *macPcuPhyErrRegval = OS_REG_READ(ah, 0x8338);
	
}

void
ar5212WritePktlogReg(struct ath_hal *ah, HAL_BOOL enable, u_int32_t rxfilterVal, u_int32_t rxcfgVal, u_int32_t phyErrMaskVal, u_int32_t macPcuPhyErrRegVal)
{
    if(enable){ /* Enable pktlog phyerr setting */
		
		OS_REG_WRITE(ah, AR_RX_FILTER, 0xffff | rxfilterVal);
		OS_REG_WRITE(ah, AR_PHY_ERR, 0xFFFFFFFF);
		OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal| AR_RXCFG_ZLFDMA);
		OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal| 0xFF);	
		
	}else{ /* Disable phyerr and Restore regs */

		OS_REG_WRITE(ah, AR_RX_FILTER, rxfilterVal);
		OS_REG_WRITE(ah, AR_PHY_ERR, phyErrMaskVal);
		OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal);
		OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal);
	}
}

#endif /* AH_SUPPORT_AR5212 */
