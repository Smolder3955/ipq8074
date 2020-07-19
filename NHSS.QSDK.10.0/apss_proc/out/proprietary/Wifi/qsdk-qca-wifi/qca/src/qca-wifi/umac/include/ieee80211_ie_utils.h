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
#include "ieee80211_options.h"

#ifndef _IEEE80211_IE_UTILS_H
#define _IEEE80211_IE_UTILS_H

/**
 * find an IE with given tag in the frame and return pointer
 * to begining of the found ie.
 * @param frm  : pointer where to begin the search.
 * @param efrm : pointer where to end the search.
 * @param tag  : tag of the ie to find.
 * @return pointer to the matching ie (points to the begining of ie including type).
*/
u_int8_t *ieee80211_find_ie(u_int8_t *frm, u_int8_t *efrm,u_int8_t tag);


/**
 * for a given management frame type, return pointer pointing to the begining of ie data 
 * in the frame  
 * @param wbuf    : wbuf containing the frame.
 * @param subtype : subtye of the management frame.
 * @return pointer to the begining of ie data.
*/
u_int8_t *ieee80211_mgmt_iedata(wbuf_t wbuf, int subtype);


#endif
