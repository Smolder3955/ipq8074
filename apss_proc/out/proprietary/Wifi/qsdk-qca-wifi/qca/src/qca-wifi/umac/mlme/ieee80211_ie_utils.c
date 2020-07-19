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
 *
 *  all the IE parsing/processing routines.
 */
#include <ieee80211_var.h>
#include <ieee80211_ie_utils.h>


/*
* ie processing utilities.
*/
/*
* find an IE with given tag in the frame and return pointer 
* to begining of the found ie.
* frm  : pointer where to begin the search.
* efrm : pointer where to end the search.  
* tag  : tag of the ie to find.
*/
u_int8_t *ieee80211_find_ie(u_int8_t *frm, u_int8_t *efrm,u_int8_t tag)
{
   struct ieee80211_ie_header       *info_element;
   u_int32_t                        remaining_ie_length;

   if (frm == NULL) {
      return NULL;
   }
    info_element = (struct ieee80211_ie_header *) frm;
    remaining_ie_length = efrm-frm;
    /* Walk through to check nothing is malformed */
    while (remaining_ie_length >= sizeof(struct ieee80211_ie_header)) {
        /* At least one more header is present */
        remaining_ie_length -= sizeof(struct ieee80211_ie_header);
        
        if (remaining_ie_length < info_element->length) {
            /* Incomplete/bad info element */
            return NULL; 
        }
        if (info_element->element_id == tag) {
           return (efrm - remaining_ie_length - sizeof(struct ieee80211_ie_header));
        }
        if (info_element->length == 0) {
            info_element += 1;    /* next IE */
            continue;
        }

        /* Consume info element */
        remaining_ie_length -= info_element->length;

        /* Go to next IE */
        info_element = (struct ieee80211_ie_header *) 
            (((u_int8_t *) info_element) + sizeof(struct ieee80211_ie_header) + info_element->length); 
    }

  return NULL;
}

#define IEEE80211_SKIP_LEN(frm,efrm,skip_len) do { \
    if( ((efrm)-(frm)) < skip_len) {               \
          frm=NULL;                                \
     } else {                                      \
          frm += skip_len;                         \
     }                                             \
  } while(0)

/*
** return pointer to ie data portion of the management frame.
*/
u_int8_t *ieee80211_mgmt_iedata(wbuf_t wbuf, int subtype)
{
    struct ieee80211_frame *wh;
    u_int8_t *frm,*efrm;
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);
    frm = (u_int8_t *)&wh[1];

    switch(subtype) {
       case  IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
       IEEE80211_SKIP_LEN(frm ,efrm, 4);
       break;

       case  IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
       IEEE80211_SKIP_LEN( frm,efrm, 10);
       break;

       case  IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
       case  IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
       IEEE80211_SKIP_LEN( frm,efrm, 6);
       break;

       case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
       case IEEE80211_FC0_SUBTYPE_BEACON:
       IEEE80211_SKIP_LEN( frm,efrm, 12);
       break;

       case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
	/* ie data fllows at 0 offset */
       break;

       default:
         frm=NULL;
    }
    return frm;
}

