/*
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc
 *
 * 2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "netlink.h"
#ifdef IFACE_SUPPORT_CFG80211
#include "wlanif_cmn.h"
#endif
#include "priv_netlink.h"
#include "pthread.h"
#include "drv_nl80211.h"

#define _BYTE_ORDER _BIG_ENDIAN

#include "ieee80211_external.h"
#include "iface_mgr_api.h"

#ifndef MGMT_FRAM_TAG_SIZE
#define MGMT_FRAM_TAG_SIZE 30
#endif

#define SON_ACTION_BUF_SIZE 256
#define WPS_STATE_INTERVAL 120
#define WPS_SON_OUI1 0x00   /* to be set by orbi with big endian orbi_oui */
#define WPS_SON_OUI2 0x03
#define WPS_SON_OUI3 0x7f
#define WPS_NBHVAP_INTERVAL 60
#define WPS_NBH_STATUS_TIMER 60
#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0

#ifdef IFACE_SUPPORT_CFG80211
extern struct wlanif_config *wlanIfIfM;
#endif

#define null_addr (const u8 *) "\x00\x00\x00\x00\x00\x00"

static int wps_state;
/* Global variable to make sure that we do not get
   more than 1 press in 2 minutes
   Set to 1 on button press and reset when 2 min timer expires
   Set to 1 when a WPS_LISTEN is received and reset when ?
     */
static int push_active;
int g_push_local=0;
int sigusr_needed=0;

char ap_ifname[3][16];
char sta_ifname[3][16];
char ap_radio[3][16];
char sta_radio[3][16];
char ap_macaddr[3][20];;
char sta_macaddr[3][20];
unsigned char ap_macaddr_hex[3][20];
unsigned char bh_sta_macaddr_hex[3][20];
unsigned char action_sta_macaddr_hex[20];
char * tmp_ap_macaddr_hex = NULL;
char * tmp_bh_sta_macaddr_hex= NULL;
char * tmp_action_sta_macaddr_hex=NULL;

char nbh_apiface[3][16];
char nbh_radio[3][16];
char nbh_macaddr[3][20];
int num_nbh_vaps = 0;
int num_bh_vaps = 0;
int num_bh_sta_vaps =0;

static int is_re;
static int is_root;
static int inform_root_propagate;
static int wps_probe_req_seen;
/* There is a possibility of SIGUSR signal being received at the instant of
   1 min timer expiry. In such cases, though local push is a success, an
   action would also be sent to try wps, it is redundant, hence flag
   wps_success is used*/
static int wps_success=0;
struct son_driver_data *drv = NULL;

struct son_driver_data {
    int ioctl_sock;
    struct netlink_data *netlink;
    char *wifi_ifname;
    s32 pipeFd;
#if IFACE_SUPPORT_CFG80211
    struct nlk80211_global *g_nl;
#endif
    enum driver_mode drv_mode;
};

struct son_action_mgt_args {
    u_int8_t    category;
    u_int8_t    action;
};

union son_event_data {
    struct son_rx_mgmt {
        const u8 *frame;
        size_t frame_len;
    } son_rx_mgmt;
};

enum son_event_type {
    WPSIE_LISTEN_RE,
    WPS_INFORM_ROOT,
    START_WPS,
    DISABLE_WPS,
    STOP_WPS,
    ROOT_INFORM_REP,
    REP_INFORM_ROOT,
    LISTEN_ONE_MIN,
};

static const char *config_file = "/var/run/sta_macaddr";
//static const char *command = "wlanconfig ath0 list sta | tail -n  1 | cut -f1 -d\" \" > /var/run/sta_macaddr";

struct ieee80211_action_vs_public {
    u_int8_t    vendor_oui[3];         /* 0x8c, 0xfd, 0xf0 for QCA */
    u_int8_t    vs_public_action;   /* type of vendor specific variable length content */
    /* followed by vendor specific variable length content */
}__packed;

#define WPS_LISTEN_EV 0x01 // Generated when button is pushed at the root
#define ROOT_INFORMED_EV 0x02
#define RE_START_WPS_EV 0x03
#define DISABLE_WPS_EV 0x04
#define STOP_WPS_EV 0x05
#define ROOT_INFORMED_REP_EV 0x06
#define REP_INFORMED_ROOT_EV 0x08
#define LISTEN_ONE_MIN_EV    0x09
#define WPS_SON_PIPE_PATH "/var/run/sonwps.pipe"

void pbc_sonwps_get_pipemsg(s32 fd, void *eloop_ctx, void *sock_ctx);
static void sig_handler(int);
static void son_send_action(void *ctx, enum son_event_type event,union son_event_data *data , char *ifname, int iface_id);
static void stop_wps_on_nbh_ap();
static void stop_wps_on_bh_ap();
static void start_wps_on_nbh_ap();
static void start_wps_on_bh_ap();
static int stop_wps_on_ap(char *vap, char *radio);
static int start_wps_on_ap(char *vap, char *radio);
static int char2hex(char *charstr);

 /*  Timer to handle the 2 min expiry from when the button was pressed
     We do not listen to ACTION / WPS PROBE REQ after this expiry
     Armed when button is pressed or when ACTION Sequnce starts as per
     instruction from ROOT for the BH Vaps
     cancelled when this node asks another node to start FH WPS - to be discussed
     */

void handle_wps_state_timer(void *eloop_ctx, void *timeout_ctx)
{
    wps_state = 0;
    g_push_local = 0;
    push_active = 0;
    printf("wps timeout: wps_state set to 0\n");
}

/*Timer to indicate 1 min aftef FH WPS was started on remote node
  Not sure if this is needed or the 2 min timer could be run to
  completion
  */

void handle_nbh_status_timer(void *eloop_ctx, void *timeout_ctx)
{
    push_active=0;
/* When PB in repeater, BH fails to find orbie within a minute, sends action
   to root, root starts action sequence for 1 minute,rep starts timer for
   2 min as it receives a LISTEN action, but root and rep should make
   their wps_state as 0 after a min as 1 min is already wasted by rep*/
    wps_state = 0;
    g_push_local = 0;
    printf("Releasing push active and wps_state=0!\n");
}


void  mac_printf(const u_int8_t *mac)
{
    printf("%02x:%02x:%02x:%02x:%02x:%02x \n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int copy_macaddr_hex(char *macaddr_hex_copy, const char *macaddr)
{
    /* The caller declares the string macaddr of size 20 */
    if (strlcpy(macaddr_hex_copy, macaddr, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    if (strlcpy(macaddr_hex_copy+2, macaddr+3, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    if (strlcpy(macaddr_hex_copy+4, macaddr+6, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    if (strlcpy(macaddr_hex_copy+6, macaddr+9, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    if (strlcpy(macaddr_hex_copy+8, macaddr+12, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    if (strlcpy(macaddr_hex_copy+10, macaddr+15, 3) >= 20*sizeof(macaddr[1]))
        return -1;
    char2hex(macaddr_hex_copy);
    return 0;
}

int get_stamac_addr()
{
    FILE *file;
    int i,err;
    char macaddr[20];
    char buffer[100];
    size_t size;
    for(i=0;i<num_bh_vaps;i++)
    {
        snprintf(buffer,sizeof(buffer),"wlanconfig %s list sta | grep -v 'ADDR' | head -n 1 | cut -f1 -d\" \" > /var/run/sta_macaddr",ap_ifname[i]);
        system(buffer);
        file = fopen(config_file, "r");
        if(file !=NULL)
        {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            fclose(file);
            if(size==0)
                continue;
            else
                break;
        }
    }

    file=fopen(config_file,"r");
    if(file==0)
    {
        printf("Could not open sta_config_file\n");
        return -1;
    }
    else
    {
        int ret=0;
        ret=fscanf(file,"%s",macaddr);
        if(ret!=EOF)
        {
            memset(action_sta_macaddr_hex, 0, sizeof(macaddr));
            tmp_action_sta_macaddr_hex = (char *)&(action_sta_macaddr_hex);
            err = copy_macaddr_hex(tmp_action_sta_macaddr_hex, macaddr);
            if (err == -1)
            {
                fclose(file);
                return -1;
            }
            mac_printf(action_sta_macaddr_hex);
        }
        else
        {
            printf("Station not present in CAP, Cant send action frame\n");
            return -1;
        }
    }
    fclose(file);
    return 0;
}


 /*Timer to handle 1 min of FH WPS
   Armed when button is pressed and WPS is started locally
   WPS gets  started locally in the FH VAPs when button is pressed
   Cancelled in the root / rptr when the local FH VAP wps process is a success
   Cancelled in the root when a WPS Probe was seen by a BH VAP of a repeater and it informs the root
   Cancelled in the root when a WPS Probe was seen by a BH VAP of  the ROOT
   Expiry sends a ATION frame to the remote node to start WPS on FH
       */

void handle_nbh_local_timer(void *eloop_ctx, void *timeout_ctx)
{

    struct son_driver_data *drv = (struct son_driver_data*)eloop_ctx;
    union son_event_data event;
    char buf[256];
    int i;

    os_memset(&event, 0, sizeof(event));
    if(wps_success!=1)
    {
        wps_state = 0;
        push_active=1;
        /*  2 min timer has to be cancelled on root, so that no other orbi client
            tries to connect to root as root is informing its repeater to start wps,
            at a time either root/repeater can start wps,Since we cancel the 2 min timer,
            timer handler will not be called, so the variables that
            are reset inside the timer expiry will not be done, Remains set
            which affects the next iteration of push event*/
        g_push_local = 0;
        printf("1 min timer expired,NBH connection failed locally, sending action\n");
        /* Asking rep to start_wps,cancel all wps processes locally*/
        eloop_cancel_timeout(handle_wps_state_timer, NULL, NULL);

        eloop_cancel_timeout(handle_nbh_status_timer, NULL, NULL);
        eloop_register_timeout(WPS_NBH_STATUS_TIMER, 0, handle_nbh_status_timer,
                               NULL, NULL);
        stop_wps_on_bh_ap();
        stop_wps_on_nbh_ap();

        if(is_root)
        {
            /* send action frame for all nbh vaps */
            int s;
            s=get_stamac_addr();
            if(s== -1)
            {
                printf("Error reading sta_config file\n");
            }
            else
            {
                printf("Sending ROOT_INFORM_REP event\n");
                for(i= 0; i < num_bh_vaps; i++)
                {
                    os_memset(buf, 0, sizeof(buf));
                    event.son_rx_mgmt.frame = (u8 *)buf + MGMT_FRAM_TAG_SIZE;
                    son_send_action(drv,ROOT_INFORM_REP,&event,ap_ifname[i],i);
                }
            }

        }
        if(is_re)
        {

            printf("Sending REP_INFORM_ROOT event\n");
            for (i=0;i< num_bh_sta_vaps; i++)
            {
		os_memset(buf, 0, sizeof(buf));
                event.son_rx_mgmt.frame = (u8 *)buf + MGMT_FRAM_TAG_SIZE;
                son_send_action(drv,REP_INFORM_ROOT,&event,sta_ifname[i],i);
            }
        }
    }
    /* TODO */
    /* free buffer ?*/
}

static int re_macaddr_match(char *macaddr)
{
   int j;
   for (j=0; j<num_bh_vaps; j++) {

       if (memcmp(macaddr, ap_macaddr_hex[j],IEEE80211_ADDR_LEN) == 0) 
            return j;
   }
   return -1;
}

static int sta_macaddr_match(char * macaddr)
{
    int j;
    /* first byte is Dont care for sta vap */
    macaddr[0]=0;
    for(j=0;j<num_bh_sta_vaps;j++){
        {
        bh_sta_macaddr_hex[j][0]=0;
        if (memcmp(macaddr,bh_sta_macaddr_hex [j],IEEE80211_ADDR_LEN) == 0)
                return 0;
        }
    }
        return -1;
}

static int char2hex(char *charstr)
{
    int i ;
    int hex_len, len = strlen(charstr);

    for(i=0; i<len; i++) {
        /* check 0~9, A~F */
        charstr[i] = ((charstr[i]-48) < 10) ? (charstr[i] - 48) : (charstr[i] - 55);
        /* check a~f */
        if ( charstr[i] >= 42 )
            charstr[i] -= 32;
        if ( charstr[i] > 0xf )
            return -1;
    }

    /* hex len is half the string len */
    hex_len = len /2 ;
    if (hex_len *2 < len)
        hex_len ++;

    for(i=0; i<hex_len; i++)
        charstr[i] = (charstr[(i<<1)] << 4) + charstr[(i<<1)+1];

    return 0;
}

static const char * athr_get_ioctl_name(int op)
{
    switch (op) {
        case IEEE80211_IOCTL_SEND_MGMT:
                return "SEND_MGMT";
        case IEEE80211_IOCTL_FILTERFRAME:
                return "FILTER_TYPE";
        default:
                return "??";
    }
}

/* function to start ACTION based sequence for BH vaps
   on all repeater and itself
   Called on push-button OR when the RPTR sends a REP_INFORMED_ROOT_EV
   REP_INFORMED_ROOT_EV is sent from RPT to ROOT when button is pushed on
   RPTR and neither FH nor BH is success in the first minute
   */
static void start_action_in_root( struct son_driver_data *drv)
{
    int timeout;
    char buf[256];
    union son_event_data event;
    timeout = (g_push_local == 0) ? 60 : 120;

    os_memset(&event, 0, sizeof(event));
    if(num_bh_vaps != 0)
    {
        wps_state = 1;
        printf("wps_state is 1!\n");
        eloop_cancel_timeout(handle_wps_state_timer, NULL, NULL);
        eloop_register_timeout(timeout, 0, handle_wps_state_timer,
                               NULL,NULL);
        int i;
        for (i=0;i< num_bh_vaps;i++)
        {
            event.son_rx_mgmt.frame = (u8 *)buf + MGMT_FRAM_TAG_SIZE;
            if(timeout==120)
                // To all BH Vaps
            {
                printf("Sending WPSIE_LISTEN_RE to REs\n");
                son_send_action(drv, WPSIE_LISTEN_RE, &event,ap_ifname[i],i);
            }
            else
            {
                printf("Sending LISTEN_ONE_MIN  to REs\n");
                son_send_action(drv, LISTEN_ONE_MIN, &event,ap_ifname[i],i);
            }
        }
    }
}

static void stop_wps_on_nbh_ap()
{
    int i;
    for(i = 0; i < num_nbh_vaps; i++)
    {
        stop_wps_on_ap(nbh_apiface[i], nbh_radio[i]);
    }

}


static void stop_wps_on_bh_ap()
{
    int i;
    for(i = 0; i < num_bh_vaps; i++)
    {
        stop_wps_on_ap(ap_ifname[i],ap_radio[i]);
    }
}

static void start_wps_on_nbh_ap()
{
    int i;
    for(i=0;i<num_nbh_vaps;i++)
    {
        start_wps_on_ap(nbh_apiface[i],nbh_radio[i]);
    }
}

static void  start_wps_on_bh_ap()
{
    int i;
    for(i=0;i<num_bh_vaps;i++)
    {
        start_wps_on_ap(ap_ifname[i],ap_radio[i]);
    }
}

static int stop_wps_on_ap(char *vap, char *radio)
{
    char name[50];
    struct dirent *d_ent;
    DIR *d;
    pid_t pid;
    int status = -1;
    name[0] = '\0';
    snprintf(name, sizeof(name), "/var/run/hostapd-%s", radio);
    printf("opening directory %s\n", name);
    d = opendir(name);
    if (!d) {
        printf("can't open directory %s\n", name);
        return -1;
    }

    name[0] = '\0';
    d_ent = readdir(d);
    while (d_ent) {
        if (!strncmp(d_ent->d_name, vap, IFNAMSIZ)) {
            snprintf(name, sizeof(name), "/var/run/hostapd-%s", radio);
            break;
        }
        d_ent = readdir(d);
    }
    closedir(d);

    if (name[0] != '\0') {
        char *const args[] = {
                   "hostapd_cli", "-i", vap, "-p", name, "wps_cancel", NULL };

        pid = fork();

        switch (pid) {
        case 0: // Child
            // if (execve("/usr/sbin/hostapd_cli", args, NULL) < 0) {}
            if (execvp("/usr/sbin/hostapd_cli", args) < 0) {
                printf("execvp failed (errno "
                        "%d %s)\n", errno, strerror(errno));
            }

            /* Shouldn't come here if execvp succeeds */
            exit(EXIT_FAILURE);

        case -1:
            printf("fork failed (errno "
                    "%d %s)\n", errno, strerror(errno));
            return -1;

        default: // Parent
            if (wait(&status) < 0) {
                printf("wait failed (errno "
                        "%d %s)\n", errno, strerror(errno));
                return -1;
            }

            if (!WIFEXITED(status)
                || WEXITSTATUS(status) == EXIT_FAILURE) {
                printf("child exited with error %d\n",
                !WIFEXITED(status) ? -1 : WEXITSTATUS(status));
                return -1;
            }

            return 0;
        }
    } else {
        printf("file %s not found\n", vap);
        return -1;
    }
}

static int start_wps_on_ap(char *vap, char *radio)
{
    char name[50];
    char name1[50];
    struct dirent *d_ent;
    DIR *d;
    FILE *f;
    pid_t pid;
    int status = -1;
    int pid_file_exist = 0;

    name1[0] = '\0';
    snprintf(name1, sizeof(name1), "/var/run/hostap-hotplug-%s.pid", vap);

    name[0] = '\0';
    snprintf(name, sizeof(name), "/var/run/hostapd-%s", radio);
    printf("opening directory %s\n", name);
    d = opendir(name);
    if (!d) {
        printf("can't open directory %s\n", name);
        return -1;
    }

    name[0] = '\0';
    d_ent = readdir(d);
    while (d_ent) {
        if (!strncmp(d_ent->d_name, vap, IFNAMSIZ)) {
            snprintf(name, sizeof(name), "/var/run/hostapd-%s", radio);
            break;  
        }
        d_ent = readdir(d);
    }
    closedir(d);

    if (name[0] != '\0') {
        char *const args[] = {
            "hostapd_cli", "-i", vap, "-p", name, "wps_pbc", NULL };
        char *const args1[] = {
            "hostapd_cli", "-i", vap, "-p", name, "-a",
            "/lib/wifi/wps-hostapd-nbh-update",
            "-P", name1, "-B", NULL };

        pid = fork();
        switch (pid) {
            case 0: // Child
                if (execvp("/usr/sbin/hostapd_cli", args) < 0) {
                    printf("exexecvp failed (errno "
                            "%d %s)\n", errno, strerror(errno));
                }
                /* Shouldn't come here if execvp succeeds */
                exit(EXIT_FAILURE);

            case -1:
                printf("fork failed (errno "
                        "%d %s)\n", errno, strerror(errno));
                return -1;

            default: // Parent
                if (wait(&status) < 0) {
                    printf("wait failed (errno "
                            "%d %s)\n", errno, strerror(errno));
                    return -1;
                }

                if (!WIFEXITED(status)
                        || WEXITSTATUS(status) == EXIT_FAILURE) {
                    printf("child exited with error %d\n",
                            !WIFEXITED(status) ? -1 : WEXITSTATUS(status));
                    return -1;
                }

                if (sigusr_needed==1)
                {
                    printf("Registering for signal catching\n");

                    if ((f = fopen(name1, "r")) == NULL) {
                        //printf("PID File not found for %s vap\n",vap);
                    }
                    else
                    {
                        pid_file_exist = 1;
                        fclose(f);
                    }

                    if (pid_file_exist)
                    {
                        printf(" pid_file_exist\n");
                        return 0;
                    }

                    pid = fork();
                    switch (pid) {
                        case 0: // Child
                            if (execvp("/usr/sbin/hostapd_cli", args1) < 0) 
                            {
                                printf("exexecvp failed (errno "
                                        "%d %s)\n", errno, strerror(errno));
                            }
                            exit(EXIT_FAILURE);

                        case -1:
                            printf("fork failed (errno "
                                    "%d %s)\n", errno, strerror(errno));
                            return -1;

                        default: // Parent
                            if (wait(&status) < 0) {
                                printf("wait failed (errno "
                                        "%d %s)\n", errno, strerror(errno));
                                return -1;
                            }

                            if (!WIFEXITED(status)
                                    || WEXITSTATUS(status) == EXIT_FAILURE) {
                                printf("child exited with error %d\n",
                                        !WIFEXITED(status) ? -1 : WEXITSTATUS(status));
                                return -1;
                            }

                            return 0;
                    }
                }
                return 0;
        }
    } else {
        printf("file %s not found\n", vap);
        return -1;
    }
}

static int
set80211priv(struct son_driver_data *drv, int op, void *data, int len, char *ifname )
{
#ifndef IFACE_SUPPORT_CFG80211
    struct iwreq iwr;
    int do_inline = len < IFNAMSIZ;

    /* Certain ioctls must use the non-inlined method */
    if ( op == IEEE80211_IOCTL_FILTERFRAME)
    {
        do_inline = 0;
    }

    memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
 //   printf("%s:called with ifname %s and op %x \n", __func__,iwr.ifr_name, op);

    if (do_inline)
    {
        /*
        * Argument data fits inline; put it there.
        */
        memcpy(iwr.u.name, data, len);
    }
    else
    {
        /*
        * Argument data too big for inline transfer; setup a
        * parameter block instead; the kernel will transfer
        * the data for the driver.
        */
        iwr.u.data.pointer = data;
        iwr.u.data.length = len;
    }

    if (ioctl(drv->ioctl_sock, op, &iwr) < 0)
    {
        printf("son: %s: %s: ioctl op=0x%x "
                           "(%s) len=%d failed: %d (%s)",
                           __func__, iwr.ifr_name, op,
                           athr_get_ioctl_name(op),
                           len, errno, strerror(errno));
        return -1;
    }
#else
    if (op == IEEE80211_IOCTL_FILTERFRAME) {
        if (wlanIfIfM->ops->setFilter(wlanIfIfM->ctx, ifname, data, len) < 0) {
            ifacemgr_printf("son: %s: %s: ioctl op=0x%x "
                    "(%s) len=%d failed: %d (%s)",
                    __func__, ifname, op,
                    athr_get_ioctl_name(op),
                    len, errno, strerror(errno));
            return -1;
        }
    } else if (op == IEEE80211_IOCTL_SEND_MGMT) {
        if (wlanIfIfM->ops->sendMgmt(wlanIfIfM->ctx, ifname, data, len) < 0) {
            ifacemgr_printf("son: %s: %s: ioctl op=0x%x "
                    "(%s) len=%d failed: %d (%s)",
                    __func__, ifname, op,
                    athr_get_ioctl_name(op),
                    len, errno, strerror(errno));
            return -1;
        }
    } else {
        ifacemgr_printf("%s: Un-supported op %d is sent \n", __func__, op);
        return -1;
    }
#endif
    return 0;
}

static int son_tx_wpspbc_action(
    struct son_driver_data *drv,
    struct son_action_mgt_args *actionargs,
    const u8 *frm , char *ifname, int iface_id)
{
    u8 buf[256];
    struct ieee80211_mgmt *mgmt;
    u8 *pos;
    int len;
    size_t length;
    struct ieee80211req_mgmtbuf *mgmt_frm;
    struct ieee80211_action_vs_public ven;

    mgmt = (struct ieee80211_mgmt *) frm;
    mgmt_frm = (struct ieee80211req_mgmtbuf *) buf;
    memset(&buf, 0, sizeof(buf));
    mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
                                           (WLAN_FC_STYPE_ACTION));

    memcpy(mgmt_frm->macaddr, (u8 *)mgmt->sa, IEEE80211_ADDR_LEN);
    if(actionargs->category ==  WLAN_ACTION_VENDOR_SPECIFIC) {
        mgmt->u.action.category = 0x7f;//WLAN_ACTION_VENDOR_SPECIFIC;
        ven.vendor_oui[0] = WPS_SON_OUI1;
        ven.vendor_oui[1] = WPS_SON_OUI2;
        ven.vendor_oui[2] = WPS_SON_OUI3;
        ven.vs_public_action = WLAN_EID_VENDOR_SPECIFIC;
        switch(actionargs->action) {
            case WPSIE_LISTEN_RE:
                memcpy(mgmt_frm->macaddr, broadcast_ether_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x07;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x01; //WPSIE_LISTEN_RE;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[7] = 0x00;
                pos +=1;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (mgmt_frm->buflen > sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                // TODO - Any SON way of finding out the addresses of REs
                break;

            case WPS_INFORM_ROOT:
                memcpy(mgmt_frm->macaddr, null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->bssid;
                memcpy(pos, (u8*)null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x0c;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x02; //WPS_INFORM_ROOT;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                if (inform_root_propagate == 1)
                {
                    memcpy(pos, (u8 *)(mgmt->u.action.u.vs_public_action.variable+7), IEEE80211_ADDR_LEN);
                }
                else
                {
                    memcpy(pos, (u8 *)ap_macaddr_hex[iface_id], IEEE80211_ADDR_LEN);
                }
                pos += IEEE80211_ADDR_LEN;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (&mgmt_frm->buf[0] + length > buf + sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;

            case START_WPS:
                memcpy(mgmt_frm->macaddr, broadcast_ether_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x0c;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x03;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                memcpy(pos, (u8 *)(mgmt->u.action.u.vs_public_action.variable+7), IEEE80211_ADDR_LEN);
                pos += IEEE80211_ADDR_LEN;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (&mgmt_frm->buf[0] + length > buf + sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;

            case STOP_WPS:
                memcpy(mgmt_frm->macaddr, null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->bssid;
                memcpy(pos, (u8*)null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x07;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x05;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[7] = 0x00;
                pos +=1;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (mgmt_frm->buflen > sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;

            case DISABLE_WPS:
                memcpy(mgmt_frm->macaddr, broadcast_ether_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x07;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x04; //DISABLE_WPS_EV;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[7] = 0x00;
                pos +=1;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (mgmt_frm->buflen > sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;
            case ROOT_INFORM_REP:
                memcpy(mgmt_frm->macaddr,broadcast_ether_addr , IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x0c;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x06; //ROOT_INFORM_REP
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                memcpy(pos, (u8 *)action_sta_macaddr_hex, IEEE80211_ADDR_LEN);
                pos += IEEE80211_ADDR_LEN;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (&mgmt_frm->buf[0] + length > buf + sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;

            case REP_INFORM_ROOT:
                memcpy(mgmt_frm->macaddr, null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->bssid;
                memcpy(pos, (u8*)null_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x0c;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x08; //REP_INFORM_ROOT
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[7] = 0x00;
                pos +=1;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (mgmt_frm->buflen > sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }

                break;

            case LISTEN_ONE_MIN:
                memcpy(mgmt_frm->macaddr, broadcast_ether_addr, IEEE80211_ADDR_LEN);
                pos = (u8*)&mgmt->u.action.u.vs_public_action;
                memcpy(pos, (u8*) &ven, 4);
                pos = mgmt->u.action.u.vs_public_action.variable;
                mgmt->u.action.u.vs_public_action.variable[0] = 0x07;
                pos += 1;
                memcpy(pos, (u8*) &ven.vendor_oui, 3);
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[4] = 0x0d;
                mgmt->u.action.u.vs_public_action.variable[5] = 0x09; //LISTEN_ONE_MIN;
                mgmt->u.action.u.vs_public_action.variable[6] = 0x00;
                pos += 3;
                mgmt->u.action.u.vs_public_action.variable[7] = 0x00;
                pos +=1;
                length = pos - (u8*) &mgmt->u.action.category;
                mgmt_frm->buflen = length + 24;

                if (mgmt_frm->buflen > sizeof(buf))
                {
                    printf("frame too long\n");
                    return -1;
                }
                break;

            default:
                return -1;
                break;
        }
        memcpy(&mgmt_frm->buf[0], mgmt, mgmt_frm->buflen);
        len = ((mgmt_frm->buflen) + (sizeof(struct ieee80211req_mgmtbuf)));
        return set80211priv(drv, IEEE80211_IOCTL_SEND_MGMT, mgmt_frm, len, ifname );
    }
    else
        return -1;
}

/* data is a union with only one structure that has a buffer for holding the mgmt frame and a frame length
 */
static void son_send_action(void *ctx, enum son_event_type event,
                          union son_event_data *data , char *ifname, int iface_id)
{
    struct son_driver_data *drv = ctx;

    switch (event) {
        case WPSIE_LISTEN_RE:
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = WPSIE_LISTEN_RE;

            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname, iface_id);
        }
            break;
        case WPS_INFORM_ROOT:
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = WPS_INFORM_ROOT;

            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
        }
            break;
        case START_WPS:
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = START_WPS;

            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
        }
             break;
        case DISABLE_WPS://broadcast disable
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = DISABLE_WPS;

            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
        }
            break;
        case STOP_WPS://RE specific disable
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = STOP_WPS;

            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
        }
            break;
         case ROOT_INFORM_REP:
          {
              struct son_action_mgt_args act_args;
              act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
              act_args.action   = ROOT_INFORM_REP;
              son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
          }
            break;
         case REP_INFORM_ROOT:
            {
              struct son_action_mgt_args act_args;
              act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
              act_args.action   = REP_INFORM_ROOT;
              son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname,iface_id);
            }
            break;

        case LISTEN_ONE_MIN:
        {
            struct son_action_mgt_args act_args;
            act_args.category = WLAN_ACTION_VENDOR_SPECIFIC;
            act_args.action   = LISTEN_ONE_MIN;
            son_tx_wpspbc_action(drv, &act_args, data->son_rx_mgmt.frame, ifname, iface_id);
        }
            break;
        default:
            printf("Unknown event %d", event);
            break;
    }
}

static
void son_raw_receive(void *ctx, void *src_addr, void *buf, uint32_t len, int ifidx)
{
    struct son_driver_data *drv;
    const struct ieee80211_mgmt *mgmt;
    union son_event_data event;
    u16 fc, stype;
#if IFACE_SUPPORT_CFG80211
    struct nlk80211_global *g_nl;

    g_nl = (struct nlk80211_global *) ctx;
    drv = g_nl->ctx;
#else
    drv = ctx;
#endif

    if (len < IEEE80211_HDRLEN)
     return;

    mgmt = (const struct ieee80211_mgmt *) buf;

    fc = le_to_host16(mgmt->frame_control);

    if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT)
        return;

    stype = WLAN_FC_GET_STYPE(fc);

    switch (stype) {
        case WLAN_FC_STYPE_PROBE_REQ:      //when wps_ie seen in probe request
            os_memset(&event, 0, sizeof(event));
            event.son_rx_mgmt.frame = buf;
            event.son_rx_mgmt.frame_len = len;
            if(wps_state == 1 && is_root)
            {
                int i;
                printf("Probe seen by root\n");
                /* stop nbh timer which is running for 1 min */
                eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
                stop_wps_on_nbh_ap();
                /* TODO */

                //action frame to REs to disable listen
                // On All backhaul VAPS
                for(i=0;i<num_bh_vaps;i++)
                {
                son_send_action(drv, DISABLE_WPS, &event, ap_ifname[i],i);
                }
                printf("disable wps listen and start wps on root\n");
                wps_state = 0;
                start_wps_on_bh_ap();
            }
            /* This frame is received for a backhaul vap*/ 
            /* TODO Send ACTION frame only if g_push_local == 0 */
            /* g_push_local == 0 check is needed so that when the button is
               pushed at the repeater, we do not look to the ROOT for
               advice
               */
            else if( g_push_local == 0 && wps_state == 1 && wps_probe_req_seen == 0)
            {
                int i;
                //On all bh sta vaps
                printf("Probe seen by repeater,inform root of wps prob_req\n");
                for( i=0;i<num_bh_sta_vaps;i++)
                {
                    son_send_action(drv, WPS_INFORM_ROOT, &event,sta_ifname[i],i); //ACTION 1
                }
               wps_probe_req_seen = 1;
            }
            
            break;

        case WLAN_FC_STYPE_ACTION:
            os_memset(&event, 0, sizeof(event));
            event.son_rx_mgmt.frame = buf;
            event.son_rx_mgmt.frame_len = len;

            switch(mgmt->u.action.u.vs_public_action.variable[5]) {
                //root to RE to start listening
                case WPS_LISTEN_EV:
                {
                    wps_state = 1;
                    wps_probe_req_seen = 0;
                    printf("wps_state is 1,push active is 1, probe not seen!\n");
                    eloop_cancel_timeout(handle_wps_state_timer, NULL, NULL);
                    eloop_register_timeout(WPS_STATE_INTERVAL, 0, handle_wps_state_timer,
                               NULL, NULL);
                    push_active = 1 ;
                    int i;
                    for( i=0;i<num_bh_vaps;i++)
                    {
                    son_send_action(drv, WPSIE_LISTEN_RE, &event, ap_ifname[i],i);
                    }
                    break;
                }

                case ROOT_INFORMED_EV:
                if(wps_state == 1 && is_root)
                    {
                        // To be sent on the correct backhaul AP VAP, check if we could send it on all BH VAPS in loop TODO: To be optimized
                        //root to RE to start wps in response to WPS_INFORM_ROOT frame
                        /* stop nbh timer which is running for 1 min */
                        eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
                        stop_wps_on_nbh_ap();

                        printf("Root sending  RE start wps action\n");
                        int i;
                        for( i=0;i<num_bh_vaps;i++)
                        {
                        son_send_action(drv, START_WPS, &event, ap_ifname[i],i);   //ACTION2
                        }
                        wps_state = 0;
                        break;
                    }
                    else if (wps_state == 1)
                    {
                        /* propagate action to root. Since, sta_iface
                         * is to be used, wps_propagate is not set */
                        inform_root_propagate = 1;
                        wps_probe_req_seen = 1;
                        // To be sent on all STA Vaps
                        int i;
                        printf("I am not root,propagating WPS_INFORM_ROOT\n" );
                        for( i=0; i<num_bh_sta_vaps;i++)
                        {
                        son_send_action(drv, WPS_INFORM_ROOT, &event,sta_ifname[i],i);
                        }
                        inform_root_propagate = 0;
                        break;
                    }
                   else if (wps_state == 0)
                    {
                        // To be sent to the VAP which saw WPS IE after wps_state=0 from netlink: TODO:
                   //     son_send_action(drv, STOP_WPS, &event, ap_ifname[1]);
                        break;
                    }
                    break;

                case RE_START_WPS_EV:
                    if (wps_state == 1)
                    {
                        unsigned char macaddr[20];
                        int interface_id = -1;
                        memcpy(macaddr, (u8 *)(mgmt->u.action.u.vs_public_action.variable+7), IEEE80211_ADDR_LEN);
                        printf("Mac addr of repeater to which root is sending start_wps:");
                        mac_printf(macaddr);

                        if ((interface_id = re_macaddr_match(macaddr)) != -1)
                        {
                            printf("command to start wps in RE \n");//calls hostapd to start wps in RE
                            /* stop nbh timer which is running for 1 min */
                            int j;
                            if (g_push_local == 1 ) {
                                eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
                                stop_wps_on_nbh_ap();
                            }
                            /* changing to loop on bh_ap_vaps may help */
                            start_wps_on_bh_ap();

                            for  (j=0;j< num_bh_vaps ;j++)
                            {
                               // son_send_action(drv, DISABLE_WPS, &event, ap_ifname[j]);
                                son_send_action(drv, DISABLE_WPS, &event, ap_ifname[j],j);
                            }
                            wps_state=0;
                        }
                        else
                        {
                            printf(" I am not the rep to start_wps,Propagating WPS\n");
                            // On all STA VAps in loop
                            int i;
                            for(i=0; i<num_bh_vaps;i++)
                            {
                            son_send_action(drv, START_WPS, &event,ap_ifname[i],i);
                            }
                        }
                    }
                    break;
                case ROOT_INFORMED_REP_EV:
                    {
                        unsigned char macaddr[20];
                        int interface_id = -1;
                        memcpy(macaddr, (u8 *)(mgmt->u.action.u.vs_public_action.variable+7), IEEE80211_ADDR_LEN);
                        printf("Mac addr of repeater to which root is sending start_wps:");
                        mac_printf(macaddr );

                        if ((interface_id = sta_macaddr_match(macaddr)) != -1)
                        {
                            if(g_push_local!=1)
                            {
                                eloop_cancel_timeout(handle_nbh_status_timer, NULL, NULL);
                                eloop_register_timeout(WPS_NBH_STATUS_TIMER, 0, handle_nbh_status_timer,
                                           NULL, NULL);
                                push_active = 1;
                                printf(" Root's Command to start wps in RE on NBH vaps \n");
                                int j;
                                start_wps_on_nbh_ap();
                                for  (j=0;j< num_bh_vaps ;j++)
                                {
                                son_send_action(drv, DISABLE_WPS, &event, ap_ifname[j],j);
                                }
                            }
                            else
                                printf("Local Push is active, cant honor request from root\n");
                            break;
                        }
                        else
                        {
                            printf(" I am not the rep to start_wps, propagating ROOT_INFORM_REP\n") ;
                            int i;
                            for(i=0; i<num_bh_vaps;i++)
                            {
                            son_send_action(drv, ROOT_INFORM_REP, &event,ap_ifname[i],i);
                            }
                            break;
                        }
                    break;
                    }

                case REP_INFORMED_ROOT_EV:
               if(is_root)
                        {
                            if(g_push_local!=1)
                         {
                            eloop_cancel_timeout(handle_nbh_status_timer, NULL, NULL);
                            eloop_register_timeout(WPS_NBH_STATUS_TIMER, 0, handle_nbh_status_timer,
                                           NULL, NULL);
                            push_active=1;
                            start_wps_on_nbh_ap();
                            start_action_in_root(drv);
                            break;
                        }
                            else
                                printf("Local Push is active, cant honor request from repeater\n");
                            break;
                        }

                        else
                        {
                            int i;
                            printf("I am not the root,Sending REP_INFORM_ROOT\n" );
                            for( i=0; i<num_bh_sta_vaps;i++)
                            {
                            son_send_action(drv, REP_INFORM_ROOT, &event,sta_ifname[i],i);
                            }
                            break;
                        }
                        break;


                case LISTEN_ONE_MIN_EV:
                {
                    wps_state = 1;
                    wps_probe_req_seen = 0;
                    printf("For a min, wps_state is 1,push active is 1, probe not seen!\n");
                    eloop_cancel_timeout(handle_nbh_status_timer, NULL, NULL);
                    eloop_register_timeout(WPS_NBH_STATUS_TIMER, 0, handle_nbh_status_timer,
                               NULL, NULL);
                    push_active = 1 ;
                    int i;
                    for( i=0;i<num_bh_vaps;i++)
                    {
                    son_send_action(drv, LISTEN_ONE_MIN, &event, ap_ifname[i],i);
                    }
                    break;
                }


                case STOP_WPS_EV:
                case DISABLE_WPS_EV:
                if (wps_state == 1)
                {
                    wps_state = 0;
                    int i;
                    printf("wps disabled at RE\n");
                    for(i=0; i<num_bh_vaps;i++)
                    {
                    son_send_action(drv, DISABLE_WPS, &event,ap_ifname[i],i);
                    }
                    break;
                }

                default:
                    break;
            }
        default:
            break;
    }

}
#if IFACE_SUPPORT_CFG80211
int
wps_create_cfg_socket(struct son_driver_data *drv)
{
    int ret = 0;
    drv->g_nl = (struct nlk80211_global *)os_malloc(sizeof(struct nlk80211_global));
    if (drv->g_nl == NULL) {
        goto err;
    }
    drv->g_nl->ctx = (void *) drv;
    drv->g_nl->do_process_driver_event = son_raw_receive;
    ret = nlk80211_create_socket(drv->g_nl);
    if (ret < 0) {
        goto err1;
    }
    return 1;
err1:
    os_free(drv->g_nl);
err:
    return 0;
}
#endif
static int sonwps_init_pipefd() {
    int err;
    int fd;

    unlink(WPS_SON_PIPE_PATH);
    err = mkfifo(WPS_SON_PIPE_PATH, 0666);
    if ((err == -1) && (errno != EEXIST)) {
        return -1;
    }

    fd = open(WPS_SON_PIPE_PATH, O_RDWR);
    if (fd == -1) {
        perror("open(pipe)");
        return -1;
    }

        //sonwps_reset_pipefd(drv);
    return fd;
}

int sonwps_reset_pipefd(struct son_driver_data *pData)
{
    if (pData->pipeFd > 0 )
    {
        eloop_unregister_read_sock(pData->pipeFd);
        close (pData->pipeFd);
    }

    pData->pipeFd = sonwps_init_pipefd();
    if (pData->pipeFd < 0) {
        printf("InitPipe");
        return -1;
    }
    printf("Reset pipe FD: %d\n", pData->pipeFd);
    eloop_register_read_sock(pData->pipeFd, pbc_sonwps_get_pipemsg, pData, NULL);

    return 0;
}

void pbc_sonwps_get_pipemsg(s32 fd, void *eloop_ctx, void *sock_ctx)
{
    char buf[256];
    char *pos;
    int  len;
    int  duration;

    struct son_driver_data *drv = (struct son_driver_data*)eloop_ctx;
    os_memset(buf, 0, sizeof(buf));

    len = read(fd, buf, sizeof(buf) -1 );
    if (len <= 0) {
        printf("pbcGetPipeMsgCB - read");
        sonwps_reset_pipefd(drv);
        return;
    }
    buf[len] = '\0';

    printf("Got event: %s\n", buf);
    if (strncmp(buf, "wps_pbc", 7) != 0)
    {
        printf("Reset pipe beacuse of Unknown event: %s\n", buf);
        sonwps_reset_pipefd(drv);
        return;
    }

    printf("PUSH BUTTON EVENT!\n");
    pos = buf + 7;
    duration = atoi(pos);
    printf("got duration: %d\n", duration);
    if (duration < 0)
        duration = 0;
    if (push_active == 0)
     {
        printf("wps activated\n");
        g_push_local = 1;
        push_active = 1;

        if(is_root)
        {
           start_action_in_root(drv);
        } else  { /* this is a repeater */
            /* start regular WPS for 1 min on BH vaps */

            /* start_wps should be done */
            eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
            eloop_register_timeout(WPS_NBHVAP_INTERVAL, 0, handle_nbh_local_timer,
                    drv, NULL);
            wps_success=0;
            sigusr_needed=1;
            start_wps_on_bh_ap();
            sigusr_needed=0;
        }
        /* common to root and repeater */

            if(num_nbh_vaps!=0)
            {
                /* will this handle_nbh_local_timer be called during cancel? */
                eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
                eloop_register_timeout(WPS_NBHVAP_INTERVAL, 0, handle_nbh_local_timer,
                        drv, NULL);
                wps_success=0;
                sigusr_needed=1;
                start_wps_on_nbh_ap();
                sigusr_needed=0;
            }


        return;
    }
    else { /* push is already active */
        printf(" Push already active IGNORING  \n");
        return;
    }
}

void sig_handler(int signal)
{
    printf("Received Signal:%d\n",signal);
    if(signal == SIGUSR1)
    {
        wps_success=1;
        printf("Received SIGUSR1 Signal\n");
    }

    /* check the status - reading a known file
       try to have iface name and success and failure in that file
       -- not needed today since this timer expires in 1 min
       */
    /* SIGUSR1 implies success */
    /* cancel the timer */
    eloop_cancel_timeout(handle_nbh_local_timer, drv, NULL);
    /* When PB in repeater, if  SIGUSR1 signal is obtained, implies an
       orbie client connected to its BH within a minute. So cancel the
       1 minute timer, release the local push and push active which were
       set when PB was pressed in repeater*/
    /* this flag reset is redundant for a root, as in root both 1 min and 2 min
       timer run on PB pressed, 2 min expiry handler resets it. */
    if(is_re)
    {
        g_push_local=0;
        push_active=0;
    }
    /* To ensure that rep's BH does not start wps when root's BH is doing wps*/
    /* other wps sessions - stop ? */

}

static void
son_wireless_event_wireless_custom(struct son_driver_data *drv,
                                       char *custom, char *end)
{

    if (strncmp(custom, "Manage.prob_req_wps ", 20) == 0)
    {
        uint32_t len = atoi(custom + 20);
        if (len < 0 || custom + MGMT_FRAM_TAG_SIZE + len > end)
        {
            printf("Invalid Manage.prob_req event length %d", len);
            return;
        }
        son_raw_receive(drv, NULL, (void *) custom + MGMT_FRAM_TAG_SIZE, len, -1);
    }
    else if (strncmp(custom, "Manage.action ", 14) == 0)
    {
        /* Format: "Manage.action <frame len>" | zero padding | frame
        */
        uint32_t len = atoi(custom + 14);
        if (len < 0 || custom + MGMT_FRAM_TAG_SIZE + len > end)
        {
            printf("Invalid Manage.action event length %d", len);
            return;
        }
        son_raw_receive(drv, NULL, (void *) custom + MGMT_FRAM_TAG_SIZE, len, -1);
    }
}

static void
son_wireless_event_wireless(struct son_driver_data *drv,
                                char *data, int len)
{
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    char *pos, *end, *custom, *buf;

    pos = data;
    end = data + len;


    while (pos + IW_EV_LCP_LEN <= end)
    {
        /* Event data may be unaligned, so make a local, aligned copy
                 * before processing. */
        memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        if (iwe->len <= IW_EV_LCP_LEN)
            return;

        custom = pos + IW_EV_POINT_LEN;
        if ((iwe->cmd == IWEVMICHAELMICFAILURE || iwe->cmd == IWEVASSOCREQIE ||
                     iwe->cmd == IWEVCUSTOM)) {
            /* WE-19 removed the pointer from struct iw_point */
            char *dpos = (char *) &iwe_buf.u.data.length;
            int dlen = dpos - (char *) &iwe_buf;
            memcpy(dpos, pos + IW_EV_LCP_LEN, sizeof(struct iw_event) - dlen);
        }
        else
        {
            memcpy(&iwe_buf, pos, sizeof(struct iw_event));
            custom += IW_EV_POINT_OFF;
        }

        switch (iwe->cmd) {
            case IWEVREGISTERED:
                break;
            case IWEVASSOCREQIE:
            case IWEVCUSTOM:
                if (custom + iwe->u.data.length > end)
                    return;
                buf = malloc(iwe->u.data.length + 1);
                if (buf == NULL)
                    return;         /* XXX */
                memcpy(buf, custom, iwe->u.data.length);
                buf[iwe->u.data.length] = '\0';
                son_wireless_event_wireless_custom(drv, buf, buf + iwe->u.data.length);
                free(buf);
                break;
        }

        pos += iwe->len;
    }
}



static void
son_wireless_event_rtm_newlink(void *ctx,
                                   struct ifinfomsg *ifi, u8 *buf, size_t len)
{
    struct son_driver_data *drv = ctx ;
    int attrlen, rta_len;
    struct rtattr *attr;


    attrlen = len;
    attr = (struct rtattr *) buf;

    rta_len = RTA_ALIGN(sizeof(struct rtattr));

    while (RTA_OK(attr, attrlen))
    {
        if (attr->rta_type == IFLA_WIRELESS)
        {
            son_wireless_event_wireless(drv, ((char *) attr) + rta_len, attr->rta_len - rta_len);
        }
        attr = RTA_NEXT(attr, attrlen);
    }
}



void son_main_function(void *conf_file, enum driver_mode drv_mode)
{
    struct netlink_config *cfg;
    struct ieee80211req_set_filter filt;
    const char *son_conf = (const char *)conf_file;
    FILE *file;
    int ret = EXIT_SUCCESS;
    int error=0;

    file = fopen(son_conf, "r");

    if ( file == 0 )
    {
        printf( "Could not open son config file\n" );
        error=1;
        goto out2;
    }

    drv=  os_zalloc(sizeof(*drv));

    if(!drv)
    {
        error=1;
        goto out2;
    }

    drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);

    if (drv->ioctl_sock < 0)
    {
        error=1;
        goto out1;
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
    signal(SIGUSR1, sig_handler) == SIG_ERR) {
    printf("Signal failed(errno ""%d %s)\n", errno, strerror(errno));
    ret = EXIT_FAILURE;
    error=1;
    goto out1;
    }
    cfg = os_zalloc(sizeof(*cfg));
    if (cfg == NULL)
    {
        error=1;
        printf("cfg file is NULL\n");
        goto out1;
    }
    cfg->ctx = drv;
    cfg->newlink_cb = son_wireless_event_rtm_newlink;
    drv->netlink = netlink_init(cfg);

#if IFACE_SUPPORT_CFG80211
    if (drv_mode == CFG80211_MODE) {
        ret = wps_create_cfg_socket(drv);
        if (!ret) {
            printf("Failed to create netlink socket\n");
            goto out1;
        }
    }
#endif

    if (drv->netlink == NULL)
    {
        printf("netlink init Error \n");
        os_free(cfg);
        error=1;
        goto out;
    }
    drv->pipeFd = sonwps_init_pipefd();
    if (drv->pipeFd < 0) {
        perror("InitPipe");
        error=1;
        goto out;
    }
    eloop_register_read_sock(drv->pipeFd, pbc_sonwps_get_pipemsg, drv, NULL);

    {
        int x;
        char mode[8];
        char ifname[16];
        char radio[16];;
        char macaddr[20];
        int retval = 0;
        while ((x = fgetc(file)) != EOF)
        {
            retval = fscanf(file, "%s %s %s %s", mode, ifname, radio, macaddr);
            if (retval < 4) {
                   break;
            }
            if(strcmp(mode,"sta") == 0)
            {
                memcpy(&sta_ifname[num_bh_sta_vaps],ifname,sizeof(ifname));
                if (&sta_ifname[num_bh_sta_vaps] == NULL)
                    goto out;
                memcpy(&sta_radio[num_bh_sta_vaps],radio,sizeof(radio));
                if (&sta_radio[num_bh_sta_vaps] == NULL)
                    goto out;
                memset(&sta_macaddr[num_bh_sta_vaps],0,sizeof(macaddr));
                memcpy(&sta_macaddr[num_bh_sta_vaps],macaddr,sizeof(macaddr));
                if (&sta_macaddr[num_bh_sta_vaps] == NULL)
                    goto out;
                memset(&bh_sta_macaddr_hex[num_bh_sta_vaps], 0, sizeof(macaddr));
                tmp_bh_sta_macaddr_hex= (char *) &(bh_sta_macaddr_hex[num_bh_sta_vaps]);
                ret = copy_macaddr_hex(tmp_bh_sta_macaddr_hex, macaddr);
                if (ret == -1)
                {
                    fclose(file);
                    goto out;
                }
                mac_printf(bh_sta_macaddr_hex[num_bh_sta_vaps]);
                num_bh_sta_vaps++;
            }
            if(strcmp(mode,"ap") == 0)
               
            {
                memset(&sta_ifname[num_bh_sta_vaps],0, sizeof(ifname));
                memcpy(&ap_ifname[num_bh_vaps], ifname, sizeof(ifname));
                if (&ap_ifname[num_bh_vaps] == NULL)
                    goto out;
                memcpy(&ap_radio[num_bh_vaps], radio, sizeof(radio));
                if (&ap_radio[num_bh_vaps] == NULL)
                    goto out;
                memset(&ap_macaddr[num_bh_vaps], 0, sizeof(macaddr));
                memcpy(&ap_macaddr[num_bh_vaps], macaddr, sizeof(macaddr));
                if (&ap_macaddr[num_bh_vaps] == NULL)
                    goto out;
                tmp_ap_macaddr_hex= (char *) &(ap_macaddr_hex[num_bh_vaps]);
                ret = copy_macaddr_hex(tmp_ap_macaddr_hex, macaddr);
                if (ret == -1)
                {
                    fclose(file);
                    goto out;
                }
                mac_printf(ap_macaddr_hex[num_bh_vaps]);
                num_bh_vaps++;

            }
            if(strcmp(mode,"nbh_ap") == 0)
            {
                memset(&ap_ifname[num_bh_vaps],0, sizeof(ifname));
                memcpy(&nbh_apiface[num_nbh_vaps], ifname, sizeof(ifname));
                if (&nbh_apiface[num_nbh_vaps] == NULL)
                    goto out;
                memcpy(nbh_radio[num_nbh_vaps], radio, sizeof(radio));
                if (&nbh_radio[num_nbh_vaps] == NULL)
                    goto out;
                memset(nbh_macaddr[num_nbh_vaps], 0, sizeof(macaddr));
                memcpy(nbh_macaddr[num_nbh_vaps], macaddr, sizeof(macaddr));
                if (nbh_macaddr[num_nbh_vaps] == NULL)
                    goto out;
                num_nbh_vaps ++;
            }

        }
        fclose(file);
        if (num_bh_sta_vaps!= 0)
            is_re = 1;
        else
            is_root = 1;
    }

    if (is_re == 0 && is_root == 0)
    {
        error=1;
        goto out;
    }

    if (is_re) {
        printf("Its a Repeater!\n");

        // On stavaps filter needed for sta to forward frames to application from driver
        int i;
        for (i=0;i<num_bh_sta_vaps;i++)
        {
            filt.app_filterype = IEEE80211_FILTER_TYPE_ACTION;
            set80211priv(drv, IEEE80211_IOCTL_FILTERFRAME, &filt, sizeof(struct ieee80211req_set_filter),sta_ifname[i]);
        }
    }

if(error==1)
{
out:
#if IFACE_SUPPORT_CFG80211
    if (drv->g_nl) {
        nlk80211_close_socket(drv->g_nl);
    }
#endif
out1:
    os_free(drv);
out2:
    printf("son: Exiting %s\n", __func__);
}
}
