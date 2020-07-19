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
#include "ar5212/ar5212phy.h"
#ifdef AH_SUPPORT_5311
#include "ar5212/ar5311reg.h"
#endif

#ifdef AH_NEED_DESC_SWAP
static void ar5212SwapTxDesc(void *ds);
#endif

/*
 * Update Tx FIFO trigger level.
 *
 * Set bIncTrigLevel to TRUE to increase the trigger level.
 * Set bIncTrigLevel to FALSE to decrease the trigger level.
 *
 * Returns TRUE if the trigger level was updated
 */
HAL_BOOL
ar5212UpdateTxTrigLevel(struct ath_hal *ah, HAL_BOOL bIncTrigLevel)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	u_int32_t txcfg, curLevel, newLevel;
	HAL_INT omask;

	/*
	 * Disable interrupts while futzing with the fifo level.
	 */
	omask = ar5212SetInterrupts(ah, ahp->ah_maskReg &~ HAL_INT_GLOBAL, 0);

	txcfg = OS_REG_READ(ah, AR_TXCFG);
	curLevel = MS(txcfg, AR_FTRIG);
	newLevel = curLevel;
	if (bIncTrigLevel)		/* increase the trigger level */
		newLevel += (MAX_TX_FIFO_THRESHOLD - curLevel) / 2;
	else if (curLevel > MIN_TX_FIFO_THRESHOLD)
		newLevel--;
	if (newLevel != curLevel)
		/* Update the trigger level */
		OS_REG_WRITE(ah, AR_TXCFG,
			(txcfg &~ AR_FTRIG) | SM(newLevel, AR_FTRIG));

	/* re-enable chip interrupts */
	ar5212SetInterrupts(ah, omask, 0);

    AH_PRIVATE(ah)->ah_tx_trig_level = newLevel;

	return (newLevel != curLevel);
}

/*
 * Return Tx FIFO trigger level.
 *
 * Ar5212 does not keep track of this value, so return 0.
 */
u_int16_t 
ar5212GetTxTrigLevel(struct ath_hal *ah)
{
    return 0;
}

/*
 * Set the properties of the tx queue with the parameters
 * from q_info.  
 */
HAL_BOOL
ar5212SetTxQueueProps(struct ath_hal *ah, int q, const HAL_TXQ_INFO *q_info)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

	if (q >= pCap->hal_total_queues) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
		return AH_FALSE;
	}
	return ath_hal_set_tx_q_props(ah, &ahp->ah_txq[q], q_info);
}

/*
 * Return the properties for the specified tx queue.
 */
HAL_BOOL
ar5212GetTxQueueProps(struct ath_hal *ah, int q, HAL_TXQ_INFO *q_info)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;


	if (q >= pCap->hal_total_queues) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
		return AH_FALSE;
	}
	return ath_hal_get_tx_q_props(ah, q_info, &ahp->ah_txq[q]);
}

/*
 * Allocate and initialize a tx DCU/QCU combination.
 */
int
ar5212SetupTxQueue(struct ath_hal *ah, HAL_TX_QUEUE type,
	const HAL_TXQ_INFO *q_info)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	HAL_TX_QUEUE_INFO *qi;
	HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
	int q;

	/* XXX move queue assignment to driver */
	switch (type) {
	case HAL_TX_QUEUE_BEACON:
		q = pCap->hal_total_queues-1;	/* highest priority */
		break;
	case HAL_TX_QUEUE_CAB:
		q = pCap->hal_total_queues-2;	/* next highest priority */
		break;
	case HAL_TX_QUEUE_UAPSD:
		q = pCap->hal_total_queues-3;	/* nextest highest priority */
		if (ahp->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE) {
			HDPRINTF(ah, HAL_DBG_QUEUE, "%s: no available UAPSD tx queue\n", __func__);
			return -1;
		}
		break;
	case HAL_TX_QUEUE_DATA:
		for (q = 0; q < pCap->hal_total_queues; q++)
			if (ahp->ah_txq[q].tqi_type == HAL_TX_QUEUE_INACTIVE)
				break;
		if (q == pCap->hal_total_queues) {
			HDPRINTF(ah, HAL_DBG_QUEUE, "%s: no available tx queue\n", __func__);
			return -1;
		}
		break;
	default:
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: bad tx queue type %u\n", __func__, type);
		return -1;
	}

	HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u\n", __func__, q);

	qi = &ahp->ah_txq[q];
	if (qi->tqi_type != HAL_TX_QUEUE_INACTIVE) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: tx queue %u already active\n", __func__, q);
		return -1;
	}
	OS_MEMZERO(qi, sizeof(HAL_TX_QUEUE_INFO));
	qi->tqi_type = type;
	if (q_info == AH_NULL) {
		/* by default enable OK+ERR+DESC+URN interrupts */
		qi->tqi_qflags =
			  TXQ_FLAG_TXOKINT_ENABLE
			| TXQ_FLAG_TXERRINT_ENABLE
			| TXQ_FLAG_TXDESCINT_ENABLE
			| TXQ_FLAG_TXURNINT_ENABLE
			;
		qi->tqi_aifs = INIT_AIFS;
		qi->tqi_cwmin = HAL_TXQ_USEDEFAULT;	/* NB: do at reset */
		qi->tqi_cwmax = INIT_CWMAX;
		qi->tqi_shretry = INIT_SH_RETRY;
		qi->tqi_lgretry = INIT_LG_RETRY;
		qi->tqi_phys_comp_buf = 0;
	} else {
		qi->tqi_phys_comp_buf = q_info->tqi_comp_buf;
		(void) ar5212SetTxQueueProps(ah, q, q_info);
	}
	/* NB: must be followed by ar5212ResetTxQueue */
	return q;
}

/*
 * Update the h/w interrupt registers to reflect a tx q's configuration.
 */
static void
setTxQInterrupts(struct ath_hal *ah, HAL_TX_QUEUE_INFO *qi)
{
	struct ath_hal_5212 *ahp = AH5212(ah);

	HDPRINTF(ah, HAL_DBG_TX, "%s: tx ok 0x%x err 0x%x desc 0x%x eol 0x%x urn 0x%x\n"
		, __func__
		, ahp->ah_txOkInterruptMask
		, ahp->ah_txErrInterruptMask
		, ahp->ah_txDescInterruptMask
		, ahp->ah_txEolInterruptMask
		, ahp->ah_txUrnInterruptMask
	);

	OS_REG_WRITE(ah, AR_IMR_S0,
		  SM(ahp->ah_txOkInterruptMask, AR_IMR_S0_QCU_TXOK)
		| SM(ahp->ah_txDescInterruptMask, AR_IMR_S0_QCU_TXDESC)
	);
	OS_REG_WRITE(ah, AR_IMR_S1,
		  SM(ahp->ah_txErrInterruptMask, AR_IMR_S1_QCU_TXERR)
		| SM(ahp->ah_txEolInterruptMask, AR_IMR_S1_QCU_TXEOL)
	);
	OS_REG_RMW_FIELD(ah, AR_IMR_S2,
		AR_IMR_S2_QCU_TXURN, ahp->ah_txUrnInterruptMask);
}

/*
 * Free a tx DCU/QCU combination.
 */
HAL_BOOL
ar5212ReleaseTxQueue(struct ath_hal *ah, u_int q)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
	HAL_TX_QUEUE_INFO *qi;

	if (q >= pCap->hal_total_queues) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
		return AH_FALSE;
	}
	qi = &ahp->ah_txq[q];
	if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue %u\n", __func__, q);
		return AH_FALSE;
	}

	HDPRINTF(ah, HAL_DBG_QUEUE, "%s: release queue %u\n", __func__, q);

	qi->tqi_type = HAL_TX_QUEUE_INACTIVE;
	ahp->ah_txOkInterruptMask &= ~(1 << q);
	ahp->ah_txErrInterruptMask &= ~(1 << q);
	ahp->ah_txDescInterruptMask &= ~(1 << q);
	ahp->ah_txEolInterruptMask &= ~(1 << q);
	ahp->ah_txUrnInterruptMask &= ~(1 << q);
	setTxQInterrupts(ah, qi);

	return AH_TRUE;
}

/*
 * Set the retry, aifs, cwmin/max, readyTime regs for specified queue
 * Assumes:
 *  phwChannel has been set to point to the current channel
 */
HAL_BOOL
ar5212ResetTxQueue(struct ath_hal *ah, u_int q)
{
	struct ath_hal_5212     *ahp  = AH5212(ah);
    struct ath_hal_private  *ap   = AH_PRIVATE(ah);
	HAL_CAPABILITIES        *pCap = &AH_PRIVATE(ah)->ah_caps;
	HAL_CHANNEL_INTERNAL    *chan = AH_PRIVATE(ah)->ah_curchan;
	HAL_TX_QUEUE_INFO       *qi;
	u_int32_t               cw_min, chanCwMin, value;

	if (q >= pCap->hal_total_queues) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
		return AH_FALSE;
	}
	qi = &ahp->ah_txq[q];
	if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue %u\n", __func__, q);
		return AH_TRUE;		/* XXX??? */
	}

	HDPRINTF(ah, HAL_DBG_QUEUE, "%s: reset queue %u\n", __func__, q);

	if (qi->tqi_cwmin == HAL_TXQ_USEDEFAULT) {
		/*
		 * Select cwmin according to channel type.
		 * NB: chan can be NULL during attach
		 */
		if (chan && IS_CHAN_B(chan)) {
			chanCwMin = INIT_CWMIN_11B;
		} else {
			chanCwMin = INIT_CWMIN;
        }
		/* make sure that the CWmin is of the form (2^n - 1) */
		for (cw_min = 1; cw_min < chanCwMin; cw_min = (cw_min << 1) | 1)
			;
	} else {
		cw_min = qi->tqi_cwmin;
    }

	/* set cw_min/Max and AIFS values */
	OS_REG_WRITE(ah, AR_DLCL_IFS(q),
		  SM(cw_min, AR_D_LCL_IFS_CWMIN)
		| SM(qi->tqi_cwmax, AR_D_LCL_IFS_CWMAX)
		| SM(qi->tqi_aifs, AR_D_LCL_IFS_AIFS));

	/* Set retry limit values */
	OS_REG_WRITE(ah, AR_DRETRY_LIMIT(q), 
		   SM(INIT_SSH_RETRY, AR_D_RETRY_LIMIT_STA_SH)
		 | SM(INIT_SLG_RETRY, AR_D_RETRY_LIMIT_STA_LG)
		 | SM(qi->tqi_lgretry, AR_D_RETRY_LIMIT_FR_LG)
		 | SM(qi->tqi_shretry, AR_D_RETRY_LIMIT_FR_SH)
	);

	/* enable early termination on the QCU */
	OS_REG_WRITE(ah, AR_QMISC(q), AR_Q_MISC_DCU_EARLY_TERM_REQ);

	/* enable DCU to wait for next fragment from QCU */
	OS_REG_WRITE(ah, AR_DMISC(q), OS_REG_READ(ah, AR_DMISC(q))|AR_D_MISC_FRAG_WAIT_EN);

#ifdef AH_SUPPORT_5311
	if (AH_PRIVATE(ah)->ah_mac_version < AR_SREV_VERSION_OAHU) {
		/* Configure DCU to use the global sequence count */
		OS_REG_WRITE(ah, AR_DMISC(q), AR5311_D_MISC_SEQ_NUM_CONTROL);
	}
#endif
	/* multiqueue support */
	if (qi->tqi_cbr_period) {
		OS_REG_WRITE(ah, AR_QCBRCFG(q), 
			  SM(qi->tqi_cbr_period,AR_Q_CBRCFG_CBR_INTERVAL)
			| SM(qi->tqi_cbr_overflow_limit, AR_Q_CBRCFG_CBR_OVF_THRESH));
		OS_REG_WRITE(ah, AR_QMISC(q),
			OS_REG_READ(ah, AR_QMISC(q)) |
			AR_Q_MISC_FSP_CBR |
			(qi->tqi_cbr_overflow_limit ?
				AR_Q_MISC_CBR_EXP_CNTR_LIMIT : 0));
	}
	if (qi->tqi_ready_time && (qi->tqi_type != HAL_TX_QUEUE_CAB)) {
		OS_REG_WRITE(ah, AR_QRDYTIMECFG(q),
			SM(qi->tqi_ready_time, AR_Q_RDYTIMECFG_INT) | 
			AR_Q_RDYTIMECFG_ENA);
	}
	
	OS_REG_WRITE(ah, AR_DCHNTIME(q),
			SM(qi->tqi_burst_time, AR_D_CHNTIME_DUR) |
			(qi->tqi_burst_time ? AR_D_CHNTIME_EN : 0));
			
	if (qi->tqi_burst_time && qi->tqi_qflags & TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE) {
			OS_REG_WRITE(ah, AR_QMISC(q),
			     OS_REG_READ(ah, AR_QMISC(q)) |
			     AR_Q_MISC_RDYTIME_EXP_POLICY);
		
	}

	if (qi->tqi_qflags & TXQ_FLAG_BACKOFF_DISABLE) {
		OS_REG_WRITE(ah, AR_DMISC(q),
			OS_REG_READ(ah, AR_DMISC(q)) |
			AR_D_MISC_POST_FR_BKOFF_DIS);
	}
	if (qi->tqi_qflags & TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE) {
		OS_REG_WRITE(ah, AR_DMISC(q),
			OS_REG_READ(ah, AR_DMISC(q)) |
			AR_D_MISC_FRAG_BKOFF_EN);
	}
	switch (qi->tqi_type) {
	case HAL_TX_QUEUE_BEACON:		/* beacon frames */
		OS_REG_WRITE(ah, AR_QMISC(q),
			OS_REG_READ(ah, AR_QMISC(q))
			| AR_Q_MISC_FSP_DBA_GATED
			| AR_Q_MISC_BEACON_USE
			| AR_Q_MISC_CBR_INCR_DIS1);

		OS_REG_WRITE(ah, AR_DMISC(q),
			OS_REG_READ(ah, AR_DMISC(q))
			| (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL <<
				AR_D_MISC_ARB_LOCKOUT_CNTRL_S)
			| AR_D_MISC_BEACON_USE
			| AR_D_MISC_POST_FR_BKOFF_DIS);
		break;
	case HAL_TX_QUEUE_CAB:			/* CAB  frames */
		/* 
		 * No longer Enable AR_Q_MISC_RDYTIME_EXP_POLICY,
		 * bug #6079.  There is an issue with the CAB Queue
		 * not properly refreshing the Tx descriptor if
		 * the TXE clear setting is used.
		 */
		OS_REG_WRITE(ah, AR_QMISC(q),
			OS_REG_READ(ah, AR_QMISC(q))
			| AR_Q_MISC_FSP_DBA_GATED
			| AR_Q_MISC_CBR_INCR_DIS1
			| AR_Q_MISC_CBR_INCR_DIS0);

		value = TU_TO_USEC(qi->tqi_ready_time)
			- ap->ah_config.ath_hal_additional_swba_backoff
			- ap->ah_config.ath_hal_sw_beacon_response_time
			+ ap->ah_config.ath_hal_dma_beacon_response_time;
		OS_REG_WRITE(ah, AR_QRDYTIMECFG(q), value | AR_Q_RDYTIMECFG_ENA);

		OS_REG_WRITE(ah, AR_DMISC(q),
			OS_REG_READ(ah, AR_DMISC(q))
			| (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL <<
				AR_D_MISC_ARB_LOCKOUT_CNTRL_S));
		break;
	default:			/* NB: silence compiler */
		break;
	}

#ifndef AH_DISABLE_WME
	/*
	 * Yes, this is a hack and not the right way to do it, but
	 * it does get the lockout bits and backoff set for the
	 * high-pri WME queues for testing.  We need to either extend
	 * the meaning of queueInfo->mode, or create something like
	 * queueInfo->dcumode.
	 */
	if (qi->tqi_int_flags & HAL_TXQ_USE_LOCKOUT_BKOFF_DIS) {
		OS_REG_WRITE(ah, AR_DMISC(q),
			 OS_REG_READ(ah, AR_DMISC(q)) |
			 SM(AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL,
			    AR_D_MISC_ARB_LOCKOUT_CNTRL)|
			 AR_D_MISC_POST_FR_BKOFF_DIS);
	}
#endif

	/* Setup compression scratchpad buffer */
	/* 
	 * XXX: calling this asynchronously to queue operation can
	 *      cause unexpected behavior!!!
	 */
	if (qi->tqi_phys_comp_buf) {
		HALASSERT((qi->tqi_type == HAL_TX_QUEUE_DATA || qi->tqi_type == HAL_TX_QUEUE_UAPSD));
		OS_REG_WRITE(ah, AR_Q_CBBS, (80 + 2*q));
		OS_REG_WRITE(ah, AR_Q_CBBA, qi->tqi_phys_comp_buf);
		OS_REG_WRITE(ah, AR_Q_CBC,  HAL_COMP_BUF_MAX_SIZE/1024);
		OS_REG_WRITE(ah, AR_Q0_MISC + 4*q,
			     OS_REG_READ(ah, AR_Q0_MISC + 4*q)
			     | AR_Q_MISC_QCU_COMP_EN);
	}
	
	/*
	 * Always update the secondary interrupt mask registers - this
	 * could be a new queue getting enabled in a running system or
	 * hw getting re-initialized during a reset!
	 *
	 * Since we don't differentiate between tx interrupts corresponding
	 * to individual queues - secondary tx mask regs are always unmasked;
	 * tx interrupts are enabled/disabled for all queues collectively
	 * using the primary mask reg
	 */
	if (qi->tqi_qflags & TXQ_FLAG_TXOKINT_ENABLE)
		ahp->ah_txOkInterruptMask |= 1 << q;
	else
		ahp->ah_txOkInterruptMask &= ~(1 << q);
	if (qi->tqi_qflags & TXQ_FLAG_TXERRINT_ENABLE)
		ahp->ah_txErrInterruptMask |= 1 << q;
	else
		ahp->ah_txErrInterruptMask &= ~(1 << q);
	if (qi->tqi_qflags & TXQ_FLAG_TXDESCINT_ENABLE)
		ahp->ah_txDescInterruptMask |= 1 << q;
	else
		ahp->ah_txDescInterruptMask &= ~(1 << q);
	if (qi->tqi_qflags & TXQ_FLAG_TXEOLINT_ENABLE)
		ahp->ah_txEolInterruptMask |= 1 << q;
	else
		ahp->ah_txEolInterruptMask &= ~(1 << q);
	if (qi->tqi_qflags & TXQ_FLAG_TXURNINT_ENABLE)
		ahp->ah_txUrnInterruptMask |= 1 << q;
	else
		ahp->ah_txUrnInterruptMask &= ~(1 << q);
	setTxQInterrupts(ah, qi);

	return AH_TRUE;
}

/*
 * Get the TXDP for the specified queue
 */
u_int32_t
ar5212GetTxDP(struct ath_hal *ah, u_int q)
{
	HALASSERT(q < AH_PRIVATE(ah)->ah_caps.hal_total_queues);
	return OS_REG_READ(ah, AR_QTXDP(q));
}

/*
 * Set the TxDP for the specified queue
 */
HAL_BOOL
ar5212SetTxDP(struct ath_hal *ah, u_int q, u_int32_t txdp)
{
	HALASSERT(q < AH_PRIVATE(ah)->ah_caps.hal_total_queues);
	HALASSERT(AH5212(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

	/*
	 * Make sure that TXE is deasserted before setting the TXDP.  If TXE
	 * is still asserted, setting TXDP will have no effect.
	 */
	HALASSERT((OS_REG_READ(ah, AR_Q_TXE) & (1 << q)) == 0);

	OS_REG_WRITE(ah, AR_QTXDP(q), txdp);

	return AH_TRUE;
}

/*
 * Set Transmit Enable bits for the specified queue
 */
HAL_BOOL
ar5212StartTxDma(struct ath_hal *ah, u_int q)
{
	HALASSERT(q < AH_PRIVATE(ah)->ah_caps.hal_total_queues);

	HALASSERT(AH5212(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

	HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u\n", __func__, q);

	/* Check to be sure we're not enabling a q that has its TXD bit set. */
	HALASSERT((OS_REG_READ(ah, AR_Q_TXD) & (1 << q)) == 0);

#ifdef AH_SUPPORT_DFS
	if (AH5212(ah)->ah_dfsDisabled & HAL_DFS_DISABLE) {
		u_int32_t val;

		val = OS_REG_READ(ah, AR_Q_TXE);
		val &= ~(1 << q);
		OS_REG_WRITE(ah, AR_Q_TXE, val);
		return AH_FALSE;
	}
#endif
	OS_REG_WRITE(ah, AR_Q_TXE, 1 << q);
	return AH_TRUE;
}

/*
 * Return the number of pending frames or 0 if the specified
 * queue is stopped.
 */
u_int32_t
ar5212NumTxPending(struct ath_hal *ah, u_int q)
{
	u_int32_t npend;

	HALASSERT(q < AH_PRIVATE(ah)->ah_caps.hal_total_queues);

	npend = OS_REG_READ(ah, AR_QSTS(q)) & AR_Q_STS_PEND_FR_CNT;
	if (npend == 0) {
		/*
		 * Pending frame count (PFC) can momentarily go to zero
		 * while TXE remains asserted.  In other words a PFC of
		 * zero is not sufficient to say that the queue has stopped.
		 */
		if (OS_REG_READ(ah, AR_Q_TXE) & (1 << q))
			npend = 1;		/* arbitrarily return 1 */
	}
#ifdef DEBUG
	if (npend && (AH5212(ah)->ah_txq[q].tqi_type == HAL_TX_QUEUE_CAB)) {
		if (OS_REG_READ(ah, AR_Q_RDYTIMESHDN) & (1 << q)) {
			HDPRINTF(ah, HAL_DBG_QUEUE, "RTSD on CAB queue\n");
			/* Clear the ReadyTime shutdown status bits */
			OS_REG_WRITE(ah, AR_Q_RDYTIMESHDN, 1 << q);
		}
	}
#endif
    HALASSERT((npend == 0) || (AH5212(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE));
        
	return npend;
}

/*
 * Stop transmit on the specified queue
 */
HAL_BOOL
ar5212StopTxDma(struct ath_hal *ah, u_int q, u_int timeout)
{
#define AH_TX_STOP_DMA_TIMEOUT 4000    /* usec */  
#define AH_TIME_QUANTUM        100     /* usec */

	u_int i;
	u_int wait;

	HALASSERT(q < AH_PRIVATE(ah)->ah_caps.hal_total_queues);

	HALASSERT(AH5212(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

    if (timeout == 0) {
        timeout = AH_TX_STOP_DMA_TIMEOUT;
    }

	OS_REG_WRITE(ah, AR_Q_TXD, 1 << q);
	for (i = timeout / AH_TIME_QUANTUM; i != 0; i--) {
		if (ar5212NumTxPending(ah, q) == 0)
			break;
		OS_DELAY(AH_TIME_QUANTUM);        /* XXX get actual value */
	}
#ifdef AH_DEBUG
	if (i == 0) {
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u DMA did not stop in 100 msec\n",
			__func__, q);
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: QSTS 0x%x Q_TXE 0x%x Q_TXD 0x%x Q_CBR 0x%x\n"
			, __func__
			, OS_REG_READ(ah, AR_QSTS(q))
			, OS_REG_READ(ah, AR_Q_TXE)
			, OS_REG_READ(ah, AR_Q_TXD)
			, OS_REG_READ(ah, AR_QCBRCFG(q))
		);
		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Q_MISC 0x%x Q_RDYTIMECFG 0x%x Q_RDYTIMESHDN 0x%x\n"
			, __func__
			, OS_REG_READ(ah, AR_QMISC(q))
			, OS_REG_READ(ah, AR_QRDYTIMECFG(q))
			, OS_REG_READ(ah, AR_Q_RDYTIMESHDN)
		);
	}
#endif /* AH_DEBUG */

	/* 2413+ and up can kill packets at the PCU level */
	if (ar5212NumTxPending(ah, q) && (IS_2413(ah) || IS_5413(ah) || IS_2425(ah) || IS_2417(ah))) {
		u_int32_t tsfLow, j;

		HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Num of pending TX Frames %d on Q %d\n",
			 __func__, ar5212NumTxPending(ah, q), q);

		/* Kill last PCU Tx Frame */
		/* TODO - save off and restore current values of Q1/Q2? */
		for (j = 0; j < 2; j++) {
			tsfLow = OS_REG_READ(ah, AR_TSF_L32);
			OS_REG_WRITE(ah, AR_QUIET2, SM(100, AR_QUIET2_QUIET_PER) |
				     SM(10, AR_QUIET2_QUIET_DUR));
			OS_REG_WRITE(ah, AR_QUIET1, AR_QUIET1_QUIET_ENABLE |
				     SM(tsfLow >> 10, AR_QUIET1_NEXT_QUIET));
			if ((OS_REG_READ(ah, AR_TSF_L32) >> 10) == (tsfLow >> 10)) {
				break;
			}
			HDPRINTF(ah, HAL_DBG_TX, "%s: TSF have moved while trying to set quiet time TSF: 0x%08x\n", __func__, tsfLow);
			HALASSERT(j < 1); /* TSF shouldn't count twice or reg access is taking forever */
		}
		
		OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_CHAN_IDLE);
		
		/* Allow the quiet mechanism to do its work */
		OS_DELAY(200);
		OS_REG_CLR_BIT(ah, AR_QUIET1, AR_QUIET1_QUIET_ENABLE);
	
		/* Verify all transmit is dead */
		wait = timeout / AH_TIME_QUANTUM;
		while (ar5212NumTxPending(ah, q)) {
			if ((--wait) == 0) {
#ifdef AH_DEBUG
				HDPRINTF(ah, HAL_DBG_TX, "%s: Failed to stop Tx DMA in %d msec after killing last frame\n",
					 __func__, timeout / 1000);
#endif
				break;
			}
			OS_DELAY(AH_TIME_QUANTUM);
		}

		OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_CHAN_IDLE);
	}

	OS_REG_WRITE(ah, AR_Q_TXD, 0);
	return (i != 0);

#undef AH_TX_STOP_DMA_TIMEOUT
#undef AH_TIME_QUANTUM
}

/*
 * Descriptor Access Functions
 */

#define	VALID_PKT_TYPES \
	((1<<HAL_PKT_TYPE_NORMAL)|(1<<HAL_PKT_TYPE_ATIM)|\
	 (1<<HAL_PKT_TYPE_PSPOLL)|(1<<HAL_PKT_TYPE_PROBE_RESP)|\
	 (1<<HAL_PKT_TYPE_BEACON))
#define	isValidPktType(_t)	((1<<(_t)) & VALID_PKT_TYPES)
#define	VALID_TX_RATES \
	((1<<0x0b)|(1<<0x0f)|(1<<0x0a)|(1<<0x0e)|(1<<0x09)|(1<<0x0d)|\
	 (1<<0x08)|(1<<0x0c)|(1<<0x1b)|(1<<0x1a)|(1<<0x1e)|(1<<0x19)|\
	 (1<<0x1d)|(1<<0x18)|(1<<0x1c))
#define	isValidTxRate(_r)	((1<<(_r)) & VALID_TX_RATES)

HAL_BOOL
ar5212SetupTxDesc(struct ath_hal *ah, struct ath_desc *ds,
	u_int pktLen,
	u_int hdrLen,
	HAL_PKT_TYPE type,
	u_int txPower,
	u_int txRate0, u_int txTries0,
	u_int key_ix,
	u_int antMode,
	u_int flags,
	u_int rts_cts_rate,
	u_int rts_cts_duration,
	u_int compicvLen,
	u_int compivLen,
	u_int comp)
{
#define	RTSCTS	(HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)
	struct ar5212_desc *ads = AR5212DESC(ds);
	struct ath_hal_5212 *ahp = AH5212(ah);

	(void) hdrLen;

	HALASSERT(txTries0 != 0);
	HALASSERT(isValidPktType(type));
	HALASSERT(isValidTxRate(txRate0));
	HALASSERT((flags & RTSCTS) != RTSCTS);
	/* XXX validate antMode */

        txPower = (txPower + ahp->ah_txPowerIndexOffset );
        if(txPower > 63)  txPower=63;

	ads->ds_ctl0 = (pktLen & AR_FrameLen)
		     | (txPower << AR_XmitPower_S)
		     | (flags & HAL_TXDESC_VEOL ? AR_VEOL : 0)
		     | (flags & HAL_TXDESC_CLRDMASK ? AR_ClearDestMask : 0)
		     | SM(antMode, AR_AntModeXmit)
		     | (flags & HAL_TXDESC_INTREQ ? AR_TxInterReq : 0)
		     ;
	ads->ds_ctl1 = (type << AR_FrmType_S)
		     | (flags & HAL_TXDESC_NOACK ? AR_NoAck : 0)
                     | (comp << AR_CompProc_S)
                     | (compicvLen << AR_CompICVLen_S)
                     | (compivLen << AR_CompIVLen_S)
                     ;
	ads->ds_ctl2 = SM(txTries0, AR_XmitDataTries0)
		     ;
	ads->ds_ctl3 = (txRate0 << AR_XmitRate0_S)
		     ;
	if (key_ix != HAL_TXKEYIX_INVALID) {
		/* XXX validate key index */
		ads->ds_ctl1 |= SM(key_ix, AR_DestIdx);
		ads->ds_ctl0 |= AR_DestIdxValid;
	}
	if (flags & RTSCTS) {
		if (!isValidTxRate(rts_cts_rate)) {
			HDPRINTF(ah, HAL_DBG_TX, "%s: invalid rts/cts rate 0x%x\n",
				__func__, rts_cts_rate);
			return AH_FALSE;
		}
		/* XXX validate rts_cts_duration */
		ads->ds_ctl0 |= (flags & HAL_TXDESC_CTSENA ? AR_CTSEnable : 0)
			     | (flags & HAL_TXDESC_RTSENA ? AR_RTSCTSEnable : 0)
			     ;
		ads->ds_ctl2 |= SM(rts_cts_duration, AR_RTSCTSDuration);
		ads->ds_ctl3 |= (rts_cts_rate << AR_RTSCTSRate_S);
	}
	return AH_TRUE;
#undef RTSCTS
}

void
ar5212IntrReqTxDesc(struct ath_hal *ah, void *ds)
{
	struct ar5212_desc *ads = AR5212DESC(ds);

#ifdef AH_NEED_DESC_SWAP
	ads->ds_ctl0 |= __bswap32(AR_TxInterReq);
#else
	ads->ds_ctl0 |= AR_TxInterReq;
#endif
}

HAL_BOOL
ar5212FillTxDesc (struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
        u_int32_t *seg_len, u_int desc_id, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL first_seg,
        HAL_BOOL last_seg, const void *ds0)
{
	struct ar5212_desc *ads = AR5212DESC(ds);

	HALASSERT((seg_len[0] &~ AR_BufLen) == 0);

    /* Set the buffer addresses */
    ads->ds_data = bufAddr[0];

	if (first_seg) {
		/*
		 * First descriptor, don't clobber xmit control data
		 * setup by ar5212SetupTxDesc.
		 */
		ads->ds_ctl1 |= seg_len[0] | (last_seg ? 0 : AR_More);
	} else if (last_seg) {		/* !first_seg && last_seg */
		/*
		 * Last descriptor in a multi-descriptor frame,
		 * copy the multi-rate transmit parameters from
		 * the first frame for processing on completion. 
		 */
		ads->ds_ctl0 = AR5212DESC_CONST(ds0)->ds_ctl0 & AR_TxInterReq;
		ads->ds_ctl1 = seg_len[0];
#ifdef AH_NEED_DESC_SWAP
		ads->ds_ctl2 = __bswap32(AR5212DESC_CONST(ds0)->ds_ctl2);
		ads->ds_ctl3 = __bswap32(AR5212DESC_CONST(ds0)->ds_ctl3);
#else
		ads->ds_ctl2 = AR5212DESC_CONST(ds0)->ds_ctl2;
		ads->ds_ctl3 = AR5212DESC_CONST(ds0)->ds_ctl3;
#endif
	} else {			/* !first_seg && !last_seg */
		/*
		 * Intermediate descriptor in a multi-descriptor frame.
		 */
		ads->ds_ctl0 = AR5212DESC_CONST(ds0)->ds_ctl0 & AR_TxInterReq;
		ads->ds_ctl1 = seg_len[0] | AR_More;
		ads->ds_ctl2 = 0;
		ads->ds_ctl3 = 0;
	}
	ads->ds_txstatus0 = ads->ds_txstatus1 = 0;
	return AH_TRUE;
}

void
ar5212SetDescLink(struct ath_hal *ah, void *ds, u_int32_t link)
{
	struct ar5212_desc *ads = AR5212DESC(ds);

    ads->ds_link = link;
}
void
ar5212GetDescLinkPtr(struct ath_hal *ah, void *ds, u_int32_t **link)
{
    struct ar5212_desc *ads = AR5212DESC(ds);

    *link = &ads->ds_link;
}

void
ar5212ClearTxDescStatus(struct ath_hal *ah, void *ds)
{
	struct ar5212_desc *ads = AR5212DESC(ds);
	ads->ds_txstatus0 = ads->ds_txstatus1 = 0;
}

#ifdef ATH_SWRETRY
void 
ar5212ClearDestMask(struct ath_hal *ah, void *ds)
{
	struct ar5212_desc *ads = AR5212DESC(ds);
	ads->ds_ctl0 |= AR_ClearDestMask;
}
#endif

#ifdef AH_NEED_DESC_SWAP
/* Swap transmit descriptor */
static __inline void
ar5212SwapTxDesc(void *ds)
{
	ds->ds_data = __bswap32(ds->ds_data);
        ds->ds_ctl0 = __bswap32(ds->ds_ctl0);
        ds->ds_ctl1 = __bswap32(ds->ds_ctl1);
        ds->ds_hw[0] = __bswap32(ds->ds_hw[0]);
        ds->ds_hw[1] = __bswap32(ds->ds_hw[1]);
        ds->ds_hw[2] = __bswap32(ds->ds_hw[2]);
        ds->ds_hw[3] = __bswap32(ds->ds_hw[3]);
}
#endif

/*
 * Processing of HW TX descriptor.
 */
HAL_STATUS
ar5212ProcTxDesc(struct ath_hal *ah, void *desc)
{
	struct ar5212_desc *ads = AR5212DESC(desc);
    struct ath_desc *ds = (struct ath_desc *)desc;
#ifdef AH_SUPPORT_DFS
	struct ath_hal_5212 *ahp = AH5212(ah);

	if (ahp->ah_dfsDisabled & HAL_DFS_TIMEMARK) {
		u_int64_t thisTsf;
		u_int q;

		thisTsf = ar5212GetTsf64(ah);
		if ((thisTsf - ahp->ah_dfsMarkTime) >= HAL_RADAR_DISABLE_TIME) {
			ahp->ah_dfsDisabled |= HAL_DFS_DISABLE;
			for (q=0;q<AH_PRIVATE(ah)->ah_caps.hal_total_queues; q++) {
				HDPRINTF(ah, HAL_DBG_DFS, "Disabling q %d\n",q);
				ar5212StopTxDma(ah, q, 0);
			}
		}
	}
	if (ahp->ah_dfsDisabled & HAL_DFS_DISABLE)
		return HAL_EINPROGRESS;
#endif

#ifdef AH_NEED_DESC_SWAP
	if ((ads->ds_txstatus1 & __bswap32(AR_Done)) == 0)
                return HAL_EINPROGRESS;

	ar5212SwapTxDesc(ds);
#else
	if ((ads->ds_txstatus1 & AR_Done) == 0)
		return HAL_EINPROGRESS;
#endif

	/* Update software copies of the HW status */
	ds->ds_txstat.ts_seqnum = MS(ads->ds_txstatus1, AR_SeqNum);
	ds->ds_txstat.ts_tstamp = MS(ads->ds_txstatus0, AR_SendTimestamp);
	ds->ds_txstat.ts_status = 0;
	if ((ads->ds_txstatus0 & AR_FrmXmitOK) == 0) {
		if (ads->ds_txstatus0 & AR_ExcessiveRetries)
			ds->ds_txstat.ts_status |= HAL_TXERR_XRETRY;
		if (ads->ds_txstatus0 & AR_Filtered)
			ds->ds_txstat.ts_status |= HAL_TXERR_FILT;
		if (ads->ds_txstatus0 & AR_FIFOUnderrun)
			ds->ds_txstat.ts_status |= HAL_TXERR_FIFO;
	}
	/*
	 * Extract the transmit rate used.
	 */
        ds->ds_txstat.ts_rateindex = MS(ads->ds_txstatus1, AR_FinalTSIndex);
	switch (ds->ds_txstat.ts_rateindex) {
	case 0:
		ds->ds_txstat.ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate0);
		break;
	case 1:
		ds->ds_txstat.ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate1);
		break;
	case 2:
		ds->ds_txstat.ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate2);
		break;
	case 3:
		ds->ds_txstat.ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate3);
		break;
	}
	ds->ds_txstat.ts_rssi = MS(ads->ds_txstatus1, AR_AckSigStrength);
	ds->ds_txstat.ts_shortretry = MS(ads->ds_txstatus0, AR_RTSFailCnt);
	ds->ds_txstat.ts_longretry = MS(ads->ds_txstatus0, AR_DataFailCnt);
	/*
	 * The retry count has the number of un-acked tries for the
	 * final series used.  When doing multi-rate retry we must
	 * fixup the retry count by adding in the try counts for
	 * each series that was fully-processed.  Beware that this
	 * takes values from the try counts in the final descriptor.
	 * These are not required by the hardware.  We assume they
	 * are placed there by the driver as otherwise we have no
	 * access and the driver can't do the calculation because it
	 * doesn't know the descriptor format.
	 */
	switch (MS(ads->ds_txstatus1, AR_FinalTSIndex)) {
	case 3:
		ds->ds_txstat.ts_longretry +=
			MS(ads->ds_ctl2, AR_XmitDataTries2);
	case 2:
		ds->ds_txstat.ts_longretry +=
			MS(ads->ds_ctl2, AR_XmitDataTries1);
	case 1:
		ds->ds_txstat.ts_longretry +=
			MS(ads->ds_ctl2, AR_XmitDataTries0);
	}
	ds->ds_txstat.ts_virtcol = MS(ads->ds_txstatus0, AR_VirtCollCnt);
	ds->ds_txstat.ts_antenna = (ads->ds_txstatus1 & AR_XmitAtenna ? 2 : 1);

	return HAL_OK;
}

/*
 * Calculate air time of a transmit packet
 */
u_int32_t
ar5212CalcTxAirtime(struct ath_hal *ah, void *ds, struct ath_tx_status *ts, 
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes)
{
    return 0;
}

/*
 * Determine which tx queues need interrupt servicing.
 */
void
ar5212GetTxIntrQueue(struct ath_hal *ah, u_int32_t *txqs)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	*txqs &= ahp->ah_intrTxqs;
	ahp->ah_intrTxqs &= ~(*txqs);
}

void
ar5212Set11nTxDesc(struct ath_hal *ah, 
				void *ds, 
				u_int pktLen, 
				HAL_PKT_TYPE type, 
				u_int txPower, 
				u_int key_ix, 
				HAL_KEY_TYPE keyType, 
				u_int flags)
{
	struct ar5212_desc *ads = AR5212DESC(ds);
	struct ath_hal_5212 *ahp = AH5212(ah);

	HALASSERT(isValidPktType(type));

	txPower = (txPower + ahp->ah_txPowerIndexOffset );
	if(txPower > 63)  txPower=63;

	ads->ds_ctl0 =  (pktLen & AR_FrameLen)
				| SM(txPower, AR_XmitPower)
				| (flags & HAL_TXDESC_VEOL ? AR_VEOL : 0)
				| (flags & HAL_TXDESC_CLRDMASK ? AR_ClearDestMask : 0)
				| (flags & HAL_TXDESC_INTREQ ? AR_TxInterReq : 0)
				| (key_ix != HAL_TXKEYIX_INVALID ? AR_DestIdxValid : 0)
				;

	ads->ds_ctl1 =  (key_ix != HAL_TXKEYIX_INVALID ? SM(key_ix, AR_DestIdx) : 0)
				| SM(type, AR_FrmType)
				| (flags & HAL_TXDESC_NOACK ? AR_NoAck : 0)
				;

}

void
ar5212Set11nRateScenario(struct ath_hal *ah,
					void *ds,
					void *lastds,
					u_int dur_update_en,
					u_int rts_cts_rate,
					u_int rts_cts_duration,
					HAL_11N_RATE_SERIES series[],
					u_int nseries,
					u_int flags, u_int32_t smartAntenna)
{
	struct ar5212_desc *ads = AR5212DESC(ds);
	struct ar5212_desc *last_ads = AR5212DESC(lastds);
	u_int32_t ds_ctl0;

	HALASSERT(nseries == 4);
	(void)nseries;

	/*
	 * Rate control settings override
	 */

	ads->ds_ctl2 = (dur_update_en ? AR_DurUpdateEna : 0)
				| SM(series[0].Tries, AR_XmitDataTries0)
				| SM(series[1].Tries, AR_XmitDataTries1)
				| SM(series[2].Tries, AR_XmitDataTries2)
				| SM(series[3].Tries, AR_XmitDataTries3)
				;


	ads->ds_ctl3 =   SM(series[0].Rate, AR_XmitRate0)
				| SM(series[1].Rate, AR_XmitRate1)
				| SM(series[2].Rate, AR_XmitRate2)
				| SM(series[3].Rate, AR_XmitRate3)
				;

	if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) {
		ds_ctl0 = ads->ds_ctl0;

		if (flags & HAL_TXDESC_RTSENA) {
			ds_ctl0 &= ~AR_CTSEnable;
			ds_ctl0 |= AR_RTSCTSEnable;
		} else {
			ds_ctl0 &= ~AR_RTSCTSEnable;
			ds_ctl0 |= AR_CTSEnable;
		}
		ads->ds_ctl0 = ds_ctl0;

		if (!isValidTxRate(rts_cts_rate)) {
			HDPRINTF(ah, HAL_DBG_TX, "%s: invalid rts/cts rate 0x%x\n",
				__func__, rts_cts_rate);
			return;
		}
		ads->ds_ctl2 |= SM(rts_cts_duration, AR_RTSCTSDuration);
		ads->ds_ctl3 |= SM(rts_cts_rate, AR_RTSCTSRate);
	}
#ifdef AH_NEED_DESC_SWAP
        last_ads->ds_ctl2 = __bswap32(ads->ds_ctl2);
        last_ads->ds_ctl3 = __bswap32(ads->ds_ctl3);
#else
        last_ads->ds_ctl2 = ads->ds_ctl2;
        last_ads->ds_ctl3 = ads->ds_ctl3;
#endif
}

#ifdef AH_PRIVATE_DIAG
void
ar5212_ContTxMode(struct ath_hal *ah, void *ds, int mode)
{
	static int qnum =0;
	int i;
	unsigned int qbits, val, val1, val2;
	struct ar5212_desc *ads = AR5212DESC(ds);

	if(mode == 10) return;

    if (mode==7) {			// print status from the cont tx desc
		if (ads) {
			val1 = ads->ds_txstatus0;
			val2 = ads->ds_txstatus1;
			HDPRINTF(ah, HAL_DBG_TX, "s0(%x) s1(%x)\n",
						   (unsigned)val1, (unsigned)val2);
		}
		HDPRINTF(ah, HAL_DBG_TX, "txe(%x) txd(%x)\n",
					   OS_REG_READ(ah, AR_Q_TXE),
					   OS_REG_READ(ah, AR_Q_TXD)
			);
		for(i=0;i<HAL_NUM_TX_QUEUES; i++) {
			val = OS_REG_READ(ah, AR_QTXDP(i));
			val2 = OS_REG_READ(ah, AR_QSTS(i)) & AR_Q_STS_PEND_FR_CNT;
			HDPRINTF(ah, HAL_DBG_TX, "[%d] %x %d\n", i, val, val2);
		}
		return;
    }
    if (mode==8) {			// set TXE for qnum
		OS_REG_WRITE(ah, AR_Q_TXE, 1<<qnum);
		return;
    }
    if (mode==9) {
		return;
    }

    if (mode >= 1) {			// initiate cont tx operation
		/* Disable AGC to A2 */
		qnum = (int)ds;

		OS_REG_WRITE(ah, AR_PHY_TEST,
					 (OS_REG_READ(ah, AR_PHY_TEST) | PHY_AGC_CLR) );

		OS_REG_WRITE(ah, 0x9864, OS_REG_READ(ah, 0x9864) | 0x7f000);
		OS_REG_WRITE(ah, 0x9924, OS_REG_READ(ah, 0x9924) | 0x7f00fe);
		OS_REG_WRITE(ah, AR_DIAG_SW,
					 (OS_REG_READ(ah, AR_DIAG_SW) | (DIAG_FORCE_RXCLR+DIAG_IGNORE_NAV)) );


		OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);	// set receive disable

		if (mode == 3 || mode == 4) {
			int txcfg;

			if (mode == 3) {
				OS_REG_WRITE(ah, AR_DLCL_IFS(qnum), 0);
				OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
				OS_REG_WRITE(ah, AR_D_GBL_IFS_SIFS, 100);
				OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, 100);
				OS_REG_WRITE(ah, AR_TIME_OUT, 2);
				OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, 100);
			}
			OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
			OS_REG_WRITE(ah, AR_D_FPCTL, 0x10|qnum);	// enable prefetch on qnum
			txcfg = 5 | (6<<AR_FTRIG_S);
			OS_REG_WRITE(ah, AR_TXCFG, txcfg);

			OS_REG_WRITE(ah, AR_QMISC(qnum),	// set QCU modes
						 AR_Q_MISC_DCU_EARLY_TERM_REQ
						 +AR_Q_MISC_FSP_ASAP
						 +AR_Q_MISC_CBR_INCR_DIS1
						 +AR_Q_MISC_CBR_INCR_DIS0
				);

			/* stop tx dma all all except qnum */
			qbits = 0x3ff;
			qbits &= ~(1<<qnum);
			for (i=0; i<10; i++) {
				if (i==qnum) continue;
				OS_REG_WRITE(ah, AR_Q_TXD, 1<<i);
			}
			OS_REG_WRITE(ah, AR_Q_TXD, qbits);

			/* clear and freeze MIB counters */
			OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_CMC);
			OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_FMC);

			OS_REG_WRITE(ah, AR_DMISC(qnum),
						 (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL << AR_D_MISC_ARB_LOCKOUT_CNTRL_S)
						 +(AR_D_MISC_ARB_LOCKOUT_IGNORE)
						 +(AR_D_MISC_POST_FR_BKOFF_DIS)
						 +(AR_D_MISC_VIRT_COLL_POLICY)
						 +(AR_D_MISC_VIR_COL_HANDLING_IGNORE)
				);

			for(i=0; i<HAL_NUM_TX_QUEUES+2; i++) {	// disconnect QCUs
				if (i==qnum) continue;
				OS_REG_WRITE(ah, AR_DQCUMASK(i), 0);
			}
		}
    }
    if (mode == 0) {

		OS_REG_WRITE(ah, AR_PHY_TEST,
					 (OS_REG_READ(ah, AR_PHY_TEST) & ~PHY_AGC_CLR) );
		OS_REG_WRITE(ah, AR_DIAG_SW,
					 (OS_REG_READ(ah, AR_DIAG_SW) & ~(DIAG_FORCE_RXCLR+DIAG_IGNORE_NAV)) );
    }
}
#endif

/*
 * Abort transmit on all queues
 */
HAL_BOOL
ar5212AbortTxDma(struct ath_hal *ah)
{
    return AH_TRUE;
}

#endif /* AH_SUPPORT_AR5212 */
