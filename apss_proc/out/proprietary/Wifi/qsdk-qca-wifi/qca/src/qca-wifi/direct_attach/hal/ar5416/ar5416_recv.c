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

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416desc.h"

/*
 * Get the RXDP.
 */
u_int32_t
ar5416GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype)
{
    HALASSERT(qtype == 0);
    return OS_REG_READ(ath, AR_RXDP);
}

/*
 * Set the RxDP.
 */
void
ar5416SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype)
{
    OS_REG_WRITE(ah, AR_RXDP, rxdp);
    HALASSERT(OS_REG_READ(ah, AR_RXDP) == rxdp);
    HALASSERT(qtype == 0);
}

/*
 * Set Receive Enable bits.
 */
void
ar5416EnableReceive(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXE);
}

/*
 * Set the RX abort bit.
 */
HAL_BOOL
ar5416SetRxAbort(struct ath_hal *ah, HAL_BOOL set)
{
    if (set) {
        /* Set the ForceRXAbort bit */
        OS_REG_SET_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));

        /* Wait for Rx state to return to 0 */
        if (!ath_hal_wait(ah, AR_OBS_BUS_1, AR_OBS_BUS_1_RX_STATE, 0, AH_WAIT_TIMEOUT)) {
            /* abort: chip rx failed to go idle in 10 ms */
            OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));

            HDPRINTF(ah, HAL_DBG_RX, "%s: rx failed to go idle in 10 ms\n",
                    __func__);

            return AH_FALSE;    // Indicate failure
        }
    }
    else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
    }

    return AH_TRUE;    // Function completed successfully
}

/*
 * Stop Receive at the DMA engine
 */
HAL_BOOL
ar5416StopDmaReceive(struct ath_hal *ah, u_int timeout)
{
    int wait;
    HAL_BOOL status;  

#define AH_RX_STOP_DMA_TIMEOUT 10000   /* usec */  
#define AH_TIME_QUANTUM        100     /* usec */

    if (timeout == 0) {
        timeout = AH_RX_STOP_DMA_TIMEOUT;
    }

    /* Set receive disable bit */
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD); 

    /* Wait for rx enable bit to go low */
    for (wait = timeout / AH_TIME_QUANTUM; wait != 0; wait--) {

        if ((OS_REG_READ(ah, AR_CR) & AR_CR_RXE) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);        
    }

    if (wait == 0) {
        AH5416(ah)->ah_dma_stuck = AH_TRUE;
        HDPRINTF(ah, HAL_DBG_RX, "%s: dma failed to stop in %d ms\n"
                "AR_CR=0x%08x\nAR_DIAG_SW=0x%08x\n",
                __func__,
                timeout / 1000,
                OS_REG_READ(ah, AR_CR),
                OS_REG_READ(ah, AR_DIAG_SW));
        status=AH_FALSE;
    } else {
        status=AH_TRUE;
    }

#ifdef AR9100
    OS_DELAY(3000);
#endif
    return status;
#undef AH_RX_STOP_DMA_TIMEOUT
#undef AH_TIME_QUANTUM 
}

/*
 * Start Transmit at the PCU engine (unpause receive)
 */
void
ar5416StartPcuReceive(struct ath_hal *ah, HAL_BOOL is_scanning)
{

    ar5416EnableMIBCounters(ah);

    ar5416AniReset(ah);

    /* Clear RX_DIS and RX_ABORT after enabling phy errors in aniReset */
    OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
}

/*
 * Stop Transmit at the PCU engine (pause receive)
 */
void
ar5416StopPcuReceive(struct ath_hal *ah)
{
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_DIS);

    ar5416DisableMIBCounters(ah);

#if 0
    ar5416RadarDisable(ah);
#endif
}

/*
 * Set multicast filter 0 (lower 32-bits)
 *               filter 1 (upper 32-bits)
 */
void
ar5416SetMulticastFilter(struct ath_hal *ah, u_int32_t filter0, u_int32_t filter1)
{
    OS_REG_WRITE(ah, AR_MCAST_FIL0, filter0);
    OS_REG_WRITE(ah, AR_MCAST_FIL1, filter1);
}

/*
 * Get the receive filter.
 */
u_int32_t
ar5416GetRxFilter(struct ath_hal *ah)
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
ar5416SetRxFilter(struct ath_hal *ah, u_int32_t bits)
{
    u_int32_t phybits;

	ENABLE_REG_WRITE_BUFFER
    if ( (AR_SREV_MERLIN_10_OR_LATER(ah) || AR_SREV_KITE_10_OR_LATER(ah)) &&
         AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP ) {
        OS_REG_WRITE(ah, AR_RX_FILTER,
            (bits & 0xffff) | AR_RX_UNCOM_BA_BAR | AR_RX_COMPR_BAR | HAL_RX_FILTER_PSPOLL);
    } else {
        OS_REG_WRITE(ah, AR_RX_FILTER, (bits & 0xffff) | AR_RX_UNCOM_BA_BAR | AR_RX_COMPR_BAR);
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
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

/*
 * Select to pass PLCP headr or EVM data.
 */
HAL_BOOL
ar5416SetRxSelEvm(struct ath_hal *ah, HAL_BOOL selEvm, HAL_BOOL justQuery)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
	HAL_BOOL old_value = ahp->ah_getPlcpHdr == 0;
	if (justQuery) return old_value;
	if (selEvm) {
		OS_REG_SET_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
	} else {
		OS_REG_CLR_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
	}
	ahp->ah_getPlcpHdr = !selEvm;
	return old_value;
}
void ar5416PromiscMode(struct ath_hal *ah, HAL_BOOL enable)
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
ar5416ReadPktlogReg(struct ath_hal *ah, u_int32_t *rxfilterVal, u_int32_t *rxcfgVal, u_int32_t *phyErrMaskVal, u_int32_t *macPcuPhyErrRegval)
{

    *rxfilterVal = OS_REG_READ(ah, AR_RX_FILTER);
    *rxcfgVal    = OS_REG_READ(ah, AR_RXCFG);
    *phyErrMaskVal = OS_REG_READ(ah, AR_PHY_ERR);
    *macPcuPhyErrRegval = OS_REG_READ(ah, 0x8338);
	
	HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d] rxfilterVal 0x%08x , rxcfgVal 0x%08x, phyErrMaskVal 0x%08x macPcuPhyErrRegval 0x%08x\n", __func__, __LINE__, *rxfilterVal, *rxcfgVal, *phyErrMaskVal, *macPcuPhyErrRegval);
}

void
ar5416WritePktlogReg(struct ath_hal *ah, HAL_BOOL enable, u_int32_t rxfilterVal, u_int32_t rxcfgVal, u_int32_t phyErrMaskVal, u_int32_t macPcuPhyErrRegVal)
{
    if(enable){ /* Enable pktlog phyerr setting */
		
		OS_REG_WRITE(ah, AR_RX_FILTER, 0xffff | AR_RX_COMPR_BAR | rxfilterVal);
		OS_REG_WRITE(ah, AR_PHY_ERR, 0xFFFFFFFF);
		OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal| AR_RXCFG_ZLFDMA);
		OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal| 0xFF);	
		
	}else{ /* Disable phyerr and Restore regs */

		OS_REG_WRITE(ah, AR_RX_FILTER, rxfilterVal);
		OS_REG_WRITE(ah, AR_PHY_ERR, phyErrMaskVal);
		OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal);
		OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal);
	}
	HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d] ena %d rxfilterVal 0x%08x , rxcfgVal 0x%08x, phyErrMaskVal 0x%08x macPcuPhyErrRegval 0x%08x\n", __func__, __LINE__, enable, rxfilterVal, rxcfgVal, phyErrMaskVal, macPcuPhyErrRegVal);
}

#endif /* AH_SUPPORT_AR5416 */
