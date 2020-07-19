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
#include "ar5212/ar5212desc.h"

/*
 * Initializes all of the hardware registers used to
 * send beacons.  Note that for station operation the
 * driver calls ar5212SetStaBeaconTimers instead.
 */
void
ar5212BeaconInit(struct ath_hal *ah,
    u_int32_t next_beacon, u_int32_t beacon_period, 
    u_int32_t beacon_period_fraction,HAL_OPMODE opmode)
{
    struct ath_hal_5212     *ahp = AH5212(ah);
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    u_int32_t beacon_reg_val;

    /* 
     * TIMER1: in AP/adhoc mode this controls the DMA beacon
     * alert timer; otherwise it controls the next wakeup time.
     * TIMER2: in AP mode, it controls the SBA beacon alert
     * interrupt; otherwise it sets the start of the next CFP.
     */
    switch (opmode) {
    case HAL_M_STA:
    case HAL_M_MONITOR:
    case HAL_M_RAW_ADC_CAPTURE:
        OS_REG_WRITE(ah, AR_TIMER0, ONE_EIGHTH_TU_TO_TU(next_beacon));
        OS_REG_WRITE(ah, AR_TIMER1, 0xffff);
        OS_REG_WRITE(ah, AR_TIMER2, 0x7ffff);
        break;
    case HAL_M_IBSS:
        OS_REG_SET_BIT(ah, AR_TXCFG, AR_TXCFG_ADHOC_BEACON_ATIM_TX_POLICY);
    case HAL_M_HOSTAP:
        if (AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP) {
            OS_REG_WRITE(ah, AR_TIMER0, 0);
        } else {
            OS_REG_WRITE(ah, AR_TIMER0, ONE_EIGHTH_TU_TO_TU(next_beacon));
        }
        /* Convert next_beacon from eighths of time-units to microseconds,
           then subtract ath_hal_dma_beacon_response_time
           (or sw_beacon_response_time), then convert from microseconds
           to eighths of time-units.
         */
        OS_REG_WRITE(ah, AR_TIMER1,
            USEC_TO_ONE_EIGHTH_TU(ONE_EIGHTH_TU_TO_USEC(next_beacon) -
            ap->ah_config.ath_hal_dma_beacon_response_time));
        OS_REG_WRITE(ah, AR_TIMER2, 
            USEC_TO_ONE_EIGHTH_TU(ONE_EIGHTH_TU_TO_USEC(next_beacon) -
            ap->ah_config.ath_hal_sw_beacon_response_time));
        break;
    }
    /*
     * Set the ATIM window 
     * Our hardware does not support an ATIM window of 0
     * (beacons will not work).  If the ATIM windows is 0,
     * force it to 1.
     */
    OS_REG_WRITE(ah, AR_TIMER3, ONE_EIGHTH_TU_TO_TU(next_beacon) +
        (ahp->ah_atimWindow ? ahp->ah_atimWindow : 1));

    /*
     * Set the Beacon register after setting all timers.
     */
    beacon_period &= AR_BEACON_PERIOD | AR_BEACON_RESET_TSF | AR_BEACON_EN;
    if (beacon_period & AR_BEACON_RESET_TSF) {
        HAL_CHANNEL hchan;
        ar5212RadarWait(ah,&hchan);
        AH_PRIVATE(ah)->ah_curchan->ah_tsf_last = 0;

        /*
         * workaround for hw bug! when resetting the TSF,
         * write twice to the corresponding register; each
         * write to the RESET_TSF bit toggles the internal
         * signal to cause a reset of the TSF - but if the signal
         * is left high, it will reset the TSF on the next
         * chip reset also! writing the bit an even number
         * of times fixes this issue
         */
        OS_REG_WRITE(ah, AR_BEACON, AR_BEACON_RESET_TSF);
    }
    beacon_reg_val = ONE_EIGHTH_TU_TO_TU(beacon_period & HAL_BEACON_PERIOD_TU8);
    if ( beacon_period & HAL_BEACON_ENA ) beacon_reg_val |= AR_BEACON_EN;
    OS_REG_WRITE(ah, AR_BEACON, beacon_reg_val);
}

/*
 * Set all the beacon related bits on the h/w for stations
 * i.e. initializes the corresponding h/w timers;
 * also tells the h/w whether to anticipate PCF beacons
 */
void
ar5212SetStaBeaconTimers(struct ath_hal *ah, const HAL_BEACON_STATE *bs)
{
    u_int32_t nextTbtt, nextdtim,beaconintval, dtimperiod;

    HALASSERT(bs->bs_intval != 0);
    /* if the AP will do PCF */
    if (bs->bs_cfpmaxduration != 0) {
        /* tell the h/w that the associated AP is PCF capable */
        OS_REG_WRITE(ah, AR_STA_ID1,
            OS_REG_READ(ah, AR_STA_ID1) | AR_STA_ID1_PCF);

        /* set CFP_PERIOD(1.024ms) register */
        OS_REG_WRITE(ah, AR_CFP_PERIOD, bs->bs_cfpperiod);

        /* set CFP_DUR(1.024ms) register to max cfp duration */
        OS_REG_WRITE(ah, AR_CFP_DUR, bs->bs_cfpmaxduration);

        /* set TIMER2(128us) to anticipated time of next CFP */
        OS_REG_WRITE(ah, AR_TIMER2, bs->bs_cfpnext << 3);
    } else {
        /* tell the h/w that the associated AP is not PCF capable */
        OS_REG_WRITE(ah, AR_STA_ID1,
            OS_REG_READ(ah, AR_STA_ID1) &~ AR_STA_ID1_PCF);
    }

    /*
     * Set TIMER0(1.024ms) to the anticipated time of the next beacon.
     */
    OS_REG_WRITE(ah, AR_TIMER0, bs->bs_nexttbtt);

    /*
     * Start the beacon timers by setting the BEACON register
     * to the beacon interval; also write the tim offset which
     * we should know by now.  The code, in ar5211WriteAssocid,
     * also sets the tim offset once the AID is known which can
     * be left as such for now.
     */
    OS_REG_WRITE(ah, AR_BEACON, 
        (OS_REG_READ(ah, AR_BEACON) &~ (AR_BEACON_PERIOD|AR_BEACON_TIM))
        | SM(bs->bs_intval, AR_BEACON_PERIOD)
        | SM(bs->bs_timoffset ? bs->bs_timoffset + 4 : 0, AR_BEACON_TIM)
    );

    /*
     * Configure the BMISS interrupt.  Note that we
     * assume the caller blocks interrupts while enabling
     * the threshold.
     */
    HALASSERT(bs->bs_bmissthreshold <=
        (AR_RSSI_THR_BM_THR >> AR_RSSI_THR_BM_THR_S));
    OS_REG_RMW_FIELD(ah, AR_RSSI_THR,
        AR_RSSI_THR_BM_THR, bs->bs_bmissthreshold);
    OS_REG_WRITE(ah, AR_ISR, HAL_INT_BMISS); /* clear any pending BMISS interrupt so far */

    /*
     * Program the sleep registers to correlate with the beacon setup.
     */

    /*
     * Oahu beacons timers on the station were used for power
     * save operation (waking up in anticipation of a beacon)
     * and any CFP function; Venice does sleep/power-save timers
     * differently - so this is the right place to set them up;
     * don't think the beacon timers are used by venice sta hw
     * for any useful purpose anymore
     * Setup venice's sleep related timers
     * Current implementation assumes sw processing of beacons -
     *   assuming an interrupt is generated every beacon which
     *   causes the hardware to become awake until the sw tells
     *   it to go to sleep again; beacon timeout is to allow for
     *   beacon jitter; cab timeout is max time to wait for cab
     *   after seeing the last DTIM or MORE CAB bit
     */
#define CAB_TIMEOUT_VAL     10 /* in TU */
#define BEACON_TIMEOUT_VAL  10 /* in TU */
#define SLEEP_SLOP          3  /* in TU */

    /*
     * For max powersave mode we may want to sleep for longer than a
     * beacon period and not want to receive all beacons; modify the
     * timers accordingly; make sure to align the next TIM to the
     * next DTIM if we decide to wake for DTIMs only
     */
    beaconintval = bs->bs_intval & HAL_BEACON_PERIOD;
    HALASSERT(beaconintval != 0);
    if (bs->bs_sleepduration > beaconintval) {
        HALASSERT(roundup(bs->bs_sleepduration, beaconintval) ==
                bs->bs_sleepduration);
        beaconintval = bs->bs_sleepduration;
    }
    dtimperiod = bs->bs_dtimperiod;
    if (bs->bs_sleepduration > dtimperiod) {
        HALASSERT(dtimperiod == 0 ||
            roundup(bs->bs_sleepduration, dtimperiod) ==
                bs->bs_sleepduration);
        dtimperiod = bs->bs_sleepduration;
    }
    HALASSERT(beaconintval <= dtimperiod);
    if (beaconintval == dtimperiod)
        nextTbtt = bs->bs_nextdtim;
    else
        nextTbtt = bs->bs_nexttbtt;
    nextdtim = bs->bs_nextdtim;

    OS_REG_WRITE(ah, AR_SLEEP1,
          SM((nextdtim - SLEEP_SLOP) << 3, AR_SLEEP1_NEXT_DTIM)
        | SM(CAB_TIMEOUT_VAL, AR_SLEEP1_CAB_TIMEOUT)
        | AR_SLEEP1_ASSUME_DTIM
        | AR_SLEEP1_ENH_SLEEP_ENA
    );
    OS_REG_WRITE(ah, AR_SLEEP2,
          SM((nextTbtt - SLEEP_SLOP) << 3, AR_SLEEP2_NEXT_TIM)
        | SM(BEACON_TIMEOUT_VAL, AR_SLEEP2_BEACON_TIMEOUT)
    );
    OS_REG_WRITE(ah, AR_SLEEP3,
          SM(beaconintval, AR_SLEEP3_TIM_PERIOD)
        | SM(dtimperiod, AR_SLEEP3_DTIM_PERIOD)
    );

    /* TSF out of range threshold */
    OS_REG_WRITE(ah, AR_TSFOOR_THRESHOLD, bs->bs_tsfoor_threshold);
 
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: next DTIM %d\n", __func__, bs->bs_nextdtim);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: next beacon %d\n", __func__, nextTbtt);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: beacon period %d\n", __func__, beaconintval);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: DTIM period %d\n", __func__, dtimperiod);
#undef CAB_TIMEOUT_VAL
#undef BEACON_TIMEOUT_VAL
#undef SLEEP_SLOP
}
#endif /* AH_SUPPORT_AR5212 */
