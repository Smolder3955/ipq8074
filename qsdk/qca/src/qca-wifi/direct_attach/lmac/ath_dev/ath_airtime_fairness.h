/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

#ifndef _IF_ATH_AIRTIME_FAIRNESS_H
#define _IF_ATH_AIRTIME_FAIRNESS_H

#include "ath_timer.h"

#define ATF_ISSET_TID_BITMAP(an, tid)        (an->an_atf_nodepaused & (1 << tid))
#define ATF_SET_TID_BITMAP(an, tid)          (an->an_atf_nodepaused |= (1 << tid))
#define ATF_RESET_TID_BITMAP(an, tid)        (an->an_atf_nodepaused &= ~(1 << tid))
/**
 * enum ATH_ATF_BUFFER_ALLOC_STATUS - ATF Buffer Allocation Status
 * @ATH_ATF_BUF_ALLOC: Alloc Buffer
 * @ATH_ATF_BUF_NOALLOC: Do not allocate buffer
 * @ATH_ATF_BUF_ALLOC_SKIP: Allocate buffer, but skip ATF buf accounting
 *
 * This is being used internally to manage ATF Buffer Allocation.
 */
typedef enum {
	ATH_ATF_BUF_ALLOC,
	ATH_ATF_BUF_NOALLOC,
	ATH_ATF_BUF_ALLOC_SKIP
}ATH_ATF_BUFFER_ALLOC_STATUS;

void ath_atf_sc_node_unblock(struct ath_softc *sc, struct ath_node *an);
void ath_atf_sc_tokens_unassigned(struct ath_softc *scn, u_int32_t airtime_unassigned);
int
ath_atf_check_txtokens(struct ath_softc *sc, struct ath_buf *bf, ath_atx_tid_t *tid);
int
ath_atf_node_airtime_consumed( struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts, int txok);

void ath_atf_sc_set_clear(struct ath_softc *sc, u_int32_t enable_disable);
void ath_atf_sc_node_resume(struct ath_node *an);
void ath_atf_sc_capable_node(struct ath_node *an, u_int8_t val, u_int8_t atfstate_change);
u_int8_t ath_atf_all_tokens_used(struct ath_node *an);
u_int32_t ath_calc_ack_duration(struct ath_softc *sc, u_int8_t cix);
u_int32_t ath_get_retries_num(struct ath_buf *bf, u_int32_t rateindex, u_int32_t retries);
u_int32_t ath_atf_debug_node_state(struct ath_node *an, struct atf_peerstate *peerstate);
int ath_atf_bypass_frame(ath_atx_tid_t *tid, struct ath_buf *bf);
QDF_STATUS ath_atf_account_tokens(ath_atx_tid_t *tid, uint32_t pkt_duration);
QDF_STATUS ath_atf_realign_tokens(struct ath_node *an, uint8_t tidno,
                                  uint32_t acutal_duration,
                                  uint32_t est_duration);
#endif //_IF_ATH_AIRTIME_FAIRNESS_H

