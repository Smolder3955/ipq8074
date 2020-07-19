/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include "pktlog_fmt.h"
#include <fcntl.h>

char *FrameType[] = { "mgt", "ctrl", "data", "reserved" };

char *FrameMgmtSubType[] = { "assoc_req", "assoc_resp", "reassoc_req",
    "reassoc_resp", "probe_req", "probe_resp", "unknown6", "unknown7",
    "beacon", "atim", "disassoc", "auth deauth"
};

char *FrameCtrlSubType[] =
    { "unknown0", "unknown1", "unknown2", "unknown3",
    "unknown4", "unknown5", "unknown6", "unknown7", "unknown8",
    "unknown9", "pspoll", "rts", "cts", "ack", "cfend", "cfendack"
};

char *FrameDataSubType[] =
    { "data", "data_cfack", "data_cfpoll", "data_cfack_cfpoll",
    "nodata", "nodata_cfack", "nodata_cfpoll", "nodata_cfack_cfpoll"
};


#if 0
void printframeCtrlDesc(unsigned short framectrl, int log_type,
                        unsigned status)
{
    int type = (framectrl & 0xc) >> 2;
    int subType = (framectrl & 0xf0) >> 4;

    if (framectrl & 0x4000)
        printf(" wep");
    if (framectrl & 0x2000)
        printf(" moreData");
    if (framectrl & 0x1000)
        printf(" pwr");
    if (framectrl & 0x800)
        printf(" retry");
    if (framectrl & 0x400)
        printf("moreFrag");
    if (framectrl & 0x200)
        printf(" down");
    if (framectrl & 0x100)
        printf(" up");

    if (type == 0) {
        printf(" %s %s", FrameType[type], FrameMgmtSubType[subType]);
    } else if (type == 1) {
        printf(" %s %s", FrameType[type], FrameCtrlSubType[subType]);
    } else if (type == 2) {
        printf(" %s %s", FrameType[type], FrameDataSubType[subType]);
    } else {
        printf(" ???");
    }

    if (log_type == PKTLOG_TYPE_RX) {
        if (status & PKTLOG_RXSTATUS_CRCERR)
            printf(" CRC");
    } else if (log_type == PKTLOG_TYPE_TXSTATUS) {
        if (status & PKTLOG_TXSTATUS_XRETRIES)
            printf(" Xretry");
        if (status & PKTLOG_TXSTATUS_FILTERED)
            printf(" TxFilt");
    }
}
#endif

void print_log(struct ath_pktlog_buf *log_buf, int log_size,
               unsigned long filter)
{
    int cur_rd_offset = log_buf->rd_offset;

    while (1) {
        struct ath_pktlog_hdr *log_hdr;
        int misc_cnt, log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        misc_cnt = log_hdr->flags & PHFLAGS_MISCCNT_MASK;

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        log_data_offset = ((log_size - log_data_offset) >= log_hdr->size) ?
            log_data_offset : 0;

        switch (log_hdr->log_type) {
        case PKTLOG_TYPE_TXCTL:
            if (filter & ATH_PKTLOG_TX) {
                int i;
                struct ath_pktlog_txctl *tx_log =
                    (struct ath_pktlog_txctl *) (log_buf->log_data +
                                              log_data_offset);

                printf("TC: %4X %4X", tx_log->framectrl, tx_log->seqctrl);
                for(i = 0; i < PKTLOG_MAX_TXCTL_WORDS; i++)
                    printf(" %8X", tx_log->txdesc_ctl[i]);

                /* Print misc log data if available */
                if (misc_cnt) {
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", tx_log->misc[i]);
                }
                printf("\n");
            }
            break;
        case PKTLOG_TYPE_TXSTATUS:
            if (filter & ATH_PKTLOG_TX) {
                int i;
                struct ath_pktlog_txstatus *tx_log =
                    (struct ath_pktlog_txstatus *) (log_buf->log_data +
                                              log_data_offset);

                printf("TS:");
                for(i = 0; i < PKTLOG_MAX_TXSTATUS_WORDS; i++)
                    printf(" %8X", tx_log->txdesc_status[i]);

                /* Print misc log data if available */
                if (misc_cnt) {
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", tx_log->misc[i]);
                }
                printf("\n");
            }
            break;
        case PKTLOG_TYPE_RX:
            if (filter & ATH_PKTLOG_RX) {
                int i;
                struct ath_pktlog_rx *rx_log =
                    (struct ath_pktlog_rx *) (log_buf->log_data +
                                              log_data_offset);

                printf("Rx: %4X %4X", rx_log->framectrl, rx_log->seqctrl);
                for(i = 0; i < PKTLOG_MAX_RXSTATUS_WORDS; i++)
                    printf(" %8X", rx_log->rxdesc_status[i]);   

                /* Print misc log data if available */
                if (misc_cnt) {
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", rx_log->misc[i]);
                }
                printf("\n");
            }
            break;
        case PKTLOG_TYPE_RCFIND:
            if (filter & ATH_PKTLOG_RCFIND) {
                struct ath_pktlog_rcfind *rcf_log =
                    (struct ath_pktlog_rcfind *) (log_buf->log_data +
                                                  log_data_offset);

                printf
                    ("RF: %4u %5d %5d %5d %5d %5u %5d %5d %5d %5u",
                     rcf_log->rate, rcf_log->rcRssiLast,
                     rcf_log->rcRssiLastPrev, rcf_log->rcRssiLastPrev2,
                     rcf_log->rssiReduce, rcf_log->rcProbeRate,
                     rcf_log->isProbing, rcf_log->primeInUse,
                     rcf_log->currentPrimeState, rcf_log->rcRateTableSize);

                /* Print misc log data if available */
                if (misc_cnt) {
                    int i;
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", rcf_log->misc[i]);
                }
                printf("\n");
            }
            break;
        case PKTLOG_TYPE_RCUPDATE:
            if (filter & ATH_PKTLOG_RCUPDATE) {
                struct ath_pktlog_rcupdate *rcu_log =
                    (struct ath_pktlog_rcupdate *) (log_buf->
                                                    log_data +
                                                    log_data_offset);
                int i;

                printf
                    ("RU: %4u %5d %5d %5d %5d %5u %5d %5d %5d %5u %5d %5d %5u",
                     rcu_log->txRate, rcu_log->rssiAck,
                     rcu_log->rcRssiLast, rcu_log->rcRssiLastLkup,
                     rcu_log->rcRssiLastPrev, rcu_log->rcRssiLastPrev2,
                     rcu_log->Xretries, rcu_log->retries,
                     rcu_log->rcProbeRate, rcu_log->rcRateMax,
                     rcu_log->useTurboPrime, rcu_log->currentBoostState,
                     rcu_log->rcHwMaxRetryRate);

                printf("\nthr:");

                for (i = 0; i < MAX_TX_RATE_TBL; i++)
                    printf("%3d ", rcu_log->rcRssiThres[i]);

                printf("\nper:");

                for (i = 0; i < MAX_TX_RATE_TBL; i++)
                    printf("%3u ", rcu_log->rcPer[i]);

                /* Print misc log data if available */
                if (misc_cnt) {
                    int i;
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", rcu_log->misc[i]);
                }

                printf("\n");
            }
            break;
        case PKTLOG_TYPE_ANI:
            if (filter & ATH_PKTLOG_ANI) {
                struct ath_pktlog_ani *ani_log =
                    (struct ath_pktlog_ani *) (log_buf->log_data +
                                               log_data_offset);

                printf
                    ("NI: %5u %5u %5u %5u %5u %5u %5u %5u %5u %5u %5i",
                     ani_log->phyStatsDisable, ani_log->noiseImmunLvl,
                     ani_log->spurImmunLvl, ani_log->ofdmWeakDet,
                     ani_log->cckWeakThr, ani_log->firLvl,
                     ani_log->listenTime, ani_log->cycleCount,
                     ani_log->ofdmPhyErrCount, ani_log->cckPhyErrCount,
                     ani_log->rssi);

                /* Print misc log data if available */
                if (misc_cnt) {
                    int i;
                    printf("\n    Misc: ");
                    /* Print misc log data if available */
                    for (i = 0; i < misc_cnt; i++)
                        printf("%d ", ani_log->misc[i]);
                }
                printf("\n");
            }
            break;
        case PKTLOG_TYPE_RX_CBF:
           if ((filter & ATH_PKTLOG_CBF) ||
               (filter & ATH_PKTLOG_H_INFO) ||
               (filter & ATH_PKTLOG_STEERING)) {
                    struct ath_pktlog_cbf_submix *cbf_mix_log = (struct ath_pktlog_cbf_submix *) (log_buf->log_data + log_data_offset);
                    printf(" CBF:log_hdr->log_type =%d  cbf_mix_log->sub_type=%d",log_hdr->log_type,cbf_mix_log->sub_type);
                    printf("PKTLOG TYPE IS RX CBF\n");
            }
            break;
        default:
            printf("Error: Unknown log type\n");
            break;
        }

        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, log_size);
        if (cur_rd_offset == log_buf->wr_offset)
            break;
    }
}

void
dump_log(struct ath_pktlog_buf *log_buf, int log_size)
{
    int cur_rd_offset = log_buf->rd_offset;
    int j = 0;

    /* For the perl scripts to parse */
    printf(":::: PKTLOG START ::::\n");
    while (1) {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;
        int siz,i;
        u_int8_t* buf;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        log_data_offset = ((log_size - log_data_offset) >= log_hdr->size) ?
            log_data_offset : 0;

	/* We dump the header part first */
        siz=sizeof(struct ath_pktlog_hdr); 
        buf=(u_int8_t*)log_hdr;
        for(i=0;i<siz;i++) { 
            printf("%02x",*buf);
            if((j+=2)>=78) { 
                printf("\n");
                j=0;
            }
            buf++;
        }
        /* We dump the data part here */
        buf=(u_int8_t*)(log_buf->log_data+log_data_offset);
        siz=log_hdr->size;
        for(i=0;i<siz;i++) { 
            printf("%02x",*buf);
            if((j+=2)>=78) { 
                printf("\n");
                j=0;
            }
            buf++;
        }
        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, log_size);
        if (cur_rd_offset == log_buf->wr_offset)
            break;
    }
    printf("\n:::: PKTLOG END ::::\n");
}

typedef unsigned long cntr_t;
typedef struct list_entry {
    void *data;
    unsigned long id;
    struct list_entry *nxt;
} list_entry_t;


list_entry_t *getEntryById(list_entry_t ** list_head, unsigned long id)
{
    list_entry_t *cptr, *cptr_prev = 0;

    for (cptr = *list_head; cptr && (cptr->id < id); cptr = cptr->nxt) {
        cptr_prev = cptr;
    }

    if (cptr && (cptr->id == id))
        return cptr;

    cptr = (list_entry_t *) malloc(sizeof(list_entry_t));
    if (cptr == NULL) {
        printf("malloc of list entry failed\n");
        exit(-1);
    }

    cptr->id = id;
    cptr->data = 0;

    if (cptr_prev) {
        cptr->nxt = cptr_prev->nxt;
        cptr_prev->nxt = cptr;
    } else {
        cptr->nxt = *list_head;
        *list_head = cptr;
    }

    return cptr;
}

#define RATE_TABLE_SIZE  64
#define MAX_RETRIES      15
#define PKT_MAX_LEN      7935

/* TBD: Add 11n rates */
int rateTable[RATE_TABLE_SIZE] =
    { 0, 1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54 };


int rate_from_code(int code)
{
    if (code < RATE_TABLE_SIZE)
        return rateTable[code];
    return -1;
}


typedef struct rx_stats {
    /* Rx Counters */
    cntr_t rx_cnt_per_len;
    cntr_t rx_dup_per_len;
    cntr_t rx_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_dup_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_crc_per_len;
    cntr_t rx_crc_per_rate[RATE_TABLE_SIZE];
    int rx_rssi0_sum_per_rate[RATE_TABLE_SIZE];
    int rx_rssi1_sum_per_rate[RATE_TABLE_SIZE];
    int rx_rssi2_sum_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_rssi0_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_rssi1_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_rssi2_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t rx_retries_per_rate[RATE_TABLE_SIZE];

    list_entry_t *rx_ipt_per_rate_retries[RATE_TABLE_SIZE][MAX_RETRIES];
    list_entry_t
        *rx_ipt_per_rate_retries_phy[RATE_TABLE_SIZE][MAX_RETRIES];
} rx_stats_t;

typedef struct tx_stats {
    /* Tx Counters */
    cntr_t tx_cnt_per_len;
    cntr_t tx_cnt_per_rate[RATE_TABLE_SIZE];
    int tx_rssi0_sum_per_rate[RATE_TABLE_SIZE];
    int tx_rssi1_sum_per_rate[RATE_TABLE_SIZE];
    int tx_rssi2_sum_per_rate[RATE_TABLE_SIZE];
    cntr_t tx_rssi0_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t tx_rssi1_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t tx_rssi2_cnt_per_rate[RATE_TABLE_SIZE];
    cntr_t tx_sretries_per_rate[RATE_TABLE_SIZE];
    cntr_t tx_lretries_per_rate[RATE_TABLE_SIZE];
} tx_stats_t;

#if 0
void print_stats(struct ath_pktlog_buf *log_buf, int log_size, int opt_i,
                 int opt_I)
{
    /* TBD: Add rssi stats */

    list_entry_t *rs_list = NULL;
    list_entry_t *ts_list = NULL;
    int seq = -1, last_rx_rate = -1, last_rx_tstamp;
    cntr_t rx_pkts = 0, tx_pkts = 0, total_rxlen = 0, total_txlen = 0;
    unsigned rx_dups = 0;
    int start_offset, end_offset;
    int rateIdx, saw_phy_err = 0;
    int delta_usec, rx_retries = 0;
    int first_offset, last_offset, last_nxt_offset;
    unsigned totalTime, totalTimeCollected, totalTimeOutsideMeasurement;
    int rate;
    int cur_rd_offset = log_buf->rd_offset;

    cntr_t probe_reqs_in_measurement_window,
        probe_resps_in_measurement_window;
    cntr_t probe_reqs_out_measurement_window,
        probe_resps_out_measurement_window;

    start_offset = cur_rd_offset;

    first_offset = last_offset = -1;

    /* Find boundaries */
    do {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        log_data_offset = ((log_size - log_data_offset) >= log_hdr->size) ?
            log_data_offset : 0;

        if (log_hdr->log_type == ATH_PKTLOG_RX) {
            struct ath_pktlog_rx *rx_log =
                (struct ath_pktlog_rx *) (log_buf->log_data +
                                          log_data_offset);
            if (!(rx_log->status & PKTLOG_RXSTATUS_CRCERR) && rx_log->datalen > 0) {
                if (first_offset == -1)
                    first_offset = cur_rd_offset;
                last_offset = cur_rd_offset;
            }
        } else if (log_hdr->log_type == ATH_PKTLOG_TX) {
            struct ath_pktlog_tx *tx_log =
                (struct ath_pktlog_tx *) (log_buf->log_data +
                                          log_data_offset);
            if (!(tx_log->status & (PKTLOG_TXSTATUS_XRETRIES | PKTLOG_TXSTATUS_FILTERED))
                && tx_log->length > 0) {
                if (first_offset == -1)
                    first_offset = cur_rd_offset;
                last_offset = cur_rd_offset;
            }
        }

        end_offset = cur_rd_offset;
        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, log_size);

    } while (cur_rd_offset != log_buf->wr_offset);

    if (first_offset == -1) {
        printf("Boundary check failed\n");
        return;
    }

    totalTime =
        ((struct ath_pktlog_hdr *) (log_buf->log_data +
                                    last_offset))->timestamp -
        ((struct ath_pktlog_hdr *) (log_buf->log_data +
                                    first_offset))->timestamp;
    cur_rd_offset = first_offset;

    last_nxt_offset = last_offset;
    PKTLOG_MOV_RD_IDX(last_nxt_offset, log_buf, log_size);

    printf("fi..%d, li..%d\n", first_offset, last_offset);

    do {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        log_data_offset = ((log_size - log_data_offset) >= log_hdr->size) ?
            log_data_offset : 0;

        switch (log_hdr->log_type) {
        case ATH_PKTLOG_RX:{
                rx_stats_t *rx_stats;
                list_entry_t *rs_entry;

                struct ath_pktlog_rx *rx_log =
                    (struct ath_pktlog_rx *) (log_buf->log_data +
                                              log_data_offset);

                if (rx_log->status & PKTLOG_RXSTATUS_FIFO_OVERRUN)
                    continue;


                rs_entry = getEntryById(&rs_list, rx_log->datalen);

                if (rs_entry->data == 0) {
                    rs_entry->data = malloc(sizeof(rx_stats_t));
                    if (rs_entry->data == NULL) {
                        printf("malloc of rx stats failed\n");
                        exit(-1);
                    }
                    memset(rs_entry->data, 0, sizeof(rx_stats_t));
                }

                rx_stats = (rx_stats_t *) (rs_entry->data);

                rateIdx = rx_log->rate;

                /* Count only if no crc error */
                /* TBD: Do we need per antenna stats */
                if (!(rx_log->status & PKTLOG_RXSTATUS_CRCERR)) {
                    if (seq == rx_log->seqctrl) {
                        rx_stats->rx_dup_per_len++;
                        rx_stats->rx_dup_per_rate[rateIdx]++;
                        rx_dups++;
                    } else {
                        rx_stats->rx_cnt_per_len++;
                        rx_stats->rx_cnt_per_rate[rateIdx]++;
                        /* TBD: should we add per rssi counts */
                        rx_pkts++;
                        total_rxlen += rx_log->datalen;
                    }
                } else {
                    /* count CRC errors */
                    rx_stats->rx_crc_per_len++;
                    rx_stats->rx_crc_per_rate[rateIdx]++;
                }


                /* Total RSSI */
                rx_stats->rx_rssi0_sum_per_rate[rateIdx] += rx_log->rssi00;
                rx_stats->rx_rssi1_sum_per_rate[rateIdx] += rx_log->rssi01;
                rx_stats->rx_rssi2_sum_per_rate[rateIdx] += rx_log->rssi02;
                rx_stats->rx_rssi0_cnt_per_rate[rateIdx]++;
                rx_stats->rx_rssi1_cnt_per_rate[rateIdx]++;
                rx_stats->rx_rssi2_cnt_per_rate[rateIdx]++;


                if (rx_log->framectrl * 0x800) {
                    rx_stats->rx_retries_per_rate[rateIdx]++;
                    rx_retries++;
                } else {
                    rx_retries = 0;
                }

                /* Inter packet arrival stats */

                if (last_rx_rate == rx_log->rate) {
                    delta_usec = rx_log->tstamp - last_rx_tstamp;

                    if (delta_usec < 0)
                        delta_usec += 32768;

                    ((cntr_t) (getEntryById
                               (&
                                (rx_stats->rx_ipt_per_rate_retries[rateIdx]
                                 [rx_retries]), delta_usec))->data)++;
                }

                if (!(rx_log->status & PKTLOG_RXSTATUS_PHYERR)) {
                    if ((last_rx_rate == rx_log->rate)
                        && (saw_phy_err == 1)) {
                        ((cntr_t) (getEntryById
                                   (&
                                    (rx_stats->
                                     rx_ipt_per_rate_retries_phy[rateIdx]
                                     [rx_retries]), delta_usec))->data)++;
                        saw_phy_err = 0;
                    }

                    last_rx_rate = rx_log->rate;
                    last_rx_tstamp = rx_log->tstamp;
                } else {
                    saw_phy_err = 1;
                }
                break;
            }
        case ATH_PKTLOG_TX:{
                struct ath_pktlog_tx *tx_log =
                    (struct ath_pktlog_tx *) (log_buf->log_data +
                                              log_data_offset);

                tx_stats_t *tx_stats;
                list_entry_t *ts_entry;

                if (tx_log->status & PKTLOG_TXSTATUS_DATA_UNDERRUN)
                    continue;

                ts_entry = getEntryById(&ts_list, tx_log->length);

                if (ts_entry->data == 0) {
                    ts_entry->data = malloc(sizeof(tx_stats_t));
                    if (ts_entry->data == NULL) {
                        printf("malloc of tx stats failed\n");
                        exit(-1);
                    }
                    memset(ts_entry->data, 0, sizeof(tx_stats_t));
                }

                tx_stats = (tx_stats_t *) (ts_entry->data);

                rateIdx = tx_log->rate;

                /* TBD: Do we need per antenna stats */
                if (!(tx_log->status & PKTLOG_TXSTATUS_XRETRIES)) {
                    tx_stats->tx_cnt_per_len++;
                    tx_stats->tx_cnt_per_rate[rateIdx]++;
                    tx_stats->tx_rssi0_sum_per_rate[rateIdx] +=
                        tx_log->rssi00;
                    tx_stats->tx_rssi1_sum_per_rate[rateIdx] +=
                        tx_log->rssi01;
                    tx_stats->tx_rssi2_sum_per_rate[rateIdx] +=
                        tx_log->rssi02;
                    tx_stats->tx_rssi0_cnt_per_rate[rateIdx]++;
                    tx_stats->tx_rssi1_cnt_per_rate[rateIdx]++;
                    tx_stats->tx_rssi2_cnt_per_rate[rateIdx]++;
                    tx_pkts++;
                    total_txlen += tx_log->length;
                }

                tx_stats->tx_sretries_per_rate[rateIdx] +=
                    tx_log->shortretry;
                tx_stats->tx_lretries_per_rate[rateIdx] +=
                    tx_log->longretry;
                break;
            }
        default:
            last_rx_rate = -1;
        }
        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, log_size);

    } while (cur_rd_offset != last_nxt_offset);
#if 0
    printf("Throughput mode=%s,  short_11b=%d, ack rate=%s\n",
           throughput_mode, throughput_is_short_11b,
           throughput_use_1_6_mbps_ack ? "1 or 6Mbps" :
           "Highest mandatory rate");
# endif

    /* Display Rx and Tx stats */

    if ((rx_pkts == 0) || (totalTime == 0)) {
        printf("No packets received\n\n");
    } else {
        list_entry_t *rs_entry;

        printf
            ("Total of %d bytes (non-duplicate) rx in %d pkts in %d ms\n",
             total_rxlen, rx_pkts, totalTime);
        printf("   Average actual rx throughput of %.3f Mbps\n",
               total_rxlen * 0.008 / totalTime);
        printf
            ("   Not including %d duplicate packets  (%5.1f%% of total)\n",
             rx_dups, (float) rx_dups / (rx_dups + rx_pkts) * 100);

        for (rs_entry = rs_list; rs_entry; rs_entry = rs_entry->nxt) {
            rx_stats_t *rstats = (rx_stats_t *) (rs_entry->data);

            printf
                ("   %4d bytes: %5d pkts (%5.1f%% by pkt, %5.1f%% by data)\n",
                 rs_entry->id, rstats->rx_cnt_per_len,
                 (float) rstats->rx_cnt_per_len / rx_pkts * 100,
                 (float) rstats->rx_cnt_per_len * (rs_entry->id) /
                 total_rxlen * 100);

            for (rate = 0; rate < RATE_TABLE_SIZE; rate++) {
                float net_per;

                if ((rstats->rx_cnt_per_rate[rate] == 0) &&
                    (rstats->rx_dup_per_rate[rate] == 0) &&
                    (rstats->rx_crc_per_rate[rate] == 0) &&
                    (rstats->rx_retries_per_rate[rate] == 0)) {
                    continue;
                }

                net_per = (float) rstats->rx_retries_per_rate[rate] /
                    (rstats->rx_cnt_per_rate[rate] +
                     rstats->rx_retries_per_rate[rate]);


#if 0
                throughput_ack_rate_mbps = 0;

                if (throughput_use_1_6_mbps_ack) {
                    if ((rate <= 11) && (rate != 6)) {
                        throughput_ack_rate_mbps = 1;
                    } else {
                        throughput_ack_rate_mbps = 6;
                    }
                }

                pred_throughput = compute_throughput(net_per, $rate,
                                                     throughput_ack_rate_mbps,
                                                     len,
                                                     throughput_is_short_11b,
                                                     throughput_mode);

                /* Compute time w/o cw */
                pkt_time_no_cw_usec = compute_throughput(-1, rate,
                                                         throughput_ack_rate_mbps,
                                                         len,
                                                         throughput_is_short_11b,
                                                         throughput_mode);

                if ($pred_throughput > 0) {
                    pred_time = len * 8 * cnt_by_rate[rate]
                        / pred_throughput / 1000;
                } else {
                    $pred_time = 0;
                }
                $total_rx_pred_time += $pred_time;
#endif
                printf
                    ("          Rate %3d Mbps: %5d pkts rcvd (%5.1f%% of this len)\n",
                     rate_from_code(rate), rstats->rx_cnt_per_rate[rate],
                     (float) rstats->rx_cnt_per_rate[rate] /
                     rstats->rx_cnt_per_len * 100);

                printf
                    ("                >= %5d total retries.  PER>=%5.1f%%\n",
                     rstats->rx_retries_per_rate[rate], net_per * 100);

                printf
                    ("                %5d duplicates not counted (%5.1f%%)\n",
                     rstats->rx_dup_per_rate[rate],
                     (float) rstats->rx_dup_per_rate[rate] /
                     (rstats->rx_cnt_per_rate[rate] +
                      rstats->rx_dup_per_rate[rate]) * 100);

                printf("                %5d CRC errors",
                       rstats->rx_crc_per_rate[rate]);

                if (rstats->rx_retries_per_rate[rate] > 0) {
                    printf(" (%5.1f%% of retries)\n",
                           (float) rstats->rx_crc_per_rate[rate] /
                           rstats->rx_retries_per_rate[rate] * 100);
                } else {
                    printf("\n");
                }
#if 0
                printf
                    ("                  Predicted throughput<=%6.3f Mbps   Time>=%4.0f ms\n",
                     pred_throughput, pred_time);

                if ($len > 64) {
                    printf
                        ("                  Predicted UDP throughput<=%6.3f Mbps\n",
                         pred_throughput * (len - 64) / len);
                }
#endif

                if (rstats->rx_rssi0_cnt_per_rate[rate] > 0) {
                    printf("                    Ave RSSI0: %4.1f dB\n",
                           (float) rstats->rx_rssi0_sum_per_rate[rate] /
                           rstats->rx_rssi0_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave RSSI0: ---- dB\n");
                }

                if (rstats->rx_rssi1_cnt_per_rate[rate] > 0) {
                    printf("                    Ave RSSI1: %4.1f dB\n",
                           (float) rstats->rx_rssi1_sum_per_rate[rate] /
                           rstats->rx_rssi1_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave RSSI1: ---- dB\n");
                }

                if (rstats->rx_rssi2_cnt_per_rate[rate] > 0) {
                    printf("                    Ave RSSI2: %4.1f dB\n",
                           (float) rstats->rx_rssi2_sum_per_rate[rate] /
                           rstats->rx_rssi2_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave RSSI2: ---- dB\n");
                }

                /* RSSI stats per antenna were removed */

                printf("\n");

                if (1) {
                    int retries;
                    cntr_t total_count = 0;
                    cntr_t total_count_noPhy = 0;
                    cntr_t total_delta_usec = 0;
                    cntr_t total_delta_usec_noPhy = 0;

                    for (retries = 0; retries < MAX_RETRIES; retries++) {
                        list_entry_t *cptr =
                            rstats->rx_ipt_per_rate_retries[rate][retries];

                        for (; cptr; cptr = cptr->nxt) {
                            cntr_t count = (cntr_t) (cptr->data);
                            total_count += count;
                            total_delta_usec += count * cptr->id;
                        }
                    }

                    total_delta_usec_noPhy = total_delta_usec;
                    total_count_noPhy = total_count;

                    for (retries = 0; retries < MAX_RETRIES; retries++) {
                        list_entry_t *cptr =
                            rstats->
                            rx_ipt_per_rate_retries_phy[rate][retries];

                        for (; cptr; cptr = cptr->nxt) {
                            cntr_t count = (cntr_t) (cptr->data);
                            total_count_noPhy -= count;
                            total_delta_usec_noPhy -= count * cptr->id;
                        }
                    }
                    if ((total_count != 0) && (total_delta_usec != 0)) {
                        printf
                            ("   Average inter-packet spacing = %d us  (throughput=%.3f Mbps) \n",
                             total_delta_usec / total_count,
                             (float) total_count / total_delta_usec *
                             (rs_entry->id) * 8);
                    }

                    if ((total_count_noPhy != 0)
                        && (total_delta_usec_noPhy != 0)) {
                        printf
                            ("   Average inter-packet spacing (without PhyErr gap)= %d us  (throughput=%.3f Mbps)\n",
                             total_delta_usec_noPhy / total_count_noPhy,
                             (float) total_count_noPhy /
                             total_delta_usec_noPhy * (rs_entry->id) * 8);
                    }
                }

                if (opt_I | opt_i) {
                    unsigned best_slot_time = 0, best_slot_offset = 0;
                    int retries;
#if 0
                    printf
                        ("                  Base packet+ACK (no c/w) time = %d usec\n",
                         $pkt_time_no_cw_usec);
                    printf
                        ("                   (note that there are slight discrepancies to be resolved)\n");
#endif

                    /* Determine slot time and offset */

                    for (retries = 0; retries < MAX_RETRIES; retries++) {

                        list_entry_t *rx_ipt_retries_list =
                            rstats->rx_ipt_per_rate_retries[rate][retries];
                        list_entry_t *rx_ipt_retries_phy_list =
                            rstats->
                            rx_ipt_per_rate_retries_phy[rate][retries];

                        list_entry_t *cptr;
                        cntr_t total_count = 0;
                        cntr_t total_count_phy = 0;

                        for (cptr = rx_ipt_retries_list; cptr;
                             cptr = cptr->nxt) {
                            total_count += (cntr_t) (cptr->data);
                        }

                        if (opt_i && rx_ipt_retries_list) {
                            printf
                                ("                  Interpacket histogram retries=%d: \n",
                                 retries);

                            list_entry_t *cptr2;
                            cntr_t countsum = 0;
#if 0
                            cntr_t total_count = 0;
                            for (; cptr2; cptr2 = cptr2->nxt) {
                                total_count += cptr2->data;
                            }
#endif


                            for (cptr2 = rx_ipt_retries_list; cptr2;
                                 cptr2 = cptr2->nxt) {

                                cntr_t count = (cntr_t) cptr2->data;

                                countsum += count;

                                printf
                                    ("                   %4d usec:  %4d (%5.1f%%)   Sum=%5d (%5.1f%%)\n",
                                     cptr2->id, count,
                                     (float) count / total_count * 100,
                                     countsum,
                                     (float) countsum / total_count * 100);
                            }
                            printf("\n");
                        }


                        /* Determine slot time and offset */
                        for (cptr = rx_ipt_retries_phy_list; cptr;
                             cptr = cptr->nxt) {
                            total_count_phy += (cntr_t) (cptr->data);
                        }


                        if (opt_i && rx_ipt_retries_phy_list) {
                            list_entry_t *cptr2;
                            cntr_t countsum_phy = 0;
                            printf
                                ("                  PhyErr Interpacket histogram retries=%d: \n",
                                 retries);
#if 0
                            cntr_t total_count_phy = 0;
                            for (; cptr2; cptr2 = cptr2->nxt) {
                                total_count_phy += cptr->data;
                            }
#endif


                            for (cptr2 = rx_ipt_retries_phy_list; cptr2;
                                 cptr2 = cptr2->nxt) {
                                cntr_t count = (cntr_t) cptr2->data;

                                countsum_phy += count;
                                printf
                                    ("                   %4d usec:  %4d (%5.1f%%)   Sum=%5d (%5.1f%%)\n",
                                     delta_usec, count,
                                     (float) count / total_count_phy * 100,
                                     countsum_phy,
                                     (float) countsum_phy /
                                     total_count_phy * 100);
                            }
                            printf("\n");
                        }
                        /* Determine the slot time by histogramming interpacket arrival
                           time modulo slot time 
                           Discount ones arriving before 10 usec after 10% of the packets have passed
                           to avoid bunching */
                        if (rx_ipt_retries_list) {
                            unsigned slot_time_score,
                                slot_time_best_offset;
                            unsigned cum_sum = 0;
                            unsigned delta_usec_thresh = 0;
                            list_entry_t *cptr_trimmed = NULL;

                            for (cptr = rx_ipt_retries_list; cptr;
                                 cptr = cptr->nxt) {
                                cum_sum += (cntr_t) (cptr->data);
                                if (cum_sum >= total_count / 10) {
                                    delta_usec_thresh = cptr->id + 10;
                                    break;
                                }
                            }

                            /* Trim delta_usecs into delta_usecs_trimmed */
                            for (cptr = rx_ipt_retries_list;
                                 cptr && (cptr->id < delta_usec_thresh);
                                 cptr = cptr->nxt);
                            cptr_trimmed = cptr;
                            cntr_t total_trimmed_count = 0;

                            for (; cptr; cptr = cptr->nxt) {
                                total_trimmed_count +=
                                    (cntr_t) (cptr->data);
                            }

                            if (best_slot_time == 0) {
                                float min_norm_std = 0, norm_std;
                                int slot_time;

                                for (slot_time = 2; slot_time < 64;
                                     slot_time++) {
                                    int slot_time2 = slot_time / 2;
                                    unsigned modulo_cnt[64];
                                    int offset, peak_offset;
                                    cntr_t peak_cnt;
                                    float std;

                                    for (cptr = cptr_trimmed; cptr;
                                         cptr = cptr->nxt) {
                                        modulo_cnt[cptr->id %
                                                   slot_time] +=
                                            (cntr_t) (cptr->data);
                                    }

                                    /* Find peak offset */
                                    for (offset = 0; offset < slot_time;
                                         offset++) {
                                        if (peak_cnt <= modulo_cnt[offset]) {
                                            peak_cnt = modulo_cnt[offset];
                                            peak_offset = offset;
                                        }
                                    }

                                    /*  Compute std relative to peak_offset */
                                    unsigned std_sum = 0;
                                    for (offset = 0; offset < slot_time;
                                         offset++) {
                                        int delta_offset =
                                            offset - peak_offset;
                                        if (delta_offset > slot_time2) {
                                            delta_offset -= slot_time;
                                        } else if (delta_offset <
                                                   -slot_time2) {
                                            delta_offset = slot_time;
                                        }
                                        std_sum +=
                                            modulo_cnt[offset] *
                                            delta_offset * delta_offset;
                                    }
                                    std =
                                        sqrt(std_sum /
                                             total_trimmed_count);
                                    norm_std = std / slot_time;
                                    if ((best_slot_time == 0)
                                        || (min_norm_std > norm_std)) {
                                        min_norm_std = norm_std;
                                        best_slot_time = slot_time;
                                        best_slot_offset = peak_offset;
                                    }

                                }
                                printf
                                    ("                  Measured slot time=%d usec\n\n",
                                     best_slot_time, best_slot_offset);
                            }

                            if (opt_I) {
                                printf
                                    ("                  Distilled histogram info retries=%d: \n",
                                     retries);
                                /* Go back through deltas and bin to closes slot interval.
                                   Compute mean and std for that slot */
                                typedef struct {
                                    unsigned long sum;
                                    unsigned long sqsum;
                                    unsigned long cnt;
                                    int min;
                                    int max;
                                } per_slot_offset_stats_t;

                                list_entry_t *ps_stats_list = NULL;
                                list_entry_t *ps_node;
                                per_slot_offset_stats_t *per_slot_stats;

                                for (cptr = rx_ipt_retries_list; cptr;
                                     cptr = cptr->nxt) {
                                    cntr_t count = (cntr_t) (cptr->data);
                                    unsigned long delta_usec = cptr->id;
                                    unsigned long delta_usec_interval =
                                        floor((double) ((delta_usec -
                                                         best_slot_offset)
                                                        / best_slot_time +
                                                        0.5));
                                    unsigned long delta_usec_offset =
                                        delta_usec -
                                        (delta_usec_interval *
                                         best_slot_time +
                                         best_slot_offset);
                                    ps_node =
                                        getEntryById(&ps_stats_list,
                                                     delta_usec_interval);
                                    if (ps_node->data == 0) {
                                        ps_node->data = malloc(sizeof
                                                               (per_slot_offset_stats_t));
                                        if (ps_node->data == NULL) {
                                            printf
                                                ("malloc of per slot stats failed\n");
                                            exit(-1);
                                        }
                                        memset(ps_node->data, 0,
                                               sizeof
                                               (per_slot_offset_stats_t));
                                    }

                                    per_slot_stats =
                                        (per_slot_offset_stats_t *)
                                        ps_node->data;
                                    per_slot_stats->sum +=
                                        delta_usec_offset * count;
                                    per_slot_stats->sqsum +=
                                        delta_usec_offset *
                                        delta_usec_offset * count;
                                    if ((per_slot_stats->cnt == 0)
                                        || (per_slot_stats->max <
                                            delta_usec)) {
                                        per_slot_stats->max = delta_usec;
                                    }

                                    if ((per_slot_stats->cnt == 0) ||
                                        (per_slot_stats->min >
                                         delta_usec)) {
                                        per_slot_stats->min = delta_usec;
                                    }
                                    per_slot_stats->cnt += count;
                                }

                                cntr_t countsum = 0;
                                for (ps_node = ps_stats_list; ps_node;
                                     ps_node = ps_node->nxt) {
                                    per_slot_stats =
                                        (per_slot_offset_stats_t *)
                                        ps_node->data;
                                    cntr_t cnt = per_slot_stats->cnt;
                                    countsum += cnt;
                                    printf
                                        ("                   %4d-%4dus (%6.1f std %.1f): %4d (%4.1f%%) Sum=%5.1f%%\n",
                                         per_slot_stats->min,
                                         per_slot_stats->max,
                                         (float) per_slot_stats->sum /
                                         cnt +
                                         ps_node->id * best_slot_time +
                                         best_slot_offset,
                                         (float) sqrt((float)
                                                      per_slot_stats->
                                                      sqsum / cnt -
                                                      ((float)
                                                       per_slot_stats->
                                                       sum / cnt) *
                                                      ((float)
                                                       per_slot_stats->
                                                       sum / cnt)), cnt,
                                         (float) cnt / total_count * 100,
                                         (float) countsum / total_count *
                                         100);
                                }

                                printf("\n");
                            }
                        }
                    }
                }
            }
        }
    }

    if ((tx_pkts == 0) || (totalTime == 0)) {
        printf("No packets transmitted\n\n");
    } else {
        list_entry_t *ts_entry = NULL;

        printf("Total of %d bytes tx in %d pkts in %d ms\n",
               total_txlen, tx_pkts, totalTime);
        printf("   Average tx actual throughput of %.3f Mbps\n",
               total_txlen * 0.008 / totalTime);

        for (ts_entry = ts_list; ts_entry; ts_entry = ts_entry->nxt) {
            tx_stats_t *tstats = (tx_stats_t *) (ts_entry->data);

            printf
                ("   %4d bytes: %5d pkts (%5.1f%% by pkt, %5.1f%% by data)\n",
                 ts_entry->id, tstats->tx_cnt_per_len,
                 (float) tstats->tx_cnt_per_len / tx_pkts * 100,
                 (float) tstats->tx_cnt_per_len * ts_entry->id /
                 total_txlen * 100);

            for (rate = 0; rate < RATE_TABLE_SIZE; rate++) {
                if ((tstats->tx_cnt_per_rate[rate] == 0) &&
                    ((tstats->tx_sretries_per_rate[rate]
                      + tstats->tx_lretries_per_rate[rate]) == 0)) {
                    continue;
                }

                float net_per =
                    (float) (tstats->tx_sretries_per_rate[rate] +
                             tstats->tx_lretries_per_rate[rate]) /
                    (tstats->tx_cnt_per_rate[rate] +
                     tstats->tx_sretries_per_rate[rate] +
                     tstats->tx_lretries_per_rate[rate]);
#if 0
                throughput_ack_rate_mbps = 0;

                if (throughput_use_1_6_mbps_ack) {
                    if ((rate <= 11) && (rate != 6)) {
                        throughput_ack_rate_mbps = 1;
                    } else {
                        throughput_ack_rate_mbps = 6;
                    }
                }

                pred_throughput = compute_throughput(net_per, rate,
                                                     throughput_ack_rate_mbps,
                                                     len,
                                                     throughput_is_short_11b,
                                                     throughput_mode);

                if (pred_throughput > 0) {
                    pred_time =
                        len * 8 * tstats->tx_cnt_per_rate[rate] /
                        pred_throughput / 1000;
                } else {
                    pred_time = 0;
                }
                total_tx_pred_time += pred_time;
#endif

                printf
                    ("          Rate %3d Mbps: %5d pkts sent (%5.1f%% of this len)\n",
                     rate_from_code(rate), tstats->tx_cnt_per_rate[rate],
                     (float) (tstats->tx_cnt_per_rate[rate] /
                              tstats->tx_cnt_per_len) * 100);

                printf
                    ("                %5d short retries, %5d long retries.  PER=%5.1f%%\n",
                     tstats->tx_sretries_per_rate[rate],
                     tstats->tx_lretries_per_rate[rate], net_per * 100);
#if 0
                printf
                    ("                  Predicted throughput=%6.3f Mbps   Time=%4.0f ms\n",
                     pred_throughput, pred_time);

                if ($len > 64) {
                    printf
                        ("                  Predicted UDP throughput=%6.3f Mbps\n",
                         pred_throughput * ($len - 64) / len);
                }
#endif
                if (tstats->tx_rssi0_cnt_per_rate[rate] > 0) {
                    printf
                        ("                    Ave Ack RSSI0: %4.1f dB\n",
                         (float) tstats->tx_rssi0_sum_per_rate[rate] /
                         tstats->tx_rssi0_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave Ack RSSI0: ---- dB\n");
                }

                if (tstats->tx_rssi1_cnt_per_rate[rate] > 0) {
                    printf
                        ("                    Ave Ack RSSI1: %4.1f dB\n",
                         (float) tstats->tx_rssi1_sum_per_rate[rate] /
                         tstats->tx_rssi1_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave Ack RSSI1: ---- dB\n");
                }
                if (tstats->tx_rssi2_cnt_per_rate[rate] > 0) {
                    printf
                        ("                    Ave Ack RSSI2: %4.1f dB\n",
                         (float) tstats->tx_rssi2_sum_per_rate[rate] /
                         tstats->tx_rssi2_cnt_per_rate[rate]);
                } else {
                    printf("                    Ave Ack RSSI2: ---- dB\n");
                }
                printf("\n");

                /* RSSI stats per antenna were removed */
            }
        }
    }

#if 0                           /* Computing predicted throuhput should be added */
    printf
        ("    Net predicted tx throughput = %.3f Mbps  Total time = %.0f ms\n",
         total_txlen * 8 / total_tx_pred_time / 1000, total_tx_pred_time);

#endif


    /* Look at overhead of management packets */

    /* Probe requests and responses */
    totalTimeCollected =
        ((struct ath_pktlog_hdr *) (log_buf->log_data +
                                    end_offset))->timestamp -
        ((struct ath_pktlog_hdr *) (log_buf->log_data +
                                    start_offset))->timestamp;
    totalTimeOutsideMeasurement = totalTimeCollected - totalTime;

    probe_resps_in_measurement_window = 0;
    probe_resps_out_measurement_window = 0;
    probe_reqs_in_measurement_window = 0;
    probe_reqs_out_measurement_window = 0;

    cur_rd_offset = start_offset;

    do {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

        log_data_offset = ((log_size - log_data_offset) >= log_hdr->size) ?
            log_data_offset : 0;

        /* Look for transmit or recieve packets of type probe req or probe resp */
        if (log_hdr->log_type == ATH_PKTLOG_RX) {
            struct ath_pktlog_rx *rx_log =
                (struct ath_pktlog_rx *) (log_buf->log_data +
                                          log_data_offset);

            if ((rx_log->framectrl & 0xfc) == 0x50) {
                if (((last_offset >= first_offset)
                     && ((cur_rd_offset >= first_offset)
                         && (cur_rd_offset <= last_offset)))
                    || (last_offset < first_offset)
                    && ((cur_rd_offset > first_offset)
                        || (cur_rd_offset < last_offset))) {
                    probe_resps_in_measurement_window++;
                } else {
                    probe_resps_out_measurement_window++;
                }
            }
        } else if ((log_hdr->log_type == ATH_PKTLOG_TX)) {
            struct ath_pktlog_tx *tx_log =
                (struct ath_pktlog_tx *) (log_buf->log_data +
                                          log_data_offset);

            if ((tx_log->framectrl & 0xfc) == 0x40) {
                if (((last_offset >= first_offset)
                     && ((cur_rd_offset >= first_offset)
                         && (cur_rd_offset <= last_offset)))
                    || (last_offset < first_offset)
                    && ((cur_rd_offset >= first_offset)
                        || (cur_rd_offset <= last_offset))) {
                    probe_reqs_in_measurement_window++;
                } else {
                    probe_reqs_out_measurement_window++;
                }
            }
        }

        PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf, log_size);
    } while (cur_rd_offset != log_buf->wr_offset);

    printf("\nInside measurement:  %3d (%5.1f/sec) probe reqs\n",
           probe_reqs_in_measurement_window,
           (float) probe_reqs_in_measurement_window / totalTime * 1000);
    printf("                 and %3d (%5.1f/sec) probe resps\n",
           probe_resps_in_measurement_window,
           (float) probe_resps_in_measurement_window / totalTime * 1000);

    printf("Outside measurement: %3d (%5.1f/sec) probe reqs\n",
           probe_reqs_out_measurement_window,
           (float) probe_reqs_out_measurement_window /
           totalTimeOutsideMeasurement * 1000);
    printf("                 and %3d (%5.1f/sec) probe resps\n",
           probe_resps_out_measurement_window,
           (float) probe_resps_out_measurement_window /
           totalTimeOutsideMeasurement * 1000);

#if 0
    if ($totalTime != 0) {
        printf
            ("\nTotal predicted time >= %.0f ms (vs actual %d ms for <= %.1f%% overhead)\n",
             total_tx_pred_time + total_rx_pred_time, totalTime,
             (totalTime -
              (total_tx_pred_time +
               total_rx_pred_time)) / totalTime * 100);
    }
#endif

    printf("\n");
}
#endif

void pktlogdump_usage()
{
    fprintf(stderr,
            "Packet log dump\n"
            "usage: pktlogdump [-p [event-list]] [-s] \n"
            "options:\n"
            "    -h    show this usage\n"
            "    -a    dumps log for specific 'adapter';\n"
            "          dumps system-wide log if this option is\n"
            "          not specified\n"
            "    -l    print all log events listed in the 'event-list'\n"
            "          event-list is a comma(,) seperated list of one or more\n"
            "          of the foll: rx tx rcf rcu ani cbf hinfo steering[eg., pktlogdump -f rx,rcu]\n"
            "          prints all events if 'event-list' is not specified\n"
	    "	       cannot be used with -x option \n"
	    "    -x    Dump the packetlog buffer \n"
            "    -P    print PER statistics\n"
            "    -I    print detailed interpacket arrival time histogram stats\n"
            "    -i    print distilled interpacket arrival time histogram stats\n");
    exit(-1);
}


int main(int argc, char *argv[])
{
    int fd;
    FILE *fp;
    struct ath_pktlog_buf *log_buf;
    int c;
    int size = 0;
    unsigned long filter = 0;
    char fstr[24];
    char ad_name[24];
    char proc_name[128];
    char sysctl_size[128];
    char sysctl_enable[128];
    char buff[8];
    int opt_P = 0, opt_l = 0, fflag = 0, opt_a = 0, opt_x = 0;

    ad_name[0] = '\0';

    for (;;) {
        c = getopt(argc, argv, "l::iIlPa:x::");
        if (c < 0)
            break;
        switch (c) {
        case 'l':
            opt_l = 1;
            if (optarg) {
                fflag = 1;
                if (strlcpy(fstr, optarg, sizeof(fstr)) >= sizeof(fstr)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
            }
            break;
        case 'x':
            opt_x = 1;
            if(opt_l) { 
                    pktlogdump_usage();
                    exit(-1);
            }
            break;
        case 'P':
            opt_P = 1;
            break;

        case 'I':
            opt_P = 1;
            break;

        case 'i':
            opt_P = 1;
            break;

        case 'a':
            opt_a = 1;
            if (optarg) {
                if (strlcpy(ad_name, optarg, sizeof(ad_name)) >= sizeof(ad_name)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
            }
            break;

        default:
            pktlogdump_usage();
        }
    }

    if (opt_a) {
        snprintf(proc_name,sizeof(proc_name), "/proc/" PKTLOG_PROC_DIR "/%s", ad_name);
        snprintf(sysctl_size,sizeof(sysctl_size), "/proc/sys/" PKTLOG_PROC_DIR "/%s/size",
                ad_name);
        snprintf(sysctl_enable,sizeof(sysctl_enable), "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable",
                ad_name);
    } else {
        snprintf(proc_name,sizeof(proc_name),
                "/proc/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM);
        snprintf(sysctl_size,sizeof(sysctl_size),
                "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM
                "/size");
        snprintf(sysctl_enable,sizeof(sysctl_enable),
                "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM
                "/enable");
    }

    if (fflag) {
        if (strstr(fstr, "rx"))
            filter |= ATH_PKTLOG_RX;
        if (strstr(fstr, "tx"))
            filter |= ATH_PKTLOG_TX;
        if (strstr(fstr, "rcf"))
            filter |= ATH_PKTLOG_RCFIND;
        if (strstr(fstr, "rcu"))
            filter |= ATH_PKTLOG_RCUPDATE;
        if (strstr(fstr, "ani"))
            filter |= ATH_PKTLOG_ANI;
        if (strstr(fstr, "cbf"))
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_CBF;
        if (strstr(fstr, "hinfo"))
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_H_INFO;
        if (strstr(fstr, "streeing"))
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_STEERING;
        if (filter == 0)
            pktlogdump_usage();
    } else {
        filter =
            ATH_PKTLOG_ANI | ATH_PKTLOG_RCUPDATE |
            ATH_PKTLOG_RCFIND | ATH_PKTLOG_RX | ATH_PKTLOG_TX | ATH_PKTLOG_CBF | ATH_PKTLOG_H_INFO | ATH_PKTLOG_STEERING;
    }

    fp = fopen(sysctl_enable, "w");

    if (fp == NULL) {
        printf("unable to open sysctl %s\n", sysctl_enable);
        return 1;
    }
    fprintf(fp, "%i", 0);
    fclose(fp);

    fp = fopen(sysctl_size, "r");
    if (fp == NULL) {
        printf("unable to open sysctl %s\n", sysctl_size);
        exit(-1);
    }

    buff[0] = '\0';
    if((fgets(buff, sizeof(buff)/sizeof(buff[0]), fp)) != NULL) {
        size = atoi(buff);
    }
    else {
        printf("Error reading file\n");
        fclose(fp);
        return 0;
    }

    fclose(fp);

    fd = open(proc_name, O_RDWR);
    if (fd < 0) {
        printf("pktlogdump: proc open failed %s\n", proc_name);
        exit(-1);
    }

    log_buf = (struct ath_pktlog_buf *) mmap(0, sizeof(*log_buf)
                                             + size,
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED, fd, 0);

    if (log_buf == MAP_FAILED) {
        perror("mmap");
        exit(-1);
    }


    if (log_buf->bufhdr.version != CUR_PKTLOG_VER) {
        printf("Log version mismatch\n");
        exit(-1);
    }

    printf("\nLog data %d %d %d\n", log_buf->rd_offset, log_buf->wr_offset,
           size);

    if (log_buf->rd_offset < 0) {
        printf("Log buffer is empty\n");
        return 0;
    }

    if (opt_l)
        print_log(log_buf, size, filter);

    if (opt_x) 
        dump_log(log_buf,size);


    if (opt_P) {
#if 0 /* Not supported yet */
        print_stats(log_buf, size, opt_i, opt_I);
#endif
        printf("Not supported\n");
    }
    return 0;
}
