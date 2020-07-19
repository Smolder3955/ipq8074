#ifndef _WLAN_LOCATION_DEFS_H
#define _WLAN_LOCATION_DEFS_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        WIPS module - WLAN location definition

GENERAL DESCRIPTION
  This file contains the definition of the WLAN location module.

Copyright (c) 2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#define NUM_OFDM_TONES_ACK_FRAME 52
#define RESOLUTION_IN_BITS       10
#define RTTM_CDUMP_SIZE(rxchain,bw40) (NUM_OFDM_TONES_ACK_FRAME * 2 * (bw40 + 1) * (rxchain) * RESOLUTION_IN_BITS) / 8
#define MAX_RTTREQ_MEAS 10
#define NUM_WLAN_BANDS 2
#define MAX_CHAINS 2
#define NUM_CLKOFFSETCAL_SAMPLES 5
#define CLKOFFSET_GPIO_PIN 26

#define NSP_MRQST_MEASTYPEMASK 0x3
#define NSP_MRQST_FRAMETYPEMASK   0x7
#define NSP_MRQST_TXCHAINMASK  0x3
#define NSP_MRQST_RXCHAINMASK  0x3
#define NSP_MRQST_REQMETHODMASK  0x3
#define NSP_MRQST_MOREREQMASK 0x1
#define NSP_MRQST_RTT2MASK 0x1

#define NSMP_MRQST_MEASTYPE(mode) (mode & NSP_MRQST_MEASTYPEMASK)
#define NSP_MRQST_FRAMETYPE(mode) ((mode>>2) & NSP_MRQST_FRAMETYPEMASK)
#define NSP_MRQST_TXCHAIN(mode) ((mode>>5) & NSP_MRQST_TXCHAINMASK)
#define NSP_MRQST_RXCHAIN(mode) ((mode>>7) & NSP_MRQST_RXCHAINMASK)
#define NSP_MRQST_REQMETHOD(mode) ((mode>>9) & NSP_MRQST_REQMETHODMASK)
#define NSP_MRQST_MOREREQ(mode) ((mode>>11) & NSP_MRQST_MOREREQMASK)
#define NSP_MRQST_RTT2(mode) ((mode>>12) & NSP_MRQST_RTT2MASK)

#define NSMP_MRQST_MEASTYPE_SET(mode,val) (mode |= (val & NSP_MRQST_MEASTYPEMASK ))
#define NSP_MRQST_FRAMETYPE_SET(mode,val) (mode |= ((val & NSP_MRQST_FRAMETYPEMASK) << 2 ))
#define NSP_MRQST_TXCHAIN_SET(mode,val) (mode |= ((val & NSP_MRQST_TXCHAINMASK) << 5 ))
#define NSP_MRQST_RXCHAIN_SET(mode,val) (mode |= ((val & NSP_MRQST_RXCHAINMASK) << 7 ))
#define NSP_MRQST_REQMETHOD_SET(mode,val) (mode |= ((val & NSP_MRQST_REQMETHODMASK) << 9 ))
#define NSP_MRQST_MOREREQ_SET(mode,val) (mode |= ((val & NSP_MRQST_MOREREQMASK) << 11))
#define NSP_MRQST_RTT2_SET(mode,val) (mode |= ((val & NSP_MRQST_RTT2MASK) << 12))

struct nsp_header {
        __u8 version;
        __u8 frame_type;
    };
#define NSP_HDR_LEN sizeof(struct nsp_header)

struct nsp_mrqst {
  __u32 sta_info;
  /* A bit field used to provide information about the STA
   * The bit fields include the station type (WP Capable/Non Capable)
   * and other fields to be defined in the future.
   * STAInfo[0]: Station Type: 0=STA is not WP Capable, 1=STA is WP Capable
   * STAInfo[31:1]: TBD */


  __u32 transmit_rate;
  /* Rate requested for transmission. If value is all zeros
   * the AP will choose its own rate. If not the AP will honor this
   * 31:0: IEEE Physical Layer Transmission Rate of the Probe frame */

  __u32 timeout;
  /* Time out for collecting all measurements. This includes the
   * time taken to send out all sounding frames and retry attempts.
   * Measured in units of milliseconds. For the Immediate Measurement mode,
   * the WLAN AP system must return a Measurement Response after the
   * lapse of an interval equal to “timeout” milliseconds after
   * reception of the Measurement Request */
  __u32 reserved;
  __u16 mode;
#define FRAME_TYPE_HASH 0x001c
#define NULL_FRAME 0
#define QOS_NULL_FRAME 4
#define RTS_CTS 8
  /* Bits 1:0: Type of measurement:
          00: RTT, 01: CIR
   * Bits 4:2: 802.11 Frame Type to use as Probe
          000: NULL, 001: Qos NULL, 010: RTS/CTS
   * Bits 6:5 Transmit chainmask to use for transmission
          01: 1, 10: 2, 11:3
   * Bits 8:7: Receive chainmask to use for reception
          01: 1, 10: 2, 11:3
   * Bits 10:9: The method by which the request should be serviced
          00 = Immediate: The request must be serviced as soon as possible
          01 = Delayed: The WPC can defer the request to when it deems appropriate
          10 = Cached: The WPC should service the request from cached results only */


  __u8 request_id;
  /* A unique ID which identifies the request. It is the responsibility
   * of the application to ensure that this is unique.
   * The library will use this Id in its response. */

  __u8 sta_mac_addr[ETH_ALEN];
  /* The MAC Address of the STA for which a measurement is requested.*/

  __u8 spoof_mac_addr[ETH_ALEN];
  /* The MAC Address which the AP SW should use as the source
   * address when sending out the sounding frames */

  __u8 channel;
  /* The channel on which the STA is currently listening.
   * The channel is specified in the notation (1-11 for 2.4 GHz and 36 – 169 for 5 GHz.
   * If a STA is in HT40 mode, then the channel will indicate the
   * control channel. Probe frames will always be sent at HT20 */

  __u8 no_of_measurements;
  /* The number of results requested i.e the WLAN AP can stop measuring
   * after it successfully completes a number of measurements equal
   * to this number. For RTT based measurement this will always = 1 */
  __u8 reserved2[3];


};
/* NSP Capability Request */
struct nsp_crqst
{
  __u8 request_id;
  /* A unique ID which identifies the request. It is the responsibility
   * of the application to ensure that this is unique.
   * The library will use this Id in its response. */

};
/*NSP Status Request*/
struct nsp_srqst
{
  __u8 request_id;
  /* A unique ID which identifies the request. It is the responsibility
   * of the application to ensure that this is unique.
   * The library will use this Id in its response. */

};

#define NSP_MRESP_RESULT_RANEGMASK 0x1
#define NSP_MRESP_RESULT_CLKMASK   0x2

struct nsp_mresphdr {
  __u8 request_id;
  /* A unique ID which identifies the request. It is the responsibility
   * of the application to ensure that this is unique.
   * The library will use this Id in its response. */
  __u8 sta_mac_addr[ETH_ALEN];
  /* The MAC Address of the STA for which a measurement is requested.*/

  __u8 response_type;
  /*Type Of Response 0:CIR 1:RTT*/

  __u16 no_of_responses;
  /* no of responses */

  __u16 result;
  /* Result Of Probe Bit-0 0:Complete  1:More Responses For this
   * Request ID*/

  __u32 begints;
  /* FW Timestamp when RTT Req Begins */

  __u32 endts;
  /* FW Timestamp when RTT Req processing completes*/

  __u32 reserved;
};

#define RTTM_CHANNEL_DUMP_LEN RTTM_CDUMP_SIZE(MAX_CHAINS,1)

struct nsp_cir_resp {

     __u32 tod;
    /* A timestamp indicating when the measurement probe was sent */

    __u32 toa;
    /* A timestamp indicating when the probe response was received */

    __u32 sendrate;
    /* IEEE Physical Layer Transmission Rate of the Send Probe frame */

    __u32 recvrate;
    /*IEEE Physical Layer Transmission Rate of the response to the
     * Probe Frame*/

    __u8 channel_dump[RTTM_CHANNEL_DUMP_LEN];
    /* channel dump for max no of chains */

    __u8 no_of_chains;
    /* Number of chains used for reception */

    __u8 rssi[MAX_CHAINS];
    /* Received signal strength indicator */

    __u8  isht40;

    __s8  fwcorr;

    __u8 reserved[3];

};

typedef struct rttresp_meas_debug
{
     __u32 rtt;
     int rtt_cal;
     __s8 corr;
     __u8 rssi0;
     __u8 rssi1;
     __u8 reserved;
     __u32 range;
   int clockoffset;
}__attribute((__packed__))rttresp_meas_debug;
typedef struct nsp_rttresp_debuginfo
{
   struct rttresp_meas_debug rttmeasdbg[MAX_RTTREQ_MEAS];
   __u8 nTotalMeas;
   __u8 wlanband;
   __u8 isht40;
   __u8 reserved1;
   __u32 reserved2;
}__attribute((__packed__))nsp_rttresp_debuginfo;


#define RTTRESP_DEBUG_SIZE sizeof(struct nsp_rttresp_debuginfo)

struct nsp_rtt_resp
{
    __u32 range_average;
    /*Result Of Probe*/

    __u32 range_stddev;
    /*Results Of Range*/

    __u32 range_min;

    __u32 range_max;

    __u32 rtt_min;

    __u32 rtt_max;

    __u32 rtt_average;
    /*The mean of the samples of RTT calculated by WML(ns)*/

    __u32 rtt_stddev;
    /*The standard deviation of the samples of RTT calculated by
     * WML(ns)*/

    __u8 rssi_average;
    /*The mean of the samples of the RSSI(dB)*/

    __u8 rssi_stddev;
    /*The Standard Deviation of the samples of the RSSI(dB)*/

    __u8 rssi_min;
    /*Min RSSI*/

    __u8 rssi_max;
    /*Max RSSI*/

    __u8 rtt_samples;
    /*The Number Of Samples used in the RTT Calculation*/

    __u8 reserved1[3];

    __u32 reserved2;

    __u32 clock_delta;

    __u8 debug[RTTRESP_DEBUG_SIZE];

};

struct nsp_rtt_config
{
    __u32 ClkCal[NUM_WLAN_BANDS];
    __u8  FFTScale;
    __u8  RangeScale;
    __u8  ClkSpeed[NUM_WLAN_BANDS];
    __u32 reserved[4];
};

struct stRttHostTS
{
    __u32 sec;
    __u32 nsec;
};

struct nsp_rttd2h2_clkoffset
{
    struct stRttHostTS tabs_h2[NUM_CLKOFFSETCAL_SAMPLES];
    __u8 numsamples;
    __u8 reserved[3];
};



struct nsp_rtt_clkoffset
{
    __u32 tdelta_d1d2[NUM_CLKOFFSETCAL_SAMPLES];
    struct stRttHostTS tabs_h1[NUM_CLKOFFSETCAL_SAMPLES];
    struct stRttHostTS tabs_h2[NUM_CLKOFFSETCAL_SAMPLES];
    __u32 tabs_d1[NUM_CLKOFFSETCAL_SAMPLES];
    __u32 tabs_d2[NUM_CLKOFFSETCAL_SAMPLES];
    __u8  numsamples;
    __u8  reserved[3];
};

#define MREQ_LEN sizeof(struct nsp_mrqst)
#define MRES_LEN sizeof(struct nsp_mresphdr)
#define CIR_RES_LEN sizeof(struct nsp_cir_resp)

enum NSP_FRAME_TYPE {
    NSP_MRQST = 1,
    NSP_CIRMRESP,
    NSP_RTTMRESP,
    NSP_SRQST,
    NSP_SRESP,
    NSP_CRQST,
    NSP_CRESP,
    NSP_WRQST,
    NSP_WRESP,
    NSP_SLRQST,
    NSP_SLRESP,
    NSP_RTTCONFIG,
};
#endif
