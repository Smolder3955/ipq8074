/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef _WLAN_OFFCHAN_TXRX_API_H_
#define _WLAN_OFFCHAN_TXRX_API_H_

/**
 * DOC:  wlan_offchan_txrx_api.h
 *
 * offchan tx/rx layer public API and structures
 * for umac converged components.
 *
 */

#include "wlan_objmgr_cmn.h"
#include "qdf_nbuf.h"
#include "qdf_atomic.h"

#define offchan_log(level, args...) \
		QDF_TRACE(QDF_MODULE_ID_OFFCHAN_TXRX, level, ## args)
#define offchan_logfl(level, format, args...) \
		offchan_log(level, FL(format), ## args)
#define offchan_alert(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define offchan_err(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define offchan_warn(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define offchan_notice(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define offchan_info(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_INFO_HIGH, format, ## args)
#define offchan_debug(format, args...) \
		offchan_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

/* Max Number of off chan frames in one request */
#define OFFCHAN_TX_MAX 20

/**
 * enum offchan_status - Offchan tx status enumeration
 * @OFFCHAN_TX_STATUS_NONE: Default
 * @OFFCHAN_TX_STATUS_SENT: Sent frames for TX
 * @OFFCHAN_TX_STATUS_DROPPED: Dropped frame due to dwell time complete
 */
enum offchan_status {
	OFFCHAN_TX_STATUS_NONE,
	OFFCHAN_TX_STATUS_SUCCESS,
	OFFCHAN_TX_STATUS_SENT,
	OFFCHAN_TX_STATUS_ERROR,
	OFFCHAN_TX_STATUS_DROPPED,
	OFFCHAN_TX_STATUS_XRETRY,
	OFFCHAN_TX_STATUS_TIMEOUT,
};

/**
 * struct offchan_tx_status - Off chan tx status
 * @count: Number of valid status
 * @id: id
 * @offchan_txcomp_list: nbuf of the offchantx completed packet
 * @status: enum offchan_status type to indicate the status
 */
struct offchan_tx_status {
	uint8_t count;
	uint8_t id[OFFCHAN_TX_MAX];
	qdf_nbuf_queue_t offchan_txcomp_list;
	enum offchan_status status[OFFCHAN_TX_MAX];
};

/**
 * struct offchan_stats - Off Chan Stats
 * @dwell_time: Actual dwell time in microsec spent on foreign channel
 * @chanswitch_time_htof: Home to foreign channel switch time in microsec
 * @chanswitch_time_ftoh: Foreign to home channel switch time in microsec
 */
struct offchan_stats {
	uint32_t dwell_time;
	uint32_t chanswitch_time_htof;
	uint32_t chanswitch_time_ftoh;
};

/**
 * struct offchan_evt_tstamps - Off Chan event time stamps received from FW
 * @scan_start - Scan start timestamp
 * @scan_completed - Scan complete timestamp
 * @scan_bss_entry - BSS channel entry timestamp
 * @scan_foreign_entry - Foreign channel entry timestamp
 * @scan_foreign_exit - Foreign channel exit timestamp
 */
struct offchan_evt_tstamp {
	uint32_t scan_start;
	uint32_t scan_completed;
	uint32_t scan_bss_entry;
	uint32_t scan_foreign_entry;
	uint32_t scan_foreign_exit;
};

/* offchan tx_complete callback typedef */
typedef void (*offchan_tx_complete)(struct wlan_objmgr_vdev *vdev,
		struct offchan_tx_status *status,
		struct offchan_stats *stats);

/* offchan rx indication callback typedef */
typedef void (*offchan_rx_ind)(struct wlan_objmgr_vdev *vdev);

struct wide_band_scan {
	uint16_t chan_no;
	uint8_t bw_mode;
	uint8_t sec_chan_offset;
};

/**
 * struct offchan_tx_req: Offchan tx request structure
 * @chan: Off Channel number to tx data
 * @dwell_time: Dwell time for off-chan
 * @bw_mode: bandwidth mode 20MHz,40MHz,80MHz -scan
 * @offchan_rx: enable offchan_rx
 * @complete_dwell_tx: Do not truncate dwell on receving tx completions for
 *              all offchan TX frames. Complete dwell period requested.
 * @dequeue_rate: Number of frames to send in one tx oppurtunity
 * @req_nbuf_ontx_comp: nbuf is needed by the requestor,
 *                      if set true, nbuf needs to given back along with status
 *                      if set false, nbuf can be cleared in offchan completion
 * @offchan_tx_list: Nbuf list send in off-chan
 * @offchan_tx_complete tx_comp: Callback after completion of offchan tx
 * @offchan_rx_ind rx_ind: Callback to collect stats in offchan Rx
 */
struct offchan_tx_req {
	uint32_t chan;
	uint32_t dwell_time;
	struct wide_band_scan wide_scan;
	bool offchan_rx;
	bool complete_dwell_tx;
	uint8_t dequeue_rate;
	bool req_nbuf_ontx_comp;
	qdf_nbuf_queue_t offchan_tx_list;
	offchan_tx_complete tx_comp;
	offchan_rx_ind rx_ind;
};

/**
 * ucfg_offchan_txrx_request() - API to request offchan tx
 * @vdev: Pointer to vdev object
 * @req: pointer to offchan_tx_req request
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS ucfg_offchan_tx_request(
		struct wlan_objmgr_vdev *vdev,
		struct offchan_tx_req *req);

/**
 * ucfg_offchan_tx_cancel() - API to cancel current offchan tx
 * @vdev: Pointer to vdev object
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS ucfg_offchan_tx_cancel(struct wlan_objmgr_vdev *vdev);
/**
 * ucfg_scan_from_offchan_requestor() - API to check if scan requested by
 *                                      offchan module.
 * @vdev: Pointer vdev on which scan started
 * @event: pointer to scan event to check
 *
 * Return: true for offchan scan. false otherise.
 */
bool ucfg_scan_from_offchan_requestor(struct wlan_objmgr_vdev *vdev,
		struct scan_event *event);

/**
 * wlan_offchan_txrx_init() - Initialize offchan object
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_init(void);

/**
 * wlan_offchan_txrx_deinit() - cleanup offchan object
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_deinit(void);

#endif
