/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * IEEE 802.11 generic handler
 */

#include <osdep.h> 

#include <ieee80211_var.h>
#include <if_athproto.h>

#include <qdf_mem.h> /* qdf_mem_copy */

#ifdef ATH_SUPPORT_HTC

#include "htc_ieee_common.h"

#ifdef ATH_HTC_MII_RXIN_TASKLET
#include "htc_thread.h"
#endif
void
ieee80211_update_node_target(struct ieee80211_node *ni, struct ieee80211vap *vap);

/* target node bitmap valid dump */
void 
print_target_node_bitmap(struct ieee80211_node *ni)
{
    int i;
    
    printk("%s : ", __FUNCTION__);
    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        printk("%d ", ni->ni_ic->target_node_bitmap[i].ni_valid);    
    }    
    printk("\n");
}

/* add/update target node nt dump */
void
print_nt(struct ieee80211_node_target* nt)
{
    printk("------------------------------------------------------------\n");    
    printk("ni_associd: 0x%x\n", nt->ni_associd);
    //printk("ni_txpower: 0x%x\n", nt->ni_txpower);

    printk("mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        nt->ni_macaddr[0], nt->ni_macaddr[1], nt->ni_macaddr[2],
        nt->ni_macaddr[3], nt->ni_macaddr[4], nt->ni_macaddr[5]);
    printk("bssid: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        nt->ni_bssid[0], nt->ni_bssid[1], nt->ni_bssid[2],
        nt->ni_bssid[3], nt->ni_bssid[4], nt->ni_bssid[5]);     

    printk("nodeindex: %d\n", nt->ni_nodeindex);
    printk("vapindex: %d\n", nt->ni_vapindex);
    //printk("ni_vapnode: 0x%x\n", nt->ni_vapnode);
    printk("ni_flags: 0x%x\n", nt->ni_flags);
    printk("ni_htcap: %d\n", nt->ni_htcap);
    //printk("ni_valid: %d\n", nt->ni_valid);
    printk("ni_capinfo: 0x%x\n", nt->ni_capinfo);
    //printk("ni_ic: 0x%08x\n", nt->ni_ic);
    //printk("ni_vap: 0x%08x\n", nt->ni_vap);
    //printk("ni_txseqmgmt: %d\n", nt->ni_txseqmgmt);
    printk("ni_is_vapnode: %d\n", nt->ni_is_vapnode);   
    printk("ni_maxampdu: %d\n", nt->ni_maxampdu);   
    //printk("ni_iv16: %d\n", nt->ni_iv16);   
    //printk("ni_iv32: %d\n", nt->ni_iv32);   
#ifdef ENCAP_OFFLOAD
    //printk("ni_ucast_keytsc: %d\n", nt->ni_ucast_keytsc);   
    //printk("ni_ucast_keytsc: %d\n", nt->ni_ucast_keytsc);   
#endif
    printk("------------------------------------------------------------\n");    
}

u_int8_t
ieee80211_mac_addr_cmp(u_int8_t *src, u_int8_t *dst)
{
    int i;

    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        if (src[i] != dst[i])
            return 1;
    }

    return 0;
}
int
ieee80211_chk_vap_target(struct ieee80211com *ic)
{
	int i;

	for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
		/* avoid upper layer add the same vap twice */
		if (ic->target_vap_bitmap[i].vap_valid == 0) {
			printk("%s: target_vap_bitmap(%d) is invalid\n", __FUNCTION__, i);
			break;
		}
	}

	/* If we can't find any suitable vap entry */
	if (i == HTC_MAX_VAP_NUM) {
		printk("%s: exceed maximum vap number\n", __FUNCTION__);
		return 1 ;
	}


	return 0;
}

void
ieee80211_add_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_target vt;
    int i;
    u_int8_t vapindex = 0;

#if 0
    /* Dump mac address */
    printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, vap->iv_myaddr[0], vap->iv_myaddr[1], vap->iv_myaddr[2],
        vap->iv_myaddr[3], vap->iv_myaddr[4], vap->iv_myaddr[5]);
#endif

    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        /* avoid upper layer add the same vap twice */
        if (IEEE80211_ADDR_EQ(ic->target_vap_bitmap[i].vap_macaddr, vap->iv_myaddr)) {
            printk("%s: add the same vap twice\n", __FUNCTION__);
            return;
        }

        if (ic->target_vap_bitmap[i].vap_valid == 0) {
            printk("%s: target_vap_bitmap(%d) is invalid\n", __FUNCTION__, i);

            IEEE80211_ADDR_COPY(ic->target_vap_bitmap[i].vap_macaddr, vap->iv_myaddr);
            ic->target_vap_bitmap[i].vap_valid = 1;
            vapindex = i;
            break;
        }
        else {
#if 0        	
            printk("%s: target_vap_bitmap(%d) vap_valid = %d\n", __FUNCTION__, i,
            ic->target_vap_bitmap[i].vap_valid);
#endif            
        }
    }

    printk("%s: vap index: %d\n", __FUNCTION__, vapindex);

    /* If we can't find any suitable vap entry */
    if (i == HTC_MAX_VAP_NUM) {
        printk("%s: exceed maximum vap number\n", __FUNCTION__);
        return;
    }

    vt.iv_create_flags = cpu_to_be32(vap->iv_create_flags);
    vt.iv_vapindex = vapindex;
    vt.iv_opmode = cpu_to_be32(vap->iv_opmode);
    vt.iv_mcast_rate = 0;
    vt.iv_rtsthreshold = cpu_to_be16(2304);
    qdf_mem_copy(&vt.iv_myaddr, &(vap->iv_myaddr), IEEE80211_ADDR_LEN);

#if ENCAP_OFFLOAD
    qdf_mem_copy(&vt.iv_myaddr, &(vap->iv_myaddr), IEEE80211_ADDR_LEN);
#endif
    /* save information to vap target */
    ic->target_vap_bitmap[vapindex].iv_vapindex = vapindex;
    ic->target_vap_bitmap[vapindex].iv_opmode = vap->iv_opmode;
    ic->target_vap_bitmap[vapindex].iv_mcast_rate = vap->iv_mcast_rate;
    ic->target_vap_bitmap[vapindex].iv_rtsthreshold = 2304;

    ic->ic_add_vap_target(ic, &vt, sizeof(vt));

    //printk("%s Exit \n", __FUNCTION__);
}

int
ieee80211_chk_node_target(struct ieee80211com *ic)
{
   int i;

	for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
		if (ic->target_node_bitmap[i].ni_valid == 0) {
			break;
		}
	}

	if (i == HTC_MAX_NODE_NUM) {
		printk("%s: exceed maximum node number\n", __FUNCTION__);
		return 1;
	}
	return 0;

}

void
ieee80211_add_node_target(struct ieee80211_node *ni, struct ieee80211vap *vap,
    u_int8_t is_vap_node)
{
    struct ieee80211_node_target nt, *ntar;
    int i;
    u_int8_t nodeindex = 0;
    u_int8_t vapindex = 0xff;
    
    OS_MEMZERO(&nt, sizeof(struct ieee80211_node_target));

    //printk("%s: vap: 0x%08x, ni: 0x%08x, is_vap_node: %d\n", __FUNCTION__, vap, ni, is_vap_node);
    //print_target_node_bitmap(ni);

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        /* avoid upper layer add the same vap twice */
        if (IEEE80211_ADDR_EQ(ni->ni_ic->target_node_bitmap[i].ni_macaddr, ni->ni_macaddr)) {
            printk("%s: add the same node : Updating Target node  %d\n", __FUNCTION__, i);
            ieee80211_update_node_target(ni, ni->ni_vap); 
            return;
        }

        if (ni->ni_ic->target_node_bitmap[i].ni_valid == 0) {
            IEEE80211_ADDR_COPY(ni->ni_ic->target_node_bitmap[i].ni_macaddr, ni->ni_macaddr);
            ni->ni_ic->target_node_bitmap[i].ni_valid = 1;
            nodeindex = i;
            break;
        }
    }

    if (i == HTC_MAX_NODE_NUM) {
        printk("%s: exceed maximum node number\n", __FUNCTION__);
        return;
    }

    /* search for vap index of this node */
    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ni->ni_ic->target_vap_bitmap[i].vap_valid == 1 && 
            IEEE80211_ADDR_EQ(ni->ni_ic->target_vap_bitmap[i].vap_macaddr, ni->ni_vap->iv_myaddr)) {
            vapindex = i;
            break;
        }
    }

    /*
     * Fill the target node nt.
     */
    nt.ni_associd    = htons(ni->ni_associd);
    nt.ni_txpower    = htons(ni->ni_txpower);

    IEEE80211_ADDR_COPY(&nt.ni_macaddr, &(ni->ni_macaddr));
    IEEE80211_ADDR_COPY(&nt.ni_bssid, &(ni->ni_bssid));

    nt.ni_nodeindex  = nodeindex;//ni->ni_nodeindex;
    nt.ni_vapindex   = vapindex;
    nt.ni_vapnode    = 0;//add for rate control test
    nt.ni_flags      = htonl(ni->ni_flags);

    nt.ni_htcap      = htons(ni->ni_htcap);
    nt.ni_capinfo    = htons(ni->ni_capinfo);
    nt.ni_is_vapnode = is_vap_node; //1;
    nt.ni_maxampdu   = htons(ni->ni_maxampdu);
#ifdef ENCAP_OFFLOAD    
    nt.ni_ucast_keytsc = 0;
    nt.ni_mcast_keytsc = 0;
#endif

#ifdef ATH_SUPPORT_QUICK_KICKOUT
   nt.ni_kickoutlimit =  ni->ni_vap->iv_sko_th;
#else
#define ATH_NODEKICKOUT_LIMIT 50    
   nt.ni_kickoutlimit = ATH_NODEKICKOUT_LIMIT;
#endif
    
    ntar = &nt;
   
    /* save information to node target */
    ni->ni_ic->target_node_bitmap[nodeindex].ni_nodeindex = nodeindex;
    ni->ni_ic->target_node_bitmap[nodeindex].ni_vapindex = vapindex;
    ni->ni_ic->target_node_bitmap[nodeindex].ni_is_vapnode = is_vap_node;

    ni->ni_ic->ic_add_node_target(ni->ni_ic, ntar, sizeof(struct ieee80211_node_target));
    //print_nt(&nt);
}

void
ieee80211_delete_node_target(struct ieee80211_node *ni, struct ieee80211com *ic,
    struct ieee80211vap *vap, u_int8_t is_reset_bss)
{
    int i;
    u_int8_t nodeindex = 0xff;

    //printk("%s: vap: 0x%08x, ni: 0x%08x, is_reset_bss: %d\n", __FUNCTION__, vap, ni, is_reset_bss);

    /* Dump mac address */
    /* printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, ni->ni_macaddr[0], ni->ni_macaddr[1], ni->ni_macaddr[2],
        ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5]); */

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if (ni->ni_ic->target_node_bitmap[i].ni_valid == 1) {
            /* avoid upper layer add the same vap twice */
            if (IEEE80211_ADDR_EQ(ic->target_node_bitmap[i].ni_macaddr, ni->ni_macaddr)) {
                if(is_reset_bss == 1 && ic->target_node_bitmap[i].ni_is_vapnode == 1)
                {
                    return;
                }                                            
                OS_MEMZERO(ic->target_node_bitmap[i].ni_macaddr, IEEE80211_ADDR_LEN);
                ic->target_node_bitmap[i].ni_valid = 0;
                nodeindex = i;
                break;
            }
        }
    }

    if (i == HTC_MAX_NODE_NUM) {
        /*printk("%s: Can't find desired node\n", __FUNCTION__);*/
        return;
    }

    /* printk("%s: ni: 0x%08x, nodeindex: %d, is_reset_bss: %d\n", __FUNCTION__,
    (u_int32_t)ni, nodeindex, is_reset_bss); */

    ic->ic_delete_node_target(ic, &nodeindex, sizeof(nodeindex));
}

#if ATH_SUPPORT_HTC
void
ieee80211_update_node_target(struct ieee80211_node *ni, struct ieee80211vap *vap)
{
    struct ieee80211_node_target nt, *ntar;
    int i;
    u_int8_t nodeindex = 0;
    u_int8_t vapindex = 0xff;

    OS_MEMZERO(&nt, sizeof(struct ieee80211_node_target));
    
    //printk("%s: vap: 0x%08x, ni: 0x%08x\n", __FUNCTION__, vap, ni);
    //print_target_node_bitmap(ni);

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if (IEEE80211_ADDR_EQ(ni->ni_ic->target_node_bitmap[i].ni_macaddr, ni->ni_macaddr)) {                                        
            break;
        }
    }
    
    if ((i < HTC_MAX_NODE_NUM) && (ni->ni_ic->target_node_bitmap[i].ni_valid == 1)) {
        //printk("%s : find node index (%d)\n", __FUNCTION__, i);
        nodeindex = i;            
    }
    else {
        //printk("%s:  can't find the update node\n", __FUNCTION__);
        return;
    }

    /* search for vap index of this node */
    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ni->ni_ic->target_vap_bitmap[i].vap_valid == 1 && 
            IEEE80211_ADDR_EQ(ni->ni_ic->target_vap_bitmap[i].vap_macaddr, ni->ni_vap->iv_myaddr)) {
            vapindex = i;
            break;
        }
    }

    /*
     * Fill the target node nt.
     */
    nt.ni_associd    = htons(ni->ni_associd);
    nt.ni_txpower    = htons(ni->ni_txpower);

    IEEE80211_ADDR_COPY(&nt.ni_macaddr, &(ni->ni_macaddr));
    IEEE80211_ADDR_COPY(&nt.ni_bssid, &(ni->ni_bssid));

    nt.ni_nodeindex  = nodeindex;//ni->ni_nodeindex;
    nt.ni_vapindex   = vapindex;
    nt.ni_vapnode    = 0;//add for rate control test
    nt.ni_flags      = htonl(ni->ni_flags);

    nt.ni_htcap      = htons(ni->ni_htcap);
    nt.ni_capinfo    = htons(ni->ni_capinfo);
    nt.ni_is_vapnode = ni->ni_ic->target_node_bitmap[nodeindex].ni_is_vapnode;
    nt.ni_maxampdu   = htons(ni->ni_maxampdu);
#ifdef ENCAP_OFFLOAD    
    nt.ni_ucast_keytsc = 0;
    nt.ni_mcast_keytsc = 0;
#endif

    ntar = &nt;

    ni->ni_ic->ic_update_node_target(ni->ni_ic, ntar, sizeof(struct ieee80211_node_target));

    /*
     * debug dump nt
     */
    //print_nt(&nt);   
}
#endif

#if ENCAP_OFFLOAD
void
ieee80211_update_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_update_tgt tmp_vap_update;
    u_int8_t vapindex = 0;

    qdf_mem_zero(&tmp_vap_update , sizeof(struct ieee80211vap_update_tgt)); 

    /*searching vapindex */
    for (vapindex = 0; vapindex < HTC_MAX_VAP_NUM; vapindex++) {
        if ((ic->target_vap_bitmap[vapindex].vap_valid) && 
                (IEEE80211_ADDR_EQ(ic->target_vap_bitmap[vapindex].vap_macaddr,vap->iv_myaddr))) 
            break;
    }

    tmp_vap_update.iv_flags         = htonl(vap->iv_flags);
    tmp_vap_update.iv_flags_ext     = htonl(vap->iv_flags_ext);
    tmp_vap_update.iv_vapindex      = htonl(vapindex);
    tmp_vap_update.iv_rtsthreshold  = htons(vap->vdev_mlme->mgmt.generic.rts_threshold);

    ic->ic_update_vap_target(ic, &tmp_vap_update, sizeof(tmp_vap_update));
}
#endif

void
ieee80211_update_target_ic(struct ieee80211_node *ni)
{
    struct ieee80211com_target ic_tgt;
    //WLAN_DEV_INFO *pDev = pIaw->pDev;
    //WLAN_STA_CONFIG *pCfg = &pDev->staConfig;
    int val;

    ni->ni_ic->ic_get_config_chainmask(ni->ni_ic, &val);
    ic_tgt.ic_tx_chainmask = val;    

    /* FIXME: Currently we fix these values */

    ic_tgt.ic_flags = cpu_to_be32(0x400c2400);
    ic_tgt.ic_flags_ext = cpu_to_be32(0x106080);
    ic_tgt.ic_ampdu_limit = cpu_to_be32(0xffff);    
    ic_tgt.ic_ampdu_subframes = 20;
    ic_tgt.ic_tx_chainmask_legacy = 1;
    ic_tgt.ic_rtscts_ratecode = 0;
    ic_tgt.ic_protmode = 1;   
    
    ni->ni_ic->ic_htc_ic_update_target(ni->ni_ic, &ic_tgt, sizeof(struct ieee80211com_target));
}

void
ieee80211_delete_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_target vt;
    u_int8_t vapindex = 0;
    int i;

#if 0
    /* Dump mac address */
    printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, vap->iv_myaddr[0], vap->iv_myaddr[1], vap->iv_myaddr[2],
        vap->iv_myaddr[3], vap->iv_myaddr[4], vap->iv_myaddr[5]);
#endif

    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ic->target_vap_bitmap[i].vap_valid == 1) {
            /* Compare mac address */
            if (IEEE80211_ADDR_EQ(ic->target_vap_bitmap[i].vap_macaddr, vap->iv_myaddr)) {
                vapindex = i;
                ic->target_vap_bitmap[i].vap_valid = 0;
                OS_MEMZERO(ic->target_vap_bitmap[i].vap_macaddr, IEEE80211_ADDR_LEN);
                break;
            }
        }
    }

    if (i == HTC_MAX_VAP_NUM) {
        printk("%s: Can't find desired vap\n", __FUNCTION__);
        return;
    }

    //printk("%s: vapindex: %d\n", __FUNCTION__, vapindex);

    vt.iv_vapindex = vapindex;//vap->iv_vapindex;
    vt.iv_opmode = 0;//vap->iv_opmode;
    vt.iv_mcast_rate = 0;//vap->iv_mcast_rate; 
    vt.iv_rtsthreshold = 0;//vap->iv_rtsthreshold;

    ic->ic_delete_vap_target(ic, &vt, sizeof(vt));

    //printk("%s Exit \n", __FUNCTION__);
}

#ifdef ATH_HTC_MII_RXIN_TASKLET

void
_ieee80211_node_leave_deferwork(void *arg)

{ 
    struct ieee80211com *ic = (struct ieee80211com *)arg;
    struct ieee80211_node *ni = NULL;

    

    do {

        OS_NODELEAVE_LOCKBH(&ic->ic_nodeleave_lock);
        ni = TAILQ_FIRST(&ic->ic_nodeleave_queue);
        if(ni)
            TAILQ_REMOVE(&ic->ic_nodeleave_queue,ni,nodeleave_list);
        OS_NODELEAVE_UNLOCKBH(&ic->ic_nodeleave_lock);
        if(!ni)
            break;
        _ieee80211_node_leave(ni);
         ieee80211_free_node(ni);
    }while(1);
    atomic_set(&ic->ic_nodeleave_deferflags, DEFER_DONE);
}


bool
_ieee80211_node_leave_defer(struct ieee80211_node *ni)
{


    struct ieee80211com *ic =  ni->ni_ic;
 
    if (!ieee80211_try_ref_node(ni))
        return false;

    TAILQ_INSERT_TAIL(&ic->ic_nodeleave_queue, ni, nodeleave_list);
    
    
    if(atomic_read(&ic->ic_nodeleave_deferflags) != DEFER_PENDING){
        {
            atomic_set(&ic->ic_nodeleave_deferflags, DEFER_PENDING);
            OS_PUT_DEFER_ITEM(ic->ic_osdev,
                    _ieee80211_node_leave_deferwork,
                    WORK_ITEM_SINGLE_ARG_DEFERED,
                    ic, NULL, NULL);

        }
    }

    return true;
}

bool
IEEE80211_NODE_LEAVE(struct ieee80211_node *ni)
{

  if(OS_IN_SOFTIRQ())
    return _ieee80211_node_leave_defer(ni);
  else
     return _ieee80211_node_leave(ni);

}
void 
_ath_htc_netdeferfn_init(struct ieee80211com *ic)
{
    OS_MGMT_LOCKINIT(&ic->ic_mgmt_lock);
    atomic_set(&ic->ic_mgmt_deferflags, DEFER_DONE);
    qdf_nbuf_queue_init(&ic->ic_mgmt_nbufqueue);

    OS_NODELEAVE_LOCKINIT(&ic->ic_nodeleave_lock);
    atomic_set(&ic->ic_nodeleave_deferflags, DEFER_DONE);
	TAILQ_INIT(&ic->ic_nodeleave_queue);


    OS_NAWDSDEFER_LOCKINIT(&ic->ic_nawdsdefer_lock);
    TAILQ_INIT(&ic->ic_nawdslearnlist);
    atomic_set(&ic->ic_nawds_deferflags, DEFER_DONE);
 
}

void _ath_htc_netdeferfn_cleanup(struct ieee80211com *ic)
{

	wbuf_t wbuf = QDF_NBUF_NULL ;
	nawds_dentry_t * nawds_entry = NULL ;

	/* Freeing MGMT defer buffer */
	do {

		OS_MGMT_LOCKBH(&ic->ic_mgmt_lock);
		wbuf = qdf_nbuf_queue_remove(&ic->ic_mgmt_nbufqueue);
		OS_MGMT_UNLOCKBH(&ic->ic_mgmt_lock);

		if(!wbuf)
			break;
		wbuf_free(wbuf);
	}while(wbuf);

	atomic_set(&ic->ic_mgmt_deferflags, DEFER_DONE);


	do {

		OS_NAWDSDEFER_LOCKBH(&ic->ic_nawdsdefer_lock);
		nawds_entry = TAILQ_FIRST(&ic->ic_nawdslearnlist);
		if(nawds_entry)
			TAILQ_REMOVE(&ic->ic_nawdslearnlist,nawds_entry,nawds_dlist);
		OS_NAWDSDEFER_UNLOCKBH(&ic->ic_nawdsdefer_lock);
		if(!nawds_entry)
			break;

		OS_FREE(nawds_entry);

	}while(1);
	atomic_set(&ic->ic_nawds_deferflags, DEFER_DONE);

}
void
ieee80211_recv_mgmt_defer(void *arg)
{
    struct ieee80211com *ic = (struct ieee80211com *)arg;
    struct ieee80211_recv_mgt_args *entry = NULL;
    wbuf_t  wbuf = QDF_NBUF_NULL;
    struct ieee80211_rx_status  rs;
     struct ieee80211vap *vap ;
    
    /* Dequeue and process */
    do {

        OS_MGMT_LOCKBH(&ic->ic_mgmt_lock);
        wbuf = qdf_nbuf_queue_remove(&ic->ic_mgmt_nbufqueue);
        OS_MGMT_UNLOCKBH(&ic->ic_mgmt_lock);

        if(!wbuf)
            break;
        entry = (struct ieee80211_recv_mgt_args *)qdf_nbuf_get_priv(wbuf);
        ieee80211_recv_mgmt( entry->ni, wbuf, entry->subtype, &rs);

        vap = entry->ni->ni_vap;
        if (vap->iv_evtable) {
            vap->iv_evtable->wlan_receive(vap->iv_ifp,wbuf,IEEE80211_FC0_TYPE_MGT,entry->subtype, &rs);
        }

    }while(1);
    atomic_set(&ic->ic_mgmt_deferflags, DEFER_DONE);
    return;
}


#endif
#endif /* #ifdef ATH_SUPPORT_HTC */
