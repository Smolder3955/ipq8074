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
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"


/*
 * Checks to see if an interrupt is pending on our NIC
 *
 * Returns: TRUE    if an interrupt is pending
 *          FALSE   if not
 */
HAL_BOOL
ar5212IsInterruptPending(struct ath_hal *ah)
{
	/* 
	 * Some platforms trigger our ISR before applying power to
	 * the card, so make sure the INTPEND is really 1, not 0xffffffff.
	 */
	return (OS_REG_READ(ah, AR_INTPEND) == AR_INTPEND_TRUE);
}

/*
 * Reads the Interrupt Status Register value from the NIC, thus deasserting
 * the interrupt line, and returns both the masked and unmasked mapped ISR
 * values.  The value returned is mapped to abstract the hw-specific bit
 * locations in the Interrupt Status Register.
 *
 * Returns: A hardware-abstracted bitmap of all non-masked-out
 *          interrupts pending, as well as an unmasked value
 */
HAL_BOOL
ar5212GetPendingInterrupts(struct ath_hal *ah, HAL_INT *masked,
                           HAL_INT_TYPE type, u_int8_t msi, HAL_BOOL nortc)
{
	u_int32_t isr;
	u_int32_t mask2=0;
	HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

    if (nortc)
        return AH_FALSE;

	isr = OS_REG_READ(ah, AR_ISR);
	if (isr & AR_ISR_BCNMISC) {
		u_int32_t isr2;
		isr2 = OS_REG_READ(ah, AR_ISR_S2);
		if (isr2 & AR_ISR_S2_TIM)
			mask2 |= HAL_INT_TIM;
		if (isr2 & AR_ISR_S2_DTIM)
			mask2 |= HAL_INT_DTIM;
		if (isr2 & AR_ISR_S2_DTIMSYNC)
			mask2 |= HAL_INT_DTIMSYNC;
		if (isr2 & (AR_ISR_S2_CABEND ))
			mask2 |= HAL_INT_CABEND;
				if (isr2 & AR_ISR_S2_TSFOOR)
					mask2 |= HAL_INT_TSFOOR;

		if (!pCap->hal_isr_rac_support) {
			/*
			 * EV61133 (missing interrupts due to ISR_RAC):
			 * If not using ISR_RAC, clear interrupts by writing to ISR_S2.
			 * This avoids a race condition where a new BCNMISC interrupt
			 * could come in between reading the ISR and clearing the interrupt
			 * via the primary ISR.  We therefore clear the interrupt via
			 * the secondary, which avoids this race.
			 */ 
			OS_REG_WRITE(ah, AR_ISR_S2, isr2);
			isr &= ~AR_ISR_BCNMISC;
		}
	}

	/* Use AR_ISR_RAC only if chip supports it. 
	 * See EV61133 (missing interrupts due to ISR_RAC) 
	 */
	if (pCap->hal_isr_rac_support) {
		isr = OS_REG_READ(ah, AR_ISR_RAC);
	}
	if (isr == 0xffffffff) {
		*masked = 0;
		return AH_FALSE;;
	}

	*masked = isr & HAL_INT_COMMON;

	if (isr & AR_ISR_HIUERR) {
		HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: fatal error, ISR_RAC=0x%x ISR_S2_S=0x%x\n",
			__func__, isr, OS_REG_READ(ah, AR_ISR_S2_S));
		*masked |= HAL_INT_FATAL;
	}
	if (isr & (AR_ISR_RXOK | AR_ISR_RXERR))
		*masked |= HAL_INT_RX;
	if (isr & (AR_ISR_TXOK | AR_ISR_TXDESC | AR_ISR_TXERR | AR_ISR_TXEOL)) {
		struct ath_hal_5212 *ahp = AH5212(ah);
		u_int32_t s0, s1;

		*masked |= HAL_INT_TX;

		if (pCap->hal_isr_rac_support) {
			/* Use secondary shadow registers if using ISR_RAC */
			s0 = OS_REG_READ(ah, AR_ISR_S0_S);
			s1 = OS_REG_READ(ah, AR_ISR_S1_S);
		} else {
			/*
			 * EV61133 (missing interrupts due to ISR_RAC):
			 * If not using ISR_RAC, clear interrupts by writing to ISR_S0/S1.
			 * This avoids a race condition where a new interrupt
			 * could come in between reading the ISR and clearing the interrupt
			 * via the primary ISR.  We therefore clear the interrupt via
			 * the secondary, which avoids this race.
			 */ 
			s0 = OS_REG_READ(ah, AR_ISR_S0);
			OS_REG_WRITE(ah, AR_ISR_S0, s0);
			s1 = OS_REG_READ(ah, AR_ISR_S1);
			OS_REG_WRITE(ah, AR_ISR_S1, s1);

			isr &= ~(AR_ISR_TXOK | AR_ISR_TXDESC | AR_ISR_TXERR | AR_ISR_TXEOL);
		}
		ahp->ah_intrTxqs |= MS(s0, AR_ISR_S0_QCU_TXOK);
		ahp->ah_intrTxqs |= MS(s0, AR_ISR_S0_QCU_TXDESC);
		ahp->ah_intrTxqs |= MS(s1, AR_ISR_S1_QCU_TXERR);
		ahp->ah_intrTxqs |= MS(s1, AR_ISR_S1_QCU_TXEOL);
	}

	/*
	 * Receive overrun is usually non-fatal on Oahu/Spirit.
	 *
	 * BUT early silicon had a bug which causes the rx to fail
	 * and the chip must be reset.
	 *
	 * AND even AR5311 v3.1 and AR5211 v4.2 silicon seems to
	 * have a problem with RXORN which hangs the receive path
	 * and requires a chip reset to proceed (see bug 3996).
	 * So for now, we force a hardware reset in all cases.
	 */
	if (isr & AR_ISR_RXORN) {
		HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: receive FIFO overrun interrupt\n", __func__);
		*masked |= HAL_INT_RXORN;
	}
	*masked |= mask2;

	if (!pCap->hal_isr_rac_support) {
		/*
		 * EV61133 (missing interrupts due to ISR_RAC):
		 * If not using ISR_RAC, clear the interrupts we've read by writing back ones 
		 * in these locations to the primary ISR (except for interrupts that
		 * have a secondary isr register - see above).
		 */ 
		OS_REG_WRITE(ah, AR_ISR, isr);

		/* Flush prior write */
		(void) OS_REG_READ(ah, AR_ISR);
	}

	return AH_TRUE;
}

HAL_INT
ar5212GetInterrupts(struct ath_hal *ah)
{
	return AH5212(ah)->ah_maskReg;
}

/*
 * Atomically enables NIC interrupts.  Interrupts are passed in
 * via the enumerated bitmask in ints.
 */
HAL_INT
ar5212SetInterrupts(struct ath_hal *ah, HAL_INT ints, HAL_BOOL nortc)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	u_int32_t omask = ahp->ah_maskReg;
	u_int32_t mask,mask2;

    if (nortc)
        return omask;
    
    HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: 0x%x => 0x%x\n", __func__, omask, ints);

	if (omask & HAL_INT_GLOBAL) {
		HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: disable IER\n", __func__);
		OS_REG_WRITE(ah, AR_IER, AR_IER_DISABLE);
		(void) OS_REG_READ(ah, AR_IER);   /* flush write to HW */
	}

	mask = ints & HAL_INT_COMMON;
	mask2 = 0;
	if (ints & HAL_INT_TX) {
		if (ahp->ah_txOkInterruptMask)
			mask |= AR_IMR_TXOK;
		if (ahp->ah_txErrInterruptMask)
			mask |= AR_IMR_TXERR;
		if (ahp->ah_txDescInterruptMask)
			mask |= AR_IMR_TXDESC;
		if (ahp->ah_txEolInterruptMask)
			mask |= AR_IMR_TXEOL;
	}
	if (ints & HAL_INT_RX)
		mask |= AR_IMR_RXOK | AR_IMR_RXERR | AR_IMR_RXDESC;

	if (ints & (HAL_INT_BMISC)) {
		mask |= AR_IMR_BCNMISC;
		if (ints & HAL_INT_TIM)
			mask2 |= AR_IMR_S2_TIM;
		if (ints & HAL_INT_DTIM)
			mask2 |= AR_IMR_S2_DTIM;
		if (ints & HAL_INT_DTIMSYNC)
			mask2 |= AR_IMR_S2_DTIMSYNC;
		if (ints & HAL_INT_CABEND)
			mask2 |= AR_IMR_S2_CABEND;
                if (ints & HAL_INT_TSFOOR) {
                    mask2 |= AR_ISR_S2_TSFOOR;
                }
	}
	if (ints & HAL_INT_FATAL) {
		/*
		 * NB: ar5212Reset sets MCABT+SSERR+DPERR in AR_IMR_S2
		 *     so enabling HIUERR enables delivery.
		 */
		mask |= AR_IMR_HIUERR;
	}

	/* Write the new IMR and store off our SW copy. */
	HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: new IMR 0x%x\n", __func__, mask);
	OS_REG_WRITE(ah, AR_IMR, mask);
    /* For Some processors: It was found with the serial driver that,
       under certain conditions, a spurious CIC interrupt could be triggered
       if the interrupt enable/mask registers weren't updated before the
       CIC mask register.  A read of the register just written to
       enable/disable such interrupts should remedy the problem.  It was
       suggested that the madwifi driver was also succeptible to these
       spurious CIC interrupts, so a similar fix may be needed.
    */
    (void) OS_REG_READ(ah, AR_IMR);


	OS_REG_WRITE(ah, AR_IMR_S2, 
				 (OS_REG_READ(ah, AR_IMR_S2) & 
				  ~(AR_IMR_S2_TIM |
					AR_IMR_S2_DTIM |
					AR_IMR_S2_DTIMSYNC |
					AR_IMR_S2_CABEND |
					AR_IMR_S2_CABTO  |
					AR_IMR_S2_TSFOOR ) ) 
				 | mask2);
	ahp->ah_maskReg = ints;

	/* Re-enable interrupts if they were enabled before. */
	if (ints & HAL_INT_GLOBAL) {
		HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: enable IER\n", __func__);
		OS_REG_WRITE(ah, AR_IER, AR_IER_ENABLE);
        /* See explanation above... */
        (void) OS_REG_READ(ah, AR_IER);
	}

	return omask;
}
#endif /* AH_SUPPORT_AR5212 */
