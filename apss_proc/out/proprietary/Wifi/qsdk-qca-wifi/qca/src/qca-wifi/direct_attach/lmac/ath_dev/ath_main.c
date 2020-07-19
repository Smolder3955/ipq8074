/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 */

/* \file ath_main.c
 * \brief Main Rx/Tx code
 *
 *  This file contains the main implementation of the ATH layer.  Most member
 *  functions of the ATH layer are defined here.
 *
 */
#if ATH_GEN_RANDOMNESS
#include <linux/kthread.h>
#endif
#include "wlan_opts.h"
#include "ath_internal.h"
#include "osdep.h"
#include "ratectrl.h"
#include "ath_ald.h"
#if QCA_AIRTIME_FAIRNESS
#include "ath_airtime_fairness.h"
#endif

#include "ath_lmac_state_event.h"

#ifdef ATH_TX99_DIAG
#include "ath_tx99.h"
#endif

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_api.h>
#include "scan_sm.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#if WLAN_SPECTRAL_ENABLE
#include "spectral.h"
int ath_dev_get_ctldutycycle(ath_dev_t dev);
int ath_dev_get_extdutycycle(ath_dev_t dev);
u_int32_t ath_init_spectral_ops(struct ath_spectral_ops* p_sops);
#endif

static int ath_sc_chan_busy(void *arg);

#if ATH_RESET_SERIAL
int
_ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *head);
#endif

#if ATH_SUPPORT_RAW_ADC_CAPTURE
#include "spectral_raw_adc_ioctl.h"
#endif

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#ifdef ATH_SUPPORT_TxBF
#include "ath_txbf.h"
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
#include <flowmac_api.h>
#endif

#ifdef __linux__
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <linux/moduleparam.h>
static char *macaddr_override = NULL;
module_param(macaddr_override, charp, S_IRUSR);
MODULE_PARM_DESC(macaddr_override, "Local MAC address override");

unsigned int paprd_enable = 1; /*paprd is enable by default*/
module_param(paprd_enable, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(paprd_enable,"paprd_enable");

#endif
#endif /* __linux__ */

#if ATH_SUPPORT_VOW_DCS
#define INTR_DETECTION_CNT 6      /* Interference detection cnt for channel change */
#define COCH_INTR_THRESH 30
#define TXERR_THRESH 30
#define PHYERR_RATE_THRESH 300    /* Phyerr per sec */
#define RADARERR_THRESH    1000   /* radar error per sec */
#define INTR_DETECTION_WINDOW 10  /* interference observation window
                                   * before channel change */
#define PHYERR_PENALTY_TIME 500   /* micro sec */
#endif
#define RXTID_SEND_DELBA 8        /* secs timeout used to send DELBA for all Rx tids which are stuck */

#if ATH_GEN_RANDOMNESS
static int ath_rng_kthread(void *data);
void ath_rng_start(struct ath_softc *sc)
{
   if (sc->rng_task)
       return;

   sc->rng_task = kthread_run(ath_rng_kthread, sc, "ath-hwrng");
   if (IS_ERR(sc->rng_task))
       sc->rng_task = NULL;
}
OS_EXPORT_SYMBOL(ath_rng_start);
void ath_rng_stop(struct ath_softc *sc)
{
   if (sc->rng_task)
       kthread_stop(sc->rng_task);
}
OS_EXPORT_SYMBOL(ath_rng_stop);
#endif
static inline void ath_dfs_clear_cac(struct ath_softc *sc)
{
#ifdef ATH_SUPPORT_DFS
    if (sc->sc_dfs_wait ) {
        printk("%s: Exit DFS_WAIT state %d\n", __func__, sc->sc_dfs_wait);
        sc->sc_dfs_wait = 0;
        /*
        ** CAC is over, use normal value of PRSSI for pre-Peregrine chips
        */
        ath_hal_dfs_cac_war (sc->sc_ah, 0);
    }
#endif
}

/* Global configuration overrides */
static    const int countrycode = -1;
static    const int xchanmode = -1;
static    const int ath_outdoor = AH_FALSE;        /* enable outdoor use */
static    const int ath_indoor  = AH_FALSE;        /* enable indoor use  */
/*
 * Chainmask to numchains mapping.
 */
#if !NO_HAL
static    u_int8_t ath_numchains[8] = {
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */,
    2 /* 101 */,
    2 /* 110 */,
    3 /* 111 */
};
#endif
#if ATH_SUPPORT_MCI
#include "ath_mci.h"
#endif

#if ATH_DEBUG
void dprintf(struct ath_softc *sc, unsigned category, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    asf_vprint_category(&sc->sc_print, category, fmt, args);
    va_end(args);
}

/*
 * ath_print - ATH custom print function
 *
 * This is the custom print function for if_ath/ath layer. It is the callback
 * funtion registered in asf_print control object (sc_print) and will be
 * used when dprintf is called (via asf_vprint_category)
 */
static void ath_print(void *ctxt, const char *fmt, va_list args)
{
#ifndef REMOVE_PKT_LOG
    struct ath_softc *sc = (struct ath_softc *)ctxt;
    char buf[256];
    va_list args_copy;

    /* On very new machine, using va_list args second time without release it first
     could cause risk condition. Then KP happens. So by copy it to another va_list,
     the problem get solved .Refer to Bug EV #87853 */
    va_copy(args_copy, args);
    vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);
    ath_log_text(ctxt, buf, sc->sc_print_flag);
#endif
    qdf_vprint(fmt, args);
}

/* ATH global category bit -> name translation */
static const struct asf_print_bit_spec ath_print_categories[] = {
    {ATH_DEBUG_XMIT, "xmit"},
    {ATH_DEBUG_XMIT_DESC, "xmit descriptor"},
    {ATH_DEBUG_RECV, "recv"},
    {ATH_DEBUG_RECV_DESC, "recv descriptor"},
    {ATH_DEBUG_RATE, "rate"},
    {ATH_DEBUG_RESET, "reset"},
    {ATH_DEBUG_BEACON, "beacon"},
    {ATH_DEBUG_WATCHDOG, "watchdog"},
    {ATH_DEBUG_SCAN, "scan"},
    {ATH_DEBUG_HTC_WMI, "HTC wmi"},
    {ATH_DEBUG_INTR, "intr"},
    {ATH_DEBUG_TX_PROC, "tx intr"},
    {ATH_DEBUG_RX_PROC, "rx intr"},
    {ATH_DEBUG_BEACON_PROC, "beacon proc"},
    {ATH_DEBUG_CALIBRATE, "calibrate"},
    {ATH_DEBUG_KEYCACHE, "keycache"},
    {ATH_DEBUG_STATE, "state"},
    {ATH_DEBUG_NODE, "node"},
    {ATH_DEBUG_LED, "LED"},
    {ATH_DEBUG_UAPSD, "UAPSD"},
    {ATH_DEBUG_DOTH, "DOTH"},
    {ATH_DEBUG_CWM, "CWM"},
    {ATH_DEBUG_PPM, "PPM"},
    {ATH_DEBUG_PWR_SAVE, "powersave"},
    {ATH_DEBUG_SWR, "SWretry"},
    {ATH_DEBUG_AGGR_MEM, "aggr memory"},
    {ATH_DEBUG_BTCOEX, "BT coex"},
    {ATH_DEBUG_FATAL, "fatal"}
};
#endif

const u_int8_t ath_bcast_mac[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


#ifdef ATH_CHAINMASK_SELECT
u_int32_t   ath_chainmask_sel_up_rssi_thres = ATH_CHAINMASK_SEL_UP_RSSI_THRES;
u_int32_t   ath_chainmask_sel_down_rssi_thres = ATH_CHAINMASK_SEL_DOWN_RSSI_THRES;
u_int32_t   ath_chainmask_sel_period = ATH_CHAINMASK_SEL_TIMEOUT;

static void ath_chainmask_sel_timerstart(struct ath_chainmask_sel *cm);
static void ath_chainmask_sel_timerstop(struct ath_chainmask_sel *cm);
static void ath_chainmask_sel_init(struct ath_softc *sc, struct ath_node *an);
#endif

#ifdef ATH_SWRETRY
extern void ath_tx_flush_node_sxmitq(struct ath_softc *sc, struct ath_node *an);
#endif

#if ATH_C3_WAR
extern int  ath_c3_war_handler(void *arg);
#endif

#if ATH_RX_LOOPLIMIT_TIMER
extern int ath_rx_looplimit_handler(void *arg);
#endif

static void ath_printreg(ath_dev_t dev, u_int32_t printctrl);

static u_int32_t ath_getmfpsupport(ath_dev_t dev);
static void ath_setmfpQos(ath_dev_t dev, u_int32_t dot11w);

static u_int64_t ath_get_tx_hw_retries(ath_dev_t dev);
static u_int64_t ath_get_tx_hw_success(ath_dev_t dev);

#if UMAC_SUPPORT_WNM
void ath_wnm_beacon_config(ath_dev_t dev, int reason, int if_id);
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
u_int8_t ath_get_pwrsaveq_len(ath_node_t node, u_int8_t frame_type);
#endif
#if ATH_SUPPORT_TIDSTUCK_WAR
static void ath_clear_rxtid(ath_dev_t, ath_node_t);
#endif
static void ath_node_pspoll(ath_dev_t ,ath_node_t, bool);

#ifdef ATH_TX99_DIAG
extern u_int8_t tx99_ioctl(ath_dev_t dev, struct ath_softc *sc, int cmd, void *addr);
u_int8_t ath_tx99_ops(ath_dev_t dev, int cmd, void *tx99_wcmd);
#endif
u_int8_t ath_set_ctl_pwr(ath_dev_t dev, u_int8_t *ctl_pwr_array,
                            int rd, u_int8_t chainmask);
bool ath_ant_swcom_sel(ath_dev_t dev, u_int8_t ops, u_int32_t *tbl1, u_int32_t *tbl2);

static int ath_get_vap_stats(ath_dev_t dev, int if_id, struct ath_vap_dev_stats *vap_stats);

/*
** Internal Prototypes
*/

#if WAR_DELETE_VAP
static int ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons, void **ret_avp);
static int  ath_vap_detach(ath_dev_t dev, int if_id, void *athvap);
#else
static int ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons);
static int ath_vap_detach(ath_dev_t dev, int if_id);
#endif
static int ath_vap_config(ath_dev_t dev, int if_id, struct ath_vap_config *if_config);

static ath_node_t ath_node_attach(ath_dev_t dev, int if_id, ieee80211_node_t ni, bool tmpnode, void *peer);
static void ath_node_detach(ath_dev_t dev, ath_node_t an);
static int ath_node_cleanup(ath_dev_t dev, ath_node_t an);
static void ath_node_update_pwrsave(ath_dev_t dev, ath_node_t node, int pwrsave, int pause_resume);

/* Key cache interfaces */
static u_int16_t ath_key_alloc_2pair(ath_dev_t);
static u_int16_t ath_key_alloc_pair(ath_dev_t);
static u_int16_t ath_key_alloc_single(ath_dev_t);
static void ath_key_reset(ath_dev_t, u_int16_t keyix, int freeslot);
static int ath_keyset(ath_dev_t, u_int16_t keyix, HAL_KEYVAL *hk,
                      const u_int8_t mac[IEEE80211_ADDR_LEN]);
#if ATH_SUPPORT_KEYPLUMB_WAR
static void ath_key_save_halkey(ath_node_t node, HAL_KEYVAL *hk, u_int16_t keyix, const u_int8_t *macaddr);
int ath_checkandplumb_key(ath_dev_t, ath_node_t node, u_int16_t keyix);
#endif
static u_int ath_keycache_size(ath_dev_t);
#if ATH_SUPPORT_KEYPLUMB_WAR
void ath_keycache_print(ath_dev_t);
#endif

static u_int32_t ath_get_device_info(ath_dev_t dev, u_int32_t type);

/* regdomain interfaces */
static int ath_set_country(ath_dev_t, char *isoName, u_int16_t, enum ieee80211_clist_cmd);
static void ath_get_currentCountry(ath_dev_t, HAL_COUNTRY_ENTRY *ctry);
static int ath_set_regdomain(ath_dev_t, int regdomain, HAL_BOOL);
static int ath_get_regdomain(ath_dev_t dev);
static int ath_set_quiet(ath_dev_t, u_int32_t period, u_int32_t duration,
                         u_int32_t nextStart, HAL_QUIET_FLAG flag);
static int ath_get_dfsdomain(ath_dev_t dev);
static void ath_set_use_cac_prssi(ath_dev_t dev);
static u_int16_t ath_find_countrycode(ath_dev_t, char* isoName);
static u_int8_t ath_get_ctl_by_country(ath_dev_t, u_int8_t *country, HAL_BOOL is2G);
#if 0
static u_int16_t ath_dfs_isdfsregdomain(ath_dev_t);
static u_int16_t ath_dfs_usenol(ath_dev_t);
static u_int16_t ath_dfs_check_attach(ath_dev_t);
#endif
static int ath_set_tx_antenna_select(ath_dev_t, u_int32_t antenna_select);
static u_int32_t ath_get_current_tx_antenna(ath_dev_t);
static u_int32_t ath_get_default_antenna(ath_dev_t);
static int16_t ath_get_noisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags, int wait_time);
static void ath_get_chainnoisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags, int16_t *nfBuf);
static int ath_get_rx_nf_offset(ath_dev_t dev, u_int16_t freq, u_int chan_flags, int8_t *nf_pwr, int8_t *nf_cal);
static int ath_get_rx_signal_dbm(ath_dev_t dev, int8_t *signal_dbm);
#if ATH_SUPPORT_VOW_DCS
static void ath_disable_dcsim(ath_dev_t dev);
static void ath_enable_dcsim(ath_dev_t dev);
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
static int ath_get_min_agc_gain(ath_dev_t dev, u_int16_t freq, int32_t *gain_buf, int num_gain_buf);
#endif

static void ath_update_sm_pwrsave(ath_dev_t dev, ath_node_t node, ATH_SM_PWRSAV mode,
    int ratechg);
/* Rate control interfaces */
static u_int32_t ath_node_gettxrate(ath_node_t node);
void ath_node_set_fixedrate(ath_node_t node, u_int8_t fixed_ratecode);
static u_int32_t ath_node_getmaxphyrate(ath_dev_t dev, ath_node_t node);
static int athop_rate_newassoc(ath_dev_t dev, ath_node_t node, int isnew, unsigned int capflag,
                                  struct ieee80211_rateset *negotiated_rates,
                                  struct ieee80211_rateset *negotiated_htrates);
static void
athop_rate_newstate(ath_dev_t dev, int if_id, int up);
int ath_update_mib_macstats(ath_dev_t dev);
int ath_get_mib_macstats(ath_dev_t dev, struct ath_mib_mac_stats *pStats);
void ath_clear_mib_counters(ath_dev_t dev);
#ifdef DBG
static u_int32_t ath_register_read(ath_dev_t dev, u_int32_t offset);
static void ath_register_write(ath_dev_t dev, u_int32_t offset, u_int32_t value);
#endif

void ath_do_pwrworkaround(ath_dev_t dev, u_int16_t channel,  u_int32_t channel_flags, u_int16_t opmodeSta);
void ath_get_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *retval);
void ath_set_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value);
void ath_update_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value);
void ath_get_hwrevs(ath_dev_t dev, struct ATH_HW_REVISION *hwrev);
u_int32_t ath_rc_rate_maprix(ath_dev_t dev, u_int16_t curmode, int isratecode);
int ath_rc_rate_set_mcast_rate(ath_dev_t dev, u_int32_t req_rate);
void ath_set_diversity(ath_dev_t dev, u_int diversity);
void ath_set_rx_chainmask(ath_dev_t dev, u_int8_t mask);
void ath_set_tx_chainmask(ath_dev_t dev, u_int8_t mask);
void ath_set_rx_chainmasklegacy(ath_dev_t dev, u_int8_t mask);
void ath_set_tx_chainmasklegacy(ath_dev_t dev, u_int8_t mask);
void ath_set_tx_chainmaskopt(ath_dev_t dev, u_int8_t mask);
void ath_get_maxtxpower(ath_dev_t dev, u_int32_t *txpow);
void ath_read_from_eeprom(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread);
int ath_rate_newstate_set(ath_dev_t dev , int index, int up);
#if ATH_SUPPORT_WIFIPOS
static void ath_vap_reap_txqs(ath_dev_t dev,int if_id);
#endif
static void  ath_vap_pause_control(ath_dev_t dev,int if_id, bool pause );
static void  ath_node_pause_control(ath_dev_t dev,ath_node_t node, bool pause );
static void  ath_cdd_control(ath_dev_t dev, u_int32_t enabled);
static void  ath_set_rifs(ath_dev_t dev, bool enable);
int8_t ath_get_noderssi(ath_node_t node, int8_t chain, u_int8_t flag);
u_int32_t ath_get_noderate(ath_node_t node, u_int8_t type);
u_int32_t ath_get_last_txpower(ath_node_t node);
void ath_get_min_and_max_powers(ath_dev_t dev,
                                int8_t *max_tx_power,
                                int8_t *min_tx_power);
uint32_t ath_get_modeSelect(ath_dev_t dev);
uint32_t ath_get_chip_mode(ath_dev_t dev);
void     ath_get_freq_range(ath_dev_t dev,
                            uint32_t flags,
                            uint32_t *low_freq,
                            uint32_t *high_freq);
void ath_fill_hal_chans_from_regdb(ath_dev_t dev,
                                   uint16_t freq,
                                   int8_t maxregpower,
                                   int8_t maxpower,
                                   int8_t minpower,
                                   uint32_t flags,
                                   int index);

#if ATH_SUPPORT_CFEND
extern int ath_cfendq_config(struct ath_softc *sc);
#endif

static inline int ath_hw_tx_hang_check(struct ath_softc *sc);

#if ATH_SUPPORT_IBSS_DFS
static void ath_ibss_beacon_update_start(ath_dev_t dev);
static void ath_ibss_beacon_update_stop(ath_dev_t dev);
#endif /* ATH_SUPPORT_IBSS_DFS */
static u_int32_t ath_get_txbuf_free(ath_dev_t dev);
#if UNIFIED_SMARTANTENNA
static int ath_smart_ant_enable(ath_dev_t dev, int enable, int mode);
static void ath_smart_ant_set_tx_antenna(ath_node_t node, u_int32_t *antenna_array, u_int8_t num_rates);
static void ath_smart_ant_set_tx_defaultantenna(ath_dev_t dev, u_int32_t antenna);
static void ath_smart_ant_set_training_info(ath_node_t node, uint32_t *rate_array, uint32_t *antenna_array, uint32_t numpkts);
static void ath_smart_ant_prepare_rateset(ath_dev_t dev, ath_node_t node, void *prate_info);
#endif
static u_int8_t* ath_get_tpc_tables(ath_dev_t dev, u_int8_t *ratecount);
static void ath_set_node_tx_chainmask(ath_node_t node, u_int8_t chainmask);
#if ATH_SUPPORT_DFS
int ath_dfs_get_caps(ath_dev_t dev, struct wlan_dfs_caps *dfs_caps);
static int ath_detach_dfs(ath_dev_t dev);
static int ath_enable_dfs(ath_dev_t dev, int *is_fastclk, HAL_PHYERR_PARAM *pe, u_int32_t dfsdomain);
static int ath_disable_dfs(ath_dev_t dev);
static int ath_get_mib_cycle_counts_pct(ath_dev_t dev, u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt);
static int ath_get_radar_thresholds(ath_dev_t dev, HAL_PHYERR_PARAM *pe);
#endif /* ATH_SUPPORT_DFS */

static void ath_conf_rx_intr_mit(void* arg,u_int32_t enable);
static void ath_set_beacon_interval(ath_dev_t dev, u_int16_t intval);
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
static void  ath_txbf_loforceon_update(ath_dev_t dev,bool loforcestate);
#endif
#ifdef WLAN_SUPPORT_GREEN_AP
static QDF_STATUS ath_green_ap_ps_on_off(ath_dev_t dev, u_int32_t val,
				u_int8_t mac_id);
static QDF_STATUS ath_green_ap_get_capab(ath_dev_t dev);
static u_int16_t ath_green_ap_get_current_channel(ath_dev_t dev);
static u_int64_t ath_green_ap_get_current_channel_flags(ath_dev_t dev);
static QDF_STATUS ath_green_ap_reset_dev(ath_dev_t dev);
#endif

/******************************************************************************/
/*!
**  Wait for other threads that may access sc to finish
**
**  \param timeout      maximum wait time in milli-second.
**
**  \return -EIO if timeout
**  \return  0 on success
*/
int
ath_wait_sc_inuse_cnt(struct ath_softc *sc, u_int32_t timeout)
{
    u_int32_t        elapsed_time = 0;

    /* convert timeout to usec */
    timeout = timeout * 1000;

    /* Wait for any hardware access to complete */
#define WAIT_SC_INTERVAL 100
    while (atomic_read(&sc->sc_inuse_cnt)){
        OS_DELAY(WAIT_SC_INTERVAL);
        elapsed_time += WAIT_SC_INTERVAL;

        if (elapsed_time > timeout) {
           printk("%s: sc_inuse count(%d) stuck after waiting for %d msec. Investigate!!!\n",
                  __func__, atomic_read(&sc->sc_inuse_cnt), timeout);
           return -EIO;
        }
    }
#undef WAIT_SC_INTERVAL

    return 0;
}



/******************************************************************************/
/*!
**  \brief Expand time stamp to TSF
**
**  Extend stamp from rx descriptor to
**  a full 64-bit TSF using the current h/w TSF.
**  The bit-width of the timestamp field is fetched from the HAL capabilities.
**
**  \param sc Pointer to ATH object (this)
**  \param rstamp Time stamp value
**
**  \return 64 bit TSF (Timer Synchronization Function) value
*/
u_int64_t
ath_extend_tsf(struct ath_softc *sc, u_int32_t rstamp)
{
    u_int64_t tsf;
    u_int32_t desc_timestamp_bits;
    u_int32_t timestamp_mask = 0;
    u_int64_t timestamp_period = 0;

    /* Chips before Ar5416 contained 15 bits of timestamp in the Descriptor */
    desc_timestamp_bits = 15;

    ath_hal_getcapability(sc->sc_ah, HAL_CAP_RXDESC_TIMESTAMPBITS, 0, &desc_timestamp_bits);

    if (desc_timestamp_bits < 32) {
        timestamp_period = ((u_int64_t)1 << desc_timestamp_bits);
        timestamp_mask = timestamp_period - 1;
    }
    else {
        timestamp_period = (((u_int64_t) 1) << 32);
        timestamp_mask = 0xffffffff;
    }

    tsf = ath_hal_gettsf64(sc->sc_ah);

    /* Check to see if the TSF value has wrapped-around after the descriptor timestamp */
    if ((tsf & timestamp_mask) < (rstamp & timestamp_mask)) {
        tsf -= timestamp_period;
    }

    return ((tsf &~ (u_int64_t) timestamp_mask) | (u_int64_t) (rstamp & timestamp_mask));
}

/******************************************************************************/
/*!
**  \brief Set current operating mode
**
**  This function initializes and fills the rate table in the ATH object based
**  on the operating mode.  The blink rates are also set up here, although
**  they have been superceeded by the ath_led module.
**
**  \param sc Pointer to ATH object (this).
**  \param mode WIRELESS_MODE Enumerated value indicating the
**              wireless operating mode
**
**  \return N/A
*/

static void
ath_setcurmode(struct ath_softc *sc, WIRELESS_MODE mode)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    /* NB: on/off times from the Atheros NDIS driver, w/ permission */
    static const struct {
        u_int        rate;        /* tx/rx 802.11 rate */
        u_int16_t    timeOn;        /* LED on time (ms) */
        u_int16_t    timeOff;    /* LED off time (ms) */
    } blinkrates[] = {
        { 108,  40,  10 },
        {  96,  44,  11 },
        {  72,  50,  13 },
        {  48,  57,  14 },
        {  36,  67,  16 },
        {  24,  80,  20 },
        {  22, 100,  25 },
        {  18, 133,  34 },
        {  12, 160,  40 },
        {  10, 200,  50 },
        {   6, 240,  58 },
        {   4, 267,  66 },
        {   2, 400, 100 },
        {   0, 500, 130 },
    };
    const HAL_RATE_TABLE *rt;
    int i, j;

    OS_MEMSET(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
    rt = sc->sc_rates[mode];
    KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
    for (i = 0; i < rt->rateCount; i++) {
        sc->sc_rixmap[rt->info[i].rate_code] = (u_int8_t)i;
    }
    OS_MEMZERO(sc->sc_hwmap, sizeof(sc->sc_hwmap));
    for (i = 0; i < 256; i++) {
        u_int8_t ix = rt->rateCodeToIndex[i];
        if (ix == 0xff) {
            sc->sc_hwmap[i].ledon = 500;
            sc->sc_hwmap[i].ledoff = 130;
            continue;
        }
        ASSERT(ix < ARRAY_INFO_SIZE);
        sc->sc_hwmap[i].ieeerate =
            rt->info[ix].dot11Rate & IEEE80211_RATE_VAL;
        sc->sc_hwmap[i].rateKbps = rt->info[ix].rateKbps;

        if (rt->info[ix].shortPreamble ||
            rt->info[ix].phy == IEEE80211_T_OFDM) {
            //sc->sc_hwmap[i].flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
        }
        /* setup blink rate table to avoid per-packet lookup */
        for (j = 0; j < N(blinkrates)-1; j++) {
            if (blinkrates[j].rate == sc->sc_hwmap[i].ieeerate) {
                break;
            }
        }
        /* NB: this uses the last entry if the rate isn't found */
        /* XXX beware of overlow */
        sc->sc_hwmap[i].ledon = blinkrates[j].timeOn;
        sc->sc_hwmap[i].ledoff = blinkrates[j].timeOff;
    }
    sc->sc_currates = rt;
    sc->sc_curmode = mode;

    /*
     * If use iwpriv to re-set the cts rate,
     * it should be not chage again.
     */
    if (!sc->sc_cus_cts_set)
    {
        /*
                * All protection frames are transmited at 2Mb/s for
                * 11g, otherwise at 1Mb/s.
                * XXX select protection rate index from rate table.
                */
	    if (HAL_OK == ath_hal_getcapability(sc->sc_ah, HAL_CAP_OFDM_RTSCTS, 0, NULL)) {
	        switch(mode) {
	        case WIRELESS_MODE_11g:
	        case WIRELESS_MODE_108g:
	        case WIRELESS_MODE_11NG_HT20:
	        case WIRELESS_MODE_11NG_HT40PLUS:
	        case WIRELESS_MODE_11NG_HT40MINUS:
	            sc->sc_protrix = 4;         /* use 6 Mb for RTSCTS */
	            break;
	        default:
	            sc->sc_protrix = 0;
	        }
	    }
	    else
	        sc->sc_protrix = (mode == WIRELESS_MODE_11g ? 1 : 0);
    }
    if(sc->sc_user_en_rts_cts)
        sc->sc_en_rts_cts = (mode == WIRELESS_MODE_11b ? 0 :1);
    else
        sc->sc_en_rts_cts = 0;

    /* rate index used to send mgt frames */
    sc->sc_minrateix = 0;
}
#undef N

/******************************************************************************/
/*!
**  \brief Select Rate Table
**
**  Based on the wireless mode passed in, the rate table in the ATH object
**  is set to the mode specific rate table.  This also calls the callback
**  function to set the rate in the protocol layer object.
**
**  \param sc Pointer to ATH object (this)
**  \param mode Enumerated mode value used to select the specific rate table.
**
**  \return 0 if no valid mode is selected
**  \return 1 on success
*/

static int
ath_rate_setup(struct ath_softc *sc, WIRELESS_MODE mode)
{
    struct ath_hal *ah = sc->sc_ah;
    const HAL_RATE_TABLE *rt;

    switch (mode) {
    case WIRELESS_MODE_11a:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11A);
        break;
    case WIRELESS_MODE_11b:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11B);
        break;
    case WIRELESS_MODE_11g:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11G);
        break;
    case WIRELESS_MODE_108a:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_TURBO);
        break;
    case WIRELESS_MODE_108g:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_108G);
        break;
    case WIRELESS_MODE_11NA_HT20:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT20);
        break;
    case WIRELESS_MODE_11NG_HT20:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT20);
        break;
    case WIRELESS_MODE_11NA_HT40PLUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT40PLUS);
        break;
    case WIRELESS_MODE_11NA_HT40MINUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT40MINUS);
        break;
    case WIRELESS_MODE_11NG_HT40PLUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT40PLUS);
        break;
    case WIRELESS_MODE_11NG_HT40MINUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT40MINUS);
        break;
    default:
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid mode %u\n",
                __func__, mode);
        return 0;
    }
    rt = sc->sc_rates[mode];
    if (rt == NULL)
        return 0;

    /* setup rate set in 802.11 protocol layer */
    if (sc->sc_ieee_ops->setup_rate)
        sc->sc_ieee_ops->setup_rate(sc->sc_ieee, mode, NORMAL_RATE, rt);

    return 1;
}

/******************************************************************************/
/*!
**  \brief Setup half/quarter rate table support
**
**  Get pointers to the half and quarter rate tables.  Note that the mode
**  must have been selected previously.  Callbacks to the protocol object
**  are made if a table is selected.
**
**  \param sc Pointer to ATH object
**
**  \return N/A
*/

static void
ath_setup_subrates(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    const HAL_RATE_TABLE *rt;

    sc->sc_half_rates = ath_hal_getratetable(ah, HAL_MODE_11A_HALF_RATE);
    rt = sc->sc_half_rates;
    if (rt != NULL) {
        if (sc->sc_ieee_ops->setup_rate)
            sc->sc_ieee_ops->setup_rate(sc->sc_ieee,
                                        WIRELESS_MODE_11a, HALF_RATE, rt);
    }

    sc->sc_quarter_rates = ath_hal_getratetable(ah,
                                                HAL_MODE_11A_QUARTER_RATE);
    rt = sc->sc_quarter_rates;
    if (rt != NULL) {
        if (sc->sc_ieee_ops->setup_rate)
            sc->sc_ieee_ops->setup_rate(sc->sc_ieee, WIRELESS_MODE_11a,
                                        QUARTER_RATE, rt);
    }
}

/******************************************************************************/
/*!
**  \brief Set Transmit power limit
**
**  This is intended as an "external" interface routine, since it uses the
**  ath_dev_t as input vice the ath_softc.
**
**  \param dev Void pointer to ATH object used by upper layers
**  \param txpowlimit 16 bit transmit power limit
**
**  \return N/A
*/

static int
ath_set_txpowlimit(ath_dev_t dev, u_int16_t txpowlimit, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    /*
     * Just save it in configuration blob. It will take effect
     * after a chip reset.
     */
    if (is2GHz) {
        if (txpowlimit > ATH_TXPOWER_MAX_2G) {
            error = -EINVAL;
        } else {
            sc->sc_config.txpowlimit2G = txpowlimit;
        }
    } else {
        if (txpowlimit > ATH_TXPOWER_MAX_5G) {
            error = -EINVAL;
        } else {
            sc->sc_config.txpowlimit5G = txpowlimit;
        }
    }
    return error;
}

static void
ath_enable_tpc(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_enabletpc(ah);
    ath_wmi_set_desc_tpc(dev, 1);
}

void
ath_update_tpscale(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t tpscale;

    /* Get current tpscale setting and compare with the newly configured one */
    ath_hal_gettpscale(ah, &tpscale);

    if (sc->sc_config.tpscale != tpscale) {
        ath_hal_settpscale(ah, sc->sc_config.tpscale);
        /* If user tries to set lower than MIN, the value will be set to MIN.
        Do a read back to make sure sc_config reflects the actual value set. */
        ath_hal_gettpscale(ah, &tpscale);
        sc->sc_config.tpscale = tpscale;
    }
    /* Must do a reset for the new tpscale to take effect */

    if(!sc->sc_invalid) {
        ath_internal_reset(sc);
    }
}

void
ath_control_pwscale(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t pwscale;

    /* Get current pwscale setting and compare with the newly configured one */
    ath_hal_getpwscale(ah, &pwscale);

    if (sc->sc_config.pwscale != pwscale) {
        ath_hal_setpwscale(ah, sc->sc_config.pwscale);
        /* If user tries to set lower than MIN, the value will be set to MIN.
           Do a read back to make sure sc_config reflects the actual value set. */
        ath_hal_getpwscale(ah, &pwscale);
        sc->sc_config.pwscale = pwscale;
    }
    /* Must do a reset for the new tpscale to take effect */

    if(!sc->sc_invalid) {
        ath_internal_reset(sc);
    }
}

/******************************************************************************/
/*!
**  \brief Set Transmit power in HAL
**
**  This routine makes the actual HAL calls to set the new transmit power
**  limit.  This also calls back into the protocol layer setting the max
**  transmit power limit.
**
**  \param sc Pointer to ATH object (this)
**
**  \return N/A
*/

void
ath_update_txpow(struct ath_softc *sc, u_int16_t tpcInDb)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t txpow, txpowlimit;
    int32_t txpow_adjust, txpow_changed;

    ATH_TXPOWER_LOCK(sc);
    if (IS_CHAN_2GHZ(&sc->sc_curchan)) {
        txpowlimit = sc->sc_config.txpowlimit2G;
        txpow_adjust = sc->sc_config.txpowupdown2G;
    } else {
        txpowlimit = sc->sc_config.txpowlimit5G;
        txpow_adjust = sc->sc_config.txpowupdown5G;
    }

    txpowlimit = (sc->sc_config.txpowlimit_override) ?
        sc->sc_config.txpowlimit_override : txpowlimit;
    txpow_changed = 0;

    /* if txpow_adjust is <0, update max txpower always firstly */
    if ((sc->sc_curtxpow != txpowlimit) ||
        (txpow_adjust < 0) || sc->sc_config.txpow_need_update) {
       ath_hal_settxpowlimit(ah, txpowlimit, txpow_adjust, tpcInDb);
       /* read back in case value is clamped */
       ath_hal_gettxpowlimit(ah, &txpow);
       sc->sc_curtxpow = txpow;
       txpow_changed = 1;
       sc->sc_config.txpow_need_update = 0;
   }
    /*
     * Fetch max tx power level and update protocal stack
     */
    ath_hal_getmaxtxpow(sc->sc_ah, &txpow);

    if (txpow_adjust != 0) { //do nothing if >=0, can't go beyond power limit.
        /* set adjusted txpow to hal */
        txpow += txpow_adjust;
        ath_hal_settxpowlimit(ah, txpow, 0, tpcInDb);

        txpow_changed = 1;
        /* Fetch max tx power again */
        ath_hal_getmaxtxpow(sc->sc_ah, &txpow);
    }

    if (txpow_changed) {
        /* Since Power is changed, apply PAPRD table again*/
       if (sc->sc_paprd_enable) {
           int retval;
           sc->sc_curchan.paprd_table_write_done = 0;
           if (!sc->sc_scanning && (sc->sc_curchan.paprd_done == 1)) {
               retval = ath_apply_paprd_table(sc);
               DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: ath_apply_paprd_table retval %d \n", __func__, __LINE__, retval);
           }
       }
       DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: paprd_enable %d paprd_done %d Apply %d\n", __func__, __LINE__, sc->sc_paprd_enable, sc->sc_curchan.paprd_done, sc->sc_curchan.paprd_table_write_done);
    }
    if (!sc->sc_paprd_enable) {
        ath_hal_paprd_dec_tx_pwr(ah);
    }

    if (sc->sc_ieee_ops->update_txpow)
        sc->sc_ieee_ops->update_txpow(sc->sc_ieee, sc->sc_curtxpow, txpow);

#ifdef ATH_SUPPORT_TxBF
    /* Update the per rate TxBF disable flag status from the HAL */
    if ((sc->sc_txbfsupport == AH_TRUE) && sc->sc_rc) {
        struct atheros_softc *asc = sc->sc_rc;
        OS_MEMZERO(asc->sc_txbf_disable_flag,sizeof(asc->sc_txbf_disable_flag));
        ath_hal_get_perrate_txbfflag(sc->sc_ah, asc->sc_txbf_disable_flag);
     }
#endif
    ATH_TXPOWER_UNLOCK(sc);
}

/******************************************************************************/
/*!
**  \brief Set up New Node
**
** Setup driver-specific state for a newly associated node.  This routine
** really only applies if compression or XR are enabled, there is no code
** covering any other cases.
**
**  \param dev Void pointer to ATH object, to be called from protocol layre
**  \param node ATH node object that represents the other station
**  \param isnew Flag indicating that this is a "new" association
**
**  \return xxx
*/
static void
ath_newassoc(ath_dev_t dev, ath_node_t node, int isnew, int isuapsd)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    int tidno;
#if UNIFIED_SMARTANTENNA
     int i;
#endif
    /*
     * if station reassociates, tear down the aggregation state.
     */
    if (!isnew) {
        for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
            if (sc->sc_txaggr) {
                ath_tx_aggr_teardown(sc,an,tidno);
            }
            if (sc->sc_rxaggr) {
                ath_rx_aggr_teardown(sc,an,tidno);
            }
        }
    }
#ifdef ATH_SUPPORT_UAPSD
    an->an_flags = (isuapsd) ? ATH_NODE_UAPSD : 0;
#else
    an->an_flags=0;
#endif
    if ((sc->sc_opmode == HAL_M_HOSTAP)
        && (an->is_sta_node != 1)) {
        /* Mark the node as a STA node */
        an->is_sta_node = 1;
    }
#if UNIFIED_SMARTANTENNA
        for (i=0; i<= sc->max_fallback_rates; i++) {
        an->smart_ant_antenna_array[i] = sc->smart_ant_tx_default;
    }
#endif

}

/******************************************************************************/
/*!
**  \brief Convert Freq to channel number
**
**  Convert the frequency, provided in MHz, to an ieee 802.11 channel number.
**  This is done by calling a conversion routine in the ATH object.  The
**  sc pointer is required since this is a call from the protocol layer which
**  does not "know" about the internals of the ATH layer.
**
**  \param dev Void pointer to ATH object provided by protocol layer
**  \param freq Integer frequency, in MHz
**  \param flags Flag Value indicating if this is in the 2.4 GHz (CHANNEL_2GHZ)
**                    band or in the 5 GHz (CHANNEL_5GHZ).
**
**  \return Integer channel number
*/

static u_int
ath_mhz2ieee(ath_dev_t dev, u_int freq, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
     return ath_hal_mhz2ieee(sc->sc_ah, freq, flags);
}

/******************************************************************************/
/*!
**  \brief Set up channel list
**
**  Determines the proper set of channelflags based on the selected mode,
**  allocates a channel array, and passes it to the HAL for initialization.
**  If successful, the list is passed to the upper layer, then de-allocated.
**
**  \param sc Pointer to ATH object (this)
**  \param cc Integer Country Code value (normally read from EEPROM)
**  \param outDoor Boolean indicating if this transmitter is outdoors.
**  \param xchanMode Boolean indicating if extension channels are enabled.
**
**  \return -ENOMEM If channel strucure cannot be allocated
**  \return -EINVAL If channel initialization fails
**  \return 0 on success
*/

static int
ath_getchannels(struct ath_softc *sc, u_int cc,
                HAL_BOOL outDoor, HAL_BOOL xchanMode)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int8_t regclassids[ATH_REGCLASSIDS_MAX];
    u_int nregclass = 0;
    u_int wMode;
    u_int netBand;
    struct ieee80211com* ic = (struct ieee80211com*)(sc->sc_ieee);
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    int err = 0;
    uint16_t regdmn;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is NULL", __func__);
        return -EINVAL;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        qdf_print("%s : psoc is NULL", __func__);
        return -EINVAL;
    }

    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);


    wMode = sc->sc_reg_parm.wModeSelect;

    if (!(wMode & HAL_MODE_11A)) {
        wMode &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(wMode & HAL_MODE_11G)) {
        wMode &= ~(HAL_MODE_108G);
    }
    netBand = sc->sc_reg_parm.NetBand;
    if (!(netBand & HAL_MODE_11A)) {
        netBand &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(netBand & HAL_MODE_11G)) {
        netBand &= ~(HAL_MODE_108G);
    }
    wMode &= netBand;

    /*
     * remove some of the modes based on different compile time
     * flags.
     */
    ATH_DEV_RESET_HT_MODES(wMode);   /* reset the HT modes */

    regdmn = getEepromRD(sc->sc_ah);

    reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
    if (!(reg_rx_ops && reg_rx_ops->reg_program_default_cc)) {
        qdf_print("%s : reg_rx_ops is NULL", __func__);
        return -EINVAL;
    }

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
            QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }

    reg_rx_ops->reg_program_default_cc(pdev, regdmn);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

    if (sc->sc_ieee_ops->get_channel_list_from_regdb) {
        err = sc->sc_ieee_ops->get_channel_list_from_regdb(sc->sc_ieee);
        if (err)
            return err;
    }

    /* Initialize NF cal history buffer */
    ath_hal_init_NF_buffer_for_regdb_chans(ah, cc);

    if (sc->sc_ieee_ops->setup_channel_list) {
        sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, CLIST_UPDATE,
                                            regclassids, nregclass,
                                            CTRY_DEFAULT);
    }

    if(dfs_rx_ops && dfs_rx_ops->dfs_get_radars) {
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -EINVAL;
        }

        dfs_rx_ops->dfs_get_radars(pdev);

        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
    }

    return 0;
}

/******************************************************************************/
/*!
**  \brief Set Default Antenna
**
**  Call into the HAL to set the default antenna to use.  Not really valid for
**  MIMO technology.
**
**  \param context Void pointer to ATH object (called from Rate Control)
**  \param antenna Index of antenna to select
**
**  \return N/A
*/

void
ath_setdefantenna(void *context, u_int antenna)
{
    struct ath_softc *sc = (struct ath_softc *)context;
    struct ath_hal *ah = sc->sc_ah;
    ath_hal_setdefantenna(ah, antenna);
#if UNIFIED_SMARTANTENNA
    sc->smart_ant_tx_default = antenna;
#endif
    if (sc->sc_defant != antenna)
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_ant_defswitch_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_ant_defswitch++;
#endif
    sc->sc_defant = antenna;
    sc->sc_rxotherant = 0;

}

/******************************************************************************/
/*!
**  \brief Set Slot Time
**
**  This will wake up the chip if required, and set the slot time for the
**  frame (maximum transmit time).  Slot time is assumed to be already set
** in the ATH object member sc_slottime
**
**  \param sc Pointer to ATH object (this)
**
**  \return N/A
*/

void
ath_setslottime(struct ath_softc *sc)
{
    ATH_FUNC_ENTRY_VOID(sc);

    ATH_PS_WAKEUP(sc);
    ath_hal_setslottime(sc->sc_ah, sc->sc_slottime);
    sc->sc_updateslot = ATH_OK;
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Update slot time
**
**  This is a call interface used by the protocol layer to update the current
**  slot time setting based on updated settings.  This updates the hardware
**  also.
**
**  \param dev Void pointer to ATH object used by protocol layer
**  \param slottime Slot duration in microseconds
**
**  \return N/A
*/

static void
ath_updateslot(ath_dev_t dev, int slottime)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_FUNC_ENTRY_VOID(sc);
    ATH_PS_WAKEUP(sc);

    /* save slottime setting */
    sc->sc_slottime = slottime;

    /*!
    ** When not coordinating the BSS, change the hardware
    ** immediately.  For other operation we defer the change
    ** until beacon updates have propagated to the stations.
     */
    if (sc->sc_opmode == HAL_M_HOSTAP)
        sc->sc_updateslot = ATH_UPDATE;
    else
        ath_setslottime(sc);

    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Determine mode from channel flags
**
**  This routine will provide the enumerated WIRELESSS_MODE value based
**  on the settings of the channel flags.  If ho valid set of flags
**  exist, the lowest mode (11b) is selected.
**
**  \param chan Pointer to channel flags value
**
**  \return Higest channel mode selected by the flags.
*/

static WIRELESS_MODE
ath_halchan2mode(HAL_CHANNEL *chan)
{
    if ((chan->channel_flags & CHANNEL_108G) == CHANNEL_108G)
        return WIRELESS_MODE_108g;
    else if ((chan->channel_flags & CHANNEL_108A) == CHANNEL_108A)
        return WIRELESS_MODE_108a;
    else if ((chan->channel_flags & CHANNEL_A) == CHANNEL_A)
        return WIRELESS_MODE_11a;
    else if ((chan->channel_flags & CHANNEL_G) == CHANNEL_G)
        return WIRELESS_MODE_11g;
    else if ((chan->channel_flags & CHANNEL_B) == CHANNEL_B)
        return WIRELESS_MODE_11b;
    else if ((chan->channel_flags & CHANNEL_A_HT20) == CHANNEL_A_HT20)
        return WIRELESS_MODE_11NA_HT20;
    else if ((chan->channel_flags & CHANNEL_G_HT20) == CHANNEL_G_HT20)
        return WIRELESS_MODE_11NG_HT20;
    else if ((chan->channel_flags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS)
        return WIRELESS_MODE_11NA_HT40PLUS;
    else if ((chan->channel_flags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS)
        return WIRELESS_MODE_11NA_HT40MINUS;
    else if ((chan->channel_flags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS)
        return WIRELESS_MODE_11NG_HT40PLUS;
    else if ((chan->channel_flags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS)
        return WIRELESS_MODE_11NG_HT40MINUS;

    /* NB: should not get here */
    printk("%s: cannot map channel to mode; freq %u flags 0x%x\n",
           __func__, chan->channel, chan->channel_flags);
    return WIRELESS_MODE_11b;
}

#ifdef ATH_SUPPORT_DFS

/******************************************************************************/
/*!
**  \brief  Adjust DIFS value
**
**  This routine will adjust the DIFS value when VAP is set for
**  fixed rate in FCC domain. This is a fix for EV753454
**
**  \param sc Pointer to ATH object (this)
**
**  \return xxx
*/

static void ath_adjust_difs (struct ath_softc *sc, u_int32_t dfsdomain)
{
    int i;
    u_int32_t val;
    int adjust = 0;
    struct ath_vap *vap;

    /*
     * for now we modify DIFS only for FCC
     */

    if (!((dfsdomain == DFS_FCC_DOMAIN) || (dfsdomain == DFS_MKK4_DOMAIN) )) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d] Not FCC DOMAIN: %d\n", __func__, __LINE__, dfsdomain);
        return;
   }
   DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d] DFS DOMAIN: %d\n", __func__, __LINE__, dfsdomain);

    /*
     * check if the vap is set for fixed manual rate
     */

    for (i = 0; ((i < sc->sc_nvaps) && (adjust == 0)); i++) {
        vap = sc->sc_vaps[i];
        if (vap) {
            val = vap->av_config.av_fixed_rateset;
            if (val == IEEE80211_FIXED_RATE_NONE) {
                continue;
            }
            /*
             * we have a fixed rate. check if it is below 24 Mb
             *
             */
            switch (val & 0x7f) {
            case 0x0d:              /* 36 Mb */
            case 0x08:              /* 48 Mb */
            case 0x0c:              /* 54 Mb */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: fixed rate=0x%x is over 24 Mb. Default DIFS OK\n",
                       __func__,val);
                break;
            case 0x1b:              /* 1 Mb  */
            case 0x1a:              /* 2 Mb  */
            case 0x19:              /* 5 Mb  */
            case 0x0b:              /* 6 Mb  */
            case 0x0f:              /* 9 Mb  */
            case 0x18:              /* 11 Mb */
            case 0x0a:              /* 12 Mb */
            case 0x0e:              /* 18 Mb */
            case 0x09:              /* 24 Mb */
                adjust = 1;
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: fixed rate=0x%x is at or below 24 Mb. Adjust DIFS\n",
                       __func__,val);
                break;
            default:
                break;
            }
        }
    }


    /*
     * set DIFS between packets to  the new value
     *
     */

    if (adjust) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: DFS_FCC_DOMAIN, adjust DIFS\n", __func__);
        ath_hal_adjust_difs(sc->sc_ah, 1);
    }
 }
#endif /* ATH_SUPPORT_DFS */

/******************************************************************************/
/*!
**  \brief Change Channels
**
**  Performs the actions to change the channel in the hardware, and set up
**  the current operating mode for the new channel.
**
**  \param sc Pointer to ATH object (this)
**  \param chan Pointer to channel descriptor for new channel
**
**  \return xxx
*/

void
ath_chan_change(struct ath_softc *sc, HAL_CHANNEL *chan, bool ignore_dfs)
{
    WIRELESS_MODE mode;

    mode = ath_halchan2mode(chan);

    ath_rate_setup(sc, mode);
    ath_setcurmode(sc, mode);

    /*
     * Trigger setcurmode on the target
     */
    ATH_HTC_SETMODE(sc, mode);

#if WLAN_SPECTRAL_ENABLE
        {
            struct ath_spectral* spectral = (struct ath_spectral*)sc->sc_spectral;
            /* DFS and EACS are mutually exclussive. When the scan is done to find the best channel, we do need the
            * spectral scan data and the DCS need not scanning till the channel is selected, so there is no point in testing for
            * DCS being on before enabling spectral scan. The old code has been commented out
            */
             if (spectral && (spectral->sc_spectral_scan || spectral->sc_spectral_full_scan)) {
                    int error = 0;
                    SPECTRAL_LOCK(spectral);
                    DPRINTF(sc, ATH_DEBUG_ANY, "%s: ** spectral_scan_enabled during chan chg - restart\n", __func__);
                    error = spectral_scan_enable_params(spectral, &spectral->params);
                    if (error) {
                        DPRINTF(sc, ATH_DEBUG_ANY, "%s: spectral_scan__enable failed, HW may not be spectral capable \n", __func__);
                    }
                    SPECTRAL_UNLOCK(spectral);
            }
    }
#endif  //WLAN_SPECTRAL_ENABLE

}

#if ATH_SUPPORT_WIFIPOS

int
ath_get_channel_busy_info(ath_dev_t dev, u_int32_t *rxclear_pct, u_int32_t *rxframe_pct, u_int32_t *txframe_pct)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_getMibCycleCountsPct(sc->sc_ah, rxclear_pct, rxframe_pct,txframe_pct);
}

bool
ath_disable_hwq(ath_dev_t dev, u_int16_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_disable_hwq(sc->sc_ah, mask);
}

/******************************************************************************/
/*!
** \brief fast Set current channel. (Fast channel change)
** Fast channel change.  If the channel change is in-band then we don't need to do
** do chip reset. This function is written to take the minimal amount of time to do
** channel change
** DMA, then restart stuff after a la ath_init.
**
**  \param sc Pointer to ATH object (this)
**  \param hchan Pointer to channel descriptor for the new channel.
**  \return -EIO on failure
**  \return 0 on success
**  Limitations:
**  1) This only works for 2.4 Ghz in-band channel change.
**  2) The caller of this function should take care of draining/ pausing the software
**     queue.
**  3) Since we are removing all the packets from HW queue in this function. If there
**     are packet still residing in the txq, then  Tx tasklet will not be called, and
**     the traffic will be stuck. The user of this function has use the resched_tx
**     function to restart the transmission.
*/
int
ath_lean_chan_set(struct ath_softc *sc, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool retry)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_BOOL fastcc = AH_TRUE, stopped;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz) 0x%x -> %u (%u MHz) 0x%x\n",
            __func__,
            ath_hal_mhz2ieee(ah, sc->sc_curchan.channel, sc->sc_curchan.channel_flags),
            sc->sc_curchan.channel, sc->sc_curchan.channel_flags,
            ath_hal_mhz2ieee(ah, hchan->channel, hchan->channel_flags),
            hchan->channel, hchan->channel_flags);

    if (hchan->channel != sc->sc_curchan.channel ||
        hchan->channel_flags != sc->sc_curchan.channel_flags ||
        sc->sc_update_chainmask ||
        sc->sc_full_reset)
    {
        ATH_INTR_DISABLE(sc);
        ATH_USB_TX_STOP(sc->sc_osdev);
        ATH_CLEANHWQ(sc);

        stopped = ATH_STOPRECV_EX(sc, rx_timeout);  /* turn off frame recv */
        if (sc->sc_scanning || !(hchan->channel_flags & CHANNEL_5GHZ))
            ath_hal_setcoverageclass(ah, 0, 0);
        else
            ath_hal_setcoverageclass(ah, sc->sc_coverageclass, 0);

        if (!stopped || sc->sc_full_reset)
            fastcc = AH_FALSE;

        sc->sc_paprd_done_chain_mask = 0;
#if !ATH_RESET_SERIAL
        ATH_LOCK_PCI_IRQ(sc);
#endif

        sc->sc_paprd_done_chain_mask = 0;

        if(!ath_hal_lean_channel_change(ah, sc->sc_opmode, hchan,
                           ht_macmode, sc->sc_tx_chainmask, sc->sc_rx_chainmask))
        {
            HAL_STATUS status;
            printk("%s: unable to change channel %u (%uMhz) "
                   "flags 0x%x \n",
                   __func__,
                   ath_hal_mhz2ieee(ah, hchan->channel, hchan->channel_flags),
                   hchan->channel, hchan->channel_flags);
            // resetting the chip because of channel change failure.
            atomic_inc(&sc->sc_in_reset);
#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_halresets++;
#endif
            if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan, ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning)){
                DPRINTF(sc, ATH_DEBUG_RESET,
                "%s: unable to reset hardware; hal status %u "
               "(freq %u flags 0x%x)\n", __func__, status,
               sc->sc_curchan.channel, sc->sc_curchan.channel_flags);
                printk("%s: unable to reset hardware; hal status %u "
               "(freq %u flags 0x%x)\n", __func__, status,
               sc->sc_curchan.channel, sc->sc_curchan.channel_flags);
#if !ATH_RESET_SERIAL
               ATH_UNLOCK_PCI_IRQ(sc);
               atomic_dec(&sc->sc_in_reset);
#endif
                if (ATH_STARTRECV(sc) != 0) {
                    printk("%s: unable to restart recv logic\n",
                           __func__);
                }

               ATH_INTR_ENABLE(sc);
               ATH_USB_TX_START(sc->sc_osdev);
                return  -EIO;
            }
#if !ATH_RESET_SERIAL
            ATH_UNLOCK_PCI_IRQ(sc);
            atomic_dec(&sc->sc_in_reset);
#endif

            if (ATH_STARTRECV(sc) != 0) {
                printk("%s: unable to restart recv logic\n", __func__);
            } else {
                printk(KERN_DEBUG"STARTED RECV \n");
            }
            return -EIO;
        }
#if !ATH_RESET_SERIAL
        ATH_UNLOCK_PCI_IRQ(sc);
#endif

        sc->sc_curchan = *hchan;
        sc->sc_curchan.paprd_done = 0;
        sc->sc_curchan.paprd_table_write_done = 0;
        sc->sc_update_chainmask = 0;
        sc->sc_full_reset = 0;

        /*
         * Re-enable rx framework.
         */
        ath_wmi_start_recv(sc);
        if (ATH_STARTRECV(sc) != 0) {
            printk("%s: unable to restart recv logic\n",
                   __func__);
            ATH_INTR_ENABLE(sc);
            ATH_USB_TX_START(sc->sc_osdev);
            return -EIO;
        }
        ath_chan_change(sc, hchan, false);

        ath_update_txpow(sc, sc->tx_power);        /* update tx power state */
        /*!
        ** This is only needed for AP or IBSS mode. Also, we don't
        ** want to send beacons on a non-home channel during a
        ** background scan in IBSS mode.
         */


        /*
         * Re-enable interrupts.
         */
        ATH_INTR_ENABLE(sc);
        ATH_USB_TX_START(sc->sc_osdev);
    }

    return 0;
}
#endif

/******************************************************************************/
/*!
**  \brief Set the current channel
**
** Set/change channels.  If the channel is really being changed, it's done
** by reseting the chip.  To accomplish this we must first cleanup any pending
** DMA, then restart stuff after a la ath_init.
**
**  \param sc Pointer to ATH object (this)
**  \param hchan Pointer to channel descriptor for the new channel.
**  \param ignore_dfs when true does not enable DFS when operating in a DFS channel
**
**  \return -EIO on failure
**  \return 0 on success
*/

static int
ath_chan_set(struct ath_softc *sc, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool retry, bool ignore_dfs)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_BOOL fastcc = AH_TRUE, stopped;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
#if 0 //def ATH_SUPPORT_DFS
    u_int8_t chan_flags_changed=0;
    HAL_CHANNEL old_hchan;
#endif
    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz) 0x%x -> %u (%u MHz) 0x%x\n",
            __func__,
            ath_hal_mhz2ieee(ah, sc->sc_curchan.channel, sc->sc_curchan.channel_flags),
            sc->sc_curchan.channel, sc->sc_curchan.channel_flags,
            ath_hal_mhz2ieee(ah, hchan->channel, hchan->channel_flags),
            hchan->channel, hchan->channel_flags);

    if (hchan->channel != sc->sc_curchan.channel ||
        hchan->channel_flags != sc->sc_curchan.channel_flags ||
        sc->sc_update_chainmask ||
        sc->sc_full_reset)
    {
        HAL_STATUS status;

        /*!
        ** This is only performed if the channel settings have
        ** actually changed.
        **
        ** To switch channels clear any pending DMA operations;
        ** wait long enough for the RX fifo to drain, reset the
        ** hardware at the new frequency, and then re-enable
        ** the relevant bits of the h/w.
        */

        /* Set the flag so that other functions will not try to restart tx/rx DMA. */
        /* Note: we should re-organized the fellowing code to use ath_reset_start(),
           ath_reset(), and ath_reset_end() */
#if ATH_RESET_SERIAL
        ATH_RESET_ACQUIRE_MUTEX(sc);
        ATH_RESET_LOCK(sc);
        atomic_inc(&sc->sc_dpc_stop);
        printk("%s %d %d\n", __func__,
__LINE__, sc->sc_dpc_stop);
        ATH_USB_TX_STOP(sc->sc_osdev);
        ATH_INTR_DISABLE(sc);
        ATH_RESET_UNLOCK(sc);
#else
        atomic_inc(&sc->sc_in_reset);
        ATH_USB_TX_STOP(sc->sc_osdev);
        ATH_INTR_DISABLE(sc);
#endif


#if ATH_RESET_SERIAL
        OS_DELAY(100);
        ATH_RESET_LOCK(sc);
        atomic_inc(&sc->sc_hold_reset);
        atomic_set(&sc->sc_reset_queue_flag, 1);
        ath_reset_wait_tx_rx_finished(sc);
        ATH_RESET_UNLOCK(sc);
        ath_reset_wait_intr_dpc_finished(sc);
        atomic_dec(&sc->sc_dpc_stop);
        printk("%s %d %d\n", __func__,
__LINE__, sc->sc_dpc_stop);
        printk("Begin chan_set sequence......\n");
#endif
        ATH_DRAINTXQ(sc, retry, tx_timeout);     /* clear or requeue pending tx frames */
        stopped = ATH_STOPRECV_EX(sc, rx_timeout);  /* turn off frame recv */

        /* XXX: do not flush receive queue here. We don't want
         * to flush data frames already in queue because of
         * changing channel. */

        /* Set coverage class */
        if (sc->sc_scanning || !(hchan->channel_flags & CHANNEL_5GHZ))
            ath_hal_setcoverageclass(ah, 0, 0);
        else
            ath_hal_setcoverageclass(ah, sc->sc_coverageclass, 0);

        if (!stopped || sc->sc_full_reset)
            fastcc = AH_FALSE;

        ATH_HTC_TXRX_STOP(sc, fastcc);


#if ATH_C3_WAR
        STOP_C3_WAR_TIMER(sc);
#endif

#if !ATH_RESET_SERIAL
        ATH_LOCK_PCI_IRQ(sc);
#endif
        sc->sc_paprd_done_chain_mask = 0;
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_halresets++;
#endif
        if (!ath_hal_reset(ah, sc->sc_opmode, hchan,
                           ht_macmode, sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                           sc->sc_ht_extprotspacing, fastcc, &status,
                           sc->sc_scanning)) {
            printk("%s: unable to reset channel %u (%uMhz) "
                   "flags 0x%x hal status %u\n",
                   __func__,
                   ath_hal_mhz2ieee(ah, hchan->channel, hchan->channel_flags),
                   hchan->channel, hchan->channel_flags, status);
#if ATH_RESET_SERIAL
            atomic_set(&sc->sc_reset_queue_flag, 0);
            atomic_dec(&sc->sc_hold_reset);
            ATH_RESET_RELEASE_MUTEX(sc);
#else
            ATH_UNLOCK_PCI_IRQ(sc);
            atomic_dec(&sc->sc_in_reset);
#endif
            ASSERT(atomic_read(&sc->sc_in_reset) >= 0);

            if (ATH_STARTRECV(sc) != 0) {
                printk("%s: unable to restart recv logic\n", __func__);
            }
            return -EIO;
        }
        ATH_HTC_ABORTTXQ(sc);
#if !ATH_RESET_SERIAL
        ATH_UNLOCK_PCI_IRQ(sc);
#endif
        sc->sc_curchan = *hchan;
        sc->sc_curchan.paprd_done = 0;
        sc->sc_curchan.paprd_table_write_done = 0;
        sc->sc_update_chainmask = 0;
        sc->sc_full_reset = 0;

#ifdef ATH_BT_COEX
#if ATH_SUPPORT_MCI
        if (sc->sc_hasbtcoex && (!fastcc || ath_hal_hasmci(sc->sc_ah))) {
#else
        if (sc->sc_hasbtcoex && (!fastcc)) {
#endif
            u_int32_t   scanState = ATH_COEX_WLAN_SCAN_RESET;
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
        }
#endif

#if ATH_RESET_SERIAL
            /* to keep lock sequence as same as ath_tx_start_dma->ath_tx_txqaddbuf,
                 * we have to lock all of txq firstly.
                 */
            if (!sc->sc_enhanceddmasupport) {
                int i;
                for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
                     ATH_TXQ_LOCK(&sc->sc_txq[i]);

                ATH_RESET_LOCK(sc);
                atomic_set(&sc->sc_reset_queue_flag, 0);
                atomic_dec(&sc->sc_hold_reset);
                if (atomic_read(&sc->sc_hold_reset))
                    printk("Wow, still multiple reset in process,--investigate---\n");

                 /* flush all the queued tx_buf */
                for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {

                    ath_bufhead *head, buf_head;
                    struct ath_buf *buf, *last_buf;

                    //ATH_TXQ_LOCK(&sc->sc_txq[i]);
                    //ATH_RESET_LOCK_Q(sc, i);

                    head = &(sc->sc_queued_txbuf[i].txbuf_head);

                    do {
                        buf = TAILQ_FIRST(head);
                        if (!buf)
                            break;
                        last_buf = buf->bf_lastbf;
                        TAILQ_INIT(&buf_head);
                        TAILQ_REMOVE_HEAD_UNTIL(head, &buf_head, last_buf, bf_list);
                        _ath_tx_txqaddbuf(sc, &sc->sc_txq[i], &buf_head); //can't call directly
                    } while (!TAILQ_EMPTY(head));


                    //ATH_RESET_UNLOCK_Q(sc, i);
                    //ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
                }
                ATH_RESET_UNLOCK(sc);
                for (i = HAL_NUM_TX_QUEUES -1 ; i >= 0; i--)
                     ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
            } else {
                atomic_set(&sc->sc_reset_queue_flag, 0);
                atomic_dec(&sc->sc_hold_reset);
            }
#else

        /* Clear the "In Reset" flag. */
        atomic_dec(&sc->sc_in_reset);
#endif
        ASSERT(atomic_read(&sc->sc_in_reset) >= 0);

        /*
         * Re-enable rx framework.
         */
        ath_wmi_start_recv(sc);

        if (ATH_STARTRECV(sc) != 0) {
            printk("%s: unable to restart recv logic\n",
                   __func__);
            return -EIO;
        }

        /*
         * Change channels and update the h/w rate map
         * if we're switching; e.g. 11a to 11b/g.
         */
        ath_chan_change(sc, hchan, ignore_dfs);

        ath_update_txpow(sc, sc->tx_power);        /* update tx power state */

        /*!
        ** This is only needed for AP or IBSS mode. Also, we don't
        ** want to send beacons on a non-home channel during a
        ** background scan in IBSS mode.
         */
        if (sc->sc_opmode == HAL_M_HOSTAP ||
            sc->sc_opmode == HAL_M_IBSS) {
            if (!sc->sc_scanning)
                ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
        }

#ifdef ATH_GEN_TIMER
        /* Inform the generic timer module that a reset just happened. */
        ath_gen_timer_post_reset(sc);
#endif  //ATH_GEN_TIMER

        /*
         * Re-enable interrupts.
         */
        ATH_INTR_ENABLE(sc);
        ATH_USB_TX_START(sc->sc_osdev);
#if ATH_RESET_SERIAL
        ATH_RESET_RELEASE_MUTEX(sc);
#endif
    }
    else {
        /*
         * In certain case, we do offchan scan on the home channel.
         * Should also enable RX in this case, since vap_pause_control
         * already stops RX.
         */
        if (sc->sc_scanning) {
            /* reclaim sc_rxbuf first */
            ATH_STOPRECV(sc, rx_timeout);
            if (ATH_STARTRECV(sc) != 0) {
                printk("%s: unable to restart recv logic\n",
                       __func__);
                return -EIO;
            }
        }
    }

#if 0 //def ATH_SUPPORT_DFS
    ath_detach_dfs(sc, &old_hchan, hchan);
#endif

    return 0;
}

/******************************************************************************/
/*!
**  \brief Calculate the Calibration Interval
**
**  The interval must be the shortest necessary to satisfy ANI,
**  short calibration and long calibration.
**
**  \param sc_Caldone: indicates whether calibration has already completed
**         enableANI: indicates whether ANI is enabled
**
**  \return The timer interval
*/

static u_int32_t
ath_get_cal_interval(HAL_BOOL sc_Caldone, u_int32_t ani_interval,
        HAL_BOOL isOpenLoopPwrCntrl, HAL_BOOL is_crdc, u_int32_t cal_interval)
{
    if (isOpenLoopPwrCntrl) {
        cal_interval = min(cal_interval, (u_int32_t)ATH_OLPC_CALINTERVAL);
    }

    if (is_crdc) {
        cal_interval = min(cal_interval, (u_int32_t)ATH_CRDC_CALINTERVAL);
    }

    if (ani_interval) {
        // non-zero means ANI enabled
        cal_interval = min(cal_interval, ani_interval);
    }
    if (! sc_Caldone) {
        cal_interval = min(cal_interval, (u_int32_t)ATH_SHORT_CALINTERVAL);
    }

    /*
     * If we wanted to be really precise we'd now compare cal_interval to the
     * time left before each of the calibrations must be run.
     * For example, it is possible that sc_Caldone changed from FALSE to TRUE
     * just 1 second before the long calibration interval would elapse.
     * Is this case the timer interval is still set to ATH_LONG_CALINTERVAL,
     * causing an extended delay before the long calibration is performed.
     *
     * Using values for ani and short and long calibrations that are not
     * multiples of each other would cause a similar problem (try setting
     * ATH_SHORT_CALINTERVAL = 20000 and ATH_LONG_CALINTERVAL = 30000)
     */

    return cal_interval;
}

#ifdef ATH_TX_INACT_TIMER
static
OS_TIMER_FUNC(ath_inact)
{
    struct ath_softc    *sc;
    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid ) {
        goto out;
    }
#if 0 //def ATH_SUPPORT_DFS
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
        OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
        return;
    }
#endif

    if (sc->sc_tx_inact >= sc->sc_max_inact) {
    int i;
        sc->sc_tx_inact = 0;
    for (i = 0; i < 9; i++) { /* beacon stuck will handle queue 9 */
        if (ath_hal_numtxpending(sc->sc_ah, i)) {
        ath_internal_reset(sc);
        OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
        return;
        }
        }
    } else {
        sc->sc_tx_inact ++;
    }

out:
    OS_SET_TIMER(&sc->sc_inact, 1000 /* ms */);
    return;
}
#endif

static unsigned int hw_read_count = 0;
static u_int16_t reset_required = 0;
#define WAIT_COUNT 50 /* Suggested by Ravi, Srikanth and Mani: This is 500 milliseconds delay */
static
OS_TIMER_FUNC(ath_rx_hwreg_checker)
{
    struct ath_softc *sc;
    u_int32_t pcu_obs;
    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid || sc->sc_scanning || atomic_read(&sc->sc_in_reset)) {
        return;
    }

    if(hw_read_count < WAIT_COUNT) {
        pcu_obs = ath_hal_readRegister(sc->sc_ah,0x806C);
        if((pcu_obs >> 20 & 0x3f) == 0x17) {
            reset_required++;
        }
        hw_read_count++;
    } /* End of while(count < 1000) { */
    if(hw_read_count >= WAIT_COUNT || reset_required > 2) {
        if(reset_required > 2) {
            DPRINTF(sc, ATH_DEBUG_ANY, "\n %s resetting hardware for Rx stuck ",__func__);
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_resetOnError++;
#endif
            reset_required = 0;
        }
        hw_read_count = 0;
    } else {
        if (!sc->sc_rxstuck_shutdown_flag) {
            OS_SET_TIMER(&sc->sc_hwstuck, 10);
        } else {
            OS_CANCEL_TIMER(&sc->sc_hwstuck);
        }
    }
    return;
}

#define RXSTUCK_TIMER_VAL 1000 /* 1 sec */
static
OS_TIMER_FUNC(ath_rx_stuck_checker)
{
    struct ath_softc *sc;
    u_int32_t cur_lp_rx_dp;
    u_int32_t cur_fifo_depth;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid || sc->sc_scanning || atomic_read(&sc->sc_in_reset) ) {
        goto out;
    }

    /*
     * if we get receive overrun interrupt then check
     * if FIFO depth is > 0 and then check if LP Rx_DP
     * is not moving. If not moving reset the CHIP
     *  Size of LP RxDP FIFO  {0x0070}LP[7:0]
     *  Size of HP RxDP FIFO  {0x0070}LP[12:8]
     */
    cur_lp_rx_dp = ath_hal_readRegister(sc->sc_ah,0x0078);
    cur_fifo_depth = ath_hal_readRegister(sc->sc_ah,0x0070) & 0x00001FFF;
    if(sc->sc_stats.ast_rxorn &&  cur_fifo_depth && (sc->sc_stats.ast_rxorn != sc->sc_last_rxorn)) {
         DPRINTF(sc, ATH_DEBUG_ANY, "\n %s overrun is hit\n",__func__);
        if(sc->sc_last_lp_rx_dp && (cur_lp_rx_dp == sc->sc_last_lp_rx_dp)) {
            reset_required++;
        }
    }
    else {
        if((sc->sc_last_lp_rx_dp && (cur_lp_rx_dp == sc->sc_last_lp_rx_dp))) {
            OS_SET_TIMER(&sc->sc_hwstuck, 10);
        } /* End of if */
    } /* End of else */


    if (reset_required > 2) {
        /* hardware Rx stuck detected !!!! reset hardware */
        DPRINTF(sc, ATH_DEBUG_ANY, "\n %s resetting hardware for Rx stuck ",__func__);
        sc->sc_reset_type = ATH_RESET_NOLOSS;
        ath_internal_reset(sc);
        sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_resetOnError++;
#endif
        reset_required = 0;
    }
    sc->sc_last_rxorn = sc->sc_stats.ast_rxorn;
    sc->sc_last_lp_rx_dp = cur_lp_rx_dp;
out:
    if (!sc->sc_rxstuck_shutdown_flag) {
        OS_SET_TIMER(&sc->sc_rxstuck, RXSTUCK_TIMER_VAL);
    } else {
        OS_CANCEL_TIMER(&sc->sc_rxstuck);
    }
    return;
}


#if ATH_HW_TXQ_STUCK_WAR
#define MAX_QSTUCK_TIME 3 /* 3*1000=3 which means min of 2 sec and max of 3 sec */
#define QSTUCK_TIMER_VAL 1000 /* 1 sec */
static
OS_TIMER_FUNC(ath_txq_stuck_checker)
{
    struct ath_softc *sc;
    struct ath_txq *txq;
    struct ath_hal *ah;
    u_int16_t i;
    u_int16_t reset_required = 0;
    systime_t current_jiffies;
    u_int32_t rxeol_mask_time = 0;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid || sc->sc_scanning || atomic_read(&sc->sc_in_reset)) {
        goto out;
    }
#if 0 //def ATH_SUPPORT_DFS
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
        OS_SET_TIMER(&sc->sc_qstuck, 60000 /* ms */);
        return;
    }
#endif

    /* check for all queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
        txq = &sc->sc_txq[i];
        ATH_TXQ_LOCK(txq);
        if(txq->axq_tailindex != txq->axq_headindex) {
            txq->tx_done_stuck_count++;
            if(txq->tx_done_stuck_count >= MAX_QSTUCK_TIME) {
                reset_required = 1;
                txq->tx_done_stuck_count = 0;
                DPRINTF(sc, ATH_DEBUG_ANY, "\n %s ******** Hardware Q-%d is stuck !!!!",__func__,i);
            }
        } else {
            /* no packets in hardware fifo */
            txq->tx_done_stuck_count = 0;
        }
        ATH_TXQ_UNLOCK(txq);
    }
    /* WAR : RXEOL masked due to some reason - unmasking it here in timer function */
    ah = sc->sc_ah;
    if(sc->sc_enhanceddmasupport) {
        if(!(sc->sc_imask & HAL_INT_RXEOL)) {
            current_jiffies = OS_GET_TIMESTAMP();
            rxeol_mask_time = CONVERT_SYSTEM_TIME_TO_SEC(current_jiffies - sc->sc_last_rxeol);
            if (rxeol_mask_time > MAX_QSTUCK_TIME) {
                sc->sc_imask |= HAL_INT_RXEOL | HAL_INT_RXORN;
                sc->sc_stats.ast_rxeol_recover++;
                reset_required = 1;
                DPRINTF(sc, ATH_DEBUG_ANY, "\n %s ******** Hardware RX Q is stuck !!!!",__func__);
            }
        }
    }
    if (reset_required) {
        /* hardware q stuck detected !!!! reset hardware */
        DPRINTF(sc, ATH_DEBUG_ANY, "\n %s resetting hardware for Q stuck ",__func__);
        sc->sc_reset_type = ATH_RESET_NOLOSS;
        ath_internal_reset(sc);
        sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_resetOnError++;
#endif
    }

out:
    OS_SET_TIMER(&sc->sc_qstuck, QSTUCK_TIMER_VAL);
    return;
}
#endif


static int
ath_up_beacon(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;

    sc->sc_up_beacon_purge = 0;
    return 1;   /* don't Re-Arm */
}

#if ATH_SUPPORT_VOW_DCS
static int
ath_sc_chk_intr(struct ath_softc *sc, HAL_ANISTATS *ani_stats)
{
    struct ath_dcs_params       *dcs_params;
    u_int32_t cur_ts;
    u_int32_t ts_d = 0;          /* elapsed time                */
    u_int32_t rxclr_d = 0;       /* differential rx_clear count */
    u_int32_t cur_thp, unusable_cu, rxc_cu;
    u_int32_t total_cu, tx_cu, rx_cu, wasted_txcu, total_wasted_cu, tx_err;
    u_int32_t maxerr_cnt, maxerr_rate, ofdmphyerr_rate, cckphyerr_rate;
    int higherr = 0;
    u_int32_t err_diff = 0;
#if WLAN_SPECTRAL_ENABLE
    struct ath_spectral* spectral = sc->sc_spectral;
    struct ath_spectral_ops *p_sops = (struct ath_spectral_ops*)GET_SPECTRAL_OPS(spectral);
#endif  /* WLAN_SPECTRAL_ENABLE */
    dcs_params = &sc->sc_dcs_params;
    DPRINTF(sc, ATH_DEBUG_DCS, "DCS: dcrx %u dctx %u rxt %u wtxt %u\n",
            dcs_params->bytecntrx, dcs_params->bytecnttx, dcs_params->rxtime,
            dcs_params->wastedtxtime );
    /*DPRINTF(sc, ATH_DEBUG_DCS, "DCS: %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
            ani_stats->cyclecnt_diff,  ani_stats->rxclr_cnt,  ani_stats->txframecnt_diff,
            ani_stats->rxframecnt_diff,  ani_stats->listen_time,  ani_stats->ofdmphyerr_cnt,
            ani_stats->cckphyerr_cnt,  ani_stats->valid, ani_stats->ofdmphyerrcnt_diff );*/

    cur_ts = ath_hal_gettsf32(sc->sc_ah);
    if (dcs_params->prev_ts > cur_ts ){
        DPRINTF(sc, ATH_DEBUG_DCS, "DCS:%s timestamp Counter wrap!!!\n",__func__);
        dcs_params->intr_cnt = 0;
        dcs_params->samp_cnt = 0;
        goto terminate;
    }
    else {
        ts_d = cur_ts - dcs_params->prev_ts;
        /* compute current thp in Mbps */
        if ( dcs_params->bytecntrx + dcs_params->bytecnttx  > 0xfffffff ){
            cur_thp = ((( dcs_params->bytecntrx + dcs_params->bytecnttx ) >> 8 )/ (ts_d >> 8))*8;
        }
        else{
            cur_thp = ((( dcs_params->bytecntrx + dcs_params->bytecnttx )*8)/ts_d);
        }
    }

    if ((dcs_params->prev_rxclr > ani_stats->rxclr_cnt) || (!ani_stats->valid) ||
            (ts_d > 1100000)){
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS:%s Counter wrap!!!\n",__func__ );
        goto terminate;
    }
    else {
        rxclr_d = ani_stats->rxclr_cnt - dcs_params->prev_rxclr;

        total_cu = ( (rxclr_d >> 8 )* 100)/ (ani_stats->cyclecnt_diff >> 8 );
        tx_cu = ( (ani_stats->txframecnt_diff >>8) * 100)/ (ani_stats->cyclecnt_diff >> 8 );
        rx_cu =  ((dcs_params->rxtime >> 8) * 100) / (ts_d >> 8) ;
        rxc_cu = ( (ani_stats->rxframecnt_diff >>8) * 100)/ (ani_stats->cyclecnt_diff >> 8 );
        if (rx_cu > rxc_cu ){
            rx_cu = rxc_cu;
        }
        wasted_txcu = ((dcs_params->wastedtxtime >> 8) * 100) / (ts_d>>8) ;
        if (tx_cu < wasted_txcu){
            wasted_txcu = tx_cu;
        }
        tx_err = (tx_cu && wasted_txcu)? (wasted_txcu * 100)/tx_cu : 0;

        unusable_cu = (total_cu >= (tx_cu + rx_cu))? (total_cu - tx_cu - rx_cu) : 0;

        /* rx loss is included in unusable_cu */
        total_wasted_cu = unusable_cu + wasted_txcu;
#if WLAN_SPECTRAL_ENABLE
        if (!p_sops->is_spectral_active((struct ath_spectral*)sc->sc_spectral))
#endif /* WLAN_SPECTRAL_ENABLE */
        {
            err_diff = ani_stats->ofdmphyerrcnt_diff * sc->sc_phyerr_penalty; /* PHYERR_PENALTY_TIME */
            total_wasted_cu  +=  err_diff? (((err_diff >> 8)*100)/(ts_d >> 8)) : 0;

            ofdmphyerr_rate = ani_stats->ofdmphyerr_cnt * 1000 / ani_stats->listen_time;
            cckphyerr_rate  = ani_stats->cckphyerr_cnt * 1000 / ani_stats->listen_time;
            maxerr_rate     = MAX( ofdmphyerr_rate, cckphyerr_rate);
            maxerr_cnt      = MAX(ani_stats->ofdmphyerr_cnt , ani_stats->cckphyerr_cnt);

            if ((maxerr_rate >=  PHYERR_RATE_THRESH) && (maxerr_cnt >=  PHYERR_RATE_THRESH)){
                higherr = 1;
            }else if(dcs_params->phyerrcnt > RADARERR_THRESH){
                higherr = 1;
                //this may happen because of hw limitations, not sure
                //if we should change channal here
            }

        }

        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: totcu %u txcu %u rxcu %u %u txerr %u\n",
                total_cu, tx_cu, rx_cu, rxc_cu, tx_err);
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: unusable_cu %d wasted_cu %u thp %u ts %u\n",
                unusable_cu, total_wasted_cu, cur_thp, ts_d );
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: err %d %u %u %u %u time %u\n", higherr, ani_stats->ofdmphyerr_cnt,
                ani_stats->cckphyerr_cnt, dcs_params->phyerrcnt, err_diff, ani_stats->listen_time);

    }

    if (unusable_cu >= sc->sc_coch_intr_thresh) {
        dcs_params->intr_cnt += 2;
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: INTR DETECTED\n",__func__ );
    } else if (higherr && ( ((total_wasted_cu >= MAX(40,(sc->sc_coch_intr_thresh+10)))
                    && ((tx_cu + rx_cu) > 50)) ||
                ((tx_cu > 30) && (tx_err >= sc->sc_tx_err_thresh)) )){
          /* far range case is excluded here, as wasted ch timer is updated
           only when RSSI is above a threshold.
           To Do list :
           Tune the parameters just after apup, when the performnace is good.
           Update sc_coch_intr_thresh with the curr ch load of channel
           chosen by EACS */
        dcs_params->intr_cnt++;
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: INTR DETECTED\n",__func__ );
    }

    if( !dcs_params->intr_cnt ){
        dcs_params->samp_cnt = 0;
    } else if (dcs_params->intr_cnt >= INTR_DETECTION_CNT){
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: Trigger EACS \n",__func__);
        printk("Interference Detected, Changing Channel\n");
#if WLAN_SPECTRAL_ENABLE
        if (sc->sc_icm_active) {
            /* ICM is active, let it handle the channel change */
            DPRINTF(sc, ATH_DEBUG_DCS,
                    "ICM is active, sending message to change "
                    "channel with flag %d\n",
                    sc->sc_dcs_enabled);
            spectral_send_intf_found_msg(spectral, 0, sc->sc_dcs_enabled);
        } else {
            /* Channel change to be handled by the driver */
            sc->sc_ieee_ops->cw_interference_handler(sc->sc_ieee);
        }
#else
        sc->sc_ieee_ops->cw_interference_handler(sc->sc_ieee);
#endif /* WLAN_SPECTRAL_ENABLE */

        dcs_params->intr_cnt = 0;
        dcs_params->samp_cnt = 0;
    }else if( dcs_params->samp_cnt >= INTR_DETECTION_WINDOW ){
        dcs_params->intr_cnt = 0;
        dcs_params->samp_cnt = 0;
    } else {
        dcs_params->samp_cnt++;
    }

terminate:
    /* reset counters */
    dcs_params->bytecntrx = 0;             /* Running received data sw counter */
    dcs_params->rxtime = 0;
    dcs_params->wastedtxtime = 0;
    dcs_params->bytecnttx = 0;           /* Running transmitted data sw counter */
    dcs_params->phyerrcnt = 0;
    dcs_params->prev_rxclr = ani_stats->rxclr_cnt;
    dcs_params->prev_ts = cur_ts;

    return 0;
}
#endif

/******************************************************************************/
/*!
**  \brief Calibration Task
**
**  This routine performs the periodic noise floor calibration function
**  that is used to adjust and optimize the chip performance.  This
**  takes environmental changes (location, temperature) into account.
**  When the task is complete, it reschedules itself depending on the
**  appropriate interval that was calculated.
**
**  \param xxx Parameters are OS dependant.  See the definition of the
**             OS_TIMER_FUNC macro
**
**  \return N/A
*/

static int
ath_calibrate(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;
    struct ath_hal      *ah;
    HAL_BOOL            longcal    = AH_FALSE;
    HAL_BOOL            shortcal   = AH_FALSE;
    HAL_BOOL            olpccal    = AH_FALSE;
    HAL_BOOL            aniflag    = AH_FALSE;
    HAL_BOOL            multical   = AH_FALSE;
    HAL_BOOL            crdccal    = AH_FALSE;
    HAL_BOOL            work_to_do = AH_FALSE;
    systime_t           timestamp  = OS_GET_TIMESTAMP();
    u_int32_t           i, sched_cals;
    HAL_CALIBRATION_TIMER   *cal_timerp;
    u_int32_t           ani_interval, skip_paprd_cal = 0;
#ifdef __linux__
    int                 reset_serialize_locked = 0;
#endif
    int16_t noise_to_report = 0;

    ah = sc->sc_ah;

    if (!ath_hal_enabledANI(ah) ||
        !ath_hal_getanipollinterval(ah, &ani_interval)) {
        ani_interval = 0; // zero means ANI disabled
    }

    /*
     * don't calibrate when we're scanning.
     * we are most likely not on our home channel.
     */
    if (! sc->sc_scanning && !atomic_read(&sc->sc_in_reset)
#ifdef __linux__
            && ATH_RESET_SERIALIZE_TRYLOCK(sc)
#endif
            ) {
        int8_t rerun_cals = 0;

#ifdef __linux__
                reset_serialize_locked = 1;
#endif

        /* Background scanning may cause calibration data to be lost. Query the HAL to see
         * if calibration data is invalid. If so, reschedule all periodic cals to run
         */
        ATH_PS_WAKEUP(sc);
        if (ath_hal_get_cal_intervals(ah, &sc->sc_cals, HAL_QUERY_RERUN_CALS)) {
            rerun_cals = 1;
            for (i=0; i<sc->sc_num_cals; i++) {
                HAL_CALIBRATION_TIMER *calp = &sc->sc_cals[i];
                sc->sc_sched_cals |= calp->cal_timer_group;
            }
        }
        ATH_PS_SLEEP(sc);

        /*
         * Long calibration runs independently of short calibration.
         */
        if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_longcal_timer) >= ATH_LONG_CALINTERVAL) {
            longcal = AH_TRUE;
            sc->sc_longcal_timer = timestamp;
        }

        /*
         * Short calibration applies only while (sc->sc_Caldone == AH_FALSE)
         */
        if (! sc->sc_Caldone) {
            if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_shortcal_timer) >= ATH_SHORT_CALINTERVAL) {
                shortcal = AH_TRUE;
                sc->sc_shortcal_timer = timestamp;
            }
        } else {
            for (i=0; i<sc->sc_num_cals; i++) {
                cal_timerp = &sc->sc_cals[i];

                if (rerun_cals ||
                    CONVERT_SYSTEM_TIME_TO_MS(timestamp - cal_timerp->cal_timer_ts) >= cal_timerp->cal_timer_interval) {
                    ath_hal_reset_calvalid(ah, &sc->sc_curchan, &sc->sc_Caldone, cal_timerp->cal_timer_group);
                    if (sc->sc_Caldone == AH_TRUE) {
                        cal_timerp->cal_timer_ts = timestamp;
                    } else {
                        sc->sc_sched_cals |= cal_timerp->cal_timer_group;
                        multical = AH_TRUE;
                    }
                }
            }
        }

        /*
         * Verify whether we must check ANI
         */
        if (ani_interval &&
            CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_ani_timer) >= ani_interval) {
            aniflag = AH_TRUE;
            sc->sc_ani_timer = timestamp;
        }


        if (ath_hal_isOpenLoopPwrCntrl(ah)) {
            if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_olpc_timer) >= ATH_OLPC_CALINTERVAL) {
                olpccal = AH_TRUE;
                sc->sc_olpc_timer = timestamp;
            }
        }

        /*
         * Decide whether there's any work to do
         */
        work_to_do = longcal || shortcal || aniflag || multical || olpccal || crdccal;
    }

    /*
     * Skip all processing if there's nothing to do.
     */
    if (work_to_do) {
        ATH_PS_WAKEUP(sc);

        /* Call ANI routine if necessary */
        if (aniflag) {
            HAL_ANISTATS ani_stats;
            ath_hal_rxmonitor(ah, &sc->sc_halstats, &sc->sc_curchan, &ani_stats);
#if ATH_SUPPORT_VOW_DCS
            /* enable dcsim only for  AP and 11NA mode */
#if WLAN_SPECTRAL_ENABLE
            if (((sc->sc_dcs_enabled & ATH_CAP_DCS_WLANIM) || (sc->sc_icm_active))
#else
            if ((sc->sc_dcs_enabled & ATH_CAP_DCS_WLANIM)
#endif /* WLAN_SPECTRAL_ENABLE */
                &&  (sc->sc_opmode == HAL_M_HOSTAP) &&
                    ((sc->sc_curmode == WIRELESS_MODE_11NA_HT20) ||
                     (sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS) ||
                     (sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS))){
#if ATH_SUPPORT_DFS
                if (!(sc->sc_dfs_wait))
#endif
                {
                    if (ath_hal_getcapability(ah, HAL_CAP_DCS_SUPPORT, 0, NULL) == HAL_OK ) {
                        ath_sc_chk_intr(sc, &ani_stats);
                    }
                }
            }
#endif
        }


        if (olpccal) {
            ath_hal_olpcTempCompensation(ah);
        }

        /* Perform calibration if necessary */
        if (longcal || shortcal || multical) {
            HAL_BOOL    isCaldone = AH_FALSE;

#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_per_cal_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_per_cal++;
#endif

            if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
                /*
                 * Rfgain is out of bounds, reset the chip
                 * to load new gain values.
                 */
                DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: RFgain out of bounds, resetting device.\n", __func__);
#ifdef QCA_SUPPORT_CP_STATS
                pdev_lmac_cp_stats_ast_per_rfgain_inc(sc->sc_pdev, 1);
#else
                sc->sc_stats.ast_per_rfgain++;
#endif
                sc->sc_curchan.paprd_done = 0;
                skip_paprd_cal = 1;
                ath_internal_reset(sc);
            }

            sched_cals = sc->sc_sched_cals;
            if (ath_hal_calibrate(ah, &sc->sc_curchan, sc->sc_rx_chainmask, longcal, &isCaldone, sc->sc_scanning, &sc->sc_sched_cals)) {
                for (i=0; i<sc->sc_num_cals; i++) {
                    cal_timerp = &sc->sc_cals[i];
                    if (cal_timerp->cal_timer_group & sched_cals) {
                        if (sched_cals ^ sc->sc_sched_cals) {
                            cal_timerp->cal_timer_ts = timestamp;
                        }
                    }
                }

                /*
                 * XXX Need to investigate why ath_hal_getChanNoise is
                 * returning a value that seems to be incorrect (-66dBm on
                 * the last test using CB72).
                 */
                if (longcal) {
                    noise_to_report = sc->sc_noise_floor =
                        ah->ah_get_chan_noise(ah, &sc->sc_curchan);
                }
                /* don't check calibaration done since iq calibration might fail at
                 * clear air
                 */
                if (sc->sc_paprd_enable) {
                    if (ath_hal_is_skip_paprd_by_greentx(ah)) {
                        /* reset paprd related parameters */
                        sc->sc_paprd_lastchan_num = 0;
                        sc->sc_curchan.paprd_done  = 0;
                        sc->sc_paprd_done_chain_mask = 0;
                        ath_hal_paprd_enable(sc->sc_ah, AH_FALSE,
                            &sc->sc_curchan);
                        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
                           "disable paprd by green tx.\n", __func__, __LINE__);
                    } else {
                        int retval;
                        if (!sc->sc_scanning &&
                            (sc->sc_curchan.paprd_done == 0) && !skip_paprd_cal
                            && !sc->sc_paprd_cal_started)
                        {
                            sc->sc_paprd_retrain_count = 0;
                            sc->sc_paprd_cal_started = 1;
                            retval = ath_paprd_cal(sc);
                            DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                               "%s[%d]: ath_paprd_cal retval %d \n",
                               __func__, __LINE__, retval);
                            if (sc->sc_curchan.paprd_done == 0) {
                                ath_hal_paprd_dec_tx_pwr(sc->sc_ah);
                            }
                        }
                        if (!sc->sc_scanning &&
                            (sc->sc_curchan.paprd_done == 1) &&
                            (sc->sc_curchan.paprd_table_write_done == 0))
                        {
                            retval = ath_apply_paprd_table(sc);
                            DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                                "%s[%d]: ath_apply_paprd_table retval %d \n",
                                __func__, __LINE__, retval);
                        }
                        DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                            "%s[%d]: paprd_enable %d paprd_done %d Apply %d\n",
                            __func__, __LINE__,
                            sc->sc_paprd_enable, sc->sc_curchan.paprd_done,
                            sc->sc_curchan.paprd_table_write_done);
                    }
                }
                DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                    "%d.%03d | %s: channel %u/%x noise_floor=%d\n",
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) % 1000,
                    __func__,
                    sc->sc_curchan.channel,
                    sc->sc_curchan.channel_flags,
                    sc->sc_noise_floor);
            } else {
                DPRINTF(sc, ATH_DEBUG_ANY,
                    "%d.%03d | %s: calibration of channel %u/%x failed\n",
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) % 1000,
                    __func__,
                    sc->sc_curchan.channel,
                    sc->sc_curchan.channel_flags);
#ifdef QCA_SUPPORT_CP_STATS
                pdev_lmac_cp_stats_ast_per_calfail_inc(sc->sc_pdev, 1);
#else
                sc->sc_stats.ast_per_calfail++;
#endif
            }
            sc->sc_Caldone = isCaldone;
            /* Check if the DCS is being handled by ICM */
            if (sc->sc_curchan.channel_flags & CHANNEL_CW_INT) {
#if WLAN_SPECTRAL_ENABLE
                if (sc->sc_icm_active) {
                    /* ICM is active, let it handle the channel change */
                    DPRINTF(sc, ATH_DEBUG_ANY,
                        "%s: Found CW interference. Sending message to application layer\n", __func__);
                    spectral_send_intf_found_msg((struct ath_spectral*)sc->sc_spectral,
                                           1,
                                           sc->sc_dcs_enabled);
                } else
#endif  /* WLAN_SPECTRAL_ENABLE */
                {
                    /* Channel change to be handled by the driver if enabled */
                if( sc->sc_dcs_enabled & ATH_CAP_DCS_CWIM) {
                    /* Channel change for CW interference has been enabled, change channels */
                    printk("Too much interference, Trying to change channels\n");
                    /* Try changing channels */
                    sc->sc_ieee_ops->cw_interference_handler(sc->sc_ieee);
                }
                }
                /* Clear the flags flag */
                sc->sc_curchan.channel_flags &= (~CHANNEL_CW_INT);
            }
        }
    }
    /*using flag work_to_do to make sure we follow same WAKE and SLEEP sequence.*/
    if(!work_to_do)
        ATH_PS_WAKEUP(sc);

    if(!noise_to_report && !atomic_read(&sc->sc_in_reset)) {
    /* we are using this variable for exactly opposite scenarios ,
       trying to make sure if somehow noise is not calculated till not
       we should do it.
     */
        noise_to_report = ath_hal_get_noisefloor(ah);
    }

    ATH_PS_SLEEP(sc);
    if(sc->sc_enable_noise_detection) {
        if(noise_to_report > sc->sc_noise_floor_th)
        {
            sc->sc_noise_floor_report_iter++;
    }
        sc->sc_noise_floor_total_iter++;
        sc->sc_Caldone = 0;/*Enabling short cal */
    }

#ifdef __linux__
    if (reset_serialize_locked)
        ATH_RESET_SERIALIZE_UNLOCK(sc);
#endif

    /*
     * Set timer interval based on previous results.
     * The interval must be the shortest necessary to satisfy ANI,
     * short calibration and long calibration.
     *
     */
    ath_set_timer_period(&sc->sc_cal_ch,
                         ath_get_cal_interval(sc->sc_Caldone, ani_interval,
                            ath_hal_isOpenLoopPwrCntrl(ah), ath_hal_is_crdc(ah),
                            sc->sc_min_cal));
    return 0;  /* Re-arm */
}

#if ATH_LOW_PRIORITY_CALIBRATE
static int
ath_calibrate_defer(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;

    ath_schedule_defer_call(&sc->sc_calibrate_deferred);
    return 0;
    //return ath_cal_workitem(sc, ath_calibrate, arg);
}
#endif

#if ATH_TRAFFIC_FAST_RECOVER
/* EV #75248 */
static int
ath_traffic_fast_recover(void *arg)
{
    struct ath_softc            *sc = (struct ath_softc *)arg;
    struct ath_hal              *ah;
    u_int32_t rx_packets;
    u_int32_t pll3_sqsum_dvc;

    static u_int32_t last_rx_packets = 0;

    rx_packets = sc->sc_stats.ast_11n_stats.rx_pkts;

    ah = sc->sc_ah;

    if (rx_packets - last_rx_packets <= 5) {
        pll3_sqsum_dvc = ath_hal_get_pll3_sqsum_dvc(ah);

        if (pll3_sqsum_dvc >= 0x40000) {
            ath_internal_reset(sc);
        }
    }

    last_rx_packets = rx_packets;

    return 0;
}
#endif
#ifdef ATH_SUPPORT_TxBF

/* this timer routine is used to reset lowest tx rate for self-generated frame*/
static int
ath_sc_reset_lowest_txrate(void *arg)
{
    struct ath_softc        *sc = (struct ath_softc *)arg;
    struct ath_hal          *ah = sc->sc_ah;

    /* reset rate to maximum tx rate*/
    ath_hal_resetlowesttxrate(ah);

    /* restart timer*/
    ath_set_timer_period(&sc->sc_selfgen_timer, LOWEST_RATE_RESET_PERIOD);
    return 0;
}
#endif

/******************************************************************************/
/*!
**  \brief Start Scan
**
**  This function is called when starting a channel scan.  It will perform
**  power save wakeup processing, set the filter for the scan, and get the
**  chip ready to send broadcast packets out during the scan.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_scan_start(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;
    u_int32_t           rfilt;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);
    sc->sc_scanning = 1;
    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);
    ath_hal_setassocid(ah, ath_bcast_mac, 0);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex) {
        u_int32_t   scanState = ATH_COEX_WLAN_SCAN_START;
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
    }
#endif

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);

    DPRINTF_TIMESTAMP(sc, ATH_DEBUG_STATE,
        "%s: RX filter 0x%x aid 0\n", __func__, rfilt);
}

/******************************************************************************/
/*!
**  \brief Scan End
**
**  This routine is called by the upper layer when the scan is completed.  This
**  will set the filters back to normal operating mode, set the BSSID to the
**  correct value, and restore the power save state.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_scan_end(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;
    u_int32_t           rfilt;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);
    sc->sc_scanning = 0;

    /*
     * Request for a full reset due to rx packet filter changes.
     */
#if ATH_SUPPORT_MCI
    if (!ath_hal_hasmci(sc->sc_ah)) {
        /*
         * When MCI is enabled, every full reset needs to go through a GPM
         * exchange sequence with BT. Try to reduce the number of full
         * reset as much as possible. From hardware point of view, this
         * full reset is not necessary.
         */
        sc->sc_full_reset = 1;
    }
#else
    sc->sc_full_reset = 1;
#endif

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);
    ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex) {
        u_int32_t   scanState = ATH_COEX_WLAN_SCAN_END;
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
    }
#endif

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);

    DPRINTF_TIMESTAMP(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x aid 0x%x\n",
        __func__, rfilt, sc->sc_curaid);
}
void
ath_scan_enable_txq(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int i =0;
    struct ath_txq *txq = NULL;
    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;
    for(i=0; i < HAL_NUM_TX_QUEUES; i++) {
        txq = &sc->sc_txq[i];
        ATH_TXQ_LOCK(txq);
        if( (ath_hal_numtxpending(sc->sc_ah, i)) == 0)
            ath_txq_schedule(sc, txq);
        ATH_TXQ_UNLOCK(txq);
    }
    ATH_FUNC_ENTRY_VOID(sc);
}

/******************************************************************************/
/*!
**  \brief Set Channel
**
**  This is the entry point from the protocol level to chang the current
**  channel the AP is serving.
**
**  \param dev Void pointer to ATH object passed from the protocol layer
**  \param hchan Pointer to channel structure with new channel information
**  \param tx_timeout Time in us to wait for transmit DMA to stop (0 - use default timeout)
**  \param rx_timeout Time in us to wait for receive DMA to stop (0 - use default timeout)
**  \param no_flush If true, packets in the hw queue shall be requeued into sw queues and will be transmitted later
**  \param ignore_dfs if true, do not enable radar detection in the channel even if it is a DFS channel
**
**  \return N/A
*/
#ifdef PRINT_CHAN_CHANGE
#include "asm/mipsregs.h"
#endif
void
ath_set_channel(ath_dev_t dev, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool no_flush, bool ignore_dfs)
{
#ifdef PRINT_CHAN_CHANGE
    unsigned long ts1;
    unsigned long ts2;
#endif
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;

#ifdef PRINT_CHAN_CHANGE
    printk("====================================================\n");
    printk(" Chan %u(%08x) - %u(%08x) \n", sc->sc_curchan.channel,
                                            sc->sc_curchan.channel_flags,
                                            hchan->channel,
                                            hchan->channel_flags);
    ts1 = read_c0_count();
#endif
    ATH_FUNC_ENTRY_VOID(sc);

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    (void) ath_chan_set(sc, hchan, tx_timeout, rx_timeout, no_flush, ignore_dfs);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
#ifdef PRINT_CHAN_CHANGE
    ts2 = read_c0_count();
    printk("dur: %lu\n", ts2 - ts1);
    printk("====================================================\n");
#endif
}
#if ATH_SUPPORT_WIFIPOS
/******************************************************************************/
/*!
**  \brief fast Set Channel
**
**  This is the entry point from the protocol level to chang the current
**  channel the AP is serving.
**
**  \param dev Void pointer to ATH object passed from the protocol layer
**  \param hchan Pointer to channel structure with new channel information
**  \param tx_timeout Time in us to wait for transmit DMA to stop (0 - use default timeout)
**  \param rx_timeout Time in us to wait for receive DMA to stop (0 - use default timeout)
**  \param no_flush If true, packets in the hw queue shall be requeued into sw queues and will be transmitted later
**
**  \return N/A
*/

void
ath_lean_set_channel(ath_dev_t dev, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool no_flush)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;
    ATH_FUNC_ENTRY_VOID(sc);

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    (void) ath_lean_chan_set(sc, hchan, tx_timeout, rx_timeout, no_flush);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}
#endif

/******************************************************************************/
/*!
**  \brief Return the current channel
**
**  \param dev Void pointer to ATH object passed from the protocol layer
**
**  \return pointer to current channel descriptor
*/
static HAL_CHANNEL *
ath_get_channel(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_invalid) return NULL; /* if the device is invalid or removed */
    return &sc->sc_curchan;
}

/******************************************************************************/
/*!
**  \brief Get MAC address
**
**  Copy the MAC address into the buffer provided
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr Pointer to buffer to place MAC address into
**
**  \return N/A
*/

static void
ath_get_mac_address(ath_dev_t dev, u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    ATH_ADDR_COPY(macaddr, ATH_DEV_TO_SC(dev)->sc_myaddr);
}

static void
ath_get_hw_mac_address(ath_dev_t dev, u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    ATH_ADDR_COPY(macaddr, ATH_DEV_TO_SC(dev)->sc_my_hwaddr);
}

/******************************************************************************/
/*!
**  \brief set MAC address
**
**  Copies the provided MAC address into the proper ATH member, and sets the
**  value into the chipset through the HAL.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr buffer containing MAC address to set into hardware
**
**  \return N/A
*/

static void
ath_set_mac_address(ath_dev_t dev, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    OS_MEMCPY(sc->sc_myaddr, macaddr, IEEE80211_ADDR_LEN);
    ath_hal_setmac(sc->sc_ah, sc->sc_myaddr);
}

static void
ath_set_rxfilter(ath_dev_t dev) {
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t        rxfilter;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    rxfilter = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(sc->sc_ah, rxfilter);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}

static u_int32_t
ath_get_rxfilter(ath_dev_t dev) {
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return ath_hal_getrxfilter(sc->sc_ah);
}

static void ath_set_bssid_mask(ath_dev_t dev, const u_int8_t bssid_mask[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    OS_MEMCPY(sc->sc_bssidmask, bssid_mask, IEEE80211_ADDR_LEN);
    ath_hal_setbssidmask(sc->sc_ah, sc->sc_bssidmask);
}

static int
ath_set_sel_evm(ath_dev_t dev, int selEvm, int justQuery) {
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int old_value;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    if (!justQuery) ATH_PS_WAKEUP(sc);

    old_value = ath_hal_setrxselevm(sc->sc_ah, selEvm, justQuery);

    /*
     * Restore previous power management state.
     */
    if (!justQuery) ATH_PS_SLEEP(sc);

    return old_value;
}

#if ATH_SUPPORT_WIRESHARK
static int
ath_monitor_filter_phyerr(ath_dev_t dev, int filterPhyErr, int justQuery)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int old_val = sc->sc_wireshark.filter_phy_err;
    if (!justQuery) {
        sc->sc_wireshark.filter_phy_err = filterPhyErr != 0;
    }
    return old_val;
}
#endif /* ATH_SUPPORT_WIRESHARK */

/******************************************************************************/
/*!
**  \brief set multi-cast addresses
**
**  Copies the provided MAC address into the proper ATH member, and sets the
**  value into the chipset through the HAL.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr buffer containing MAC address to set into hardware
**
**  \return N/A
*/
static void
ath_set_mcastlist(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t mfilt[2];

    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;

    ATH_FUNC_ENTRY_VOID(sc);

    /* calculate and install multicast filter */
    if (sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee) & ATH_NETIF_ALLMULTI) {
        mfilt[0] = mfilt[1] = ~0;
    }
    else {
        sc->sc_ieee_ops->netif_mcast_merge(sc->sc_ieee, mfilt);
    }

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Enter Scan State
**
**  This routine will call the LED routine that "blinks" the light indicating
**  the device is in Scan mode. This sets no other status.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_enter_led_scan_state(ath_dev_t dev)
{
    ath_led_scan_start(&(ATH_DEV_TO_SC(dev)->sc_led_control));
}

static u_int32_t
ath_led_state(ath_dev_t dev, u_int32_t bOn)
{
    u_int32_t ret;
    ret = ath_LEDinfo(&(ATH_DEV_TO_SC(dev)->sc_led_control),bOn);
    return ret;
}

static int
ath_radio_info(ath_dev_t dev)
{
    //u_int32_t ret;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_hw_phystate = !ath_get_rfkill(sc);
    //ret = ath_get_hw_phystate(dev);
    return sc->sc_hw_phystate;
}

/******************************************************************************/
/*!
**  \brief Leave Scan State
**
**  Calls the LED Scan End function to change the LED state back to normal.
**  Does not set any other states.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_leave_led_scan_state(ath_dev_t dev)
{
    ath_led_scan_end(&(ATH_DEV_TO_SC(dev)->sc_led_control));
}

#ifdef ATH_USB
static void
ath_heartbeat_callback(ath_dev_t dev)
{
    //struct ath_softc   *sc = ATH_DEV_TO_SC(dev);

}
#endif

/******************************************************************************/
/*!
**  \brief Activate ForcePPM
**
**  Activates/deactivates ForcePPM.
**  Rules for ForcePPM activation depend on the operation mode:
**      -STA:   active when connected.
**      -AP:    active when a single STA is connected to it.
**      -ADHOC: ??
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param enable   Flag indicating whether to enable ForcePPM
**  \param bssid    Pointer to BSSID. Must point to a valid BSSID if flag 'enable' is set.
**
**  \return N/A
*/

static void
ath_notify_force_ppm(ath_dev_t dev, int enable, u_int8_t *bssid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Notify PPM module that we are returning to the home channel; report connection status */
    ath_force_ppm_notify(&sc->sc_ppm_info, enable, bssid);
}
/******************************************************************************/
/*!
**  \Stop all TX as we are DFS Wait State
**
**  This routine will set sc_dfs_wait, that will indicate to not TX anny beacon or data frame while in CAC state.
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param if_id    Devicescape style interface identifier, integer
**  \param flags    Flags indicating device capabilities
**
**  \return -EINVAL if the ID is invalid
**  \return  0 on success
*/

static int
ath_vap_dfs_wait(ath_dev_t dev, int if_id)
{
#ifdef ATH_SUPPORT_DFS
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_dfs_wait = 1;
    /*
    **  During CAC use CAC work around related value for PRSSI
    **  for pre-Peregrine chips
    */
    ath_hal_dfs_cac_war (sc->sc_ah, 1);
#endif
    return 0;
}

/******************************************************************************/
/*!
**  \brief Down VAP instance
**
**  This routine will stop the indicated VAP and put it in a "down" state.
**  The down state is basically an initialization state that can be brought
**  back up by callint the opposite up routine.
**  This routine will bring the interface out of power save mode, set the
**  LED states, update the rate control processing, stop DMA transfers, and
**  set the VAP into the down state.
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param if_id    Devicescape style interface identifier, integer
**  \param flags    Flags indicating device capabilities
**
**  \return -EINVAL if the ID is invalid
**  \return  0 on success
*/

static int
ath_vap_stopping(ath_dev_t dev, int if_id)
{
#define WAIT_BEACON_INTERVAL 100
    u_int32_t elapsed_time = 0;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;

    avp = sc->sc_vaps[if_id];

    ath_dfs_clear_cac(sc);
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }

    /* if vap was up, mark it down and update the vap type counters */
    if (avp->av_up) {
        u_int32_t rfilt = 0;
        if (avp->av_opmode == HAL_M_STA) {
            /* use atomic variable */
            atomic_dec(&sc->sc_nsta_vaps_up);
        } else if (avp->av_opmode == HAL_M_HOSTAP) {
            atomic_dec(&sc->sc_nap_vaps_up);
        }
        /* recalculate the rx filter */
        rfilt = ath_calcrxfilter(sc);
        ath_hal_setrxfilter(ah, rfilt);
        avp->av_up=0;
    }

    if(avp->av_opmode != HAL_M_MONITOR) {
        ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_VAP_DOWN,if_id);
    }

    atomic_set(&avp->av_stop_beacon, 1);

    /*
     * Wait until ath_beacon_generate complete
     */
    do {
        if (!atomic_read(&avp->av_beacon_cabq_use_cnt))
            break;

        OS_DELAY(WAIT_BEACON_INTERVAL);
        elapsed_time += WAIT_BEACON_INTERVAL;

        if (elapsed_time > (1000 * WAIT_BEACON_INTERVAL)) {
           DPRINTF(sc, ATH_DEBUG_ANY,"%s: Rx beacon_cabq_use_count stuck. Investigate!!!\n", __func__);
        }
    } while (1);

    /*
     * Reclaim any pending mcast bufs on the vap.
     * so that vap bss node ref could be decremented
     * and vap could be deleted.
     */
    ath_tx_mcast_draintxq(sc, &avp->av_mcastq);
#if UMAC_SUPPORT_WNM
    if (sc->sc_ieee_ops->wnm_fms_enabled(sc->sc_ieee, if_id))
    {
        int i;
        for (i = 0; i < ATH_MAX_FMS_QUEUES; i++)
        {
            DPRINTF(sc, ATH_DEBUG_WNM_FMS, "%s : Draining FMS queue %d\n", __func__, i);
            ath_tx_mcast_draintxq(sc, &avp->av_fmsq[i]);
        }
    }
#endif /* UMAC_SUPPORT_WNM */
    /* Reclaim beacon resources */
    if (avp->av_opmode == HAL_M_HOSTAP || avp->av_opmode == HAL_M_IBSS) {
        /* For now, there is only one beacon source. Just stop beacon dma */
        if (!sc->sc_removed)
        {
            ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);
            /* There may be some packets in CABQ waiting for next beacon, drain them */
            if (ath_hal_stoptxdma_indvq(sc->sc_ah, sc->sc_cabq->axq_qnum, 0))
            {
                ath_tx_draintxq(sc, sc->sc_cabq, AH_FALSE);
            } else
            {
                /*Failed to stop cabq. Try stopping all queue */
                printk("%s:%d  Unable to stop CABQ. Now Stopping all Queues \n", __func__, __LINE__);
                ath_draintxq(sc, AH_FALSE, 0);
            }
        }

        ath_beacon_return(sc, avp);
#if UMAC_SUPPORT_WNM
         if(sc->sc_ieee_ops->timbcast_enabled(sc->sc_ieee, if_id)) {
              ath_timbcast_return(sc, avp);
         }
#endif
    }

    atomic_set(&avp->av_stop_beacon, 0);

    return 0;
#undef WAIT_BEACON_INTERVAL
}

static int
ath_vap_down(ath_dev_t dev, int if_id, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;
    u_int32_t imask = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }
    ath_dfs_clear_cac(sc);

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);

    /* handle vap counter cleanup if is called without stopping */
    if (avp->av_up) {
        if (avp->av_opmode == HAL_M_STA) {
            /* use atomic variable */
            atomic_dec(&sc->sc_nsta_vaps_up);
        } else if (avp->av_opmode == HAL_M_HOSTAP) {
            atomic_dec(&sc->sc_nap_vaps_up);
        }
        /* recalculate the rx filter */
        rfilt = ath_calcrxfilter(sc);
        ath_hal_setrxfilter(ah, rfilt);
        avp->av_up=0;
    }


#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv)
        ath_slow_ant_div_stop(&sc->sc_antdiv);
#endif

#if ATH_ANT_DIV_COMB
    if (sc->sc_smart_antenna)
        ath_smartant_stop(sc->sc_sascan);
#endif
    /* set LEDs to INIT state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_INIT);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);

    if (flags & ATH_IF_HW_OFF) {
        /*Check if HW IF Stop in Progress*/
        if(atomic_dec_and_test(&sc->sc_hwif_stop_refcnt)) {
            HAL_BOOL gpio_intr = AH_FALSE;

            sc->sc_full_reset = 1;
            ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

            if (ath_timer_is_initialized(&sc->sc_up_beacon)) {
                ath_cancel_timer(&sc->sc_up_beacon, CANCEL_NO_SLEEP); /* beacon config timer */
                sc->sc_up_beacon_purge = 0;
            }

            sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_BRSSI);
#if ATH_TRAFFIC_FAST_RECOVER
            if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
                ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
            }
#endif
            /*
             * Disable interrupts other than GPIO to capture RfKill event.
             */
#ifdef ATH_RFKILL
            if (sc->sc_hasrfkill && ath_rfkill_hasint(sc))
                gpio_intr = AH_TRUE;
#endif

            if (gpio_intr) {
                ath_hal_intrset(ah, HAL_INT_GLOBAL | HAL_INT_GPIO);
            }
            else {
                ath_hal_intrset(ah, sc->sc_imask &~ HAL_INT_GLOBAL);
            }

            imask = ath_hal_intrget(ah);
            if (imask & HAL_INT_GLOBAL)
                ath_hal_intrset(ah, 0);
            ath_draintxq(sc, AH_FALSE, 0);  /* stop xmit side */
            if (imask & HAL_INT_GLOBAL)
                ath_hal_intrset(ah, imask);
        }
        atomic_inc(&sc->sc_hwif_stop_refcnt);
    }

    ATH_PS_SLEEP(sc);
    return 0;
}

/******************************************************************************/
/*!
**  \brief VAP in Listen mode
**
**  This routine brings the VAP out of the down state into a "listen" state
**  where it waits for association requests.  This is used in AP and AdHoc
**  modes.
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param if_id    Devicescape style interface identifier, integer
**
**  \return -EINVAL if interface ID is not valid
**  \return  0 on success
*/

static int
ath_vap_listen(ath_dev_t dev, int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }
    ath_dfs_clear_cac(sc);
    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);


    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv)
        ath_slow_ant_div_stop(&sc->sc_antdiv);
#endif

    /* set LEDs to READY state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_READY);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    if (sc->sc_opmode == HAL_M_STA || sc->sc_opmode == HAL_M_IBSS) {
        ath_hal_setassocid(ah, ath_bcast_mac, sc->sc_curaid);
    } else
        sc->sc_curaid = 0;

    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    /*
     * XXXX
     * Disable BMISS and BRSSI interrupt when we're not associated
     */
    ath_hal_intrset(ah,sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_BRSSI));
    sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_BRSSI);

    avp->av_dfswait_run=0; /* reset the dfs wait flag */

    ATH_PS_SLEEP(sc);
    return 0;
}

/******************************************************************************/
/*!
**  \brief xxxxxe
**
**  -- Enter Detailed Description --
**
**  \param xx xxxxxxxxxxxxx
**  \param xx xxxxxxxxxxxxx
**
**  \return xxx
*/

static int
ath_vap_join(ath_dev_t dev, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }
    ath_dfs_clear_cac(sc);
    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);

    if (!(flags & ATH_IF_HW_ON)) {
        return 0;
    }

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);


    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    /* set LEDs to READY state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_READY);

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    /*
     * set the bssid and aid values in the following 2 cases.
     * 1) no active stattion vaps and this is the first vap coming up.
     * 2) there is only one station vap and it is rejoining .
     * the case where a second STA vap is coming, do not touch the
     * bssid,aid registers. leave them to the existing  active STA vap.
     */
    if (atomic_read(&sc->sc_nsta_vaps_up) == 0 ||
         ( atomic_read(&sc->sc_nsta_vaps_up) == 1 &&  avp->av_opmode ==  HAL_M_STA &&
           avp->av_up ) ) {
        ATH_ADDR_COPY(sc->sc_curbssid, bssid);
        sc->sc_curaid = 0;
        ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);
    }

    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    /*
     * Update tx/rx chainmask. For legacy association, hard code chainmask to 1x1,
     * for 11n association, use the chainmask configuration.
     */
    sc->sc_update_chainmask = 1;
    if (flags & ATH_IF_HT) {
        sc->sc_tx_chainmask = sc->sc_config.txchainmask;
        sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    } else {
        sc->sc_tx_chainmask = sc->sc_config.txchainmasklegacy;
        sc->sc_rx_chainmask = sc->sc_config.rxchainmasklegacy;
    }
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    /*
     * Enable rx chain mask detection if configured to do so
     */
    if (sc->sc_reg_parm.rxChainDetect)
        sc->sc_rx_chainmask_detect = 1;
    else
        sc->sc_rx_chainmask_detect = 0;

    /*
    ** Set aggregation protection mode parameters
    */

    sc->sc_config.ath_aggr_prot = sc->sc_reg_parm.aggrProtEnable;
    sc->sc_config.ath_aggr_prot_duration = sc->sc_reg_parm.aggrProtDuration;
    sc->sc_config.ath_aggr_prot_max = sc->sc_reg_parm.aggrProtMax;

    /*
     * Reset our TSF so that its value is lower than the beacon that we are
     * trying to catch. Only then hw will update its TSF register with the
     * new beacon. Reset the TSF before setting the BSSID to avoid allowing
     * in any frames that would update our TSF only to have us clear it
     * immediately thereafter.
     */
    ath_hal_resettsf(ah);

    /* Reset average beacon RSSI computed by hardware when joining a new AP */
    if (ath_hal_hashwbeaconprocsupport(ah) && !sc->sc_config.swBeaconProcess) {
        ath_hw_beacon_rssi_reset(sc);
    }

    /*
     * XXXX
     * Disable BMISS and BRSSI interrupt when we're not associated
     */
    ath_hal_intrset(ah,sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_BRSSI));
    sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_BRSSI);

    avp->av_dfswait_run=0; /* reset the dfs wait flag */

#if ATH_ANT_DIV_COMB
    if (sc->sc_smart_antenna)
        ath_smartant_get_apconf(sc->sc_sascan, bssid);
#endif

    ATH_PS_SLEEP(sc);
    return 0;
}

static int
ath_vap_up(ath_dev_t dev, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN],
           u_int8_t aid, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;
    int i, error = 0;
    systime_t timestamp = OS_GET_TIMESTAMP();

    ASSERT(if_id != ATH_IF_ID_ANY);
    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }
    ath_dfs_clear_cac(sc);

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);


    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    /* set LEDs to RUN state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_RUN);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 1);


    /*
     * set the bssid and aid values in the following 2 cases.
     * 1) no active stattion vaps and this is the first vap coming up.
     * 2) IBSS vap is coming up.
     * 3) there is only one station vap and it is rejoining .
     * the case where a second STA vap is coming, do not touch the
     * bssid,aid registers. leave them to the existing  active STA vap.
     */
    if (atomic_read(&sc->sc_nsta_vaps_up) == 0 ||  avp->av_opmode == HAL_M_IBSS  ||
         ( atomic_read(&sc->sc_nsta_vaps_up) == 1 &&  avp->av_opmode ==  HAL_M_STA &&
           avp->av_up ) ) {
        ATH_ADDR_COPY(sc->sc_curbssid, bssid);
        sc->sc_curaid = aid;
        ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);
    }

#if !ATH_SUPPORT_WRAP
    if ((avp->av_opmode != HAL_M_STA) && (flags & ATH_IF_PRIVACY)) {
        for (i = 0; i < IEEE80211_WEP_NKID; i++)
            if (ath_hal_keyisvalid(ah, (u_int16_t)i))
                ath_hal_keysetmac(ah, (u_int16_t)i, bssid);
    }
#endif

    switch (avp->av_opmode) {
    case HAL_M_HOSTAP:
    case HAL_M_IBSS:
        /*
         * Allocate and setup the beacon frame.
         *
         * Stop any previous beacon DMA.  This may be
         * necessary, for example, when an ibss merge
         * causes reconfiguration; there will be a state
         * transition from RUN->RUN that means we may
         * be called with beacon transmission active.
         */
        ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);
#if UMAC_SUPPORT_WNM
         if(sc->sc_ieee_ops->timbcast_enabled(sc->sc_ieee, if_id)) {
             error = ath_timbcast_alloc(sc, if_id);
             if (error != 0)
                 goto bad;
         }
#endif
        error = ath_beacon_alloc(sc, if_id);
        if (error != 0)
            goto bad;

#ifdef ATH_BT_COEX
        if (sc->sc_hasbtcoex) {
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_CONNECT, NULL);
        }
#endif

        if(avp->av_opmode == HAL_M_IBSS)
        {
            /*For HAL_M_IBSS mode, ath_vap_down() (/iawDown()) will be called
            in mlmeProcessJointFrame(), and interrupt will be disable. So re-enable
            the interrupt here */

            ath_hal_intrset(ah, sc->sc_imask | HAL_INT_GLOBAL);
        }
        break;

    case HAL_M_STA:
        /*
         * Record negotiated dynamic turbo state for
         * use by rate control modules.
         */
        sc->sc_dturbo = (flags & ATH_IF_DTURBO) ? 1 : 0;

        /*
         * start rx chain mask detection if it is enabled.
         * Use the default chainmask as starting point.
         */
        if (sc->sc_rx_chainmask_detect) {
            if (flags & ATH_IF_HT) {
                sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
            } else {
                sc->sc_rx_chainmask = sc->sc_config.rxchainmasklegacy;
            }
            sc->sc_rx_chainmask_start = 1;
            sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
        }

#if ATH_SLOW_ANT_DIV
        /* Start slow antenna diversity */
        if (sc->sc_slowAntDiv)
        {
            u_int32_t num_antcfg;

            if (sc->sc_curchan.channel_flags & CHANNEL_5GHZ)
                ath_hal_getcapability(ah, HAL_CAP_ANT_CFG_5GHZ, 0, &num_antcfg);
            else
                ath_hal_getcapability(ah, HAL_CAP_ANT_CFG_2GHZ, 0, &num_antcfg);

            if (num_antcfg > 1)
                ath_slow_ant_div_start(&sc->sc_antdiv, num_antcfg, bssid);
        }
#endif
#ifdef ATH_BT_COEX
        if ((sc->sc_hasbtcoex) && (sc->sc_btinfo.bt_coexAgent == 0)) {
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_CONNECT, NULL);
        }
#endif

        if (ath_timer_is_initialized(&sc->sc_up_beacon) && !ath_timer_is_active(&sc->sc_up_beacon)) {
            ath_start_timer(&sc->sc_up_beacon);
            sc->sc_up_beacon_purge = 1;
        }

#if ATH_ANT_DIV_COMB
        if (sc->sc_smart_antenna)
            ath_smartant_start(sc->sc_sascan, bssid);
#endif
        break;

    default:
        break;
    }

    if (avp->av_up == 0) {
       if (avp->av_opmode == HAL_M_STA) {
           /* use atomic variable */
           atomic_inc(&sc->sc_nsta_vaps_up);
       } else if (avp->av_opmode == HAL_M_HOSTAP) {
           atomic_inc(&sc->sc_nap_vaps_up);
       }
    }

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    /* Moved beacon_config after dfs_wait check
     * so that ath_beacon_config won't be called duing dfswait
     * period - this will fix the beacon stuck afer DFS
     * CAC period issue
     */
    /*
     * Configure the beacon and sleep timers.
     * multiple AP VAPs case we  need to reconfigure beacon for
     * vaps differently for each vap and chip mode combinations.
     * AP + STA vap with chip mode STA.
     */
    if(avp->av_opmode != HAL_M_MONITOR) {
        ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_VAP_UP,if_id);
    }
     avp->av_up=1;
    /*
     * Reset rssi stats; maybe not the best place...
     */
    if (flags & ATH_IF_HW_ON) {
        sc->sc_halstats.ns_avgbrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgtxrate = ATH_RATE_DUMMY_MARKER;
    }

    /* start periodic recalibration timer */
    sc->sc_longcal_timer  = timestamp;
    sc->sc_shortcal_timer = timestamp;
    sc->sc_ani_timer      = timestamp;
    for (i=0; i<sc->sc_num_cals; i++) {
        HAL_CALIBRATION_TIMER *cal_timerp = &sc->sc_cals[i];
        cal_timerp->cal_timer_ts = timestamp;
        sc->sc_sched_cals |= cal_timerp->cal_timer_group;
    }
    if (!ath_timer_is_active(&sc->sc_cal_ch)) {
        u_int32_t ani_interval;
        if (!ath_hal_enabledANI(ah) ||
            !ath_hal_getanipollinterval(ah, &ani_interval)) {
            ani_interval = 0;
        }

        ath_set_timer_period(&sc->sc_cal_ch,
                             ath_get_cal_interval(sc->sc_Caldone, ani_interval,
                                ath_hal_isOpenLoopPwrCntrl(ah),
                                ath_hal_is_crdc(ah),
                                sc->sc_min_cal));
        ath_start_timer(&sc->sc_cal_ch);
    }

    if (!ath_timer_is_active(&sc->sc_chan_busy)) {
        ath_set_timer_period(&sc->sc_chan_busy, ATH_CHAN_BUSY_INTVAL);
        ath_start_timer(&sc->sc_chan_busy);
     }

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer) &&
        !ath_timer_is_active(&sc->sc_traffic_fast_recover_timer)) {
        ath_start_timer(&sc->sc_traffic_fast_recover_timer);
    }
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
    if (sc->sc_osnetif_flowcntrl) {
        /* do not pause the ingress here */
        ath_netif_wake_queue(sc);
    }
#endif

bad:
    ATH_PS_SLEEP(sc);
    return error;
}

/*
************************************
*  PnP routines
************************************
*/

int
ath_stop_locked(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t    imask = 0;

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: invalid %u\n",
            __func__, sc->sc_invalid);

// RNWF    if (dev->flags & IFF_RUNNING) {
    /*
     * Shutdown the hardware and driver:
     *    stop output from above
     *    reset 802.11 state machine
     *    (sends station deassoc/deauth frames)
     *    turn off timers
     *    disable interrupts
     *    clear transmit machinery
     *    clear receive machinery
     *    turn off the radio
     *    reclaim beacon resources
     *
     * Note that some of this work is not possible if the
     * hardware is gone (invalid).
     */

    /* Stop ForcePPM module */
    ath_force_ppm_halt(&sc->sc_ppm_info);


// RNWF upper layers will know to stop themselves
//        netif_stop_queue(dev);    /* XXX re-enabled by ath_newstate */
//        dev->flags &= ~IFF_RUNNING;    /* NB: avoid recursion */
    ATH_PS_WAKEUP(sc);

#if ATH_TX_DUTY_CYCLE
    if (!sc->sc_invalid && sc->sc_tx_dc_enable && !(sc->sc_tx_dc_active_pct)) {
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: clear tx duty cycle\n", __func__);
        ath_set_tx_duty_cycle(sc, 0, false);
    }
#endif

    // Stop LED module.
    // ath_stop_locked can be called multiple times, so we wait until we are
    // actually dettaching from the device (sc->sc_invalid = TRUE) before
    // stopping the LED module.
    if (sc->sc_invalid) {
        ath_led_halt_control(&sc->sc_led_control);
    }

    if (!sc->sc_invalid) {
        if (ah != NULL) {
            if (ah->ah_set_interrupts != NULL)
                ath_hal_intrset(ah, 0);
            else
                printk("%s[%d] : ah_set_interrupts was NULL\n", __func__, __LINE__);

        }
        else {
            printk("%s[%d] : ah was NULL\n", __func__, __LINE__);
        }
    }

    if (!sc->sc_invalid && ah != NULL) {
        imask = ath_hal_intrget(ah);
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, 0);
    }
    ath_draintxq(sc, AH_FALSE, 0);
    if (!sc->sc_invalid && ah != NULL) {
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, imask);
    }

    if (!sc->sc_invalid) {
        ATH_STOPRECV(sc, 0);
        /* Modify for static analysis, prevent ah is NULL */
        if (ah != NULL) {
            ath_hal_phydisable(ah);
        }
    } else {
        ATH_RXBUF_LOCK(sc);
        sc->sc_rxlink = NULL;
        ATH_RXBUF_UNLOCK(sc);
    }
    ATH_PS_SLEEP(sc);
    return 0;
}
static int
ath_open(ath_dev_t dev, HAL_CHANNEL *initial_chan)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    int error = 0;
    u_int32_t rfilt = 0;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
    u_int32_t imask;

    //ATH_LOCK(sc);

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: mode %d\n", __func__, sc->sc_opmode);

    ath_pwrsave_init(sc);

    /*
     * Stop anything previously setup.  This is safe
     * whether this is the first time through or not.
     */
    ath_stop_locked(sc);

#if ATH_CAP_TPC
    ath_hal_setcapability(ah, HAL_CAP_TPC, 0, 1, NULL);
#endif

    /* Initialize chanmask selection */
    sc->sc_tx_chainmask = sc->sc_config.txchainmask;
    sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    /* Start ForcePPM module */
    ath_force_ppm_start(&sc->sc_ppm_info);

    /* Reset SERDES registers */
    ath_hal_configpcipowersave(ah, 0, 0);

    /*
     * The basic interface to setting the hardware in a good
     * state is ``reset''.  On return the hardware is known to
     * be powered up and with interrupts disabled.  This must
     * be followed by initialization of the appropriate bits
     * and then setup of the interrupt mask.
     */
    sc->sc_curchan = *initial_chan;
    sc->sc_curchan.paprd_done = 0;
    sc->sc_curchan.paprd_table_write_done = 0;
    /* EV96307: Reset PAPRD state vars during i/f up */
    sc->sc_paprd_cal_started = 0;
    sc->sc_paprd_lastchan_num = 0;
    sc->sc_paprd_chain_num = 0;

    imask = ath_hal_intrget(ah);
    if (imask & HAL_INT_GLOBAL)
        ath_hal_intrset(ah, 0);

#if ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif

#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_hold_reset);
    ath_reset_wait_tx_rx_finished(sc);
    ATH_RESET_UNLOCK(sc);
#else
    ATH_LOCK_PCI_IRQ(sc);
#endif

#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan, ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning))
    {
        DPRINTF(sc, ATH_DEBUG_RESET,
                "%s: unable to reset hardware; hal status %u "
               "(freq %u flags 0x%x)\n", __func__, status,
               sc->sc_curchan.channel, sc->sc_curchan.channel_flags);
        error = -EIO;
#if ATH_RESET_SERIAL
        atomic_dec(&sc->sc_hold_reset);
        ATH_RESET_RELEASE_MUTEX(sc);
#else
        ATH_UNLOCK_PCI_IRQ(sc);
#endif

        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, imask);

        goto done;
    }

#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_hold_reset);
    ATH_RESET_RELEASE_MUTEX(sc);
#else
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    if (imask & HAL_INT_GLOBAL)
        ath_hal_intrset(ah, imask);

    /*
     * This is needed only to setup initial state
     * but it's best done after a reset.
     */
    ath_update_txpow(sc, sc->tx_power);

    /*
     * Setup the hardware after reset: the key cache
     * is filled as needed and the receive engine is
     * set going.  Frame transmit is handled entirely
     * in the frame output path; there's nothing to do
     * here except setup the interrupt mask.
     */

    if (ATH_STARTRECV(sc) != 0) {
        printk("%s: unable to start recv logic\n", __func__);
        error = -EIO;
        goto done;
    }

    /*
     *  Setup our intr mask. Note, GPIO intr mask can be set earlier.
     */
    sc->sc_imask &= HAL_INT_GPIO;
    sc->sc_imask |= HAL_INT_TX
        | HAL_INT_RXEOL | HAL_INT_RXORN
        | HAL_INT_FATAL | HAL_INT_GLOBAL;

    if (sc->sc_enhanceddmasupport) {
        sc->sc_imask |= HAL_INT_RXHP | HAL_INT_RXLP;
    } else {
        sc->sc_imask |= HAL_INT_RX;
    }

    if (ath_hal_gttsupported(ah))
        sc->sc_imask |= HAL_INT_GTT;

    if (sc->sc_hashtsupport)
        sc->sc_imask |= HAL_INT_CST;

    /*
     * Enable MIB interrupts when there are hardware phy counters.
     * Note we only do this (at the moment) for station mode.
     */
    if ((sc->sc_opmode == HAL_M_STA) ||
        (sc->sc_opmode == HAL_M_IBSS)) {

#ifdef ATH_MIB_INTR_FILTER
             /*
             * EV 63034. We should not be enabling MIB interrupts unless
             * we have all the counter-reading code enabled so it can 0 out
             * counters and clear the interrupt condition (saturated counters).
             */
        if (sc->sc_needmib) {
            sc->sc_imask |= HAL_INT_MIB;
        }
#endif // ATH_MIB_INTR_FILTER

        /*
         * Enable TSFOOR in STA and IBSS modes.
         */
        sc->sc_imask |= HAL_INT_TSFOOR;
    }

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill) {
        if (ath_rfkill_hasint(sc))
            sc->sc_imask |= HAL_INT_GPIO;

        /*
         * WAR for Bug 33276: In certain systems, the RfKill signal is slow to
         * stabilize when waking up from power suspend. The RfKill is an
         * active-low GPIO signal and the problem is the slow rise from 0V to VCC.
         * For this workaround, we will delayed implementing the new RfKill
         * state if there is a change in RfKill during the sleep. This WAR is only
         * when the previous RfKill state is OFF and the new awaken state is ON.
         */
        if (sc->sc_reg_parm.rfKillDelayDetect) {
            ath_rfkill_delay_detect(sc);
        }
    }
#endif

    if (sc->sc_wpsgpiointr)
        sc->sc_imask |= HAL_INT_GPIO;

#ifdef ATH_BT_COEX
    if (sc->sc_btinfo.bt_gpioIntEnabled) {
        sc->sc_imask |= HAL_INT_GPIO;
    }
#endif

    /*
     * At least while Osprey is in alpha,
     * default BB panic watchdog to be on to aid debugging
     */
    if (sc->sc_bbpanicsupport) {
        sc->sc_imask |= HAL_INT_BBPANIC;
    }

#if ATH_SUPPORT_MCI
    if (sc->sc_mci.mci_support) {
        sc->sc_imask |= HAL_INT_MCI;
    }
#endif

#if ATH_GEN_TIMER
    sc->sc_imask |= HAL_INT_GENTIMER;
#endif

    /*
     *  Don't enable interrupts here as we've not yet built our
     *  vap and node data structures, which will be needed as soon
     *  as we start receiving.
     */

    ath_chan_change(sc, initial_chan, false);

    if (!sc->sc_reg_parm.pcieDisableAspmOnRfWake) {
        ath_pcie_pwrsave_enable(sc, 1);
    } else {
        ath_pcie_pwrsave_enable(sc, 0);
    }

    /* XXX: we must make sure h/w is ready and clear invalid flag
     * before turning on interrupt. */
    sc->sc_invalid = 0;

    /*
     * It has been seen that after a config change, when the channel
     * is set, the fast path is taken in the chipset reset code. The
     * net result is that the interrupt ref count is left hanging in
     * a non-zero state, causing interrupt to NOT get enabled. The fix
     * here is that on ath_open, we set the sc_full_reset flag to 1
     * preventing the fast path from being taken on an initial config.
    */
    sc->sc_full_reset = 1;

    /* Start LED module; pass radio state as parameter */
    ath_led_start_control(&sc->sc_led_control,
                          sc->sc_hw_phystate && sc->sc_sw_phystate);

    rfilt = ath_hal_getrxfilter(sc->sc_ah);
    rfilt |= HAL_RX_FILTER_PHYRADAR;
    ath_hal_setrxfilter(sc->sc_ah, rfilt);
    sc->sc_ieee_ops->ath_net80211_enable_radar_dfs(sc->sc_ieee);
done:
    //ATH_UNLOCK(sc);
    return error;
}

/*
 * Stop the device, grabbing the top-level lock to protect
 * against concurrent entry through ath_init (which can happen
 * if another thread does a system call and the thread doing the
 * stop is preempted).
 */
static int
ath_stop(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error;

    //ATH_LOCK(sc);

    if (!sc->sc_invalid) {
        ath_pwrsave_awake(sc);

        ath_led_disable(&sc->sc_led_control);
    }

    error = ath_stop_locked(sc);
#if 0
    if (error == 0 && !sc->sc_invalid) {
        /*
         * Set the chip in full sleep mode.  Note that we are
         * careful to do this only when bringing the interface
         * completely to a stop.  When the chip is in this state
         * it must be carefully woken up or references to
         * registers in the PCI clock domain may freeze the bus
         * (and system).  This varies by chip and is mostly an
         * issue with newer parts that go to sleep more quickly.
         */
        ath_hal_setpower(sc->sc_ah, HAL_PM_FULL_SLEEP);
    }
#endif
    //ATH_UNLOCK(sc);

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    return error;
}

static void
ath_reset_handle_fifo_intsafe(ath_dev_t dev)
{
    ath_rx_intr(dev, HAL_RX_QUEUE_HP);
    ath_rx_intr(dev, HAL_RX_QUEUE_LP);
}

/*
 * spin_lock is to exclude access from other thread(passive or dpc level).
 * intsafe is to exclude interrupt access. ath_rx_intr() normally is called
 * in ISR context.
 * note: spin_lock may hold a long time.
 */
static void
ath_reset_handle_fifo(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_RXBUF_LOCK(sc);
    OS_EXEC_INTSAFE(sc->sc_osdev, ath_reset_handle_fifo_intsafe, dev);
    ATH_RXBUF_UNLOCK(sc);
}

#if ATH_RESET_SERIAL
/*
 * Reset routine waits until both ath_tx_txqaddbuf() and
 * ath_rx_buf_link() finish their processing.
 */
int
ath_reset_wait_tx_rx_finished(struct ath_softc *sc)
{
    while(atomic_read(&sc->sc_tx_add_processing)) { //waiting
        DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for tx_txqaddbuf:%s, %d\n",
            __func__, atomic_read(&sc->sc_tx_add_processing));
    }

    while(atomic_read(&sc->sc_rx_return_processing)) {
        DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for rx_return:%s, %d\n",
            __func__, atomic_read(&sc->sc_rx_return_processing));
    }
    return 0;
}

int
ath_reset_wait_intr_dpc_finished(struct ath_softc *sc)
{
    int wait_count = 0;

    while(atomic_read(&sc->sc_intr_processing)){
        OS_DELAY(100);
        if (wait_count++ >= 100) {
            DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for intr return timeout:%s, %d\n",
                    __func__, atomic_read(&sc->sc_intr_processing));
       }
    }

    wait_count = 0;
    while(atomic_read(&sc->sc_dpc_processing)){
        OS_DELAY(100);
        if (wait_count++ >= 100) {
            DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for dpc return timeout:%s, %d\n",
                    __func__, atomic_read(&sc->sc_dpc_processing));
        }
    }
   return 0;
}
#endif

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from errors rx overrun
 * and to reset the hardware when rf gain settings must be reset.
 */
int
ath_reset_start(ath_dev_t dev, HAL_BOOL no_flush, int tx_timeout, int rx_timeout)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex)
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_RESET_START, NULL);
#endif
    sc->sc_rc_rx_mgmt_num = 0;
#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_dpc_stop);
    printk("%s %d %d\n", __func__,
__LINE__, sc->sc_dpc_stop);
    ath_hal_intrset(ah, 0);
    ATH_RESET_UNLOCK(sc);
#else
    /* Set the flag so that other functions will not try to restart tx/rx DMA. */
    atomic_inc(&sc->sc_in_reset);
    ath_hal_intrset(ah, 0);                     /* disable interrupts */
#endif


#if ATH_RESET_SERIAL
    OS_DELAY(100);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_hold_reset);
    atomic_set(&sc->sc_reset_queue_flag, 1);
    ath_reset_wait_tx_rx_finished(sc);
    ATH_RESET_UNLOCK(sc);
    ath_reset_wait_intr_dpc_finished(sc);
    atomic_dec(&sc->sc_dpc_stop);
    printk("%s %d %d\n", __func__,
__LINE__, sc->sc_dpc_stop);
    printk("Begin reset sequence......\n");
#endif

    ath_reset_draintxq(sc, no_flush, tx_timeout); /* stop xmit side */
    ath_wmi_drain_txq(sc);                      /* Stop target firmware tx */


    /* In case of STA reset (because of BB panic), few subframes in rx FiFo are
       getting dropped because of ATH_STOPRECV() call before ATH_RX_TASKLET() */
    if(no_flush && sc->sc_enhanceddmasupport)
    {
        ath_reset_handle_fifo(dev);
    }

    ATH_STOPRECV(sc, rx_timeout);                        /* stop recv side */

    /*
     * We cannot indicate received packets while holding a lock.
     * use cmpxchg here.
     */
    if (cmpxchg(&sc->sc_rxflush, 0, 1) == 0) {
        if (no_flush) {
            ath_reset(dev);
            ATH_RX_TASKLET(sc, RX_FORCE_PROCESS);
        } else {
            ATH_RX_TASKLET(sc, RX_DROP);
        }

        if (cmpxchg(&sc->sc_rxflush, 1, 0) != 1)
            DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: flush is not protected.\n", __func__);
    }

    /* Suspend ForcePPM when entering a reset */
    ath_force_ppm_notify(&sc->sc_ppm_info, ATH_FORCE_PPM_SUSPEND, NULL);

    ATH_PS_SLEEP(sc);
    return 0;
}

int
ath_reset_end(ath_dev_t dev, HAL_BOOL no_flush)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);


#if ATH_RESET_SERIAL
    /* to keep lock sequence as same as ath_tx_start_dma->ath_tx_txqaddbuf,
     * we have to lock all of txq firstly.
     */
    /* osprey seems no need to sync with reset */
    if (!sc->sc_enhanceddmasupport) {
        int i;
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
            ATH_TXQ_LOCK(&sc->sc_txq[i]);

        ATH_RESET_LOCK(sc);
        atomic_set(&sc->sc_reset_queue_flag, 0);
        atomic_dec(&sc->sc_hold_reset);
        if (atomic_read(&sc->sc_hold_reset))
            printk("Wow, still multiple reset in process,--investigate---\n");



         /* flush all the queued tx_buf */

        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {

            ath_bufhead *head, buf_head;
            struct ath_buf *buf, *last_buf;

           // ATH_TXQ_LOCK(&sc->sc_txq[i]);
            ATH_RESET_LOCK_Q(sc, i);

            head = &(sc->sc_queued_txbuf[i].txbuf_head);

            do {
                buf = TAILQ_FIRST(head);
                if (!buf)
                    break;
                last_buf = buf->bf_lastbf;
                TAILQ_INIT(&buf_head);
                TAILQ_REMOVE_HEAD_UNTIL(head, &buf_head, last_buf, bf_list);
                _ath_tx_txqaddbuf(sc, &sc->sc_txq[i], &buf_head); //can't call directly
            } while (!TAILQ_EMPTY(head));


            ATH_RESET_UNLOCK_Q(sc, i);
           // ATH_TXQ_UNLOCK(&sc->sc_txq[i]);

        }

        ATH_RESET_UNLOCK(sc);
        for (i = HAL_NUM_TX_QUEUES -1 ; i >= 0; i--)
             ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
    } else {
        atomic_set(&sc->sc_reset_queue_flag, 0);
        atomic_dec(&sc->sc_hold_reset);
    }
#else


    /* Clear the flag so that we can start hardware. */
    atomic_dec(&sc->sc_in_reset);
#endif

    ath_wmi_start_recv(sc);

    if (ATH_STARTRECV(sc) != 0) /* restart recv */
        printk("%s: unable to start recv logic\n", __func__);

    /*
     * We may be doing a reset in response to an ioctl
     * that changes the channel so update any state that
     * might change as a result.
     */
    ath_chan_change(sc, &sc->sc_curchan, false);
#ifdef ATH_SUPPORT_DFS
    sc->sc_ieee_ops->ath_net80211_enable_radar_dfs(sc->sc_ieee);
#endif /* ATH_SUPPORT_DFS */

    ath_update_txpow(sc, sc->tx_power);        /* update tx power state */

    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);   /* restart beacons */
    sc->sc_reset_type = ATH_RESET_DEFAULT; /* reset sc_reset_type */

    ath_hal_intrset(ah, sc->sc_imask);

    /* Resume ForcePPM after reset is completed */
    ath_force_ppm_notify(&sc->sc_ppm_info, ATH_FORCE_PPM_RESUME, NULL);

    /* Restart the txq */
    if (no_flush) {
        int i;
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
            if (ATH_TXQ_SETUP(sc, i)) {
                ATH_TXQ_LOCK(&sc->sc_txq[i]);
                ath_txq_schedule(sc, &sc->sc_txq[i]);
                ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
            }
        }
    }

#ifdef ATH_GEN_TIMER
        /* Inform the generic timer module that a reset just happened. */
        ath_gen_timer_post_reset(sc);
#endif  //ATH_GEN_TIMER
#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex)
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_RESET_END, NULL);
#endif
    ATH_PS_SLEEP(sc);

#if ATH_RESET_SERIAL
    /* release the lock */
    ATH_RESET_RELEASE_MUTEX(sc);
#endif

    return 0;
}

int
ath_reset(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    int error = 0;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);

    ATH_PS_WAKEUP(sc);

    ATH_USB_TX_STOP(sc->sc_osdev);
    ATH_HTC_TXRX_STOP(sc, AH_FALSE);
#if ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif

    /* NB: indicate channel change so we do a full reset */
#if !ATH_RESET_SERIAL
    ATH_LOCK_PCI_IRQ(sc);
#endif

#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan,
                       ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing,
                       AH_FALSE, &status, sc->sc_scanning))
    {
        printk("%s: unable to reset hardware; hal status %u,\n"
               "sc_opmode=%d, sc_curchan: chan=%d, Flags=0x%x, sc_scanning=%d\n",
               __func__, status,
               sc->sc_opmode, sc->sc_curchan.channel, sc->sc_curchan.channel_flags,
               sc->sc_scanning
               );
        error = -EIO;
    }
    ATH_HTC_ABORTTXQ(sc);
#if !ATH_RESET_SERIAL
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    ATH_USB_TX_START(sc->sc_osdev);
    ATH_PS_SLEEP(sc);
    return error;
}

static int
ath_switch_opmode(ath_dev_t dev, HAL_OPMODE opmode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if (sc->sc_opmode != opmode) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: switch opmode from %d to %d\n", __func__,sc->sc_opmode, opmode);
        sc->sc_opmode = opmode;
        ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH,ATH_IF_ID_ANY);
        ath_internal_reset(sc);
    }
    return 0;
}

static int
ath_suspend(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal   *ah = sc->sc_ah;

#ifdef ATH_BT_COEX
    ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_FULL_SLEEP, NULL);
#endif

    /* cancel timers before the device is put to sleep */
    ath_cancel_timer(&sc->sc_chan_busy, CANCEL_NO_SLEEP);
    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

    /*
    ** No I/O if device has been surprise removed
    */

#if WLAN_SUPPORT_GREEN_AP
    if (ath_green_ap_get_enable(sc))
        ath_green_ap_suspend(sc);
#endif /* WLAN_SUPPORT_GREEN_AP */

    if (ath_get_sw_phystate(sc) && ath_get_hw_phystate(sc)) {
        /* stop the hardware */
        ath_stop(sc);

        if (sc->sc_invalid)
            return -EIO;
    }
    else {
        if (sc->sc_invalid)
            return -EIO;

        /*
         * Work around for bug #24058: OS hang during restart after software RF off.
         * Condore has problem waking up on some machines (no clue why) if ath_suspend is called
         * in which nothing is done when RF is off.
         */
        ath_pwrsave_awake(sc);

        /* Shut off the interrupt before setting sc->sc_invalid to '1' */
        ath_hal_intrset(ah, 0);

        /* Even though the radio is OFF, the LED might still be alive. Disable it. */
        ath_led_disable(&sc->sc_led_control);
    }

    /* XXX: we must make sure h/w will not generate any interrupt
     * before setting the invalid flag. */
    sc->sc_invalid = 1;

    /* Wait for any hardware access to complete */
    ath_wait_sc_inuse_cnt(sc, 1000);

    /* disable HAL and put h/w to sleep */
    ath_hal_disable(sc->sc_ah);

    ath_hal_configpcipowersave(sc->sc_ah, 1, 1);

    ath_pwrsave_fullsleep(sc);

    return 0;
}

static void
ath_enable_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_intrset(sc->sc_ah, sc->sc_imask);
}

static void
ath_disable_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_intrset(sc->sc_ah, 0);
}

#ifdef ATH_MIB_INTR_FILTER
/*
 * Filter excessive MIB interrupts so that the system doesn't freeze because
 * it cannot handle the lower-priority threads.
 *
 * Algorithm:
 *     -The beginning of a possible burst is characterized by two consecutive
 *      MIB interrupts arriving within MIB_FILTER_MAX_INTR_ELAPSED_TIME (20ms)
 *      of each other.
 *
 *     -A MIB interrupt is considered part of a burst if it arrives within
 *      MIB_FILTER_MAX_INTR_ELAPSED_TIME (20ms) of the previous one AND within
 *      MIB_FILTER_MAX_BURST_ELAPSED_TIME (100ms) of the beginning of the burst.
 *
 *     -Once the number of consecutive MIB interrupts reaches
 *      MIB_FILTER_COUNT_THRESHOLD (500) we disable reception of MIB interrupts.
 *
 *     -Reception of a MIB interrupt that is longer part of the burst or
 *      reception of a different interrupt cause the counting to start over.
 *
 *     -Once the MIB interrupts have been disabled, we wait for
 *      MIB_FILTER_RECOVERY_TIME (50ms) and then reenable MIB interrupts upon
 *      reception of the next non-MIB interrupt.
 *
 * Miscellaneous:
 *     -The algorithm is always enabled if ATH_MIB_INTR_FILTER is defined.
 */
static void ath_filter_mib_intr(struct ath_softc *sc, u_int8_t is_mib_intr)
{
    struct ath_intr_filter    *intr_filter  = &sc->sc_intr_filter;
    u_int32_t                 current_time  = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    u_int32_t                 intr_interval = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time - intr_filter->last_intr_time);
    u_int32_t                 burst_dur     = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time - intr_filter->burst_start_time);

    switch (intr_filter->state) {
    case INTR_FILTER_OFF:
        /*
         * Two MIB interrupts arriving too close to each other may mark the
         * beggining of a burst of MIB interrupts.
         */
        if (is_mib_intr && (intr_interval <= MIB_FILTER_MAX_INTR_ELAPSED_TIME)) {
            intr_filter->state            = INTR_FILTER_DEGLITCHING;
            intr_filter->burst_start_time = current_time;
            intr_filter->intr_count++;
        }
        break;

    case INTR_FILTER_DEGLITCHING:
        /*
         * Consider a MIB interrupt arrived within a short time of the
         * previous one and withing a short time of the beginning of the burst
         * to be still part of the same burst.
         */
        if (is_mib_intr &&
            (intr_interval <= MIB_FILTER_MAX_INTR_ELAPSED_TIME) &&
            (burst_dur <= MIB_FILTER_MAX_BURST_ELAPSED_TIME)) {
            intr_filter->intr_count++;

            /*
             * Too many consecutive within a short time of each other
             * characterize a MIB burst ("storm")
             */
            if (intr_filter->intr_count >= MIB_FILTER_COUNT_THRESHOLD) {
                /* MIB burst identified */
                intr_filter->activation_count++;
                intr_filter->state = INTR_FILTER_ON;

                /* Disable MIB interrupts */
                sc->sc_imask &= ~HAL_INT_MIB;

                DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%d.%03d | %s: Start Mib Storm index=%3d intrCount=%8d start=%d.%03d duration=%6u ms\n",
                    current_time / 1000, current_time % 1000, __func__,
                    intr_filter->activation_count,
                    intr_filter->intr_count,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) % 1000,
                    burst_dur);

                /* Want a register dump? */
                /* ath_printreg(sc, HAL_DIAG_PRINT_REG_COUNTER | HAL_DIAG_PRINT_REG_ALL); */
            }
        }
        else {
            /*
             * The gap from the previous interrupt is long enough to
             * characterize the latest as not being part of a burst.
             * Go back to the initial state.
             */
            intr_filter->state      = INTR_FILTER_OFF;
            intr_filter->intr_count = 1;
        }
        break;

    case INTR_FILTER_ON:
        /*
         * Wait for a non-MIB interrupt to be received more than a certain time
         * after the last MIB interrupt to consider it the end of the interrupt
         * burst                                                      UE
         */
        if (! is_mib_intr) {
            if (intr_interval > MIB_FILTER_RECOVERY_TIME) {
                DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%d.%03d | %s: Mib Storm index=%3d intrCount=%8d start=%d.%03d duration=%6u ms\n",
                    current_time / 1000, current_time % 1000, __func__,
                    intr_filter->activation_count,
                    intr_filter->intr_count,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) % 1000,
                    burst_dur);

                /* Re-enable MIB interrupt */
                sc->sc_imask |= HAL_INT_MIB;

                /* Return to the initial state and monitor for new bursts */
                intr_filter->state      = INTR_FILTER_OFF;
                intr_filter->intr_count = 1;
            }
        }
        else {
            /* Another MIB interrupt generated was we were disabling them */
            intr_filter->intr_count++;
        }
        break;

    default:
        /* Bad state */
        ASSERT(0);
        break;
    }

    /* Do not save timestamp of non-MIB interrupts */
    if (is_mib_intr) {
        intr_filter->last_intr_time = current_time;
    }
}
#endif

#ifndef ATH_UPDATE_COMMON_INTR_STATS
#define ATH_UPDATE_COMMON_INTR_STATS(sc, status)
#endif
#ifndef ATH_UPDATE_INTR_STATS
#define ATH_UPDATE_INTR_STATS(sc, intr)
#endif

/*
 * Common Interrupt handler for MSI and Line interrutps.
 * Most of the actual processing is deferred.
 * It's the caller's responsibility to ensure the chip is awake.
 */
static inline int
ath_common_intr(ath_dev_t dev, HAL_INT status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int sched = ATH_ISR_NOSCHED;

    ATH_UPDATE_COMMON_INTR_STATS(sc, status);

    do {
#ifdef ATH_MIB_INTR_FILTER
        /* Notify the MIB interrupt filter that we received some other interrupt. */
        if (! (status & HAL_INT_MIB)) {
            ath_filter_mib_intr(sc, AH_FALSE);
        }
#endif

        if (status & HAL_INT_FATAL) {
            /* need a chip reset */
#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_hardware_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_hardware++;
#endif
            sched = ATH_ISR_SCHED;
        } else if ((status & HAL_INT_RXORN) && !sc->sc_enhanceddmasupport) {
            /* need a chip reset? */
#if ATH_SUPPORT_DESCFAST
            ath_rx_proc_descfast(dev);
#endif
            sc->sc_stats.ast_rxorn++;
            sched = ATH_ISR_SCHED;
        } else {
            if (status & HAL_INT_SWBA) {
#ifdef ATH_BEACON_DEFERRED_PROC
                /* Handle beacon transmission in deferred interrupt processing */
                sched = ATH_ISR_SCHED;
#else
                int needmark = 0;

                /*
                 * Software beacon alert--time to send a beacon.
                 * Handle beacon transmission directly; deferring
                 * this is too slow to meet timing constraints
                 * under load.
                 */
                ath_beacon_tasklet(sc, &needmark);

                if (needmark) {
                    /* We have a beacon stuck. Beacon stuck processing
                     * should be done in DPC instead of here. */
                    sched = ATH_ISR_SCHED;
                }
#endif /* ATH_BEACON_DEFERRED_PROC */
                ATH_UPDATE_INTR_STATS(sc, swba);
            }
            if (status & HAL_INT_TXURN) {
                sc->sc_stats.ast_txurn++;
                /* bump tx trigger level */
                ath_hal_updatetxtriglevel(ah, AH_TRUE);
            }
            if (sc->sc_enhanceddmasupport) {
#if ATH_OSPREY_RXDEFERRED
                if (status & HAL_INT_RXEOL) {
                    /* TODO - check rx fifo threshold here */

                    /*
                     * RXEOL is always asserted after a chip reset. Ideally rxfifodepth 0
                     * should actually indicate a true RXEOL condition. Therefore checking
                     * rxfifodepth == 0 or consecutive RXEOL's to disable further interrupts.
                     * Otherwise, if the interrupt is disabled here and no packets are rx'ed
                     * for 3 secs (implicitly meaning this interrupt doesn't get re-enabled),
                     * the txq hang checker falsely indentifies it as a hang condition and
                     * does a chip reset.
                     */
                    if (sc->sc_rxedma[HAL_RX_QUEUE_HP].rxfifodepth == 0 ||
                        sc->sc_rxedma[HAL_RX_QUEUE_LP].rxfifodepth == 0 ||
                        sc->sc_consecutive_rxeol_count > 5) {
                        //No buffers available - disable RXEOL/RXORN to avoid interrupt storm
                        // Disable and then enable to satisfy global isr enable reference counter
                        //For further investigation

                        sc->sc_consecutive_rxeol_count = 0;
                        sc->sc_imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
                        ath_hal_intrset(ah, sc->sc_imask);
#if ATH_HW_TXQ_STUCK_WAR
                        sc->sc_last_rxeol = OS_GET_TIMESTAMP();
#endif
                    } else {
                        sc->sc_consecutive_rxeol_count++;
                    }
                    sched = ATH_ISR_SCHED;
                    sc->sc_stats.ast_rxeol++;
                } else {
                    sc->sc_consecutive_rxeol_count = 0;
                }
                if (status & (HAL_INT_RXHP | HAL_INT_RXLP | HAL_INT_RXORN)) {
                    sched = ATH_ISR_SCHED;
                }
                if (status & HAL_INT_RXORN) {
                    sc->sc_stats.ast_rxorn++;
                }
                if (status & HAL_INT_RX) {
                    ATH_UPDATE_INTR_STATS(sc, rx);
                }
#else // ATH_OSPREY_RXDEFERRED
                ath_rx_edma_intr(sc, status, &sched);
#endif // ATH_OSPREY_RXDEFERRED
            }
            else {
                if ((status & HAL_INT_RXEOL)) {
                    /*
                     * NB: the hardware should re-read the link when
                     *     RXE bit is written, but it doesn't work at
                     *     least on older hardware revs.
                     */
#if ATH_SUPPORT_DESCFAST
                    ath_rx_proc_descfast(dev);
#endif
                    sc->sc_imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
                    ath_hal_intrset(ah, sc->sc_imask);
#if ATH_HW_TXQ_STUCK_WAR
                    sc->sc_last_rxeol = OS_GET_TIMESTAMP();
#endif
                    sc->sc_stats.ast_rxeol++;
                    sched = ATH_ISR_SCHED;
                }
                if (status & HAL_INT_RX) {
                    ATH_UPDATE_INTR_STATS(sc, rx);
#if ATH_SUPPORT_DESCFAST
                    ath_rx_proc_descfast(dev);
#endif
                    sched = ATH_ISR_SCHED;
                }
            }

            if (status & HAL_INT_TX) {
                ATH_UPDATE_INTR_STATS(sc, tx);
#ifdef ATH_SUPERG_DYNTURBO
                /*
                 * Check if the beacon queue caused the interrupt
                 * when a dynamic turbo switch
                 * is pending so we can initiate the change.
                 * XXX must wait for all vap's beacons
                 */

                if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_dturbo_switch) {
                    u_int32_t txqs= (1 << sc->sc_bhalq);
                    ath_hal_gettxintrtxqs(ah,&txqs);
                    if(txqs & (1 << sc->sc_bhalq)) {
                        sc->sc_dturbo_switch = 0;
                        /*
                         * Hack: defer switch for 10ms to permit slow
                         * clients time to track us.  This especially
                         * noticeable with Windows clients.
                         */

                    }
                }
#endif
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_BMISS) {
#ifdef QCA_SUPPORT_CP_STATS
                pdev_lmac_cp_stats_ast_bmiss_inc(sc->sc_pdev, 1);
#else
                sc->sc_stats.ast_bmiss++;
#endif
#if ATH_WOW
                if (sc->sc_wow_sleep) {
                    /*
                     * The system is in WOW sleep and we used the BMISS intr to
                     * wake the system up. Note this BMISS by setting the
                     * sc_wow_bmiss_intr flag but do not process this interrupt.
                     */
                    DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR,"%s: During Wow Sleep and got BMISS\n", __func__);
                    sc->sc_wow_bmiss_intr = 1;
                    sc->sc_wow_sleep = 0;
                    sched = ATH_ISR_NOSCHED;
                } else
#endif
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_BRSSI) {
#ifdef QCA_SUPPORT_CP_STATS
                pdev_lmac_cp_stats_ast_brssi_inc(sc->sc_pdev, 1);
#else
                sc->sc_stats.ast_brssi++;
#endif

                /* Disable BRSSI interrupt to prevent repeated firings. */
                sc->sc_imask &= ~HAL_INT_BRSSI;
                ath_hal_intrset(ah, sc->sc_imask);

                ath_hal_set_hw_beacon_rssi_threshold(ah, 0);

                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_GTT) { /* tx timeout interrupt */
                sc->sc_stats.ast_txto++;
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_CST) { /* carrier sense timeout */
                sc->sc_stats.ast_cst++;
                sched = ATH_ISR_SCHED;
            }

            if (status & HAL_INT_MIB) {
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_mib_int_count_inc(sc->sc_pdev, 1);
#else
                sc->sc_stats.ast_mib++;
#endif
                /*
                 * Disable interrupts until we service the MIB
                 * interrupt; otherwise it will continue to fire.
                 */
                ath_hal_intrset(ah, 0);

#ifdef ATH_MIB_INTR_FILTER
                /* Check for bursts of MIB interrupts */
                ath_filter_mib_intr(sc, AH_TRUE);
#endif

                /*
                 * Let the hal handle the event.  We assume it will
                 * clear whatever condition caused the interrupt.
                 */
                ath_hal_mibevent(ah, &sc->sc_halstats);
                ath_hal_intrset(ah, sc->sc_imask);
            }
            if (status & HAL_INT_GPIO) {
                ATH_UPDATE_INTR_STATS(sc, gpio);
                /* Check if this GPIO interrupt is caused by RfKill */
#ifdef ATH_RFKILL
                if (ath_rfkill_gpio_isr(sc))
                    sched = ATH_ISR_SCHED;
#endif
                if (sc->sc_wpsgpiointr) {
                    /* Check for WPS push button press (GPIO polarity low) */
                    if (ath_hal_gpioget(sc->sc_ah, sc->sc_reg_parm.wpsButtonGpio) == 0) {
                        sc->sc_wpsbuttonpushed = 1;

                        /* Disable associated GPIO interrupt to prevent flooding */
                        ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_DISABLE);
                        sc->sc_wpsgpiointr = 0;
                    }
                }
#ifdef ATH_BT_COEX
                if (sc->sc_btinfo.bt_gpioIntEnabled) {
                    sched = ATH_ISR_SCHED;
                }
#endif
            }
            if (status & HAL_INT_TIM) {
                ATH_UPDATE_INTR_STATS(sc, tim);
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_DTIM) {
                ATH_UPDATE_INTR_STATS(sc, dtim);
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_TIM_TIMER) {
                ATH_UPDATE_INTR_STATS(sc, tim_timer);
                if (! sc->sc_hasautosleep) {
                    /* Clear RxAbort bit so that we can receive frames */
                    ath_hal_setrxabort(ah, 0);
                    /* Set flag indicating we're waiting for a beacon */
                    sc->sc_waitbeacon = 1;

                    sched = ATH_ISR_SCHED;
                }
            }
#ifdef ATH_GEN_TIMER
            if (status & HAL_INT_GENTIMER) {
                ATH_UPDATE_INTR_STATS(sc, gentimer);
                /* generic TSF timer interrupt */
                sched = ATH_ISR_SCHED;
            }
#endif

#if ATH_SUPPORT_MCI
            if (status & HAL_INT_MCI) {
                sched = ATH_ISR_SCHED;
            }
#endif
            if (status & HAL_INT_TSFOOR) {
                ATH_UPDATE_INTR_STATS(sc, tsfoor);
                DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
                        "%s: HAL_INT_TSFOOR - syncing beacon\n",
                        __func__);
                /* Set flag indicating we're waiting for a beacon */
                sc->sc_waitbeacon = 1;

                sched = ATH_ISR_SCHED;
            }

            if (status & HAL_INT_BBPANIC) {
                ATH_UPDATE_INTR_STATS(sc, bbevent);
                /* schedule the DPC to get bb panic info */
                sched = ATH_ISR_SCHED;
            }

        }
    } while (0);

    if (sched == ATH_ISR_SCHED) {
        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: Scheduling BH/DPC\n",__func__);
        if (sc->sc_enhanceddmasupport) {
            /* For enhanced DMA turn off all interrupts except RXEOL, RXORN, SWBA.
             * Disable and then enable to satisfy the global isr enable reference counter.
             */
#ifdef ATH_BEACON_DEFERRED_PROC
            ath_hal_intrset(ah, 0);
            ath_hal_intrset(ah, sc->sc_imask & (HAL_INT_GLOBAL | HAL_INT_RXEOL | HAL_INT_RXORN));
#else
            ath_hal_intrset(ah, 0);
            ath_hal_intrset(ah, sc->sc_imask & (HAL_INT_GLOBAL | HAL_INT_RXEOL | HAL_INT_RXORN | HAL_INT_SWBA));
#endif
        } else {
#ifdef ATH_BEACON_DEFERRED_PROC
            /* turn off all interrupts */
            ath_hal_intrset(ah, 0);
#else
            /* turn off every interrupt except SWBA */
            ath_hal_intrset(ah, (sc->sc_imask & HAL_INT_SWBA));
#endif
        }
    }

    return sched;
}

/*
 * Line Interrupt handler.  Most of the actual processing is deferred.
 *
 * It's the caller's responsibility to ensure the chip is awake.
 */
static int
ath_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_INT status;
    int    isr_status = ATH_ISR_NOTMINE;

    atomic_inc(&sc->sc_inuse_cnt);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_intr_processing);
    if (atomic_read(&sc->sc_hold_reset)) {
        isr_status = ATH_ISR_NOTMINE;
        atomic_dec(&sc->sc_intr_processing);
        atomic_dec(&sc->sc_inuse_cnt);
        return isr_status;
    }
#endif

    do {
        if (!ath_hal_intrpend(ah)) {    /* shared irq, not for us */
            isr_status = ATH_ISR_NOTMINE;
            break;
        }

        if (sc->sc_invalid) {
            /*
             * The hardware is either not ready or is entering full sleep,
             * don't touch any RTC domain register.
             */
            ath_hal_intrset_nortc(ah, 0);
            ath_hal_getisr_nortc(ah, &status, 0, 0);
            isr_status = ATH_ISR_NOSCHED;
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: recv interrupts when invalid.\n",__func__);
            break;
        }

        /*
         * Figure out the reason(s) for the interrupt.  Note
         * that the hal returns a pseudo-ISR that may include
         * bits we haven't explicitly enabled so we mask the
         * value to insure we only process bits we requested.
         */
        ath_hal_getisr(ah, &status, HAL_INT_LINE, 0);       /* NB: clears ISR too */
        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: status 0x%x  Mask: 0x%x\n",
                __func__, status, sc->sc_imask);

        status &= (sc->sc_imask | HAL_INT_RXEOL | HAL_INT_RXORN);   /* discard unasked-for bits */

        /*
        ** If there are no status bits set, then this interrupt was not
        ** for me (should have been caught above).
        */

        if(!status)
        {
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: Not My Interrupt\n",__func__);
            isr_status = ATH_ISR_NOSCHED;
            break;
        }

        sc->sc_intrstatus |= status;

        isr_status = ath_common_intr(dev, status);
    } while (FALSE);

#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_intr_processing);
#endif
    atomic_dec(&sc->sc_inuse_cnt);

    return isr_status;
}

/*
 * Message Interrupt handler.  Most of the actual processing is deferred.
 *
 * It's the caller's responsibility to ensure the chip is awake.
 */
static int
ath_mesg_intr (ath_dev_t dev, u_int mesgid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_INT status;
    int    isr_status = ATH_ISR_NOTMINE;

    atomic_inc(&sc->sc_inuse_cnt);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_intr_processing);
    if (atomic_read(&sc->sc_hold_reset)) {
        isr_status = ATH_ISR_NOTMINE;
        atomic_dec(&sc->sc_intr_processing);
        atomic_dec(&sc->sc_inuse_cnt);
        return isr_status;
    }
#endif

    do {
        /* handle all other interrutps */
        ath_hal_getisr(ah, &status, HAL_INT_MSI, mesgid);       /* NB: clears ISR too */

        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: status 0x%x  Mask: 0x%x\n",
                __func__, status, sc->sc_imask);

        if (sc->sc_invalid) {
            /*
             * The hardware is not ready/present, don't touch anything.
             * Note this can happen early on if the IRQ is shared.
             */
            ath_hal_intrset_nortc(ah, 0);
            ath_hal_getisr_nortc(ah, &status, 0, 0);
            isr_status = ATH_ISR_NOSCHED;
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: recv interrupts when invalid.\n",__func__);
            break;
        }

        status &= sc->sc_imask;         /* discard unasked-for bits */
        sc->sc_intrstatus |= status;

        isr_status = ath_common_intr(dev, status);
    } while (FALSE);
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_intr_processing);
#endif

    atomic_dec(&sc->sc_inuse_cnt);

    return isr_status;
}

struct ath_intr_status_params{
    struct ath_softc    *sc;
    u_int32_t           intr_status;
};

/*
 * intsafe is to exclude interrupt access. ath_handle_intr() normally is called
 * in DPC context.
 */
static void
ath_update_intrstatus_intsafe(struct ath_intr_status_params *p_params)
{
    p_params->intr_status = p_params->sc->sc_intrstatus;
    p_params->sc->sc_intrstatus &= (~p_params->intr_status);
}


static inline int
ath_hw_tx_hang_check(struct ath_softc *sc)
{
    if(ath_hal_support_tx_hang_check(sc->sc_ah)) {
        if (ATH_MAC_HANG_WAR_REQ(sc)) {
            if (AH_TRUE == ath_hal_is_mac_hung(sc->sc_ah)) {
                ATH_MAC_GENERIC_HANG(sc);
                return 1;
            }
        }
    }
    return 0;
}

bool
ath_gpio_config(struct ath_softc *sc, u_int pin, u_int type)
{
    switch (pin) {
    case ATH_GPIO_PIN_INPUT:
        return ath_hal_gpioCfgInput(sc->sc_ah, pin);
    case ATH_GPIO_PIN_OUTPUT:
        return ath_hal_gpioCfgOutput(sc->sc_ah, pin,
                                     0 /* AR_GPIO_OUTPUT_MUX_AS_OUTPUT */);
    default:
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid type %d for GPIO pin %d\n",
                __func__, type, pin);
    }
    return -EINVAL;
}

int
ath_gpio_register_callback(struct ath_softc *sc, unsigned int mask,
                           ath_gpio_intr_func_t func, void *context)
{
    ath_gpio_callback_t *handle;
    int i;
    unsigned int valid_mask = ath_hal_gpioGetMask(sc->sc_ah);

    if (mask & ~valid_mask) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: Invalid GPIO Mask required: 0x%x\n",
                __func__, mask & ~valid_mask);
        return -EINVAL;
    }
    for (i = 0; i < ATH_GPIO_MAX_NUM; i++) {
        if (mask & (1 << i)) {
            handle = &sc->sc_gpio_intr_handle[i];
            handle->func = func;
            handle->context = context;
        }
    }
    return 0;
}

u_int32_t ath_gpio_get_intr(struct ath_softc *sc)
{
    return ath_hal_gpioGetIntr(sc->sc_ah);
}

void ath_gpio_set_intr(struct ath_softc *sc, u_int pin, u_int32_t ilevel)
{
    /*
     * Ath layer marks the polarity of the corresponding GPIO bit as:
     * low -> 1; high -> 0
     */
    if (ilevel == ATH_GPIO_INTR_LOW) {
        sc->sc_gpio_polarity_map |= 1 << pin;
        sc->sc_gpio_mask |= 1 << pin;
    } else if (ilevel == ATH_GPIO_INTR_HIGH) {
        sc->sc_gpio_polarity_map &= ~(1 << pin);
        sc->sc_gpio_mask |= 1 << pin;
    } else { /* ATH_GPIO_INTR_DISABLE */
        sc->sc_gpio_mask &= ~(1 << pin);
    }

    /* Mask the interrupt mask bit so that it won't get lost. */
    if (sc->sc_gpio_mask)
        sc->sc_imask |= HAL_INT_GPIO;
    else
        sc->sc_imask &= ~HAL_INT_GPIO;

    ath_hal_gpioSetMask(sc->sc_ah, sc->sc_gpio_mask, sc->sc_gpio_polarity_map);

    if (!sc->sc_invalid) {
        /* Delay interrupt enabling until sc becomes valid. */
        ath_hal_gpioSetIntr(sc->sc_ah, pin, ilevel);
    }
}

static void
ath_gpio_handle_intr(struct ath_softc *sc)
{
    ath_gpio_callback_t *handle;
    int i;
    unsigned int mask;
    unsigned int polarity = ath_hal_gpioGetPolarity(sc->sc_ah);

    mask = ath_hal_gpioGetIntr(sc->sc_ah);
    mask &= sc->sc_gpio_mask;

    for (i = 0; i < ATH_GPIO_MAX_NUM; i++) {
        if (mask & (1 << i)) {
            if ((1 << i) & (polarity ^ sc->sc_gpio_polarity_map)) {
                /* polarity mismatch */
                continue;
            }
            handle = &sc->sc_gpio_intr_handle[i];
            if (handle && handle->func) {
                handle->func(handle->context, i, mask);
            }
        }
    }
    /*
     * Flip polarity for all the GPIO pins recevie interrupts so that
     * interrupts won't be keep firing for them.
     */
    ath_hal_gpioSetPolarity(sc->sc_ah, polarity ^ mask, mask);
}

/*
 * Deferred interrupt processing
 */
#if !ATH_DRIVER_SIM
static
#endif
void
ath_handle_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_intr_status_params params;
    u_int32_t status;
    u_int32_t rxmask;
    struct hal_bb_panic_info hal_bb_panic;
    struct ath_bb_panic_info bb_panic;
    int i;

    ATH_INTR_STATUS_LOCK(sc);
    /* Must synchronize with the ISR */
    params.sc = sc;
    params.intr_status = 0;
    OS_EXEC_INTSAFE(sc->sc_osdev, ath_update_intrstatus_intsafe, &params);
    status = params.intr_status;
    ATH_INTR_STATUS_UNLOCK(sc);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_dpc_processing);
    if (atomic_read(&sc->sc_dpc_stop)) {
        atomic_dec(&sc->sc_dpc_processing);
        return;
    }
#endif
    ATH_PS_WAKEUP(sc);

    do {
        if (sc->sc_invalid) {
            /*
             * The hardware is not ready/present, don't touch anything.
             * Note this can happen early on if the IRQ is shared.
             */
            DPRINTF(sc, ATH_DEBUG_INTR, "%s called when invalid.\n",__func__);
            break;
        }

        if (status & HAL_INT_FATAL) {
            /* need a chip reset */
            DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got fatal intr\n", __func__);
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
            break;
        } else {
            if (status & HAL_INT_BBPANIC) {
                if (!ath_hal_get_bbpanic_info(sc->sc_ah, &hal_bb_panic)) {
                    bb_panic.status = hal_bb_panic.status;
                    bb_panic.tsf = hal_bb_panic.tsf;
                    bb_panic.wd = hal_bb_panic.wd;
                    bb_panic.det = hal_bb_panic.det;
                    bb_panic.rdar = hal_bb_panic.rdar;
                    bb_panic.r_odfm = hal_bb_panic.r_odfm;
                    bb_panic.r_cck = hal_bb_panic.r_cck;
                    bb_panic.t_odfm = hal_bb_panic.t_odfm;
                    bb_panic.t_cck = hal_bb_panic.t_cck;
                    bb_panic.agc = hal_bb_panic.agc;
                    bb_panic.src = hal_bb_panic.src;
                    bb_panic.phy_panic_wd_ctl1 = hal_bb_panic.phy_panic_wd_ctl1;
                    bb_panic.phy_panic_wd_ctl2 = hal_bb_panic.phy_panic_wd_ctl2;
                    bb_panic.phy_gen_ctrl = hal_bb_panic.phy_gen_ctrl;
                    bb_panic.cycles = hal_bb_panic.cycles;
                    bb_panic.rxc_pcnt = hal_bb_panic.rxc_pcnt;
                    bb_panic.rxf_pcnt = hal_bb_panic.rxf_pcnt;
                    bb_panic.txf_pcnt = hal_bb_panic.txf_pcnt;
                    bb_panic.valid = 1;

                    for (i = 0; i < MAX_BB_PANICS - 1; i++)
                        sc->sc_stats.ast_bb_panic[i] =
                                        sc->sc_stats.ast_bb_panic[i + 1];
                    sc->sc_stats.ast_bb_panic[MAX_BB_PANICS - 1] = bb_panic;
                }

                if (!(ath_hal_handle_radar_bbpanic(sc->sc_ah)) ){
                    /* reset to recover from the BB hang */
                    sc->sc_reset_type = ATH_RESET_NOLOSS;
                    ATH_RESET_LOCK(sc);
                    ath_hal_set_halreset_reason(sc->sc_ah, HAL_RESET_BBPANIC);
                    ATH_RESET_UNLOCK(sc);
                    ath_internal_reset(sc);
                    ATH_RESET_LOCK(sc);
                    ath_hal_clear_halreset_reason(sc->sc_ah);
                    ATH_RESET_UNLOCK(sc);
                    sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
                    pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
                    sc->sc_stats.ast_resetOnError++;
#endif
                    /* EV92527 -- we are doing internal reset. break out */
                    break;
                }
                /* EV 92527 -- We are not doing any internal reset, continue normally */
            }
#ifdef ATH_BEACON_DEFERRED_PROC
            /* Handle SWBA first */
            if (status & HAL_INT_SWBA) {
                int needmark = 0;
                ath_beacon_tasklet(sc, &needmark);
            }
#endif

            if (((AH_TRUE == sc->sc_hang_check) && ath_hw_hang_check(sc)) ||
                (!sc->sc_noreset && sc->sc_nvaps && (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)))) {
                ath_bstuck_tasklet(sc);
                ATH_CLEAR_HANGS(sc);
                break;
            }
            /*
             * Howl needs DDR FIFO flush before any desc/dma data can be read.
             */
            ATH_FLUSH_FIFO();
            if (sc->sc_enhanceddmasupport) {
                rxmask = (HAL_INT_RXHP | HAL_INT_RXLP | HAL_INT_RXEOL | HAL_INT_RXORN);
            } else {
                rxmask = (HAL_INT_RX | HAL_INT_RXEOL | HAL_INT_RXORN);
            }

            if (status & rxmask
#if ATH_SUPPORT_RX_PROC_QUOTA
                    || (sc->sc_rx_work_lp) || (sc->sc_rx_work_hp)
#endif
                    ) {
                if (status & HAL_INT_RXORN) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got RXORN intr\n", __func__);
                }
                if (status & HAL_INT_RXEOL) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got RXEOL intr\n", __func__);
                }
#if ATH_SUPPORT_RX_PROC_QUOTA
                sc->sc_rx_work_lp=0;
                sc->sc_rx_work_hp=0;
#endif
                ath_handle_rx_intr(sc);
            }
            if (sc->sc_rxfreebuf != NULL) {
                DPRINTF(sc, ATH_DEBUG_INTR, "%s[%d] ---- Athbuf FreeQ Not Empty - Calling AllocRxbufs for FreeList \n", __func__, __LINE__);
                // There are athbufs with no associated mbufs. Let's try to allocate some mbufs for these.
                if (sc->sc_enhanceddmasupport) {
                    ath_edmaAllocRxbufsForFreeList(sc);
                }
                else {
                    ath_allocRxbufsForFreeList(sc);
                }
            }
#if ATH_TX_POLL
            ath_handle_tx_intr(sc);
#else
            if (status & HAL_INT_TX) {
                sc->sc_consecutive_gtt_count = 0;
#ifdef ATH_TX_INACT_TIMER
                sc->sc_tx_inact = 0;
#endif
                ath_handle_tx_intr(sc);
            }
#endif
            if (status & HAL_INT_BMISS) {
                ath_bmiss_tasklet(sc);
            }

#define MAX_CONSECUTIVE_GTT_COUNT 5
            if (status & HAL_INT_GTT) {
                DPRINTF(sc, ATH_DEBUG_INTR, " GTT %d\n",sc->sc_consecutive_gtt_count);
                sc->sc_consecutive_gtt_count++;
                if(sc->sc_consecutive_gtt_count >= MAX_CONSECUTIVE_GTT_COUNT) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Consecutive GTTs\n", __func__);
                    if (ath_hw_tx_hang_check(sc)) {
                        /* reset the chip */
                        sc->sc_reset_type = ATH_RESET_NOLOSS;
                        ath_internal_reset(sc);
                        sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
                        pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
                        sc->sc_stats.ast_resetOnError++;
#endif
                        DPRINTF(sc, ATH_DEBUG_INTR, "%s: --- TX Hang detected resetting chip --- \n",__func__);
                    }
                    sc->sc_consecutive_gtt_count = 0;
                }
            }
#if UMAC_SUPPORT_STA_MODE
            if (status & HAL_INT_BRSSI) {
                ath_brssi_tasklet(sc);
            }
#endif
            if (status & HAL_INT_CST) {
                ath_txto_tasklet(sc);
            }
            if (status & HAL_INT_TIM) {
                if (sc->sc_ieee_ops->proc_tim) {
                    sc->sc_ieee_ops->proc_tim(sc->sc_ieee);
                }
            }
#if UMAC_SUPPORT_STA_MODE
            if (status & (HAL_INT_DTIM | HAL_INT_DTIMSYNC)) {
                if (sc->sc_ieee_ops->proc_dtim) {
                    sc->sc_ieee_ops->proc_dtim(sc->sc_ieee);
                }
            }
#endif
            if (status & HAL_INT_GPIO) {
#ifdef ATH_RFKILL
                ath_rfkill_gpio_intr(sc);
#endif
#ifdef ATH_BT_COEX
                if (sc->sc_btinfo.bt_gpioIntEnabled) {
                    ath_bt_coex_gpio_intr(sc);
                }
#endif
                ath_gpio_handle_intr(sc);
            }

#ifdef ATH_GEN_TIMER

            if (status & HAL_INT_TSFOOR) {
                /* There is a jump in the TSF time with this OUT OF RANGE interupt. */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: Got HAL_INT_TSFOOR intr\n", __func__);

                /* If the current mode is Station, then we need to reprogram the beacon timers. */
                if (sc->sc_opmode  ==  HAL_M_STA) {
                    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
                }

                ath_gen_timer_tsfoor_isr(sc);
            }

            if (status & HAL_INT_GENTIMER) {
                #ifdef ATH_USB
                ath_gen_timer_isr(sc,0,0,0);
                #else
                ath_gen_timer_isr(sc);
                #endif
            }
#endif
        }
#if ATH_SUPPORT_MCI
        if (status & HAL_INT_MCI) {
            ath_coex_mci_intr(sc);
        }
#endif

#if ATH_GEN_RANDOMNESS
        if (!(status & HAL_INT_RXHP)) {
            ath_gen_randomness(sc);
        }
#endif

#if ATH_RESET_SERIAL
        ATH_RESET_LOCK(sc);
        if (atomic_read(&sc->sc_dpc_stop)) {
            ATH_RESET_UNLOCK(sc);
            break;
        }
#endif
        /* re-enable hardware interrupt */
        if (sc->sc_enhanceddmasupport) {
            /* For enhanced DMA, certain interrupts are already enabled (e.g. RXEOL),
             * but now re-enable _all_ interrupts.
             * Note: disable and then enable to satisfy the global isr enable reference counter.
             */
            ath_hal_intrset(sc->sc_ah, 0);
            ath_hal_intrset(sc->sc_ah, sc->sc_imask);
        } else {
            ath_hal_intrset(sc->sc_ah, sc->sc_imask);
        }
#if ATH_RX_LOOPLIMIT_TIMER
    if (sc->sc_rx_looplimit) {
        if (sc->sc_rx_looplimit_timer->cached_state.active != true) {
            ath_gen_timer_stop(sc, sc->sc_rx_looplimit_timer);
            ath_gen_timer_start(sc,
                                sc->sc_rx_looplimit_timer,
                                ath_gen_timer_gettsf32(sc, sc->sc_rx_looplimit_timer)
                                + sc->sc_rx_looplimit_timeout,
                                0); /* one shot */
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_rx_looplimit_start_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_looplimit_start ++;
#endif
        }
    }
#endif
#if ATH_RESET_SERIAL
        ATH_RESET_UNLOCK(sc);
#endif
    } while (FALSE);

    ATH_PS_SLEEP(sc);
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_dpc_processing);
#endif
}
/*
 * ATH symbols exported to the HAL.
 * Make table static so that clients can keep a pointer to it
 * if they so choose.
 */
static u_int32_t
ath_read_pci_config_space(struct ath_softc *sc, u_int32_t offset, void *buffer, u_int32_t len)
{
    return OS_PCI_READ_CONFIG(sc->sc_osdev, offset, buffer, len);
}

static const struct ath_hal_callback halCallbackTable = {
    /* Callback Functions */
    /* read_pci_config_space */ (HAL_BUS_CONFIG_READER)ath_read_pci_config_space
};

static int
ath_get_caps(ath_dev_t dev, ATH_CAPABILITY_TYPE type)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int supported = 0;

    switch (type) {
    case ATH_CAP_FF:
        supported = ath_hal_fastframesupported(ah);
        break;
    case ATH_CAP_BURST:
        supported = ath_hal_burstsupported(ah);
        break;
    case ATH_CAP_TURBO:
        supported = ath_hal_turboagsupported(ah, sc->sc_config.ath_countrycode);
        break;
    case ATH_CAP_XR:
        supported = ath_hal_xrsupported(ah);
        break;
    case ATH_CAP_TXPOWER:
        supported = (sc->sc_hastpc || ath_hal_hastxpowlimit(ah));
        break;
    case ATH_CAP_DIVERSITY:
        supported = sc->sc_diversity;
        break;
    case ATH_CAP_BSSIDMASK:
        supported = sc->sc_hasbmask;
        break;
    case ATH_CAP_TKIP_SPLITMIC:
        supported = ath_hal_tkipsplit(ah);
        break;
    case ATH_CAP_MCAST_KEYSEARCH:
        supported = ath_hal_getmcastkeysearch(ah);
        break;
    case ATH_CAP_TKIP_WMEMIC:
        supported = ath_hal_wmetkipmic(ah);
        break;
    case ATH_CAP_WMM:
        supported = sc->sc_haswme;
        break;
    case ATH_CAP_HT:
        supported = sc->sc_hashtsupport;
        break;
    case ATH_CAP_HT20_SGI:
        supported = sc->sc_ht20sgisupport;
        break;
    case ATH_CAP_RX_STBC:
        supported = sc->sc_rxstbcsupport;
        break;
    case ATH_CAP_TX_STBC:
        supported = sc->sc_txstbcsupport;
        break;
    case ATH_CAP_LDPC:
        supported = sc->sc_ldpcsupport;
        break;
    case ATH_CAP_4ADDR_AGGR:
        supported = ath_hal_4addraggrsupported(ah);
        break;
    case ATH_CAP_HW_BEACON_PROC_SUPPORT:
        supported = ath_hal_hashwbeaconprocsupport(ah);
        break;
    case ATH_CAP_WPS_BUTTON:
        if (ath_hal_haswpsbutton(ah) && sc->sc_reg_parm.wpsButtonGpio) {

            /* Overload push button status when reporting capability */
            supported |= (ATH_WPS_BUTTON_EXISTS | ATH_WPS_BUTTON_DOWN_SUP | ATH_WPS_BUTTON_STATE_SUP);

            supported |= (sc->sc_wpsbuttonpushed ? ATH_WPS_BUTTON_PUSHED : 0);
            sc->sc_wpsbuttonpushed = 0;  /* Clear status after query */

            /* Get current push button status, GPIO polarity low */
            if (ath_hal_gpioget(sc->sc_ah, sc->sc_reg_parm.wpsButtonGpio)) {
                /* GPIO line normal, re-enable GPIO interrupt */
                ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_LOW);
                sc->sc_wpsgpiointr = 1;
            } else {
                supported |= ATH_WPS_BUTTON_PUSHED_CURR;
            }
        }
        break;
    case ATH_CAP_MBSSID_AGGR_SUPPORT:/* MBSSID Aggregation support capability*/
          supported = ath_hal_hasMbssidAggrSupport(ah);
          break;
#ifdef ATH_SWRETRY
    case ATH_CAP_SWRETRY_SUPPORT:
        supported = sc->sc_swRetryEnabled;
        break;
#endif
#ifdef ATH_SUPPORT_UAPSD
    case ATH_CAP_UAPSD:
        supported = sc->sc_uapsdsupported;
        break;
#endif
    case ATH_CAP_DYNAMIC_SMPS:
        supported = ath_hal_hasdynamicsmps(ah);
        break;
    case ATH_CAP_EDMA_SUPPORT:
        supported = ath_hal_hasenhanceddmasupport(ah);
        break;
    case ATH_CAP_WEP_TKIP_AGGR:
        supported = ath_hal_weptkipaggrsupport(ah);
        break;
    case ATH_CAP_DS:
        supported = ath_hal_getcapability(ah, HAL_CAP_DS, 0, NULL) == HAL_OK ? TRUE : FALSE;
        break;
    case ATH_CAP_TS:
        supported = ath_hal_getcapability(ah, HAL_CAP_TS, 0, NULL) == HAL_OK ? TRUE : FALSE;
        break;
    case ATH_CAP_NUMTXCHAINS:
        {
            int chainmask, ret;
            ret = ath_hal_gettxchainmask(ah, (u_int32_t *)&chainmask);
            supported = 0;
            if(sc->sc_tx_chainmask != chainmask)
            {
                printk("%s[%d] tx chainmask mismatch actual %d sc_chainmak %d\n", __func__, __LINE__,
                    chainmask, sc->sc_tx_chainmask);
            }
            switch (chainmask) {
            case 1:
                supported = 1;
                break;
            case 3:
            case 5:
            case 6:
                supported = 2;
                break;
            case 7:
                supported = 3;
                break;
            }
        }
        break;
    case ATH_CAP_NUMRXCHAINS:
        {
            int chainmask, ret;
            ret = ath_hal_getrxchainmask(ah, (u_int32_t *)&chainmask);
            supported = 0;
            if(sc->sc_rx_chainmask != chainmask)
            {
                printk("%s[%d] rx chainmask mismatch actual %d sc_chainmak %d\n", __func__, __LINE__,
                    chainmask, sc->sc_rx_chainmask);
            }
            switch (chainmask) {
            case 1:
                supported = 1;
                break;
            case 3:
            case 5:
            case 6:
                supported = 2;
                break;
            case 7:
                supported = 3;
                break;
            }
        }
        break;
    case ATH_CAP_RXCHAINMASK:
        supported = sc->sc_rx_chainmask;
        //ath_hal_gettxchainmask(ah, (u_int32_t *)&supported);
    break;
    case ATH_CAP_TXCHAINMASK:
        if (sc->sc_tx_chainmaskopt)
            supported = sc->sc_tx_chainmaskopt;
        else
        supported = sc->sc_tx_chainmask;
    break;
    case ATH_CAP_DEV_TYPE:
        ath_hal_getcapability(ah, HAL_CAP_DEVICE_TYPE, 0, (u_int32_t *)&supported);
        break;
    case ATH_CAP_EXTRADELIMWAR:
        ath_hal_getcapability(ah, HAL_CAP_EXTRADELIMWAR, 0,
                    (u_int32_t*)&supported);
        break;
#if ATH_SUPPORT_WRAP
    case ATH_CAP_PROXYSTARXWAR:
        ath_hal_getcapability(ah, HAL_CAP_PROXYSTARXWAR, 0,
                    (u_int32_t*)&supported);
        break;
#endif
#ifdef ATH_SUPPORT_TxBF
    case ATH_CAP_TXBF:
        supported = ath_hal_hastxbf(ah);
        break;
#endif
    case ATH_CAP_LDPCWAR:
        supported = ath_hal_getcapability(ah, HAL_CAP_LDPCWAR, 0, 0);
        break;
    case ATH_CAP_ZERO_MPDU_DENSITY:
        supported = sc->sc_ent_min_pkt_size_enable;
        break;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    case ATH_CAP_DYN_TXCHAINMASK:
        supported = sc->sc_dyn_txchainmask;
        break;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
#if ATH_SUPPORT_WAPI
    case ATH_CAP_WAPI_MAXTXCHAINS:
        ath_hal_getwapimaxtxchains(ah, (u_int32_t *)&supported);
        break;
    case ATH_CAP_WAPI_MAXRXCHAINS:
        ath_hal_getwapimaxrxchains(ah, (u_int32_t *)&supported);
        break;
#endif
    case ATH_CAP_PN_CHECK_WAR:
        if (ath_hal_getcapability(ah, HAL_CAP_PN_CHECK_WAR_SUPPORT, 0, NULL) == HAL_OK )
            supported = TRUE ;
        else
            supported = FALSE;
        break;
    default:
        supported = 0;
        break;
    }
    return supported;
}

static void
ath_suspend_led_control(ath_dev_t dev)
{
    ath_led_suspend(dev);
}

static int
ath_get_ciphers(ath_dev_t dev, HAL_CIPHER cipher)
{
    return ath_hal_ciphersupported(ATH_DEV_TO_SC(dev)->sc_ah, cipher);
}

static struct ath_phy_stats *
ath_get_phy_stats(ath_dev_t dev, WIRELESS_MODE wmode)
{
    return &(ATH_DEV_TO_SC(dev)->sc_phy_stats[wmode]);
}

static struct ath_phy_stats *
ath_get_phy_stats_allmodes(ath_dev_t dev)
{
    return (ATH_DEV_TO_SC(dev)->sc_phy_stats);
}

static struct ath_stats *
ath_get_ath_stats(ath_dev_t dev)
{
    return &(ATH_DEV_TO_SC(dev)->sc_stats);
}

static struct ath_11n_stats *
ath_get_11n_stats(ath_dev_t dev)
{
    return &(ATH_DEV_TO_SC(dev)->sc_stats.ast_11n_stats);
}

static struct ath_dfs_stats *
ath_get_dfs_stats(ath_dev_t dev)
{
    return &(ATH_DEV_TO_SC(dev)->sc_stats.ast_dfs_stats);

}

static void
ath_clear_stats(ath_dev_t dev)
{
    OS_MEMZERO(&(ATH_DEV_TO_SC(dev)->sc_phy_stats[0]),
               sizeof(struct ath_phy_stats) * WIRELESS_MODE_MAX);
}

static void
ath_update_beacon_info(ath_dev_t dev, int avgbrssi)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Update beacon RSSI */
    sc->sc_halstats.ns_avgbrssi = avgbrssi;

    if (! sc->sc_hasautosleep) {
        sc->sc_waitbeacon = 0;
    }
    ath_hal_chk_rssi(sc->sc_ah, ATH_RSSI_OUT(avgbrssi));
}
static void
ath_update_txpow_mgmt(ath_dev_t dev, int if_id, int frame_subtype,u_int8_t transmit_power)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;

    avp = sc->sc_vaps[if_id];
    avp->av_txpow_mgt_frm[(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;
}

static void
ath_update_txpow_ctrl(ath_dev_t dev, int if_id, int frame_type, int frame_subtype,u_int8_t transmit_power)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    u_int32_t loop_index;

    avp = sc->sc_vaps[if_id];
    if (0xff == frame_subtype) {
        for (loop_index = 0; loop_index < MAX_NUM_TXPOW_MGT_ENTRY; loop_index ++) {
            avp->av_txpow_frm[frame_type >> IEEE80211_FC0_TYPE_SHIFT][loop_index]=transmit_power;
        }
    } else {
        avp->av_txpow_frm[frame_type >> IEEE80211_FC0_TYPE_SHIFT][(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;
    }
}

#if ATH_SUPPORT_WIFIPOS
static void
ath_update_loc_ctl_reg(ath_dev_t dev, int pos_bit)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_update_loc_ctl_reg(sc->sc_ah, pos_bit);
}
static u_int32_t
ath_read_loc_timer_reg(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_read_loc_timer_reg(sc->sc_ah);
}
static u_int8_t
ath_get_eeprom_chain_mask(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_get_eeprom_chain_mask(sc->sc_ah);
}
#endif

static void
ath_set_macmode(ath_dev_t dev, HAL_HT_MACMODE macmode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_FUNC_ENTRY_VOID(sc);
    ATH_PS_WAKEUP(sc);
    ath_hal_set11nmac2040(sc->sc_ah, macmode);
    ATH_PS_SLEEP(sc);
}

static void
ath_set_extprotspacing(ath_dev_t dev, HAL_HT_EXTPROTSPACING extprotspacing)
{
    ATH_DEV_TO_SC(dev)->sc_ht_extprotspacing = extprotspacing;
}

static int
ath_get_extbusyper(ath_dev_t dev)
{
    return ath_hal_get11nextbusy(ATH_DEV_TO_SC(dev)->sc_ah);
}

static unsigned int
ath_get_chbusyper(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return sc->sc_chan_busy_per;
}

#ifndef REMOVE_PKT_LOG
static int
ath_start_pktlog(void *scn, int log_state)
//ath_start_pktlog(struct ath_softc *sc, int log_state)
{
    int error;
    //struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_pktlog_start(scn, log_state, error);
    return error;
}

static int
ath_read_pktlog_hdr(ath_dev_t dev, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len)
{
    int error;
    ath_pktlog_read_hdr(ATH_DEV_TO_SC(dev), buf, buf_len,
                        required_len, actual_len, error);
    return error;
}

static int
ath_read_pktlog_buf(ath_dev_t dev, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len)
{
    int error;
    ath_pktlog_read_buf(ATH_DEV_TO_SC(dev), buf, buf_len,
                        required_len, actual_len, error);
    return error;
}
#endif

#if ATH_SUPPORT_IQUE
static void
ath_setacparams(ath_dev_t dev, u_int8_t ac, u_int8_t use_rts,
                u_int8_t aggrsize_scaling, u_int32_t min_kbps)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (ac < WME_NUM_AC) {
        sc->sc_ac_params[ac].use_rts = use_rts;
        sc->sc_ac_params[ac].min_kbps = min_kbps;
        sc->sc_ac_params[ac].aggrsize_scaling = aggrsize_scaling;
    }
}

static void
ath_setrtparams(ath_dev_t dev, u_int8_t rt_index, u_int8_t perThresh,
                u_int8_t probeInterval)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (rt_index < 2) {
        sc->sc_rc_params[rt_index].per_threshold = perThresh;
        sc->sc_rc_params[rt_index].probe_interval = probeInterval;
    }
}

static void
ath_sethbrparams(ath_dev_t dev, u_int8_t ac, u_int8_t enable, u_int8_t perlow)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t perhigh;
    perhigh = sc->sc_rc_params[0].per_threshold;
    if (perhigh < sc->sc_rc_params[1].per_threshold)
        perhigh = sc->sc_rc_params[1].per_threshold;

    /* Now we only support HBR for VI */
    if (ac == WME_AC_VI) {
        sc->sc_hbr_params[ac].hbr_enable = enable;
        sc->sc_hbr_params[ac].hbr_per_low = (perlow > (perhigh - 10))? (perhigh-10):perlow;
    }
}

/*
 * Dump all AC and rate control parameters related to IQUE.
 */
static void
ath_getiqueconfig(ath_dev_t dev)
{
#if ATH_DEBUG
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    printk("\n========= Rate Control Table Config========\n");
    printk("AC\tPER\tProbe Interval\n\n");
    printk("BE&BK\t%d\t\t%d \n",
           sc->sc_rc_params[0].per_threshold,
           sc->sc_rc_params[0].probe_interval);
    printk("VI&VO\t%d\t\t%d \n",
           sc->sc_rc_params[1].per_threshold,
           sc->sc_rc_params[1].probe_interval);

    printk("\n========= Access Category Config===========\n");
    printk("AC\tRTS \tAggr Scaling\tMin Rate(Kbps)\tHBR \tPER LO \tRXTIMEOUT\n\n");
    printk("BE\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[0].use_rts?"Yes":"No",
           sc->sc_ac_params[0].aggrsize_scaling,
           sc->sc_ac_params[0].min_kbps,
           sc->sc_hbr_params[0].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[0].hbr_per_low,
           sc->sc_rxtimeout[0]);
    printk("BK\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[1].use_rts?"Yes":"No",
           sc->sc_ac_params[1].aggrsize_scaling,
           sc->sc_ac_params[1].min_kbps,
           sc->sc_hbr_params[1].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[1].hbr_per_low,
           sc->sc_rxtimeout[1]);
    printk("VI\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[2].use_rts?"Yes":"No",
           sc->sc_ac_params[2].aggrsize_scaling,
           sc->sc_ac_params[2].min_kbps,
           sc->sc_hbr_params[2].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[2].hbr_per_low,
           sc->sc_rxtimeout[2]);
    printk("VO\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[3].use_rts?"Yes":"No",
           sc->sc_ac_params[3].aggrsize_scaling,
           sc->sc_ac_params[3].min_kbps,
           sc->sc_hbr_params[3].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[3].hbr_per_low,
           sc->sc_rxtimeout[3]);
#endif /* ATH_DEBUG */
}

#endif

static void
ath_setrxtimeout(ath_dev_t dev, u_int8_t ac, u_int8_t rxtimeout)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (ac < WME_NUM_AC) {
        sc->sc_rxtimeout[ac] = rxtimeout;
    }
}

static void
ath_set_ps_awake(ath_dev_t dev)
{
    ath_pwrsave_awake(dev);
}

static void
ath_set_ps_netsleep(ath_dev_t dev)
{
    ath_pwrsave_netsleep(dev);
}

static void
ath_set_ps_fullsleep(ath_dev_t dev)
{
    ath_pwrsave_fullsleep(dev);
}

static int
ath_get_ps_state(ath_dev_t dev)
{
    int state;
    state = ath_pwrsave_get_state(dev);
    return state;
}

static void
ath_set_ps_state(ath_dev_t dev, int newstateint)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_LOCK(sc);
    ath_pwrsave_set_state(dev, newstateint);
    ATH_PS_UNLOCK(sc);
}

#if QCA_AIRTIME_FAIRNESS
QDF_STATUS
ath_atf_realign_tokens(struct ath_node *an, u_int8_t tidno,
                       u_int32_t actual_duration,
                       u_int32_t est_duration)
{
    int8_t ac = WME_AC_BE;
    struct ath_softc *sc = an->an_sc;

    ac = TID_TO_WME_AC(tidno);
    if (sc->sc_ieee_ops->atf_adjust_tokens) {
        return sc->sc_ieee_ops->atf_adjust_tokens(an->an_node, ac,
                                     actual_duration, est_duration);
    }

    return QDF_STATUS_E_INVAL;
}

QDF_STATUS
ath_atf_account_tokens( ath_atx_tid_t *tid, u_int32_t pkt_duration)
{
    int8_t ac = WME_AC_BE;
    struct ath_node *an = tid->an;
    struct ath_softc *sc = an->an_sc;

    ac = TID_TO_WME_AC(tid->tidno);

    if (sc->sc_ieee_ops->atf_account_tokens) {
        return sc->sc_ieee_ops->atf_account_tokens(an->an_node, ac,
                                                   pkt_duration);
    }

    return QDF_STATUS_E_INVAL;
}

static void ath_atf_node_unblock(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_atf_sc_node_unblock(sc, an);
}

static void ath_atf_tokens_unassigned(ath_dev_t dev , u_int32_t tokens_unassigned)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_atf_sc_tokens_unassigned(sc, tokens_unassigned);
}

static void ath_atf_set_clear(ath_dev_t dev, u_int8_t enable_disable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_atf_sc_set_clear(sc, enable_disable);
}

static u_int32_t ath_atf_debug_nodestate(ath_node_t node, struct atf_peerstate *atfpeerstate)
{
    struct ath_node *an = ATH_NODE(node);

    return ath_atf_debug_node_state(an, atfpeerstate);
}

static u_int8_t ath_atf_tokens_used(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    return ath_atf_all_tokens_used(an);
}

static void ath_atf_node_resume( ath_node_t node )
{
    struct ath_node *an = ATH_NODE(node);
    ath_atf_sc_node_resume(an);
}

static void ath_atf_capable_node( ath_node_t node, u_int8_t val, u_int8_t atfstate_change)
{
    struct ath_node *an = ATH_NODE(node);
    ath_atf_sc_capable_node(an, val, atfstate_change);
}

static u_int32_t ath_atf_airtime_estimate(ath_dev_t dev,
                         ath_node_t node, u_int32_t tput, u_int32_t *possible_tput)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    return ath_rate_atf_airtime_estimate(sc, an, tput, possible_tput);
}
#endif

static int16_t ath_dev_noisefloor( ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int16_t nf;
    ATH_PS_WAKEUP(sc);
    nf = ath_hal_get_noisefloor(sc->sc_ah);
    ATH_PS_SLEEP(sc);
    return nf;
}

static void  ath_dev_get_chan_stats( ath_dev_t dev, void *chan_stats )
{
    HAL_VOWSTATS pcounts;
    struct ath_chan_stats *cur_chan_stats = ( struct ath_chan_stats *)chan_stats;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_getVowStats((ATH_DEV_TO_SC(dev))->sc_ah,&pcounts, (AR_REG_RX_CLR_CNT | AR_REG_CYCLE_CNT) );
    cur_chan_stats->cycle_cnt = pcounts.cycle_count;
    cur_chan_stats->chan_clr_cnt = pcounts.rx_clear_count;
    cur_chan_stats->duration_11b_data = sc->sc_11b_data_dur;

#if 0
    printk ( "cnt %d %d %d %d %d \n",  pcounts.tx_frame_count, pcounts.rx_frame_count,  pcounts.rx_clear_count,  pcounts.cycle_count,  pcounts.ext_cycle_count);
    printk ( "cnt %d %d %d %d %d \n",  pcounts.tx_frame_count, pcounts.rx_frame_count,  pcounts.rx_clear_count,  pcounts.cycle_count,  pcounts.ext_rx_cycle_count);
    /*phy error count will be added later.*/
    struct ath_phy_stats phy_stats = (ATH_DEV_TO_SC(dev))->sc_phy_stats[(ATH_DEV_TO_SC(dev))->sc_curmode];
    printk ( "err %d %d %llu %llu %x \n",  (u_int64_t)phy_stats.ast_rx_err, phy_stats.ast_rx_crcerr,  phy_stats.ast_rx_fifoerr,  phy_stats.ast_rx_phyerr,  phy_stats.ast_rx_decrypterr);
#endif
}

static int
ath_set_proxysta(ath_dev_t dev, int enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (enable ^ sc->sc_enableproxysta) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: set proxy sta mode to %d\n", __func__, enable);
        sc->sc_enableproxysta = !!enable;

        if (!sc->sc_invalid && sc->sc_nvaps) {
            sc->sc_full_reset = 1;
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
            ATH_INTERNAL_RESET_LOCK(sc);
#endif
            ath_reset_start(sc, 0, 0, 0);
            ath_reset(sc);
            ath_reset_end(sc, 0);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
            ATH_INTERNAL_RESET_UNLOCK(sc);
#endif
        }
    }
    return 0;
}

static u_int32_t ath_get_goodput(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    if (an) return an->an_rc_node->txRateCtrl.rptGoodput;
    return 0;
}

#if ATH_SUPPORT_FLOWMAC_MODULE
/*
 * Get the flow control feature enabled state
 */
int
ath_get_flowmac_enabled_state(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return sc->sc_osnetif_flowcntrl;
}

void
ath_netif_flowmac_pause(ath_dev_t dev, int stall)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    flowmac_pause(sc->sc_flowmac_dev, stall,
            stall ? FLOWMAC_PAUSE_TIMEOUT:0);
}
void
ath_netif_stop_queue(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!sc->sc_devstopped) {
        OS_NETIF_STOP_QUEUE(sc);
        sc->sc_devstopped = 1;
        sc->sc_stats.ast_tx_stop++;
        if (sc->sc_flowmac_debug) {
            DPRINTF(sc, ATH_DEBUG_XMIT, "devid 0x%pK stopped stop_count %d\n",
                    sc, sc->sc_stats.ast_tx_stop);
    }
    }
}

void
ath_netif_wake_queue(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_devstopped) {
        OS_NETIF_WAKE_QUEUE(sc);
        sc->sc_devstopped = 0;
        sc->sc_stats.ast_tx_resume++;

        if (sc->sc_flowmac_debug) {
            DPRINTF(sc, ATH_DEBUG_XMIT, "devid 0x%pK resume %d\n",
                    sc, sc->sc_stats.ast_tx_resume);
    }

}
}

void
ath_netif_set_wdtimeout(ath_dev_t dev, int val)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_SET_WDTIMEOUT(sc, val);
    }
}

void
ath_netif_clr_flag(ath_dev_t dev, unsigned int flag)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_CLR_NETDEV_FLAG(sc, flag);
    }
}

void
ath_netif_update_trans(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_NETDEV_UPDATE_TRANS(sc);
    }
}
#endif

#if UMAC_SUPPORT_VI_DBG
/* This function is used to reset video debug static variables in lmac
 * related to logging pkt stats using athstats
 */
static void ath_set_vi_dbg_restart(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_vi_dbg_start = 0;
}

/* This function is used to enable logging of rx pkt stats using
 * athstats
 */
static void ath_set_vi_dbg_log(ath_dev_t dev, bool enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_en_vi_dbg_log = enable;
}
#endif


static void ath_get_noise_detection_param(ath_dev_t dev, int cmd,int *val)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
#define IEEE80211_ENABLE_NOISE_DETECTION 1
#define IEEE80211_NOISE_THRESHOLD 2
#define IEEE80211_GET_COUNTER_VALUE 3
    switch(cmd)
    {
        case IEEE80211_ENABLE_NOISE_DETECTION:
          *val = sc->sc_enable_noise_detection;
          break;
        case IEEE80211_NOISE_THRESHOLD:
          *val =(int) sc->sc_noise_floor_th;
          break;
        case IEEE80211_GET_COUNTER_VALUE:
          if( sc->sc_noise_floor_total_iter) {
              *val =  (sc->sc_noise_floor_report_iter *100)/sc->sc_noise_floor_total_iter;
          } else
              *val = 0;
          break;
        default:
              printk("UNKNOWN param %s %d \n",__func__,__LINE__);
          break;
    }
#undef IEEE80211_ENABLE_NOISE_DETECTION
#undef IEEE80211_NOISE_THRESHOLD
    return;
}

static void ath_set_noise_detection_param(ath_dev_t dev, int cmd,int val)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t flush_stats = 0;
#define IEEE80211_ENABLE_NOISE_DETECTION 1
#define IEEE80211_NOISE_THRESHOLD 2
    switch(cmd)
    {
        case IEEE80211_ENABLE_NOISE_DETECTION:
            sc->sc_enable_noise_detection = val;
            if(val) {
                /* Disabling DCS as soon as we enable noise detection algo */
                sc->sc_dcs_enabled &= ~ATH_CAP_DCS_CWIM;
                flush_stats = 1;
            }
          break;
        case IEEE80211_NOISE_THRESHOLD:
          if(val) {
              sc->sc_noise_floor_th = val;
              flush_stats = 1;
          }
          break;

        default:
              printk("UNKNOWN param %s %d \n",__func__,__LINE__);
          break;
    }
    if(flush_stats) {
        sc->sc_noise_floor_report_iter = 0;
        sc->sc_noise_floor_total_iter = 0;
    }
#undef IEEE80211_ENABLE_NOISE_DETECTION
#undef IEEE80211_NOISE_THRESHOLD
    return;
}
/* This function returns the next valid sequence number for a given
 * node/tid combination. If tx addba is not established, it returns 0.
 */
static u_int16_t ath_get_tid_seqno(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    struct ath_atx_tid *tid = ATH_AN_2_TID(ATH_NODE(node), tidno);
    // Not holding locks since this is opportunistic
    u_int16_t seq = tid->seq_next;

    if (ath_aggr_query(tid)) { // If addba is completed or pending
        return seq;
    } else {
        return 0;
    }
}

/*
 * This will set the av_modify_bcn_rate to the user specified rate
 * which will be passed while setting the beacon rate during
 * beacon allocation.
 */
static bool
ath_modify_bcn_rate(ath_dev_t dev, int if_id, int new_rate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return false;
    }

    avp->av_modify_bcn_rate = new_rate;
    return true;
}

/*
 * Callback table for Atheros ARXXXX MACs
 */
static const struct ath_ops ath_ar_ops = {
    ath_get_device_info,        /* get_device_info */
    ath_get_caps,               /* have_capability */
    ath_get_ciphers,            /* has_cipher */
    ath_open,                   /* open */
    ath_suspend,                /* stop */
    ath_vap_attach,             /* add_interface */
    ath_vap_detach,             /* remove_interface */
    ath_vap_config,             /* config_interface */
    ath_vap_down,               /* down */
    ath_vap_listen,             /* listen */
    ath_vap_join,               /* join */
    ath_vap_up,                 /* up */
    ath_vap_dfs_wait,        /* dfs_wait */
    ath_vap_stopping,           /* stopping */
    ath_node_attach,            /* alloc_node */
    ath_node_detach,            /* free_node */
    ath_node_cleanup,           /* cleanup_node */
    ath_node_update_pwrsave,    /* update_node_pwrsave */
    ath_newassoc,               /* new_assoc */
    ath_enable_intr,            /* enable_interrupt */
    ath_disable_intr,           /* disable_interrupt */
    ath_intr,                   /* isr */
    ath_mesg_intr,              /* msisr */
    ath_handle_intr,            /* handle_intr */
    ath_reset_start,            /* reset_start */
    ath_reset,                  /* reset */
    ath_reset_end,              /* reset_end */
    ath_switch_opmode,          /* switch_opmode */
    ath_set_channel,            /* set_channel */
    ath_get_channel,            /* get_channel */
    ath_scan_start,             /* scan_start */
    ath_scan_end,               /* scan_end */
    ath_scan_enable_txq,        /* scan_enable_txq */
    ath_enter_led_scan_state,   /* led_scan_start */
    ath_leave_led_scan_state,   /* led_scan_end */
    ath_notify_force_ppm,       /* force_ppm_notify */
    ath_beacon_sync,            /* sync_beacon */
    ath_update_beacon_info,     /* update_beacon_info */
    ath_modify_bcn_rate,        /* modify_bcn_rate */
    ath_update_txpow_mgmt,      /* txpow_mgmt */
    ath_update_txpow_ctrl,      /* txpow_ctrl */
#if ATH_SUPPORT_WIFIPOS
    ath_update_loc_ctl_reg,     /* update_loc_ctl_reg */
    ath_read_loc_timer_reg,     /* read_loc_timer_reg */
    ath_get_eeprom_chain_mask,  /* get_eeprom_chain_mask */
#endif
    ath_led_state,              /* ledctl_state */
    ath_radio_info,             /* radio_info */
#ifdef ATH_USB
    ath_heartbeat_callback,     /* heartbeat_callback */
#endif
    ath_tx_init,                /* tx_init */
    ath_tx_cleanup,             /* tx_cleanup */
    ath_tx_get_qnum,            /* tx_get_qnum */
    ath_txq_update,             /* txq_update */
    ath_tx_start,               /* tx */
    ath_tx_tasklet,             /* tx_proc */
    ath_tx_flush,               /* tx_flush */
    ath_txq_depth,              /* txq_depth */
    ath_txq_aggr_depth,         /* txq_aggr_depth */
#ifdef ATH_TX_BUF_FLOW_CNTL
    ath_txq_set_minfree,        /* txq_set_minfree */
    ath_txq_num_buf_used,
#endif
#if ATH_TX_DROP_POLICY
    ath_tx_drop_policy,         /* tx_drop_policy */
#endif
    ath_rx_init,      /* rx_init */
    ath_rx_cleanup,             /* rx_cleanup */
    ath_rx_tasklet,             /* rx_proc */
    ath_rx_requeue,             /* rx_requeue */
    ath_rx_input,               /* rx_proc_frame */
    ath_aggr_check,             /* check_aggr */
    ath_set_ampduparams,        /* set_ampdu_params */
    ath_set_weptkip_rxdelim,    /* set_weptkip_rxdelim */
    ath_addba_requestsetup,     /* addba_request_setup */
    ath_addba_responsesetup,    /* addba_response_setup */
    ath_addba_requestprocess,   /* addba_request_process */
    ath_addba_responseprocess,  /* addba_response_process */
    ath_addba_clear,            /* addba_clear */
    ath_addba_cancel_timers,    /* addba_cancel_timers */
    ath_delba_process,          /* delba_process */
    ath_addba_status,           /* addba_status */
    ath_aggr_teardown,          /* aggr_teardown */
    ath_set_addbaresponse,      /* set_addbaresponse */
    ath_clear_addbaresponsestatus, /* clear_addbaresponsestatus */
    ath_updateslot,             /* set_slottime */
    ath_set_protmode,           /* set_protmode */
    ath_set_txpowlimit,         /* set_txpowlimit */
    ath_enable_tpc,             /* enable_tpc */
    ath_get_mac_address,        /* get_macaddr */
    ath_get_hw_mac_address,     /* get_hw_macaddr */
    ath_set_mac_address,        /* set_macaddr */
    ath_set_mcastlist,          /* set_mcastlist */
    ath_set_rxfilter,           /* set_rxfilter */
    ath_get_rxfilter,           /* get_rxfilter */
    ath_opmode_init,            /* mc_upload */
    ath_key_alloc_2pair,        /* key_alloc_2pair */
    ath_key_alloc_pair,         /* key_alloc_pair */
    ath_key_alloc_single,       /* key_alloc_single */
    ath_key_reset,              /* key_delete */
    ath_keyset,                 /* key_set */
    ath_keycache_size,          /* keycache_size */
    ath_get_sw_phystate,        /* get_sw_phystate */
    ath_get_hw_phystate,        /* get_hw_phystate */
    ath_set_sw_phystate,        /* set_sw_phystate */
    ath_radio_enable,           /* radio_enable */
    ath_radio_disable,          /* radio_disable */
    ath_suspend_led_control,    /* led_suspend */
    ath_set_ps_awake,           /* awake */
    ath_set_ps_netsleep,        /* netsleep */
    ath_set_ps_fullsleep,       /* fullsleep */
    ath_get_ps_state,           /* getpwrsavestate */
    ath_set_ps_state,           /* setpwrsavestate */
    ath_set_country,            /* set_country */
    ath_get_currentCountry,     /* get_current_country */
    ath_set_regdomain,          /* set_regdomain */
    ath_get_regdomain,          /* get_regdomain */
    ath_get_dfsdomain,          /* get_dfsdomain */
    ath_set_quiet,              /* set_quiet */
    ath_find_countrycode,       /* find_countrycode */
    ath_get_ctl_by_country,     /* get_ctl_by_country */
    ath_set_tx_antenna_select,  /* set_antenna_select */
    ath_get_current_tx_antenna, /* get_current_tx_antenna */
    ath_get_default_antenna,    /* get_default_antenna */
    ath_notify_device_removal,  /* notify_device_removal */
    ath_detect_card_present,    /* detect_card_present */
    ath_mhz2ieee,               /* mhz2ieee */
    ath_get_phy_stats,          /* get_phy_stats */
    ath_get_phy_stats_allmodes, /* get_phy_stats_allmodes */
    ath_get_ath_stats,          /* get_ath_stats */
    ath_get_11n_stats,          /* get_11n_stats */
    ath_get_dfs_stats,          /* get_dfs_stats */
    ath_clear_stats,            /* clear_stats */
    ath_set_macmode,            /* set_macmode */
    ath_set_extprotspacing,     /* set_extprotspacing */
    ath_get_extbusyper,         /* get_extbusyper */
    ath_get_chbusyper,          /* get_chbusyper */
#if ATH_SUPPORT_DFS
    ath_dfs_get_caps,           /* ath_dfs_get_caps */
    ath_detach_dfs,             /* ath_detach_dfs */
    ath_enable_dfs,             /* ath_enable_dfs */
    ath_disable_dfs,            /* ath_disable_dfs */
    ath_get_mib_cycle_counts_pct, /* ath_get_mib_cycle_counts_pct */
    ath_get_radar_thresholds,     /* ath_get_radar_thresholds */
#endif /* ATH_SUPPORT_DFS */
    ath_get_min_and_max_powers,         /* get_min_and_max_powers */
    ath_get_modeSelect,                 /* get_modeSelect */
    ath_get_chip_mode,                  /* get_chip_mode */
    ath_get_freq_range,                 /* get_freq_range */
    ath_fill_hal_chans_from_regdb,      /* fill_hal_chans_from_regdb */
#if ATH_WOW
    ath_get_wow_support,                 /* ath_get_wow_support */
    ath_set_wow_enable,                  /* ath_set_wow_enable */
    ath_wow_wakeup,                      /* ath_wow_wakeup */
    ath_set_wow_events,                  /* ath_set_wow_events */
    ath_get_wow_events,                  /* ath_get_wow_events */
    ath_wow_add_wakeup_pattern,          /* ath_wow_add_wakeup_pattern */
    ath_wow_remove_wakeup_pattern,       /* ath_wow_remove_wakeup_pattern */
    ath_wow_get_wakeup_pattern,          /* ath_wow_get_wakeup_pattern */
    ath_get_wow_wakeup_reason,           /* ath_get_wow_wakeup_reason */
    ath_wow_matchpattern_exact,          /* ath_wow_matchpattern_exact */
    ath_wow_set_duration,                /* ath_wow_set_duration */
    ath_set_wow_timeout,                 /* ath_set_wow_timeout */
    ath_get_wow_timeout,                 /* ath_get_wow_timeout */
#if ATH_WOW_OFFLOAD
    ath_set_wow_enabled_events,             /* ath_set_wow_enabled_events */
    ath_get_wow_enabled_events,             /* ath_get_wow_enabled_events */
    ath_get_wowoffload_support,             /* ath_get_wowoffload_support */
    ath_get_wowoffload_gtk_support,         /* ath_get_wowoffload_gtk_support */
    ath_get_wowoffload_arp_support,         /* ath_get_wowoffload_arp_support */
    ath_get_wowoffload_max_arp_offloads,    /* ath_get_wowoffload_max_arp_offloads */
    ath_get_wowoffload_ns_support,          /* ath_get_wowoffload_ns_support */
    ath_get_wowoffload_max_ns_offloads,     /* ath_get_wowoffload_max_arp_offloads */
    ath_get_wowoffload_4way_hs_support,     /* ath_get_wowoffload_4way_hs_support */
    ath_get_wowoffload_acer_magic_support,  /* ath_get_wowoffload_acer_magic_support */
    ath_get_wowoffload_acer_swka_support,   /* ath_get_wowoffload_acer_swka_support */
    ath_wowoffload_acer_magic_enable,       /* ath_wowoffload_acer_magic_enable */
    ath_wowoffload_set_rekey_suppl_info, /* ath_wowoffload_set_rekey_suppl_info */
    ath_wowoffload_remove_offload_info,  /* ath_wowoffload_remove_offload_info */
    ath_wowoffload_set_rekey_misc_info,  /* ath_wowoffload_set_rekey_misc_info */
    ath_wowoffload_get_rekey_info,       /* ath_wowoffload_get_rekey_info */
    ath_wowoffload_update_txseqnum,      /* ath_wowoffload_update_txseqnum */
    ath_acer_keepalive,         /* ath_acer_keepalive */
    ath_acer_keepalive_query,   /* ath_acer_keepalive_query */
    ath_acer_wakeup_match,      /* ath_acer_wakeup_match */
    ath_acer_wakeup_disable,    /* ath_acer_wakeup_disable */
    ath_acer_wakeup_query,      /* ath_acer_wakeup_query */
    ath_wowoffload_set_arp_info,/* ath_wowoffload_set_arp_info */
    ath_wowoffload_set_ns_info, /* ath_wowoffload_set_ns_info */
#endif /* ATH_WOW_OFFLOAD */
#endif

    ath_get_config,             /* ath_get_config_param */
    ath_set_config,             /* ath_set_config_param */
    ath_get_noisefloor,         /* get_noisefloor */
    ath_get_chainnoisefloor,    /* get_chainnoisefloor */
    ath_get_rx_nf_offset,	    /* get_rx_nf_offset */
    ath_get_rx_signal_dbm,     /* get_rx_signal_dbm */
#if ATH_SUPPORT_VOW_DCS
    ath_disable_dcsim,    /* disable_dcsim */
    ath_enable_dcsim,     /* enable_dcsim */
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    ath_get_min_agc_gain,       /* get_min_agc_gain */
#endif
    ath_update_sm_pwrsave,      /* update_sm_pwrsave */
    ath_node_gettxrate,         /* node_gettxrate */
    ath_node_set_fixedrate,     /* node_setfixedrate */
    ath_node_getmaxphyrate,     /* node_getmaxphyrate */
    athop_rate_newassoc,        /* ath_rate_newassoc */
    athop_rate_newstate,        /* ath_rate_newstate */
#ifdef DBG
    ath_register_read,          /* Read register value  */
    ath_register_write,         /* Write register value */
#endif
    ath_setTxPwrAdjust,         /* ath_set_txPwrAdjust */
    ath_setTxPwrLimit,          /* ath_set_txPwrLimit */
    ath_get_common_power,       /* get_common_power */
    ath_gettsf32,               /* ath_get_tsf32 */
    ath_gettsf64,               /* ath_get_tsf64 */
#if ATH_SUPPORT_WIFIPOS
    ath_gettsftstamp,           /* ath_get_tsftstamp */
#endif
    ath_setrxfilter,            /* ath_set_rxfilter */
    ath_set_sel_evm,            /* ath_set_sel_evm */
    ath_get_mib_cyclecounts,    /* ath_get_mibCycleCounts */
    ath_update_mib_macstats,    /* ath_update_mibMacstats */
    ath_get_mib_macstats,       /* ath_get_mibMacstats */
    ath_clear_mib_counters,     /* ath_clear_mibCounters */
#ifdef ATH_CCX
    ath_rcRateValueToPer,       /* rcRateValueToPer */
    ath_getserialnumber,        /* ath_get_sernum */
    ath_getchandata,            /* ath_get_chandata */
    ath_getcurrssi,             /* ath_get_curRSSI */
#endif
#if ATH_GEN_RANDOMNESS
    ath_get_rssi_chain0,        /* ath_get_rssi_chain0 */
#endif
    ath_get_amsdusupported,     /* get_amsdusupported */

#ifdef ATH_SWRETRY
    ath_set_swretrystate,       /* set_swretrystate */
    ath_handlepspoll,           /* ath_handle_pspoll */
    ath_exist_pendingfrm_tidq,  /* get_exist_pendingfrm_tidq */
    ath_reset_paused_tid,       /* reset_paused_tid */
#endif
#ifdef ATH_SUPPORT_UAPSD
    ath_process_uapsd_trigger,  /* process_uapsd_trigger */
    ath_tx_uapsd_depth,         /* uapsd_depth */
#endif
#ifndef REMOVE_PKT_LOG
    ath_start_pktlog,           /* pktlog_start */
    ath_read_pktlog_hdr,        /* pktlog_read_hdr */
    ath_read_pktlog_buf,        /* pktlog_read_buf */
#endif
#if ATH_SUPPORT_IQUE
    ath_setacparams,            /* ath_set_acparams */
    ath_setrtparams,            /* ath_set_rtparams */
    ath_getiqueconfig,          /* ath_get_iqueconfig */
    ath_sethbrparams,            /* ath_set_hbrparams */
#endif
    ath_do_pwrworkaround,       /* do_pwr_workaround */
    ath_get_txqproperty,        /* get_txqproperty */
    ath_set_txqproperty,        /* set_txqproperty */
    ath_update_txqproperty,     /* update_txqproperty */
    ath_get_hwrevs,                /* get_hwrevs */
    ath_rc_rate_maprix,         /* rc_rate_maprix */
    ath_rc_rate_set_mcast_rate, /* rc_rate_set_mcast_rate */
    ath_setdefantenna,          /* set_defaultantenna */
    ath_set_diversity,          /* set_diversity */
    ath_set_rx_chainmask,       /* set_rx_chainmask */
    ath_set_tx_chainmask,       /* set_tx_chainmask */
    ath_set_rx_chainmasklegacy, /* set_rx_chainmasklegacy */
    ath_set_tx_chainmasklegacy, /* set_tx_chainmasklegacy */
    ath_set_tx_chainmaskopt,    /* set_tx_chainmaskopt */
    ath_get_maxtxpower,         /* get_maxtxpower */
    ath_read_from_eeprom,       /* ath_eeprom_read */
    ath_pkt_log_text,           /* log_text_fmt */
    ath_fixed_log_text,         /* log_text */
#ifdef AR_DEBUG
    ath_tx_node_queue_stats,     /* node_queue_stats */
#endif
    ath_tx_node_queue_depth,    /* node_queue_depth */
    ath_rate_newstate_set,      /* rate_newstate_set*/
    ath_rate_findrc,            /* rate_findrc*/
    ath_printreg,
    ath_getmfpsupport,          /* ath_get_mfpsupport */
    ath_setrxtimeout,           /* ath_set_rxtimeout */




#ifdef ATH_USB
    ath_usb_suspend,
    ath_usb_rx_cleanup,
#endif
#if ATH_SUPPORT_WIRESHARK
    ath_fill_radiotap_hdr,      /* ath_fill_radiotap_hdr */
    ath_monitor_filter_phyerr,  /* ath_monitor_filter_phyerr */
#endif
#ifdef ATH_BT_COEX
    ath_bt_coex_ps_complete,    /* bt_coex_ps_complete */
    ath_bt_coex_get_info,       /* bt_coex_get_info */
    ath_bt_coex_event,          /* bt_coex_event */
    ath_btcoexinfo,             /* ath_btcoex_info*/
    ath_coexwlaninfo,           /* ath_coex_wlan_info*/
#endif
#if QCA_AIRTIME_FAIRNESS
    ath_atf_node_unblock,           /* ath_atf_node_unblock */
    ath_atf_set_clear,              /* ath_atf_set_clear */
    ath_atf_tokens_used,            /* ath_atf_tokens_used */
    ath_atf_node_resume,            /* ath_atf_node_resume */
    ath_atf_tokens_unassigned,      /* ath_atf_tokens_unassigned */
    ath_atf_capable_node,           /* ath_atf_capable_node */
    ath_atf_airtime_estimate,       /* ath_atf_airtime_estimate */
    ath_atf_debug_nodestate,        /* ath_atf_debug_nodestate */
#endif
    /* Noise floor function */
    ath_dev_noisefloor,
    ath_dev_get_chan_stats,

#if ATH_PCIE_ERROR_MONITOR
    ath_start_pcie_error_monitor,
    ath_read_pcie_error_monitor,
    ath_stop_pcie_error_monitor,
#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE
    ath_dev_start_spectral_scan,
    ath_dev_stop_spectral_scan,
    ath_dev_record_chan_info,
    ath_init_spectral_ops,
#endif
    ath_set_proxysta,           /* set_proxysta */

#ifdef ATH_GEN_TIMER
    ath_tsf_timer_alloc,        /* ath_tsf_timer_alloc */
    ath_tsf_timer_free,         /* ath_tsf_timer_free */
    ath_tsf_timer_start,        /* ath_tsf_timer_start */
    ath_tsf_timer_stop,         /* ath_tsf_timer_stop */
#endif  //ifdef ATH_GEN_TIMER

#ifdef ATH_RFKILL
    ath_enable_RFKillPolling,   /* enableRFKillPolling */
#endif  //ifdef ATH_RFKILL
#if ATH_SLOW_ANT_DIV
    ath_slow_ant_div_suspend,
    ath_slow_ant_div_resume,
#endif  //if ATH_SLOW_ANT_DIV

    ath_reg_notify_tx_bcn,      /* ath_reg_notify_tx_bcn */
    ath_dereg_notify_tx_bcn,    /* ath_dereg_notify_tx_bcn */
    ath_reg_vap_info_notify,    /* ath_reg_vap_info_notify */
    ath_vap_info_update_notify, /* ath_vap_info_update_notify */
    ath_dereg_vap_info_notify,  /* ath_dereg_vap_info_notify */
    ath_vap_info_get,           /* ath_vap_info_get */
    ath_vap_pause_control,      /* ath_vap_pause_control */
    ath_node_pause_control,     /* ath_node_pause_control */
    ath_get_goodput,
#ifdef ATH_SUPPORT_TxBF
    ath_get_txbfcaps,           /* get_txbf_caps */
    ath_txbf_alloc_key,         /* txbf_alloc_key */
    ath_txbf_set_key,           /* txbf_set_key */
    ath_txbf_del_key,           /* txbf_del_key */
    ath_txbf_set_hw_cvtimeout,
    ath_txbf_print_cv_cache,
    ath_txbf_get_cvcache_nr,
    ath_txbf_set_rpt_received,
#endif
    ath_set_rifs,
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    ath_ald_collect_data,       /* ald_collect_data */
    ath_ald_collect_ni_data,    /* ald_collect_ni_data */
#endif
#if UMAC_SUPPORT_VI_DBG
    ath_set_vi_dbg_restart,     /* ath_set_vi_dbg_restart */
    ath_set_vi_dbg_log,         /* ath_set_vi_dbg_log */
#endif
    ath_cdd_control,            /* ath_cdd_control */
    ath_get_tid_seqno,          /* ath_get_tid_seqno */
    ath_get_noderssi,           /* get_noderssi */
    ath_get_noderate,           /* get_noderate */
    ath_setmfpQos,              /* ath_set_hw_mfp_qos */
#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_netif_stop_queue,
    ath_netif_wake_queue,
    ath_netif_set_wdtimeout,
    ath_netif_flowmac_pause,
#endif
#if ATH_SUPPORT_IBSS_DFS
    ath_ibss_beacon_update_start,         /* ath_ibss_beacon_update_start */
    ath_ibss_beacon_update_stop,          /* ath_ibss_beacon_update_stop */
#endif
#if ATH_ANT_DIV_COMB
    ath_smartant_query,                 /* smartAnt_query */
    ath_smartant_config,                /* smartAnt_config */
    ath_smartant_normal_scan_handle,    /* ath_sa_normal_scan_handle */
    ath_set_lna_div_use_bt_ant,
#endif
    ath_set_bssid_mask,         /* ath_set_bssid_mask */
    ath_get_hwbeaconproc_active,          /* get_hwbeaconproc_active */
    ath_hw_beacon_rssi_threshold_enable,  /* hw_beacon_rssi_threshold_enable */
    ath_hw_beacon_rssi_threshold_disable, /* hw_beacon_rssi_threshold_disable */
    ath_conf_rx_intr_mit,                 /* conf_rx_intr_mit */
    ath_get_txbuf_free,                 /* get_txbuf_free */

#if UNIFIED_SMARTANTENNA
    ath_smart_ant_enable,              /* smart_ant_enable */
    ath_smart_ant_set_tx_antenna,      /* smart_ant_set_tx_antenna */
    ath_smart_ant_set_tx_defaultantenna, /* smart_ant_set_tx_defaultantenna */
    ath_smart_ant_set_training_info,   /* smart_ant_set_training_info */
    ath_smart_ant_prepare_rateset,     /* smart_ant_prepare_rateset */
#endif
#if ATH_TX_DUTY_CYCLE
    ath_set_tx_duty_cycle,              /* tx_duty_cycle */
#endif
#if ATH_SUPPORT_WIFIPOS
    ath_lean_set_channel,               /* ath_lean_set_channel */
    ath_resched_txq,                    /* ath_resched_txq */
    ath_disable_hwq,                   /* ath_disable_hwq */
    ath_vap_reap_txqs,                  /* ath_vap_reap_txqs */
    ath_get_channel_busy_info,          /* ath_get_channel_busy_info */
#endif
    ath_get_tx_hw_retries,              /* get_tx_hw_retries */
    ath_get_tx_hw_success,              /* get_tx_hw_success */
#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_get_flowmac_enabled_state,      /* get_flowmac_enabled_state */
#endif
#if UMAC_SUPPORT_WNM
    ath_wnm_beacon_config,              /* set_beacon_config */
#endif
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    ath_get_pwrsaveq_len,               /* get_pwrsaveq_len, get LMAC node power save queue len */
    ath_node_pwrsaveq_send,             /* node_pwrsaveq_send */
    ath_node_pwrsaveq_flush,            /* node_pwrsaveq_flush */
    ath_node_pwrsaveq_drain,            /* node_pwrsaveq_drain */
    ath_node_pwrsaveq_age,              /* node_pwrsaveq_age */
    ath_node_pwrsaveq_get_info,         /* node_pwrsaveq_get_info */
    ath_node_pwrsaveq_set_param,        /* node_pwrsaveq_set_param */
#endif
    ath_get_tpc_tables,                 /* get_tpc_tables */
    ath_get_tx_chainmask,             /* get_tx_chainmask */
#if ATH_SUPPORT_TIDSTUCK_WAR
    ath_clear_rxtid,                   /* clear_rxtid */
#endif
#if ATH_SUPPORT_KEYPLUMB_WAR
    ath_key_save_halkey,                /* save_halkey */
    ath_checkandplumb_key,              /* checkandplumb_key */
#endif
    ath_set_noise_detection_param,    /* set_noise_detection_param */
    ath_get_noise_detection_param,    /* get_noise_detection_param */
#ifdef ATH_TX99_DIAG
    ath_tx99_ops,                       /* tx99_ops */
#endif
    ath_set_ctl_pwr,                    /* set_ctl_pwr */
    ath_ant_swcom_sel,                  /* ant_switch_sel */
    ath_set_node_tx_chainmask,          /* set_node_tx_chainmask */
    ath_set_beacon_interval,            /* set_beacon_interval */
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    ath_txbf_loforceon_update,           /* txbf_loforceon_update */
#endif
    ath_node_pspoll,                      /* node_pspoll*/
    ath_get_vap_stats,                 /* get_vap_stats */
    ath_get_last_txpower,                  /* get last data tx power*/
    ath_rate_check,                   /*direct_rate_check*/
    ath_set_use_cac_prssi,            /* set_use_cac_prssi */
    /* scan ops */
    ath_scan_sm_start,
    ath_scan_cancel,
    ath_scan_sta_power_events,
    ath_scan_resmgr_events,
    ath_scan_connection_lost,
    ath_scan_get_sc_isscan,
    ath_scan_set_channel_list,
    ath_scan_in_home_channel,
#ifdef WLAN_SUPPORT_GREEN_AP
    ath_green_ap_ps_on_off,
    ath_green_ap_reset_dev,
    ath_green_ap_get_current_channel,
    ath_green_ap_get_current_channel_flags,
    ath_green_ap_get_capab,
#endif
};

int init_sc_params(struct ath_softc *sc,struct ath_hal *ah)
{
    HAL_STATUS status;
    int error = 0;
    u_int32_t rd;

#if ATH_TX_DUTY_CYCLE
    sc->sc_tx_dc_enable = 0;
    sc->sc_tx_dc_active_pct = 0;
    sc->sc_tx_dc_period = 25; /* 25ms period */
#endif
    sc->sc_limit_legacy_frames = 25;

    /*
     * Collect the channel list using the default country
     * code and including outdoor channels.  The 802.11 layer
     * is resposible for filtering this list based on settings
     * like the phy mode.
     */
    ath_hal_getregdomain(ah, &rd);

    if (sc->sc_reg_parm.regdmn) {
        ath_hal_setregdomain(ah, sc->sc_reg_parm.regdmn, &status);
    }

    sc->sc_config.ath_countrycode = CTRY_DEFAULT;
    if (countrycode != -1) {
        sc->sc_config.ath_countrycode = countrycode;
    }

    /*
     * Only allow set country code for debug domain.
     */
    if ((rd == 0) && sc->sc_reg_parm.countryName[0]) {
        sc->sc_config.ath_countrycode = findCountryCode((u_int8_t *)sc->sc_reg_parm.countryName);
    }

    sc->sc_config.ath_xchanmode = AH_TRUE;
    if (xchanmode != -1) {
        sc->sc_config.ath_xchanmode = xchanmode;
    } else {
        sc->sc_config.ath_xchanmode = sc->sc_reg_parm.extendedChanMode;
    }
    error = ath_getchannels(sc, sc->sc_config.ath_countrycode,
                            ath_outdoor, sc->sc_config.ath_xchanmode);
    if (error != 0) {
        printk( "%s[%d]: Failed to get channel information! error[%d]\n", __func__, __LINE__, error );
        return -1;
    }

    /* default to STA mode */
    sc->sc_opmode = HAL_M_STA;

    /* Set the dcs_CWIM on by default */
    sc->sc_dcs_enabled = ATH_CAP_DCS_CWIM;
#if ATH_SUPPORT_VOW_DCS
    OS_MEMZERO(&sc->sc_dcs_params, sizeof(struct ath_dcs_params));
    sc->sc_phyerr_penalty = PHYERR_PENALTY_TIME;
    sc->sc_coch_intr_thresh = COCH_INTR_THRESH;
    sc->sc_tx_err_thresh = TXERR_THRESH;
    sc->sc_dcs_enabled &= ~ATH_CAP_DCS_WLANIM;
#endif
    return 0;
}

void init_sc_params_complete(struct ath_softc *sc,struct ath_hal *ah)
{
    u_int32_t chainmask;
    u_int32_t stbcsupport;
    u_int32_t ldpcsupport;

    if(sc->sc_reg_parm.stbcEnable) {
        if (ath_hal_txstbcsupport(ah, &stbcsupport))
            sc->sc_txstbcsupport = stbcsupport;

        if (ath_hal_rxstbcsupport(ah, &stbcsupport))
            sc->sc_rxstbcsupport = stbcsupport;
    }

    if (sc->sc_reg_parm.ldpcEnable) {
        if (ath_hal_ldpcsupport(ah, &ldpcsupport)) {
            sc->sc_ldpcsupport = ldpcsupport;
        }
    }

    sc->sc_config.txpowlimit2G = ATH_TXPOWER_MAX_2G;
    sc->sc_config.txpowlimit5G = ATH_TXPOWER_MAX_5G;
    /* 11n Capabilities */
    if (sc->sc_hashtsupport) {
        sc->sc_config.ampdu_limit = sc->sc_reg_parm.aggrLimit;
        sc->sc_config.ampdu_subframes = sc->sc_reg_parm.aggrSubframes;

        sc->sc_txaggr = (sc->sc_reg_parm.txAggrEnable) ? 1 : 0;
        sc->sc_rxaggr = (sc->sc_reg_parm.rxAggrEnable) ? 1 : 0;

        sc->sc_txamsdu = (sc->sc_reg_parm.txAmsduEnable) ? 1 : 0;

        if (ath_hal_hasrifstx(ah)) {
            sc->sc_txrifs = (sc->sc_reg_parm.txRifsEnable) ? 1 : 0;
            sc->sc_config.rifs_ampdu_div =
                sc->sc_reg_parm.rifsAggrDiv;
        }
#ifdef ATH_RB
        if (!ath_hal_hasrifsrx(ah)) {
            sc->sc_rxrifs_timeout = sc->sc_reg_parm.rxRifsTimeout;
            sc->sc_rxrifs_skipthresh =
                                 sc->sc_reg_parm.rxRifsSkipThresh;
            sc->sc_rxrifs = sc->sc_reg_parm.rxRifsEnable;
            sc->sc_do_rb_war = 1;
            ath_rb_init(sc);
        }
#endif
    }
    sc->sc_cus_cts_set = 0;
    /*
     * Check for misc other capabilities.
     */
    sc->sc_hasbmask = ath_hal_hasbssidmask(ah);
    sc->sc_hastsfadd = ath_hal_hastsfadjust(ah);
    if (sc->sc_reg_parm.txChainMask) {
        chainmask = sc->sc_reg_parm.txChainMask;
    } else {
        if (!ath_hal_gettxchainmask(ah, &chainmask))
            chainmask = 0;
    }
#ifdef ATH_CHAINMASK_SELECT
    /*
     * If we cannot transmit on three chains, prevent chain mask
     * selection logic from switching between 2x3 and 3x3 chain
     * masks based on RSSI.
     * Even when we can transmit on three chains, our default
     * behavior is to use two transmit chains only.
     */
    if ((chainmask == ATH_CHAINMASK_SEL_3X3) &&
        (ath_hal_getcapability(ah, HAL_CAP_TS, 0, NULL) != HAL_OK))
    {
        sc->sc_no_tx_3_chains = AH_TRUE;
        chainmask = ATH_CHAINMASK_SEL_2X3;
    } else {
        sc->sc_no_tx_3_chains = AH_FALSE;
    }
    sc->sc_config.chainmask_sel = sc->sc_no_tx_3_chains;
#endif

    sc->sc_config.txchainmask = chainmask;
    if (sc->sc_reg_parm.rxChainMask) {
        chainmask = sc->sc_reg_parm.rxChainMask;
    } else {
        if (!ath_hal_getrxchainmask(ah, &chainmask))
            chainmask = 0;
    }

    sc->sc_config.rxchainmask = chainmask;
    /* legacy chain mask */
    if (sc->sc_reg_parm.txChainMaskLegacy) {
        chainmask = sc->sc_reg_parm.txChainMaskLegacy;
    } else {
        if (!ath_hal_gettxchainmask(ah, &chainmask))
            chainmask = 0;
    }
    sc->sc_config.txchainmasklegacy = chainmask;
    if (sc->sc_reg_parm.rxChainMaskLegacy) {
        chainmask = sc->sc_reg_parm.rxChainMaskLegacy;
    } else {
        if (!ath_hal_getrxchainmask(ah, &chainmask))
            chainmask = 0;
    }
    sc->sc_config.rxchainmasklegacy = chainmask;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    /* turn offDTCS by default */
    sc->sc_dyn_txchainmask = 0;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */

    /*
     * Configuration for rx chain detection
     */
    sc->sc_rxchaindetect_ref = sc->sc_reg_parm.rxChainDetectRef;
    sc->sc_rxchaindetect_thresh5GHz = sc->sc_reg_parm.rxChainDetectThreshA;
    sc->sc_rxchaindetect_thresh2GHz = sc->sc_reg_parm.rxChainDetectThreshG;
    sc->sc_rxchaindetect_delta5GHz = sc->sc_reg_parm.rxChainDetectDeltaA;
    sc->sc_rxchaindetect_delta2GHz = sc->sc_reg_parm.rxChainDetectDeltaG;

    sc->sc_config.pcieDisableAspmOnRfWake = sc->sc_reg_parm.pcieDisableAspmOnRfWake;
    sc->sc_config.pcieAspm = sc->sc_reg_parm.pcieAspm;
#if ATH_WOW
    sc->sc_wowenable = sc->sc_reg_parm.wowEnable;
#endif
    sc->sc_toggle_immunity = 0;

#if ATH_SUPPORT_IQUE
    /* Initialize Rate control and AC parameters with default values */
    sc->sc_rc_params[0].per_threshold = 55;
    sc->sc_rc_params[1].per_threshold = 35;
    sc->sc_rc_params[0].probe_interval = 50;
    sc->sc_rc_params[1].probe_interval = 50;
    sc->sc_hbr_params[WME_AC_VI].hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_params[WME_AC_VO].hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_per_high = HBR_TRIGGER_PER_HIGH;
    sc->sc_retry_duration = ATH_RC_DURATION_WITH_RETRIES;
    sc->total_delay_timeout = ATH_RC_TOTAL_DELAY_TIMEOUT;
#endif
#if ATH_SUPPORT_VOWEXT
    /* Enable video features by default */
    sc->sc_vowext_flags = ATH_CAP_VOWEXT_FAIR_QUEUING |
                          ATH_CAP_VOWEXT_AGGR_SIZING  |
                          ATH_CAP_VOWEXT_BUFF_REORDER |
                          ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR;
    sc->agtb_tlim = ATH_11N_AGTB_TLIM;
    sc->agtb_blim = ATH_11N_AGTB_BLIM;
    sc->agthresh  = ATH_11N_AGTB_THRESH;
    sc->sc_phyrestart_disable = 0;
    /* RCA variables */
    sc->rca_aggr_uptime = ATH_11N_RCA_AGGR_UPTIME;
    sc->aggr_aging_factor = ATH_11N_AGGR_AGING_FACTOR;
    sc->per_aggr_thresh = ATH_11N_PER_AGGR_THRESH;
    sc->excretry_aggr_red_factor = ATH_11N_EXCRETRY_AGGR_RED_FACTOR;
    sc->badper_aggr_red_factor = ATH_11N_BADPER_AGGR_RED_FACTOR;
    sc->vsp_enable       = VSP_ENABLE_DEFAULT;
    sc->vsp_threshold    = VSP_THRESHOLD_DEFAULT;
    sc->vsp_evalinterval = VSP_EVALINTERVAL_DEFAULT;
    sc->vsp_prevevaltime = 0;
    sc->vsp_bependrop    = 0;
    sc->vsp_bkpendrop    = 0;
    sc->vsp_vipendrop    = 0;
    sc->vsp_vicontention = 0;
#if ATH_SUPPORT_FLOWMAC_MODULE

    /* When there are no transmit buffers, if there is way to inform kernel
     * not to send any more frames to Ath layer, inform the kernel's network
     * stack not to forward any more frames.
     * Currently ath_reg_parm.osnetif_flowcntrl is enabled for only Linux
     */
    sc->sc_osnetif_flowcntrl = sc->sc_reg_parm.osnetif_flowcntrl;
    DPRINTF(sc, ATH_DEBUG_XMIT,
            "%s sc->sc_osnetif_flowcntrl %d \n",
            __func__, sc->sc_osnetif_flowcntrl);
#endif
#endif

#ifdef VOW_TIDSCHED
    TAILQ_INIT(&sc->tid_q);
    sc->tidsched = ATH_TIDSCHED;
    sc->acqw[3] = ATH_TIDSCHED_VOQW;
    sc->acqw[2] = ATH_TIDSCHED_VIQW;
    sc->acqw[1] = ATH_TIDSCHED_BKQW;
    sc->acqw[0] = ATH_TIDSCHED_BEQW;
    sc->acto[3] = ATH_TIDSCHED_VOTO;
    sc->acto[2] = ATH_TIDSCHED_VITO;
    sc->acto[1] = ATH_TIDSCHED_BKTO;
    sc->acto[0] = ATH_TIDSCHED_BETO;
#endif

    sc->sc_reset_type = ATH_RESET_DEFAULT;
	/*Enable PERFORM_KEY_SEARCH_ALWAYS by default, this can be disabled through ioctl*/
    sc->sc_keysearch_always_enable = 1;
    ath_hal_keysearch_always_war_enable(sc->sc_ah, sc->sc_keysearch_always_enable );

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    sc->sc_ald.sc_ald_free_buf_lvl = ATH_TXBUF -  ((ATH_TXBUF * DEFAULT_BUFF_LEVEL_IN_PERCENT) / 100);
#endif
#if ATH_ANI_NOISE_SPUR_OPT
    /* disabled by default */
    sc->sc_config.ath_noise_spur_opt = 0;
#endif

#if ATH_SUPPORT_RX_PROC_QUOTA
    sc->sc_rx_work_hp = 0;
    sc->sc_rx_work_lp = 0;
    sc->sc_process_rx_num = RX_PKTS_PROC_PER_RUN;
#endif

#if ATH_RX_LOOPLIMIT_TIMER
    sc->sc_rx_looplimit = 0;
    sc->sc_rx_looplimit_timeout = 1000;
#endif

    /* optional txchainmask not used by default */
    sc->sc_tx_chainmaskopt = 0;

    /* Set the dcs on by default */
    sc->sc_dcs_enabled = 1;
    sc->sc_allow_promisc = 0;
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    sc->sc_loforce_enable = 0;
#endif


    /*Enable rts cts if rate is less then 52000 during Field trials
      we found that generally rate drop to 53Mbps at mid range and
      retries start so to to prevent data loss we should send rts and wait
      for cts before sending data */
    sc->sc_limitrate = 52000;
    /* Enable RTS/CTS by defualt if rate is less then 52000 */
    sc->sc_user_en_rts_cts = 1;

 /* Fixed for 11n chips, 4 hardware rate series */
#if UNIFIED_SMARTANTENNA
    sc->max_fallback_rates = 3;
#endif

#if QCA_AIRTIME_FAIRNESS
    sc->sc_adjust_possible_tput = 0;
    sc->sc_use_aggr_for_possible_tput = 0;
    sc->sc_adjust_tput = ATF_ADJUST_TPUT_DFL;
    sc->sc_adjust_tput_more = ATF_ADJUST_TPUT_MORE_DFL;
    sc->sc_adjust_tput_more_thrs = ATF_ADJUST_TPUT_MORE_THRS_DFL;
#endif

}

#if NO_HAL
int ath_dev_attach(u_int16_t            devid,
	void                 *bus_context,
	ieee80211_handle_t   ieee,
	struct ieee80211_ops *ieee_ops,
    struct wlan_objmgr_pdev *sc_pdev,
    struct wlan_lmac_if_rx_ops *rx_ops,
	osdev_t              osdev,
	ath_dev_t            *dev,
	struct ath_ops       **ops,
	struct ath_reg_parm  *ath_conf_parm,
	struct hal_reg_parm  *hal_conf_parm,
	struct ath_hal *ah)
{
	 return 0;
}
#else
int ath_dev_attach(u_int16_t            devid,
                   void                 *bus_context,
                   ieee80211_handle_t   ieee,
                   struct ieee80211_ops *ieee_ops,
                   struct wlan_objmgr_pdev *sc_pdev,
                   struct wlan_lmac_if_rx_ops *rx_ops,
                   osdev_t              osdev,
                   ath_dev_t            *dev,
                   struct ath_ops       **ops,
                   struct ath_reg_parm  *ath_conf_parm,
                   struct hal_reg_parm  *hal_conf_parm,
				   struct ath_hal *ah)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
#ifndef REMOVE_PKT_LOG
    ath_generic_softc_handle scn;
#endif
    struct ath_softc *sc = NULL;
    int error = 0, i;
    u_int32_t pcieLcrOffset = 0;
    u_int32_t enableapm;
    u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
    u_int32_t cal_interval, regval = 0;
    u_int8_t splitmic_required = 0;
    u_int32_t enable_msi = 0;
#ifdef HOST_OFFLOAD
    int lower_rx_mitigation = 1;
#endif
    sc = (struct ath_softc *)OS_MALLOC(osdev, sizeof(struct ath_softc), GFP_KERNEL);
    if (sc == NULL) {
        printk("%s: Unable to allocate device object.\n", __func__);
        error = -ENOMEM;
        goto bad;
    }
    OS_MEMZERO(sc, sizeof(struct ath_softc));

    *dev = (ath_dev_t)sc;
#if ATH_TX_COMPACT
    sc->sc_nodebug =1;
#else
    sc->sc_nodebug =0;
#endif

    /* Initialize DPRINTF control object */
    ath_dprintf_init(sc);

    /*
     * ath_ops is stored per device instance because EDMA devices
     * have EDMA specific functions.
     */
    sc->sc_ath_ops = ath_ar_ops;
    *ops = &sc->sc_ath_ops;

    ATH_HTC_SET_CALLBACK_TABLE(sc->sc_ath_ops);     /* Update HTC functions */

    sc->sc_osdev = osdev;
    sc->qdf_dev = OS_GET_ADFDEV(osdev);
    sc->sc_ieee = ieee;
    sc->sc_ieee_ops = ieee_ops;
    sc->sc_pdev = sc_pdev;
    sc->sc_rx_ops = rx_ops;
    sc->sc_reg_parm = *ath_conf_parm;
    sc->sc_mask2chains = ath_numchains;
    /* XXX: hardware will not be ready until ath_open() being called */
    sc->sc_invalid = 1;
    ATH_USB_SET_INVALID(sc, 1);

    /* debug not available */
    //sc->sc_debug = DBG_DEFAULT | ATH_DEBUG_UAPSD;

    ath_dprintf_set_default_mask(sc);
    if(ath_conf_parm->AthDebugLevel)
    {
        ath_dprintf_set_mask(sc,ath_conf_parm->AthDebugLevel);
    }

    DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

    ATH_HTC_SET_HANDLE(sc, osdev);
    ATH_HTC_SET_EPID(sc, osdev);

    /*
     * Cache line size is used to size and align various
     * structures used to communicate with the hardware.
     */
    ATH_GET_CACHELINE_SIZE(sc);

    //ATH_LOCK_INIT(sc);
    ATH_VAP_PAUSE_LOCK_INIT(sc);
    ATH_PS_LOCK_INIT(sc);
    ATH_PS_INIT_LOCK_INIT(sc);
    ATH_RESET_LOCK_INIT(sc);
    ATH_RESET_SERIALIZE_LOCK_INIT(sc);

    ATH_INTR_STATUS_LOCK_INIT(sc);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
    ATH_INTERNAL_RESET_LOCK_INIT(sc);
    qdf_print("%s: Init internal reset lock ",__func__);
#endif

#if ATH_RESET_SERIAL
    ATH_RESET_INIT_MUTEX(sc);
    ATH_RESET_HAL_INIT_MUTEX(sc);
     for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
         ATH_RESET_LOCK_Q_INIT(sc, i);
         TAILQ_INIT(&(sc->sc_queued_txbuf[i].txbuf_head));
     }
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK_INIT(sc);
    LIST_INIT(&sc->sc_nt);
#endif
	ath_hal_set_sc(ah, sc);
    sc->sc_ah = ah;
    /* save the mac address from EEPROM */
    ath_hal_getmac(ah, sc->sc_my_hwaddr);
    if (!ATH_ADDR_EQ(sc->sc_reg_parm.networkAddress, empty_macaddr) &&
        !IEEE80211_IS_MULTICAST(sc->sc_reg_parm.networkAddress) &&
        !IEEE80211_IS_BROADCAST(sc->sc_reg_parm.networkAddress) &&
        (sc->sc_reg_parm.networkAddress[0] & 0x02)) {
        /* change hal mac address */
        ath_hal_setmac(sc->sc_ah, sc->sc_reg_parm.networkAddress);
    }

#ifdef __linux__
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    if (macaddr_override) {
        u_int8_t addr[IEEE80211_ADDR_LEN];
        if (sscanf(macaddr_override, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &addr[0], &addr[1], &addr[2], &addr[3], &addr[4],
               &addr[5]) == 6)
            ath_hal_setmac(sc->sc_ah, addr);
    }
#endif
#endif /* __linux__ */

#ifdef HOST_OFFLOAD
    ath_hal_set_config_param(sc->sc_ah, HAL_CONFIG_LOWER_INT_MITIGATION_RX,
            &lower_rx_mitigation);
#endif
    /*
     * Attach the enhanced DMA module.
     */
    ath_edma_attach(sc, ops);

    /*
     * Query the HAL for tx  and rx descriptor lengths
     */
    ath_hal_gettxdesclen(sc->sc_ah, &sc->sc_txdesclen);
    ath_hal_gettxstatuslen(sc->sc_ah, &sc->sc_txstatuslen);
    ath_hal_getrxstatuslen(sc->sc_ah, &sc->sc_rxstatuslen);

    /*
     * Check TxBF support
     */
#ifdef ATH_SUPPORT_TxBF
     sc->sc_txbfsupport = ath_hal_hastxbf(ah);
#endif
    /*
     * Get the chipset-specific aggr limit.
     */
    ath_hal_getrtsaggrlimit(sc->sc_ah, &sc->sc_rtsaggrlimit);

    /*
     * Check if the device has hardware counters for PHY
     * errors.  If so we need to enable the MIB interrupt
     * so we can act on stat triggers.
     */
    if (ath_hal_hwphycounters(ah)) {
        sc->sc_needmib = 1;
    }

    /*
     * Get the hardware key cache size.
     */
    sc->sc_keymax = ath_hal_keycachesize(ah);
    if (sc->sc_keymax > ATH_KEYMAX) {
        printk("%s: Warning, using only %u entries in %u key cache\n",
               __func__, ATH_KEYMAX, sc->sc_keymax);
        sc->sc_keymax = ATH_KEYMAX;
    }

    /*
     * Get the max number of buffers that can be stored in a TxD.
     */
    ath_hal_getnumtxmaps(ah, &sc->sc_num_txmaps);

    // single key cache writes
    // Someday Soon we should probe the chipset id to see if it is
    // one of the broken ones, then turn on the singleWriteKC, which
    // should be just a hal property, IMNSHO.
    // ath_hal_set_singleWriteKC(ah, TRUE);

    /*
     * Reset the key cache since some parts do not
     * reset the contents on initial power up.
     */
    for (i = 0; i < sc->sc_keymax; i++) {
        ath_hal_keyreset(ah, (u_int16_t)i);
    }
    /*
     * Mark key cache slots associated with global keys
     * as in use.  If we knew TKIP was not to be used we
     * could leave the +32, +64, and +32+64 slots free.
     * XXX only for splitmic.
     */
    if(ath_hal_ciphersupported(ah,HAL_CIPHER_MIC)) {
            splitmic_required = ath_hal_tkipsplit(ah);
    }
    for (i = 0; i < IEEE80211_WEP_NKID; i++) {
        setbit(sc->sc_keymap, i);
        if(splitmic_required) {
            setbit(sc->sc_keymap, i+32);
            setbit(sc->sc_keymap, i+64);
            setbit(sc->sc_keymap, i+32+64);
        }
    }

    /*
     * Initialize timer module
     */
    ath_initialize_timer_module(osdev);

    if (init_sc_params(sc,sc->sc_ah)) {
        qdf_print("init sc params failed");
        error = -EIO;
        goto bad;
    }

    if (HAL_OK == ath_hal_getcapability(ah, HAL_CAP_ENTERPRISE_MODE,
                                        0, &regval))
    {
        sc->sc_ent_min_pkt_size_enable =
           (regval & AH_ENT_MIN_PKT_SIZE_DISABLE) ? 0 : 1;
        sc->sc_ent_spectral_mask =
           (regval & AH_ENT_SPECTRAL_PRECISION) >> AH_ENT_SPECTRAL_PRECISION_S;
        sc->sc_ent_rtscts_delim_war =
           (regval & AH_ENT_RTSCTS_DELIM_WAR) ? 1 : 0;
    }

    /*
     * Initialize rate tables.
     */
    ath_hal_get_max_weptkip_ht20tx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht20_tx_rateKbps);
    ath_hal_get_max_weptkip_ht40tx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht40_tx_rateKbps);
    ath_hal_get_max_weptkip_ht20rx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht20_rx_rateKbps);
    ath_hal_get_max_weptkip_ht40rx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht40_rx_rateKbps);
    ath_rate_table_init();

    /*
     * Setup rate tables for all potential media types.
     */
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11a);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11b);
    ath_rate_setup(sc, WIRELESS_MODE_11g);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_108a);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_108g);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT20);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT20);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT40PLUS);
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT40MINUS);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT40PLUS);
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT40MINUS);

    /* Setup for half/quarter rates */
    ath_setup_subrates(sc);

    /* NB: setup here so ath_rate_update is happy */
#ifndef ATH_NO_5G_SUPPORT
    ath_setcurmode(sc, WIRELESS_MODE_11a);
#else
    ath_setcurmode(sc, WIRELESS_MODE_11g);
#endif

    /* This initializes Target Hardware Q's; It is independent of Host endpoint initialization */
    ath_htc_initialize_target_q(sc);

    /* Initialize the tasket for rx */
    ATH_USB_TQUEUE_INIT(sc, ath_htc_tasklet);
    ATH_HTC_TXTQUEUE_INIT(sc, ath_htc_tx_tasklet);
    ATH_HTC_UAPSD_CREDITUPDATE_INIT(sc, ath_htc_uapsd_creditupdate_tasklet);
#ifdef ATH_USB
    /* Initialize thread */
    ath_usb_create_thread(sc);
#endif

    /*
     * Allocate hardware transmit queues: one queue for
     * beacon frames and one data queue for each QoS
     * priority.  Note that the hal handles reseting
     * these queues at the needed time.
     *
     * XXX PS-Poll
     */
    sc->sc_bhalq = ath_beaconq_setup(sc, ah);
    if (sc->sc_bhalq == (u_int) -1) {
        printk("%s: unable to setup a beacon xmit queue!\n", __func__);
        error = -EIO;
        goto bad2;
    }
    ATH_HTC_SET_BCNQNUM(sc, sc->sc_bhalq);

    sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
    if (sc->sc_cabq == NULL) {
        printk("%s: unable to setup CAB xmit queue!\n", __func__);
        error = -EIO;
        goto bad2;
    }

#ifdef ATH_BEACON_DEFERRED_PROC
        sc->sc_cabq->irq_shared = 0;
#else
        sc->sc_cabq->irq_shared = 1;
#endif
    sc->sc_config.cabqReadytime = ATH_CABQ_READY_TIME;

    ath_cabq_update(sc);

#ifdef ATH_SUPPORT_CFEND
    /* at this point of time, we do not know whether device works in AP mode
     * or in any other modes, queue configuration should not depend on mode
    * Also make sure that we enable this only for ar5416chips
     */
   error = ath_cfendq_config(sc);
   if (error) goto bad2;
#endif
    for (i = 0; i < N(sc->sc_haltype2q); i++)
        sc->sc_haltype2q[i] = -1;

    /* NB: insure BK queue is the lowest priority h/w queue */
    if (!ATH_SET_TX(sc)) {
        printk( "%s[%d]: Failed to set Tx h/w queue\n", __func__, __LINE__ );
        error = -EIO;
        goto bad2;
    }

    if(sc->sc_reg_parm.burstEnable) {
        sc->sc_aggr_burst = true;
        sc->sc_aggr_burst_duration = sc->sc_reg_parm.burstDur;
    } else {
        sc->sc_aggr_burst = false;
        sc->sc_aggr_burst_duration = ATH_BURST_DURATION;
    }

    if (sc->sc_aggr_burst) {
        ath_txq_burst_update(sc, HAL_WME_AC_BE,
                             sc->sc_aggr_burst_duration);
    }
    ath_htc_host_txep_init(sc);

#if ATH_WOW
    /*
     * If hardware supports wow, allocate memory for wow information.
     */
    sc->sc_hasWow = ath_hal_hasWow(ah);
    if (sc->sc_hasWow)
    {
        sc->sc_wowInfo = (struct wow_info *)OS_MALLOC(sc->sc_osdev, sizeof(struct wow_info), GFP_KERNEL);
        if (sc->sc_wowInfo != NULL) {
            OS_MEMZERO(sc->sc_wowInfo, sizeof(struct wow_info));

#if ATH_WOW_OFFLOAD
            /* Check if WoW offload is supported */
            sc->sc_hasWowGtkOffload     = ath_hal_wowGtkOffloadSupported(ah);
            sc->sc_hasWowArpOffload     = ath_hal_wowArpOffloadSupported(ah);
            sc->sc_hasWowNsOffload      = ath_hal_wowNsOffloadSupported(ah);
            sc->sc_wowSupport4wayHs     = ath_hal_wow4wayHsSupported(ah);
            sc->sc_wowSupportAcerMagic  = ath_hal_wowAcerMagicSupported(ah);
            sc->sc_wowSupportAcerSwKa   = ath_hal_wowAcerSwKaSupported(ah);
            sc->sc_wowSupportOffload    = sc->sc_hasWowGtkOffload    ||
                                          sc->sc_hasWowArpOffload    ||
                                          sc->sc_hasWowNsOffload     ||
                                          sc->sc_wowSupport4wayHs    ||
                                          sc->sc_wowSupportAcerMagic ||
                                          sc->sc_wowSupportAcerSwKa;

            /*
             * The rekeyinfo is enabled as long as any SW WOW offload enabled
             * because it contains the local MAC and BSSID info.
             */
            if (sc->sc_wowSupportOffload) {
                sc->sc_wowInfo->rekeyInfo = (struct wow_offload_rekey_info *)OS_MALLOC(sc->sc_osdev,
                        sizeof(struct wow_offload_rekey_info), GFP_KERNEL);
                if (sc->sc_wowInfo->rekeyInfo != NULL) {
                    OS_MEMZERO(sc->sc_wowInfo->rekeyInfo, sizeof(struct wow_offload_rekey_info));
                }
                else {
                    error = -ENOMEM;
                    goto bad2;
                }
            }
#endif /* ATH_WOW_OFFLOAD */
        } else {
            printk( "%s[%d]: Allocation of WoW information failed!\n", __func__, __LINE__ );
            error = -ENOMEM;
            goto bad2;
        }
    }
    sc->sc_wow_sleep = 0;
#endif

    if (ath_hal_htsupported(ah))
        sc->sc_hashtsupport=1;

     if (ath_hal_ht20sgisupported(ah) &&
         !sc->sc_reg_parm.disable_ht20_sgi)
     {
         sc->sc_ht20sgisupport=1;
     } else {
         sc->sc_ht20sgisupport=0;
     }

    if (ath_hal_hasrxtxabortsupport(ah)) {
        /* TODO: allow this to be runtime configurable */
        sc->sc_fastabortenabled = 1;
    }

    sc->sc_setdefantenna = ath_setdefantenna;
    sc->sc_rc = ath_rate_attach(ah);
    if (sc->sc_rc == NULL) {
        printk( "%s[%d]: Rate attach failed!\n", __func__, __LINE__ );
        error = EIO;
        goto bad2;
    }

    if (!ath_hal_getanipollinterval(ah, &cal_interval)) {
        cal_interval = ATH_SHORT_CALINTERVAL;
    }
#if ATH_LOW_PRIORITY_CALIBRATE
    ath_init_defer_call(sc->sc_osdev, &sc->sc_calibrate_deferred, ath_calibrate, sc);
    ath_initialize_timer(sc->sc_osdev, &sc->sc_cal_ch, cal_interval, ath_calibrate_defer, sc);
#else
    ath_initialize_timer(sc->sc_osdev, &sc->sc_cal_ch, cal_interval, ath_calibrate, sc);
#endif
    ath_initialize_timer(sc->sc_osdev, &sc->sc_up_beacon, ATH_UP_BEACON_INTVAL, ath_up_beacon, sc);

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_TRAFFIC_FAST_RECOVER, 0, NULL) == HAL_OK) {
        ath_initialize_timer(sc->sc_osdev, &sc->sc_traffic_fast_recover_timer, 100, ath_traffic_fast_recover, sc);
    }
#endif

#ifdef ATH_SUPPORT_TxBF
    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_initialize_timer(sc->sc_osdev, &sc->sc_selfgen_timer,
            LOWEST_RATE_RESET_PERIOD, ath_sc_reset_lowest_txrate, sc);
        ath_set_timer_period(&sc->sc_selfgen_timer, LOWEST_RATE_RESET_PERIOD);
        ath_start_timer(&sc->sc_selfgen_timer);
    }
#endif

#ifdef ATH_TX_INACT_TIMER
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_inact, ath_inact, sc, QDF_TIMER_TYPE_WAKE_APPS);
    sc->sc_tx_inact = 0;
    sc->sc_max_inact = 5;
    OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
#endif

    sc->sc_rxstuck_shutdown_flag = 0;
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_RXDEAF_WAR_NEEDED, 0, NULL) == HAL_OK) {
        OS_INIT_TIMER(sc->sc_osdev, &sc->sc_rxstuck, ath_rx_stuck_checker, sc, QDF_TIMER_TYPE_WAKE_APPS);
        OS_INIT_TIMER(sc->sc_osdev, &sc->sc_hwstuck, ath_rx_hwreg_checker, sc, QDF_TIMER_TYPE_WAKE_APPS);
        OS_SET_TIMER(&sc->sc_rxstuck, RXSTUCK_TIMER_VAL);
    }

#if ATH_HW_TXQ_STUCK_WAR
    if(sc->sc_enhanceddmasupport) {
        OS_INIT_TIMER(sc->sc_osdev, &sc->sc_qstuck, ath_txq_stuck_checker, sc, QDF_TIMER_TYPE_WAKE_APPS);
        OS_SET_TIMER(&sc->sc_qstuck, QSTUCK_TIMER_VAL);
        sc->sc_last_rxeol = 0;
    }
#endif

    sc->sc_consecutive_rxeol_count = 0;

#ifdef ATH_TX99_DIAG
    sc->sc_tx99 = NULL;
    if (tx99_attach(sc)) {
        error = EIO;
        printk("%s TX99 init failed\n", __func__);
    }
#endif

#if WLAN_SPECTRAL_ENABLE
    error = pdev_spectral_postinit(sc);
#endif

    // initialize LED module
    ath_led_initialize_control(sc, sc->sc_osdev, sc->sc_ah,
                               &sc->sc_led_control, &sc->sc_reg_parm,
                               !ath_hal_hasPciePwrsave(sc->sc_ah));

    sc->sc_sw_phystate = sc->sc_hw_phystate = 1;    /* default hw/sw phy state to be on */
    if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP)) {
        /*
         * Whether we should enable h/w TKIP MIC.
         * XXX: if we don't support WME TKIP MIC, then we wouldn't report WMM capable,
         * so it's always safe to turn on TKIP MIC in this case.
         */
        ath_hal_setcapability(sc->sc_ah, HAL_CAP_TKIP_MIC, 0, 1, NULL);
    }
    sc->sc_hasclrkey = ath_hal_ciphersupported(ah, HAL_CIPHER_CLR);
    /* turn on mcast key search if possible */
    if (ath_hal_hasmcastkeysearch(ah)) {
        (void) ath_hal_setmcastkeysearch(ah, 1);
    }
    /*
     * TPC support can be done either with a global cap or
     * per-packet support.  The latter is not available on
     * all parts.  We're a bit pedantic here as all parts
     * support a global cap.
     */
    sc->sc_hastpc = ath_hal_hastpc(ah);
    init_sc_params_complete(sc,sc->sc_ah);
    ATH_TXPOWER_INIT(sc);
    sc->sc_config.txpowlimit_override = sc->sc_reg_parm.overRideTxPower;
    /*
     * Adaptive Power Management:
     * Some 3 stream chips exceed the PCIe power requirements.
     * This workaround will reduce power consumption by using 2 tx chains
     * for 1 and 2 stream rates (5 GHz only)
     *
     */
    if (ath_hal_getenableapm(ah, &enableapm)) {
         sc->sc_enableapm = enableapm;
    }

    /* Initialize ForcePPM module */
    ath_force_ppm_attach(&sc->sc_ppm_info,
                                    sc,
                                    sc->sc_osdev,
                                    ah,
                                    &sc->sc_reg_parm);

    /*
     * Query the hal about antenna support
     * Enable rx fast diversity if hal has support
     */
    if (ath_hal_hasdiversity(ah)) {
        sc->sc_hasdiversity = 1;
        ath_hal_setdiversity(ah, AH_TRUE);
#if UNIFIED_SMARTANTENNA
        sc->sc_diversity = 0;
#else
        sc->sc_diversity = 1;
#endif
    } else {
        sc->sc_hasdiversity = 0;
        sc->sc_diversity=0;
        ath_hal_setdiversity(ah, AH_FALSE);
    }
    sc->sc_defant = ath_hal_getdefantenna(ah);

    /*
     * Not all chips have the VEOL support we want to
     * use with IBSS beacons; check here for it.
     */
    sc->sc_hasveol = ath_hal_hasveol(ah);

    ath_hal_getmac(ah, sc->sc_myaddr);
    if (sc->sc_hasbmask) {
        ath_hal_getbssidmask(ah, sc->sc_bssidmask);
        ATH_SET_VAP_BSSID_MASK(sc->sc_bssidmask);
        ath_hal_setbssidmask(ah, sc->sc_bssidmask);
    }

#ifdef ATH_RFKILL
    if (ath_hal_hasrfkill(ah) && !sc->sc_reg_parm.disableRFKill) {
        sc->sc_hasrfkill = 1;
        ath_rfkill_attach(sc);
    }
#endif

    sc->sc_wpsgpiointr = 0;
    sc->sc_wpsbuttonpushed = 0;
    if (sc->sc_reg_parm.wpsButtonGpio && ath_hal_haswpsbutton(ah)) {
        /* Set corresponding line as input as older versions may have had it as output (bug 37281) */
        ath_hal_gpioCfgInput(ah, sc->sc_reg_parm.wpsButtonGpio);
        ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_LOW);
        sc->sc_wpsgpiointr = 1;
    }

    sc->sc_hasautosleep = ath_hal_hasautosleep(ah);
    sc->sc_waitbeacon   = 0;

#ifdef ATH_GEN_TIMER
    if (ath_hal_hasgentimer(ah)) {
        sc->sc_hasgentimer = 1;
        ath_gen_timer_attach(sc);
    }
#if ATH_C3_WAR
    /* Work around for low PCIe bus utilization when CPU is in C3 state */
    if (sc->sc_reg_parm.c3WarTimerPeriod && sc->sc_hasgentimer) {
        sc->sc_c3_war_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  ath_c3_war_handler,
                                                  ath_c3_war_handler,
                                                  NULL,
                                                  sc);
        sc->c3_war_timer_active = 0;
        atomic_set(&sc->sc_pending_tx_data_frames, 0);
        sc->sc_fifo_underrun = 0;
        sc->sc_txop_burst = 0;
        spin_lock_init(&sc->sc_c3_war_lock);

    } else {
        sc->sc_c3_war_timer = NULL;
    }
#endif

#if ATH_RX_LOOPLIMIT_TIMER
    if (sc->sc_hasgentimer) {
        sc->sc_rx_looplimit_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  (ath_hwtimer_function)ath_rx_looplimit_handler,
                                                  (ath_hwtimer_function)ath_rx_looplimit_handler,
                                                  NULL,
                                                  sc);
        printk("Register the looplimit timer\n");
    } else {
        sc->sc_rx_looplimit_timer = NULL;
    }
#endif
#endif

    atomic_set(&sc->sc_hwif_stop_refcnt,1);

#ifdef ATH_BT_COEX
    if ((sc->sc_reg_parm.bt_reg.btCoexEnable) && ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_attach(sc);
    }
#endif

#if ATH_SUPPORT_MCI
    if (ath_hal_hasmci(sc->sc_ah)) {
        ath_coex_mci_attach(sc);
    }
#endif


    sc->sc_slottime = HAL_SLOT_TIME_9; /* default to short slot time */

    /* initialize beacon slots */
    for (i = 0; i < N(sc->sc_bslot); i++)
        sc->sc_bslot[i] = ATH_IF_ID_ANY;

#ifndef REMOVE_PKT_LOG
    //sc->pl_dev = &pl_dev;
   /* scn = ath_netdev_priv(osdev->netdev);*/
    if(enable_pktlog_support) {
        ath_pktlog_get_scn(osdev, &scn);
        /*
        * Get the dev name for the pl_dev handle
        */
        ath_pktlog_get_dev_name(osdev, scn);
        ath_pktlog_attach(scn);
    }
    sc->ah_mac_version = ath_hal_get_device_info(sc->sc_ah, HAL_MAC_VERSION);
    sc->ah_mac_rev = ath_hal_get_device_info(sc->sc_ah, HAL_MAC_REV);
#endif

    /* save MISC configurations */
#ifdef ATH_SWRETRY
    if (sc->sc_enhanceddmasupport) {
        if (sc->sc_reg_parm.numSwRetries) {
            sc->sc_swRetryEnabled = AH_TRUE;
            sc->sc_num_swretries = sc->sc_reg_parm.numSwRetries;
        }
    }
    else {
        if (sc->sc_reg_parm.numSwRetries &&
            (!(ath_hal_htsupported(ah)) ||
            (ath_hal_htsupported(ah) && !(sc->sc_txaggr)))) {
            sc->sc_swRetryEnabled = AH_TRUE;
            sc->sc_num_swretries = sc->sc_reg_parm.numSwRetries;
        }
    }
#endif
    ath_hal_pcieGetLcrOffset(ah, &pcieLcrOffset);
    sc->sc_config.pcieLcrOffset = (u_int8_t)pcieLcrOffset;

    sc->sc_config.swBeaconProcess = sc->sc_reg_parm.swBeaconProcess;
    ath_hal_get_hang_types(ah, &sc->sc_hang_war);
    sc->sc_slowAntDiv = (sc->sc_reg_parm.slowAntDivEnable) ? 1 : 0;

#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv) {
        ath_slow_ant_div_init(&sc->sc_antdiv, sc, sc->sc_reg_parm.slowAntDivThreshold,
                              sc->sc_reg_parm.slowAntDivMinDwellTime,
                              sc->sc_reg_parm.slowAntDivSettleTime);
    }
#endif

#if ATH_ANT_DIV_COMB
    sc->sc_antDivComb = ath_hal_AntDivCombSupport(ah);
    if (sc->sc_antDivComb) {
        sc->sc_antDivComb = AH_TRUE;
        ath_ant_div_comb_init(&sc->sc_antcomb, sc);
    }
    sc->sc_antDivComb_record = sc->sc_antDivComb;
    ath_hal_getsmartantenna(ah, (u_int32_t *)&sc->sc_smart_antenna);
    if (sc->sc_smart_antenna || sc->sc_reg_parm.forceSmartAntenna)
        ath_smartant_attach(sc);

#endif

    sc->sc_rxtimeout[WME_AC_VI] = ATH_RX_VI_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_VO] = ATH_RX_VO_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_BE] = ATH_RX_BE_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_BK] = ATH_RX_BK_TIMEOUT;

#ifdef ATH_MIB_INTR_FILTER
    sc->sc_intr_filter.state            = INTR_FILTER_OFF;
    sc->sc_intr_filter.activation_count = 0;
    sc->sc_intr_filter.intr_count       = 0;
#endif

    sc->sc_bbpanicsupport = (ath_hal_getcapability(ah, HAL_CAP_BB_PANIC_WATCHDOG, 0, NULL) == HAL_OK);

    sc->sc_num_cals = ath_hal_get_cal_intervals(ah, &sc->sc_cals, HAL_QUERY_CALS);
    sc->sc_pn_checknupdate_war = (ath_hal_getcapability(ah, HAL_CAP_PN_CHECK_WAR_SUPPORT, 0, NULL) == HAL_OK);

    sc->sc_min_cal = ATH_LONG_CALINTERVAL;
    for (i=0; i<sc->sc_num_cals; i++) {
        HAL_CALIBRATION_TIMER *cal_timerp = &sc->sc_cals[i];
        sc->sc_min_cal = min(sc->sc_min_cal, cal_timerp->cal_timer_interval);
    }

    ath_hal_get_config_param(sc->sc_ah, HAL_CONFIG_ENABLEMSI, &enable_msi);
    sc->sc_msi_enable = enable_msi ? 1 : 0;
    /*
     * sc is still not valid and we are in suspended state
     * until ath_open is called
     * keep the chip in disabled/full sleep state.
     */

    /* disable HAL and put h/w to sleep */
    ath_hal_disable(sc->sc_ah);

    ath_hal_configpcipowersave(sc->sc_ah, 1,1);

    ath_pwrsave_fullsleep(sc);


    ath_beacon_attach(sc);

    ath_scan_attach(sc);
#if ATH_GEN_RANDOMNESS
    ath_rng_start(sc);
#endif
    ath_initialize_timer(sc->sc_osdev, &sc->sc_chan_busy, ATH_CHAN_BUSY_INTVAL, ath_sc_chan_busy, sc);
    ath_set_timer_period(&sc->sc_chan_busy, ATH_CHAN_BUSY_INTVAL);
    ath_start_timer(&sc->sc_chan_busy);

    return 0;

bad2:
    /* cleanup tx queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
        if (ATH_TXQ_SETUP(sc, i))
            ath_tx_cleanupq(sc, &sc->sc_txq[i]);
#if ATH_WOW
     if (sc->sc_wowInfo) {
         OS_FREE(sc->sc_wowInfo);
     }
#endif
     if (sc->sc_rc) {
         ath_rate_detach(sc->sc_rc);
     }
#ifdef ATH_TX99_DIAG
    if (sc->sc_tx99) {
        tx99_detach(sc);
    }
#endif
#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill)
        ath_rfkill_detach(sc);
#endif

#ifdef ATH_GEN_TIMER
#if ATH_C3_WAR
    if (sc->sc_c3_war_timer) {
        ath_gen_timer_free(sc, sc->sc_c3_war_timer);
        sc->sc_c3_war_timer = NULL;
    }
#endif
#if ATH_RX_LOOPLIMIT_TIMER
    if (sc->sc_rx_looplimit_timer) {
        ath_gen_timer_free(sc, sc->sc_rx_looplimit_timer);
        sc->sc_rx_looplimit_timer = NULL;
    }
#endif
    if (sc->sc_hasgentimer)
        ath_gen_timer_detach(sc);
#endif
#ifdef ATH_BT_COEX
    if (ah && ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_detach(sc);
    }
#endif

#if ATH_SUPPORT_MCI
    if (sc->sc_mci.mci_support) {
        ath_coex_mci_detach(sc);
    }
#endif


bad:
    ath_dprintf_deregister(sc);

    if (ah) {
        ath_hal_detach(ah);
    }

    ATH_INTR_STATUS_LOCK_DESTROY(sc);
    //ATH_LOCK_DESTROY(sc);
    ATH_VAP_PAUSE_LOCK_DESTROY(sc);

    if (sc)
        OS_FREE(sc);

    /* null "out" parameters */
    *dev = NULL;
    *ops = NULL;

    return error;
#undef N
}
#endif // NO_HAL

#ifdef NO_HAL
void
ath_dev_free(ath_dev_t dev)
{
}
#else
void
ath_dev_free(ath_dev_t dev)
{
    struct ath_softc *sc = (struct ath_softc *)dev;
    struct ath_hal *ah = sc->sc_ah;
    int i;
#ifndef REMOVE_PKT_LOG
    ath_generic_softc_handle scn;
#endif

    DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);

    sc->sc_rxstuck_shutdown_flag = 1;

    ATH_USB_TQUEUE_FREE(sc);
    ATH_HTC_TXTQUEUE_FREE(sc);
    ATH_HTC_UAPSD_CREDITUPDATE_FREE(sc);
    ath_stop(sc);

    if (!sc->sc_invalid) {
        ath_hal_setpower(sc->sc_ah, HAL_PM_AWAKE);
    }

#if ATH_ANT_DIV_COMB
    if (sc->sc_smart_antenna)
        ath_smartant_detach(sc);
#endif

#ifdef ATH_TX_INACT_TIMER
    OS_CANCEL_TIMER(&sc->sc_inact);
#endif

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_RXDEAF_WAR_NEEDED, 0, NULL) == HAL_OK) {
        qdf_timer_sync_cancel(&sc->sc_rxstuck);
        qdf_timer_sync_cancel(&sc->sc_hwstuck);
    }

#if ATH_HW_TXQ_STUCK_WAR
    if(sc->sc_enhanceddmasupport) {
        OS_CANCEL_TIMER(&sc->sc_qstuck);
    }
#endif

#if ATH_WOW
     if (sc->sc_wowInfo) {
#if ATH_WOW_OFFLOAD
         if (sc->sc_wowInfo->rekeyInfo) {
             OS_FREE(sc->sc_wowInfo->rekeyInfo);
         }
#endif /* ATH_WOW_OFFLOAD */
         OS_FREE(sc->sc_wowInfo);
     }
#endif

    ath_rate_detach(sc->sc_rc);

#ifdef ATH_TX99_DIAG
    if (sc->sc_tx99) {
        tx99_detach(sc);
    }
#endif

    /* cleanup tx queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
        if (ATH_TXQ_SETUP(sc, i))
            ath_tx_cleanupq(sc, &sc->sc_txq[i]);

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill)
        ath_rfkill_detach(sc);
#endif

#ifdef ATH_GEN_TIMER
#if ATH_C3_WAR
    if (sc->sc_c3_war_timer) {
        ath_gen_timer_free(sc, sc->sc_c3_war_timer);
        sc->sc_c3_war_timer = NULL;
    }
#endif

#if ATH_RX_LOOPLIMIT_TIMER
    if (sc->sc_rx_looplimit_timer) {
        ath_gen_timer_free(sc, sc->sc_rx_looplimit_timer);
        sc->sc_rx_looplimit_timer = NULL;
    }
#endif
    if (sc->sc_hasgentimer)
        ath_gen_timer_detach(sc);
#endif   /* ATH_GEN_TIMER*/

#ifdef ATH_BT_COEX
    if (ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_detach(sc);
    }
#endif

#if ATH_SUPPORT_MCI
    if (sc->sc_mci.mci_support) {
        ath_coex_mci_detach(sc);
    }
#endif

#ifndef REMOVE_PKT_LOG
    //if (sc->pl_info)
    if(enable_pktlog_support) {
        ath_pktlog_get_scn(sc->sc_osdev, &scn);
        ath_pktlog_detach(scn);
    }
#endif
    ATH_INTR_STATUS_LOCK_DESTROY(sc);
    ATH_TXPOWER_DESTROY(sc);
    //ATH_LOCK_DESTROY(sc);
    ATH_RESET_LOCK_DESTROY(sc);
    ATH_RESET_SERIALIZE_LOCK_DESTROY(sc);
    ATH_VAP_PAUSE_LOCK_DESTROY(sc);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
    ATH_INTERNAL_RESET_LOCK_DESTROY(sc);
#endif

#if ATH_RESET_SERIAL
     for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
         ATH_RESET_LOCK_Q_DESTROY(sc, i);
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK_DESTROY(sc);
#endif
#ifdef ATH_SUPPORT_TxBF

    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_cancel_timer(&sc->sc_selfgen_timer, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_selfgen_timer);
    }
    ath_cancel_timer(&sc->sc_chan_busy, CANCEL_NO_SLEEP);
    ath_free_timer(&sc->sc_chan_busy);

#endif  // for TxBF RC
#if ATH_GEN_RANDOMNESS
    ath_rng_stop(sc);
#endif
#if ATH_LOW_PRIORITY_CALIBRATE
    ath_free_defer_call(&sc->sc_calibrate_deferred);
#endif
    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);
    ath_free_timer(&sc->sc_cal_ch);

    if (ath_timer_is_initialized(&sc->sc_up_beacon)) {
        ath_cancel_timer(&sc->sc_up_beacon, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_up_beacon);
    }
#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_traffic_fast_recover_timer);
    }
#endif
    ath_led_free_control(&sc->sc_led_control);
    ath_beacon_detach(sc);

	ath_scan_detach(sc);

    ath_hal_disable_interrupts_on_exit(ah);

    /* Detach HAL after the timers which still access the HAL */
    ath_hal_detach(ah);

    /* Deregister asf_print control object for DPRINTF */
    ath_dprintf_deregister(sc);

    OS_FREE(sc);
}
#endif //NO_HAL

static const char* hal_opmode2string( HAL_OPMODE opmode)
{
    switch ( opmode )
    {
    case HAL_M_STA:
         return "HAL_M_STA";
    case HAL_M_IBSS:
     return "HAL_M_IBSS";
    case HAL_M_HOSTAP:
     return "HAL_M_HOSTAP";
    case HAL_M_MONITOR:
         return "HAL_M_MONITOR";

    default:
     return "Unknown hal_opmode";
    }
};

#if WAR_DELETE_VAP
static int
ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons, void **ret_avp)
#else
static int
ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons)
#endif
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    int rx_mitigation;

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s : enter. dev=0x%pK, if_id=0x%x, if_data=0x%pK, opmode=%s, iv_opmode=%s, nostabeacons=0x%x\n",
            __func__,    dev,      if_id,      if_data,
            hal_opmode2string(opmode),hal_opmode2string(iv_opmode), nostabeacons
       );

#if WAR_DELETE_VAP
    // NOTE :: Not MP Safe. This is to be used to WAR an issue seen under Darwin while it is being root caused.
    if (if_id >= ATH_VAPSIZE)
    {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }

    if (sc->sc_vaps[if_id] != NULL) {
        // The previous delete did not go through. So, save this vap in the pending deletion list.
        sc->sc_vaps[if_id]->av_del_next = NULL;
        sc->sc_vaps[if_id]->av_del_prev = NULL;
        if (sc->sc_vap_del_pend_head == NULL) {
            sc->sc_vap_del_pend_head = sc->sc_vap_del_pend_tail = sc->sc_vaps[if_id];
        }
        else {
            sc->sc_vap_del_pend_tail->av_del_next = sc->sc_vaps[if_id];
            sc->sc_vaps[if_id]->av_del_prev = sc->sc_vap_del_pend_tail;
            sc->sc_vap_del_pend_tail = sc->sc_vaps[if_id];
        }
        sc->sc_vap_del_pend_count++;
        //sc->sc_vaps[if_id].av_del_timestamp = OS_GET_TIMESTAMP();
        sc->sc_vaps[if_id] = NULL;
        //now work around our work around. we deleted an item from the array, so decrease the counter
        //fixme - we should never come here, we need to know why this happens.
        if (sc->sc_nvaps) sc->sc_nvaps--;
        printk("%s: WAR_DELETE_VAP sc_nvaps=%d\n", __func__, sc->sc_nvaps);
    }
#else
    if (if_id >= ATH_VAPSIZE || sc->sc_vaps[if_id] != NULL) {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }
#endif

    switch (opmode) {
    case HAL_M_STA:
        sc->sc_nostabeacons = nostabeacons;
        break;
    case HAL_M_IBSS:
    case HAL_M_MONITOR:
        break;
    case HAL_M_HOSTAP:
        /* copy nostabeacons - for WDS client */
        sc->sc_nostabeacons = nostabeacons;
        /* XXX not right, beacon buffer is allocated on RUN trans */
        if (TAILQ_EMPTY(&sc->sc_bbuf)) {
            return -ENOMEM;
        }
        break;
    default:
        return -EINVAL;
    }

    /* create ath_vap */
    avp = (struct ath_vap *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_vap), GFP_KERNEL);
    if (avp == NULL)
        return -ENOMEM;

    OS_MEMZERO(avp, sizeof(struct ath_vap));
    avp->av_if_data = if_data;
    /* Set the VAP opmode */
    avp->av_opmode = iv_opmode;

    /*
     * The 9300 HAL always enables Rx mitigation (default), but this fails the
     * TGnInteropTP 5.2.9 STA test (inquiryl script).
     * So disable Rx Intr mitigation for STAs, to pass this test.
     * Note: This overrides the setting (if any) in HAL attach.
     *
     * FIXME: The assumption is that, if the 1st intf is a STA, it is more
     * likely that there are no other AP intfs (AP intfs are always first).
     */
    if (if_id == 0 && avp->av_opmode == HAL_M_STA) {
        rx_mitigation = 0;
        ath_hal_set_config_param(sc->sc_ah, HAL_CONFIG_INTR_MITIGATION_RX,
                                 &rx_mitigation);
    }

    avp->av_atvp = ath_rate_create_vap(sc->sc_rc, avp);
    if (avp->av_atvp == NULL) {
        OS_FREE(avp);
        return -ENOMEM;
    }

    OS_MEMSET(avp->av_txpow_mgt_frm, 0xff, sizeof(avp->av_txpow_mgt_frm));
    OS_MEMSET(avp->av_txpow_frm, 0xff, sizeof(avp->av_txpow_frm));
    avp->av_bslot = -1;
    TAILQ_INIT(&avp->av_mcastq.axq_q);
#ifdef ATH_BEACON_DEFERRED_PROC
        avp->av_mcastq.irq_shared = 0;
#else
        avp->av_mcastq.irq_shared = 1;
#endif

    ATH_TXQ_LOCK_INIT(&avp->av_mcastq);
    atomic_set(&avp->av_stop_beacon, 0);
    atomic_set(&avp->av_beacon_cabq_use_cnt, 0);

#if UMAC_SUPPORT_WNM
    {
        int i;
        for (i = 0; i < ATH_MAX_FMS_QUEUES; i++) {
            TAILQ_INIT(&avp->av_fmsq[i].axq_q);
#if ATH_BEACON_DEFERRED_PROC
            avp->av_fmsq[i].irq_shared = 0;
#else
            avp->av_fmsq[i].irq_shared = 1;
#endif
            ATH_TXQ_LOCK_INIT(&avp->av_fmsq[i]);
        }
    }
#endif /* UMAC_SUPPORT_WNM */

    if (sc->sc_hastsfadd) {
        /*
         * Multiple vaps are to transmit beacons and we
         * have h/w support for TSF adjusting; enable use
         * of staggered beacons.
         */
        /* XXX check for beacon interval too small */
        if (sc->sc_reg_parm.burst_beacons)
            sc->sc_stagbeacons = 0;
        else
            sc->sc_stagbeacons = 1;
    }
    if (sc->sc_hastsfadd)
        ath_hal_settsfadjust(sc->sc_ah, sc->sc_stagbeacons);


    sc->sc_vaps[if_id] = avp;
    sc->sc_nvaps++;
    /* Set the device opmode */
    sc->sc_opmode = opmode;

    /* default VAP configuration */
    avp->av_config.av_fixed_rateset = IEEE80211_FIXED_RATE_NONE;
    avp->av_config.av_fixed_retryset = 0x03030303;

#if WAR_DELETE_VAP
    // Load the return vap pointer. This will be passed in during deletion.
    *ret_avp = (void *)avp;
#endif

    ath_vap_info_attach(sc, avp);

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: exit.\n",
            __func__
        );

    return 0;
}

static int
#if WAR_DELETE_VAP
ath_vap_detach(ath_dev_t dev, int if_id, void *athvap)
#else
ath_vap_detach(ath_dev_t dev, int if_id)
#endif
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
#if 0
    /* Unused variables. */
    struct ath_hal *ah = sc->sc_ah;
    int flags;
#endif

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: enter. dev=0x%pK, if_id=0x%x\n",
            __func__,
        dev,
        if_id
        );

    ATH_PS_WAKEUP(sc);
#if WAR_DELETE_VAP
    avp = (struct ath_vap *)athvap;
#else
    avp = sc->sc_vaps[if_id];
#endif
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        ATH_PS_SLEEP(sc);
        return -EINVAL;
    }

    ath_vap_info_detach(sc, avp);

#if 0
    /*
     * In multiple vap scenario, this will cause data loss in other vaps.
     * Presumbly, there should be no resource associate with this vap.
     */
    flags = sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee);
    if (flags & ATH_NETIF_RUNNING) {
        /*
         * Quiesce the hardware while we remove the vap.  In
         * particular we need to reclaim all references to the
         * vap state by any frames pending on the tx queues.
         *
         * XXX can we do this w/o affecting other vap's?
         */
        ath_hal_intrset(ah, 0);         /* disable interrupts */
        ath_draintxq(sc, AH_FALSE, 0);  /* stop xmit side */
        ATH_STOPRECV(sc, 0);            /* stop recv side */
        ath_flushrecv(sc);              /* flush recv queue */
    }
#endif

    /*
     * Reclaim any pending mcast bufs on the vap.
     */
    ath_tx_mcast_draintxq(sc, &avp->av_mcastq);
    ATH_TXQ_LOCK_DESTROY(&avp->av_mcastq);
#if UMAC_SUPPORT_WNM
    if (sc->sc_ieee_ops->wnm_fms_enabled(sc->sc_ieee, if_id))
    {
        int i;
        for (i = 0; i < ATH_MAX_FMS_QUEUES; i++) {
            ath_tx_mcast_draintxq(sc, &avp->av_fmsq[i]);
            ATH_TXQ_LOCK_DESTROY(&avp->av_fmsq[i]);
        }
    }
#endif /* UMAC_SUPPORT_WNM */

    if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_nostabeacons)
        sc->sc_nostabeacons = 0;

    ath_rate_free_vap(avp->av_atvp);
#if WAR_DELETE_VAP
    // Relink pointers in the queue.
    if (sc->sc_vap_del_pend_head) {
        if (avp == sc->sc_vap_del_pend_head) {
            sc->sc_vap_del_pend_head = avp->av_del_next;
            if (sc->sc_vap_del_pend_head) {
                sc->sc_vap_del_pend_head->av_del_prev = NULL;
            }
            else {
                sc->sc_vap_del_pend_tail = NULL;
            }
        }
        else if (avp == sc->sc_vap_del_pend_tail) {
            sc->sc_vap_del_pend_tail = avp->av_del_prev;
            sc->sc_vap_del_pend_tail->av_del_next = NULL;
        }
        else {
            if (avp->av_del_prev) {
                avp->av_del_prev->av_del_next = avp->av_del_next;
            }
            if (avp->av_del_next) {
                avp->av_del_next->av_del_prev = avp->av_del_prev;
            }
        }
        sc->sc_vap_del_pend_count--;
    }
#endif

#ifdef WAR_DELETE_VAP
    if (sc->sc_vaps[if_id] == avp) {
        sc->sc_vaps[if_id] = NULL;
    } else {
        //war case of a late delete, try to keep a sane sc_nvaps..
        sc->sc_nvaps++;
    }
#else
    sc->sc_vaps[if_id] = NULL;
#endif
    OS_FREE(avp);
    sc->sc_nvaps--;

#if 0
    /*
     * In multiple vap scenario, this will cause data loss in other vaps.
     * Presumbly, there should be no resource associate with this vap.
     */
    /*
    ** restart H/W in case there are other VAPs, however
    ** we must ensure that it's "valid" to have interrupts on at
    ** this time
    */

    if ((flags & ATH_NETIF_RUNNING) && sc->sc_nvaps && !sc->sc_invalid) {
        /*
         * Restart rx+tx machines if device is still running.
         */
        if (ATH_STARTRECV(sc) != 0)    /* restart recv */
            printk("%s: unable to start recv logic\n",__func__);
        ath_hal_intrset(ah, sc->sc_imask);
    }
#endif

    ATH_PS_SLEEP(sc);

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: exit.\n",
            __func__
        );
#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    return 0;
}

static int
ath_vap_config(ath_dev_t dev, int if_id, struct ath_vap_config *if_config)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;

    if (if_id >= ATH_VAPSIZE) {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }

    avp = sc->sc_vaps[if_id];
    ASSERT(avp != NULL);

    if (avp) {
        OS_MEMCPY(&avp->av_config, if_config, sizeof(avp->av_config));
    }

    return 0;
}

#if AR_DEBUG
static void ath_sm_update_announce(ath_dev_t dev, ath_node_t node)
{
    char smpstate[32] = "";
    struct ath_node *an = ATH_NODE(node);
    int capflag = an->an_cap;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    OS_MEMCPY(smpstate,"", 1);
    if (capflag & ATH_RC_HT_FLAG) {
        if (an->an_smmode == ATH_SM_ENABLE) {
            OS_MEMCPY(smpstate, "SM Enabled", strlen("SM Enabled"));
        } else {
            if (an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) {
                OS_MEMCPY(smpstate, "SM Dynamic pwrsav",
                    strlen("SM Dynamic pwrsav"));
            } else {
                OS_MEMCPY(smpstate,"SM Static pwrsav",
                    strlen("SM Static pwrsav"));
            }
        }
    }

    DPRINTF(((struct ath_softc *)sc), ATH_DEBUG_PWR_SAVE,
           "%s capflag=0x%u Flags:%s%s%s%s\n",
           smpstate, capflag,
           (capflag & ATH_RC_HT_FLAG) ? " HT" : "",
           (capflag & ATH_RC_CW40_FLAG) ? " HT40" : "",
           (capflag & ATH_RC_SGI_FLAG) ? " SGI" : "",
           (capflag & ATH_RC_DS_FLAG) ? " DS" : "");

}
#endif /* AR_DEBUG */

static u_int32_t
ath_get_device_info(ath_dev_t dev, u_int32_t type)
{
    HAL_CHANNEL *chan;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t result = 0;

    switch (type) {
    case ATH_MAC_VERSION:        /* MAC version id */
        result = ath_hal_get_device_info(ah, HAL_MAC_VERSION);
        break;
    case ATH_MAC_REV:            /* MAC revision */
        result = ath_hal_get_device_info(ah, HAL_MAC_REV);
        break;
    case ATH_PHY_REV:            /* PHY revision */
        result = ath_hal_get_device_info(ah, HAL_PHY_REV);
        break;
    case ATH_ANALOG5GHZ_REV:     /* 5GHz radio revision */
        result = ath_hal_get_device_info(ah, HAL_ANALOG5GHZ_REV);
        break;
    case ATH_ANALOG2GHZ_REV:     /* 2GHz radio revision */
        result = ath_hal_get_device_info(ah, HAL_ANALOG2GHZ_REV);
        break;
    case ATH_WIRELESSMODE:       /* Wireless mode */
        chan = &sc->sc_curchan;
        result = ath_halchan2mode(chan);
        break;
    default:
        //printk("AthDeviceInfo: Unknown type %d\n", type);
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: Unknown type %d\n", __func__, type);
        ASSERT(0);
    }
    return result;
}

static void
ath_update_sm_pwrsave(ath_dev_t dev, ath_node_t node, ATH_SM_PWRSAV mode,
    int ratechg)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    switch (sc->sc_opmode) {
    case HAL_M_HOSTAP:
        /*
         * Update spatial multiplexing mode of client node.
         */
        if (an != NULL) {
            an->an_smmode = mode;
            DPRINTF(sc,ATH_DEBUG_PWR_SAVE,"%s: ancaps %#x\n", __func__, an->an_smmode);
            if (ratechg) {
                ath_rate_node_update(an);
#if AR_DEBUG
                ath_sm_update_announce(dev, node);
#endif
            }
        }
        break;

    case HAL_M_STA:
        switch (mode) {
        case ATH_SM_ENABLE:
            /* Spatial Multiplexing enabled, revert back to default SMPS register settings */
            ath_hal_setsmpsmode(sc->sc_ah, HAL_SMPS_DEFAULT);
            break;
        case ATH_SM_PWRSAV_STATIC:
            /*
             * Update current chainmask from configuration parameter.
             * A subsequent reset is needed for new chainmask to take effect.
             */
            sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
            sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
            break;
        case ATH_SM_PWRSAV_DYNAMIC:
            /* Enable hardware control of SM Power Save */
            ath_hal_setsmpsmode(sc->sc_ah, HAL_SMPS_HW_CTRL);
            break;
        default:
            break;
        }
        if (an != NULL) {
            an->an_smmode = mode;
            if (ratechg)
                ath_rate_node_update(an);
        }
        break;

    case HAL_M_IBSS:
    case HAL_M_MONITOR:
    default:
        /* Not supported */
        break;
    }
}

static u_int32_t
ath_node_gettxrate(ath_node_t node)
{
    struct ath_node *an = (struct ath_node *) node;

    return ath_rate_node_gettxrate(an);
}

void
ath_node_set_fixedrate(ath_node_t node, u_int8_t fixed_ratecode)
{
    struct ath_node *an = (struct ath_node *) node;
    struct ath_softc *sc = an->an_sc;
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];

    u_int8_t i;
    u_int8_t ndx = 0;

    for (i = 0; i < rt->rateCount; i++) {
        if (rt->info[i].rate_code == fixed_ratecode) {
            ndx = i;
            break;
        }
    }

    an->an_fixedrate_enable = fixed_ratecode == 0 ? 0 : 1;
    an->an_fixedrix = ndx;
    an->an_fixedratecode = fixed_ratecode;
    printk("%s: an_fixedrate_enable = %d, an_fixedrix = %d, an_fixedratecode = 0x%x\n",
            __func__, an->an_fixedrate_enable, an->an_fixedrix, an->an_fixedratecode);

    return;
}


bool
ath_rate_check(ath_dev_t dev,int val)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];
    u_int8_t i;

    for (i = 0; i < rt->rateCount; i++) {
        if (rt->info[i].rateKbps == val) {
            return true;
        }
    }

    return false;
}

static u_int32_t
ath_node_getmaxphyrate(ath_dev_t dev, ath_node_t node)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    return ath_rate_getmaxphyrate(sc, an);
}

static int
athop_rate_newassoc(ath_dev_t dev, ath_node_t node, int isnew, unsigned int capflag,
                                  struct ieee80211_rateset *negotiated_rates,
                                  struct ieee80211_rateset *negotiated_htrates)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    ath_wmi_rc_node_update(dev, an, isnew, capflag, negotiated_rates, negotiated_htrates);

    return ath_rate_newassoc(sc, an, isnew, capflag, negotiated_rates, negotiated_htrates);
}

static void
athop_rate_newstate(ath_dev_t dev, int if_id, int up)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp = sc->sc_vaps[if_id];

    ath_rate_newstate(sc, avp, up);
}


static ath_node_t
ath_node_attach(ath_dev_t dev, int if_id, ieee80211_node_t ni, bool tmpnode, void *peer)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    struct ath_node *an;
    int i;

    avp = sc->sc_vaps[if_id];
    ASSERT(avp != NULL);

    an = (struct ath_node *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_node), GFP_ATOMIC);
    if (an == NULL) {
        return NULL;
    }
    OS_MEMZERO(an, sizeof(*an));

    an->an_node = ni;
    an->an_peer = peer;
    an->an_sc = sc;
    an->an_avp = avp;

    an->an_rc_node = ath_rate_node_alloc(avp->av_atvp);
    if (an->an_rc_node == NULL) {
        OS_FREE(an);
        return NULL;
    }

    /* XXX no need to setup vap pointer in ni here.
     * It should already be taken care of by caller. */
    ath_rate_node_init(an->an_rc_node);

    if (tmpnode) {
        an->an_flags |= ATH_NODE_TEMP;
    }

    /* set up per-node tx/rx state */
    ath_tx_node_init(sc, an);
    ath_rx_node_init(sc, an);

#ifdef ATH_SUPPORT_UAPSD
    /* U-APSD init */
    TAILQ_INIT(&an->an_uapsd_q);
#ifdef ATH_VAP_PAUSE_SUPPORT
    TAILQ_INIT(&an->an_uapsd_stage_q);
#endif
    an->an_uapsd_qdepth = 0;
#endif

#if QCA_AIRTIME_FAIRNESS
    ATH_NODE_ATF_TOKEN_LOCK_INIT(an);
    an->an_atf_nodepaused = 0;
    an->an_tx_unusable_tokens_updated = 0;
    an->an_atf_nomore_tokens = 0;
    an->an_atf_nomore_tokens_show = 0;
    an->an_old_max_tput = 350000;
#endif

#ifdef ATH_SWRETRY
    /* As of now, SW Retry mechanism will be enabled only when
     * STA enters into RUN state. Need to revisit this part
     * if Sw retries are to be enabled right from JOIN state
     */
    ATH_NODE_SWRETRY_TXBUF_LOCK_INIT(an);
    TAILQ_INIT(&an->an_softxmit_q);
    an->an_swrenabled = AH_FALSE;
    an->an_softxmit_qdepth = 0;
    an->an_total_swrtx_successfrms = 0;
    an->an_total_swrtx_flushfrms = 0;
    an->an_total_swrtx_pendfrms = 0;
    atomic_set(&an->an_pspoll_pending, AH_FALSE);
    atomic_set(&an->an_pspoll_response, AH_FALSE);

    an->an_tim_set = AH_FALSE;
#ifdef ATH_SUPPORT_UAPSD
    an->an_uapsd_num_addbuf = 0;
#endif
    for (i=0; i<HAL_NUM_TX_QUEUES; i++) {
        (an->an_swretry_info[i]).swr_state_filtering = AH_FALSE;
        (an->an_swretry_info[i]).swr_num_eligible_frms = 0;
    }
#endif
#ifdef ATH_CHAINMASK_SELECT
    ath_chainmask_sel_init(sc, an);
    ath_chainmask_sel_timerstart(&an->an_chainmask_sel);
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK(sc);
    LIST_INSERT_HEAD(&sc->sc_nt, an, an_le);
    ATH_NODETABLE_UNLOCK(sc);
#endif

#if ATH_SUPPORT_VOWEXT
    an->min_depth = ATH_AGGR_MIN_QDEPTH;
    an->throttle = 0;
#endif
    an->an_rc_node->txRateCtrl.rate_fast_drop_en =
        avp->av_config.av_rc_txrate_fast_drop_en;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    ath_node_pwrsaveq_attach(an);
#endif

    TAILQ_INIT(&an->an_tx_frag_q);

#if UNIFIED_SMARTANTENNA
    for (i=0; i<= sc->max_fallback_rates; i++) {
        if (SMART_ANT_ENABLED(sc)) {
            an->smart_ant_antenna_array[i] = sc->smart_ant_tx_default;
        }
    }
#endif

    an->an_fixedrate_enable = 0;
    an->an_fixedrix = 0;    /* Lowest rate by default if fixed rate is enabled for the node */

    an->an_tx_chainmask = 0;    /* by default use the global sc_tx_chainmask */

/*
 * Init the RSSI and rate information
 */

    an->an_avgrssi   = ATH_RSSI_DUMMY_MARKER;
    an->an_avgbrssi  = ATH_RSSI_DUMMY_MARKER;
    an->an_avgdrssi  = ATH_RSSI_DUMMY_MARKER;
    an->an_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
    an->an_avgtxrate = ATH_RATE_DUMMY_MARKER;
    an->an_avgrxrate = ATH_RATE_DUMMY_MARKER;
    an->an_lasttxrate = ATH_RATE_DUMMY_MARKER;
    an->an_lastrxrate = ATH_RATE_DUMMY_MARKER;
    for (i = 0; i < ATH_HOST_MAX_ANTENNA; ++i) {
        an->an_avgtxchainrssi[i]    = ATH_RSSI_DUMMY_MARKER;
        an->an_avgtxchainrssiext[i] = ATH_RSSI_DUMMY_MARKER;
        an->an_avgbchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        an->an_avgbchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
        an->an_avgdchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        an->an_avgdchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
        an->an_avgchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        an->an_avgchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
    }

    DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %pK\n", __func__, an);
    return an;
}

static void
ath_node_detach(ath_dev_t dev, ath_node_t node)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

    an->an_peer = NULL;
    ath_tx_node_free(sc, an);
    ath_rx_node_free(sc, an);

    ath_rate_node_free(an->an_rc_node);

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK_BH(sc);
    LIST_REMOVE(an, an_le);
    ATH_NODETABLE_UNLOCK_BH(sc);
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    ath_node_pwrsaveq_detach(an);
#endif

    OS_FREE(an);
}

void
ath_tx_frag_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_buf *bf = NULL;
    struct ath_buf *lastbf = NULL;
    ath_bufhead bf_head;

    do {
        TAILQ_INIT(&bf_head);
        bf = TAILQ_FIRST(&an->an_tx_frag_q);
        if (!bf)
            break;

        lastbf = bf->bf_lastfrm;
        TAILQ_REMOVE_HEAD_UNTIL(&an->an_tx_frag_q, &bf_head, lastbf, bf_list);
#ifdef  ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif

    } while (TRUE);
}

static int
ath_node_cleanup(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

#ifdef ATH_CHAINMASK_SELECT
    ath_chainmask_sel_timerstop(&an->an_chainmask_sel);
#endif

    an->an_flags |= ATH_NODE_CLEAN;

    /*
     * We could have been called from thread or tasklet. Waiting is not an option.
     * If the active_tx_cnt is non-zero, simply return. flagging ATH_NODE_CLEAN
     * above will make sure no more pkts are processed by LMAC.
     * We hope to be called again during UMAC node free to cleanup.
     * It allows any tx threads to complete and dec active_tx_cnt.
     */
    if (atomic_read(&an->an_active_tx_cnt)) {
        printk("skipping lmac node %pK cleanup due to tx activity\n", an);
        return 1;
    }

#ifdef ATH_SWRETRY
    if (sc->sc_swRetryEnabled) {
        ATH_NODE_SWRETRY_TXBUF_LOCK(an);
        an->an_flags |= ATH_NODE_SWRETRYQ_FLUSH;
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
        ath_tx_flush_node_sxmitq(sc, an);
    }
    if (an->an_tim_set == AH_TRUE)
    {
        ATH_NODE_SWRETRY_TXBUF_LOCK(an);
        an->an_tim_set = AH_FALSE;
        sc->sc_ieee_ops->set_tim(an->an_node,0);
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
    }
#endif
#ifdef ATH_SUPPORT_UAPSD
    if (an->an_flags & ATH_NODE_UAPSD) {
        an->an_flags &= ~ATH_NODE_UAPSD;
        ATH_TX_UAPSD_NODE_CLEANUP(sc, an);
    }
#endif
    ath_tx_frag_cleanup(sc, an);
    ATH_TX_NODE_CLEANUP(sc, an);
    ath_rx_node_cleanup(sc, an);
    ath_rate_node_cleanup(an->an_rc_node);

    return 0;
}

static void
ath_node_update_pwrsave(ath_dev_t dev, ath_node_t node, int pwrsave, int pause_resume)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

#ifdef ATH_SWRETRY
    ATH_NODE_SWRETRY_TXBUF_LOCK(an);
    atomic_set(&an->an_pspoll_pending, AH_FALSE);
    atomic_set(&an->an_pspoll_response, AH_FALSE);
#endif

    if (pwrsave) {
        an->an_flags |= ATH_NODE_PWRSAVE;
#ifdef  ATH_SWRETRY
        if (!an->an_tim_set &&
#if LMAC_SUPPORT_POWERSAVE_QUEUE
            sc->sc_ath_ops.get_pwrsaveq_len(an, 0)==0 &&
#else
            sc->sc_ieee_ops->get_pwrsaveq_len(an->an_node)==0 &&
#endif
            (ath_exist_pendingfrm_tidq(dev,an)==0))
        {
            sc->sc_ieee_ops->set_tim(an->an_node,1);
            an->an_tim_set = AH_TRUE;
        }
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
#endif
        if (pause_resume) {
            ath_tx_node_pause(sc,an);
        }
    } else {
        an->an_flags &= ~ATH_NODE_PWRSAVE;
#ifdef ATH_SWRETRY
        if (an->an_tim_set &&
#if LMAC_SUPPORT_POWERSAVE_QUEUE
            sc->sc_ath_ops.get_pwrsaveq_len(an, 0)==0)
#else
            sc->sc_ieee_ops->get_pwrsaveq_len(an->an_node)==0)
#endif
        {
            sc->sc_ieee_ops->set_tim(an->an_node,0);
            an->an_tim_set = AH_FALSE;
        }
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
#endif
        if (pause_resume) {
            ath_tx_node_resume(sc,an);
        }
    }
}
/*
 * Allocate data  key slots for TKIP.  We allocate two slots for
 * one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t
ath_key_alloc_pair(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    /* XXX could optimize */
    for (i = 0; i < N(sc->sc_keymap)/2; i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots in this byte are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
again:
                keyix++;
                b >>= 1;
            }
            /* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
            if ( isset(sc->sc_keymap, keyix+64) ) {
                /* pair unavailable */
                /* XXX statistic */
                if (keyix == (i+1)*NBBY || keyix+64 == (sizeof(sc->sc_keymap)-1)) {
                    /* no slots were appropriate, advance */
                    continue;
                }
                goto again;
            }
            setbit(sc->sc_keymap, keyix);
            setbit(sc->sc_keymap, keyix+64);
            DPRINTF(sc, ATH_DEBUG_KEYCACHE,
                "%s: key pair %u ,%u\n",
                __func__, keyix, keyix+64 );
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
    return -1;
#undef N
}

/*
 * Allocate tx/rx key slots for TKIP.  We allocate two slots for
 * each key, one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t
ath_key_alloc_2pair(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    KASSERT(ath_hal_tkipsplit(sc->sc_ah), ("key cache !split"));
    /* XXX could optimize */
    for (i = 0; i < N(sc->sc_keymap)/4; i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots in this byte are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
again:
                keyix++;
                b >>= 1;
            }
            /* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
            if (isset(sc->sc_keymap, keyix+32) ||
                isset(sc->sc_keymap, keyix+64) ||
                isset(sc->sc_keymap, keyix+32+64)) {
                /* full pair unavailable */
                /* XXX statistic */
                if (keyix == (i+1)*NBBY) {
                    /* no slots were appropriate, advance */
                    continue;
                }
                goto again;
            }
            setbit(sc->sc_keymap, keyix);
            setbit(sc->sc_keymap, keyix+64);
            setbit(sc->sc_keymap, keyix+32);
            setbit(sc->sc_keymap, keyix+32+64);
#if 0
            DPRINTF(sc, ATH_DEBUG_KEYCACHE,
                    "%s: key pair %u,%u %u,%u\n",
                    __func__, keyix, keyix+64,
                    keyix+32, keyix+32+64);
#endif
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
    return -1;
#undef N
}

/*
 * Allocate a single key cache slot.
 */
static u_int16_t
ath_key_alloc_single(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    /* XXX try i,i+32,i+64,i+32+64 to minimize key pair conflicts */
    for (i = 0; i < N(sc->sc_keymap); i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
                keyix++, b >>= 1;
            }
            setbit(sc->sc_keymap, keyix);
            DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: key %u\n",
                    __func__, keyix);
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of space\n", __func__);
    return -1;
#undef N
}

static void
ath_key_reset(ath_dev_t dev, u_int16_t keyix, int freeslot)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_WAKEUP(sc);

    ath_hal_keyreset(sc->sc_ah, keyix);
    if (freeslot) {
        if (keyix < sizeof(sc->sc_keymap)*NBBY)
        clrbit(sc->sc_keymap, keyix);
    }
    ATH_PS_SLEEP(sc);
}

static int
ath_keyset(ath_dev_t dev, u_int16_t keyix, HAL_KEYVAL *hk,
           const u_int8_t mac[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_BOOL status;

    ATH_FUNC_ENTRY_CHECK(sc, 0);
    ATH_PS_WAKEUP(sc);

    status = ath_hal_keyset(sc->sc_ah, keyix, hk, mac);

    ATH_PS_SLEEP(sc);

    return (status != AH_FALSE);
}

static u_int
ath_keycache_size(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_keymax;
}

#if ATH_SUPPORT_KEYPLUMB_WAR
void
ath_keycache_print(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_printkeycache(sc->sc_ah);
}
OS_EXPORT_SYMBOL(ath_keycache_print);
#endif

/*
 * Set the 802.11D country
 */
static int
ath_set_country(ath_dev_t dev, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS  status;
    u_int8_t regclassids[ATH_REGCLASSIDS_MAX];
    u_int nregclass = 0;
    u_int wMode;
    u_int netBand;
    int outdoor = ath_outdoor;
    int indoor = ath_indoor;
    int err = 0;

    if(!isoName || !isoName[0] || !isoName[1]) {
        if (cc)
            sc->sc_config.ath_countrycode = cc;
        else
            sc->sc_config.ath_countrycode = CTRY_DEFAULT;
    } else {
        sc->sc_config.ath_countrycode =
                            findCountryCode((u_int8_t *)isoName);

        /* Map the ISO name ' ', 'I', 'O' */
        if (isoName[2] == 'O') {
            outdoor = AH_TRUE;
            indoor  = AH_FALSE;
        }
        else if (isoName[2] == 'I') {
            indoor  = AH_TRUE;
            outdoor = AH_FALSE;
        }
        else if ((isoName[2] == ' ') || (isoName[2] == 0)) {
            outdoor = AH_FALSE;
            indoor  = AH_FALSE;
        }
        else
            return -EINVAL;
    }

    wMode = sc->sc_reg_parm.wModeSelect;
    if (!(wMode & HAL_MODE_11A)) {
        wMode &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(wMode & HAL_MODE_11G)) {
        wMode &= ~(HAL_MODE_108G);
    }
    netBand = sc->sc_reg_parm.NetBand;
    if (!(netBand & HAL_MODE_11A)) {
        netBand &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(netBand & HAL_MODE_11G)) {
        netBand &= ~(HAL_MODE_108G);
    }
    wMode &= netBand;

    ATH_PS_WAKEUP(sc);
    ATH_DEV_RESET_HT_MODES(wMode);   /* reset the HT modes */

    /* Reset the regdomain if already configured before */
    ath_hal_setcapability(sc->sc_ah, HAL_CAP_REG_DMN, 0, 0, &status);

    if (sc->sc_ieee_ops->program_reg_cc) {
        err = sc->sc_ieee_ops->program_reg_cc(sc->sc_ieee, isoName, cc);
        if (err)
            return err;
    }

    if (sc->sc_ieee_ops->get_channel_list_from_regdb) {
        err = sc->sc_ieee_ops->get_channel_list_from_regdb(sc->sc_ieee);
        if (err)
            return err;
    }

    /* Initialize NF cal history buffer */
    ath_hal_init_NF_buffer_for_regdb_chans(ah, cc);

    ATH_PS_SLEEP(sc);

    if (sc->sc_ieee_ops->setup_channel_list) {
        sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, cmd,
                                            regclassids, nregclass,
                                            sc->sc_config.ath_countrycode);
    }

    return 0;
}

/*
 * Return the current country and domain information
 */
static void
ath_get_currentCountry(ath_dev_t dev, HAL_COUNTRY_ENTRY *ctry)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_getCurrentCountry(sc->sc_ah, ctry);

    /* If HAL not specific yet, since it is band dependent, use the one we passed in.*/
    if (ctry->countryCode == CTRY_DEFAULT) {
        ctry->iso[0] = 0;
        ctry->iso[1] = 0;
    }
    else if (ctry->iso[0] && ctry->iso[1]) {
        if (ath_outdoor)
            ctry->iso[2] = 'O';
        else if (ath_indoor)
            ctry->iso[2] = 'I';
        else
            ctry->iso[2] = ' ';
    }
}

static int
ath_set_regdomain(ath_dev_t dev, int regdomain, HAL_BOOL wrtEeprom)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_STATUS status;
    int ret;

    if (wrtEeprom) {
        ret = ath_hal_setregdomain(sc->sc_ah, regdomain, &status);
    }

        if (ath_hal_setcapability(sc->sc_ah, HAL_CAP_REG_DMN, 0, regdomain, &status))
            ret = !ath_getchannels(sc, CTRY_DEFAULT, AH_FALSE, AH_TRUE);
        else
            ret = AH_FALSE;

    printk(" %s: ret %d, regdomain %d \n", __func__, ret, regdomain);

    if (ret == AH_TRUE)
        return 0;
    else
        return -EIO;
}

static int
ath_get_regdomain(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t regdomain;
    HAL_STATUS status;

    status = ath_hal_getregdomain(sc->sc_ah, &regdomain);
    KASSERT(status == HAL_OK, ("get_regdomain failed"));

    return (regdomain);
}

static int
ath_set_quiet(ath_dev_t dev, u_int32_t period, u_int32_t duration,
              u_int32_t nextStart, HAL_QUIET_FLAG flag)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int retval;

    ATH_PS_WAKEUP(sc);
    retval = ath_hal_setQuiet(sc->sc_ah, period, duration, nextStart, flag);
    ATH_PS_SLEEP(sc);

    return retval;
}

static int
ath_get_dfsdomain(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t regdomain;
    HAL_STATUS status;

	status = ath_hal_getcapability(sc->sc_ah, HAL_CAP_DFS_DMN, 0, &regdomain);
    KASSERT(status == HAL_OK, ("get_dfsdomain failed"));

    return (regdomain);
}

static void ath_set_use_cac_prssi(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_ah->ah_use_cac_prssi = 1;
}

#if ATH_TX_DUTY_CYCLE
int
ath_set_tx_duty_cycle(ath_dev_t dev, int active_pct, bool enabled)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t duration;
    int status = 0;

    if (enabled) {
        /* enable tx dc */
        sc->sc_tx_dc_enable = 1;
        sc->sc_tx_dc_active_pct = active_pct;
        if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_nbcnvaps != 0) {
             /* config quiet time relative to beacons */
            ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);
        } else if (sc->sc_nvaps) {
            /* configure quiet time based on current TSF */
            duration = ((100-sc->sc_tx_dc_active_pct)*sc->sc_tx_dc_period)/100;
            status = ath_set_quiet(dev, sc->sc_tx_dc_period, duration, 0, true);
        }
    } else {
        sc->sc_tx_dc_enable = 0;
        status = ath_set_quiet(dev, 0, 0, 0, false);
    }

    return status;
}
#endif // ATH_TX_DUTY_CYCLE


static u_int16_t
ath_find_countrycode(ath_dev_t dev, char* isoName)
{
    UNREFERENCED_PARAMETER(dev);

    return findCountryCode((u_int8_t*)isoName);
}

static u_int8_t
ath_get_ctl_by_country(ath_dev_t dev, u_int8_t* isoName, HAL_BOOL is2G)
{
    HAL_CTRY_CODE cc;
    UNREFERENCED_PARAMETER(dev);

    cc = findCountryCode(isoName);

    return findCTLByCountryCode(cc, is2G);
}

static int
ath_set_tx_antenna_select(ath_dev_t dev, u_int32_t antenna_select)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t antenna_cfgd = 0;

    if (!sc->sc_cfg_txchainmask) {
        sc->sc_cfg_txchainmask = sc->sc_config.txchainmask;
        sc->sc_cfg_rxchainmask = sc->sc_config.rxchainmask;
        sc->sc_cfg_txchainmask_leg = sc->sc_config.txchainmasklegacy;
        sc->sc_cfg_rxchainmask_leg = sc->sc_config.rxchainmasklegacy;
    }

    if (ath_hal_setAntennaSwitch(sc->sc_ah, antenna_select, &sc->sc_curchan,
                                 &sc->sc_tx_chainmask, &sc->sc_rx_chainmask,
                                 &antenna_cfgd)) {
        if (antenna_cfgd) {
            if (antenna_select) {
                /* Overwrite chainmask configuration so that antenna selection is
                 * retained during join.
                 */
                sc->sc_config.txchainmask = sc->sc_tx_chainmask;
                sc->sc_config.rxchainmask = sc->sc_rx_chainmask;
                sc->sc_config.txchainmasklegacy = sc->sc_tx_chainmask;
                sc->sc_config.rxchainmasklegacy = sc->sc_rx_chainmask;
            } else {
                /* Restore original chainmask config */
                sc->sc_config.txchainmask = sc->sc_cfg_txchainmask;
                sc->sc_config.rxchainmask = sc->sc_cfg_rxchainmask;
                sc->sc_config.txchainmasklegacy = sc->sc_cfg_txchainmask_leg;
                sc->sc_config.rxchainmasklegacy = sc->sc_cfg_rxchainmask_leg;
                sc->sc_cfg_txchainmask = 0;
            }
        }
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Tx Antenna Switch. Do internal reset.\n", __func__);
        ath_internal_reset(sc);
        return 0;
    }
    return -EIO;
}

static u_int32_t
ath_get_current_tx_antenna(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_cur_txant;
}

static u_int32_t
ath_get_default_antenna(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_defant;
}

#ifdef DBG
static u_int32_t
ath_register_read(ath_dev_t dev, u_int32_t offset)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return(ath_hal_readRegister(sc->sc_ah, offset));
}

static void
ath_register_write(ath_dev_t dev, u_int32_t offset, u_int32_t value)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_writeRegister(sc->sc_ah, offset, value);
}
#endif

#if ATH_SUPPORT_WIFIPOS
/* Function     : ath_resched_txq
 * Params       : ath_dev
 * Functionality: If HW queue is empty, and SW queue is full.
 *                In certain circumstance we might not have scheduled
 *                any queue for transmission. So to restart the
 *                transmission, we can call this function. This function
 *                will look at all the queues sequencetially and if there
 *                is any SW queue with frame, this function will schedule
 *                it and hence it will restart the tranmission logic.
 */
void
ath_resched_txq(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int qdepth = 0;
    int i;
    for (i = 0; i < HAL_NUM_TX_QUEUES - 1; i++) {
        if(ATH_TXQ_SETUP(sc,i)) {
            struct ath_txq *txq = &sc->sc_txq[i];
            if ((txq != sc->sc_cabq) && (txq != sc->sc_uapsdq)) {
                qdepth = ath_txq_depth(sc, txq->axq_qnum);
                if(qdepth) {
                    ATH_TXQ_LOCK(txq);
                    ath_txq_schedule(sc, txq);
                    ATH_TXQ_UNLOCK(txq);
                    break;
                } else {
                    continue;
                }
            }
        }
    }
}
#endif

int
ath_setTxPwrAdjust(ath_dev_t dev, int32_t adjust, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    if (!adjust)
        return 0;

    ATH_TXPOWER_LOCK(sc);
    if (is2GHz) {
        sc->sc_config.txpowupdown2G += adjust;
    } else {
        sc->sc_config.txpowupdown5G += adjust;
    }

    sc->sc_config.txpow_need_update = 1;
    ATH_TXPOWER_UNLOCK(sc);

    ath_update_txpow(sc, sc->tx_power);        /* update tx power state */


    return error;
}

int
ath_setTxPwrLimit(ath_dev_t dev, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;
    int update_txpwr = 1;
    int i;
    struct ath_vap *avp;
    int active_vap = 0;

    if((sc->sc_curchan.channel == 0) ||
        (sc->sc_curchan.channel_flags == 0)) {
        /* no channel set */
        update_txpwr = 0;
    }
    /* check any active vaps ?? */
    for(i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if(avp) {
            if(avp->av_up) {
                active_vap = 1;
                break;
            }
        }
    }
    if(!active_vap) {
        /* no active vap */
        update_txpwr = 0;
    }

    if(update_txpwr) {
        if (is2GHz) {
            limit = min(limit,(u_int32_t)ATH_TXPOWER_MAX_2G);
            sc->sc_config.txpowlimit2G = (u_int16_t)limit;
        } else {
            limit =min(limit,(u_int32_t)ATH_TXPOWER_MAX_5G);
            sc->sc_config.txpowlimit5G = (u_int16_t)limit;
        }
    } else {
        /* reflect to both 2g and 5g txpowlimit */
        sc->sc_config.txpowlimit2G = (u_int16_t)limit;
        sc->sc_config.txpowlimit5G = (u_int16_t)limit;
    }

    if (error == 0) {
        sc->tx_power = tpcInDb;
		if(update_txpwr)
        	ath_update_txpow(sc, tpcInDb);
    }
    return error;
}

u_int8_t
ath_get_common_power(u_int16_t freq)
{
    return getCommonPower(freq);
}

#ifdef ATH_CHAINMASK_SELECT

static int
ath_chainmask_sel_timertimeout(void *timerArg)
{
    struct ath_chainmask_sel *cm = (struct ath_chainmask_sel *)timerArg;
    cm->switch_allowed = 1;
    return 1; /* no re-arm  */
}

/*
 * Start chainmask select timer
 */
static void
ath_chainmask_sel_timerstart(struct ath_chainmask_sel *cm)
{
    cm->switch_allowed = 0;
    ath_start_timer(&cm->timer);
}

/*
 * Stop chainmask select timer
 */
static void
ath_chainmask_sel_timerstop(struct ath_chainmask_sel *cm)
{
    cm->switch_allowed = 0;
    ath_cancel_timer(&cm->timer, CANCEL_NO_SLEEP);
}

int
ath_chainmask_sel_logic(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_chainmask_sel *cm  = &an->an_chainmask_sel;

    /*
     * Disable auto-swtiching in one of the following if conditions.
     * sc_chainmask_auto_sel is used for internal global auto-switching
     * enabled/disabled setting
     */
    if ((sc->sc_no_tx_3_chains == AH_FALSE) ||
        (sc->sc_config.chainmask_sel == AH_FALSE)) {
        cm->cur_tx_mask = sc->sc_config.txchainmask;
        return cm->cur_tx_mask;
    }

    if (cm->tx_avgrssi == ATH_RSSI_DUMMY_MARKER) {
        return cm->cur_tx_mask;
    }

    if (cm->switch_allowed) {
        /* Switch down from tx 3 to tx 2. */
        if (cm->cur_tx_mask == ATH_CHAINMASK_SEL_3X3  &&
            ATH_RSSI_OUT(cm->tx_avgrssi) >= ath_chainmask_sel_down_rssi_thres) {
            cm->cur_tx_mask = sc->sc_config.txchainmask;

            /* Don't let another switch happen until this timer expires */
            ath_chainmask_sel_timerstart(cm);
        }
        /* Switch up from tx 2 to 3. */
        else if (cm->cur_tx_mask == sc->sc_config.txchainmask  &&
                 ATH_RSSI_OUT(cm->tx_avgrssi) <= ath_chainmask_sel_up_rssi_thres) {
            cm->cur_tx_mask =  ATH_CHAINMASK_SEL_3X3;

            /* Don't let another switch happen until this timer expires */
            ath_chainmask_sel_timerstart(cm);
        }
    }

    return cm->cur_tx_mask;
}

static void
ath_chainmask_sel_init(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_chainmask_sel *cm  = &an->an_chainmask_sel;

    OS_MEMZERO(cm, sizeof(struct ath_chainmask_sel));

    cm->cur_tx_mask = sc->sc_config.txchainmask;
    cm->cur_rx_mask = sc->sc_config.rxchainmask;
    cm->tx_avgrssi = ATH_RSSI_DUMMY_MARKER;

    ath_initialize_timer(sc->sc_osdev, &cm->timer, ath_chainmask_sel_period,
                         ath_chainmask_sel_timertimeout, cm);
}

#endif

static int16_t
ath_get_noisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags, int wait_time)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL hchan;

    hchan.channel = freq;
    hchan.channel_flags = chan_flags;

    if (wait_time > 0 && ah->ah_get_nf_from_reg) {
        //If wait_time > 0, we will try to get the current NF value saved in register.
        //If the current NF cal is not complete, we will continue and return the value chan->raw_noise_floor
        int16_t nf_reg=0;
        if ((nf_reg = ah->ah_get_nf_from_reg(ah, &hchan, wait_time)) != 0) {
            return nf_reg;
        }
    }

    return ah->ah_get_chan_noise(ah, &hchan);
}

static void
ath_get_chainnoisefloor(ath_dev_t dev, u_int16_t freq, u_int chan_flags, int16_t *nfBuf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL hchan;

    hchan.channel = freq;
    hchan.channel_flags = chan_flags;

    /* If valid buffer is passed, populate individual chain nf values */
    if (nfBuf) {
        /* see if the specified channel is the home channel */

        ah->ah_get_chain_noise_floor(ah, nfBuf, &hchan, sc->sc_scanning);
    }
}

static int ath_get_rx_nf_offset(ath_dev_t dev, u_int16_t freq, u_int chan_flags, int8_t *nf_pwr, int8_t *nf_cal)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL hchan;

    hchan.channel = freq;
    hchan.channel_flags = chan_flags;

    if (ah->ah_get_rx_nf_offset) {
        return ah->ah_get_rx_nf_offset(ah, &hchan, nf_pwr, nf_cal);
    }

    return -1;
}

static int ath_get_rx_signal_dbm(ath_dev_t dev, int8_t *signal_dbm)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_CHANNEL hchan;
    HAL_CHANNEL *ext_ch;
    int8_t nf_pwr[6], nf_cal[6], rssi[6];
    int16_t nf_run[6];
    int i;

    /*Getting nf_pwr, nf_cal(from EEPROM) for both control and extension channel,
     * Runtime NF values only for control channel*/
    hchan.channel = sc->sc_curchan.channel;
    hchan.channel_flags = sc->sc_curchan.channel_flags;
    ath_get_rx_nf_offset(dev, hchan.channel, hchan.channel_flags, nf_pwr, nf_cal);
    ath_get_chainnoisefloor(dev, hchan.channel, hchan.channel_flags, nf_run);

    rssi[0] = sc->sc_stats.ast_rx_rssi_ctl0;
    rssi[1] = sc->sc_stats.ast_rx_rssi_ctl1;
    rssi[2] = sc->sc_stats.ast_rx_rssi_ctl2;
    rssi[3] = sc->sc_stats.ast_rx_rssi_ext0;
    rssi[4] = sc->sc_stats.ast_rx_rssi_ext1;
    rssi[5] = sc->sc_stats.ast_rx_rssi_ext2;

    for (i=0; i<3; i++) {
   	if (rssi[i] != ATH_RSSI_BAD) {
    		signal_dbm[i] = rssi[i] + nf_run[i] + nf_pwr[i] - nf_cal[i];
	} else {
		signal_dbm[i] = ATH_RSSI_BAD;
	}
    }

    ext_ch = ath_hal_get_extension_channel(sc->sc_ah);

    if (ext_ch) {
 	/*Getting runtime NF vlaues for extension channel*/
	hchan.channel = ext_ch->channel;
	hchan.channel_flags = ext_ch->channel_flags;

	ath_get_rx_nf_offset(dev, hchan.channel, hchan.channel_flags, nf_pwr, nf_cal);
	for (i=3; i<6; i++) {
		if (rssi[i] != ATH_RSSI_BAD) {
			signal_dbm[i] = rssi[i] + nf_run[i] + nf_pwr[i-3] - nf_cal[i-3];
		} else {
			signal_dbm[i] = ATH_RSSI_BAD;
		}
	}
    }

    return 1;
}
#if ATH_SUPPORT_VOW_DCS
static void
ath_disable_dcsim(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_dcs_enabled &= ~ATH_CAP_DCS_WLANIM;
}
static void
ath_enable_dcsim(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    /* Enable wlanim for DCS */
    sc->sc_dcs_enabled |= ATH_CAP_DCS_WLANIM;
}
#endif

#if ATH_SUPPORT_RAW_ADC_CAPTURE
static int
ath_get_min_agc_gain(ath_dev_t dev, u_int16_t freq, int32_t *gain_buf, int num_gain_buf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_RAW_ADC_CAPTURE, 0, NULL) != HAL_OK) {
        return -ENOTSUP;
    }

    if (ath_hal_get_min_agc_gain(ah, freq, gain_buf, num_gain_buf) != HAL_OK) {
        return -ENOTSUP;
    }
    return 0;
}
#endif


#if ATH_SUPPORT_IBSS_DFS
static void ath_ibss_beacon_update_start(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);

    sc->sc_hasveol = 0;
    sc->sc_imask |= HAL_INT_SWBA;
}

static void ath_ibss_beacon_update_stop(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_hasveol = 1;
    sc->sc_imask &= ~HAL_INT_SWBA;

    /* If the channel is not DFS */
    if ((sc->sc_curchan.priv_flags & CHANNEL_DFS) == 0) {
        ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);
    }
}
#endif /* ATH_SUPPORT_IBSS_DFS */

u_int32_t
ath_gettsf32(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t retval;

    ATH_PS_WAKEUP(sc);
    retval=ath_hal_gettsf32(ah);
    ATH_PS_SLEEP(sc);

    return retval;
}

u_int64_t
ath_gettsf64(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int64_t retval;

    ATH_PS_WAKEUP(sc);
    retval=ath_hal_gettsf64(ah);
    ATH_PS_SLEEP(sc);

    return retval;
}

#if ATH_SUPPORT_WIFIPOS
u_int64_t
ath_gettsftstamp(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_tsf_tstamp;
}
#endif

void
ath_setrxfilter(ath_dev_t dev, u_int32_t filter)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    u_int32_t           rxfilt;

    if(filter) {
        ath_hal_setrxfilter(sc->sc_ah, filter);
    } else {
        rxfilt = ath_calcrxfilter(sc);
        ath_hal_setrxfilter(sc->sc_ah, rxfilt);
    }
}

int
ath_get_mib_cyclecounts(ath_dev_t dev, HAL_COUNTERS *pCnts)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pCnts == NULL) {
        return -EINVAL;
    }

    ath_hal_getMibCycleCounts(ah, pCnts);
    return 0;
}

int
ath_update_mib_macstats(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);
    ath_hal_updateMibMacStats(ah);
    ATH_PS_SLEEP(sc);

    return 0;
}

int
ath_get_mib_macstats(ath_dev_t dev, struct ath_mib_mac_stats *pStats)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pStats == NULL) {
        return -EINVAL;
    }

    ath_hal_getMibMacStats(ah, (HAL_MIB_STATS*)pStats);

    return 0;
}


void
ath_clear_mib_counters(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_clearMibCounters(ah);
}

#ifdef ATH_CCX
int
ath_getserialnumber(ath_dev_t dev, u_int8_t *pSerNum, int limit)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pSerNum == NULL) {
        return -EINVAL;
    }

    return ath_hal_get_sernum(ah, pSerNum, limit);
}

int
ath_getchandata(ath_dev_t dev, struct ieee80211_ath_channel *pChan, struct ath_chan_data *pData)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    if (ath_hal_get_chandata(ah, (HAL_CHANNEL*)pChan, (HAL_CHANNEL_DATA*)pData) == AH_TRUE){
        return 0;
    } else {
        return -EINVAL;
    }
}

u_int32_t
ath_getcurrssi(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    return ath_hal_getCurRSSI(ah);
}
#endif /* ATH_CCX */

#if ATH_GEN_RANDOMNESS
u_int32_t
ath_get_rssi_chain0(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    return ath_hal_get_rssi_chain0(ah);
}
#endif

void
ath_do_pwrworkaround(ath_dev_t dev, u_int16_t channel,  u_int32_t channel_flags, u_int16_t opmodeSta )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL dummychan;     /* used for ath hal reset */

    /*
     * re program the PCI powersave regs.
     */
    ath_hal_configpcipowersave(ah, 0, 0);

    /*
     * Power consumption WAR to keep the power consumption
     * minimum when loaded first time and we are in OFF state.
     * ath hal reset requires a valid channel in order to reset.
     * Do this if in STA mode.
     */
    if (opmodeSta) {
        ATH_PWRSAVE_STATE power;
        HAL_STATUS        status;

        // ATH_LOCK(sc->sc_osdev);

        power = ath_pwrsave_get_state(dev);
        ath_pwrsave_awake(sc);
        dummychan.channel = channel;
        dummychan.channel_flags = channel_flags;
#ifdef QCA_SUPPORT_CP_STATS
        pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_halresets++;
#endif
        if (!ath_hal_reset(ah, sc->sc_opmode, &dummychan,
                           sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee),
                           sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                           sc->sc_ht_extprotspacing, AH_FALSE, &status,
                           sc->sc_scanning))
        {
            printk("%s: unable to reset hardware; hal status %u\n", __func__, status);
        }
        ATH_PS_LOCK(sc);
        ath_pwrsave_set_state(sc, power);
        ATH_PS_UNLOCK(sc);
        // ATH_UNLOCK(sc->sc_osdev);
    }

}

void
ath_get_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *retval)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    ath_hal_gettxqueueprops(ah, q_id, &qi);

    switch (property) {
    case TXQ_PROP_SHORT_RETRY_LIMIT:
        *(u_int16_t *)retval = qi.tqi_shretry;
        break;
    case TXQ_PROP_LONG_RETRY_LIMIT:
        *(u_int16_t *)retval = qi.tqi_lgretry;
        break;
    }
}


void
ath_set_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    qi.tqi_shretry = HAL_TQI_NONVAL;
    qi.tqi_lgretry = HAL_TQI_NONVAL;
    qi.tqi_cbr_period = HAL_TQI_NONVAL;
    qi.tqi_cbr_overflow_limit = HAL_TQI_NONVAL;
    qi.tqi_burst_time = HAL_TQI_NONVAL;
    qi.tqi_ready_time = HAL_TQI_NONVAL;

    switch (property) {
    case TXQ_PROP_SHORT_RETRY_LIMIT:
        qi.tqi_shretry = *(u_int16_t *)value;
        break;
    case TXQ_PROP_LONG_RETRY_LIMIT:
        qi.tqi_lgretry = *(u_int16_t *)value;
        break;
    }
    ath_hal_settxqueueprops(ah, q_id, &qi);
}

void
ath_update_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    ath_hal_gettxqueueprops(ah, q_id, &qi);

    switch (property) {
        case TXQ_PROP_SHORT_RETRY_LIMIT:
            qi.tqi_shretry = *(u_int16_t *)value;
            break;

        case TXQ_PROP_LONG_RETRY_LIMIT:
            qi.tqi_lgretry = *(u_int16_t *)value;
            break;
        default:
            break;
    }

    ath_hal_settxqueueprops(ah, q_id, &qi);
    ath_hal_resettxqueue(ah, q_id); /* push to h/w */
}

void
ath_get_hwrevs(ath_dev_t dev, struct ATH_HW_REVISION *hwrev)
{
    //ARTODO by MR: finish this function
}

u_int32_t
ath_rc_rate_maprix(ath_dev_t dev, u_int16_t curmode, int isratecode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_rate_maprix(sc, curmode, isratecode);
}

int
ath_rc_rate_set_mcast_rate(ath_dev_t dev, u_int32_t req_rate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_rate_set_mcast_rate(sc, req_rate);
}


void ath_set_diversity(ath_dev_t dev, u_int diversity)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_setdiversity(sc->sc_ah, diversity);
    sc->sc_diversity = diversity;

}
void ath_set_rx_chainmask(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.rxchainmask = sc->sc_rx_chainmask = mask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];

    printk("%s[%d] sc->sc_config.rxchainmask %d mask %d\n", __func__, __LINE__,
        sc->sc_config.rxchainmask, mask);

}
void ath_set_tx_chainmask(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.txchainmask = sc->sc_tx_chainmask = mask;
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    printk("%s[%d] sc->sc_config.txchainmask %d mask %d\n", __func__, __LINE__,
        sc->sc_config.txchainmask, mask);
}

void ath_set_rx_chainmasklegacy(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.rxchainmasklegacy = sc->sc_rx_chainmask = mask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
}
void ath_set_tx_chainmasklegacy(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.txchainmasklegacy = sc->sc_tx_chainmask = mask;
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];
}

void ath_set_tx_chainmaskopt(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    int i;

    /* optional txchainmask should be subset of primary txchainmask */
    if ((mask & sc->sc_tx_chainmask) != mask)
        return;

    sc->sc_tx_chainmaskopt = mask;

    /* update transmit rate table */
    for (i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if (avp) {
            ath_rate_newstate(sc, avp, 1);
        }
    }

    /* update tx power table */
    ath_hal_set_txchainmaskopt(sc->sc_ah, mask);

    /* internal reset for power table re-calculation */
    sc->sc_reset_type = ATH_RESET_NOLOSS;
    ath_internal_reset(sc);
    sc->sc_reset_type = ATH_RESET_DEFAULT;

    printk("Primary txchainmask 0x%02x, optional txchainmask 0x%02x\n",
            sc->sc_tx_chainmask, sc->sc_tx_chainmaskopt);
}

void ath_get_maxtxpower(ath_dev_t dev, u_int32_t *txpow)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_getmaxtxpow(sc->sc_ah, txpow);
}

void ath_read_from_eeprom(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_getdiagstate(sc->sc_ah, 17 /*HAL_DIAG_EEREAD*/, (void *) &address, len, (void **)value, bytesread);
}

void ath_pkt_log_text (ath_dev_t dev,  u_int32_t iflags, const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifndef REMOVE_PKT_LOG
    ath_log_text(dev, buf, iflags);
#endif
}

void ath_fixed_log_text (ath_dev_t dev, const char *text)
{
#ifndef REMOVE_PKT_LOG
    ath_log_text(dev, text, 0);
#endif
}

int ath_rate_newstate_set(ath_dev_t dev, int index, int up)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_rate_newstate(sc, sc->sc_vaps[index], up);
    return 0;
}

/* Get the correct transmit rate code from rate (iv_mgt_rate)*/

u_int8_t ath_rate_findrc(ath_dev_t dev, u_int32_t rate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];
    u_int8_t i, rc = 0;

    for(i=0; i<rt->rateCount; i++) {
        if (rt->info[i].rateKbps == rate) {
            rc = rt->info[i].rate_code;
            break;
        }
    }
    return rc;
}

static void
ath_printreg(ath_dev_t dev, u_int32_t printctrl)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

    ath_hal_getdiagstate(ah, HAL_DIAG_PRINT_REG, &printctrl, sizeof(printctrl), NULL, 0);

    ATH_PS_SLEEP(sc);
}

u_int32_t
ath_getmfpsupport(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t        mfpsupport;
    ath_hal_getmfpsupport(ah, &mfpsupport);
    switch(mfpsupport) {
    case HAL_MFP_QOSDATA:
        return ATH_MFP_QOSDATA;
    case HAL_MFP_PASSTHRU:
        return ATH_MFP_PASSTHRU;
    case HAL_MFP_HW_CRYPTO:
        return ATH_MFP_HW_CRYPTO;
    default:
        return ATH_MFP_QOSDATA;
    };
}

void
ath_setmfpQos(ath_dev_t dev, u_int32_t dot11w)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_set_mfp_qos(ah, dot11w);
}

static u_int64_t ath_get_tx_hw_retries(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return sc->sc_stats.ast_tx_hw_retries;
}

static u_int64_t ath_get_tx_hw_success(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return sc->sc_stats.ast_tx_hw_success;
}

#if ATH_SUPPORT_TIDSTUCK_WAR
/*
 * Send DELBA for all Rx tids which are stuck
 */
static void
ath_clear_rxtid(ath_dev_t dev, ath_node_t node)
{
#define    N(a)    (sizeof (a) / sizeof (a[0]))
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int i;
    struct ath_arx_tid *rxtid;
    systime_t curtime;
    u_int32_t timediff = 0;


    for (i = 0; i < N(an->an_rx_tid); i++) {
        rxtid = &an->an_rx_tid[i];
        if (rxtid->addba_exchangecomplete) {
            curtime = OS_GET_TIMESTAMP();
            timediff = CONVERT_SYSTEM_TIME_TO_SEC(curtime - rxtid->rxtid_lastdata);
            /*EV 111562: 8 secs timeout used because in Veriwave Throughput test,between each tests,
			 *there would be around 5 secs with no traffic. This time gap will teardown aggregation.
			 *For next traffic, aggregation has to be formed again,also packet loss will occur.
			 *To avoid this 8 sec timeout used here
			 */
			if ( timediff >= RXTID_SEND_DELBA) {
                ath_rx_aggr_teardown(sc, an, i);
                sc->sc_ieee_ops->rxtid_delba(an->an_node, i);
            }
        }
    }
#undef N
}
#endif

#if ATH_SUPPORT_KEYPLUMB_WAR
/*
 * Save a copy of HAL key in an_halkey to later check if it is corrupted
 */
static void
ath_key_save_halkey(ath_node_t node, HAL_KEYVAL *hk, u_int16_t keyix, const u_int8_t *macaddr)
{
    struct ath_node *an = ATH_NODE(node);

    an->an_keyix = keyix;
    OS_MEMCPY(&(an->an_halkey), hk, sizeof(HAL_KEYVAL));
    OS_MEMCPY(an->an_macaddr, macaddr, IEEE80211_ADDR_LEN);
}

/*
 * Check if SW key matches with Keycache entry
 */
int
ath_checkandplumb_key(ath_dev_t dev, ath_node_t node, u_int16_t keyix)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    HAL_KEYVAL *hk = &(an->an_halkey);
    bool status;

    ATH_FUNC_ENTRY_CHECK(sc, 0);
    ATH_PS_WAKEUP(sc);

    status = ath_hal_checkkey(sc->sc_ah, keyix, hk);

    ATH_PS_SLEEP(sc);
    if (!status) {
        DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: Hardware Key\n",
                __func__);
        ath_keyset(dev, keyix, &(an->an_halkey), an->an_macaddr);
    }

    return status;
}
OS_EXPORT_SYMBOL(ath_checkandplumb_key);
#endif

#if ATH_PCIE_ERROR_MONITOR
int
ath_start_pcie_error_monitor(ath_dev_t dev,int b_auto_stop)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;

    return ath_hal_start_pcie_error_monitor(ah,b_auto_stop);
}

int
ath_read_pcie_error_monitor(ath_dev_t dev, void* pReadCounters)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;

    return ath_hal_read_pcie_error_monitor(ah, pReadCounters);
}

int
ath_stop_pcie_error_monitor(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;

    return ath_hal_stop_pcie_error_monitor(ah);
}

#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE

/*
 * Function     : ath_spectral_get_tsf64
 * Description  : Get TSF64 Value
 * Input        : Pointer to Spectral
 * Ouput        : TSF64
 *
 */

static u_int64_t ath_spectral_get_tsf64(void* arg)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    return ath_hal_gettsf64(ah);
}

/*
 * Function     : spectral_to_halcap
 * Description  : convert from enum spectral_capability_type to HAL_CAPABILITY_TYPE
 * Input        : enum spectral_capability_type
 * Ouput        : HAL_CAPABILITY_TYPE
 *
 */
static HAL_CAPABILITY_TYPE spectral_to_halcap(enum spectral_capability_type type)
{
    HAL_CAPABILITY_TYPE hal_cap;

    switch(type)
    {
        case SPECTRAL_CAP_PHYDIAG:
            hal_cap = HAL_CAP_PHYDIAG;
            break;
        case SPECTRAL_CAP_RADAR:
            hal_cap = HAL_CAP_RADAR;
            break;
        case SPECTRAL_CAP_SPECTRAL_SCAN:
            hal_cap = HAL_CAP_SPECTRAL_SCAN;
            break;
        case SPECTRAL_CAP_ADVNCD_SPECTRAL_SCAN:
            hal_cap = HAL_CAP_ADVNCD_SPECTRAL_SCAN;
            break;
    }
    return hal_cap;
}

/*
 * Function     : ath_spectral_get_capability
 * Description  : Get the Hardware capability
 * Input        : Pointer to Spectral
 * Ouput        : Supported/Not Supported
 *
 */
static u_int32_t ath_spectral_get_capability(void* arg, enum spectral_capability_type type)
{
    u_int32_t result;
    u_int32_t ret;
    HAL_CAPABILITY_TYPE hal_cap;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;

    hal_cap = spectral_to_halcap(type);
    ret = ath_hal_getcapability(ah, hal_cap, 0, &result);
    ret = (ret != HAL_OK)?AH_FALSE:AH_TRUE;
    return ret;
}


/*
 * Function     : ath_set_rxfilter
 * Description  : Configure the RX Filter
 * Input        : Pointer to Spectral and RX Filter
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_set_rxfilter(void* arg, int rxfilter)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    ath_hal_setrxfilter(ah, rxfilter);
    return AH_TRUE;
}


/*
 * Function     : ath_spectral_get_rxfilter
 * Description  : Get the current RX Filter configuration
 * Input        : Pointer to Spectral
 * Ouput        : Current RX Configuration
 *
 */
static u_int32_t ath_spectral_get_rxfilter(void* arg)
{
    int rxfilter;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    rxfilter = ath_hal_getrxfilter(ah);
    return rxfilter;
}

/*
 * Function     : ath_spectral_is_spectral_active
 * Description  : Check if SPECTRAL is active
 * Input        : Pointer to spectral
 * Ouput        : Active/Inactive
 *
 */
static u_int32_t ath_spectral_is_spectral_active(void* arg)
{
    u_int32_t is_active = AH_FALSE;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    is_active = ath_hal_is_spectral_active(ah);
    return is_active;
}

/*
 * Function     : ath_spectral_is_spectral_enabled
 * Description  : Check if SPECTRAL is enabled
 * Input        : Pointer to spectral
 * Ouput        : Enabled/Disabled
 *
 */
static u_int32_t ath_spectral_is_spectral_enabled(void* arg)
{
    u_int32_t is_enabled = AH_FALSE;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    is_enabled = ath_hal_is_spectral_enabled(ah);
    return is_enabled;
}

/*
 * Function     : ath_spectral_start_spectral_scan
 * Description  : Starts Spectral scan
 * Input        : Pointer to Spectral
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_start_spectral_scan(void* arg)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);
    ath_hal_start_spectral_scan(ah);
    ATH_PS_SLEEP(sc);

    return AH_TRUE;
}

/*
 * Function     : ath_spectral_stop_spectral_scan
 * Description  : Stops Spectral scan
 * Input        : Pointer to Spectral
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_stop_spectral_scan(void* arg)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);
    ath_hal_stop_spectral_scan(ah);
    ATH_PS_SLEEP(sc);

    return AH_TRUE;
}

/*
 * Function     : ath_spectral_get_extension_channel
 * Description  : Get current Extension channeL
 * Input        : Pointer to Spectral
 * Ouput        : Extension channel
 *
 */
static u_int32_t ath_spectral_get_extension_channel(void* arg)
{
    int channel = 0;
    HAL_CHANNEL* ext_ch;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    ext_ch = ath_hal_get_extension_channel(ah);
    if (ext_ch) {
        channel = ext_ch->channel;
    }
    return channel;
}

/*
 * Function     : ath_spectral_reset_hw
 * Description  : Reset the hardware
 * Input        : Pointer to Spectral
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_reset_hw(void* arg)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    sc->sc_full_reset = 1;
    sc->sc_ath_ops.reset_start(sc, 0, 0, 0);
    sc->sc_ath_ops.reset(sc);
    sc->sc_ath_ops.reset_end(sc, 0);
    return AH_TRUE;
}

/*
 * Function     : ath_spectral_get_chain_noise_floor
 * Description  : Get chain noise floor (History buffer)
 * Input        : Pointer to Spectral and Pointer to buffer
 * Ouput        : Noisefloor values
 *
 */
static u_int32_t ath_spectral_get_chain_noise_floor(void* arg, int16_t* nfBuf)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal *ah = sc->sc_ah;

    if (nfBuf) {
        ah->ah_get_chain_noise_floor(ah, nfBuf, &sc->sc_curchan, 0);
    }
    return AH_TRUE;
}

/*
 * Function     : ath_spectral_get_current_channel
 * Description  : Get current operating channel
 * Input        : Pointer to Spectral
 * Ouput        : Channel
 *
 */
static u_int32_t ath_spectral_get_current_channel(void* arg)
{
    HAL_CHANNEL* hal_channel = NULL;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    hal_channel = &sc->sc_curchan;
    return hal_channel->channel;
}

/*
 * Function     : ath_spectral_get_ctl_noisefloor
 * Description  : Get Contrl channel noisefloor
 * Input        : Pointer to Spectral
 * Ouput        : Noise floor
 *
 */
static int8_t ath_spectral_get_ctl_noisefloor(void* arg)
{
    int8_t ctl_noisefloor = 0;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);
    ctl_noisefloor = ath_hal_get_ctl_nf(ah);
    ATH_PS_SLEEP(sc);

    return ctl_noisefloor;
}

/*
 * Function     : ath_spectral_get_ext_noisefloor
 * Description  : Get extension channel noisefloor
 * Input        : Pointer to Spectral
 * Ouput        : Noise floor
 *
 */
static int8_t ath_spectral_get_ext_noisefloor(void* arg)
{
    int8_t ext_noisefloor = 0;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    ATH_PS_WAKEUP(sc);
    ext_noisefloor = ath_hal_get_ext_nf(ah);
    ATH_PS_SLEEP(sc);
    return ext_noisefloor;
}

/*
 * Function     : convert_to_hal_freq
 * Description  : convert from type band_info to HAL_FREQ_BAND
 * Input        : enum band_info
 * Ouput        : HAL_FREQ_BAND
 *
 */
HAL_FREQ_BAND convert_to_hal_freq(enum band_info band)
{
    HAL_FREQ_BAND hal_freq = 0;

    switch(band)
    {
        case BAND_2G:
            hal_freq = HAL_FREQ_BAND_2GHZ;
            break;
        case BAND_5G:
            hal_freq = HAL_FREQ_BAND_5GHZ;
            break;
        default:
            break;
    }
    return hal_freq;
}

/*
 * Function     : ath_spectral_get_nominal_noisefloor
 * Description  : Get Nominal noise floor
 * Input        : Pointer to Spectral
 * Ouput        : Nominal Noise floor
 *
 */
static int16_t ath_spectral_get_nominal_noisefloor(void* arg, int band)
{
    int16_t nominal_nf = 0;
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    HAL_FREQ_BAND hal_freq;

    hal_freq = convert_to_hal_freq(band);
    nominal_nf = ath_hal_get_nominal_nf(ah, hal_freq);
    return nominal_nf;
}

/*
 * Function     : ath_spectral_configure_params
 * Description  : Configure the Spectral params
 * Input        : Pointer to Spectral and HAL Param pointer
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_configure_params(void* arg, HAL_SPECTRAL_PARAM* params)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    ATH_PS_WAKEUP(sc);
    ath_hal_configure_spectral(ah, params);
    ATH_PS_SLEEP(sc);
    return AH_TRUE;
}

/*
 * Function     : ath_spectral_get_params
 * Description  : Get current Spectral configuration
 * Input        : Pointer to Spectral and HAL params
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_get_params(void* arg, HAL_SPECTRAL_PARAM* params)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    struct ath_hal* ah = sc->sc_ah;
    ATH_PS_WAKEUP(sc);
    ath_hal_get_spectral_config(ah, params);
    ATH_PS_SLEEP(sc);
    return AH_TRUE;
}

/*
 * Function     : ath_spectral_get_ent_mask
 * Description  :
 * Input        : Pointer to Spectral
 * Ouput        : Mask
 *
 */
static u_int32_t ath_spectral_get_ent_mask(void *arg)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    return sc->sc_ent_spectral_mask;
}

/*
 * Function     : ath_spectral_get_mac_address
 * Description  : Get the MAC Address
 * Input        : Pointer to Spectral
 * Ouput        : Success
 *
 */
static u_int32_t ath_spectral_get_mac_address(void* arg, char* addr)
{
    struct ath_softc* sc = GET_SPECTRAL_ATHSOFTC(arg);
    ATH_ADDR_COPY(addr, sc->sc_myaddr);
    return 0;
}

/*
 * Function     : ath_spectral_indicate
 * Description  :
 * Input        :
 * Ouput        :
 *
 */
void
ath_spectral_indicate(struct ath_softc *sc, void* spectral_buf, u_int32_t buf_size)
{
    if (sc->sc_ieee_ops->spectral_indicate)
        sc->sc_ieee_ops->spectral_indicate(sc->sc_ieee, spectral_buf, buf_size);
}



/*
 * Function     : ath_dev_start_spectral_scan
 * Description  : Starts spectral scan for EACS, so
 *                the spectral paramaters have to be set for default
 * Input        : Pointer to Dev
 * Ouput        : Void
 *
 * TODO: This should be replaced with Spectral ops
 *
 */
void ath_dev_start_spectral_scan(ath_dev_t dev, u_int8_t priority)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_spectral *spectral = (struct ath_spectral*)sc->sc_spectral;

    ATH_PS_WAKEUP(sc);
    SPECTRAL_LOCK(spectral);
    /* Set the spectral values to default */
    spectral_scan_enable(spectral, priority);
    start_spectral_scan_da(spectral->pdev_obj);
    SPECTRAL_UNLOCK(spectral);
}

/*
 * Function     : ath_dev_stop_spectral_scan
 * Description  : Stop spectral scan
 * Input        : Pointer to dev
 * Ouput        : Void
 *
 * TODO: This should be replaced with Spectral ops
 */
void ath_dev_stop_spectral_scan(ath_dev_t dev)
{
    struct ath_softc *sc    = ATH_DEV_TO_SC(dev);
    struct ath_spectral *spectral = (struct ath_spectral*)sc->sc_spectral;
    ATH_PWRSAVE_STATE state = ath_pwrsave_get_state(dev);

    if (state != ATH_PWRSAVE_AWAKE) {
        printk("%s: Warning - Not in Awake state. Explicitly waking up\n",
                __func__);
        ATH_PS_WAKEUP(sc);
    }

    SPECTRAL_LOCK(spectral);
    stop_current_scan_da(spectral->pdev_obj);
    SPECTRAL_UNLOCK(spectral);
    ATH_PS_SLEEP(sc);
}

void ath_dev_record_chan_info(ath_dev_t dev,
                              u_int16_t chan_num,
                              bool are_chancnts_valid,
                              u_int32_t scanend_clr_cnt,
                              u_int32_t scanstart_clr_cnt,
                              u_int32_t scanend_cycle_cnt,
                              u_int32_t scanstart_cycle_cnt,
                              bool is_nf_valid,
                              int16_t nf,
                              bool is_per_valid,
                              u_int32_t per)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_spectral *spectral = sc->sc_spectral;

    if(spectral==NULL)
      return;
    ATH_PS_WAKEUP(sc);
    SPECTRAL_LOCK(spectral);
    spectral_record_chan_info(spectral,
                              chan_num,
                              are_chancnts_valid,
                              scanend_clr_cnt,
                              scanstart_clr_cnt,
                              scanend_cycle_cnt,
                              scanstart_cycle_cnt,
                              is_nf_valid,
                              nf,
                              is_per_valid,
                              per);
    SPECTRAL_UNLOCK(spectral);

    ATH_PS_SLEEP(sc);
}

/*
 * Function     : ath_init_spectral_ops
 * Description  : Initialize Spectral ops, this will be given to Spectral module
 * Input        : Pointer to Spectral ops table
 * Ouput        : Success
 *
 */
u_int32_t ath_init_spectral_ops(struct ath_spectral_ops* p_sops)
{
    p_sops->get_tsf64               = ath_spectral_get_tsf64;
    p_sops->get_capability          = ath_spectral_get_capability;
    p_sops->set_rxfilter            = ath_spectral_set_rxfilter;
    p_sops->get_rxfilter            = ath_spectral_get_rxfilter;
    p_sops->is_spectral_enabled     = ath_spectral_is_spectral_enabled;
    p_sops->is_spectral_active      = ath_spectral_is_spectral_active;
    p_sops->start_spectral_scan     = ath_spectral_start_spectral_scan;
    p_sops->stop_spectral_scan      = ath_spectral_stop_spectral_scan;
    p_sops->get_extension_channel   = ath_spectral_get_extension_channel;
    p_sops->get_ctl_noisefloor      = ath_spectral_get_ctl_noisefloor;
    p_sops->get_ext_noisefloor      = ath_spectral_get_ext_noisefloor;
    p_sops->configure_spectral      = ath_spectral_configure_params;
    p_sops->get_spectral_config     = ath_spectral_get_params;
    p_sops->get_ent_spectral_mask   = ath_spectral_get_ent_mask;
    p_sops->get_mac_address         = ath_spectral_get_mac_address;
    p_sops->get_current_channel     = ath_spectral_get_current_channel;
    p_sops->reset_hw                = ath_spectral_reset_hw;
    p_sops->get_chain_noise_floor   = ath_spectral_get_chain_noise_floor;

    return AH_TRUE;
}
#endif /* WLAN_SPECTRAL_ENABLE */

#ifdef WLAN_SUPPORT_GREEN_AP
static QDF_STATUS ath_green_ap_ps_on_off(ath_dev_t dev, u_int32_t val,
				u_int8_t mac_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_setGreenApPsOnOff(sc->sc_ah, val);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ath_green_ap_get_capab(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if(ath_hal_singleAntPwrSavePossible(sc->sc_ah)) {
        return QDF_STATUS_SUCCESS;
    }

    return QDF_STATUS_E_NOSUPPORT;
}

static u_int16_t ath_green_ap_get_current_channel(ath_dev_t dev)
{
    HAL_CHANNEL* hal_channel = NULL;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    hal_channel = &sc->sc_curchan;

    return hal_channel->channel;
}

static u_int64_t ath_green_ap_get_current_channel_flags(ath_dev_t dev)
{
    HAL_CHANNEL* hal_channel = NULL;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    hal_channel = &sc->sc_curchan;
    return (u_int64_t)hal_channel->channel_flags;
}

static QDF_STATUS ath_green_ap_reset_dev(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
     ATH_INTERNAL_RESET_LOCK(sc);
#endif
    ath_reset_start(sc, 0, 0, 0);
    ath_reset(sc);
    ath_reset_end(sc, 0);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
    ATH_INTERNAL_RESET_UNLOCK(sc);
#endif
    return QDF_STATUS_SUCCESS;
}

bool ath_green_ap_is_powersave_on(struct ath_softc *sc)
{
    struct wlan_lmac_if_green_ap_rx_ops *green_ap_rx_ops;
    struct wlan_objmgr_psoc *psoc;

    psoc = wlan_pdev_get_psoc(sc->sc_pdev);
    green_ap_rx_ops = wlan_lmac_if_get_green_ap_rx_ops(psoc);
    if (!green_ap_rx_ops) {
        qdf_print("invalid rx_ops");
        return false;
    }

    return (green_ap_rx_ops->is_ps_enabled(sc->sc_pdev));
}

bool ath_green_ap_get_enable_print(struct ath_softc *sc)
{
    struct wlan_lmac_if_green_ap_rx_ops *green_ap_rx_ops;
    struct wlan_objmgr_psoc *psoc;

    psoc = wlan_pdev_get_psoc(sc->sc_pdev);
    green_ap_rx_ops = wlan_lmac_if_get_green_ap_rx_ops(psoc);
    if (!green_ap_rx_ops) {
        qdf_print("invalid rx_ops");
        return false;
    }

    return (green_ap_rx_ops->is_dbg_print_enabled(sc->sc_pdev));
}
EXPORT_SYMBOL(ath_green_ap_get_enable_print);

u_int8_t ath_green_ap_get_enable(struct ath_softc *sc)
{
    struct wlan_lmac_if_green_ap_rx_ops *green_ap_rx_ops;
    struct wlan_objmgr_psoc *psoc;
    uint8_t val;

    psoc = wlan_pdev_get_psoc(sc->sc_pdev);
    green_ap_rx_ops = wlan_lmac_if_get_green_ap_rx_ops(psoc);
    if (!green_ap_rx_ops) {
        qdf_print("invalid rx_ops");
        return 0;
    }

    green_ap_rx_ops->ps_get(sc->sc_pdev, &val);
    return val;
}
EXPORT_SYMBOL(ath_green_ap_get_enable);

bool ath_green_ap_set_enable(struct ath_softc *sc, uint32_t value)
{
    struct wlan_lmac_if_green_ap_rx_ops *green_ap_rx_ops;
    struct wlan_objmgr_psoc *psoc;

    psoc = wlan_pdev_get_psoc(sc->sc_pdev);
    green_ap_rx_ops = wlan_lmac_if_get_green_ap_rx_ops(psoc);
    if (!green_ap_rx_ops) {
        qdf_print("invalid rx_ops");
        return false;
    }

    return green_ap_rx_ops->ps_set(sc->sc_pdev, value);
}
EXPORT_SYMBOL(ath_green_ap_set_enable);

void ath_green_ap_suspend(struct ath_softc *sc)
{
    struct wlan_lmac_if_green_ap_rx_ops *green_ap_rx_ops;
    struct wlan_objmgr_psoc *psoc;

    psoc = wlan_pdev_get_psoc(sc->sc_pdev);
    green_ap_rx_ops = wlan_lmac_if_get_green_ap_rx_ops(psoc);
    if (!green_ap_rx_ops) {
        qdf_print("invalid rx_ops");
        return;
    }

    green_ap_rx_ops->suspend_handle(sc->sc_pdev);
}
#endif /* WLAN_SUPPORT_GREEN_AP */

/*
 * get node rssi as queried from umac layer
 */
int8_t ath_get_noderssi(ath_node_t node, int8_t chain, u_int8_t flags)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = an->an_sc;
    int32_t avgrssi;
    int32_t rssi;
    /*
     * When only one frame is received there will be no state in
     * avgrssi so fallback on the value recorded by the 802.11 layer.
     */

    avgrssi = ATH_RSSI_DUMMY_MARKER;

    if (flags & IEEE80211_RSSI_BEACON) {
        if (chain == -1) {
            if (ath_get_hwbeaconproc_active(sc)) {
                /* hw beacon processing => return hardware rssi */
                ATH_PS_WAKEUP(sc);
                rssi = ath_hw_beacon_rssi(sc);
                ATH_PS_SLEEP(sc);
                return rssi;
            } else {
                avgrssi = an->an_avgbrssi;
            }
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = an->an_avgbchainrssi[chain];
            } else {
                avgrssi = an->an_avgbchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_RXDATA) {
        if (chain == -1) {
            avgrssi = an->an_avgdrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = an->an_avgdchainrssi[chain];
            } else {
                avgrssi = an->an_avgdchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_RX) {
        if (chain == -1) {
            avgrssi = an->an_avgrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = an->an_avgchainrssi[chain];
            } else {
                avgrssi = an->an_avgchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_TX) {
        if (chain == -1) {
            avgrssi = an->an_avgtxrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = an->an_avgtxchainrssi[chain];
            } else {
                avgrssi = an->an_avgtxchainrssiext[chain];
            }
        }
    }

    if (avgrssi != ATH_RSSI_DUMMY_MARKER)
        rssi = ATH_EP_RND(avgrssi, HAL_RSSI_EP_MULTIPLIER);
    else
        rssi =  -1;

    return rssi;
}

/*
 * get node rate as queried from umac layer
 */
u_int32_t ath_get_noderate(ath_node_t node, u_int8_t type)
{
    struct ath_node *an = ATH_NODE(node);
    if (type == IEEE80211_RATE_TX) {
        return ATH_RATE_OUT(an->an_avgtxrate);
    } else if (type == IEEE80211_LASTRATE_TX) {
        return an->an_lasttxrate;
    } else if (type == IEEE80211_LASTRATE_RX) {
        return an->an_lastrxrate;
    } else if (type == IEEE80211_RATECODE_TX) {
        return an->an_txratecode;
    } else if (type == IEEE80211_RATECODE_RX) {
        return an->an_rxratecode;
    } else if (type == IEEE80211_RATE_RX) {
        return ATH_RATE_OUT(an->an_avgrxrate);
    } else if (type == IEEE80211_RATEFLAGS_TX) {
        return an->an_txrateflags;
    } else {
        return 0;
    }
}

#if ATH_SUPPORT_WIFIPOS
static void ath_vap_reap_txqs(ath_dev_t dev,int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp=NULL;
    u_int32_t i;
    int npend = 0;
    struct ath_hal *ah = sc->sc_ah;
    u_int8_t q_needs_pause[HAL_NUM_TX_QUEUES];
    int restart_after_reset=0;

    if (if_id != ATH_IF_ID_ANY) {
        avp = sc->sc_vaps[if_id];
    }
    ATH_INTR_DISABLE(sc);
restart_reaping:
     OS_MEMZERO(q_needs_pause, sizeof(q_needs_pause));
if (!sc->sc_invalid) {
        struct ath_txq *txq=NULL;
        (void) ath_hal_aborttxdma(ah);
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
            if (ATH_TXQ_SETUP(sc, i)) {
                int n_q_pending=0;
                txq = &sc->sc_txq[i];
                /* The TxDMA may not really be stopped.
                     * Double check the hal tx pending count
                     */
                     n_q_pending = ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     if (n_q_pending) {
                        (void) ath_hal_stoptxdma(ah, txq->axq_qnum, 0);
                        npend += ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     }
            }
                /*
                 * at this point all Data queues are paused
                 * all the queues need to processed and restarted.
                 */
                q_needs_pause[i] = AH_TRUE;
        }
    }
if (npend && !restart_after_reset) {
#ifdef AR_DEBUG
       ath_dump_descriptors(sc);
#endif
        /* TxDMA not stopped, reset the hal */
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Unable to stop TxDMA. Reset HAL!\n", __func__);

#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
        ATH_INTERNAL_RESET_LOCK(sc);
#endif
        ath_reset_start(sc, 0, 0, 0);
        ath_reset(sc);
        ath_reset_end(sc, 0);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
        ATH_INTERNAL_RESET_UNLOCK(sc);
#endif
        restart_after_reset=1;
        goto restart_reaping;
    }

    if (npend && restart_after_reset) {
        /* TxDMA not stopped, reset the hal */
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Unable to stop TxDMA Even after Reset, ignore and continue \n", __func__);
    }
    /* TO-DO need to handle cab queue */

    /* at this point the HW xmit should have been completely stopped. */
    if (sc->sc_enhanceddmasupport) {
        ath_tx_edma_process(sc);
    }
    ath_reset_draintxq(sc,0,0);
    if (ATH_STARTRECV(sc) != 0) {
             printk("%s: unable to restart recv logic\n",
                                        __func__);
    }

    ATH_INTR_ENABLE(sc);
}
#endif
/*
 * pause/unpause dat trafic on HW queues for the given vap(s).
 * if pause is true then perform pause operation.
 * if pause is false then perform unpause operation.
 * if vap is null the perform the requested operation on all the vaps.
 * if vap is non null the perform the requested operation on the vap.
 */
static void  ath_vap_pause_control(ath_dev_t dev,int if_id, bool pause)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp=NULL;

    if (if_id != ATH_IF_ID_ANY) {
        avp = sc->sc_vaps[if_id];
    }

    DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_id = %d, pause = %d\n",
            __func__, if_id, pause);

    ath_tx_vap_pause_control(sc,avp,pause);
}

/*
 * if pause == true then pause the node (pause all the tids )
 * if pause == false then unpause the node (unpause all the tids )
 */
static void  ath_node_pause_control(ath_dev_t dev,ath_node_t node, bool pause)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

    if (pause) {
        ath_tx_node_pause(sc,an);
    } else {
        ath_tx_node_resume(sc,an);
    }

}

static void  ath_cdd_control(ath_dev_t dev, u_int32_t enabled)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_cddLimitingEnabled = enabled;
}

/*
 * if enable == true then enable rx rifs
 * if enable == false then diasable rx rifs
 */
static void ath_set_rifs(ath_dev_t dev, bool enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_set_rifs(sc->sc_ah, enable);
}

#if ATH_ANT_DIV_COMB
/*
 * if enable == true then enable Rx(LNA) diversity
 * if enable == false then diasable Rx(LNA) diversity
 */
void ath_set_lna_div_use_bt_ant(ath_dev_t dev, bool enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_WAKEUP(sc);
    if ( sc->sc_reg_parm.bt_reg.btCoexEnable && sc->sc_reg_parm.lnaDivUseBtAntEnable ) {
        // Make sure this scheme is only used for WB225(Astra) and enable lna diversity feature
        if (enable) {
            sc->sc_antDivComb = AH_TRUE;
            ath_ant_div_comb_init(&sc->sc_antcomb, sc);
        } else {
            sc->sc_antDivComb = sc->sc_antDivComb_record;
        }
        ath_hal_set_lna_div_use_bt_ant(sc->sc_ah, enable, &sc->sc_curchan);
    }
    ATH_PS_SLEEP(sc);
}
#endif /* ATH_ANT_DIV_COMB */

/*
 * Total free buffer avaliable in common pool of buffers
 */
static u_int32_t ath_get_txbuf_free(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_txbuf_free;
}


#if UMAC_SUPPORT_WNM
void ath_wnm_beacon_config(ath_dev_t dev, int reason, int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_WAKEUP(sc);
    ath_beacon_config(sc, (ath_beacon_config_reason)reason, if_id);
    ATH_PS_SLEEP(sc);
    return;
}
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
u_int8_t ath_get_pwrsaveq_len(ath_node_t node, u_int8_t frame_type)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_node_pwrsaveq *dataq,*mgtq;
    int qlen;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an);
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an);

    /*
     * calculate the combined queue length of management and data queues.
     */
    qlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);
    qlen += ATH_NODE_PWRSAVEQ_QLEN(mgtq);

    return qlen;
}
#endif

#if ATH_GEN_RANDOMNESS
void ath_gen_randomness(struct ath_softc *sc)
{
    sc->sc_random_value |= (sc->sc_ath_ops.ath_get_rssi_chain0(sc) & 0x1) <<
                   sc->sc_random_pos;

    if (++sc->sc_random_pos == BIT_POS) {
        OS_ADD_RANDOMNESS(&sc->sc_random_value, 1, BIT_POS);
        sc->sc_random_value = 0;
        sc->sc_random_pos = 0;
    }
}
EXPORT_SYMBOL(ath_gen_randomness);
#endif

static u_int8_t* ath_get_tpc_tables(ath_dev_t dev, u_int8_t *ratecount)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (ah->ah_get_tpc_tables)
        return ath_hal_get_tpc_tables(ah, ratecount);
    else
        return NULL;
}

static void ath_set_node_tx_chainmask(ath_node_t node,
                                    u_int8_t chainmask)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc;

    if (!an)
        return;

    sc = an->an_sc;
    if (!sc || chainmask == sc->sc_tx_chainmask)
        return;

    chainmask &= (1 << MAX_CHAINS) - 1;

    /* per node tx chainmask for LMAC internally generated frame */
    an->an_tx_chainmask = chainmask;

    return;
}

#ifdef ATH_TX99_DIAG
u_int8_t ath_tx99_ops(ath_dev_t dev, int cmd, void *tx99_wcmd)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct net_device *wifidev = OS_ATH_GET_NETDEV(sc);

    return tx99_ioctl(wifidev, sc, cmd, tx99_wcmd);
}
#endif

u_int8_t ath_set_ctl_pwr(ath_dev_t dev, u_int8_t *ctl_pwr_array,
                            int rd, u_int8_t chainmask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t err = 0;

	if (sc->sc_ieee_ops->set_regdomaincode) {
    	err = sc->sc_ieee_ops->set_regdomaincode(sc->sc_ieee, rd);
    }

    /*
     * Copy the ctl_pwr_array content to eeprom ctl section in software.
     * Keep the ctl_index arrays (for FCC/Japan/ETSI only).
     */
    if (!err) {
        err = ath_hal_set_ctl_pwr(sc->sc_ah, ctl_pwr_array);

        /* internal reset for power table re-calculation */
        sc->sc_reset_type = ATH_RESET_NOLOSS;
        ath_internal_reset(sc);
        sc->sc_reset_type = ATH_RESET_DEFAULT;
    }

    return err;
}

bool ath_ant_swcom_sel(ath_dev_t dev, u_int8_t ops,
                        u_int32_t *common_tbl1, u_int32_t *common_tbl2)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!common_tbl1 || !common_tbl2)
        return false;

    return ath_hal_ant_swcom_sel(sc->sc_ah, ops, common_tbl1, common_tbl2);
}

#if ATH_SUPPORT_DFS
int ath_dfs_get_caps(ath_dev_t dev, struct wlan_dfs_caps *dfs_caps)
{
#define HAL_CAP_RADAR           0
#define HAL_CAP_AR                 1
#define   HAL_CAP_STRONG_DIV 2

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Get capability information - can extension channel radar be detected and should we use combined radar RSSI or not.*/
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_COMBINED_RADAR_RSSI, 0, 0)
                               == HAL_OK) {
            sc->ath_dfs_combined_rssi_ok = 1;
    } else {
            sc->ath_dfs_combined_rssi_ok = 0;
    }
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_EXT_CHAN_DFS, 0, 0)
                                == HAL_OK) {
        sc->ath_dfs_ext_chan_ok = 1;
    } else {
        sc->ath_dfs_ext_chan_ok = 0;
    }

    if (ath_hal_hasenhanceddfssupport(sc->sc_ah)) {
        sc->ath_dfs_use_enhancement = 1;
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: use DFS enhancements\n", __func__);
    } else {
        sc->ath_dfs_use_enhancement = 0;
    }

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PHYDIAG,
              HAL_CAP_RADAR, NULL) == HAL_OK) {
    u_int32_t val;
    /*
     * If we have fast diversity capability, read off
     * Strong Signal fast diversity count set in the ini
     * file, and store so we can restore the value when
     * radar is disabled
     */
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_DIVERSITY, HAL_CAP_STRONG_DIV,
                  &val) == HAL_OK) {
            sc->ath_strong_signal_diversiry = 1;
            sc->ath_fastdiv_val = val;
           printk( "%s[%d] SHOULD ABORT \n", __func__, __LINE__);
    }
    } else {
        sc->ath_strong_signal_diversiry = 0;
        sc->ath_fastdiv_val = 0;
    }

    dfs_caps->wlan_dfs_ext_chan_ok = sc->ath_dfs_ext_chan_ok;
    dfs_caps->wlan_dfs_combined_rssi_ok = sc->ath_dfs_combined_rssi_ok;
    dfs_caps->wlan_dfs_use_enhancement = sc->ath_dfs_use_enhancement;
    dfs_caps->wlan_strong_signal_diversiry = sc->ath_strong_signal_diversiry;
    dfs_caps->wlan_chip_is_bb_tlv = sc->ath_chip_is_bb_tlv;
    dfs_caps->wlan_chip_is_over_sampled = sc->ath_chip_is_over_sampled;
    dfs_caps->wlan_chip_is_ht160 = sc->ath_chip_is_ht160;
    dfs_caps->wlan_chip_is_false_detect = sc->ath_chip_is_false_detect;
    dfs_caps->wlan_fastdiv_val = sc->ath_fastdiv_val;

    return 0;
#undef   HAL_CAP_STRONG_DIV
#undef  HAL_CAP_AR
#undef  HAL_CAP_RADAR
}
int ath_detach_dfs(ath_dev_t dev)
{
    //struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return 0;
}

int ath_enable_dfs(ath_dev_t dev, int *is_fastclk, HAL_PHYERR_PARAM *pe, u_int32_t dfsdomain)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t rfilt;
    ath_disable_dfs(dev);
    ATH_PS_WAKEUP(sc);                   /*  Must be called ATH_PS_SLEEP(), before return from function */
    ath_hal_enabledfs(sc->sc_ah, pe);
    rfilt = ath_hal_getrxfilter(sc->sc_ah);
    rfilt |= HAL_RX_FILTER_PHYRADAR;
    ath_hal_setrxfilter(sc->sc_ah, rfilt);
    if (is_fastclk) {
        *is_fastclk = ath_hal_is_fast_clock_enabled(sc->sc_ah);
    }
    ath_adjust_difs(sc, dfsdomain);
    ATH_PS_SLEEP(sc);
    return 0;
}




int ath_disable_dfs(ath_dev_t dev)
{
    u_int32_t rfilt;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ATH_PS_WAKEUP(sc);                   /* Must be called  ATH_PS_SLEEP(), before return from function */
    rfilt = ath_hal_getrxfilter(sc->sc_ah);
    rfilt &= ~HAL_RX_FILTER_PHYRADAR;
    ath_hal_setrxfilter(sc->sc_ah, rfilt);
    ATH_PS_SLEEP(sc);
    return 0;
}
int ath_get_mib_cycle_counts_pct(ath_dev_t dev, u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return(ath_hal_getMibCycleCountsPct(sc->sc_ah,  rxc_pcnt, rxf_pcnt, rxf_pcnt));
}

static int
ath_get_radar_thresholds(ath_dev_t dev, HAL_PHYERR_PARAM *pe)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);


    ath_hal_getdfsthresh(sc->sc_ah, pe);
    return (0);
}

void
ath_process_phyerr(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs, u_int64_t fulltsf)
{
    u_int8_t rssi;
    u_int8_t ext_rssi=0;
    u_int16_t datalen;

    if (((rxs->rs_phyerr != HAL_PHYERR_RADAR)) &&
        ((rxs->rs_phyerr != HAL_PHYERR_FALSE_RADAR_EXT)))  {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: rs_phyer=0x%x not a radar error\n",__func__, rxs->rs_phyerr);
            return;
        }

        datalen = rxs->rs_datalen;
        /* WAR: Never trust combined RSSI on radar pulses for <=
         * OWL2.0. For short pulses only the chain 0 rssi is present
         * and remaining descriptor data is all 0x80, for longer
         * pulses the descriptor is present, but the combined value is
         * inaccurate. This HW capability is queried in dfs_attach and stored in
         * the sc_dfs_combined_rssi_ok flag.*/

        if (sc->ath_dfs_combined_rssi_ok) {
                rssi = (u_int8_t) rxs->rs_rssi;
        } else {
            rssi = (u_int8_t) rxs->rs_rssi_ctl0;
        }

        ext_rssi = (u_int8_t) rxs->rs_rssi_ext0;


        /* hardware stores this as 8 bit signed value.
         * we will cap it at 0 if it is a negative number
         */

        if (rssi & 0x80)
            rssi = 0;

        if (ext_rssi & 0x80)
            ext_rssi = 0;


        /* If radar can be detected on the extension channel (for SOWL onwards), we have to read radar data differently as the HW supplies bwinfo and duration for both primary and extension channel.*/
        if (sc->ath_dfs_ext_chan_ok) {

            /* If radar can be detected on the extension channel, datalen zero pulses are bogus, discard them.*/
            if (!datalen) {
                //UMACDFS: dfs->ath_dfs_stats.datalen_discards++;
                //return;
            }

        }
      sc->sc_ieee_ops->dfs_proc_phyerr(sc->sc_ieee, bf->bf_vdata, datalen, rssi, ext_rssi, rxs->rs_tstamp, fulltsf);
#if 0
#undef EXT_CH_RADAR_FOUND
#undef PRI_CH_RADAR_FOUND
#undef EXT_CH_RADAR_EARLY_FOUND
#endif
}
EXPORT_SYMBOL(ath_process_phyerr);
#endif /* ATH_SUPPORT_DFS */

#if UNIFIED_SMARTANTENNA
static int ath_smart_ant_enable(ath_dev_t dev, int enable, int mode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int retval;

    retval = ath_hal_set_smartantenna(sc->sc_ah, enable & SMART_ANT_ENABLE_MASK, mode);
    if (retval == -1)
    {
        printk("Smart antenna is not supported\n");
        return 0;
    }
    if (enable & SMART_ANT_ENABLE_MASK) {
        switch(mode) {
            case 0:
                /* Serial mode GPIO */
                ath_hal_gpioCfgOutput(sc->sc_ah, 0, HAL_GPIO_OUTPUT_MUX_AS_SMART_ANT_SERIAL_STROBE);
                ath_hal_gpioCfgOutput(sc->sc_ah, 0, HAL_GPIO_OUTPUT_MUX_AS_SMART_ANT_SERIAL_DATA);
                break;
            case 1:
                /* Parallel mode */
                ath_hal_gpioCfgOutput(sc->sc_ah, 0, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL0);
                ath_hal_gpioCfgOutput(sc->sc_ah, 0, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL1);
                ath_hal_gpioCfgOutput(sc->sc_ah, 0, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL2);
                break;
        }
    }
    sc->sc_smart_ant_enable = enable;
    sc->sc_smart_ant_mode = mode;
    return 0;
}

static void ath_smart_ant_set_tx_antenna(ath_node_t node, u_int32_t *antenna_array, u_int8_t num_rates)
{
    int i=0;
    struct ath_node *an = (struct ath_node *) node;
    struct ath_softc *sc = an->an_sc;;

    for (i=0; i<= sc->max_fallback_rates; i++) {
        an->smart_ant_antenna_array[i] = antenna_array[i];
    }
}

static void ath_smart_ant_set_tx_defaultantenna(ath_dev_t dev, u_int32_t antenna)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->smart_ant_tx_default = antenna;
}

static void ath_smart_ant_set_training_info(ath_node_t node, uint32_t *rate_array, uint32_t *antenna_array, uint32_t numpkts)
{
    struct ath_node *an = (struct ath_node *) node;
    struct ath_softc *sc = an->an_sc;;

    OS_MEMCPY(an->traininfo.antenna_array, antenna_array, ((sc->max_fallback_rates+1)*sizeof(u_int32_t)));
    OS_MEMCPY(an->traininfo.rate_array, rate_array, ((sc->max_fallback_rates+1)*sizeof(uint32_t)));

    an->traininfo.num_pkts = numpkts;
    an->smart_ant_train_status = 1;
    an->trainstats.nFrames = 0;

}

static void
ath_smart_ant_prepare_rateset(ath_dev_t dev, ath_node_t node, void *prate_info)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    return ath_smart_ant_rate_prepare_rateset(sc, an, prate_info);
}
#endif


static void ath_conf_rx_intr_mit(void* arg,u_int32_t enable)
{
    struct ath_softc* sc = ATH_DEV_TO_SC(arg);
#ifdef HOST_OFFLOAD
    u_int32_t lower_rx_mitigation;
#endif
    ath_hal_set_config_param(sc->sc_ah, HAL_CONFIG_INTR_MITIGATION_RX,
                              &enable);
#ifdef HOST_OFFLOAD

    if(enable==1)
        lower_rx_mitigation=0;
    else
        lower_rx_mitigation=1;
    ath_hal_set_config_param(sc->sc_ah, HAL_CONFIG_LOWER_INT_MITIGATION_RX,
                              &lower_rx_mitigation);
#endif
}

static void ath_set_beacon_interval(ath_dev_t arg, u_int16_t intval)
{
       struct ath_softc* sc = ATH_DEV_TO_SC(arg);

       sc->sc_intval = intval;
}
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
static void  ath_txbf_loforceon_update(ath_dev_t dev,bool loforcestate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if (ath_hal_txbf_loforceon_update(sc->sc_ah,loforcestate));
        sc->sc_loforce_enable = loforcestate;
}
#endif


/*
 * value:1 pspoll received for this node
 * value:0 node exited from powersave state
 */
static void
ath_node_pspoll(ath_dev_t dev,ath_node_t node, bool value)
{
	struct ath_node *an = ATH_NODE(node);
	an->an_pspoll = value;
}

#if ATH_GEN_RANDOMNESS
#define ATH_RNG_BUF_SIZE 320
#define ATH_RNG_READ_DELAY_100MS 100000
#define ATH_RNG_READ_DELAY_50MS  50000
#define ATH_RNG_READ_DELAY_10MS  10000
/*kthread to read and update random value
Mode0 : Random is updated by rssi on interrupt.
Mode1 : Random is updated by rssi every 10 ms. and if we read same rssi for 5 times we wil read from ADC regs and sleep 50ms.
Mode2:  Random is updated by Adc regs alone every 100ms*/

static int ath_rng_kthread(void *data)
{
    int bytes_read;
    struct ath_softc *sc = (struct ath_softc *)data;
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t *rng_buf;
    u_int32_t last_rssi = 0;
    u_int32_t current_rssi = 0;
    u_int32_t rssi_const_count  = 0;
    u_int32_t mode = 0;
    u_int32_t last_mode =0;
    struct ieee80211com *ic = (struct ieee80211com *)sc->sc_ieee;
    if(ic->ic_is_mode_offload(ic))
    {
        goto out;
    }
    rng_buf= ( u_int32_t *) OS_MALLOC(sc->sc_osdev,ATH_RNG_BUF_SIZE*(sizeof(uint32_t)),GFP_KERNEL);
    if (!rng_buf)
        goto out;
    while (!kthread_should_stop()) {

        mode = ic->random_gen_mode;
        if(last_mode != mode)
        {
            printk("mode changed to %d\n",ic->random_gen_mode);
            last_mode = mode;
        }
        if (mode ==0) {
            rssi_const_count  = 0;
            OS_SLEEP_INTERRUPTIBLE( ATH_RNG_READ_DELAY_100MS);
            continue;
        }
        if (!sc->sc_nvaps || sc->sc_invalid || sc->sc_scanning || atomic_read(&sc->sc_in_reset)) {

            OS_SLEEP_INTERRUPTIBLE( ATH_RNG_READ_DELAY_100MS);
            continue;
        }
        if(mode == 1)
        {
            current_rssi =sc->sc_ath_ops.ath_get_rssi_chain0(sc);
            if(current_rssi == last_rssi)
            {
                rssi_const_count ++;
            }
            else
            {
                if(rssi_const_count )
                    rssi_const_count =0;
                ath_gen_randomness(sc);
                last_rssi = current_rssi;
            }
        }
        if ((rssi_const_count  > ATH_RSSI_CONST_COUNT_LIMIT) || mode == 2)
        {
            ATH_PS_WAKEUP(sc);
            OS_SLEEP_INTERRUPTIBLE(1000);
            bytes_read = ath_hal_adc_data_read(ah, rng_buf,
                    ATH_RNG_BUF_SIZE,&sc->rng_last);
            ATH_PS_SLEEP(sc);
            if (unlikely(!bytes_read)) {
                continue;
            }
            if(bytes_read > ATH_RNG_BUF_SIZE * 4)
            {
                printk("ERROR \n");
                bytes_read = ATH_RNG_BUF_SIZE * 4;
            }
            random_input_words(rng_buf,bytes_read >> 2,32);
            rssi_const_count  = 0;
            if(mode == 1) {
                OS_SLEEP_INTERRUPTIBLE(ATH_RNG_READ_DELAY_50MS);
            }
            else {
                OS_SLEEP_INTERRUPTIBLE( ATH_RNG_READ_DELAY_100MS);
            }

        }

        else {
            OS_SLEEP_INTERRUPTIBLE(ATH_RNG_READ_DELAY_10MS);
        }
    }

    OS_FREE(rng_buf);
out:
    sc->rng_task = NULL;

    return 0;
}
#endif
static int
ath_get_vap_stats(ath_dev_t dev, int if_id, struct ath_vap_dev_stats *vap_stats)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp = sc->sc_vaps[if_id];

    memcpy(vap_stats, (struct ath_vap_stats *)&avp->av_stats, sizeof(avp->av_stats));
    return 0;
}

static int
ath_sc_chan_busy(void *arg)
{
    struct ath_softc        *sc = (struct ath_softc *)arg;
    u_int8_t ctlrxc, extrxc, rfcnt, tfcnt;
    u_int32_t chan_busy_per = 0, bss_rx_busy_per = 0;

    if (!sc->sc_nvaps || sc->sc_invalid ||
        sc->sc_scanning || atomic_read(&sc->sc_in_reset)) {
      return 0;
    }

    /* get the channel busy percentages */
    ATH_PS_WAKEUP(sc);
    chan_busy_per = ath_hal_getchbusyper(sc->sc_ah);
    ATH_PS_SLEEP(sc);

    ctlrxc = chan_busy_per & 0xff;
    extrxc = (chan_busy_per & 0xff00) >> 8;
    rfcnt = (chan_busy_per & 0xff0000) >> 16;
    tfcnt = (chan_busy_per & 0xff000000) >> 24;

    if ((ctlrxc == 255) || (extrxc == 255) || (rfcnt == 255) || (tfcnt == 255))
        return 0;

    sc->sc_chan_busy_per = chan_busy_per;

    /* percentage of packets recieved within the timer period */
    /* ((packet dur/ 200000) * 100) */
    bss_rx_busy_per = sc->sc_rx_packet_dur / (ATH_CHAN_BUSY_INTVAL * 10);
    sc->sc_rx_packet_dur = 0;

    sc->sc_ieee_ops->proc_chan_util(sc->sc_ieee, bss_rx_busy_per, chan_busy_per);

    /* re-arm the timer */
    ath_set_timer_period(&sc->sc_chan_busy, ATH_CHAN_BUSY_INTVAL);

    return 0;
}

u_int32_t ath_get_last_txpower(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    return ((an->an_lasttxpower) / 2);
}

void ath_get_min_and_max_powers(ath_dev_t dev,
        int8_t *max_tx_power,
        int8_t *min_tx_power)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_get_chip_min_and_max_powers(sc->sc_ah, max_tx_power, min_tx_power);
}

uint32_t ath_get_modeSelect(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    uint32_t wMode;
    uint32_t netBand;

    wMode = sc->sc_reg_parm.wModeSelect;

    if (!(wMode & HAL_MODE_11A))
        wMode &= ~(HAL_MODE_TURBO | HAL_MODE_108A | HAL_MODE_11A_HALF_RATE);

    if (!(wMode & HAL_MODE_11G))
        wMode &= ~(HAL_MODE_108G);

    netBand = sc->sc_reg_parm.NetBand;
    if (!(netBand & HAL_MODE_11A))
        netBand &= ~(HAL_MODE_TURBO | HAL_MODE_108A | HAL_MODE_11A_HALF_RATE);

    if (!(netBand & HAL_MODE_11G))
        netBand &= ~(HAL_MODE_108G);

    wMode &= netBand;

    return wMode;
}

uint32_t ath_get_chip_mode(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return ath_hal_get_chip_mode(sc->sc_ah);
}

void ath_get_freq_range(ath_dev_t dev,
        uint32_t flags,
        uint32_t *low_freq,
        uint32_t *high_freq)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_get_freq_range(sc->sc_ah, flags, low_freq, high_freq);
}

void ath_fill_hal_chans_from_regdb(ath_dev_t dev,
        uint16_t freq,
        int8_t maxregpower,
        int8_t maxpower,
        int8_t minpower,
        uint32_t flags,
        int index)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_fill_hal_chans_from_regdb(sc->sc_ah,
            freq,
            maxregpower,
            maxpower,
            minpower,
            flags,
            index);
}
