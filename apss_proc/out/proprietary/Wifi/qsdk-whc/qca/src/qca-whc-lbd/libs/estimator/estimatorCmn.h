// vim: set et sw=4 sts=4 cindent:
/*
 * @File: estimatorCmn.h
 *
 * @Abstract: Functions shared by estimatorBSA and estimatorMBSA
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

#ifndef estimatorCmn__h
#define estimatorCmn__h

#include <dbg.h>
#include <evloop.h>

#include "lbd_types.h"

#include "estimator.h"
#include "estimatorPollutionAccumulator.h"

#if defined(__cplusplus)
extern "C" {
#endif

// ====================================================================
// Protected members (for use within the common functions and any
// "derived" functions that may be using this component).
// ====================================================================

// Currently only stamon needs to know when clients are eligible again
// to have their metrics measured. However, we allow up to two observers
// in case steeralg needs this in the future.
#define MAX_STA_DATA_METRICS_ALLOWED_OBSERVERS 2
#define BSTEERING_MAX_CLIENT_CLASS_GROUP 2

/**
 * @brief Internal state for the rate estimator module.
 */
struct estimatorPriv_t {
    struct dbgModule *dbgModule;

    /// Special logging area for the raw byte count statistics and estimated
    /// throughput / rate for continuous throughput sampling mode.
    /// This is used to make it easier to suppress the logs that would
    /// otherwise fill up the console.
    struct dbgModule *statsDbgModule;

    /// Configuration data obtained at init time
    struct {
        /// Maximum age (in seconds) before some measurement is considered too
        /// old and thus must be re-measured.
        u_int32_t ageLimit;

        /// RSSI difference when estimating RSSI on 5 GHz from
        /// the one measured on 2.4 GHz
        int rssiDiffEstimate5gFrom24g[BSTEERING_MAX_CLIENT_CLASS_GROUP];

        /// RSSI difference when estimating RSSI on 2.4 GHz from
        /// the one measured on 5 GHz
        int rssiDiffEstimate24gFrom5g[BSTEERING_MAX_CLIENT_CLASS_GROUP];

        /// Number of probes required when non-associted band RSSI is valid
        u_int8_t probeCountThreshold[BSTEERING_MAX_CLIENT_CLASS_GROUP];

        /// How frequently to sample the statistics for a node.
        unsigned statsSampleInterval;

        /// Maximum amount of time (in seconds) to allow for a client to
        /// respond to an 802.11k Beacon Report Request before giving up
        /// and declaring a failure.
        u_int8_t max11kResponseTime;

        /// Maximum consecutive 11k failures used for backoff to Legacy Steering
        u_int32_t max11kUnfriendly;

        /// Minimum amount of time (in seconds) between two consecutive 802.11k
        /// requests for a given STA if the first one failed.
        unsigned dot11kProhibitTimeShort[BSTEERING_MAX_CLIENT_CLASS_GROUP];

        /// Minimum amount of time (in seconds) between two consecutive 802.11k
        /// requests for a given STA if the first one succeeded.
        unsigned dot11kProhibitTimeLong[BSTEERING_MAX_CLIENT_CLASS_GROUP];

        /// Percentage factor to apply the PHY rate before deriving the
        /// airtime from the rate and throughput information.
        u_int8_t phyRateScalingForAirtime;

        /// Whether to enable the continous throughput sampling mode
        /// (primarily for demo purposes) or not.
        LBD_BOOL enableContinuousThroughput;

        /// The maximum length of time (in seconds) that a BSS can remain
        /// marked as polluted without any further updates to the pollution
        /// state.
        unsigned maxPollutionTime;

        /// Configuration parameters related to pollution accumulator
        estimatorPollutionAccumulatorParams_t accumulator;

        /// IAS should not be triggered if RSSI is below this threshold
        lbd_rssi_t iasLowRSSIThreshold;

        /// The amount the maximum PHY rate should be scaled by in creating
        /// a cap beyond which the detector curve is not used.
        u_int8_t iasMaxRateFactor;

        /// IAS should not be triggered if byte count increase is below this threshold
        u_int64_t iasMinDeltaBytes;

        /// IAS should not be triggered if packet count increase is below this threshold
        u_int32_t iasMinDeltaPackets;

        /// Whether interference detection should be done on single band
        /// devices or not.
        LBD_BOOL iasEnableSingleBandDetect;

        /// Min interval (in sec) to determine activity status of a STA
        u_int16_t actDetectMinInterval;

        /// Min increase in pkts per second to determine if a STA is active
        u_int32_t actDetectMinPktPerSec;

        /// IAS Enabled or Disabled Status
        LBD_BOOL interferenceDetectionEnable[wlanif_band_invalid];

        /// Max number of steering retries allowed
        u_int8_t apSteerMaxRetryCount;
    } config;

    /// Tracking information for an invocation of
    /// estimator_estimatePerSTAAirtimeOnChannel.
    struct estimatorAirtimeOnChannelState {
        /// The AP on which a measurement is being done
        lbd_apId_t apId;

        /// The channel on which a measurement is being done, or
        /// LBD_CHANNEL_INVALID if one is not in progress.
        lbd_channelId_t channelId;

        /// The number of STAs for which an estimate is still pending.
        size_t numSTAsRemaining;

        /// The number of STAs for which airtime was successfully measured.
        size_t numSTAsSuccess;

        /// The number of STAs for which airtime could not be successfully
        /// measured. This is only for logging/debugging purposes.
        size_t numSTAsFailure;
    } airtimeOnChannelState;

    /// Observer for when a STA becomes eligible to have its data metrics
    /// measured again.
    struct estimatorSTADataMetricsAllowedObserver {
        LBD_BOOL isValid;
        estimator_staDataMetricsAllowedObserverCB callback;
        void *cookie;
    } staDataMetricsAllowedObservers[MAX_STA_DATA_METRICS_ALLOWED_OBSERVERS];

    /// Timer used to periodically sample the byte counter stats for STAs.
    struct evloopTimeout statsSampleTimer;

    /// Timer used to check for STAs that have not responded to an 802.11k
    /// request.
    struct evloopTimeout dot11kTimer;

    /// The time (in seconds) at which to next expire the 802.11k timer.
    struct timespec nextDot11kExpiry;

    /// The number of entries for which an 802.11k timer is running.
    size_t numDot11kTimers;

    /// Whether the debug mode for interference detection is enabled or not.
    LBD_BOOL debugModeEnabled;
};

/**
 * @brief Parameters that are needed when iterating over the BSSes when
 *        writing back the estimated rates and airtime based on an 802.11k
 *        measurement.
 */
typedef struct estimatorNonServingRateAirtimeParams_t {
    /// Whether to consider it a result overall or not.
    LBD_STATUS result;

    /// MAC address of the STA
    const struct ether_addr *staAddr;

    /// Measured BSS information
    const lbd_bssInfo_t *measuredBss;

    /// RCPI reported in 802.11k beacon report
    lbd_rcpi_t rcpi;

    /// Handle to the BSS that was measured.
    stadbEntry_bssStatsHandle_t measuredBSSStats;

    /// Value for the band that was measured (just to avoid re-resolving
    /// the band each time).
    wlanif_band_e measuredBand;

    /// Tx power on the BSS that was measured
    u_int8_t txPower;
} estimatorNonServingRateAirtimeParams_t;

extern struct estimatorPriv_t estimatorState;

typedef enum estimatorMeasurementMode_e {
    /// No measurements are in progress.
    estimatorMeasurementMode_none,

    /// Full measurement, including non-serving metrics.
    estimatorMeasurementMode_full,

    /// Measuring throughput for estimates of airtime on a channel.
    estimatorMeasurementMode_airtimeOnChannel,

    /// Only measuring throughput for continuous sampling mode.
    estimatorMeasurementMode_throughputOnly,

    /// Measuring throughput for a remotely associated STA.
    estimatorMeasurementMode_remoteThroughput,
} estimatorMeasurementMode_e;

typedef enum estimatorThroughputEstimationState_e {
    /// Nothing is being estimated.
    estimatorThroughputState_idle,

    /// Waiting for the first sample to be taken.
    estimatorThroughputState_awaitingFirstSample,

    /// Waiting for the second sample to be taken.
    estimatorThroughputState_awaitingSecondSample,
} estimatorThroughputEstimationState_e;

typedef enum estimator11kState_e {
    /// No 802.11k work is in progress.
    estimator11kState_idle,

    /// Waiting for the STA to send a Beacon Report Response.
    estimator11kState_awaiting11kBeaconReport,

    /// Cannot perform another 802.11k beacon report until a timer
    /// expires (to prevent too frequent measurements).
    estimator11kState_awaiting11kProhibitExpiry,
} estimator11kState_e;

/**
 * @brief State information stored on a per STA basis, for all STAs being
 *        managed by the estimator.
 */
typedef struct estimatorSTAState_t {
    /// The type of measurement currently being undertaken.
    estimatorMeasurementMode_e measurementMode;

    /// The trigger of the current measurement
    steerexec_reason_e trigger;

    /// The stage in the throughput estimation process for this entry.
    estimatorThroughputEstimationState_e throughputState;

    /// The stage in the estimation process for 802.11k measurements.
    estimator11kState_e dot11kState;

    /// The BSS on which stats were enabled.
    lbd_bssInfo_t statsEnabledBSSInfo;

    /// The time at which the last sample was taken.
    struct timespec lastSampleTime;

    /// The statistics for the last sample.
    wlanif_staStatsSnapshot_t lastStatsSnapshot;

    /// Number of packets sent at the beginning of inactivity detection window
    u_int32_t pktsSentBase;

    /// Number of packets received at the beginning of inactivity detection window
    u_int32_t pktsRcvdBase;

    /// The time at which the activity detection started
    struct timespec actDetectStartTime;

    /// Time (in seconds) at which the 802.11k timer for this entry is to
    /// expire.
    struct timespec dot11kTimeout;

    /// Consecutive 11k failure count (since the last RRM success)
    u_int32_t countConsecutive11kFailure;

    /// Flag indicating if the device is active when 802.11k request is sent
    LBD_BOOL activeBefore11k;

    /// Timeout object used to clear pollution state.
    struct evloopTimeout pollutionExpiryTimer;

    /// Handle to the circular buffer accumulating interference
    /// detection flags to make pollution decision
    estimatorCircularBufferHandle_t pollutionAccumulator;

    /// The BSS on which last interference sample was taken
    lbd_bssInfo_t lastIntfDetectBSSInfo;

    /// The statistics for the interference sample
    wlanif_staStats_t lastInterferenceSample;
} estimatorSTAState_t;

// ====================================================================
// Protected functions
// ====================================================================

/**
 * @brief Perform BSA/MBSA specific initialization
 */
void estimatorSubInit(void);

/**
 * @brief Handle 802.11k beacon report and start data rate estimation
 *
 * @pre the beacon report is valid
 *
 * @param [in] handle  the handle of the STA sending the beacon report
 * @param [in] bcnrptEvent  the event containing valid beacon reports
 * @param [in] measuredBand  the band where 802.11k beacon report is measured
 * @param [out] reportedLocalBss  the local BSS reported in the beacon report if any
 *
 * @return LBD_OK if the data rate estimator succeeds; otherwise return LBD_NOK
 */
LBD_STATUS estimatorHandleValidBeaconReport(stadbEntry_handle_t handle,
                                            const wlanif_beaconReportEvent_t *bcnrptEvent,
                                            wlanif_band_e measuredBand,
                                            const lbd_bssInfo_t **reportedLocalBss);

/**
 * @brief Request Downlink RSSI for a STA
 *
 * @param [in] entry  the staDB handle of the STA for which to request
 *                    downlink RSSI
 * @param [in] bssStats  the stats handle of the serving BSS for the
 *                       given STA
 * @param [in] servingBSS  the serving BSS for the given STA
 * @param [in] numChannels  the number of channels in the channel list
 * @param [in] channelList  the channels for which to request downlink
 *                          RSSI on
 *
 * @return LBD_OK if the request succeeds; otherwise return LBD_NOK
 */
LBD_STATUS estimatorRequestDownlinkRSSI(stadbEntry_handle_t entry,
                                        stadbEntry_bssStatsHandle_t bssStats,
                                        const lbd_bssInfo_t *servingBSS,
                                        size_t numChannels,
                                        const lbd_channelId_t *channelList);

// ====================================================================
// Functions internally shared by BSA and MBSA
// ====================================================================

/**
 * @brief Determine if the estimator state indicates the first sample still
 *        needs to be taken.
 *
 * @param [in] state  the state object to check
 *
 * @return LBD_TRUE if the first sample needs to be taken; otherwise LBD_FALSE
 */
static inline LBD_BOOL estimatorCmnStateIsFirstSample(const estimatorSTAState_t *state) {
    return state->throughputState == estimatorThroughputState_awaitingFirstSample ? LBD_TRUE : LBD_FALSE;
}

/**
 * @brief Determine if the estimator state indicates the second sample still
 *        needs to be taken.
 *
 * @param [in] state  the state object to check
 *
 * @return LBD_TRUE if the second sample needs to be taken; otherwise LBD_FALSE
 */
static inline LBD_BOOL estimatorCmnStateIsSecondSample(const estimatorSTAState_t *state) {
    return state->throughputState == estimatorThroughputState_awaitingSecondSample ? LBD_TRUE : LBD_FALSE;;
}

/**
 * @brief Compute the number of microseconds that have elapsed between two
 *        time samples.
 *
 * @note This function assumes the two samples are close enough together
 *       that the computation will not experience an integer overflow.
 *
 * @param [in] start  the beginning timestamp
 * @param [in] end  the ending timestamp
 *
 * @return the elapsed microseconds
 */
u_int32_t estimatorCmnComputeTimeDiff(
        const struct timespec *start, const struct timespec *end);

/**
 * @brief Obtain the estimator state for the STA, creating it if it does not
 *        exist.
 *
 * @param [in] entry  the handle to the STA for which to set the state
 *
 * @return the state entry, or NULL if one could not be created
 */
estimatorSTAState_t *estimatorCmnGetOrCreateSTAState(stadbEntry_handle_t entry);

/**
 * @brief Resolve the minimum PHY capabilities between STA and AP on a given BSS
 *
 * @param [in] handle  the handle to the STA
 * @param [in] addr  the MAC address of the STA
 * @param [in] bssStats  the handle to the BSS
 * @param [in] bssInfo  basic information of the given BSS
 * @param [in] bssCap  PHY capabilities of the AP on the given BSS
 * @param [out] minPhyCap  on success, return the minimum PHY capabilities
 *
 * @return LBD_NOK if no valid capabilities for the STA, otherwise return LBD_OK
 */
LBD_STATUS estimatorCmnResolveMinPhyCap(
        stadbEntry_handle_t handle, const struct ether_addr *addr,
        stadbEntry_bssStatsHandle_t bssStats, const lbd_bssInfo_t *bssInfo,
        const wlanif_phyCapInfo_t *bssCap, wlanif_phyCapInfo_t *minPhyCap);

/**
 * @brief Estimate and store rate and airtime on non-serving BSS
 *
 * @param [in] handle  the handle to the STA
 * @param [in] staAddr  the MAC address of the STA
 * @param [in] measuredBSSStats  the BSS on which the beacon report measurement is done
 * @param [in] nonServingBSSStats  the BSS to estimate rate and airtime
 * @param [in] targetBSSInfo  the info of target BSS
 * @param [in] targetPHYCap  the PHY capability on target BSS
 * @param [in] measuredRCPI  the RCPI valid reported in beacon report
 * @param [in] measuredBSSTxPower  the Tx power on BSS where beacon report
 *                                 measurement is done
 *
 * @return LBD_OK if the estimation and store succeeds, otherwise return LBD_NOK
 */
LBD_STATUS estimatorCmnEstimateNonServingRateAirtime(
        stadbEntry_handle_t handle, const struct ether_addr *staAddr,
        stadbEntry_bssStatsHandle_t measuredBSSStats,
        stadbEntry_bssStatsHandle_t nonServingBSSStats,
        const lbd_bssInfo_t *targetBSSInfo,
        const wlanif_phyCapInfo_t *targetPHYCap,
        lbd_rcpi_t measuredRCPI, u_int8_t measuredBSSTxPower);

/**
 * @brief Handle a beacon report containing info of local BSS
 *
 * @param [in] handle  the handle to the STA
 * @param [in] staAddr  the MAC address of the STA
 * @param [in] reportedLocalBss  the BSS reported in beacon report
 * @param [inout] params  @see estimatorNonServingRateAirtimeParams_t for details
 */
void estimatorCmnHandleLocalBeaconReport(
        stadbEntry_handle_t entry, const struct ether_addr *staAddr,
        const lbd_bssInfo_t *reportedLocalBss,
        estimatorNonServingRateAirtimeParams_t *params);

/**
 * @brief Send the log indicating that a BSS's polluted state changed
 *        for a given STA.
 *
 * @param [in] addr  the MAC address of the STA
 * @param [in] bssInfo  the BSS that pollution state changed
 * @param [in] polluted  new pollution state
 * @param [in] reasonCode  the reason why pollution state changed
 */
void estimatorCmnDiaglogSTAPollutionChanged(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        LBD_BOOL polluted, estimatorPollutionChangedReason_e reasonCode);


/**
 * @brief Start the pollution timer running if it is not running, or reschedule
 *        it if it currently running but scheduled to expire after the given
 *        time.
 *
 * @param [in] staAddr  the STA for which the timer is being started
 * @param [in] entry  the handle for this entry
 * @param [in] timerSecs  the duration of the timer (in seconds)
 */
void estimatorCmnStartPollutionTimer(
        const struct ether_addr *staAddr, stadbEntry_handle_t entry,
        unsigned timerSecs);

/**
 * @brief Generate an event indicating pollution state being cleared on the given STA
 *
 * @param [in] staAddr  MAC address of the given STA
 */
void estimatorCmnGeneratePollutionClearEvent(const struct ether_addr *staAddr);

/**
 * @brief Transition the STA stats sampling to the next state.
 *
 * If a failure occurred, it will go back to the idle state. Otherwise, it will
 * proceed to performing an 802.11k measurement.
 *
 * @param [in] entry  the handle for ths STA which the stats sample
 *                    is for
 * @param [in] addr  the address of the entry which the stats sample
 *                   is for
 * @param [in] state  the internal state tracking this entry
 * @param [in] sampleTime  the time at which the stats were sampled
 * @param [in] stats  the last stats snapshot
 * @param [in] isFailure  whether the completion is due to a failure
 */
void estimatorCmnCompleteSTAStatsSample(stadbEntry_handle_t entry,
                                        const struct ether_addr *addr,
                                        estimatorSTAState_t *state,
                                        const struct timespec *sampleTime,
                                        const wlanif_staStatsSnapshot_t *stats,
                                        LBD_BOOL isFailure);

/**
 * @brief Determine whether the STA is being served by this AP.
 *
 * @param [in] entry  the STA for which to perform the check
 * @param [out] servingBSS  the stats object for the serving BSS; this will
 *                          be NULL if the serving AP is not this one
 * @param [out] servingBSSInfo  the identify info for the serving BSS; this
 *                              will be NULL if the serving AP is not this one
 *
 * @return LBD_TRUE if the serving AP is serving this STA; otherwise LBD_FALSE
 */
LBD_BOOL estimatorCmnIsSelfServing(stadbEntry_handle_t entry,
                                   stadbEntry_bssStatsHandle_t *servingBSS,
                                   const lbd_bssInfo_t **servingBSSInfo);

/**
 * @brief Determine whether the STA is being served by the given AP.
 *
 * @param [in] entry  the STA for which to perform the check
 * @param [in] apId  the AP Id to check against
 * @param [out] servingBSS  the stats object for the serving BSS; this will
 *                          be NULL if the serving AP is not this one
 * @param [out] servingBSSInfo  the identify info for the serving BSS; this
 *                              will be NULL if the serving AP is not this one
 *
 * @return LBD_TRUE if the given AP is serving this STA; otherwise LBD_FALSE
 */
LBD_BOOL estimatorCmnIsSameServingAP(stadbEntry_handle_t entry, lbd_apId_t apId,
                                     stadbEntry_bssStatsHandle_t *servingBSS,
                                     const lbd_bssInfo_t **servingBSSInfo);

/**
 * @brief Determnine if estimation can be performed for STA associated with a remote AP
 *
 * @return LBD_TRUE if remote estimation is allowed. Otherwise return LBD_FALSE
 */
LBD_BOOL estimatorIsRemoteEstimationAllowed(void);

#if defined(__cplusplus)
}
#endif

#endif // estimatorCmn__h
