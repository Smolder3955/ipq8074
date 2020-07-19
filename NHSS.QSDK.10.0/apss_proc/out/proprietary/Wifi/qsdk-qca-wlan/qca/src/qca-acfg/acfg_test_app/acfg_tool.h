/*
 * Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary
*/

#ifndef __ACFG_TOOL_H__
#define __ACFG_TOOL_H__

#include<stdio.h>
#include<stdlib.h>

#include<qdf_types.h>


/*
 * Debugging
 */
#ifdef ACFG_TRACE
#define dbglog(args...) do{ extern char *appname ;              \
    fprintf(stdout, "%s: ",appname);    \
    fprintf(stdout, args);              \
    fprintf(stdout,"\n"); }while(0)

#define dbg_print_params(fmt , args...)         \
    do{                                         \
        char *str;                          \
        \
        if(asprintf(&str, fmt, args))       \
        {                                   \
            dbglog(str);                    \
            free(str);                      \
        }                                   \
    }while(0)

#else
#define dbglog(args...)
#define dbg_print_params(fmt , args...)
#endif //ACFG_TRACE




/* -----------------------
 *  Type Declarations
 * -----------------------
 */

/**
 * @brief Type of wrapper function.
 *        We define one wrapper for each acfg lib api
 *        that we intend to test. The wrapper converts command line
 *        parameters from their string representation to the data types
 *        accepted by the acfg lib api and executes a call to the api.
 *
 * @param
 *
 * @return
 */
typedef int (*api_wrapper_t)(char *params[]) ;



/**
 * @brief Type of parameter
 */
typedef enum param_type {
    PARAM_UINT8 = 0,
    PARAM_UINT16,
    PARAM_UINT32,
    PARAM_SINT8,
    PARAM_SINT16,
    PARAM_SINT32,
    PARAM_STRING,
} param_type_t ;



/**
 * @brief Details about one parameter
 */
typedef struct param_info {
    char *name;
    param_type_t type ;
    char *desc ;
} param_info_t ;


/**
 * @brief Table of function pointers. This helps us to execute the correct
 *        wrapper for each acfg lib api. The 'num_param' field determines the
 *        number of commnd line arguments needed for each api.
 */
typedef struct fntbl{
    char *apiname ;
    api_wrapper_t wrapper ;
    int num_param ;
    param_info_t *param_info ;
} fntbl_t ;





/* ---------------------------
 *      Utility Functions
 * ---------------------------
 */


#define msg(args...)   do{                              \
    extern char *appname ;      \
    fprintf(stdout, "%s: ",appname); \
    fprintf(stdout, args);      \
    fprintf(stdout, "\n");      \
}while(0)

#define msgstatus(status)   msg("acfg lib status - %d",status)

/**
 * @brief   Convert a string representation of a
 *          number to unsigned 32 bit
 *
 * @param str
 * @param val
 *
 * @return
 */
int get_uint32(char *str, uint32_t *val) ;

/**
 * @brief   Convert a hex string representation of a
 *          number to unsigned 32 bit
 *
 * @param str
 * @param val
 *
 * @return
 */

int get_int32(char *str, int32_t *val) ;

/**
 * @brief   Convert a hex string representation of a
 *          number to signed 32 bit
 *
 * @param str
 * @param val
 *
 * @return
 */

int get_hex(char *str, uint32_t *val) ;

/**
 * @brief Convert acfg lib status to OS specific
 *        status.
 *
 * @param status
 *
 * @return
 */
static
inline int acfg_to_os_status(uint32_t status)
{
    if(status != QDF_STATUS_SUCCESS)
        return -1;
    else
        return 0;
}
#define ACFG_EVENT_LOG_FILE "/etc/acfg_event_log"
#define IEEE80211_AID(b)    ((b) &~ 0xc000)

/* capability info, copied from wlan_modules/include/ieee80211.h */
#define IEEE80211_CAPINFO_ESS               0x0001
#define IEEE80211_CAPINFO_IBSS              0x0002
#define IEEE80211_CAPINFO_CF_POLLABLE       0x0004
#define IEEE80211_CAPINFO_CF_POLLREQ        0x0008
#define IEEE80211_CAPINFO_PRIVACY           0x0010
#define IEEE80211_CAPINFO_SHORT_PREAMBLE    0x0020
#define IEEE80211_CAPINFO_PBCC              0x0040
#define IEEE80211_CAPINFO_CHNL_AGILITY      0x0080
#define IEEE80211_CAPINFO_SPECTRUM_MGMT     0x0100
#define IEEE80211_CAPINFO_QOS               0x0200
#define IEEE80211_CAPINFO_SHORT_SLOTTIME    0x0400
#define IEEE80211_CAPINFO_APSD              0x0800
#define IEEE80211_CAPINFO_RADIOMEAS         0x1000
#define IEEE80211_CAPINFO_DSSSOFDM          0x2000

/* HT capability flags, copied from wlan_modules/include/ieee80211.h */
#define IEEE80211_HTCAP_C_ADVCODING             0x0001
#define IEEE80211_HTCAP_C_CHWIDTH40             0x0002
#define IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC    0x0000 /* Capable of SM Power Save (Static) */
#define IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC   0x0004 /* Capable of SM Power Save (Dynamic) */
#define IEEE80211_HTCAP_C_SM_RESERVED           0x0008 /* Reserved */
#define IEEE80211_HTCAP_C_SM_ENABLED            0x000c /* SM enabled, no SM Power Save */
#define IEEE80211_HTCAP_C_GREENFIELD            0x0010
#define IEEE80211_HTCAP_C_SHORTGI20             0x0020
#define IEEE80211_HTCAP_C_SHORTGI40             0x0040
#define IEEE80211_HTCAP_C_TXSTBC                0x0080
#define IEEE80211_HTCAP_C_TXSTBC_S                   7
#define IEEE80211_HTCAP_C_RXSTBC                0x0300  /* 2 bits */
#define IEEE80211_HTCAP_C_RXSTBC_S                   8
#define IEEE80211_HTCAP_C_DELAYEDBLKACK         0x0400
#define IEEE80211_HTCAP_C_MAXAMSDUSIZE          0x0800  /* 1 = 8K, 0 = 3839B */
#define IEEE80211_HTCAP_C_DSSSCCK40             0x1000
#define IEEE80211_HTCAP_C_PSMP                  0x2000
#define IEEE80211_HTCAP_C_INTOLERANT40          0x4000
#define IEEE80211_HTCAP_C_LSIGTXOPPROT          0x8000
#define IEEE80211_HTCAP_C_SM_MASK               0x000c /* Spatial Multiplexing (SM) capabitlity bitmask */
/* ldpc */
#define IEEE80211_HTCAP_C_LDPC_NONE     0
#define IEEE80211_HTCAP_C_LDPC_RX       0x1
#define IEEE80211_HTCAP_C_LDPC_TX       0x2
#define IEEE80211_HTCAP_C_LDPC_TXRX     0x3


/* athcap, from wlan_modules/umac/include/ieee80211_node.h */
#define IEEE80211_NODE_TURBOP   0x01          /* Turbo prime enable */
#define IEEE80211_NODE_AR       0x10          /* AR capable */
#define IEEE80211_NODE_BOOST    0x80

#endif //__ACFG_TOOL_H__

