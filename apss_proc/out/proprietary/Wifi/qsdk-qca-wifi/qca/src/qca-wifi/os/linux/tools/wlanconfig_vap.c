/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Qualcomm Innovation Center, Inc. has elected to receive this code under
 * the terms of the BSD license only.
 * Copyright (c) 2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <wlanconfig.h>

static void print_scaninfo(struct cfg80211_data *buffer);
static void print_chaninfo(const struct ieee80211_ath_channel *c,const struct ieee80211_ath_channel *c_160);

/*
 * print_binary: prints in %b format - used for printing capabilities.
 */
void print_binary(const char *s, unsigned v, const char *bits)
{
    int i, any = 0;
    char c;

    if(!bits) {
        printf("%s=%x", s, v);
        return;
    }

    if (*bits == 8)
        printf("%s=%o", s, v);
    else
        printf("%s=%x", s, v);
    bits++;
    putchar('<');

    while ((i = *bits++) != '\0') {
        if (v & (1 << (i-1))) {
            if (any)
                putchar(',');
            any = 1;
            for (; (c = *bits) > 32; bits++)
                putchar(c);
        } else
            for (; *bits > 32; bits++);
    }
    putchar('>');
}

#define MAX_2G_CHANNEL_NUMBER      14
#define BASE_2G_CHANNEL_FREQUENCY  2407
#define LAST_2G_CHANNEL_FREQUENCY  2484
#define CHANNEL_FIFTEEN_FREQUENCY  2512
#define BASE_5G_CHANNEL_FREQUENCY  5000
#define BASE_PUBLIC_SAFETY_FREQ    4940
#define LAST_PUBLIC_SAFETY_FREQ    4990
/*
 * Convert MHz Frequency to IEEE Channel number.
 */
static u_int frequency_to_chan(u_int frequency)
{
#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c)  \
        ((_c) > BASE_PUBLIC_SAFETY_FREQ && \
        (_c) < LAST_PUBLIC_SAFETY_FREQ)

    if (frequency == LAST_2G_CHANNEL_FREQUENCY)
        return IEEE80211_MAX_2G_CHANNEL;
    if (frequency < LAST_2G_CHANNEL_FREQUENCY)
        return (frequency - BASE_2G_CHANNEL_FREQUENCY) / 5;
    if (frequency < BASE_5G_CHANNEL_FREQUENCY) {
        if (IS_CHAN_IN_PUBLIC_SAFETY_BAND(frequency)) {
            return ((frequency * 10) +
                    (((frequency % 5) == 2) ? 5 : 0)
                    - (10*BASE_PUBLIC_SAFETY_FREQ))/5;
        } else if (frequency > 4900) {
            return (frequency - 4000) / 5;
        } else {
            return 15 + ((frequency - CHANNEL_FIFTEEN_FREQUENCY) / 20);
        }
    }
    return (frequency - BASE_5G_CHANNEL_FREQUENCY) / 5;
}

static int getopmode(const char *s, u_int32_t *flags)
{
    if (streq(s, "sta"))
        return IEEE80211_M_STA;
    if (streq(s, "ibss") || streq(s, "adhoc"))
        return IEEE80211_M_IBSS;
    if (streq(s, "monitor"))
        return IEEE80211_M_MONITOR;
    if (streq(s, "ap") || streq(s, "hostap")) {
        return IEEE80211_M_HOSTAP;
    }
    if (streq(s, "p2pgo"))
        return IEEE80211_M_P2P_GO;
    if (streq(s, "p2pcli"))
        return IEEE80211_M_P2P_CLIENT;
    if (streq(s, "p2pdev"))
        return IEEE80211_M_P2P_DEVICE;
    if (streq(s, "wrap")) {
        *flags |= IEEE80211_WRAP_VAP;
        return IEEE80211_M_HOSTAP;
    }
    if (streq(s, "specialvap")) {
        *flags |= IEEE80211_SPECIAL_VAP;
        return IEEE80211_M_HOSTAP;
    }
#if MESH_MODE_SUPPORT
    if (streq(s, "mesh")) {
        *flags |= IEEE80211_MESH_VAP;
        return IEEE80211_M_HOSTAP;
    }
#endif

#if ATH_SUPPORT_NAC
    if (streq(s, "smart_monitor")) {
        *flags |= IEEE80211_SMART_MONITOR_VAP;
        *flags |= IEEE80211_SPECIAL_VAP;
        return IEEE80211_M_HOSTAP;
    }
#endif
    if (streq(s, "lp_iot_mode")) {
        *flags |= IEEE80211_LP_IOT_VAP;
        return IEEE80211_M_HOSTAP;
    }
    if (streq(s, "lite_monitor")) {
        *flags |= IEEE80211_MONITOR_LITE_VAP;
        return IEEE80211_M_MONITOR;
    }
    errx(1, "unknown operating mode %s", s);
    /*NOTREACHED*/
    return -1;
}

static int getvapid(const char *s)
{
    if (s != NULL) {
        return atoi(s);
    }
    errx(1, "Invalid vapid %s", s);
    return -1;
}

static int getflag(const char  *s)
{
    const char *cp;
    int flag = 0;

    cp = (s[0] == '-' ? s+1 : s);
    if (strcmp(cp, "bssid") == 0)
        flag = IEEE80211_CLONE_BSSID;
    if (strcmp(cp, "nosbeacon") == 0)
        flag |= IEEE80211_NO_STABEACONS;
    if (flag == 0)
        errx(1, "unknown create option %s", s);
    return (s[0] == '-' ? -flag : flag);
}

static int vap_create (struct socket_context *sock_ctx, struct ifreq *ifr)
{
    char oname[IFNAMSIZ];
    int s;

    s = sock_ctx->sock_fd;
    if (s < 0)
        err(1, "socket(SOCK_DRAGM)");
    if (strlcpy(oname, ifr->ifr_name, sizeof(oname)) >= sizeof(oname)) {
        close(s);
        fprintf(stderr, "VAP name too long\n");
        exit(-1);
    }
    if (ioctl(s, SIOC80211IFCREATE, ifr) != 0) {
        err(1, "ioctl");
        return -1;
    }
    /* NB: print name of clone device when generated */
    if (memcmp(oname, ifr->ifr_name, IFNAMSIZ) != 0)
        printf("%s\n", ifr->ifr_name);

    return 0;
}

#if ATH_SUPPORT_WRAP
static int handle_macaddr(char *mac_str, u_int8_t *mac_addr)
{
    char tmp[MACSTR_LEN] = {0};

    if (strlcpy(tmp, mac_str, sizeof(tmp)) >= sizeof(tmp)) {
        printf("Invalid wlanaddr MAC address '%s', should be in format: "
                "(xx:xx:xx:xx:xx:xx)\n", mac_str);
        return -1;
    }

    if (ether_string2mac(mac_addr, tmp) != 0) {
        printf("Invalid MAC address: %s\n", tmp);
        return -1;
    }

    return 0;
}
#endif

int handle_command_create (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    int status = 0;
    struct ieee80211_clone_params cp;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    memset(&cp, 0, sizeof(cp));
    if (strlcpy(cp.icp_name, ifname, sizeof(cp.icp_name)) >= sizeof(cp.icp_name)) {
        fprintf(stderr, "ifname %s too long\n", ifname);
        exit(-1);
    }
    /* NB: station mode is the default */
    cp.icp_opmode = IEEE80211_M_STA;
    /* NB: default is to request a unique bssid/mac */
    cp.icp_flags = IEEE80211_CLONE_BSSID;

    while (argc > 3) {
        if (strcmp(argv[3], "wlanmode") == 0) {
            if (argc < 5)
                usage();
            cp.icp_opmode = (u_int16_t) getopmode(argv[4], &cp.icp_flags);
            argc--, argv++;
        } else if (strcmp(argv[3], "wlandev") == 0) {
            if (argc < 5)
                usage();
            memset(ifr.ifr_name, '\0', IFNAMSIZ);
            if (strlcpy(ifr.ifr_name, argv[4], sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name)) {
                fprintf(stderr, "ifname %s too long\n", argv[4]);
                exit(-1);
            }
            argc--, argv++;
#if ATH_SUPPORT_WRAP
        } else if (strcmp(argv[3], "wlanaddr") == 0) {
            if (argc < 5)
                usage();
            handle_macaddr(argv[4], cp.icp_bssid);
            cp.icp_flags &= ~IEEE80211_CLONE_BSSID;
            cp.icp_flags |= IEEE80211_CLONE_MACADDR;
            argc--, argv++;
        } else if (strcmp(argv[3], "mataddr") == 0) {
            if (argc < 5)
                usage();
            handle_macaddr(argv[4], cp.icp_mataddr);
            cp.icp_flags |= IEEE80211_CLONE_MATADDR;
            argc--, argv++;
        } else if (strcmp(argv[3], "-bssid") == 0) {
            if (argc < 5)
                usage();
            handle_macaddr(argv[4], cp.icp_bssid);
            cp.icp_flags &= ~(IEEE80211_CLONE_BSSID);
            argc--, argv++;
#endif
        } else if (strcmp(argv[3], "vapid") == 0) {
            if (argc < 5)
                usage();
            int32_t vapid = getvapid(argv[4]);
            if (vapid >= 0 && vapid <= 15) {
                cp.icp_vapid = vapid;
                cp.icp_flags &= ~(IEEE80211_CLONE_BSSID);
            } else {
                usage();
            }
            argc--, argv++;
        } else {
            int flag = getflag(argv[3]);
            if (flag < 0)
                cp.icp_flags &= ~(-flag);
            else
                cp.icp_flags |= flag;
        }
        argc--, argv++;
    }
    if (ifr.ifr_name[0] == '\0')
        errx(1, "no device specified with wlandev");
    if(sock_ctx->cfg80211) {
        send_command(sock_ctx, ifr.ifr_name, (void *)&cp, sizeof(struct ieee80211_clone_params),
                NULL, QCA_NL80211_VENDOR_SUBCMD_CLONEPARAMS, 0);
    } else {
        ifr.ifr_data = (void *) &cp;
        status = vap_create(sock_ctx, &ifr);
    }
    return status;
}

int handle_command_destroy (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    struct ifreq ifr;
    int s;
    if (sock_ctx->cfg80211) {
        s = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        s = sock_ctx->sock_fd;
    }
    if (s < 0)
        err(1, "socket(SOCK_DRAGM)");
    memset(&ifr, 0, sizeof(ifr));

    if (strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name)) {
        close(s);
        err(1, "ifname too long: %s\n", ifname);
    }

    if (ioctl(s, SIOC80211IFDESTROY, &ifr) < 0)
        err(1, "ioctl");

    if (sock_ctx->cfg80211) {
        close(s);
    }
    return 0;
}


typedef enum listing_type {
    LIST_STATION      = 0,
    LIST_SCAN         = 1,
    LIST_CHANNELS     = 2,
    LIST_KEYS         = 3,
    LIST_CAPABILITIES = 4,
    LIST_WME          = 5,
    LIST_HW_CAPABILITIES = 6,
    LIST_INVALID      = 7,
} listing_type;

int human_format = 0;
int	verbose = 0;
static int ps_activity = 0;

/* unalligned little endian access */
#ifndef LE_READ_4
#define LE_READ_4(p)					\
    ((u_int32_t)					\
     ((((const u_int8_t *)(p))[0]      ) |		\
      (((const u_int8_t *)(p))[1] <<  8) |		\
      (((const u_int8_t *)(p))[2] << 16) |		\
      (((const u_int8_t *)(p))[3] << 24)))
#endif

static int __inline iswpaoui(const u_int8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static int __inline iswmeoui(const u_int8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static int __inline isatherosoui(const u_int8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

static const char * getcaps(int capinfo)
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
    if (capinfo & IEEE80211_CAPINFO_DSSSOFDM)
        *cp++ = 'D';
    *cp = '\0';

    return capstring;
}

static const char * getathcaps(int capinfo)
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

static const char* gethtcaps(int capinfo)
{
    static char capstring[32];
    char *cp = capstring;

    if (capinfo & IEEE80211_HTCAP_C_ADVCODING)
        *cp++ = 'A';
    if (capinfo & IEEE80211_HTCAP_C_CHWIDTH40)
        *cp++ = 'W';
    if ((capinfo & IEEE80211_HTCAP_C_SM_MASK) ==
            IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED)
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


static const char *getxcaps(struct ieee80211req_sta_info * si)
{
    uint32_t capinfo=0;
    uint32_t capinfo2=0;
    uint8_t capinfo3=0;
    uint8_t capinfo4=0;
    static char capstring[32];
    char *cp = capstring;

    if(!si) {
       cp = '\0';
       return capstring;
    }
    capinfo = si->isi_ext_cap;
    capinfo2 = si->isi_ext_cap2;
    capinfo3 = si->isi_ext_cap3;
    capinfo4 = si->isi_ext_cap4;

    if (capinfo & IEEE80211_EXTCAPIE_2040COEXTMGMT)
        *cp++ = 'c';
    if (capinfo & IEEE80211_EXTCAPIE_ECSA)
        *cp++ = 'E';
    if (capinfo & IEEE80211_EXTCAPIE_TFS)
        *cp++ = 'T';
    if (capinfo & IEEE80211_EXTCAPIE_FMS)
        *cp++ = 'F';
    if (capinfo & IEEE80211_EXTCAPIE_PROXYARP)
        *cp++ = 'P';
    if (capinfo & IEEE80211_EXTCAPIE_CIVLOC)
        *cp++ = 'C';
    if (capinfo & IEEE80211_EXTCAPIE_GEOLOC)
        *cp++ = 'G';
    if (capinfo & IEEE80211_EXTCAPIE_WNMSLEEPMODE)
        *cp++ = 'W';
    if (capinfo & IEEE80211_EXTCAPIE_TIMBROADCAST)
        *cp++ = 't';
    if (capinfo & IEEE80211_EXTCAPIE_BSSTRANSITION)
        *cp++ = 'B';
    if (capinfo & IEEE80211_EXTCAPIE_PEER_UAPSD_BUF_STA)
        *cp++ = 'U';
    if (capinfo2 & IEEE80211_EXTCAPIE_QOS_MAP)
        *cp++ = 'Q';
    if (capinfo2 & IEEE80211_EXTCAPIE_OP_MODE_NOTIFY)
        *cp++ = 'O';
    if (capinfo3 & IEEE80211_EXTCAPIE_FTM_RES)
        *cp++ = 'R';
    if (capinfo3 & IEEE80211_EXTCAPIE_FTM_INIT)
        *cp++ = 'I';
    if (capinfo4 & IEEE80211_EXTCAPIE_FILS)
        *cp++ = 'f';
    *cp = '\0';
    return capstring;
}

static const char *getvhtcaps(int capinfo)
{
    static char capstring[32];
    int max_mpdu = 0;
    int supp_chan_width = 0;
    char *cp = capstring;

    max_mpdu = capinfo & 0x3;
    if (max_mpdu == IEEE80211_VHTCAP_MAX_MPDU_LEN_3839)
        *cp++ = '0';
    if (max_mpdu == IEEE80211_VHTCAP_MAX_MPDU_LEN_7935)
        *cp++ = '1';
    if (max_mpdu == IEEE80211_VHTCAP_MAX_MPDU_LEN_11454)
        *cp++ = '2';
    supp_chan_width = capinfo & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_MASK;
    if (supp_chan_width == IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80)
        *cp++ = '0';
    if (supp_chan_width == IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160)
        *cp++ = '1';
    if (supp_chan_width == IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160)
        *cp++ = '2';

    if (capinfo & IEEE80211_VHTCAP_RX_LDPC)
        *cp++ = 'L';
    if (capinfo & IEEE80211_VHTCAP_SHORTGI_80)
        *cp++ = 'g';
    if (capinfo & IEEE80211_VHTCAP_SHORTGI_160)
        *cp++ = 'G';
    if (capinfo & IEEE80211_VHTCAP_TX_STBC)
        *cp++ = 'T';
    if (capinfo & IEEE80211_VHTCAP_RX_STBC)
        *cp++ = 'R';
    if (capinfo & IEEE80211_VHTCAP_SU_BFORMER)
        *cp++ = 's';
    if (capinfo & IEEE80211_VHTCAP_SU_BFORMEE)
        *cp++ = 'S';
    if (capinfo & IEEE80211_VHTCAP_MU_BFORMER)
        *cp++ = 'm';
    if (capinfo & IEEE80211_VHTCAP_MU_BFORMEE)
        *cp++ = 'M';
    if (capinfo & IEEE80211_VHTCAP_TXOP_PS)
        *cp++ = 'P';
    if (capinfo & IEEE80211_VHTCAP_PLUS_HTC_VHT)
        *cp++ = 'H';
    *cp = '\0';
    return capstring;
}

/*
 * Copy the ssid string contents into buf, truncating to fit.  If the
 * ssid is entirely printable then just copy intact.  Otherwise convert
 * to hexadecimal.  If the result is truncated then replace the last
 * three characters with "...".
 */
static size_t copy_essid(char buf[], size_t bufsize,
        const u_int8_t *essid, size_t essid_len)
{
    const u_int8_t *p;
    size_t maxlen;
    int i;
    size_t orig_bufsize =  bufsize;

    if (essid_len > bufsize)
        maxlen = bufsize;
    else
        maxlen = essid_len;
    /* determine printable or not */
    for (i = 0, p = essid; i < maxlen; i++, p++) {
        if (*p < ' ' || *p > 0x7e)
            break;
    }
    if (i != maxlen) {		/* not printable, print as hex */
        if (bufsize < 3)
            return 0;

        strlcpy(buf, "0x", bufsize);

        bufsize -= 2;
        p = essid;
        for (i = 0; i < maxlen && bufsize >= 2; i++) {
            snprintf(&buf[2 + (2 * i)], (bufsize - 2 + (2 * i)), "%02x", *p++);
            bufsize -= 2;
        }
        maxlen = (2 + (2 * i));
    } else {			/* printable, truncate as needed */
        memcpy(buf, essid, maxlen);
    }
    if (maxlen != essid_len)
        memcpy(buf+maxlen-3, "...", 3);

    /* Modify for static analysis, protect for buffer overflow */
    buf[orig_bufsize-1] = '\0';

    return maxlen;
}

static void printie(const char* tag, const uint8_t *ie,
        size_t ielen, int maxlen)
{
    printf("%s", tag);
    if (verbose) {
        maxlen -= strlen(tag)+2;
        if (2*ielen > maxlen)
            maxlen--;
        printf("<");
        for (; ielen > 0; ie++, ielen--) {
            if (maxlen-- <= 0)
                break;
            printf("%02x", *ie);
        }
        if (ielen != 0)
            printf("-");
        printf(">");
    }
}

static void printies(const u_int8_t *vp, int ielen, int maxcols)
{
    while (ielen > 0) {
        switch (vp[0]) {
            case IEEE80211_ELEMID_VENDOR:
                if (iswpaoui(vp))
                    printie(" WPA", vp, 2+vp[1], maxcols);
                else if (iswmeoui(vp))
                    printie(" WME", vp, 2+vp[1], maxcols);
                else if (isatherosoui(vp))
                    printie(" ATH", vp, 2+vp[1], maxcols);
                else
                    printie(" VEN", vp, 2+vp[1], maxcols);
                break;
            case IEEE80211_ELEMID_RSN:
                printie(" RSN", vp, 2+vp[1], maxcols);
                break;
            default:
                printie(" ???", vp, 2+vp[1], maxcols);
                break;
        }
        ielen -= 2+vp[1];
        vp += 2+vp[1];
    }
}
/* ps_activity: power save activity */
#define LIST_STATION_ALLOC_SIZE 24*1024

static void print_stainfo_human_format(struct cfg80211_data *buffer)
{
    char *ieee80211_phymode_str[34] =  {
        "IEEE80211_MODE_AUTO",
        "IEEE80211_MODE_11A",
        "IEEE80211_MODE_11B",
        "IEEE80211_MODE_11G",
        "IEEE80211_MODE_FH",
        "IEEE80211_MODE_TURBO_A",
        "IEEE80211_MODE_TURBO_G",
        "IEEE80211_MODE_11NA_HT20",
        "IEEE80211_MODE_11NG_HT20",
        "IEEE80211_MODE_11NA_HT40PLUS",
        "IEEE80211_MODE_11NA_HT40MINUS",
        "IEEE80211_MODE_11NG_HT40PLUS",
        "IEEE80211_MODE_11NG_HT40MINUS",
        "IEEE80211_MODE_11NG_HT40",
        "IEEE80211_MODE_11NA_HT40",
        "IEEE80211_MODE_11AC_VHT20",
        "IEEE80211_MODE_11AC_VHT40PLUS",
        "IEEE80211_MODE_11AC_VHT40MINUS",
        "IEEE80211_MODE_11AC_VHT40",
        "IEEE80211_MODE_11AC_VHT80",
        "IEEE80211_MODE_11AC_VHT160",
        "IEEE80211_MODE_11AC_VHT80_80",
        "IEEE80211_MODE_11AXA_HE20",
        "IEEE80211_MODE_11AXG_HE20",
        "IEEE80211_MODE_11AXA_HE40PLUS",
        "IEEE80211_MODE_11AXA_HE40MINUS",
        "IEEE80211_MODE_11AXG_HE40PLUS",
        "IEEE80211_MODE_11AXG_HE40MINUS",
        "IEEE80211_MODE_11AXA_HE40",
        "IEEE80211_MODE_11AXG_HE40",
        "IEEE80211_MODE_11AXA_HE80",
        "IEEE80211_MODE_11AXA_HE160",
        "IEEE80211_MODE_11AXA_HE80_80",
        (char *)NULL,
    };

    uint8_t *buf = buffer->data;
    uint64_t len = buffer->length;

    uint8_t *cp;
    int ret = 0;
    u_int32_t txrate, rxrate = 0, maxrate = 0;
    u_int32_t time_val=0, hour_val=0, min_val=0, sec_val=0;
    char ntoa[MAC_STRING_LENGTH + 1] = {0};
    cp = buf;
    u_int8_t size_phy_array=0;

    size_phy_array = sizeof(ieee80211_phymode_str)/sizeof(ieee80211_phymode_str[0]);

    /* Return if the length of the buffer is less than sta_info */
    if (len < sizeof(struct ieee80211req_sta_info)) {
        return;
    }

    do {
        struct ieee80211req_sta_info *si;
        uint8_t *vp;
        const char *capinfo, *xcaps, *athcaps, *htcaps, *vhtcaps;

        si = (struct ieee80211req_sta_info *) cp;
        time_val = si->isi_tr069_assoc_time.tv_sec;
        hour_val = time_val / 3600;
        time_val = time_val % 3600;
        min_val = time_val / 60;
        sec_val = time_val % 60;
        vp = (u_int8_t *)(si+1);
        if(si->isi_txratekbps == 0)
            txrate = (si->isi_rates[si->isi_txrate] & IEEE80211_RATE_VAL)/2;
        else
            txrate = si->isi_txratekbps / 1000;
        if(si->isi_rxratekbps >= 0) {
            rxrate = si->isi_rxratekbps / 1000;
        }

        capinfo = getcaps(si->isi_capinfo);
        xcaps   = getxcaps(si);
        athcaps = getathcaps(si->isi_athflags);
        maxrate = si->isi_maxrate_per_client;
        htcaps  = gethtcaps(si->isi_htcap);
        vhtcaps = getvhtcaps(si->isi_htcap);

        if (maxrate & 0x80) maxrate &= 0x7f;
        ret = ether_mac2string(ntoa, si->isi_macaddr);
        printf("ADDR :%s AID:%4u CHAN:%4d TXRATE:%3dM RXRATE:%6dM RSSI:%4d IDLE:%4d TXSEQ:%6d RXSEQ:%7d CAPS:%5.4s XCAPS:%5s ACAPS:%-5.5s ERP:%3x STATE:%10x MAXRATE(DOT11):%14d HTCAPS:%14.6s VHTCAPS:%14.6s ASSOCTIME:%02u:%02u:%02u"
                , (ret != -1) ? ntoa:"WRONG MAC"
                , IEEE80211_AID(si->isi_associd)
                , si->isi_ieee
                , txrate
                , rxrate
                , si->isi_rssi + si->isi_nf /*si->isi_rssi gives SNR valur and RSSI=SNR+NF*/
                , si->isi_inact
                , si->isi_txseqs[0]
                , si->isi_rxseqs[0]
                , (*capinfo == '\0' ? "NULL" : capinfo)
                , (*xcaps == '\0' ? "NULL" : xcaps)
                , (*athcaps == '\0' ? "NULL" : athcaps)
                , si->isi_erp
                , si->isi_state
                , maxrate
                , (*htcaps  == '\0' ? "NULL" : htcaps)
                , (*vhtcaps == '\0' ? "NULL" : vhtcaps)
                , hour_val
                , min_val
                , sec_val
              );
        printies(vp, si->isi_ie_len, 24);
        printf (" MODE: %s \r\n",(si->isi_stamode < size_phy_array)?ieee80211_phymode_str[si->isi_stamode]:"IEEE80211_MODE_11B");
        printf("  PSMODE: %d \r\n",si->isi_ps);
        cp += si->isi_len, len -= si->isi_len;
    } while (len >= sizeof(struct ieee80211req_sta_info));

}

static void print_stainfo(struct cfg80211_data *buffer)
{
    char *ieee80211_phymode_str[34] =  {
        "IEEE80211_MODE_AUTO",
        "IEEE80211_MODE_11A",
        "IEEE80211_MODE_11B",
        "IEEE80211_MODE_11G",
        "IEEE80211_MODE_FH",
        "IEEE80211_MODE_TURBO_A",
        "IEEE80211_MODE_TURBO_G",
        "IEEE80211_MODE_11NA_HT20",
        "IEEE80211_MODE_11NG_HT20",
        "IEEE80211_MODE_11NA_HT40PLUS",
        "IEEE80211_MODE_11NA_HT40MINUS",
        "IEEE80211_MODE_11NG_HT40PLUS",
        "IEEE80211_MODE_11NG_HT40MINUS",
        "IEEE80211_MODE_11NG_HT40",
        "IEEE80211_MODE_11NA_HT40",
        "IEEE80211_MODE_11AC_VHT20",
        "IEEE80211_MODE_11AC_VHT40PLUS",
        "IEEE80211_MODE_11AC_VHT40MINUS",
        "IEEE80211_MODE_11AC_VHT40",
        "IEEE80211_MODE_11AC_VHT80",
        "IEEE80211_MODE_11AC_VHT160",
        "IEEE80211_MODE_11AC_VHT80_80",
        "IEEE80211_MODE_11AXA_HE20",
        "IEEE80211_MODE_11AXG_HE20",
        "IEEE80211_MODE_11AXA_HE40PLUS",
        "IEEE80211_MODE_11AXA_HE40MINUS",
        "IEEE80211_MODE_11AXG_HE40PLUS",
        "IEEE80211_MODE_11AXG_HE40MINUS",
        "IEEE80211_MODE_11AXA_HE40",
        "IEEE80211_MODE_11AXG_HE40",
        "IEEE80211_MODE_11AXA_HE80",
        "IEEE80211_MODE_11AXA_HE160",
        "IEEE80211_MODE_11AXA_HE80_80",
        (char *)NULL,
    };
    uint8_t *buf = buffer->data;
    uint64_t len = buffer->length;

    uint8_t *cp;
    u_int32_t txrate, rxrate = 0, maxrate = 0;
    u_int32_t time_val=0, hour_val=0, min_val=0, sec_val=0;
    char ntoa[MAC_STRING_LENGTH + 1] = {0};
    u_int8_t size_phy_array=0;

    size_phy_array = sizeof(ieee80211_phymode_str)/sizeof(ieee80211_phymode_str[0]);
    cp = buf;
    if (ps_activity == 0)
    {
        printf("%-17.17s %4s %4s %4s %4s %4s %7s %7s %4s %6s %6s %5s %5s %5s %7s %8s %14s %6s %9s %9s %6s %6s %5s %5s %24s\n"
                , "ADDR"
                , "AID"
                , "CHAN"
                , "TXRATE"
                , "RXRATE"
                , "RSSI"
                , "MINRSSI"
                , "MAXRSSI"
                , "IDLE"
                , "TXSEQ"
                , "RXSEQ"
                , "CAPS"
                , "XCAPS"
                , "ACAPS"
                , "ERP"
                , "STATE"
                , "MAXRATE(DOT11)"
                , "HTCAPS"
                , "VHTCAPS"
                , "ASSOCTIME"
                , "IEs"
                , "MODE"
                , "RXNSS"
                , "TXNSS"
                , "PSMODE"
                );
    }
    if (ps_activity == 1)
    {
        printf("%-17.17s %4s %4s %4s %4s %4s %7s %7s %4s %6s %6s %5s %5s %5s %7s %8s %14s %6s %9s %9s %6s %6s %5s %5s %24s %6s %9s\n"
                , "ADDR"
                , "AID"
                , "CHAN"
                , "TXRATE"
                , "RXRATE"
                , "RSSI"
                , "MINRSSI"
                , "MAXRSSI"
                , "IDLE"
                , "TXSEQ"
                , "RXSEQ"
                , "CAPS"
                , "XCAPS"
                , "ACAPS"
                , "ERP"
                , "STATE"
                , "MAXRATE(DOT11)"
                , "HTCAPS"
                , "VHTCAPS"
                , "ASSOCTIME"
                , "IEs"
                , "MODE"
                , "RXNSS"
                , "TXNSS"
                , "PSMODE"
                , "PSTIME"
                , "AWAKETIME"
                );
    }
    cp = buf;

    /* Return if the length of the buffer is less than sta_info */
    if (len < sizeof(struct ieee80211req_sta_info)) {
        return;
    }

    do {
        struct ieee80211req_sta_info *si;
        uint8_t *vp;
        const char *capinfo, *xcaps, *athcaps, *htcaps, *vhtcaps;

        si = (struct ieee80211req_sta_info *) cp;
        time_val = si->isi_tr069_assoc_time.tv_sec;
        hour_val = time_val / 3600;
        time_val = time_val % 3600;
        min_val = time_val / 60;
        sec_val = time_val % 60;
        vp = (u_int8_t *)(si+1);
        if(si->isi_txratekbps == 0)
            txrate = (si->isi_rates[si->isi_txrate] & IEEE80211_RATE_VAL)/2;
        else
            txrate = si->isi_txratekbps / 1000;
        if(si->isi_rxratekbps >= 0) {
            rxrate = si->isi_rxratekbps / 1000;
        }

        capinfo = getcaps(si->isi_capinfo);
        xcaps   = getxcaps(si);
        athcaps = getathcaps(si->isi_athflags);
        maxrate = si->isi_maxrate_per_client;
        htcaps  = gethtcaps(si->isi_htcap);
        vhtcaps = getvhtcaps(si->isi_htcap);

        if (maxrate & 0x80) maxrate &= 0x7f;
        ether_mac2string(ntoa, si->isi_macaddr);
        printf("%s %4u %4d %3dM %6dM %4d %7d %7d %4d %6d %7d %5.4s %5.4s %-5.5s %3x %10x %14d %15.6s %15.6s %02u:%02u:%02u    "
                , (ntoa != NULL) ? ntoa:"WRONG MAC"
                , IEEE80211_AID(si->isi_associd)
                , si->isi_ieee
                , txrate
                , rxrate
                , si->isi_rssi + si->isi_nf /*si->isi_rssi gives SNR valur and RSSI=SNR+NF*/
                , si->isi_min_rssi + si->isi_nf
                , si->isi_max_rssi + si->isi_nf
                , si->isi_inact
                , si->isi_txseqs[0]
                , si->isi_rxseqs[0]
                , (*capinfo == '\0' ? "NULL" : capinfo)
                , (*xcaps   == '\0' ? "NULL" : xcaps)
                , (*athcaps == '\0' ? "NULL" : athcaps)
                , si->isi_erp
                , si->isi_state
                , maxrate
                , (*htcaps  == '\0' ? "NULL" : htcaps)
                , (*vhtcaps == '\0' ? "NULL" : vhtcaps)
                , hour_val
                , min_val
                , sec_val
                );
        printies(vp, si->isi_ie_len, 24);
        printf (" %s ",(si->isi_curr_mode < size_phy_array) ? ieee80211_phymode_str[si->isi_curr_mode]:"IEEE80211_MODE_11B");
        printf (" %d %d %3d ",((si->isi_nss >> 4) & 0x0f), (si->isi_nss & 0x0f), si->isi_ps);
        if (ps_activity == 1)
            printf (" %6d %9d\r\n", si->ps_time, si->awake_time);
        else
            printf (" \n");

        printf (" Minimum Tx Power\t\t: %d\n", si->isi_pwrcapinfo & 0xFF);
        printf (" Maximum Tx Power\t\t: %d\n", si->isi_pwrcapinfo>>8 & 0xFF);
#if ATH_EXTRA_RATE_INFO_STA
        printf (" Highest Tx MCS Rate\t\t: %d\n", si->isi_tx_rate_mcs);
#endif
        printf (" HT Capability\t\t\t: %s\n",
                (si->isi_htcap > 0 ? "Yes": "No"));
#if ATH_SUPPORT_EXT_STAT
        printf (" VHT Capability\t\t\t: %s\n",
                (si->isi_vhtcap > 0 ? "Yes": "No"));
        printf (" MU capable\t\t\t: %s\n",
                (si->isi_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE) ? "Yes" : "No");
#endif
        printf (" SNR\t\t\t\t: %d\n", si->isi_rssi);
        printf (" Operating band\t\t\t: %s\n", (frequency_to_chan(si->isi_freq) > IEEE80211_MAX_2G_CHANNEL) ? "5GHz" : "2.4 GHz");
        printf (" Current Operating class\t: %d\n", si->isi_curr_op_class);
        int op_class=0;
        if (si->isi_num_of_supp_class != 0){
            printf (" Supported Operating classes\t:");
            for (op_class=0;op_class < si->isi_num_of_supp_class;op_class++) {
                if (si->isi_supp_class[op_class])
                    printf (" %d ", si->isi_supp_class[op_class]);
            }
            printf ("\n");
        }

        int rate =0;
        printf (" Supported Rates\t\t:");
        for (rate = 0;rate < IEEE80211_RATE_MAXSIZE;rate++) {
            if (si->isi_rates[rate])
                printf (" %d ", si->isi_rates[rate] & IEEE80211_RATE_VAL);
        }
        printf("\n");

        if (si->isi_nr_channels != 0) {
            int chan = si->isi_first_channel;
            int freq = 0;
            int chan_ix = 0;

            printf(" Channels supported\t\t:");
            while (chan_ix < si->isi_nr_channels) {
                 if (chan >= 1 && chan <= 14) {
                     /* 2.4GHz channels */
                     if (chan == 14) {
                         freq = 2484;
                     } else {
                         freq = 2407 + 5 * chan;
                     }
                     printf(" %d ", freq);

                     chan++;
                 } else {
                     /* 5GHz channels */
                     freq = 5000 + 5 * chan;
                     printf (" %d ", freq);

                     chan += 4;
                     switch(chan) {
                         /* Offsetting non-contiguous channels */
                         case  68: chan = 100; break;
                         case 148: chan = 149; break;
                     }
                 }

                 chan_ix++;
            }
            printf("\n");
        }
        printf(" Max STA phymode\t\t:");
        printf (" %s ",(si->isi_stamode < size_phy_array) ? ieee80211_phymode_str[si->isi_stamode]:"IEEE80211_MODE_11B");
        printf("\n");
        cp += si->isi_len, len -= si->isi_len;
    } while (len >= sizeof(struct ieee80211req_sta_info));
}

/* unalligned little endian access */
#ifndef LE_READ_4
#define LE_READ_4(p)					\
    ((u_int32_t)					\
     ((((const u_int8_t *)(p))[0]      ) |		\
      (((const u_int8_t *)(p))[1] <<  8) |		\
      (((const u_int8_t *)(p))[2] << 16) |		\
      (((const u_int8_t *)(p))[3] << 24)))
#endif

static int get_wmm_params(struct socket_context *sock_ctx, const char *ifname, int *param)
{
#if UMAC_SUPPORT_CFG80211
    struct nl_msg *nlmsg = NULL;
    int res = -EIO;
    struct nlattr *nl_venData = NULL;
    struct cfg80211_data buffer;
    uint32_t value;
    wifi_cfg80211_context *cfg80211_ctxt = &(sock_ctx->cfg80211_ctxt);
#endif

    if (sock_ctx->cfg80211) {
#if UMAC_SUPPORT_CFG80211
        /*   Below is the format for cfg80211 vendor command for WMM params.
             Prepare nl message and send command to driver for data

             <VendorCmd name="getwmmparams" ID="75" DEFAULTS="17">
             <Attribute name="dparam0" ID="17" TYPE="u32" DEFAULT="235"/>
             <Attribute name="value0" ID="18" TYPE="u32"/>
             <Attribute name="value1" ID="19" TYPE="u32"/>
             <Attribute name="value2" ID="20" TYPE="u32"/>
             <VendorRsp name="getwmmparams" ID="75" ATTR_MAX="2">
             <Attribute name="getwmmparams" ID="1" TYPE="u32"/>
             </VendorRsp>
             </VendorCmd>
         */

        /*
         * Prepare command with vendor command
         * Fill required attributes
         * Fill required callbacks
         * send_command
         */
        buffer.data = &value;
        buffer.length = sizeof(uint32_t);
        buffer.parse_data = 0;
        buffer.callback = NULL;
        buffer.parse_data = 0;

        nlmsg = wifi_cfg80211_prepare_command(cfg80211_ctxt,
                QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
                ifname);

        if (nlmsg) {
            nl_venData = (struct nlattr *)start_vendor_data(nlmsg);
            if (!nl_venData) {
                fprintf(stderr, "failed to start vendor data\n");
                nlmsg_free(nlmsg);
                return -EIO;
            }
            /* Attribute 17 */
            if (nla_put_u32(nlmsg,
                        QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND, QCA_NL80211_VENDORSUBCMD_WMM_PARAMS)) {
                nlmsg_free(nlmsg);
                return -EIO;
            }
            /* Attribute 18 */
            if (nla_put_u32(nlmsg,
                        QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE, param[0])) {
                nlmsg_free(nlmsg);
                return -EIO;
            }
            /* Attribute 19 */
            if (nla_put_u32(nlmsg,
                        QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA, param[1])) {
                nlmsg_free(nlmsg);
                return -EIO;
            }
            /* Attribute 20 */
            if (nla_put_u32(nlmsg,
                        QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH, param[2])) {
                nlmsg_free(nlmsg);
                return -EIO;
            }

            if (nl_venData) {
                end_vendor_data(nlmsg, nl_venData);
            }
            res = send_nlmsg(cfg80211_ctxt, nlmsg, &buffer);

            if (res < 0) {
                return res;
            }
            param[0] = value;
            return res;
        }
#endif
    } else {
#if UMAC_SUPPORT_WEXT
        return (send_command(sock_ctx, ifname, param, 3 * sizeof(int), NULL, 0, IEEE80211_IOCTL_GETWMMPARAMS));
#endif
    }

    return 0;

}

void print_hw_caps(unsigned hw_caps)
{
    char buf[128] = {'\0'};
    if(!hw_caps)
       return;

    memset(buf, 0x0, sizeof(buf));
    strlcpy(buf, "802.11", strlen("802.11") + 1);
    if (hw_caps & 1 << IEEE80211_MODE_11A)
        strlcat(buf, "a", strlen("a") + strlen(buf) + 1);
    if (hw_caps & 1 << IEEE80211_MODE_11B)
        strlcat(buf, "b", strlen("b") + strlen(buf) + 1);
    if (hw_caps & 1 << IEEE80211_MODE_11G)
        strlcat(buf, "g", strlen("g") +  strlen(buf) + 1);
    if (hw_caps & (1 << IEEE80211_MODE_11NA_HT20 |
                   1 << IEEE80211_MODE_11NG_HT20 |
                   1 << IEEE80211_MODE_11NA_HT40PLUS |
                   1 << IEEE80211_MODE_11NA_HT40MINUS |
                   1 << IEEE80211_MODE_11NG_HT40PLUS |
                   1 << IEEE80211_MODE_11NG_HT40MINUS |
                   1 << IEEE80211_MODE_11NG_HT40 |
                   1 << IEEE80211_MODE_11NA_HT40))
        strlcat(buf, "n", strlen("n") + strlen(buf) + 1);
    if (hw_caps & (1 << IEEE80211_MODE_11AC_VHT20 |
                   1 << IEEE80211_MODE_11AC_VHT40PLUS |
                   1 << IEEE80211_MODE_11AC_VHT40MINUS |
                   1 << IEEE80211_MODE_11AC_VHT40 |
                   1 << IEEE80211_MODE_11AC_VHT80 |
                   1 << IEEE80211_MODE_11AC_VHT160 |
                   1 << IEEE80211_MODE_11AC_VHT80_80))
        strlcat(buf, "/ac", strlen("/ac" ) + strlen(buf) + 1);

    fprintf(stderr, "%s\n", buf);
}

static void list_hw_capabilities(struct socket_context *sock_ctx,
                                 const char *ifname)
{
    uint32_t value = 0;
#if UMAC_SUPPORT_CFG80211
    struct nl_msg *nlmsg = NULL;
    int res = -EIO;
    struct nlattr *nl_venData = NULL;
    struct cfg80211_data buffer;
    wifi_cfg80211_context *cfg80211_ctxt = &(sock_ctx->cfg80211_ctxt);
#endif
    if (sock_ctx->cfg80211) {
#if UMAC_SUPPORT_CFG80211
        buffer.data = &value;
        buffer.length = sizeof(uint32_t);
        buffer.parse_data = 0;
        buffer.callback = NULL;
        buffer.parse_data = 0;

        nlmsg = wifi_cfg80211_prepare_command(cfg80211_ctxt,
                             QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
                             ifname);
        if (nlmsg) {
            nl_venData = (struct nlattr *)start_vendor_data(nlmsg);
            if (!nl_venData) {
                fprintf(stderr, "failed to start vendor data\n");
                nlmsg_free(nlmsg);
                return;
            }

            if (nla_put_u32(nlmsg,
                            QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
                            QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS)) {
                fprintf(stderr, "failed to init vendor subcmd data\n");
                nlmsg_free(nlmsg);
                return;
            }

            if (nla_put_u32(nlmsg,
                            QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
                            IEEE80211_PARAM_MODE)) {
                fprintf(stderr, "failed to init param mode data\n");
                nlmsg_free(nlmsg);
                return;
            }

            if (nl_venData) {
                end_vendor_data(nlmsg, nl_venData);
            }

            res = send_nlmsg(cfg80211_ctxt, nlmsg, &buffer);
            if (res < 0) {
                return;
            }
       }
#endif
    } else {
#if UMAC_SUPPORT_WEXT
       if ((send_command(sock_ctx, ifname, &value, sizeof(value),
                         NULL, QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS,
                         IEEE80211_PARAM_MODE)) < 0)
            errx(1, "unable to get driver capabilities");
#endif
    }
    print_hw_caps(value);
}

static void handle_list(struct socket_context *sock_ctx, const char *ifname,
    int listing_type, int flag)
{
    int list_alloc_size;

    list_alloc_size = (sock_ctx->cfg80211)? LIST_STATION_CFG_ALLOC_SIZE : LIST_STATION_IOC_ALLOC_SIZE;

    if (listing_type == LIST_STATION) {
        struct iwreq iwr;
        uint8_t *buf;
        int s;
        int req_space;
        u_int64_t len;
        struct cfg80211_data buffer;
        int error = 0;
start:
        req_space = 0;
        len = 0;
        buf = malloc(list_alloc_size);
        if (!buf) {
            fprintf (stderr, "Unable to allocate memory for station list\n");
            return;
        }
        if(sock_ctx->cfg80211) {
#if UMAC_SUPPORT_CFG80211
            buffer.data = buf;
            buffer.length = list_alloc_size;
            if (flag == HUMAN_FORMAT) {
                buffer.callback = &print_stainfo_human_format;
            } else {
                buffer.callback = &print_stainfo;
            }
            buffer.parse_data = 0;
            wifi_cfg80211_send_generic_command(&(sock_ctx->cfg80211_ctxt),
                    QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                    214, ifname, (char *)&buffer, list_alloc_size);
#endif
        } else {
#if UMAC_SUPPORT_WEXT
            s = sock_ctx->sock_fd;
            if (s < 0){
                free(buf);
                err(1, "socket(SOCK_DRAGM)");
            }

            if (!strncmp(ifname, "wifi", 4)) {
                free(buf);
                err(1, "Not a valid interface");
            }

            (void) memset(&iwr, 0, sizeof(iwr));

            if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
                free(buf);
                err(1, "ifname too long: %s\n", ifname);
            }

            iwr.u.data.pointer = (void *) buf;
            iwr.u.data.length = list_alloc_size;
            buffer.data = buf;
            buffer.length = list_alloc_size;
            iwr.u.data.flags = 0;
            //Support for 512 client
            req_space = ioctl(s, IEEE80211_IOCTL_STA_INFO, &iwr);
            if (req_space < 0 ){
                free(buf);
                errx(1, "unable to get station information");
            } else if(req_space > 0) {
                free(buf);
                buf = malloc(req_space);
                if (!buf) {
                    fprintf (stderr, "Unable to allocate memory for station list\n");
                    return;
                }
                iwr.u.data.pointer = (void *) buf;
                /* Because u_int_16 can't hold the entire size, we might need
                 * to good divisor so that it fits in short max
                 */
                iwr.u.data.length = req_space/LIST_STA_SPLIT_UNIT;
                if(iwr.u.data.length < req_space)
                    iwr.u.data.flags = 1;

                error = ioctl(s, IEEE80211_IOCTL_STA_INFO, &iwr);
                /* EAGAIN is returned if a station has connected or
                 * disconnected between the 2 ioctl request time. In this
                 * case, list_sta needs to be executed again to fetch correct
                 * amount of memory needed.
                 */
                if (error < 0 && errno == EAGAIN){
                    free(buf);
                    goto start;
                }
                if (error < 0){
                    free(buf);
                    errx(1, "unable to get station information");
                }
                len = req_space;
            }
            else {
                len = iwr.u.data.length;
            }

            if (len < sizeof(struct ieee80211req_sta_info)){
                free(buf);
                return;
            }
            buffer.data = buf;
            buffer.length = len;
            if (flag == HUMAN_FORMAT) {
                print_stainfo_human_format(&buffer);
            } else {
                print_stainfo(&buffer);
            }
#endif
        }
        free(buf);
    } else if (listing_type == LIST_SCAN) {
        struct iwreq iwr;
        int s;
        struct cfg80211_data buffer;
        uint8_t *buf = malloc(list_alloc_size);
        int len = list_alloc_size;
        printf("%-14.14s  %-17.17s  %4s %4s  %-5s %3s %4s\n"
                , "SSID"
                , "BSSID"
                , "CHAN"
                , "RATE"
                , "S:N"
                , "INT"
                , "CAPS"
              );

        if(sock_ctx->cfg80211) {
#if UMAC_SUPPORT_CFG80211
            buffer.data = buf;
            buffer.length = list_alloc_size;
            buffer.callback = &print_scaninfo;
            buffer.parse_data = 0;
            wifi_cfg80211_send_generic_command(&(sock_ctx->cfg80211_ctxt),
                    QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                    212, ifname, (char *)&buffer, list_alloc_size);
#endif
        } else {
#if UMAC_SUPPORT_WEXT
            s = sock_ctx->sock_fd;
            printf("\n scan req sent in wlanconfig \n");
            if (s < 0){
                free(buf);
                err(1, "socket(SOCK_DRAGM)");
            }

            if (!strncmp(ifname, "wifi", 4)) {
                free(buf);
                err(1, "Not a valid interface");
            }

            (void) memset(&iwr, 0, sizeof(iwr));

            if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
                free(buf);
                err(1, "ifname too long: %s\n", ifname);
            }

            iwr.u.data.pointer = (void *) buf;
            iwr.u.data.length = list_alloc_size;
            buffer.data = buf;
            buffer.length = list_alloc_size;
            if (ioctl(s, IEEE80211_IOCTL_SCAN_RESULTS, &iwr) < 0) {
                free(buf);
                errx(1, "unable to connect to ioctl");
            }
            len = iwr.u.data.length;
            buffer.length = len;
            if (len == -1)
                errx(1, "unable to get scan results");
            if (len < sizeof(struct ieee80211req_scan_result))
                return;
            printf("\n scan req sent in wlanconfig \n");
            print_scaninfo(&buffer);

#endif /*UMAC_SUPPORT_WEXT*/
        }
    } else if (listing_type == LIST_CHANNELS) {
        /* seg1 in ieee80211_channel structure is common to both 80 Mhz and 160 Mhz,
           hence we use a separate call to retrieve 160 Mhz information */
        struct ieee80211req_chaninfo chans = {0}, chans_160 = {0};
        struct ieee80211req_chaninfo achans = {0}, achans_160 = {0};
        const struct ieee80211_ath_channel *c;
        const struct ieee80211_ath_channel *c_160;
        int i, half;
        struct ieee80211_wlanconfig *config;
        u_int8_t is_vht80p80_available = 0;
        u_int8_t is_he80p80_available = 0;

        config = (struct ieee80211_wlanconfig *)&chans_160;
        memset(config, 0, sizeof(*config));
        config->cmdtype = IEEE80211_WLANCONFIG_GETCHANINFO_160;
        send_command(sock_ctx, ifname, &chans, list_alloc_size,
                NULL, QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN, IEEE80211_IOCTL_GETCHANINFO);
        send_command(sock_ctx, ifname, &chans_160, list_alloc_size,
                NULL, QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN160, IEEE80211_IOCTL_CONFIG_GENERIC);

        if (flag != ALLCHANS) {
            struct ieee80211req_chanlist active;
            send_command(sock_ctx, ifname, &active, list_alloc_size,
                    NULL, QCA_NL80211_VENDOR_SUBCMD_ACTIVECHANLIST, IEEE80211_IOCTL_GETCHANLIST);
            memset(&achans, 0, sizeof(achans));
            memset(&achans_160, 0, sizeof(achans_160));
            for (i = 0; i < chans.ic_nchans; i++) {
                c = &chans.ic_chans[i];
                if (!c) {
                    errx(1, "%s:%d Invalid channel encountered\n", __func__, __LINE__);
                }
                if (isset(active.ic_channels, c->ic_ieee) || flag)
                    memcpy(&(achans.ic_chans[achans.ic_nchans++]),
                        c, sizeof(achans.ic_chans[achans.ic_nchans]));
                c_160 = &chans_160.ic_chans[i];
                if (!c_160) {
                    errx(1, "%s:%d Invalid channel encountered\n", __func__, __LINE__);
                }
                if (isset(active.ic_channels, c_160->ic_ieee) || flag)
                    memcpy(&(achans_160.ic_chans[achans_160.ic_nchans++]),
                        c_160, sizeof(achans_160.ic_chans[achans_160.ic_nchans]));
            }
        } else
            achans = chans;
        achans_160 = chans_160;
        half = achans.ic_nchans / 2;
        if (achans.ic_nchans % 2)
            half++;
        for (i = 0; i < achans.ic_nchans / 2; i++) {
            print_chaninfo(&achans.ic_chans[i], &achans_160.ic_chans[i]);
            print_chaninfo(&achans.ic_chans[half+i], &achans_160.ic_chans[half+i]);
            printf("\n");

            if (IEEE80211_IS_CHAN_11AC_VHT80_80(&achans.ic_chans[i]) ||
                    IEEE80211_IS_CHAN_11AC_VHT80_80(&achans.ic_chans[half + i])) {
                is_vht80p80_available = 1;
            }

            if (IEEE80211_IS_CHAN_11AXA_HE80_80(&achans.ic_chans[i]) ||
                    IEEE80211_IS_CHAN_11AXA_HE80_80(&achans.ic_chans[half + i])) {
                is_he80p80_available = 1;
            }
        }
        if (achans.ic_nchans % 2) {
            print_chaninfo(&achans.ic_chans[i],&achans_160.ic_chans[i]);
            printf("\n");

            if (IEEE80211_IS_CHAN_11AC_VHT80_80(&achans.ic_chans[i])) {
                is_vht80p80_available = 1;
            }

            if (IEEE80211_IS_CHAN_11AXA_HE80_80(&achans.ic_chans[i])) {
                is_he80p80_available = 1;
            }
        }

        /* As per regulatory rules, if the chipset is VHT80_80 capable, then any
         * channel that can be the primary 20 MHz of a VHT80 BSS can also be made
         * the primary 20 MHz of the VHT80_80 BSS. Another VHT80 capable block of
         * channels that is dis-contiguous with the 80 MHz block containing the
         * primary channel can be made the secondary 80 MHz of the VHT80_80 BSS.
         *
         * We utilize this to avoid having to print VHT80_80 info separately, and
         * instead provide a single line at the end. This helps avoid excessive
         * clutter associated with representing all possible combinations
         * individually.
         *
         * Since the driver doesn't provide a complete list of combinations but
         * rather a flat list containing just the primary 20 MHz channels and
         * associated flags, we cannot properly sanity check to ensure that VHT80_80
         * and VHT80 flags are in sync. The driver is supposed to provide correct
         * information in line with regulatory definitions, and we rely on it to do
         * so.
         *
         * The above would apply to 802.11ax as well.
         * 11AX TODO  - In the unlikely case of revision of regulatory rules in this
         * respect for 802.11ax, revisit this.
         */

        if (is_vht80p80_available) {
            printf("\nV80_80 capable. Pri20: Any V80 pri20 above. Sec80 centre: "
                    "Centre of any V80 block discontiguous with primary 80.\n");
        }

        if (is_he80p80_available) {
            if (!is_vht80p80_available) {
                /* Note that this is improbable. But check added for completeness. */
                printf("\n");
            }

            printf("H80_80 capable. Pri20: Any H80 pri20 above. Sec80 centre: "
                    "Centre of any H80 block discontiguous with primary 80.\n");
        }
    } else if (listing_type == LIST_CAPABILITIES) {
        u_int32_t caps = 0;
        if ((send_command(sock_ctx, ifname, &caps, sizeof(caps),
                        NULL, QCA_NL80211_VENDOR_SUBCMD_LIST_CAP, IEEE80211_PARAM_DRIVER_CAPS)) < 0)
            errx(1, "unable to get driver capabilities");
        print_binary(ifname, caps, IEEE80211_C_BITS);
        putchar('\n');
    } else if (listing_type == LIST_HW_CAPABILITIES) {
        list_hw_capabilities(sock_ctx, ifname);
    } else if (listing_type == LIST_WME) {
        static const char *acnames[] = { "AC_BE", "AC_BK", "AC_VI", "AC_VO" };
        int param[3];
        int ac;

        param[2] = 0;       /* channel params */
        for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
again:
            if (param[2] != 0)
                printf("\t%s", "     ");
            else
                printf("\t%s", acnames[ac]);

            param[1] = ac;

            /* show WME BSS parameters */
            param[0] = IEEE80211_WMMPARAMS_CWMIN;
            if ((get_wmm_params(sock_ctx, ifname, &param[0]) >= 0))
                printf(" cwmin %2u", param[0]);
            param[0] = IEEE80211_WMMPARAMS_CWMAX;
            if ((get_wmm_params(sock_ctx, ifname, &param[0]) >= 0))
                printf(" cwmax %2u", param[0]);
            param[0] = IEEE80211_WMMPARAMS_AIFS;
            if ((get_wmm_params(sock_ctx, ifname, &param[0]) >= 0))
                printf(" aifs %2u", param[0]);
            param[0] = IEEE80211_WMMPARAMS_TXOPLIMIT;
            if ((get_wmm_params(sock_ctx, ifname, &param[0]) >= 0))
                printf(" txopLimit %3u", param[0]);
            param[0] = IEEE80211_WMMPARAMS_ACM;
            if ((get_wmm_params(sock_ctx, ifname, &param[0]) >= 0)) {
                if (param[0])
                    printf(" acm");
                else if (verbose)
                    printf(" -acm");
            }
            /* !BSS only */
            if (param[2] == 0) {
                param[0] = IEEE80211_WMMPARAMS_NOACKPOLICY;
                if ((get_wmm_params(sock_ctx, ifname, &param[0]) != -1)) {
                    if (param[0])
                        printf(" -ack");
                    else if (verbose)
                        printf(" ack");
                }
            }

            printf("\n");
            if (param[2] == 0) {
                param[2] = 1;       /* bss params */
                goto again;
            } else
                param[2] = 0;
        }
    } else {
        printf("%s: Unsupported command", __func__);
    }
}

typedef u_int8_t uint8_t;

static int getmaxrate(uint8_t rates[15], uint8_t nrates)
{
    int i, maxrate = -1;

    for (i = 0; i < nrates; i++) {
        int rate = rates[i] & IEEE80211_RATE_VAL;
        if (rate > maxrate)
            maxrate = rate;
    }
    return maxrate / 2;
}

static void print_scaninfo(struct cfg80211_data *buffer)
{
    uint8_t *cp;
    int ret = 0;
    char ssid[14];
    uint8_t *buf = buffer->data;
    uint64_t len = buffer->length;
    cp = buf;
    do {
        struct ieee80211req_scan_result *sr;
        uint8_t *vp;
        char ntoa[MAC_STRING_LENGTH + 1] = {0};

        sr = (struct ieee80211req_scan_result *) cp;
        vp = (u_int8_t *)(sr+1);
        ret = ether_mac2string(ntoa, sr->isr_bssid);
        printf("%-14.*s  %s  %3d  %3dM %2d:%-2d  %3d %-4.4s"
                , copy_essid(ssid, sizeof(ssid), vp, sr->isr_ssid_len)
                , ssid
                , (ret != -1) ? ntoa:"WRONG MAC"
                , frequency_to_chan(sr->isr_freq)
                , getmaxrate(sr->isr_rates, sr->isr_nrates)
                , (int8_t) sr->isr_rssi, sr->isr_noise
                , sr->isr_intval
                , getcaps(sr->isr_capinfo)
              );
        printies(vp + sr->isr_ssid_len, sr->isr_ie_len, 24);;
        printf("\n");
        cp += sr->isr_len, len -= sr->isr_len;
    } while (len >= sizeof(struct ieee80211req_scan_result));
}

static void print_chaninfo(const struct ieee80211_ath_channel *c,
        const struct ieee80211_ath_channel *c_160)
{
    char buf[72];
    char buf1[4];

    buf[0] = '\0';
    if (IEEE80211_IS_CHAN_FHSS(c))
        strlcat(buf, " FHSS", sizeof(buf));
    if (IEEE80211_IS_CHAN_11NA(c))
        strlcat(buf, " 11na", sizeof(buf));
    else if (IEEE80211_IS_CHAN_A(c))
        strlcat(buf, " 11a", sizeof(buf));
    else if (IEEE80211_IS_CHAN_11NG(c))
        strlcat(buf, " 11ng", sizeof(buf));
    /* XXX 11g schizophrenia */
    else if (IEEE80211_IS_CHAN_G(c) || IEEE80211_IS_CHAN_PUREG(c))
        strlcat(buf, " 11g", sizeof(buf));
    else if (IEEE80211_IS_CHAN_B(c))
        strlcat(buf, " 11b", sizeof(buf));
    if (IEEE80211_IS_CHAN_TURBO(c))
        strlcat(buf, " Turbo", sizeof(buf));
    if(IEEE80211_IS_CHAN_11N_CTL_CAPABLE(c))
        strlcat(buf, " C", sizeof(buf));
    if(IEEE80211_IS_CHAN_11N_CTL_U_CAPABLE(c))
        strlcat(buf, " CU", sizeof(buf));
    if(IEEE80211_IS_CHAN_11N_CTL_L_CAPABLE(c))
        strlcat(buf, " CL", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AC_VHT20(c))
        strlcat(buf, " V", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AC_VHT40PLUS(c))
        strlcat(buf, " VU", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AC_VHT40MINUS(c))
        strlcat(buf, " VL", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AC_VHT80(c)) {
        strlcat(buf, " V80-", sizeof(buf));
        snprintf(buf1, sizeof(buf1), "%3u", c->ic_vhtop_ch_freq_seg1);
        strlcat(buf, buf1, sizeof(buf));
    }
    if(IEEE80211_IS_CHAN_11AC_VHT160(c_160)) {
        strlcat(buf, " V160-", sizeof(buf));
        snprintf(buf1, sizeof(buf1), "%3u", c_160->ic_vhtop_ch_freq_seg2);
        strlcat(buf, buf1, sizeof(buf));
    }

    if(IEEE80211_IS_CHAN_11AX_CTL_CAPABLE(c))
        strlcat(buf, " H", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AX_CTL_U_CAPABLE(c))
        strlcat(buf, " HU", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AX_CTL_L_CAPABLE(c))
        strlcat(buf, " HL", sizeof(buf));
    if(IEEE80211_IS_CHAN_11AXA_HE80(c)) {
        strlcat(buf, " H80-", sizeof(buf));
        snprintf(buf1, sizeof(buf1), "%3u", c->ic_vhtop_ch_freq_seg1);
        strlcat(buf, buf1, sizeof(buf));
    }
    if(IEEE80211_IS_CHAN_11AXA_HE160(c_160)) {
        strlcat(buf, " H160-", sizeof(buf));
        snprintf(buf1, sizeof(buf1), "%3u", c_160->ic_vhtop_ch_freq_seg2);
        strlcat(buf, buf1, sizeof(buf));
    }

    /* XXX For now we use an optimized column width of 60. But can be increased
     * to say, 72 if required in the future.
     */
    printf("Channel %3u : %u%c%c%c Mhz%-60.60s",
            c->ic_ieee, c->ic_freq,
            IEEE80211_IS_CHAN_HALF(c) ? 'H' : (IEEE80211_IS_CHAN_QUARTER(c) ? 'Q' :  ' '),
            IEEE80211_IS_CHAN_PASSIVE(c) ? '*' : ' ',IEEE80211_IS_CHAN_DFSFLAG(c) ?'~':' ', buf);
}

int handle_command_list (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if (argc > 3) {
        const char *arg = argv[3];

        if (streq(arg, "sta")) {
            if (argc > 4)
            {
                const char *arg_activity = argv[4];
                if (streq(arg_activity, "psactivity")) {
                    ps_activity = PS_ACTIVITY;
                }
            }
            handle_list(sock_ctx, ifname, LIST_STATION, ps_activity);
        } else if (streq(arg, "scan") || streq(arg, "ap")) {
            handle_list(sock_ctx, ifname, LIST_SCAN, 0);
        } else if (streq(arg, "chan") || streq(arg, "freq")) {
            handle_list(sock_ctx, ifname, LIST_CHANNELS, ALLCHANS);
        } else if (streq(arg, "active")) {
            handle_list(sock_ctx, ifname, LIST_CHANNELS, 0);
        } else if (streq(arg, "keys")) {
            handle_list(sock_ctx, ifname, LIST_KEYS, 0);
        } else if (streq(arg, "caps")){
            handle_list(sock_ctx, ifname, LIST_CAPABILITIES, 0);
        } else if (streq(arg, "hwcaps")){
            handle_list(sock_ctx, ifname, LIST_HW_CAPABILITIES, 0);
        } else if (streq(arg, "wme")){
            handle_list(sock_ctx, ifname, LIST_WME, 0);
        } else if (streq(arg, "-H")){
            handle_list(sock_ctx, ifname, LIST_STATION, HUMAN_FORMAT);
        }
    } else				/* NB: for compatibility */
        handle_list(sock_ctx, ifname, LIST_STATION, ps_activity);
    return 0;
}
