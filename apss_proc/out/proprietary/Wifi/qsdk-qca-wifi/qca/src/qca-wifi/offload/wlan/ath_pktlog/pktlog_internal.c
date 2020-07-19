/*
 * Copyright (c) 2013-2015, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef REMOVE_PKT_LOG
#include "qdf_mem.h"
#include "target_type.h"
#include "pktlog_ac_i.h"
#include <dbglog_host.h>
#include "fw_dbglog_api.h"
#include "target_if.h"
#include "ieee80211.h"
#include <qdf_mem.h>
#include <init_deinit_lmac.h>

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_PKTLOG_INTERNAL 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_PKTLOG_INTERNAL
#endif

char dbglog_print_buffer[1024];
qdf_export_symbol(dbglog_print_buffer);
void
pktlog_getbuf_intsafe(struct ath_pktlog_arg *plarg)
{
    struct ath_pktlog_buf *log_buf;
    int32_t buf_size;
    ath_pktlog_hdr_t *log_hdr;
    int32_t cur_wr_offset;
    char *log_ptr;
    struct ath_pktlog_info *pl_info = NULL;
    u_int16_t log_type = 0;
    size_t log_size = 0;
    uint32_t flags = 0;
    size_t pktlog_hdr_size;
#if REMOTE_PKTLOG_SUPPORT
    ol_pktlog_dev_t *pl_dev = NULL; /* pointer to pktlog dev */
    struct ol_ath_softc_net80211 *scn; /* pointer to scn object */
    struct pktlog_remote_service *service = NULL; /* pointer to remote packet service */
#endif

    if(!plarg) {
        qdf_info("Invalid parg in %s\n", __FUNCTION__);
        return;
    }

    pl_info  = plarg->pl_info;
    log_type = plarg->log_type;
    log_size = plarg->log_size;
    flags    = plarg->flags;
    log_buf  = pl_info->buf;
    pktlog_hdr_size = plarg->pktlog_hdr_size;

#if REMOTE_PKTLOG_SUPPORT
    scn =  (struct ol_ath_softc_net80211 *)pl_info->scn;
    pl_dev   = scn->pl_dev;
    service = &pl_dev->rpktlog_svc;
#endif
    if(!log_buf) {
        qdf_info("Invalid log_buf in %s\n", __FUNCTION__);
        return;
    }

#if REMOTE_PKTLOG_SUPPORT

    /* remote Logging */
    if (service->running) {

        /* if no connection do not fill buffer */
        if (!service->connect_done) {
            plarg->buf = NULL;
            return;
        }

        /*
         * if size of header+log_size+rlog_write_offset > max allocated size
         * check read pointer (header+logsize) < rlog_read_index; then
         * mark is_wrap = 1 and reset max size to rlog_write_index.
         * reset rlog_write_index = 0
         * starting giving buffer untill we reach (header+logzies <= rlog_read_index).
         * # is_wrap should be reset by read thread
         */

        if (!pl_info->is_wrap) { /* no wrap */

               /* data can not fit to buffer - wrap condition */
            if ((pl_info->rlog_write_index+(log_size + pl_dev->pktlog_hdr_size)) > pl_info->buf_size) {
                /* required size < read_index */
                if((log_size + pl_dev->pktlog_hdr_size) < pl_info->rlog_read_index) {
                    pl_info->rlog_max_size = pl_info->rlog_write_index;
                    pl_info->is_wrap = 1;
                    pl_info->rlog_write_index = 0;
                } else {
#if REMOTE_PKTLOG_DEBUG
                    qdf_info("No Space Untill we read \n");
                    qdf_info("ReadIndex: %d WriteIndex:%d MaxIndex:%d \n", pl_info->rlog_read_index
                            , pl_info->rlog_write_index
                            , pl_info->rlog_max_size);
#endif
                    service->missed_records++;
                    plarg->buf = NULL;
                    return;
                }
            }
        } else {  /* write index is wrapped & we should not over write previous writes untill we read*/
            if((pl_info->rlog_write_index+(log_size + pl_dev->pktlog_hdr_size)) >= pl_info->rlog_read_index) {
#if REMOTE_PKTLOG_DEBUG
                qdf_info("No Space Untill we read \n");
                qdf_info("ReadIndex: %d WriteIndex:%d MaxIndex:%d \n", pl_info->rlog_read_index
                        , pl_info->rlog_write_index
                        , pl_info->rlog_max_size);
#endif
                service->missed_records++;
                plarg->buf = NULL;
                return;
            }
        }

        cur_wr_offset = pl_info->rlog_write_index;

        log_hdr =
            (ath_pktlog_hdr_t *) (log_buf->log_data + cur_wr_offset);

        log_hdr->log_type = log_type;
        log_hdr->flags = flags;
        log_hdr->size = (u_int16_t)log_size;
        log_hdr->missed_cnt = plarg->missed_cnt;
        log_hdr->timestamp = plarg->timestamp;
#ifdef CONFIG_AR900B_SUPPORT
        log_hdr->type_specific_data = plarg->type_specific_data;
#endif
        cur_wr_offset += sizeof(*log_hdr);

        log_ptr = &(log_buf->log_data[cur_wr_offset]);

        cur_wr_offset += log_hdr->size;

        /* Update the write offset */
        pl_info->rlog_write_index = cur_wr_offset;
        plarg->buf = log_ptr;
    } else { /* Regular Packet log */
#endif
    buf_size = pl_info->buf_size;

    /*Return NULL if the buffer size is too small to fit the log*/
    if (buf_size < log_size)
    {
        plarg->buf = NULL;
        return;
    }

    cur_wr_offset = log_buf->wr_offset;
    /* Move read offset to the next entry if there is a buffer overlap */
    if (log_buf->rd_offset >= 0) {
        if ((cur_wr_offset <= log_buf->rd_offset)
                && (cur_wr_offset + pktlog_hdr_size) >
                log_buf->rd_offset) {
            PKTLOG_MOV_RD_IDX_HDRSIZE(log_buf->rd_offset, log_buf, buf_size, pktlog_hdr_size);
        }
    } else {
        log_buf->rd_offset = cur_wr_offset;
    }

    log_hdr =
        (ath_pktlog_hdr_t *) (log_buf->log_data + cur_wr_offset);
    log_hdr->log_type = log_type;
    log_hdr->flags = flags;
    log_hdr->size = (u_int16_t)log_size;
    log_hdr->missed_cnt = plarg->missed_cnt;
    log_hdr->timestamp = plarg->timestamp;
#ifdef CONFIG_AR900B_SUPPORT
    log_hdr->type_specific_data = plarg->type_specific_data;
#endif

    cur_wr_offset += pktlog_hdr_size;

    if ((buf_size - cur_wr_offset) < log_size) {
        while ((cur_wr_offset <= log_buf->rd_offset)
                && (log_buf->rd_offset < buf_size)) {
            PKTLOG_MOV_RD_IDX_HDRSIZE(log_buf->rd_offset, log_buf, buf_size, pktlog_hdr_size);
        }
        cur_wr_offset = 0;
    }

    while ((cur_wr_offset <= log_buf->rd_offset)
            && (cur_wr_offset + log_size) > log_buf->rd_offset) {
        PKTLOG_MOV_RD_IDX_HDRSIZE(log_buf->rd_offset, log_buf, buf_size, pktlog_hdr_size);
    }

    log_ptr = &(log_buf->log_data[cur_wr_offset]);

    cur_wr_offset += log_hdr->size;

    log_buf->wr_offset =
        ((buf_size - cur_wr_offset) >=
         pktlog_hdr_size) ? cur_wr_offset : 0;

    plarg->buf = log_ptr;
#if REMOTE_PKTLOG_SUPPORT
    }
#endif

}

char *
pktlog_getbuf(ol_pktlog_dev_t *pl_dev,
                struct ath_pktlog_info *pl_info,
                size_t log_size,
                ath_pktlog_hdr_t *pl_hdr)
{
    struct ath_pktlog_arg plarg;
    uint8_t flags = 0;
    plarg.pl_info = pl_info;
    plarg.log_type = pl_hdr->log_type;
    plarg.log_size = log_size;
    plarg.flags = pl_hdr->flags;
    plarg.missed_cnt = pl_hdr->missed_cnt;
    plarg.timestamp = pl_hdr->timestamp;
    plarg.pktlog_hdr_size = pl_dev->pktlog_hdr_size;
    plarg.buf = NULL;
#ifdef CONFIG_AR900B_SUPPORT
    plarg.type_specific_data = pl_hdr->type_specific_data;
#endif

    if(flags & PHFLAGS_INTERRUPT_CONTEXT) {
        /*
         * We are already in interupt context, no need to make it intsafe
         * call the function directly.
         */
        pktlog_getbuf_intsafe(&plarg);
    }
    else {
        qdf_spin_lock(&pl_info->log_lock);
        OS_EXEC_INTSAFE(pl_dev->sc_osdev, pktlog_getbuf_intsafe, &plarg);
        qdf_spin_unlock(&pl_info->log_lock);
    }

    return plarg.buf;
}
qdf_export_symbol(pktlog_getbuf);

static struct txctl_frm_hdr frm_hdr;

/* process_text_info: this function forms the packet log evet of type debug print
 * from the text
 * */
A_STATUS
process_text_info(ol_pktlog_dev_t *pl_dev, char *text)
{
    ath_pktlog_hdr_t pl_hdr;
    size_t log_size;
    struct ath_pktlog_info *pl_info;
    struct ath_pktlog_dbg_print dbg_print_s;

    if (!pl_dev) {
        qdf_info("Invalid pdev in %s", __FUNCTION__);
        return A_ERROR;
    }

    pl_info = pl_dev->pl_info;

    if (!pl_info) {
        /* Invalid pl_info */
        return A_ERROR;
    }

    if ((pl_info->log_state & ATH_PKTLOG_DBG_PRINT) == 0) {
        /* log text not enabled */
        return A_ERROR;
    }

    /*
     *      * Makes the short words (16 bits) portable b/w little endian
     *      * and big endian
     *      */
    pl_hdr.flags = 1;
    pl_hdr.flags |= PHFLAGS_INTERRUPT_CONTEXT;
    pl_hdr.missed_cnt =  0;
    pl_hdr.log_type =  PKTLOG_TYPE_DBG_PRINT;
    pl_hdr.size =  strlen(text);
    pl_hdr.timestamp = 0;
#ifdef CONFIG_AR900B_SUPPORT
    pl_hdr.type_specific_data = 0;
#endif

    log_size = strlen(text);

    dbg_print_s.dbg_print = (void *)
            pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);

    if(!dbg_print_s.dbg_print) {
        return A_ERROR;
    }

    qdf_mem_copy(dbg_print_s.dbg_print, text, log_size);

    return A_OK;
}

A_STATUS
process_tx_info(struct ol_pktlog_dev_t *pl_dev, void *data,
                        ath_pktlog_hdr_t *pl_hdr)
{
    struct ath_pktlog_info *pl_info;
    void *txdesc_hdr_ctl = NULL;

    /*
     *  Must include to process different types
     *  TX_CTL, TX_STATUS, TX_MSDU_ID, TX_FRM_HDR
     */
    size_t log_size = 0;
    size_t tmp_log_size = 0;

    pl_info = pl_dev->pl_info;

    tmp_log_size = sizeof(frm_hdr) + pl_hdr->size;
    log_size = pl_hdr->size;
    txdesc_hdr_ctl = (void *)
    pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
    if (txdesc_hdr_ctl == NULL) {
         return A_NO_MEMORY;
    }
    qdf_assert(txdesc_hdr_ctl);

    qdf_assert(pl_hdr->size < PKTLOG_MAX_TX_WORDS * sizeof(u_int32_t));
    qdf_mem_copy(txdesc_hdr_ctl, ((void *)data + pl_dev->pktlog_hdr_size),
                pl_hdr->size);

    return A_OK;
}


A_STATUS
process_pktlog_lite(void *context, void *log_data, uint16_t log_type)
{
    struct ol_pktlog_dev_t *pl_dev = (struct ol_pktlog_dev_t *)context;
    struct ath_pktlog_info *pl_info;
    ath_pktlog_hdr_t pl_hdr;
    struct ath_pktlog_rx_info rxstat_log;
    size_t log_size;
    qdf_nbuf_t log_nbuf = (qdf_nbuf_t)log_data;

    pl_info = pl_dev->pl_info;
    pl_hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
    pl_hdr.missed_cnt = 0;
    pl_hdr.log_type = log_type;
    pl_hdr.size = qdf_nbuf_len(log_nbuf);
    pl_hdr.timestamp = 0;
#ifdef CONFIG_AR900B_SUPPORT
    pl_hdr.type_specific_data = 0;
#endif
    log_size = pl_hdr.size;
    rxstat_log.rx_desc = (void *)pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
    if (rxstat_log.rx_desc == NULL) {
        return A_ERROR;
    }
    qdf_mem_copy(rxstat_log.rx_desc, qdf_nbuf_data(log_nbuf), pl_hdr.size);
    return A_OK;
}

A_STATUS
process_rx_desc_remote(ol_pktlog_dev_t *pl_dev, void *log_data)
{
    struct ath_pktlog_info *pl_info;
    ath_pktlog_hdr_t pl_hdr;
    struct ath_pktlog_rx_info rxstat_log;
    size_t log_size;
    qdf_nbuf_t log_nbuf = (qdf_nbuf_t)log_data;
    uint8_t pdev_id;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)pl_dev->scn;

    pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    pl_info = pl_dev->pl_info;
    pl_hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
    pl_hdr.missed_cnt = 0;
    pl_hdr.log_type = PKTLOG_TYPE_RX_STATBUF;
    pl_hdr.size = qdf_nbuf_len(log_nbuf);
    pl_hdr.timestamp = 0;
#ifdef CONFIG_AR900B_SUPPORT
    pdev_id = PKTLOG_SW2HW_MACID(pdev_id);
    PKTLOG_MAC_ID_SET(&pl_hdr,pdev_id);
#endif
    log_size = pl_hdr.size;
    rxstat_log.rx_desc = (void *)pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
    if (rxstat_log.rx_desc == NULL) {
        return A_ERROR;
    }
    qdf_mem_copy(rxstat_log.rx_desc, qdf_nbuf_data(log_nbuf), pl_hdr.size);
    return A_OK;
}



/* TODO::*/
A_STATUS
process_dbg_print(struct ol_pktlog_dev_t *pl_dev, void *data)
{
    ath_pktlog_hdr_t pl_hdr;
    size_t log_size;
    struct ath_pktlog_info *pl_info;
    struct ath_pktlog_dbg_print dbg_print_s;
    uint32_t *pl_tgt_hdr;
    struct ol_ath_softc_net80211 *scn;
    void *dbglog_handle;
    struct target_psoc_info *tgt_psoc_info;
    ol_ath_soc_softc_t *soc;

    if(!pl_dev) {
	qdf_info("Invalid pl dev in %s", __FUNCTION__);
        return A_ERROR;
    }
    if (!data) {
        qdf_info("Invalid data in %s", __FUNCTION__);
        return A_ERROR;
    }

    scn = (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                    scn->soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return -EINVAL;
    }

    if (!(dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info))) {
        qdf_info("%s: dbglog_handle is null ", __func__);
        return -EINVAL;
    }

    scn = (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    soc = scn->soc;
    pl_tgt_hdr = (uint32_t *)data;
    /*
     * Makes the short words (16 bits) portable b/w little endian
     * and big endian
     */
    pl_hdr.flags = (*(pl_tgt_hdr + ATH_PKTLOG_HDR_FLAGS_OFFSET) &
                                    ATH_PKTLOG_HDR_FLAGS_MASK) >>
                                    ATH_PKTLOG_HDR_FLAGS_SHIFT;
    pl_hdr.missed_cnt =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_MISSED_CNT_OFFSET) &
                                        ATH_PKTLOG_HDR_MISSED_CNT_MASK) >>
                                        ATH_PKTLOG_HDR_MISSED_CNT_SHIFT;
    pl_hdr.log_type =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_LOG_TYPE_OFFSET) &
                                        ATH_PKTLOG_HDR_LOG_TYPE_MASK) >>
                                        ATH_PKTLOG_HDR_LOG_TYPE_SHIFT;
    pl_hdr.size =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_SIZE_OFFSET) &
                                    ATH_PKTLOG_HDR_SIZE_MASK) >>
                                    ATH_PKTLOG_HDR_SIZE_SHIFT;
    pl_hdr.timestamp = *(pl_tgt_hdr + ATH_PKTLOG_HDR_TIMESTAMP_OFFSET);
#ifdef CONFIG_AR900B_SUPPORT
    pl_hdr.type_specific_data = 0;
#endif
    pl_info = pl_dev->pl_info;

    qdf_mem_set(dbglog_print_buffer,sizeof(dbglog_print_buffer), 0);
    fwdbg_parse_debug_logs(dbglog_handle, soc,
                            (char *)data +  pl_dev->pktlog_hdr_size, pl_hdr.size,
                            (void *)DBGLOG_PRT_PKTLOG);
    log_size = strlen(dbglog_print_buffer);
    dbg_print_s.dbg_print = (void *)
                    pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
    if (dbg_print_s.dbg_print == NULL) {
        return A_NO_MEMORY;
    }
    qdf_mem_copy(dbg_print_s.dbg_print, dbglog_print_buffer, log_size);
    return A_OK;
}


A_STATUS
process_offload_pktlog_3_0(struct ol_pktlog_dev_t *pl_dev, void *data)
{
    ath_pktlog_hdr_t pl_hdr;
    uint32_t *pl_tgt_hdr;
    A_STATUS rc = A_OK;

    if(!pl_dev) {
        qdf_info("Invalid context in %s\n", __FUNCTION__);
        return A_ERROR;
    }
    if(!data) {
        qdf_info("Invalid data in %s\n", __FUNCTION__);
        return A_ERROR;
    }
    pl_tgt_hdr = (uint32_t *)data;
    /*
     * Makes the short words (16 bits) portable b/w little endian
     * and big endian
     */
    pl_hdr.flags = (*(pl_tgt_hdr + ATH_PKTLOG_HDR_FLAGS_OFFSET) &
            ATH_PKTLOG_HDR_FLAGS_MASK) >>
        ATH_PKTLOG_HDR_FLAGS_SHIFT;
    pl_hdr.missed_cnt =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_MISSED_CNT_OFFSET) &
            ATH_PKTLOG_HDR_MISSED_CNT_MASK) >>
        ATH_PKTLOG_HDR_MISSED_CNT_SHIFT;
    pl_hdr.log_type =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_LOG_TYPE_OFFSET) &
            ATH_PKTLOG_HDR_LOG_TYPE_MASK) >>
        ATH_PKTLOG_HDR_LOG_TYPE_SHIFT;
    pl_hdr.size =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_SIZE_OFFSET) &
            ATH_PKTLOG_HDR_SIZE_MASK) >>
        ATH_PKTLOG_HDR_SIZE_SHIFT;
    pl_hdr.timestamp = *(pl_tgt_hdr + ATH_PKTLOG_HDR_TIMESTAMP_OFFSET);

    pl_hdr.type_specific_data = *(pl_tgt_hdr + ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET);
    rc = process_tx_info(pl_dev, data, &pl_hdr);

    return rc;
}
#endif /*REMOVE_PKT_LOG*/
