#ifndef _WRAPD_API__H
#define _WRAPD_API__H
#include "cfg80211_cmn.h"

#include <pthread.h>
#if 1
#define wrapd_printf(fmt, args...) do { \
        printf("wrapd: %s: %d: " fmt "\n", __func__, __LINE__, ## args); \
} while (0)
#else
#define wrapd_printf(args...) do { } while (0)
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#define MAC2STR_ADDR(a) &(a)[0], &(a)[1], &(a)[2], &(a)[3], &(a)[4], &(a)[5]

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/*
 * RADIO_CNT can be 2 for DBDC qwrap case
 * vaps on 2G and 5G radios
 */
#define RADIO_CNT     2

#define HOSTAPD_CONN_TIMES      3
#define WPA_S_CONN_TIMES        3


extern const char *hostapd_ctrl_iface_dir;
extern const char *wpa_s_ctrl_iface_dir;
extern const char *wrapd_ctrl_iface_dir;

extern const char *wrapd_ctrl_iface_path;
extern const char *global_wpa_s_ctrl_iface_path;
extern const char *global_hostapd_ctrl_iface_path;

struct wrapd_global {
    struct wpa_ctrl *global_wpa_supp;
    struct wpa_ctrl *global_hostapd;
    struct wrapd_ctrl *global_wrapd;
    struct wrap_wifi_iface *wrap_wifi_list;
    struct wrap_wifi_iface *max_aptr;
    struct netlink_data *netlink;
    int do_timer;
    int in_timer;
    int ioctl_sock;
    int wrapd_psta_config;
    int enable_proxysta_addition;
    pthread_t wrap_main_func;
    pthread_t wrap_cli_sock;
    char *brname;
    int wifi_count;
    int auto_wired_started;
    char ethernet_extender[IFNAMSIZ];
    char wrapd_ctrl_intf[128];
    u8 wifiX_dev_addr[6];
    struct sock_context sock;
    char dbglvl[IFNAMSIZ];
    char dbglvl_high[IFNAMSIZ];
};

extern  struct wrapd_global *wrapg;
typedef void wrapd_hdl_t;

typedef unsigned char mac_addr_t[ETH_ALEN];
typedef unsigned int mac_addr_int_t[ETH_ALEN];

typedef enum {
WRAPD_STATUS_OK = 0,
WRAPD_STATUS_ETIMEDOUT,
WRAPD_STATUS_FAILED,
WRAPD_STATUS_BAD_ARG,
WRAPD_STATUS_IN_USE,
WRAPD_STATUS_ARG_TOO_BIG,
} wrapd_status_t;

typedef enum {
   PROXY_STA_LIMIT_EXCEEDED = -2,
   PROXY_STA_CREATION_ERROR,
   PROXY_STA_SUCCESS,
} psta_status_t;

struct wrapd_global*
wrapd_init_global(const char *global_supp, const char *global_hostapd, const char *global_wrapd, char *brname, int timer);
void
wrapd_global_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx);
void wrapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx);
void wrapd_hostapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx);
void wrapd_wpa_s_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx);
void wrapd_load_vma_list(const char *conf_file, wrapd_hdl_t *handle);
int wrapd_store_ifaces();
void cleanup_ifaces();
int ifconfig_up(const char *if_name);
int ifconfig_down(const char *if_name);
int wifi_ifname_to_num(const char *wifi_ifname);
struct wrapd_ctrl* wrapd_ctrl_open(const char *ctrl_iface, wrapd_hdl_t *handle);
void start_wired_client_addition();
wrapd_status_t wrapd_psta_if_remove(wrapd_hdl_t *mctx, char *ifname);
void wrapd_disconn_all(struct wrap_wifi_iface *aptr);
int wrapd_remove_to_global(struct wrap_wifi_iface *aptr, const char *ifname);

#define IS_PSTA_MY_CHILD(parent, psta) ((psta.parent_ap) && (psta.parent_ap->parent_wifi == parent))
#define IS_PSTA_FOSTERED(psta)  ((psta.parent_ap) && (psta.parent_ap->parent_wifi != psta.parent_ap->aptr))
#define IS_PSTA_WIRED(psta) (psta.flags & WRAPD_PSTA_FLAG_WIRED)
#define IS_PSTA_MOVABLE(other, psta) (other->mpsta_conn ? (IS_PSTA_FOSTERED(other->psta[i]) || IS_PSTA_WIRED(other->psta[i])) : (1))

#define IS_AP_MY_CHILD(parent, conn) (conn->parent_wifi == parent)
#define IS_AP_FOSTERED(conn) (conn->parent_wifi != conn->aptr)
#endif //_WRAPD_API__H
