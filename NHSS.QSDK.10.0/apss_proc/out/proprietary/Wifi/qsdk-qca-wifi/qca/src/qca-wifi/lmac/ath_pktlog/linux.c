/*
 * Copyright (c) 2013, 2017-2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2000-2007, Atheros Communications Inc.
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

/* 
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */ 

#ifndef REMOVE_PKT_LOG

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * Atheros packet log module (Linux-specific code)
 */
#include <osdep.h>

#include <ath_dev.h>

#define PKTLOG_INC 1
#include "ath_internal.h"
#undef PKTLOG_INC

#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <ieee80211_var.h>

#include "pktlog_i.h"
#include "pktlog_rc.h"
#include "ah_pktlog.h"
#include "if_llc.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef __MOD_INC_USE_COUNT
#define	PKTLOG_MOD_INC_USE_COUNT					\
	if (!try_module_get(THIS_MODULE)) {					\
		qdf_err(KERN_WARNING "try_module_get failed\n"); \
	}

#define	PKTLOG_MOD_DEC_USE_COUNT	module_put(THIS_MODULE)
#else
#define	PKTLOG_MOD_INC_USE_COUNT	MOD_INC_USE_COUNT
#define	PKTLOG_MOD_DEC_USE_COUNT	MOD_DEC_USE_COUNT
#endif

#define PKTLOG_SYSCTL_SIZE      20

/* Permissions for creating proc entries */
#define PKTLOG_PROC_PERM        0444
#define PKTLOG_PROCSYS_DIR_PERM 0555
#define PKTLOG_PROCSYS_PERM     0644

#define PKTLOG_DEVNAME_SIZE 32

#define PKTLOG_PROC_DIR "ath_pktlog"
#define PKTLOG_PROC_SYSTEM "system"

#define WLANDEV_BASENAME "wifi"
#define MAX_WLANDEV 10

#define PKTLOG_TAG "ATH_PKTLOG" 

#define TCP_HDR_MIN_LEN 20
#define TCP_SACK_MIN_LEN 10

/* TCP option type */
#define TCPOPT_NOP 1 /* Padding */
#define TCPOPT_SACK 5 /* SACK block */

/*
 * Linux specific pktlog state information
 */
struct ath_pktlog_info_lnx {
    struct ath_pktlog_info info;
    struct ctl_table sysctls[PKTLOG_SYSCTL_SIZE];
    struct proc_dir_entry *proc_entry;
    struct ctl_table_header *sysctl_header;
};

#define PL_INFO_LNX(_pl_info)   ((struct ath_pktlog_info_lnx *)(_pl_info))

static struct proc_dir_entry *g_pktlog_pde;

static int pktlog_attach(ath_generic_softc_handle scn);
static void pktlog_detach(ath_generic_softc_handle scn);
static int pktlog_open(struct inode *i, struct file *f);
static int pktlog_release(struct inode *i, struct file *f);
static int pktlog_mmap(struct file *f, struct vm_area_struct *vma);
static ssize_t pktlog_read(struct file *file, char *buf, size_t nbytes,
                           loff_t * ppos);
static void pktlog_get_scn(osdev_t dev,
                    ath_generic_softc_handle *scn);
static void pktlog_get_dev_name(osdev_t dev, ath_generic_softc_handle scn);

static struct ath_pktlog_funcs g_exported_pktlog_funcs = {
    .pktlog_attach = pktlog_attach, /* Used for other platforms */
    .pktlog_detach = pktlog_detach,
    .pktlog_txctl = pktlog_txctl,
    .pktlog_txstatus = pktlog_txstatus,
    .pktlog_rx = pktlog_rx,
    .pktlog_text = pktlog_text,
    .pktlog_start = pktlog_start,
    .pktlog_readhdr = pktlog_read_hdr,
    .pktlog_readbuf = pktlog_read_buf,
    .pktlog_get_scn = pktlog_get_scn,
    .pktlog_get_dev_name = pktlog_get_dev_name,
};

struct pktlog_handle_t pktlog_handle_dev;
//set_pktlog_handle_funcs(g_exported_pktlog_funcs)

/*static struct ath_generic_softc {
    struct ieee80211com sc_ic;
    struct pktlog_handle_t *pl_dev;
};*/

#define get_scn(_dev)           \
        ath_netdev_priv(_dev)

static void
pktlog_get_dev_name(osdev_t dev,
        ath_generic_softc_handle scn)
{
    struct pktlog_handle_t *pl_dev;
    if ((pl_dev = get_pl_handle(scn))) {
        pl_dev->name = dev->netdev->name; 
    }
}

static void
pktlog_get_scn(osdev_t dev,
        ath_generic_softc_handle *scn)
{
    *scn = (ath_generic_softc_handle) get_scn(dev->netdev);
}



static struct pktlog_handle_t *get_pl_handle_dev(struct net_device *dev)
{
    struct pktlog_handle_t *pl_dev;
    ath_generic_softc_handle scn = get_scn(dev);
    if((pl_dev = get_pl_handle(scn))) {
        return pl_dev;
    }
    return NULL; 
}

static struct ath_pktlog_rcfuncs g_exported_pktlog_rcfuncs = {
    .pktlog_rcupdate = pktlog_rcupdate,
    .pktlog_rcfind = pktlog_rcfind
};

static struct file_operations pktlog_fops = {
    open:pktlog_open,
    release:pktlog_release,
    mmap:pktlog_mmap,
    read:pktlog_read,
};

extern struct ath_softc *ath_get_softc(struct net_device *dev);
extern struct ath_pktlog_info *g_pktlog_info;
extern void osif_pktlog_log_ani_callback_register(void *pktlog_ani);

static int
ATH_SYSCTL_DECL(ath_sysctl_pktlog_filter_mac, ctl, write, filp, buffer, lenp,
                ppos)
{
    int ret;
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)ctl->extra1;
    ath_generic_softc_handle scn = (pl_dev == NULL) ? NULL : pl_dev->scn;

    static char str[20];
    static char macaddr[IEEE80211_ADDR_LEN];
    ctl->data = &str[0];
    ctl->maxlen = sizeof(str);

    if (pl_dev == NULL) {
        qdf_err("Invalid Pointer\n");
        return -1;
    }

    if (write) {
       ret = proc_dostring(ctl, write, buffer, lenp, ppos);
        if (ret == 0) {
                qdf_info("pl_dev:%pK; pl_funcs:%pK; pktlog_filter_mac:%pK; scn:%pK\n",
                                        (void*)pl_dev, (void*)pl_dev->pl_funcs,
                                        (void*)pl_dev->pl_funcs->pktlog_filter_mac,
                                        (void*)pl_dev->scn);
                sscanf( str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%*c",
                    &macaddr[0], &macaddr[1], &macaddr[2],
                    &macaddr[3], &macaddr[4], &macaddr[5] );
                return pl_dev->pl_funcs->pktlog_filter_mac(scn, &macaddr[0]);
        } else {
            qdf_err(PKTLOG_TAG "%s:proc_dointvec failed\n", __FUNCTION__);
        }
    } else {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
        if (ret)
            qdf_err(PKTLOG_TAG "%s:proc_dointvec failed\n", __FUNCTION__);
    }
    return ret;
}

static int
ATH_SYSCTL_DECL(ath_sysctl_pktlog_enable, ctl, write, filp, buffer, lenp,
                ppos)
{
    int ret;
    static int enable;
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)ctl->extra1;
    ath_generic_softc_handle scn = (pl_dev == NULL) ? NULL : pl_dev->scn;

    ctl->data = &enable;
    ctl->maxlen = sizeof(enable);

    if (pl_dev && ((pl_dev->pl_info->log_state) & ATH_PKTLOG_TX_CAPTURE_ENABLE)) {
        /*
         * Wlan F/W doesnt allow the TX_CAPTURE to be enabled with other
         * pktlog flags.
         */
        qdf_err("Cannot enable tx capture : Check the flag value 0x%x\n", (pl_dev->pl_info->log_state));
        return -1;
    }

    if (write) {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
        if (ret == 0) {
            qdf_info("Enable in %s:%d\n", __FUNCTION__, enable);
            if (pl_dev && (((pl_dev->pl_info->log_state) & ATH_PKTLOG_TX_CAPTURE_ENABLE) == 0)) {
                qdf_info("pl_dev:%pK; pl_funcs:%pK; pktlog_enable:%pK; scn:%pK\n",
                                        (void*)pl_dev, (void*)pl_dev->pl_funcs,
                                        (void*)pl_dev->pl_funcs->pktlog_enable,
                                        (void*)pl_dev->scn);
#ifdef OL_ATH_SMART_LOGGING
               if (pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block &&
                    (pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block(scn,
                            enable, false) != QDF_STATUS_SUCCESS)) {
                    /* Details will be printed by the API we called, but using
                     * Smart Log internal error logging framework.
                     */
                    return -1;
                }
#endif /* OL_ATH_SMART_LOGGING */

                ret = pl_dev->pl_funcs->pktlog_enable(scn, enable);

#ifdef OL_ATH_SMART_LOGGING
                if (!enable && pl_dev->pl_sl_funcs.smart_log_fw_pktlog_unblock)
                    pl_dev->pl_sl_funcs.smart_log_fw_pktlog_unblock(scn);
#endif /* OL_ATH_SMART_LOGGING */

                return ret;
            } else {
                 qdf_err("Cannot enable pktlog as Tx Capture has been enabled\n");
            }
        } else {
            qdf_err(PKTLOG_TAG "%s:proc_dointvec failed\n", __FUNCTION__);
        }
    } else {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
        if (ret)
            qdf_err(PKTLOG_TAG "%s:proc_dointvec failed\n", __FUNCTION__);
    }
    return ret;
}

static int
ATH_SYSCTL_DECL(ath_sysctl_pktlog_size, ctl, write, filp, buffer, lenp,
                ppos)
{
    int ret;
    static int size;
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)ctl->extra1;
    ath_generic_softc_handle scn = (pl_dev == NULL) ? NULL : pl_dev->scn;

    ctl->data = &size;
    ctl->maxlen = sizeof(size);

    if (write) {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
        if (ret == 0)
                return pktlog_setsize(scn, size);
    } else {
        size = get_pktlog_bufsize(pl_dev);
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
    }
    return ret;
}

static int
ATH_SYSCTL_DECL(ath_sysctl_pktlog_reset_buffer, ctl, write, filp, buffer, lenp,
                ppos)
{
    int ret;
    static int reset;
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)ctl->extra1;
    ath_generic_softc_handle scn = (pl_dev == NULL) ? NULL : pl_dev->scn;

    ctl->data = &reset;
    ctl->maxlen = sizeof(reset);

    if (write) {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
        if (ret == 0)
                return pktlog_reset_buffer(scn, reset);
    } else {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                                       lenp, ppos);
    }
    return ret;
}

static int
ATH_SYSCTL_DECL(ath_sysctl_pktlog_remote_port, ctl, write, filp, buffer, lenp,
        ppos)
{

    int ret = 0;
    static int port = 0;
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)ctl->extra1;
    struct ath_pktlog_info *pl_info = NULL;

    ctl->data = &port;
    ctl->maxlen = sizeof(port);

    if (write) {
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                lenp, ppos);
        if (ret == 0) {
            if (pl_dev){
                pl_info = pl_dev->pl_info;
                pl_info->remote_port = port;
            }
        }
    } else {
        if (pl_dev){
            pl_info = pl_dev->pl_info;
            port = pl_info->remote_port;
        }
        ret = ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,
                lenp, ppos);
    }

    return ret;
}

static int
pktlog_sysctl_register(ath_generic_softc_handle scn)
{
    struct pktlog_handle_t *pl_dev = get_pl_handle(scn);
    struct ath_pktlog_info_lnx *pl_info_lnx;
    char *proc_name;

    if (pl_dev) {
        pl_info_lnx = PL_INFO_LNX(pl_dev->pl_info);
        proc_name = pl_dev->name;
    } else {
        pl_info_lnx = PL_INFO_LNX(g_pktlog_info);
        proc_name = PKTLOG_PROC_SYSTEM;
    }

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31))
#	define set_ctl_name(a, b)	/* nothing */
#else
#	define set_ctl_name(a, b)	pl_info_lnx->sysctls[a].ctl_name = b
#endif

    if (!pl_info_lnx) {
        return -1;
    }
    /*
     * Setup the sysctl table for creating the following sysctl entries:
     * /proc/sys/PKTLOG_PROC_DIR/<adapter>/enable for enabling/disabling pktlog
     * /proc/sys/PKTLOG_PROC_DIR/<adapter>/size for changing the buffer size
     */
    memset(pl_info_lnx->sysctls, 0, sizeof(pl_info_lnx->sysctls));
    set_ctl_name(0, CTL_AUTO);
    pl_info_lnx->sysctls[0].procname = PKTLOG_PROC_DIR;
    pl_info_lnx->sysctls[0].mode = PKTLOG_PROCSYS_DIR_PERM;
    pl_info_lnx->sysctls[0].child = &pl_info_lnx->sysctls[2];
    /* [1] is NULL terminator */
    set_ctl_name(2, CTL_AUTO);
    pl_info_lnx->sysctls[2].procname = proc_name;
    pl_info_lnx->sysctls[2].mode = PKTLOG_PROCSYS_DIR_PERM;
    pl_info_lnx->sysctls[2].child = &pl_info_lnx->sysctls[4];
    /* [3] is NULL terminator */
    set_ctl_name(4, CTL_AUTO);
    pl_info_lnx->sysctls[4].procname = "enable";
    pl_info_lnx->sysctls[4].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[4].proc_handler = ath_sysctl_pktlog_enable;
    pl_info_lnx->sysctls[4].extra1 = pl_dev;

    set_ctl_name(5, CTL_AUTO);
    pl_info_lnx->sysctls[5].procname = "size";
    pl_info_lnx->sysctls[5].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[5].proc_handler = ath_sysctl_pktlog_size;
    pl_info_lnx->sysctls[5].extra1 = pl_dev;

    set_ctl_name(6, CTL_AUTO);
    pl_info_lnx->sysctls[6].procname = "options";
    pl_info_lnx->sysctls[6].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[6].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[6].data = &pl_info_lnx->info.options;
    pl_info_lnx->sysctls[6].maxlen = sizeof(pl_info_lnx->info.options);

    set_ctl_name(7, CTL_AUTO);
    pl_info_lnx->sysctls[7].procname = "sack_thr";
    pl_info_lnx->sysctls[7].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[7].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[7].data = &pl_info_lnx->info.sack_thr;
    pl_info_lnx->sysctls[7].maxlen = sizeof(pl_info_lnx->info.sack_thr);

    set_ctl_name(8, CTL_AUTO);
    pl_info_lnx->sysctls[8].procname = "tail_length";
    pl_info_lnx->sysctls[8].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[8].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[8].data = &pl_info_lnx->info.tail_length;
    pl_info_lnx->sysctls[8].maxlen = sizeof(pl_info_lnx->info.tail_length);

    set_ctl_name(9, CTL_AUTO);
    pl_info_lnx->sysctls[9].procname = "thruput_thresh";
    pl_info_lnx->sysctls[9].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[9].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[9].data = &pl_info_lnx->info.thruput_thresh;
    pl_info_lnx->sysctls[9].maxlen = sizeof(pl_info_lnx->info.thruput_thresh);

    set_ctl_name(10, CTL_AUTO);
    pl_info_lnx->sysctls[10].procname = "phyerr_thresh";
    pl_info_lnx->sysctls[10].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[10].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[10].data = &pl_info_lnx->info.phyerr_thresh;
    pl_info_lnx->sysctls[10].maxlen = sizeof(pl_info_lnx->info.phyerr_thresh);

    set_ctl_name(11, CTL_AUTO);
    pl_info_lnx->sysctls[11].procname = "per_thresh";
    pl_info_lnx->sysctls[11].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[11].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[11].data = &pl_info_lnx->info.per_thresh;
    pl_info_lnx->sysctls[11].maxlen = sizeof(pl_info_lnx->info.per_thresh);

    set_ctl_name(12, CTL_AUTO);
    pl_info_lnx->sysctls[12].procname = "trigger_interval";
    pl_info_lnx->sysctls[12].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[12].proc_handler = proc_dointvec;
    pl_info_lnx->sysctls[12].data = &pl_info_lnx->info.trigger_interval;
    pl_info_lnx->sysctls[12].maxlen = sizeof(pl_info_lnx->info.trigger_interval);

    set_ctl_name(13, CTL_AUTO);
    pl_info_lnx->sysctls[13].procname = "reset_buffer";
    pl_info_lnx->sysctls[13].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[13].proc_handler = ath_sysctl_pktlog_reset_buffer;
    pl_info_lnx->sysctls[13].extra1 = pl_dev;

    set_ctl_name(14, CTL_AUTO);
    pl_info_lnx->sysctls[14].procname = "remote_port";
    pl_info_lnx->sysctls[14].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[14].proc_handler = ath_sysctl_pktlog_remote_port;
    pl_info_lnx->sysctls[14].extra1 = pl_dev;

    set_ctl_name(15, CTL_AUTO);
    pl_info_lnx->sysctls[15].procname = "mac";
    pl_info_lnx->sysctls[15].mode = PKTLOG_PROCSYS_PERM;
    pl_info_lnx->sysctls[15].proc_handler = ath_sysctl_pktlog_filter_mac;
    pl_info_lnx->sysctls[15].extra1 = pl_dev;

    /* [14] is NULL terminator */

    /* and register everything */
    /* register_sysctl_table changed from 2.6.21 onwards */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20))
    pl_info_lnx->sysctl_header = register_sysctl_table(pl_info_lnx->sysctls);
#else
    pl_info_lnx->sysctl_header = register_sysctl_table(pl_info_lnx->sysctls, 1);
#endif
    if (!pl_info_lnx->sysctl_header) {
        qdf_err("%s: failed to register sysctls!\n", proc_name);
        return -1;
    }

    return 0;
}

static void
pktlog_sysctl_unregister(struct pktlog_handle_t *pl_dev)
{
    struct ath_pktlog_info_lnx *pl_info_lnx = (pl_dev) ?
        PL_INFO_LNX(pl_dev->pl_info) : PL_INFO_LNX(g_pktlog_info);

    if (pl_info_lnx->sysctl_header) {
        unregister_sysctl_table(pl_info_lnx->sysctl_header);
        pl_info_lnx->sysctl_header = NULL;
    }
}

static int __init
pktlogmod_init(void)
{
    int i;
    char dev_name[PKTLOG_DEVNAME_SIZE];
    struct net_device *dev;
    int ret;
    struct pktlog_handle_t *pl_dev;

    /* Init system-wide logging */
    g_pktlog_pde = proc_mkdir(PKTLOG_PROC_DIR, NULL);

    if (g_pktlog_pde == NULL) {
        qdf_err(PKTLOG_TAG "%s: proc_mkdir failed\n", __FUNCTION__);
        return -1;
    }
    if ((ret = pktlog_attach(NULL)))
        goto init_fail1;

    /* Init per-adapter logging */
    for (i = 0; i < MAX_WLANDEV; i++) {
        snprintf(dev_name, sizeof(dev_name), WLANDEV_BASENAME "%d", i);
        //snprintf(dev_name, sizeof(dev_name), "wifi-sim0");

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23))
        if ((dev = dev_get_by_name(&init_net,dev_name)) != NULL) {
#else
        if ((dev = dev_get_by_name(dev_name)) != NULL) {
#endif
            //dev = dev_get_by_name(&init_net,dev_name);
            //struct ath_softc *sc = ath_get_softc(dev);
            pl_dev = get_pl_handle_dev(dev);
            //if (pktlog_attach(sc))
            if (!pl_dev || pktlog_attach(get_scn(dev)))
                qdf_err(PKTLOG_TAG "%s: pktlog_attach failed for %s\n",
                       __FUNCTION__, dev_name);
            dev_put(dev);
        }
    }

    g_pktlog_funcs = &g_exported_pktlog_funcs;
    osif_pktlog_log_ani_callback_register((void*)&pktlog_ani);
    g_pktlog_rcfuncs = &g_exported_pktlog_rcfuncs;
#if ATH_PERF_PWR_OFFLOAD
    g_ol_pl_os_dep_funcs =  (struct ol_pl_os_dep_funcs *)&g_exported_pktlog_funcs;
#endif
    return 0;

init_fail1:
    remove_proc_entry(PKTLOG_PROC_DIR, NULL);
    return ret;
}
module_init(pktlogmod_init);

static void __exit
pktlogmod_exit(void)
{
    int i;
    char dev_name[PKTLOG_DEVNAME_SIZE];
    struct net_device *dev;

    g_pktlog_funcs = NULL;
    osif_pktlog_log_ani_callback_register(NULL);
    g_pktlog_rcfuncs = NULL;
#if ATH_PERF_PWR_OFFLOAD
    g_ol_pl_os_dep_funcs = NULL;
#endif

    for (i = 0; i < MAX_WLANDEV; i++) {
        //snprintf(dev_name, sizeof(dev_name), "wifi-sim0");
        snprintf(dev_name, sizeof(dev_name), WLANDEV_BASENAME "%d", i);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23))
        if ((dev = dev_get_by_name(&init_net,dev_name)) != NULL) {
#else
        if ((dev = dev_get_by_name(dev_name)) != NULL) {
#endif
            ath_generic_softc_handle scn = get_scn(dev);
            struct pktlog_handle_t *pl_dev = get_pl_handle_dev(dev);
            /* 
             *  Disable firmware side pktlog function
             */
#ifdef OL_ATH_SMART_LOGGING
            /*
             * We call smart_log_fw_pktlog_stop_and_block() without checking if
             * pl_dev->tgt_pktlog_enabled is true, since
             * smart_log_fw_pktlog_stop_and_block() will internally take care of
             * checking whether a FW initiated packetlog is in progress, and
             * since we need to call this function to block any future FW
             * requests for starting packetlog irrespective of whether
             * pl_dev->tgt_pktlog_enabled is true or not.
             */
            if (pl_dev &&
                 pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block &&
                 (pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block(scn, 0,
                            false) != QDF_STATUS_SUCCESS)) {
                qdf_err("Call to smart_log_fw_pktlog_stop_and_block() "
                        "unexpectedly failed. But proceeding with module "
                        "unload. Investigate!");
             }
            /*
             * There won't be a later call to unblock FW initiated packetlog,
             * since the packetlog module is exiting.
             */
#endif /* OL_ATH_SMART_LOGGING */

            if(pl_dev && pl_dev->tgt_pktlog_enabled) {
                if(pl_dev->pl_funcs) {
                   if(pl_dev->pl_funcs->pktlog_enable(scn, 0)) {
                        qdf_err("Unable to Disable pktlog in the target\n");
                    }
                    /* detach for offload */
                    pl_dev->pl_funcs->pktlog_deinit(scn);
                }
            }
            if(pl_dev)
                pktlog_detach(scn);

            /*
             *  pdev kill needs to be implemented
             */
            dev_put(dev);
        }
    }

    pktlog_detach(NULL);
    remove_proc_entry(PKTLOG_PROC_DIR, NULL);
}
module_exit(pktlogmod_exit);

/*
 * Initialize logging for system or adapter
 * Parameter sc should be NULL for system wide logging
 */
static int
pktlog_attach(ath_generic_softc_handle scn)
{
    struct pktlog_handle_t *pl_dev;
    struct ath_pktlog_info_lnx *pl_info_lnx;
    char *proc_name;
    struct proc_dir_entry *proc_entry;

    pl_dev = get_pl_handle(scn);

    if (pl_dev != NULL) {
        pl_info_lnx = qdf_mem_malloc(sizeof(*pl_info_lnx));
        if (pl_info_lnx == NULL) {
            qdf_err(PKTLOG_TAG "%s:allocation failed for pl_info\n"
                                                    , __FUNCTION__);
            return -ENOMEM;
        }
        pl_dev->pl_info = &pl_info_lnx->info;
        proc_name = pl_dev->name;
        if (!pl_dev->pl_funcs) {
            pl_dev->pl_funcs = &pl_funcs;
        }

        /*
         *  Valid for both direct attach and offload architecture
         */
        pl_dev->pl_funcs->pktlog_init(scn);
    } else {
        if (g_pktlog_info == NULL) {
            // allocate the global log structure
            pl_info_lnx = qdf_mem_malloc(sizeof(*pl_info_lnx));
            if (pl_info_lnx == NULL) {
                qdf_err(PKTLOG_TAG "%s:allocation failed for pl_info\n", __FUNCTION__);
                return -ENOMEM;
            }
            g_pktlog_info = &pl_info_lnx->info;
        } else {
            pl_info_lnx = PL_INFO_LNX(g_pktlog_info);
        }
        proc_name = PKTLOG_PROC_SYSTEM;
        /*
         * pktlog_init is valid only for the direct attach path
         */
        pktlog_init(scn);
    }

    /*
     * initialize log info
     * might be good to move to pktlog_init
     */
    //pl_dev->tgt_pktlog_enabled = false;
    pl_info_lnx->proc_entry = NULL;
    pl_info_lnx->sysctl_header = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    proc_entry = proc_create_data(proc_name, PKTLOG_PROC_PERM, g_pktlog_pde,
                                  &pktlog_fops, &pl_info_lnx->info);
#else
    proc_entry = create_proc_entry(proc_name, PKTLOG_PROC_PERM, g_pktlog_pde);
#endif
    if (proc_entry == NULL) {
        qdf_err(PKTLOG_TAG "%s: create_proc_entry failed for %s\n",
               __FUNCTION__, proc_name);
        goto attach_fail1;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
    proc_entry->data = &pl_info_lnx->info;
    proc_entry->owner = THIS_MODULE;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    proc_entry->proc_fops = &pktlog_fops;
#endif
    pl_info_lnx->proc_entry = proc_entry;

    if (pktlog_sysctl_register(scn)) {
        qdf_err(PKTLOG_TAG "%s: sysctl register failed for %s\n",
               __FUNCTION__, proc_name);
        goto attach_fail2;
    }
    return 0;

attach_fail2:
    remove_proc_entry(proc_name, g_pktlog_pde);
attach_fail1:
    if(pl_dev)
        qdf_mem_free(pl_dev->pl_info);
    return -1;
    return 0;
}

static void
pktlog_detach(ath_generic_softc_handle scn)
{
    struct pktlog_handle_t *pl_dev = (struct pktlog_handle_t *)
                                                get_pl_handle(scn);
    struct ath_pktlog_info *pl_info = pl_dev ? pl_dev->pl_info : g_pktlog_info;

    if (pl_dev) {
        remove_proc_entry(pl_dev->name, g_pktlog_pde);
    } else {
        remove_proc_entry(PKTLOG_PROC_SYSTEM, g_pktlog_pde);
    }

    pktlog_sysctl_unregister(pl_dev);

    qdf_spin_lock(&pl_info->log_lock);
    if (pl_info->buf)
        pktlog_release_buf(pl_info);
    qdf_spin_unlock(&pl_info->log_lock);
    pktlog_cleanup(pl_info);

    if (pl_dev) {
        qdf_mem_free(pl_info);
        pl_dev->pl_info = NULL;
        if (!is_mode_offload(scn))
            pl_dev->pl_funcs = NULL;
    } else {
        qdf_mem_free(g_pktlog_info);
        g_pktlog_info = NULL;
    }

    return;
}

static int pktlog_open(struct inode *i, struct file *f)
{
    PKTLOG_MOD_INC_USE_COUNT;
    return 0;
}


static int pktlog_release(struct inode *i, struct file *f)
{
    PKTLOG_MOD_DEC_USE_COUNT;
    return 0;
}

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

static ssize_t
__pktlog_read(struct file *file, char *buf, size_t nbytes, loff_t *ppos)
{
    size_t bufhdr_size;
    size_t count = 0, ret_val = 0;
    int rem_len;
    int start_offset, end_offset;
    int fold_offset, ppos_data, cur_rd_offset;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct proc_dir_entry *proc_entry = PDE(file->f_dentry->d_inode);
    struct ath_pktlog_info *pl_info =
        (struct ath_pktlog_info *) proc_entry->data;
#else
    struct ath_pktlog_info *pl_info = PDE_DATA(file->f_path.dentry->d_inode);
#endif
    struct ath_pktlog_buf *log_buf;
    size_t pktlog_hdr_size = pl_info->pktlog_hdr_size;

    qdf_spin_lock(&pl_info->log_lock);
    log_buf =  pl_info->buf;

    if (log_buf == NULL){
        qdf_spin_unlock(&pl_info->log_lock);
        return 0;
    }

    if (!pktlog_hdr_size) {
        pktlog_hdr_size = sizeof(struct ath_pktlog_hdr);
        qdf_info("%s: WARNING: pl_info->pktlog_hdr_size = %zu, Using pktlog_hdr_size as: %zu\n",
                 __func__, pl_info->pktlog_hdr_size, pktlog_hdr_size);
    }

    if(*ppos == 0 && pl_info->log_state) {
        pl_info->saved_state = pl_info->log_state;
        pl_info->log_state = 0;
    }

    bufhdr_size = sizeof(log_buf->bufhdr);

    /* copy valid log entries from circular buffer into user space */
    rem_len = nbytes;

    count = 0;

    if (*ppos < bufhdr_size) {
        count = MIN((bufhdr_size - *ppos), rem_len);
        qdf_spin_unlock(&pl_info->log_lock);
        if (copy_to_user(buf, ((char *) &log_buf->bufhdr) + *ppos, count))
	    return -EFAULT;
        rem_len -= count;
        ret_val += count;
        qdf_spin_lock(&pl_info->log_lock);
    }

    start_offset = log_buf->rd_offset;

    if ((rem_len == 0) || (start_offset < 0))
        goto rd_done;

    fold_offset = -1;
    cur_rd_offset = start_offset;

    /* Find the last offset and fold-offset if the buffer is folded */
    do {
        struct ath_pktlog_hdr *log_hdr;
        int log_data_offset;

        log_hdr =
            (struct ath_pktlog_hdr *) (log_buf->log_data + cur_rd_offset);

        log_data_offset = cur_rd_offset + pktlog_hdr_size;

        if ((fold_offset == -1)
            && ((pl_info->buf_size - log_data_offset) <= log_hdr->size))
            fold_offset = log_data_offset - 1;

        PKTLOG_MOV_RD_IDX_HDRSIZE(cur_rd_offset, log_buf, pl_info->buf_size, pktlog_hdr_size);

        if ((fold_offset == -1) && (cur_rd_offset == 0)
            && (cur_rd_offset != log_buf->wr_offset))
            fold_offset = log_data_offset + log_hdr->size - 1;

        end_offset = log_data_offset + log_hdr->size - 1;
    } while (cur_rd_offset != log_buf->wr_offset);

    ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

    if (fold_offset == -1) {
        if (ppos_data > end_offset)
            goto rd_done;

        count = MIN(rem_len, (end_offset - ppos_data + 1));
        qdf_spin_unlock(&pl_info->log_lock);
        if (copy_to_user(buf + ret_val, log_buf->log_data + ppos_data, count))
	    return -EFAULT;
        ret_val += count;
        rem_len -= count;
        qdf_spin_lock(&pl_info->log_lock);
    } else {
        if (ppos_data <= fold_offset) {
            count = MIN(rem_len, (fold_offset - ppos_data + 1));
            qdf_spin_unlock(&pl_info->log_lock);
            if (copy_to_user(buf + ret_val, log_buf->log_data + ppos_data,
                         count)) return -EFAULT;
            ret_val += count;
            rem_len -= count;
            qdf_spin_lock(&pl_info->log_lock);
        }

        if (rem_len == 0)
            goto rd_done;

        ppos_data =
            *ppos + ret_val - (bufhdr_size +
                               (fold_offset - start_offset + 1));

        if (ppos_data <= end_offset) {
            count = MIN(rem_len, (end_offset - ppos_data + 1));
            qdf_spin_unlock(&pl_info->log_lock);
            if (copy_to_user(buf + ret_val, log_buf->log_data + ppos_data,
                         count)) return -EFAULT;
            ret_val += count;
            rem_len -= count;
            qdf_spin_lock(&pl_info->log_lock);
        }
    }

rd_done:
    if((ret_val < nbytes) && pl_info->saved_state) {
        pl_info->log_state = pl_info->saved_state;
        pl_info->saved_state = 0;
    }
    *ppos += ret_val;
    qdf_spin_unlock(&pl_info->log_lock);
    return ret_val;
}


static ssize_t
pktlog_read(struct file *file, char *buf, size_t nbytes, loff_t *ppos)
{
    size_t ret_val = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct proc_dir_entry *proc_entry = PDE(file->f_dentry->d_inode);
    struct ath_pktlog_info *pl_info =
        (struct ath_pktlog_info *) proc_entry->data;
#else
    struct ath_pktlog_info *pl_info = PDE_DATA(file->f_path.dentry->d_inode);
#endif

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    ret_val = __pktlog_read(file, buf, nbytes, ppos);
    qdf_mutex_release(&pl_info->pktlog_mutex);
    return ret_val;
}

#ifndef VMALLOC_VMADDR
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
/* Convert a kernel virtual address to a kernel logical address */
static volatile void *pktlog_virt_to_logical(volatile void *addr)
{
    pgd_t *pgd;
    pmd_t *pmd;
    pte_t *ptep, pte;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15) || (defined (__i386__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)))
    pud_t *pud;
#endif
    unsigned long vaddr, ret = 0UL;

    vaddr = VMALLOC_VMADDR((unsigned long) addr);

    pgd = pgd_offset_k(vaddr);

    if (!pgd_none(*pgd)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15) || (defined (__i386__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)))
		pud = pud_offset(pgd, vaddr);
        pmd = pmd_offset(pud, vaddr);
#else
        pmd = pmd_offset(pgd, vaddr);
#endif

        if (!pmd_none(*pmd)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
            ptep = pte_offset_map(pmd, vaddr);
#else
            ptep = pte_offset(pmd, vaddr);
#endif
            pte = *ptep;

            if (pte_present(pte)) {
                ret = (unsigned long) page_address(pte_page(pte));
                ret |= (vaddr & (PAGE_SIZE - 1));
            }
        }
    }
    return ((volatile void *) ret);
}
#endif

/* vma operations for mapping vmalloced area to user space */
static void pktlog_vopen(struct vm_area_struct *vma)
{
    PKTLOG_MOD_INC_USE_COUNT;
}

static void pktlog_vclose(struct vm_area_struct *vma)
{
    PKTLOG_MOD_DEC_USE_COUNT;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
int pktlog_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	unsigned long address = (unsigned long)vmf->virtual_address;

	if (address == 0UL) {
		qdf_err(PKTLOG_TAG "%s: page fault out of range\n", __FUNCTION__);
		return VM_FAULT_NOPAGE;
    	}
	if (vmf->pgoff > vma->vm_end)
		return VM_FAULT_SIGBUS;

	/*    
	page = alloc_page (GFP_HIGHUSER);
 	if (!page) 
	{
		return VM_FAULT_OOM;
	}
	*/
	get_page(virt_to_page((void *)address));
	vmf->page = virt_to_page((void *)address);
	return VM_FAULT_MINOR;
}
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
struct page *pktlog_vmmap(struct vm_area_struct *vma, unsigned long addr,
                          int *type)
#else
struct page *pktlog_vmmap(struct vm_area_struct *vma, unsigned long addr,
                          int write_access)
#endif
{
    unsigned long offset, vaddr;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct proc_dir_entry *proc_entry = PDE(vma->vm_file->f_dentry->d_inode);
    struct ath_pktlog_info *pl_info =
        (struct ath_pktlog_info *) proc_entry->data;
#else
    struct ath_pktlog_info *pl_info = PDE_DATA(vma->vm_file->f_path.dentry->d_inode);
#endif

    offset = addr - vma->vm_start + (vma->vm_pgoff << PAGE_SHIFT);

    vaddr = (unsigned long) pktlog_virt_to_logical((void *) (pl_info->buf) + offset);

    if (vaddr == 0UL) {
        qdf_err(PKTLOG_TAG "%s: page fault out of range\n", __FUNCTION__);
        return ((struct page *) 0UL);
    }

    /* increment the usage count of the page */
    get_page(virt_to_page((void*)vaddr));
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if(type) {
        *type = VM_FAULT_MINOR;
    }
#endif

    return (virt_to_page((void*)vaddr));
}
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25) */

static struct vm_operations_struct pktlog_vmops = {
    open:pktlog_vopen,
    close:pktlog_vclose,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
    fault:pktlog_fault,
#else 
    nopage:pktlog_vmmap,		
#endif
};

static int pktlog_mmap(struct file *file, struct vm_area_struct *vma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct proc_dir_entry *proc_entry = PDE(file->f_dentry->d_inode);
    struct ath_pktlog_info *pl_info =
        (struct ath_pktlog_info *) proc_entry->data;
#else
    struct ath_pktlog_info *pl_info = PDE_DATA(file->f_path.dentry->d_inode);
#endif

    if (vma->vm_pgoff != 0) {
        /* Entire buffer should be mapped */
        return -EINVAL;
    }

    if (!pl_info->buf) {
        qdf_err(PKTLOG_TAG "%s: Log buffer unavailable\n", __FUNCTION__);
        return -ENOMEM;
    }

    vma->vm_flags |= VM_LOCKED;
    vma->vm_ops = &pktlog_vmops;
    pktlog_vopen(vma);
    return 0;
}

/*
 * Linux implementation of helper functions
 */
void
pktlog_disable_adapter_logging(void)
{
    struct net_device *dev;
    int i;
    char dev_name[PKTLOG_DEVNAME_SIZE];

    // Disable any active per adapter logging 
    for (i = 0; i < MAX_WLANDEV; i++) {
        snprintf(dev_name, sizeof(dev_name),
                 WLANDEV_BASENAME "%d", i);


#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23))
        if ((dev = dev_get_by_name(&init_net,dev_name)) != NULL) {
#else
        if ((dev = dev_get_by_name(dev_name)) != NULL) {
#endif
            struct ath_softc *dev_sc = ath_get_softc(dev);
            dev_sc->pl_info->log_state = 0;
            dev_put(dev);
        }
    }
}

int
pktlog_alloc_buf(ath_generic_softc_handle scn, struct ath_pktlog_info *pl_info)
{
    u_int32_t page_cnt;
    unsigned long vaddr;
    struct page *vpg;
    struct ath_pktlog_buf *buffer;

    page_cnt = (sizeof(*(pl_info->buf)) +
                pl_info->buf_size) / PAGE_SIZE;

    qdf_spin_lock(&pl_info->log_lock);
    if (pl_info->buf != NULL) {
        qdf_warn(PKTLOG_TAG
                "%s: Buffer is already in use\n",__FUNCTION__);
        qdf_spin_unlock(&pl_info->log_lock);
        return -ENOMEM;
    }
    qdf_spin_unlock(&pl_info->log_lock);

    if ((buffer = vmalloc((page_cnt + 2) * PAGE_SIZE)) == NULL) {
        qdf_err(PKTLOG_TAG
       "%s: Unable to allocate buffer"
       "(%d pages)\n", __FUNCTION__, page_cnt);
       return -ENOMEM;
    }


    buffer = (struct ath_pktlog_buf *)
        (((unsigned long) (buffer) + PAGE_SIZE - 1)
         & PAGE_MASK);

    for (vaddr = (unsigned long) (buffer);
         vaddr < ((unsigned long) (buffer)
                  + (page_cnt * PAGE_SIZE)); vaddr += PAGE_SIZE) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))
        vpg = vmalloc_to_page((const void *) vaddr);
#else
        vpg = virt_to_page(pktlog_virt_to_logical((void *) vaddr));
#endif
        SetPageReserved(vpg);
    }

    qdf_spin_lock(&pl_info->log_lock);
    if(pl_info->buf != NULL)
        pktlog_release_buf(pl_info);

    pl_info->buf =  buffer;
    qdf_spin_unlock(&pl_info->log_lock);

    return 0;
}

void
pktlog_release_buf(struct ath_pktlog_info *pl_info)
{
    unsigned long page_cnt;
    unsigned long vaddr;
    struct page *vpg;

    qdf_assert(pl_info);
    page_cnt =
        ((sizeof(*(pl_info->buf)) + pl_info->buf_size) / PAGE_SIZE) + 1;

    for (vaddr = (unsigned long) (pl_info->buf); vaddr <
         (unsigned long) (pl_info->buf) + (page_cnt * PAGE_SIZE);
         vaddr += PAGE_SIZE) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))
        vpg = vmalloc_to_page((const void *) vaddr);
#else
        vpg = virt_to_page(pktlog_virt_to_logical((void *) vaddr));
#endif
        ClearPageReserved(vpg);
    }

    vfree(pl_info->buf);
    pl_info->buf = NULL;
}

int
pktlog_tcpip(struct ath_softc *sc, wbuf_t wbuf, struct llc *llc, u_int32_t *proto_log, int *proto_len, HAL_BOOL *isSack)
{
    struct iphdr *ip;
    struct tcphdr *tcp;
    u_int32_t *proto_hdr;
    u_int8_t optlen, *offset;
    int i, tail_len;

    ip = (struct iphdr *)((u_int8_t *)llc + LLC_SNAPFRAMELEN);

    switch (ip->protocol) {
    case IPPROTO_TCP:
        /* 
         * tcp + ip hdr len are in units of 32-bit words
         */
        tcp = (struct tcphdr *)((u_int32_t *)ip + ip->ihl);
        proto_hdr = (u_int32_t *)tcp;

        for (i = 0; i < tcp->doff; i++) {
            *proto_log = ntohl(*proto_hdr);
            proto_log++, proto_hdr++;
        }
        *proto_len = tcp->doff * sizeof(u_int32_t);
	if ((*proto_len > TCP_HDR_MIN_LEN) && (tcp->ack)) {
            /* option part exist in TCP header */
            offset = (u_int8_t *)tcp + TCP_HDR_MIN_LEN; 
            tail_len = *proto_len - TCP_HDR_MIN_LEN;
            while (tail_len > 0) {
              while(*offset == TCPOPT_NOP) {
                  ++offset;
                  --tail_len;
              }
              optlen = *(offset+1);
              /* check whether it is TCP_SACK type and validate related fields */
              if ((*offset==TCPOPT_SACK) 
                  && (optlen>=TCP_SACK_MIN_LEN) && (optlen <= tail_len)) {
	        *isSack = TRUE;
                break;
              }
              offset += optlen;
              tail_len -= optlen;
            }
	}
        return PKTLOG_PROTO_TCP;

    case IPPROTO_UDP:
    case IPPROTO_ICMP:
    default:
        return PKTLOG_PROTO_NONE;
    }
}
#endif /* REMOVE_PKT_LOG */
