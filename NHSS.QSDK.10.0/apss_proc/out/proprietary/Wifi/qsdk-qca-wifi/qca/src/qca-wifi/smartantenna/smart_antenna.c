/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "smart_antenna_internal.h"

unsigned int sa_default_antenna = SA_DEFAULT_ANTENNA;
module_param(sa_default_antenna, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(sa_default_antenna,
        "Default Antenna for RX and TX");
EXPORT_SYMBOL(sa_default_antenna);

unsigned int sa_enable_test_mode = 1; /* For beeliner untill algo is deveoped, by default enable test mode */
module_param(sa_enable_test_mode, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(sa_enable_test_mode,
        "Enable Smart Antenna API test mode");
EXPORT_SYMBOL(sa_enable_test_mode);

void sa_update_ant_map(struct sa_node_ccp *node,uint32_t selected_antenna);

/* Global declarations */
struct sa_global g_sa;
struct sa_testmode_train_params g_train_params;

/* Number of training packets calculation --
 * - calculate 20 on air packets with 4 ms as channel time assuming 1500 byte pkts,maximum aggretaion assumed as 32 packets.
 * - formula (min(rounddown(x/3),32)*20)
 * - lookup table calculated both ht and vht packet for all rates and is stored in global.
 */

u_int16_t lut_train_pkts_ht[2][SA_MAX_HT_RATES] = {{ 40, 100, 140, 180, 280, 380, 420, 480, 100, 180, 280, 380, 560, 640, 640, 640, 140, 280, 420, 560, 640, 640, 640, 640},
                                                   {100, 200, 300, 400, 580, 640, 640, 640, 200, 400, 580, 640, 640, 640, 640, 640, 300, 580, 640, 640, 640, 640, 640, 640}};

u_int16_t lut_train_pkts_vht[SA_MAX_BW][SA_MAX_VHT_RATES] = {{  40, 80,130,170,260,340,390,430,520,  0,    /*nss = 0 bw=20*/
                                                                80,170,260,340,520,640,640,640,640,  0,    /*nss = 1*/
                                                               130,260,390,520,640,640,640,640,640,640,    /*nss = 2*/
                                                               160,340,520,640,640,640,640,640,640,  0,},  /*nss = 3*/
                                                             {  90,180,270,360,540,640,640,640,640,640,       /*nss = 0 bw=40*/
                                                               180,360,540,640,640,640,640,640,640,640,       /*nss = 1*/
                                                               270,540,640,640,640,640,640,640,640,640,       /*nss = 2*/
                                                               360,640,640,640,640,640,640,640,640,640},      /*nss = 3*/
                                                             { 190,390,580,640,640,640,640,640,640,640,   /*nss = 0 bw=80*/
                                                               390,640,640,640,640,640,640,640,640,640,   /*nss = 1*/
                                                               580,640,640,640,640,640,  0,640,640,640,   /*nss = 2*/
                                                               640,640,640,640,640,640,640,640,640,640}}; /*nss = 3*/

u_int16_t vht_rates[SA_MAX_BW][SA_MAX_VHT_RATES] = {{   65, 130, 195, 260, 390, 520, 585,   722,  867,    0,  /*nss = 0 bw=20*/
                                                       130, 260, 390, 520, 780,1040, 1170, 1444, 1733,    0,  /*nss = 1*/
                                                       195, 390, 585, 780,1170,1560, 1755, 2167, 2600, 2889,  /*nss = 2*/
                                                       260, 520, 780,1040,1560,2080, 2600, 2889, 3467,    0}, /*nss = 3*/
                                                     { 135, 270, 405, 540, 810,1080, 1215, 1500, 1800, 2000,     /*nss = 0 bw=40*/
                                                       270, 540, 810,1080,1620,2160, 2430, 3000, 3600, 4000,     /*nss = 1*/
                                                       405, 810,1215,1620,2430,3240, 3645, 4500, 5400, 6000,     /*nss = 2*/
                                                       540,1080,1620,2160,3240,4320, 4860, 6000, 7200, 8000},    /*nss = 3*/
                                                     { 293, 585, 878,1170,1755,2340, 2633, 3250, 3900, 4333, /*nss = 0 bw=80*/
                                                       585,1170,1755,2340,3510,4680, 5265, 6500, 7800, 8667, /*nss = 1*/
                                                       878,1755,2633,3510,5265,7020,    0, 9750,11700,13000, /*nss = 2*/
                                                      1170,2340,3510,4680,7020,9360,10530,13000,15600,17333} /*nss = 3*/
                                                   };
/*  */
u_int16_t ht_rates[2][SA_MAX_HT_RATES] = {{ 65, 130, 195, 260, 390, 520, 585, 722,  /*nss = 0 bw=20*/
                                           130, 260, 390, 520, 780,1040,1170,1444,  /*nss = 1*/
                                           195, 390, 585, 780,1170,1560,1755,2167}, /*nss = 2*/
                                          {135, 270, 405, 540, 810,1080,1215,1500,   /*nss = 0 bw=40*/
                                           270, 540, 810,1080,1620,2160,2430,3000,   /*nss = 1*/
                                           405, 810,1215,1620,2430,3240,3645,4500}   /*nss = 2*/
                                         };
u_int16_t legacy_rates[] = {0,1,2,3,4,5,6,7,8,9,10,11};

/* functions */

/**
 *   print_mac_addr: prints the mac address.
 *   @mac: pointer to the mac address
 *
 *   return pointer to string
 */
static char *print_mac_addr(const uint8_t *mac)
{
    static char buf[32] = {'\0', };
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

/**
 *   sa_set_train_bw: finds the current training bandwidth
 *   @node: pointer to node
 *
 *   returns bandwidth
 */
void sa_set_train_bw(struct sa_node_ccp *node)
{
    uint32_t *nppdu_bw = &node->rate_stats.nppdu_bw[SA_BW_20];
    struct node_train_stats *train_state = &node->train_info.train_state;

    if (!nppdu_bw[SA_BW_20] && !nppdu_bw[SA_BW_40] && !nppdu_bw[SA_BW_80]) {
        train_state->bw = node->train_info.last_train_bw;
    } else {
        train_state->bw = (nppdu_bw[SA_BW_20] > nppdu_bw[SA_BW_40]) ?
                   ((nppdu_bw[SA_BW_20] > nppdu_bw[SA_BW_80]) ? SA_BW_20 : SA_BW_80) :
                   ((nppdu_bw[SA_BW_40] > nppdu_bw[SA_BW_80]) ? SA_BW_40 : SA_BW_80);
    }

    if (train_state->bw > node->node_info.max_ch_bw) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s mac %s nppdu20 %d nppdu40 %d nppdu80 %d trainbw %d maxbw %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),nppdu_bw[SA_BW_20],nppdu_bw[SA_BW_40],nppdu_bw[SA_BW_80],train_state->bw,node->node_info.max_ch_bw);
        train_state->bw = node->node_info.max_ch_bw;
    }
    node->train_info.last_train_bw = train_state->bw;

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s %d %d %d \n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),nppdu_bw[SA_BW_20],nppdu_bw[SA_BW_40],nppdu_bw[SA_BW_80]);
}

/**
 *   sa_get_train_bw: finds the current training bandwidth
 *   @node: pointer to node
 *
 *   returns bandwidth
 */
static inline uint8_t sa_get_train_bw(struct sa_node_ccp *node)
{
    struct node_train_stats *train_state = &node->train_info.train_state;

    return train_state->bw;
}

/**
 *   sa_get_max_rate: finds the max rate of the node
 *   @node: pointer to node
 *
 *   returns max rate code
 */
uint32_t sa_get_max_rate(struct sa_node_ccp *node)
{
    uint8_t bw = 0;
    uint32_t i = 0;

    bw = sa_get_train_bw(node);
    switch (bw) {
        case SA_BW_20 :
            if (node->node_info.opmode) {
                i = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_20] - 1;
                return node->node_info.rate_cap.ratecode_20[i];
            } else {
                i = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_LEGACY] - 1;
                return node->node_info.rate_cap.ratecode_legacy[i];
            }
            break;
        case SA_BW_40 :
            i = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_40] - 1;
            return node->node_info.rate_cap.ratecode_40[i];
            break;
        case SA_BW_80 :
            i = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_80] - 1;
            return node->node_info.rate_cap.ratecode_80[i];
            break;
        default:
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s ERROR in bw\n",__func__);
            i = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_80] - 1;
            return node->node_info.rate_cap.ratecode_80[i];
    }
}

/**
 *   sa_move_rate: moves the training rate in specified direction
 *   @node: pointer to node
 *   @train_nxt_rate: direction of training
 *
 *   returns rate code if valid rate is there otherwise returns -ve number
 */
int32_t sa_move_rate(struct sa_node_ccp *node, int8_t train_nxt_rate)
{
    int i = 0;
    uint8_t bw = 0;
    uint8_t num_of_rates;
    uint8_t *rate_array;
    uint16_t *tp_array;
    uint16_t rate = node->train_info.train_state.rate;
    uint8_t vht_mode = (node->node_info.opmode == SA_OPMODE_HT) ? 0 : 1;
    uint8_t current_index, nxt_index;
    uint16_t current_tp,nxt_tp = 0;
    uint16_t nxt_rate;

    if (node->train_info.training_start > 1) {
        /* fixed rate manual training */
        return -1;
    }

    /* get the bandwidth in which we are doing training */
    bw = sa_get_train_bw(node);

    tp_array = vht_mode ? &vht_rates[bw][0] : &ht_rates[bw][0];

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s \n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr));
    switch (bw) {
        case SA_BW_20 :
            if (node->node_info.opmode) {
                num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_20];
                rate_array = &node->node_info.rate_cap.ratecode_20[i];
            } else {
                num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_LEGACY];
                rate_array = &node->node_info.rate_cap.ratecode_legacy[i];
                tp_array = &legacy_rates[0];
            }
            break;
        case SA_BW_40 :
            num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_40];
            rate_array = &node->node_info.rate_cap.ratecode_40[i];
            break;
        case SA_BW_80 :
            num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_80];
            rate_array = &node->node_info.rate_cap.ratecode_80[i];
            break;
        default:
            num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_80];
            rate_array = &node->node_info.rate_cap.ratecode_80[i];
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s ERROR in bw\n",__func__);
    }

    for(i = 0; i < num_of_rates; i++) {
        if (rate_array[i] == rate) {
            break;
        }
    }

    if(i == num_of_rates) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s ERROR in sa rate table\n",__func__);
    }

    if(IS_SA_11B_RATE(rate)) {
        current_index = SA_STAT_INDEX_11B(rate);
    } else if (IS_SA_11G_RATE(rate)) {
        current_index = SA_STAT_INDEX_11G(rate);
    } else {
        current_index = vht_mode ? SA_RATE_INDEX_11AC(rate) : SA_RATE_INDEX_11N(rate);
    }

    current_tp = tp_array[current_index];

    if(train_nxt_rate < 0) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"-rate  %x %d min %d\n",rate,i,num_of_rates);
        while (i) {
            i--;
            nxt_rate = rate_array[i];
            if(IS_SA_11B_RATE(rate)) {
                nxt_index = SA_STAT_INDEX_11B(nxt_rate);
            } else if (IS_SA_11G_RATE(rate)) {
                nxt_index = SA_STAT_INDEX_11G(nxt_rate);
            } else {
                nxt_index = vht_mode ? SA_RATE_INDEX_11AC(nxt_rate) : SA_RATE_INDEX_11N(nxt_rate);
            }

            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"rc %x c %d p %d\n",nxt_rate,nxt_tp,current_tp);
            nxt_tp = tp_array[nxt_index];
            if ((nxt_tp < current_tp) && (nxt_tp)) {
                return nxt_rate;
            }
        }
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s already at min valid rate  %x %x\n",__func__,rate,rate_array[0]);
    } else {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"+rate  %x %d max %d\n",rate,i,num_of_rates);
        while (i < (num_of_rates-1)) {
            i++;
            nxt_rate = rate_array[i];
            if(IS_SA_11B_RATE(rate)) {
                nxt_index = SA_STAT_INDEX_11B(nxt_rate);
            } else if (IS_SA_11G_RATE(rate)) {
                nxt_index = SA_STAT_INDEX_11G(nxt_rate);
            } else {
                nxt_index = vht_mode ? SA_RATE_INDEX_11AC(nxt_rate) : SA_RATE_INDEX_11N(nxt_rate);
            }
            nxt_tp = tp_array[nxt_index];
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"rc %x c %d p %d\n",nxt_rate,nxt_tp,current_tp);
            if ((nxt_tp > current_tp) && (nxt_tp)) {
                return nxt_rate;
            }
        }
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s already at max valid rate  %x %x\n",__func__,rate,rate_array[num_of_rates-1]);
    }
    return -1;
}

/**
 *   sa_update_ratestats: function to update rate stats from tx feedback
 *   @node: pointer to node
 *   @feedback: tx feedback for xmitted packet
 *
 *   returns 0 indicating success
 */
int32_t sa_update_ratestats(struct sa_node_ccp *node, struct sa_tx_feedback *feedback)
{
    uint8_t bw,nss,rate,mcs,series,index,num_fb = 1, nfb = 0, npkts = 0;
    struct radio_config *radio = &g_sa.radio[node->node_info.radio_id];

    SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id," %s:  Mac: %s, nFrame:%d nBad: %d SR: [%d %d %d %d %d %d] LR: [%d %d %d %d %d %d]"
            "Rate:[0x%x 0x%x] RateIdx: %d Antenna: [0x%06x 0x%06x] RSSI: [0x%x 0x%x 0x%x]"
            "Trainpkt: %d ratemaxphy: 0x%04x goodput:%d \n"
            ,__func__ , print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), feedback->nPackets, feedback->nBad
            ,feedback->nshort_retries[0], feedback->nshort_retries[1], feedback->nshort_retries[2]
            ,feedback->nshort_retries[4], feedback->nshort_retries[5], feedback->nshort_retries[6]
            ,feedback->nlong_retries[0], feedback->nlong_retries[1], feedback->nlong_retries[2]
            ,feedback->nlong_retries[4], feedback->nlong_retries[5], feedback->nlong_retries[6]
            ,feedback->rate_mcs[0], feedback->rate_mcs[1],feedback->rate_index
            ,feedback->tx_antenna[0], feedback->tx_antenna[1]
            ,feedback->rssi[0], feedback->rssi[1], feedback->rssi[2]
            ,feedback->is_trainpkt, feedback->ratemaxphy
            ,feedback->goodput);

    if (feedback->is_trainpkt) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," %s:  Error: invalid update ",__func__);
        return SA_STATUS_FAILURE;
    }

    if (feedback->num_comb_feedback) {
	    SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"num comp feedbacks: %d \n",feedback->num_comb_feedback);
	    SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"fb1: rate 0x%x bw 0x%d nppdu_combined %d npkts %d nbad %d \n",feedback->comb_fb[0].rate,(feedback->comb_fb[0].bw >> 4 & 0x0F), (feedback->comb_fb[0].bw & 0x0F), feedback->comb_fb[0].npkts,feedback->comb_fb[0].nbad);
	    SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"fb2: rate 0x%x bw 0x%d nppdu_combined %d npkts %d nbad %d \n",feedback->comb_fb[1].rate,(feedback->comb_fb[1].bw >> 4 & 0x0F), (feedback->comb_fb[1].bw & 0x0F), feedback->comb_fb[1].npkts,feedback->comb_fb[1].nbad);
    }

    bw = nss = rate = mcs = series = 0;

    num_fb += feedback->num_comb_feedback;

    while (num_fb) {
        num_fb--;
        if (num_fb) {
            struct sa_comb_fb *pcomb_fb = &feedback->comb_fb[num_fb - 1];
            nfb   = SA_GET_NFB_COMB_FEEDBACK(pcomb_fb->bw);
            bw    = SA_GET_BW_COMB_FEEDBACK(pcomb_fb->bw);
            rate  = pcomb_fb->rate;
            npkts = pcomb_fb->npkts;
        } else {
            npkts = feedback->nPackets;
            nfb   = 1;

            if (radio->radiotype == wifi0) {
                /* direct attach */
                bw = node->node_info.max_ch_bw;
                rate = SA_GET_RATE_FEEDBACK(feedback->rate_mcs[feedback->rate_index], bw);
            } else {
                /* offload */
                bw = SA_GET_BW_FEEDBACK(feedback->rate_index);
                series = SA_GET_SERIES_FEEDBACK(feedback->rate_index);
                rate = SA_GET_RATE_FEEDBACK(feedback->rate_mcs[series], bw);
            }
        }

        if(IS_SA_11B_RATE(rate)) {
            /* 11b rate code */
            index = SA_STAT_INDEX_11B(rate);
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"%s 11B: bw = %d nss = %d rate = %d mcs = %d series %d index %d \n",__func__, bw, nss, rate, mcs, series, index);
            node->rate_stats.npackets_legacy[index] += npkts;
        } else if (IS_SA_11G_RATE(rate)) {
            /* 11g rate code */
            index = SA_STAT_INDEX_11G(rate);
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"%s, 11G/A: bw = %d nss = %d rate = %d mcs = %d series %d index %d \n",__func__, bw, nss, rate, mcs, series, index );
            node->rate_stats.npackets_legacy[index] += npkts;
        } else {
            nss = SA_GET_NSS_FROM_RATE(rate);
            mcs = SA_GET_MCS_FROM_RATE(rate);
            index = SA_STAT_INDEX_11N_11AC(nss,mcs);
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"%s VHT: bw = %d nss = %d rate = %d mcs = %d series %d index: %d \n",__func__, bw, nss, rate, mcs, series, index);
            node->rate_stats.npackets[bw][index] += npkts;
        }
        node->rate_stats.nppdu_bw[bw] += nfb;
    }

    node->rate_stats.ins_goodput[bw] = ((node->rate_stats.ins_goodput[bw] * node->rate_stats.nppdu_gp[bw]) + feedback->goodput)/(node->rate_stats.nppdu_gp[bw] + 1);
    node->rate_stats.nppdu_gp[bw]++;
    node->rate_stats.rate_mcs = feedback->rate_mcs[0];
    if (((feedback->ratemaxphy >> SA_RATEPOSITION_20) & SA_BYTE_MASK) && ((feedback->ratemaxphy >> SA_RATEPOSITION_40) & SA_BYTE_MASK) && ((feedback->ratemaxphy >> SA_RATEPOSITION_80) & SA_BYTE_MASK)) {
        node->rate_stats.rate_maxphy = feedback->ratemaxphy;
    } else {
        SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id, " Error: %s invalid ratemax phy in normal pkt %x",print_mac_addr((const uint8_t *)node->node_info.mac_addr),feedback->ratemaxphy);
    }
    return 0;
}

/**
 *   find_min_rssi : This function finds the minimum rssi value for particular index in 2-dimentional rssi array.
 *      this function is called in iwpriv context, used to intiate manual training.
 *   @base_addr : base address of multi dimentional array.
 *   @index : index of RSSI set.
 *   @num_recv_chains: number of receive chains
 *   @ret: returns minimum value found in the array
 */
static inline int8_t find_min_rssi(int8_t *base_addr, int index, uint32_t num_recv_chains)
{
    int i;
    int8_t min = base_addr[index];

    for (i = 1; i < num_recv_chains; i++) {
        if ((base_addr[(i*SA_MAX_RSSI_SAMPLES) + index] >= 0) && (min > base_addr[(i*SA_MAX_RSSI_SAMPLES) + index])) {
            min = base_addr[(i*SA_MAX_RSSI_SAMPLES) + index];
        }
    }
    return min;
}

/**
 *   find_max_rssi : This function finds the maximum rssi value for particular inndex in 2-dimentional rssi array.
 *      this function is called in iwpriv context, used to intiate manual training.
 *   @base_addr : base address of multi dimentional array.
 *   @index : index of RSSI set.
 *   @ret: returns minimum value found in the array
 */
static inline int8_t find_max_rssi(int8_t *base_addr, int index, uint32_t num_recv_chains)
{
    int i;
    int8_t max = base_addr[index];

    for (i = 1; i < num_recv_chains; i++) {
        if (max < base_addr[(i*SA_MAX_RSSI_SAMPLES) + index]) {
            max = base_addr[(i*SA_MAX_RSSI_SAMPLES) + index];
        }
    }
    return max;
}

/**
 *   sa_compare_rssi: function to compare rssi stats
 *   @node: pointer to node
 *   @train_data: current training stats
 *   @x: rssi index in an array
 *
 *   returns inidication. -1 indicates a tie
 */
int32_t sa_compare_rssi(struct sa_node_ccp *node, struct sa_node_train_data *train_data, uint8_t x)
{
    int8_t train_rssi,current_rssi;
    struct node_train_stats *train_state = &node->train_info.train_state;
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;

    train_rssi = find_min_rssi((int8_t *)train_state->rssi, x, sa_params->num_recv_chains);

    current_rssi = find_min_rssi((int8_t *)train_data->rssi, x, sa_params->num_recv_chains);

    if (current_rssi > train_rssi) {
        /* Maximizing the Minimum */
        return 1;
    } else if (current_rssi == train_rssi) {
        /* if we have same min RSSI values then check for MAX RSSI in that BA RSSI SET */
        train_rssi = find_max_rssi((int8_t *)train_state->rssi,x, sa_params->num_recv_chains);

        current_rssi = find_max_rssi((int8_t *)train_data->rssi,x, sa_params->num_recv_chains);

        if(current_rssi > train_rssi) {
            return 1;
        } else if (current_rssi < train_rssi) {
            return 0;
        } else {
            return -1;
        }
    }
    return 0;
}

/**
 *   sa_apply_secondary_metric : function to apply secondary metric
 *   @node: pointer to node
 *
 *   return 1 indicates progressive antenna selection
 */
uint8_t sa_apply_secondary_metric(struct sa_node_ccp *node)
{
    struct sa_node_train_data *train_data  = &node->train_data;

    uint32_t x = 0;
    uint32_t cant = 0, tant = 0;
    uint8_t retval;

    for (x = 0; x < SA_MAX_RSSI_SAMPLES; x++) {

        retval = sa_compare_rssi(node, train_data,x);
        if (retval == 0) {
            cant++;
        } else if (retval == 1) {
            tant++;
        }
    }

    if (cant >= tant) {
        return 0;
    } else {
        return 1;
    }
}

/**
 *   sa_get_num_trainpkts: function to find the number of training packets for a particular rate in particular mode and bandwidth
 *   @node: pointer to node
 *   @rate: rate code of training rate
 *
 *   return number of training packets
 */
u_int16_t sa_get_num_trainpkts(struct sa_node_ccp *node, uint32_t rate)
{
    u_int16_t num_trainpkts = 0;
    u_int16_t index = 0;
    uint8_t bw = sa_get_train_bw(node);
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;

    switch (node->node_info.opmode) {
        case SA_OPMODE_LEGACY:
            num_trainpkts = SA_NUM_LEGACY_TRAIN_PKTS;
            break;
        case SA_OPMODE_HT:
            if(bw > SA_BW_40) {
                qdf_print("%s invalid HT bandwidth %d",__func__,bw);
                return SA_NUM_DEFAULT_TRAIN_PKTS;
            }
            index = SA_RATE_INDEX_11N(rate);
            if (index < SA_MAX_HT_RATES){
                num_trainpkts = lut_train_pkts_ht[bw][index];
            }
            break;
        case SA_OPMODE_VHT:
            index = SA_RATE_INDEX_11AC(rate);
            if (index < SA_MAX_VHT_RATES){
                num_trainpkts = lut_train_pkts_vht[bw][index];
            }
            break;
        default :
            num_trainpkts = SA_NUM_DEFAULT_TRAIN_PKTS;
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id, " Error: %s invalid bw",print_mac_addr((const uint8_t *)node->node_info.mac_addr));
    }

    if (sa_params->n_packets_to_train) {
        num_trainpkts = sa_params->n_packets_to_train;
    }

    if (sa_params->config & SA_CONFIG_INTENSETRAIN) {
        if(node->train_info.intense_train) {
            num_trainpkts = ((num_trainpkts * 2) > SA_MAX_INTENSETRAIN_PKTS) ? SA_MAX_INTENSETRAIN_PKTS : (num_trainpkts * 2);
        }
    }

    return num_trainpkts;
}

void sa_cancel_train_params(struct sa_node_ccp *node, uint32_t antenna)
{
    struct sa_node_train_data *train_data  = &node->train_data;
    uint32_t rate_mcs = node->rate_stats.rate_mcs;

    train_data->antenna = antenna;
    train_data->ratecode = rate_mcs;
    train_data->numpkts = 0;
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s train antenna %d, rate %x, numpkts %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),train_data->antenna,train_data->ratecode,train_data->numpkts);

}

/**
 *   sa_set_train_params: function to store current train params
 *   @node: pointer to node
 *   @antenna: training antenna
 *   @rate: rate code of training rate
 *
 *   return none
 */
void sa_set_train_params(struct sa_node_ccp *node, uint32_t antenna, uint32_t rate)
{
    struct sa_node_train_data *train_data  = &node->train_data;
    int8_t bw = sa_get_train_bw(node);
    uint32_t rate_mcs = node->rate_stats.rate_mcs;

    train_data->antenna = antenna;
    switch (bw) {
        case SA_BW_20:
            train_data->ratecode = rate;
            break;
        case SA_BW_40:
            train_data->ratecode = (rate << SA_RATEPOSITION_40) | (rate_mcs & SA_RATEMASK_20);
            break;
        case SA_BW_80:
            train_data->ratecode = (rate << SA_RATEPOSITION_80) | (rate_mcs & (SA_RATEMASK_20 | SA_RATEMASK_40));
            break;
    }
    train_data->numpkts = sa_get_num_trainpkts(node,rate);
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s train antenna %d, rate %x, numpkts %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),train_data->antenna,train_data->ratecode,train_data->numpkts);

}

/**
 *   sa_get_train_rate_antenna: function to retrive current train params
 *   @node: pointer to node
 *   @rate_array: rate code array for training
 *   @antenna_array: training antenna array
 *   @numpkts: number of training packets
 *
 *   return none
 */
void sa_get_train_rate_antenna(struct sa_node_ccp *node, uint32_t *rate_array, uint32_t *antenna_array, uint32_t *numpkts)
{
    struct sa_node_train_data *train_data  = &node->train_data;

    if ((train_data->ratecode == 0) && (node->node_info.opmode != SA_OPMODE_LEGACY)) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s ERROR Zero rate code !!! Mac: %s max bw: %d \n",
                __func__, print_mac_addr((const uint8_t *)node->node_info.mac_addr), node->node_info.max_ch_bw);
    }

    rate_array[0] = train_data->ratecode;
    rate_array[1] = train_data->ratecode;
    rate_array[2] = train_data->ratecode;
    rate_array[3] = train_data->ratecode;

    antenna_array[0] = train_data->antenna;
    antenna_array[1] = train_data->antenna;
    antenna_array[2] = train_data->antenna;
    antenna_array[3] = train_data->antenna;

    *numpkts = train_data->numpkts ? 0x7FFF : 0;
}

/**
 *   sa_select_rx_antenna: function to select receive antenna. This is called after tx antenna training completion.
 *                         for sta mode rx antenna is selected same as tx antenna
 *                         for ap mode if all connected nodes have same tx antenna and same is selected as tx antenna other wise default antenna is selected.
 *   @node: pointer to node
 *   @radio: pointer to radio
 *
 *   return status indicating rx antenna change
 */
uint32_t sa_select_rx_antenna(struct sa_node_ccp *node,struct radio_config *radio)
{
    struct sa_node_train_info *train_info = &node->train_info;
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;
    uint32_t status = 0;
    int i;

    if (!radio->init_config.bss_mode) {
        /* ap mode */
        if (train_info->previous_selected_antenna != train_info->selected_antenna) {
            radio->node_count_per_antenna[node->train_info.previous_selected_antenna]--;
            radio->node_count_per_antenna[node->train_info.selected_antenna]++;
            if (train_info->selected_antenna != radio->default_antenna) {
                if(radio->nodes_connected == radio->node_count_per_antenna[train_info->selected_antenna]) {
                    radio->default_antenna = train_info->selected_antenna;
                    status |= SMART_ANT_RX_CONFIG_REQUIRED;
                }
            }
        } else {
            if ((train_info->selected_antenna != radio->default_antenna) &&
                    (radio->nodes_connected == radio->node_count_per_antenna[train_info->selected_antenna])) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s: mac %s receive antenna selection error\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr));
            }
        }
        if (1) { /* initail debugging purpose, will be removed */
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: nodes connected %d\n",__func__,radio->nodes_connected);
            for (i = 0;i < sa_params->num_tx_antennas; i++) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"    ant %d , nodes %d\n",i,radio->node_count_per_antenna[i]);
            }
        }
    } else {
        /* in case of station switch rx antenna selected tx antenna */
        if (radio->default_antenna != train_info->selected_antenna) {
            radio->default_antenna = train_info->selected_antenna;
            status |= SMART_ANT_RX_CONFIG_REQUIRED;
        }
        train_info->hybrid_train = 0;
        train_info->long_retrain_count = 0;
    }

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: mac %s receive antenna %d selected ant %d pselected antenna %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr), radio->default_antenna, train_info->selected_antenna,train_info->previous_selected_antenna);
    train_info->previous_selected_antenna = train_info->selected_antenna;

    return status;

}

/**
 *   sa_get_tx_chainmask: function to get tx chainmask for particular radio
 *   @radio: pointer to radio
 *
 *   return tx chain mask
 */
uint8_t sa_get_tx_chainmask(struct radio_config *radio)
{
    return (radio->init_config.txrx_chainmask & SA_MASK_TXCHAIN);
}

/**
 *   sa_process_trainstats: processes the collected train stats and applies smart antenna algo to find the best antenna
 *   @node: pointer to node
 *
 *   returns status indicating training, tx antenna change, rx antenna change
 */
uint32_t sa_process_trainstats(struct sa_node_ccp *node)
{
    struct sa_node_train_data *train_data  = &node->train_data;
    struct sa_node_train_info *train_info = &node->train_info;
    struct node_train_stats *train_state = &node->train_info.train_state;
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;
    struct radio_config *radio = &g_sa.radio[node->node_info.radio_id];

    uint32_t per = 0,per_diff = 0;
    int32_t ridx;
    uint8_t train_nxt_antenna = 1;
    int8_t train_nxt_rate = 0;
    uint8_t swith_antenna = 0;
    uint32_t skipmask;
    uint32_t status = 0;
    uint32_t num_rssi_samples = SA_MAX_RSSI_SAMPLES * sa_params->num_recv_chains;
#if SA_DEBUG
    struct sa_debug_train_cmd *dtrain_cmd;
#endif

    if (train_state->extra_stats) {
        if (!train_state->extra_sel) {
            if(train_data->antenna != train_info->selected_antenna) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id," Error in taking extra stats!!!\n");
            }
        }
    }
    /* Move to next rate/antenna for Now */
    if (train_data->nFrames) {
        per = (SA_MAX_PERCENTAGE *train_data->nBad/train_data->nFrames);
    } else {
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id," %s ERROR in PER stats\n",__func__);
        per = SA_MAX_PERCENTAGE;
    }

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," %s :current states: Antenna: %02d Rate: 0x%02x nFrame: %03d nBad: %03d nppdu: %03d BARSSI: %02d %02d %02d PER: %d \n",
            print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), train_data->antenna, train_data->ratecode,train_data->nFrames, train_data->nBad,node->train_info.nppdu_bw[node->train_info.train_state.bw],train_data->rssi[0][0],train_data->rssi[1][0],train_data->rssi[2][0], per);

    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id," %s :train state: Antenna: %02d Rate: 0x%02x lrate: 0x%02x ldir: %d nFrame: %03d nBad: %03d BARSSI: %02d %02d %02d PER: %d \n",
            __func__, train_state->antenna, train_state->rate,train_state->last_rate,train_state->last_train_dir,train_state->nframes, train_state->nbad,train_state->rssi[0][0],train_state->rssi[1][0],train_state->rssi[2][0], per);

#if SA_DEBUG
    dtrain_cmd = &node->debug_data.train_cmd[node->debug_data.cmd_count];
    if (node->debug_data.cmd_count < SA_MAX_DEBUG_TRAIN_CMDS) {
       OS_MEMCPY(dtrain_cmd, train_data, 12);
#if not_yet
       dtrain_cmd->rssi[0] = train_data->rssi[0][0];
       dtrain_cmd->rssi[1] = train_data->rssi[1][0];
       dtrain_cmd->rssi[2] = train_data->rssi[2][0];
#endif
       dtrain_cmd->per = per;
       dtrain_cmd->bw = node->train_info.train_state.bw;
       dtrain_cmd->last_train_dir = train_state->last_train_dir;
       dtrain_cmd->last_rate = train_state->last_rate;
       dtrain_cmd->nppdu = node->train_info.nppdu_bw[node->train_info.train_state.bw];
       dtrain_cmd->ant_sel = 0;
       node->debug_data.cmd_count++;
    }
#endif

    if (train_state->extra_stats) {
        if (!train_state->extra_sel) {
            train_state->extra_sel = 1;
            train_state->per =(100*(train_data->nBad + train_state->nbad)/(train_state->nframes + train_data->nFrames));
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s : Extra stats selant %d %d %d %d new per %d\n",__func__,train_data->nBad,train_state->nbad,train_state->nframes,train_data->nFrames,train_state->per);
            train_state->nbad += train_data->nBad;
            train_state->nframes += train_data->nFrames;
            /* copy RSSI Samples */
            OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
            goto extra_stats;
        } else if (!train_state->extra_cmp) {
            train_state->extra_cmp = 1;
            per = (100*(train_data->nBad + train_state->extra_nbad)/(train_data->nFrames + train_state->extra_nframes));
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s : Extra stats cmpant %d %d %d %d new per %d\n",__func__,train_data->nBad,train_state->extra_nbad,train_data->nFrames,train_state->extra_nframes,per);
            train_state->extra_stats = 0;
        } else {
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s : error in getting extra stats \n",__func__);
        }
    }

    if (train_info->selected_antenna == train_state->ant_map[train_state->antenna]) {
        /* curretly selected antenna */
        if (train_state->first_per) {
            train_state->first_per = 0;
            train_state->per = per;
            train_state->nbad = train_data->nBad;
            train_state->nframes = train_data->nFrames;
            /* copy RSSI Samples */
            OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
            train_state->last_rate = train_state->rate;
            if (per > sa_params->upper_bound) {
                train_nxt_rate = -1;
            } else if (per < sa_params->lower_bound) {
                train_nxt_rate = 1;
            }
        } else {
            /* check if PER can be used */
            if (per < sa_params->upper_bound) {
                if (per < sa_params->lower_bound) {
                    if (train_state->last_train_dir > 0) {
                        train_nxt_rate = 1;
                        train_state->per = per;
                        train_state->nbad = train_data->nBad;
                        train_state->nframes = train_data->nFrames;
                        /* copy RSSI Samples */
                        OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
                    } else {
                        /* PER 100, 100, 15 in this case store the PER and trying next rate */
                        train_state->per = per;
                        train_state->nbad = train_data->nBad;
                        train_state->nframes = train_data->nFrames;
                        /* copy RSSI Samples */
                        OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
                        train_state->last_rate = train_state->rate;
                    }
                } else {
                    train_state->per = per;
                    train_state->nbad = train_data->nBad;
                    train_state->nframes = train_data->nFrames;
                    /* copy RSSI Samples */
                    OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
                    train_state->last_rate = train_state->rate;
                }
            } else {
                if (train_state->last_train_dir < 0) {
                    train_nxt_rate = -1;
                    train_state->per = per;
                    train_state->nbad = train_data->nBad;
                    train_state->nframes = train_data->nFrames;
                    /* copy RSSI Samples */
                    OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
                } else {
                    train_state->rate = train_state->last_rate;
                }
            }
        } /* not first per */
    } else { /* Update from other than current selected antenna */
        /* compare PER and update stats */
        /*
         * a) PER < lower bound on train other than current selected antenna
         *          Move to next higer rate
         */

        /* per < lowerbound && rateIndex is not Max valid rate ; In case of max validrate we wont chek for per and secondary metric */
        if ((per < sa_params->lower_bound) && (sa_get_max_rate(node) != train_state->rate) && !train_state->extra_cmp) {
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: Current antenna previous tried rate: 0x%02x and trained antenna rate:0x%02x \n",print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), train_state->last_rate, train_state->rate);
            train_nxt_antenna = 0;
            train_nxt_rate = 1;
            if (train_state->rate > train_state->last_rate) {
                swith_antenna = 1;
#if SA_DEBUG
                dtrain_cmd->ant_sel |= SA_DBG_CODE_NXT_RATE_LLB;
#endif
            } else if (train_state->rate == train_state->last_rate) {
                per_diff = per < train_state->per ? (train_state->per - per) : 0;
                if ((per_diff > sa_params->per_diff_threshold) && (train_state->per > sa_params->lower_bound)) {
                    swith_antenna = 1;
#if SA_DEBUG
                    dtrain_cmd->ant_sel |= SA_DBG_CODE_CUR_RATE_LLB;
#endif
                } else {
                    /* If rate are same; save The per for comparsion on same rate
                       for trained antena antenna againest current antenna if trained antena has UpperBound PER on next rate
                       Lower bound : 20
                        Example:  0 - 18 ;1 - 10 But for next rate 1 - 95;
                        In this case we will comapre 1-10 vs 0 -18
                     */
                    train_state->nextant_per = per;
                    train_state->nextant_nbad = train_data->nBad;
                    train_state->nextant_nframes = train_data->nFrames;
                    /* copy RSSI Samples to node nextant*/
                    OS_MEMCPY(&train_state->nextant_rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
                }
            }
        } else {
            if (train_state->rate > train_state->last_rate) {
                if (per < sa_params->upper_bound) {
                    /* current Idx < trained idx but PER is not in lower bound i
Example: 0 -25 on x rate
1 - 15 on x rate after trying x+1 we got PER < 90 ;say 65
1 - 65 on x+1 but trainedIdx > currentIdx; So move antenna
                     */
                    swith_antenna = 1;
#if SA_DEBUG
                    dtrain_cmd->ant_sel |= SA_DBG_CODE_NXT_RATE_LUB;
#endif
                } else { /* per > upper bound: Need to compare with the previous PER */

                    /*
                     *  Make current rate is equal to last rate  to follow normal path
                     *  Restore the previous PER
                     *  Move rate down and try Next antenna
                     */
                    train_state->rate = train_state->last_rate; /* This make rate to previous */

                    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: previous tried rate: %d and rate:%d \n",print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)),train_state->last_rate, train_state->rate);
                    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s: PER %d and Nextant per: %d RSSI ch0:%d ch1:%d ch2:%d Previous PER: %d \n",print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)),per
                            ,train_state->nextant_per
                            ,train_state->nextant_rssi[0][0]
                            ,train_state->nextant_rssi[1][0]
                            ,train_state->nextant_rssi[2][0]
                            ,train_state->per);

                    per = train_state->nextant_per;
                    train_data->nBad = train_state->nextant_nbad;
                    train_data->nFrames = train_state->nextant_nframes;
                    /* copy RSSI Samples */
                    OS_MEMCPY(&train_data->rssi[0][0], &train_state->nextant_rssi[0][0], num_rssi_samples);
                    train_nxt_antenna = 1;
                }
            }

            if (train_state->rate == train_state->last_rate) {
                per_diff = per > train_state->per ? (per - train_state->per) :
                    (train_state->per - per);
                if (per_diff <= sa_params->per_diff_threshold) {
                    /* apply secondary metric */
                    if ((sa_params->config & SA_CONFIG_EXTRATRAIN) && !train_state->extra_cmp) {
                        train_state->extra_stats = 1;//double_train
                        train_state->extra_nbad = train_data->nBad;
                        train_state->extra_nframes = train_data->nFrames;
                        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s : extra stats for ant %d and %d\n",__func__,train_state->ant_map[0],train_state->ant_map[train_state->antenna]);
                    } else {
                        if (sa_params->config & SA_CONFIG_EXTRATRAIN) {
                            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s : extra stats already taken\n",__func__);
                        }
                    }
                    if (!train_state->extra_stats) {
                        swith_antenna = sa_apply_secondary_metric(node);
#if SA_DEBUG
                        dtrain_cmd->ant_sel |= SA_DBG_CODE_RSSI_BETTER;
#endif
                    }
                } else {
                    if (per < train_state->per) {
                        swith_antenna = 1;
#if SA_DEBUG
                    dtrain_cmd->ant_sel |= SA_DBG_CODE_PER_BETTER;
#endif
                    }
                }
            } /* train_state->rate == train_state.last_rate */
        } /* else part of per < lowerbound */

        if (swith_antenna) {
            /* choose trained antenna as current antenna */
            train_info->selected_antenna = train_state->ant_map[train_state->antenna];

            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: progeressive antenna selection to: %d \n", print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), train_state->ant_map[train_state->antenna]);
#if SA_DEBUG
            dtrain_cmd->ant_sel |= SA_DBG_CODE_ANT_SWITCH;
#endif

            status |= SMART_ANT_TX_CONFIG_REQUIRED;

            train_state->per = per;
            train_state->nextant_nbad = train_data->nBad;
            train_state->nextant_nframes = train_data->nFrames;
            /* copy RSSI Samples */
            OS_MEMCPY(&train_state->rssi[0][0], &train_data->rssi[0][0], num_rssi_samples);
            train_state->last_rate = train_state->rate; /* Update last trained rate; if we per > lowerbound we are not updating this previously */

            train_state->extra_sel = train_state->extra_cmp;

            if (per < sa_params->lower_bound) {
                train_nxt_rate = 1;
            }
        } /* switch Antenna */
    }  /* else part of update from not current antenna */

extra_stats:
    if (train_state->extra_stats) {
        if (!train_state->extra_sel) {
            status |= SMART_ANT_TRAINING_REQUIRED;
            sa_set_train_params(node, train_state->ant_map[0], train_state->rate);
        } else if (!train_state->extra_cmp) {
            status |= SMART_ANT_TRAINING_REQUIRED;
            sa_set_train_params(node, train_state->ant_map[train_state->antenna], train_state->rate);
        } else {
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"%s: error in getting extra stats\n",__func__);
        }
        return status;
    }
    train_state->extra_cmp = 0;
    if (train_nxt_rate != 0) {
        /* Move one rate above and do the training with same antenna */
        ridx = sa_move_rate(node, train_nxt_rate);
        if (ridx < 0) {
            /* We are at the extreme. no next rate  */
            train_nxt_antenna = 1;
            train_state->last_rate = train_state->rate;
        } else {
            train_state->last_train_dir = train_nxt_rate;
            train_state->last_rate = train_state->rate;
            train_nxt_antenna = 0;
            train_state->rate = (uint32_t)ridx;
            status |= SMART_ANT_TRAINING_REQUIRED;
            sa_set_train_params(node, train_state->ant_map[train_state->antenna], train_state->rate);

            if (train_state->ant_map[train_state->antenna] == train_info->selected_antenna) {
                train_state->extra_sel = 0;
            }
        }
    }

    if (train_nxt_antenna) {
        train_state->last_train_dir = 0;
        /* update antenna map */
        if (train_state->ant_map[0] != train_info->selected_antenna) {
            sa_update_ant_map(node,train_info->selected_antenna);
        }

        train_state->skipmask |= (1 << train_state->antenna);
        skipmask = train_state->skipmask;
        /* to skip the antenna combination already trained */
        while (train_state->antenna < sa_params->num_tx_antennas) {
            train_state->antenna++;
            if (!((1 << train_state->antenna) & skipmask)) {
                break;
            }
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s: Training skipped for %d ant map %d \n",
                            __func__,train_state->antenna,train_state->ant_map[train_state->antenna]);
        }

        if (train_state->antenna < sa_params->num_tx_antennas) {
            status |= SMART_ANT_TRAINING_REQUIRED;
            sa_set_train_params(node, train_state->ant_map[train_state->antenna], train_state->rate);
        } else {
            /* training completed */
            SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s: Training completed %d %d %d %d\n"
                    , print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), train_state->ant_map[0], train_state->ant_map[1]
                    ,train_state->ant_map[2],train_state->ant_map[3]);
            node->train_info.smartantenna_state = SMARTANTENNA_DEFAULT;
            node->train_info.intense_train = 0;

            /* DO RX antenna selection */

            if (sa_params->train_enable & SA_RX_TRAIN_EN) {
                status |= sa_select_rx_antenna(node,radio);
            } else {
                /* update node count info for each combination */
                radio->node_count_per_antenna[node->train_info.previous_selected_antenna]--;
                radio->node_count_per_antenna[node->train_info.selected_antenna]++;
                train_info->previous_selected_antenna = train_info->selected_antenna;
            }

            train_info->is_training = 0;
            train_info->ts_traindone = A_MS_TICKGET();
            train_info->training_start = 0;

            train_info->ts_perfslot = train_info->ts_traindone;
#if SA_DEBUG
            train_info->last_training_time = train_info->ts_traindone - train_info->ts_trainstart;
#endif
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s :Tx antenna Selected  %d : train time %d\n",print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), train_info->selected_antenna,(train_info->ts_traindone - train_info->ts_trainstart));
        } /* training completed */
    } /* train next antenna */

    return status;
}

/**
 *   sa_check_bw_change: detects the bw change when training is happenning
 *   @node: pointer to node
 *
 *   returns status indicating bw change
 */
uint32_t sa_check_bw_change(struct sa_node_ccp *node)
{
    uint32_t *nppdu_bw = &node->train_info.nppdu_bw[SA_BW_20];
    uint8_t bw;
    struct node_train_stats *train_state = &node->train_info.train_state;

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s %d %d %d",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),node->train_info.nppdu_bw[SA_BW_80],node->train_info.nppdu_bw[SA_BW_40],node->train_info.nppdu_bw[SA_BW_20]);
    switch (train_state->bw) {
        case SA_BW_80:
            bw = (nppdu_bw[SA_BW_20] > nppdu_bw[SA_BW_40]) ?
                ((nppdu_bw[SA_BW_20] > nppdu_bw[SA_BW_80]) ? SA_BW_20 : SA_BW_80) :
                ((nppdu_bw[SA_BW_40] > nppdu_bw[SA_BW_80]) ? SA_BW_40 : SA_BW_80);
            break;
        case SA_BW_40:
            bw = (nppdu_bw[SA_BW_20] > nppdu_bw[SA_BW_40]) ? SA_BW_20 : SA_BW_40;
            break;
        case SA_BW_20:
            bw = SA_BW_20;
	    break;
        default:
            bw = SA_BW_20;
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id, " Error: %s invalid bw",print_mac_addr((const uint8_t *)node->node_info.mac_addr));
    }

    if (bw != train_state->bw) {
        train_state->bw_change = 1;
        train_state->bw = bw;
        return SA_STATUS_SUCCESS;
    }
    return SA_STATUS_FAILURE;
}

/**
 *   sa_check_retrain_trigger : checks for retraining triggers
 *   @node: pointer to node
 *
 *   returns status indicating trigger
 */
uint32_t sa_check_retrain_trigger(struct sa_node_ccp *node)
{
    uint8_t status = 0;
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;
    uint32_t current_ts;
    uint32_t elapsed_time;
    int bw;
    struct sa_perf_info *perf_info;
    uint32_t ins_goodput;
    uint32_t bw_active;
    uint32_t diff;
    uint32_t threshold;
    uint8_t train_enable = node->train_info.train_enable & sa_params->train_enable;
#if not_yet
    uint32_t loop_count = 0;
#endif

    current_ts = A_MS_TICKGET();
    elapsed_time = current_ts - node->train_info.ts_traindone;

    if (((elapsed_time >= sa_params->retrain_interval) && (train_enable & SA_PERIOD_TRAIN_EN))
                    || (node->train_info.training_start && (train_enable & (SA_PERIOD_TRAIN_EN | SA_PERF_TRAIN_EN | SA_INIT_TRAIN_EN)))) {
        /* check for periodic trigger */
        node->train_info.smartantenna_state = SMARTANTENNA_PRETRAIN;
        status |= SMART_ANT_TRAINING_REQUIRED;
#if SA_DEBUG
            node->train_info.prd_trigger_count++;
#endif
        SA_DEBUG_PRINT(SA_DEBUG_LVL2, node->node_info.radio_id, " Periodic Trigger : Mac: %s : TimeStamp: 0x%x\n", print_mac_addr((const uint8_t *)node->node_info.mac_addr), A_MS_TICKGET());
    } else if (train_enable & SA_PERF_TRAIN_EN) {
        elapsed_time = current_ts - node->train_info.ts_perfslot;

#if not_yet
        while (elapsed_time >= (sa_params->perftrain_interval << 1)) {
            loop_count++;
            elapsed_time -= sa_params->perftrain_interval;
            if (loop_count > SA_MAX_NOTRAFFIC_TIME) {
                /* too much idle time - reset perf counters */
                break;
            }
        }
#endif

        /* check for peformence trigger */
        if (elapsed_time >= sa_params->perftrain_interval) {
            SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id, " %ss: %s %d %d %d ",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),node->rate_stats.nppdu_bw[SA_BW_20],node->rate_stats.nppdu_bw[SA_BW_40],node->rate_stats.nppdu_bw[SA_BW_80]);
            /* for each band width check performence */
            for (bw = 0; bw < SA_MAX_BW; bw++) {
                bw_active = 0;
                perf_info = &node->train_info.perf_info[bw];

                if (node->rate_stats.nppdu_bw[bw] > sa_params->num_min_pkt_threshod[bw]) {
                    bw_active = 1;
                }

                if (bw_active) {
                    ins_goodput = node->rate_stats.ins_goodput[bw];
                    if (perf_info->gput_avg_interval < (sa_params->goodput_ignore_interval + sa_params->goodput_avg_interval)) {
                        if (perf_info->gput_avg_interval >= sa_params->goodput_ignore_interval) {
                            /* do goodput averaging */
                            if (perf_info->avg_goodput) {
                                perf_info->avg_goodput += ins_goodput;
                                perf_info->avg_goodput >>= 1;
                            } else {
                                perf_info->avg_goodput = ins_goodput;
                            }
                        }
                        perf_info->gput_avg_interval++;
                    } else {
                        /* check for trigger */
                        threshold = (perf_info->avg_goodput * sa_params->max_throughput_change) / 100;
                        /* Threshold should be  max of %max_throughput_change of goodput or min_goodput_threshold.If goodput
                           are less then %drop of goodput will be zero to avoid these condations min_goodput_threshold is configured to MIN_GOODPUT_THRESHOLD */

                        threshold = (threshold > sa_params->min_goodput_threshold) ? threshold : sa_params->min_goodput_threshold;
                        diff = perf_info->avg_goodput > ins_goodput
                            ? perf_info->avg_goodput - ins_goodput
                            : ins_goodput - perf_info->avg_goodput;

                        if (diff >= threshold) { /* throughput max_throughput_change by %drop of previous intervel good put*/
                            /* Need to consider both directions */
                            switch (perf_info->trigger_type)
                            {
                                case INIT_TRIGGER:
                                    perf_info->hysteresis++;
                                    if (perf_info->avg_goodput < ins_goodput)  { /* +ve trigger */
                                        perf_info->trigger_type = POS_TRIGGER;
                                    } else {
                                        perf_info->trigger_type = NEG_TRIGGER;
                                    }
                                    break;
                                case POS_TRIGGER:
                                    if (perf_info->avg_goodput < ins_goodput)  { /* +ve trigger */
                                        perf_info->hysteresis++;
                                    } else {
                                        perf_info->hysteresis = 0; /* Reset hysteresis */
                                        perf_info->trigger_type = INIT_TRIGGER;
                                    }
                                    break;
                                case NEG_TRIGGER:
                                    if (perf_info->avg_goodput > ins_goodput)  { /* -ve trigger */
                                        perf_info->hysteresis++;
                                    } else {
                                        perf_info->hysteresis = 0; /* Reset hysteresis */
                                        perf_info->trigger_type = INIT_TRIGGER;
                                    }
                                    break;
                                default:
                                    SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id, " Error: %s invalid trigger type",print_mac_addr((const uint8_t *)node->node_info.mac_addr));
                            }

                            if (perf_info->hysteresis >= sa_params->hysteresis) {
                                SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id, "BW %d Avg Goodput : %d : Ins Goodput %d hysteresis: %d\n",bw,
                                        node->train_info.perf_info[bw].avg_goodput, ins_goodput, node->train_info.perf_info[bw].hysteresis);
                                perf_info->hysteresis = 0;
                                perf_info->avg_goodput = 0;
                                perf_info->gput_avg_interval = 0;
                                perf_info->trigger_type = INIT_TRIGGER;
                                node->train_info.smartantenna_state = SMARTANTENNA_PRETRAIN;
                                status |= SMART_ANT_TRAINING_REQUIRED;

#if SA_DEBUG
                                node->train_info.perf_trigger_count++;
#endif
                                SA_DEBUG_PRINT(SA_DEBUG_LVL2, node->node_info.radio_id, " Perf Trigger : Mac: %s : TimeStamp: 0x%x\n", print_mac_addr((const uint8_t *)node->node_info.mac_addr), A_MS_TICKGET());
                            }
                        } else {
                            perf_info->hysteresis = 0;
                            perf_info->trigger_type = INIT_TRIGGER;
                            perf_info->avg_goodput = ((7 * perf_info->avg_goodput) + ins_goodput) >> 3;
                        }
                    }
                } else {
                    /* bw is not active */
                    if (perf_info->gput_avg_interval < (sa_params->goodput_ignore_interval + sa_params->goodput_avg_interval)) {
                        perf_info->gput_avg_interval = (perf_info->gput_avg_interval > sa_params->goodput_ignore_interval) ? sa_params->goodput_ignore_interval : perf_info->gput_avg_interval;
                    }
                }
                SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id, "BW %d Avg Goodput : %d : Ins Goodput %d nppdu %d hysteresis: %d\n",bw,
                node->train_info.perf_info[bw].avg_goodput, node->rate_stats.ins_goodput[bw], node->rate_stats.nppdu_bw[bw], node->train_info.perf_info[bw].hysteresis);

                /* reset info in rate stats only if training is not triggered */
                if (!status) {
                    node->rate_stats.ins_goodput[bw] = 0;
                    node->rate_stats.nppdu_bw[bw] = 0;
                    node->rate_stats.nppdu_gp[bw] = 0;
                } else {
                    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id, " %s Trigger: %s %d %d %d ",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),node->rate_stats.nppdu_bw[SA_BW_20],node->rate_stats.nppdu_bw[SA_BW_40],node->rate_stats.nppdu_bw[SA_BW_80]);
                    /* break as already trigger found */
                    break;
                }
            }
            node->train_info.ts_perfslot = current_ts;
        }
    }
    if (!status) {
        OS_MEMZERO(&node->rate_stats.npackets[0][0], (sizeof(uint32_t) * SA_MAX_BW * MAX_MCS_RATES));
        OS_MEMZERO(&node->rate_stats.npackets_legacy[0], (sizeof(uint32_t) * MAX_CCK_OFDM_RATES));
    }
    return status;
}

/**
 *   __sa_update_txfeedback: api function that updated tx feedback
 *   @ccp: pointer to node
 *   @feedback: pointer to tx feedback
 *   @pointer to status
 *
 *   returns success or failure
 */
int __sa_update_txfeedback(void *ccp, struct sa_tx_feedback *feedback, uint8_t *status)
{
    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;
    struct radio_config *radio = &g_sa.radio[node->node_info.radio_id];
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;
    uint8_t bw_sent, num_fb = 1, nfb = 0, rate = 0, npkts = 0, nbad = 0, check_bw_change = 0;
    uint16_t npkts_total = 0;
    uint8_t rate_configured;

    if (sa_enable_test_mode) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"%s:  Mac: %s, nFrame:%d nBad: %d SR: [%d %d %d %d %d %d] LR: [%d %d %d %d %d %d]"
                "Rate:[0x%x 0x%x] RateIdx: %d Antenna: [0x%06x 0x%06x] RSSI: [0x%x 0x%x 0x%x]"
                "Trainpkt: %d ratemaxphy: 0x%04x goodput:%d \n"
                ,__func__ , print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), feedback->nPackets, feedback->nBad
                ,feedback->nshort_retries[0], feedback->nshort_retries[1], feedback->nshort_retries[2]
                ,feedback->nshort_retries[4], feedback->nshort_retries[5], feedback->nshort_retries[6]
                ,feedback->nlong_retries[0], feedback->nlong_retries[1], feedback->nlong_retries[2]
                ,feedback->nlong_retries[4], feedback->nlong_retries[5], feedback->nlong_retries[6]
                ,feedback->rate_mcs[0], feedback->rate_mcs[1],feedback->rate_index
                ,feedback->tx_antenna[0], feedback->tx_antenna[1]
                ,feedback->rssi[0], feedback->rssi[1], feedback->rssi[2]
                ,feedback->is_trainpkt,feedback->ratemaxphy
                ,feedback->goodput);
        return 0;
    }

    if (node->train_info.smartantenna_state == SMARTANTENNA_PRETRAIN) {
        if (feedback->nPackets > feedback->nBad) {
            sa_update_ratestats(node, feedback);
            node->train_info.txed_packets += feedback->nPackets;
            if (feedback->num_comb_feedback == 1) {
                node->train_info.txed_packets += feedback->comb_fb[0].npkts;
            } else if (feedback->num_comb_feedback == 2) {
                node->train_info.txed_packets += feedback->comb_fb[0].npkts;
                node->train_info.txed_packets += feedback->comb_fb[1].npkts;
            }
        }
        if (node->train_info.txed_packets >= sa_params->num_pretrain_packets) {
            *status |= SMART_ANT_TRAINING_REQUIRED;
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"PRE TRAINING COMPLETED %s\n",print_mac_addr((const uint8_t *)node->node_info.mac_addr));
        }
    } else if (node->train_info.smartantenna_state == SMARTANTENNA_TRAIN_INPROGRESS) {
        /* update PER and RSSI stats*/
        SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"%s:  Mac: %s, nFrame:%d nBad: %d SR: [%d %d %d %d %d %d] LR: [%d %d %d %d %d %d]"
            "Rate:[0x%x 0x%x] RateIdx: %d Antenna: [0x%06x 0x%06x] RSSI: [0x%x 0x%x 0x%x]"
            "Trainpkt: %d ratemaxphy: 0x%04x goodput:%d \n"
            ,__func__ , print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)), feedback->nPackets, feedback->nBad
            ,feedback->nshort_retries[0], feedback->nshort_retries[1], feedback->nshort_retries[2]
            ,feedback->nshort_retries[4], feedback->nshort_retries[5], feedback->nshort_retries[6]
            ,feedback->nlong_retries[0], feedback->nlong_retries[1], feedback->nlong_retries[2]
            ,feedback->nlong_retries[4], feedback->nlong_retries[5], feedback->nlong_retries[6]
            ,feedback->rate_mcs[0], feedback->rate_mcs[1],feedback->rate_index
            ,feedback->tx_antenna[0], feedback->tx_antenna[1]
            ,feedback->rssi[0], feedback->rssi[1], feedback->rssi[2]
            ,feedback->is_trainpkt,feedback->ratemaxphy
            ,feedback->goodput);

        if (feedback->num_comb_feedback) {
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"num comp feedbacks: %d \n",feedback->num_comb_feedback);
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"fb1: rate 0x%x bw 0x%d nppdu_combined %d npkts %d nbad %d \n",feedback->comb_fb[0].rate,(feedback->comb_fb[0].bw >> 4 & 0x0F), (feedback->comb_fb[0].bw & 0x0F), feedback->comb_fb[0].npkts,feedback->comb_fb[0].nbad);
            SA_DEBUG_PRINT(SA_DEBUG_LVL4,node->node_info.radio_id,"fb2: rate 0x%x bw 0x%d nppdu_combined %d npkts %d nbad %d \n",feedback->comb_fb[1].rate,(feedback->comb_fb[1].bw >> 4 & 0x0F), (feedback->comb_fb[1].bw & 0x0F), feedback->comb_fb[1].npkts,feedback->comb_fb[1].nbad);
        }

        if (feedback->is_trainpkt) {
            num_fb += feedback->num_comb_feedback;

            while (num_fb) {
                num_fb--;
                if (num_fb) {
                    struct sa_comb_fb *pcomb_fb = &feedback->comb_fb[num_fb - 1];
                    nfb     = SA_GET_NFB_COMB_FEEDBACK(pcomb_fb->bw);
                    bw_sent = SA_GET_BW_COMB_FEEDBACK(pcomb_fb->bw);
                    rate  = pcomb_fb->rate;
                    npkts = pcomb_fb->npkts;
                    nbad  = pcomb_fb->nbad;
                } else {
                    nfb   = 1;
                    bw_sent = SA_GET_BW_FEEDBACK(feedback->rate_index);
                    rate = SA_GET_RATE_FEEDBACK(feedback->rate_mcs[0],bw_sent);
                    npkts = feedback->nPackets;
                    nbad = feedback->nBad;
                }
                rate_configured = SA_GET_RATE_FEEDBACK(node->train_data.ratecode,bw_sent);

                if ((feedback->tx_antenna[0] != node->train_data.antenna) ||  ((rate_configured != rate))) {
                    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"train pkt with mismatch %d train antenna %d rates %x %x\n",feedback->tx_antenna[0],node->train_data.antenna,rate_configured,rate);
                } else {
                    node->train_info.nppdu_bw[bw_sent] += nfb;
                    node->train_info.nppdu += nfb;
                    if (bw_sent == node->train_info.train_state.bw) {
                        node->train_data.nBad += nbad;
                        node->train_data.nFrames += npkts;
                        npkts_total += npkts;
                        if (!num_fb) {
                            node->train_data.rssi[0][node->train_data.samples] = (uint8_t)feedback->rssi[0];
                            node->train_data.rssi[1][node->train_data.samples] = (uint8_t)feedback->rssi[1];
                            node->train_data.rssi[2][node->train_data.samples] = (uint8_t)feedback->rssi[2];
                            node->train_data.samples++;
                            if (node->train_data.samples >= sa_params->num_recv_chains) {
                                node->train_data.samples = 0;
                            }
                        }
                    } else {
                        check_bw_change = 1;
                    }
                }
            }

            if (check_bw_change) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s train pkt with bw mismatch %d\n",print_mac_addr((const uint8_t *)node->node_info.mac_addr),node->train_info.nppdu);
                if ((node->train_info.nppdu > sa_params->num_other_bw_pkts_th) && (node->train_info.train_state.bw != SA_BW_20)) {
                    if (!sa_check_bw_change(node)) {
                        *status |= SMART_ANT_TRAINING_REQUIRED;
                    }
                }
            }
            SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"train pkt %d, npkts %d,total train pkts %d fw stats %d\n",feedback->is_trainpkt,npkts_total,node->train_data.nFrames,feedback->goodput);
            if ((feedback->goodput == 0) && ((node->train_data.nFrames) < node->train_data.numpkts)) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"Error: FW out of sync total train pkts %d fw stats %d\n",node->train_data.nFrames,feedback->goodput);
            }
        } else {
            if (feedback->nPackets > feedback->nBad) {
                sa_update_ratestats(node,feedback);
            }
            SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"train pkt %d, npkts %d,total train pkts %d\n",feedback->is_trainpkt,feedback->nPackets,node->train_data.nFrames);
        }

        if (((node->train_data.nFrames) >= node->train_data.numpkts) || (node->train_info.nppdu_bw[node->train_info.train_state.bw] > sa_params->max_train_ppdu)) {
            *status |= sa_process_trainstats(node);
        }
    } else if (node->train_info.smartantenna_state == SMARTANTENNA_TRAIN_HOLD) {
        /* invalid state */
    } else {
        /* SMARTANTENNA_DEFAULT */
        *status |= sa_check_retrain_trigger(node);
        *status |= node->train_info.antenna_change_ind;

        if (node->train_info.antenna_change_ind) {
            /* update node count info for each combination */
            radio->node_count_per_antenna[node->train_info.previous_selected_antenna]--;
            radio->node_count_per_antenna[node->train_info.selected_antenna]++;
            node->train_info.previous_selected_antenna = node->train_info.selected_antenna;
        }

        node->train_info.antenna_change_ind = 0; /* manual antenna change happen when we are only in default state */

        if (sa_params->antenna_change_ind) {
            /* global antenna change indication */
            if (sa_params->antenna_change_ind & SMART_ANT_TX_CONFIG_REQUIRED) {
                if (node->train_info.selected_antenna != sa_params->default_tx_antenna) {
                    node->train_info.selected_antenna = sa_params->default_tx_antenna;
                    *status |= SMART_ANT_TX_CONFIG_REQUIRED;
                    sa_update_ant_map(node,node->train_info.selected_antenna);

                    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"antenna fixed for node %s antenna %d\n",print_mac_addr((const uint8_t *)&(node->node_info.mac_addr)),node->train_info.selected_antenna);

                    /* update node count info for each combination */
                    radio->node_count_per_antenna[node->train_info.previous_selected_antenna]--;
                    radio->node_count_per_antenna[node->train_info.selected_antenna]++;
                    node->train_info.previous_selected_antenna = node->train_info.selected_antenna;

                    if (radio->nodes_connected == radio->node_count_per_antenna[node->train_info.selected_antenna]) {
                        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"global tx antenna change completed antenna %d\n",node->train_info.selected_antenna);
                        sa_params->antenna_change_ind &= ~SMART_ANT_TX_CONFIG_REQUIRED;
                    }
                }
            }
        }

        if (feedback->is_trainpkt) {
            *status |= SMART_ANT_TRAINING_REQUIRED;
        } else {
            if (feedback->nPackets > feedback->nBad) {
                sa_update_ratestats(node,feedback);
            }
        }
    }

    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_update_rxfeedback: api function that updated rx feedback
 *   @ccp: pointer to node
 *   @rx_feedback: pointer to rx feedback
 *   @pointer to status
 *
 *   returns success or failure
 */
int  __sa_update_rxfeedback(void  *ccp, struct sa_rx_feedback *rx_feedback, uint8_t *status)
{

    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;

    SA_DEBUG_PRINT(SA_DEBUG_LVL5,node->node_info.radio_id,"%s: Mac: %s Rate:0x%08x Antenna: 0x%06x Rssi:[0x%08x] [0x%08x] [0x%08x] [0x%08x] npakcets: %d\n"
                                                            ,__func__, print_mac_addr((const uint8_t *)&(node->node_info.mac_addr))
                                                            ,rx_feedback->rx_rate_mcs
                                                            ,rx_feedback->rx_antenna
                                                            ,rx_feedback->rx_rssi[0]
                                                            ,rx_feedback->rx_rssi[1]
                                                            ,rx_feedback->rx_rssi[2]
                                                            ,rx_feedback->rx_rssi[3]
                                                            ,rx_feedback->npackets
                                                            );
    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_get_txantenna: api function to return tx antenne
 *   @ccp: pointer to node
 *   @antenna_array: pointer to antenna_array
 *
 *   returns success or failure
 */
int  __sa_get_txantenna(void *ccp, uint32_t *antenna_array)
{
    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;
    struct sa_node_train_info *train_info = &node->train_info;

    antenna_array[0] = train_info->selected_antenna;
    antenna_array[1] = train_info->selected_antenna;
    antenna_array[2] = train_info->selected_antenna;
    antenna_array[3] = train_info->selected_antenna;

    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_get_node_config: api function to return node config
 *   @ccp: pointer to node
 *   @tx_feedback_config: pointer to config
 *
 *   returns success or failure
 */
int __sa_get_node_config (void *ccp, uint32_t id, uint32_t *config)
{
    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;
    struct sa_node_train_info *train_info = &node->train_info;
    int retval = 0;

    switch (id) {
        case SA_TX_FEEDBACK_CONFIG:
            *config = train_info->tx_feedback_config;
            break;
        default:
            retval = -1;
            break;
    }
    return retval;
}

/**
 *   __sa_get_rxantenna: api function to return rx antenna
 *   @ccp: pointer to node
 *   @rx_antenna: pointer to fill rx antenna
 *
 *   returns success or failure
 */
int __sa_get_rxantenna(enum radioId radio_id, uint32_t *rx_antenna)
{
    *rx_antenna = g_sa.radio[radio_id].default_antenna;
    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_get_txdefaultantenna: api function to set tx default antenna
 *   @ccp: pointer to node
 *   @antenna: pointer to fill antenna
 *
 *   returns success or failure
 */
int  __sa_get_txdefaultantenna(enum radioId radio_id, uint32_t *antenna)
{
    *antenna = g_sa.radio[radio_id].default_antenna;
    return SA_STATUS_SUCCESS;
}

/**
 *   sa_get_train_rate : function to find the train rate when bw is changed.
 *   @node: pointer to node
 *
 *   returns train rate code
 */
uint32_t sa_get_train_rate_reinit(struct sa_node_ccp *node)
{
    uint8_t bw = sa_get_train_bw(node);
    uint32_t rate;
    uint32_t rate_maxphy = node->rate_stats.rate_maxphy;

    switch (bw) {
        case SA_BW_20 :
            rate = (rate_maxphy >> SA_RATEPOSITION_20) & SA_BYTE_MASK;
            break;
        case SA_BW_40 :
            rate = (rate_maxphy >> SA_RATEPOSITION_40) & SA_BYTE_MASK;
            break;
        case SA_BW_80 :
            rate = (rate_maxphy >> SA_RATEPOSITION_80) & SA_BYTE_MASK;
            break;
        default :
            rate = rate_maxphy & SA_BYTE_MASK;
            SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id, " Error: %s node %s invalid bw",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr));
            break;
    }

    return rate;
}

/**
 *   sa_get_train_rate : function to find the train rate
 *   @node: pointer to node
 *
 *   returns train rate code
 */
uint32_t sa_get_train_rate(struct sa_node_ccp *node)
{
    uint8_t bw,sent = 0;
    uint8_t *rate_array;
    uint32_t *stats_array;
    uint32_t num_of_rates;
    uint32_t max = 0,maxidx = 0;
    int i,stat_index;
    uint8_t rate;

    sa_set_train_bw(node);
    bw = sa_get_train_bw(node);

    switch (bw) {
        case SA_BW_20 :
            if (node->node_info.opmode) {
                num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_20];
                rate_array = &node->node_info.rate_cap.ratecode_20[0];
                stats_array = &node->rate_stats.npackets[bw][0];
            } else {
                num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_LEGACY];
                rate_array = &node->node_info.rate_cap.ratecode_legacy[0];
                stats_array = &node->rate_stats.npackets_legacy[0];
            }
            break;
        case SA_BW_40 :
            num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_40];
            rate_array = &node->node_info.rate_cap.ratecode_40[0];
            stats_array = &node->rate_stats.npackets[bw][0];
            break;
        default:
            num_of_rates = node->node_info.rate_cap.ratecount[SA_RATECOUNT_IDX_80];
            rate_array = &node->node_info.rate_cap.ratecode_80[0];
            stats_array = &node->rate_stats.npackets[bw][0];
            break;
    }

    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s num_of_rates = %d bw %d opmode %d\n",__func__,num_of_rates,bw,node->node_info.opmode);

    for(i = 0; i < num_of_rates; i++) {
        rate = rate_array[i];
        if(IS_SA_11B_RATE(rate)) {
            stat_index = SA_STAT_INDEX_11B(rate);
        } else if (IS_SA_11G_RATE(rate)) {
            stat_index = SA_STAT_INDEX_11G(rate);
        } else {
            stat_index = SA_STAT_INDEX_11N_11AC(SA_GET_NSS_FROM_RATE(rate),SA_GET_MCS_FROM_RATE(rate));
        }
        if(stats_array[stat_index]) {
            sent = 1;
        }
        SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s max = %d,rate=%x, i=%d,stat_index=%d,num=%d\n",__func__,max,rate,i,stat_index,stats_array[stat_index]);
        if (max <= stats_array[stat_index]) {
            max = stats_array[stat_index];
            maxidx = i;
        }
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s max =%d,maxidx = %d, sent %d,\n",__func__,max, maxidx,sent);

    /* reset rate stats */
    OS_MEMZERO(&node->rate_stats, sizeof(struct sa_node_rate_stats));

    if (!sent) {
        /* no packets sent in this interval, use previous rate or other bw TODO */
        return sa_get_max_rate(node);
    }

    SA_DEBUG_PRINT(SA_DEBUG_LVL3,node->node_info.radio_id,"%s rate %x,\n",__func__,rate_array[maxidx]);
    return rate_array[maxidx];
}

/**
 *   __sa_get_training_info: api function that gives traing info
 *   @ccp: pointer to node
 *   @rate_array: pointer to fill rates
 *   @antenna_array: pointer to fill antenna array
 *   @numpkts: pointer to fill num train packets
 *
 *   returns success or failure
 */
int  __sa_get_training_info(void *ccp , uint32_t *rate_array, uint32_t *antenna_array, uint32_t *numpkts)
{
    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;
    struct sa_node_train_info *train_info = &node->train_info;
    uint32_t ratecode;
    int i;
    struct sa_perf_info *perf_info;
    struct smartantenna_params *sa_params = &g_sa.radio[node->node_info.radio_id].sa_params;

    if (sa_enable_test_mode) {

        rate_array[0] = g_train_params.ratecode;
        rate_array[1] = g_train_params.ratecode;
        rate_array[2] = g_train_params.ratecode;
        rate_array[3] = g_train_params.ratecode;

        antenna_array[0] = g_train_params.antenna;
        antenna_array[1] = g_train_params.antenna;
        antenna_array[2] = g_train_params.antenna;
        antenna_array[3] = g_train_params.antenna;

        *numpkts = g_train_params.numpkts;
        return SA_STATUS_SUCCESS;
    }


    if(node->train_info.smartantenna_state == SMARTANTENNA_PRETRAIN) {
        node->train_info.smartantenna_state = SMARTANTENNA_TRAIN_INPROGRESS;

        node->train_info.ts_trainstart = A_MS_TICKGET();
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"Training start:%s :mac: %s 0x%08x \n",__func__ ,print_mac_addr((const uint8_t *)node->node_info.mac_addr),node->train_info.ts_trainstart);

        ratecode = sa_get_train_rate(node);

        /* reset perf info */
        for (i = 0; i < SA_MAX_BW; i++) {
            perf_info = &node->train_info.perf_info[i];
            perf_info->trigger_type = INIT_TRIGGER;
            perf_info->hysteresis = 0;
            perf_info->gput_avg_interval = 0;
            perf_info->avg_goodput = 0;
        }

        /* initialize train info */
        train_info->is_training = 1;
        train_info->train_state.antenna = 0;
        train_info->train_state.skipmask = 0;
        train_info->train_state.rate = ratecode;
        train_info->train_state.first_per = 1;
        train_info->train_state.last_train_dir = 0;
        train_info->train_state.last_rate = 0;
        train_info->train_state.extra_sel = 0;
        train_info->train_state.extra_cmp = 0;
        train_info->intense_train = 1;

        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s initial rate code 0x%x, bw %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),ratecode, train_info->train_state.bw);
        sa_set_train_params(node, train_info->train_state.ant_map[0], train_info->train_state.rate);
#if SA_DEBUG
        node->debug_data.cmd_count = 0;
#endif
    } else if ((node->train_info.smartantenna_state == SMARTANTENNA_TRAIN_INPROGRESS) && (train_info->train_state.bw_change)) {
        ratecode = sa_get_train_rate_reinit(node);

        train_info->train_state.antenna = 0;
        train_info->train_state.skipmask &= ~1;
        train_info->train_state.rate = ratecode;
        train_info->train_state.first_per = 1;
        train_info->train_state.last_train_dir = 0;
        train_info->train_state.last_rate = 0;
        train_info->train_state.extra_sel = 0;
        train_info->train_state.extra_cmp = 0;
        train_info->intense_train = 1;
        train_info->train_state.bw_change = 0;

        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s mac %s re init rate code 0x%x, bw %d\n",__func__,print_mac_addr((const uint8_t *)node->node_info.mac_addr),ratecode, train_info->train_state.bw);
        sa_set_train_params(node, train_info->train_state.ant_map[0], train_info->train_state.rate);
    } else if ((node->train_info.smartantenna_state == SMARTANTENNA_DEFAULT)) {
        sa_cancel_train_params(node, node->train_info.selected_antenna);
    }

    sa_get_train_rate_antenna(node, rate_array, antenna_array,numpkts);
    /* reset train stats */
    node->train_data.nBad = 0;
    node->train_data.nFrames = 0;
    node->train_data.samples = 0;
    node->train_info.nppdu = 0;
    OS_MEMZERO(&node->train_data.rssi[0][0], (SA_MAX_RSSI_SAMPLES * sa_params->num_recv_chains));
    OS_MEMZERO(&node->train_info.nppdu_bw[0],sizeof(node->train_info.nppdu_bw));

    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_get_bcn_txantenna: api function that gives beacon antenna info
 *   @ccp: pointer to node
 *   @bcn_txantenna: pointer to fill beacon antenna
 *
 *   returns success or failure
 */
int __sa_get_bcn_txantenna(enum radioId radio_id, uint32_t *bcn_txantenna)
{
    if (sa_enable_test_mode) {
        *bcn_txantenna = g_sa.radio[radio_id].bcn_antenna;
    } else  {
        *bcn_txantenna = g_sa.radio[radio_id].default_antenna;
    }
    return SA_STATUS_SUCCESS;
}

/**
 *   sa_init_antmap: function that initializes antenna mapping array
 *   @ccp: pointer to node
 *
 *   returns success or failure
 */
int sa_init_antmap(struct sa_node_ccp *ccp)
{
    int i,temp_ant,temp_ant_mask;
    struct radio_config *radio = &g_sa.radio[ccp->node_info.radio_id];
    struct smartantenna_params *sa_params = &g_sa.radio[ccp->node_info.radio_id].sa_params;
    int chainmask;

    chainmask = sa_get_tx_chainmask(radio);

    if (sa_enable_test_mode == 0) {
        ccp->train_info.train_state.ant_map[0] = 0;
        for (i = 1,temp_ant = 0; i < sa_params->num_tx_antennas; i++) {
        /* to skip the antenna combination for unused chain */
            do {
                temp_ant++;
                if(temp_ant >= sa_params->num_tx_antennas || chainmask == 0){
                    break;
                }
                temp_ant_mask = temp_ant | chainmask;
            } while((temp_ant_mask ^ chainmask) != 0);

            ccp->train_info.train_state.ant_map[i] = temp_ant;


        }
        for (i = 0; i < sa_params->num_tx_antennas; i++) {
            SA_DEBUG_PRINT(SA_DEBUG_LVL2,ccp->node_info.radio_id,"%s Mac: %s RadioId: %d ant_map[%d] = %d\n",
            __func__, print_mac_addr((const uint8_t *)ccp->node_info.mac_addr), ccp->node_info.radio_id,i,ccp->train_info.train_state.ant_map[i]);
        }

    } else {
          for (i = 0; i < sa_params->num_tx_antennas; i++) {
              ccp->train_info.train_state.ant_map[i] = i;
          }
      }

    return SA_STATUS_SUCCESS;
}

/**
 *   sa_init_node: function that initializes default parameters of node
 *   @ccp: pointer to node
 *
 *   returns success or failure
 */
int sa_init_node( struct sa_node_ccp *ccp,struct radio_config *radio)
{
    uint8_t train_enable = 0;

    ccp->train_info.selected_antenna = radio->sa_params.default_tx_antenna;
    ccp->train_info.previous_selected_antenna = radio->sa_params.default_tx_antenna;
    ccp->train_info.training_start = 0;
    ccp->train_info.last_train_bw = SA_BW_20;
    ccp->train_info.tx_feedback_config = SA_TX_FEEDBACK_CONFIG_DEFAULT;
    ccp->train_info.train_enable = (SA_INIT_TRAIN_EN | SA_PERIOD_TRAIN_EN | SA_PERF_TRAIN_EN | SA_RX_TRAIN_EN);

    train_enable = ccp->train_info.train_enable & radio->sa_params.train_enable;
    if (train_enable & SA_INIT_TRAIN_EN) {
        ccp->train_info.smartantenna_state = SMARTANTENNA_PRETRAIN;
    } else {
        ccp->train_info.smartantenna_state = SMARTANTENNA_DEFAULT;
    }

    sa_init_antmap(ccp);
    sa_update_ant_map(ccp,ccp->train_info.selected_antenna);
    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_node_connect: api function that indicates node addition
 *   @ccp: pointer to pointer of node
 *   @node_info: node info
 *
 *   returns success or failure
 */
int __sa_node_connect(void  **ccp, struct  sa_node_info *node_info)
{
    /* allocate memory for node ccp */
    int i=0;
    struct sa_node_ccp *node = NULL;
    struct radio_config *radio;

    if (*ccp == NULL) {
        node = (struct sa_node_ccp *)
                qdf_mem_malloc_atomic(sizeof(struct sa_node_ccp));
        if (node == NULL) {
            *ccp = NULL;
            return SA_STATUS_FAILURE;
        }
        *ccp = node;
    } else {
        node = *ccp;
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,
            "%s called with non NULL ccp, Mac: %s RadioId: %d max bw: %d"
            " chainmask: %d opmode: %d  channel : %d\n",
            __func__, print_mac_addr((const uint8_t *)node_info->mac_addr),
            node_info->radio_id, node_info->max_ch_bw, node_info->chainmask,
            node_info->opmode, node_info->channel_num);
    }

    /* init default values for the node */
    OS_MEMZERO(node, sizeof(struct sa_node_ccp));
    OS_MEMCPY(&node->node_info, node_info, sizeof(struct sa_node_info));

    radio = &g_sa.radio[node->node_info.radio_id];

    sa_init_node(node, radio);

    radio->nodes_connected++;
    radio->node_count_per_antenna[node->train_info.previous_selected_antenna]++;


    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s Mac: %s RadioId: %d max bw: %d chainmask: %d opmode: %d  channel : %d\n",
            __func__, print_mac_addr((const uint8_t *)node_info->mac_addr), node_info->radio_id, node_info->max_ch_bw,
            node_info->chainmask, node_info->opmode, node_info->channel_num);

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," Legacy Rates: Count: %d: ", node_info->rate_cap.ratecount[0]);
    for (i =0 ;i < node_info->rate_cap.ratecount[0]; i++) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"0x%x, ", node_info->rate_cap.ratecode_legacy[i]);
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"\n");

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," HT20 Rates: Count: %d: ", node_info->rate_cap.ratecount[1]);
    for (i =0 ;i < node_info->rate_cap.ratecount[1]; i++) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"0x%x, ", node_info->rate_cap.ratecode_20[i]);
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"\n");

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," HT40 Rates: Count: %d: ", node_info->rate_cap.ratecount[2]);
    for (i =0 ;i < node_info->rate_cap.ratecount[2]; i++) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"0x%x, ", node_info->rate_cap.ratecode_40[i]);
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"\n");

    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," HT80 Rates: Count: %d: ", node_info->rate_cap.ratecount[3]);
    for (i =0 ;i < node_info->rate_cap.ratecount[3]; i++) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"0x%x, ", node_info->rate_cap.ratecode_80[i]);
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"\n");

    return SA_STATUS_SUCCESS;
}

/**
 *   __sa_node_disconnect: api function that indicates node disconnect
 *   @ccp: pointer to node
 *
 *   returns success or failure
 */
int  __sa_node_disconnect (void *ccp)
{
    struct sa_node_ccp *node = (struct sa_node_ccp *) ccp;
    struct radio_config *radio;

    if (ccp) {
        radio = &g_sa.radio[node->node_info.radio_id];
        radio->nodes_connected--;
        radio->node_count_per_antenna[node->train_info.previous_selected_antenna]--;

        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id,"%s Mac: %s RadioId: %d max bw: %d chainmask: %d opmode: %d  channel : %d\n",
                __func__, print_mac_addr((const uint8_t *)node->node_info.mac_addr), node->node_info.radio_id, node->node_info.max_ch_bw,
                node->node_info.chainmask, node->node_info.opmode, node->node_info.channel_num);

        qdf_mem_free(ccp);
    }
    return SA_STATUS_SUCCESS;
}

/*
 * @DisplayHelp: displays hep to set/use iwprivs
 *
 *  returns none
 */
void DisplayHelp(void)
{
    qdf_print("    ParamName                          ParamId          Description ");
    qdf_print(" SMART_ANT_PARAM_HELP                    %d    Displays current available commands list ", SMART_ANT_PARAM_HELP);
    qdf_print(" SMART_ANT_PARAM_TRAIN_MODE              %d    Training mode self packet generation or existing traffic mode ", SMART_ANT_PARAM_TRAIN_MODE);
    qdf_print(" SMART_ANT_PARAM_TRAIN_PER_THRESHOLD     %d    Smart antenna lower, upper and per diff thresholds ", SMART_ANT_PARAM_TRAIN_PER_THRESHOLD);
    qdf_print(" SMART_ANT_PARAM_PKT_LEN                 %d    Packet length of the training packet ", SMART_ANT_PARAM_PKT_LEN);
    qdf_print(" SMART_ANT_PARAM_NUM_PKTS                %d    Number of packets requied for training ", SMART_ANT_PARAM_NUM_PKTS);
    qdf_print(" SMART_ANT_PARAM_TRAIN_START             %d    Training init for a particualr node ", SMART_ANT_PARAM_TRAIN_START);
    qdf_print(" SMART_ANT_PARAM_TRAIN_ENABLE            %d    Enable bitmap for init,periodic & performence triggers ", SMART_ANT_PARAM_TRAIN_ENABLE);
    qdf_print(" SMART_ANT_PARAM_PERFTRAIN_INTERVAL      %d    Performence monitoring interval  ", SMART_ANT_PARAM_PERFTRAIN_INTERVAL);
    qdf_print(" SMART_ANT_PARAM_RETRAIN_INTERVAL        %d    Periodic retrain interval ", SMART_ANT_PARAM_RETRAIN_INTERVAL);
    qdf_print(" SMART_ANT_PARAM_PERFTRAIN_THRESHOLD     %d    percentage change in goodput to tigger performance training ",SMART_ANT_PARAM_PERFTRAIN_THRESHOLD);
    qdf_print(" SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD   %d    Minimum Good put threshold to tigger performance training ",SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD);
    qdf_print(" SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL    %d    goodput avg interval",SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL);
    qdf_print(" SMART_ANT_PARAM_DEFAULT_ANTENNA         %d    Default antenna for RX ",SMART_ANT_PARAM_DEFAULT_ANTENNA);
    qdf_print(" SMART_ANT_PARAM_DEFAULT_TX_ANTENNA      %d    Default tx antenna when node connects ",SMART_ANT_PARAM_DEFAULT_TX_ANTENNA);
    qdf_print(" SMART_ANT_PARAM_TX_ANTENNA              %d    Selected antenna for TX ", SMART_ANT_PARAM_TX_ANTENNA);
    qdf_print(" SMART_ANT_PARAM_DBG_LEVEL               %d    Debug level for each radio ", SMART_ANT_PARAM_DBG_LEVEL);
    qdf_print(" SMART_ANT_PARAM_PRETRAIN_PKTS           %d    num of pre train packets",SMART_ANT_PARAM_PRETRAIN_PKTS);
    qdf_print(" SMART_ANT_PARAM_OTHER_BW_PKTS_TH        %d    Threshold for other bw packets",SMART_ANT_PARAM_OTHER_BW_PKTS_TH);
    qdf_print(" SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL %d    goodput ignore interval",SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL);
    qdf_print(" SMART_ANT_PARAM_MIN_PKT_TH_BW20         %d    min num packets in 20 to indicate active bw",SMART_ANT_PARAM_MIN_PKT_TH_BW20);
    qdf_print(" SMART_ANT_PARAM_MIN_PKT_TH_BW40         %d    min num packets in 40 to indicate active bw",SMART_ANT_PARAM_MIN_PKT_TH_BW40);
    qdf_print(" SMART_ANT_PARAM_MIN_PKT_TH_BW80         %d    min num packets in 80 to indicate active bw",SMART_ANT_PARAM_MIN_PKT_TH_BW80);
    qdf_print(" SMART_ANT_PARAM_DEBUG_INFO              %d    Displays node debug info ",SMART_ANT_PARAM_DEBUG_INFO);
    qdf_print(" SMART_ANT_PARAM_MAX_TRAIN_PPDU          %d    max number of train ppdus ",SMART_ANT_PARAM_MAX_TRAIN_PPDU);
    qdf_print(" SMART_ANT_PARAM_PERF_HYSTERESIS         %d    hysteresis for performence based trigger ",SMART_ANT_PARAM_PERF_HYSTERESIS);
    qdf_print(" SMART_ANT_PARAM_TX_FEEDBACK_CONFIG      %d    Tx Feedback config ",SMART_ANT_PARAM_TX_FEEDBACK_CONFIG);
    if (sa_enable_test_mode) {
        qdf_print(" SMART_ANT_PARAM_BCN_ANTENNA             %d    Beacon Antenna  ",SMART_ANT_PARAM_BCN_ANTENNA);
        qdf_print(" SMART_ANT_PARAM_TRAIN_RATE_TESTMODE     %d    Training Rate Code in test mode",SMART_ANT_PARAM_TRAIN_RATE_TESTMODE);
        qdf_print(" SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE  %d    Training Antenna in test mode ",SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE);
        qdf_print(" SMART_ANT_PARAM_TRAIN_PACKETS_TETSMODE  %d    Number of training packets in test mode ",SMART_ANT_PARAM_TRAIN_PACKETS_TETSMODE);
        qdf_print(" SMART_ANT_PARAM_TRAIN_START_TETSMODE    %d    Training start in test mode  ",SMART_ANT_PARAM_TRAIN_START_TETSMODE);
    }
}

void sa_update_ant_map(struct sa_node_ccp *node,uint32_t selected_antenna)
{
    int i;
    uint32_t temp_ant;

    for (i = 0; i < g_sa.radio[node->node_info.radio_id].sa_params.num_tx_antennas; i++) {
        if (node->train_info.train_state.ant_map[i] == node->train_info.selected_antenna) {
            break;
        }
    }
    if (i == g_sa.radio[node->node_info.radio_id].sa_params.num_tx_antennas) {
        SA_DEBUG_PRINT(SA_DEBUG_LVL2,node->node_info.radio_id," %s:Error in changing selected antenna \n", __func__);
    }

    temp_ant = node->train_info.train_state.ant_map[0];
    node->train_info.train_state.ant_map[0] = node->train_info.train_state.ant_map[i];
    node->train_info.train_state.ant_map[i] = temp_ant;
}

/**
 *   __sa_set_param: api function to set configuration parameter
 *   @params: pointer to structure containg configuration values
 *
 *   returns success or failure
 */
int  __sa_set_param (struct sa_config_params *params)
{
    int retval = 0;
    struct sa_node_ccp *node = NULL;
    struct smartantenna_params *sa_params = NULL;
    struct radio_config *radio = &g_sa.radio[params->radio_id];
    OS_ASSERT(params->radio_id < SA_MAX_RADIOS);

    SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"%s: Type:%d Id: %d value: %d radioId: %d\n", __func__, params->param_type
                                                                                   , params->param_id
                                                                                   , params->param_value
                                                                                   , params->radio_id
                                                                                   );

    if (radio->init_done) {
        sa_params = &radio->sa_params;
    } else {
        return SA_STATUS_FAILURE;
    }

    switch (params->param_id)
    {
        case SMART_ANT_PARAM_HELP:
             DisplayHelp();
             break;
        case SMART_ANT_PARAM_TRAIN_MODE:
            sa_params->training_mode = params->param_value;
            break;
        case SMART_ANT_PARAM_TRAIN_PER_THRESHOLD:
            sa_params->lower_bound = (params->param_value) & SA_BYTE_MASK;
            sa_params->upper_bound = (params->param_value >> SA_UB_POSITION) & SA_BYTE_MASK;
            sa_params->per_diff_threshold = (params->param_value >> SA_PER_THRESHOLD_POSITION) & SA_BYTE_MASK;
            sa_params->config = (params->param_value >> SA_CONFIG_POSITION) & SA_BYTE_MASK;
            break;
        case SMART_ANT_PARAM_PKT_LEN:
            sa_params->packetlen = params->param_value;
            break;
        case SMART_ANT_PARAM_NUM_PKTS:
            sa_params->n_packets_to_train = params->param_value;
            break;
        case SMART_ANT_PARAM_PERFTRAIN_INTERVAL:
            sa_params->perftrain_interval = params->param_value;
            break;
        case SMART_ANT_PARAM_RETRAIN_INTERVAL:
            sa_params->retrain_interval = params->param_value;
            break;
        case SMART_ANT_PARAM_PERFTRAIN_THRESHOLD:
            sa_params->max_throughput_change = params->param_value;
            break;
        case SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD:
            sa_params->min_goodput_threshold = params->param_value;
            break;
        case SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL:
            sa_params->goodput_avg_interval = params->param_value;
            break;
        case SMART_ANT_PARAM_DEFAULT_ANTENNA:
            radio->default_antenna = params->param_value;
            retval |= SMART_ANT_RX_CONFIG_REQUIRED;
            sa_params->train_enable &= ~(SA_RX_TRAIN_EN);
            break;
        case SMART_ANT_PARAM_DEFAULT_TX_ANTENNA:
            sa_params->default_tx_antenna = params->param_value;
            break;
        case SMART_ANT_PARAM_TX_ANTENNA:
            if (sa_params->train_enable & (SA_INIT_TRAIN_EN | SA_PERF_TRAIN_EN | SA_PERIOD_TRAIN_EN)) {
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"Fixing Antenna is not permitted when Smart Antenna algo is enabled %d\n",sa_params->train_enable);
                return SA_STATUS_FAILURE;
            }
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                node->train_info.selected_antenna = params->param_value;
                sa_update_ant_map(node,node->train_info.selected_antenna);
                node->train_info.antenna_change_ind |= SMART_ANT_TX_CONFIG_REQUIRED;
                retval |= SMART_ANT_TX_CONFIG_REQUIRED;
            } else {
                sa_params->default_tx_antenna = params->param_value;
                if (radio->nodes_connected == radio->node_count_per_antenna[sa_params->default_tx_antenna]) {
                    sa_params->antenna_change_ind &= ~SMART_ANT_TX_CONFIG_REQUIRED;
                    SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"global tx antenna change completed antenna %d\n", sa_params->default_tx_antenna);
                } else {
                    sa_params->antenna_change_ind |= SMART_ANT_TX_CONFIG_REQUIRED;
                }
            }
            break;
        case SMART_ANT_PARAM_TRAIN_START:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                node->train_info.training_start = params->param_value;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_ENABLE:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                node->train_info.train_enable = params->param_value;

                if (node->train_info.train_enable & (SA_INIT_TRAIN_EN | SA_PERF_TRAIN_EN | SA_PERIOD_TRAIN_EN)) {
                    node->train_info.antenna_change_ind &= ~SMART_ANT_TX_CONFIG_REQUIRED;
                }
            } else {
                sa_params->train_enable = params->param_value;

                if (sa_params->train_enable & (SA_INIT_TRAIN_EN | SA_PERF_TRAIN_EN | SA_PERIOD_TRAIN_EN)) {
                    sa_params->antenna_change_ind &= ~SMART_ANT_TX_CONFIG_REQUIRED;
                }
            }
            break;
        case SMART_ANT_PARAM_DBG_LEVEL:
            radio->sa_dbg_level = params->param_value;
            break;
        case SMART_ANT_PARAM_PRETRAIN_PKTS:
            sa_params->num_pretrain_packets = params->param_value;
            break;
        case SMART_ANT_PARAM_OTHER_BW_PKTS_TH:
            sa_params->num_other_bw_pkts_th = params->param_value;
            break;
        case SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL:
            sa_params->goodput_ignore_interval = params->param_value;
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW20:
            sa_params->num_min_pkt_threshod[SA_BW_20] = params->param_value;
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW40:
            sa_params->num_min_pkt_threshod[SA_BW_40] = params->param_value;
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW80:
            sa_params->num_min_pkt_threshod[SA_BW_80] = params->param_value;
            break;
        case SMART_ANT_PARAM_DEBUG_INFO:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                node->train_info.last_training_time = 0;
                node->train_info.prd_trigger_count = 0;
                node->train_info.perf_trigger_count = 0;
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"Last training time :%d ,Periodic triggers: %d ,performence triggers: %d\n",
                            node->train_info.last_training_time,node->train_info.prd_trigger_count,node->train_info.perf_trigger_count);
            }
	        break;
        case SMART_ANT_PARAM_MAX_TRAIN_PPDU:
	        sa_params->max_train_ppdu = params->param_value;
	        break;
        case SMART_ANT_PARAM_PERF_HYSTERESIS:
	        sa_params->hysteresis =  params->param_value;
	        break;
        case SMART_ANT_PARAM_TX_FEEDBACK_CONFIG:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                node->train_info.tx_feedback_config = params->param_value;
                retval |= SMART_ANT_TX_FEEDBACK_CONFIG_REQUIRED;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_BCN_ANTENNA:
            if (sa_enable_test_mode) {
                radio->bcn_antenna = params->param_value;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_RATE_TESTMODE:
            if (sa_enable_test_mode) {
                g_train_params.ratecode = params->param_value;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE:
            if (sa_enable_test_mode) {
                g_train_params.antenna = params->param_value;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_PACKETS_TETSMODE:
            if (sa_enable_test_mode) {
                g_train_params.numpkts = params->param_value;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_START_TETSMODE:
            if (sa_enable_test_mode) {
                retval |= SMART_ANT_TRAINING_REQUIRED;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        default:
            DisplayHelp();
            retval = SA_STATUS_FAILURE;
            break;
    }

    return retval;
}

/**
 *   __sa_get_param: api function to get configuration parameter
 *   @params: pointer to structure containg configuration values
 *
 *   returns success or failure
 */
int  __sa_get_param (struct sa_config_params *params)
{
    int retval = SA_STATUS_SUCCESS;
    struct sa_node_ccp *node = NULL;
    struct smartantenna_params *sa_params = NULL;
    struct radio_config *radio = &g_sa.radio[params->radio_id];
    struct sa_debug_train_cmd *dtrain_cmd;
    uint32_t i;

    OS_ASSERT(params->radio_id < SA_MAX_RADIOS);

    if (radio->init_done) {
        sa_params = &radio->sa_params;
    } else {
        return SA_STATUS_FAILURE;
    }

    switch (params->param_id)
    {
        case SMART_ANT_PARAM_HELP:
             DisplayHelp();
             break;
        case SMART_ANT_PARAM_TRAIN_MODE:
            params->param_value = sa_params->training_mode;
            break;
        case SMART_ANT_PARAM_TRAIN_PER_THRESHOLD:
            params->param_value = (sa_params->lower_bound);
            params->param_value |= (sa_params->upper_bound << SA_UB_POSITION);
            params->param_value |= (sa_params->per_diff_threshold << SA_PER_THRESHOLD_POSITION);
            params->param_value |= (sa_params->config << SA_CONFIG_POSITION);
            break;
        case SMART_ANT_PARAM_PKT_LEN:
            params->param_value = sa_params->packetlen;
            break;
        case SMART_ANT_PARAM_NUM_PKTS:
            params->param_value = sa_params->n_packets_to_train;
            break;
        case SMART_ANT_PARAM_PERFTRAIN_INTERVAL:
            params->param_value = sa_params->perftrain_interval;
            break;
        case SMART_ANT_PARAM_RETRAIN_INTERVAL:
            params->param_value = sa_params->retrain_interval;
            break;
        case SMART_ANT_PARAM_PERFTRAIN_THRESHOLD:
            params->param_value = sa_params->max_throughput_change;
            break;
        case SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD:
            params->param_value = sa_params->min_goodput_threshold;
            break;
        case SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL:
            params->param_value = sa_params->goodput_avg_interval;
            break;
        case SMART_ANT_PARAM_DEFAULT_ANTENNA:
            params->param_value = radio->default_antenna;
            break;
        case SMART_ANT_PARAM_DEFAULT_TX_ANTENNA:
            params->param_value = sa_params->default_tx_antenna;
            break;
        case SMART_ANT_PARAM_TX_ANTENNA:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                params->param_value = node->train_info.selected_antenna;
            } else {
                return SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_ENABLE:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                params->param_value = node->train_info.train_enable;
            } else {
                params->param_value = sa_params->train_enable;
            }
            break;
        case SMART_ANT_PARAM_DBG_LEVEL:
            params->param_value = radio->sa_dbg_level;
            break;
        case SMART_ANT_PARAM_PRETRAIN_PKTS:
            params->param_value = sa_params->num_pretrain_packets;
            break;
        case SMART_ANT_PARAM_OTHER_BW_PKTS_TH:
            params->param_value = sa_params->num_other_bw_pkts_th;
            break;
        case SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL:
            params->param_value = sa_params->goodput_ignore_interval;
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW20:
            params->param_value = sa_params->num_min_pkt_threshod[SA_BW_20];
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW40:
            params->param_value = sa_params->num_min_pkt_threshod[SA_BW_40];
            break;
        case SMART_ANT_PARAM_MIN_PKT_TH_BW80:
            params->param_value = sa_params->num_min_pkt_threshod[SA_BW_80];
            break;
        case SMART_ANT_PARAM_DEBUG_INFO:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                params->param_value = node->train_info.train_enable;
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"Last training time :%d ,Periodic triggers: %d ,performence triggers: %d\n",
                            node->train_info.last_training_time,node->train_info.prd_trigger_count,node->train_info.perf_trigger_count);

                SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id," Antenna:    Rate:    nFrame: nBad:   nppdu:  bw:  lrate: ldir:  ant_sel:   PER: \n");
                for (i = 0;i < node->debug_data.cmd_count; i++) {
                    dtrain_cmd = &node->debug_data.train_cmd[i];
                    SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id,"   %02d      0x%02x     %03d     %03d     %03d    %d   0x%02x     %d       0x%2x      %3d \n",
                    dtrain_cmd->antenna, dtrain_cmd->ratecode, dtrain_cmd->nframes, dtrain_cmd->nbad,dtrain_cmd->nppdu,dtrain_cmd->bw,dtrain_cmd->last_rate,dtrain_cmd->last_train_dir,dtrain_cmd->ant_sel,dtrain_cmd->per);
                }
                for (i = 0;i < SA_MAX_BW; i++) {
                    SA_DEBUG_PRINT(SA_DEBUG_LVL1,node->node_info.radio_id, "BW %d Avg Goodput : %d : Ins Goodput %d nppdu %d hysteresis: %d\n",i,
                    node->train_info.perf_info[i].avg_goodput, node->rate_stats.ins_goodput[i], node->rate_stats.nppdu_bw[i], node->train_info.perf_info[i].hysteresis);
                }
            } else {
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"%s: nodes connected %d\n",__func__,radio->nodes_connected);
                for (i = 0;i < sa_params->num_tx_antennas; i++) {
                    SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"    ant %d , nodes %d\n",i,radio->node_count_per_antenna[i]);
                }
                SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"antenna change ind %d \n",sa_params->antenna_change_ind);
            }
            break;
        case SMART_ANT_PARAM_MAX_TRAIN_PPDU:
	        params->param_value = sa_params->max_train_ppdu;
	        break;
        case SMART_ANT_PARAM_PERF_HYSTERESIS:
	        params->param_value = sa_params->hysteresis;
	        break;
        case SMART_ANT_PARAM_TX_FEEDBACK_CONFIG:
            if (params->param_type) {
                node = (struct sa_node_ccp *) params->ccp;
                params->param_value = node->train_info.tx_feedback_config;
            } else {
            	retval = SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_BCN_ANTENNA:
            if (sa_enable_test_mode) {
                params->param_value = radio->bcn_antenna;
            } else {
                retval = SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_RATE_TESTMODE:
            if (sa_enable_test_mode) {
                params->param_value = g_train_params.ratecode;
            } else {
                retval = SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE:
            if (sa_enable_test_mode) {
                params->param_value = g_train_params.antenna;
            } else {
                retval = SA_STATUS_FAILURE;
            }
            break;
        case SMART_ANT_PARAM_TRAIN_PACKETS_TETSMODE:
            if (sa_enable_test_mode) {
                params->param_value = g_train_params.numpkts;
            } else {
                retval = SA_STATUS_FAILURE;
            }
            break;
        default:
            retval = SA_STATUS_FAILURE;
            break;
    }
    SA_DEBUG_PRINT(SA_DEBUG_LVL1,params->radio_id,"%s: Type:%d Id: %d value: %d radioId: %d\n", __func__, params->param_type
                                                                                   , params->param_id
                                                                                   , params->param_value
                                                                                   , params->radio_id
                                                                                   );
    return retval;
}

/**
 *   sa_get_antenna_combinations: function to find number of tx antenna combinations
 *   @chains: man number of tx chains
 *
 *   returns number of tx antenna combinations
 */
inline uint8_t sa_get_antenna_combinations(uint8_t chains)
{
    int i=0;
    uint8_t result = 1;
    for(i = 0; i < chains ; i++) {
        result *= (SA_ANTENNAS_PER_CHAIN);
    }
    return result;
}

/**
 *   sa_init_radio_params: function to initialize default radio params
 *   @sa_params: pointer of sa params
 *   @maxchains: man number of tx chains
 *
 *   returns number of tx antenna combinations
 */
int sa_init_radio_params(struct smartantenna_params *sa_params, struct sa_config *sa_config)
{
    uint8_t maxchains = sa_config->nss;
    /* Both radios uses same default config params */
    /* init the default values */
    sa_params->lower_bound = SA_PER_LOWER_BOUND;
    sa_params->upper_bound = SA_PER_UPPER_BOUND ;
    sa_params->per_diff_threshold = SA_PERDIFF_THRESHOLD;
    sa_params->config = 0;// (SA_CONFIG_INTENSETRAIN | SA_CONFIG_EXTRATRAIN);
    sa_params->training_mode = SA_TRAINING_MODE_EXISTING;
    sa_params->packetlen = SA_DEFAULT_PKT_LEN; /* TODO: Traffic generation move to UMAC ??? */
    sa_params->n_packets_to_train = 0;  /*0- Use configuration from Algo. i.e LUT else use this value */
    sa_params->max_throughput_change = SA_DEFAULT_TP_THRESHOLD;
    sa_params->retrain_interval = SA_DEFAULT_RETRAIN_INTERVAL * SA_NUM_MILLISECONDS_IN_MINUTE;
    sa_params->perftrain_interval = SA_PERF_TRAIN_INTERVEL;
    sa_params->num_pkt_threshold = SA_DEFAULT_MIN_TP;
    sa_params->hysteresis = SA_DEFALUT_HYSTERISYS;
    sa_params->min_goodput_threshold = MIN_GOODPUT_THRESHOLD;
    sa_params->goodput_avg_interval = SA_GPUT_AVERAGE_INTERVAL;
    sa_params->num_pretrain_packets = MAX_PRETRAIN_PACKETS;
    sa_params->num_other_bw_pkts_th = SA_N_BW_PPDU_THRESHOLD;
    sa_params->goodput_ignore_interval = SA_GPUT_IGNORE_INTERVAL;
    if (sa_enable_test_mode) {
        sa_params->train_enable = 0;
    } else {
        sa_params->train_enable = SA_INIT_TRAIN_EN | SA_PERIOD_TRAIN_EN | SA_PERF_TRAIN_EN | SA_RX_TRAIN_EN;
    }
    sa_params->num_min_pkt_threshod[SA_BW_20] = SA_NUM_PKT_THRESHOLD_20;
    sa_params->num_min_pkt_threshod[SA_BW_40] = SA_NUM_PKT_THRESHOLD_40;
    sa_params->num_min_pkt_threshod[SA_BW_80] = SA_NUM_PKT_THRESHOLD_80;
    sa_params->max_train_ppdu = SA_NUM_MAX_PPDU;
    sa_params->default_tx_antenna = sa_default_antenna;
    sa_params->num_tx_antennas = sa_get_antenna_combinations(maxchains);
    sa_params->num_recv_chains = maxchains;
    if (maxchains > SA_MAX_RECV_CHAINS) {
        sa_params->num_recv_chains = SA_MAX_RECV_CHAINS;
        SA_DEBUG_PRINT(SA_DEBUG_LVL1,(SA_GET_RADIO_ID(sa_config->radio_id)),"%s ERROR fatal,unsupported receive chains %d \n",__func__, maxchains);
    }
    return SA_PARALLEL_MODE;
}

/**
 *   __sa_init: api function to initilize sa algo
 *   @sa_config: pointer of initial sa config
 *   @new_init: indicates new initilization or reinint
 *
 *   returns success or failure
 */
int __sa_init (struct sa_config *sa_config, int new_init)
{
    struct radio_config *radio = &g_sa.radio[SA_GET_RADIO_ID(sa_config->radio_id)];
    int status = 0;

    if (new_init) {
        OS_MEMZERO(radio, sizeof(struct radio_config));
        status = sa_init_radio_params(&radio->sa_params, sa_config);
        radio->init_done = 1;
        radio->sa_dbg_level = SA_DEBUG_LVL1;
        radio->radiotype = (sa_config->radio_id) & SA_NIBBLE_MASK;
        radio->default_antenna = sa_default_antenna;
    }

    SA_DEBUG_PRINT(SA_DEBUG_LVL1,(SA_GET_RADIO_ID(sa_config->radio_id)),"%s %s: RadioId: %d max_fallback_rates: %d channel: %d bss mode: %d nss: %d chainmask: 0x%02x radiotype %d \n",
            __func__, (new_init ? "New Init" : "Re Init"), SA_GET_RADIO_ID(sa_config->radio_id),
            sa_config->max_fallback_rates, sa_config->channel_num, sa_config->bss_mode,
            sa_config->nss, sa_config->txrx_chainmask,radio->radiotype);

    OS_MEMCPY(&radio->init_config, sa_config, sizeof(struct sa_config));
    if (sa_enable_test_mode) {
        radio->bcn_antenna = sa_default_antenna;
        sa_config->txrx_feedback = SMART_ANT_TX_FEEDBACK_MASK | SMART_ANT_RX_FEEDBACK_MASK;
    } else {
        sa_config->txrx_feedback = SMART_ANT_TX_FEEDBACK_MASK;
    }
    return SA_PARALLEL_MODE;
}

/**
 *   __sa_deinit: api function to deinitilize sa algo
 *   @radio_id: radio id number
 *
 *   returns success or failure
 */
int __sa_deinit(enum radioId radio_id)
{
    /* deallocate etc ..*/
    struct radio_config *radio = &g_sa.radio[radio_id];
    radio->init_done = 0;
    SA_DEBUG_PRINT(SA_DEBUG_LVL1,radio_id," %s RadioId: %d\n", __func__, radio_id);
    return SA_STATUS_SUCCESS;
}


struct smartantenna_ops sa_ops = {
    sa_init:__sa_init,
    sa_deinit:__sa_deinit,
    sa_node_connect:__sa_node_connect,
    sa_node_disconnect:__sa_node_disconnect,
    sa_update_txfeedback:__sa_update_txfeedback,
    sa_update_rxfeedback:__sa_update_rxfeedback,
    sa_get_txantenna:__sa_get_txantenna,
    sa_get_txdefaultantenna:__sa_get_txdefaultantenna,
    sa_get_rxantenna:__sa_get_rxantenna,
    sa_get_traininginfo:__sa_get_training_info,
    sa_get_bcn_txantenna:__sa_get_bcn_txantenna,
    sa_set_param:__sa_set_param,
    sa_get_param:__sa_get_param,
    sa_get_node_config:__sa_get_node_config
};


extern int register_smart_ant_ops(struct smartantenna_ops *sa_ops);
extern int deregister_smart_ant_ops(char *dev_name);

static int __sa_init_module(void)
{
    register_smart_ant_ops(&sa_ops);
    return 0;
}

static void __sa_exit_module(void)
{
    struct radio_config *radio = &g_sa.radio[0];
    if (radio->init_done) {
        deregister_smart_ant_ops("wifi0");
    }

    radio = &g_sa.radio[1];
    if (radio->init_done) {
        deregister_smart_ant_ops("wifi1");
    }

}

module_init(__sa_init_module);
module_exit(__sa_exit_module);
