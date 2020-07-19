/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

         LOWI Wireless Scan Service Test Module (LOWI Test)

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Test. It exercises the LOWI
  module.

Copyright (c) 2013-2014 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010, 2014, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/

/*--------------------------------------------------------------------------
 * Include Files
 * -----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/timerfd.h>

#define LOG_TAG "LOWI_TST"

#include <unistd.h>
#include <limits.h>
#include <base_util/time_routines.h>
#include <base_util/log.h>

using namespace qc_loc_fw;

#include "lowi_wrapper.h"

#define LOWI_MALLOC(s)             malloc(s)
#define LOWI_CALLOC(n,s)           calloc(n, s)
#define LOWI_FREE(p)               {if (p != NULL) {free(p);} p = NULL;}
#define LOWI_MACADDR_FMT \
  "%02x:%02x:%02x:%02x:%02x:%02x"
#define LOWI_MACADDR(mac_addr) \
  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]

/* Max BSSIDs for stats reporting */
#define MAX_BSSIDS_STATS  100

/* Timeout for scan response */
#define LOWI_SCAN_TIMEOUT_MS 75000
#define LOWI_ASYNC_SCAN_TIMEOUT_MS 60000 // 1 minutes
#define NSECS_PER_SEC 1000000000
#define MSECS_PER_SEC 1000
#define NSECS_PER_MSEC (NSECS_PER_SEC/MSECS_PER_SEC)

/* Max Buffer length for Command */
#define LOWI_MAX_CMD_LEN 200

/* Data file and Summary file location */
#define LOWI_OUT_FILE_NAME "/usr/share/location/lowi/lowi_ap_res.csv"
#define LOWI_SUMMARY_FILE_NAME "/usr/share/location/lowi/lowi_ap_summary.csv"
#define DEFAULT_AP_LIST_FILE "/usr/share/location/lowi/innav_ap_list.txt"
#define RANGING_TYPE_CHOICE_V2  2
#define RANGING_TYPE_CHOICE_V3  3
#define LOWI_TEST_DEFAULT_RTT_MEAS 5

/*--------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -----------------------------------------------------------------------*/

/* Macro for bounds check on input */
#define LOWI_APPLY_LIMITS(value, lower, upper)  (((value) < (lower)) ? (lower) : \
         (((value) > (upper)) ? (upper) : (value)))


#define LOWI_TEST_WAKE_LOCK_ID "LowiTestWakeLock"

#ifdef LOWI_ON_ACCESS_POINT
#define LOWI_TEST_REQ_WAKE_LOCK
#define LOWI_TEST_REL_WAKE_LOCK
#else
#define LOWI_TEST_REQ_WAKE_LOCK system("echo " LOWI_TEST_WAKE_LOCK_ID  \
                                       " > /sys/power/wake_lock")
#define LOWI_TEST_REL_WAKE_LOCK system("echo " LOWI_TEST_WAKE_LOCK_ID  \
                                       " > /sys/power/wake_unlock")
#endif

const xmlChar XML_NODE_RANGING[]            = "ranging";
const xmlChar XML_NODE_DISCOVERY[]          = "discovery";
const xmlChar XML_NODE_SUMMARY[]            = "summary";
const xmlChar XML_NODE_AP[]                 = "mac";
const xmlChar XML_NODE_BAND[]               = "band";
const xmlChar XML_NODE_CH[]                 = "ch";
const xmlChar XML_NODE_BAND_CENTER_FREQ1[]  = "center_freq1";
const xmlChar XML_NODE_BAND_CENTER_FREQ2[]  = "center_freq2";
const xmlChar XML_NODE_RTT_TYPE[]           = "rttType";
const xmlChar XML_NODE_NUM_FRAMES_PER_BURST[] = "numFrames";
const xmlChar XML_NODE_RANGING_BW[]         = "bw";
const xmlChar XML_NODE_RANGING_PREAMBLE[]   = "preamble";

/* FTM Params*/
const xmlChar XML_NODE_FTM_RANGING_ASAP[]           = "asap";
const xmlChar XML_NODE_FTM_RANGING_LCI[]            = "lci";
const xmlChar XML_NODE_FTM_RANGING_LOC_CIVIC[]      = "civic";
const xmlChar XML_NODE_FTM_RANGING_NUM_BURSTS_EXP[] = "burstsexp";
const xmlChar XML_NODE_FTM_RANGING_BURST_DURATION[] = "burstduration";
const xmlChar XML_NODE_FTM_RANGING_BURST_PERIOD[]   = "burstperiod";

/* LCI Information*/
const xmlChar XML_NODE_LCI[]                = "lci_info";
const xmlChar XML_NODE_LCI_LAT[]            = "latitude";
const xmlChar XML_NODE_LCI_LON[]            = "longitude";
const xmlChar XML_NODE_LCI_ALT[]            = "altitude";
const xmlChar XML_NODE_LCI_LAT_UNC[]        = "latitude_unc";
const xmlChar XML_NODE_LCI_LON_UNC[]        = "longitude_unc";
const xmlChar XML_NODE_LCI_ALT_UNC[]        = "altitude_unc";
const xmlChar XML_NODE_LCI_MOTION_PATTERN[] = "motion_pattern";
const xmlChar XML_NODE_LCI_FLOOR[]          = "floor";
const xmlChar XML_NODE_LCI_HEIGHT[]         = "height_above_floor";
const xmlChar XML_NODE_LCI_HEIGHT_UNC[]     = "height_unc";

/* LCR Information*/
const xmlChar XML_NODE_LCR[]        = "lcr_info";
const xmlChar XML_NODE_LCR_CC[]     = "country_code";
const xmlChar XML_NODE_LCR_CIVIC[]  = "civic_address";

/* FTMRR Node Information*/
const xmlChar XML_NODE_FTMRR[]                = "ftmrr";
const xmlChar XML_NODE_FTMRR_ELM[]            = "element";
const xmlChar XML_NODE_FTMRR_ELM_BSSID[]      = "bssid";
const xmlChar XML_NODE_FTMRR_ELM_BSSID_INFO[] = "info_bssid";
const xmlChar XML_NODE_FTMRR_ELM_PHY_TYPE[]   = "phy_type";
const xmlChar XML_NODE_FTMRR_ELM_OP_CLASS[]   = "op_class";
const xmlChar XML_NODE_FTMRR_ELM_CH[]         = "ch";
const xmlChar XML_NODE_FTMRR_ELM_CENTER_CH1[] = "center_ch1";
const xmlChar XML_NODE_FTMRR_ELM_CENTER_CH2[] = "center_ch2";
const xmlChar XML_NODE_FTMRR_ELM_CH_WIDTH[]   = "width_ch";

/*--------------------------------------------------------------------------
 * Local Data Definitions
 * -----------------------------------------------------------------------*/
typedef signed char int8;

/* Scan type local to IWSS Test module */
typedef enum
{
  LOWI_DISCOVERY_SCAN,
  LOWI_RTS_CTS_SCAN,
  LOWI_BOTH_SCAN,
  LOWI_ASYNC_DISCOVERY_SCAN,
  LOWI_NR_REQ,
  LOWI_SET_LCI,
  LOWI_SET_LCR,
  LOWI_WRU_REQ,
  LOWI_FTMR_REQ,
  LOWI_MAX_SCAN
} t_lowi_scan;

/* Scan command for IWSS test module */
typedef struct
{
  t_lowi_scan   cmd;
  LOWIDiscoveryScanRequest::eBand band; // 0 - 2.4Ghz, 1 - 5 Ghz and 2 - All
  LOWIDiscoveryScanRequest::eScanType  scan_type; // 1 for Active and 0 for passive discovery scan
  uint16        num_requests;
  uint16        delay;
  uint32        timeout_offset;  // Timeout offset in seconds
  uint32        meas_age_filter_sec; // Measurement age filter
  uint32        fallback_tolerance; // Fallback tolerance
  eRttType  rttType;            // Type of Ranging to be performed - global incase file doesnot contain type
  eRangingBandwidth ranging_bandwidth; // BW to be used for Ranging request
  vector<LOWIPeriodicNodeInfo> ap_info;
  vector<LOWIChannelInfo> ch_info;  // Only for discovery scan supplied through xml
  vector<LOWIMacAddress> summary_ap; // Summary will be generated for only AP's in this array
  LOWILciInformation *lci_info;
  LOWILcrInformation *lcr_info;
  vector<LOWIFTMRRNodeInfo> ftmrr_info;
  uint16        rand_inter;
  uint8           min_ap_cnt;
}t_lowi_test_cmd;

/* Statistics data structure */
// For discovery scan
typedef struct
{
  int32 low;
  int32 high;
  float total;
  int32 cnt;
} t_scan_stats;

#define MAX_NUM_RTT_MEASUREMENTS_PER_AP 9
// For ranging scan
typedef struct
{
  int32 rssi_low;
  int32 rssi_high;
  float rssi_total;
  int32 rtt_low;
  int32 rtt_high;
  float rtt_total;
  int32 meas_cnt[MAX_NUM_RTT_MEASUREMENTS_PER_AP];
                     // eg: meas_cnt[5] will record down the number of occurences
                     // that this AP comes back with 5 RTT measurements
  int32 total_meas_cnt; // total RTT measurement cnt for this AP
                        // for exmaple, if in one scan request, there are 4 RTT measurements
                        // come back, then total_meas_cnt will be increased by 4
  int32 total_meas_set_cnt; // cnt of RTT scan attempts with any RTT measurement come back
                            // for example, if in one scan request, if there are one or more
                            // RTT measurments come back for this AP, total_meas_set_cnt for
                            // this AP will be increased by 1
  int32 total_attempt_cnt;  // number of attempts that RTT scan was requested by this AP
                            // in case of -pr option, this number may be different for each AP
                            // as RTT scan is only attempted for APs that were found in
                            // previous discovery scan
} t_rtt_scan_stats;

/* Maintaining RSSI and RTT stats */
typedef struct
{
  t_scan_stats     rssi;
  t_rtt_scan_stats rtt;
  double           total_rssi_mw;     // rssi total in mw for this AP found in discovery scan
  double           total_rtt_rssi_mw; // rssi total in mw for AP found in ranging scan
} t_ap_scan_stats;

t_ap_scan_stats scan_stats[MAX_BSSIDS_STATS];

/* String representation of Scan types */
const char * scan_type_str[LOWI_MAX_SCAN] = { "DISCOVERY", "RANGING",
  "BOTH - DISCOVERY & RANGING" };

t_lowi_test_cmd *lowi_cmd = NULL;
t_lowi_test_cmd rts_scan_cmd;

/* Local data */
struct
{
  pthread_mutex_t mutex;
  pthread_cond_t  ps_cond;
  pthread_cond_t  rs_cond;
  vector<LOWIScanMeasurement *> meas_results;
  uint32          seq_no;         /* Scan Seq # */
  uint32          avg_ps_rsp_ms;  /* Avg discovery scan response time in ms */
  uint32          avg_rs_rsp_ms;  /* Avg ranging scan response time in ms */
  int             clock_id;       /* Clock Id used for timekeeping */
  int64           scan_start_ms;  /* Ms at start of scan */
  int64           scan_end_ms;    /* ms at end of scan */
  FILE *          out_fp;         /* Output FILE */
  FILE *          summary_fp;     /* Summary output file */
  int             timerFd;
} lowi_test;

/*--------------------------------------------------------------------------
 * Function Definitions
 * -----------------------------------------------------------------------*/
extern int clock_nanosleep(int clock_id, int flags, const struct timespec *req,
                           struct timespec *rem);

/*=============================================================================
 * lowi_test_get_time_ms
 *
 * Description:
 *    Get the time and convert to ms.
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    Time in ms
 ============================================================================*/
static int64 lowi_test_get_time_ms(void)
{
  struct timespec ts;

  if (0 == clock_gettime(lowi_test.clock_id, &ts))
  {
    printf( "TIME Read = %ld.%09ld = %lldmsec\n", ts.tv_sec, ts.tv_nsec,
          (((int64)ts.tv_sec) * MSECS_PER_SEC) + ts.tv_nsec/NSECS_PER_MSEC);

    return (((int64)ts.tv_sec) * MSECS_PER_SEC) + ts.tv_nsec/NSECS_PER_MSEC;
  }
  return 0;

}
/*=============================================================================
 * lowi_test_sleep
 *
 * Description:
 *    Sleep for time specified. Set an android alarm for wakeup at end
 *
 * Parameters:
 *    duration_ms : Sleep duration in milli seconds
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_sleep(const uint16 duration_ms)
{
  struct timespec ts;
  int ret_val;
  int64 start_time_ms, end_time_ms;

  ts.tv_sec  = duration_ms/MSECS_PER_SEC;
  ts.tv_nsec = (duration_ms%MSECS_PER_SEC) * NSECS_PER_MSEC;
  start_time_ms = lowi_test_get_time_ms();
  ret_val = clock_nanosleep(lowi_test.clock_id, 0, &ts, NULL);
  if (ret_val != 0)
  {
    printf( "clock_nanosleep(%d) Ret (%d, errno %d:%s): %ld.%09ld\n",
          lowi_test.clock_id, ret_val, errno, strerror(errno),
          ts.tv_sec, ts.tv_nsec);
  }

  end_time_ms = lowi_test_get_time_ms();
  printf( "clock_nanosleep: Slept from: %lld to %lld", start_time_ms, end_time_ms);

}

static void lowi_test_start_timer(const uint16 duration_ms)
{
  struct itimerspec its;
  struct timespec ts;
  int64 start_time_ms;

  its.it_value.tv_sec  = duration_ms/MSECS_PER_SEC;
  its.it_value.tv_nsec = (duration_ms%MSECS_PER_SEC) * NSECS_PER_MSEC;
  its.it_interval.tv_sec  = 0;
  its.it_interval.tv_nsec = 0;
  int ret = timerfd_settime(lowi_test.timerFd, 0, &its, NULL);

  if(ret != 0)
  {
      printf( "timerfd_settime failed ret = %d %d, %s",
            ret, errno, strerror(errno));
  }

  start_time_ms = lowi_test_get_time_ms();
  printf( "Timer Set at %lld ms for %d ms", start_time_ms, duration_ms);

}

static void lowi_test_wait( const uint16 duration_ms )
{
  int        ret_val;
  int64      end_time_ms;
  uint64     exp; /* this variable will be greater than or equal to one if timer
                     expires,otherwise it will be returned zero at timefd read call   */

  if (duration_ms == 0)
  {
    return;
  }
  do
  {
    LOWI_TEST_REL_WAKE_LOCK;
    ret_val = read (lowi_test.timerFd, &exp, sizeof (exp));
    LOWI_TEST_REQ_WAKE_LOCK;

    if(ret_val < 0)
    {
        printf( "lowi_test_wait: read of timerFd failed %s",strerror(errno));
        return;
    }

    if (exp >= 1)
    {
      end_time_ms = lowi_test_get_time_ms();
      printf( "timer ends at %lld ms", end_time_ms);
      break;
    }
  }
  while (1);

}

/*=============================================================================
 * lowi_test_log_time_us_to_string
 *
 * Description:
 *
 * Parameters:
 *
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_log_time_us_to_string(char *p_buf, int buf_sz,
                                            double *p_day, uint64 time_us)
{
  uint64 seconds = time_us / 1000000;
  uint64 ms = (time_us - (seconds * 1000000)) / 1000;
  //IPE_LOG_USEC_IN_SEC = 1000000, IPE_LOG_USEC_IN_MSEC = 1000;

  struct tm *time = localtime((time_t *)&seconds);
  if (NULL != time)
  {
    snprintf(p_buf, buf_sz,
             "%d/%d/%d:%d:%d:%d:%0llu",
             time->tm_year + 1900,
             time->tm_mon + 1,
             time->tm_mday,
             time->tm_hour,
             time->tm_min,
             time->tm_sec,
             ms
             );
    if (p_day != NULL)
    {
      *p_day = time->tm_sec +  (double)ms / 1000.0; /* In seconds */
      *p_day = time->tm_min +  (*p_day) / 60.0;     /* In minutes */
      *p_day = time->tm_hour + (*p_day) / 60.0;     /* In hours */
      *p_day = time->tm_mday + (*p_day) / 24.0;     /* In days  */
    }
  }
}

/*=============================================================================
 * rssi_total_in_watt
 *
 * Description:
 *    Total the RSSI in milliWatts
 *
 * Return value:
 *   void
 ============================================================================*/
void rssi_total_in_watt(LOWIResponse::eResponseType scan_type,
                        t_ap_scan_stats *const ap_stat_ptr,
                        const int32 rssi_0p5dBm)
{
  double rssi_mwatt = pow(10, (double)rssi_0p5dBm / 20);

  if (ap_stat_ptr == NULL)
  {
    return;
  }

  if ((scan_type == LOWIResponse::DISCOVERY_SCAN) ||
      (scan_type == LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS))
  {
    ap_stat_ptr->total_rssi_mw += rssi_mwatt;
  }
  else if (scan_type == LOWIResponse::RANGING_SCAN)
  {
    ap_stat_ptr->total_rtt_rssi_mw += rssi_mwatt;
  }
  else
  {
    printf("combined scan type in scan result, not supported in lowi test\n");
  }
}

/*=============================================================================
 * add_data_to_stat
 *
 * Description:
 *    Add current results to statictics data set
 *    For discovery scan, rssi is processed.
 *    For ranging scan, both rssi and rtt is processed.
 *
 * Return value:
 *   void
 ============================================================================*/
void add_data_to_stat(LOWIResponse::eResponseType scan_type,
                      t_ap_scan_stats *const ap_stat_ptr,
                      const int32 new_data,
                      const int32 rtt_new_data)
{
  if (ap_stat_ptr == NULL)
  {
    return;
  }

  if ((scan_type == LOWIResponse::DISCOVERY_SCAN) ||
      (scan_type == LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS))
  {
    if (ap_stat_ptr->rssi.cnt == 0)
    {
      ap_stat_ptr->rssi.low  = new_data;
      ap_stat_ptr->rssi.high = new_data;
    }
    else
    {
      // we allow input rssi to be 0, so we can catch it for debug
      if (new_data < ap_stat_ptr->rssi.low)
      {
        ap_stat_ptr->rssi.low = new_data;
      }
      if (new_data > ap_stat_ptr->rssi.high)
      {
        ap_stat_ptr->rssi.high = new_data;
      }
    }
    ap_stat_ptr->rssi.total += new_data;
    ap_stat_ptr->rssi.cnt++;
  }
  else if (scan_type == LOWIResponse::RANGING_SCAN)
  {
    // we allow input rssi and rtt to be 0,
    // so we can catch it for debug
    if (ap_stat_ptr->rtt.total_meas_cnt == 0)
    {
      ap_stat_ptr->rtt.rssi_low  = new_data;
      ap_stat_ptr->rtt.rssi_high = new_data;
      ap_stat_ptr->rtt.rtt_low   = rtt_new_data;
      ap_stat_ptr->rtt.rtt_high  = rtt_new_data;
    }
    else
    {
      if (new_data < ap_stat_ptr->rtt.rssi_low)
      {
        ap_stat_ptr->rtt.rssi_low = new_data;
      }
      if (new_data > ap_stat_ptr->rtt.rssi_high)
      {
        ap_stat_ptr->rtt.rssi_high = new_data;
      }

      if (rtt_new_data < ap_stat_ptr->rtt.rtt_low)
      {
        ap_stat_ptr->rtt.rtt_low = rtt_new_data;
      }
      if (rtt_new_data > ap_stat_ptr->rtt.rtt_high)
      {
        ap_stat_ptr->rtt.rtt_high = rtt_new_data;
      }
    }

    ap_stat_ptr->rtt.rssi_total += new_data;
    ap_stat_ptr->rtt.rtt_total  += rtt_new_data;
    ap_stat_ptr->rtt.total_meas_cnt++;
  }
  else
  {
    printf("combined scan type in scan result, not supported in lowi test\n");
  }
}

/*=============================================================================
 * lowi_printf
 *
 * Description:
 *    Print data to stream pointed by fp and also stdout
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_printf(FILE *fp, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  if (fp != NULL)
  {
    vfprintf(fp, fmt, args);
  }
  vprintf(fmt, args);
  va_end(args);
}

/*=============================================================================
 * lowi_test_display_summary_stats
 *
 * Description:
 *    Display the summary stats
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_display_summary_stats(void)
{
  int ap_cnt;
  int summary_ap_cnt;
  int ap_found_in_summary_list = 0;
  int ii;
  LOWIPeriodicNodeInfo *p_ap;
  LOWIMacAddress *p_ap_summary;

  // Summary output file
  if (lowi_test.summary_fp == NULL)
  {
    /* Use default file */
    lowi_test.summary_fp = fopen(LOWI_SUMMARY_FILE_NAME, "w");
    if (lowi_test.summary_fp == NULL)
    {
      fprintf(stderr, "%s:%d: Error opening file %s. %s\n",
              __func__, __LINE__, LOWI_SUMMARY_FILE_NAME, strerror(errno));
    }
  }

  lowi_printf(lowi_test.summary_fp, "Issued scan Type: %s\n",
              scan_type_str[lowi_cmd->cmd]);

  // First print out discovery scan result
  if (lowi_cmd->cmd == LOWI_DISCOVERY_SCAN ||
      lowi_cmd->cmd == LOWI_ASYNC_DISCOVERY_SCAN ||
      lowi_cmd->cmd == LOWI_BOTH_SCAN)
  {
    lowi_printf(lowi_test.summary_fp, "Summary stats: Scan Type: %s\n",
                "DISCOVERY");
    lowi_printf(lowi_test.summary_fp, "Avg Response Time");
    lowi_printf(lowi_test.summary_fp, ":Discovery: %d ms", lowi_test.avg_ps_rsp_ms);
    lowi_printf(lowi_test.summary_fp, "\n%10s %10s %16s %15s\n",
                "AP", "Chan", "Detection rate", "RSSI(dBm)");
    lowi_printf(lowi_test.summary_fp, "%10s %10s %15s %4s %4s %7s %6s",
                "", "", "", "Min", "Max", "Avg(dBm)", "Avg(W)\n");

    // Loop first to print discovery scan statistics
    for (ap_cnt = 0; ap_cnt < lowi_cmd->ap_info.getNumOfElements(); ap_cnt++)
    {
      p_ap = &lowi_cmd->ap_info[ap_cnt];
      // Check if the user wants to only see the summary for specific AP's
      if (0 != lowi_cmd->summary_ap.getNumOfElements())
      {
        printf("%s: Number of items for summary = %d", __func__, lowi_cmd->summary_ap.getNumOfElements());
        ap_found_in_summary_list = 0;
        // Yes. User wants only specific AP's. Iterate over the
        // summary ap list and do not print summary if the ap is
        // not in list
        for (summary_ap_cnt = 0; summary_ap_cnt < lowi_cmd->summary_ap.getNumOfElements(); summary_ap_cnt++)
        {
          p_ap_summary = &lowi_cmd->summary_ap[summary_ap_cnt];
          if (p_ap->bssid.compareTo(*p_ap_summary) == 0)
          {
            ap_found_in_summary_list = 1;
            break;
          }
        }
        if (0 == ap_found_in_summary_list)
        {
          printf("%s: AP not found in the summary list specified in the xml file - continue", __func__);
          continue;
        }
      }

      LOWIChannelInfo ch = LOWIChannelInfo(p_ap->frequency);
      lowi_printf(lowi_test.summary_fp, "%.02x:%.02x:%.02x:%.02x:%.02x:%.02x %3hu",
                  p_ap->bssid[0], p_ap->bssid[1], p_ap->bssid[2],
                  p_ap->bssid[3], p_ap->bssid[4], p_ap->bssid[5],
                  ch.getChannel());

      lowi_printf(lowi_test.summary_fp, " %4d/%-4d(%3d%%)",
                  scan_stats[ap_cnt].rssi.cnt, (lowi_test.seq_no - 1),
                  scan_stats[ap_cnt].rssi.cnt * 100 / (lowi_test.seq_no - 1));

      lowi_printf(lowi_test.summary_fp, "%5d %5d %5d %5d\n",
                  scan_stats[ap_cnt].rssi.low / 2, scan_stats[ap_cnt].rssi.high / 2,
                  (int)((scan_stats[ap_cnt].rssi.cnt == 0) ?  0 :
                                                             (scan_stats[ap_cnt].rssi.total / (scan_stats[ap_cnt].rssi.cnt * 2))),
                  (scan_stats[ap_cnt].rssi.cnt == 0 ?  0 :
                                                      (int32)(10 * log10(scan_stats[ap_cnt].total_rssi_mw / scan_stats[ap_cnt].rssi.cnt))));

    }
  }

  // Loop again to print out rtt statistics
  if (lowi_cmd->cmd == LOWI_RTS_CTS_SCAN ||
      lowi_cmd->cmd == LOWI_BOTH_SCAN)
  {
    lowi_printf(lowi_test.summary_fp, "Summary stats: Scan Type: %s\n",
                "RANGING");
    lowi_printf(lowi_test.summary_fp, "Avg Response Time");
    lowi_printf(lowi_test.summary_fp, ": Ranging: %d ms", lowi_test.avg_rs_rsp_ms);
    lowi_printf(lowi_test.summary_fp, "\n%10s %10s %16s %15s %21s \n",
                "AP", "Chan", "Detection rate", "RSSI(dBm)", "RTT(psec)");
    lowi_printf(lowi_test.summary_fp, "%10s %10s %15s %4s %4s %7s %5s %5s %5s %5s \n",
                "", "", "", "Min", "Max", "Avg(dBm)", "Avg(W)", "Min", "Max", "Avg");

    // Loop first to print discovery scan statistics
    for (ap_cnt = 0; ap_cnt < lowi_cmd->ap_info.getNumOfElements(); ap_cnt++)
    {
      p_ap = &lowi_cmd->ap_info[ap_cnt];
      // Check if the user wants to only see the summary for specific AP's
      if (0 != lowi_cmd->summary_ap.getNumOfElements())
      {
        ap_found_in_summary_list = 0;
        // Yes. User wants only specific AP's. Iterate over the
        // summary ap list and do not print summary if the ap is
        // not in list
        for (summary_ap_cnt = 0; summary_ap_cnt < lowi_cmd->summary_ap.getNumOfElements(); summary_ap_cnt++)
        {
          p_ap_summary = &lowi_cmd->summary_ap[summary_ap_cnt];
          if (p_ap->bssid.compareTo(*p_ap_summary) == 0)
          {
            ap_found_in_summary_list = 1;
            break;
          }
        }
        if (0 == ap_found_in_summary_list)
        {
          printf("lowi_test_display_summary_stats AP not found in summary list - continue");
          continue;
        }
      }

      LOWIChannelInfo chan = LOWIChannelInfo(p_ap->frequency);
      lowi_printf(lowi_test.summary_fp, "%.02x:%.02x:%.02x:%.02x:%.02x:%.02x %3hu",
                  p_ap->bssid[0], p_ap->bssid[1], p_ap->bssid[2],
                  p_ap->bssid[3], p_ap->bssid[4], p_ap->bssid[5],
                  chan.getChannel());

      lowi_printf(lowi_test.summary_fp, " %4d/%-4d(%3d%%)",
                  scan_stats[ap_cnt].rtt.total_meas_set_cnt,
                  (scan_stats[ap_cnt].rtt.total_attempt_cnt),
                  ((scan_stats[ap_cnt].rtt.total_meas_set_cnt == 0) || (scan_stats[ap_cnt].rtt.total_attempt_cnt == 0)) ? 0 :
                                                                                                                          (scan_stats[ap_cnt].rtt.total_meas_set_cnt * 100 / (scan_stats[ap_cnt].rtt.total_attempt_cnt)));

      lowi_printf(lowi_test.summary_fp, "%5d %5d %5d %5d",
                  scan_stats[ap_cnt].rtt.rssi_low / 2, scan_stats[ap_cnt].rtt.rssi_high / 2,
                  ((scan_stats[ap_cnt].rtt.total_meas_cnt == 0) ?  0 :
                                                                  (int32)(scan_stats[ap_cnt].rtt.rssi_total / (2 * scan_stats[ap_cnt].rtt.total_meas_cnt))),
                  (scan_stats[ap_cnt].rtt.total_meas_cnt == 0 ?  0 :
                                                                (int32)(10 * log10(scan_stats[ap_cnt].total_rtt_rssi_mw / scan_stats[ap_cnt].rtt.total_meas_cnt))));

      lowi_printf(lowi_test.summary_fp, " %6d %6d %6d",
                  scan_stats[ap_cnt].rtt.rtt_low, scan_stats[ap_cnt].rtt.rtt_high,
                  ((scan_stats[ap_cnt].rtt.total_meas_cnt == 0) ?  0 :
                                                                  (int)(scan_stats[ap_cnt].rtt.rtt_total / scan_stats[ap_cnt].rtt.total_meas_cnt)));

      //scan_stats[ap_cnt].rtt.meas_cnt[0] = scan_stats[ap_cnt].rtt.total_attempt_cnt -
      //                                     scan_stats[ap_cnt].rtt.total_meas_set_cnt;
      //lowi_printf(lowi_test.summary_fp, "%5d %5d %5d %5d %5d %5d\n",
      //            scan_stats[ap_cnt].rtt.meas_cnt[5], scan_stats[ap_cnt].rtt.meas_cnt[4],
      //            scan_stats[ap_cnt].rtt.meas_cnt[3], scan_stats[ap_cnt].rtt.meas_cnt[2],
      //            scan_stats[ap_cnt].rtt.meas_cnt[1], scan_stats[ap_cnt].rtt.meas_cnt[0]);
    }
  }

  if (lowi_test.summary_fp != NULL)
  {
    fclose(lowi_test.summary_fp);
    lowi_test.summary_fp = NULL;
  }
}

/*=============================================================================
 * lowi_test_update_scan_stats
 *
 * Description:
 *   New scan results available. Update the stats.
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_update_scan_stats(LOWIResponse *response)
{
  int ap_cnt, meas_cnt, ap_index, match_found;

  char string_buf[128];  // IPE_LOG_STRING_BUF_SZ
  double time_in_days;
  uint8 chan = 0;

  if (response == NULL)
  {
    fprintf(stderr, "NULL results\n");
    return;
  }

  switch (response->getResponseType())
  {
  case LOWIResponse::DISCOVERY_SCAN:
  case LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS:
    {
      LOWIDiscoveryScanResponse *resp = ((LOWIDiscoveryScanResponse *)response);
      if (resp->scanStatus == LOWIResponse::SCAN_STATUS_SUCCESS)
      {
        vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
        lowi_test.meas_results = scanMeasurements;
        if (lowi_cmd->ap_info.getNumOfElements() == 0)
        {
          for (ap_cnt = 0; ap_cnt < scanMeasurements.getNumOfElements(); ap_cnt++)
          {
            LOWIScanMeasurement *ap = scanMeasurements[ap_cnt];
            LOWIPeriodicNodeInfo p_ap_info;
            if (NULL != ap)
            {
              p_ap_info.bssid = ap->bssid;
              p_ap_info.frequency = ap->frequency;
              if (lowi_cmd->ap_info.push_back(p_ap_info) != 0)
              {
                break;
              }
            }
          }
        }
        else
        {
          for (ap_cnt = 0; ap_cnt < scanMeasurements.getNumOfElements(); ap_cnt++)
          {
            LOWIScanMeasurement *ap = scanMeasurements[ap_cnt];
            if (NULL != ap)
            {
              match_found = 0;
              for (ap_index = 0; ap_index < lowi_cmd->ap_info.getNumOfElements(); ap_index++)
              {
                LOWIPeriodicNodeInfo *p_ap_info;
                p_ap_info = &lowi_cmd->ap_info[ap_index];
                if (0 == p_ap_info->bssid.compareTo(ap->bssid))
                {
                  match_found = 1;
                  break;
                }
              }
              if (0 == match_found)
              {
                LOWIPeriodicNodeInfo p_ap_info;
                p_ap_info.bssid = ap->bssid;
                p_ap_info.frequency = ap->frequency;

                if (lowi_cmd->ap_info.push_back(p_ap_info) != 0)
                {
                  break;
                }
              }
            }
          }
        }
      }
      else // resp->scanStatus != LOWIResponse::SCAN_STATUS_SUCCESS
      {
        /* No results to log or process */
        return;
      }
    }
    break;
  case LOWIResponse::RANGING_SCAN:
    {
      int rtt_scan_ap_cnt = 0;
      for (ap_cnt = 0; ap_cnt < lowi_cmd->ap_info.getNumOfElements(); ap_cnt++)
      {
        for (rtt_scan_ap_cnt = 0;
             rtt_scan_ap_cnt < rts_scan_cmd.ap_info.getNumOfElements();
             rtt_scan_ap_cnt++)
        {
          if (lowi_cmd->ap_info[ap_cnt].bssid.compareTo(rts_scan_cmd.ap_info[rtt_scan_ap_cnt].bssid) == 0)
          {
            scan_stats[ap_cnt].rtt.total_attempt_cnt++;
            break;
          }
        }
      }
      LOWIRangingScanResponse *resp = ((LOWIRangingScanResponse *)response);
      if (resp->scanStatus == LOWIResponse::SCAN_STATUS_SUCCESS)
      {
        vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
        lowi_test.meas_results = scanMeasurements;
        if (lowi_cmd->ap_info.getNumOfElements() == 0)
        {
          for (ap_cnt = 0; ap_cnt < scanMeasurements.getNumOfElements(); ap_cnt++)
          {
            LOWIScanMeasurement *ap = scanMeasurements[ap_cnt];
            if (NULL != ap)
            {
              LOWIPeriodicNodeInfo p_ap_info;
              p_ap_info.bssid = ap->bssid;
              p_ap_info.frequency = ap->frequency;

              if (lowi_cmd->ap_info.push_back(p_ap_info) != 0)
              {
                break;
              }
            }
          }
        }
      }
      else // resp->scanStatus != LOWIResponse::SCAN_STATUS_SUCCESS
      {
        /* No results to log or process */
        return;
      }
    }
    break;
  default:
    /* No results to log or process */
    return; //ignore;
  }

  vector<LOWIScanMeasurement *> scanMeasurements;
  switch (response->getResponseType())
  {
  case LOWIResponse::DISCOVERY_SCAN:
  case LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS:
    {
      LOWIDiscoveryScanResponse *resp = ((LOWIDiscoveryScanResponse *)response);
      scanMeasurements = resp->scanMeasurements;
    }
    break;
  case LOWIResponse::RANGING_SCAN:
    {
      LOWIRangingScanResponse *resp = ((LOWIRangingScanResponse *)response);
      scanMeasurements = resp->scanMeasurements;
    }
    break;
  default:
    break;
  }

  for (ap_cnt = 0; ap_cnt < lowi_cmd->ap_info.getNumOfElements(); ap_cnt++)
  {
    LOWIPeriodicNodeInfo *p_ap_info = &lowi_cmd->ap_info[ap_cnt];
    for (ap_index = 0; ap_index < scanMeasurements.getNumOfElements(); ap_index++)
    {

      if (0 == p_ap_info->bssid.compareTo(scanMeasurements[ap_index]->bssid))
      {
        /* Match Found. Add to stats. */
        if (p_ap_info->frequency == 0)
        {
          p_ap_info->frequency = scanMeasurements[ap_index]->frequency;
        }
        boolean is_ranging = (response->getResponseType() == LOWIResponse::RANGING_SCAN);
        int meas_to_look = is_ranging ? scanMeasurements[ap_index]->measurementsInfo.getNumOfElements() : 1;

        for (meas_cnt = 0; meas_cnt < meas_to_look; meas_cnt++)
        {
          int32 rssi = 0;
          int32 rtt = 0;
          LOWIMeasurementInfo *info = scanMeasurements[ap_index]->measurementsInfo[meas_cnt];
          if (NULL != info)
          {
            rssi = info->rssi;
            rtt = is_ranging ? info->rtt_ps : 0;
          }
          rssi_total_in_watt(response->getResponseType(),
                             &scan_stats[ap_cnt],
                             rssi);
          add_data_to_stat(response->getResponseType(),
                           &scan_stats[ap_cnt],
                           rssi,
                           rtt);
        }

        if (is_ranging)
        {
          // increase the cnt (with that number of measurement) for the AP
          if (meas_to_look < MAX_NUM_RTT_MEASUREMENTS_PER_AP)
          {
            scan_stats[ap_cnt].rtt.meas_cnt[meas_to_look]++;
          }
          else
          {
            printf("Wrong raning scan result, %d number of rtt measurement received",
                   meas_to_look);
          }
          scan_stats[ap_cnt].rtt.total_meas_set_cnt++;
        }
      }
    }

  }
}


/*=============================================================================
 * lowi_test_log_meas_results
 *
 * Description:
 *   Log the measurement results
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_log_meas_results(LOWIResponse *response)
{
  int ap_cnt, meas_cnt, ap_index;
  char string_buf[128];  // IPE_LOG_STRING_BUF_SZ
  double time_in_days;
  uint8 chan = 0;
  uint32 rsp_time = 0;
  uint32 seq_no;

  if (response == NULL)
  {
    fprintf(stderr, "NULL results\n");
    return;
  }

  lowi_test.scan_end_ms = lowi_test_get_time_ms();
  rsp_time = (uint32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms);
  lowi_test_update_scan_stats(response);

  switch (response->getResponseType())
  {
  case LOWIResponse::DISCOVERY_SCAN:
  case LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS:
    {
      seq_no = lowi_test.seq_no;
      lowi_test.avg_ps_rsp_ms = ((((uint64)lowi_test.avg_ps_rsp_ms) *
                                  (seq_no - 1)) +
                                 rsp_time) / seq_no;

      LOWIDiscoveryScanResponse *resp = (LOWIDiscoveryScanResponse *)response;

      if (resp->scanStatus != LOWIResponse::SCAN_STATUS_SUCCESS)
      {
        printf("Scan Failure. Nothing to log.");
        return;
      }

      vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
      lowi_test.meas_results = scanMeasurements;

      for (ap_cnt = 0; (lowi_test.out_fp != NULL) &&
           (ap_cnt < scanMeasurements.getNumOfElements()); ap_cnt++)
      {
        vector<LOWIMeasurementInfo *> measurements = scanMeasurements[ap_cnt]->measurementsInfo;
        LOWIChannelInfo ch = LOWIChannelInfo(scanMeasurements[ap_cnt]->frequency);
        if (measurements.getNumOfElements() > 0)
        {
          lowi_test_log_time_us_to_string(string_buf, 128, &time_in_days,
                                          measurements[0]->rssi_timestamp * 1000);
          fprintf(lowi_test.out_fp, "%s,%12.9f,%d,%u,%02x:%02x:%02x:%02x:%02x:%02x,%d,%d,%d,%d,%d\n",
                  string_buf, time_in_days, response->getResponseType(),
                  lowi_test.seq_no,
                  scanMeasurements[ap_cnt]->bssid[0],
                  scanMeasurements[ap_cnt]->bssid[1],
                  scanMeasurements[ap_cnt]->bssid[2],
                  scanMeasurements[ap_cnt]->bssid[3],
                  scanMeasurements[ap_cnt]->bssid[4],
                  scanMeasurements[ap_cnt]->bssid[5],
                  ch.getChannel(), measurements[0]->rssi,
                  measurements[0]->rtt_ps, rsp_time,
                  measurements[0]->meas_age);
        }
      }
    }
    break;
  case LOWIResponse::RANGING_SCAN:
    {
      seq_no = lowi_test.seq_no;
      lowi_test.avg_rs_rsp_ms = ((((uint64)lowi_test.avg_rs_rsp_ms) *
                                  (seq_no - 1)) +
                                 rsp_time) / seq_no;
      LOWIRangingScanResponse *resp = (LOWIRangingScanResponse *)response;

      if (resp->scanStatus != LOWIResponse::SCAN_STATUS_SUCCESS)
      {
        printf("Scan Failure. Nothing to log.");
        return;
      }

      vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
      lowi_test.meas_results = scanMeasurements;
      for (ap_cnt = 0, ap_index = 0; (lowi_test.out_fp != NULL) &&
           (ap_cnt < scanMeasurements.getNumOfElements()); ap_cnt++)
      {
        /* Find the AP in the scan list */
        if (0 == rts_scan_cmd.ap_info[ap_index].bssid.compareTo(scanMeasurements[ap_cnt]->bssid))
        {
          LOWIChannelInfo ch = LOWIChannelInfo(rts_scan_cmd.ap_info[ap_index].frequency);
          chan = ch.getChannel();
          break;
        }
      }
      vector<LOWIMeasurementInfo *> measurements = scanMeasurements[ap_cnt]->measurementsInfo;

      for (meas_cnt = 0; (lowi_test.out_fp != NULL) &&
           (meas_cnt < measurements.getNumOfElements()); meas_cnt++)
      {
        lowi_test_log_time_us_to_string(string_buf, 128, &time_in_days,
                                        measurements[meas_cnt]->rssi_timestamp * 1000);
        fprintf(lowi_test.out_fp, "%s,%12.9f,%d,%d,%.02x:%.02x:%.02x:%.02x:%.02x:%.02x,%d,%d,%d,%d,%d\n",
                string_buf, time_in_days, response->getResponseType(),
                lowi_test.seq_no, scanMeasurements[ap_cnt]->bssid[0],
                scanMeasurements[ap_cnt]->bssid[1], scanMeasurements[ap_cnt]->bssid[2],
                scanMeasurements[ap_cnt]->bssid[3], scanMeasurements[ap_cnt]->bssid[4],
                scanMeasurements[ap_cnt]->bssid[5],
                chan, measurements[meas_cnt]->rssi,
                measurements[meas_cnt]->rtt_ps, rsp_time,
                measurements[meas_cnt]->meas_age);
      }
    }
    break;
  default:
    break;
  }
}

static void
parse_lci_info(xmlNode *a_node, t_lowi_test_cmd **const ppCmd)
{
  xmlNode *cur_node = NULL;
  LOWILciInformation *lci = NULL;

  (*ppCmd)->lci_info = (LOWILciInformation *)LOWI_MALLOC(sizeof(LOWILciInformation));
  lci = (*ppCmd)->lci_info;
  if (NULL == lci)
  {
    printf("parse_lci_info - out of memory\n");
    return;
  }

  //flags for mandatory fileds
  bool lat_available = false;
  bool lon_available = false;
  bool alt_available  = false;

  // defult values of optional fields
  lci->latitude_unc   = CHAR_MAX;
  lci->longitude_unc  = CHAR_MAX;
  lci->altitude_unc   = CHAR_MAX;
  lci->motion_pattern = LOWI_MOTION_UNKNOWN;
  lci->floor          = 80000000;
  lci->height_above_floor = 0;
  lci->height_unc     = INT_MAX;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE)
    {
      printf("parse_lci_info: Element, name: %s\n", cur_node->name);
      if (xmlStrncmp(cur_node->name, XML_NODE_LCI_LAT, xmlStrlen(XML_NODE_LCI_LAT)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%lld", &(lci->latitude));
          if (status < 1)
          {
            // No need to continue
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Latitude = %lld\n", lci->latitude);
            break;
          }
          lat_available = true;
        }
        else
        {
          // No need to continue
          break;
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_LON, xmlStrlen(XML_NODE_LCI_LON)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%lld", &(lci->longitude));
          if (status < 1)
          {
            // No need to continue
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Longitude = %lld\n", lci->longitude);
            break;
          }
          lon_available = true;
        }
        else
        {
          // No need to continue
          break;
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_ALT, xmlStrlen(XML_NODE_LCI_ALT)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%d", &(lci->altitude));
          if (status < 1)
          {
            // No need to continue
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Altitude = %d\n", lci->altitude);
            break;
          }
          alt_available = true;
        }
        else
        {
          // No need to continue
          break;
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_LAT_UNC, xmlStrlen(XML_NODE_LCI_LAT_UNC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%hhu", &(lci->latitude_unc));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Latitude Unc = %hhu\n", lci->latitude_unc);
          }
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_LON_UNC, xmlStrlen(XML_NODE_LCI_LON_UNC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%hhu", &(lci->longitude_unc));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Longitude Unc = %hhu\n", lci->longitude_unc);
          }
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_ALT_UNC, xmlStrlen(XML_NODE_LCI_ALT_UNC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%hhu", &(lci->altitude_unc));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Altitude Unc = %hhu\n", lci->altitude_unc);
          }
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_MOTION_PATTERN, xmlStrlen(XML_NODE_LCI_MOTION_PATTERN)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          // default value for unknown
          uint8 motion = 2;
          int status = sscanf(payloadData, "%hhu", &motion);
          if (status < 1 || motion > 2)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Motion Pattern = %hhu\n", motion);
            motion = 2;
          }
          lci->motion_pattern = (eLowiMotionPattern)motion;
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_FLOOR, xmlStrlen(XML_NODE_LCI_FLOOR)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%d", &(lci->floor));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Floor = %d\n", lci->floor);
          }
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_HEIGHT, xmlStrlen(XML_NODE_LCI_HEIGHT)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%d", &(lci->height_above_floor));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Height = %d\n", lci->height_above_floor);
          }
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCI_HEIGHT_UNC, xmlStrlen(XML_NODE_LCI_HEIGHT_UNC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int status = sscanf(payloadData, "%d", &(lci->height_unc));
          if (status < 1)
          {
            // Can continue using default value
            printf("parse_lci_info: Element,"
                   " value is not formed correctly. Height Unc = %d\n", lci->height_unc);
          }
        }
      }
    }
  }
  if (!(lat_available && lon_available && alt_available))
  {
    LOWI_FREE((*ppCmd)->lci_info);
    (*ppCmd)->lci_info = NULL;
  }
}

static void
parse_lcr_info(xmlNode *a_node, t_lowi_test_cmd **const ppCmd)
{
  xmlNode *cur_node = NULL;
  LOWILcrInformation *lcr = NULL;

  (*ppCmd)->lcr_info = (LOWILcrInformation *)LOWI_MALLOC(sizeof(LOWILcrInformation));
  lcr = (*ppCmd)->lcr_info;
  if (NULL == lcr)
  {
    printf("parse_lci_info - out of memory\n");
    return;
  }

  //flags for mandatory fileds
  bool cc_available = false;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE)
    {
      printf("parse_lcr_info: Element, name: %s\n", cur_node->name);
      if (xmlStrncmp(cur_node->name, XML_NODE_LCR_CC, xmlStrlen(XML_NODE_LCR_CC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          int i = 0;
          for (; i < LOWI_COUNTRY_CODE_LEN; i++)
          {
            int status = sscanf(payloadData+i, "%c", &(lcr->country_code[i]));
            if (status < 1)
            {
              // No need to continue
              printf("parse_lcr_info: Element,"
                     " value is not formed correctly. Country Code[%d] = %c\n", i, lcr->country_code[i]);
              break;
            }
          }
          if (i != LOWI_COUNTRY_CODE_LEN)
          {
            break;
          }
          cc_available = true;
        }
        else
        {
          // No need to continue
          break;
        }
      }
      else if (xmlStrncmp(cur_node->name, XML_NODE_LCR_CIVIC, xmlStrlen(XML_NODE_LCR_CIVIC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData && strlen(payloadData) > 0)
        {
          // Civic Info is in hex bytes format
          // 010203040506a1a2a3a4a5a6.....
          size_t i;
          for (i = 0; i < strlen(payloadData)/2 && i < CIVIC_INFO_LEN; i++)
          {
            if (sscanf(payloadData + 2*i, "%2hhx", &(lcr->civic_info[i])) <= 0)
            {
              break;
            }
          }
          lcr->length = i;
        }
        else
        {
          // No need to continue
          break;
        }
      }
    }
  }
  // Since Civic info is optional, cc_available only needs to be checked
  if (!cc_available)
  {
    LOWI_FREE((*ppCmd)->lcr_info);
    (*ppCmd)->lcr_info = NULL;
  }
}

static void
flush_ftmrr_info(t_lowi_test_cmd **const ppCmd)
{
  (*ppCmd)->ftmrr_info.flush();
}

static void
parse_ftmrr_info(xmlNode *a_node, t_lowi_test_cmd **const ppCmd)
{
  xmlNode *cur_node = NULL;
  xmlNode *cur_elm_node = NULL;

  bool bssid_available = false;
  bool bssid_info_available = false;
  bool op_class_available = false;
  bool ch_available = false;
  bool ch1_available = false;
  bool ch2_available = false;
  bool chw_available = false;
  bool phy_type_available = false;

  LOWIMacAddress bssid;
  uint32 bssidInfo;
  uint8 operatingClass;
  uint8 phyType;
  uint8 ch;
  uint8 center_Ch1;
  uint8 center_Ch2;
  eRangingBandwidth bandwidth;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE && xmlStrncmp(cur_node->name, XML_NODE_FTMRR_ELM, xmlStrlen(XML_NODE_FTMRR_ELM)) == 0)
    {
      if (bssid_available && bssid_info_available &&
          op_class_available && ch_available && phy_type_available)
      {
        if (ch1_available && ch2_available &&
            chw_available)
        {
          LOWIFTMRRNodeInfo node(bssid, bssidInfo, operatingClass, phyType, ch,
                                 center_Ch1, center_Ch2, bandwidth);
          (*ppCmd)->ftmrr_info.push_back(node);
        }
        else
        {
          LOWIFTMRRNodeInfo node(bssid, bssidInfo, operatingClass, phyType, ch);
          (*ppCmd)->ftmrr_info.push_back(node);
        }
      }

      printf("%s - element initializing ftmrr_node\n", __FUNCTION__);
      bssid_available = false;
      bssid_info_available = false;
      op_class_available = false;
      ch_available = false;
      ch1_available = false;
      ch2_available = false;
      chw_available = false;
      phy_type_available = false;
      for (cur_elm_node = cur_node->children; cur_elm_node; cur_elm_node = cur_elm_node->next)
      {
        if (cur_elm_node->type == XML_ELEMENT_NODE)
        {
          printf("parse_ftmrr_info: Element, name: %s\n", cur_elm_node->name);
          if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_BSSID, xmlStrlen(XML_NODE_FTMRR_ELM_BSSID)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              unsigned char bssid_char[6];
              int status = sscanf(payloadData,
                                  "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                  &bssid_char[0],
                                  &bssid_char[1],
                                  &bssid_char[2],
                                  &bssid_char[3],
                                  &bssid_char[4],
                                  &bssid_char[5]);
              bssid.setMac(bssid_char);
              if (status < 6)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. bssid\n");
                break;
              }
              printf("%s - MAC Address from user: macAddr-parsed: " LOWI_MACADDR_FMT "\n",
                     __FUNCTION__,
                     LOWI_MACADDR(bssid));
              bssid_available = true;
            }
            else
            {
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_BSSID_INFO, xmlStrlen(XML_NODE_FTMRR_ELM_BSSID_INFO)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              uint8 bssid_Info[4] = {0};
              int status = sscanf(payloadData, "%02hhx%02hhx%02hhx%02hhx",
                                  &bssid_Info[0], &bssid_Info[1], &bssid_Info[2], &bssid_Info[3]);
              bssidInfo = *((uint32 *)bssid_Info);
              if (status < 4)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. bssidInfo\n");
                break;
              }
              bssid_info_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_CENTER_CH1, xmlStrlen(XML_NODE_FTMRR_ELM_CENTER_CH1)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &center_Ch1);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. center channel 1\n");
                break;
              }
              ch1_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_CENTER_CH2, xmlStrlen(XML_NODE_FTMRR_ELM_CENTER_CH2)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &center_Ch2);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. center channel 2\n");
                break;
              }
              ch2_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_CH, xmlStrlen(XML_NODE_FTMRR_ELM_CH)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &ch);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. channel\n");
                break;
              }
              ch_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_CH_WIDTH, xmlStrlen(XML_NODE_FTMRR_ELM_CH_WIDTH)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &bandwidth);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. channel width\n");
                break;
              }
              chw_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_OP_CLASS, xmlStrlen(XML_NODE_FTMRR_ELM_OP_CLASS)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &operatingClass);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. op class\n");
                break;
              }
              op_class_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
          else if (xmlStrncmp(cur_elm_node->name, XML_NODE_FTMRR_ELM_PHY_TYPE, xmlStrlen(XML_NODE_FTMRR_ELM_PHY_TYPE)) == 0)
          {
            const char *payloadData = (const char *)xmlNodeGetContent(cur_elm_node);
            printf("parse_ftmrr_info: Element, value: %s\n", payloadData);
            if (NULL != payloadData && strlen(payloadData) > 0)
            {
              int status = sscanf(payloadData, "%hhu", &phyType);
              if (status < 1)
              {
                printf("parse_ftmrr_info: Element,"
                       " value is not formed correctly. phy type\n");
                break;
              }
              phy_type_available = true;
            }
            else
            {
              // No need to continue
              break;
            }
          }
        }
      }
    }
  }
  if (bssid_available && bssid_info_available &&
      op_class_available && ch_available && phy_type_available)
  {
    if (ch1_available && ch2_available &&
        chw_available)
    {
      LOWIFTMRRNodeInfo node(bssid, bssidInfo, operatingClass, phyType, ch,
                             center_Ch1, center_Ch2, bandwidth);
      (*ppCmd)->ftmrr_info.push_back(node);
    }
    else
    {
      LOWIFTMRRNodeInfo node(bssid, bssidInfo, operatingClass, phyType, ch);
      (*ppCmd)->ftmrr_info.push_back(node);
    }
  }
  else
  {
    printf("%s - bssid:%d, info:%d, op_class:%d, ch:%d ch1:%d ch2:%d chw:%d phy:%d",
           __FUNCTION__, bssid_available, bssid_info_available,
           op_class_available, ch_available, ch1_available, ch2_available,
           chw_available, phy_type_available);
  }
}

static void
parse_elements(xmlNode *a_node, t_lowi_test_cmd **const ppCmd)
{
  LOWIPeriodicNodeInfo ap_info;
  LOWIDiscoveryScanRequest::eBand ch_info_band;
  LOWIMacAddress summary_info;

  //ap_info = &(*ppCmd)->ap_info;
  //ch_info = &(*ppCmd)->ch_info[(*ppCmd)->w_num_ch];
  //summary_info = &(*ppCmd)->summary_ap[(*ppCmd)->w_num_aps_summary];
  xmlNode *cur_node = NULL;

  // flags to sort out what all is found so far
  int band_found = 0;
  int ch_found = 0;
  int band_center_freq1_found = 0;
  int band_center_freq2_found = 0;
  int rttTypeFound = 0;
  int numFramesFound = 0;
  int bandwidthFound = 0;
  int preambleFound = 0;
  /*FTM Params*/
  int asapFound = 0;
  int lciFound = 0;
  int civicFound = 0;
  int burstExpFound = 0;
  int burstDurFound = 0;
  int burstPeriodFound = 0;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE)
    {
      printf("parse_elements: Element, name: %s\n", cur_node->name);
      if (xmlStrncmp(cur_node->name, XML_NODE_AP, xmlStrlen(XML_NODE_AP)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            unsigned char bssid_char[6];
            int status = sscanf(payloadData,
                                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                &bssid_char[0],
                                &bssid_char[1],
                                &bssid_char[2],
                                &bssid_char[3],
                                &bssid_char[4],
                                &bssid_char[5]);
            ap_info.bssid.setMac(bssid_char);

            if (rttTypeFound != 1) /* Type of RTT was not specified use RTTV2 as default */
            {
              ap_info.rttType = RTT2_RANGING;
            }
            if (numFramesFound != 1) /* Number for frames per burst not specified */
            {
              ap_info.num_pkts_per_meas = 0;
            }
            if (bandwidthFound != 1)
            {
              // BW not found from the xml
              // Default to 20 MHZ
              ap_info.bandwidth = qc_loc_fw::BW_20MHZ;
            }

            if (preambleFound != 1)
            {
              // Preamble not found from the xml
              // Default to LEGACY
              ap_info.preamble = qc_loc_fw::RTT_PREAMBLE_LEGACY;
            }

            /* FTM Params */
            if (asapFound != 1)
            {
              // asap not found from the xml
              // Default to ASAP=1
              FTM_SET_ASAP(ap_info.ftmRangingParameters);
            }
            if (lciFound != 1)
            {
              // lcinot requested from the xml
              // Default to Not requesting LCI
              FTM_CLEAR_LCI_REQ(ap_info.ftmRangingParameters);
            }
            if (civicFound != 1)
            {
              // Civic info not requested from the xml
              // Default to Not requesting Civic Info
              FTM_CLEAR_LOC_CIVIC_REQ(ap_info.ftmRangingParameters);
            }
            if (burstExpFound != 1)
            {
              // Burst exponent not found from the xml
              // Default to 1 burst
              FTM_SET_BURSTS_EXP(ap_info.ftmRangingParameters, 0);
            }
            if (burstDurFound != 1)
            {
              // Burst Duration not found from the xml
              // Default to 0
              FTM_SET_BURST_DUR(ap_info.ftmRangingParameters, 0);
            }
            if (burstPeriodFound != 1)
            {
              // Burst Period not found from the xml
              // Default to 0
              FTM_SET_BURST_PERIOD(ap_info.ftmRangingParameters, 0);
            }

            if (band_center_freq1_found != 1)
            {
              // band center freq1 not found from the xml
              // Default to 0
              ap_info.band_center_freq1 = 0;
            }

            if (band_center_freq2_found != 1)
            {
              // band center freq2 not found from the xml
              // Default to 0
              ap_info.band_center_freq2 = 0;
            }

            if (status < 6)
            {
              // Either the element does not contain the content or it is
              // not formatted correctly. Set all the entries to 0
              printf("parse_elements: Element,"
                     " value is not formed correctly\n");
              ap_info.bssid.setMac(0, 0);
            }
            else
            {
              if (band_found == 1 && ch_found == 1)
              {
                if (ap_info.rttType != RTT3_RANGING &&
                    ap_info.num_pkts_per_meas == 0)
                {
                  ap_info.num_pkts_per_meas = 5;
                }

                if (ap_info.rttType == RTT3_RANGING &&
                    FTM_GET_BURST_DUR(ap_info.ftmRangingParameters) != 15)
                {
                  FTM_SET_BURST_DUR(ap_info.ftmRangingParameters, 15);
                }
                printf("parse_elements: ftmParams: 0x%x, numFrames: %u\n",
                       ap_info.ftmRangingParameters, ap_info.num_pkts_per_meas);
                (*ppCmd)->ap_info.push_back(ap_info);
              }
            }
          }
          else if (xmlStrncmp(cur_node->parent->name, XML_NODE_SUMMARY,
                              xmlStrlen(XML_NODE_SUMMARY)) == 0)
          {
            unsigned char bssid_char[6];
            int status = sscanf(payloadData,
                                "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                &bssid_char[0],
                                &bssid_char[1],
                                &bssid_char[2],
                                &bssid_char[3],
                                &bssid_char[4],
                                &bssid_char[5]);
            summary_info.setMac(bssid_char);
            if (status < 6)
            {
              // Either the element does not contain the content or it is
              // not formatted correctly. Set all the entries to 0
              printf("parse_elements: Element,"
                     " value is not formed correctly\n");
              summary_info.setMac(0, 0);
            }
            else
            {
              (*ppCmd)->summary_ap.push_back(summary_info);
            } //else
          } //else if
        } //payloadData != NULL
      } //XML_NODE_AP
      if (xmlStrncmp(cur_node->name, XML_NODE_BAND,
                     xmlStrlen(XML_NODE_BAND)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          // Only 2G / 5G allowed as a valid band.
          int band = -1;
          int status = sscanf(payloadData, "%d", &band);
          if (status < 1 || band > 1 || band < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. Band = %d\n", band);
            continue;
          }
          LOWIDiscoveryScanRequest::eBand eband = LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ;;
          if (band == 1)
          {
            eband = LOWIDiscoveryScanRequest::FIVE_GHZ;
          }

          if (xmlStrncmp(cur_node->parent->parent->name,
                         XML_NODE_RANGING, xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            // ToDo: LOWI-AP: check if this is needed
            //ap_info->e_band = eband;
          }
          else
          {
            ch_info_band = eband;
          }
          band_found = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_CH, xmlStrlen(XML_NODE_CH)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          // Check if the channel is available and move to the next entry
          int ch = -1;
          int status = sscanf(payloadData, "%d", &ch);
          if (status < 1)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. Channel = %d\n", ch);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name,
                         XML_NODE_RANGING, xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            LOWIChannelInfo chan = LOWIChannelInfo(ch, LOWIDiscoveryScanRequest::BAND_ALL);
            ap_info.frequency = chan.getFrequency();
          }
          else
          {

            if (band_found == 1)
            {
              // Band was already found
              LOWIChannelInfo chan = LOWIChannelInfo(ch, ch_info_band);
              (*ppCmd)->ch_info.push_back(chan);
            }
          }
          ch_found = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_BAND_CENTER_FREQ1,
                     xmlStrlen(XML_NODE_BAND_CENTER_FREQ1)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          // Check if the channel is available and move to the next entry
          int freq = -1;
          int status = sscanf(payloadData, "%d", &freq);
          if (status < 1)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. band_center_freq1 = %d\n", freq);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            ap_info.band_center_freq1 = freq;
          }
          else
          {
            //ignore this node
            continue;
          }
          band_center_freq1_found = 1;
        }
      }

      if (xmlStrncmp(cur_node->name, XML_NODE_BAND_CENTER_FREQ2,
                     xmlStrlen(XML_NODE_BAND_CENTER_FREQ2)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          // Check if the channel is available and move to the next entry
          int freq = -1;
          int status = sscanf(payloadData, "%d", &freq);
          if (status < 1)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. band_center_freq2 = %d\n", freq);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            ap_info.band_center_freq2 = freq;
          }
          else
          {
            //ignore this node
            continue;
          }
          band_center_freq2_found = 1;
        }
      }

      if (xmlStrncmp(cur_node->name, XML_NODE_RTT_TYPE,
                     xmlStrlen(XML_NODE_RTT_TYPE)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          // Only RTT2 / RTT3 allowed as a valid RTT Types.
          int rttChoice = -1;
          int status = sscanf(payloadData, "%d", &rttChoice);
          printf("parse_elements: rttChoice: %d\n", rttChoice);
          if (status < 1 || rttChoice > 3 || rttChoice < 2)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. rttChoice = %d\n", rttChoice);
            continue;
          }
          eRttType rttType = RTT2_RANGING;
          if (rttChoice == RANGING_TYPE_CHOICE_V3)
          {
            rttType = RTT3_RANGING;
          }

          if (xmlStrncmp(cur_node->parent->parent->name,
                         XML_NODE_RANGING, xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            ap_info.rttType = rttType;
          }
          rttTypeFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_NUM_FRAMES_PER_BURST,
                     xmlStrlen(XML_NODE_NUM_FRAMES_PER_BURST)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n", payloadData);
        if (NULL != payloadData)
        {
          // Valid value >=0 frames.
          int numFramesPerBurst = -1;
          int status = sscanf(payloadData, "%d", &numFramesPerBurst);
          printf("parse_elements: number of frames: %d\n", numFramesPerBurst);
          if (status < 1 || numFramesPerBurst < 0 || numFramesPerBurst > 31)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. numFramesPerBurst = %d\n", numFramesPerBurst);
            continue;
          }
          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_NUM_FRAMES_PER_BURST)) == 0)
          {
            ap_info.num_pkts_per_meas = (uint32)numFramesPerBurst;
          }
          numFramesFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_RANGING_BW,
                     xmlStrlen(XML_NODE_RANGING_BW)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int bwChoice = -1;
          int status = sscanf(payloadData, "%d", &bwChoice);
          printf("parse_elements: bwChoice: %d\n", bwChoice);
          // Only RANGING_BW_20MHZ to RANGING_BW_160MHZ are valid BW
          if (status < 1 || bwChoice > BW_160MHZ
              || bwChoice < BW_20MHZ)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. bwChoice = %d\n", bwChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name,
                         XML_NODE_RANGING, xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            ap_info.bandwidth = (eRangingBandwidth)bwChoice;
          }
          bandwidthFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_RANGING_PREAMBLE,
                     xmlStrlen(XML_NODE_RANGING_PREAMBLE)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int preamble = -1;
          int status = sscanf(payloadData, "%d", &preamble);
          printf("parse_elements: preamble: %d\n", preamble);
          /* Only RANGING_PREAMBLE_LEGACY to RANGING_PREAMBLE_VHT
             are valid Preambles */
          if (status < 1 || preamble > RTT_PREAMBLE_VHT
              || preamble < RTT_PREAMBLE_LEGACY)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. preamble = %d\n", preamble);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            ap_info.preamble = (eRangingPreamble)preamble;
          }
          preambleFound = 1;
        }
      }

      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_ASAP,
                     xmlStrlen(XML_NODE_FTM_RANGING_ASAP)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int asapChoice = -1;
          int status = sscanf(payloadData, "%d", &asapChoice);
          printf("parse_elements: asapChoice: %d\n", asapChoice);
          if (status < 1 || asapChoice > 1
              || asapChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. asapChoice = %d\n", asapChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            if (asapChoice == 0)
            {
              FTM_CLEAR_ASAP(ap_info.ftmRangingParameters);
            }
            else if (asapChoice == 1)
            {
              FTM_SET_ASAP(ap_info.ftmRangingParameters);
            }
          }
          asapFound = 1;
        }
      }

      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_LCI,
                     xmlStrlen(XML_NODE_FTM_RANGING_LCI)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int lciChoice = -1;
          int status = sscanf(payloadData, "%d", &lciChoice);
          printf("parse_elements: lciChoice: %d\n", lciChoice);
          if (status < 1 || lciChoice > 1
              || lciChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. lciChoice = %d\n", lciChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            if (lciChoice == 0)
            {
              FTM_CLEAR_LCI_REQ(ap_info.ftmRangingParameters);
            }
            else if (lciChoice == 1)
            {
              FTM_SET_LCI_REQ(ap_info.ftmRangingParameters);
            }
          }
          lciFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_LOC_CIVIC,
                     xmlStrlen(XML_NODE_FTM_RANGING_LOC_CIVIC)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int civicChoice = -1;
          int status = sscanf(payloadData, "%d", &civicChoice);
          printf("parse_elements: civicChoice: %d\n", civicChoice);
          if (status < 1 || civicChoice > 1
              || civicChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. civicChoice = %d\n", civicChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            if (civicChoice == 0)
            {
              FTM_CLEAR_LOC_CIVIC_REQ(ap_info.ftmRangingParameters);
            }
            else if (civicChoice == 1)
            {
              FTM_SET_LOC_CIVIC_REQ(ap_info.ftmRangingParameters);
            }
          }
          civicFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_NUM_BURSTS_EXP,
                     xmlStrlen(XML_NODE_FTM_RANGING_NUM_BURSTS_EXP)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int burstExpChoice = -1;
          int status = sscanf(payloadData, "%d", &burstExpChoice);
          printf("parse_elements: burstExpChoice: %d\n", burstExpChoice);
          if (status < 1 || burstExpChoice > 15
              || burstExpChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. burstExpChoice = %d\n", burstExpChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            FTM_SET_BURSTS_EXP(ap_info.ftmRangingParameters, (uint32)burstExpChoice);
          }
          burstExpFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_BURST_DURATION,
                     xmlStrlen(XML_NODE_FTM_RANGING_BURST_DURATION)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int burstDurChoice = -1;
          int status = sscanf(payloadData, "%d", &burstDurChoice);
          printf("parse_elements: burstDurChoice: %d\n", burstDurChoice);
          if (status < 1 || burstDurChoice > 15
              || burstDurChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. burstDurChoice = %d\n", burstDurChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            FTM_SET_BURST_DUR(ap_info.ftmRangingParameters, (uint32)burstDurChoice);
          }
          burstDurFound = 1;
        }
      }
      if (xmlStrncmp(cur_node->name, XML_NODE_FTM_RANGING_BURST_PERIOD,
                     xmlStrlen(XML_NODE_FTM_RANGING_BURST_PERIOD)) == 0)
      {
        const char *payloadData = (const char *)xmlNodeGetContent(cur_node);
        printf("parse_elements: Element, value: %s\n",
               payloadData);
        if (NULL != payloadData)
        {
          int burstPerChoice = -1;
          int status = sscanf(payloadData, "%d", &burstPerChoice);
          printf("parse_elements: burstPerChoice: %d\n", burstPerChoice);
          if (status < 1 || burstPerChoice > 0xFFFF
              || burstPerChoice < 0)
          {
            // No need to continue
            printf("parse_elements: Element,"
                   " value is not formed correctly. burstPerChoice = %d\n", burstPerChoice);
            continue;
          }

          if (xmlStrncmp(cur_node->parent->parent->name, XML_NODE_RANGING,
                         xmlStrlen(XML_NODE_RANGING)) == 0)
          {
            FTM_SET_BURST_PERIOD(ap_info.ftmRangingParameters, (uint32)burstPerChoice);
          }
          printf("parse_elements-1: ftmParams: 0x%x\n", ap_info.ftmRangingParameters);
          burstPeriodFound = 1;
        }
      }
    }
    parse_elements(cur_node->children, ppCmd);
  }
}

static void
parse_gen_input_elements(xmlNode *a_node, t_lowi_test_cmd **const ppCmd)
{

  xmlNode *cur_node = NULL;
  switch ((*ppCmd)->cmd)
  {
  case LOWI_SET_LCI:
    for (cur_node = a_node; cur_node; cur_node = cur_node->next)
    {
      if (cur_node->type == XML_ELEMENT_NODE)
      {
        if (xmlStrncmp(cur_node->name, XML_NODE_LCI,
                       xmlStrlen(XML_NODE_LCI)) == 0)
        {
          parse_lci_info(cur_node->children, ppCmd);
          break;
        }
      }
      parse_gen_input_elements(cur_node->children, ppCmd);
    }
    break;
  case LOWI_SET_LCR:
    for (cur_node = a_node; cur_node; cur_node = cur_node->next)
    {
      if (cur_node->type == XML_ELEMENT_NODE)
      {
        if (xmlStrncmp(cur_node->name, XML_NODE_LCR,
                       xmlStrlen(XML_NODE_LCR)) == 0)
        {
          parse_lcr_info(cur_node->children, ppCmd);
          break;
        }
      }
      parse_gen_input_elements(cur_node->children, ppCmd);
    }
    break;
  case LOWI_FTMR_REQ:
    for (cur_node = a_node; cur_node; cur_node = cur_node->next)
    {
      if (cur_node->type == XML_ELEMENT_NODE)
      {
        if (xmlStrncmp(cur_node->name, XML_NODE_FTMRR,
                       xmlStrlen(XML_NODE_FTMRR)) == 0)
        {
          parse_ftmrr_info(cur_node->children, ppCmd);
          break;
        }
      }
      parse_gen_input_elements(cur_node->children, ppCmd);
    }
    break;
  default:
    printf("parse_gen_input_elements: unexpected command: %d\n", (*ppCmd)->cmd);
  }
}

/*=============================================================================
 * lowi_read_ap_list
 *
 * Description:
 *   Read the AP list for scan/stats parameters
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_read_ap_list(const char *const ap_list,  t_lowi_test_cmd **const ppCmd)
{
  int status;
  char lineBuf[LOWI_MAX_CMD_LEN];

  if ((ppCmd == NULL) || ((*ppCmd) == NULL))
  {
    fprintf(stderr, "Null Pointer ppCmd = %p\n",
            (void *)ppCmd);
    return;
  }

  if ((ap_list == NULL) || (strlen(ap_list) == 0))
  {
    /* Empty AP List. Use discovery Scan to create list if needed. */
    return;
  }

  // Open the xml file
  FILE *xmlFile = fopen(ap_list, "rb");
  if (xmlFile == NULL)
  {
    fprintf(stderr, "%s:%d: Error opening file %s: %s\n",
            __func__, __LINE__, ap_list, strerror(errno));
    xmlFile = fopen(DEFAULT_AP_LIST_FILE, "r");
    if (xmlFile == NULL)
    {
      fprintf(stderr, "%s:%d: Use Discovery Scan. Error opening file %s: %s\n",
              __func__, __LINE__, ap_list, strerror(errno));
    }
  }
  else
  {
    // File opened. Read it
    fseek(xmlFile, 0, SEEK_END);
    int xmlSize = (int)ftell(xmlFile);
    fseek(xmlFile, 0, SEEK_SET);
    char *buffer = NULL;
    if (xmlSize > 0)
    {
      buffer = (char *)LOWI_MALLOC(sizeof(char) * xmlSize);
    }
    if (NULL == buffer)
    {
      fprintf(stderr, "Unable to allocate memory for provided xml size = %x\n",
              xmlSize);
      return;
    }
    fread(buffer, xmlSize, 1, xmlFile);
    fclose(xmlFile);

    // We expect the  file format to be
/*
  <ranging>
    <ap>
     <band>1</band>
     <ch>11</ch>
     <rttType>2</rttType>
     <bw>0</bw>
     <mac>a0:b2:c3:d4:e5:f6</ap>
    </ap>
    <ap>
     <band>2</band>
     <ch>165</ch>
     <rttType>3</rttType>
     <bw>2</bw>
     <mac>a1:b3:c4:d5:e6:f7</ap>
    </ap>
  </ranging>
  <discovery>
    <chan>
     <band>1</band>
     <ch>11</ch>
    </chan>
    <chan>
     <band>2</band>
     <ch>165</ch>
    </chan>
  </discovery>
  <summary>
    <mac>a1:b3:c4:d5:e6:f7</mac>
    <mac>a0:b2:c3:d4:e5:f6</mac>
  </summary>
*/
    xmlDoc *doc = xmlParseMemory(buffer, xmlSize);
    xmlNode *root_element = xmlDocGetRootElement(doc);

    parse_elements(root_element, ppCmd);
    printf("parse elements completed\n");

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    // Free the buffer
    LOWI_FREE(buffer);
  }
  printf("Found %d APs in the input file\n", (*ppCmd)->ap_info.getNumOfElements());
  return;
}

/*=============================================================================
 * lowi_read_ap_list
 *
 * Description:
 *   Read the AP list for scan/stats parameters
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_read_gen_input_file(const char *const gen_input_file_name,  t_lowi_test_cmd **const ppCmd)
{
  if ((ppCmd == NULL) || ((*ppCmd) == NULL))
  {
    fprintf(stderr, "Null Pointer ppCmd = %p\n",
            (void *)ppCmd);
    return;
  }

  if ((gen_input_file_name == NULL) || (strlen(gen_input_file_name) == 0))
  {
    /* Empty AP List. Use discovery Scan to create list if needed. */
    return;
  }

  // Open the xml file
  FILE *xmlFile = fopen(gen_input_file_name, "rb");
  if (xmlFile == NULL)
  {
    fprintf(stderr, "%s:%d: Error opening file %s: %s\n",
            __func__, __LINE__, gen_input_file_name, strerror(errno));
    xmlFile = fopen(DEFAULT_AP_LIST_FILE, "r");
    if (xmlFile == NULL)
    {
      fprintf(stderr, "%s:%d: Error opening file %s: %s\n",
              __func__, __LINE__, DEFAULT_AP_LIST_FILE, strerror(errno));
    }
  }
  else
  {
    // File opened. Read it
    fseek(xmlFile, 0, SEEK_END);
    int xmlSize = (int)ftell(xmlFile);
    fseek(xmlFile, 0, SEEK_SET);
    char *buffer = NULL;
    if (xmlSize > 0)
    {
      buffer = (char *)LOWI_MALLOC(sizeof(char) * xmlSize);
    }
    if (NULL == buffer)
    {
      fprintf(stderr, "Unable to allocate memory for provided xml size = %x\n",
              xmlSize);
      return;
    }
    fread(buffer, xmlSize, 1, xmlFile);
    fclose(xmlFile);

    // We expect the  file format to be
    // refer to ap_list file for ranging and discovery requests
/*
  <lcr_info>
    <country_code>US</country_code>
    <civic_address>Carol Lane</civic_address>
  </lcr_info>
  <lci_info>
    <latitude>37.3740</latitude>
    <longitude>-121.9960</longitude>
    <altitude>7</altitude>
    <latitude_unc>61.8212</latitude_unc>
    <longitude_unc>61.9912</longitude_unc>
    <altitude_unc>59.9624</altitude_unc>
    <motion_pattern>0</motion_pattern>
    <floor>3</floor>
    <height_above_floor>2</height_above_floor>
    <height_unc>77</height_unc>
  </lci_info>
  <ftmrr>
    <element>
      <bssid></bssid>
      <info_bssid></info_bssid>
      <phy_type></phy_type>
      <op_class></op_class>
      <ch></ch>
      <center_ch1></center_ch1>
      <center_ch2></center_ch2>
      <width_ch></width_ch>
    </element>
  </ftmrr>
*/
    xmlDoc *doc = xmlParseMemory(buffer, xmlSize);
    xmlNode *root_element = xmlDocGetRootElement(doc);

    parse_gen_input_elements(root_element, ppCmd);
    printf("parse elements completed\n");

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    // Free the buffer
    LOWI_FREE(buffer);
  }
  return;
}

/*=============================================================================
 * usage
 *
 * Description:
 *   Prints usage information for lowi_test
 *
 * Return value:
 *   void
 ============================================================================*/
static void usage(char * cmd)
{
  fprintf(stderr, "Usage: %s %s %s %s %s %s %s %s %s %s %s\n", cmd,
        "[-o output_file]",
        "[-s summary_file]",
        "[-n number of Scans]", "[-d delay between scans]",
        "[-p -r -f -pr -r3 -pr3 -a -ar -ar3 ap_list|ap_file]",
        "[-b band for discovery scan 0 - 2.4Ghz, 1 - 5Ghz, 2 - All]",
        "[-bw bandwidth for ranging scan 0 - 20Mhz, 1 - 40 Mhz, 2 - 80 Mhz, 3 - 160 Mhz]",
        "[-mf measurement age filter for discovery scan 0 sec - 180 sec]",
        "[-ft fallback tolerance for discovery scan 0 sec - 180 sec]",
        "[-t request timeout in seconds 0 - 86400]");
}

/*=============================================================================
 * lowi_parse_args
 *
 * Description:
 *   Parse the arguments into LOWI test command
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_parse_args(const int argc, char * argv[],
                            t_lowi_test_cmd ** ppCmd)
{
  int i;
  char *ap_list = NULL;
  char *gen_input_file_name = NULL;

  if (argc < 1)
  {
    return;
  }

  *ppCmd = (t_lowi_test_cmd *)LOWI_CALLOC(1, sizeof(t_lowi_test_cmd));
  if (NULL == *ppCmd)
  {
    return;
  }

  (*ppCmd)->cmd = LOWI_MAX_SCAN;
  (*ppCmd)->delay = 10000; /* 10000 milliseconds */
  (*ppCmd)->num_requests = 1; /* 1 request by default */
  (*ppCmd)->timeout_offset = 0;
  (*ppCmd)->band = LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ;
  (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::PASSIVE_SCAN;  /* Passive discovery scan by default*/
  (*ppCmd)->meas_age_filter_sec = 10;  /* 10 seconds default */
  (*ppCmd)->fallback_tolerance = 0;   /* Do not care as long as it is 0 */
  (*ppCmd)->ranging_bandwidth = BW_20MHZ; // Default 20 Mhz
  (*ppCmd)->rttType = RTT2_RANGING;  // Default Ranging is RTT V2
  (*ppCmd)->lci_info = NULL;
  (*ppCmd)->lcr_info = NULL;


  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-o") == 0)
    {
      /* -o followed by file name */
      lowi_test.out_fp = fopen(argv[++i], "w");
      if (lowi_test.out_fp == NULL)
      {
        fprintf(stderr, "%s:%s:%d: Error opening file %s: %s\n",
              argv[0], __func__,__LINE__, argv[i], strerror(errno));
      }
    }
    else if (strcmp(argv[i], "-s") == 0)
    {
      /* -s followed by file name */
      lowi_test.summary_fp = fopen(argv[++i], "w");
      if (lowi_test.summary_fp == NULL)
      {
        fprintf(stderr, "%s:%s:%d: Error opening file %s: %s\n",
              argv[0], __func__,__LINE__, argv[i], strerror(errno));
      }
    }
    else if ((strcmp(argv[i], "-f") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_ASYNC_DISCOVERY_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::PASSIVE_SCAN;  // Scan type passive for discovery scan
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-p") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_DISCOVERY_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::PASSIVE_SCAN;  // for passive discovery scan
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-a") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_DISCOVERY_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::ACTIVE_SCAN;  // Scan type active for discovery scan
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-r") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_RTS_CTS_SCAN;
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-r3") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_RTS_CTS_SCAN;
      (*ppCmd)->rttType = RTT3_RANGING;  // Set Ranging to RTT V3
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-pr") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_BOTH_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::PASSIVE_SCAN;  // 1 for passive discovery scan
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-pr3") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_BOTH_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::PASSIVE_SCAN;  // 1 for passive discovery scan
      (*ppCmd)->rttType = RTT3_RANGING;  // Set Ranging to RTT V3
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-ar") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_BOTH_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::ACTIVE_SCAN;  // Scan type active for discovery scan
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-ar3") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_BOTH_SCAN;
      (*ppCmd)->scan_type = LOWIDiscoveryScanRequest::ACTIVE_SCAN;  // Scan type active for discovery scan
      (*ppCmd)->rttType = RTT3_RANGING;  // Set Ranging to RTT V3
      ap_list = ( ( i < argc - 1 && argv[i+1][0] != '-') ?
                  argv[++i] : NULL );
      lowi_read_ap_list(ap_list, ppCmd);
    }
    else if ((strcmp(argv[i], "-nrr") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_NR_REQ;
    }
    else if ((strcmp(argv[i], "-lci") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_SET_LCI;
      gen_input_file_name = ((i < argc - 1 && argv[i + 1][0] != '-') ?
                 argv[++i] : NULL);
      lowi_read_gen_input_file(gen_input_file_name, ppCmd);
    }
    else if ((strcmp(argv[i], "-lcr") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_SET_LCR;
      gen_input_file_name = ((i < argc - 1 && argv[i + 1][0] != '-') ?
                             argv[++i] : NULL);
      lowi_read_gen_input_file(gen_input_file_name, ppCmd);
    }
    else if ((strcmp(argv[i], "-w") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_WRU_REQ;
      int retVal = 0;
      i++;
      if (i < argc)
      {
        LOWIPeriodicNodeInfo apInfoP;
        unsigned char bssid_char[6];
        retVal = sscanf(argv[i], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &bssid_char[0],
                        &bssid_char[1],
                        &bssid_char[2],
                        &bssid_char[3],
                        &bssid_char[4],
                        &bssid_char[5]);
        apInfoP.bssid.setMac(bssid_char);
        (*ppCmd)->ap_info.push_back(apInfoP);
        printf("%s - MAC Address from user: %s, macAddr-parsed: " LOWI_MACADDR_FMT " status: %d",
               __FUNCTION__,
               argv[i],
               LOWI_MACADDR(apInfoP.bssid),
               retVal);
      }

      else
      {
        printf("%s: -w flag provided with no mac address",
               __FUNCTION__);
      }
    }
    else if ((strcmp(argv[i], "-ftmrr") == 0) &&
             ((*ppCmd)->cmd == LOWI_MAX_SCAN))
    {
      (*ppCmd)->cmd = LOWI_FTMR_REQ;
      int retVal = 0;
      if ((i + 3) > argc)
      {
        printf("-ftmrr flag provided with wrong arguments list args-expected %d given %d\n",
               i+4, argc);
      }
      else
      {
        LOWIPeriodicNodeInfo apInfoP;
        unsigned char bssid_char[6];
        retVal = sscanf(argv[++i], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &bssid_char[0],
                        &bssid_char[1],
                        &bssid_char[2],
                        &bssid_char[3],
                        &bssid_char[4],
                        &bssid_char[5]);
        apInfoP.bssid.setMac(bssid_char);
        (*ppCmd)->ap_info.push_back(apInfoP);
        printf("%s - MAC Address from user: %s, macAddr-parsed: " LOWI_MACADDR_FMT " status: %d \n",
               __FUNCTION__,
               argv[i],
               LOWI_MACADDR(apInfoP.bssid),
               retVal);
        retVal = sscanf(argv[++i], "%hu", &(*ppCmd)->rand_inter);
        gen_input_file_name = ((i < argc - 1 && argv[i + 1][0] != '-') ?
                               argv[++i] : NULL);
        printf("%s - input file %s\n", __FUNCTION__, (NULL==gen_input_file_name)? "NULL" : gen_input_file_name);
        lowi_read_gen_input_file(gen_input_file_name, ppCmd);
      }

    }
    else if (strcmp(argv[i], "-mac") == 0)
    {
      int retVal = 0;
      i++;
      if (i < argc)
      {
        LOWIPeriodicNodeInfo apInfoP;
        unsigned char bssid_char[6];
        retVal = sscanf(argv[i], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &bssid_char[0],
                        &bssid_char[1],
                        &bssid_char[2],
                        &bssid_char[3],
                        &bssid_char[4],
                        &bssid_char[5]);
        apInfoP.bssid.setMac(bssid_char);
        (*ppCmd)->ap_info.push_back(apInfoP);
        printf( "%s - MAC Address from user: %s, macAddr-parsed: " LOWI_MACADDR_FMT " status: %d",
                       __FUNCTION__,
                       argv[i],
                       LOWI_MACADDR(apInfoP.bssid),
                       retVal);
      }
      else
      {
        printf( "%s: -mac flag provided with no mac address",
                       __FUNCTION__);
      }

    }
    else if (strcmp(argv[i], "-n") == 0)
    {
      int numrequested = atoi(argv[++i]);
      (*ppCmd)->num_requests = LOWI_APPLY_LIMITS(numrequested, 1, 30000);
    }
    else if (strcmp(argv[i], "-d") == 0)
    {
      int delay = atoi(argv[++i]);
      (*ppCmd)->delay = LOWI_APPLY_LIMITS(delay, 10, 30000);
    }
    else if (strcmp(argv[i], "-t") == 0)
    {
      // Timeout Seconds
      int timeout = atoi(argv[++i]);
      (*ppCmd)->timeout_offset = LOWI_APPLY_LIMITS(timeout, 0, 86400);
    }
    else if (strcmp(argv[i], "-b") == 0)
    {
      // Band
      int band = atoi(argv[++i]);
      (*ppCmd)->band = (LOWIDiscoveryScanRequest::eBand)LOWI_APPLY_LIMITS(band, 0, 2);
    }
    else if (strcmp(argv[i], "-bw") == 0)
    {
      // Bandwidth
      int bw = atoi(argv[++i]);
      (*ppCmd)->ranging_bandwidth = (eRangingBandwidth)LOWI_APPLY_LIMITS(bw,
          BW_20MHZ, BW_160MHZ);
    }
    else if (strcmp(argv[i], "-mf") == 0)
    {
      // Measurement Age filter seconds - applicable to discovery scan only
      int meas_age = atoi(argv[++i]);
      (*ppCmd)->meas_age_filter_sec = LOWI_APPLY_LIMITS(meas_age, 0, 180);
    }
    else if (strcmp(argv[i], "-ft") == 0)
    {
      // Fallback tolerance seconds - applicable to discovery scan only
      int fb_tol = atoi(argv[++i]);
      (*ppCmd)->fallback_tolerance = LOWI_APPLY_LIMITS(fb_tol, 0, 180);
    }
    else
    {
      usage(argv[0]);
    }
  }
  if (lowi_test.out_fp == NULL)
  {
    /* Use default file */
    lowi_test.out_fp = fopen(LOWI_OUT_FILE_NAME, "w");
    if (lowi_test.out_fp == NULL)
    {
      fprintf(stderr, "%s:%s:%d: Error opening file %s. %s\n",
            argv[0], __func__,__LINE__, LOWI_OUT_FILE_NAME, strerror(errno));
    }
  }
  if (lowi_test.out_fp != NULL)
  {
    fprintf(lowi_test.out_fp, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
            "Time", "Time(days)", "Scan Type",
            "Seq#", "Mac_Address", "Channel", "RSSI", "RTT", "RspTime", "MeasAge");
  }
  if ((*ppCmd)->cmd == LOWI_MAX_SCAN)
  {
    LOWI_FREE((*ppCmd)->lci_info);
    (*ppCmd)->lci_info = NULL;
    LOWI_FREE((*ppCmd)->lcr_info);
    (*ppCmd)->lcr_info = NULL;
    LOWI_FREE(*ppCmd);
    flush_ftmrr_info(ppCmd);
    *ppCmd = NULL;
    usage(argv[0]);
  }
}

/*=============================================================================
 * lowi_get_cmd
 *
 * Description:
 *   Read the command line arguments
 *
 * Return value:
 *   LOWI test command or NULL if bad arguments
 ============================================================================*/
static t_lowi_test_cmd* lowi_get_cmd(const int argc, char *argv[])
{
  t_lowi_test_cmd *cmd_ptr = NULL;

  lowi_parse_args(argc, argv, &cmd_ptr);
  printf( "lowi_parse_args completed\n");

  return cmd_ptr;
}

/*=============================================================================
 * lowi_wait_for_signal_or_timeout
 *
 * Description:
 *   Wait for a signal or timeout
 *
 * Return value:
 *   1 if signaled
 *   0 if timed out
 ============================================================================*/
static int lowi_wait_for_signal_or_timeout(uint32 timeout, pthread_cond_t *cond)
{
  struct timespec     present_time;
  int ret_val = -1;

  if (0 == clock_gettime(CLOCK_REALTIME, &present_time))
  {

    present_time.tv_sec += timeout / MSECS_PER_SEC;
    present_time.tv_nsec += ((timeout % MSECS_PER_SEC) * NSECS_PER_MSEC);
    if (present_time.tv_nsec > NSECS_PER_SEC)
    {
      present_time.tv_sec++;
      present_time.tv_nsec -= (NSECS_PER_SEC);
    }

    pthread_mutex_lock(&lowi_test.mutex);
    ret_val = pthread_cond_timedwait(cond,
                                     &lowi_test.mutex,
                                     &present_time);
    pthread_mutex_unlock(&lowi_test.mutex);
  }
  return ret_val;
}

/*=============================================================================
 * lowi_wait_for_rts_scan_results
 *
 * Description:
 *   Wait for RTT Measurement results or timeout
 *
 * Return value:
 *   1 if results were received
 *   0 if timed out waiting for response
 ============================================================================*/
static int lowi_wait_for_rts_scan_results(uint32 timeout)
{
  int ret_val = lowi_wait_for_signal_or_timeout(timeout, &lowi_test.rs_cond);
  return ret_val;
}

/*=============================================================================
 * lowi_wait_for_where_are_you
 *
 * Description:
 *   Wait for Where are you results or timeout
 *
 * Return value:
 *   1 if results were received
 *   0 if timed out waiting for response
 ============================================================================*/
static int lowi_wait_for_where_are_you(uint32 timeout)
{
  // we are reusing rs_cond as lowi_test doesn't support ranging
  // and where are you request in parallel
  int ret_val = lowi_wait_for_signal_or_timeout(timeout, &lowi_test.rs_cond);
  return ret_val;
}


/*=============================================================================
 * lowi_wait_for_passive_scan_results
 *
 * Description:
 *    Wait for passive scan results until timeout
 *
 * Return value:
 *   1 if results were received
 *   0 if timed out waiting for response
 ============================================================================*/
static int lowi_wait_for_passive_scan_results(uint32 timeout)
{
  int ret_val = lowi_wait_for_signal_or_timeout(timeout, &lowi_test.ps_cond);
  return ret_val;
}

/*=============================================================================
 * lowi_copy_ap_list
 *
 * Description:
 *   Copy AP list into cmd_ptr from recent results.
 *   This is used to figure out the set of APs that we
 *   will perform RTT on.
 *
 * NOTE: This function is not called if xml provides the
 * ranging data.
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_copy_ap_list(t_lowi_test_cmd *cmd_ptr)
{
  int i, j;

  if (cmd_ptr == NULL)
  {
    fprintf(stderr, "%s:%d: Null Pointer.\n",
            __func__, __LINE__);
    return;
  }

  if (cmd_ptr->ap_info.getNumOfElements() == 0)
  {
    /* Copy the AP list from the results */
    for ((i = 0);
         (i < lowi_test.meas_results.getNumOfElements());
         i++)
    {
      // For RTT Measurement, we only scan the APs whose channel
      // number matches with the band requested in scan command
      bool scan_this_ap = false;
      LOWIChannelInfo ch = LOWIChannelInfo(lowi_test.meas_results[i]->frequency);
      // Scan command is requesting 5 Ghz
      if (lowi_cmd->band == LOWIDiscoveryScanRequest::FIVE_GHZ)
      {
        if (ch.getChannel() >= 14)
        {
          scan_this_ap = true;
        }
      }
      // Scan command is requesting for both 5 and 2.4 Ghz
      else if (lowi_cmd->band == LOWIDiscoveryScanRequest::BAND_ALL)
      {
        scan_this_ap = true;
      }
      // Scan command does not specifiy or specify 2.4 Ghz
      // In this case, we will only scan 2.4 Ghz
      else
      {
        if (ch.getChannel() >= 14)
        {
          scan_this_ap = true;
        }
      }

      if (scan_this_ap == true)
      {
        LOWIPeriodicNodeInfo ap_info_node;
        ap_info_node.bssid = lowi_test.meas_results[i]->bssid;
        ap_info_node.frequency = ch.getFrequency();

        // Load global rttType and bandwidth because they are
        // not available is the scan results.
        // Also, they are not received from the xml.
        ap_info_node.rttType = cmd_ptr->rttType;
        ap_info_node.bandwidth = cmd_ptr->ranging_bandwidth;
        ap_info_node.num_pkts_per_meas = LOWI_TEST_DEFAULT_RTT_MEAS;
        cmd_ptr->ap_info.push_back(ap_info_node);
      }
    }
  }
  else
  {
    /* Copy AP Channel information from the results */
    for (i = 0; i < cmd_ptr->ap_info.getNumOfElements(); i++)
    {
      for (j = 0; j < lowi_test.meas_results.getNumOfElements(); i++)
      {
        if (cmd_ptr->ap_info[i].bssid.compareTo(lowi_test.meas_results[i]->bssid) == 0)
        {
          cmd_ptr->ap_info[i].frequency = lowi_test.meas_results[j]->frequency;
        }
      }
    }

    //if (cmd_ptr->w_num_aps > MAX_BSSIDS_ALLOWED_FOR_RANGING_SCAN)
    //{
    //  cmd_ptr->w_num_aps = MAX_BSSIDS_ALLOWED_FOR_RANGING_SCAN;
    //}

    for (i = 0; i < cmd_ptr->ap_info.getNumOfElements(); i++)
    {
      LOWIChannelInfo ch = LOWIChannelInfo(cmd_ptr->ap_info[i].frequency);
      if (ch.getChannel() == 0xFFFF)
      {
        LOWIPeriodicNodeInfo node;
        cmd_ptr->ap_info.pop_back(node);
        /* Cannot scan this AP. Copy the last AP here, if valid */
        if (i < cmd_ptr->ap_info.getNumOfElements() - 1)
        {
          cmd_ptr->ap_info[i] = node;
        }

      }
    }
  }
}

/*=============================================================================
 * lowi_test_start_rts_cts_scan
 *
 * Description:
 *    Start the RTT Measurement
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_start_rts_cts_scan(t_lowi_test_cmd *cmd_ptr)
{

  int retVal = -1;
  retVal =  lowi_queue_rts_cts_scan_req(&cmd_ptr->ap_info);
  return retVal;
}

/*=============================================================================
 * lowi_test_do_rts_cts_scan
 *
 * Description:
 *    Do the RTT Measurement
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_do_rts_cts_scan(const uint32 seq_no)
{
  int ret_val;

  printf("*****STARTING RTT Measurement (%u)*****\n", seq_no);
  lowi_test.scan_start_ms = lowi_test_get_time_ms();
  ret_val = lowi_test_start_rts_cts_scan(&rts_scan_cmd);
  if (ret_val != 0)
  {
    printf("RTT Measurement request (%u of %d)- FAILED",
           seq_no, lowi_cmd->num_requests);
    return ret_val;
  }
  else
  {
    ret_val = lowi_wait_for_rts_scan_results(LOWI_SCAN_TIMEOUT_MS);
  }
  printf("RTT Measurement request (%u of %d)- %s, Rsp time %d",
         seq_no, lowi_cmd->num_requests,
         ret_val ? "Timeout" : "SUCCESS",
         (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));
  return ret_val;
}

/*=============================================================================
 * lowi_test_do_neighbor_report_request
 *
 * Description:
 *    Do Neighbor Report Request
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_do_neighbor_report_request(const uint32 seq_no)
{
  int ret_val = -1;

  printf("*****STARTING NEIGHBOR REPORT REQUEST (%u)*****", seq_no);
  lowi_test.scan_start_ms = lowi_test_get_time_ms();
  if (lowi_cmd == NULL)
  {
    return ret_val;
  }

  ret_val = lowi_queue_nr_request();
  if (ret_val != 0)
  {
    printf("STARTING NEIGHBOR REPORT REQUEST (%u of %d)- FAILED",
           seq_no, lowi_cmd->num_requests);
  }
  else
  {

    printf("STARTING NEIGHBOR REPORT REQUEST (%u of %d)- SUCCESS, Rsp time %d",
           seq_no, lowi_cmd->num_requests,
           (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));
  }
  return ret_val;
}

/*=============================================================================
 * lowi_test_set_lci
 *
 * Description:
 *    This function sets the LCI information.
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_set_lci(const uint32 seq_no)
{
  int ret_val = -1;

  printf("\n*****SET LCI INFORMATION (%u)*****\n", seq_no);
  if (lowi_cmd == NULL)
  {
    return ret_val;
  }
  if (lowi_cmd->lci_info == NULL)
  {
    printf("SET LCI INFORMATION (%u)- FAILED(NOT AVAILABLE)\n", seq_no);
    return ret_val;
  }

  ret_val = lowi_queue_set_lci(lowi_cmd->lci_info);
  if (ret_val != 0)
  {
    printf("SET LCI INFORMATION (%u)- FAILED\n", seq_no);
  }
  else
  {

    printf("SET LCI INFORMATION (%u)- SUCCESS\n", seq_no);
  }
  return ret_val;
}

/*=============================================================================
 * lowi_test_set_lcr
 *
 * Description:
 *    This function sets the LCR information.
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_set_lcr(const uint32 seq_no)
{
  int ret_val = -1;

  printf("\n*****SET LCR INFORMATION (%u)*****\n", seq_no);
  if (lowi_cmd == NULL)
  {
    return ret_val;
  }
  if (lowi_cmd->lcr_info == NULL)
  {
    printf("SET LCR INFORMATION (%u)- FAILED(NOT AVAILABLE)\n", seq_no);
    return ret_val;
  }

  ret_val = lowi_queue_set_lcr(lowi_cmd->lcr_info);
  if (ret_val != 0)
  {
    printf("SET LCR INFORMATION (%u)- FAILED\n", seq_no);
  }
  else
  {

    printf("SET LCR INFORMATION (%u)- SUCCESS\n", seq_no);
  }
  return ret_val;
}

/*=============================================================================
 * lowi_test_where_are_you
 *
 * Description:
 *    This function requests the where are you information.
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_where_are_you(const uint32 seq_no)
{
  int ret_val = -1;

  printf("\n*****WHERE ARE YOU REQUEST (%u)*****\n", seq_no);
  if (lowi_cmd == NULL)
  {
    return ret_val;
  }
  if (lowi_cmd->ap_info.getNumOfElements() < 1)
  {
    printf("WHERE ARE YOU REQUEST (%u)- FAILED(TARGET STA NOT AVAILABLE)\n", seq_no);
    return ret_val;
  }

  lowi_test.scan_start_ms = lowi_test_get_time_ms();
  ret_val = lowi_queue_where_are_you(lowi_cmd->ap_info[0].bssid);
  if (ret_val != 0)
  {
    printf("WHERE ARE YOU REQUEST (%u)- FAILED\n", seq_no);
  }
  else
  {
    //ret_val = lowi_wait_for_where_are_you(LOWI_SCAN_TIMEOUT_MS);
  }


  printf("WHERE ARE YOU REQUEST (%u)- %s, Rsp time %d \n",
         seq_no, ret_val ? "Timeout" : "SUCCESS",
         (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));

  return ret_val;
}

/*=============================================================================
 * lowi_test_ftmrr
 *
 * Description:
 *    This function requests the FTM Ranging measurements.
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_ftmrr(const uint32 seq_no)
{
  int ret_val = -1;

  printf("\n*****FTM RANGE REQUEST (%u)*****\n", seq_no);
  if (lowi_cmd == NULL)
  {
    return ret_val;
  }
  if (lowi_cmd->ap_info.getNumOfElements() < 1)
  {
    printf("FTM RANGE REQUEST (%u)- FAILED(TARGET STA NOT AVAILABLE)\n", seq_no);
    return ret_val;
  }
  if (lowi_cmd->ftmrr_info.getNumOfElements() < 1)
  {
    printf("FTM RANGE REQUEST (%u)- FAILED(NODE INFO NOT AVAILABLE)\n", seq_no);
    return ret_val;
  }

  lowi_test.scan_start_ms = lowi_test_get_time_ms();
  ret_val = lowi_queue_ftmrr(lowi_cmd->ap_info[0].bssid,
                             lowi_cmd->rand_inter, lowi_cmd->ftmrr_info);
  if (ret_val != 0)
  {
    printf("FTM RANGE REQUEST (%u)- FAILED\n", seq_no);
  }
  else
  {
    //ret_val = lowi_wait_for_where_are_you(LOWI_SCAN_TIMEOUT_MS);
  }


  printf("FTM RANGE REQUEST (%u)- %s, Rsp time %d \n",
         seq_no, ret_val ? "Timeout" : "SUCCESS",
         (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));

  return ret_val;
}

/*=============================================================================
 * Function description:
 *    This function will return current time in number of milli-seconds
 *    since Epoch (00:00:00 UTC, January 1, 1970Jan 1st, 1970).
 *
 * Parameters:
 *    none
 *
 * Return value:
 *    number of milli-seconds since epoch.
 *=============================================================================*/

int64 getCurrentTimeMs ()
{
  struct timeval      present_time;
  int64              current_time_msec = 0;

  // present time: seconds, and microseconds
  if(0 == gettimeofday(&present_time, NULL))
  {

    // Calculate absolute expire time (to avoid data overflow)
    current_time_msec = present_time.tv_sec;
    current_time_msec *= 1000;  // convert to milli-seconds

    // covert the micro-seconds portion to milliseconds
    current_time_msec += (present_time.tv_usec + 500) / 1000;
  }
  return current_time_msec;
}

/*=============================================================================
 * lowi_test_do_async_discovery_scan
 *
 * Description:
 *    Starts the async discovery scan
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_do_async_discovery_scan(const uint32 seq_no)
{
  int ret_val;
  int ii = 0;
  printf( "*****SENDING ASYNC DISCOVERY SCAN REQ (%u of %d)*****",
                 seq_no, lowi_cmd->num_requests); /* BAA */
  lowi_test.scan_start_ms = lowi_test_get_time_ms();

  ret_val = lowi_queue_async_discovery_scan_result_req (lowi_cmd->timeout_offset);

  if (ret_val != 0)
  {
    printf( "Async Discovery scan request (%u of %d)- FAILED\n",
                   seq_no, lowi_cmd->num_requests);
  }
  else
  {
    ret_val = lowi_wait_for_passive_scan_results(LOWI_ASYNC_SCAN_TIMEOUT_MS);
  }
  printf( "Async Discovery scan request (%u of %d)- %s, Rsp time %d\n",
                 seq_no, lowi_cmd->num_requests,
                 ret_val ? "Timeout" : "SUCCESS",
                 (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));
  return ret_val;
}


/*=============================================================================
 * lowi_test_do_passive_scan
 *
 * Description:
 *    Start the Passive Scan
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_do_passive_scan(const uint32 seq_no)
{
  int ret_val;
  int ii = 0;
  printf( "*****STARTING DISCOVERY SCAN (%u of %d)*****",
                 seq_no, lowi_cmd->num_requests); /* BAA */
  lowi_test.scan_start_ms = lowi_test_get_time_ms();

  int64 request_timeout = 0;
  if (0 != lowi_cmd->timeout_offset)
  {
    request_timeout = getCurrentTimeMs () +
        (lowi_cmd->timeout_offset*1000);
  }

  // check if the discovery scan channels are specified
  if (0 != lowi_cmd->fallback_tolerance)
  {
    ret_val = lowi_queue_passive_scan_req_xtwifi(1,
        lowi_cmd->fallback_tolerance,
        lowi_cmd->meas_age_filter_sec);
  }
  else if (0 != lowi_cmd->ch_info.getNumOfElements())
  {
    // Ignore the band specified through the -b option and use the channels
    ret_val = lowi_queue_discovery_scan_req_ch(&lowi_cmd->ch_info,
        request_timeout, lowi_cmd->scan_type,
        lowi_cmd->meas_age_filter_sec);
  }
  else
  {
    ret_val = lowi_queue_discovery_scan_req_band(lowi_cmd->band, request_timeout,
        lowi_cmd->scan_type, lowi_cmd->meas_age_filter_sec);
  }

  if (ret_val != 0)
  {
    printf( "Discovery scan request (%u of %d)- FAILED",
                   seq_no, lowi_cmd->num_requests);
  }
  else
  {
    ret_val = lowi_wait_for_passive_scan_results(LOWI_SCAN_TIMEOUT_MS);
  }
  printf( "Discovery scan request (%u of %d)- %s, Rsp time %d",
                 seq_no, lowi_cmd->num_requests,
                 ret_val ? "Timeout" : "SUCCESS",
                 (int32)(lowi_test.scan_end_ms - lowi_test.scan_start_ms));
  return ret_val;
}

/*=============================================================================================
 * prepareRtsCtsParam
 *
 * Function description:
 *   Prepare the cmd_ptr for RTT Measurement.
 *   Make sure that the AP list is valid
 *
 * Return value: Number of APs for RTT Measurement
 =============================================================================================*/
static int prepareRtsCtsParam(t_lowi_test_cmd *cmd_ptr,
                          const uint32 seq_no)
{
  int i, ret_val;
  boolean passive_scan_needed = FALSE;

  if (cmd_ptr->ap_info.getNumOfElements() == 0)
  {
    passive_scan_needed = TRUE;
  }
  else
  {
    for (i = 0; i < cmd_ptr->ap_info.getNumOfElements(); i++)
    {
      LOWIChannelInfo ch = LOWIChannelInfo(cmd_ptr->ap_info[i].frequency);
      if (ch.getChannel() == 0xFFFF)
      {
        passive_scan_needed = TRUE;
        break;
      }
    }
  }

  if (passive_scan_needed)
  {
    /* Request Discovery Scan */
    ret_val = lowi_test_do_passive_scan(seq_no);
    if (ret_val != 0)
    {
      printf( "AP List incomplete. Discovery scan request - FAILED.");
      return 0;
    }
    else
    {
      printf( "AP List complete. Discovery scan request - SUCCESS.");
      /* Got Discovery scan results */
      lowi_copy_ap_list(cmd_ptr);
    }
  }
  return cmd_ptr->ap_info.getNumOfElements();
}

/*=============================================================================
 * lowi_test_do_combo_scan
 *
 * Description:
 *    Do a discovery Scan followed by ranging Scan
 *
 * Return value:
 *   0: Success
 ============================================================================*/
static int lowi_test_do_combo_scan(const uint32 seq_no)
{
  int ret_val;

  printf( "*****STARTING Combo SCAN (%u of %d)*****",
                 seq_no, lowi_cmd->num_requests); /* BAA */
  rts_scan_cmd.ap_info.flush(); /* Need to build AP List */
  if (prepareRtsCtsParam (&rts_scan_cmd, seq_no) == 0)
  {
    fprintf(stderr, "Incorrect AP List. Num APs = %d",
            rts_scan_cmd.ap_info.getNumOfElements());
    return -1;
  }
  else
  {
    ret_val = lowi_test_do_rts_cts_scan(seq_no);
    if (ret_val != 0)
    {
      printf( "Combo scan request (%u of %d)- Timeout",
                     seq_no, lowi_cmd->num_requests);
    }
  }
  return ret_val;
}

/*=============================================================================
 * lowi_test_init
 *
 * Description:
 *    Init the LOWI Test module
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_init(void)
{
  struct timespec ts;


  pthread_mutex_init(&lowi_test.mutex, NULL);
  pthread_cond_init(&lowi_test.ps_cond, NULL);
  pthread_cond_init(&lowi_test.rs_cond, NULL);
  memset(&rts_scan_cmd, 0, sizeof(rts_scan_cmd));
  lowi_test.out_fp = NULL;
  /* Sequence number to tag the measurements */
  lowi_test.seq_no = 1;
#if ENABLED_BOOTTIME_SUPPORT
  lowi_test.clock_id  = (clock_gettime(CLOCK_BOOTTIME_ALARM, &ts) == 0 ?
                         CLOCK_BOOTTIME_ALARM : CLOCK_REALTIME);
#else
  lowi_test.clock_id  = CLOCK_REALTIME;
#endif

  printf( "clock_id = %d\n", lowi_test.clock_id);
  lowi_test.timerFd = timerfd_create(lowi_test.clock_id, 0);
  if (-1 == lowi_test.timerFd)
  {
    printf("timerfd_create failed %d, %s", errno, strerror(errno));
    return;
  }


}

/*=============================================================================
 * lowi_test_exit
 *
 * Description:
 *   Cleanup the LOWI Test module
 *
 * Return value:
 *   void
 ============================================================================*/
static void lowi_test_exit(void)
{
//  iwss_queue_stop_req();
  lowi_test_display_summary_stats();

  /* Free memory */
  if (lowi_cmd != NULL)
  {
    LOWI_FREE(lowi_cmd->lci_info);
    LOWI_FREE(lowi_cmd->lcr_info);
    flush_ftmrr_info(&lowi_cmd);
    LOWI_FREE(lowi_cmd);
  }
  pthread_mutex_destroy(&lowi_test.mutex);
  pthread_cond_destroy(&lowi_test.ps_cond);
  pthread_cond_destroy(&lowi_test.rs_cond);
  if (lowi_test.out_fp != NULL)
  {
    fclose(lowi_test.out_fp);
  }
  if (lowi_test.summary_fp != NULL)
  {
    fclose(lowi_test.summary_fp);
  }
  if (lowi_test.timerFd != -1)
  {
    close(lowi_test.timerFd);
  }
}

/*=============================================================================
 * rts_cts_scan_meas_received
 *
 * Description:
 *   This is the callback function that  will be called to send RTS CTS scan
 *   results to the client.
 *   The memory pointed by p_wlan_meas is allocated by LOWI. However, the
 *   de-allocation is the responsibility of the client.
 *
 * Parameters:
 *   p_wlan_meas - Pointer to the measurement results.
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 ============================================================================*/
int rts_cts_scan_meas_received(LOWIResponse *response)
{
  int ap_cnt = 0;
  char string_buf[128];  // IPE_LOG_STRING_BUF_SZ

  if (NULL == response)
  {
    printf( "\nNULL ranging scan results obtained!\n");
    return -1;
  }
  LOWIRangingScanResponse *resp = (LOWIRangingScanResponse *)response;
  vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements; /* AAR TODO: Should use IPE_LOG for logging */

  printf( "\nRanging scan results obtained!\n");
  printf( "=== Ranging SCAN RESULTS ===\n");

  for (int bssidCnt = 0; bssidCnt < scanMeasurements.getNumOfElements(); bssidCnt++)
  {
    if (NULL != scanMeasurements[bssidCnt])
    {
      vector<LOWIMeasurementInfo *> measurements = scanMeasurements[bssidCnt]->measurementsInfo;
      LOWIChannelInfo ch = LOWIChannelInfo(scanMeasurements[bssidCnt]->frequency);
      if (measurements.getNumOfElements() > 0)
      {
        ap_cnt++;
        for (int measCnt = 0; measCnt < measurements.getNumOfElements(); measCnt++)
        {
          lowi_test_log_time_us_to_string(string_buf, 128, NULL,
                                          measurements[measCnt]->rssi_timestamp * 1000);
          printf( "[%lu] %02x:%02x:%02x:%02x:%02x:%02x %d %u %d %s\n",
                         lowi_test.seq_no,
                         scanMeasurements[bssidCnt]->bssid[0],
                         scanMeasurements[bssidCnt]->bssid[1],
                         scanMeasurements[bssidCnt]->bssid[2],
                         scanMeasurements[bssidCnt]->bssid[3],
                         scanMeasurements[bssidCnt]->bssid[4],
                         scanMeasurements[bssidCnt]->bssid[5],
                         ch.getChannel(), measurements[measCnt]->rtt_ps,
                         measurements[measCnt]->rssi,
                         string_buf);
        }
      }
    }
  }

  printf( "=== RANGING SCAN RESULTS END (%d APs Found) ===\n",
                 ap_cnt);

  pthread_mutex_lock(&lowi_test.mutex);
  lowi_test_log_meas_results(response);
  pthread_cond_signal(&lowi_test.rs_cond);
  pthread_mutex_unlock(&lowi_test.mutex);
  return 0;
}

/*=============================================================================
 * passive_scan_meas_received
 *
 * Description:
 *   This is the callback function that  will be called to send PASSIVE scan
 *   results to the client.
 *   The memory pointed by p_wlan_meas is allocated by LOWI. However, the
 *   de-allocation is the responsibility of the client.
 *
 * Parameters:
 *   p_wlan_meas - Pointer to the measurement results.
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 ============================================================================*/
int passive_scan_meas_received(LOWIResponse *response)
{
  int ap_cnt = 0;
  char string_buf[128];  // IPE_LOG_STRING_BUF_SZ
  if (NULL == response)
  {
    printf( "\nNULL passive scan results obtained!\n");
    return -1;
  }
  LOWIDiscoveryScanResponse* resp = (LOWIDiscoveryScanResponse*) response;
  vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;

  printf( "\nDiscovery scan results obtained!\n");
  printf( "=== DISCOVERY SCAN RESULTS [%u] ===\n", lowi_test.seq_no);

  for (unsigned int bssidCnt = 0;
       bssidCnt < scanMeasurements.getNumOfElements();
       bssidCnt++)
  {
    if (NULL != scanMeasurements[bssidCnt])
    {
      vector<LOWIMeasurementInfo *> measurements = scanMeasurements[bssidCnt]->measurementsInfo;
      LOWIChannelInfo ch = LOWIChannelInfo(scanMeasurements[bssidCnt]->frequency);
      if (measurements.getNumOfElements() > 0)
      {
        ap_cnt++;
        lowi_test_log_time_us_to_string(string_buf, 128, NULL,
                                        measurements[0]->rssi_timestamp * 1000);
        printf( "[%u]itp %02x:%02x:%02x:%02x:%02x:%02x %d %d %s\n",
                       lowi_test.seq_no,
                       scanMeasurements[bssidCnt]->bssid[0],
                       scanMeasurements[bssidCnt]->bssid[1],
                       scanMeasurements[bssidCnt]->bssid[2],
                       scanMeasurements[bssidCnt]->bssid[3],
                       scanMeasurements[bssidCnt]->bssid[4],
                       scanMeasurements[bssidCnt]->bssid[5],
                       ch.getChannel(), measurements[0]->rssi,
                       string_buf);
      }
    }
  }

  printf( "=== DISCOVERY SCAN RESULTS END (%d APs Found) [%u] ===\n",
                 ap_cnt, lowi_test.seq_no);

  pthread_mutex_lock(&lowi_test.mutex);
  lowi_test_log_meas_results(response);
  pthread_cond_signal(&lowi_test.ps_cond);
  pthread_mutex_unlock(&lowi_test.mutex);
  return 0;
}

/*=============================================================================
 * driver_capabilities_received
 *
 * Description:
 *   This is the callback function that is invoked to notify the capabilities
 *   of the driver.
 *
 * Parameters:
 *   lowi_wrapper_driver_capabilities - Capabilities.
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 ============================================================================*/
int driver_capabilities_received(LOWIResponse *response)
{
  printf( "\nDriver capabilities obtained!");
  LOWICapabilities* cap = (LOWICapabilities *)response;
  if (NULL != cap)
  {
    printf( "\nDriver capabilities discovery scan enabled = %d",
        cap->discoveryScanSupported);
    printf( "\nDriver capabilities ranging scan enabled = %d",
        cap->rangingScanSupported);
    printf( "\nDriver capabilities active scan supported = %d",
        cap->activeScanSupported);
  }
  return 0;
}

void log_time ()
{
  printf( "Time from Android boot = %lld", lowi_test_get_time_ms());
  printf( "Time monotonic = %lld", get_time_monotonic_ms());
  printf( "Time current time of day = %lld", get_time_rtc_ms());

}

/*=============================================================================
 * signal_handler
 *
 * Function description:
 *   Handles a signal.
 *
 * Parameters: Signal ID.
 *
 * Return value: void
 ============================================================================*/
void signal_handler(int signal_id)
{
  printf( "\nreceived signal [%d][%s], ignore",
      signal_id, strsignal(signal_id));
}

/*=============================================================================
 * main
 *
 * Function description:
 *   The entry point to this IWSS test process.
 *
 * Parameters: Number of arguments (argc) and the arguments (argv[]).
 *
 * Return value: Process exit code
 ============================================================================*/
int main(int argc, char *argv[])
{
  int ret_val;
  int (*scan_func)(const uint32) = lowi_test_do_passive_scan;

  // zero initialize the data structure
  memset (&scan_stats, 0, sizeof (scan_stats));
  memset (&lowi_test, 0, sizeof (lowi_test));

  log_time ();
  // Register signal handler
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler);
  signal(SIGILL, signal_handler);
  signal(SIGTRAP, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGIOT, signal_handler);
  signal(SIGBUS, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGPIPE, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGKILL, signal_handler);
  signal(SIGSTOP, signal_handler);
  signal(SIGTSTP, signal_handler);
  signal(SIGALRM, signal_handler);
  signal(NSIG, signal_handler);

  // Initialize lowi wrapper
  if (0 != lowi_wrapper_init (&passive_scan_meas_received,
      &rts_cts_scan_meas_received) )
  {
    // Failed to init lowi wrapper, exit
    fprintf(stderr, "Failed to init lowi_wrapper\n");
    lowi_wrapper_destroy ();
    return 0;
  }

  // Register for capabilities callback.
  lowi_wrapper_register_capabilities_cb (&driver_capabilities_received);

  // Register for async discovery scan result callback
  lowi_wrapper_register_async_resp_cb (&passive_scan_meas_received);

  lowi_test_init();

  // Request lowi wrapper for the driver capabilities. lowi_test does not use
  // the capabilities as of now but exercises the API
  lowi_queue_capabilities_req ();

  lowi_cmd = lowi_get_cmd(argc, argv);

  if (lowi_cmd != NULL)
  {
    switch (lowi_cmd->cmd)
    {
    case LOWI_ASYNC_DISCOVERY_SCAN:
      /* --------------------------------------------------------------
      ** ASYNC DISCOVERY SCAN REQUEST SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_do_async_discovery_scan;
      break;
    case LOWI_DISCOVERY_SCAN:
      /* --------------------------------------------------------------
      ** DISCOVERY SCAN REQUEST SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_do_passive_scan;
      break;
    case LOWI_BOTH_SCAN:
      /* --------------------------------------------------------------
      ** DISCOVERY SCAN FOLLOWED BY RTT Measurement SECTION
      ** ------------------------------------------------------------*/
      {
        memcpy(&rts_scan_cmd, lowi_cmd, sizeof(t_lowi_test_cmd));
        //set lci, lcr information is not relevent for rtt. So removing
        //those
        LOWI_FREE(rts_scan_cmd.lci_info);
        rts_scan_cmd.lci_info = NULL;
        LOWI_FREE(rts_scan_cmd.lcr_info);
        rts_scan_cmd.lcr_info = NULL;
        flush_ftmrr_info(&lowi_cmd);
        scan_func = lowi_test_do_combo_scan;
        break;
      }
    case LOWI_RTS_CTS_SCAN:
      /* --------------------------------------------------------------
      ** RTT Measurement SECTION
      ** ------------------------------------------------------------*/
      memcpy(&rts_scan_cmd, lowi_cmd, sizeof(t_lowi_test_cmd));
      //set lci, lcr information is not relevent for rtt. So removing
      //those
      LOWI_FREE(rts_scan_cmd.lci_info);
      rts_scan_cmd.lci_info = NULL;
      LOWI_FREE(rts_scan_cmd.lcr_info);
      rts_scan_cmd.lcr_info = NULL;
      flush_ftmrr_info(&lowi_cmd);
      if (prepareRtsCtsParam(&rts_scan_cmd, lowi_test.seq_no) == 0)
      {
        fprintf(stderr, "Incorrect AP List. Num APs = %d",
                lowi_cmd->ap_info.getNumOfElements());
        break;
      }
      scan_func = lowi_test_do_rts_cts_scan;
      break;
    case LOWI_NR_REQ:
      /* --------------------------------------------------------------
      ** NEIGHBOR REPORT SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_do_neighbor_report_request;
      break;
    case LOWI_SET_LCI:
      /* --------------------------------------------------------------
      ** Set LCI information SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_set_lci;
      break;
    case LOWI_SET_LCR:
      /* --------------------------------------------------------------
      ** Set LCR information SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_set_lcr;
      break;
    case LOWI_WRU_REQ:
      /* --------------------------------------------------------------
      ** WHERE ARE YOU REQUEST SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_where_are_you;
      break;
    case LOWI_FTMR_REQ:
      /* --------------------------------------------------------------
      ** FTMRR SECTION
      ** ------------------------------------------------------------*/
      scan_func = lowi_test_ftmrr;
      break;
    default:
      break;
    }
    LOWI_TEST_REQ_WAKE_LOCK;
    do
    {
      scan_func(lowi_test.seq_no);
      lowi_test.seq_no++;
      if (lowi_test.seq_no > lowi_cmd->num_requests)
      {
        break;
      }
      lowi_test_start_timer(lowi_cmd->delay);
      lowi_test_wait(lowi_cmd->delay);
    } while (1);
    LOWI_TEST_REL_WAKE_LOCK;
  }

  lowi_wrapper_destroy ();
  lowi_test_exit();

  return 0;
}
