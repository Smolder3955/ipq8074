/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _ATH_AR5212_RADAR_H_
#define _ATH_AR5212_RADAR_H_

#define	HAL_RADAR_SMASK		0x0000FFFF	/* Sequence number mask */
#define	HAL_RADAR_SSHIFT	16		/* Shift for Reader seq # stored in upper
						   16 bits, writer's is lower 16 bits */
#define	HAL_RADAR_IMASK		0x0000FFFF	/* Index number mask */
#define	HAL_RADAR_ISHIFT	16		/* Shift for index stored in upper 16 bits
						   of reader reset value */
#define HAL_RADAR_FIRPWR	-45
#define HAL_RADAR_RRSSI		14
#define HAL_RADAR_HEIGHT	20
#define HAL_RADAR_PRSSI		24
#define HAL_RADAR_INBAND	6

#define HAL_RADAR_TSMASK	0x7FFF		/* Mask for time stamp from descriptor */
#define	HAL_RADAR_TSSHIFT	15		/* Shift for time stamp from descriptor */
#define	HAL_RADAR_TSF_WRAP	0xFFFFFFFFFFFFFFFFULL	/* 64 bit TSF wrap value */
#define	HAL_RADAR_64BIT_TSFMASK	0x0000000000007FFFULL	/* TS mask for 64 bit value */


#define	HAL_AR_RADAR_RSSI_THR		5	/* in dB */
#define	HAL_AR_RADAR_RESET_INT		1	/* in secs */
#define	HAL_AR_RADAR_MAX_HISTORY	500
#define	HAL_AR_REGION_WIDTH		128
#define	HAL_AR_RSSI_THRESH_STRONG_PKTS	17	/* in dB */
#define	HAL_AR_RSSI_DOUBLE_THRESHOLD	15	/* in dB */
#define	HAL_AR_MAX_NUM_ACK_REGIONS	9
#define	HAL_AR_ACK_DETECT_PAR_THRESH	20
#define	HAL_AR_PKT_COUNT_THRESH		20

#define	HAL_MAX_DL_SIZE			1024
#define	HAL_MAX_DL_MASK			0x3FF

#define HAL_RADAR_NOL_TIME		30*60*1000000	/* 30 minutes in usecs */

#define HAL_RADAR_WAIT_TIME		60*1000000	/* 1 minute in usecs */

#define	HAL_RADAR_DISABLE_TIME		3*60*1000000	/* 3 minutes in usecs */

#define	HAL_RADAR_PARAM_FIRPWR		1
#define	HAL_RADAR_PARAM_RRSSI		2
#define	HAL_RADAR_PARAM_HEIGHT		3
#define	HAL_RADAR_PARAM_PRSSI		4
#define	HAL_RADAR_PARAM_INBAND		5
#define	HAL_RADAR_PARAM_NOL		6

#define	HAL_MAX_B5_SIZE			128
#define	HAL_MAX_B5_MASK			0x0000007F	/* 128 */

#define	HAL_MAX_RADAR_OVERLAP		6		/* Max number of overlapping filters */

struct ar5212RadarPulse {
	u_int32_t	rp_numPulses;	/* Num of pulses in radar burst */
	u_int32_t	rp_pulseDur;	/* Duration of each pulse in usecs */
	u_int32_t	rp_pulseFreq;	/* Frequency of pulses in burst */
	u_int32_t	rp_pulseVar;	/* Time variation of pulse duration for
					   matched filter (single-sided) in usecs */
	u_int32_t	rp_threshold;	/* Thershold for MF output to indicate 
					   radar match */
	u_int32_t	rp_minDur;	/* Min pulse duration to be considered for
					   this pulse type */
	u_int32_t	rp_maxDur;	/* Max pusle duration to be considered for
					   this pulse type */
	u_int32_t	rp_rssiThresh;	/* Minimum rssi to be considered a radar pulse */
	u_int32_t	rp_meanOffset;	/* Offset for timing adjustment */
	u_int32_t	rp_pulseId;	/* Unique ID for identifying filter */

};

struct ar5212RadReader {
	u_int16_t	rd_index;
	u_int16_t	rd_expSeq;
	u_int32_t	rd_resetVal;
	u_int8_t	rd_start;
};

struct ar5212RadWriter {
	u_int16_t	wr_index;
	u_int16_t	wr_seq;
};

struct ar5212RadarEvent {
	u_int16_t	re_ts;		/* Original 15 bit recv timestamp */
	u_int64_t	re_fullts;	/* 64-bit full timestamp from interrupt time */
	u_int8_t	re_rssi;	/* rssi of radar event */
	u_int8_t	re_dur;		/* duration of radar pulse */
	u_int8_t	re_chanIndex;	/* Channel of event */
};

struct ar5212RadarQElem {
	u_int32_t	rq_seqNum;
	u_int32_t	rq_busy;		/* 32 bit to insure atomic read/write */
	struct ar5212RadarEvent rq_event;	/* Radar event */
};

struct ar5212RadarQInfo {
	u_int16_t	ri_qsize;		/* q size */
	u_int16_t	ri_seqSize;		/* Size of sequence ring */
	struct ar5212RadReader ri_reader;	/* State for the q reader */
	struct ar5212RadWriter ri_writer;	/* state for the q writer */
	u_int32_t	ri_numRead;		/* Number of events read from q */
	u_int32_t	ri_numWrite;		/* Number of events written to q */
};

#define HAL_MAX_ACK_RADAR_DUR	511
#define HAL_MAX_NUM_PEAKS	3
#define HAL_ARQ_SIZE		2048		/* 8K AR events for buffer size */
#define HAL_ARQ_SEQSIZE		2049		/* Sequence counter wrap for AR */
#define HAL_RADARQ_SIZE		512		/* 1K radar events for buffer size */
#define HAL_RADARQ_SEQSIZE	513		/* Sequence counter wrap for radar */
#define HAL_NUM_RADAR_STATES	64		/* Number of radar channels we keep state for */
#define HAL_MAX_NUM_RADAR_FILTERS 20		/* Max number radar filters for each type */ 
#define HAL_MAX_RADAR_TYPES	20		/* Number of different radar types */

struct ar5212ArState {
	u_int16_t	ar_prevTimeStamp;
	u_int32_t	ar_prevWidth;
	u_int32_t	ar_phyErrCount[HAL_MAX_ACK_RADAR_DUR];
	u_int32_t	ar_ackSum;
	u_int16_t	ar_peakList[HAL_MAX_NUM_PEAKS];
	u_int32_t	ar_packetThreshold;	/* Thresh to determine traffic load */
	u_int32_t	ar_parThreshold;	/* Thresh to determine peak */
	u_int32_t	ar_radarRssi;		/* Rssi threshold for AR event */
};

struct ar5212RadarDelayElem {
	u_int32_t	de_time;		/* Current "filter" time for start of pulse in usecs*/
	u_int32_t	de_dur;			/* Duration of pulse in usecs*/
};

/* NB: The first element in the circular buffer is the oldest element */

struct ar5212RadarDelayLine {
	struct ar5212RadarDelayElem dl_elems[HAL_MAX_DL_SIZE];	/* Array of pulses in delay line */
	u_int64_t	dl_lastTs;		/* Last timestamp the delay line was used (in usecs) */
	u_int32_t	dl_firstElem;		/* Index of the first element */
	u_int32_t	dl_lastElem;		/* Index of the last element */
	u_int32_t	dl_numElems;		/* Number of elements in the delay line */
};

struct ar5212RadarFilter {
	struct ar5212RadarDelayElem *rf_pulses;	/* Pulse list for radar filter */
	u_int32_t	rf_numPulses;		/* Number of pulses in the filter */
	u_int32_t	rf_filterLen;		/* Length (in usecs) of the filter */
	u_int32_t	rf_threshold;		/* match filter output threshold for radar detect */
	u_int32_t	rf_pulseDur;		/* Pulse duration to match to */
	u_int32_t	rf_pulseId;		/* Unique ID corresponding to the original filter ID */
	u_int32_t	rf_minDur;		/* Min duration for this radar filter */
	u_int32_t	rf_maxDur;		/* Max duration for this radar filter */
};

struct ar5212RadarFilterType {
	struct ar5212RadarFilter ft_filters[HAL_MAX_NUM_RADAR_FILTERS];	/* array of filters */
	u_int32_t	ft_filterDur;		/* Duration of pulse which specifies filter type */
	u_int32_t	ft_numFilters;		/* Num filters of this type */
	u_int32_t	ft_maxFilterLen;	/* Max length of all filters */
	u_int32_t	ft_minDur;		/* min pulse duration to be considered
						   for this filter type */
	u_int32_t	ft_maxDur;		/* max pulse duration to be consdiered
						   for this filter type */
	u_int32_t	ft_rssiThresh;		/* min rssi to be considered for this filter type */
	u_int32_t	ft_numPulses;		/* Num pulses in each filter of this type */
	struct ar5212RadarDelayLine ft_dl;	/* Delay line of pulses on current channel for
						   this filter type */
};

struct ar5212RadarState {
	HAL_CHANNEL_INTERNAL *rs_chan;		/* Channel info */
	u_int8_t	rs_chanIndex;		/* Channel index in radar structure */
	u_int32_t	rs_numRadarEvents;	/* Number of radar events */
	int32_t		rs_firpwr;		/* Thresh to check radar sig is gone */ 
	u_int32_t	rs_radarRssi;		/* Thresh to start radar det (dB) */
	u_int32_t	rs_height;		/* Thresh for pulse height (dB)*/
	u_int32_t	rs_pulseRssi;		/* Thresh to check if pulse is gone (dB) */
	u_int32_t	rs_inband;		/* Thresh to check if pulse is inband (0.5 dB) */
};

struct ar5212RadarNolElem {
	HAL_CHANNEL_INTERNAL *nol_chan;		/* Channel info */
	u_int64_t	nol_tsfFree;		/* 64 bit tsf when channel can be used again */
	struct ar5212RadarNolElem *nol_next;	/* next element pointer */
};

struct ar5212RadarInfo {
	HAL_BOOL	rn_useNol;		/* Use the NOL when radar found (default: TRUE) */
	u_int32_t	rn_numRadars;		/* Number of different types of radars */
	u_int64_t	rn_lastFullTs;		/* Last 64 bit timstamp from recv interrupt */
	u_int16_t	rn_lastTs;		/* last 15 bit ts from recv descriptor */
	u_int64_t	rn_tsPrefix;		/* Prefix to prepend to 15 bit recv ts */
	u_int32_t	rn_numBin5Radars;	/* Number of bin5 radar pulses to search for */
	u_int32_t	rn_fastDivGCVal;	/* Value of fast diversity gc limit from init file */
};

struct ar5212Bin5Pulses {
	u_int32_t	b5_threshold;		/* Number of bin5 pulses to indicate detection */
	u_int32_t	b5_minDur;		/* Min duration for a bin5 pulse */
	u_int32_t	b5_maxDur;		/* Max duration for a bin5 pulse */
	u_int32_t	b5_timeWindow;		/* Window over which to count bin5 pulses */
	u_int32_t	b5_rssiThresh;		/* Min rssi to be considered a pulse */
};

struct ar5212Bin5Elem {
	u_int64_t	be_ts;			/* Timestamp for the bin5 element */
	u_int32_t	be_rssi;		/* Rssi for the bin5 element */
	u_int32_t	be_dur;			/* Duration of bin5 element */
};

struct ar5212Bin5Radars {
	struct ar5212Bin5Elem br_elems[HAL_MAX_B5_SIZE];	/* List of bin5 elems that fall
								 * within the time window */
	u_int32_t	br_firstElem;		/* Index of the first element */
	u_int32_t	br_lastElem;		/* Index of the last element */
	u_int32_t	br_numElems;		/* Number of elements in the delay line */
	struct ar5212Bin5Pulses br_pulse;	/* Original info about bin5 pulse */
};

#endif  /* _ATH_AR5212_RADAR_H_ */
