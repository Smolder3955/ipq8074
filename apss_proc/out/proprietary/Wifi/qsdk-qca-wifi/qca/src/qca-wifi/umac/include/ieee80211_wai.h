/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#if ATH_SUPPORT_WAPI
#ifndef __IEEE_80211_WAPI_H
#define __IEEE_80211_WAPI_H

#include <osdep.h>
#include <ieee80211_defines.h>

#define COMMTYPE_GROUP 	0x08
#define WAI_K_MSG		0x11

#define P80211_PACKET_SETWAPI       (u_int16_t)0x0001
#define P80211_PACKET_GETWAPI       (u_int16_t)0x0002
#define P80211_PACKET_SETKEY        (u_int16_t)0x0003
#define P80211_PACKET_GETKEY        (u_int16_t)0x0004

#define WAPI_WAI_REQUEST			(u_int16_t)0x00F1
#define WAPI_STA_AGING				(u_int16_t)0x00F3

#define CALL_WAI_ERR_IE				(u_int16_t)(-1)
#define CALL_WAI_ERR_MSGTYPE		(u_int16_t)(-2)
#define CALL_WAI_ERR_NETLINKSK		(u_int16_t)(-3)
#define CALL_WAI_ERR_NOSKB			(u_int16_t)(-4)

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define ETH_P_WAPI			0x88B4

#define	IEEE80211_WPI_SMS4_KEYLEN		16	/* 128bit */
#define	IEEE80211_WPI_SMS4_IVLEN		16	/* 128bit */
#define	IEEE80211_WPI_SMS4_KIDLEN		1	/* 1 octet */
#define	IEEE80211_WPI_SMS4_PADLEN		1	/* 1 octet */
#define	IEEE80211_WPI_SMS4_CRCLEN		4	/* CRC-32 */
#define	IEEE80211_WPI_SMS4_MICLEN		16	/* trailing MIC */

#define	WPI_KID_OFFSET					0	/* Key index Offset */
#define WPI_KID_LEN						1
#define WPI_IV_LEN						16
#define WPI_PAD_LEN						3
#define WPI_ENCRPYPAD_LEN				16
#define WPI_ERR_IV						-1
#define WPI_ERR_KID						-2
#define WPI_ERR_MIC						-3
#define WPI_ERR_HMAC					-4
#define IWN_HMAC_LEN				    16
#define WPI_HDR  (WPI_KID_LEN + WPI_PAD_LEN+ WPI_IV_LEN)

#define	WAPI_OUI			0x721400
#define	WAPI_VERSION		1		/* current supported version */

/*Cipher suite*/
#define	WAPI_CSE_NULL			0x00 
#define	WAPI_CSE_WPI_SMS4		0x01

/*AKM suite */
#define	WAPI_ASE_NONE			0x00 
#define	WAPI_ASE_WAI_UNSPEC	    0x01
#define	WAPI_ASE_WAI_PSK		0x02
#define	WAPI_ASE_WAI_AUTO		0x03

#define	IEEE80211_WPI_NKID	2
#define	INSTALL_KEY_MAX_INACTIVITY 9

#define	USKREKEYCOUNT		0x4000000
#define	MSKREKEYCOUNT		0x4000000

struct wapi_sta_msg_t
{
    u_int16_t		msg_type;               		
    u_int16_t		datalen;
    u_int8_t		vap_mac[6];    	
    u_int8_t		reserve_data1[2];
    u_int8_t		sta_mac[6];    	
    u_int8_t		reserve_data2[2];
    u_int8_t		gsn[WPI_IV_LEN];
    u_int8_t		wie[256];	
};

struct wapi_ioctl_msg
{
	u_int16_t  msg_type;
	u_int16_t  msglen;
	u_int8_t   msg[96];
};

typedef struct wapi_sta_msg_t wapi_sta_msg;

struct ieee80211vap;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key;
#endif

void longint_add(u_int32_t *a, u_int32_t b, u_int16_t len);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int sta_wapi_init(struct ieee80211vap *vap, struct ieee80211_key *k);
#endif
void wlan_wapi_unicast_rekey(struct ieee80211vap* vap, struct ieee80211_node *ni);
void wlan_wapi_multicast_rekey(struct ieee80211vap* vap, struct ieee80211_node *ni);
int wlan_get_wapirekey_unicast(wlan_if_t vaphandle);
int wlan_get_wapirekey_multicast(wlan_if_t vaphandle);
int wlan_set_wapirekey_unicast(wlan_if_t vaphandle, int value);
int wlan_set_wapirekey_multicast(wlan_if_t vaphandle, int value);
int wlan_set_wapirekey_update(wlan_if_t vaphandle, unsigned char* macaddr);

#endif /*__IEEE_80211_WAPI_H*/

#endif /*ATH_SUPPORT_WAPI*/

