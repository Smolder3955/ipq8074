/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#include "ol_if_athvar.h"
#include "htc_packet.h"
#include "ol_helper.h"
void
ol_cookie_init(void *ar)
{
    A_UINT32 i;
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)ar;
    struct ol_ath_cookie *soc_cookie = &soc->cookie;
    soc_cookie->cookie_list = NULL;
    soc_cookie->cookie_count = 0;
    OS_MEMZERO(soc_cookie->s_cookie_mem, sizeof(soc_cookie->s_cookie_mem));
    qdf_spinlock_create(&(soc_cookie->cookie_lock));
    for (i = 0; i < MAX_COOKIE_NUM; i++) {
	ol_free_cookie(ar, &(soc_cookie->s_cookie_mem[i]));
    }
}

/* cleanup cookie queue */
void
ol_cookie_cleanup(void *ar)
{
    /* It is gone .... */
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)ar;
    struct ol_ath_cookie *soc_cookie = &soc->cookie;
    qdf_spin_lock_bh(&(soc_cookie->cookie_lock));
    soc_cookie->cookie_list = NULL;
    soc_cookie->cookie_count = 0;
    qdf_spin_unlock_bh(&(soc_cookie->cookie_lock));
}

void
ol_free_cookie(void *ar, struct cookie *cookie)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)ar;
    struct ol_ath_cookie *soc_cookie = &soc->cookie;
    qdf_spin_lock_bh(&(soc_cookie->cookie_lock));
    cookie->arc_list_next = soc_cookie->cookie_list;
    soc_cookie->cookie_list = cookie;
    soc_cookie->cookie_count++;
    qdf_spin_unlock_bh(&(soc_cookie->cookie_lock));
}

/* cleanup cookie queue */
struct cookie *
ol_alloc_cookie(void  *ar)

{
    struct cookie   *cookie;
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)ar;
    struct ol_ath_cookie *soc_cookie = &soc->cookie;
    qdf_spin_lock_bh(&(soc_cookie->cookie_lock));
    cookie = soc_cookie->cookie_list;
    if(cookie != NULL)
    {
        soc_cookie->cookie_list = cookie->arc_list_next;
        soc_cookie->cookie_count--;
    }
    qdf_spin_unlock_bh(&(soc_cookie->cookie_lock));
    return cookie;
}

