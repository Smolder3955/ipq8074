/*
 *  Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */


/*
 *  Radio Resource measurements enhancements for rrm on ap side 
 */

#include <ieee80211_var.h>
#include "ieee80211_rrm_priv.h"

#if UMAC_SUPPORT_RRM_MISC
static OS_TIMER_FUNC(rrm_timer);
/**
 * @brief 
 *
 * @param vap
 * @param val
 *
 * @return 
 */
inline int ieee80211_rrm_set_slwindow(wlan_if_t vap, int val)
{
    ieee80211_rrm_t rrm = vap->rrm;
    rrm->slwindow = val;
    return EOK;
}

/**
 * @brief 
 *
 * @param vap
 * @param val
 *
 * @return 
 */

inline int ieee80211_rrm_get_slwindow(wlan_if_t vap)
{
    ieee80211_rrm_t rrm = vap->rrm;
    return rrm->slwindow;
}

/**
 * @brief 
 *
 * @param vap
 * @param val
 *
 * @return 
 */
int ieee80211_set_rrmstats(wlan_if_t vap, int val)
{
    ieee80211_rrm_t rrm = vap->rrm;
    if(val)
    {
        if(atomic_read(&(rrm->timer_running)) == 0)
        {    
            OS_INIT_TIMER(rrm->rrm_osdev, &(rrm->rrm_sliding_window_timer),
            rrm_timer, (void *) (vap), QDF_TIMER_TYPE_WAKE_APPS);
            OS_SET_TIMER(&rrm->rrm_sliding_window_timer,TIME_SEC_TO_MSEC(GRANUALITY_TIMER));
            atomic_set(&(rrm->timer_running),1);    
        }
    }
    else
    { /*stop timer */
        if(atomic_read(&(rrm->timer_running)) == 1)
            OS_FREE_TIMER(&rrm->rrm_sliding_window_timer);
        atomic_set(&(rrm->timer_running),0);    
    }

    rrm->rrmstats = val;
    return EOK;
}

/**
 * @brief 
 *
 * @param vap
 * @param val
 *
 * @return 
 */
inline int ieee80211_get_rrmstats(wlan_if_t vap) 
{
    ieee80211_rrm_t rrm = vap->rrm;
    return rrm->rrmstats;

}
/**
 * @brief 
 *
 * @param pre_type
 *
 * @return 
 */
static int rrm_pre_to_rrm_type(int pre_type)
{
    if(pre_type == 0 )
        return IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ;
    if(pre_type == 1 || pre_type == 2 || pre_type == 3)
        return IEEE80211_MEASREQ_STA_STATS_REQ;
    if(pre_type == 4)
         return IEEE80211_MEASREQ_CHANNEL_LOAD_REQ;
    return EOK;
}

/**
 * @brief 
 *
 * @param rrm_timer
 */
static OS_TIMER_FUNC(rrm_timer)
{
    struct ieee80211vap *vap=NULL;
    ieee80211_rrm_t rrm =NULL;
    OS_GET_TIMER_ARG(vap , struct ieee80211vap *);

    if(NULL == vap) {
        return ;/* should not happen investigate */
    }

    rrm  = vap->rrm;

    /* 
     * Acquire the synchronization object. 
     * This is used to synchronize.
     */

    if (OS_ATOMIC_CMPXCHG(&(rrm->rrm_detach), 0, 1) == 1)
        return;

    if(atomic_read(&(rrm->timer_running)) == 0)
        return ;
    else 
    {
        qdf_sched_work(NULL,&rrm->rrm_work);
        OS_SET_TIMER(&rrm->rrm_sliding_window_timer,TIME_SEC_TO_MSEC(GRANUALITY_TIMER));
        (void) OS_ATOMIC_CMPXCHG(&(rrm->rrm_detach), 1, 0);
    }
    return ;
}

/**
 * @brief 
 *
 * @param arg
 * @param se
 *
 * @return 
 */
QDF_STATUS ieee80211_find_cochannel_ap(void *arg, wlan_scan_entry_t se)
{ 
    ieee80211_rrm_t rrm = (ieee80211_rrm_t)arg;
    struct ieee80211_ath_channel *chan;
    chan = wlan_util_scan_entry_channel(se);
    if(chan->ic_freq == rrm->rrm_vap->iv_bsschan->ic_freq)
        rrm->cochannel++;
    return EOK;
}

/**
 * @brief 
 *
 * @param data
 */
void rrm_event(void *data)
{  
    ieee80211_rrm_t  rrm=NULL;
    u_int8_t bg = 0;
    ieee80211_rrm_chloadreq_info_t chinfo;
    ieee80211_rrm_nhist_info_t nhist;
    ieee80211_rrm_cca_info_t cca;
    ieee80211_rrm_rpihist_info_t rpihist;
    ieee80211_rrm_stastats_info_t stastats; 
    struct ieee80211vap * vap=NULL;
    struct ieee80211com * ic=NULL;
    struct chload *chload=NULL;
    struct ieee80211_node_table *nt=NULL;
    struct ieee80211_node *ni=NULL;
#define DEFAULT_CLASS       0x20
#define DEFAULT_DURATION    0x64    
#define DEFUALT_CHLOAD_TRAP_VAL 60    

    rrm = (ieee80211_rrm_t)data;
    vap = rrm->rrm_vap;
    ic  = vap->iv_ic; 
    nt = &rrm->rrm_vap->iv_ic->ic_sta;
    chload= &rrm->window.chload_window;
    bg = ic->ic_sta_assoc - ic->ic_ht_sta_assoc - ic->ic_nonerpsta;

    if((!rrm->windowstats % 5) 
       && ieee80211_vap_rrm_is_set(vap))
    {
        if(rrm->chloadtrap == 0
         || ((chload->pre_trap_val > DEFUALT_CHLOAD_TRAP_VAL ) && (chload->avg < DEFUALT_CHLOAD_TRAP_VAL)) 
         || ((chload->pre_trap_val < DEFUALT_CHLOAD_TRAP_VAL ) && (chload->avg > DEFUALT_CHLOAD_TRAP_VAL)))
        {
            chload->pre_trap_val = chload->avg;
            rrm->chloadtrap = 1;
            IEEE80211_DELIVER_EVENT_CHLOAD(vap,chload->avg);
        }
        if(ic->ic_nonerpsta == 1
           && (!rrm->nonerptrap)) 
        {
            rrm->nonerptrap = 1;
            /* Raise trap */
            IEEE80211_DELIVER_EVENT_NONERP_JOINED(vap,1);
        }
        else if((!ic->ic_nonerpsta) && rrm->nonerptrap)
        {
            /* Raise trap */
            rrm->nonerptrap = 0;
            IEEE80211_DELIVER_EVENT_NONERP_JOINED(vap,0);
        }
        if(bg && !rrm->bgjointrap)
        {
            rrm->bgjointrap = 1;
            IEEE80211_DELIVER_EVENT_BGJOINED(vap,1);
        }
        else if((!bg) && rrm->bgjointrap)
        {
            rrm->bgjointrap = 0;
            IEEE80211_DELIVER_EVENT_BGJOINED(vap,0);
        }
    }
    rrm->cochannel = 0;
    ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
            ieee80211_find_cochannel_ap,rrm);
    if(rrm->cochanneltrap == 0
       ||((rrm->precochannel >= 2 ) && (rrm->cochannel  < 2))
       ||((rrm->precochannel < 2 ) && (rrm->cochannel > 2 )))
    {
        rrm->cochanneltrap = 1;
        rrm->precochannel = rrm->cochannel;
        IEEE80211_DELIVER_EVENT_COCHANNEL_AP(vap,rrm->cochannel);
    }
    if(rrm->rrmstats)
    {
        IEEE80211_DELIVER_EVENT_BGJOINED(vap,bg);
        IEEE80211_DELIVER_EVENT_NONERP_JOINED(vap,ic->ic_nonerpsta);
        if(ieee80211_vap_rrm_is_set(vap))
            IEEE80211_DELIVER_EVENT_CHLOAD(vap,chload->avg);
        IEEE80211_DELIVER_EVENT_COCHANNEL_AP(vap,rrm->cochannel);
        rrm->rrmstats = 0;
    }
    if(!(rrm->windowstats % STATS_TIMER) 
    && ieee80211_vap_rrm_is_set(vap)
    && rrm->slwindow)
    {
        OS_MEMZERO(&chinfo,sizeof(ieee80211_rrm_chloadreq_info_t));
        OS_MEMZERO(&nhist,sizeof(ieee80211_rrm_nhist_info_t ));
        OS_MEMZERO(&rpihist,sizeof(ieee80211_rrm_rpihist_info_t ));
        OS_MEMZERO(&cca,sizeof(ieee80211_rrm_cca_info_t ));
        OS_MEMZERO(&stastats,sizeof(ieee80211_rrm_stastats_info_t));
        TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
        {
            if(ni->ni_flags & IEEE80211_NODE_RRM)
            {
                switch(rrm_pre_to_rrm_type(ni->ni_rrmreq_type))
                {
                case IEEE80211_MEASREQ_CHANNEL_LOAD_REQ:
                    wlan_send_chload_req(rrm->rrm_vap,ni->ni_macaddr,&chinfo);
                break;
                case IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ:
                    nhist.regclass = DEFAULT_CLASS;
                    nhist.chnum = vap->iv_bsschan->ic_freq;
                    nhist.m_dur = DEFAULT_DURATION;
                    wlan_send_nhist_req(rrm->rrm_vap, ni->ni_macaddr, &nhist);
                break;
                case IEEE80211_MEASREQ_BR_TYPE:
                break;
                case IEEE80211_MEASREQ_FRAME_REQ:
                break;
                case IEEE80211_MEASREQ_STA_STATS_REQ:
                    stastats.gid = rrm->pre_gid ;
                    if(rrm->pre_gid == IEEE80211_STASTATS_GID10)
                        rrm->pre_gid = IEEE80211_STASTATS_GID0;
                    else if (rrm->pre_gid == IEEE80211_STASTATS_GID1)
                        rrm->pre_gid = IEEE80211_STASTATS_GID10;
                    else if(rrm->pre_gid == IEEE80211_STASTATS_GID0)
                        rrm->pre_gid = IEEE80211_STASTATS_GID1;
                    stastats.m_dur = DEFAULT_DURATION; /*default value */
                    wlan_send_stastats_req(rrm->rrm_vap, ni->ni_macaddr,&stastats);
                break;
                default:
                break;
                }
                ni->ni_rrmreq_type++;
                ni->ni_rrmreq_type %=5;
            }
        }
    }
    rrm->windowstats++;
    if(rrm->windowstats > STATS_TIMER)
        rrm->windowstats = 0 ; /* moving back to 0 */
#undef DEFAULT_CLASS
#undef DEFAULT_DURATION    
}

/**
 * @brief 
 *
 * @param rrm
 *
 * @return 
 */
int rrm_attach_misc(ieee80211_rrm_t rrm)
{
    ATH_CREATE_WORK(&rrm->rrm_work, rrm_event,(void *)rrm);
    rrm->pre_gid = 0;
    rrm->slwindow = 0;
    atomic_set(&rrm->rrm_detach, 0);
    atomic_set(&rrm->timer_running, 0);
    return EOK;
}

/**
 * @brief 
 *
 * @param rrm
 *
 * @return 
 */
int rrm_detach_misc(ieee80211_rrm_t rrm)
{
    u_int32_t tick_counter = 0;

#define RRM_DETACH_WAIT 10    
    while (OS_ATOMIC_CMPXCHG((&rrm->rrm_detach), 0, 1) == 1) 
    {
        if (tick_counter++ > 1000) 
        {    // no more than 10ms
            break;
        }
        /* busy wait; can be executed at IRQL <= DISPATCH_LEVEL */
        OS_DELAY(RRM_DETACH_WAIT);   
    }

    if(atomic_read(&rrm->timer_running) ==  1)
        OS_FREE_TIMER(&rrm->rrm_sliding_window_timer);
    atomic_set(&rrm->timer_running, 0);
    (void) OS_ATOMIC_CMPXCHG((&rrm->rrm_detach), 1, 0);
#undef RRM_DETACH_WAIT    
    return EOK;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_vo
 */

void  set_vo_window(ieee80211_rrm_t rrm,u_int8_t new_vo)
{
    struct vodelay *vodelay= &rrm->window.vo_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    vodelay->sliding_vo[vodelay->index] = new_vo;
    vodelay->valid_entries++;
    vodelay->index++;
    vodelay->min = vodelay->sliding_vo[0];
    vodelay->max = vodelay->sliding_vo[0];
    vodelay->last = new_vo;
    for(i = 0; i  < vodelay->valid_entries; i++)
    {
        if(vodelay->min >= vodelay->sliding_vo[i])
            vodelay->min = vodelay->sliding_vo[i];
        if(vodelay->max <= vodelay->sliding_vo[i])
           vodelay->max = vodelay->sliding_vo[i];
    }
    for(i = 0;i  < vodelay->valid_entries; i++)
        avg += vodelay->sliding_vo[i];
    if(vodelay->index >= SLIDING_WINDOW_SIZE)
        vodelay->index %= SLIDING_WINDOW_SIZE;
    if(vodelay->valid_entries > SLIDING_WINDOW_SIZE)
        vodelay->valid_entries = SLIDING_WINDOW_SIZE;
    vodelay->avg = avg/vodelay->valid_entries;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "VODELAY last min max avg %d %d %d %d \n"
            ,vodelay->last,vodelay->min,vodelay->max,vodelay->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "VO SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d %d %d \n",
            vodelay->sliding_vo[0],
            vodelay->sliding_vo[1],
            vodelay->sliding_vo[2],
            vodelay->sliding_vo[3],
            vodelay->sliding_vo[4],
            vodelay->sliding_vo[5],
            vodelay->sliding_vo[6],
            vodelay->sliding_vo[7],
            vodelay->sliding_vo[8],
            vodelay->sliding_vo[9],
            vodelay->sliding_vo[10],
            vodelay->sliding_vo[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_stcnt
 */

void  set_stcnt_window(ieee80211_rrm_t rrm,u_int8_t new_stcnt)
{
    struct stcnt *stcnt = &rrm->window.stcnt_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    stcnt->sliding_stcnt[stcnt->index] = new_stcnt;
    stcnt->valid_entries++;
    if(stcnt->valid_entries >SLIDING_WINDOW_SIZE)
        stcnt->valid_entries = SLIDING_WINDOW_SIZE;
    stcnt->min = stcnt->sliding_stcnt[0];
    stcnt->max = stcnt->sliding_stcnt[0];
    stcnt->last = new_stcnt;
    for(i = 0; i  < stcnt->valid_entries; i++)
    {
        if(stcnt->min >= stcnt->sliding_stcnt[i])
            stcnt->min = stcnt->sliding_stcnt[i];
        if(stcnt->max <= stcnt->sliding_stcnt[i])
            stcnt->max = stcnt->sliding_stcnt[i];
    }
    for(i = 0;i  < stcnt->valid_entries; i++)
        avg += stcnt->sliding_stcnt[i];
    stcnt->index++;
    if(stcnt->index >=SLIDING_WINDOW_SIZE)
        stcnt->index %= SLIDING_WINDOW_SIZE;
    stcnt->avg = avg/stcnt->valid_entries;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "STACNT last min max avg %d %d %d %d \n"
            ,stcnt->last,stcnt->min,stcnt->max,stcnt->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
             "STACNT SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d %d %d \n",
            stcnt->sliding_stcnt[0],
            stcnt->sliding_stcnt[1],
            stcnt->sliding_stcnt[2],
            stcnt->sliding_stcnt[3],
            stcnt->sliding_stcnt[4],
            stcnt->sliding_stcnt[5],
            stcnt->sliding_stcnt[6],
            stcnt->sliding_stcnt[7],
            stcnt->sliding_stcnt[8],
            stcnt->sliding_stcnt[9],
            stcnt->sliding_stcnt[10],
            stcnt->sliding_stcnt[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_be
 */

void  set_be_window(ieee80211_rrm_t rrm,u_int8_t new_be)
{
    struct bedelay *bedelay= &rrm->window.be_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    bedelay->sliding_be[bedelay->index] = new_be;
    bedelay->valid_entries++;
    if(bedelay->valid_entries >SLIDING_WINDOW_SIZE)
        bedelay->valid_entries = SLIDING_WINDOW_SIZE;
    bedelay->min = bedelay->sliding_be[0];
    bedelay->max = bedelay->sliding_be[0];
    bedelay->last = new_be;
    for(i = 0; i  < bedelay->valid_entries; i++)
    {
        if(bedelay->min >= bedelay->sliding_be[i])
            bedelay->min = bedelay->sliding_be[i];
        if(bedelay->max <= bedelay->sliding_be[i])
            bedelay->max = bedelay->sliding_be[i];
    }
    for(i = 0;i  < bedelay->valid_entries; i++)
        avg += bedelay->sliding_be[i];
    bedelay->index++;
    if(bedelay->index >=SLIDING_WINDOW_SIZE)
        bedelay->index %= SLIDING_WINDOW_SIZE;
    bedelay->avg = avg/bedelay->valid_entries;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "BEDELAY  last min max avg %d %d %d %d \n",
            bedelay->last,bedelay->min,bedelay->max,bedelay->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "BEDELAY SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d %d %d \n",
            bedelay->sliding_be[0],
            bedelay->sliding_be[1],
            bedelay->sliding_be[2],
            bedelay->sliding_be[3],
            bedelay->sliding_be[4],
            bedelay->sliding_be[5],
            bedelay->sliding_be[6],
            bedelay->sliding_be[7],
            bedelay->sliding_be[8],
            bedelay->sliding_be[9],
            bedelay->sliding_be[10],
            bedelay->sliding_be[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_anpi
 */
void  set_anpi_window(ieee80211_rrm_t rrm,int8_t new_anpi)
{
     struct anpi *anpi= &rrm->window.anpi_window;
     u_int32_t avg =0;
     u_int8_t i=0;
     anpi->sliding_anpi[anpi->index] = new_anpi;
     anpi->min = anpi->sliding_anpi[0];
     anpi->max = anpi->sliding_anpi[0];
     anpi->valid_entries++;
     anpi->last = new_anpi;
     if(anpi->valid_entries >SLIDING_WINDOW_SIZE)
        anpi->valid_entries = SLIDING_WINDOW_SIZE;
    for(i = 0; i  < anpi->valid_entries; i++)
    {
        if(anpi->min >= anpi->sliding_anpi[i])
            anpi->min = anpi->sliding_anpi[i];
       if(anpi->max <= anpi->sliding_anpi[i])
            anpi->max = anpi->sliding_anpi[i];
    }
    anpi->index++;
    if(anpi->index >=SLIDING_WINDOW_SIZE)
        anpi->index %= SLIDING_WINDOW_SIZE;
    for(i = 0;i  < anpi->valid_entries; i++)
        avg += anpi->sliding_anpi[i];
    anpi->avg = avg/anpi->valid_entries;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "ANPI  last min max avg %d %d %d %d \n",
            anpi->last,anpi->min,anpi->max,anpi->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "ANPI SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d%d %d %d %d %d %d %d %d %d %d \n",
            anpi->sliding_anpi[0],
            anpi->sliding_anpi[1],
            anpi->sliding_anpi[2],
            anpi->sliding_anpi[3],
            anpi->sliding_anpi[4],
            anpi->sliding_anpi[5],
            anpi->sliding_anpi[6],
            anpi->sliding_anpi[7],
            anpi->sliding_anpi[8],
            anpi->sliding_anpi[9],
            anpi->sliding_anpi[10],
            anpi->sliding_anpi[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 */
void  set_per_window(ieee80211_rrm_t rrm)
{
    struct per *per= &rrm->window.per_window;
    if(rrm->window.frmcnt_window.min)
    {
        per->min = rrm->window.ackfail_window.min *100 /rrm->window.frmcnt_window.min;
    }
    else
        per->min = 0;
    if(rrm->window.frmcnt_window.max)
        per->max = rrm->window.ackfail_window.max *100 /rrm->window.frmcnt_window.max;
    else
        per->max = 0;
    if(rrm->window.frmcnt_window.last)
        per->last = rrm->window.ackfail_window.last*100/rrm->window.frmcnt_window.last;
    else 
        per->last= 0;
    if(rrm->window.frmcnt_window.avg)
        per->avg = rrm->window.ackfail_window.avg*100/rrm->window.frmcnt_window.avg;
    else 
        per->avg= 0;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "PER  last min max avg %d %d %d %d \n",
            per->min,per->max,per->last,per->avg);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_frmcnt
 */
void  set_frmcnt_window(ieee80211_rrm_t rrm,u_int32_t new_frmcnt)
{
    struct frmcnt *frmcnt= &rrm->window.frmcnt_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    frmcnt->sliding_frmcnt[frmcnt->index] = new_frmcnt;
    frmcnt->min = frmcnt->sliding_frmcnt[0];
    frmcnt->max = frmcnt->sliding_frmcnt[0];
    frmcnt->valid_entries++;
    frmcnt->last = new_frmcnt;
    if(frmcnt->valid_entries >SLIDING_WINDOW_SIZE)
        frmcnt->valid_entries = SLIDING_WINDOW_SIZE;
    for(i = 0; i  < frmcnt->valid_entries; i++)
    {
        if(frmcnt->min >= frmcnt->sliding_frmcnt[i])
            frmcnt->min = frmcnt->sliding_frmcnt[i];
        if(frmcnt->max <= frmcnt->sliding_frmcnt[i])
            frmcnt->max = frmcnt->sliding_frmcnt[i];
    }
    frmcnt->index++;
    if(frmcnt->index >=SLIDING_WINDOW_SIZE)
        frmcnt->index %= SLIDING_WINDOW_SIZE;
    for(i = 0;i  < frmcnt->valid_entries; i++)
        avg += frmcnt->sliding_frmcnt[i];
    frmcnt->avg = avg/frmcnt->valid_entries;
    set_per_window(rrm);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "FRAMECNT  last min max avg %d %d %d %d \n",
            frmcnt->last,frmcnt->min,frmcnt->max,frmcnt->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "FRAMECNT SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d %d %d \n",
            frmcnt->sliding_frmcnt[0],
            frmcnt->sliding_frmcnt[1],
            frmcnt->sliding_frmcnt[2],
            frmcnt->sliding_frmcnt[3],
            frmcnt->sliding_frmcnt[4],
            frmcnt->sliding_frmcnt[5],
            frmcnt->sliding_frmcnt[6],
            frmcnt->sliding_frmcnt[7],
            frmcnt->sliding_frmcnt[8],
            frmcnt->sliding_frmcnt[9],
            frmcnt->sliding_frmcnt[10],
            frmcnt->sliding_frmcnt[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_ackfail
 */
void  set_ackfail_window(ieee80211_rrm_t rrm,u_int32_t new_ackfail)
{
    struct ackfail *ackfail= &rrm->window.ackfail_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    ackfail->sliding_ackfail[ackfail->index] = new_ackfail;
    ackfail->min = ackfail->sliding_ackfail[0];
    ackfail->max = ackfail->sliding_ackfail[0];
    ackfail->valid_entries++;
    ackfail->last = new_ackfail;
    if(ackfail->valid_entries >SLIDING_WINDOW_SIZE)
        ackfail->valid_entries = SLIDING_WINDOW_SIZE;
    for(i = 0; i  < ackfail->valid_entries; i++)
    {
        if(ackfail->min >= ackfail->sliding_ackfail[i])
            ackfail->min = ackfail->sliding_ackfail[i];
        if(ackfail->max <= ackfail->sliding_ackfail[i])
           ackfail->max = ackfail->sliding_ackfail[i];
    }
    ackfail->index++;
    if(ackfail->index >=SLIDING_WINDOW_SIZE)
        ackfail->index %= SLIDING_WINDOW_SIZE;
    for(i = 0;i  < ackfail->valid_entries; i++)
        avg += ackfail->sliding_ackfail[i];
    ackfail->avg = avg/ackfail->valid_entries;
    set_per_window(rrm);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "failed ack  last min max avg %d %d %d %d \n",
            ackfail->last,ackfail->min,ackfail->max,ackfail->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "ACKFAIL SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d \n",
            ackfail->sliding_ackfail[0],
            ackfail->sliding_ackfail[1],
            ackfail->sliding_ackfail[2],
            ackfail->sliding_ackfail[3],
            ackfail->sliding_ackfail[4],
            ackfail->sliding_ackfail[5],
            ackfail->sliding_ackfail[6],
            ackfail->sliding_ackfail[7],
            ackfail->sliding_ackfail[8],
            ackfail->sliding_ackfail[9],
            ackfail->sliding_ackfail[10],
            ackfail->sliding_ackfail[11]);
    return;
}

/**
 * @brief 
 *
 * @param rrm
 * @param new_chload
 */
void  set_chload_window(ieee80211_rrm_t rrm,u_int8_t new_chload)
{
    struct chload *chload= &rrm->window.chload_window;
    u_int32_t avg =0;
    u_int8_t i=0;
    chload->sliding_chload[chload->index] = new_chload;
    chload->min = chload->sliding_chload[0];
    chload->max = chload->sliding_chload[0];
    chload->valid_entries++;
    chload->last = new_chload;
    if(chload->valid_entries >SLIDING_WINDOW_SIZE)
        chload->valid_entries = SLIDING_WINDOW_SIZE;
    for(i = 0; i < chload->valid_entries; i++)
    {
        if(chload->min >= chload->sliding_chload[i])
            chload->min = chload->sliding_chload[i];
        if(chload->max <= chload->sliding_chload[i])
            chload->max = chload->sliding_chload[i];
    }
    chload->index++;
    if(chload->index >=SLIDING_WINDOW_SIZE)
        chload->index %= SLIDING_WINDOW_SIZE;
    for(i = 0;i  < chload->valid_entries; i++)
        avg += chload->sliding_chload[i];
    chload->avg = avg/chload->valid_entries;
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "chload  last min max avg %d %d %d %d \n",
            chload->last,chload->min,chload->max,chload->avg);
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "CHLOAD SLIDING WINDOW \n","");
    IEEE80211_DPRINTF(rrm->rrm_vap, IEEE80211_MSG_RRM,
            "%d %d %d %d %d %d %d %d %d %d %d %d \n",
            chload->sliding_chload[0],
            chload->sliding_chload[1],
            chload->sliding_chload[2],
            chload->sliding_chload[3],
            chload->sliding_chload[4],
            chload->sliding_chload[5],
            chload->sliding_chload[6],
            chload->sliding_chload[7],
            chload->sliding_chload[8],
            chload->sliding_chload[9],
            chload->sliding_chload[10],
            chload->sliding_chload[11]);
    return;
}
#else 
int rrm_attach_misc(ieee80211_rrm_t rrm)
{
    return EOK;
}    

int rrm_detach_misc(ieee80211_rrm_t rrm)
{
    return EOK;
}
void  set_vo_window(ieee80211_rrm_t rrm,u_int8_t new_vo)
{
    return;
}
void  set_stcnt_window(ieee80211_rrm_t rrm,u_int8_t new_stcnt)
{
    return;
}
void  set_be_window(ieee80211_rrm_t rrm,u_int8_t new_be)
{
    return;
}
void  set_anpi_window(ieee80211_rrm_t rrm,int8_t new_anpi)
{
    return;
}
void  set_per_window(ieee80211_rrm_t rrm)
{
    return;
}
void  set_frmcnt_window(ieee80211_rrm_t rrm,u_int32_t new_frmcnt)
{
    return;
}
void  set_ackfail_window(ieee80211_rrm_t rrm,u_int32_t new_ackfail)
{
    return;
}
void  set_chload_window(ieee80211_rrm_t rrm,u_int8_t new_chload)
{
    return;
}
#endif

