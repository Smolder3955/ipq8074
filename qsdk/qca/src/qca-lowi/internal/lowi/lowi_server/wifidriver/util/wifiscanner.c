/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  QuIPC Wireless Lan Scan Service Core
GENERAL DESCRIPTION
   This file contains functions which interface with WLAN drivers

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.


=============================================================================*/
#define LOG_NDEBUG 0
#include <base_util/time_routines.h>
// internal header files
#include "wifiscanner.h"
#include "innavService.h"
#include <lowi_server/lowi_log.h>
#include "lowi_time.h"

#define LOG_TAG "LOWI-Scan"

/** General coding guidelines and conventions used */
/* Data types should be prepended by a letter which clearly indicates their data type */
/*
Enum                    e_
uint8   Unsigned char   u_
int8    Signed char     b_
uint16  Unsigned short  w_
int16   Signed short    x_
uint32  Unsigned long   q_
int32   Signed long     l_
uint64  Unsigned long long      t_
int64   Signed long long        r_
FLT     Float   f_
DBL     Double  d_
Structure       z_
Pointer p_  --> i.e. Pointer to int32 should be pl_xxxx. Pointer to f should be pf_ etc..
Boolean u_ or b_
*********************/

/* Cache of wireless interfaces */
//struct wireless_iface* interface_cache = NULL;
//struct rtnl_handle      rth_struct;

#define MAX_WPA_IE_LEN 40
#define VENDOR_SPECIFIC_IE 0xdd
#define MSAP_ADVT_IND_IE 0x18 /* MSAP advertisement indicator */
#define RSN_IE 0x30

//Store the time at which RTS CTS request was issued
static uint64 rts_req_time = 0;
static const uint8 u_CiscoOUI[3] = { 0x00, 0x40, 0x96 };

// Defines and variables used by NL scan
extern int lowi_gen_nl_drv_open();
extern void lowi_gen_nl_drv_close();
extern int WipsScanUsingNL(char * results_buf_ptr,int cached, int timeout_val,
    int* pFreq, int num_of_freq);
extern int WipsSendFineTimeMeasurementReport(uint8* frameBody,
                                             uint32 frameLen,
                                             uint32 freq,
                                             uint8 sourceMac[BSSID_SIZE],
                                             uint8 selfMac[BSSID_SIZE]);
extern int WipsSendNeighborReportRequest(uint8* frameBody,
                                         uint32 frameLen,
                                         uint32 freq,
                                         uint8 sourceMac[BSSID_SIZE],
                                         uint8 selfMac[BSSID_SIZE]);
extern int WipsSendActionFrame(uint8* frameBody,
                               uint32 frameLen,
                               uint32 freq,
                               uint8 sourceMac[BSSID_SIZE],
                               uint8 selfMac[BSSID_SIZE]);

#define WLAN_CAPABILITY_PRIVACY        (1<<4)
#define ASSERT assert
quipc_wlan_meas_s_type *ptr_to_xchk = NULL;
uint32                  max_meas_age_sec_x=0;
uint64                  wipsiw_scan_req_time;

static pthread_mutex_t sWlanScanDoneEventMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sWlanScanDoneEventCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sWlanScanInProgressMutex= PTHREAD_MUTEX_INITIALIZER;

static uint32 channelToFreq (const uint32 channel)
{
  LOWI_LOG_INFO ("channelToFreq , Channel = %d",
      channel);
  switch (channel)
  {
  /*2.4 GHz Band*/
  case 1:  return 2412;
  case 2:  return 2417;
  case 3:  return 2422;
  case 4:  return 2427;
  case 5:  return 2432;
  case 6:  return 2437;
  case 7:  return 2442;
  case 8:  return 2447;
  case 9:  return 2452;
  case 10: return 2457;
  case 11: return 2462;
  case 12: return 2467;
  case 13: return 2472;
  case 14: return 2484;
  /*5 GHz Band*/
  case 34:  return 5170;
  case 36:  return 5180;
  case 38:  return 5190;
  case 40:  return 5200;
  case 42:  return 5210;
  case 44:  return 5220;
  case 46:  return 5230;
  case 48:  return 5240;
  case 52:  return 5260;
  case 56:  return 5280;
  case 60:  return 5300;
  case 64:  return 5320;
  case 100: return 5500;
  case 104: return 5520;
  case 108: return 5540;
  case 112: return 5560;
  case 116: return 5580;
  case 120: return 5600;
  case 124: return 5620;
  case 128: return 5640;
  case 132: return 5660;
  case 136: return 5680;
  case 140: return 5700;
  case 149: return 5745;
  case 153: return 5765;
  case 157: return 5785;
  case 161: return 5805;
  case 165: return 5825;
  /** Other than 2.4 / 5 Ghz band */
  default:
    LOWI_LOG_ERROR ("channelToFreq , Invalid Channel");
    return 0;
  }
}

/*=============================================================================================
 * Function description:
 * Report RTS CTS measurement to IWMM.
 *
 * Parameters:
 *    pAppMeasurementRsp
 *    pRslts_out
 *    buf_out
 *
 * Return value:
 *    pointer to parsed scan result. note the memory is allocated with QUIPC_MALLOC
 =============================================================================================*/

const quipc_wlan_meas_s_type* lowi_report_rts_cts_meas
(
  tInNavMeasRspParams* pAppMeasurementRsp,
  tRttRssiResults* pRslts_out,
  unsigned char* buf_out
)
{
  unsigned int t0 = 0;
  unsigned int tf = 0;
  int meas_size =0;
  uint16 ap_cnt = 0;
  int bssidCnt = 0;
  unsigned int dataCnt = 0;
  uint16  numMeas = 0;
  quipc_wlan_meas_s_type*         p_wlan_meas = NULL;
  quipc_wlan_ap_meas_s_type*      p_ap_meas   = NULL;
  quipc_wlan_ap_meas_info_s_type* p_ap_meas_info = NULL;
  uint64 resp_time_usec;
  uint32 rts_duration;
  int ret_val;

  // present time: seconds, and microseconds
  resp_time_usec = get_time_rtc_ms();
  rts_duration = (uint32)(lowi_get_time_from_boot() - rts_req_time);

  // Convert to usec
  resp_time_usec *= 1000;

  //Compute the number of BSSID and number of measurement
  numMeas = 0;
  meas_size = sizeof(quipc_wlan_meas_s_type);

  for(bssidCnt=0; bssidCnt < pAppMeasurementRsp->numBSSIDs; bssidCnt++)
  {
    LOWI_LOG_VERB ("%s, No of measurements for  ap[%d] : %u\n", __func__, bssidCnt, pRslts_out->numSuccessfulMeasurements);
    if( pRslts_out->numSuccessfulMeasurements < 1 )
    {
      LOWI_LOG_INFO ("%s, skip ap[%d] with 0 measurement\n", __func__, bssidCnt);
      pRslts_out++;
      buf_out = (unsigned char*) pRslts_out;
      continue;
    }

    ap_cnt++;
    meas_size += sizeof(quipc_wlan_ap_meas_s_type);
    meas_size += pRslts_out->numSuccessfulMeasurements  * sizeof(quipc_wlan_ap_meas_info_s_type);
    numMeas += pRslts_out->numSuccessfulMeasurements;
    buf_out += sizeof(tRttRssiResults)
              + sizeof(tRttRssiTimeData) * (pRslts_out->numSuccessfulMeasurements-1);
    pRslts_out = (tRttRssiResults*)(buf_out);
  }

  LOWI_LOG_INFO("************************************************************************************************************\n");
  LOWI_LOG_INFO("INNAV:RTS Record Start: NUM:BSSIDS:%2d MEAS:%d TAG:%u", pAppMeasurementRsp->numBSSIDs, numMeas, pAppMeasurementRsp->rtsctsTag);
  LOWI_LOG_INFO("************************************************************************************************************\n");

  //Reset pRsltsOut and buf out
  bssidCnt=0;
  pRslts_out = &pAppMeasurementRsp->rttRssiResults[0];
  buf_out = (unsigned char*)(pRslts_out);

  if (meas_size > 0)
  {
    p_wlan_meas = (quipc_wlan_meas_s_type * )QUIPC_MALLOC(meas_size);
  }
  if(p_wlan_meas == NULL)
  {
    LOWI_LOG_ERROR("IWSS: MALLOC error %s at line %d", __func__, __LINE__);
    return NULL;
  }
  memset (p_wlan_meas, 0, meas_size);

  // Initialize the wifi measurement
  p_wlan_meas->w_valid_mask = (QUIPC_WLAN_MEAS_VALID_SCAN_STATUS |
                               QUIPC_WLAN_MEAS_VALID_SCAN_TYPE |
                               QUIPC_WLAN_MEAS_VALID_NUM_APS |
                               QUIPC_WLAN_MEAS_VALID_AP_MEAS);

  p_wlan_meas->e_scan_status = QUIPC_WLAN_SCAN_STATUS_SUCCEEDED;
  p_wlan_meas->e_scan_type   = QUIPC_WLAN_SCAN_TYPE_RTS_CTS;
  p_wlan_meas->w_num_aps = ap_cnt ;
  p_wlan_meas->p_ap_meas = NULL;

  /* get the measurement tag */
  p_wlan_meas->rtsctsTag = pAppMeasurementRsp->rtsctsTag;

  LOWI_LOG_INFO("INNAV RTS/CTS rsp time %u msec at wifiscanner, rtsctsTag: %u\n",rts_duration, p_wlan_meas->rtsctsTag);

  if(p_wlan_meas->w_num_aps != 0)
  {
    p_wlan_meas->p_ap_meas = (quipc_wlan_ap_meas_s_type*)((char*) p_wlan_meas + sizeof (quipc_wlan_meas_s_type));

    // Initialize the first ap meas ptr
    p_ap_meas = p_wlan_meas->p_ap_meas;

    // Initialzie the first measurement ptr for the first ap
    p_ap_meas_info = (quipc_wlan_ap_meas_info_s_type*)
                     ((char*) p_wlan_meas +  sizeof (quipc_wlan_meas_s_type) + p_wlan_meas->w_num_aps * sizeof (quipc_wlan_ap_meas_s_type));

    for (bssidCnt=0; bssidCnt < pAppMeasurementRsp->numBSSIDs; bssidCnt++)
    {
      if (pRslts_out->numSuccessfulMeasurements < 1)
      {
        LOWI_LOG_INFO("Skipping AP[%u], because it has no measurements\n", bssidCnt);
        pRslts_out++;
        buf_out = (unsigned char*)pRslts_out;
        continue;
      }

      LOWI_LOG_INFO("************************************************************************************************************\n");
      LOWI_LOG_INFO("INNAV:RTS Record AP Start: BSSID[%02d] MAC:%02x:%02x:%02x:%02x:%02x:%02x NUM:MEAS:%04d CHAN:%04d\n",
              bssidCnt,
              pRslts_out->bssid[0],
              pRslts_out->bssid[1],
              pRslts_out->bssid[2],
              pRslts_out->bssid[3],
              pRslts_out->bssid[4],
              pRslts_out->bssid[5],
              pRslts_out->numSuccessfulMeasurements,
              pRslts_out->channel);

      memcpy (p_ap_meas->u_MacId, pRslts_out->bssid, WIFI_MAC_ID_SIZE);

      p_ap_meas->u_numMeas = (uint16) pRslts_out->numSuccessfulMeasurements;
      p_ap_meas->u_Chan = pRslts_out->channel;
      p_ap_meas->u_Freq = channelToFreq (pRslts_out->channel);
      p_ap_meas->p_meas_info = p_ap_meas_info;

      if(bssidCnt == 0)
      {
        t0 = pRslts_out->rttRssiTimeData[0].measurementTime;
      }

      if(bssidCnt == (pAppMeasurementRsp->numBSSIDs - 1))
      {
        tf = pRslts_out->rttRssiTimeData[pRslts_out->numSuccessfulMeasurements-1].measurementTime;
      }

      //Now get meas for this AP
      for(dataCnt=0; dataCnt < pRslts_out->numSuccessfulMeasurements; dataCnt++)
      {
        /* Convert RTT RSSI unit to 0.5dBm unit,
        ** LOWI_RSSI_05DBM_UNITS converts unit into 0.5 dBm */
        p_ap_meas_info->x_RSSI_0p5dBm = LOWI_RSSI_05DBM_UNITS(pRslts_out->rttRssiTimeData[dataCnt].rssi);

        p_ap_meas_info->t_measTime_usec = resp_time_usec; //Time in micro seconds since epoch

        p_ap_meas_info->w_RTT_nsec = (uint16)pRslts_out->rttRssiTimeData[dataCnt].rtt;

        LOWI_LOG_INFO("INNAV:RTS Record AP Meas: MAC_ID:%02x:%02x:%02x:%02x:%02x:%02x DATA[%u] RSSI:%u RTT:%u SNR:%u TIME-LO:%lu TIME-HI:%lu, RSSI:%d\n",
                pRslts_out->bssid[0],
                pRslts_out->bssid[1],
                pRslts_out->bssid[2],
                pRslts_out->bssid[3],
                pRslts_out->bssid[4],
                pRslts_out->bssid[5],
                dataCnt,
                pRslts_out->rttRssiTimeData[dataCnt].rssi,
                pRslts_out->rttRssiTimeData[dataCnt].rtt,
                pRslts_out->rttRssiTimeData[dataCnt].snr,
                pRslts_out->rttRssiTimeData[dataCnt].measurementTime,
                pRslts_out->rttRssiTimeData[dataCnt].measurementTimeHi,
                p_ap_meas_info->x_RSSI_0p5dBm);

        // advance the pointer to meas info
        p_ap_meas_info++;
      }

      LOWI_LOG_INFO("INNAV:RTS Record AP Stop: TOTAL-TIME:%lu\n",
              pRslts_out->rttRssiTimeData[pRslts_out->numSuccessfulMeasurements-1].measurementTime -
              pRslts_out->rttRssiTimeData[0].measurementTime);


      buf_out += sizeof(tRttRssiResults)
                + sizeof(tRttRssiTimeData) * (pRslts_out->numSuccessfulMeasurements-1);

      pRslts_out = (tRttRssiResults*)(buf_out);

      // advance to next ap
      p_ap_meas++;

    }

    LOWI_LOG_INFO("INNAV:RTS Record Stop: TOTAL-MEAS-TIME:%u TF:%u T0:%u\n", tf-t0, tf, t0);
    LOWI_LOG_INFO("************************************************************************************************************\n");
  }

  LOWI_LOG_INFO("IWSS: returning RTS_CTS_SCAN_MEAS %d APs", p_wlan_meas->w_num_aps);

  // p_wlan_meas is *NOT* supposed to be FREED here
  return p_wlan_meas;
}


/*=============================================================================================
 * Function description:
 *   Function to Initialize the WiFi Discovery Scan Driver.
 *   This function simply calls lowi_gen_nl_drv_open
 *
 * Parameters:
 *   Frame Call back Function
 *
 * Return value:
 *    none
 =============================================================================================*/
void lowi_scan_drv_open()
{
  lowi_gen_nl_drv_open();
}

/*=============================================================================================
 * Function description:
 *   Function to release memory block allocated for scan measurement.
 *   This function simply calls QUIPC_FREE
 *
 * Parameters:
 *   ptr_mem pointer
 *
 * Return value:
 *    none
 =============================================================================================*/
void lowi_free_meas_result (quipc_wlan_meas_s_type * const ptr_mem)
{
  // the macro QUIPC_FREE would set the pointer to NULL...
  quipc_wlan_meas_s_type * ptr_non_const_mem = (quipc_wlan_meas_s_type *)ptr_mem;
  QUIPC_FREE(ptr_non_const_mem);
}

/*=============================================================================================
 * Function description:
 *   Function to insert the next measurement record into the passive scan result data
 *   structure
 *
 * Parameters:
 *   results_buf_ptr: pointer to the passive scan result
 *   bss_rssi_0p5dBm: Rssi - In unit of 0.5 dBm
 *   bss_age_msec: Age - In units of 1 milli-second, -1 means info not available
 *   bss_ssid_ptr: pointer to the AP SSID memory
 *   p_ap_scan_res: pointer to the AP data (except SSID and MAC Addr)
 *
 * Return value:
 *    None
 =============================================================================================*/
void iwss_bss_insert_record
(
  void* results_buf_ptr,
  int16  bss_rssi_0p5dBm,
  int32  bss_age_msec,
  quipc_wlan_ap_meas_s_type* p_ap_scan_res
)
{
  quipc_wlan_ap_meas_s_type       *p_ap_meas = NULL,
                                  *p_ap_meas_start = NULL;
  quipc_wlan_ap_meas_info_s_type  *p_ap_meas_info = NULL,
                                  *p_ap_meas_info_start = NULL;
  uint64 meas_time_usec;
  int32 meas_time_delta = 0;
  //results_buf_ptr is the buffer allocated by iwss_proc_iwmm_req_passive_scan
  //until tested completely, the ptr to that buf is also stored in ptr_to_xchck
  //so do an assertion to ensure that callbacks are working correctly.
  quipc_wlan_meas_s_type *p_wlan_meas =
                  (quipc_wlan_meas_s_type *)results_buf_ptr;
  ASSERT(p_wlan_meas == ptr_to_xchk);


  if (max_meas_age_sec_x > 0)
  {
    /* Compare in msec unit. Convert each variable into msec */
    /* Make sure that the measurement is found after the scan request OR
    ** within reasonable age in the past from the scan request time */
    uint64 meas_time = lowi_get_time_from_boot() - bss_age_msec;

    /* Compute meas time delta with respect to scan request time */
    if (meas_time > wipsiw_scan_req_time)
    {
      meas_time_delta = meas_time - wipsiw_scan_req_time;
    }
    else
    {
      meas_time_delta = wipsiw_scan_req_time - meas_time;
      meas_time_delta *= -1;
    }

    LOWI_LOG_VERB ("IWSS: Time to filter Check AP %d age %lu msec, meas_age %d msecs (delta %ld ms)\n",
                   p_wlan_meas->w_num_aps,(unsigned long)bss_age_msec, (max_meas_age_sec_x*1000),
                   meas_time_delta);

    if ((meas_time + (max_meas_age_sec_x*1000)) < wipsiw_scan_req_time)
    {
      LOWI_LOG_DBG ("IWSS: Filtered out AP %d age %lu msec older than %d msecs (delta %ld ms)\n",
                     p_wlan_meas->w_num_aps,(unsigned long)bss_age_msec, (max_meas_age_sec_x*1000),
                     meas_time_delta);
      return;
    }
  }

  if (p_wlan_meas->w_num_aps == NUM_MAX_BSSIDS )
  {
     LOWI_LOG_VERB ("IWSS: exceeds maxium measurement of NUM_MAX_BSSIDS, disgard\n");
     return;
  }

  //Find the memory where info for next AP will be stored and store these
  //parameters.
  // Initialize the first ap meas ptr
  p_ap_meas_start  = p_wlan_meas->p_ap_meas;


  // Initialzie the first measurement ptr for the first ap
  p_ap_meas_info_start = (quipc_wlan_ap_meas_info_s_type*)
                            ((char*) p_wlan_meas +
                             sizeof (quipc_wlan_meas_s_type) +
                             NUM_MAX_BSSIDS * sizeof (quipc_wlan_ap_meas_s_type));
  p_ap_meas = p_ap_meas_start + (p_wlan_meas->w_num_aps);
  p_ap_meas_info = p_ap_meas_info_start + (p_wlan_meas->w_num_aps);

  p_ap_meas->p_meas_info = p_ap_meas_info;
  memcpy (p_ap_meas->u_MacId,
          p_ap_scan_res->u_MacId, WIFI_MAC_ID_SIZE);
  p_ap_meas->u_numMeas = 1;

  /* Store Associated or not Assocated information */
  p_ap_meas->u_AssociatedToAp = p_ap_scan_res->u_AssociatedToAp;
  LOWI_LOG_DBG("%s BSSID :" QUIPC_MACADDR_FMT " - u_AssociatedToAp: %u",
                 __FUNCTION__,
                 QUIPC_MACADDR(p_ap_meas->u_MacId),
                 p_ap_meas->u_AssociatedToAp);

  /* Grab the current time */
  meas_time_usec = (get_time_rtc_ms() - bss_age_msec) * 1000;
  p_ap_meas_info->t_measTime_usec = meas_time_usec;
  p_ap_meas_info->x_RSSI_0p5dBm = bss_rssi_0p5dBm;
  p_ap_meas_info->l_measAge_msec = bss_age_msec;
  p_ap_meas_info->w_RTT_nsec = 0;
  p_ap_meas->u_Chan = p_ap_scan_res->u_Chan;
  p_ap_meas->u_Freq = p_ap_scan_res->u_Freq;
  p_ap_meas->u_Band_center_freq1 = p_ap_scan_res->u_Band_center_freq1;
  p_ap_meas->u_Band_center_freq2 = p_ap_scan_res->u_Band_center_freq2;
  p_ap_meas->u_Ranging_features_supported = p_ap_scan_res->u_Ranging_features_supported;
  p_ap_meas->u_location_features_supported = p_ap_scan_res->u_location_features_supported;
  p_ap_meas->u_info = p_ap_scan_res->u_info;
  p_ap_meas->u_tsfDelta = p_ap_scan_res->u_tsfDelta;
  p_ap_meas->is_secure = p_ap_scan_res->is_secure;
  p_ap_meas->is_msap = p_ap_scan_res->is_msap;
  p_ap_meas->msap_prot_ver = p_ap_scan_res->msap_prot_ver;
  p_ap_meas->svr_idx = p_ap_scan_res->svr_idx;
  p_ap_meas->venue_hash = p_ap_scan_res->venue_hash;
  p_ap_meas->cell_power_limit_dBm = p_ap_scan_res->cell_power_limit_dBm;
  p_ap_meas->tsf = p_ap_scan_res->tsf;
  if (NULL != p_ap_scan_res->u_SSID)
  {
    p_ap_meas->u_lenSSID = p_ap_scan_res->u_lenSSID;
    memcpy(p_ap_meas->u_SSID,p_ap_scan_res->u_SSID,p_ap_meas->u_lenSSID);
  }
  else
  {
    memset(p_ap_meas->u_SSID, 0, sizeof (p_ap_meas->u_lenSSID));
    p_ap_meas->u_lenSSID = 0;
  }
  p_ap_meas->indoor_outdoor = p_ap_scan_res->indoor_outdoor;
  memcpy(p_ap_meas->country_code,p_ap_scan_res->country_code,COUNTRY_CODE_SIZE);

  p_wlan_meas->w_num_aps += 1; // One more AP has been received.
}

/*=============================================================================================
 * Function description:
 *   Function to finalize packaging the passive scan result using NL driver.
 *
 * Parameters:
 *   results_buf_ptr: pointer to the passive scan result
 *
 * Return value:
 *    None
 =============================================================================================*/
void iwss_bss_close_record(void * results_buf_ptr)
{
  quipc_wlan_meas_s_type *p_wlan_meas =
                  (quipc_wlan_meas_s_type *)results_buf_ptr;

  //Handle the scenario, where the record has been closed already.
  if (NULL == ptr_to_xchk)
  {
    LOWI_LOG_ERROR("Closed record already");
    return ;
  }
  ASSERT(p_wlan_meas == ptr_to_xchk);
  if (p_wlan_meas->w_num_aps > 0)
    p_wlan_meas->e_scan_status = QUIPC_WLAN_SCAN_STATUS_SUCCEEDED;

  // Output wifi scan record as before
  LOWI_LOG_INFO ("********* Reporting %d PASSIVE SCAN Results *********", p_wlan_meas->w_num_aps);
  int ap_index = 0;
  quipc_wlan_ap_meas_s_type *p_ap_meas = NULL;
  for (ap_index = 0; ap_index < p_wlan_meas->w_num_aps; ap_index++)
  {
     p_ap_meas =  p_wlan_meas->p_ap_meas + ap_index;
     LOWI_LOG_INFO ("%s, BSSID[%d]: MacId: %x:%x:%x:%x:%x:%x, RSSI = %d, age = %d ms, channel = %d, Freq = %d",
                    __func__, ap_index,
                    p_ap_meas->u_MacId[0],
                    p_ap_meas->u_MacId[1],
                    p_ap_meas->u_MacId[2],
                    p_ap_meas->u_MacId[3],
                    p_ap_meas->u_MacId[4],
                    p_ap_meas->u_MacId[5],
                    (int32) p_ap_meas->p_meas_info->x_RSSI_0p5dBm,
                    p_ap_meas->p_meas_info->l_measAge_msec,
                    (uint32) p_ap_meas->u_Chan,
                    (uint32) p_ap_meas->u_Freq);
  }

  pthread_mutex_lock(&sWlanScanDoneEventMutex);
  pthread_cond_signal(&sWlanScanDoneEventCond);
  ptr_to_xchk = NULL; //Set this pointer to NULL - so that the caller
                      // knows that the function is done!!
  pthread_mutex_unlock(&sWlanScanDoneEventMutex);
  LOWI_LOG_DBG("Closing Record\n");
}

void iwss_bss_reset_records(void * results_buf_ptr)
{
  quipc_wlan_meas_s_type *p_wlan_meas =
                  (quipc_wlan_meas_s_type *)results_buf_ptr;

  //Handle the scenario, where the record has been closed already.
  if (NULL == ptr_to_xchk)
  {
    LOWI_LOG_ERROR("Closed record already");
    return ;
  }
  ASSERT(p_wlan_meas == ptr_to_xchk);
  p_wlan_meas->w_num_aps = 0;
}

/*=============================================================================================
 * Function description:
 *   Function with processes IWMM passive scan request with fresh WIFI passive scan measurement
 *   with NL driver.
 *
 * Parameters:
 *   cached: indidates whether to request passive scan or fresh scan
 *   max_meas_age_sec: max age of the measurement for cached scan to be returned
 *   timeout_val: Time out value
 *   pRetVal: Pointer to the ret value
 *   pFreq: pointer to the frequency's that needs to be scanned.
 *   num_of_freq: Number of frequency's that needs to be scanned.
 *
 * Return value:
 *    Non-NULL : pointer to parsed scan result. note the memory is allocated with QUIPC_MALLOC
 *    NULL : failure
 =============================================================================================*/
static const quipc_wlan_meas_s_type * iwss_proc_iwmm_req_passive_scan_nl (
                                                const int cached,
                                                const uint32 max_meas_age_sec,
                                                int timeout_val,
                                                int* pRetVal,
                                                int* pFreq,
                                                int num_of_freq)
{
  int              meas_size;
  int err_code;
  uint64 cur_time;
  quipc_wlan_meas_s_type          *p_wlan_meas = NULL;


  // Note that this function is NOT reentracy safe.
  // If the same caller calls this function twice, then this will
  // run into multiple issues.
  // Waiting for a Mutex may seem too bad at this point - but in
  // reality, it comes at no performance penalty. When this Mutex
  // is unlocked, the WLAN scan would have been done and the resulting
  // results can be almost immediately obtained..(assuming that the
  // Caller is calling this function with "cached" parameter set to
  // true.
  // If the caller is calling this function with "Cached" set to FALSE,
  // anyway, this scan will wait in the queue of WLAN drivers. So waiting
  // here is no worse than waiting the queue of WLAN drivers.
  // We should however question a design, where one process is calling
  // two Scans to happen in Non-Cached mode.
  pthread_mutex_lock(&sWlanScanInProgressMutex);


  /* This function exactly does what it's Non-NL counterpart does */
  /* But with some differences..                                  */
  /* With NL Interface, the # of APs found in the scan is not known    */
  /* before hand. So - untlike it's non-NL counterpart function, this  */
  /* function allocates memory for maximum BSSID and the dump callback */
  /* of NL Scan dump function just keeps packing results in this buffer*/
  /* until done. Results are then sent to the other layers in the same */
  /* buffer. Which means that there is most likely always lots of junk */
  /* data sitting in the buffer at the tail end.                       */

  meas_size = sizeof (quipc_wlan_meas_s_type) +
              sizeof (quipc_wlan_ap_meas_s_type) * NUM_MAX_BSSIDS +
              sizeof (quipc_wlan_ap_meas_info_s_type) * NUM_MAX_BSSIDS;

  max_meas_age_sec_x = max_meas_age_sec;
  p_wlan_meas = (quipc_wlan_meas_s_type*)QUIPC_MALLOC(meas_size);
  if(p_wlan_meas == NULL)
  {
    LOWI_LOG_ERROR(
       "PASSIVE SCAN::: %s, error allocating memory at line %d\n",
       __func__, __LINE__);
    return NULL;
  }

  // Initialize to all zeros!!
  memset (p_wlan_meas, 0, meas_size);
  ptr_to_xchk = p_wlan_meas;

  // Initialize the wifi measurement
  p_wlan_meas->w_valid_mask = (QUIPC_WLAN_MEAS_VALID_SCAN_STATUS |
                               QUIPC_WLAN_MEAS_VALID_SCAN_TYPE |
                               QUIPC_WLAN_MEAS_VALID_NUM_APS |
                               QUIPC_WLAN_MEAS_VALID_AP_MEAS);
  p_wlan_meas->e_scan_type  = QUIPC_WLAN_SCAN_TYPE_PASSIVE;
  p_wlan_meas->w_num_aps = 0; //At this time, no AP has been recieved.


  /* Grab the time at which the passive scan results are received */
  cur_time = get_time_rtc_ms() * 1000;
  p_wlan_meas->p_ap_meas = (quipc_wlan_ap_meas_s_type*)((char*) p_wlan_meas +
                            sizeof (quipc_wlan_meas_s_type));
  err_code = WipsScanUsingNL((char *)p_wlan_meas,cached, timeout_val, pFreq, num_of_freq);
  *pRetVal = err_code;

  if (err_code >= 0)
  {
    // Not a enless loop. ptr_to_xchk is global variable,
    // updates in a callback.
    while (ptr_to_xchk != NULL)
    {
      pthread_mutex_lock(&sWlanScanDoneEventMutex);
      pthread_cond_wait(&sWlanScanDoneEventCond, &sWlanScanDoneEventMutex);
      pthread_mutex_unlock(&sWlanScanDoneEventMutex);
    }
  }
  pthread_mutex_unlock(&sWlanScanInProgressMutex);
  return p_wlan_meas;
}

const quipc_wlan_meas_s_type * lowi_proc_iwmm_req_passive_scan_with_live_meas (const int cached,
                                                                               const uint32 max_scan_age_sec,
                                                                               const uint32 max_meas_age_sec,
                                                                               int timeout,
                                                                               int* pRetVal,
                                                                               int* pFreq,
                                                                               int num_of_freq,
                                                                               int* frameArrived)
{
  int result = 1;

  const uint64 max_scan_age_usec = (uint64)max_scan_age_sec * 1000000;
  quipc_wlan_meas_s_type * ptr_measurement = NULL;

  /* Clear all Frame Storage before we begin */
  memset(&wlanFrameStore, 0, sizeof(wlanFrameStore));
  *frameArrived = 0;

  ptr_measurement = (quipc_wlan_meas_s_type *)iwss_proc_iwmm_req_passive_scan_nl(cached, max_meas_age_sec, timeout, pRetVal, pFreq, num_of_freq);

  if(NULL == ptr_measurement)
  {
    result = 3;
  }
  else
  {
    result = 0;
  }

  do
  {
    unsigned int i;
    // system time is a dangerous concept to use, but I have to follow
    // what is defined in the wifiscanner interface, for now
    uint64 pseudo_system_time_usec = get_time_rtc_ms() * 1000;
    boolean lowest_age_valid = FALSE;
    uint64 lowest_age_usec = 0;
    boolean scan_again = FALSE;

    if (cached == 0)
    {
      /* NO Worry about caching */
      break;
    }
    if(NULL == ptr_measurement)
    {
      result = 2;
      break;
    }

    for(i = 0; i < ptr_measurement->w_num_aps; ++i)
    {
      const quipc_wlan_ap_meas_s_type * const ptr_ap = ptr_measurement->p_ap_meas + i;

      if (0 <= ptr_ap->p_meas_info->l_measAge_msec)
      {
        uint64 age_usec = ptr_ap->p_meas_info->l_measAge_msec * 1000;
        LOWI_LOG_VERB("Cached scan result, age (%llu)\n", age_usec);

        if(FALSE == lowest_age_valid)
        {
          lowest_age_valid = TRUE;
          lowest_age_usec = age_usec;
        }
        else if (age_usec < lowest_age_usec)
        {
          lowest_age_usec = age_usec;
          LOWI_LOG_VERB ("Cached scan result, reset lowest age (%llu)\n", age_usec);
        }
        else
        {
          // age is valid and larger than lowest_age_usec, skip
        }
      }
      else if(-1 == ptr_ap->p_meas_info->l_measAge_msec)
      {
        // skip the AP whose age is not present
      }
      else
      {
         LOWI_LOG_ERROR("Cached scan result, AP has invalid age (%d)\n", ptr_ap->p_meas_info->l_measAge_msec);
      }
    }

    if(TRUE == lowest_age_valid)
    {
      if(lowest_age_usec < max_scan_age_usec)
      {
        // the cached scan result is fresh enough, go
        scan_again = FALSE;
        LOWI_LOG_DBG("Cached scan result is still fresh (%llu)\n", lowest_age_usec);
      }
      else
      {
        // the cached scan result is stale, rescan
        scan_again = TRUE;
        LOWI_LOG_DBG("Cached scan result is stale (%llu)\n", lowest_age_usec);
      }
    }
    else
    {
      // age is not valid. trigger another scan
      scan_again = TRUE;
      LOWI_LOG_INFO("Cached scan result doesn't have valid timestamp\n");
    }

    if(TRUE == scan_again)
    {
      if ( (NULL == pFreq) && (0 == num_of_freq) )
      {
        // Fall back to a fresh scan is not required
        LOWI_LOG_INFO ("No frequency specified. Can not trigger fresh scan,"
            " cached scan failed");
      }
      else
      {
        QUIPC_FREE(ptr_measurement);

        // scan without cached result
        LOWI_LOG_INFO("Scanning  again with timeout of %d\n", timeout);
        ptr_measurement = (quipc_wlan_meas_s_type *)
                             iwss_proc_iwmm_req_passive_scan_nl(0, max_meas_age_sec, timeout, pRetVal, pFreq, num_of_freq);

        if(NULL == ptr_measurement)
        {
          result = 3;
          break;
        }
      }
    }

    result = 0;
  } while(0);

  if(0 != result)
  {
     LOWI_LOG_ERROR("Scan failed: %d\n", result);
  }

  if (wlanFrameStore.numFrames)
  {
    LOWI_LOG_DBG("%s - WLAN Frames to be parsed\n", __FUNCTION__);
    *frameArrived = 1;
  }
  else
  {
    LOWI_LOG_DBG("%s - No WLAN Frames to be parsed\n", __FUNCTION__);
  }

  return ptr_measurement;
}

int lowi_send_action_frame(uint8* frameBody,
                           uint32 frameLen,
                           uint32 freq,
                           uint8 sourceMac[BSSID_SIZE],
                           uint8 selfMac[BSSID_SIZE])
{
  return WipsSendActionFrame(frameBody, frameLen, freq, sourceMac, selfMac);
}


