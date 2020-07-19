/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

/*
 * This module define the constant and enum that are used between the upper
 * net80211 layer and lower device ath layer.
 * NOTE: Please do not add any functional prototype in here and also do not
 * include any other header files.
 * Only constants, enum, and structure definitions.
 */

#ifndef UMAC_LMAC_COMMON_H
#define UMAC_LMAC_COMMON_H

/* The following define the various VAP Information Types to register for callbacks */
typedef u_int32_t   ath_vap_infotype;
#define ATH_VAP_INFOTYPE_SLOT_OFFSET    (1<<0)


#define INVALID_RSSI_DWORD 0x80808080
#define INVALID_RSSI_WORD 0x80800000

#ifdef ATH_USB
#define    ATH_BCBUF    8        /* should match ATH_USB_BCBUF defined in ath_usb.h */
#elif  defined(QCA_LOWMEM_PLATFORM)
#define    ATH_BCBUF    4       /* number of beacon buffers for 4 VAP support */
#else
#define    ATH_BCBUF    17       /* number of beacon buffers for 17 VAP support */
#endif

#if ATH_SUPPORT_WRAP
#define ATH_VAPSIZE 32
#else
#define ATH_VAPSIZE ATH_BCBUF
#endif


#if ATH_SUPPORT_AP_WDS_COMBO

/*
 * Define the scheme that we select MAC address for multiple AP-WDS combo VAPs on the same radio.
 * i.e. combination of normal & WDS AP vaps.
 *
 * The normal AP vaps use real MAC address while WDS AP vaps use virtual MAC address.
 * The very first AP VAP will just use the MAC address from the EEPROM.
 *
 * - Default HW mac address maps to index 0
 * - Index 1 maps to default HW mac addr + 1
 * - Index 2 maps to default HW mac addr + 2 ...
 *
 * Real MAC address is calculated as follows:
 * for 8 vaps, 3 bits of the last byte are used (bits 4,5,6 and mask of 7) for generating the
 * BSSID of the VAP.
 *
 * Virtual MAC address is calculated as follows:
 * we set the Locally administered bit (bit 1 of first byte) in MAC address,
 * and use the bits 4,5,6 for generating the BSSID of the VAP.
 *
 * For each Real MAC address there will be a corresponding virtual MAC address
 *
 * BSSID bits are used to generate index during GET operation.
 * No of bits used depends on ATH_BCBUF value.
 */

#define ATH_SET_VAP_BSSID_MASK(bssid_mask)							\
    do {											\
	((bssid_mask)[0] &= ~(((ATH_BCBUF - 1) << 4) | 0x02));                                  \
	((bssid_mask)[IEEE80211_ADDR_LEN - 1] &= ~((ATH_BCBUF >> 1) - 1));                      \
    } while(0)

#define ATH_GET_VAP_ID(bssid, hwbssid, id)                                                     	\
    do {											\
	id = bssid[IEEE80211_ADDR_LEN - 1] & ((ATH_BCBUF >> 1) - 1);                            \
	if (bssid[0] & 0x02) id += (ATH_BCBUF >> 1);                                            \
    } while(0)

#define ATH_SET_VAP_BSSID(bssid, hwbssid, id)                                                  	\
    do {											\
	u_int8_t hw_bssid = (hwbssid[0] >> 4) & (ATH_BCBUF - 1);                               	\
	u_int8_t tmp_bssid;									\
	u_int8_t tmp_id = id;									\
	    											\
	if (tmp_id > ((ATH_BCBUF >> 1) - 1)) {                                                     	\
           tmp_id -= (ATH_BCBUF >> 1) ;								\
	   (bssid)[0] &= ~((ATH_BCBUF - 1) << 4);						\
           tmp_bssid = ((1 + hw_bssid) & (ATH_BCBUF - 1));                                    	\
	   (bssid)[0] |= (((tmp_bssid) << 4) | 0x02);                                         	\
	}											\
	hw_bssid = (hwbssid[IEEE80211_ADDR_LEN - 1]) & ((ATH_BCBUF >> 1) - 1);                   \
	tmp_bssid = ((tmp_id + hw_bssid) & ((ATH_BCBUF >> 1) - 1));                                 \
	(bssid)[IEEE80211_ADDR_LEN - 1] |= tmp_bssid;                                          	\
    } while(0)

#else // ATH_SUPPORT_AP_WDS_COMBO

/*
 * Define the scheme that we select MAC address for multiple BSS on the same radio.
 * The very first VAP will just use the MAC address from the EEPROM.
 * For the next 3 VAPs, we set the Locally administered bit (bit 1) in MAC address,
 * and use the next bits as the index of the VAP.
 *
 * The logic used below is as follows:
 * - Default HW mac address maps to index 0
 * - Index 1 maps to default HW mac addr + 1
 * - Index 2 maps to default HW mac addr + 2 ...
 * The macros are used to generate new BSSID bits based on index and also
 * BSSID bits are used to generate index during GET operation.
 * No of bits used depends on ATH_BCBUF value.
 * e.g for 17 vaps, 6  bits are used.
 * in order to support 18 VAPs, use take the location and embed 6 bit VAP -ID at the location
 */

/* Following is the old scheme of generating MACs
 * This scheme should be still maintained and supported.
 * The logic used below is as follows:
 * - Default HW mac address maps to index 0
 * - Index 1 maps to default HW mac addr + 1
 * - Index 2 maps to default HW mac addr + 2 ...
 * The macros are used to generate new BSSID bits based on index and also
 * BSSID bits are used to generate index during GET operation.
 * e.g for 16 vaps, 4 bits are used (bits 4,5,6,7 and mask of 0xF).
 */
#define ATH_VAP_ID_MASK_ALTER 0x0F
/* This is to disable Locally administered bit alteration;
 * By default, this LA bit is being set for modified MAC.
 */
#if ATH_MAC_LA_DISABLE
#define IEEE802_MAC_LOCAL_ADMBIT ( 0x00 )
#else // ATH_MAC_LA_DISABLE
#define IEEE802_MAC_LOCAL_ADMBIT ( 0x02 )
#endif // ATH_MAC_LA_DISABLE

#ifdef ATH_SUPPORT_VAP_ID_LSB

#define ATH_SET_VAP_BSSID_MASK_ALTER(bssid_mask) \
      do {        \
           ((bssid_mask)[0] &= ~( IEEE802_MAC_LOCAL_ADMBIT ));             \
           ((bssid_mask)[5] &= ~(ATH_VAP_ID_MASK_ALTER));                  \
      } while(0)

#define ATH_MAX_PREALLOC_ID 0

#define ATH_GET_VAP_ID_ALTER(bssid, hwbssid, id)                           \
    do {                                                                   \
          u_int8_t hw_bssid  = (hwbssid)[5] & ATH_VAP_ID_MASK_ALTER;       \
          u_int8_t tmp_bssid = (bssid)[5] & ATH_VAP_ID_MASK_ALTER;         \
          id = (((tmp_bssid + (ATH_VAP_ID_MASK_ALTER + 1)) - hw_bssid) & ATH_VAP_ID_MASK_ALTER); \
    } while (0)

#define ATH_SET_VAP_BSSID_ALTER(bssid, hwbssid, id)                        \
    do {                                                                   \
        if (id) {                                                          \
            u_int8_t hw_bssid = ((hwbssid)[5]) & (ATH_VAP_ID_MASK_ALTER);  \
            u_int8_t tmp_bssid = 0;                                        \
            (bssid)[5] &= (ATH_VAP_ID_MASK_ALTER << 4);                    \
            tmp_bssid = ((id + hw_bssid) & ATH_VAP_ID_MASK_ALTER);         \
            (bssid)[5] |= (tmp_bssid);                                     \
            (bssid)[0] |= (IEEE802_MAC_LOCAL_ADMBIT);                      \
        }                                                                  \
    } while(0)

#else // ATH_SUPPORT_VAP_ID_LSB

#define ATH_SET_VAP_BSSID_MASK_ALTER(bssid_mask) ((bssid_mask)[0] &= ~(((ATH_VAP_ID_MASK_ALTER) << 4) | IEEE802_MAC_LOCAL_ADMBIT))

#define ATH_MAX_PREALLOC_ID ((ATH_VAP_ID_MASK_ALTER + 1) >> 1)

#define ATH_GET_VAP_ID_ALTER(bssid, hwbssid, id)                              \
    do {                                                                \
          u_int8_t hw_bssid = ((hwbssid)[0] >> 4) & ATH_VAP_ID_MASK_ALTER;         \
          u_int8_t tmp_bssid = ((bssid)[0] >> 4) & ATH_VAP_ID_MASK_ALTER;          \
          id = (((tmp_bssid + (ATH_VAP_ID_MASK_ALTER + 1)) - hw_bssid) & ATH_VAP_ID_MASK_ALTER) + 1; \
    } while (0)

#define ATH_SET_VAP_BSSID_ALTER(bssid, hwbssid, id)                        \
    do {                                                             \
        if (id) {                                                    \
            u_int8_t hw_bssid = ((hwbssid)[0] >> 4) & (ATH_VAP_ID_MASK_ALTER); \
            u_int8_t tmp_bssid;                                      \
                                                                     \
            (bssid)[0] &= ~(ATH_VAP_ID_MASK_ALTER << 4);                   \
            tmp_bssid = (((id-1) + hw_bssid) & ATH_VAP_ID_MASK_ALTER);     \
            (bssid)[0] |= (((tmp_bssid) << 4) | IEEE802_MAC_LOCAL_ADMBIT); \
        }                                                            \
    } while(0)

#endif // ATH_SUPPORT_VAP_ID_LSB

#define OCTET 8
#define BITS_IN_OCTET 3

/*  The Location where VAP-ID needs to be embedded  */
#ifdef ATH_SUPPORT_VAP_ID_LSB
#define ATH_VAP_ID_LOC ( 42 )
#else // ATH_SUPPORT_VAP_ID_LSB
#define ATH_VAP_ID_LOC ( 0 )
#endif // ATH_SUPPORT_VAP_ID_LSB

/*  Check to observe that VAP-ID is placed at proper Location  */
#if ( ( ATH_VAP_ID_LOC > 0 && ATH_VAP_ID_LOC < 8 ) || ( ATH_VAP_ID_LOC > 42 ) )
#error " Invalid location to embed VAP-ID in MAC Address "
#endif

/*  Maps to proper the proper location in proper BSSID  */
#define ATH_VAP_ID_INDEX ( ATH_VAP_ID_LOC >> BITS_IN_OCTET )
#define ATH_VAP_ID_SHIFT ( ATH_VAP_ID_LOC  - ( ATH_VAP_ID_INDEX << BITS_IN_OCTET ) )

/*  The 6 bit mask for 6 bit VAP-ID  */
#define ATH_VAP_ID_MASK ( 0xfc )

/*  This macro is to calculate the exact bit position where address change
 *  is happening. Difference between ATH_VAP_ID_LOC & ATH_VAP_ID_BIT_LOC,
 *  ATH_VAP_ID_LOC - is the starting position for vap id feild
 *  ATH_VAP_ID_BIT_LOC - where VAP id incrementation is done
 *                     and used by PCU register in case of WAPI
 */
#define ATH_VAP_ID_BIT_LOC(bit_loc) \
    do { \
        u_int8_t new_loc    = (ATH_VAP_ID_LOC + 5);\
        u_int8_t remender   = (new_loc % OCTET); \
        u_int8_t octet_pos = (new_loc / OCTET); \
        bit_loc = (((octet_pos)*OCTET) + (OCTET - remender) - 1); \
    } while (0)

#ifdef ATH_SUPPORT_VAP_ID_LSB
#define ATH_SET_VAP_BSSID_MASK(__bssid_mask)  \
    ((__bssid_mask)[0] &= ~( IEEE802_MAC_LOCAL_ADMBIT )); \
    ((__bssid_mask)[5] &= ~(ATH_VAP_ID_MASK_ALTER))
#else
#define ATH_SET_VAP_BSSID_MASK(__bssid_mask)  \
    ( (__bssid_mask)[0] &= ~( ATH_VAP_ID_MASK  | IEEE802_MAC_LOCAL_ADMBIT ) )
#endif /* ATH_SUPPORT_VAP_ID_LSB */

/*  The ATH_GET_VAP_ID extracts the VAP-ID from the VAP MAC ID
 *  The VAP ID may be distributed over two BSSID's
 *  We construct the VAP-ID using Masks and bit operations
 *  The VAP id is obtained by algebric difference of ID of BSSID and HW_BSSID
 *   We Obtain VAP-ID for VAP's which have Logical Address bit set
 */
#define ATH_GET_VAP_ID(bssid, hwbssid, id)      \
    do {                                                 \
        if (ic->ic_is_macreq_enabled(ic)) { \
            ATH_GET_VAP_ID_ALTER(bssid, hwbssid, id); \
        } else { \
            u_int8_t width = 6 ;         \
            u_int8_t bssid1 = 0;         \
            u_int8_t bssid2 = 0;         \
            u_int8_t hw_bssid2 = 0;      \
            u_int8_t temp_bssid = 0;     \
            u_int8_t temp_hw_bssid = 0;  \
            u_int8_t hw_bssid1 = ( hwbssid[ATH_VAP_ID_INDEX] &   \
                                 ( ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT ) )   \
                                   << ATH_VAP_ID_SHIFT ;        \
            if (ATH_VAP_ID_INDEX < (IEEE80211_ADDR_LEN - 1)) \
                  hw_bssid2 = ( hwbssid[ATH_VAP_ID_INDEX+1] &  \
                                 ( ATH_VAP_ID_MASK << ( OCTET - ATH_VAP_ID_SHIFT ) ) )  \
                                   >> ( OCTET - ATH_VAP_ID_SHIFT ) ;     \
            temp_hw_bssid = ( ( hw_bssid1 | hw_bssid2 ) & ATH_VAP_ID_MASK )\
                                       >> ( OCTET -width ) ;    \
            bssid1 = ( bssid[ATH_VAP_ID_INDEX] & \
                              ( ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT ) ) \
                                << ATH_VAP_ID_SHIFT ;     \
            if (ATH_VAP_ID_INDEX < (IEEE80211_ADDR_LEN - 1)) \
                  bssid2 = ( bssid[ATH_VAP_ID_INDEX+1] & \
                              ( ATH_VAP_ID_MASK << ( OCTET - ATH_VAP_ID_SHIFT ) ) ) \
                               >> ( OCTET - ATH_VAP_ID_SHIFT ) ;      \
            temp_bssid = ( ( bssid1 | bssid2   ) & ATH_VAP_ID_MASK ) \
                                   >> ( OCTET - width ) ;    \
            id = ( temp_bssid  - temp_hw_bssid ) & \
                 ( ATH_VAP_ID_MASK >> ( OCTET - width ) ) ;   \
        }           \
    } while(0)


/*  The ATH_SET_VAP_BSSID function places the VAP-ID at ATH_VAP_ID_LOC
 *  The VAP may be distributed over two BSSID's
 *  We Extract the Existing ID from two BSSID's by bit operations
 *  We construct the existing VAP id and increment it by 'id'
 *  We append the VAP-ID properly to the two BSSID's
 *  We set the Logical MAC adderess bit to '1' to indicated its a Logical MAC
 */

/* At this moment there will only be a MBSSID mac-pool and a non-MBSSID
 * mac-pool. It is assumed that all AP vaps that will be created will be
 * part of the only MBSSID set. It is a conscious design choice to keep
 * only one MBSSID set as of now. Multiple MBSSID sets support will be
 * added in future if requirement arises. All non-MBSSID VAPs will be STA
 * vaps and will be accomodated from the non-MBSSID mac-pool.
 */
#define MBSSID_IS_TX_VAP(ic) ((ic->ic_mbss.transmit_vap == NULL) ? true : false)
#define MBSSID_POOL_SIZE(ic) (1 << ic->ic_mbss.max_bssid)
#define MBSSID_GET_BSSIDX(ic) \
    (qdf_ffz(ic->ic_mbss.bssid_index_bmap[IEEE80211_DEFAULT_MBSS_SET_IDX]) + 1)

#define ATH_BSSID_INDEX 5

#define MBSSID_MAC_CALC_PREPROCESS_BLOCK(ic, bssid, hwbssid, mode)      \
    do {                                                                \
            if (mode != QDF_SAP_MODE) {                                 \
                /* Seperating the non-MBSSID mac-pool from the MBSSID   \
                 * one. We will be ok with this seperation by starting  \
                 * the non-MBSSID mac-pool at an offset of              \
                 * MBSSID_POOL_SIZE(ic) till the time the number of     \
                 * MBSSID vaps and non-MBSSID vaps do not exceed 256    \
                 */                                                     \
                hwbssid[ATH_BSSID_INDEX] &= ~(MBSSID_POOL_SIZE(ic) - 1);\
                hwbssid[ATH_BSSID_INDEX] += MBSSID_POOL_SIZE(ic);       \
            } else {                                                    \
                /* considering max_bssid will be <= 8 for the time being */ \
                bssid[ATH_BSSID_INDEX] &= ~(MBSSID_POOL_SIZE(ic) - 1);  \
                if (!MBSSID_IS_TX_VAP(ic))                              \
                    bssid[ATH_BSSID_INDEX] |= MBSSID_GET_BSSIDX(ic);    \
            }                                                           \
    } while(0)

#define MBSSID_MAC_CALC_POSTPROCESS_BLOCK(ic, hwbssid, saved_hwbssid_byte, mode) \
    do {                                                                  \
        /* Restore hwbssid */                                             \
            if ((mode < QDF_MAX_NO_OF_MODE) && (mode != QDF_SAP_MODE)) {  \
                hwbssid[ATH_BSSID_INDEX] = saved_hwbssid_byte;            \
            }                                                             \
    } while(0)

#define MBSSID_ATH_SET_VAP_BSSID(bssid, hwbssid, id, mode)                    \
    do {                                                                      \
        uint8_t saved_hwbssid_byte = hwbssid[ATH_BSSID_INDEX];                \
        MBSSID_MAC_CALC_PREPROCESS_BLOCK(ic, bssid, hwbssid, mode);           \
        if (mode != QDF_SAP_MODE) {                                           \
            bssid[ATH_BSSID_INDEX] = hwbssid[ATH_BSSID_INDEX];                \
            ATH_SET_VAP_BSSID(bssid, hwbssid, id);                            \
        }                                                                     \
        MBSSID_MAC_CALC_POSTPROCESS_BLOCK(ic, hwbssid, saved_hwbssid_byte, mode); \
    } while(0)

#define ATH_SET_VAP_BSSID(bssid, hwbssid, id)                          \
    do {                                                               \
       if (ic->ic_is_macreq_enabled(ic)) {                             \
               ATH_SET_VAP_BSSID_ALTER(bssid, hwbssid, id);            \
           } else {                                                    \
               if(id) {                                                \
                   u_int8_t width         = 6;                         \
                   u_int8_t hw_bssid2     = 0;                         \
                   u_int8_t temp_hw_bssid = 0;                         \
                   u_int8_t hw_bssid1 = (hwbssid[ATH_VAP_ID_INDEX] &           \
                                        (ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT)) \
                                         << ATH_VAP_ID_SHIFT ;                 \
                   if (ATH_VAP_ID_INDEX < (IEEE80211_ADDR_LEN - 1))            \
                         hw_bssid2 = (hwbssid[ATH_VAP_ID_INDEX+1] &            \
                                        (ATH_VAP_ID_MASK << (OCTET-ATH_VAP_ID_SHIFT))) \
                                         >> (OCTET - ATH_VAP_ID_SHIFT) ;       \
                   temp_hw_bssid = ((hw_bssid1 | hw_bssid2 ) +                 \
                                             (id << (OCTET - width))) &        \
                                             ATH_VAP_ID_MASK ;                 \
                   bssid[ATH_VAP_ID_INDEX] = (bssid[ATH_VAP_ID_INDEX] &        \
                                             (~(ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT)))\
                                             | (temp_hw_bssid >> ATH_VAP_ID_SHIFT) ;\
                   if (ATH_VAP_ID_INDEX < (IEEE80211_ADDR_LEN - 1))            \
                         bssid[ATH_VAP_ID_INDEX+1] =(bssid[ATH_VAP_ID_INDEX+1] & \
                                              (~( ATH_VAP_ID_MASK <<           \
                                              (OCTET - ATH_VAP_ID_SHIFT))))    \
                                              | (temp_hw_bssid <<              \
                                              (OCTET - ATH_VAP_ID_SHIFT)) ;    \
                   bssid[0] |= IEEE802_MAC_LOCAL_ADMBIT ;                      \
               }                                                               \
           }                                                                   \
    }  while(0)

#endif // ATH_SUPPORT_AP_WDS_COMBO


#if ATH_SUPPORT_WRAP
#define WRAP_SET_LA_BSSID_MASK(bssid_mask)  ((bssid_mask)[0] |= 0x2)
#endif

#if ATH_WOW_OFFLOAD
/* Various parameters that are likely to have changed during
 * host sleep. These need to be updated to the supplicant/WLAN
 * driver on host wakeup */
enum {
    WOW_OFFLOAD_REPLAY_CNTR,
    WOW_OFFLOAD_KEY_TSC,
    WOW_OFFLOAD_TX_SEQNUM,
};

/* Information needed from the WLAN driver for
 * offloading GTK rekeying on embedded CPU */
struct wow_offload_misc_info {
#define WOW_NODE_QOS 0x1
    u_int32_t flags;
    u_int8_t myaddr[IEEE80211_ADDR_LEN];
    u_int8_t bssid[IEEE80211_ADDR_LEN];
    u_int16_t tx_seqnum;
    u_int16_t ucast_keyix;
#define WOW_CIPHER_NONE 0x0
#define WOW_CIPHER_AES  0x1
#define WOW_CIPHER_TKIP 0x2
#define WOW_CIPHER_WEP  0x3
    u_int32_t cipher;
    u_int64_t keytsc;
};
#endif /* ATH_WOW_OFFLOAD */

/*
 * *************************
 * Update PHY stats
 * *************************
 */
#if !ATH_SUPPORT_STATS_APONLY
#define WLAN_PHY_STATS(_phystat, _field)	_phystat->_field ++
#else
#define WLAN_PHY_STATS(_phystat, _field)
#endif

#endif  //UMAC_LMAC_COMMON_H
