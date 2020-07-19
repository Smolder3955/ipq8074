/* Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2000-2003, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */


#include <osdep.h>
#include "pktlog_i.h"
#include "pktlog_rc.h"
#include "if_llc.h"
#include <ieee80211_var.h>
#include <if_athvar.h>

#ifndef REMOVE_PKT_LOG

/*
 * Our Linux driver builds the pktlog into an independent kernel module.
 * Thus, we will need to export the symbol for dprintf funcion in either
 * ath_dev/if_ath layer if we want to use the dprintf function.
 * However, ath_dev does not have any externel API now (only ath_ar_ops
 * callback table) and calling dprintf in this case does not have any benefits
 * since it is an externel function. To keep it simple, redefine DPRINTF to
 * call the asf_print_category directly. Since DPRINTF is only called 3 times
 * in this file, the size of symbol table will not expand much.
 */
#if ATH_DEBUG
#undef     DPRINTF
#define    DPRINTF(sc, _m, _fmt, ...)                           \
    asf_print_category(&(sc)->sc_print, _m, _fmt, ## __VA_ARGS__)
#endif

struct ath_pktlog_info *g_pktlog_info = NULL;
int g_pktlog_mode = PKTLOG_MODE_SYSTEM;

struct pl_arch_dep_funcs pl_funcs = {
    .pktlog_init = pktlog_init,
    .pktlog_enable = pktlog_enable,
};

struct pktlog_handle_t *get_pl_handle(ath_generic_softc_handle scn)
{
    struct pktlog_handle_t *pl_dev;
    if (!scn) {
        return NULL;
    }
    pl_dev = *((struct pktlog_handle_t **)
                                ((unsigned char*)scn +
                                 sizeof(struct ieee80211com)));
    return pl_dev;
}

bool is_mode_offload(ath_generic_softc_handle scn)
{
    struct ath_softc_net80211 *sc = (struct ath_softc_net80211 *)scn;
    struct ieee80211com *ic = &sc->sc_ic;

    return ic->ic_is_mode_offload(ic);
}

void
pktlog_init(void *_scn)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *) _scn;
    struct pktlog_handle_t *pl_dev = (scn) ? scn->pl_dev :
                                                     NULL;
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;
    struct ath_softc *sc;
    if (scn) {
        sc = (struct ath_softc *)scn->sc_dev;
        sc->pl_info = pl_info;
    }
    OS_MEMZERO(pl_info, sizeof(*pl_info));

    qdf_spinlock_create(&pl_info->log_lock);
    qdf_mutex_create(&pl_info->pktlog_mutex);

    if (pl_dev) {
        pl_dev->tgt_pktlog_enabled = false;
    }
    pl_info->buf_size = PKTLOG_DEFAULT_BUFSIZE;
    pl_info->buf = NULL;
    pl_info->log_state = 0;
    pl_info->sack_thr = PKTLOG_DEFAULT_SACK_THR;
    pl_info->tail_length = PKTLOG_DEFAULT_TAIL_LENGTH;
    pl_info->thruput_thresh = PKTLOG_DEFAULT_THRUPUT_THRESH;
    pl_info->per_thresh = PKTLOG_DEFAULT_PER_THRESH;
    pl_info->phyerr_thresh = PKTLOG_DEFAULT_PHYERR_THRESH;
    pl_info->trigger_interval = PKTLOG_DEFAULT_TRIGGER_INTERVAL;
	pl_info->pktlen = 0;
	pl_info->start_time_thruput = 0;
	pl_info->start_time_per = 0;
}

void
pktlog_cleanup(struct ath_pktlog_info *pl_info)
{
    pl_info->log_state = 0;
    qdf_spinlock_destroy(&pl_info->log_lock);
    qdf_mutex_destroy(&pl_info->pktlog_mutex);
}


static int
__pktlog_enable(ath_generic_softc_handle scn, int32_t log_state)
{
    struct pktlog_handle_t *pl_dev = get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;
    int error;

    if (!pl_info) {
        return 0;
    }

    pl_info->log_state = 0;

    if (log_state != 0) {
        if (!pl_dev) {
            if (g_pktlog_mode == PKTLOG_MODE_ADAPTER) {
                pktlog_disable_adapter_logging();
                g_pktlog_mode = PKTLOG_MODE_SYSTEM;
            }
        } else {
            if (g_pktlog_mode == PKTLOG_MODE_SYSTEM) {
                /* Currently the system wide logging is disabled */
                g_pktlog_info->log_state = 0;
                g_pktlog_mode = PKTLOG_MODE_ADAPTER;
            }
        }

        if (pl_info->buf == NULL) {
            error = pktlog_alloc_buf(scn ,pl_info);
            if (error != 0)
                return error;
            qdf_spin_lock(&pl_info->log_lock);
            pl_info->buf->bufhdr.version = CUR_PKTLOG_VER;
            pl_info->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
            pl_info->buf->wr_offset = 0;
            pl_info->buf->rd_offset = -1;
            qdf_spin_unlock(&pl_info->log_lock);
        }
	    pl_info->start_time_thruput = OS_GET_TIMESTAMP();
	    pl_info->start_time_per = pl_info->start_time_thruput;
    }
    pl_info->log_state = log_state;

    return 0;
}



int
pktlog_enable(ath_generic_softc_handle scn, int32_t log_state)
{
    struct pktlog_handle_t *pl_dev = get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;
    int status;

    if (!pl_info) {
        return 0;
    }
    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_enable(scn, log_state);
    qdf_mutex_release(&pl_info->pktlog_mutex);

    return status;
}

static int
__pktlog_setsize(ath_generic_softc_handle scn,
                                int32_t size)
{

    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)
                                                get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;

    if (size < 0)
        return -EINVAL;

    if (size == pl_info->buf_size)
        return 0;

    if (pl_info->log_state) {
        printk("Logging should be disabled before changing bufer size\n");
        return -EINVAL;
    }
    qdf_spin_lock(&pl_info->log_lock);
    if (pl_info->buf != NULL)
        pktlog_release_buf(pl_info); //remove NULL

    if (size != 0)
        pl_info->buf_size = size;
    qdf_spin_unlock(&pl_info->log_lock);

    return 0;
}


int
pktlog_setsize(ath_generic_softc_handle scn,
                                int32_t size)
{

    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)
                                                get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;
    int status;

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_setsize(scn, size);
    qdf_mutex_release(&pl_info->pktlog_mutex);

    return status;
}

static int
__pktlog_reset_buffer(ath_generic_softc_handle scn, int32_t reset)
{
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)
                                                get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;

    if (pl_info->log_state) {
        printk("Logging should be disabled before reseting bufer size\n");
        return -EINVAL;
    }

    qdf_spin_lock(&pl_info->log_lock);
    if (pl_info->buf != NULL) {
        printk("Reseting pktlog buffer!\n");
        pktlog_release_buf(pl_info);
    }
    qdf_spin_unlock(&pl_info->log_lock);

    return 0;
}


int
pktlog_reset_buffer(ath_generic_softc_handle scn, int32_t reset)
{
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)
                                                get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = (pl_dev) ?
                                        pl_dev->pl_info : g_pktlog_info;
    int status;

    if(pl_info == NULL)
        return -EINVAL;

    if (reset != 1)
        return -EINVAL;

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_reset_buffer(scn, reset);
    qdf_mutex_release(&pl_info->pktlog_mutex);

    return status;
}

static void
pktlog_getbuf_intsafe(struct ath_pktlog_arg *plarg)
{
    struct ath_pktlog_buf *log_buf;
    int32_t buf_size;
    struct ath_pktlog_hdr *log_hdr;
    int32_t cur_wr_offset;
    char *log_ptr;
    struct ath_pktlog_info *pl_info = plarg->pl_info;
    u_int16_t log_type = plarg->log_type;
    size_t log_size = plarg->log_size;
    u_int32_t flags = plarg->flags;

    log_buf = pl_info->buf;
    buf_size = pl_info->buf_size;

    cur_wr_offset = log_buf->wr_offset;
    /* Move read offset to the next entry if there is a buffer overlap */
    if (log_buf->rd_offset >= 0) {
        if ((cur_wr_offset <= log_buf->rd_offset)
            && (cur_wr_offset + sizeof(struct ath_pktlog_hdr)) >
            log_buf->rd_offset)
            PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf, buf_size);
    } else {
        log_buf->rd_offset = cur_wr_offset;
    }

    log_hdr =
        (struct ath_pktlog_hdr *) (log_buf->log_data + cur_wr_offset);
    log_hdr->log_type = log_type;
    log_hdr->flags = flags;
    log_hdr->timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    log_hdr->size = (u_int16_t)log_size;

    cur_wr_offset += sizeof(*log_hdr);

    if ((buf_size - cur_wr_offset) < log_size) {
        while ((cur_wr_offset <= log_buf->rd_offset)
               && (log_buf->rd_offset < buf_size))
            PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf, buf_size);
        cur_wr_offset = 0;
    }

    while ((cur_wr_offset <= log_buf->rd_offset)
           && (cur_wr_offset + log_size) > log_buf->rd_offset)
        PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf, buf_size);

    log_ptr = &(log_buf->log_data[cur_wr_offset]);

    cur_wr_offset += log_hdr->size;

    log_buf->wr_offset =
        ((buf_size - cur_wr_offset) >=
         sizeof(struct ath_pktlog_hdr)) ? cur_wr_offset : 0;

    plarg->buf = log_ptr;
}

static char *
pktlog_getbuf(struct ath_softc *sc, struct ath_pktlog_info *pl_info,
              u_int16_t log_type, size_t log_size,
              u_int32_t flags)
{
    struct ath_pktlog_arg plarg;
    plarg.pl_info = pl_info;
    plarg.log_type = log_type;
    plarg.log_size = log_size;
    plarg.flags = flags;

    if(flags & PHFLAGS_INTERRUPT_CONTEXT) {
        /* we are already in interupt context, no need to make it intsafe
         * call the function directly.
         */
        pktlog_getbuf_intsafe(&plarg);
    }
    else {
        qdf_spin_lock(&pl_info->log_lock);
        OS_EXEC_INTSAFE(sc->sc_osdev, pktlog_getbuf_intsafe, &plarg);
        qdf_spin_unlock(&pl_info->log_lock);
    }

    return plarg.buf;
}


#ifndef REMOVE_PKTLOG_PROTO
/*
 * Searches for the presence of a protocol header and returns protocol type.
 * proto_len and proto_log are modified to return length (in bytes) and
 * contents of header (in host byte order), if one is found.
 */
static int
pktlog_proto(struct ath_softc *sc, u_int32_t proto_log[PKTLOG_MAX_PROTO_WORDS],
             void *log_data, pktlog_proto_desc_t ds_type, int *proto_len, HAL_BOOL *isSack)
{
#define IPHDRLEN     20
    struct llc *llc = NULL;
    struct log_tx *tx_log;
    struct log_rx *rx_log;
    wbuf_t wbuf = NULL;
    static const int pktlog_proto_min_hlen = sizeof(struct ieee80211_frame) +
                                             sizeof(struct llc) + IPHDRLEN;

    switch (ds_type) {
        case PKTLOG_PROTO_TX_DESC:
            tx_log = (struct log_tx *)log_data;
            wbuf = (wbuf_t)(tx_log->bf->bf_mpdu);
            if (wbuf_get_pktlen((wbuf_t)(tx_log->bf->bf_mpdu)) < pktlog_proto_min_hlen) {
                return PKTLOG_PROTO_NONE;
            }

            llc = (struct llc *)sc->sc_ieee_ops->parse_frm(sc->sc_ieee,
                      wbuf,
                      wlan_wbuf_get_peer_node((wbuf_t)(tx_log->bf->bf_mpdu)),
                      tx_log->bf->bf_vdata,
                      0);
            break;

        case PKTLOG_PROTO_RX_DESC:
            rx_log = (struct log_rx *)log_data;
            if (rx_log->status->rs_datalen < pktlog_proto_min_hlen) {
                return PKTLOG_PROTO_NONE;
            }

            llc = (struct llc *)sc->sc_ieee_ops->parse_frm(sc->sc_ieee,
                                                           NULL, NULL,
                                                           rx_log->bf->bf_vdata,
                                                           rx_log->status->rs_keyix);
            break;

        default:
            return PKTLOG_PROTO_NONE;
    }

    if(!llc) {
        return PKTLOG_PROTO_NONE;
    }

    return pktlog_tcpip(sc, wbuf, llc, proto_log, proto_len, isSack);
#undef IPHDRLEN
}
#endif

/*
 * if we can tell a packet is TCP SACK, increase its count here.
 * when it reaches a threshold, mark a flag to stop pktlog but log some
 * extra packets for context
 */
static void pktlog_trigger_sack(HAL_BOOL isSack, struct ath_pktlog_info *pl_info)
{
    static HAL_BOOL trigger_stop = FALSE;
    static int sack_cnt, tail_cnt;

	if (pl_info->log_state == 0) {
		return;
	}

            if (isSack && !trigger_stop) {
			    sack_cnt++;
			    if (sack_cnt >= pl_info->sack_thr) {
                    trigger_stop = TRUE;
			    }
		    }
            if (trigger_stop) {
              if (++tail_cnt >= pl_info->tail_length) {
				/* temporarily stop logging; once buffer is read logging will be re-enabled */
	            DPRINTF(pl_info->pl_sc, ATH_DEBUG_ANY, "pktlog stopped and has seen %d TCP SACK packets\n", sack_cnt);
				pl_info->saved_state = pl_info->log_state;
				pl_info->log_state = 0;
				sack_cnt = 0;
                tail_cnt = 0;
                trigger_stop = FALSE;
              }
            }
}

/*
 * count PER and stop pktlog if it's over a threshold. Clear the count per an interval.
 */
static void pktlog_trigger_per(HAL_BOOL pkt_good, struct ath_pktlog_info *pl_info, int failcount, int nframes, int nbad)
{
    static u_int32_t pkt_err = 0, pkt_cnt = 0;
	u_int32_t now;

	if(nframes > 1) {
		/*
		 * this is a tx aggregate packet
		 * failcount: number of tx retries due to not seeing Block ACK
		 * nframes:   number of subframes in an aggregate
		 */
		pkt_cnt += failcount * nframes;
		pkt_err += failcount * nframes;
		if (pkt_good) {
			pkt_cnt += nframes;
		    pkt_err += nbad;
		}
	} else {
	    pkt_cnt += failcount;
	    pkt_err += failcount;
	    if (pkt_good)
		    pkt_cnt++;
	}
	now = OS_GET_TIMESTAMP();
	if (CONVERT_SYSTEM_TIME_TO_MS(now - pl_info->start_time_per) > pl_info->trigger_interval) {
		if (pkt_err * 100 > pkt_cnt * pl_info->per_thresh) {
			//trigger stop
	        DPRINTF(pl_info->pl_sc, ATH_DEBUG_ANY, "pktlog stopped: seen %d bad packets out of %d in past %dms\n",
				pkt_err, pkt_cnt, pl_info->trigger_interval);
			if (pl_info->log_state) {
			    pl_info->saved_state = pl_info->log_state;
			    pl_info->log_state = 0;
			}
		}
		pkt_err = 0;
		pkt_cnt = 0;
		pl_info->start_time_per = now;
	}
}

/*
 * count PER and stop pktlog if it's over a threshold. Clear the count per an interval.
 */
static void pktlog_trigger_thruput(HAL_BOOL pkt_good, u_int32_t pktlen, struct ath_pktlog_info *pl_info)
{
    static u_int32_t total = 0; // in bytes
	u_int32_t now;

	if (pkt_good)
		total += pktlen;
	now = OS_GET_TIMESTAMP();
	if (CONVERT_SYSTEM_TIME_TO_MS(now - pl_info->start_time_thruput) > pl_info->trigger_interval) {
		if (total < pl_info->thruput_thresh) {
			//trigger stop
	        DPRINTF(pl_info->pl_sc, ATH_DEBUG_ANY, "pktlog stopped: only %d bytes thruput in past %dms\n", total, pl_info->trigger_interval);
			if (pl_info->log_state) {
			    pl_info->saved_state = pl_info->log_state;
			    pl_info->log_state = 0;
			}
		}
		total = 0;
		pl_info->start_time_thruput = now;
	}
}

/*
 * Log Tx data - logs into adapter's buffer if sc is not NULL;
 *               logs into system-wide buffer if sc is NULL.
 */
void
pktlog_txctl(struct ath_softc *sc, struct log_tx *log_data, u_int32_t iflags)
{
    struct ath_pktlog_txctl *tx_log;
    int i, proto = PKTLOG_PROTO_NONE, proto_len = 0;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    struct ieee80211_frame *wh;
    u_int8_t dir;
    u_int32_t *ds_words, proto_hdr[PKTLOG_MAX_PROTO_WORDS];
    HAL_DESC_INFO desc_info;
    u_int32_t    flags = iflags;
    HAL_BOOL isSack = FALSE;
	wbuf_t wbuf;
	int frmlen;
    u_int32_t    *proto_hdrp, *misc_p;

    if ((NULL == log_data->bf) || (NULL == log_data->bf->bf_mpdu)) {
        return;
    }

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_TX)== 0 ||
        log_data->bf->bf_vdata == 0) {
        return;
    }

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));


#ifndef REMOVE_PKTLOG_PROTO
    if (pl_info->options & ATH_PKTLOG_PROTO) {
        proto = pktlog_proto(sc, proto_hdr, log_data, PKTLOG_PROTO_TX_DESC,
                             &proto_len, &isSack);
        flags |= (proto << PHFLAGS_PROTO_SFT) & PHFLAGS_PROTO_MASK;
    }
#endif

    tx_log = (struct ath_pktlog_txctl *)pktlog_getbuf(sc, pl_info,
              PKTLOG_TYPE_TXCTL, sizeof(*tx_log) +
              misc_cnt * sizeof(tx_log->misc[0]) + proto_len, flags);
    proto_hdrp = (u_int32_t *)&tx_log->proto_hdr;
    misc_p = (u_int32_t *) ((char *) proto_hdrp + roundup(proto_len,sizeof(int)));


    wh = (struct ieee80211_frame *) (wbuf_header(log_data->bf->bf_mpdu));
    tx_log->framectrl = *(u_int16_t *)(wh->i_fc);
    tx_log->seqctrl   = *(u_int16_t *)(wh->i_seq);

    dir = (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK);
    if(dir == IEEE80211_FC1_DIR_TODS) {
        tx_log->bssid_tail = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
        tx_log->sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        tx_log->da_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
    }
    else if(dir == IEEE80211_FC1_DIR_FROMDS) {
        tx_log->bssid_tail = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        tx_log->sa_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
        tx_log->da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
    }
    else {
        tx_log->bssid_tail = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
        tx_log->sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        tx_log->da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
    }

    ath_hal_getdescinfo(sc->sc_ah, &desc_info);

    ds_words = (u_int32_t *)(log_data->firstds) + desc_info.txctl_offset;
    for(i = 0; i < desc_info.txctl_numwords; i++)
        tx_log->txdesc_ctl[i] = ds_words[i];

    for (i = 0; i < misc_cnt; i++)
        misc_p[i] = log_data->misc[i];

    if ((pl_info->options & ATH_PKTLOG_TRIGGER_THRUPUT)) {
        wbuf = (wbuf_t)(log_data->bf->bf_mpdu);
        frmlen = wbuf_get_pktlen((wbuf_t)(log_data->bf->bf_mpdu));
        pl_info->pktlen += frmlen;
    }

    if (proto != PKTLOG_PROTO_NONE) {
        OS_MEMCPY(proto_hdrp, proto_hdr, proto_len);
        if ((proto == PKTLOG_PROTO_TCP) && (pl_info->options & ATH_PKTLOG_TRIGGER_SACK)) {
            pktlog_trigger_sack(isSack, pl_info);
        }
    }
}

void
pktlog_txstatus(struct ath_softc *sc, struct log_tx *log_data, u_int32_t iflags)
{
    struct ath_pktlog_txstatus *tx_log;
    int i;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    u_int32_t *ds_words;
    HAL_DESC_INFO desc_info;
    u_int32_t flags = iflags;
	struct ath_desc *txdesc = (struct ath_desc *)log_data->lastds;

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_TX)== 0)
        return;

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
    tx_log = (struct ath_pktlog_txstatus *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_TXSTATUS,
                  sizeof(*tx_log) + misc_cnt * sizeof(tx_log->misc[0]), flags);

    ath_hal_getdescinfo(sc->sc_ah, &desc_info);

    ds_words = (u_int32_t *)(log_data->lastds) + desc_info.txstatus_offset;

    for(i = 0; i < desc_info.txstatus_numwords; i++)
        tx_log->txdesc_status[i] = ds_words[i];

    for (i = 0; i < misc_cnt; i++)
        tx_log->misc[i] = log_data->misc[i];

    pl_info->pl_sc = sc;
    if ((pl_info->options & ATH_PKTLOG_TRIGGER_THRUPUT)) {
    //check tx status
        if (txdesc->ds_txstat.ts_status == 0)
            pktlog_trigger_thruput(TRUE, pl_info->pktlen, pl_info);
        else
           pktlog_trigger_thruput(FALSE, pl_info->pktlen, pl_info);
        pl_info->pktlen = 0;
    }
    if ((pl_info->options & ATH_PKTLOG_TRIGGER_PER)) {
        //check tx status
        //shortretry count RTS fails, longretry counts data fails
        int retry = txdesc->ds_txstat.ts_longretry + txdesc->ds_txstat.ts_shortretry;
        int series;
        for (series = 0; series < txdesc->ds_txstat.ts_rateindex; series++) {
            retry += log_data->bf->bf_rcs[series].tries;
        }
        if (txdesc->ds_txstat.ts_status == 0)
            pktlog_trigger_per(TRUE, pl_info, retry, log_data->bf->bf_nframes, log_data->nbad);
        else
            pktlog_trigger_per(FALSE, pl_info, retry, log_data->bf->bf_nframes, log_data->nbad);
    }
}

void
pktlog_rx(struct ath_softc *sc, struct log_rx *log_data, u_int32_t iflags)
{
    struct ath_pktlog_rx *rx_log;
    int i, proto = PKTLOG_PROTO_NONE, proto_len = 0;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    struct ieee80211_frame *wh;
    u_int32_t *ds_words, proto_hdr[PKTLOG_MAX_PROTO_WORDS];
    HAL_DESC_INFO desc_info;
    u_int8_t dir;
    u_int32_t flags = iflags;
    HAL_BOOL isSack = FALSE;

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_RX) == 0)
        return;

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));

#ifndef REMOVE_PKTLOG_PROTO
    if (pl_info->options & ATH_PKTLOG_PROTO) {
        proto = pktlog_proto(sc, proto_hdr, log_data, PKTLOG_PROTO_RX_DESC,
                             &proto_len, &isSack);
        flags |= (proto << PHFLAGS_PROTO_SFT) & PHFLAGS_PROTO_MASK;
    }
#endif

    rx_log = (struct ath_pktlog_rx *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_RX,
             sizeof(*rx_log) + misc_cnt * sizeof(rx_log->misc[0]) + proto_len,
             flags);
    rx_log->proto_hdr = (u_int32_t *) ((char *) rx_log +
        sizeof(struct ath_pktlog_rx));
    rx_log->misc = (u_int32_t *) ((char *) rx_log->proto_hdr +
        roundup(proto_len,sizeof(int)));

    if(log_data->status->rs_datalen > sizeof(struct ieee80211_frame)) {
        wh = (struct ieee80211_frame *) (log_data->bf->bf_vdata);
        rx_log->framectrl = *(u_int16_t *)(wh->i_fc);
        rx_log->seqctrl   = *(u_int16_t *)(wh->i_seq);

        dir = (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK);
        if(dir == IEEE80211_FC1_DIR_TODS) {
            rx_log->bssid_tail = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
            rx_log->sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
            rx_log->da_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
        } else if(dir == IEEE80211_FC1_DIR_FROMDS) {
            rx_log->bssid_tail = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
            rx_log->sa_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
            rx_log->da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
        } else {
            rx_log->bssid_tail = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
            rx_log->sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
            rx_log->da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
        }
    } else {
        wh = (struct ieee80211_frame *) (log_data->bf->bf_vdata);

        if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
            rx_log->framectrl = *(u_int16_t *)(wh->i_fc);
            rx_log->da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                                 (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
            if(log_data->status->rs_datalen < sizeof(struct ieee80211_ctlframe_addr2)) {
                rx_log->sa_tail = 0;
            } else {
                rx_log->sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                                     (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
            }
        } else {
            rx_log->framectrl = 0xFFFF;
            rx_log->da_tail = 0;
            rx_log->sa_tail = 0;
        }

        rx_log->seqctrl   = 0;
        rx_log->bssid_tail = 0;
    }

    ath_hal_getdescinfo(sc->sc_ah, &desc_info);

    ds_words = (u_int32_t *)(log_data->ds) + desc_info.rxstatus_offset;

    for(i = 0; i < desc_info.rxstatus_numwords; i++)
        rx_log->rxdesc_status[i] = ds_words[i];

    for (i = 0; i < misc_cnt; i++)
        rx_log->misc[i] = log_data->misc[i];

    pl_info->pl_sc = sc;

    if (proto != PKTLOG_PROTO_NONE) {
        OS_MEMCPY(rx_log->proto_hdr, proto_hdr, proto_len);
		if ((proto == PKTLOG_PROTO_TCP) && (pl_info->options & ATH_PKTLOG_TRIGGER_SACK)) {
                    pktlog_trigger_sack(isSack, pl_info);
                }
    }

	if ((pl_info->options & ATH_PKTLOG_TRIGGER_THRUPUT)) {
		//check rx status
		if (log_data->status->rs_status == 0)
			pktlog_trigger_thruput(TRUE, log_data->status->rs_datalen, pl_info);
		else
			pktlog_trigger_thruput(FALSE, log_data->status->rs_datalen, pl_info);
	}
	if ((pl_info->options & ATH_PKTLOG_TRIGGER_PER)) {
		//check rx status
		if (log_data->status->rs_status == 0)
			pktlog_trigger_per(TRUE, pl_info, 0, 0, 0);
		else
			pktlog_trigger_per(FALSE, pl_info, 1, 0, 0);
	}
}

void
__ahdecl3 pktlog_ani(HAL_SOFTC hal_sc, struct log_ani *log_data, u_int32_t iflags)
{
    struct ath_softc *sc = (struct ath_softc *)hal_sc;
    struct ath_pktlog_ani *ani_log;
    int i;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    u_int32_t flags = 0;

    if (iflags == 1) {
        /* we are in interrupt context. Set the flag to let pktlog_getbuf()
         * know so that it would not try to acquire any lock */
        flags |= PHFLAGS_INTERRUPT_CONTEXT;
    }

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_ANI) == 0)
        return;

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
    ani_log = (struct ath_pktlog_ani *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_ANI,
                  sizeof(*ani_log) + misc_cnt * sizeof(ani_log->misc[0]), flags);

    ani_log->phyStatsDisable = log_data->phy_stats_disable;
    ani_log->noiseImmunLvl = log_data->noise_immun_lvl;
    ani_log->spurImmunLvl = log_data->spur_immun_lvl;
    ani_log->ofdmWeakDet = log_data->ofdm_weak_det;
    ani_log->cckWeakThr = log_data->cck_weak_thr;
    ani_log->firLvl = log_data->fir_lvl;
    ani_log->listenTime = (u_int16_t) (log_data->listen_time);
    ani_log->cycleCount = log_data->cycle_count;
    ani_log->ofdmPhyErrCount = log_data->ofdm_phy_err_count;
    ani_log->cckPhyErrCount = log_data->cck_phy_err_count;
    ani_log->rssi = log_data->rssi;

    for (i = 0; i < misc_cnt && i < 8; i++)	/* i<8 is req as log_data->misc is an array of 8 int32*/
        ani_log->misc[i] = log_data->misc[i];
}

void
pktlog_rcfind(struct ath_softc *sc, struct log_rcfind *log_data, u_int32_t iflags)
{
    struct ath_pktlog_rcfind *rcf_log;
    struct TxRateCtrl_s *pRc = log_data->rc;
    int i;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    u_int32_t    flags = iflags;

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_RCFIND) == 0)
        return;

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
    rcf_log = (struct ath_pktlog_rcfind *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_RCFIND,
                  sizeof(*rcf_log) + misc_cnt * sizeof(rcf_log->misc[0]), flags);

    rcf_log->rate = log_data->rate;
    rcf_log->rateCode = log_data->rateCode;
    rcf_log->rcRssiLast = pRc->rssiLast;
    rcf_log->rcRssiLastPrev = pRc->rssiLastPrev;
    rcf_log->rcRssiLastPrev2 = pRc->rssiLastPrev2;
    rcf_log->rssiReduce = log_data->rssiReduce;
    rcf_log->rcProbeRate = log_data->isProbing? pRc->probeRate:0;
    rcf_log->isProbing = log_data->isProbing;
    rcf_log->primeInUse = log_data->primeInUse;
    rcf_log->currentPrimeState = log_data->currentPrimeState;
    rcf_log->ac = log_data->ac;
    rcf_log->rcRateMax = pRc->rateMaxPhy;
    rcf_log->rcRateTableSize = pRc->rateTableSize;

    for (i = 0; i < misc_cnt; i++)
        rcf_log->misc[i] = log_data->misc[i];
}

void
pktlog_rcupdate(struct ath_softc *sc, struct log_rcupdate *log_data, u_int32_t iflags)
{
    struct ath_pktlog_rcupdate *rcu_log;
    struct TxRateCtrl_s *pRc = log_data->rc;
    int i;
    u_int8_t misc_cnt;
    struct ath_pktlog_info *pl_info;
    u_int32_t    flags = iflags;

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if ((pl_info->log_state & ATH_PKTLOG_RCUPDATE) == 0)
        return;

    misc_cnt = flags & PHFLAGS_MISCCNT_MASK;
    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
    rcu_log = (struct ath_pktlog_rcupdate *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_RCUPDATE,
                  sizeof(*rcu_log) + misc_cnt * sizeof(rcu_log->misc[0]), flags);

    rcu_log->txRate = log_data->txRate;
    rcu_log->rateCode = log_data->rateCode;
    rcu_log->Xretries = log_data->Xretries;
    rcu_log->retries = log_data->retries;
    rcu_log->rssiAck = log_data->rssiAck;
    rcu_log->ac = log_data->ac;
    rcu_log->rcRssiLast = pRc->rssiLast;
    rcu_log->rcRssiLastLkup = pRc->rssiLastLkup;
    rcu_log->rcRssiLastPrev = pRc->rssiLastPrev;
    rcu_log->rcRssiLastPrev2 = pRc->rssiLastPrev2;
    rcu_log->rcProbeRate = pRc->probeRate;
    rcu_log->rcRateMax = pRc->rateMaxPhy;
    rcu_log->useTurboPrime = log_data->useTurboPrime;
    rcu_log->currentBoostState = log_data->currentBoostState;
    rcu_log->rcHwMaxRetryRate = pRc->hwMaxRetryRate;

    rcu_log->headFail = 0;
    rcu_log->tailFail = 0;
    rcu_log->aggrSize = 0;
    rcu_log->aggrLimit = 0;
    rcu_log->lastRate = 0;

#ifdef ATH_SUPPORT_VOWEXT  /* for RCA */
    rcu_log->headFail = pRc->nHeadFail;
    rcu_log->tailFail = pRc->nTailFail;
    rcu_log->aggrSize = pRc->nAggrSize;
    rcu_log->aggrLimit = pRc->aggrLimit;
    rcu_log->lastRate = pRc->lastRate;
#endif

    for (i = 0; i < MAX_TX_RATE_TBL; i++) {
        rcu_log->rcRssiThres[i] = pRc->state[i].rssiThres;
        rcu_log->rcPer[i] = pRc->state[i].per;
        rcu_log->rcMaxAggrSize[i] = 0;
#ifdef ATH_SUPPORT_VOWEXT /* for RCA */
        rcu_log->rcMaxAggrSize[i] = pRc->state[i].maxAggrSize;
#endif
    }

    for (i = 0; i < misc_cnt; i++)
        rcu_log->misc[i] = log_data->misc[i];
}

int
pktlog_text(struct ath_softc *sc, const char *tbuf, u_int32_t iflags)
{
    size_t len;
    struct ath_pktlog_info *pl_info;
    u_int8_t *data;
    u_int32_t    flags = iflags;

    if (!tbuf)
        return 0;

    if (g_pktlog_mode == PKTLOG_MODE_ADAPTER)
        pl_info = sc->pl_info;
    else
        pl_info = g_pktlog_info;

    if (!pl_info || ((pl_info->log_state & ATH_PKTLOG_TEXT) == 0)) {
        return 0;
    }

    flags |= (((sc->ah_mac_rev << PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
        ((sc->ah_mac_version << PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
    len = strlen(tbuf);

    data = (u_int8_t *)pktlog_getbuf(sc, pl_info, PKTLOG_TYPE_TEXT, len, flags);
    OS_MEMCPY(data, tbuf, len);
    return 1;
}

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

static u_int
__pktlog_read(struct ath_pktlog_info *pl_info, u_int8_t *buf, int nbytes, u_int *ppos)
{
    u_int bufhdr_size;
    u_int count = 0, ret_val = 0;
    int rem_len;
    int start_offset, end_offset;
    int fold_offset, ppos_data, cur_rd_offset;
    struct ath_pktlog_buf *log_buf;

    qdf_spin_lock(&pl_info->log_lock);
    log_buf =  pl_info->buf;

    if (log_buf == NULL){
        qdf_spin_unlock(&pl_info->log_lock);
        return 0;
    }

    if (*ppos == 0 && pl_info->log_state) {
        pl_info->saved_state = pl_info->log_state;
        pl_info->log_state = 0;
    }

    bufhdr_size = sizeof(log_buf->bufhdr);

    /* copy valid log entries from circular buffer into user buffer */
    rem_len = nbytes;

    if (*ppos < bufhdr_size) {
        count = MIN((bufhdr_size - *ppos), rem_len);
        qdf_spin_unlock(&pl_info->log_lock);
        OS_MEMCPY(buf, ((u_int8_t *) &log_buf->bufhdr) + *ppos, count);
        rem_len -= count;
        ret_val += count;
        qdf_spin_lock(&pl_info->log_lock);
    }

    start_offset = log_buf->rd_offset;

    if ((rem_len == 0) || (start_offset < 0))
        goto rd_done;

    fold_offset = -1;
    cur_rd_offset = start_offset;

    /* Find the last offset and fold-offset if the buffer is folded */
    do {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        if ((fold_offset == -1)
            && ((pl_info->buf_size - log_data_offset) <= log_hdr->size))
            fold_offset = log_data_offset - 1;

        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, pl_info->buf_size);

        if ((fold_offset == -1) && (cur_rd_offset == 0)
            && (cur_rd_offset != log_buf->wr_offset))
            fold_offset = log_data_offset + log_hdr->size - 1;

        end_offset = log_data_offset + log_hdr->size - 1;
    } while (cur_rd_offset != log_buf->wr_offset);

    ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

    if (fold_offset == -1) {
        if (ppos_data > end_offset)
            goto rd_done;

        count = MIN(rem_len, (end_offset - ppos_data + 1));
        qdf_spin_unlock(&pl_info->log_lock);
        OS_MEMCPY(buf + ret_val, log_buf->log_data + ppos_data, count);
        ret_val += count;
        qdf_spin_lock(&pl_info->log_lock);
    } else {
        if (ppos_data <= fold_offset) {
            count = MIN(rem_len, (fold_offset - ppos_data + 1));
            qdf_spin_unlock(&pl_info->log_lock);
            OS_MEMCPY(buf + ret_val, log_buf->log_data + ppos_data,
                      count);
            ret_val += count;
            rem_len -= count;
            qdf_spin_lock(&pl_info->log_lock);
        }

        if (rem_len == 0)
            goto rd_done;

        ppos_data =
            *ppos + ret_val - (bufhdr_size +
                               (fold_offset - start_offset + 1));

        if (ppos_data <= end_offset) {
            count = MIN(rem_len, (end_offset - ppos_data + 1));
            qdf_spin_unlock(&pl_info->log_lock);
            OS_MEMCPY(buf + ret_val, log_buf->log_data + ppos_data,
                      count);
            ret_val += count;
            qdf_spin_lock(&pl_info->log_lock);
        }
    }

rd_done:
    *ppos += ret_val;
    qdf_spin_unlock(&pl_info->log_lock);
    return ret_val;
}

static u_int
pktlog_read(struct ath_pktlog_info *pl_info,  u_int8_t *buf, int nbytes, u_int *ppos)
{
    u_int ret_val = 0;

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    ret_val = __pktlog_read(pl_info, buf, nbytes, ppos);
    qdf_mutex_release(&pl_info->pktlog_mutex);
    return ret_val;
}


int
pktlog_start(ath_generic_softc_handle scn, int log_state)
{
    struct pktlog_handle_t *pl_dev = get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = NULL;
    int error = 0;

    if(pl_dev != NULL) {
        pl_info = pl_dev->pl_info;
    } else {
        return -EINVAL;
    }

    if (log_state == 0) {
        /* use default log_state */
        log_state = ATH_PKTLOG_TX | ATH_PKTLOG_RX | ATH_PKTLOG_ANI |
            ATH_PKTLOG_RCFIND | ATH_PKTLOG_RCUPDATE | ATH_PKTLOG_TEXT;
    }

    if(pl_info == NULL) {
        return -EINVAL;
    }

    if (pl_info->log_state)     /* already started, do nothing */
        return 0;

    if (pl_info->saved_state) {
        /* restore previous log state and log buffer */
        pl_info->log_state = pl_info->saved_state;
        pl_info->saved_state = 0;
    } else {
        error = pktlog_enable(scn, log_state);
    }

    return error;
}

int
pktlog_read_hdr(struct ath_softc *sc, void *buf, u_int32_t buf_len,
                u_int32_t *required_len, u_int32_t *actual_len)
{
    struct ath_pktlog_info *pl_info = sc->pl_info;
    struct ath_pktlog_buf *log_buf;
    u_int pos;

    if ((pl_info == NULL) || (pl_info->buf == NULL))
        return 0;
    log_buf = pl_info->buf;

    if (buf_len < sizeof(log_buf->bufhdr)) {
        *required_len = sizeof (struct ath_pktlog_bufhdr);
        return -EINVAL;

    }

    pos = 0;
    *actual_len = pktlog_read(pl_info, buf, sizeof(log_buf->bufhdr), &pos);

    return 0;
}

int
pktlog_read_buf(struct ath_softc *sc, void *buf, u_int32_t buf_len,
                u_int32_t *required_len, u_int32_t *actual_len)
{
    struct ath_pktlog_info *pl_info = sc->pl_info;
    struct ath_pktlog_buf *log_buf;
    u_int pos;

    if ((pl_info == NULL) || (pl_info->buf == NULL))
        return 0;
    log_buf = pl_info->buf;

    if (buf_len < pl_info->buf_size) {
        *required_len = pl_info->buf_size;
        return -EINVAL;
    }

    pos = sizeof(log_buf->bufhdr);
    *actual_len = pktlog_read(pl_info, buf, pl_info->buf_size, &pos);

    return 0;
}

#endif /* REMOVE_PKT_LOG */
