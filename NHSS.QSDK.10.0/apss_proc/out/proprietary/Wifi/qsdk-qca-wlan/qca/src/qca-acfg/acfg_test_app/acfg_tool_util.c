/*
 * Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
*/


#include<stdlib.h>
#include<stdint.h>

#include<acfg_tool.h>
#include<qdf_types.h>

int get_uint32(char *str, uint32_t *val)
{
    unsigned long int ret ;

    ret = strtoul(str , NULL , 10);
    *val = (uint32_t) ret ;
    return 0;
}

int get_int32(char *str, int32_t *val)
{
    long int ret ;

    ret = strtol(str , NULL , 10);
    *val = (int32_t) ret ;
    return 0;
}

int get_hex(char *str, uint32_t *val)
{
    unsigned long int ret ;

    ret = strtoul(str , NULL , 16);
    *val = (uint32_t) ret ;
    return 0;
}

const char *
getcaps(int capinfo)
{
    static char capstring[32];
    char *cp = capstring;

    if (capinfo & IEEE80211_CAPINFO_ESS)
        *cp++ = 'E';
    if (capinfo & IEEE80211_CAPINFO_IBSS)
        *cp++ = 'I';
    if (capinfo & IEEE80211_CAPINFO_CF_POLLABLE)
        *cp++ = 'c';
    if (capinfo & IEEE80211_CAPINFO_CF_POLLREQ)
        *cp++ = 'C';
    if (capinfo & IEEE80211_CAPINFO_PRIVACY)
        *cp++ = 'P';
    if (capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)
        *cp++ = 'S';
    if (capinfo & IEEE80211_CAPINFO_PBCC)
        *cp++ = 'B';
    if (capinfo & IEEE80211_CAPINFO_CHNL_AGILITY)
        *cp++ = 'A';
    if (capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
        *cp++ = 's';
#if 0
    if (capinfo & IEEE80211_CAPINFO_RSN)
        *cp++ = 'R';
#endif
    if (capinfo & IEEE80211_CAPINFO_DSSSOFDM)
        *cp++ = 'D';
    *cp = '\0';
    return capstring;
}


const char *
getathcaps(int capinfo)
{
    static char capstring[32];
    char *cp = capstring;

    if (capinfo & IEEE80211_NODE_TURBOP)
        *cp++ = 'D';
    if (capinfo & IEEE80211_NODE_AR)
        *cp++ = 'A';
    if (capinfo & IEEE80211_NODE_BOOST)
        *cp++ = 'T';
    *cp = '\0';
    return capstring;
}

const char *
gethtcaps(int capinfo)
{
    static char capstring[32];
    char *cp = capstring;

    if (capinfo & IEEE80211_HTCAP_C_ADVCODING)
        *cp++ = 'A';
    if (capinfo & IEEE80211_HTCAP_C_CHWIDTH40)
        *cp++ = 'W';
    if ((capinfo & IEEE80211_HTCAP_C_SM_MASK) ==
             IEEE80211_HTCAP_C_SM_ENABLED)
        *cp++ = 'P';
    if ((capinfo & IEEE80211_HTCAP_C_SM_MASK) ==
             IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC)
        *cp++ = 'Q';
    if ((capinfo & IEEE80211_HTCAP_C_SM_MASK) ==
             IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC)
        *cp++ = 'R';
    if (capinfo & IEEE80211_HTCAP_C_GREENFIELD)
        *cp++ = 'G';
    if (capinfo & IEEE80211_HTCAP_C_SHORTGI40)
        *cp++ = 'S';
    if (capinfo & IEEE80211_HTCAP_C_DELAYEDBLKACK)
        *cp++ = 'D';
    if (capinfo & IEEE80211_HTCAP_C_MAXAMSDUSIZE)
        *cp++ = 'M';
    *cp = '\0';
    return capstring;
}

