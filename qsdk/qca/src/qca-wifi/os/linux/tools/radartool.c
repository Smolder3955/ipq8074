/*
 * Copyright (c) 2013,2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <qcatools_lib.h> /* library for common headerfiles */
#include <if_athioctl.h>
#define _LINUX_TYPES_H
/*
 * Provide dummy defs for kernel types whose definitions are only
 * provided when compiling with __KERNEL__ defined.
 * This is required because ah_internal.h indirectly includes
 * kernel header files, which reference these data types.
 */
#define __be64 u_int64_t
#define __le64 u_int64_t
#define __be32 u_int32_t
#define __le32 u_int32_t
#define __be16 u_int16_t
#define __le16 u_int16_t
#define __be8  u_int8_t
#define __le8  u_int8_t
typedef struct {
        volatile int counter;
} atomic_t;

#include <wlan_dfs_ioctl.h>

#ifndef ATH_DEFAULT
#define ATH_DEFAULT "wifi0"
#endif

#define RADAR_NL80211_CMD_SOCK_ID    DEFAULT_NL80211_CMD_SOCK_ID
#define RADAR_NL80211_EVENT_SOCK_ID  DEFAULT_NL80211_EVENT_SOCK_ID

/*
 * Device revision information.
 */
typedef struct {
    u_int16_t   ah_devid;            /* PCI device ID */
    u_int16_t   ah_subvendorid;      /* PCI subvendor ID */
    u_int32_t   ah_mac_version;      /* MAC version id */
    u_int16_t   ah_mac_rev;          /* MAC revision */
    u_int16_t   ah_phy_rev;          /* PHY revision */
    u_int16_t   ah_analog_5Ghz_rev;  /* 5GHz radio revision */
    u_int16_t   ah_analog_2Ghz_Rev;  /* 2GHz radio revision */
} HAL_REVS;

struct radarhandler {
	int	s;
	struct ath_diag atd;
    struct socket_context sock_ctx;
};

/*
 * radar_send_command; function to send the cfg command or ioctl command.
 * @radar     : pointer to radarhandler
 * @ifname    : interface name
 * @buf       : buffer
 * @buflen    : buffer length
 * return     : 0 for sucess, -1 for failure
 */
int radar_send_command (struct radarhandler *radar, const char *ifname, void *buf, size_t buflen, int ioctl_sock_fd)
{
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data buffer;
    int nl_cmd = QCA_NL80211_VENDOR_SUBCMD_PHYERR;
    int msg;
    wifi_cfg80211_context pcfg80211_sock_ctx;
#endif /* UMAC_SUPPORT_CFG80211 */
#if UMAC_SUPPORT_WEXT
    struct ifreq ifr;
    int ioctl_cmd = SIOCGATHPHYERR;
#endif

    if (radar->sock_ctx.cfg80211) {
#if UMAC_SUPPORT_CFG80211
        pcfg80211_sock_ctx = radar->sock_ctx.cfg80211_ctxt;
        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = NULL;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_sendcmd(&pcfg80211_sock_ctx,
                nl_cmd, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            fprintf(stderr, "Couldn't send NL command\n");
            return -1;
        }
#endif /* UMAC_SUPPORT_CFG80211 */
    } else {
#if UMAC_SUPPORT_WEXT
        if (ifname) {
            size_t dstsize;

            memset(ifr.ifr_name, '\0', IFNAMSIZ);
            dstsize = (strlen(ifname)+1) < IFNAMSIZ ?
                (strlen(ifname)+1) : IFNAMSIZ;
            strlcpy(ifr.ifr_name, ifname, dstsize);
        } else {
            fprintf(stderr, "no such file or device\n");
            return -1;
        }
        ifr.ifr_data = buf;
        if (ioctl(ioctl_sock_fd, ioctl_cmd, &ifr) < 0) {
            perror("ioctl failed");
            return -1;
        }
#endif /* UMAC_SUPPORT_WEXT */
    }
    return 0;
}


/*
 * handle_radar : Function to handle all radar related operations.
 * @radar       : Pointer to radar handler
 * @dfs_flag    : Flag that decides the operation handled
 * @value       : value to be set/variable to get
 * @set         : 1 for set param and 0 for get param
 * returns 0 for set params and value to be get in get params
 */
static int handle_radar(struct radarhandler *radar, u_int32_t dfs_flag,
                        u_int32_t value, u_int32_t set)
{
    radar->atd.ad_id = dfs_flag;
    if (set) {
        radar->atd.ad_out_data = NULL;
        radar->atd.ad_out_size = 0;
        radar->atd.ad_in_data = (void *) &value;
        radar->atd.ad_in_size = sizeof(u_int32_t);
    } else {
        radar->atd.ad_in_data = NULL;
        radar->atd.ad_in_size = 0;
        radar->atd.ad_out_data = (void *) &value;
        radar->atd.ad_out_size = sizeof(u_int32_t);
    }

    if (radar_send_command(radar, radar->atd.ad_name,
        (caddr_t)&radar->atd, sizeof(struct ath_diag),
        radar->sock_ctx.sock_fd) < 0) {
        err(1, "%s", radar->atd.ad_name);
    }

    /* Clear references to local variables*/
    if (set) {
        radar->atd.ad_in_data = NULL;
        return 0;
    } else {
        radar->atd.ad_out_data = NULL;
        return value;
    }
}

/*
 * radarGetThresholds : collect threshold info
 * @radar             : pointer to radarhandler
 * @pe                : pointer to structure of type struct dfs_ioctl_params
 */
static void radarGetThresholds(struct radarhandler *radar,
    struct dfs_ioctl_params *pe)
{
    radar->atd.ad_id = DFS_GET_THRESH | ATH_DIAG_DYN;
    radar->atd.ad_out_data = (void *) pe;
    radar->atd.ad_out_size = sizeof(struct dfs_ioctl_params);
    if (radar_send_command(radar, radar->atd.ad_name,
        (caddr_t)&radar->atd, sizeof(struct ath_diag),
		radar->sock_ctx.sock_fd) < 0) {
            err(1, "%s", radar->atd.ad_name);
    }
}

/*
 * radarBangradar : Handle bangradar commands.
 * bangradar_type : Type of bangradar command issued based on number of arguments.
 * seg_id         : Segment ID.
 * is_chirp       : Chirp information.
 * freq_offset    : Frequency offset.
 */
static void radarBangradar(struct radarhandler *radar,
    enum dfs_bangradar_types bangradar_type, int seg_id, int is_chirp,
    int freq_offset)
{
    struct dfs_bangradar_params pe;

	pe.bangradar_type = bangradar_type;
	pe.seg_id = seg_id;
	pe.is_chirp = is_chirp;
	pe.freq_offset = freq_offset;

	radar->atd.ad_id = DFS_BANGRADAR | ATH_DIAG_IN;
	radar->atd.ad_out_data = NULL;
	radar->atd.ad_out_size = 0;
	radar->atd.ad_in_data = (void *) &pe;
	radar->atd.ad_in_size = sizeof(struct dfs_bangradar_params);
	if (radar_send_command(radar, radar->atd.ad_name, (caddr_t)&radar->atd, sizeof(struct ath_diag),
			radar->s) < 0) {
		err(1, "%s", radar->atd.ad_name);
	}
	radar->atd.ad_in_data = NULL;
}

/*
 * radarset  : set various parameters for radar detection
 * @radar    : pointer to radar handler
 * @op       : enum values for different options
 * @param    : param input
 */
void radarset(struct radarhandler *radar, int op, u_int32_t param)
{
    struct dfs_ioctl_params pe;

    pe.dfs_firpwr = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_rrssi = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_height = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_prssi = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_inband = DFS_IOCTL_PARAM_NOVAL;

    /* 5413 specific */
    pe.dfs_relpwr = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_relstep = DFS_IOCTL_PARAM_NOVAL;
    pe.dfs_maxlen = DFS_IOCTL_PARAM_NOVAL;

    switch(op) {
        case DFS_PARAM_FIRPWR:
            pe.dfs_firpwr = param;
            break;
        case DFS_PARAM_RRSSI:
            pe.dfs_rrssi = param;
            break;
        case DFS_PARAM_HEIGHT:
            pe.dfs_height = param;
            break;
        case DFS_PARAM_PRSSI:
            pe.dfs_prssi = param;
            break;
        case DFS_PARAM_INBAND:
            pe.dfs_inband = param;
            break;
            /* following are valid for 5413 only */
        case DFS_PARAM_RELPWR:
            pe.dfs_relpwr = param;
            break;
        case DFS_PARAM_RELSTEP:
            pe.dfs_relstep = param;
            break;
        case DFS_PARAM_MAXLEN:
            pe.dfs_maxlen = param;
            break;
    }
    radar->atd.ad_id = DFS_SET_THRESH | ATH_DIAG_IN;
    radar->atd.ad_out_data = NULL;
    radar->atd.ad_out_size = 0;
    radar->atd.ad_in_data = (void *) &pe;
    radar->atd.ad_in_size = sizeof(struct dfs_ioctl_params);
    if (radar_send_command(radar, radar->atd.ad_name,
        (caddr_t)&radar->atd, sizeof(struct ath_diag),
        radar->sock_ctx.sock_fd) < 0) {
            err(1, "%s", radar->atd.ad_name);
    }
    radar->atd.ad_in_data = NULL;
}

/*
 * radarGetNol : get NOL channel info
 * @radar      : pointer to radar handler
 * @fname      : file name
 */
void radarGetNol(struct radarhandler *radar, char *fname)
{
    struct dfsreq_nolinfo nolinfo;
    FILE *fp = NULL;
    char buf[100];

    if (fname != NULL) {
        fp = fopen(fname, "wb");
        if (!fp) {
            memset(buf, '\0', sizeof(buf));
            snprintf(buf, sizeof(buf) - 1,"%s: fopen %s error",__func__, fname);
            perror(buf);
            return;
        }
    }

    radar->atd.ad_id = DFS_GET_NOL | ATH_DIAG_DYN;
    radar->atd.ad_in_data = NULL;
    radar->atd.ad_in_size = 0;
    radar->atd.ad_out_data = (void *) &nolinfo;
    radar->atd.ad_out_size = sizeof(struct dfsreq_nolinfo);

    if (radar_send_command(radar, radar->atd.ad_name,
        (caddr_t)&radar->atd, sizeof(struct ath_diag),
        radar->sock_ctx.sock_fd) < 0) {
        err(1, "%s", radar->atd.ad_name);
    }

    /*
     * Optionally dump the contents of dfsreq_nolinfo
     */
    if (fp != NULL) {
        fwrite(&nolinfo, sizeof(struct dfsreq_nolinfo), 1, fp);
        fclose(fp);
    }

    /* clear references to local variables */
    radar->atd.ad_out_data = NULL;
}

/*
 * radarSetNOl  : set NOL channel info
 * @radar       : pointer to radar handler
 * @fname       : file name
 */
void radarSetNol(struct radarhandler *radar, char *fname)
{
    struct dfsreq_nolinfo nolinfo;
    FILE *fp;
    char buf[100];
    int i;

    fp = fopen(fname, "rb");
    if (!fp)
    {
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1,"%s: fopen %s error",__func__, fname);
        perror(buf);
        return;
    }

    fread(&nolinfo, sizeof(struct dfsreq_nolinfo), 1, fp);
    fclose(fp);

    for (i=0; i<nolinfo.dfs_ch_nchans; i++)
    {
        /* Modify for static analysis, prevent overrun */
        if ( i < DFS_CHAN_MAX ) {
            printf("nol:%d channel=%d startticks=%llu timeout=%d \n",
                    i, nolinfo.dfs_nol[i].nol_freq,
                    (unsigned long long)nolinfo.dfs_nol[i].nol_start_ticks,
                    nolinfo.dfs_nol[i].nol_timeout_ms);
        }
    }

    radar->atd.ad_id = DFS_SET_NOL | ATH_DIAG_IN;
    radar->atd.ad_out_data = NULL;
    radar->atd.ad_out_size = 0;
    radar->atd.ad_in_data = (void *) &nolinfo;
    radar->atd.ad_in_size = sizeof(struct dfsreq_nolinfo);

    if (radar_send_command(radar, radar->atd.ad_name,
        (caddr_t)&radar->atd, sizeof(struct ath_diag),
         radar->sock_ctx.sock_fd) < 0) {
        err(1, "%s", radar->atd.ad_name);
    }
    radar->atd.ad_in_data = NULL;
}

/*
 * usage: prints radartool usage message
 */
static void usage(void)
{
    const char *msg = "\
Usage: radartool (-i <interface>) [cmd]\n\
       for cfg : radartool -n (-i <interface>) [cmd]\n\
firpwr X            set firpwr (thresh to check radar sig is gone) to X (int32)\n\
rrssi X             set radar rssi (start det) to X dB (u_int32)\n\
height X            set threshold for pulse height to X dB (u_int32)\n\
prssi               set threshold to checkif pulse is gone to X dB (u_int32)\n\
inband X            set threshold to check if pulse is inband to X (0.5 dB) (u_int32)\n\
dfstime X           set dfs test time to X secs\n\
en_relpwr_check X   enable/disable radar relative power check (AR5413 only)\n\
relpwr X            set threshold to check the relative power of radar (AR5413 only)\n\
usefir128 X         en/dis using in-band pwr measurement over 128 cycles(AR5413 only)\n\
en_block_check X    en/dis to block OFDM weak sig as radar det(AR5413 only)\n\
en_max_rrssi X      en/dis to use max rssi instead of last rssi (AR5413 only)\n\
en_relstep X        en/dis to check pulse relative step (AR5413 only)\n\
relstep X           set threshold to check relative step for pulse det(AR5413 only)\n\
maxlen X            set max length of radar signal(in 0.8us step) (AR5413 only)\n\
numdetects          get number of radar detects\n\
getnol              get NOL channel information\n\
setnol              set NOL channel information\n\
dfsdebug            set the DFS debug mask\n\
dfs_disable_radar_marking X\n\
                    set this flag so that after radar detection on a DFS chan,\n\
                    the channel is not marked as radar and is not blocked from\n\
                    being set as AP's channel. However,the radar hit chan will\n\
                    be added to NOL list.\n\
g_dfs_disable_radar_marking\n\
                    Retrieve the value of disable_radar_marking flag.\n\
usenol X            set nol to X, where X is:\n\
                    1 (default) make CSA and switch to a new channel on radar detect\n\
                    0, make CSA with next channel same as current on radar detect\n\
                    2, no CSA and stay on the same channel on radar detect\n\
use_precacnol X     set precacnol to X, where X is:\n\
                    1 (default) mark agile channel on radar detect as NOL\n\
                    0, do not mark agile channel as radar affected channel\n\
ignorecac X         enable (X=0) or disable (X=1) CAC\n\
setnoltimeout X     set nol timeout for X secs (Default value = 1800 sec)\n\
bangradar           simulate radar on entire current channel\n\
bangradar X         simulate radar at the given segment ID, where\n\
                    X is segment id(0, 1)\n\
bangradar X Y Z     simulate radar at particular frequency, where\n\
                    X is segment id(0, 1)\n\
                    Y is chirp information(0 - Non chirp, 1 - Chrip)\n\
                    Z is frequency offset(-40MHz <= Z <= 40MHz)\n\
                    Example:\n\
                    To simulate chirp radar on segment 1 with frequency offset -10Mhz:\n\
                    radartool -i wifi0 bangradar 1 1 -10\n\
showPreCACLists     show pre CAC List (Required, Done, PreCAC NOL)\n\
resetPreCACLists    reset pre CAC list\n\
shownol             show NOL channels\n\
shownolhistory      show NOL channel history\n\
disable             disable radar detection\n\
enable              enable radar detection in software\n\
false_rssi_thr X    set false rssi threshold to X (Default is 50)\n\
rfsat_peak_mag X    set peak magnitude to X (Default is 40)\n\
setcacvalidtime X   set CAC validity time to X secs\n\
getcacvalidtime     get CAC validity time in secs\n";

    fprintf(stderr, "%s", msg);
}

int main(int argc, char *argv[])
{
    struct radarhandler radar;
    u_int32_t temp_result = 0;
    int err;

    memset(&radar, 0, sizeof(radar));

    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    radar.sock_ctx.cfg80211 = get_config_mode_type();

#if UMAC_SUPPORT_CFG80211
    /* figure out whether cfg80211 is enabled */
    if (argc > 1 && (strcmp(argv[1], "-n") == 0)) {
        if (!radar.sock_ctx.cfg80211){
            fprintf(stderr, "Invalid tag '-n' for current mode.\n");
            return -EINVAL;
        }
        radar.sock_ctx.cfg80211 = CONFIG_CFG80211;
        argc -= 1;
        argv += 1;
    }
#endif /* UMAC_SUPPORT_CFG80211 */

    err = init_socket_context(&radar.sock_ctx, RADAR_NL80211_CMD_SOCK_ID,
            RADAR_NL80211_CMD_SOCK_ID);
    if (err < 0) {
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        if (argc < 2) {
            fprintf(stderr, "%s: missing interface name for -i\n",
                    argv[0]);
            exit(-1);
        }
        if (strlcpy(radar.atd.ad_name, argv[2], sizeof(radar.atd.ad_name)) >=
                sizeof(radar.atd.ad_name)) {
            printf("%s..Arg too long %s\n",__func__,argv[2]);
            exit(-1);
        }
        argc -= 2;
        argv += 2;
    } else
        if (strlcpy(radar.atd.ad_name, ATH_DEFAULT, sizeof(radar.atd.ad_name)) >=
                sizeof (radar.atd.ad_name)) {
            printf("%s..Arg too long %s\n",__func__,ATH_DEFAULT);
            exit(-1);
        }

    /*
     * For strtoul():
     * A base of '0' means "interpret as either base 10 or
     * base 16, depending upon the string prefix".
     */
    if (argc >= 2) {
        if(streq(argv[1], "firpwr")) {
            radarset(&radar, DFS_PARAM_FIRPWR, (u_int32_t) atoi(argv[2]));
        } else if (streq(argv[1], "rrssi")) {
            radarset(&radar, DFS_PARAM_RRSSI, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "height")) {
            radarset(&radar, DFS_PARAM_HEIGHT, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "prssi")) {
            radarset(&radar, DFS_PARAM_PRSSI, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "inband")) {
            radarset(&radar, DFS_PARAM_INBAND, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "dfstime")) {
            handle_radar(&radar, DFS_MUTE_TIME | ATH_DIAG_IN,
                    strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "usenol")) {
            handle_radar(&radar, DFS_SET_USENOL | ATH_DIAG_IN,
                    atoi(argv[2]), 1);
        } else if (streq(argv[1], "use_precacnol")) {
            handle_radar(&radar, DFS_SET_USE_PRECACNOL | ATH_DIAG_IN,
                    atoi(argv[2]), 1);
        } else if (streq(argv[1], "dfsdebug")) {
            handle_radar(&radar, DFS_SET_DEBUG_LEVEL | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "ignorecac")) {
            handle_radar(&radar, DFS_IGNORE_CAC | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "setnoltimeout")) {
            handle_radar(&radar, DFS_SET_NOL_TIMEOUT | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "fft")) {
            handle_radar(&radar, DFS_ENABLE_FFT | ATH_DIAG_DYN,
                    temp_result, 1);
        } else if (streq(argv[1], "nofft")) {
            handle_radar(&radar, DFS_DISABLE_FFT | ATH_DIAG_DYN,
                    temp_result, 1);
        } else if (streq(argv[1], "bangradar")) {
            if (argc == 2)
            {
                /* This is without any argument "bangradar"
                 * This will add all the subchannels of the current channel
                 */
                radarBangradar(&radar, DFS_BANGRADAR_FOR_ALL_SUBCHANS, 0, 0, 0);
            } else if (argc == 3) {
                /* This is with segid argument "bangradar <segid>"
                 * This will add all subchannels of the given segment
                 */
                radarBangradar(&radar, DFS_BANGRADAR_FOR_ALL_SUBCHANS_OF_SEGID,
                    strtoul(argv[2], NULL, 0), 0, 0);
            } else if (argc == 5) {
                /* This is with segid, chirp/nonChirp and freq offset argument
                 * "bangradar <segid> <chirp/nonChirp> <freq_offset>"
                 * This will add specific subchannels based on the arguments
                 */
                radarBangradar(&radar, DFS_BANGRADAR_FOR_SPECIFIC_SUBCHANS,
                    strtoul(argv[2], NULL, 0), strtoul(argv[3], NULL, 0),
                    strtol(argv[4], NULL, 0));
            } else {
                fprintf(stderr, "Invalid Number of arguments for Bangradar\n");
		return -EINVAL;
            }
#if ATH_SUPPORT_ZERO_CAC_DFS
        } else if (streq(argv[1], "showPreCACLists")) {
            handle_radar(&radar, DFS_SHOW_PRECAC_LISTS | ATH_DIAG_DYN,
                    temp_result, 1);
        } else if (streq(argv[1], "resetPreCACLists")) {
            handle_radar(&radar, DFS_RESET_PRECAC_LISTS | ATH_DIAG_DYN,
                    temp_result, 1);
#endif
        } else if (streq(argv[1], "shownol")) {
            handle_radar(&radar, DFS_SHOW_NOL | ATH_DIAG_DYN,
                    temp_result, 1);
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        } else if (streq(argv[1], "shownolhistory")) {
            handle_radar(&radar, DFS_SHOW_NOLHISTORY | ATH_DIAG_DYN,
                    temp_result, 1);
#endif
        } else if (streq(argv[1], "disable")) {
            handle_radar(&radar, DFS_DISABLE_DETECT | ATH_DIAG_DYN,
                    temp_result, 1);
        } else if (streq(argv[1], "enable")) {
            handle_radar(&radar, DFS_ENABLE_DETECT | ATH_DIAG_DYN,
                    temp_result, 1);
        } else if (streq(argv[1], "dfs_disable_radar_marking")) {
            handle_radar(&radar, DFS_SET_DISABLE_RADAR_MARKING | ATH_DIAG_IN,
                         (uint32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "g_dfs_disable_radar_marking")) {
            printf("Disable Radar Marking : %d\n",
                   handle_radar(&radar, DFS_GET_DISABLE_RADAR_MARKING |
                                ATH_DIAG_DYN, temp_result, 0));
        } else if (streq(argv[1], "numdetects")) {
            printf("Radar: detected %d radars\n",
                    handle_radar(&radar, DFS_RADARDETECTS | ATH_DIAG_DYN,
                        temp_result, 0));
        } else if (streq(argv[1], "getnol")){
            radarGetNol(&radar, argv[2]);
        } else if (streq(argv[1], "setnol")) {
            radarSetNol(&radar, argv[2]);
        } else if (streq(argv[1],"-h")) {
            usage();
        /* Following are valid for 5413 only */
        } else if (streq(argv[1], "relpwr")) {
            radarset(&radar, DFS_PARAM_RELPWR, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "relstep")) {
            radarset(&radar, DFS_PARAM_RELSTEP, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "maxlen")) {
            radarset(&radar, DFS_PARAM_MAXLEN, strtoul(argv[2], NULL, 0));
        } else if (streq(argv[1], "false_rssi_thr")) {
            handle_radar(&radar, DFS_SET_FALSE_RSSI_THRES | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "rfsat_peak_mag")) {
            handle_radar(&radar, DFS_SET_PEAK_MAG | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        } else if (streq(argv[1], "getcacvalidtime")) {
            printf(" dfstime : %d\n", handle_radar(&radar,
                        DFS_GET_CAC_VALID_TIME | ATH_DIAG_DYN, temp_result, 0));
        } else if (streq(argv[1], "setcacvalidtime")) {
            handle_radar(&radar, DFS_SET_CAC_VALID_TIME | ATH_DIAG_IN,
                    (u_int32_t) strtoul(argv[2], NULL, 0), 1);
        }
    } else if (argc == 1) {
        struct dfs_ioctl_params pe = {0};
        u_int32_t nol;
        u_int32_t precacnol;
        nol = handle_radar(&radar, DFS_GET_USENOL | ATH_DIAG_DYN,
                temp_result, 0);
        precacnol = handle_radar(&radar, DFS_GET_USE_PRECACNOL | ATH_DIAG_DYN,
                      temp_result, 0);
        /*
         *      channel switch announcement (CSA). The AP does
         *      the following on radar detect:
         *      nol = 0, use CSA, but new channel is same as old channel
         *      nol = 1, use CSA and switch to new channel (default)
         *      nol = 2, do not use CSA and stay on same channel
         */

        printf ("Radar;\nUse NOL: %s\n",(nol==1) ? "yes" : "no");
        if (nol >= 2)
            printf ("No Channel Switch announcement\n");

        printf ("Use preCAC NOL: %s\n",(precacnol==1) ? "yes" : "no");

        radarGetThresholds(&radar, &pe);
        printf ("Firpwr (thresh to see if radar sig is gone):  %d\n",pe.dfs_firpwr);
        printf ("Radar Rssi (thresh to start radar det in dB): %u\n",pe.dfs_rrssi);
        printf ("Height (thresh for pulse height (dB):         %u\n",pe.dfs_height);
        printf ("Pulse rssi (thresh if pulse is gone in dB):   %u\n",pe.dfs_prssi);
        printf ("Inband (thresh if pulse is inband (in 0.5dB): %u\n",pe.dfs_inband);
        /* Following are valid for 5413 only */
        if (pe.dfs_relpwr & DFS_IOCTL_PARAM_ENABLE)
            printf ("Relative power check, thresh in 0.5dB steps: %u\n", pe.dfs_relpwr & ~DFS_IOCTL_PARAM_ENABLE);
        else
            printf ("Relative power check disabled\n");
        if (pe.dfs_relstep & DFS_IOCTL_PARAM_ENABLE)
            printf ("Relative step thresh in 0.5dB steps: %u\n", pe.dfs_relstep & ~DFS_IOCTL_PARAM_ENABLE);
        else
            printf ("Relative step for pulse detection disabled\n");                
            printf ("Max length of radar sig in 0.8us units: %u\n",pe.dfs_maxlen);
    } else {
        usage ();
    }
    destroy_socket_context(&radar.sock_ctx);
    return 0;
}
