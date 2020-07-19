/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
#include <osdep.h>
#include <ieee80211_power_priv.h>

#if UMAC_SUPPORT_AP_POWERSAVE 
#define TIM_BITMAP_GUARD 4
#define MAX_TIM_BITMAP_LENGTH 64                     /* Max allowed TIM bitmap for 512 client is 64 */
void
ieee80211_ap_power_vattach(struct ieee80211vap *vap)
{
    vap->iv_set_tim = ieee80211_set_tim;
    vap->iv_alloc_tim_bitmap = ieee80211_power_alloc_tim_bitmap;

    /*
     * Allocate these only if needed
     */
    if (!vap->iv_tim_bitmap) {
        vap->iv_tim_len = howmany(vap->iv_max_aid,8) * sizeof(u_int8_t);
        vap->iv_tim_bitmap = (u_int8_t *) OS_MALLOC(vap->iv_ic->ic_osdev, MAX_TIM_BITMAP_LENGTH + TIM_BITMAP_GUARD, 0);
        if (vap->iv_tim_bitmap == NULL) {
            printf("%s: no memory for TIM bitmap!\n", __func__);
            vap->iv_tim_len = 0;
        } else {
            OS_MEMZERO(vap->iv_tim_bitmap,vap->iv_tim_len);
        }
    }
    
}

void
ieee80211_ap_power_vdetach(struct ieee80211vap *vap)
{
    if (vap->iv_tim_bitmap != NULL) {
        OS_FREE(vap->iv_tim_bitmap);
        vap->iv_tim_bitmap = NULL;
        vap->iv_tim_len = 0;
        vap->iv_ps_sta = 0;
    }
}
void 
ieee80211_protect_set_tim(struct ieee80211vap *vap)
{
	int set = vap->iv_tim_infor.set;
	u_int16_t aid = vap->iv_tim_infor.aid;
	if (set != (isset(vap->iv_tim_bitmap, aid) != 0)) {
	
		if (set) {
			setbit(vap->iv_tim_bitmap, aid);
			vap->iv_ps_pending++;
		} else {
			vap->iv_ps_pending--;
			clrbit(vap->iv_tim_bitmap, aid);
		}		
		IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
	}
}

/*
 * Indicate whether there are frames queued for a station in power-save mode.
 */
void
ieee80211_set_tim(struct ieee80211_node *ni, int set,bool isr_context)
{
    struct ieee80211vap *vap = ni->ni_vap;
    u_int16_t aid;
    rwlock_state_t lock_state;
    unsigned long flags;

    KASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP 
            || vap->iv_opmode == IEEE80211_M_IBSS ,
            ("operating mode %u", vap->iv_opmode));

    aid = IEEE80211_AID(ni->ni_associd);

	vap->iv_tim_infor.set = set;
	vap->iv_tim_infor.aid = aid;

	if (!isr_context)
	{
		OS_EXEC_INTSAFE(vap->iv_ic->ic_osdev, ieee80211_protect_set_tim, vap);
		
	}
	else
	{
        /* avoid the race with beacon update */
        OS_RWLOCK_WRITE_LOCK_IRQSAVE(&vap->iv_tim_update_lock, &lock_state, flags);

    if (set != (isset(vap->iv_tim_bitmap, aid) != 0)) {

        if (set) {
            setbit(vap->iv_tim_bitmap, aid);
            vap->iv_ps_pending++;
        } else {
	        	vap->iv_ps_pending--;
            clrbit(vap->iv_tim_bitmap, aid);
        }
        IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
    }
		OS_RWLOCK_WRITE_UNLOCK_IRQRESTORE(&vap->iv_tim_update_lock, &lock_state, flags);
	}
}

int ieee80211_power_alloc_tim_bitmap(struct ieee80211vap *vap)
{
    u_int8_t *tim_bitmap = NULL;
    u_int32_t old_len = vap->iv_tim_len;

    //printk("[%s] entry\n",__func__);

    vap->iv_tim_len = howmany(vap->iv_max_aid, 8) * sizeof(u_int8_t);
    tim_bitmap = OS_MALLOC(vap->iv_ic->ic_osdev, MAX_TIM_BITMAP_LENGTH + TIM_BITMAP_GUARD, 0);
    if(!tim_bitmap) {
        vap->iv_tim_len = old_len;
        return -1;
    }    
    OS_MEMZERO(tim_bitmap, vap->iv_tim_len);
    if (vap->iv_tim_bitmap) { 
        OS_MEMCPY(tim_bitmap, vap->iv_tim_bitmap,
            vap->iv_tim_len > old_len ? old_len : vap->iv_tim_len);
        OS_FREE(vap->iv_tim_bitmap);
    }
    vap->iv_tim_bitmap = tim_bitmap;

    //printk("[%s] exits\n",__func__);

    return 0;
}
#undef TIM_BITMAP_GUARD
#undef MAX_TIM_BITMAP_LENGTH
#endif
