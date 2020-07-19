/*
* Copyright (c) 2013 Qualcomm Atheros, Inc..
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#ifndef __SMART_ANT_API_H__
#define __SMART_ANT_API_H__

#define SMART_ANTENNA_MAX_RATE_SERIES 4
#define SMART_ANT_MAX_SA_CHAINS 4
#define MAX_EVM_SIZE 16 /* Max 16 pilot EVMs for 11 ac */
#define MAX_RETRIES_INDEX 8 /* For Direct attach failed tries for series0 at index 0
                                                                  series1 at index 1 etc
                               For Offload(dyn bw)  failed tries for index0 - s0_bw20,
                                     index1 - s0_bw40  index4 - s1_bw20 ... index7: s1_bw160.*/

#define SMART_ANT_RX_CONFIG_REQUIRED 		0x01
#define SMART_ANT_TRAINING_REQUIRED  		0x02
#define SMART_ANT_TX_CONFIG_REQUIRED 		0x04
#define SMART_ANT_TX_FEEDBACK_CONFIG_REQUIRED   0x08
#define SA_TX_FEEDBACK_CONFIG			        0x01

#define SMART_ANT_STATUS_SUCCESS 0
#define SMART_ANT_ANTENNA_MASK  0x00FFFFFF

#define DYNAMIC_BW_SUPPORTED 0x01

#define MAX_VALID_RATES 52    /* 40 MCS rates + 12 OFDM and CCK rates */
#define MAX_CCK_OFDM_RATES 12 /* Maximum CCK, OFDM rates supported */
#define MAX_MCS_RATES 40 /* Maximum MCS rates supported; 4 rates in each dword */
#define MAX_RATE_COUNTERS 4

#define SMART_ANT_BSS_MODE_AP         0x00
#define SMART_ANT_BSS_MODE_STA        0x01

#define SMART_ANT_TX_FEEDBACK_MASK  0x10 /* Tx feedback is required */
#define SMART_ANT_RX_FEEDBACK_MASK  0x20 /* Rx feedback is required */

#define SA_MAX_COMB_FB 2

enum radioId {
    wifi0 = 0,
    wifi1
};

struct sa_config {
    /* This is required in SmartAntenna at training phase which need to return
     * (antenna array and rate array) to the caller */
    uint8_t max_fallback_rates;  /* 0 - single rate , 1 - 2 rates etc ... */
    enum radioId radio_id;
    uint8_t channel_num;
    uint8_t bss_mode; /* 0 - AP mode 1-STA mode */
    uint8_t nss; /* number of spatial streams */
    uint8_t txrx_chainmask; /* Rx and Tx chain mask (0x23 - Tx chain mask: 3 ,Rx chain mask: 2)*/
    u_int32_t txrx_feedback; /* Rx and Tx feedback subscription status */
};

/* TODO: ratecode_160 needs to add for future chips */
struct sa_rate_cap {
    uint8_t ratecode_legacy[MAX_CCK_OFDM_RATES]; /* Rate code array for CCK OFDM */
    uint8_t ratecode_20[MAX_MCS_RATES];          /* Rate code array for 20MHz BW */
    uint8_t ratecode_40[MAX_MCS_RATES];          /* Rate code array for 40MHz BW */
    uint8_t ratecode_80[MAX_MCS_RATES];          /* Rate code array for 80MHz BW */
    uint8_t ratecount[MAX_RATE_COUNTERS];        /* Max Rate count for each mode */
};

struct sa_node_info {
    uint8_t mac_addr[6];
    enum radioId radio_id;
    uint8_t max_ch_bw;	/* 0-20 Mhz, 1-40 Mhz , 2-80 Mhz, 3-160 Mhz */
    uint8_t chainmask;  /* Rx and Tx chain mask (0x23 - Tx chain mask: 3 ,Rx chain mask: 2)*/
    uint8_t opmode;  	/* 0-Legacy, 1-11n Mode, 2-11ac mode */
    uint8_t channel_num; /* operating channel number */
    u_int32_t ni_cap;
    struct sa_rate_cap rate_cap;
};

struct sa_comb_fb {
    uint8_t nbad;
    uint8_t npkts;
    uint8_t bw;
    uint8_t rate;
};

struct  sa_tx_feedback {
    uint16_t nPackets; /* number packets corresponding this TX feed back */
    uint16_t nBad;     /* number of bad/failed packets */
    uint16_t nshort_retries[MAX_RETRIES_INDEX]; /* For Direct attach failed tries for series0 at index 0 i
                                                               series1 at index 1 etc
                                                   For Offload(dyn bw)  failed tries for index0 - s0_bw20,
                                                   index1 - s0_bw40  index4 - s1_bw20 ... index7: s1_bw160 */
    uint16_t nlong_retries[MAX_RETRIES_INDEX];
    uint32_t  tx_antenna[SMART_ANTENNA_MAX_RATE_SERIES]; /* antenna array used for transmission */
    uint32_t  rssi[SMART_ANT_MAX_SA_CHAINS];  /* rssi of ACK/ Block ACK */
    uint32_t  rate_mcs[SMART_ANTENNA_MAX_RATE_SERIES]; /* rate series used for transmission */
    uint8_t rate_index;  /* index of successful transmited rate */
    uint8_t is_trainpkt; /* Train packet indication */
    uint32_t ratemaxphy; /* max phy rate for all bandwidth */
    uint32_t goodput; /* goodput*/
    uint8_t num_comb_feedback; /* number of distinct combined feedback */
    struct sa_comb_fb comb_fb[SA_MAX_COMB_FB];
};

struct  sa_rx_feedback {
    uint32_t rx_rate_mcs;  /* received rate mcs */
    uint32_t rx_antenna;  /* recevied antenna */
    uint32_t  rx_evm[MAX_EVM_SIZE];  /* EVM for stream 0 , 1, 2, 3 */
    uint32_t  rx_rssi[SMART_ANT_MAX_SA_CHAINS]; /* rssi of received packet */
    uint16_t  npackets; /* Total number of succesfully received packets of aggregate/non-aggr  */
};

struct  sa_config_params {
    uint8_t  radio_id;
    uint32_t param_type;  /* 0 - radio config param; 1 - node config param */
    uint32_t param_id;   /* custom configuared parameter id */
    uint32_t param_value;   /* custom configuared parameter id */
    void *ccp; /* ccp for node specific configuration, This will be NULL for radio config params */
};

struct smartantenna_ops {
    int  (*sa_init) (struct sa_config *sa_config, int new_init);
    int  (*sa_deinit) (enum radioId radio_id);
    int  (*sa_node_connect) (void **ccp, struct  sa_node_info *node_info);
    int  (*sa_node_disconnect) (void *ccp);
    int  (*sa_update_txfeedback) (void  *ccp,  struct sa_tx_feedback *feedback, uint8_t *status);
    int  (*sa_update_rxfeedback) (void  *ccp,  struct sa_rx_feedback *feedback, uint8_t *status);
    int  (*sa_get_txantenna) (void  *ccp, uint32_t *antenna_array);
    int  (*sa_get_txdefaultantenna) ( enum radioId radio_id, uint32_t *antenna);
    int  (*sa_get_rxantenna) (enum radioId radio_id, uint32_t *rx_antenna);
    int  (*sa_get_traininginfo) (void *ccp , uint32_t *rate_array, uint32_t *antenna_array, uint32_t * numpkts);
    int  (*sa_get_bcn_txantenna) (enum radioId radio_id, uint32_t *bcn_txantenna);
    int  (*sa_set_param) (struct sa_config_params *params);
    int  (*sa_get_param) (struct sa_config_params *params);
    int  (*sa_get_node_config) (void  *ccp, uint32_t id, uint32_t *config);
};

#endif
