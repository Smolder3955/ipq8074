/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <sys/queue.h>
#include <ieee80211_var.h>
#include "osif_private.h"
#include "osif_wrap_private.h"
#include "osif_wrap.h"
#include "if_athvar.h"

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include "osif_nss_wifiol_if.h"
#include "osif_nss_wifiol_vdev_if.h"
#endif

/**
 * @brief Find wrap dev object based on MAC address.
 *
 * @param wdt Ptr to the wrap device table.
 * @param mac MAC address to look up.
 *
 * @return osif_dev on success
 * @return NULL on failure
 */
osif_dev *osif_wrap_wdev_find(struct wrap_devt *wdt,unsigned char *mac)
{
    osif_dev *osdev;
    int hash;
    rwlock_state_t lock_state;

    hash = WRAP_DEV_HASH(mac);
    OS_RWLOCK_READ_LOCK(&wdt->wdt_lock,&lock_state);
    LIST_FOREACH(osdev, &wdt->wdt_hash[hash], osif_dev_hash) {
        if (IEEE80211_ADDR_EQ(osdev->osif_dev_oma, mac)){
            OS_RWLOCK_READ_UNLOCK(&wdt->wdt_lock,&lock_state);
            return osdev;
        }
    }
    OS_RWLOCK_READ_UNLOCK(&wdt->wdt_lock,&lock_state);
    return NULL;
}
EXPORT_SYMBOL(osif_wrap_wdev_find);
/**
 * @brief Find wrap dev object based on VMA MAC address.
 *
 * @param wdt Ptr to the wrap device table.
 * @param mac MAC address to look up.
 *
 * @return osif_dev on success
 * @return NULL on failure
 */
osif_dev *osif_wrap_wdev_vma_find(struct wrap_devt *wdt,unsigned char *mac)
{
    osif_dev *osdev;
    int hash;
    rwlock_state_t lock_state;

    hash = WRAP_DEV_HASH(mac);
    OS_RWLOCK_READ_LOCK(&wdt->wdt_lock,&lock_state);
    LIST_FOREACH(osdev, &wdt->wdt_hash_vma[hash], osif_dev_hash_vma) {
        if (IEEE80211_ADDR_EQ(osdev->osif_dev_vma, mac)){
            OS_RWLOCK_READ_UNLOCK(&wdt->wdt_lock,&lock_state);
            return osdev;
        }
    }
    OS_RWLOCK_READ_UNLOCK(&wdt->wdt_lock,&lock_state);
    return NULL;
}
EXPORT_SYMBOL(osif_wrap_wdev_vma_find);



/**
 * @brief Add wrap dev object to the device table, also
 * registers bridge hooks if this the first object.
 *
 * @param osdev Pointer to osif_dev to add.
 *
 * @return 0 on success
 * @return -ve on failure
 */
int osif_wrap_dev_add(osif_dev *osdev)
{
	int hash, hash_vma;
    wlan_if_t vap = osdev->os_if;
    struct wrap_devt *wdt = &vap->iv_ic->ic_wrap_com->wc_devt;
    struct wrap_com *wrap_com = vap->iv_ic->ic_wrap_com;
    rwlock_state_t lock_state;
    struct net_device *netdev = OSIF_TO_NETDEV(osdev);

    OSIF_WRAP_MSG("Adding %s to the list mat\n",OSIF_TO_DEVNAME(osdev));
    if(vap->iv_mat == 1) {
        IEEE80211_ADDR_COPY(osdev->osif_dev_oma,vap->iv_mat_addr);
	IEEE80211_ADDR_COPY(osdev->osif_dev_vma,netdev->dev_addr);
    }
    else {
        IEEE80211_ADDR_COPY(osdev->osif_dev_oma,netdev->dev_addr);
        IEEE80211_ADDR_COPY(osdev->osif_dev_vma,netdev->dev_addr);
    }

    hash = WRAP_DEV_HASH(osdev->osif_dev_oma);
    hash_vma = WRAP_DEV_HASH(osdev->osif_dev_vma);
    OS_RWLOCK_WRITE_LOCK_BH(&wdt->wdt_lock,&lock_state);
    LIST_INSERT_HEAD(&wdt->wdt_hash[hash], osdev, osif_dev_hash);
    TAILQ_INSERT_TAIL(&wdt->wdt_dev, osdev, osif_dev_list);
    LIST_INSERT_HEAD(&wdt->wdt_hash_vma[hash_vma], osdev, osif_dev_hash_vma);
    TAILQ_INSERT_TAIL(&wdt->wdt_dev_vma, osdev, osif_dev_list_vma);
    osdev->wrap_handle = wrap_com;
    OS_RWLOCK_WRITE_UNLOCK_BH(&wdt->wdt_lock,&lock_state);
    return 0;
}

/**
 * @brief Delete wrap dev object from the device table, also
 * unregisters bridge hooks if this the last object.
 *
 * @param osdev Ptr to the osif_dev to delete
 *
 * @return void
 */
void osif_wrap_dev_remove(osif_dev *osdev)
{
    int hash;
    osif_dev *osd, *temp;
    wlan_if_t vap = osdev->os_if;
    struct wrap_devt *wdt = &vap->iv_ic->ic_wrap_com->wc_devt;
    rwlock_state_t lock_state;
    hash = WRAP_DEV_HASH(osdev->osif_dev_oma);
    OS_RWLOCK_WRITE_LOCK_BH(&wdt->wdt_lock,&lock_state);
    LIST_FOREACH_SAFE(osd, &wdt->wdt_hash[hash], osif_dev_hash, temp) {
        if ((osd == osdev) && IEEE80211_ADDR_EQ(osd->osif_dev_oma,osdev->osif_dev_oma)){
            LIST_REMOVE(osd,osif_dev_hash);
	    TAILQ_REMOVE(&wdt->wdt_dev, osd, osif_dev_list);
	    OSIF_WRAP_MSG("Removing %s from the list\n",OSIF_TO_DEVNAME(osdev));
            OS_RWLOCK_WRITE_UNLOCK_BH(&wdt->wdt_lock,&lock_state);
	    return;
	}
    }
    OS_RWLOCK_WRITE_UNLOCK_BH(&wdt->wdt_lock,&lock_state);
    return;
}

/**
 * @brief Delete wrap dev object from the vma device table
 * @param osdev Ptr to the osif_dev to delete
 * @return void
 */
void osif_wrap_dev_remove_vma(osif_dev *osdev)
{
    int hash;
    osif_dev *osd, *temp;
    wlan_if_t vap = osdev->os_if;
    struct wrap_devt *wdt = &vap->iv_ic->ic_wrap_com->wc_devt;
    rwlock_state_t lock_state;

    hash = WRAP_DEV_HASH(osdev->osif_dev_vma);
    OS_RWLOCK_WRITE_LOCK_BH(&wdt->wdt_lock,&lock_state);
    LIST_FOREACH_SAFE(osd, &wdt->wdt_hash_vma[hash], osif_dev_hash_vma, temp) {
        if ((osd == osdev) && IEEE80211_ADDR_EQ(osd->osif_dev_vma,osdev->osif_dev_vma)){
            LIST_REMOVE(osd,osif_dev_hash_vma);
            TAILQ_REMOVE(&wdt->wdt_dev_vma, osd, osif_dev_list_vma);
            OSIF_WRAP_MSG("Removing %s from VMA list\n",OSIF_TO_DEVNAME(osdev));
            OS_RWLOCK_WRITE_UNLOCK_BH(&wdt->wdt_lock,&lock_state);
            return;
        }
    }
    OS_RWLOCK_WRITE_UNLOCK_BH(&wdt->wdt_lock,&lock_state);
    return;
}

/**
 * @brief WRAP device table attach
 *
 * @param wc Ptr to the wrap common.
 * @param wdt Ptr to wrap device table.
 *
 * @return void
 */
static void osif_wrap_devt_init(struct wrap_com *wc, struct wrap_devt *wdt, struct ieee80211com *ic)
{
    int i;

    OS_RWLOCK_INIT(&wdt->wdt_lock);
    TAILQ_INIT(&wdt->wdt_dev);
    TAILQ_INIT(&wdt->wdt_dev_vma);
    for(i=0;i<WRAP_DEV_HASHSIZE;i++) {
        LIST_INIT(&wdt->wdt_hash[i]);
        LIST_INIT(&wdt->wdt_hash_vma[i]);
    }
    wdt->wdt_wc=wc;
    OSIF_WRAP_MSG("osif wrap dev table init done\n");
    return;
}

/**
 * @brief wrap device table detach
 *
 * @param wrap comm
 *
 * @return
 */
static void osif_wrap_devt_detach(struct wrap_com *wc)
{
    struct wrap_devt *wdt = &wc->wc_devt;
    OS_RWLOCK_DESTROY(&wdt->wdt_lock);
    wdt->wdt_wc=NULL;
    OSIF_WRAP_MSG("osif wrap dev table detached\n");
    return;
}

/**
 * @brief wrap attach
 *
 * @param void
 *
 * @return 0 on success
 * @return -ve on failure
 */
int osif_wrap_attach(wlan_dev_t ic)
{
    int ret=0;
    struct wrap_com *wrap_com ;

    wrap_com = OS_MALLOC(NULL,sizeof(struct wrap_com),GFP_KERNEL);
    if (!wrap_com) {
        OSIF_WRAP_MSG_ERR("Failed to alloc mem for wrap common\n");
	return -EINVAL;
    } else{
        OSIF_WRAP_MSG("osif wrap attached\n");
    }
    OS_MEMZERO(wrap_com,sizeof(struct wrap_com));
    wrap_com->wc_use_cnt++;
    osif_wrap_devt_init(wrap_com, &wrap_com->wc_devt, ic);
    ic->ic_wrap_com=wrap_com;
    if(ic->ic_wrap_com)
        ic->ic_wrap_com->wc_isolation = WRAP_ISOLATION_DEFVAL;
    qdf_info(" Wrap Attached: Wrap_com =%pK ic->ic_wrap_com=%pK &wrap_com->wc_devt=%pK \n", wrap_com, ic->ic_wrap_com, &wrap_com->wc_devt);
    return ret;
}

/**
 * @brief wrap detach
 *
 * @param void
 *
 * @return 0 on success
 * @return -ve on failure
 */
int osif_wrap_detach(wlan_dev_t ic)
{
    int ret=0;
    struct wrap_com *wrap_com = ic->ic_wrap_com;

    ASSERT(wrap_com != NULL);

    wrap_com->wc_use_cnt--;
    if(wrap_com->wc_use_cnt==0){
        osif_wrap_devt_detach(wrap_com);
	OS_FREE(wrap_com);
	ic->ic_wrap_com = NULL;
	OSIF_WRAP_MSG("osif wrap detached\n");
    }
    return ret;
}
