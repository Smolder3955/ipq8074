/*
 * Copyright (c) 2013-2015, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2010 Atheros Communications, Inc.

 * All rights reserved.
 */


#include <qcatools_lib.h>         /* library for common headerfiles */
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <time.h>
#include <glob.h>                 /* Including glob.h for glob() function used in find_pid() */
#include <sys/queue.h>
#include <pthread.h>
#include <netlink/attr.h>
#include <ieee80211_band_steering_api.h>
#include <ieee80211_external.h>
#include <unistd.h>
#if QCA_LTEU_SUPPORT
#define MAX_NUM_TRIES 5
#endif

static const unsigned NL80211_ATTR_MAX_INTERNAL = 256;
enum qca_wlan_genric_data {
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
    QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
    QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST,
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX =
        QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST - 1
};

#define TSPECTOTIME(tspec,tmval,buffer)                           \
    tzset();                                                      \
    if (localtime_r(&(tspec.tv_sec), &tmval) == NULL)             \
        return ;                                                  \
    strftime(buffer, sizeof(buffer),"%Y-%m-%d %H:%M:%S %Z", &tmval)

#ifndef MAX_FIPS_BUFFER_SIZE
#define MAX_FIPS_BUFFER_SIZE (sizeof(struct ath_fips_output) + 1500)
#endif

#define WIFI_NL80211_CMD_SOCK_ID    DEFAULT_NL80211_CMD_SOCK_ID
#define WIFI_NL80211_EVENT_SOCK_ID  DEFAULT_NL80211_EVENT_SOCK_ID

void (*usage_cmd)(void);

struct queue_entry {
    void *value;
    TAILQ_ENTRY(queue_entry) tailq;
};
TAILQ_HEAD(queue_head, queue_entry);

struct queue {
    pthread_mutex_t mutex;
    struct queue_head head;
};

int q_init(struct queue *q)
{
    TAILQ_INIT(&q->head);
    return pthread_mutex_init(&q->mutex, NULL);
}

void q_insert(struct queue *q, void *value)
{
    struct queue_entry *entry;
    entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        fprintf(stderr, "%s:%d Could not allocate memory\n", __func__, __LINE__);
        exit(-1);
    }
    entry->value = value;
    pthread_mutex_lock(&q->mutex);
    TAILQ_INSERT_TAIL(&q->head, entry, tailq);
    pthread_mutex_unlock(&q->mutex);
}

int q_remove(struct queue *q, void **value)
{
    struct queue_entry *entry;
    pthread_mutex_lock(&q->mutex);
    if (TAILQ_EMPTY(&q->head)) {
        pthread_mutex_unlock(&q->mutex);
        return -EINVAL;
    }
    entry = TAILQ_FIRST(&q->head);
    *value = entry->value;
    TAILQ_REMOVE(&q->head, entry, tailq);
    pthread_mutex_unlock(&q->mutex);
    free(entry);
    return 0;
}

#define MAX_INTERFACES 100
#define MAX_IFNAME_LEN 20
struct queue event_q;
static uint8_t event_filter_array[IEEE80211_DBGREQ_MAX] = {0};
static const char *if_name_filter;
struct dbg_event_q_entry {
    struct ieee80211req_athdbg_event *event;
    uint32_t total_buf_size;
};

static void inline set_event_filter(uint8_t cmd)
{
    event_filter_array[cmd] = 1;
}

static void inline clear_event_filter(uint8_t cmd)
{
    event_filter_array[cmd] = 0;
}

static void inline set_if_name_filter(const char *ifname)
{
    if_name_filter = ifname;
}

static int if_name_filter_set(char *ifname)
{
    if (if_name_filter == NULL) {
        return 0;
    } else if (strncmp(if_name_filter, ifname, MAX_IFNAME_LEN) == 0) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * Global context to decide whether
 * ioctl calls should be used or cfg80211
 * calls should be used.
 * Default is ioctl.
 */
struct socket_context sock_ctx;

static void usage(void)
{
    fprintf(stderr, "usage: wifitool athX cmd args\n");
    fprintf(stderr, "cmd: fips\t args: interface_name input_file\n"
            "\tInput file format: Each set of inputs seperated by newline\n"
            "\t<FIPS Command> <MODE> <Key length> <Input Data Length> <Key> <Input Data> <Expected Output> <IV with 16 bytes><newline>"
            "\tExample: wifitool ath0 fips input_file \n Refer README_FIPS in drivers/wlan_modules/os/linux/tools\n");
    fprintf(stderr, "cmd: [sendaddba senddelba setaddbaresp getaddbastats sendaddts senddelts refusealladdbas \n");
    fprintf(stderr, "cmd: [sendstastats sendchload sendnhist sendlcireq rrmstats bcnrpt setchanlist getchanlist] \n");
    fprintf(stderr, "cmd: [sendtsmrpt sendneigrpt sendlmreq sendbstmreq  sendbcnrpt sendcca sendrpihist] \n");
    fprintf(stderr, "cmd: [block_acs_channel] \n");
    fprintf(stderr, "cmd: [rrm_sta_list] \n");
    fprintf(stderr, "cmd: [btm_sta_list] \n");
    fprintf(stderr, "cmd: [mu_scan lteu_cfg ap_scan] \n");
    fprintf(stderr, "cmd: [atf_debug_size atf_dump_debug] \n");
    fprintf(stderr, "cmd: [atf_debug_nodestate] \n");
    fprintf(stderr, "cmd: [tr069_get_vap_stats] \n");
    fprintf(stderr, "cmd: [tr069_chanhist] \n");
    fprintf(stderr, "cmd: [tr069_chan_inuse] \n");
    fprintf(stderr, "cmd: [tr069_set_oper_rate] \n");
    fprintf(stderr, "cmd: [tr069_get_oper_rate] \n");
    fprintf(stderr, "cmd: [tr069_get_posiblrate] \n");
    fprintf(stderr, "cmd: [chmask_persta] \n");
    fprintf(stderr, "cmd: [peer_nss] \n");
    fprintf(stderr, "cmd: [beeliner_fw_test] \n");
    fprintf(stderr, "cmd: [init_rtt3]\n");
    fprintf(stderr, "cmd: [bsteer_getparams bsteer_setparams] \n");
    fprintf(stderr, "cmd: [bsteer_getdbgparams bsteer_setdbgparams] \n");
    fprintf(stderr, "cmd: [bsteer_enable] [bsteer_enable_events] \n");
    fprintf(stderr, "cmd: [bsteer_getoverload bsteer_setoverload] \n");
    fprintf(stderr, "cmd: [bsteer_getrssi] \n");
    fprintf(stderr, "cmd: [bsteer_setproberespwh bsteer_getproberespwh] \n");
    fprintf(stderr, "cmd: [bsteer_setauthallow] \n");
    fprintf(stderr, "cmd: [set_antenna_switch] \n");
    fprintf(stderr, "cmd: [set_usr_ctrl_tbl] \n");
    fprintf(stderr, "cmd: [offchan_tx_test] \n");
    fprintf(stderr, "cmd: [offchan_rx_test] \n");
    fprintf(stderr, "cmd: [sendbstmreq sendbstmreq_target] \n");
    fprintf(stderr, "cmd: [bsteer_getdatarateinfo] \n");
    fprintf(stderr, "cmd: [tr069_get_fail_retrans] \n");
    fprintf(stderr, "cmd: [tr069_get_success_retrans] \n");
    fprintf(stderr, "cmd: [tr069_get_success_mul_retrans] \n");
    fprintf(stderr, "cmd: [tr069_get_ack_failures] \n");
    fprintf(stderr, "cmd: [tr069_get_retrans] \n");
    fprintf(stderr, "cmd: [tr069_get_aggr_pkts] \n");
    fprintf(stderr, "cmd: [tr069_get_sta_stats] [STA MAC] \n");
    fprintf(stderr, "cmd: [tr069_get_sta_bytes_sent] [STA MAC] \n");
    fprintf(stderr, "cmd: [tr069_get_sta_bytes_rcvd] [STA MAC] \n");
    fprintf(stderr, "cmd: [bsteer_setsteering] \n");
    fprintf(stderr, "cmd: [custom_chan_list] \n");
#if UMAC_SUPPORT_VI_DBG
    fprintf(stderr, "cmd: [vow_debug_set_param] \n");
    fprintf(stderr, "cmd: [vow_debug] \n");
#endif
    fprintf(stderr, "cmd: [setUnitTestCmd] \n");
    fprintf(stderr, "cmd: [setUnitTestCmdEvent] \n");
#if ATH_ACL_SOFTBLOCKING
    fprintf(stderr, "cmd: [softblocking] [STA MAC] [0/1] \n");
    fprintf(stderr, "cmd: [softblocking_get] [STA MAC] \n");
#endif
#ifdef WLAN_SUPPORT_TWT
    fprintf(stderr, "cmd: [ap_twt_add_dialog]\n");
    fprintf(stderr, "cmd: [ap_twt_del_dialog]\n");
    fprintf(stderr, "cmd: [ap_twt_pause_dialog]\n");
    fprintf(stderr, "cmd: [ap_twt_resume_dialog]\n");
#endif
    fprintf(stderr, "cmd: [bsscolor_collision_ap_period]\n");
    fprintf(stderr, "cmd: [bsscolor_color_change_announcemt_count]\n");
    fprintf(stderr, "cmd: [get_chan_survey]\n");
    fprintf(stderr, "cmd: [reset_chan_survey]\n");
    exit(-1);
}

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
static void usage_set_primary_chanlist(void)
{
    fprintf(stderr, "usage: wifitool athX setprimarychans ch1 ch2 .....chn \n");
}

static void usage_get_primary_chanlist(void)
{
    fprintf(stderr, "usage: wifitool athX getprimarychans \n");
}
#endif

static void usage_setchanlist(void)
{
    fprintf(stderr, "usage: wifitool athX setchanlist ch1 ch2 .....n \n");
}

static void usage_set_usr_ctrl_tbl(void)
{
    fprintf(stderr, "usage: wifitool athX set_usr_ctrl_tbl val1 val2 .....n \n");
    exit(-1);
}

static void usage_offchan_tx_test(void)
{
    fprintf(stderr, "usage: wifitool athX offchan_tx_test chan dwell_time \n");
    exit(-1);
}

static void usage_offchan_rx_test(void)
{
    fprintf(stderr, "usage: wifitool athX offchan_rx_test chan dwell_time bw_mode(optional) \n");
    exit(-1);
}

static void usage_getchanlist(void)
{
    fprintf(stderr, "usage: wifitool athX getchanlist \n");
}

static void usage_getrrmutil(void)
{
    fprintf(stderr, "usage: wifitool athX get_rrmutil\n");
}

static void usage_getrrrmstats(void)
{
    fprintf(stderr, "usage: wifitool athX get_rrmstats  [dstmac]\n");
    fprintf(stderr, "[dstmac] - stats reported by the given station\n");
}

static void usage_acsreport(void)
{
    fprintf(stderr, "usage: wifitool athX acsreport\n");
}

static void usage_sendfrmreq(void)
{
    fprintf(stderr, "usage: wifitool athX sendfrmreq  <dstmac> <n_rpts> <reg_class> <chnum> \n");
    fprintf(stderr, "<rand_invl> <mandatory_duration> <req_type> <ref mac> \n");
    exit(-1);
}

static void usage_sendlcireq(void)
{
    fprintf(stderr, "usage: wifitool athX sendlcireq  <dstmac> <location> <latitude_res> <longitude_res> \n");
    fprintf(stderr, "<altitude_res> [azimuth_res] [azimuth_type]\n");
    fprintf(stderr, "<dstmac> - MAC address of the receiving station \n");
    fprintf(stderr, "<location> - location of requesting/reporting station \n");
    fprintf(stderr, "<latitude_res> - Number of most significant bits(max 34) for fixed-point value of latitude \n");
    fprintf(stderr, "<longitude_res> - Number of most significant bits(max 34) for fixed-point value of longitude\n");
    fprintf(stderr, "<altitude_res> - Number of most significant bits(max 30) for fixed-point value of altitude\n");
    fprintf(stderr, "<azimuth_res> -  Number of most significant bits(max 9) for fixed-point value of Azimuth\n");
    fprintf(stderr, "<azimuth_type> - specifies report of azimuth of radio reception(0) or front surface(1) of reporting station\n");
    exit(-1);
}

static void usage_sendcca(void)
{
   fprintf(stderr, "usage: wifitool athX sendcca  <dstmac> <n_rpts> <chnum> <Meas_start_time> <Meas_duration> \n");
   exit(-1);
}

static void
usage_sendrpihist(void)
{
   fprintf(stderr, "usage: wifitool athX sendrpihist  <dstmac> <n_rpts> <chnum> <Meas_start_time> <Meas_duration> \n");
   exit(-1);
}

static void usage_sendchloadrpt(void)
{
    fprintf(stderr, "usage: wifitool athX sendchload  <dstmac> <n_rpts> <reg_class> <chnum> \n");
    fprintf(stderr, "<rand_invl> <mandatory_duration> <optional_condtion> <condition_val>\n");
    exit(-1);
}

static void usage_sendnhist(void)
{
    fprintf(stderr, "usage: wifitool athX sendnhist  <dstmac> <n_rpts> <reg_class> <chnum> \n");
    fprintf(stderr, "<rand_invl> <mandator_duration> <optional_condtion> <condition_val>\n");
    exit(-1);
}

static void usage_sendstastatsrpt(void)
{
    fprintf(stderr, "usage: wifitool athX sendstastats  <dstmac> <duration> <gid>\n");
    exit(-1);
}

static void usage_rrmstalist(void)
{
    fprintf(stderr, "usage: wifitool athX rrm_sta_list \n");
    exit(-1);
}

static void usage_btmstalist(void)
{
    fprintf(stderr, "usage: wifitool athX btm_sta_list \n");
    exit(-1);
}

static void usage_sendaddba(void)
{
    fprintf(stderr, "usage: wifitool athX sendaddba <aid> <tid> <buffersize>\n");
    exit(-1);
}

static void usage_senddelba(void)
{
    fprintf(stderr, "usage: wifitool athX senddelba <aid> <tid> <initiator> <reasoncode> \n");
    exit(-1);
}

static void usage_refusealladdbas(void)
{
    fprintf(stderr, "usage: wifitool athX refusealladdbas <enable> \n");
    exit(-1);
}

static void usage_sendsingleamsdu(void)
{
    fprintf(stderr, "usage: wifitool athX sendsingleamsdu <aid> <tid> \n");
    exit(-1);
}

static void usage_beeliner_fw_test(void)
{
    fprintf(stderr, "usage: wifitool athX beeliner_fw_test <arg> <value> \n");
    exit(-1);
}

static void usage_init_rtt3(void)
{
    fprintf(stderr, "usage: wifitool athX init_rtt3 <dstmac> <extra> ");
    exit(-1);
}

static void usage_getaddbastats(void)
{
    fprintf(stderr, "usage: wifitool athX setaddbaresp <aid> <tid> \n");
    exit(-1);
}

static void usage_sendbcnrpt(void)
{
    fprintf(stderr, "usage: wifitool athX sendbcnrpt <dstmac> <regclass> <channum> \n");
    fprintf(stderr, "       <rand_ivl> <duration> <mode> \n");
    fprintf(stderr, "       <req_ssid> <rep_cond> <rpt_detail>\n");
    fprintf(stderr, "       <req_ie> <chanrpt_mode> [specific_bssid]\n");
    fprintf(stderr, "       req_ssid = 1 for ssid, 2 for wildcard ssid \n");
    exit(-1);
}

static void usage_chmask_persta(void)
{
    fprintf(stderr, "usage: wifitool athX chmask_persta <mac_addr> <nss> \n");
    exit(-1);
}

static void usage_peer_nss(void)
{
    fprintf(stderr, "usage: wifitool athX peer_nss <aid> <nss> \n");
    exit(-1);
}

static void usage_set_antenna_switch(void)
{
    fprintf(stderr, "usage: wifitool athX set_antenna_switch <ctrl_cmd_1> <ctrl_cmd_2> \n");
    exit(-1);
}

static void usage_sendtsmrpt(void)
{
    fprintf(stderr, "usage: wifitool athX sendtsmrpt <num_rpt> <rand_ivl> <meas_dur>\n");
    fprintf(stderr, "       <tid> <macaddr> <bin0-range> <trig_cond> \n");
    fprintf(stderr, "       <avg_err_thresh> <cons_err_thresh> <delay_thresh> <trig_timeout>\n");
    exit(-1);
}

static void usage_sendneigrpt(void)
{
    fprintf(stderr, "usage: wifitool athX sendneigrpt <mac_addr> <ssid> <dialog_token>  \n");
    exit(-1);
}

static void usage_sendlmreq(void)
{
    fprintf(stderr, "usage: wifitool athX sendlmreq <mac_addr> \n");
    exit(-1);
}

static void usage_setbssidpref(void)
{
    fprintf(stderr, "usage: wifitool athX setbssidpref <mac_addr> <pref_val> <operating class> <channel number> \n");
    exit(-1);
}

static void usage_sendbstmreq(void)
{
    fprintf(stderr, "usage: wifitool athX sendbstmreq <mac_addr> <candidate_list> <disassoc_timer> <validityItrv> [disassoc_imminent][bss_term_inc] \n");
    exit(-1);
}

static void usage_sendbstmreq_target(void)
{
    fprintf(stderr, "usage: wifitool athX sendbstmreq_target <mac_addr>\n[<candidate_bssid> <candidate channel> <candidate_preference> <operating class> <PHY type>...]\n");
    exit(-1);
}

static void usage_senddelts(void)
{
    fprintf(stderr, "usage: wifitool athX senddelts <mac_addr> <tid> \n");
    exit(-1);
}

static void usage_sendaddts(void)
{
    fprintf(stderr, "usage: wifitool athX sendaddts <mac_addr> <tid> <dir> <up>\
            <nominal_msdu> <mean_data_rate> <mean_phy_rate> <surplus_bw>\
            <uapsd-bit> <ack_policy> <max_burst_size>\n");
    exit(-1);
}

static void usage_tr069chanhist(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_chanhist\n");
}

static void usage_tr069_txpower(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_txpower Power(in percentage)\n");
}

static void usage_tr069_gettxpower(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_gettxpower \n");
}

static void usage_tr069_guardintv(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_guardintv time(in nanoseconds) (800 or 0 for auto)\n");
}

static void usage_tr069_get_guardintv(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_get_guardintv \n");
}

static void usage_tr069_getassocsta(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_getassocsta \n");
}

static void usage_tr069_gettimestamp(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_gettimestamp \n");
}

static void usage_tr069_getacsscan(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_getacsscan (1 to issue a scan and 0 to know the result of scan) \n");
}

static void usage_tr069_perstastatscount(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_persta_statscount \n");
}

static void usage_tr069_get11hsupported(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_get_11hsupported \n");
}

static void usage_tr069_getpowerrange(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_get_powerrange \n");
}

static void usage_tr069_chan_inuse(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_chan_inuse\n");
}

static void usage_tr069_setoprate(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_setoprate rate1,rate2, .....n \n");
}

static void usage_tr069_getoprate(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_getoprate \n");
}

static void usage_tr069_getposrate(void)
{
    fprintf(stderr, "usage: wifitool athX tr069_getposrate \n");
}

static void usage_tr069_getsupportedfrequency(void)
{
    fprintf(stderr, "usage: wifitool athX supported_freq \n");
}

static void usage_assoc_dev_watermark_time(void)
{
    fprintf(stderr, "usage: wifitool athX get_assoc_dev_watermark_time \n");
}

static void usage_bsteer_getparams(void)
{
    fprintf(stderr, "usage: wifitool athX bsteer_getparams \n");
}

static void usage_bsteer_setparams(void)
{
    fprintf(stderr, "usage: wifitool athX bsteer_setparams <inact_normal> <inact_overload> <util_sample_period>\n"
            "           <util_average_num_samples> <inactive_rssi_xing_high_threshold> <inactive_rssi_xing_low_threshold>\n"
            "           <low_rssi_crossing_threshold> <inact_check_period> <tx_rate_low_crossing_threshold>\n"
            "           <tx_rate_high_crossing_threshold> <rssi_rate_low_crossing_threshold> <rssi_rate_high_crossing_threshold>\n"
            "           <ap_steer_rssi_low_threshold> <interference_detection_enable> <client_classification_enable>\n");
}

static void usage_bsteer_getdbgparams(void)
{
    fprintf(stderr, "usage: wifitool athX bsteer_getdbgparams \n");
}

static void usage_bsteer_setdbgparams(void)
{
    fprintf(stderr, "usage: wifitool athX bsteer_setdbgparams "
            "<raw_chan_util_log_enable> "
            "<raw_rssi_log_enable> <raw_tx_rate_log_enable>\n");
}

static void usage_bsteer_enable(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_enable <enable_flag>\n");
}

static void usage_bsteer_enable_events(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_enable_events <enable_flag>\n");
}

static void usage_bsteer_setoverload(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_setoverload <overload_flag>\n");
}

static void usage_bsteer_getoverload(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_getoverload\n");
}

static void usage_bsteer_getrssi(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_getrssi <dstmac> <num_samples>\n");
}

static void usage_bsteer_setproberespwh(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_setproberespwh <dstmac> <enable_flag>\n");
}

static void usage_bsteer_getproberespwh(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_getproberespwh <dstmac>\n");
}

static void usage_bsteer_setauthallow(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_setauthallow <dstmac> <enable_flag>\n");
}

static void usage_bsteer_getdatarateinfo(void)
{
    fprintf(stderr, "Usage: wifitool athX bsteer_getdatarateinfo <dstmac>\n");
}

static void usage_bsteer_setsteering(void)
{
    fprintf(stderr, "Usage, wifitool athX bsteer_setsteering <dstmac> <steering_flag>\n");
}

static void usage_coex_cfg(void)
{
    fprintf(stderr, "usage: wifitool athX coex_cfg <type> [arg0] [arg1] ... [arg5]\n");
}

static void usage_set_innetwork_2g(void)
{
   fprintf(stderr, "usage: wifitool athX set_innetwork_2g [mac] channel\n");
}

static void usage_get_innetwork_2g(void)
{
   fprintf(stderr, "usage: wifitool athX get_innetwork_2g [0/1] [num_entries/channel]\n");
}

#if UMAC_SUPPORT_VI_DBG
static void usage_vow_debug(void)
{
    fprintf(stderr, "Usage, wifitool athX vow_debug <stream_num> <marker_num> <marker_offset> <marker_match>\n");
}

    static void
usage_vow_debug_set_param(void)
{
    fprintf(stderr, "Usage, wifitool athX vow_debug_set_param <num_stream> <num_marker> <rxseqnum> <rxseqshift> <rxseqmax> <time_offset>\n");
}
#endif

static void usage_display_traffic_statistics(void)
{
    fprintf(stderr, "usage: wifitool athX display_traffic_statistics \n");
}

static void usage_fw_unit_test(void)
{
    fprintf(stderr, "Usage, wifitool athX setUnitTestCmd[Event] <module_id> <num_args> <args_list>\n");
}

#if ATH_ACL_SOFTBLOCKING
static void usage_set_acl_softblocking(void)
{
    fprintf(stderr, "Usage: wifitool athX softblocking <dstmac> <enable_flag>\n");
}

static void usage_get_acl_softblocking(void)
{
    fprintf(stderr, "Usage: wifitool athX softblocking_get <dstmac>\n");
}
#endif

/*
 * Input an arbitrary length MAC address and convert to binary.
 * Return address size.
 */
typedef unsigned char uint8_t;

uint8_t delim_unix = ':';
uint8_t delim_win = '-';

#define IS_VALID(s)         (((s >= '0') && (s <= '9')) || ((s >= 'A') && (s <= 'F')) || ((s >= 'a') && (s <= 'f')))
#define TO_UPPER(s)         (((s >= 'a') && (s <= 'z')) ? (s - 32) : s)
#define IS_NUM(c)           ((c >= '0') && (c <= '9'))
#define CHAR_TO_HEX_INT(c)  (IS_NUM(c) ? (c - '0') : (TO_UPPER(c) - 55))

/*
 * wifitool_mac_aton: converts mac address string into integer array format
 * @str: pointer to mac string which is to be converted
 * @out: pointer to address for output to be stored
 * @len: length of the string to be converted
 * returns 0 for fail else len if success
 */
int wifitool_mac_aton(const char *str, unsigned char *out, int len)
{
    const uint8_t *tmp = NULL;
    int plen = 0;
    uint8_t delim;
    int num = 0;
    int flag_num_valid = 0;
    int ccnt = 0;

    while((*str == ' ') || (*str == '\t') || (*str == '\n')) {
        ++str;
    }

    tmp = (uint8_t *) str;

    while ((*tmp != delim_unix) && (*tmp != delim_win)) {
        ++tmp;
    }

    delim = *tmp;

    tmp = (uint8_t *)str;

    while (*tmp != '\0') {

        if (IS_VALID(*tmp) && (++ccnt < 3)) {
            num = (num * 16) + CHAR_TO_HEX_INT(*tmp);
            flag_num_valid = 1;
        } else if ((*tmp == delim) && (flag_num_valid)) {
            *out = num;
            out++;
            num = 0;
            plen++;
            ccnt = 0;
            flag_num_valid = 0;

            if (plen > len) {
                return 0;
            }
        } else {
            return 0;
        }
        tmp++;
    }

    if (*tmp == '\0') {
        *out = num;
        plen++;
    }

    if (plen == len) {
        return len;
    } else {
        return 0;
    }

}

/*
 * ba_opt: handles block ack aggregation streams
 *         currently supports: send_addba, send_delba, send_addbaresp, refusealladdbas, send_singleamsdu, get-addbastats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int ba_opt(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = {0};
    int arg_num, i, param_num, cmd;
    switch (req_cmd) {
        case IEEE80211_DBGREQ_SENDADDBA:
            cmd = IEEE80211_DBGREQ_SENDADDBA;
            arg_num = 6;
            usage_cmd = usage_sendaddba;
            param_num = 3;
            break;

        case IEEE80211_DBGREQ_SENDDELBA:
            cmd = IEEE80211_DBGREQ_SENDDELBA;
            arg_num = 7;
            usage_cmd = usage_senddelba;
            param_num = 4;
            break;

        case IEEE80211_DBGREQ_SETADDBARESP:
            cmd = IEEE80211_DBGREQ_SETADDBARESP;
            arg_num = 6;
            usage_cmd = usage_sendaddba;
            param_num = 3;
            break;

        case IEEE80211_DBGREQ_REFUSEALLADDBAS:
            cmd = IEEE80211_DBGREQ_REFUSEALLADDBAS;
            arg_num = 4;
            usage_cmd = usage_refusealladdbas;
            param_num = 1;
            break;

        case IEEE80211_DBGREQ_SENDSINGLEAMSDU:
            cmd = IEEE80211_DBGREQ_SENDSINGLEAMSDU;
            arg_num = 5;
            usage_cmd = usage_sendsingleamsdu;
            param_num = 2;
            break;

        case IEEE80211_DBGREQ_GETADDBASTATS:
            cmd = IEEE80211_DBGREQ_GETADDBASTATS;
            arg_num = 5;
            usage_cmd = usage_getaddbastats;
            param_num = 2;
            break;

        default:
            return -1;
    }
    if (argc < arg_num) {
        usage_cmd();
    } else {
        req.cmd = cmd;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        for(i = 0; i < param_num; i++) {
            req.data.param[i] = atoi(argv[i+3]);
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * bcnrpt_append_chan_report: parse the command and append AP channel report subelements
 *                            to the beacon report.
 * This is only valid for US Operating classes.
 * @bcnrpt: the beacon report to be filled with AP channel report subelements
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @offset: the start of channel number arguments
 * return -1 if any channel number provided is invalid; otherwise return 0 on success
 */
static int bcnrpt_append_chan_report(ieee80211_rrm_beaconreq_info_t* bcnrpt,
        int argc, char *argv[], int offset)
{
    /* For now only consider Operating class 1,2,3,4 and 12,
       as defined in Table E-1 in 802.11-REVmb/D12, November 2011 */
#define MAX_CHANNUM_PER_REGCLASS 11
    typedef enum {
        REGCLASS_1 = 1,
        REGCLASS_2 = 2,
        REGCLASS_3 = 3,
        REGCLASS_4 = 4,
        REGCLASS_12 = 12,

        REGCLASS_MAX
    } regclass_e;

    int num_chanrep = 0;
    regclass_e regclassnum = REGCLASS_MAX;
    struct {
        int numchans;
        int channum[MAX_CHANNUM_PER_REGCLASS];
    } regclass[REGCLASS_MAX] = {{0}};

    while (offset < argc) {
        int channum = atoi(argv[offset++]);
        switch (channum) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                regclassnum = REGCLASS_12;
                break;
            case 36:
            case 40:
            case 44:
            case 48:
                regclassnum = REGCLASS_1;
                break;
            case 52:
            case 56:
            case 60:
            case 64:
                regclassnum = REGCLASS_2;
                break;
            case 149:
            case 153:
            case 157:
            case 161:
                regclassnum = REGCLASS_3;
                break;
            case 100:
            case 104:
            case 108:
            case 112:
            case 116:
            case 120:
            case 124:
            case 128:
            case 132:
            case 136:
            case 140:
                regclassnum = REGCLASS_4;
                break;
            default:
                return -1;
        }

        if (regclass[regclassnum].numchans >= MAX_CHANNUM_PER_REGCLASS) {
            /* Must have duplicated entries, raise error to user */
            return -1;
        }

        regclass[regclassnum].channum[regclass[regclassnum].numchans] = channum;
        ++regclass[regclassnum].numchans;
    }

    for (regclassnum = REGCLASS_1; regclassnum < REGCLASS_MAX; regclassnum++) {
        if (regclass[regclassnum].numchans > 0) {
            /* Use global op class if specified in the bcnrpt */
            if ((bcnrpt->regclass == 81) || (bcnrpt->regclass == 115))
                bcnrpt->apchanrep[num_chanrep].regclass = bcnrpt->regclass;
            else
                bcnrpt->apchanrep[num_chanrep].regclass = regclassnum;

            bcnrpt->apchanrep[num_chanrep].numchans = regclass[regclassnum].numchans;
            int i;
            for (i = 0; i < regclass[regclassnum].numchans; i++) {
                bcnrpt->apchanrep[num_chanrep].channum[i] = regclass[regclassnum].channum[i];
            }
            if (++num_chanrep >= IEEE80211_RRM_NUM_CHANREP_MAX) {
                break;
            }
        }
    }

    bcnrpt->num_chanrep = num_chanrep;
    return 0;
}

/*
 * send_rpt: send beacon, tsm and neighbour report
 *           currently supports: sendbcnrpt, sendtsmrpt, sendneigrpt
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int send_rpt(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, arg_index, cmd;
    int chan_rptmode = 0;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_SENDBCNRPT:
            cmd = IEEE80211_DBGREQ_SENDBCNRPT;
            arg_num = 14;
            arg_index = 3;
            usage_cmd = usage_sendbcnrpt;
            break;

        case IEEE80211_DBGREQ_SENDTSMRPT:
            cmd = IEEE80211_DBGREQ_SENDTSMRPT;
            arg_num = 14;
            arg_index = 7;
            usage_cmd = usage_sendtsmrpt;
            break;

        case IEEE80211_DBGREQ_SENDNEIGRPT:
            cmd  = IEEE80211_DBGREQ_SENDNEIGRPT;
            arg_num = 5;
            arg_index = 3;
            usage_cmd = usage_sendneigrpt;
            break;

        default:
            return -1;
    }

    if (argc < arg_num) {
        usage_cmd();
    } else {
        ieee80211_rrm_beaconreq_info_t* bcnrpt = &req.data.bcnrpt;
        ieee80211_rrm_tsmreq_info_t *tsmrpt = &req.data.tsmrpt;
        ieee80211_rrm_nrreq_info_t *neigrpt = &req.data.neigrpt;
        req.cmd = cmd;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[arg_index], req.dstmac, 6)) {
            errx(1, "Invalid destination mac address");
            return -1;
        }

        switch (req_cmd) {
            case IEEE80211_DBGREQ_SENDBCNRPT:
                bcnrpt->regclass = atoi(argv[4]);
                bcnrpt->channum = atoi(argv[5]);
                bcnrpt->random_ivl = atoi(argv[6]);
                bcnrpt->duration = atoi(argv[7]);
                bcnrpt->mode = atoi(argv[8]);
                bcnrpt->req_ssid = atoi(argv[9]);
                bcnrpt->rep_cond = atoi(argv[10]);
                bcnrpt->rep_detail = atoi(argv[11]);
                bcnrpt->req_ie = atoi(argv[12]);
                chan_rptmode = atoi(argv[13]);
                if (argc < 15) {
                    memset(bcnrpt->bssid,0xff,IEEE80211_ADDR_LEN);
                } else {
                    if (!wifitool_mac_aton(argv[14], bcnrpt->bssid, 6)) {
                        errx(1, "Invalid BSSID");
                        return -1;
                    }
                }
                if (argc < 16) {
                    bcnrpt->lastind = 0;
                } else {
                    bcnrpt->lastind = atoi(argv[15]);
                }
                if (!chan_rptmode) {
                    bcnrpt->num_chanrep = 0;
                } else if (argc < 17) {
                    /* If no channel is provided, use pre-canned channel values */
                    if (bcnrpt->regclass == 81) {
                        /* Global op class 81 */
                        bcnrpt->num_chanrep = 1;
                        bcnrpt->apchanrep[0].regclass = bcnrpt->regclass;
                        bcnrpt->apchanrep[0].numchans = 2;
                        bcnrpt->apchanrep[0].channum[0] = 1;
                        bcnrpt->apchanrep[0].channum[1] = 6;
                    } else if (bcnrpt->regclass == 115) {
                        /* Global op class 115 */
                        bcnrpt->num_chanrep = 1;
                        bcnrpt->apchanrep[0].regclass = bcnrpt->regclass;
                        bcnrpt->apchanrep[0].numchans = 2;
                        bcnrpt->apchanrep[0].channum[0] = 36;
                        bcnrpt->apchanrep[0].channum[1] = 48;
                    } else {
                        bcnrpt->num_chanrep = 2;
                        bcnrpt->apchanrep[0].regclass = 12;
                        bcnrpt->apchanrep[0].numchans = 2;
                        bcnrpt->apchanrep[0].channum[0] = 1;
                        bcnrpt->apchanrep[0].channum[1] = 6;
                        bcnrpt->apchanrep[1].regclass = 1;
                        bcnrpt->apchanrep[1].numchans = 2;
                        bcnrpt->apchanrep[1].channum[0] = 36;
                        bcnrpt->apchanrep[1].channum[1] = 48;
                    }
                } else if (bcnrpt_append_chan_report(bcnrpt, argc, argv, 16) < 0) {
                    errx(1, "Invalid AP Channel Report channel number(s)");
                    usage_sendbcnrpt();
                    return -1;
                }
                break;

            case IEEE80211_DBGREQ_SENDTSMRPT:
                tsmrpt->num_rpt = atoi(argv[3]);
                tsmrpt->rand_ivl = atoi(argv[4]);
                tsmrpt->meas_dur = atoi(argv[5]);
                tsmrpt->tid = atoi(argv[6]);
                if (!wifitool_mac_aton(argv[7], tsmrpt->macaddr, 6)) {
                    errx(1, "Invalid mac address");
                    return -1;
                }
                tsmrpt->bin0_range = atoi(argv[8]);
                tsmrpt->trig_cond = atoi(argv[9]);
                tsmrpt->avg_err_thresh = atoi(argv[10]);
                tsmrpt->cons_err_thresh = atoi(argv[11]);
                tsmrpt->delay_thresh = atoi(argv[12]);
                tsmrpt->trig_timeout = atoi(argv[13]);
                memcpy(req.dstmac, tsmrpt->macaddr, 6);
                break;

            case IEEE80211_DBGREQ_SENDNEIGRPT:
                memset((char *)neigrpt->ssid, '\0', sizeof(neigrpt->ssid));
                if (strlcpy((char *)neigrpt->ssid, argv[4], sizeof(neigrpt->ssid)) >= sizeof (neigrpt->ssid)) {
                    errx(1, "ssid string length is too long\n");
                    return -1;
                }

                neigrpt->ssid[sizeof(neigrpt->ssid)-1] = '\0';
                neigrpt->ssid_len = strlen((char *)neigrpt->ssid);
                neigrpt->dialogtoken = atoi(argv[5]);
                break;

            default:
                return -1;
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * set_bssidpref: set bssid preferences for mac address, preference value, operating class, channel number
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int set_bssidpref(const char *ifname,int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };

    if (argc < 7) {
        usage_setbssidpref();
    } else {
        struct ieee80211_user_bssid_pref* userpref = &req.data.bssidpref;
        req.cmd = IEEE80211_DBGREQ_MBO_BSSIDPREF;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (strcmp( argv[3],"0") == 0) {
            userpref->pref_val = 0;
        } else {
            if (!wifitool_mac_aton(argv[3], userpref->bssid, 6)) {
                errx(1, "Invalid mac address entered");
                return -1;
            }
            userpref->pref_val = (u_int8_t)(atoi(argv[4]));
            userpref->regclass = (u_int8_t)(atoi(argv[5]));
            userpref->chan = (u_int8_t)(atoi(argv[6]));
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * send_req: specify parameters set during the sending of LM and BTM request frames
 *           currently supports: send_lmreq, send_bstmreq
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int send_req (const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = {0};
    int arg_num, cmd;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_SENDLMREQ:
            cmd = IEEE80211_DBGREQ_SENDLMREQ;
            arg_num = 4;
            usage_cmd = usage_sendlmreq;
            break;

        case IEEE80211_DBGREQ_SENDBSTMREQ:
            cmd = IEEE80211_DBGREQ_SENDBSTMREQ;
            arg_num = 7;
            usage_cmd = usage_sendbstmreq;
            break;

        default:
            return -1;
    }
    if (argc < arg_num) {
        usage_cmd();
    } else {
        struct ieee80211_bstm_reqinfo* reqinfo = &req.data.bstmreq;
        req.cmd = cmd;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], req.dstmac, 6)) {
            errx(1, "Invalid destination mac address");
            return -1;
        }
        if (req_cmd == IEEE80211_DBGREQ_SENDBSTMREQ) {
            reqinfo->dialogtoken = 1;
            reqinfo->candidate_list = atoi(argv[4]);
            reqinfo->disassoc_timer = atoi(argv[5]);
            reqinfo->validity_itvl = atoi(argv[6]);
            reqinfo->bssterm_inc = 0;
            reqinfo->disassoc = 0;
            reqinfo->bssterm_tsf = 0;
            reqinfo->bssterm_duration = 0;

            if (argc > 7) {
                reqinfo->disassoc = atoi(argv[7]);
            }
            if (argc > 8) {
                reqinfo->bssterm_inc = atoi(argv[8]);
            }
            if (argc > 9) {
                reqinfo->bssterm_tsf = atoi(argv[9]);
            }
            if (argc > 10) {
                reqinfo->bssterm_duration = atoi(argv[10]);
            }
            if (argc > 11) {
                reqinfo->abridged = atoi(argv[11]);
            }
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * send_bstmreq_target: specify parameters set during the sending of BTM request frame
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int send_bstmreq_target(const char *ifname, int argc, char *argv[])
{
    int i;
    struct ieee80211req_athdbg req = { 0 };
    /* constants for convenient checking of arguments */
    static const u_int32_t fixed_length_args = 4;
    static const u_int32_t per_candidate_args = 5;

    if (argc < fixed_length_args) {
        usage_sendbstmreq_target();
    } else {
        struct ieee80211_bstm_reqinfo_target* reqinfo = &req.data.bstmreq_target;
        req.cmd = IEEE80211_DBGREQ_SENDBSTMREQ_TARGET;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], req.dstmac, MAC_ADDR_LEN)) {
            errx(1, "Invalid destination mac address");
            return -1;
        }
        reqinfo->dialogtoken = 1;

        /* check the number of arguments is appropriate to have the full complement
           of parameters for the command */
        if ((argc - fixed_length_args) % per_candidate_args) {
            usage_sendbstmreq_target();
            return -1;
        }

        reqinfo->num_candidates = (argc - fixed_length_args) / per_candidate_args;

        /* make sure the maximum number of candidates is not exceeded */
        if (reqinfo->num_candidates > ieee80211_bstm_req_max_candidates) {
            errx(1, "Invalid number of candidates: %d, maximum is %d",
                    reqinfo->num_candidates, ieee80211_bstm_req_max_candidates);
            return -1;
        }

        /* read the candidates */
        for (i = 0; i < reqinfo->num_candidates; i++) {
            if (!wifitool_mac_aton(argv[fixed_length_args + i * per_candidate_args],
                        reqinfo->candidates[i].bssid, MAC_ADDR_LEN)) {
                errx(1, "Candidate entry %d: Invalid BSSID", i);
                return -1;
            }
            reqinfo->candidates[i].channel_number = atoi(argv[fixed_length_args + i * per_candidate_args + 1]);
            reqinfo->candidates[i].preference = atoi(argv[fixed_length_args + i * per_candidate_args + 2]);
            reqinfo->candidates[i].op_class = atoi(argv[fixed_length_args + i * per_candidate_args + 3]);
            reqinfo->candidates[i].phy_type = atoi(argv[fixed_length_args + i * per_candidate_args + 4]);
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * send_ts_cmd: sends command for deletion and addition of Traffic Streams (TS)
 *              currently supports: send_delts, send_addts
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int send_ts_cmd (const char *ifname, int argc, char *argv[], int req_cmd)
{
    int arg_num, cmd;
    struct ieee80211req_athdbg req = {0};

    switch (req_cmd) {
        case IEEE80211_DBGREQ_SENDDELTS:
            cmd = IEEE80211_DBGREQ_SENDDELTS;
            arg_num = 5;
            usage_cmd = usage_senddelts;
            break;

        case IEEE80211_DBGREQ_SENDADDTSREQ:
            cmd = IEEE80211_DBGREQ_SENDADDTSREQ;
            arg_num = 13;
            usage_cmd = usage_sendaddts;
            break;

        default:
            return 0;
    }
    if (argc < arg_num) {
        usage_senddelts();
    } else {
        ieee80211_tspec_info* tsinfo = &req.data.tsinfo;
        req.cmd = cmd;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], req.dstmac, 6)) {
            errx(1, "Invalid destination mac address");
            return -1;
        }

        switch (req_cmd) {
            case IEEE80211_DBGREQ_SENDDELTS:
                req.data.param[0] = atoi(argv[4]);
                break;

            case IEEE80211_DBGREQ_SENDADDTSREQ:
                tsinfo->tid = atoi(argv[4]);
                tsinfo->direction = atoi(argv[5]);
                tsinfo->dot1Dtag = atoi(argv[6]);
                tsinfo->norminal_msdu_size = atoi(argv[7]);
                tsinfo->mean_data_rate = atoi(argv[8]);
                tsinfo->min_phy_rate = atoi(argv[9]);
                tsinfo->surplus_bw = atoi(argv[10]);
                tsinfo->psb = atoi(argv[11]);
                tsinfo->ack_policy = atoi(argv[12]);
                tsinfo->max_burst_size = atoi(argv[13]);
                tsinfo->acc_policy_edca = 1;
                break;

            default:
                return -1;
        }
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

static void send_cca(const char *ifname, int argc, char *argv[])
{
    int s;
    struct iwreq iwr;
    struct ieee80211req_athdbg req;
    ieee80211_rrm_cca_info_t *cca = &req.data.cca;

    if ((argc != 8)) {
        usage_sendcca();
    }
    else {
        memset(&req, 0, sizeof(struct ieee80211req_athdbg));
        s = socket(AF_INET, SOCK_DGRAM, 0);
        (void) memset(&iwr, 0, sizeof(iwr));

        if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
            fprintf(stderr, "ifname too long: %s\n", ifname);
            close(s);
            return;
        }
        req.cmd = IEEE80211_DBGREQ_SENDCCA_REQ;
        if (!wifitool_mac_aton(argv[3], cca->dstmac, 6)) {
           errx(1, "Invalid mac address");
            return;
        }
        cca->num_rpts = atoi(argv[4]);
        cca->chnum = atoi(argv[5]);
        cca->tsf = atoll(argv[6]);
        cca->m_dur  = atoi(argv[7]);
        memcpy(req.dstmac, cca->dstmac, 6);
        iwr.u.data.pointer = (void *) &req;
        iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
        if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
            errx(1, "IEEE80211_IOCTL_DBGREQ: IEEE80211_DBGREQ_SENDCCA failed");
        }
        close(s);
    }
    return;
}

static void
send_rpihistogram(const char *ifname, int argc, char *argv[])
{
    int s;
    struct iwreq iwr;
    struct ieee80211req_athdbg req;
    ieee80211_rrm_rpihist_info_t *rpihist = &req.data.rpihist;

    if ((argc != 8)) {
        usage_sendrpihist();
    }
    else {
        memset(&req, 0, sizeof(struct ieee80211req_athdbg));
        s = socket(AF_INET, SOCK_DGRAM, 0);
        (void) memset(&iwr, 0, sizeof(iwr));

        if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
            fprintf(stderr, "ifname too long: %s\n", ifname);
            close(s);
            return;
        }
        req.cmd = IEEE80211_DBGREQ_SENDRPIHIST;
        if (!wifitool_mac_aton(argv[3], rpihist->dstmac, 6)) {
            errx(1, "Invalid mac address");
            return;
        }
        rpihist->num_rpts = atoi(argv[4]);
        rpihist->chnum = atoi(argv[5]);
        rpihist->tsf = atoll(argv[6]);
        rpihist->m_dur  = atoi(argv[7]);
        memcpy(req.dstmac, rpihist->dstmac, 6);
        iwr.u.data.pointer = (void *) &req;
        iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
        if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
            errx(1, "IEEE80211_IOCTL_DBGREQ: IEEE80211_DBGREQ_SENDRPIHISTT failed");
        }
        close(s);
    }
    return;
}

/*
 * send_chn_cmd: set parameters for noise histogram and channel loading
 *               currently supports: send_noisehistogram, send_chload
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int send_chn_cmd(const char *ifname, int argc, char *argv[], int req_cmd)
{
    int cmd;
    struct ieee80211req_athdbg req = { 0 };
    switch (req_cmd) {
        case IEEE80211_DBGREQ_SENDNHIST:
            cmd = IEEE80211_DBGREQ_SENDNHIST;
            usage_cmd = usage_sendnhist;
            break;

        case IEEE80211_DBGREQ_SENDCHLOADREQ:
            cmd = IEEE80211_DBGREQ_SENDCHLOADREQ;
            usage_cmd = usage_sendchloadrpt;
            break;

        default:
            return -1;
    }
    if ((argc < 9) || (argc > 11)) {
        usage_cmd();
    } else {
        ieee80211_rrm_nhist_info_t *nhist = &req.data.nhist;
        ieee80211_rrm_chloadreq_info_t * chloadrpt = &req.data.chloadrpt;
        req.cmd = cmd;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], nhist->dstmac, 6)) {
            errx(1, "Invalid mac address");
            return -1;
        }
        switch (req_cmd) {
            case IEEE80211_DBGREQ_SENDNHIST:
                nhist->num_rpts = atoi(argv[4]);
                nhist->regclass = atoi(argv[5]);
                nhist->chnum = atoi(argv[6]);
                nhist->r_invl = atoi(argv[7]);
                nhist->m_dur  = atoi(argv[8]);
                if(argc > 9 ) { /*optional element */
                    nhist->cond  = atoi(argv[9]);
                    nhist->c_val  = atoi(argv[10]);
                }
                break;

            case IEEE80211_DBGREQ_SENDCHLOADREQ:
                chloadrpt->num_rpts = atoi(argv[4]);
                chloadrpt->regclass = atoi(argv[5]);
                chloadrpt->chnum = atoi(argv[6]);
                chloadrpt->r_invl = atoi(argv[7]);
                chloadrpt->m_dur  = atoi(argv[8]);
                if(argc > 9 ) { /*optional element */
                    chloadrpt->cond  = atoi(argv[9]);
                    chloadrpt->c_val  = atoi(argv[10]);
                }
                break;

            default:
                return -1;
        }
        memcpy(req.dstmac, nhist->dstmac, 6);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * print_rrmstats: prints info related to Radio Resource Management (RRM) for currently present channels
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int print_rrmstats(FILE *fd, ieee80211_rrmstats_t *rrmstats,int unicast)
{
    u_int32_t chnum = 0, i;
    u_int8_t nullrpi[IEEE80211_RRM_RPI_SIZE] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    ieee80211_rrm_noise_data_t  *noise_dptr;
    ieee80211_rrm_rpi_data_t    *rpi_dptr;
    ieee80211_rrm_lci_data_t    *lci_info;
    ieee80211_rrm_statsgid0_t   *gid0;
    ieee80211_rrm_statsgid10_t  *gid10;
    ieee80211_rrm_statsgid1_t   *gid1;
    ieee80211_rrm_statsgidupx_t *gidupx;
    ieee80211_rrm_tsm_data_t    *tsmdata;
    ieee80211_rrm_lm_data_t     *lmdata;
    ieee80211_rrm_frmcnt_data_t *frmcnt;

    if (!unicast) {
        fprintf(fd, "Channel# Chan_load \tANPI\t\tIPI[0 - 11]");
        for (chnum = 0; chnum < IEEE80211_RRM_CHAN_MAX; chnum++)
        {
            if (rrmstats->noise_data[chnum].anpi != 0 || rrmstats->chann_load[chnum] != 0)
            {
                fprintf(fd,"\n");
                fprintf(fd ,"%d\t ",chnum);
                fprintf(fd ,"%d \t\t",rrmstats->chann_load[chnum]);
                fprintf(fd, "%d\t\t ",rrmstats->noise_data[chnum].anpi);
                noise_dptr = &rrmstats->noise_data[chnum];
                for (i = 0; i < 11; i++)
                {
                    fprintf(fd, "%d, ", noise_dptr->ipi[i]);
                }
            }
        }
        fprintf(fd,"\n");
    } else {
        for (chnum = 0; chnum < IEEE80211_RRM_CHAN_MAX;chnum++)
        {
            if ((memcmp(rrmstats->rpi_data[chnum].rpi,nullrpi,IEEE80211_RRM_RPI_SIZE) != 0) || rrmstats->cca_data[chnum].cca_busy != 0)
            {
                fprintf(fd, "CCA Busy information for channel %d:\n",chnum);
                fprintf(fd, "--------------------\n");
                fprintf(fd, "%d\t\t\n ",rrmstats->cca_data[chnum].cca_busy);
                rpi_dptr   = &rrmstats->rpi_data[chnum];
                fprintf(fd, "RPI Histogram information for channel %d:\n",chnum);
                fprintf(fd, "--------------------\n");
                for (i = 0; i < IEEE80211_RRM_RPI_SIZE; i++)
                {
                    fprintf(fd, "%d, ", rpi_dptr->rpi[i]);
                }
                fprintf(fd,"\n");
            }
        }
        lci_info = &rrmstats->ni_rrm_stats.ni_vap_lciinfo;
        fprintf(fd, "\n");
        fprintf(fd, "LCI local information :\n");
        fprintf(fd, "--------------------\n");
        fprintf(fd, "\t\t latitude %d.%d longitude %d.%d Altitude %d.%d\n", lci_info->lat_integ,
                lci_info->lat_frac, lci_info->alt_integ, lci_info->alt_frac,
                lci_info->alt_integ, lci_info->alt_frac);
        lci_info = &rrmstats->ni_rrm_stats.ni_rrm_lciinfo;
        fprintf(fd, "\n");
        fprintf(fd, "LCI local information :\n");
        fprintf(fd, "--------------------\n");
        fprintf(fd, "\t\t latitude %d.%d longitude %d.%d Altitude %d.%d\n", lci_info->lat_integ,
                lci_info->lat_frac, lci_info->alt_integ, lci_info->alt_frac,
                lci_info->alt_integ, lci_info->alt_frac);
        gid0 = &rrmstats->ni_rrm_stats.gid0;
        fprintf(fd, "GID0 stats: \n");
        fprintf(fd, "\t\t txfragcnt %d mcastfrmcnt %d failcnt %d rxfragcnt %d mcastrxfrmcnt %d \n",
                gid0->txfragcnt, gid0->mcastfrmcnt, gid0->failcnt,gid0->rxfragcnt,gid0->mcastrxfrmcnt);
        fprintf(fd, "\t\t fcserrcnt %d  txfrmcnt %d\n",  gid0->fcserrcnt, gid0->txfrmcnt);
        gid1 = &rrmstats->ni_rrm_stats.gid1;
        fprintf(fd, "GID1 stats: \n");
        fprintf(fd, "\t\t rty %d multirty %d frmdup %d rtsuccess %d rtsfail %d ackfail %d\n", gid1->rty, gid1->multirty,gid1->frmdup,
                gid1->rtsuccess, gid1->rtsfail, gid1->ackfail);
        for (i = 0; i < 8; i++)
        {
            gidupx = &rrmstats->ni_rrm_stats.gidupx[i];
            fprintf(fd, "dup stats[%d]: \n", i);
            fprintf(fd, "\t\t qostxfragcnt %d qosfailedcnt %d qosrtycnt %d multirtycnt %d\n"
                    "\t\t qosfrmdupcnt %d qosrtssuccnt %d qosrtsfailcnt %d qosackfailcnt %d\n"
                    "\t\t qosrxfragcnt %d qostxfrmcnt %d qosdiscadrfrmcnt %d qosmpdurxcnt %d qosrtyrxcnt %d \n",
                    gidupx->qostxfragcnt,gidupx->qosfailedcnt,
                    gidupx->qosrtycnt,gidupx->multirtycnt,gidupx->qosfrmdupcnt,
                    gidupx->qosrtssuccnt,gidupx->qosrtsfailcnt,gidupx->qosackfailcnt,
                    gidupx->qosrxfragcnt,gidupx->qostxfrmcnt,gidupx->qosdiscadrfrmcnt,
                    gidupx->qosmpdurxcnt,gidupx->qosrtyrxcnt);
        }
        gid10 = &rrmstats->ni_rrm_stats.gid10;
        fprintf(fd, "GID10 stats: \n");
        fprintf(fd, "\t\tap_avg_delay %d be_avg_delay %d bk_avg_delay %d\n"
                "vi_avg_delay %d vo_avg_delay %d st_cnt %d ch_util %d\n",
                gid10->ap_avg_delay,gid10->be_avg_delay,gid10->bk_avg_delay,
                gid10->vi_avg_delay,gid10->vo_avg_delay,gid10->st_cnt,gid10->ch_util);
        tsmdata = &rrmstats->ni_rrm_stats.tsm_data;
        fprintf(fd, "TSM data : \n");
        fprintf(fd, "\t\ttid %d brange %d mac:%02x:%02x:%02x:%02x:%02x:%02x tx_cnt %d\n",tsmdata->tid,tsmdata->brange,
                tsmdata->mac[0],tsmdata->mac[1],tsmdata->mac[2],tsmdata->mac[3],tsmdata->mac[4],tsmdata->mac[5],tsmdata->tx_cnt);
        fprintf(fd,"\t\tdiscnt %d multirtycnt %d cfpoll %d qdelay %d txdelay %d bin[0-5]: %d %d %d %d %d %d\n\n",
                tsmdata->discnt,tsmdata->multirtycnt,tsmdata->cfpoll,
                tsmdata->qdelay,tsmdata->txdelay,tsmdata->bin[0],tsmdata->bin[1],tsmdata->bin[2],
                tsmdata->bin[3],tsmdata->bin[4],tsmdata->bin[5]);
        lmdata = &rrmstats->ni_rrm_stats.lm_data;
        fprintf(fd, "Link Measurement information :\n");
        fprintf(fd, "\t\ttx_pow %d lmargin %d rxant %d txant %d rcpi %d rsni %d\n\n",
                lmdata->tx_pow,lmdata->lmargin,lmdata->rxant,lmdata->txant,
                lmdata->rcpi,lmdata->rsni);
        fprintf(fd, "Frame Report Information : \n\n");
        for (i = 0; i < 12; i++)
        {
            frmcnt = &rrmstats->ni_rrm_stats.frmcnt_data[i];
            fprintf(fd,"Transmitter MAC: %02x:%02x:%02x:%02x:%02x:%02x",frmcnt->ta[0], frmcnt->ta[1],frmcnt->ta[2],frmcnt->ta[3],frmcnt->ta[4],frmcnt->ta[5]);
            fprintf(fd," BSSID: %02x:%02x:%02x:%02x:%02x:%02x",frmcnt->bssid[0], frmcnt->bssid[1], frmcnt->bssid[2],\
                    frmcnt->bssid[3], frmcnt->bssid[4],frmcnt->bssid[5]);
            fprintf(fd," phytype %d arsni %d lrsni %d lrcpi %d antid %d frame count %d\n",
                    frmcnt->phytype,frmcnt->arcpi,frmcnt->lrsni,frmcnt->lrcpi,frmcnt->antid, frmcnt->frmcnt);
        }
    }
    return 0;
}



/*
 * print_channel_survey_stats: print channel survey stats
 * @arg: pointer to cfg80211_data
 *
 * Helper rotine: prints channel survey stats
 */
void print_channel_survey_stats (struct cfg80211_data *arg)
{
#if UMAC_SUPPORT_CFG80211
    char *vendata = arg->nl_vendordata;
    int datalen = arg->nl_vendordata_len;
    struct nlattr *attr_vendor[NL80211_ATTR_MAX_INTERNAL];
    u_int32_t num_elements = 0;
    size_t msg_len = 0;
    struct channel_stats *chan_stats = NULL;
    int index = 0;

    /* extract data from NL80211_ATTR_VENDOR_DATA attributes */
    nla_parse(attr_vendor, QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX,
            (struct nlattr *)vendata,
            datalen, NULL);

    if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]) {
        chan_stats = nla_data(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]);
    }

    if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH]) {
        msg_len = nla_get_u32(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH]);
    }

    if (chan_stats == NULL) {
        return;
    }
    num_elements =  msg_len / sizeof(*chan_stats);
    printf("Home channel survey stats msg_len: %d, num_elements: %d\n", msg_len, num_elements);
    printf("freq: %4d, rx_bss: %12llu, total: %12llu, tx: %12llu, "
            "rx: %12llu, busy: %12llu, busy_ext: %12llu\n",
            chan_stats->freq, chan_stats->bss_rx_cnt, chan_stats->cycle_cnt,
            chan_stats->tx_frm_cnt, chan_stats->rx_frm_cnt, chan_stats->clear_cnt,
            chan_stats->ext_busy_cnt);

    printf("Scan channel survey stats \n");
    chan_stats++;
    for (index = 1; index < num_elements; index++) {
        if (chan_stats->cycle_cnt) {
            printf("freq: %4d, rx_bss: %12llu, total: %12llu, tx: %12llu, "
                    "rx: %12llu, busy: %12llu, busy_ext: %12llu\n",
                    chan_stats->freq, chan_stats->bss_rx_cnt, chan_stats->cycle_cnt,
                    chan_stats->tx_frm_cnt, chan_stats->rx_frm_cnt, chan_stats->clear_cnt,
                    chan_stats->ext_busy_cnt);
        }
        chan_stats++;
    }
#endif
    return;
}

/*
 * reset_channel_survey_stats: collect channel survery stats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error or 0 for success
 */
static int reset_channel_survey_stats(const char *ifname, int argc, char *argv[])
{
    int err;
    struct ieee80211req_athdbg req = { 0 };
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data arg;
#endif
    req.cmd = IEEE80211_DBGREQ_RESET_SURVEY_STATS;

    if (sock_ctx.cfg80211) {
#if UMAC_SUPPORT_CFG80211
        arg.data = (void *)&req;
        arg.length = sizeof(req);
        arg.flags = 0;
        arg.parse_data = 0;
        arg.callback = NULL;
        err = wifi_cfg80211_send_generic_command(&(sock_ctx.cfg80211_ctxt),
            QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
            QCA_NL80211_VENDOR_SUBCMD_DBGREQ, ifname, (char*)&arg, arg.length);

        if (err < 0) {
            fprintf(stderr, "CFG80211 Command failed, status: %d\n", err);
        }
#endif
    } else {
        fprintf(stderr, "IOCTL get survey channel not supported\n");
        return -1;
    }

    return 0;
}

/*
 * get_channel_survey_stats: collect channel survery stats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error or 0 for success
 */
static int get_channel_survey_stats(const char *ifname, int argc, char *argv[])
{
    int err;
    struct ieee80211req_athdbg req = { 0 };
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data arg;
#endif
    req.cmd = IEEE80211_DBGREQ_GET_SURVEY_STATS;

    if (sock_ctx.cfg80211) {
#if UMAC_SUPPORT_CFG80211
        arg.data = (void *)&req;
        arg.length = sizeof(req);
        arg.flags = 0;
        arg.parse_data = 1;
        arg.callback = print_channel_survey_stats;
        err = wifi_cfg80211_send_generic_command(&(sock_ctx.cfg80211_ctxt),
            QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
            QCA_NL80211_VENDOR_SUBCMD_DBGREQ, ifname, (char*)&arg, arg.length);

        if (err < 0) {
            fprintf(stderr, "CFG80211 Command failed, status: %d\n", err);
        }
#endif
    } else {
        fprintf(stderr, "IOCTL get survey channel not supported\n");
        return -1;
    }

    return 0;
}

/*
 * print_rrm_bcn_report: print parameters for RRM beacon report
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
void print_rrm_bcn_report (struct cfg80211_data *arg)
{
#if UMAC_SUPPORT_CFG80211
    char *vendata = arg->nl_vendordata;
    int datalen = arg->nl_vendordata_len;
    struct nlattr *attr_vendor[NL80211_ATTR_MAX_INTERNAL];
    u_int32_t num_elements = 0;
    ieee80211_bcnrpt_t *bcnrpt = NULL;
    int index = 0;

    /* extract data from NL80211_ATTR_VENDOR_DATA attributes */
    nla_parse(attr_vendor, QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX,
            (struct nlattr *)vendata,
            datalen, NULL);

    if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]) {
        bcnrpt = nla_data(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]);
    }

    if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS]) {
        num_elements = nla_get_u32(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS]);
    }

    if (bcnrpt == NULL) {
        return;
    }
    for (index = 0; index < num_elements; index++) {
        printf(" \t%02x %02x %02x %02x %02x %02x\t %d \t %d \n",
                bcnrpt->bssid[0],bcnrpt->bssid[1],
                bcnrpt->bssid[2],bcnrpt->bssid[3],
                bcnrpt->bssid[4],bcnrpt->bssid[5],
                bcnrpt->chnum,bcnrpt->rcpi);
        bcnrpt++;
    }
#endif
    return;
}

/*
 * get_bcnrpt: collect and print parameters related to beacon frame
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_bcnrpt(const char *ifname, int argc, char *argv[])
{
    int err;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_bcnrpt_t *bcnrpt = NULL;
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data arg;
#endif
    ieee80211req_rrmstats_t *rrmstats_req;
    req.cmd = IEEE80211_DBGREQ_GETBCNRPT;

    printf("\t BSSID \t\t\tCHNUM\tRCPI \n");

    if (!sock_ctx.cfg80211) {
#if UMAC_SUPPORT_WEXT
        bcnrpt  = (ieee80211_bcnrpt_t *)malloc(sizeof(ieee80211_bcnrpt_t));

        if(NULL == bcnrpt) {
            fprintf(stderr, "insufficient memory \n");
            return -1;
        }
        rrmstats_req = &req.data.rrmstats_req;
        rrmstats_req->data_addr = (void *) bcnrpt;
        rrmstats_req->data_size = (sizeof(ieee80211_bcnrpt_t));
        rrmstats_req->index = 1;

        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        while(rrmstats_req->index) {
            if (send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ) < 0) {
                errx(1, "IEEE80211_IOCTL_DBGREQ: ieee80211_dbgreq_bcnrpt failed");
            }

            if (bcnrpt->more) {
                rrmstats_req->index++;
                printf(" \t%02x %02x %02x %02x %02x %02x\t %d \t %d \n",
                        bcnrpt->bssid[0],bcnrpt->bssid[1],
                        bcnrpt->bssid[2],bcnrpt->bssid[3],
                        bcnrpt->bssid[4],bcnrpt->bssid[5],
                        bcnrpt->chnum,bcnrpt->rcpi);
            } else {
                rrmstats_req->index = 0;
            }
        }

        free(bcnrpt);
#endif
   } else {
#if UMAC_SUPPORT_CFG80211
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        arg.data = (void *)&req;
        arg.length = sizeof(req);
        arg.flags = 0;
        arg.parse_data = 1;
        arg.callback = print_rrm_bcn_report;
        err = wifi_cfg80211_send_generic_command(&(sock_ctx.cfg80211_ctxt),
            QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
            QCA_NL80211_VENDOR_SUBCMD_DBGREQ, ifname, (char*)&arg, arg.length);

        if (err < 0) {
            fprintf(stderr, "CFG80211 Command failed, status: %d\n", err);
        }
#endif
    }
    return 0;
}

/*
 * get_rssi: RSSI stats reported for a given dest mac station
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_rssi(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = {0};
    if ((argc < 4) || (argc > 4)) {
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_GETRRSSI;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    memset(req.dstmac,0x00,IEEE80211_ADDR_LEN);

    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
        errx(1, "Invalid mac address");
        return -1;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    return 0;
}

/*
 * set_primary_allowed_chanlist: set primary channels
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
static int set_primary_allowed_chanlist(const char *ifname, int argc, char *argv[])
{
#define MIN_PARAM 4
    struct iwreq iwr;
    int s, i;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_primary_allowed_chanlist_t chanlist;
    u_int8_t *chan  = NULL ; /*user channel list */

    if ((argc < MIN_PARAM)) {
        usage_set_primary_chanlist();
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    (void) memset(&iwr, 0, sizeof(iwr));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(s);
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_SETPRIMARY_ALLOWED_CHANLIST;
    chan = (u_int8_t *) malloc(sizeof(u_int8_t) * (argc - 3));

    if (NULL == chan) {
        close(s);
        return -1;
    }
    chanlist.chan = chan;
    chanlist.n_chan = argc - 3;
    req.data.param[0] = (u_long)&chanlist; /*typecasting to avoid warning */

    for(i = 3;i < argc; i++)
        chan[i - 3] =  atoi(argv[i]);

    iwr.u.data.pointer = (void *) &req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));

    if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0)
    {
        errx(1, "ieee80211_ioctl_dbgreq: ieee80211_dbgreq_setprimary_allowed_chanlist failed");
        printf("error in ioctl \n");
    }

    free(chan);
    close(s);
#undef MIN_PARAM
    return 0;
}

/*
 * get_primary_allowed_chanlist: get and display primary allowed channels
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_primary_allowed_chanlist(const char *ifname, int argc, char *argv[])
{
#define MAX_PARAM  3
    struct iwreq iwr;
    int s, i;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_primary_allowed_chanlist_t chanlist;
    u_int8_t *chan  = NULL ; /*user channel list */

    if ((argc > MAX_PARAM)) {
        usage_get_primary_chanlist();
        return -1;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    (void) memset(&iwr, 0, sizeof(iwr));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(s);
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_GETPRIMARY_ALLOWED_CHANLIST;

    iwr.u.data.pointer = (void *) &req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
    chan = (u_int8_t *) malloc(sizeof(u_int8_t) * (255));
    if (chan == NULL) {
        close(s);
        return -1;
    }

    chanlist.chan = chan;
    chanlist.n_chan = 0;
    req.data.param[0] = (u_long)&chanlist; /*typecasting to avoid warning */

    if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
        errx(1, "ieee80211_ioctl_dbgreq: ieee80211_dbgreq_get_primary_chanlist failed");
        printf("error in ioctl \n");
        free(chan);
        close(s);
        return -1;
    }
    if(chanlist.n_chan == 0) {
        printf("Primary allowed channel list is empty \n");
    } else {
        printf("Allowed primary channel list \n");

        for(i = 0;i < chanlist.n_chan;i++)
            printf("%d    ",chanlist.chan[i]);

        printf("\n");
    }
    free(chan);
    close(s);
#undef MAX_PARAM
    return 0;
}
#endif

/*
 * set_usr_ctrl_tbl: set user control table
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int set_usr_ctrl_tbl(const char *ifname, int argc, char *argv[])
{
#define MIN_CTRL_PARAM_COUNT  4
    int i;
    ieee80211_user_ctrl_tbl_t  ctrl_tbl;
    u_int16_t array_len = 0;
    u_int8_t *ctrl_array  = NULL;
    struct ieee80211req_athdbg req = { 0 };

    array_len = argc - 3;
    if ((argc < MIN_CTRL_PARAM_COUNT)) {
        usage_set_usr_ctrl_tbl();
        return -1;
    }

    if (array_len > MAX_USER_CTRL_TABLE_LEN) {
        fprintf(stderr, "Number of arguments cannot exceed %d\n", MAX_USER_CTRL_TABLE_LEN);
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_SETSUSERCTRLTBL;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    ctrl_array = (u_int8_t *) malloc(sizeof(u_int8_t) * array_len);
    if (ctrl_array == NULL) {
        fprintf(stderr, "Could not allocate memory\n");
        return -1;
    }

    ctrl_tbl.ctrl_table_buff = ctrl_array;
    ctrl_tbl.ctrl_len = array_len;
    req.data.user_ctrl_tbl = (ieee80211_user_ctrl_tbl_t *) &ctrl_tbl;

    for(i = 3; i < argc; i++) {
        ctrl_array[i - 3] =  atoi(argv[i]);
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    free(ctrl_array);
    return 0;
#undef MIN_CTRL_PARAM_COUNT
}

/*
 * channel_loading_channel_list_cmd: set and get the list of valid channels for participating in channel loading algorithm
 *                                   currently supports: channel_loading_channel_list_set, channel_loading_channel_list_get
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
static int channel_loading_channel_list_cmd(const char *ifname, int argc, char *argv[], int req_cmd) {
#define MIN_PARAM  4
#define MAX_PARAM  3
    bool cond;
    int num_chn, reply, i, cmd;
    u_int8_t *chan  = NULL ; /*user channel list */
    struct ieee80211req_athdbg req = { 0 };

    switch (req_cmd) {
        case IEEE80211_DBGREQ_SETACSUSERCHANLIST:
            cond = (argc < MIN_PARAM);
            usage_cmd = usage_setchanlist;
            reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
            num_chn = 0;
            cmd = IEEE80211_DBGREQ_SETACSUSERCHANLIST;
            break;

        case IEEE80211_DBGREQ_GETACSUSERCHANLIST:
            cond = (argc > MAX_PARAM);
            usage_cmd = usage_getchanlist;
            reply = DBGREQ_REPLY_IS_REQUIRED;
            num_chn= ACS_MAX_CHANNEL_COUNT;
            cmd = IEEE80211_DBGREQ_GETACSUSERCHANLIST;
            break;

        default:
            return -1;
    }

    if (cond) {
        usage_cmd();
        return -1;
    }

    req.cmd = cmd;
    req.needs_reply = reply;
    chan = (u_int8_t *) &(req.data.user_chanlist.chan);
    req.data.user_chanlist.n_chan = num_chn;

    if (req.data.user_chanlist.n_chan > sizeof(req.data.user_chanlist.chan)) {
        fprintf(stderr, "error: %s: chan buff size: %u, num chans: %u\n", __func__,
                sizeof(req.data.user_chanlist.chan), req.data.user_chanlist.n_chan);
        return -1;
    }

    switch (req_cmd) {
        case IEEE80211_DBGREQ_SETACSUSERCHANLIST:
            for(i = 3;i < argc; i++)
                chan[i - 3] =  atoi(argv[i]);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            break;

        case IEEE80211_DBGREQ_GETACSUSERCHANLIST:
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            printf("Following list used in channel load report \n");
            for(i = 0;i < req.data.user_chanlist.n_chan;i++)
                printf(" %d ",req.data.user_chanlist.chan[i]);
            printf("\n");
            break;

        default:
            return -1;
    }
    return 0;
#undef MIN_PARAM
#undef MAX_PARAM
}

/*
 * print_rrm_sta_list: print RRM capable STA's MAC address
 * @arg: pointer to the argument of type struct cfg80211_data
 */
void print_rrm_sta_list (struct cfg80211_data *arg)
{
#if UMAC_SUPPORT_CFG80211
    u_int32_t len = arg->length;/*IEEE80211_ADDR_LEN*/
    u_int8_t *addr_list = arg->data;
    int index = 0;
    printf("\t RRM Capable station's Mac address ");
    for(index = 0;index < len; index++){
        if(!(index%6))
            printf("\n");
        printf("%02x:",addr_list[index]);
    }
    printf( "\n");
#endif
}

/*
 * print_btm_sta_list: print BTM capable STA's MAC address
 * @argc: pointer to the argument of type struct cfg80211_data
 */
void print_btm_sta_list (struct cfg80211_data *arg)
{
#if UMAC_SUPPORT_CFG80211
    u_int32_t len = arg->length; /*IEEE80211_ADDR_LEN*/
    u_int8_t *addr_list = arg->data;
    int index = 0;
    printf("\t BTM Capable station's Mac address ");
    for(index = 0;index < len; index++){
        if(!(index%6))
            printf("\n");
        printf("%02x:",addr_list[index]);
    }
    printf( "\n");
#endif
}

#define RRM_STA 0
#define BTM_STA 1

/*
 * get_sta_list: fetches RRM/BTM stations list information
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @sta_type: station type (RRM/BTM)
 * return -1 on error; otherwise return 0 on success
 */
static int get_sta_list(const char *ifname, int argc, char *argv[], int sta_type)
{
#define MAX_CLIENTS 256
#define MAX_PARAM 3
    struct ieee80211req_athdbg req = { 0 };
    int err = 0, sta_count = 0, bufflen = 0;
    unsigned char *addr_list = NULL;
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data arg;
#endif
    if (argc > MAX_PARAM)
    {
        if (sta_type == RRM_STA)
            usage_rrmstalist();
        else
            usage_btmstalist();
        return -1;
    }
    if (sta_type == RRM_STA)
        req.cmd = IEEE80211_DBGREQ_GET_RRM_STA_LIST;
    else
        req.cmd = IEEE80211_DBGREQ_GET_BTM_STA_LIST;

    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    memset(req.dstmac,0xff,IEEE80211_ADDR_LEN); /* Assigning broadcast address to get the STA count */

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if(req.data.param[0] > MAX_CLIENTS) {
        perror("can not display long list __investigate__\n");
        return -1;
    }

    sta_count = req.data.param[0];
    bufflen = sta_count * IEEE80211_ADDR_LEN;
    if (!sock_ctx.cfg80211) {
        addr_list = (u_int8_t *) malloc(bufflen);
        if(addr_list == NULL) {
            perror("Memory allocation failed  __investigate__\n");
            return -1;
        }
        req.data.param[1] = (uintptr_t)addr_list;
    } else {
        /* Explicitely set NULL pointer for cfg80211*/
        req.data.param[1] = (uintptr_t) NULL;
    }

    memset(req.dstmac,0x00,IEEE80211_ADDR_LEN);
    req.data.param[0] = (int) sta_count;
    if (sta_type == RRM_STA)
        err = send_command(&sock_ctx, ifname, &req, sizeof(req), print_rrm_sta_list, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    else
        err = send_command(&sock_ctx, ifname, &req, sizeof(req), print_btm_sta_list, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (err < 0) {
        fprintf(stderr, "Unable to fetch rrm/btm sta list\n");
        if (addr_list) {
            free(addr_list);
            return -1;
        }
    }
    if (addr_list) {
#if UMAC_SUPPORT_CFG80211
        arg.data = addr_list;
        arg.length = req.data.param[0] * IEEE80211_ADDR_LEN;
        if (sta_type == RRM_STA) {
            print_rrm_sta_list(&arg);
        } else {
            print_btm_sta_list(&arg);
        }
        free(addr_list);
#endif
    }
    return 0;
#undef MAX_PARAM
#undef MAX_CLIENTS
}

/*
 * rrm_sta_list: display the MAC address of the RRM capable STA
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */

static void inline rrm_sta_list(const char *ifname, int argc, char *argv[])
{
    get_sta_list(ifname, argc, argv, RRM_STA);
}

static void inline btm_sta_list(const char *ifname, int argc, char *argv[])
{
    get_sta_list(ifname, argc, argv, BTM_STA);
}


#if QCA_LTEU_SUPPORT
static int ret_frm_thd;
#endif

/*
 * display_traffic_statistics: function to get the traffic statistics like rssi,minimum rssi,maximum rssi and median rssi
 *                             of each connected node from the driver and display the values
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int display_traffic_statistics(const char *ifname, int argc, char *argv[])
{
#define MAX_PARAM 3
    struct ieee80211req_athdbg *temp, *req = NULL;
    int count = 0, bin_number = 0, traffic_rate,index;
    char *addr_list = NULL;
    ieee80211_noise_stats_t *noise_stats;
    ieee80211_node_info_t *node_info;
    int bin_index, bin_stats = 0 ;

    if (argc > MAX_PARAM) {
        usage_display_traffic_statistics();
        return -1;
    }

    if (!(req = (void *) calloc(1, sizeof(struct ieee80211req_athdbg)))) {
        printf("Memory allocation failed  __investigate__\n");
        return -1;
    }

    req->cmd = IEEE80211_DBGREQ_DISPLAY_TRAFFIC_STATISTICS;
    req->needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    node_info = &req->data.node_info;
    memset(req->dstmac, 0xff, IEEE80211_ADDR_LEN); /* Assigning broadcast address to get the STA count */

    if (send_command(&sock_ctx, ifname, req, sizeof(*req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ) < 0) {
        perror("display traffic_statistics  failed \n");
        goto MEMORY_FAIL;
    }

    count = node_info->count;
    bin_number = node_info->bin_number;
    traffic_rate = node_info->traf_rate;

    if (!(temp = (void *) realloc(req, (sizeof(*req) + (IEEE80211_ADDR_LEN * (count)) + (sizeof(ieee80211_noise_stats_t) * bin_number * count))))) {
        printf("Memory allocation failed  __investigate__\n");
        goto MEMORY_FAIL;
    }

    req = temp;
    req->cmd = IEEE80211_DBGREQ_DISPLAY_TRAFFIC_STATISTICS;
    req->needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    node_info = &req->data.node_info;
    node_info->count = count;
    node_info->bin_number = bin_number;
    memset(req->dstmac, 0x00, IEEE80211_ADDR_LEN);

    if (send_command(&sock_ctx, ifname, req, (sizeof(*req) + (IEEE80211_ADDR_LEN * (count)) +
                     (sizeof(ieee80211_noise_stats_t) * bin_number * count)),
                      NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ) < 0) {
        perror("Traffic statistics display failed \n");
        goto MEMORY_FAIL;
    }

    addr_list = (char *) ((char *) req + sizeof(*req));
    noise_stats = (ieee80211_noise_stats_t *) ((char *)addr_list + (IEEE80211_ADDR_LEN * (count)));

    count = (node_info->count > count) ?  count: node_info->count;
    bin_number = (node_info->bin_number > bin_number) ? bin_number : node_info->bin_number;
    traffic_rate = node_info->traf_rate;

    for (index = 0;index<(IEEE80211_ADDR_LEN*count);index++){
        if (index % IEEE80211_ADDR_LEN == 0) {
            printf("Mac address \t");
        }
        printf("%02x:",addr_list[index]);

        if (index % 6 == (IEEE80211_ADDR_LEN-1)) {
            printf("\n");
            for(bin_index = 0; (bin_index < bin_number) && bin_stats <(count * bin_number);bin_index++,bin_stats++)
            {
                printf(" At %d sec : NOISE value is %d MIN value is %d MAX value is %d  MEDIAN value is %d\n",(traffic_rate)*(bin_index+1), noise_stats[bin_stats].noise_value,noise_stats[bin_stats].min_value,noise_stats[bin_stats].max_value,noise_stats[bin_stats].median_value);
            }
        }
    }
#undef MAX_PARAM
MEMORY_FAIL:
    free(req);
    return 0;
}

/*
 * get_next_wireless_custom_event: parse event sent using wireless_send_event()
 * @event_v: pointer to event_v
 * return NULL on completion
 */
static void * get_next_wireless_custom_event(void *event_v)
{
#if QCA_LTEU_SUPPORT
    int sock, len, done, count;
    struct sockaddr_nl local = {0};
    struct sockaddr_nl from = {0};
    socklen_t fromlen;
    char buf[8192];
    char ifname[IFNAMSIZ];
    struct nlmsghdr *h;
    fd_set r;
    struct timeval to;
    int *event = (int *)event_v;

    sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (sock < 0) {
        perror("socket:\n");
        return NULL;
    }

    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind:\n");
        close(sock);
        return NULL;
    }

    memset(ifname, 0, sizeof(ifname));
    count = 0;

    do {
        done = 0;
        FD_ZERO(&r);
        FD_SET(sock, &r);
        to.tv_sec = 1;
        to.tv_usec = 0;

        if (select(sock + 1, &r, NULL, NULL, &to) < 0) {
            perror("select:\n");
            close(sock);
            return NULL;
        }

        if (ret_frm_thd) {
            close(sock);
            return NULL;
        }

        if (!FD_ISSET(sock, &r)) {
            continue;
        }

        fromlen = sizeof(from);
        len = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from, &fromlen);

        if (len < 0) {
            perror("recvfrom:\n");
            close(sock);
            return NULL;
        }

        h = (struct nlmsghdr *)buf;

        while (NLMSG_OK(h, len)) {
            if (h->nlmsg_type != RTM_NEWLINK) {
                h = NLMSG_NEXT(h, len);
                continue;
            }
            if (NLMSG_PAYLOAD(h, 0) >= sizeof(struct ifinfomsg)) {
                struct rtattr *attr = (struct rtattr *)((char *)NLMSG_DATA(h) + NLMSG_ALIGN(sizeof(struct ifinfomsg)));
                int attrlen = NLMSG_PAYLOAD(h, sizeof(struct ifinfomsg));
                int rlen = RTA_ALIGN(sizeof(struct rtattr));
                while (RTA_OK(attr, attrlen)) {
                    struct iw_event iwe;
                    char *start;
                    char *end;

                    if (attr->rta_type == IFLA_IFNAME) {
                        int ilen;
                        ilen = attr->rta_len - rlen;
                        if (ilen > sizeof(ifname) - 1) {
                            printf("Interface name exceeds length\n");
                            break;
                        }
                        memcpy(ifname, RTA_DATA(attr), ilen);
                        ifname[0] = '\0';
                    }

                    if (attr->rta_type != IFLA_WIRELESS) {
                        attr = RTA_NEXT(attr, attrlen);
                        continue;
                    }

                    start = ((char *)attr) + rlen;
                    end = start + (attr->rta_len - rlen);
                    while (start + IW_EV_LCP_LEN <= end) {
                        memcpy(&iwe, start, IW_EV_LCP_LEN);
                        if (iwe.len <= IW_EV_LCP_LEN) {
                            break;
                        }
                        if ((iwe.cmd == IWEVCUSTOM) || (iwe.cmd == IWEVGENIE)) {
                            char *pos = (char *)&iwe.u.data.length;
                            char *data = start + IW_EV_POINT_LEN;
                            memcpy(pos, start + IW_EV_LCP_LEN, sizeof(struct iw_event) - (pos - (char *)&iwe));
                            if (data + iwe.u.data.length <= end) {
                                if (*event == IEEE80211_EV_SCAN && iwe.u.data.flags == *event &&
                                        iwe.u.data.length >= sizeof(struct event_data_scan)) {
                                    struct event_data_scan *scan = (struct event_data_scan *)data;
                                    printf("Scan completion event:\n");
                                    printf("req_id=%d\n", scan->scan_req_id);
                                    printf("status=%d\n", scan->scan_status);
                                    done = 1;
                                } else if (*event == IEEE80211_EV_MU_RPT && iwe.u.data.flags == *event &&
                                        iwe.u.data.length >= sizeof(struct event_data_mu_rpt)) {
                                    int i;
                                    struct event_data_mu_rpt *mu_rpt = (struct event_data_mu_rpt *)data;
                                    printf("MU report event:\n");
                                    printf("req_id=%d\n", mu_rpt->mu_req_id);
                                    printf("channel=%d\n", mu_rpt->mu_channel);
                                    printf("status=%d\n", mu_rpt->mu_status);
                                    for (i = 0; i < (MU_MAX_ALGO-1); i++)
                                        printf("total_val[%d]=%d\n", i, mu_rpt->mu_total_val[i]);
                                    printf("num_bssid=%d\n", mu_rpt->mu_num_bssid);
                                    printf("actual_duration=%d\n", mu_rpt->mu_actual_duration);
                                    printf("mu_hidden_node=");
                                    for (i = 0; i < LTEU_MAX_BINS; i++)
                                        printf("%d ",mu_rpt->mu_hidden_node_algo[i]);
                                    printf("\n");

                                    printf("num_ta_entries=%d\n",mu_rpt->mu_num_ta_entries);

                                    for (i = 0; i < mu_rpt->mu_num_ta_entries; i++) {
                                        printf("TA_MU_entry[%d]= ",i);
                                        printf("device_type=");
                                        switch(mu_rpt->mu_database_entries[i].mu_device_type) {
                                            case 0:
                                                printf("AP ");
                                                break;
                                            case 1:
                                                printf("STA ");
                                                break;
                                            case 2:
                                                printf("SC_SAME_OP ");
                                                break;
                                            case 3:
                                                printf("SC_DIFF_OP ");
                                                break;
                                            default:
                                                printf("Unknown ");
                                        }
                                        printf("BSSID=%02x:%02x:%02x:%02x:%02x:%02x ",
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[0],
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[1],
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[2],
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[3],
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[4],
                                                mu_rpt->mu_database_entries[i].mu_device_bssid[5]);
                                        printf("TA_mac_address=%02x:%02x:%02x:%02x:%02x:%02x ",
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[0],
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[1],
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[2],
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[3],
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[4],
                                                mu_rpt->mu_database_entries[i].mu_device_macaddr[5]);
                                        printf("Average_duration=%d ",mu_rpt->mu_database_entries[i].mu_avg_duration);
                                        printf("Average_RSSI=%d ",mu_rpt->mu_database_entries[i].mu_avg_rssi);
                                        printf("MU_percent=%d\n",mu_rpt->mu_database_entries[i].mu_percentage);
                                    }
                                    done = 1;
                                }
                            }
                        }
                        start += iwe.len;
                    }
                    attr = RTA_NEXT(attr, attrlen);
                }
            }
            h = NLMSG_NEXT(h, len);
        }
        ++count;
    } while(!done && count <= MAX_NUM_TRIES);

    close(sock);
#endif
    return NULL;
}

/*
 * lteu_param: handle scan params / mu params.
 *             used for LTEU.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
static void lteu_param(const char *ifname, int argc, char *argv[])
{
#if QCA_LTEU_SUPPORT
    struct ieee80211req_athdbg req = { 0 };

    if (streq(argv[2], "rpt_prb_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_REPEAT_PROBE_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_rpt_prb_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_REPEAT_PROBE_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    } else if (streq(argv[2], "rest_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_REST_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_rest_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_REST_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    } else if (streq(argv[2], "idle_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_IDLE_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_idle_time")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_IDLE_TIME;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    } else if (streq(argv[2], "prb_delay")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_PROBE_DELAY;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_prb_delay")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_PROBE_DELAY;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    } else if (streq(argv[2], "mu_delay")) {
        req.cmd = IEEE80211_DBGREQ_MU_DELAY;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_mu_delay")) {
        req.cmd = IEEE80211_DBGREQ_MU_DELAY;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    } else if (streq(argv[2], "sta_tx_pow")) {
        req.cmd = IEEE80211_DBGREQ_WIFI_TX_POWER;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = 1;
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        }
    } else if (streq(argv[2], "g_sta_tx_pow")) {
        req.cmd = IEEE80211_DBGREQ_WIFI_TX_POWER;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        req.data.param[0] = 0;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);

    } else if (streq(argv[2], "prb_spc_int")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_PROBE_SPACE_INTERVAL;
        req.data.param[0] = 1;
#if 0
        if (argc >= 4) {
            req.data.param[1] = strtoul(argv[3], NULL, 10);
            if (ioctl(sock, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
                perror("ioctl:\n");
            }
        }
#endif
    } else if (streq(argv[2], "g_prb_spc_int")) {
        req.cmd = IEEE80211_DBGREQ_SCAN_PROBE_SPACE_INTERVAL;
        req.data.param[0] = 0;
        printf("%s: %u\n", argv[2], (unsigned int)req.data.param[1]);
    }

#endif
}

/*
 * lteu_cfg: wifitool athX lteu_cfg
 * [ -g 0|1 ] [ -n 1-10 ] [ -w 1-10 0-100+ ] [ -y 1-10 0-5000+ ] [ -t 1-10 (-110)-0+ ]
 * [ -o 10-10000 ] [ -f 0|1 ] [ -e 0|1 ] [ -h ]
 * -g : gpio start or not
 * -n : number of bins
 * -w : number of weights followed by the individual weights
 * -y : number of gammas followed by individual gammas
 * -t : number of thresholds followed by individual thresholds
 * -o : timeout
 * -f : 1 to use actual NF, 0 to use a hardcoded one
 * -e : 1 to include packets with PHY error code in MU computation, 0 to exclude them
 * -h : help
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
static void lteu_cfg(const char *ifname, int argc, char *argv[])
{
#if QCA_LTEU_SUPPORT
    struct ieee80211req_athdbg req = { 0 };
    int i, j, n, x, y;
    req.cmd = IEEE80211_DBGREQ_LTEU_CFG;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    req.data.lteu_cfg.lteu_gpio_start = 0;
    req.data.lteu_cfg.lteu_num_bins = LTEU_MAX_BINS;
    for (i = 0; i < LTEU_MAX_BINS; i++) {
        req.data.lteu_cfg.lteu_weight[i] = 49;
        req.data.lteu_cfg.lteu_thresh[i] = 90;
        req.data.lteu_cfg.lteu_gamma[i] = 51;
    }
    req.data.lteu_cfg.lteu_scan_timeout = 10;
    req.data.lteu_cfg.use_actual_nf = 0;
    req.data.lteu_cfg.lteu_cfg_reserved_1 = 1;
    argc -= 3;
    j = 3;

    while (argc) {
        if (!strcmp(argv[j], "-g")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 0 && x <= 1) {
                argc--;
                j++;
                req.data.lteu_cfg.lteu_gpio_start = x;
            }
        } else if (!strcmp(argv[j], "-n")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 1 && x <= 10) {
                argc--;
                j++;
                req.data.lteu_cfg.lteu_num_bins = x;
            }
        } else if (!strcmp(argv[j], "-w")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 1 && x <= 10) {
                argc--;
                j++;
                n = x;
                i = 0;
                while (n-- && argc) {
                    y = strtoul(argv[j], NULL, 10);
                    if (y >= 0 && y <= 100) {
                        argc--;
                        j++;
                        req.data.lteu_cfg.lteu_weight[i++] = y;
                    } else
                        break;
                }
                if (i != x) {
                    for (i = 0; i < LTEU_MAX_BINS; i++)
                        req.data.lteu_cfg.lteu_weight[i] = 49;
                }
            }
        } else if (!strcmp(argv[j], "-y")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 1 && x <= 10) {
                argc--;
                j++;
                n = x;
                i = 0;
                while (n-- && argc) {
                    y = strtoul(argv[j], NULL, 10);
                    if (y >= 0 && y <= 5000) {
                        argc--;
                        j++;
                        req.data.lteu_cfg.lteu_gamma[i++] = y;
                    } else
                        break;
                }
                if (i != x) {
                    for (i = 0; i < LTEU_MAX_BINS; i++)
                        req.data.lteu_cfg.lteu_gamma[i] = 51;
                }
            }
        } else if (!strcmp(argv[j], "-t")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 1 && x <= 10) {
                argc--;
                j++;
                n = x;
                i = 0;
                while (n-- && argc) {
                    y = strtoul(argv[j], NULL, 10);
                    if (y >= -110 && y <= 0) {
                        argc--;
                        j++;
                        req.data.lteu_cfg.lteu_thresh[i++] = y;
                    } else
                        break;
                }
                if (i != x) {
                    for (i = 0; i < LTEU_MAX_BINS; i++)
                        req.data.lteu_cfg.lteu_thresh[i] = 90;
                }
            }
        } else if (!strcmp(argv[j], "-o")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 10 && x <= 10000) {
                argc--;
                j++;
                req.data.lteu_cfg.lteu_scan_timeout = x;
            }
        } else if (!strcmp(argv[j], "-f")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x >= 0 && x <= 1) {
                argc--;
                j++;
                req.data.lteu_cfg.use_actual_nf = x;
            }
        } else if (!strcmp(argv[j], "-e")) {
            argc--;
            j++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[j], NULL, 10);
            if (x == 0 || x == 1) {
                argc--;
                j++;
                req.data.lteu_cfg.lteu_cfg_reserved_1 = x;
            }
        } else if (!strcmp(argv[j], "-h")) {
            argc--;
            j++;
            printf("wifitool athX lteu_cfg "
                    "[ -g 0|1 ] [ -n 1-10 ] [ -w 1-10 0-100+ ] [ -y 1-10 -0-5000+ ] "
                    "[ -t 1-10 (-110)-0+ ] [ -o 10-10000 ] [ -f 0|1 ] [ -e 0|1 ] [ -h ]\n");
            printf("-g : gpio start or not\n");
            printf("-n : number of bins\n");
            printf("-w : number of weights followed by the individual weights\n");
            printf("-y : number of gammas followed by the individual gammas\n");
            printf("-t : number of thresholds followed by the individual thresholds\n");
            printf("-o : timeout\n");
            printf("-f : 1 to use actual NF, 0 to use a hardcoded one\n");
            printf("-e : 1 to include erroneous packets in MU calculation, 0 to exclude\n");
            printf("-h : help\n");
            return;
        } else {
            argc--;
            j++;
        }
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
#endif
}

#if QCA_AIRTIME_FAIRNESS

static void atf_debug_nodestate(const char *ifname, int argc, char *argv[])
{
#if QCA_AIRTIME_FAIRNESS
#define ATH_NUM_TID 17
    struct iwreq iwr;
    int sock;
    struct ieee80211req_athdbg req;
    int i;
    struct atf_peerstate *atfpeerstate = NULL;

    if (argc < 4) {
        printf("usage: wifitool athX atf_debug_nodestate <mac addr>\n");
        return;
    }
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("open:\n");
        return;
    }
    memset(&iwr, 0, sizeof(iwr));
    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(sock);
        return;
    }
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    iwr.u.data.pointer = (void *)&req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_ATF_DUMP_NODESTATE;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    atfpeerstate = malloc(sizeof(struct atf_peerstate));
    if (!atfpeerstate) {
        printf("out of memory\n");
        close(sock);
        return;
    }
    memset(atfpeerstate, 0, sizeof(struct atf_peerstate));
    req.data.atf_dbg_req.ptr = atfpeerstate;
    req.data.atf_dbg_req.size = (sizeof(struct atf_peerstate));
    if (!wifitool_mac_aton(argv[3], req.dstmac, sizeof(req.dstmac))) {
        printf("invalid mac address\n");
        close(sock);
        return;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    printf(" ==========================\n");
    printf("   AC to Subgroup Mapping\n");
    printf(" ==========================\n");
    printf(" AC ID       Subgroup Name\n");
    for (i = 0; i < WME_NUM_AC; i++) {
        if (atfpeerstate->subgroup_name) {
            printf("   %d           %s \n", i, atfpeerstate->subgroup_name[i]);
        }
    }
    printf("\n");

    printf("Node state : 0x%08x\n", atfpeerstate->peerstate);
    for (i = 0; i < ATH_NUM_TID; i++)
    {
        if( (atfpeerstate->peerstate >> i) & 0x1)
        {
            printf("tid : %d paused \n", i);
        }
    }
    printf("ATF TID Bitmap: 0x%08x\n", atfpeerstate->atf_tidbitmap);
    close(sock);
#endif
}

/*
 * print_atf_stats: prints ATF stats
 * @arg: pointer to the argument of type struct cfg80211_data
 */
void print_atf_stats (struct cfg80211_data *arg)
{
#if UMAC_SUPPORT_CFG80211
    unsigned int num_stats, i;
    struct atf_stats *stats;

    num_stats = arg->length / sizeof(struct atf_stats);
    stats = (struct atf_stats *)arg->data;
    /* Driver sends the stats from latest to oldest order */
    for (i = 0; i < num_stats; i++) {
        printf("%8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u %8u\n",
                stats[i].timestamp, stats[i].tokens,
                stats[i].total, stats[i].act_tokens,
                stats[i].tokens_common, stats[i].act_tokens_common,
                stats[i].unused, stats[i].contribution,
                stats[i].tot_contribution, stats[i].borrow,
                stats[i].allowed_bufs, stats[i].max_num_buf_held,
                stats[i].min_num_buf_held, stats[i].pkt_drop_nobuf,
                stats[i].num_tx_bufs, stats[i].num_tx_bytes,
                stats[i].weighted_unusedtokens_percent,
                stats[i].raw_tx_tokens, stats[i].throughput);

    }
#endif
    return;
}

static void atf_get_group_subgroup(const char *ifname,
    struct atfgrouplist_table *list_group)
{
#if QCA_AIRTIME_FAIRNESS
    struct iwreq iwr;
    int sock;
    struct ieee80211req_athdbg req;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("open:\n");
        return;
    }
    memset(&iwr, 0, sizeof(iwr));
    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        goto end;
    }
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    iwr.u.data.pointer = (void *)&req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_ATF_GET_GROUP_SUBGROUP;
    memset(list_group, 0, sizeof(struct atfgrouplist_table));
    req.data.atf_dbg_req.ptr = list_group;
    req.data.atf_dbg_req.size = sizeof(struct atfgrouplist_table);
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
end:
    close(sock);

    return;
#endif
}
/*
 * atf_dump_debug: dump the history logged of ATF stats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int atf_dump_debug(const char *ifname, int argc, char *argv[])
{
#if QCA_AIRTIME_FAIRNESS
    struct iwreq iwr;
    int sock;
    int grp, sg, i;
    struct ieee80211req_athdbg req = { 0 };
    unsigned int size, id;
    u_int32_t *ptr;
    struct atf_stats *stats;
    struct atfgrouplist_table list_group;

    if (argc != 3) {
        printf("usage: wifitool athX atf_dump_debug\n");
        return -1;
    }

    /* get list of groups & subgroups configured */
    atf_get_group_subgroup(ifname, &list_group);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("open:\n");
        return -1;
    }

    for (grp = 0; grp < list_group.info_cnt; grp++) {
        for (sg = 0; sg < list_group.atf_list[grp].num_subgroup; sg++) {
            memset(&iwr, 0, sizeof(iwr));
            if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
                fprintf(stderr, "ifname too long: %s\n", ifname);
                close(sock);
                return -1;
            }
            memset(&req, 0, sizeof(struct ieee80211req_athdbg));
            iwr.u.data.pointer = (void *)&req;
            iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
            req.cmd = IEEE80211_DBGREQ_ATF_DUMP_DEBUG;
            size = 1024 * sizeof(struct atf_stats);
            ptr = malloc((sizeof(u_int32_t) * 2) + size);
            if (!ptr) {
                printf("out of memory\n");
                close(sock);
                return -1;
            }
            req.data.atf_dbg_req.ptr = ptr;
            req.data.atf_dbg_req.size = (sizeof(u_int32_t) * 2) + size;

            /* add the group & subgroup name for which stats are fetched */
            memset(ptr, 0, (sizeof(u_int32_t) * 2) + size);
            memcpy(((struct atf_stats *)ptr)->group_name, list_group.atf_list[grp].grpname, IEEE80211_NWID_LEN);
            memcpy(((struct atf_stats *)ptr)->subgroup_name, list_group.atf_list[grp].sg_table[sg].subgrpname, IEEE80211_NWID_LEN);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            if (!sock_ctx.cfg80211) {
                size = ptr[0] / sizeof(struct atf_stats);
                id = ptr[1];
                stats = (struct atf_stats *)&ptr[2];

                printf("====================\n");
                printf("Subgroup : %s stats\n", list_group.atf_list[grp].sg_table[sg].subgrpname);
                printf("====================\n");
                printf("                     total   actual                    total                         ");
                printf("max      min     nobuf   txbufs  txbytes                                        contribute\n");
                printf("    time    allot    allot    allot   unused  contrib  contrib   borrow    allow     held");
                printf("     held     drop     sent     sent        up     wup     raw-tok max-tput  level   TID-state\n");
                for (i = 0; i < size; i++) {
                    printf("%8u %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d %8d      %06x\n",
                            stats[id].timestamp, stats[id].tokens,
                            stats[id].total, stats[id].act_tokens,
                            stats[id].unused, stats[id].contribution,
                            stats[id].tot_contribution, stats[id].borrow,
                            stats[id].allowed_bufs, stats[id].max_num_buf_held,
                            stats[id].min_num_buf_held, stats[id].pkt_drop_nobuf,
                            stats[id].num_tx_bufs, stats[id].num_tx_bytes,
                            stats[id].unusedtokens_percent,
                            stats[id].weighted_unusedtokens_percent,
                            stats[id].raw_tx_tokens, stats[id].throughput,
                            stats[id].contr_level, stats[id].atftidstate);
                    id++;
                    id &= (size - 1);
                }
            }
            if (ptr) {
                free(ptr);
            }
        }
    }
    close(sock);
#endif
    return 0;
}

/*
 * atf_debug_size: change the ATF log buffer size.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int atf_debug_size(const char *ifname, int argc, char *argv[])
{
#if QCA_AIRTIME_FAIRNESS
    struct ieee80211req_athdbg req = { 0 };

    if (argc < 4) {
        printf("usage: wifitool athX atf_debug_size <size>\n");
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_ATF_DEBUG_SIZE;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    req.data.param[0] = strtoul(argv[3], NULL, 10);
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    printf("log size changed\n");
#endif
    return 0;
}
#endif

/*
 * scan:
 * wifitool athX ap_scan
 * [ -i 1-200 ] [ -c 1-32 36-165+ ] [ -p 0-100 ] [ -r 0-100 ]
 * [ -l 0-100 ] [ -t 0-100 ] [ -d 50-2000 ] [ -a 0|1 ] [ -w ] [ -h ]
 * -i : request id
 * -c : number of channels followed by IEEE number(s) of the channel(s)
 *      eg. to scan 36 and 40 #wifitool athX ap_scan -c 2 36 40
 * -p : scan repeat probe time
 * -r : scan rest time
 * -l : scan idle time
 * -t : scan probe delay
 * -d : channel time
 * -a : 1 for active scan, 0 for passive
 * -w : wait for wireless event
 * -h : help
 *
 * wifitool athX mu_scan
 * [ -i 1-200 ] [ -c 36-165 ] [ -t 1-15 ] [ -d 0-5000 ] [ -p 0-100 ] [ -b (-110)-0 ]
 * [ -s (-110)-0 ] [ -u (-110)-0 ] [ -m 00000-999999 ] [ -a 0-100 ] [ -w ] [ -h ]
 * -i : request id
 * -c : IEEE number of the channel
 * -t : algo(s) to use
 * -d : time
 * -p : LTEu Tx power
 * -b : RSSI threshold for AP
 * -s : RSSI threshold for STA
 * -u : RSSI threshold for SC
 * -m : the home PLMN ID is a 5 or 6 digit value
 * -a : alpha for num active bssid calc
 * -w : wait for wireless event
 * -h : help
 * currently supports: ap_scan, mu_scan
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 */
static void scan(const char *ifname, int argc, char *argv[], int req_cmd)
{
#if QCA_LTEU_SUPPORT
#define DEFAULT_PLMN_ID 0xFFFFFF

    struct ieee80211req_athdbg req = { 0 };
    int i, x, y, j, err, wait_event = 0;
    pthread_t thread;

    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    ret_frm_thd = 0;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_AP_SCAN:
            req.cmd = IEEE80211_DBGREQ_AP_SCAN;
            req.data.ap_scan_req.scan_req_id = 1;
            req.data.ap_scan_req.scan_num_chan = 2;
            req.data.ap_scan_req.scan_channel_list[0] = 36;
            req.data.ap_scan_req.scan_channel_list[1] = 149;
            req.data.ap_scan_req.scan_type = SCAN_PASSIVE;
            req.data.ap_scan_req.scan_duration = 1000;
            req.data.ap_scan_req.scan_repeat_probe_time = (u_int32_t)-1;
            req.data.ap_scan_req.scan_rest_time = (u_int32_t)-1;
            req.data.ap_scan_req.scan_idle_time = (u_int32_t)-1;
            req.data.ap_scan_req.scan_probe_delay = (u_int32_t)-1;
            argc -= 3;
            i = 3;
            while (argc) {
                if (!strcmp(argv[i], "-i")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 1 && x <= 200) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_req_id = x;
                    }
                } else if (!strcmp(argv[i], "-c")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 1 && x <= 32) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_num_chan = x;
                        j = 0;
                        while (x-- && argc) {
                            y = strtoul(argv[i], NULL, 10);
                            if (y >= 36 && y <= 165) {
                                argc--;
                                i++;
                                req.data.ap_scan_req.scan_channel_list[j++] = y;
                            } else
                                break;
                        }
                        if (j != req.data.ap_scan_req.scan_num_chan)
                            req.data.ap_scan_req.scan_num_chan = 0;
                    }
                } else if (!strcmp(argv[i], "-p")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_repeat_probe_time = x;
                    }
                } else if (!strcmp(argv[i], "-r")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_rest_time = x;
                    }
                } else if (!strcmp(argv[i], "-l")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_idle_time = x;
                    }
                } else if (!strcmp(argv[i], "-t")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_probe_delay = x;
                    }
                } else if (!strcmp(argv[i], "-d")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 50 && x <= 2000) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_duration = x;
                    }
                } else if (!strcmp(argv[i], "-a")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 1) {
                        argc--;
                        i++;
                        req.data.ap_scan_req.scan_type = x;
                    }
                } else if (!strcmp(argv[i], "-h")) {
                    argc--;
                    i++;
                    printf("wifitool athX ap_scan "
                            "[ -i 1-200 ] [ -c 1-32 36-165+ ] [ -p 0-100 ] [ -r 0-100 ] "
                            "[ -l 0-100 ] [ -t 0-100 ] [ -d 50-2000 ] [ -a 0|1 ] [ -w ] [ -h ]\n");
                    printf("-i : request id\n");
                    printf("-c : number of channels followed by IEEE number(s) of the channel(s)\n");
                    printf("     eg. to scan 36 and 40 #wifitool athX ap_scan -c 2 36 40\n");
                    printf("-p : scan repeat probe time\n");
                    printf("-r : scan rest time\n");
                    printf("-l : scan idle time\n");
                    printf("-t : scan probe delay\n");
                    printf("-d : channel time\n");
                    printf("-a : 1 for active scan, 0 for passive\n");
                    printf("-w : wait for wireless event\n");
                    printf("-h : help\n");
                    return;
                } else if (!strcmp(argv[i], "-w")) {
                    argc--;
                    i++;
                    wait_event = IEEE80211_EV_SCAN;
                } else {
                    argc--;
                    i++;
                }
            }

            break;

        case IEEE80211_DBGREQ_MU_SCAN:
            req.cmd = IEEE80211_DBGREQ_MU_SCAN;
            req.data.mu_scan_req.mu_req_id = 1;
            req.data.mu_scan_req.mu_channel = 100;
            req.data.mu_scan_req.mu_type = MU_ALGO_1 | MU_ALGO_2 | MU_ALGO_3 | MU_ALGO_4;
            req.data.mu_scan_req.mu_duration = 1000;
            req.data.mu_scan_req.lteu_tx_power = 10;
            req.data.mu_scan_req.mu_rssi_thr_bssid = 90;
            req.data.mu_scan_req.mu_rssi_thr_sta   = 90;
            req.data.mu_scan_req.mu_rssi_thr_sc    = 90;
            req.data.mu_scan_req.home_plmnid = DEFAULT_PLMN_ID;
            req.data.mu_scan_req.alpha_num_bssid = 50;
            argc -= 3;
            i = 3;
            while (argc) {
                if (!strcmp(argv[i], "-i")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 1 && x <= 200) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_req_id = x;
                    }
                } else if (!strcmp(argv[i], "-c")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 36 && x <= 165) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_channel = x;
                    }
                } else if (!strcmp(argv[i], "-t")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 1 && x <= 15) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_type = x;
                    }
                } else if (!strcmp(argv[i], "-d")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 5000) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_duration = x;
                    }
                } else if (!strcmp(argv[i], "-p")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.lteu_tx_power = x;
                    }
                } else if (!strcmp(argv[i], "-b")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= -110 && x <= 0) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_rssi_thr_bssid = x;
                    }
                } else if (!strcmp(argv[i], "-s")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= -110 && x <= 0) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_rssi_thr_sta = x;
                    }
                } else if (!strcmp(argv[i], "-u")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= -110 && x <= 0) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.mu_rssi_thr_sc = x;
                    }
                } else if (!strcmp(argv[i], "-m")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 16);
                    if (x > 0 && x <= DEFAULT_PLMN_ID) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.home_plmnid = x;
                    }
                } else if (!strcmp(argv[i], "-a")) {
                    argc--;
                    i++;
                    if (!argc) {
                        break;
                    }
                    x = strtoul(argv[i], NULL, 10);
                    if (x >= 0 && x <= 100) {
                        argc--;
                        i++;
                        req.data.mu_scan_req.alpha_num_bssid = x;
                    }
                } else if (!strcmp(argv[i], "-h")) {
                    argc--;
                    i++;
                    printf("wifitool athX mu_scan "
                            "[ -i 1-200 ] [ -c 36-165 ] [ -t 1-15 ] [ -d 0-5000 ] [ -p 0-100 ] "
                            "[ -b (-110)-0 ] [ -s (-110)-0 ] [ -u (-110)-0 ] [ -m 00000-999999 ] "
                            "[ -a 0-100 ] [ -w ] [ -h ]\n");
                    printf("-i : request id\n");
                    printf("-c : IEEE number of the channel\n");
                    printf("-t : algo(s) to use\n");
                    printf("-d : time\n");
                    printf("-p : LTEu Tx power\n");
                    printf("-b : RSSI threshold for AP\n");
                    printf("-s : RSSI threshold for STA\n");
                    printf("-u : RSSI threshold for SC\n");
                    printf("-m : the home PLMN ID\n");
                    printf("-a : alpha for num active bssid calc\n");
                    printf("-w : wait for wireless event\n");
                    printf("-h : help\n");
                    return;
                } else if (!strcmp(argv[i], "-w")) {
                    argc--;
                    i++;
                    wait_event = IEEE80211_EV_MU_RPT;
                } else {
                    argc--;
                    i++;
                }
            }
            break;

        default:
            return;
    }

    if (pthread_create(&thread, NULL, get_next_wireless_custom_event, &wait_event)) {
        printf("can't create thread\n");
        wait_event = 0;
    }

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    if (err < 0 && wait_event) {
        ret_frm_thd = 1;
    }

    if (wait_event) {
        pthread_join(thread, NULL);
    }
#undef DEFAULT_PLMN_ID
#endif
}

/*
 * block_acs_channel: api from user land to set block channel list to driver
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
static void block_acs_channel(const char *ifname, int argc, char *argv[])
{
    int count = 0,temph = 0,cnt = 0,i = 0;
    u_int8_t channel[ACS_MAX_CHANNEL_COUNT] = { 0 }, valid[ACS_MAX_CHANNEL_COUNT];
    char *p = NULL;
    struct ieee80211req_athdbg req = { 0 };
    u_int8_t *chan  = NULL ; /*user channel list */

    if ((argc < 4)) {
        fprintf(stderr, "usage: wifitool athX block_acs_channel ch1.....chN\n");
        fprintf(stderr, "usage: wifitool athX block_acs_channel channels must be comma separated \n");
        return;
    }
    req.cmd = IEEE80211_DBGREQ_BLOCK_ACS_CHANNEL;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    p = argv[3];
    while(*p != '\0') {
        while( *p != ',' && (*p != '\0'))
        {
            sscanf(p, "%1d", &temph);
            valid[i] = temph;
            p++;
            i++;
        }
        if(i) {
            for( cnt = 0;cnt < i;cnt++) {
                channel[count] = channel[count] + valid[cnt]*power(10,(i-cnt-1));
            }
            count++;
            i = 0;
            if(*p == '\0')
                break;
            else
                p++; /*by pass commma */
        }
        if(count >= ACS_MAX_CHANNEL_COUNT) {
            count = ACS_MAX_CHANNEL_COUNT;
            break;
        }
    }
    if(count) {
        chan = (u_int8_t *)(req.data.user_chanlist.chan);
        req.data.user_chanlist.n_chan = count;
        if (req.data.user_chanlist.n_chan > sizeof(req.data.user_chanlist.chan)) {
            fprintf(stderr, "error: %s: chan buff size: %u, num chans: %u\n", __func__,
                    sizeof(req.data.user_chanlist.chan), req.data.user_chanlist.n_chan);
            return;
        }
        memcpy(chan,channel,count);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        if(!chan[0]) {
            printf("List Flushed \n");
        } else {
            printf("Following channels are blocked from Channel selection algorithm  \n");
            for(i = 0;i < count; i++) {
                printf("[%d] ",channel[i]);
            }
            printf("\n");
        }

    } else  {
        perror("Invalid channel list \n");
        return;
    }

    return;
}

/*
 * acs_report: print scanned channels and its various parameters in acs report
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int acs_report(const char *ifname, int argc, char *argv[])
{
    int err, i, j, ret;
    struct ieee80211req_athdbg req;
    struct ieee80211_acs_dbg *acs = NULL;
    if ((argc < 3) || (argc > 3))
    {
        usage_acsreport();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    acs = (void *)malloc(sizeof(struct ieee80211_acs_dbg));

    if(NULL == acs) {
        printf("insufficient memory \n");
        goto cleanup;
    }

    memset(acs, 0, sizeof(struct ieee80211_acs_dbg));

    req.data.acs_rep.data_addr = acs;
    req.data.acs_rep.data_size = sizeof(struct ieee80211_acs_dbg);
    req.data.acs_rep.index = 0;
    acs->entry_id = 0;
    acs->acs_type = ACS_CHAN_STATS;
    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (err < 0) {
        return err;
    }

    fprintf(stdout,"\n\nLegend:\n"
            "SC: Secondary Channel            WR: Weather Radar\n"
            "DFS: DFS Channel                 HN: High Noise\n"
            "RS: Low RSSI                     CL: High Channel Load\n"
            "RP: Regulatory Power             N2G: Not selected 2G\n"
            "P80X: Primary 80X80              NS80X: Only for primary 80X80\n"
            "NP80X: Only for Secondary 80X80  SR: Spacial reuse\n\n");

    fprintf(stdout," The number of channels scanned for acs report is:  %d \n\n",acs->nchans);
    fprintf(stdout," Channel | BSS  | minrssi | maxrssi | NF | Ch load | spect load | sec_chan | SR bss | SR load | Ranking | Unused  | Radar Noise | Chan in Pool \n");
    fprintf(stdout,"-----------------------------------------------------------------------------------------------------------------\n");

    /* output the current configuration */
    for (i = 0; i < acs->nchans; i++) {
        acs->entry_id = i;
        acs->acs_type = ACS_CHAN_STATS;
        req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

        /*To make sure we are not getting more than 100 %*/
        if(acs->chan_load  > 100)
            acs->chan_load = 100;

        fprintf(stdout," %4d(%3d) %4d     %4d      %4d   %4d    %4d        %4d       %4d     %4d      %4d       %4d    %s  %4d   %4d\n",
                acs->chan_freq,
                acs->ieee_chan,
                acs->chan_nbss,
                acs->chan_minrssi,
                acs->chan_maxrssi,
                acs->noisefloor,
                acs->chan_load,
                acs->channel_loading,
                acs->sec_chan,
                acs->chan_nbss_srp,
                acs->chan_srp_load,
                acs->acs_rank.rank,
                acs->acs_rank.desc,
                acs->chan_radar_noise,
                acs->chan_in_pool);
    }

    switch(acs->acs_status) {
        case ACS_SUCCESS:
            fprintf(stdout,"ACS_SUCCESS: Current channel is selected by ACS algorithm \n");
            break;
        case ACS_FAILED_NBSS:
            fprintf(stdout,"ACS_FAILED_NBSS: Current channel is selected based on the min bss count \n");
            break;
        case ACS_FAILED_RNDM:
            fprintf(stdout,"ACS_FAILED_RNDM: Current channel is selected by random channel selection \n");
            break;
        default:
            break;
    }

    fprintf(stdout,"-----------------------------------------------------------------------------------------\n");
    fprintf(stdout, "Additonal Channel Statistics \n");
    fprintf(stdout,"-----------------------------------------------------------------------------------------\n");
    fprintf(stdout," Index | Channel | NBSS |         SSID       |             BSSID       |    RSSI | PHYMODE   \n");
    fprintf(stdout,"-----------------------------------------------------------------------------------------\n");
    for (i = 0; i < acs->nchans; i++) {
        acs->entry_id = i;
        acs->acs_type = ACS_NEIGHBOUR_GET_LIST_COUNT;
        req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        ret = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        if (ret < 0) {
            fprintf(stdout, "Get neighbor list count failed\n");
            goto cleanup;
        }

        if (acs->chan_nbss) {
            acs->neighbor_list = (void *) malloc (sizeof(ieee80211_neighbor_info) * acs->chan_nbss);

            if(!acs->neighbor_list) {
                fprintf(stdout, "Memory allocation failed\n");
                goto cleanup;
            }

            memset(acs->neighbor_list, 0, sizeof(ieee80211_neighbor_info) * acs->chan_nbss);
            acs->neighbor_size = sizeof(ieee80211_neighbor_info) * acs->chan_nbss;
            acs->entry_id = i;
            acs->acs_type = ACS_NEIGHBOUR_GET_LIST;
            req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
            req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
            ret = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            if (ret < 0) {
                perror("DBG req failed");
                goto cleanup;
            }

            for (j = 0; j <  acs->chan_nbss; j++) {
                fprintf(stdout,"%-10d%-10d%-10d%-25s  %02x:%02x:%02x:%02x:%02x:%02x%5d%7d\n",
                        j+1,
                        acs->ieee_chan,
                        acs->chan_nbss,
                        acs->neighbor_list[j].ssid,
                        acs->neighbor_list[j].bssid[0],
                        acs->neighbor_list[j].bssid[1],
                        acs->neighbor_list[j].bssid[2],
                        acs->neighbor_list[j].bssid[3],
                        acs->neighbor_list[j].bssid[4],
                        acs->neighbor_list[j].bssid[5],
                        -acs->neighbor_list[j].rssi,
                        acs->neighbor_list[j].phymode);
            }

            free(acs->neighbor_list);
            acs->neighbor_list = NULL;
        }
    }

cleanup:
    if (acs) {
        if (acs->neighbor_list) {
            free(acs->neighbor_list);
        }
        free(acs);
    }
    return 0;
}

/*
 * get_rrmstats: collects information for RRM stats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_rrmstats(const char *ifname, int argc, char *argv[])
{
    int err, unicast=0;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211req_rrmstats_t *rrmstats_req;
    ieee80211_rrmstats_t *rrmstats = NULL;

    if ((argc < 3) || (argc > 4)) {
        usage_getrrrmstats();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_GETRRMSTATS;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    memset(req.dstmac,0x00,IEEE80211_ADDR_LEN);
    if (argc == 4) {
        unicast = 1;
        if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
            errx(1, "Invalid mac address");
            return -1;
        }
    }

    rrmstats = (ieee80211_rrmstats_t *)(malloc(sizeof(ieee80211_rrmstats_t)));

    if (NULL == rrmstats) {
        printf("insufficient memory \n");
        return -1;
    }

    rrmstats_req = &req.data.rrmstats_req;
    rrmstats_req->data_addr = (void *) rrmstats;
    rrmstats_req->data_size = (sizeof(ieee80211_rrmstats_t));

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    if (err >= 0) {
        print_rrmstats(stdout, rrmstats,unicast);
    }

    free(rrmstats);
    return 0;
}

/*
 * get_rrmutil: collects information for RRM utilization
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return
 */
static void get_rrmutil(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req;
    ieee80211_rrmutil_t *rrmutil = NULL;
    if ((argc < 3) || (argc > 4)) {
         usage_getrrmutil();
         return;
    }

    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = TR069_GET_RRM_UTIL;
    req.data.tr069_req.data_size = sizeof(ieee80211_rrmutil_t);
    if (req.data.tr069_req.data_size > sizeof(req.data.tr069_req.data_buff)) {
        fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n",
                __func__, sizeof(req.data.tr069_req.data_buff),
                req.data.tr069_req.data_size);
        return;
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    rrmutil = (ieee80211_rrmutil_t*)req.data.tr069_req.data_buff;
    printf("channel         \t: %d\n", rrmutil->channel);
    printf("chan util       \t: %d\n", rrmutil->chann_util);
    printf("obss util       \t: %d\n", rrmutil->obss_util);
    printf("noise floor     \t: %d\n", rrmutil->noise_floor);
    printf("radar detect    \t: %d\n", rrmutil->radar_detect);
    return;
}

/*
 * send_frmreq: specify parameters set during the sending of request frames
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int send_frmreq(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_rrm_frame_req_info_t *frm_req = &req.data.frm_req;

    if (argc != 11)
    {
        usage_sendfrmreq();
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_SENDFRMREQ;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    if (!wifitool_mac_aton(argv[3], frm_req->dstmac, 6))
    {
        errx(1, "Invalid mac address");
        return -1;
    }

    memcpy(req.dstmac, frm_req->dstmac, 6);
    frm_req->num_rpts = atoi(argv[4]);
    frm_req->regclass = atoi(argv[5]);
    frm_req->chnum = atoi(argv[6]);
    frm_req->r_invl = atoi(argv[7]);
    frm_req->m_dur = atoi(argv[8]);
    frm_req->ftype = atoi(argv[9]);

    if (!wifitool_mac_aton(argv[10], frm_req->peermac, 6))
    {
        errx(1, "Invalid mac address");
        return -1;
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
}

/*
 * send_lcireq: specify parameters set during the sending of LCI request frames
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int send_lcireq(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_rrm_lcireq_info_t *lci_req = &req.data.lci_req;

    if ((argc < 9) || (argc > 11) || (argc == 10))
    {
        usage_sendlcireq();
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_SENDLCIREQ;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    if (!wifitool_mac_aton(argv[3], lci_req->dstmac, 6))
    {
        errx(1, "Invalid mac address");
        return -1;
    }

    memcpy(req.dstmac, lci_req->dstmac, 6);
    lci_req->num_rpts = atoi(argv[4]);
    lci_req->location = atoi(argv[5]);
    lci_req->lat_res = atoi(argv[6]);
    lci_req->long_res = atoi(argv[7]);
    lci_req->alt_res = atoi(argv[8]);

    if ((lci_req->lat_res > 34) || (lci_req->long_res > 34) ||
            (lci_req->alt_res > 30))
    {
        fprintf(stderr, "Incorrect number of resolution bits !!\n");
        usage_sendlcireq();
        exit(-1);
    }

    if (argc == 11)
    {
        lci_req->azi_res = atoi(argv[9]);
        lci_req->azi_type =  atoi(argv[10]);

        if (lci_req->azi_type !=1)
        {
            fprintf(stderr, "Incorrect azimuth type !!\n");
            usage_sendlcireq();
            exit(-1);
        }

        if (lci_req->azi_res > 9)
        {
            fprintf(stderr, "Incorrect azimuth resolution value(correct range 0 - 9) !!\n");
            usage_sendlcireq();
            exit(-1);
        }
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    return 0;
}

/*
 * send_stastats: specify stats of stations associated to a client
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int send_stastats(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req;
    ieee80211_rrm_stastats_info_t *stastats = &req.data.stastats;

    if (argc < 6) {
        usage_sendstastatsrpt();
        return -1;
    } else {
        req.cmd = IEEE80211_DBGREQ_SENDSTASTATSREQ;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], stastats->dstmac, 6)) {
            errx(1, "Invalid mac address");
            return -1;
        }
        stastats->m_dur = atoi(argv[4]);
        stastats->gid = atoi(argv[5]);
        memcpy(req.dstmac,stastats->dstmac, 6);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * tr069_chan_history: show history of channels
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
static void tr069_chan_history(const char *ifname, int argc, char *argv[])
{
    int i = 0;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_channelhist_t* chandata = NULL;
    char chanSelTime[40] = "\0";
    u_int8_t act_idx=0;
    struct tm tm;
    struct timespec *ts = NULL, nowtime, tstamp;

    if ((argc < 3) || (argc > 3))
    {
        usage_tr069chanhist();
        return;
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = TR069_CHANHIST;
    req.data.tr069_req.data_size = sizeof(ieee80211_channelhist_t);
    /* Make sure tr069_req.data_buff is not smaller than ieee80211_channelhist_t. */
    if (sizeof(ieee80211_channelhist_t) > sizeof(req.data.tr069_req.data_buff)) {
        fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__,
                sizeof(req.data.tr069_req.data_buff), sizeof(ieee80211_channelhist_t));
        return;
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    /* read back nl response from driver.
     */
    chandata = (ieee80211_channelhist_t*) req.data.tr069_req.data_buff;
    act_idx = chandata->act_index;
    fprintf(stdout," Channel | Selection Time \n");
    fprintf(stdout,"----------------------------------\n");
    /* print from latest to first */
    for(i = act_idx; i >= 0 ; i--) {
        clock_gettime(CLOCK_REALTIME, &nowtime);
        ts = &(chandata->chanlhist[i].chan_time);
        tstamp.tv_sec = nowtime.tv_sec - ts->tv_sec;
        /*Convert timespec to date/time*/
        TSPECTOTIME(tstamp,tm,chanSelTime);
        fprintf(stdout," %4d      %s \n",
                chandata->chanlhist[i].chanid,chanSelTime);
    }
    if((act_idx < (IEEE80211_CHAN_MAXHIST - 1))
            && (chandata->chanlhist[act_idx+1].chanid)) {
        for(i = (IEEE80211_CHAN_MAXHIST - 1); i > act_idx ; i--) {
            clock_gettime(CLOCK_REALTIME, &nowtime);
            ts = &(chandata->chanlhist[i].chan_time);
            tstamp.tv_sec = nowtime.tv_sec - ts->tv_sec;
            /*Convert timespec to date/time*/
            TSPECTOTIME(tstamp,tm,chanSelTime);
            fprintf(stdout," %4d      %s \n",
                    chandata->chanlhist[i].chanid,chanSelTime);
        }
    }

    return;
}

/*
 * get_assoc_time: specify different mac addresses and time associated
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
static void get_assoc_time(const char *ifname, int argc, char *argv[])
{
    u_int8_t buf[24*1024];
    struct iwreq iwr;
    int s=0, len=0;
    u_int8_t *cp;
    struct timespec assoc_ts, delta_ts;
    struct timespec now_ts = {0};
    struct tm assoc_tm = {0};
    char assoc_time[40]={'\0'};
    char mac_string[MAC_STRING_LENGTH + 1] = {0};

    if (argc != 3) {
        fprintf(stderr, "usage: wifitool athX get_assoc_time");
        return;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s < 0) {
        fprintf(stderr, "Socket error");
        return;
    }
    (void) memset(&iwr, 0, sizeof(iwr));
    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(s);
        return;
    }

    iwr.u.data.pointer = (void *) buf;
    iwr.u.data.length = sizeof(buf);
    if (ioctl(s, IEEE80211_IOCTL_STA_INFO, &iwr) < 0) {
        fprintf(stderr, "Unable to get station information");
        close(s);
        return;
    }
    len = iwr.u.data.length;
    if (len < sizeof(struct ieee80211req_sta_info)){
        fprintf(stderr, "Unable to get station information");
        close(s);
        return;
    }
    printf("%4s %9s\n", "ADDR", "ASSOCTIME");
    cp = buf;
    do {
        struct ieee80211req_sta_info *si;
        si = (struct ieee80211req_sta_info *) cp;
        clock_gettime(CLOCK_REALTIME, &now_ts);
        (void) memcpy(&assoc_ts, &si->isi_tr069_assoc_time, sizeof(si->isi_tr069_assoc_time));
        delta_ts.tv_sec = now_ts.tv_sec - assoc_ts.tv_sec;
        TSPECTOTIME(delta_ts, assoc_tm, assoc_time);
        ether_mac2string(mac_string, si->isi_macaddr);
        printf("%s %s"
                , (mac_string != NULL) ? mac_string :"NO MAC ADDR"
                , assoc_time
              );
        printf("\n");
        cp += si->isi_len, len -= si->isi_len;
    } while (len >= sizeof(struct ieee80211req_sta_info));

    close(s);
    return;
}

/*
 * tr069_txpower_cmd: collect and print transmission power info
 *                    currently supports: tr069_txpower, tr069_gettxpower
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 * return -1 on error; otherwise return 0 on success
 */
static int tr069_txpower_cmd(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, percent_value, cmdid;
    int *txpower;

    switch (req_cmdid) {
        case TR069_TXPOWER:
            cmdid = TR069_TXPOWER;
            arg_num = 4;
            usage_cmd = usage_tr069_txpower;
            break;

        case TR069_GETTXPOWER:
            cmdid = TR069_GETTXPOWER;
            arg_num = 3;
            usage_cmd = usage_tr069_gettxpower;
            break;

        default:
            return -1;
    }

    if ((argc != arg_num))  {
        usage_cmd();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = cmdid;
    req.data.tr069_req.data_size = sizeof(int);

    if (req_cmdid == TR069_TXPOWER) {
        percent_value = atoi(argv[3]);
        if ((percent_value > 100)||(percent_value < 0)){
            fprintf(stderr, "usage: Percentage value should be below 100 and more than 0 \n");
            return -1;
        }
        if (percent_value <= 100){
            req.data.param[0] = percent_value;
        }

    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    txpower = (int *)&(req.data.tr069_req.data_buff);
    if(*txpower == -1){
        printf("This operation is not permitted when the Vap is up \n");
        return -1;
    }
    printf("%s: Txpower: %d\n", __func__, *txpower);
    return 0;
}

/*
 * tr069_guardintv_cmd: collects and prints guard interval info
 *                      currently supports: tr069_guardintv, tr069_getguardintv
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 * return -1 on error; otherwise return 0 on success
 */
static int tr069_guardintv_cmd(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    struct ieee80211req_athdbg req = {0};
    int arg_num, cmdid, reply, value;
    int *guardintv = NULL;
    switch (req_cmdid) {
        case TR069_GUARDINTV:
            cmdid = TR069_GUARDINTV;
            arg_num = 4;
            usage_cmd = usage_tr069_guardintv;
            reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
            break;

        case TR069_GET_GUARDINTV:
            cmdid = TR069_GET_GUARDINTV;
            arg_num = 3;
            usage_cmd = usage_tr069_get_guardintv;
            reply = DBGREQ_REPLY_IS_REQUIRED;
            break;

        default:
            return -1;
    }

    if ((argc != arg_num)) {
        usage_cmd();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = reply;
    req.data.tr069_req.cmdid = cmdid;
    switch (req_cmdid) {
        case TR069_GUARDINTV:
            value = atoi(argv[3]);
            req.data.param[0] = value;
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            break;

        case TR069_GET_GUARDINTV:
            req.data.tr069_req.data_size = sizeof(int);
            send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            guardintv = (int *)&(req.data.tr069_req.data_buff);
            if(*guardintv == 0)
                printf(" %s:  TR69GUARDINTV VALUE:     %d (AUTO) \n",argv[1], *guardintv);
            else
                printf(" %s:  TR69GUARDINTV VALUE:     %d \n",argv[1], *guardintv);
            break;

        default:
            return -1;
    }
    return 0;
}

/*
 * tr069_get_cmd: collect and print tr069 parameters like associated stations count, acs channel scan time, acs diagnostic state,
 *                stations stats count, supported value for 11h, power range, frequencies supported by channel
 *                currently supports:  tr069_getassocsta, tr069_gettimestamp, tr069_getacsscan, tr069_perstastatscount,
 *                                     tr069_get11hsupported, tr069_getpowerrange, tr069_getsupportedfrequency
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 */
static void tr069_get_cmd(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, i, cmdid;
    int *sta_count, *supported;
    struct timespec *time, nowtime, tstamp;
    char chanScanTime[40] = "\0";
    struct tm tm;
    char *state = NULL;
    ieee80211_tr069_txpower_range *range;

    switch (req_cmdid) {
        case TR069_GETASSOCSTA_CNT:
            cmdid = TR069_GETASSOCSTA_CNT;
            arg_num = 3;
            usage_cmd = usage_tr069_getassocsta;
            req.data.tr069_req.data_size = sizeof(int);
            break;

        case TR069_GETTIMESTAMP:
            cmdid = TR069_GETTIMESTAMP;
            arg_num = 3;
            usage_cmd = usage_tr069_gettimestamp;
            req.data.tr069_req.data_size = sizeof(struct timespec);
            break;

        case TR069_GETDIAGNOSTICSTATE:
            cmdid = TR069_GETDIAGNOSTICSTATE;
            arg_num = 4;
            usage_cmd = usage_tr069_getacsscan;
            req.data.tr069_req.data_size = TR69SCANSTATEVARIABLESIZE * sizeof(char);
            break;

        case TR069_GETNUMBEROFENTRIES:
            cmdid = TR069_GETNUMBEROFENTRIES;
            arg_num = 3;
            usage_cmd = usage_tr069_perstastatscount;
            req.data.tr069_req.data_size = sizeof(int);
            break;

        case TR069_GET11HSUPPORTED:
            cmdid = TR069_GET11HSUPPORTED;
            arg_num = 3;
            usage_cmd = usage_tr069_get11hsupported;
            req.data.tr069_req.data_size = sizeof(int);
            break;

        case TR069_GETPOWERRANGE:
            cmdid = TR069_GETPOWERRANGE;
            arg_num = 3;
            usage_cmd = usage_tr069_getpowerrange;
            req.data.tr069_req.data_size = sizeof(ieee80211_tr069_txpower_range);
            break;

        case TR069_GETSUPPORTEDFREQUENCY:
            cmdid = TR069_GETSUPPORTEDFREQUENCY;
            arg_num = 3;
            usage_cmd = usage_tr069_getsupportedfrequency;
            req.data.tr069_req.data_size = 15 * sizeof(char);
            break;

        default:
            return;
    }

    if (argc != arg_num) {
        usage_cmd();
        return;
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = cmdid;
    if (req_cmdid == TR069_GETDIAGNOSTICSTATE) {
        req.data.param[3] = atoi(argv[3]);
        if (req.data.tr069_req.data_size > sizeof(req.data.tr069_req.data_buff)) {
            fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__,
                    sizeof(req.data.tr069_req.data_buff), req.data.tr069_req.data_size);
            return;
        }

    }
    if (req_cmdid == TR069_GETPOWERRANGE) {
        if (req.data.tr069_req.data_size > sizeof(req.data.tr069_req.data_buff)){
            fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__, sizeof(req.data.tr069_req.data_buff), req.data.tr069_req.data_size);
        }
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    switch (req_cmdid) {
        case TR069_GETASSOCSTA_CNT:
            sta_count = (int *)&(req.data.tr069_req.data_buff);
            printf(" %s:  TR069GETASSOCSTA VALUE:     %d \n",argv[1], *sta_count);
            break;

        case TR069_GETTIMESTAMP:
            time = (struct timespec *)&(req.data.tr069_req.data_buff);
            clock_gettime(CLOCK_REALTIME, &nowtime);
            tstamp.tv_sec = nowtime.tv_sec - time->tv_sec;
            /*Convert timespec to date/time*/
            TSPECTOTIME(tstamp,tm,chanScanTime);
            printf(" %s:  TR069ACSTIMESTAMP VALUE:     %s \n",argv[1], chanScanTime);
            break;

        case TR069_GETDIAGNOSTICSTATE:
            state = (char *)&(req.data.tr069_req.data_buff);
            printf(" %s:  TR069ACSDIAGNOSTICSTATE:     %s \n",argv[1], state);
            break;

        case TR069_GETNUMBEROFENTRIES:
            sta_count = (int *)&(req.data.tr069_req.data_buff);
            printf(" %s:  TR069 PER STA STATS COUNT :     %d \n",argv[1], *sta_count);
            break;

        case TR069_GET11HSUPPORTED:
            supported = (int *)&(req.data.tr069_req.data_buff);
            printf(" %s:  TR069GET11HSUPPORTED VALUE:     %d \n",argv[1], *supported);
            break;

        case TR069_GETPOWERRANGE:
            range = (ieee80211_tr069_txpower_range *)&(req.data.tr069_req.data_buff);
            if(range->value == -1){
                printf("This operation is not permitted when the Vap is up \n");
                return;
            }
            for(i = 0; i <= range->value; i++){
                printf(" %s:  TR069GETPOWERRANGE VALUE:     %d \n",argv[1], range->value_array[i]);
            }
            break;

        case TR069_GETSUPPORTEDFREQUENCY:
            state = (char *)&(req.data.tr069_req.data_buff);
            printf(" supported frequency is : %s \n", state);
            break;

        default:
            return;
    }
    return;
}

/*
 * tr069_chan_inuse: collects and prints list of current channels
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int tr069_chan_inuse(const char *ifname, int argc, char *argv[])
{
    int err, i;
    struct ieee80211req_athdbg req = { 0 };
    struct ieee80211_acs_dbg *acs = NULL;
    char buff[5] = "/0";
    char chanbuff[1024] = "/0"; /*max according to the spec*/

    if ((argc < 3) || (argc > 3))
    {
        usage_tr069_chan_inuse();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    acs = (void *)malloc(sizeof(struct ieee80211_acs_dbg));

    if (NULL == acs) {
        fprintf(stderr, "insufficient memory \n");
        return -1;
    }
    req.data.acs_rep.data_addr = acs;
    req.data.acs_rep.data_size = sizeof(struct ieee80211_acs_dbg);
    req.data.acs_rep.index = 0;
    acs->entry_id = 0;

    err  = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (err < 0) {
        fprintf(stderr, "error while sending command\n");
        free(acs);
        return -1;
    }

    for (i = 0; i < acs->nchans; i++) {
        acs->entry_id = i;
        req.cmd = IEEE80211_DBGREQ_GETACSREPORT;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
        if (err < 0) {
            fprintf(stderr, "error while sending command\n");
            free(acs);
            return -1;
        }

        /*To make sure we are not getting more than 100 %*/
        if(acs->chan_load  > 100)
            acs->chan_load = 100;

        if(i == (acs->nchans - 1)) /*no comma at the end*/
            snprintf(buff, sizeof(buff), "%d", acs->ieee_chan);
        else
            snprintf(buff, sizeof(buff), "%d,", acs->ieee_chan);

        if (strlcat(chanbuff, buff, strlen(chanbuff) + strlen(buff) + 1) >=
                sizeof(chanbuff)) {
            fprintf(stderr, "Channel number string too long to handle\n");
            free(acs);
            return -1;
        }
    }

    fprintf(stdout," List of Channels In Use \n");
    fprintf(stdout,"-------------------------\n");
    fprintf(stdout,"%s\n",chanbuff);

    free(acs);
    return 0;
}

/*
 * tr069_set_oprate: set and store operation rate list to data buffer
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int tr069_set_oprate(const char *ifname, int argc, char *argv[])
{
#define MIN_PARAM  4
    char *ratelist  = NULL ;

    if ((argc < MIN_PARAM)) {
        usage_tr069_setoprate();
        return -1;
    }

    ratelist  = argv[3] ;
    if (strlen(argv[3]) > 256)
        return -1;
    struct ieee80211req_athdbg req = { 0 };
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    req.data.tr069_req.cmdid = TR069_SET_OPER_RATE;
    req.data.tr069_req.data_size = strlen(ratelist);
    if (req.data.tr069_req.data_size >= sizeof(req.data.tr069_req.data_buff)) {
        fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__,
                sizeof(req.data.tr069_req.data_buff), req.data.tr069_req.data_size + 1);
        return -1;
    }
    memcpy(&req.data.tr069_req.data_buff, ratelist, req.data.tr069_req.data_size);
    req.data.tr069_req.data_buff[req.data.tr069_req.data_size] = '\0';

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
#undef MIN_PARAM
}

/*
 * assoc_dev_watermark_time:  get the timestamp when the maximum number of stations
 *                            has been associated from the driver and display it
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void assoc_dev_watermark_time(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    int sock = 0;
    char assoc_watermarkTime[40] = "\0";
    struct tm assoc_tm = {0};
    struct timespec tstamp = {0};
    struct timespec nowtime = {0};

    if (argc != 3) {
        usage_assoc_dev_watermark_time();
        return;
    }

    req.cmd = IEEE80211_DBGREQ_ASSOC_WATERMARK_TIME;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    clock_gettime(CLOCK_REALTIME, &nowtime);
    tstamp.tv_sec = nowtime.tv_sec - req.data.t_spec.tv_sec;
    /*Convert timespec to date/time*/
    TSPECTOTIME(tstamp,assoc_tm,assoc_watermarkTime);

    printf(" %s:  ASSOC_WATERMARK_TIME VALUE:     %s \n",argv[1], assoc_watermarkTime);
    return;
}

/*
 * beeliner_fw_test: fw_test with arg and different value for fw testing in beeliner.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int beeliner_fw_test(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if (argc < 5) {
        usage_beeliner_fw_test();
        return -1;
    } else {
        req.cmd = IEEE80211_DBGREQ_FW_TEST;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = strtoul(argv[3], NULL, 0);
        req.data.param[1] = strtoul(argv[4], NULL, 0);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * fw_unit_test_common: set unit test command with module id, number of args, args list for fw testing
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req: a pointer to a structure of type struct ieee80211req_athdbg
 * @diag_token: diag token value
 */
void fw_unit_test_common(const char *ifname,
        int argc,
        char *argv[],
        struct ieee80211req_athdbg *req,
        uint32_t diag_token
        )
{
    int i, j;

    if (argc < 5) {
        usage_fw_unit_test();
        return;
    }

    memset(req, 0, sizeof(struct ieee80211req_athdbg));
    req->cmd = IEEE80211_DBGREQ_FW_UNIT_TEST;
    req->data.fw_unit_test_cmd.module_id = strtoul(argv[3], NULL, 0);
    req->data.fw_unit_test_cmd.diag_token = diag_token;
    req->data.fw_unit_test_cmd.num_args = strtoul(argv[4], NULL, 0);
    for (i = 0, j = 5; i < req->data.fw_unit_test_cmd.num_args; i++, j++) {
        req->data.fw_unit_test_cmd.args[i] = strtoul(argv[j], NULL, 0);
    }

    send_command(&sock_ctx, ifname, req, sizeof(struct ieee80211req_athdbg), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return;
}

/*
 * fw_unit_test: fw_unit_test_cmd with arg and different value for fw testing.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void fw_unit_test(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req;

    fw_unit_test_common(ifname, argc, argv, &req, 0);
}

#define POINTER_DIFF(a, b) (((char *)a) - ((char *)b))

/*
 * display_unit_test_event: prints test event
 * @event: a pointer to a structure of type struct ieee80211req_athdbg_event
 * @total_event_size: Complete size of event
 * @pid: process id
 * return -1 on error; otherwise result of bitwise AND operation between event flag and mask value
 */
int display_unit_test_event(struct ieee80211req_athdbg_event *event,
        uint32_t total_event_size,
        uint32_t pid)
{
    const int FW_UNIT_TEST_EVENT_DONE_MASK = 0x2;
    uint32_t derived_buf_size = total_event_size -
        POINTER_DIFF(event->fw_unit_test.buffer, event);
    uint32_t buf_size;
    printf("EVENT RECEIVED\n");
    printf("MODULE-ID: %d\n", event->fw_unit_test.hdr.module_id);
    printf("FLAG: %d\n", event->fw_unit_test.hdr.flag);
    if (derived_buf_size != event->fw_unit_test.hdr.buffer_len) {
        printf("ERROR!! Mismatch between buffer len and derived buffer len\n");
        printf("ERROR!!!DERIVED = %u\n", derived_buf_size);
        printf("ERROR!!!FOUND= %u\n", event->fw_unit_test.hdr.buffer_len);
        return -1;
    } else {
        printf("TOTAL BUFFER LEN: %d\n", event->fw_unit_test.hdr.buffer_len);
    }
    buf_size = (derived_buf_size > event->fw_unit_test.hdr.payload_len)?
        event->fw_unit_test.hdr.payload_len:derived_buf_size;
    print_hex_buffer(event->fw_unit_test.buffer, buf_size);
    return (event->fw_unit_test.hdr.flag & FW_UNIT_TEST_EVENT_DONE_MASK);
}

/*
 * fw_unit_test_event: fw testing event
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void fw_unit_test_event(const char *ifname, int argc, char *argv[])
{

    struct ieee80211req_athdbg req;
    long idle_time_in_seconds = 0;
    uint32_t pid = (uint32_t)getpid();
    struct timespec start, current;

    set_event_filter(IEEE80211_DBGREQ_FW_UNIT_TEST);
    set_if_name_filter(ifname);
#if UMAC_SUPPORT_CFG80211
    if (start_event_thread(&sock_ctx)) {
        printf("ERROR!!! Unable to setup nl80211 event thread\n");
        return;
    }
#endif

    fw_unit_test_common(ifname, argc, argv, &req, pid);

    clock_gettime(CLOCK_REALTIME, &start);
    while (1) {
        struct dbg_event_q_entry *q_entry;
        if (idle_time_in_seconds >= 10) {
            printf("ERROR!! TIMED OUT WAITING FOR EVENT\n");
            break;
        }
        if (q_remove(&event_q, (void **)&q_entry)) {
            usleep(1000);
            goto continue_while_loop;
        }
        if (pid != q_entry->event->fw_unit_test.hdr.diag_token) {
            goto continue_while_loop_free;
        }

        if (display_unit_test_event(q_entry->event, q_entry->total_buf_size,
                    pid)) {
            free(q_entry->event);
            free(q_entry);
            break;
        }
        clock_gettime(CLOCK_REALTIME, &start);
continue_while_loop_free:
        free(q_entry->event);
        free(q_entry);
continue_while_loop:
        clock_gettime(CLOCK_REALTIME, &current);
        idle_time_in_seconds = current.tv_sec - start.tv_sec;
    }
}

/*
 * set_antenna_switch: set antenna switch dynamically
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int set_antenna_switch(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if (argc < 5) {
        usage_set_antenna_switch();
        return -1;
    } else {
        req.cmd = IEEE80211_DBGREQ_SET_ANTENNA_SWITCH;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = atoi(argv[3]);
        req.data.param[1] = atoi(argv[4]);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * init_rtt3: initialize round trip time (RTT)
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void init_rtt3(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if ((argc < 5) || (argc > 5)) {
        usage_init_rtt3();
        return;
    }
    req.cmd = IEEE80211_DBGREQ_INITRTT3;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    memset(req.dstmac,0x00,IEEE80211_ADDR_LEN);
    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6))
    {
        errx(1, "Invalid mac address");
        return;
    }
    req.data.param[0] = atoi(argv[4]);

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
}

/*
 * chmask_per_sta: set chainmask per sta given the input macaddress and the nss value.
 *                 set the txchainmask for the node with the given macaddress.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int chmask_per_sta(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };

    if (argc < 5) {
        usage_chmask_persta();
    }
    else {
        req.cmd = IEEE80211_DBGREQ_CHMASKPERSTA;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        if (!wifitool_mac_aton(argv[3], req.dstmac, 6)) {
            errx(1, "Invalid destination mac address");
            return -1;
        }
        req.data.param[0] = atoi(argv[4]);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL,
                QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * peer_nss: set nss per peer given the input AID and the nss value.
 *                 set the txchainmask for the node with the given macaddress.
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return none.
 */
static void
set_peer_nss(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req;

    if (argc < 5) {
        usage_peer_nss();
    }
    else {
        memset(&req, 0, sizeof(struct ieee80211req_athdbg));
        req.cmd = IEEE80211_DBGREQ_PEERNSS;
        req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
        req.data.param[0] = atoi(argv[3]);
        req.data.param[1] = atoi(argv[4]);
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL,
                QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return;
}

/* tr069_get_rate: get operational and possible rate
 *                 currently supports: tr069_get_oprate, tr069_get_posrate
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 * return -1 on error; otherwise return 0 on success
 */
int tr069_get_rate(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    int i, cmdid;
    char *ratelist = NULL;
    char ratetype[20];
    struct ieee80211req_athdbg req = { 0 };

    switch (req_cmdid) {
        case TR069_GET_OPER_RATE:
            cmdid = TR069_GET_OPER_RATE;
            usage_cmd = usage_tr069_getoprate;
            strlcpy(ratetype,"operational",sizeof(ratetype));
            break;

        case TR069_GET_POSIBLRATE:
            cmdid = TR069_GET_POSIBLRATE;
            usage_cmd= usage_tr069_getposrate;
            strlcpy(ratetype,"possible",sizeof(ratetype));
            break;

        default:
            return -1;
    }
    if ((argc < 3) || (argc > 3))
    {
        usage_cmd();
        return -1;
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = cmdid;
    req.data.tr069_req.data_size = 256;

    if (req.data.tr069_req.data_size > sizeof(req.data.tr069_req.data_buff)) {
        fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__,
                sizeof(req.data.tr069_req.data_buff), req.data.tr069_req.data_size);
        return -1;
    }

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    ratelist = (int *)&(req.data.tr069_req.data_buff);

    fprintf(stdout," List of %s Rates \n", ratetype);
    fprintf(stdout,"-------------------------\n");

    char *rate = strtok_r(ratelist, ",", &ratelist);
    while (rate != NULL) {
      fprintf(stdout,"%s\n",rate);
      rate = strtok_r(NULL, ",", &ratelist);
    }

    return 0;
}

/* tr069_bsrate_cmd: set and get bsrate
 *                   currently supports: tr069_set_bsrate, tr069_get_bsrate
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 * return -1 on error; otherwise return 0 on success
 */
int tr069_bsrate_cmd(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    int cmdid;
    char *ratelist  = NULL ;
    struct ieee80211req_athdbg req = { 0 };

    switch (req_cmdid) {
        case TR069_SET_BSRATE:
            cmdid = TR069_SET_BSRATE;
            break;

        case TR069_GET_BSRATE:
            cmdid = TR069_GET_BSRATE;
            break;

        default:
            return -1;
    }
    if ((argc != 4)) {
        fprintf(stderr, "usage: wifitool athX tr069_set_bsrate value(s)");
        return -1;
    }
    ratelist = argv[3];
    if (strlen(argv[3]) > 256)
        return -1;
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    req.data.tr069_req.cmdid = cmdid;
    req.data.tr069_req.data_size = strlen(ratelist);

    if (req.data.tr069_req.data_size >= sizeof(req.data.tr069_req.data_buff)) {
        fprintf(stderr, "error: %s: buff_size: %u, response len: %u\n", __func__,
                sizeof(req.data.tr069_req.data_buff), req.data.tr069_req.data_size + 1);
        return -1;
    }
    memcpy(&req.data.tr069_req.data_buff, ratelist, req.data.tr069_req.data_size);
    req.data.tr069_req.data_buff[req.data.tr069_req.data_size - 1] = '\0';

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    return 0;
}

/* tr069_get_trans: tr069_get_success_retrans: command to get successful retransmission count from the VAP
 *                  tr069_get_fail_retrans: command to get failed retransmission count from the VAP
 *                  tr069_get_success_mul_retrans: command to get successful retries after multiple attempts from the VAP
 *                  tr069_get_ack_failures: command to get ACK failures count from the VAP
 *                  tr069_get_retrans: command to get total re-transmission count from the VAP
 *                  tr069_get_aggr_pkts: command to get aggregate packets from the VAP
 *                  tr069_get_sta_bytes_sent: command to get bytes sent to the specific station from the VAP
 *                  tr069_get_sta_bytes_rcvd: command to get bytes received from the specific station in the VAP
 *                  currently supports: tr069_get_success_retrans, tr069_get_fail_retrans, tr069_get_success_mul_retrans,
 *                  tr069_get_ack_failures, tr069_get_retrans, tr069_get_aggr_pkts, tr069_get_sta_bytes_sent, tr069_get_sta_bytes_rcvd
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmdid: enum value for function specific command id
 * return -1 on error; otherwise return 0 on success
 */
int tr069_get_trans(const char *ifname, int argc, char *argv[], int req_cmdid)
{
    int arg_num, cmdid;
    u_int32_t *trans;
    char usage_msg[60], msg[40];
    struct ieee80211req_athdbg req = { 0 };

    switch (req_cmdid) {
        case TR069_GET_FAIL_RETRANS_CNT:
            cmdid = TR069_GET_FAIL_RETRANS_CNT;
            arg_num = 3;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_fail_retrans",sizeof(usage_msg));
            strlcpy(msg,"Fail Retransmit count \n",sizeof(msg));
            break;

        case TR069_GET_RETRY_CNT:
            cmdid = TR069_GET_RETRY_CNT;
            arg_num = 3;
            strlcpy(usage_msg, "usage: wifitool athX tr069_get_success_retrans",sizeof(usage_msg));
            strlcpy(msg,"Successful Retransmit count \n",sizeof(msg));
            break;

        case TR069_GET_MUL_RETRY_CNT:
            cmdid = TR069_GET_MUL_RETRY_CNT;
            arg_num = 3;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_success_retrans",sizeof(usage_msg));
            strlcpy(msg,"Successful Multiple Retransmit count \n",sizeof(msg));
            break;

        case TR069_GET_ACK_FAIL_CNT:
            cmdid = TR069_GET_ACK_FAIL_CNT;
            arg_num = 3;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_ack_failures",sizeof(usage_msg));
            strlcpy(msg,"ACK Failures \n",sizeof(msg));
            break;

        case TR069_GET_RETRANS_CNT:
            cmdid = TR069_GET_RETRANS_CNT;
            arg_num = 3;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_retrans",sizeof(usage_msg));
            strlcpy(msg,"Retransmit count \n",sizeof(msg));
            break;

        case TR069_GET_AGGR_PKT_CNT:
            cmdid = TR069_GET_AGGR_PKT_CNT;
            arg_num = 3;
            strlcpy(usage_msg, "usage: wifitool athX tr069_get_aggr_pkts",sizeof(usage_msg));
            strlcpy(msg,"Aggregated packet count \n",sizeof(msg));
            break;

        case TR069_GET_STA_BYTES_SENT:
            cmdid = TR069_GET_STA_BYTES_SENT;
            arg_num = 4;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_sta_bytes_sent <STA MAC>",sizeof(usage_msg));
            strlcpy(msg,"Station bytes sent count \n",sizeof(msg));
            break;

        case TR069_GET_STA_BYTES_RCVD:
            cmdid = TR069_GET_STA_BYTES_RCVD;
            arg_num = 4;
            strlcpy(usage_msg,"usage: wifitool athX tr069_get_sta_bytes_rcvd <STA MAC>",sizeof(usage_msg));
            strlcpy(msg,"Station bytes received count \n",sizeof(msg));
            break;

        default:
            return -1;
    }
    if(argc != arg_num) {
        fprintf(stderr, "%s", usage_msg);
        return -1;
    }
    if ((req_cmdid == TR069_GET_STA_BYTES_SENT) || (req_cmdid == TR069_GET_STA_BYTES_RCVD)) {
        if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
            fprintf(stderr, "Invalid mac address");
            return -1;
        }
    }
    req.cmd = IEEE80211_DBGREQ_TR069;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    req.data.tr069_req.cmdid = cmdid;
    req.data.tr069_req.data_size = sizeof(*trans);
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    trans  = (u_int32_t *)&(req.data.tr069_req.data_buff);

    fprintf(stdout," %s", msg);
    fprintf(stdout,"-----------------\n");
    fprintf(stdout,"%u\n", *trans);

    return 0;
}

/*
 * bsteer_set_cmd: bsteer_setdbgparams: set debug parameters for band steering
 *                 bsteer_setparams: set parameters for band steering
 *                 bsteer_setsteering: set band steering with destination mac address and steering flag
 *                 bsteer_setoverload: set overload using overload flag
 *                 bsteer_setproberespwh: set probe responses withheld
 *                 currently handles: bsteer_setdbgparams, bsteer_setparams, bsteer_setsteering, bsteer_setoverload, bsteer_setproberespwh
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
int bsteer_set_cmd (const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, cmd;
    u_int8_t index = 0;
    switch (req_cmd) {
        case IEEE80211_DBGREQ_BSTEERING_SET_PARAMS:
            cmd = IEEE80211_DBGREQ_BSTEERING_SET_PARAMS;
            arg_num = 18;
            usage_cmd = usage_bsteer_setparams;
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_DBG_PARAMS:
            cmd = IEEE80211_DBGREQ_BSTEERING_SET_DBG_PARAMS;
            arg_num = 6;
            usage_cmd = usage_bsteer_setdbgparams;
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_STEERING:
            cmd = IEEE80211_DBGREQ_BSTEERING_SET_STEERING;
            arg_num = 5;
            usage_cmd = usage_bsteer_setsteering;
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD:
            cmd = IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD;
            arg_num = 4;
            usage_cmd = usage_bsteer_setoverload;
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH:
            cmd = IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH;
            arg_num = 5;
            usage_cmd = usage_bsteer_setproberespwh;
            break;

        default:
            return -1;
    }

    if (argc != arg_num) {
        usage_cmd();
        return -1;
    }
    req.cmd = cmd;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_BSTEERING_SET_PARAMS:
            req.data.bsteering_param.inactivity_timeout_overload = atoi(argv[4]);
            req.data.bsteering_param.utilization_sample_period = atoi(argv[5]);
            req.data.bsteering_param.utilization_average_num_samples = atoi(argv[6]);
            req.data.bsteering_param.low_rssi_crossing_threshold = atoi(argv[9]);
            req.data.bsteering_param.inactivity_check_period = atoi(argv[10]);
            req.data.bsteering_param.low_tx_rate_crossing_threshold = atoi(argv[11]);
            req.data.bsteering_param.low_rate_rssi_crossing_threshold = atoi(argv[13]);
            req.data.bsteering_param.interference_detection_enable = atoi(argv[16]);
            req.data.bsteering_param.client_classification_enable = atoi(argv[17]);
            for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
                req.data.bsteering_param.inactivity_timeout_normal[index] = atoi(argv[3]);
                req.data.bsteering_param.inactive_rssi_xing_high_threshold[index] = atoi(argv[7]);
                req.data.bsteering_param.inactive_rssi_xing_low_threshold[index] = atoi(argv[8]);
                req.data.bsteering_param.high_tx_rate_crossing_threshold[index] = atoi(argv[12]);
                req.data.bsteering_param.high_rate_rssi_crossing_threshold[index] = atoi(argv[14]);
                req.data.bsteering_param.ap_steer_rssi_xing_low_threshold[index] = atoi(argv[15]);
            }
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_DBG_PARAMS:
            req.data.bsteering_dbg_param.raw_chan_util_log_enable = atoi(argv[3]);
            req.data.bsteering_dbg_param.raw_rssi_log_enable = atoi(argv[4]);
            req.data.bsteering_dbg_param.raw_tx_rate_log_enable = atoi(argv[5]);
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_STEERING:
            if (!wifitool_mac_aton(argv[3], &req.dstmac[0], MAC_ADDR_LEN)) {
                errx(1, "Invalid mac address");
                return -1;
            }
            req.data.bsteering_steering_in_progress = atoi(argv[4]);
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD:
            req.data.bsteering_overload = atoi(argv[3]);
            break;

        case IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH:
            if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
                errx(1, "Invalid mac address");
                return -1;
            }
            req.data.bsteering_probe_resp_wh = atoi(argv[4]);
            break;

        default:
            return -1;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
}

/*
 * bsteer_get_cmd: bsteer_getparams: get parameters for band steering
 *                 bsteer_getdbgparams: get debug parameters for band steering
 *                 bsteer_getdatarateinfo: get data rate info of provided dest mac addr for band steering
 *                 bsteer_getoverload: get overload information
 *                 bsteer_getproberespwh: get and print probe responses withheld
 *                 currently handles: bsteer_getparams, bsteer_getdbgparams, bsteer_getdatarateinfo, bsteer_getoverload, bsteer_getproberespwh
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
int bsteer_get_cmd(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, cmd;
    u_int8_t index = 0;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_BSTEERING_GET_PARAMS:
            cmd = IEEE80211_DBGREQ_BSTEERING_GET_PARAMS;
            arg_num = 3;
            usage_cmd = usage_bsteer_getparams;
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_DBG_PARAMS:
            cmd = IEEE80211_DBGREQ_BSTEERING_GET_DBG_PARAMS;
            arg_num = 3;
            usage_cmd =  usage_bsteer_getdbgparams;
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO:
            cmd = IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO;
            arg_num = 4;
            usage_cmd = usage_bsteer_getdatarateinfo;
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_OVERLOAD:
            cmd = IEEE80211_DBGREQ_BSTEERING_GET_OVERLOAD;
            arg_num = 3;
            usage_cmd = usage_bsteer_getoverload;
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_PROBE_RESP_WH:
            cmd = IEEE80211_DBGREQ_BSTEERING_GET_PROBE_RESP_WH;
            arg_num = 4;
            usage_cmd = usage_bsteer_getproberespwh;
            break;

        default:
            return -1;
    }
    if (argc != arg_num) {
        usage_cmd();
        return -1;
    }
    req.cmd = cmd;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    if (req_cmd == IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO) {
        if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
            errx(1, "Invalid mac address");
            return -1;
        }
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

    switch (req_cmd) {
        case IEEE80211_DBGREQ_BSTEERING_GET_PARAMS:
            printf("Band steering parameters: \n");
            printf("-------------------------- ------------\n");
            printf("Overload inactivity timeout: %u s\n",
                    req.data.bsteering_param.inactivity_timeout_overload);
            printf("Utilization sampling period: %u s\n",
                    req.data.bsteering_param.utilization_sample_period);
            printf("Utilization num samples to average: %u\n",
                    req.data.bsteering_param.utilization_average_num_samples);
            printf("Low RSSI crossing threshold: %u dB\n",
                    req.data.bsteering_param.low_rssi_crossing_threshold);
            printf("Inactivity check interval: %u s\n",
                    req.data.bsteering_param.inactivity_check_period);
            printf("Active steering low threshold: Tx rate %u Kbps, RSSI %u dB\n",
                    req.data.bsteering_param.low_tx_rate_crossing_threshold,
                    req.data.bsteering_param.low_rate_rssi_crossing_threshold);
            printf("Interference detection enable: %u\n",
                    req.data.bsteering_param.interference_detection_enable);
            printf("Client classification enable: %u\n",
                    req.data.bsteering_param.client_classification_enable);
            for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
                printf("Normal inactivity timeout [Class %d]: %u s\n",
                        index, req.data.bsteering_param.inactivity_timeout_normal[index]);
                printf("Inactive RSSI crossing high threshold [Class %d]: %u dB\n",
                        index, req.data.bsteering_param.inactive_rssi_xing_high_threshold[index]);
                printf("Inactive RSSI crossing low threshold [Class %d]: %u dB\n",
                        index, req.data.bsteering_param.inactive_rssi_xing_low_threshold[index]);
                printf("Active steering high threshold [Class %d]: Tx rate %u Kbps, RSSI %u dB\n",
                        index, req.data.bsteering_param.high_tx_rate_crossing_threshold[index],
                        req.data.bsteering_param.high_rate_rssi_crossing_threshold[index]);
                printf("AP steering low RSSI threshold [Class %d]: %u dB\n",
                        index, req.data.bsteering_param.ap_steer_rssi_xing_low_threshold[index]);
            }
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_DBG_PARAMS:
            printf("Band steering debug parameters: \n");
            printf("-------------------------- ------------\n");
            printf("Enable raw channel utilization logging: %s\n",
                    req.data.bsteering_dbg_param.raw_chan_util_log_enable ?
                    "yes" : "no");
            printf("Enable raw RSSI measurement logging: %s\n",
                    req.data.bsteering_dbg_param.raw_rssi_log_enable ?
                    "yes" : "no");
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO:
            printf("Data rate info for %s on %s: Max Bandwidth: %u, Num streams: %u, "
                    "PHY mode: %u, Max MCS: %u, Max TX power: %u\n",
                    argv[3], ifname, req.data.bsteering_datarate_info.max_chwidth,
                    req.data.bsteering_datarate_info.num_streams,
                    req.data.bsteering_datarate_info.phymode,
                    req.data.bsteering_datarate_info.max_MCS,
                    req.data.bsteering_datarate_info.max_txpower);
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_OVERLOAD:
            printf("%s is %soverloaded\n", ifname,
                    req.data.bsteering_overload ? "" : "not ");
            break;

        case IEEE80211_DBGREQ_BSTEERING_GET_PROBE_RESP_WH:
            printf("Probe responses withheld for %s on %s: %s\n", argv[3], ifname,
                    req.data.bsteering_probe_resp_wh ? "yes" : "no");
            break;

        default:
            return -1;
    }
    return 0;
}

/*
 * bsteer_enable: enable band steering and band steering events using enable flag
 *                currently supports: bsteer_enable_events, bsteer_enable
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
int bsteer_enable(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int err, cmd;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_BSTEERING_ENABLE:
            cmd = IEEE80211_DBGREQ_BSTEERING_ENABLE;
            usage_cmd = usage_bsteer_enable;
            break;

        case IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS:
            cmd = IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS;
            usage_cmd = usage_bsteer_enable_events;
            break;

        default:
            return -1;
    }
    if (argc != 4) {
        usage_cmd();
        return -1;
    }
    req.data.bsteering_enable = atoi(argv[3]);
    req.cmd =  cmd;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (req_cmd == IEEE80211_DBGREQ_BSTEERING_ENABLE) {
        if (err < 0) {
            fprintf(stderr, "IEEE80211_DBGREQ_BSTEERING_ENABLE failed\n");
        }
    }
    return 0;
}

/*
 * bsteer_getrssi: get RSSI for given dest mac addr and number of samples
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void bsteer_getrssi(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if (argc != 5) {
        usage_bsteer_getrssi();
        return;
    }

    req.cmd = IEEE80211_DBGREQ_BSTEERING_GET_RSSI;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
        errx(1, "Invalid mac address");
        return;
    }
    req.data.bsteering_rssi_num_samples = atoi(argv[4]);
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
}

/*
 * bsteer_setauthallow: set authentication allowance for band steering
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int bsteer_setauthallow(const char *ifname, int argc, char *argv[])
{
    struct iwreq iwr = { 0 };
    struct ieee80211req_athdbg req = { 0 };
    int s = 0;
    if (argc != 5) {
        usage_bsteer_setauthallow();
        return -1;
    }

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        errx(1, "ifname too long: %s\n", ifname);
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_BSTEERING_SET_AUTH_ALLOW;
    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
        errx(1, "Invalid mac address");
        return -1;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        errx(1, "Error in opening socket");
        return -1;
    }

    req.data.bsteering_auth_allow = atoi(argv[4]);
    iwr.u.data.pointer = (void *) &req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));

    if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
        perror("bsteer_setauthallow");
        errx(1, "IEEE80211_IOCTL_DBGREQ:"
                "IEEE80211_DBGREQ_BSTEERING_SET_AUTH_ALLOW failed");
    }
    close(s);
    return 0;
}

/* bsteer_set_sta_stats_update_interval_da: set interval for DA sta stats
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int bsteer_set_sta_stats_update_interval_da(const char *ifname, int argc, char *argv[])
{
    struct iwreq iwr;
    struct ieee80211req_athdbg req = { 0 };
    int s = 0;
    if (argc != 4) {
        return -1;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&iwr, 0, sizeof(iwr));
    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        close(s);
        errx(1, "ifname too long: %s\n", ifname);
        return -1;
    }

    req.cmd = IEEE80211_DBGREQ_BSTEERING_SET_DA_STAT_INTVL;
    req.data.bsteering_sta_stats_update_interval_da = atoi(argv[3]);

    iwr.u.data.pointer = (void *) &req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
    if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
        errx(1, "IEEE80211_IOCTL_DBGREQ:"
                "IEEE80211_DBGREQ_BSTEERING_SET_STEERING failed");
    }

    close(s);
    return 0;
}

/*
 * custom_chan_list: wifitool athX custom_chan_list [-a 1-101 1-165] [-n 1-101 1-165]
 *                   -a number of channels followed by IEEE number(s) of the channel(s) when sta is connected
 *                   -n number of channels followed by IEEE number(s) of the channel(s) when sta is disconnected
 *                   for ex: to fill channel 1 and 6 in associated list and 11 in non associated list
 *                   wifitool athx custom_chan_list -a 2 1 6 -n 1 11
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void custom_chan_list(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    int i, j, x, y;

    req.cmd = IEEE80211_DBGREQ_CHAN_LIST;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    argc -= 3;
    i = 3;
    while (argc) {
        if (!strncmp(argv[i], "-a",2)) {
            argc--;
            i++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[i], NULL, 10);
            if (x >= 1 && x <= MAX_CUSTOM_CHANS) {
                argc--;
                i++;
                req.data.custom_chan_req.scan_numchan_associated = x;
                j = 0;
                while (x-- && argc) {
                    y = strtoul(argv[i], NULL, 10);
                    if (y >= 1 && y <= 165) {
                        argc--;
                        i++;
                        req.data.custom_chan_req.scan_channel_list_associated[j++] = y;
                    } else
                        break;
                }
                if (j != req.data.custom_chan_req.scan_numchan_associated)
                {
                    req.data.custom_chan_req.scan_numchan_associated = 0;
                    printf("USAGE: wifitool athX custom_chan_list -h ---for help\n");
                }
            } else {
                printf("Max supported channels is %d\n",MAX_CUSTOM_CHANS);
                printf("USAGE: wifitool athX custom_chan_list -h ---for help\n");
                return;
            }
        } else if (!strncmp(argv[i], "-n",2)) {
            argc--;
            i++;
            if (!argc) {
                break;
            }
            x = strtoul(argv[i], NULL, 10);
            if (x >= 1 && x <= MAX_CUSTOM_CHANS) {
                argc--;
                i++;
                req.data.custom_chan_req.scan_numchan_nonassociated = x;
                j = 0;
                while (x-- && argc) {
                    y = strtoul(argv[i], NULL, 10);
                    if (y >= 1 && y <= 165) {
                        argc--;
                        i++;
                        req.data.custom_chan_req.scan_channel_list_nonassociated[j++] = y;
                    } else
                        break;
                }
                if (j != req.data.custom_chan_req.scan_numchan_nonassociated)
                {
                    req.data.custom_chan_req.scan_numchan_nonassociated = 0;
                    printf("USAGE: wifitool athX custom_chan_list -h ---for help\n");
                }
            }
            else {
                printf("Max supported channels is %d\n",MAX_CUSTOM_CHANS);
                printf("USAGE: wifitool athX custom_chan_list -h ---for help\n");
                return;
            }
        } else if (!strncmp(argv[i], "-h",2)) {
            argc--;
            i++;
            printf("wifitool athX custom_chan_list [-a 1-101 1-165] [-n 1-101 1-165]\n");
            printf(" -a number of channels followed by IEEE number(s) of the channel(s) to scan when sta is connected\n");
            printf("-n :number of channels followed by IEEE number(s) of the channel(s) to scan when sta is not connected\n");
            printf("for ex: to fill channel 1 and 6 in associated list and 11 in non associated list\n");
            printf("wifitool athx custom_chan_list -a 2 1 6 -n 1 11\n");
            return;

        } else {
            printf("USAGE: wifitool athX custom_chan_list -h ---for help\n");
            return;

        }
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
}

#if UMAC_SUPPORT_VI_DBG
/*
 * vow_debug_cmd: vow_debug: wifitool athX vow_debug <stream_no> <marker_num> <marker_offset> <marker_match>
 *                           wifitool athx vow_debug 1 2 0x00ffff 5
 *                vow_debug_set_params: wifitool athX vow_debug_set_param <num of stream> <num of marker> <rxq offset> <rxq max> <time offset>
 *                                      wifitool athx vow_debug 1 1 0xffff0011 0xff112233 0xffffffff 0x12efde11
 *                currently supports: vow_debug, vow_debug_set_param
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
int vow_debug_cmd(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    struct iwreq iwr;
    int s, cmd, arg_num;

    switch (req_cmd) {
        case IEEE80211_DBGREQ_VOW_DEBUG_PARAM_PERSTREAM:
            cmd = IEEE80211_DBGREQ_VOW_DEBUG_PARAM_PERSTREAM;
            arg_num = 7;
            usage_cmd = usage_vow_debug;
            break;

        case IEEE80211_DBGREQ_VOW_DEBUG_PARAM:
            cmd = IEEE80211_DBGREQ_VOW_DEBUG_PARAM;
            arg_num = 9;
            usage_cmd = usage_vow_debug_set_param;
            break;

        default:
            return -1;
    }

    if (argc != arg_num) {
        usage_cmd();
        return -1;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    (void) memset(&iwr, 0, sizeof(iwr));
    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        close(s);
        errx(1, "ifname too long: %s\n", ifname);
        return -1;
    }
    switch (req_cmd) {
        case IEEE80211_DBGREQ_VOW_DEBUG_PARAM_PERSTREAM:
            req.data.vow_dbg_stream_param.stream_num = atoi(argv[3]);
            req.data.vow_dbg_stream_param.marker_num = atoi(argv[4]);
            req.data.vow_dbg_stream_param.marker_offset = strtoul(argv[5], NULL, 16);
            req.data.vow_dbg_stream_param.marker_match = strtoul(argv[6], NULL, 16);
            break;

        case IEEE80211_DBGREQ_VOW_DEBUG_PARAM:
            req.data.vow_dbg_param.num_stream = atoi(argv[3]);
            req.data.vow_dbg_param.num_marker = atoi(argv[4]);
            req.data.vow_dbg_param.rxq_offset = strtoul(argv[5], NULL, 16);
            req.data.vow_dbg_param.rxq_shift = strtoul(argv[6], NULL, 16);
            req.data.vow_dbg_param.rxq_max = strtoul(argv[7], NULL, 16);
            req.data.vow_dbg_param.time_offset = strtoul(argv[8], NULL, 16);
            break;

        default:
            return -1;
    }
    req.cmd = cmd;
    iwr.u.data.pointer = (void *) &req;
    iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
    if (ioctl(s, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
        errx(1, "IEEE80211_IOCTL_DBGREQ:"
                "IEEE80211_DBGREQ_VOW_DEBUG_PARAMPERSTREAM failed");
    }

    close(s);
    return 0;
}
#endif


#if QCA_NSS_PLATFORM
#define HOSTAPD_CONFIG_FILE_PREFIX "/var/run/hostapd"
#else
#define HOSTAPD_CONFIG_FILE_PREFIX "/tmp/sec"
#endif

/* find_pid: obtains process id
 * @process_name: pointer to name of the process
 * return -1 if error; otherwise pid on success
 */
pid_t find_pid(const char *process_name)
{
    pid_t pid = -1;
    glob_t pglob;
    char *procname = {'\0'}, *buf;
    int buflen = strlen(process_name) + 1;
    unsigned tmp;

#if QCA_NSS_PLATFORM
#define PROC_PATH "/proc/*/comm"
#else
#define PROC_PATH "/proc/*/cmdline"
#endif

    /* Using glob function to list all the
     * processes names and their path using
     * pattern matching over /proc filesystem
     */
    if (glob(PROC_PATH, 0, NULL, &pglob) != 0)
        return pid;

    procname = malloc(buflen);
    if(procname == NULL) {
        goto proc_mem_fail;
    }

    if (strlcpy(procname, process_name, sizeof(procname)) >= sizeof(procname)) {
        fprintf(stderr, "process name is too long\n");
        goto buf_mem_fail;
    }

    buf = malloc(buflen);
    if(buf == NULL) {
        goto buf_mem_fail;
    }

    for (tmp = 0; tmp < pglob.gl_pathc; ++tmp) {
        FILE *comm;

        if((comm = fopen(pglob.gl_pathv[tmp], "r")) == NULL)
            continue;
        if((fgets(buf, buflen, comm)) == NULL) {
            fclose(comm);
            continue;
        }
        fclose(comm);
        if(strstr(buf, procname) != NULL) {
            pid = (pid_t)atoi(pglob.gl_pathv[tmp] + strlen("/proc/"));
            break;
        }
    }

    free(buf);
buf_mem_fail:
    free(procname);
proc_mem_fail:
    globfree(&pglob);
    return pid;
#undef PROC_PATH
}

/*
 * get_hostapd_param: obtain hostapd parameters
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_hostapd_param(const char *ifname, int argc, char *argv[])
{
    char fname[30];
    FILE *fp;
    char *pos;
    int  buflen = 0;
    char buf[80] = {'\0'}, param[40] = {'\0'};
    int res = 0;

    if (argc != 4) {
        fprintf(stderr, "usage: wifitool athX get_hostapd_param param");
        return -1;
    }

    if (strlcpy(param, argv[3], sizeof(param)) >= sizeof(param)) {
        fprintf(stderr, "Param value too long %s\n", argv[3]);
        return -1;
    }

    buflen = sizeof(buf);

#if QCA_NSS_PLATFORM
    res = snprintf(fname, sizeof(fname), "%s-%s.conf", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(fname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }
#else
    res = snprintf(fname, sizeof(fname), "%s%s", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(fname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }
#endif

    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "Unable to open file %s\n", fname);
        return -1;
    }
    while(fgets(buf, buflen, fp)) {
        char fpar[40] = {'\0'};

        if((pos = strchr(buf, '=')) != NULL)
            strlcpy(fpar, buf, (pos - buf) * sizeof(char));

        if (strcmp(fpar, param) == 0) {
            pos++;
            printf("%s: %s\n", ifname, pos);
            fclose(fp);
            return 0;
        }
    }
    printf("%s: Parameter not found\n", ifname);
    fclose(fp);
    return -1;
}

/*
 * set_hostapd_param: wifitool athX set_hostapd_param param value
 *                    This command can be used to dynamically change a value in
 *                    the hostapd conf file by specifying the param exactly and
 *                    the value to be set
 *
 *                    A temporary file is created and all the previous paramters
 *                    and their values are copied here except the parameter to be
 *                    changed which gets the new value
 *
 *                    A SIGHUP is sent to the hostapd daemon and thus the changes
 *                    are effected through a daemon restart
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int set_hostapd_param(const char *ifname, int argc, char *argv[])
{
    char fname[40], tempfname[40];
    FILE *fp, *fp2;
    int buflen = 0;
    char *pos;
    char buf[80] = {'\0'}, param[40] = {'\0'}, val[64] = {'\0'}, result[80] = {'\0'};
    pid_t hostapd_pid;
    u_int8_t file_changed = 0;
    char hostapd_name[20]= "hostapd";
    int res = 0;

    if (argc != 5) {
        fprintf(stderr, "usage: wifitool athX set_hostapd_param param value");
        return -1;
    }

    if (strlcpy(param, argv[3], sizeof(param)) >= sizeof(param)) {
        fprintf(stderr, "length of param is too long %s\n", argv[3]);
        return -1;
    }

    if (strlcpy(val, argv[4], sizeof(val)) >= sizeof(val)) {
        fprintf(stderr, "length of param value is too long %s\n", argv[4]);
        return -1;
    }

    buflen = sizeof(buf);

#if QCA_NSS_PLATFORM
    res = snprintf(fname, sizeof(fname), "%s-%s.conf", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(fname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }

    res = snprintf(tempfname, sizeof(tempfname), "%s-%s.conf.temp", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(tempfname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }

#else
    res = snprintf(fname, sizeof(fname), "%s%s", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(fname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }

    res = snprintf(tempfname, sizeof(tempfname), "%s%s.temp", HOSTAPD_CONFIG_FILE_PREFIX, ifname);
    if (res < 0) {
        fprintf(stderr, "Unable to generate filename\n");
        return -1;
    } else if (res >= sizeof(tempfname)) {
        fprintf(stderr, "length of ifname is too long %s\n", ifname);
        return -1;
    }

#endif

    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "Unable to open %s\n", fname);
        return -1;
    }
    if ((fp2 = fopen(tempfname, "w")) == NULL) {
        fprintf(stderr, "Unable to create a temp file %s\n", tempfname);
        fclose(fp);
        return -1;
    }
    rewind(fp);
    rewind(fp2);

    while (!feof(fp)) {
        if( fgets(buf, buflen, fp) == NULL )
            break;
        char fpar[40] = {'\0'};
        if((pos = strchr(buf, '=')) != NULL)
            strlcpy(fpar, buf, (pos - buf) * sizeof(char));

        if( strcmp(fpar, param) == 0) {
            snprintf(result, sizeof(result), "%s=%s\n", param, val);
            fputs(result, fp2);
            file_changed=1;
            continue;
        }
        fputs(buf, fp2);
    }
    hostapd_pid = find_pid(hostapd_name);
    if(file_changed != 1 || hostapd_pid <= 0) {
        fprintf(stderr, "Parameter set error\n");
        remove(tempfname);
        goto clean_exit;
    }
    else {
        if(remove(fname) != 0) {
            fprintf(stderr, "Unable to remove %s\n", fname);
            goto clean_exit;
        }
        if(rename(tempfname, fname) != 0) {
            fprintf(stderr, "Unable to rename temp file %s to hostapd file %s\n", fname, tempfname);
            goto clean_exit;
        }
        kill(hostapd_pid, SIGHUP);
    }
clean_exit:
    fclose(fp);
    fclose(fp2);
    return 0;
}

/*
 * get_number: read maximum of 8 number digit
 *             following numbers would be rejected
 *             - any numnber more than 8 digits
 *             - any number that looks like hex, but had non-hex in it
 *             - ignores all white spaces, ' ', '\t'
 *             - ignores all '\r', '\n'
 *             - empty file
 * @fp: pointer to file to be read
 * @hex: hex flag
 * @n: pointer to n
 * @eol: pointer to end of line
 * return -1 on error; otherwise return 0 on success
 */
int get_number(FILE *fp, int hex, unsigned int *n, int *eol)
{
    int d = 0, i = 0;
    char c = 0, t[9] = { 0 };

    if (!n) return -1;
    if (!fp) return -1;
    for (; (c=fgetc(fp))!=EOF;)
        if ( (i == 0) && (c==' ' || c=='\t')) continue;
        else if ((i!=0) && (c==' ' || c=='\t' || c == '\r' || c == '\n'))
            break;
        else if (!isdigit(c)) return -1;
        else t[i++]=c;
    if (c==EOF && i==0) return -1;
    if (i>8) return -1;
    if (c=='\n') *eol=1;
    if(hex) {
        sscanf(t,"%x", &d);
    } else {
        sscanf(t,"%d", &d);
    }
    *n = d;
    return 0;
}

inline unsigned char tohex(const unsigned char c)
{
    return ((c >= '0') && (c <= '9')) ? (c-'0') : (0xa+(c-'a'));
}

/*
 * get_xstream_2_xbytes: convert to hex byte stream irrespective of case
 * @fp: pointer to file to be read
 * @byte_stream: pointer to byte stream
 * @len: number of bytes
 * @eol: pointer to end of line
 * return hex converted output; otherwise -1 on error
 */
int get_xstream_2_xbytes(FILE *fp, u_int8_t *byte_stream, int len, int *eol)
{
    int i = 0, xi = 0;
    char c = 0;
    /* len is number of hex bytes, eg, len of ff is actually 1, it takes
     * two hex digits of length 4 bits each,
     * passed length is assumed to be number of bytes not number of nibbles
     */
    if (!byte_stream) return -1;
    if (len < 0) return -1;

    for (;((c=fgetc(fp))!=EOF)  ;)  {
        if (i==0 && (c==' ' || c=='\t')) continue;
        else if ((i!=0) && (c==' ' || c=='\t' || c=='\r' || c=='\n')) break;
        else if (!isxdigit(c)) return -1;
        else {
            if (!(i%2)) byte_stream[xi]=tohex(tolower((unsigned char)c));
            else {
                byte_stream[xi] = byte_stream[xi] << 4 | tohex(tolower((unsigned char)c));
                xi++;
            }
            i++;
        }
        if (i>len) return -1;
    }
    if (c=='\n') *eol=1;
    if (xi != len/2) return -1;
    return xi;
}

/*
 * ignore_extra_bytes: ignore and flush extra bytes
 * @fp: pointer to the file
 * @eol: pointer to end of line
 * return -1 on failure; otherwise 0 on success
 */
int ignore_extra_bytes(FILE *fp, int *eol)
{
    char c = 0;
    /* flush out any thing buffered in stdout */
    fflush(stdout);
    fprintf(stderr, "\nIgnoring extra bytes at end\n");
    for (; (c=fgetc(fp))!= EOF;) {
        putc(c, stderr);
        fflush(stderr);
        if (c=='\n') {
            *eol=1;
            return 0;
        }
    }
    return -1;
}

enum {
    FIPS_SUCCESS=0,
    FIPS_INVALID_COMMAND=1,
    FIPS_INVALID_MODE=2,
    FIPS_INVALID_KEYLEN=3,
    FIPS_INVALID_DATALEN=4,
    FIPS_RANGE_CHECK_FAIL=5,
    FIPS_INVALID_KEY_MISMATCH=6,
    FIPS_INVALID_DATA_MISMATCH=7,
    FIPS_INVALID_EXPOP_MISMATCH=8,
    FIPS_MALLOC_FAIL=9,
    FIPS_TEST_FAIL=10,
};

char *fips_error[]={
    "fips_success",
    "fips_invalid_command",
    "fips_invalid_mode",
    "fips_invalid_keylen",
    "fips_invalid_datalen",
    "fips_range_check_fail",
    "fips_invalid_key_mismatch",
    "fips_invalid_data_mismatch",
    "fips_invalid_expop_mismatch",
    "fips_malloc_fail",
    "fips_test_fail"
};

static int xor_with_block_output(int len, u_int8_t* text, u_int8_t* exp_text, u_int8_t* block, int decrypt)
{
    int i = 0;

    if (decrypt == 1)
        printf("\n Expected Plaintext \n");
    else if (decrypt == 0)
        printf("\n Expected Ciphertext \n");
    for (i = 0; i < len; i++)
        printf("%02x ", exp_text[i]);

    /* XORing text and block output */
    for (i = 0; i < len; i++)
        text[i] = text[i] ^ block[i];

    if (decrypt == 1)
        printf("\n Plaintext Output \n");
    else if (decrypt == 0)
        printf("\n Ciphertext Output \n");
    for (i = 0; i < len; i++)
        printf("%02x ", text[i]);

    if (memcmp(text, exp_text, len) == 0)
        return 0;
    else
        return 1;
}

/*
 * set_fips: set and validate FIPS implementation
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int set_fips(const char *ifname, int argc, char *argv[])
{
    int err, i, size, en = 0, fips_fail = 0;
    unsigned int cmd, mode, keylen, datalen;
    const char *decrypt;
    struct ath_ioctl_fips *req=NULL;
    struct ath_fips_output *output_fips = NULL;
    u_int8_t *ptr = NULL;
    FILE *fp = NULL;
    u_int8_t key[32] = {0};
    u_int8_t data[1500] = {0};
    u_int8_t exp_op[1500] = {0};
    u_int8_t plaintext[1500] = {0};
    u_int8_t ciphertext[1500] = {0};
    u_int8_t iv[16] = {0};
    int eol=0;
    struct ieee80211req_athdbg dbg;
    if(argc < 4) {
        printf("%s:%d\n Incorrect Usage!\n",__func__,__LINE__);
        usage();
    } else {
        fp = fopen (argv[3] , "r");
        if (fp == NULL) {
            printf("\n Unable to open given file %s\n", argv[3]);
            return -EFAULT;
        }
        if (argc > 4)
            decrypt = argv[4];
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        /* validate the file contents */
        while (ftell(fp) < size)
        {
            if (-1 == get_number(fp, 0, &cmd, &eol) || eol) {
                en=FIPS_INVALID_COMMAND;
                goto close_n_exit;
            }
            if (-1 == get_number(fp, 0, &mode, &eol) || eol) {
                en=FIPS_INVALID_MODE;
                goto close_n_exit;
            }
            if (-1 == get_number(fp, 0, &keylen, &eol) || eol) {
                en=FIPS_INVALID_KEYLEN;
                goto close_n_exit;
            }
            if (-1 == get_number(fp, 0, &datalen, &eol) || eol) {
                en=FIPS_INVALID_DATALEN;
                goto close_n_exit;
            }
            if ((cmd < 0) || (cmd > 1)  || (mode <0) || (mode > 1)||\
                    ((keylen != 16) && (keylen != 32)) ||\
                    ((datalen > 1488) ||\
                     (datalen < 16) || (datalen%16))) {
                en=FIPS_RANGE_CHECK_FAIL;
                goto close_n_exit;
            }

            if (keylen != get_xstream_2_xbytes(fp, key, keylen*2, &eol) || eol){
                en=FIPS_INVALID_KEY_MISMATCH;
                goto close_n_exit;
            }
            if (datalen != get_xstream_2_xbytes(fp, data, datalen*2, &eol) || eol){
                en=FIPS_INVALID_DATA_MISMATCH;
                goto close_n_exit;
            }
            /* there is possbility if IV not being present in the text file,
               so file ends with only expected output, just do not break after
               here, because we do not use IV any ways
             */
            if (datalen != get_xstream_2_xbytes(fp, exp_op, datalen*2, &eol)){
                en=FIPS_INVALID_EXPOP_MISMATCH;
                goto close_n_exit;
            }
            if (argc > 4) {
                if (datalen != get_xstream_2_xbytes(fp, plaintext, datalen*2, &eol) || eol){
                    en=FIPS_INVALID_EXPOP_MISMATCH;
                    goto close_n_exit;
                }
                if (datalen != get_xstream_2_xbytes(fp, ciphertext, datalen*2, &eol)){
                    en=FIPS_INVALID_EXPOP_MISMATCH;
                    goto close_n_exit;
                }
            }
            /* if IV is not present, do not break */
            if (!eol) {
                /* more likely IV is present, there could be a chance that there
                 * are few spaces and extra white spaces. That need to be ignored
                 */
                if (16 != get_xstream_2_xbytes(fp, iv, 16*2, &eol)){
                    if (eol) {
                        fprintf(stderr, "Invalid or no iv is present, ignoring for now \n");
                    }
                }
            } else {
                memset(iv, 0, 16);
            }

            /* Allocating ath_ioctl_fips for sending to driver. Size would be dynamic based on input data length*/
            req = (struct ath_ioctl_fips *) malloc(sizeof(struct ath_ioctl_fips) + (datalen - sizeof(u_int32_t)));
            if (!req)  {
                en=FIPS_MALLOC_FAIL;
                goto close_n_exit;
            }
            req->fips_cmd = cmd;    /* 1 - encrypt/ 2 - decrypt*/
            req->key_len = keylen;  /* no of bytes*/
            req->data_len = datalen;/* no of bytes*/
            req->mode = mode;
            memcpy(req->key, key, req->key_len);
            memcpy(req->data, data, req->data_len);

            memcpy(req->iv, iv, 16);

            dbg.cmd = IEEE80211_DBGREQ_FIPS;
            dbg.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
            dbg.data.fips_req.data_addr = (void *) req;
            dbg.data.fips_req.data_size = sizeof(struct ath_ioctl_fips) +
                (req->data_len - sizeof(u_int32_t));
            err = send_command(&sock_ctx, ifname, &dbg, sizeof(dbg), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
            sleep (1);
            if (err < 0) {
                errx(1, "Unable to set fips");
                en=FIPS_TEST_FAIL;
                goto close_n_exit;
            }

            /* Read the output text from fips_req.data_addr */
            if (req) {
                output_fips = (struct ath_fips_output *)req;
                if (output_fips->error_status == 0) {
                    printf("\n Output Data Length:%d",
                            output_fips->data_len);
                    printf("\n Output Data from Crypto Engine\n");

                    ptr = (u_int8_t *) (output_fips->data);
                    for(i=0; i < output_fips->data_len; i++)
                    {
                        printf("%02x ",(*(ptr+i)));
                    }
                    if (output_fips->data_len < sizeof(exp_op) && memcmp(ptr, exp_op, output_fips->data_len) == 0)
                        fips_fail = 0;
                    else
                        fips_fail = 1;

                    if (argc < 5) {
                        printf("\n Expected Output \n");
                        for (i = 0; i < datalen; i++)
                            printf("%02x ", exp_op[i]);
                        if (fips_fail == 1)
                            printf("\n Known Answer Test Failed\n");
                        else
                            printf("\n Known Answer Test Passed\n");
                    } else {
                        printf("\n Expected Block Output \n");
                        for (i = 0; i < datalen; i++)
                            printf("%02x ", exp_op[i]);
                        if (streq(decrypt, "decrypt") && output_fips->data_len < sizeof(exp_op)) {
                                fips_fail = xor_with_block_output(output_fips->data_len, ciphertext, plaintext, exp_op, 1);
                        } else if (streq(decrypt, "encrypt") && output_fips->data_len < sizeof(exp_op)) {
                                fips_fail = xor_with_block_output(output_fips->data_len, plaintext, ciphertext, exp_op, 0);
                        }
                        if (fips_fail == 1)
                            printf("\n Known Answer Test Failed\n");
                        else
                            printf("\n Known Answer Test Passed\n");
                    }
                } else {
                    printf("\nOutput Error status from Firmware returned: %d\n",
                            output_fips->error_status);
                }
            }

            /* Freeing allocated ath_ioctl_fips data structure */
            if(req) {
                free(req);
                req=NULL;
            }
            /* ignore any junk in the line of the file, until we get \r\n */
            if(!eol) {
                ignore_extra_bytes(fp, &eol);
            }
            eol=0;
        }
        fclose(fp);
    }
    return 0;

close_n_exit:
    if(fp) fclose(fp);
    if(req) free(req);
    fflush(stdout);
    {
        int i = 0;

        for (i = 0; i < 7; i++)
            fprintf(stderr, "%s\n", fips_error[i]);
    }
    fprintf(stderr, "Input Error: Please fix them first error code :%s", fips_error[en]);
    fflush(stderr);
    return -1;
}

/* offchan_tx_test: set offchan transmission test parameters (channel and dwell time)
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int offchan_tx_test(const char *ifname, int argc, char *argv[])
{
#define MIN_OFFCHAN_PARAM_COUNT  5
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_offchan_tx_test_t *offchan_req;

    if ((argc != MIN_OFFCHAN_PARAM_COUNT)) {
        usage_offchan_tx_test();
        return -1;
    }

    offchan_req = (ieee80211_offchan_tx_test_t *)&req.data.offchan_req;

    req.cmd = IEEE80211_DBGREQ_OFFCHAN_TX;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    offchan_req->ieee_chan = atoi(argv[3]);
    offchan_req->dwell_time = atoi(argv[4]);

    if (send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ) < 0) {
            errx(1, "ieee80211_ioctl_dbgreq: IEEE80211_DBGREQ_OFFCHAN_TX failed");
    }

    return 0;
#undef MIN_OFFCHAN_PARAM_COUNT
}

int offchan_rx_get_params(char *str, uint8_t *bw_mode, uint8_t *sec_chan_offset)
{
  uint8_t ret;
  *sec_chan_offset = OFFCHAN_RX_SCN;

  if (strcmp("40-", str) == 0) {
      *bw_mode = OFFCHAN_RX_BANDWIDTH_40MHZ;
      *sec_chan_offset = OFFCHAN_RX_SCB;
  } else if (strcmp("40+", str) == 0) {
      *bw_mode = OFFCHAN_RX_BANDWIDTH_40MHZ;
      *sec_chan_offset = OFFCHAN_RX_SCA;
  } else {
      ret = strtol(str, NULL, 10);
      switch(ret) {
             case 20: *bw_mode = OFFCHAN_RX_BANDWIDTH_20MHZ;
                       break;
             case 40: *bw_mode = OFFCHAN_RX_BANDWIDTH_40MHZ;
                       break;
             case 80: *bw_mode = OFFCHAN_RX_BANDWIDTH_80MHZ;
                       break;
             case 160: *bw_mode = OFFCHAN_RX_BANDWIDTH_160MHZ;
                        break;
             default:  return -1;
      }
  }
  return 0;
}
/*
 * offchan_rx_test: set offchan reception test parameters (channel and dwell time)
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int offchan_rx_test(const char *ifname, int argc, char *argv[])
{
#define MIN_OFFCHAN_PARAM_COUNT  5
#define MAX_OFFCHAN_PARAM_COUNT  6
    int status;
    struct ieee80211req_athdbg req = { 0 };
    ieee80211_offchan_tx_test_t *offchan_req;
    uint8_t bw_mode = OFFCHAN_RX_BANDWIDTH_20MHZ;
    uint8_t sec_chan_offset = OFFCHAN_RX_SCN;
    if (!(argc ==  MIN_OFFCHAN_PARAM_COUNT || argc == MAX_OFFCHAN_PARAM_COUNT)) {
        usage_offchan_rx_test();
        return -1;
    }

    offchan_req = (ieee80211_offchan_tx_test_t *)&req.data.offchan_req;

    req.cmd = IEEE80211_DBGREQ_OFFCHAN_RX;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    offchan_req->ieee_chan = atoi(argv[3]);
    offchan_req->dwell_time = atoi(argv[4]);

    if(argv[5]) {
        status = offchan_rx_get_params(argv[5], &bw_mode, &sec_chan_offset);
        if (status == -1) {
             printf("Invalid Bandwidth \n");
             usage_offchan_rx_test();
             return status;
        }
    }

    offchan_req->wide_scan.bw_mode = bw_mode;
    offchan_req->wide_scan.sec_chan_offset = sec_chan_offset;

    if (send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ) < 0) {
            errx(1, "ieee80211_ioctl_dbgreq: IEEE80211_DBGREQ_OFFCHAN_TX failed");
    }

    return 0;
#undef MIN_OFFCHAN_PARAM_COUNT
#undef MAX_OFFCHAN_PARAM_COUNT
}
/*
 * coex_cfg: wlan coexistence command for cfg80211
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
int coex_cfg(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    int i;

    if (argc < 4) {
        usage_coex_cfg();
        return -1;
    } else {
        req.cmd = IEEE80211_DBGREQ_COEX_CFG;
        if (!strncmp (argv[3], "0x", 2)) {
            sscanf(argv[3], "%x", &req.data.coex_cfg_req.type);
        } else {
            sscanf(argv[3], "%d", &req.data.coex_cfg_req.type);
        }
        for (i = 0; i < COEX_CFG_MAX_ARGS; i++) {
            if (argv[i + 4] == NULL)
                break;
            if (!strncmp (argv[i + 4], "0x", 2)) {
                sscanf(argv[i + 4], "%x", &req.data.coex_cfg_req.arg[i]);
            } else {
                sscanf(argv[i + 4], "%d", &req.data.coex_cfg_req.arg[i]);
            }
        }

        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    }
    return 0;
}

/*
 * cfg80211_event_getwifi: Command for cfg80211 event to establish wifi
 * @ifname: interface name
 * @cmdid: enum value as command id
 * @buffer: pointer to buffer received
 * @len: total buffer length
 */
void cfg80211_event_getwifi(char *ifname, int cmdid, void *buffer,
        uint32_t len)
{
    struct ieee80211req_athdbg_event *dbg_event;
    struct dbg_event_q_entry *q_entry;

    if (buffer == NULL) {
        printf("ERROR!! DBGREQ received with NULL buffer\n");
        return;
    }

    dbg_event = buffer;

    if (cmdid != QCA_NL80211_VENDOR_SUBCMD_DBGREQ) {
        /*
         * Wifitool is only interested in vendor events of type
         * QCA_NL80211_VENDOR_SUBCMD_DBGREQ
         */
        return;
    }

    if (!if_name_filter_set(ifname)) {
        /*
         * Only accept events coming from the correct interface
         */
        return;
    }

    if (!event_filter_array[dbg_event->cmd]) {
        /*
         * Ignore events which no body is interested in
         */
        return;
    }

    q_entry = malloc(sizeof(*q_entry));

    if (q_entry == NULL) {
        fprintf(stderr, "%s:%d Could not allocate memory\n", __func__, __LINE__);
        return;
    }

    q_entry->event = malloc(len);

    if (q_entry->event == NULL) {
        free(q_entry);
        fprintf(stderr, "%s:%d Could not allocate memory\n", __func__, __LINE__);
        return;
    }

    memcpy(q_entry->event, dbg_event, len);
    q_entry->total_buf_size = len;
    q_insert(&event_q, q_entry);
}

#if UMAC_SUPPORT_CFG80211
static void nl80211_vendor_event_qca_parse_get_wifi(char *ifname,
        uint8_t *data, size_t len)
/*
 * nl80211_vendor_event_qca_parse_get_wifi: nl80211 vendor event to get wifi configuration
 * @ifname: interface name
 * @data: pointer to data
 * @len: length of the data
 */
{
    struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
    struct nlattr *tb;
    void *buffer = NULL;
    uint32_t buffer_len = 0;
    uint32_t subcmd;

    if (nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
                (struct nlattr *) data, len, NULL)) {
        printf("INVALID EVENT\n");
        return;
    }
    tb = tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND];
    if (!tb) {
        printf("ERROR!!!GENERIC CMD not found within get-wifi subcmd\n");
        return;
    }
    subcmd = nla_get_u32(tb);

    tb = tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA];
    if (tb) {
        buffer = nla_data(tb);
        buffer_len = nla_len(tb);
        cfg80211_event_getwifi(ifname, subcmd, buffer, buffer_len);
    }
}

/*
 * cfg80211_event_callback: cfg80211 event callback to get wifi configuration
 * @ifname: interface name
 * @subcmd: enum value for sub command
 * @data: pointer to the data
 * @len: length of the data
 */
void cfg80211_event_callback(char *ifname,
        uint32_t subcmd, uint8_t *data, size_t len)
{
    switch(subcmd) {
        case QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION:
            nl80211_vendor_event_qca_parse_get_wifi(ifname, data, len);
            break;
    }
}
#endif

/*
 * set_acl_softblocking: set ACL softblocking for given dest MAC addr
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
#if ATH_ACL_SOFTBLOCKING
static int set_acl_softblocking(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if (argc != 5) {
        usage_set_acl_softblocking();
        return -1;
    }
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_SET_SOFTBLOCKING;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    if (!wifitool_mac_aton(argv[3], req.dstmac, 6)) {
        errx(1, "Invalid destination mac address");
        return -1;
    }
    req.data.acl_softblocking = atoi(argv[4]);
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
}

/*
 * get_acl_softblocking: get ACL softblocking for given dest MAC addr
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * return -1 on error; otherwise return 0 on success
 */
static int get_acl_softblocking(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req = { 0 };
    if (argc != 4) {
        usage_get_acl_softblocking();
        return -1;
    }
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_GET_SOFTBLOCKING;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
        errx(1, "Invalid mac address");
        return -1;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    printf("Softblocking for %s on %s: %s\n", argv[3], ifname,
            req.data.acl_softblocking ? "yes" : "no");
    return 0;
}
#endif

/*
 * usage_dptrace: usage prints for data path trace operations
 */
void usage_dptrace()
{
    fprintf(stderr, "usage\n");
    fprintf(stderr, " wifitool ath0 dp_trace pbitmap <value>\n");
    fprintf(stderr, " wifitool ath0 dp_trace nrecords <value>\n");
    fprintf(stderr, " wifitool ath0 dp_trace verbosity <value>\n");
    fprintf(stderr, " wifitool ath0 dp_trace dump_all <value> <pdev_id>\n");
    fprintf(stderr, " wifitool ath0 dp_trace livemode_on\n");
    fprintf(stderr, " wifitool ath0 dp_trace livemode_off\n");
    fprintf(stderr, " wifitool ath0 dp_trace clear_buf\n");
}

/*
 * set_dptrace: set parameters related to data path trace operations
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 */
void set_dptrace(const char *ifname, int argc, char *argv[])
{
    struct ieee80211req_athdbg req;
    if (argc < 4 || argc > 6) {
        usage_dptrace();
        return;
    }

    if (strncmp(argv[2], "dp_trace", 8))
        return;

    if (!strncmp(argv[3], "pbitmap", strlen("pbitmap"))) {
        if (!argv[4])
            return;
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_PROTO_BITMAP;
        req.data.dptrace.val1 = atoi(argv[4]);
    } else if (!strncmp(argv[3], "nrecords", strlen("nrecords"))) {
        if (!argv[4])
            return;
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_NO_OF_RECORD;
        req.data.dptrace.val1 = atoi(argv[4]);
    } else if (!strncmp(argv[3], "verbosity", strlen("verbosity"))) {
        if (!argv[4])
            return;
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_VERBOSITY;
        req.data.dptrace.val1 = atoi(argv[4]);
    } else if (!strncmp(argv[3], "dump_all", strlen("dump_all"))) {
        if (!argv[4])
            return;
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_DUMPALL;
        req.data.dptrace.val1 = atoi(argv[4]);
        req.data.dptrace.val2 = atoi(argv[5]);
    } else if (!strncmp(argv[3], "livemode_on", strlen("livemode"))) {
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_LIVEMODE_ON;
    } else if (!strncmp(argv[3], "livemode_off", strlen("livemode"))) {
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_LIVEMODE_OFF;
    } else if (!strncmp(argv[3], "clear_buff", strlen("clear_buff"))) {
        req.data.dptrace.dp_trace_subcmd = IEEE80211_DBG_DPTRACE_CLEAR;
    }
    req.cmd = IEEE80211_DBGREQ_DPTRACE;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
}

int bsscolor_unit_test_cmd(const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num;
    char usage_msg[90];

    switch(req_cmd) {
        case IEEE80211_DBGREQ_BSSCOLOR_DOT11_COLOR_COLLISION_AP_PERIOD:
            strlcpy(usage_msg, "Usage: wifitool athX "
                              "bsscolor_collision_ap_period "
                              "<dot11BSSColorCollisionAPPeriod>\n", 90);
        break;
        case IEEE80211_DBGREQ_BSSCOLOR_DOT11_COLOR_COLLISION_CHG_COUNT:
            strlcpy(usage_msg, "Usage: wifitool athX "
                              "bsscolor_color_change_announcemt_count "
                              "<change_count>\n", 90);
        break;
        default:
            fprintf(stderr, "unhandeled bsscolor cmd id\n");
            return -1;
        break;
    }

    arg_num = 4;
    if (argc != arg_num) {
        fprintf(stderr, "%s", usage_msg);
        return -1;
    }
    req.data.param[0] = atoi(argv[3]);

    req.cmd         = req_cmd;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL,
            QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
}
#ifdef WLAN_SUPPORT_TWT

/* ap_twt_dialog_req_cmd: AP target wake time (TWT) operations of adding, deleting, pausing, and resuming dialog
 *                        currently supports: ap_twt_add_dialog_req, ap_twt_del_dialog_req, ap_twt_pause_dialog_req, ap_twt_resume_dialog_req
 * @ifname: interface name
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * @req_cmd: enum value for function specific command
 * return -1 on error; otherwise return 0 on success
 */
int ap_twt_dialog_req_cmd (const char *ifname, int argc, char *argv[], int req_cmd)
{
    struct ieee80211req_athdbg req = { 0 };
    int arg_num, cmd;
    char usage_msg[150];

    switch (req_cmd) {
        case IEEE80211_DBGREQ_TWT_ADD_DIALOG:
            cmd = IEEE80211_DBGREQ_TWT_ADD_DIALOG;
            arg_num = 11;
            strlcpy(usage_msg,"Usage:wifitool athX ap_twt_add_dialog <peer_mac> <dialog id>"
                    "<wake intvl(us)> <wake int mantis> <wake dur(us)> <sp offset(us)> <cmd (0-7)> <flags> \n",sizeof(usage_msg));
            break;

        case IEEE80211_DBGREQ_TWT_DEL_DIALOG:
            cmd = IEEE80211_DBGREQ_TWT_DEL_DIALOG;
            arg_num = 5;
            strlcpy(usage_msg,"wifitool athX ap_twt_del_dialog <peer_mac> <dialog id>\n",sizeof(usage_msg));
            break;

        case IEEE80211_DBGREQ_TWT_PAUSE_DIALOG:
            cmd = IEEE80211_DBGREQ_TWT_PAUSE_DIALOG;
            arg_num = 5;
            strlcpy(usage_msg,"wifitool athX ap_twt_pause_dialog <peer_mac> <dialog id>\n",sizeof(usage_msg));
            break;

        case IEEE80211_DBGREQ_TWT_RESUME_DIALOG:
            cmd = IEEE80211_DBGREQ_TWT_RESUME_DIALOG;
            arg_num = 7;
            strlcpy(usage_msg,"wifitool athX ap_twt_resume_dialog <peer_mac> <dialog id> <sp offset(us)> <next_twt_size>\n",sizeof(usage_msg));
            break;

        default:
            return -1;
    }
    if (argc != arg_num) {
        fprintf(stderr, "%s", usage_msg);
        return -1;
    }
    req.cmd = cmd;
    req.needs_reply = DBGREQ_REPLY_IS_NOT_REQUIRED;

    if (!wifitool_mac_aton(argv[3], req.dstmac, 6)) {
        errx(1, "Invalid mac address");
        return -1;
    }

    switch (req_cmd) {
        case IEEE80211_DBGREQ_TWT_ADD_DIALOG:
            req.data.twt_add.dialog_id = atoi(argv[4]);
            req.data.twt_add.wake_intvl_us = atoi(argv[5]);
            req.data.twt_add.wake_intvl_mantis = atoi(argv[6]);
            req.data.twt_add.wake_dura_us = atoi(argv[7]);
            req.data.twt_add.sp_offset_us = atoi(argv[8]);
            req.data.twt_add.twt_cmd = atoi(argv[9]);
            req.data.twt_add.flags = atoi(argv[10]);
            break;

        case IEEE80211_DBGREQ_TWT_DEL_DIALOG:
            req.data.twt_del_pause.dialog_id = atoi(argv[4]);
            break;

        case IEEE80211_DBGREQ_TWT_PAUSE_DIALOG:
            req.data.twt_del_pause.dialog_id = atoi(argv[4]);
            break;

        case IEEE80211_DBGREQ_TWT_RESUME_DIALOG:
            req.data.twt_resume.dialog_id = atoi(argv[4]);
            req.data.twt_resume.sp_offset_us = atoi(argv[5]);
            req.data.twt_resume.next_twt_size = atoi(argv[6]);
            break;

        default:
            return -1;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return 0;
}
#endif

static void set_innetwork_2g_mac(const char *ifname, int argc, char *argv[]) {
    struct ieee80211req_athdbg req = {0};
    if ((argc < 5) || (argc > 5)) {
        usage_set_innetwork_2g();
        return;
    }
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_BSTEERING_SETINNETWORK_2G;
    req.data.param[0] = atoi(argv[4]);
    if (!wifitool_mac_aton(argv[3], &req.dstmac[0], 6)) {
        errx(1, "Invalid mac address");
        return;
    }
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    return;
}

static void get_last_four_beacon_rssi(const char *ifname, int argc, char *argv[])
{
    int err;
    struct ieee80211req_athdbg req = {0};
    beacon_rssi_info *info = NULL;
    int i, mask;

    if ((argc < 4) || (argc > 4))
    {
        fprintf(stdout, "wifitool athX get_last_beacon_rssi\n");
        return;
    }

    if (!wifitool_mac_aton(argv[3], &req.data.beacon_rssi_info.macaddr[0], 6)) {
        fprintf(stdout, "Invalid MAC address\n");
        return;
    }

    req.cmd = IEEE80211_DBGREQ_LAST_BEACON_RSSI;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (err < 0) {
        fprintf(stdout, "Command failed: %d\n", err);
        return;
    }

    info = &req.data.beacon_rssi_info;

    fprintf(stdout, "%-10s%-10s%-10s%-10s%-10s\n", "Beacon", "RSSI", "Age", "Count", "jiffies");
    fprintf(stdout, "---------------------------------------------------------------------------\n");
    mask = 0xFF;
    for (i = 0; i < 4; i++) {
        fprintf(stdout, "%-10d%-10u%-10u%-10u%-10llu\n", (i + 1), info->stats.ni_last_bcn_rssi & mask,
        info->stats.ni_last_bcn_age & mask, info->stats.ni_last_bcn_cnt, info->stats.ni_last_bcn_jiffies);
        info->stats.ni_last_bcn_rssi  = info->stats.ni_last_bcn_rssi >> 8;
        info->stats.ni_last_bcn_age = info->stats.ni_last_bcn_age >> 8;
    }
}

static void get_last_four_ack_rssi(const char *ifname, int argc, char *argv[])
{
    int err;
    struct ieee80211req_athdbg req = {0};
    ack_rssi_info *info = NULL;
    int i, mask;

    if ((argc < 4) || (argc > 4))
    {
        fprintf(stdout, "wifitool athX get_last_ack_rssi\n");
        return;
    }

    if (!wifitool_mac_aton(argv[3], &req.data.ack_rssi_info.macaddr[0], 6)) {
        fprintf(stdout, "Invalid MAC address\n");
        return;
    }

    req.cmd = IEEE80211_DBGREQ_LAST_ACK_RSSI;
    req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;

    err = send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    if (err < 0) {
        fprintf(stdout, "Command failed: %d\n", err);
        return;
    }

    info = &req.data.ack_rssi_info;

    fprintf(stdout, "%-10s%-10s%-10s%-10s%-10s\n", "ACK", "RSSI", "Age", "Count", "jiffies");
    fprintf(stdout, "---------------------------------------------------------------------------\n");
    mask = 0xFF;
    for (i = 0; i < 4; i++) {
        fprintf(stdout, "%-10d%-10u%-10u%-10u%-10llu\n", (i + 1), info->stats.ni_last_ack_rssi & mask,
        info->stats.ni_last_ack_age & mask, info->stats.ni_last_ack_cnt, info->stats.ni_last_ack_jiffies);
        info->stats.ni_last_ack_rssi  = info->stats.ni_last_ack_rssi >> 8;
        info->stats.ni_last_ack_age = info->stats.ni_last_ack_age >> 8;
    }
}


static void get_innetwork_2g_mac(const char *ifname, int argc, char *argv[]) {
    int len, i, param, num_entries = 0, ch;
    struct ieee80211req_athdbg req = {0};

    if ((argc < 5) || (argc > 5)) {
        usage_get_innetwork_2g();
        return;
    }

    memset(&req, 0, sizeof(struct ieee80211req_athdbg));
    req.cmd = IEEE80211_DBGREQ_BSTEERING_GETINNETWORK_2G;
    req.data.innetwork_2g_req.index = 0;
    req.data.innetwork_2g_req.data_addr = &num_entries;
    send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);
    fprintf(stdout, " Total Entries : %d\n", num_entries);

    param = atoi(argv[3]);
    if (!param) {

        len = atoi(argv[4]);
        if (len > num_entries || !len) {
            fprintf(stderr, " Enter valid entry/entry less than : %d\n", num_entries);
            return;
        }

        struct ieee80211_bsteering_in_network_2G_table *table_2g = NULL;
        table_2g = (void *)malloc(len * sizeof(struct ieee80211_bsteering_in_network_2G_table));
        if (NULL == table_2g) {
            errx(1, "insufficient memory \n");
            return;
        }

        (void)memset(table_2g, 0, len * sizeof(struct ieee80211_bsteering_in_network_2G_table));
        req.data.innetwork_2g_req.index = len;
        req.data.innetwork_2g_req.ch = 0;
        req.data.innetwork_2g_req.data_addr = table_2g;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

        fprintf(stdout, " Number of Entries : %d\n", table_2g->total_index);
        if (table_2g->total_index != 0) {
            fprintf(stdout, "-------------------------\n");
            fprintf(stdout, " STA MAC         STA CH\n");
            fprintf(stdout, "-------------------------\n");
            for (i = 0; i < table_2g->total_index; i++) {
                fprintf(stdout, "%2x:%2x:%2x:%2x:%2x:%2x  %d\n", table_2g[i].mac_addr[0],
                        table_2g[i].mac_addr[1], table_2g[i].mac_addr[2], table_2g[i].mac_addr[3],
                        table_2g[i].mac_addr[4], table_2g[i].mac_addr[5], table_2g[i].ch);
            }
        } else
            fprintf(stdout, "no data\n");
        free(table_2g);
    } else {
        ch = atoi(argv[4]);
        if (ch > 11 || ch < 1) {
            fprintf(stderr, " Enter valid 2g channel \n");
            return;
        }

        u_int8_t zeroMAC[6] = {0, 0, 0, 0, 0, 0};
        u_int8_t mac_addr[128][IEEE80211_ADDR_LEN] = {0};
        req.data.innetwork_2g_req.index = num_entries;
        req.data.innetwork_2g_req.ch = ch;
        req.data.innetwork_2g_req.data_addr = mac_addr;
        send_command(&sock_ctx, ifname, &req, sizeof(req), NULL, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, IEEE80211_IOCTL_DBGREQ);

        if (num_entries != 0) {
            fprintf(stdout, "-------------------------\n");
            fprintf(stdout, " STA MAC         STA CH\n");
            fprintf(stdout, "-------------------------\n");
            for (i = 0; i < num_entries; i++) {
                if (!memcmp(zeroMAC, mac_addr[i], IEEE80211_ADDR_LEN)) {
                    continue;
                }
                fprintf(stdout, "%2x:%2x:%2x:%2x:%2x:%2x  %d\n", mac_addr[i][0], mac_addr[i][1],
                        mac_addr[i][2], mac_addr[i][3], mac_addr[i][4], mac_addr[i][5], ch);
            }
        } else
            fprintf(stdout, "no data\n");
    }
    return;
}

int main(int argc, char *argv[])
{
    const char *ifname, *cmd;

    if (argc < 3)
    {
        printf("%s:%d",__func__,__LINE__);
        usage();
    }

    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext?cfg80211)
     */
    sock_ctx.cfg80211 = get_config_mode_type();

    if(streq(argv[argc-1],"-cfg80211") ||
            streq(argv[argc-1],"--cfg80211")) {
        --argc;
        if (!sock_ctx.cfg80211){
            fprintf(stderr, "Invalid tag 'cfg80211' for current mode.\n");
            return -EINVAL;
        }
        sock_ctx.cfg80211 = 1;
    }
    ifname = argv[1];
    cmd = argv[2];

#if UMAC_SUPPORT_CFG80211
    if (sock_ctx.cfg80211 && streq(cmd, "setUnitTestCmdEvent")) {
        sock_ctx.cfg80211_ctxt.event_callback = cfg80211_event_callback;
    }
#endif

    q_init(&event_q);
    init_socket_context(&sock_ctx, WIFI_NL80211_CMD_SOCK_ID, WIFI_NL80211_EVENT_SOCK_ID);

    if (streq(cmd, "dp_trace")) {
        set_dptrace(ifname, argc, argv);
    } else if (streq(cmd, "fips")) {
        set_fips(ifname, argc, argv);
    } else if (streq(cmd, "sendaddba")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_SENDADDBA);
    } else if (streq(cmd, "senddelba")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_SENDDELBA);
    } else if (streq(cmd, "setaddbaresp")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_SETADDBARESP);
    } else if (streq(cmd, "refusealladdbas")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_REFUSEALLADDBAS);
    } else if (streq(cmd, "sendsingleamsdu")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_SENDSINGLEAMSDU);
    } else if (streq(cmd, "getaddbastats")) {
        ba_opt(ifname, argc, argv, IEEE80211_DBGREQ_GETADDBASTATS);
    } else if (streq(cmd, "sendbcnrpt")) {
        send_rpt(ifname, argc, argv, IEEE80211_DBGREQ_SENDBCNRPT);
    } else if (streq(cmd, "sendtsmrpt")) {
        send_rpt(ifname, argc, argv, IEEE80211_DBGREQ_SENDTSMRPT);
    } else if (streq(cmd, "sendneigrpt")) {
        send_rpt(ifname, argc, argv, IEEE80211_DBGREQ_SENDNEIGRPT);
    } else if (streq(cmd, "sendlmreq")) {
        send_req(ifname, argc, argv, IEEE80211_DBGREQ_SENDLMREQ);
    } else if (streq(cmd, "sendbstmreq")) {
        send_req(ifname, argc, argv, IEEE80211_DBGREQ_SENDBSTMREQ);
    } else if (streq(cmd, "sendbstmreq_target")) {
        send_bstmreq_target(ifname, argc, argv);
    } else if (streq(cmd, "setbssidpref")) {
        set_bssidpref(ifname, argc, argv);
    } else if (streq(cmd, "senddelts")) {
        send_ts_cmd(ifname, argc, argv, IEEE80211_DBGREQ_SENDDELTS);
    } else if (streq(cmd, "sendaddts")) {
        send_ts_cmd(ifname, argc, argv, IEEE80211_DBGREQ_SENDADDTSREQ);
    } else if (streq(cmd, "sendchload")) {
        send_chn_cmd(ifname, argc, argv, IEEE80211_DBGREQ_SENDCHLOADREQ);
    } else if (streq(cmd, "sendnhist")) {
        send_chn_cmd(ifname,argc,argv, IEEE80211_DBGREQ_SENDNHIST);
    } else if (streq(cmd, "sendcca")) {
        send_cca(ifname,argc,argv);
    } else if (streq(cmd, "sendrpihist")) {
        send_rpihistogram(ifname,argc,argv);
    }
     else if (streq(cmd, "sendstastats")) {
        send_stastats(ifname, argc, argv);
    } else if (streq(cmd, "sendlcireq")) {
        send_lcireq(ifname, argc, argv);
    } else if (streq(cmd, "rrmstats")) {
        get_rrmstats(ifname, argc, argv);
    } else if (streq(cmd, "get_rrmutil")) {
        get_rrmutil(ifname, argc, argv);
    } else if (streq(cmd, "sendfrmreq")) {
        send_frmreq(ifname, argc, argv);
    } else if (streq(cmd, "bcnrpt")) {
        get_bcnrpt(ifname, argc, argv);
    } else if (streq(cmd, "getrssi")) {
        get_rssi(ifname, argc, argv);
    } else if (streq(cmd, "acsreport")) {
        acs_report(ifname, argc, argv);
    } else if (streq(cmd, "setchanlist")) {
        channel_loading_channel_list_cmd(ifname, argc, argv, IEEE80211_DBGREQ_SETACSUSERCHANLIST);
    } else if (streq(cmd, "getchanlist")) {
        channel_loading_channel_list_cmd(ifname, argc, argv, IEEE80211_DBGREQ_GETACSUSERCHANLIST);
    } else if (streq(cmd, "block_acs_channel")) {
        block_acs_channel(ifname, argc, argv);
    } else if (streq(cmd, "tr069_chanhist")) {
        tr069_chan_history(ifname, argc, argv);
    } else if (streq(cmd, "get_assoc_time")) {
        get_assoc_time(ifname, argc, argv);
    } else if (streq(cmd, "tr069_txpower")) {
        tr069_txpower_cmd(ifname, argc, argv,TR069_TXPOWER);
    } else if (streq(cmd, "tr069_gettxpower")) {
        tr069_txpower_cmd(ifname, argc, argv,TR069_GETTXPOWER);
    } else if (streq(cmd, "tr069_guardintv")) {
        tr069_guardintv_cmd(ifname, argc, argv, TR069_GUARDINTV);
    } else if (streq(cmd, "tr069_get_guardintv")) {
        tr069_guardintv_cmd(ifname, argc, argv, TR069_GET_GUARDINTV);
    } else if (streq(cmd, "tr069_getassocsta")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GETASSOCSTA_CNT);
    } else if (streq(cmd, "tr069_getacstimestamp")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GETTIMESTAMP);
    } else if (streq(cmd, "tr069_getacsscan")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GETDIAGNOSTICSTATE);
    } else if (streq(cmd, "tr069_persta_statscount")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GETNUMBEROFENTRIES);
    } else if (streq(cmd, "tr069_get_11hsupported")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GET11HSUPPORTED);
    } else if (streq(cmd, "tr069_get_powerrange")) {
        tr069_get_cmd(ifname, argc, argv, TR069_GETPOWERRANGE);
    } else if (streq(cmd, "tr069_chan_inuse")) {
        tr069_chan_inuse(ifname, argc, argv);
    } else if (streq(cmd, "tr069_set_oprate")) {
        tr069_set_oprate(ifname, argc, argv);
    } else if (streq(cmd, "tr069_get_oprate")) {
        tr069_get_rate(ifname, argc, argv, TR069_GET_OPER_RATE);
    } else if (streq(cmd, "tr069_get_posrate")) {
        tr069_get_rate(ifname, argc, argv, TR069_GET_POSIBLRATE);
    } else if (streq(cmd, "tr069_set_bsrate")) {
        tr069_bsrate_cmd(ifname, argc, argv, TR069_SET_BSRATE);
    } else if (streq(cmd, "tr069_get_bsrate")) {
        tr069_bsrate_cmd(ifname, argc, argv, TR069_GET_BSRATE);
    } else if (streq(cmd, "get_hostapd_param")) {
        get_hostapd_param(ifname, argc, argv);
    } else if (streq(cmd, "set_hostapd_param")) {
        set_hostapd_param(ifname, argc, argv);
    } else if (streq(cmd, "supported_freq")) {
        tr069_get_cmd(ifname,argc,argv, TR069_GETSUPPORTEDFREQUENCY);
    } else if (streq(cmd, "chmask_persta")) {
        chmask_per_sta(ifname, argc, argv);
    } else if (streq(cmd, "peer_nss")) {
        set_peer_nss(ifname, argc, argv);
    } else if (streq(cmd, "beeliner_fw_test")) {
        beeliner_fw_test(ifname, argc, argv);
    } else if (streq(cmd, "init_rtt3")) {
        init_rtt3(ifname, argc, argv);
    } else if (streq(cmd, "bsteer_getparams")) {
        bsteer_get_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_GET_PARAMS);
    } else if (streq(cmd, "bsteer_setparams")) {
        bsteer_set_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_SET_PARAMS);
    } else if (streq(cmd, "bsteer_getdbgparams")) {
        bsteer_get_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_GET_DBG_PARAMS);
    } else if (streq(cmd, "bsteer_setdbgparams")) {
        bsteer_set_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_SET_DBG_PARAMS);
    } else if (streq(cmd, "bsteer_enable")) {
        bsteer_enable(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_ENABLE );
    } else if (streq(cmd, "bsteer_enable_events")) {
        bsteer_enable(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS);
    } else if (streq(cmd, "bsteer_setoverload")) {
        bsteer_set_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD);
    } else if (streq(cmd, "bsteer_getoverload")) {
        bsteer_get_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_GET_OVERLOAD);
    } else if (streq(cmd, "bsteer_getrssi")) {
        bsteer_getrssi(ifname, argc, argv);
    }  else if (streq(cmd, "bsteer_setproberespwh")) {
        bsteer_set_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH);
    } else if (streq(cmd, "bsteer_getproberespwh")) {
        bsteer_get_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_GET_PROBE_RESP_WH);
    }  else if (streq(cmd, "bsteer_setauthallow")) {
        bsteer_setauthallow(ifname, argc, argv);
    } else if (streq(cmd, "set_antenna_switch")) {
        set_antenna_switch(ifname, argc, argv);
    } else if (streq(cmd, "set_usr_ctrl_tbl")) {
        set_usr_ctrl_tbl(ifname, argc, argv);
    } else if (streq(cmd, "offchan_tx_test")) {
        offchan_tx_test(ifname, argc, argv);
    } else if (streq(cmd, "offchan_rx_test")) {
        offchan_rx_test(ifname, argc, argv);
    } else if (streq(cmd, "rrm_sta_list")) {
        rrm_sta_list(ifname, argc, argv);
    } else if (streq(cmd, "btm_sta_list")) {
        btm_sta_list(ifname, argc, argv);
    } else if (streq(cmd, "bsteer_getdatarateinfo")) {
        bsteer_get_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO);
    } else if (streq(cmd, "mu_scan")) {
#if QCA_LTEU_SUPPORT
        scan(ifname, argc, argv, IEEE80211_DBGREQ_MU_SCAN);
#endif
    } else if (streq(cmd, "lteu_cfg")) {
        lteu_cfg(ifname, argc, argv);
    } else if (streq(cmd, "ap_scan")) {
#if QCA_LTEU_SUPPORT
        scan(ifname, argc, argv, IEEE80211_DBGREQ_AP_SCAN);
#endif
    } else if (streq(cmd, "rpt_prb_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_rpt_prb_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "rest_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_rest_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "idle_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_idle_time")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "prb_delay")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_prb_delay")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "mu_delay")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_mu_delay")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "sta_tx_pow")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_sta_tx_pow")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "prb_spc_int")) {
        lteu_param(ifname, argc, argv);
    } else if (streq(cmd, "g_prb_spc_int")) {
        lteu_param(ifname, argc, argv);
#if QCA_AIRTIME_FAIRNESS
    } else if (streq(cmd, "atf_debug_size")) {
        atf_debug_size(ifname, argc, argv);
    } else if (streq(cmd, "atf_dump_debug")) {
        atf_dump_debug(ifname, argc, argv);
    } else if (streq(cmd, "atf_debug_nodestate")) {
        atf_debug_nodestate(ifname, argc, argv);
#endif
    } else if (streq(cmd, "tr069_get_vap_stats")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_AGGR_PKT_CNT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_RETRANS_CNT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_RETRY_CNT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_MUL_RETRY_CNT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_ACK_FAIL_CNT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_FAIL_RETRANS_CNT);
    } else if (streq(cmd, "tr069_get_fail_retrans")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_FAIL_RETRANS_CNT);
    } else if (streq(cmd, "tr069_get_success_retrans")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_RETRY_CNT);
    } else if (streq(cmd, "tr069_get_success_mul_retrans")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_MUL_RETRY_CNT);
    } else if (streq(cmd, "tr069_get_ack_failures")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_ACK_FAIL_CNT);
    } else if (streq(cmd, "tr069_get_retrans")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_RETRANS_CNT);
    } else if (streq(cmd, "tr069_get_aggr_pkts")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_AGGR_PKT_CNT);
    } else if (streq(cmd, "tr069_get_sta_stats")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_STA_BYTES_SENT);
        tr069_get_trans(ifname, argc, argv, TR069_GET_STA_BYTES_RCVD);
    } else if (streq(cmd, "tr069_get_sta_bytes_sent")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_STA_BYTES_SENT);
    } else if (streq(cmd, "tr069_get_sta_bytes_rcvd")) {
        tr069_get_trans(ifname, argc, argv, TR069_GET_STA_BYTES_RCVD);
    } else if (streq(cmd, "bsteer_setsteering")) {
        bsteer_set_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSTEERING_SET_STEERING);
    } else if (streq(cmd, "custom_chan_list")) {
        custom_chan_list(ifname, argc, argv) ;
#if UMAC_SUPPORT_VI_DBG
    } else if (streq(cmd, "vow_debug_set_param")) {
        vow_debug_cmd(ifname, argc, argv, IEEE80211_DBGREQ_VOW_DEBUG_PARAM);
    } else if (streq(cmd,"vow_debug")) {
        vow_debug_cmd(ifname, argc, argv, IEEE80211_DBGREQ_VOW_DEBUG_PARAM_PERSTREAM);
#endif
    } else if (streq(cmd, "display_traffic_statistics")) {
        display_traffic_statistics(ifname, argc, argv) ;
    }else if (streq(cmd, "get_assoc_dev_watermark_time")) {
        assoc_dev_watermark_time(ifname, argc, argv);

    } else if (streq(cmd, "setUnitTestCmd")) {
        fw_unit_test(ifname, argc, argv) ;
    } else if (streq(cmd, "setUnitTestCmdEvent")) {
        fw_unit_test_event(ifname, argc, argv) ;
    } else if (streq(cmd, "coex_cfg")) {
        coex_cfg(ifname, argc, argv);
#if ATH_ACL_SOFTBLOCKING
    } else if (streq(cmd, "softblocking")) {
        set_acl_softblocking(ifname, argc, argv);
    } else if (streq(cmd, "softblocking_get")) {
        get_acl_softblocking(ifname, argc, argv);
#endif
#ifdef WLAN_SUPPORT_TWT
    } else if (streq(cmd, "ap_twt_add_dialog")) {
        ap_twt_dialog_req_cmd(ifname, argc, argv, IEEE80211_DBGREQ_TWT_ADD_DIALOG);
    } else if (streq(cmd, "ap_twt_del_dialog")) {
        ap_twt_dialog_req_cmd(ifname, argc, argv, IEEE80211_DBGREQ_TWT_DEL_DIALOG);
    } else if (streq(cmd, "ap_twt_pause_dialog")) {
        ap_twt_dialog_req_cmd(ifname, argc, argv, IEEE80211_DBGREQ_TWT_PAUSE_DIALOG);
    } else if (streq(cmd, "ap_twt_resume_dialog")) {
        ap_twt_dialog_req_cmd(ifname, argc, argv, IEEE80211_DBGREQ_TWT_RESUME_DIALOG);
#endif
    } else if (streq(cmd, "bsscolor_collision_ap_period")) {
        bsscolor_unit_test_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSSCOLOR_DOT11_COLOR_COLLISION_AP_PERIOD);
    } else if (streq(cmd, "bsscolor_color_change_announcemt_count")) {
        bsscolor_unit_test_cmd(ifname, argc, argv, IEEE80211_DBGREQ_BSSCOLOR_DOT11_COLOR_COLLISION_CHG_COUNT);
#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    } else if (streq(cmd, "setprimarychans")) {
        set_primary_allowed_chanlist(ifname, argc, argv);
    } else if(streq(cmd, "getprimarychans")) {
        get_primary_allowed_chanlist(ifname, argc, argv);
#endif
    } else if (streq(cmd, "set_innetwork_2g")) {
        set_innetwork_2g_mac(ifname, argc, argv);
    } else if (streq(cmd, "get_innetwork_2g")) {
        get_innetwork_2g_mac(ifname, argc, argv);
    } else if (streq(cmd, "get_chan_survey")) {
        get_channel_survey_stats(ifname, argc, argv);
    } else if (streq(cmd, "reset_chan_survey")) {
        reset_channel_survey_stats(ifname, argc, argv);
    } else if (streq(cmd, "get_last_beacon_rssi")) {
        get_last_four_beacon_rssi(ifname, argc, argv);
    } else if (streq(cmd, "get_last_ack_rssi")) {
        get_last_four_ack_rssi(ifname, argc, argv);
    } else {
        usage();
    }

    destroy_socket_context(&sock_ctx);

    return 0;
}
