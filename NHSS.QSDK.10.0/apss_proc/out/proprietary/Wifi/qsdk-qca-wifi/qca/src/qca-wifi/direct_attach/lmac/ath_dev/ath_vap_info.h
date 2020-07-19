/*
 *  Copyright (c) 2010 Atheros Communications Inc.  All rights reserved.
 */
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _ATH_VAP_INFO_H
#define _ATH_VAP_INFO_H

#ifdef UNINET
#include <uninet_lmac_common.h>
#else
#include <umac_lmac_common.h>
#endif

struct ath_vap_info;
typedef struct ath_vap_info *ath_vap_info_t;

#if ATH_SUPPORT_ATHVAP_INFO
    
void
ath_vap_info_attach(struct ath_softc *sc, struct ath_vap *avp);

void
ath_vap_info_detach(struct ath_softc *sc, struct ath_vap *avp);

int
ath_vap_info_get(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            vap_info_type,
    u_int32_t                   *ret_param1,
    u_int32_t                   *ret_param2);

void
ath_bslot_info_notify_change(struct ath_vap *avp, u_int64_t tsf_offset);

int
ath_reg_vap_info_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask,
    ath_vap_info_notify_func    callback,
    void                        *arg);

int
ath_vap_info_update_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask);

int
ath_dereg_vap_info_notify(
    ath_dev_t   dev,
    int         vap_if_id);

#else   //ATH_SUPPORT_ATHVAP_INFO

/* Benign Functions */
    
#define ath_vap_info_attach(sc, avp)                        /**/ 
#define ath_vap_info_detach(sc, avp)                        /**/ 
#define ath_bslot_info_notify_change(avp, tsf_offset)       /**/ 

static INLINE int
ath_vap_info_get(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            vap_info_type,
    u_int32_t                   *ret_param1,
    u_int32_t                   *ret_param2)
{   return -1;  }

static INLINE int
ath_reg_vap_info_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask,
    ath_vap_info_notify_func    callback,
    void                        *arg)
{
    printk("Error: ATH_VAP_INFO module is not included\n");
    return -1;
}

static INLINE int
ath_vap_info_update_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask)
{   return -1;  }

static INLINE int
ath_dereg_vap_info_notify(
    ath_dev_t   dev,
    int         vap_if_id)
{
    printk("Error: ATH_VAP_INFO module is not included\n");
    return -1;
}

#endif  //ATH_SUPPORT_ATHVAP_INFO

#endif  //_ATH_VAP_INFO_H

