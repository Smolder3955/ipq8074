/*
 * Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _OL_RAWMODE_SIM_API__H_
#define _OL_RAWMODE_SIM_API__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */


/* Raw Mode simulation - conversion between Raw 802.11 format and other
 * formats.
 */

/* Statistics for the Raw Mode simulation module. These do not cover events
 * occurring outside the modules (such as higher layer failures to process a
 * successfully decapped MPDU, etc.)*/

struct rawmode_pkt_sim_rxstats {
    /* Rx Side simulation module statistics */

    /* Decap successes */

    /* Number of non-AMSDU bearing MPDUs decapped */
    u_int64_t num_rx_mpdu_noamsdu;

    /* Number of A-MSDU bearing MPDUs (fitting within single nbuf)
       decapped */
    u_int64_t num_rx_smallmpdu_withamsdu;

    /* Number of A-MSDU bearing MPDUs (requiring multiple nbufs) decapped */
    u_int64_t num_rx_largempdu_withamsdu;


    /* Decap errors */

    /* Number of MSDUs (contained in A-MSDU) with invalid length field */
    u_int64_t num_rx_inval_len_msdu;

    /* Number of A-MSDU bearing MPDUs which are shorter than expected from
       parsing A-MSDU fields */
    u_int64_t num_rx_tooshort_mpdu;

    /* Number of A-MSDU bearing MPDUs received which are longer than
       expected from parsing A-MSDU fields */
    u_int64_t num_rx_toolong_mpdu;

    /* Number of non-AMSDU bearing MPDUs (requiring multiple nbufs) seen
       (unhandled) */
    u_int64_t num_rx_chainedmpdu_noamsdu;

    /* Add anything else of interest */
};

struct rawmode_pkt_sim_txstats {
    /* Tx Side simulation module statistics */

    /* Number of non-AMSDU bearing MPDUs encapped */
    u_int64_t num_tx_mpdu_noamsdu;

    /* Number of A-MSDU bearing MPDUs encapped */
    u_int64_t num_tx_mpdu_withamsdu;

    /* Add anything else of interest */
};

#include <ieee80211_var.h>      /* Generally required constructs */

int
osif_rsim_tx_encap(struct ieee80211vap *vap, qdf_nbuf_t *pnbuf);
/**
 * @brief decap a list of nbufs from 802.11 Raw format to Ethernet II format
 * @details
 *  Note that the list head can be set to NULL if errors are encountered
 *  for each and every nbuf in the list.
 *
 * @param vdev - the data virtual device object
 * @param peer - the peer object
 * @param pdeliver_list_head - pointer to head of delivery list
 * @param pdeliver_list_head - pointer to tail of delivery list
 */
extern void
osif_rsim_rx_decap(os_if_t osif,
        qdf_nbuf_t *pdeliver_list_head,
        qdf_nbuf_t *pdeliver_list_tail, void *peer);


/**
 * @brief print raw mode packet simulation statistics
 *
 * @param vdev - the data virtual device object
 */
extern void
print_rawmode_pkt_sim_stats(struct ieee80211vap *vap);

/**
 * @brief clear raw mode packet simulation statistics
 *
 * @param vdev - the data virtual device object
 */
extern void
clear_rawmode_pkt_sim_stats(struct ieee80211vap *vap);

enum {
    osif_raw_sec_mcast = 0,
    osif_raw_sec_ucast
};

enum osif_raw_sec_type {
	osif_raw_sec_type_none,
	osif_raw_sec_type_wep128,
	osif_raw_sec_type_wep104,
	osif_raw_sec_type_wep40,
	osif_raw_sec_type_tkip,
	osif_raw_sec_type_tkip_nomic,
	osif_raw_sec_type_aes_ccmp,
	osif_raw_sec_type_wapi,
	osif_raw_sec_type_aes_ccmp_256,
	osif_raw_sec_type_aes_gcmp,
	osif_raw_sec_type_aes_gcmp_256,

	/* keep this last! */
	osif_raw_num_sec_types
};

struct osif_rawsim_ast_entry {
	uint8_t ast_found;
	uint8_t mac_addr[6];
};

#define RAWSIM_MIN_FRAGS_PER_TX_MPDU 2

#endif /* _OL_RAWMODE_SIM_API__H_ */
