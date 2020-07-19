/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */
#include <ieee80211_var.h>
#include <ieee80211_objmgr_priv.h>
#include "if_athvar.h"

#define BITMAP_SIZE 32
/* Tranlation table for IEEE flags to PSOC flags */
u_int32_t ieee2psoc_flags[BITMAP_SIZE] = {
       /* psoc flags */
       WLAN_SOC_F_FF,                /*IEEE80211_F_FF                      */
       WLAN_SOC_F_TURBOP,            /*IEEE80211_F_TURBOP                  */
       WLAN_SOC_F_PROMISC,           /*IEEE80211_F_PROMISC                 */
       WLAN_SOC_F_ALLMULTI,          /*IEEE80211_F_ALLMULTI                */
       0,                            /*IEEE80211_F_PRIVACY                 */
       0,                            /*IEEE80211_F_PUREG                   */
       WLAN_SOC_F_SIBSS,             /*IEEE80211_F_SCAN                    */
       0,                            /*IEEE80211_F_SIBSS                   */
       0,                            /*IEEE80211_F_SHSLOT                  */
       WLAN_SOC_F_PMGTON,            /*IEEE80211_F_PMGTON                  */
       0,                            /*IEEE80211_F_DESBSSID                */
       0,                            /*IEEE80211_F_DFS_CHANSWITCH_PENDING  */
       0,                            /*IEEE80211_F_BGSCAN                  */
       0,                            /*IEEE80211_F_SWRETRY                 */
       0,                            /*IEEE80211_F_TXPOW_FIXED             */
       WLAN_SOC_F_IBSSON,            /*IEEE80211_F_IBSSON                  */
       0,                            /*IEEE80211_F_SHPREAMBLE              */
       0,                            /*IEEE80211_F_DATAPAD                 */
       0,                            /*IEEE80211_F_USEPROT                 */
       0,                            /*IEEE80211_F_USEBARKER               */
       0,                            /*IEEE80211_F_TIMUPDATE               */
       0,                            /*IEEE80211_F_WPA1                    */
       0,                            /*IEEE80211_F_WPA2                    */
      /* 0,                          IEEE80211_F_WPA                     */
       0,                            /*IEEE80211_F_DROPUNENC               */
       0,                            /*IEEE80211_F_COUNTERM                */
       0,                            /*IEEE80211_F_HIDESSID                */
       0,                            /*IEEE80211_F_NOBRIDGE                */
       0,                            /*IEEE80211_F_WMEUPDATE               */
       0,                            /*IEEE80211_F_COEXT_DISABLE           */
       WLAN_SOC_F_CHANSWITCH,        /*IEEE80211_F_CHANSWITCH              */
       0,
       0,
};


/* Tranlation table for IEEE ext flags to PSOC flags */
u_int32_t ieee_ext2psoc_flags[BITMAP_SIZE] = {
       0,                         /*IEEE80211_FEXT_WDS                  */
       WLAN_SOC_F_COUNTRYIE,      /*IEEE80211_FEXT_COUNTRYIE            */
       0,                         /*IEEE80211_FEXT_SCAN_PENDING         */
       WLAN_SOC_F_BGSCAN,         /*IEEE80211_FEXT_BGSCAN               */
       WLAN_SOC_F_UAPSD,          /*IEEE80211_FEXT_UAPSD                */
       WLAN_SOC_F_SLEEP,          /*IEEE80211_FEXT_SLEEP                */
       0,                         /*IEEE80211_FEXT_EOSPDROP             */
       WLAN_SOC_F_MARKDFS,        /*IEEE80211_FEXT_MARKDFS              */
       0,                         /*IEEE80211_FEXT_REGCLASS             */
       0,                         /*IEEE80211_FEXT_BLKDFSCHAN           */
       WLAN_SOC_F_CCMPSW_ENCDEC,  /*IEEE80211_FEXT_CCMPSW_ENCDEC        */
       WLAN_SOC_F_HIBERNATION,    /*IEEE80211_FEXT_HIBERNATION          */
       0,                         /*IEEE80211_FEXT_SAFEMODE             */
       WLAN_SOC_F_DESCOUNTRY,     /*IEEE80211_FEXT_DESCOUNTRY           */
       WLAN_SOC_F_PWRCNSTRIE,     /*IEEE80211_FEXT_PWRCNSTRIE           */
       WLAN_SOC_F_DOT11D,         /*IEEE80211_FEXT_DOT11D               */
       0,                         /*IEEE80211_FEXT_RADAR                */
       0,                         /*IEEE80211_FEXT_AMPDU                */
       0,                         /*IEEE80211_FEXT_AMSDU                */
       0,                         /*IEEE80211_FEXT_HTPROT               */
       0,                         /*IEEE80211_FEXT_RESET                */
       0,                         /*IEEE80211_FEXT_APPIE_UPDATE         */
       0,                         /*IEEE80211_FEXT_IGNORE_11D_BEACON    */
       0,                         /*IEEE80211_FEXT_WDS_AUTODETECT       */
       0,                         /*IEEE80211_FEXT_PUREB                */
       0,                         /*IEEE80211_FEXT_HTRATES              */
       0,                         /*IEEE80211_FEXT_HTVIE                */
       0,                         /*IEEE80211_FEXT_AP                   */
       0,                         /*IEEE80211_FEXT_DELIVER_80211        */
       0,                         /*IEEE80211_FEXT_SEND_80211           */
       0,                         /*IEEE80211_FEXT_WDS_STATIC           */
       0,                         /*IEEE80211_FEXT_PURE11N              */
};

/* Tranlation table for IEEE ext2 flags to PSOC flags */
u_int32_t ieee_ext22psoc_flags[BITMAP_SIZE] = {
       0, /*IEEE80211_FEXT2_CSA_WAIT            */
       0, /*IEEE80211_FEXT2_PURE11AC            */
       0, /*IEEE80211_FEXT2_BR_UPDATE           */
       0, /*IEEE80211_FEXT2_STRICT_BW           */
       0, /*IEEE80211_FEXT2_NON_BEACON          */
       0, /*IEEE80211_FEXT2_SON                 */
       0, /*IEEE80211_FEXT2_MBO                 */
       0, /*IEEE80211_FEXT2_AP_INFO_UPDATE      */
       0, /*IEEE80211_FEXT2_BACKHAUL            */
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
};

/* Tranlation table for IEEE flags to PDEV flags */
u_int32_t ieee2pdev_flags[BITMAP_SIZE] = {
   0,                                     /*IEEE80211_F_FF                     */
   0,                                     /*IEEE80211_F_TURBOP                 */
   0,                                     /*IEEE80211_F_PROMISC                */
   0,                                     /*IEEE80211_F_ALLMULTI               */
   0,                                     /*IEEE80211_F_PRIVACY                */
   0,                                     /*IEEE80211_F_PUREG                  */
   0,
   WLAN_PDEV_F_SCAN,                      /*IEEE80211_F_SCAN                   */
   0,
   0,                                     /*IEEE80211_F_SIBSS                  */
   WLAN_PDEV_F_SHSLOT,                    /*IEEE80211_F_SHSLOT                 */
   0,                                     /*IEEE80211_F_PMGTON                 */
   0,                                     /*IEEE80211_F_DESBSSID               */
   WLAN_PDEV_F_DFS_CHANSWITCH_PENDING,    /*IEEE80211_F_DFS_CHANSWITCH_PENDING */
   0,                                     /*IEEE80211_F_BGSCAN                 */
   0,                                     /*IEEE80211_F_SWRETRY                */
   WLAN_PDEV_F_TXPOW_FIXED,               /*IEEE80211_F_TXPOW_FIXED            */
   0,                                     /*IEEE80211_F_IBSSON                 */
   WLAN_PDEV_F_SHPREAMBLE,                /*IEEE80211_F_SHPREAMBLE             */
   WLAN_PDEV_F_DATAPAD,                   /*IEEE80211_F_DATAPAD                */
   WLAN_PDEV_F_USEPROT,                   /*IEEE80211_F_USEPROT                */
   WLAN_PDEV_F_USEBARKER,                 /*IEEE80211_F_USEBARKER              */
   0,                                     /*IEEE80211_F_TIMUPDATE              */
   0,                                     /*IEEE80211_F_WPA1                   */
   0,                                     /*IEEE80211_F_WPA2                   */
                                          /*IEEE80211_F_WPA                    */
   0,                                     /*IEEE80211_F_DROPUNENC              */
   0,                                     /*IEEE80211_F_COUNTERM               */
   0,                                     /*IEEE80211_F_HIDESSID               */
   0,                                     /*IEEE80211_F_NOBRIDGE               */
   0,                                     /*IEEE80211_F_WMEUPDATE              */
   WLAN_PDEV_F_COEXT_DISABLE,             /*IEEE80211_F_COEXT_DISABLE          */
   0,                                     /*IEEE80211_F_CHANSWITCH             */
};

/* Tranlation table for IEEE ext flags to PDEV flags */
u_int32_t ieee_ext2pdev_flags[BITMAP_SIZE] = {
   0,                               /*IEEE80211_FEXT_WDS                 */
   0,                               /*IEEE80211_FEXT_COUNTRYIE           */
   WLAN_PDEV_F_SCAN_PENDING,        /*IEEE80211_FEXT_SCAN_PENDING        */
   0,                               /*IEEE80211_FEXT_BGSCAN              */
   0,                               /*IEEE80211_FEXT_UAPSD               */
   0,                               /*IEEE80211_FEXT_SLEEP               */
   0,                               /*IEEE80211_FEXT_EOSPDROP            */
   0,                               /*IEEE80211_FEXT_MARKDFS             */
   WLAN_PDEV_F_REGCLASS,            /*IEEE80211_FEXT_REGCLASS            */
   WLAN_PDEV_F_BLKDFSCHAN,          /*IEEE80211_FEXT_BLKDFSCHAN          */
   0,                               /*IEEE80211_FEXT_CCMPSW_ENCDEC       */
   0,                               /*IEEE80211_FEXT_HIBERNATION         */
   0,                               /*IEEE80211_FEXT_SAFEMODE            */
   0,                               /*IEEE80211_FEXT_DESCOUNTRY          */
   0,                               /*IEEE80211_FEXT_PWRCNSTRIE          */
   WLAN_PDEV_F_DOT11D,              /*IEEE80211_FEXT_DOT11D              */
   WLAN_PDEV_F_RADAR,               /*IEEE80211_FEXT_RADAR               */
   WLAN_PDEV_F_AMPDU,               /*IEEE80211_FEXT_AMPDU               */
   WLAN_PDEV_F_AMSDU,               /*IEEE80211_FEXT_AMSDU               */
   WLAN_PDEV_F_HTPROT,              /*IEEE80211_FEXT_HTPROT              */
   WLAN_PDEV_F_RESET,               /*IEEE80211_FEXT_RESET               */
   0,                               /*IEEE80211_FEXT_APPIE_UPDATE        */
   WLAN_PDEV_F_IGNORE_11D_BEACON,   /*IEEE80211_FEXT_IGNORE_11D_BEACON   */
   0,                               /*IEEE80211_FEXT_WDS_AUTODETECT      */
   0,                               /*IEEE80211_FEXT_PUREB               */
   WLAN_PDEV_F_HTVIE,               /*IEEE80211_FEXT_HTRATES             */
   0,                               /*IEEE80211_FEXT_HTVIE               */
   0,                               /*IEEE80211_FEXT_AP                  */
   0,                               /*IEEE80211_FEXT_DELIVER_80211       */
   0,                               /*IEEE80211_FEXT_SEND_80211          */
   0,                               /*IEEE80211_FEXT_WDS_STATIC          */
   0,                               /*IEEE80211_FEXT_PURE11N             */
};

/* Tranlation table for IEEE ext2 flags to PDEV flags */
u_int32_t ieee_ext22pdev_flags[BITMAP_SIZE] = {
   0, /*IEEE80211_FEXT2_CSA_WAIT        */
   0, /*IEEE80211_FEXT2_PURE11AC        */
   0, /*IEEE80211_FEXT2_BR_UPDATE       */
   0, /*IEEE80211_FEXT2_STRICT_BW       */
   0, /*IEEE80211_FEXT2_NON_BEACON      */
   0, /*IEEE80211_FEXT2_SON             */
   0, /*IEEE80211_FEXT2_MBO             */
   0, /*IEEE80211_FEXT2_AP_INFO_UPDATE  */
   0, /*IEEE80211_FEXT2_BACKHAUL        */
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
};

/* Tranlation table for IEEE flags to VDEV flags */
u_int32_t ieee2vdev_flags[BITMAP_SIZE] = {
   0,                       /*IEEE80211_F_FF                     */
   0,                       /*IEEE80211_F_TURBOP                 */
   0,                       /*IEEE80211_F_PROMISC                */
   0,                       /*IEEE80211_F_ALLMULTI               */
   WLAN_VDEV_F_PRIVACY,     /*IEEE80211_F_PRIVACY                */
   WLAN_VDEV_F_PUREG,       /*IEEE80211_F_PUREG                  */
   0,                       /*IEEE80211_F_SCAN                   */
   0,                       /*IEEE80211_F_SIBSS                  */
   0,                       /*IEEE80211_F_SHSLOT                 */
   0,                       /*IEEE80211_F_PMGTON                 */
   WLAN_VDEV_F_DESBSSID,    /*IEEE80211_F_DESBSSID               */
   0,                       /*IEEE80211_F_DFS_CHANSWITCH_PENDING */
   WLAN_VDEV_F_BGSCAN,      /*IEEE80211_F_BGSCAN                 */
   WLAN_VDEV_F_SWRETRY,     /*IEEE80211_F_SWRETRY                */
   0,                       /*IEEE80211_F_TXPOW_FIXED            */
   0,                       /*IEEE80211_F_IBSSON                 */
   0,                       /*IEEE80211_F_SHPREAMBLE             */
   0,                       /*IEEE80211_F_DATAPAD                */
   0,                       /*IEEE80211_F_USEPROT                */
   0,                       /*IEEE80211_F_USEBARKER              */
   WLAN_VDEV_F_TIMUPDATE,   /*IEEE80211_F_TIMUPDATE              */
   WLAN_VDEV_F_WPA1,        /*IEEE80211_F_WPA1                   */
   WLAN_VDEV_F_WPA2,        /*IEEE80211_F_WPA2                   */
/*   WLAN_VDEV_F_WPA           IEEE80211_F_WPA                     */
   WLAN_VDEV_F_DROPUNENC,   /*IEEE80211_F_DROPUNENC              */
   WLAN_VDEV_F_COUNTERM,    /*IEEE80211_F_COUNTERM               */
   WLAN_VDEV_F_HIDESSID,    /*IEEE80211_F_HIDESSID               */
   WLAN_VDEV_F_NOBRIDGE,    /*IEEE80211_F_NOBRIDGE               */
   WLAN_VDEV_F_WMEUPDATE,   /*IEEE80211_F_WMEUPDATE              */
   0,                       /*IEEE80211_F_COEXT_DISABLE          */
   0,                       /*IEEE80211_F_CHANSWITCH             */
   0,
   0,
};

/* Tranlation table for IEEE ext flags to VDEV flags */
u_int32_t ieee_ext2vdev_flags[BITMAP_SIZE] = {
   WLAN_VDEV_F_WDS,             /*IEEE80211_FEXT_WDS                  */
   0,                           /*IEEE80211_FEXT_COUNTRYIE            */
   0,                           /*IEEE80211_FEXT_SCAN_PENDING         */
   0,                           /*IEEE80211_FEXT_BGSCAN               */
   WLAN_VDEV_F_UAPSD,           /*IEEE80211_FEXT_UAPSD                */
   WLAN_VDEV_F_SLEEP,           /*IEEE80211_FEXT_SLEEP                */
   WLAN_VDEV_F_EOSPDROP,        /*IEEE80211_FEXT_EOSPDROP             */
   0,                           /*IEEE80211_FEXT_MARKDFS              */
   0,                           /*IEEE80211_FEXT_REGCLASS             */
   0,                           /*IEEE80211_FEXT_BLKDFSCHAN           */
   0,                           /*IEEE80211_FEXT_CCMPSW_ENCDEC        */
   0,                           /*IEEE80211_FEXT_HIBERNATION          */
   0,                           /*IEEE80211_FEXT_SAFEMODE             */
   0,                           /*IEEE80211_FEXT_DESCOUNTRY           */
   0,                           /*IEEE80211_FEXT_PWRCNSTRIE           */
   0,                           /*IEEE80211_FEXT_DOT11D               */
   0,                           /*IEEE80211_FEXT_RADAR                */
   WLAN_VDEV_F_AMPDU,           /*IEEE80211_FEXT_AMPDU                */
   WLAN_VDEV_FEXT_AMSDU,        /*IEEE80211_FEXT_AMSDU                */
   0,                           /*IEEE80211_FEXT_HTPROT               */
   0,                           /*IEEE80211_FEXT_RESET                */
   WLAN_VDEV_F_APPIE_UPDATE,    /*IEEE80211_FEXT_APPIE_UPDATE         */
   0,                           /*IEEE80211_FEXT_IGNORE_11D_BEACON    */
   WLAN_VDEV_F_WDS_AUTODETECT,  /*IEEE80211_FEXT_WDS_AUTODETECT       */
   WLAN_VDEV_F_PUREB,           /*IEEE80211_FEXT_PUREB                */
   WLAN_VDEV_F_HTRATES,         /*IEEE80211_FEXT_HTRATES              */
   0,                           /*IEEE80211_FEXT_HTVIE                */
   WLAN_VDEV_F_AP,              /*IEEE80211_FEXT_AP                   */
   WLAN_VDEV_F_DELIVER_80211,   /*IEEE80211_FEXT_DELIVER_80211        */
   WLAN_VDEV_F_SEND_80211,      /*IEEE80211_FEXT_SEND_80211           */
   WLAN_VDEV_F_WDS_STATIC,      /*IEEE80211_FEXT_WDS_STATIC           */
   WLAN_VDEV_F_PURE11N,         /*IEEE80211_FEXT_PURE11N              */
};

/* Tranlation table for IEEE ext2 flags to VDEV flags */
u_int32_t ieee_ext22vdev_flags[BITMAP_SIZE] = {
   WLAN_VDEV_F_PURE11AC,       /*IEEE80211_FEXT2_PURE11AC            */
   WLAN_VDEV_F_BR_UPDATE,      /*IEEE80211_FEXT2_BR_UPDATE           */
   WLAN_VDEV_F_STRICT_BW,      /*IEEE80211_FEXT2_STRICT_BW           */
   0,                          /*IEEE80211_FEXT2_NON_BEACON          */
   WLAN_VDEV_F_SON,            /*IEEE80211_FEXT2_SON                 */
   WLAN_VDEV_F_MBO,            /*IEEE80211_FEXT2_MBO                 */
   0,                          /*IEEE80211_FEXT2_AP_INFO_UPDATE      */
   0,                          /*IEEE80211_FEXT2_BACKHAUL            */
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
};

/* Tranlation table for IEEE ext2 flags to VDEV EXT flags */
u_int32_t ieee_ext222vdev_flags[BITMAP_SIZE] = {
    0,                           /*IEEE80211_FEXT2_CSA_WAIT            */
    0,                           /*IEEE80211_FEXT2_PURE11AC            */
    0,                           /*IEEE80211_FEXT2_BR_UPDATE           */
    0,                           /*IEEE80211_FEXT2_STRICT_BW           */
    WLAN_VDEV_FEXT_NON_BEACON,   /*IEEE80211_FEXT2_NON_BEACON          */
    0,                           /*IEEE80211_FEXT2_SON                 */
    0,                           /*IEEE80211_FEXT2_MBO                 */
    0,                           /*IEEE80211_FEXT2_AP_INFO_UPDATE      */
    0,                           /*IEEE80211_FEXT2_BACKHAUL            */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};


/* Tranlation table for IEEE ext flags to VDEV EXT flags */
u_int32_t ieee_ext2222vdev_ext_flags[BITMAP_SIZE] = {
   0,                           /*IEEE80211_FEXT_WDS                  */
   0,                           /*IEEE80211_FEXT_COUNTRYIE            */
   0,                           /*IEEE80211_FEXT_SCAN_PENDING         */
   0,                           /*IEEE80211_FEXT_BGSCAN               */
   0,                           /*IEEE80211_FEXT_UAPSD                */
   0,                           /*IEEE80211_FEXT_SLEEP                */
   0,                           /*IEEE80211_FEXT_EOSPDROP             */
   0,                           /*IEEE80211_FEXT_MARKDFS              */
   0,                           /*IEEE80211_FEXT_REGCLASS             */
   0,                           /*IEEE80211_FEXT_BLKDFSCHAN           */
   0,                           /*IEEE80211_FEXT_CCMPSW_ENCDEC        */
   0,                           /*IEEE80211_FEXT_HIBERNATION          */
   WLAN_VDEV_FEXT_SAFEMODE,     /*IEEE80211_FEXT_SAFEMODE             */
   0,                           /*IEEE80211_FEXT_DESCOUNTRY           */
   0,                           /*IEEE80211_FEXT_PWRCNSTRIE           */
   0,                           /*IEEE80211_FEXT_DOT11D               */
   0,                           /*IEEE80211_FEXT_RADAR                */
   0,                           /*IEEE80211_FEXT_AMPDU                */
   0,                           /*IEEE80211_FEXT_AMSDU                */
   0,                           /*IEEE80211_FEXT_HTPROT               */
   0,                           /*IEEE80211_FEXT_RESET                */
   0,                           /*IEEE80211_FEXT_APPIE_UPDATE         */
   0,                           /*IEEE80211_FEXT_IGNORE_11D_BEACON    */
   0,                           /*IEEE80211_FEXT_WDS_AUTODETECT       */
   0,                           /*IEEE80211_FEXT_PUREB                */
   0,                           /*IEEE80211_FEXT_HTRATES              */
   0,                           /*IEEE80211_FEXT_HTVIE                */
   0,                           /*IEEE80211_FEXT_AP                   */
   0,                           /*IEEE80211_FEXT_DELIVER_80211        */
   0,                           /*IEEE80211_FEXT_SEND_80211           */
   0,                           /*IEEE80211_FEXT_WDS_STATIC           */
   0,                           /*IEEE80211_FEXT_PURE11N              */
};

/* Tranlation table for IEEE caps to PSOC caps */
u_int32_t ieee2psoc_caps[BITMAP_SIZE] = {
 /*psoc cap flags*/
    WLAN_SOC_C_WEP,              /*IEEE80211_C_WEP           */
    WLAN_SOC_C_TKIP,             /*IEEE80211_C_TKIP          */
    WLAN_SOC_C_AES,              /*IEEE80211_C_AES           */
    WLAN_SOC_C_AES_CCM,          /*IEEE80211_C_AES_CCM       */
    WLAN_SOC_C_HT,               /*IEEE80211_C_HT            */
    WLAN_SOC_C_CKIP,             /*IEEE80211_C_CKIP          */
    WLAN_SOC_C_FF,               /*IEEE80211_C_FF            */
    WLAN_SOC_C_TURBOP,           /*IEEE80211_C_TURBOP        */
    WLAN_SOC_C_IBSS,             /*IEEE80211_C_IBSS          */
    WLAN_SOC_C_PMGT,             /*IEEE80211_C_PMGT          */
    WLAN_SOC_C_HOSTAP,           /*IEEE80211_C_HOSTAP        */
    WLAN_SOC_C_AHDEMO,           /*IEEE80211_C_AHDEMO        */
    0,                           /*IEEE80211_C_SWRETRY       */
    WLAN_SOC_C_TXPMGT,           /*IEEE80211_C_TXPMGT        */
    WLAN_SOC_C_SHSLOT,           /*IEEE80211_C_SHSLOT        */
    WLAN_SOC_C_SHPREAMBLE,       /*IEEE80211_C_SHPREAMBLE    */
    WLAN_SOC_C_MONITOR,          /*IEEE80211_C_MONITOR       */
    WLAN_SOC_C_TKIPMIC,          /*IEEE80211_C_TKIPMIC       */
    WLAN_SOC_C_WAPI,             /*IEEE80211_C_WAPI          */
    WLAN_SOC_C_WDS_AUTODETECT,   /*IEEE80211_C_WDS_AUTODETECT*/
    0,
    0,
    0,
    WLAN_SOC_C_WPA1,             /*IEEE80211_C_WPA1          */
    WLAN_SOC_C_WPA2,             /*IEEE80211_C_WPA2          */
  /* WLAN_SOC_C_WPA             IEEE80211_C_WPA              */
    WLAN_SOC_C_BURST,            /*IEEE80211_C_BURST         */
    WLAN_SOC_C_WME,              /*IEEE80211_C_WME           */
    WLAN_SOC_C_WDS,              /*IEEE80211_C_WDS           */
    WLAN_SOC_C_WME_TKIPMIC,      /*IEEE80211_C_WME_TKIPMIC   */
    WLAN_SOC_C_BGSCAN,           /*IEEE80211_C_BGSCAN        */
    WLAN_SOC_C_UAPSD,            /*IEEE80211_C_UAPSD         */
    WLAN_SOC_C_DOTH,             /*IEEE80211_C_DOTH          */
                                 /*IEEE80211_C_CRYPTO        */
};

/* Tranlation table for IEEE ext caps to PSOC ext caps */
u_int32_t ieee_ext2psoc_caps[BITMAP_SIZE] = {
   WLAN_SOC_CEXT_FASTCC,        /*IEEE80211_CEXT_FASTCC         */
   WLAN_SOC_CEXT_P2P,           /*IEEE80211_CEXT_P2P            */
   WLAN_SOC_CEXT_MULTICHAN,     /*IEEE80211_CEXT_MULTICHAN      */
   WLAN_SOC_CEXT_PERF_PWR_OFLD, /*IEEE80211_CEXT_PERF_PWR_OFLD  */
   WLAN_SOC_CEXT_11AC,          /*IEEE80211_CEXT_11AC           */
   WLAN_SOC_CEXT_ACS_CHAN_HOP,  /*IEEE80211_ACS_CHANNEL_HOPPING */
   WLAN_SOC_CEXT_STADFS,        /*IEEE80211_CEXT_STADFS         */
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
};

/* Tranlation table for IEEE caps to PDEV caps */
u_int32_t ieee2pdev_caps[BITMAP_SIZE] = {
    WLAN_SOC_C_WEP,              /*IEEE80211_C_WEP           */
    WLAN_SOC_C_TKIP,             /*IEEE80211_C_TKIP          */
    WLAN_SOC_C_AES,              /*IEEE80211_C_AES           */
    WLAN_SOC_C_AES_CCM,          /*IEEE80211_C_AES_CCM       */
    WLAN_SOC_C_HT,               /*IEEE80211_C_HT            */
    WLAN_SOC_C_CKIP,             /*IEEE80211_C_CKIP          */
    WLAN_SOC_C_FF,               /*IEEE80211_C_FF            */
    WLAN_SOC_C_TURBOP,           /*IEEE80211_C_TURBOP        */
    WLAN_SOC_C_IBSS,             /*IEEE80211_C_IBSS          */
    WLAN_SOC_C_PMGT,             /*IEEE80211_C_PMGT          */
    WLAN_SOC_C_HOSTAP,           /*IEEE80211_C_HOSTAP        */
    WLAN_SOC_C_AHDEMO,           /*IEEE80211_C_AHDEMO        */
    0,                           /*IEEE80211_C_SWRETRY       */
    WLAN_SOC_C_TXPMGT,           /*IEEE80211_C_TXPMGT        */
    WLAN_SOC_C_SHSLOT,           /*IEEE80211_C_SHSLOT        */
    WLAN_SOC_C_SHPREAMBLE,       /*IEEE80211_C_SHPREAMBLE    */
    WLAN_SOC_C_MONITOR,          /*IEEE80211_C_MONITOR       */
    WLAN_SOC_C_TKIPMIC,          /*IEEE80211_C_TKIPMIC       */
    WLAN_SOC_C_WAPI,             /*IEEE80211_C_WAPI          */
    WLAN_SOC_C_WDS_AUTODETECT,   /*IEEE80211_C_WDS_AUTODETECT*/
    0,
    0,
    0,
    WLAN_SOC_C_WPA1,             /*IEEE80211_C_WPA1          */
    WLAN_SOC_C_WPA2,             /*IEEE80211_C_WPA2          */
  /* WLAN_SOC_C_WPA             IEEE80211_C_WPA              */
    WLAN_SOC_C_BURST,            /*IEEE80211_C_BURST         */
    WLAN_SOC_C_WME,              /*IEEE80211_C_WME           */
    WLAN_SOC_C_WDS,              /*IEEE80211_C_WDS           */
    WLAN_SOC_C_WME_TKIPMIC,      /*IEEE80211_C_WME_TKIPMIC   */
    WLAN_SOC_C_BGSCAN,           /*IEEE80211_C_BGSCAN        */
    WLAN_SOC_C_UAPSD,            /*IEEE80211_C_UAPSD         */
    WLAN_SOC_C_DOTH,             /*IEEE80211_C_DOTH          */
};

/* Tranlation table for IEEE ext caps to PDEV ext caps */
u_int32_t ieee_ext2pdev_caps[BITMAP_SIZE] = {
   0, /*IEEE80211_CEXT_FASTCC          */
   0, /*IEEE80211_CEXT_P2P             */
   0, /*IEEE80211_CEXT_MULTICHAN       */
   0, /*IEEE80211_CEXT_PERF_PWR_OFLD   */
   0, /*IEEE80211_CEXT_11AC            */
   0, /*IEEE80211_ACS_CHANNEL_HOPPING  */
   0, /*IEEE80211_CEXT_STADFS          */
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
   0,
};

/* Tranlation table for IEEE caps to VDEV caps */
u_int32_t ieee2vdev_caps[BITMAP_SIZE] = {
  /*vdev*/
     0,                         /*IEEE80211_C_WEP                 */
     0,                         /*IEEE80211_C_TKIP                */
     0,                         /*IEEE80211_C_AES                 */
     0,                         /*IEEE80211_C_AES_CCM             */
     0,                         /*IEEE80211_C_HT                  */
     0,                         /*IEEE80211_C_CKIP                */
     0,                         /*IEEE80211_C_FF                  */
     0,                         /*IEEE80211_C_TURBOP              */
     WLAN_VDEV_C_IBSS,          /*IEEE80211_C_IBSS                */
     0,                         /*IEEE80211_C_PMGT                */
     WLAN_VDEV_C_HOSTAP,        /*IEEE80211_C_HOSTAP              */
     WLAN_VDEV_C_AHDEMO,        /*IEEE80211_C_AHDEMO              */
     WLAN_VDEV_C_SWRETRY,       /*IEEE80211_C_SWRETRY             */
     0,                         /*IEEE80211_C_TXPMGT              */
     0,                         /*IEEE80211_C_SHSLOT              */
     0,                         /*IEEE80211_C_SHPREAMBLE          */
     WLAN_VDEV_C_MONITOR,       /*IEEE80211_C_MONITOR             */
     WLAN_VDEV_C_TKIPMIC,       /*IEEE80211_C_TKIPMIC             */
     0,                         /*IEEE80211_C_WAPI                */
     0,                         /*IEEE80211_C_WDS_AUTODETECT      */
     0,
     0,
     0,
     0,                         /*IEEE80211_C_WPA1                */
     0,                         /*IEEE80211_C_WPA2                */
                                /*IEEE80211_C_WPA                 */
     0,                         /*IEEE80211_C_BURST               */
     0,                         /*IEEE80211_C_WME                 */
     WLAN_VDEV_C_WDS,           /*IEEE80211_C_WDS                 */
     WLAN_VDEV_C_WME_TKIPMIC,   /*IEEE80211_C_WME_TKIPMIC         */
     WLAN_VDEV_C_BGSCAN,        /*IEEE80211_C_BGSCAN              */
     0,                         /*IEEE80211_C_UAPSD               */
     0,                         /*IEEE80211_C_DOTH                */
                                /*IEEE80211_C_CRYPTO             */
};

/* Tranlation table for IEEE ext caps to VDEV ext caps */
u_int32_t ieee_ext2vdev_caps[BITMAP_SIZE] = {
     0,  /*IEEE80211_CEXT_FASTCC           */
     0,  /*IEEE80211_CEXT_P2P              */
     0,  /*IEEE80211_CEXT_MULTICHAN        */
     0,  /*IEEE80211_CEXT_PERF_PWR_OFLD    */
     0,  /*IEEE80211_CEXT_11AC             */
     0,  /*IEEE80211_ACS_CHANNEL_HOPPING   */
     0,  /*IEEE80211_CEXT_STADFS           */
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
};

/* Tranlation table for IEEE node flags to PEER flags */
u_int32_t ieee_node2peer_flags[BITMAP_SIZE] = {
      WLAN_PEER_F_AUTH,        /*IEEE80211_NODE_AUTH                 */
      WLAN_PEER_F_QOS,         /*IEEE80211_NODE_QOS                  */
      WLAN_PEER_F_ERP,         /*IEEE80211_NODE_ERP                  */
      WLAN_PEER_F_HT,          /*IEEE80211_NODE_HT                   */
      WLAN_PEER_F_PWR_MGT,     /*IEEE80211_NODE_PWR_MGT              */
      WLAN_PEER_F_TSC_SET,     /*IEEE80211_NODE_TSC_SET              */
      WLAN_PEER_F_UAPSD,       /*IEEE80211_NODE_UAPSD                */
      WLAN_PEER_F_UAPSD_TRIG,  /*IEEE80211_NODE_UAPSD_TRIG           */
      WLAN_PEER_F_UAPSD_SP,    /*IEEE80211_NODE_UAPSD_SP             */
      WLAN_PEER_F_ATH,         /*IEEE80211_NODE_ATH                  */
      WLAN_PEER_F_OWL_WDSWAR,  /*IEEE80211_NODE_OWL_WDSWAR           */
      WLAN_PEER_F_WDS,         /*IEEE80211_NODE_WDS                  */
      WLAN_PEER_F_NOAMPDU,     /*IEEE80211_NODE_NOAMPDU              */
      WLAN_PEER_F_WEPTKIPAGGR, /*IEEE80211_NODE_WEPTKIPAGGR          */
      WLAN_PEER_F_WEPTKIP,     /*IEEE80211_NODE_WEPTKIP              */
      WLAN_PEER_F_TEMP,        /*IEEE80211_NODE_TEMP                 */
      /*IEEE80211_NODE_11NG_VHT_INTEROP_AMSDU_DISABLE */
      WLAN_PEER_F_11NG_VHT_INTEROP_AMSDU_DISABLE,
      WLAN_PEER_F_40MHZ_INTOLERANT,   /*IEEE80211_NODE_40MHZ_INTOLERANT     */
      WLAN_PEER_F_PAUSED,             /*IEEE80211_NODE_PAUSED               */
      WLAN_PEER_F_EXTRADELIMWAR,      /*IEEE80211_NODE_EXTRADELIMWAR        */
      0,                              /*IEEE80211_NODE_NAWDS                */
      WLAN_PEER_F_REQ_20MHZ,          /*IEEE80211_NODE_REQ_20MHZ            */
      WLAN_PEER_F_ATH_PAUSED,         /*IEEE80211_NODE_ATH_PAUSED           */
      WLAN_PEER_F_UAPSD_CREDIT_UPDATE,/*IEEE80211_NODE_UAPSD_CREDIT_UPDATE  */
      WLAN_PEER_F_KICK_OUT_DEAUTH,    /*IEEE80211_NODE_KICK_OUT_DEAUTH      */
      WLAN_PEER_F_RRM,                /*IEEE80211_NODE_RRM                  */
      WLAN_PEER_F_WAKEUP,             /*IEEE80211_NODE_WAKEUP               */
      WLAN_PEER_F_VHT,                /*IEEE80211_NODE_VHT                  */
      WLAN_PEER_F_DELAYED_CLEANUP,    /*IEEE80211_NODE_DELAYED_CLEANUP      */
      WLAN_PEER_F_EXT_STATS,          /*IEEE80211_NODE_EXT_STATS            */
      WLAN_PEER_F_LEAVE_ONGOING,      /*IEEE80211_NODE_LEAVE_ONGOING        */
};

/* Tranlation table for IEEE node ext flags to PEER flags */
u_int32_t ieee_node2peer_extflags[BITMAP_SIZE] = {
      WLAN_PEER_F_BSTEERING_CAPABLE, /*IEEE80211_NODE_BSTEERING_CAPABLE */
      WLAN_PEER_F_LOCAL_MESH_PEER,   /*IEEE80211_LOCAL_MESH_PEER        */
      0,                             /*IEEE80211_NODE_NON_DOTH_STA      */
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
};

/*
 * This API iterates through the given word gets first least significant bit
 * index is set
 */
uint8_t wlan_get_flag_ix(uint32_t flag)
{
   uint8_t ix = 0xff;
   uint8_t iter = 0;
   uint32_t mask[8] = {0xf,0xf0,0xf00,0xf000,0xf0000,0xf00000,0xf000000,
                       0xf0000000};

   while(iter < 8) {
      if(flag & mask[iter]) {
         ix = iter*4;
         while(ix< ((iter+1)*4)) {
           if(flag & (0x1<<ix)) {
              break;
           }
           ix++;
        }
        break;
      }
      iter++;
   }
   return ix;
}

/*
 * This API sets the flag in PSOC object
 */
void wlan_ic_psoc_set_flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_psoc_nif_feat_cap_set(psoc, ieee2psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_set_flag);

/*
 * This API clears the flag in PSOC object
 */
void wlan_ic_psoc_clear_flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_psoc_nif_feat_cap_clear(psoc, ieee2psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_clear_flag);

/*
 * This API set the ext flag in PSOC object
 */
void wlan_ic_psoc_set_extflag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_psoc_nif_feat_cap_set(psoc, ieee_ext2psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_set_extflag);

/*
 * This API clears the ext flag in PSOC object
 */
void wlan_ic_psoc_clear_extflag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_psoc_nif_feat_cap_clear(psoc, ieee_ext2psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the ext2 flag in PSOC object
 */
void wlan_ic_psoc_set_ext2flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_psoc_nif_feat_cap_set(psoc, ieee_ext22psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears the ext2 flag in PSOC object
 */
void wlan_ic_psoc_clear_ext2flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_psoc_nif_feat_cap_clear(psoc, ieee_ext22psoc_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the flag in PDEV object
 */
void wlan_ic_pdev_set_flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_feat_cap_set(pdev, ieee2pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_pdev_set_flag);

/*
 * This API clears the flag in PDEV object
 */
void wlan_ic_pdev_clear_flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_feat_cap_clear(pdev, ieee2pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_pdev_clear_flag);

/*
 * This API sets the ext flag in PDEV object
 */
void wlan_ic_pdev_set_extflag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_feat_cap_set(pdev, ieee_ext2pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_pdev_set_extflag);

/*
 * This API clears the ext flag in PDEV object
 */
void wlan_ic_pdev_clear_extflag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag",__func__);
         return;
      }
      wlan_pdev_nif_feat_cap_clear(pdev, ieee_ext2pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the ext2 flag in PDEV object
 */
void wlan_ic_pdev_set_ext2flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL",__func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_feat_cap_set(pdev, ieee_ext22pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears the ext2 flag in PDEV object
 */
void wlan_ic_pdev_clear_ext2flag(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_feat_cap_clear(pdev, ieee_ext22pdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the flag in VDEV object
 */
void wlan_vap_vdev_set_flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s:failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_set(vdev, ieee2vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_vap_vdev_set_flag);

/*
 * This API clears the flag in VDEV object
 */
void wlan_vap_vdev_clear_flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }

   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_clear(vdev, ieee2vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_vap_vdev_clear_flag);

/*
 * This API sets the ext flag in VDEV object
 */
void wlan_vap_vdev_set_extflag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_set(vdev, ieee_ext2vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_vap_vdev_set_extflag);

/*
 * This API clears the ext flag in VDEV object
 */
void wlan_vap_vdev_clear_extflag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_clear(vdev, ieee_ext2vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the ext2 flag in VDEV object
 */
void wlan_vap_vdev_set_ext2flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_set(vdev, ieee_ext22vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_vap_vdev_set_ext2flag);

/*
 * This API clears the ext2 flag in VDEV object
 */
void wlan_vap_vdev_clear_ext2flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_cap_clear(vdev, ieee_ext22vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_vap_vdev_clear_ext2flag);

/*
 * This API sets the ext2 flag in VDEV object
 */
void wlan_vap_vdev_set_ext22flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         return;
      }
      wlan_vdev_mlme_feat_ext_cap_set(vdev, ieee_ext222vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears the ext2 flag in VDEV object
 */
void wlan_vap_vdev_clear_ext22flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_ext_cap_clear(vdev, ieee_ext222vdev_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets IEEE ext flag in VDEV ext flags
 */
void wlan_vap_vdev_set_ext222flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         return;
      }
      wlan_vdev_mlme_feat_ext_cap_set(vdev, ieee_ext2222vdev_ext_flags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears IEEE ext flag in VDEV ext flags
 */
void wlan_vap_vdev_clear_ext222flag(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_feat_ext_cap_clear(vdev, ieee_ext2222vdev_ext_flags[ix]);
      flag &= ~(1<<ix);
   }
}
/*
 * This API sets the cap in PSOC object
 */
void wlan_ic_psoc_set_cap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
     wlan_psoc_nif_fw_cap_set(psoc, ieee2psoc_caps[ix]);
     flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_set_cap);

/*
 * This API clears the cap in PSOC object
 */
void wlan_ic_psoc_clear_cap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_psoc_nif_fw_cap_clear(psoc, ieee2psoc_caps[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_clear_cap);

/*
 * This API sets the ext cap in PSOC object
 */
void wlan_ic_psoc_set_extcap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_psoc_nif_fw_ext_cap_set(psoc, ieee_ext2psoc_caps[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_set_extcap);

/*
 * This API clears the ext cap in PSOC object
 */
void wlan_ic_psoc_clear_extcap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;
   struct wlan_objmgr_psoc *psoc;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   psoc = wlan_pdev_get_psoc(pdev);
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_psoc_nif_fw_ext_cap_clear(psoc, ieee_ext2psoc_caps[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_psoc_clear_extcap);

/*
 * This API sets the cap in PDEV object
 */
void wlan_ic_pdev_set_cap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_fw_cap_set(pdev, ieee2pdev_caps[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_pdev_set_cap);

/*
 * This API clears the cap in PDEV object
 */
void wlan_ic_pdev_clear_cap(struct ieee80211com *ic, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_pdev *pdev;

   pdev = ic->ic_pdev_obj;
   if(pdev == NULL) {
      QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
              "%s:pdev is NULL", __func__);
      return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_pdev_nif_fw_cap_clear(pdev, ieee2pdev_caps[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_ic_pdev_clear_cap);

/*
 * This API sets the ext cap in PDEV object
 */
void wlan_ic_pdev_set_extcap(struct ieee80211com *ic, uint32_t flag)
{
}
EXPORT_SYMBOL(wlan_ic_pdev_set_extcap);

/*
 * This API clears the ext cap in PDEV object
 */
void wlan_ic_pdev_clear_extcap(struct ieee80211com *ic, uint32_t flag)
{
}
EXPORT_SYMBOL(wlan_ic_pdev_clear_extcap);

/*
 * This API sets the cap in VDEV object
 */
void wlan_vap_vdev_set_cap(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_cap_set(vdev, ieee2vdev_caps[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears the cap in VDEV object
 */
void wlan_vap_vdev_clear_cap(struct ieee80211vap *vap, uint32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

   if(vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s: vdev is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_vdev_mlme_cap_clear(vdev, ieee2vdev_caps[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API sets the flag in PEER object
 */
void wlan_node_peer_set_flag(struct ieee80211_node *ni, u_int32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_peer *peer = ni->peer_obj;

   if(peer == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s:PEER is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_peer_mlme_flag_set(peer, ieee_node2peer_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_node_peer_set_flag);

/*
 * This API clears the flag in PEER object
 */
void wlan_node_peer_clear_flag(struct ieee80211_node *ni, u_int32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_peer *peer = ni->peer_obj;

   if(peer == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s:PEER is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_peer_mlme_flag_clear(peer, ieee_node2peer_flags[ix]);
      flag &= ~(1<<ix);
   }
}
EXPORT_SYMBOL(wlan_node_peer_clear_flag);

/*
 * This API sets the ext flag in PEER object
 */
void wlan_node_peer_set_extflag(struct ieee80211_node *ni, u_int32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_peer *peer = ni->peer_obj;

   if(peer == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s:PEER is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_peer_mlme_flag_set(peer, ieee_node2peer_extflags[ix]);
      flag &= ~(1<<ix);
   }
}

/*
 * This API clears the ext flag in PEER object
 */
void wlan_node_peer_clear_extflag(struct ieee80211_node *ni, u_int32_t flag)
{
   uint8_t ix;
   struct wlan_objmgr_peer *peer = ni->peer_obj;

   if(peer == NULL) {
        QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                "%s:PEER is NULL", __func__);
        return;
   }
   /* Since translation is required, each flag has to be set seperately */
   while(flag) {
      ix = wlan_get_flag_ix(flag);
      if(ix == 0xff || ix >= BITMAP_SIZE) {
         QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_ERROR,
                 "%s: failed to update the flag", __func__);
         return;
      }
      wlan_peer_mlme_flag_clear(peer, ieee_node2peer_extflags[ix]);
      flag &= ~(1<<ix);
   }
}
