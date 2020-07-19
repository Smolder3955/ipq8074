/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* SON Libraries - Common file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <split.h>

#ifdef GMOCK_UNIT_TESTS
#include "strlcpy.h"
#endif

#include "sonlib_cmn.h"

/****  Arrays to store Ifname, Vendor and Driver handle mappings ****/
//Array for storing ifnames (index of ifname is later used to find drvHandle
static char ifnameList[MAX_VAP_PER_BAND  * MAX_RADIOS][INTFNAMESIZE + 1];

//Array for storing driver handles (indexed by vendor enum)
static struct soncmnDrvHandle *vendorDrvHandleMap[SONCMN_VENDOR_MAX]={NULL};

//Array for storing ifname-drvhandle mapping (indexed by ifnames in ifnameList[])
static struct soncmnDrvHandle *ifnameDrvHandleMap[MAX_VAP_PER_BAND * MAX_RADIOS]={NULL};

//Array for storing Vendor names for each interface (indexed by ifnames in ifnameList[])
static enum soncmnVendors ifnameVendorMap[MAX_VAP_PER_BAND * MAX_RADIOS]={0};

//Arrays for storing radioName-to-Vendor mapping
static char radioNameList[MAX_RADIOS][INTFNAMESIZE + 1];
static enum soncmnVendors radioNameVendorMap[MAX_RADIOS]={0};

/* Storing LBD event handler */
void * soncmnLbdEventHandler;
void * soncmnLbdLinkHandler;
void * soncmnLbdControlHandler;

/* Forward decleration for each vendor specific get_ops functions */
struct soncmnOps * sonGetOps_qti();

/* Forward decleration for each vendor specific get_ops functions */
struct soncmnOps * sonGetOps_ath10k();

/**
 * @brief Function to initialize driver Handle and call vendor Init
 *
 * @param [in] vendor  vendor enum
 *
 * @return driver handle for this vendor
 */
static struct soncmnDrvHandle * soncmnInit(enum soncmnVendors vendor, char *ifname)
{
    struct soncmnDrvHandle * drvhandle = NULL;

    //soncmnLbdEventHandler = NULL;
    dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Initializing for vendor: %d",__func__, (int) vendor);
    //Allocate memory for driver Handle
    drvhandle = (struct soncmnDrvHandle *) calloc(1, sizeof(struct soncmnDrvHandle));
    if(!drvhandle)
    {
        dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: malloc failed",__func__);
        return NULL;
    }
    drvhandle->ctx =NULL;

    /* Call vendor specific Init */
    switch (vendor)
    {
        case SONCMN_VENDOR_QTI:
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Calling vendor specific init for QTI",__func__);
             #ifdef WIFISON_SUPPORT_QSDK
             drvhandle->ops = sonGetOps_qti();
             #endif
            break;
        case SONCMN_VENDOR_ATHXK:
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Calling vendor specific init for ATH10k",__func__);
            #ifdef WIFISON_SUPPORT_ATH10K
            drvhandle->ops = sonGetOps_ath10k();
            #endif
            break;
        case SONCMN_VENDOR_BRCM:
            //drvhandle->ops = son_getops_brcm();
            break;
        case SONCMN_VENDOR_INTC:
            //drvhandle->ops = son_getops_intc();
            break;
        default:
            dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Invalid Vendor Name",__func__);
            goto err;
            break;
    }
    if ( drvhandle->ops->init((drvhandle), ifname))
    {
        dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Vendor init failed", __func__);
        goto err;
    }
    dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Successful Initialization",__func__);
    return drvhandle;
err:
    free(drvhandle);
    return NULL;
}

/**
 * @brief Function to return Vendor enum from Vendor string
 *
 * @param [in] vendorName  vendor name string from config file
 *
 * @return Vendor enum soncmnVendors
 *
 */
static enum soncmnVendors soncmnGetEnumForVendorName( const char * vendorName )
{
   if (strcmp(vendorName,"QTI")==0) {
       return SONCMN_VENDOR_QTI;
   } else if (strcmp(vendorName,"ATHXK")==0) {
       return SONCMN_VENDOR_ATHXK;
   } else if (strcmp(vendorName,"BRCM")==0) {
       return SONCMN_VENDOR_BRCM;
   } else if (strcmp(vendorName,"INTC")==0) {
       return SONCMN_VENDOR_INTC;
   }

  return 0;
}

/**
 * @brief Function to store radioName to Vendor Mapping
 *        It uses two arrays, one for storing radioName
 *        and one for Vendor (both radioName and Vendor
 *        are indexed in same position in arrays)
 *
 * @param [in] radioName  radio name (wifi0, wifi1 etc)
 * @param [in] vendor     vendor name (QTI, BRCM etc)
 *
 */
//NOTE: Currently max radio supported is MAX_RADIOS
static void soncmnStoreRadioVendorMap(const char* radioName, const char *vendor)
{
    int i;
    //Check if this radioName is already stored earlier in radioNameList[]
    for(i=0; i<(MAX_RADIOS); i++) {
        if (strcmp(radioNameList[i],radioName)==0) {
            //This radioName mapping was already stored
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Radio: %s found stored at index %d",
                                              __func__, radioName, i);
            return;
        }
        if (strlen(radioNameList[i]) == 0) {
            //We have hit the empty entry,
            //radioName needs to be stored here
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Radio %s, Vendor %s will be stored at index %d",
                                                __func__, radioName, vendor, i);
            //Add radioName in radioNameList[]
            //Add Vendor in radioNameVendorMap[]
            strlcpy(radioNameList[i], radioName, sizeof(radioNameList[i]));
            radioNameVendorMap[i] = soncmnGetEnumForVendorName(vendor);
            return;
        }
    }
    dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Max Radio limit (%d) reached,"
                                       "Vendor %s cannot be stored in RadioVendorMap",
                                       __func__, MAX_RADIOS, vendor);

    return;

}

/**
 * @brief Function to create ifname, vendor, drvHandle mappings
 *        Called by HYD/LBD during its Init. Goes through list of
 *        ifnames in config file, creating corresponding drvHandles
 *
 * @param [in] wlanInterfaces wlanIfname-to-radio mapping from config file
 * @param [in] radioVendorMap  radio-to-vendor mapping from config file
 * @param [in] isHYD  flag to indicate if HYD is running
 * @param [in] isLBD  flag to indicate if LBD/WLB is running
 *
 */
void soncmnIfnameVendorMapResolve(const char* wlanInterfaces,
           const char* radioVendorMap, uint8_t isHYD, uint8_t isLBD)
{
    /* Format for ifnamePair and rvPair is IfName:Radio & Radio:Vendor respectively */
    char ifnamePair[MAX_VAP_PER_BAND * MAX_RADIOS][1 + 2 * (INTFNAMESIZE + 1)];
    char rvPair[MAX_VAP_PER_BAND * MAX_RADIOS][1 + 2 * (INTFNAMESIZE + 1)];
    u_int8_t i,j = 0;
    int numInterfaces, numRadios;
    static int soncmn_InitDone = 0;
    static int dbgModInit = 0;

    if(!dbgModInit){
        /* Initialize debug module used for logging */
        soncmnDbgS.dbgModule = dbgModuleFind( "sonlib" );
        soncmnDbgS.dbgModule->Level=DBGINFO;
        dbgModInit = 1;
    }

    /* Prevent multiple initializations */
    if (soncmn_InitDone) {
        dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Ifname-Vendor Map Resolve was already done", __func__);
        return;
    }

    if (!wlanInterfaces) {
        dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: No WLAN interface listed in config file", __func__);
        return;
    }

    /* Get number of interfaces */
    numInterfaces = splitByToken(wlanInterfaces,
                              sizeof(ifnamePair) / sizeof(ifnamePair[0]),
                              sizeof(ifnamePair[0]),
                              (char *)ifnamePair, ',');
    if (numInterfaces < MIN_NUM_IFACES) {
        dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Failed to resolve WLAN interfaces from %s:"
                                       " at least one interface per band is required",
                                       __func__, wlanInterfaces);
        return;
    }

    /* Iterate through each interface name */
    for (i = 0; i < numInterfaces; i++) {
        char ifnames[2][INTFNAMESIZE + 1]; //ifnames
        char rvendor_names[2][INTFNAMESIZE + 1];  //radio-vendor map
        char vendor[INTFNAMESIZE + 1] = "QTI";  //vendor names

        if (splitByToken(ifnamePair[i], sizeof(ifnames) / sizeof(ifnames[0]),
                         sizeof(ifnames[0]), (char *) ifnames,
                         ':') != 2) {
            dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Failed to resolve radio and VAP names from %s",
                                                       __func__, ifnamePair[i]);
            return;
        }
        dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Processing for radio:%s : intf: %s", __func__, ifnames[0], ifnames[1]);

        /* If Radio-to-Vendor mapping is not present, assume all radios are QTI */
        if (!radioVendorMap) {
           dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Default vendor QTI is picked "
                      "as no Radio-To-Vendor mapping present in config file", __func__);
        }
        else {
            /* Get radio for this ifname, then vendor for this radio */
            numRadios = splitByToken(radioVendorMap,
                                 sizeof(rvPair) / sizeof(rvPair[0]),
                                 sizeof(rvPair[0]),
                                 (char *)rvPair, ',');
            for (j = 0; j < numRadios; j++) {
                if (splitByToken(rvPair[j], sizeof(rvendor_names) / sizeof(rvendor_names[0]),
                         sizeof(rvendor_names[0]), (char *) rvendor_names,
                         ':') != 2) {
                    dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Picking QTI default vendor"
                             " (failed to resolve radio-vendor map from config)", __func__);
                    break;
                }
                /* Match radio names */
                if (strcmp(rvendor_names[0],ifnames[0]) == 0){
                    dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Found Vendor: %s for Ifname: %s (radio:%s)",
                             __func__,  rvendor_names[1], ifnames[0], rvendor_names[0]);
                    strlcpy(vendor, rvendor_names[1], sizeof(vendor));
                    break; /* Go to next interface name */
                }
            }
        }

        /* We should have ifname and Vendor by now. Initialize mapping arrays */
        //Store radio-vendor mapping in array
        soncmnStoreRadioVendorMap(ifnames[0], vendor);

        // Step1: Copy ifname to ifnameList[i]
        strlcpy(ifnameList[i], ifnames[1], sizeof(ifnames[1]));

        // Step2: Convert vendor to enum, check vendorDrvHandleMap[vendor enum]
        // Null means, drvHandle was not created yet, call Init to create drvHandle
        int vEnum = (int) soncmnGetEnumForVendorName (vendor);
        if( !vendorDrvHandleMap[vEnum] ) {
            //Call init, store drvHandle (indexed by vendor enum)
            vendorDrvHandleMap[vEnum] = soncmnInit(soncmnGetEnumForVendorName (vendor), ifnameList[i]);
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: New DrvHandle (%p) created for vendor %s", __func__,
                                                           vendorDrvHandleMap[vEnum], vendor);
        }
        else {
            //DrvHandle is present i.e. Vendor already initailized before, call only vendor specific init
            dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: DrvHandle (%p) was already created for Vendor %s", __func__,
                                                                   vendorDrvHandleMap[vEnum], vendor);
            vendorDrvHandleMap[vEnum]->ops->init(vendorDrvHandleMap[vEnum], ifnameList[i]);
        }

        // Step3: Populate ifnameDrvHandleMap[ifname index]
        ifnameDrvHandleMap[i] = vendorDrvHandleMap[vEnum];
        ifnameVendorMap[i] = soncmnGetEnumForVendorName (vendor);

    } /* For each interface (numInterfaces) */

    //Set flag that Init was done
    soncmn_InitDone = 1;
    return;
}

/**
 * @brief Function to get index for a given Ifname
 *
 * @param [in] ifname  wlan interface name
 *
 * @return Index for this Ifname
 *
 */
int soncmnGetIfIndex(const char *ifname)
{
    int i, index;
    index = -1;
    //Search this ifname in ifnameList[] and get index.
    for(i=0; i<(MAX_VAP_PER_BAND  * MAX_RADIOS); i++) {
        if (strcmp(ifnameList[i],ifname)==0) {
            index = i;
            break;
        }
    }

    return index;
}

/**
 * @brief Function to get DrvHandle for a given Ifname
 *
 * @param [in] ifname  wlan interface name
 *
 * @return Driver Handle for this ifname
 *
 */
struct soncmnDrvHandle * soncmnGetDrvHandleFromIfname(const char *ifname)
{
    int i = soncmnGetIfIndex(ifname);
    if (i == -1) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,
          "%s: Failed to get index (and DrvHandle) for ifname: %s",__func__, ifname);
        return NULL;
    }
    //return driver handle for this index
    return ifnameDrvHandleMap[i];
}

/**
 * @brief Function to get Vendor name for a given Ifname
 *
 * @param [in] ifname  wlan interface name
 *
 * @return Vendor enum soncmnVendors
 *
 */
enum soncmnVendors soncmnGetVendorFromIfname(const char *ifname)
{
    int i = soncmnGetIfIndex(ifname);
    if (i == -1) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,
          "%s: Failed to get index for ifname: %s",__func__, ifname);
        return i;
    }

    //return Vendor for this index from ifname-Vendor-map
    return ifnameVendorMap[i];
}

/**
 * @brief Function to get Vendor name for a given Radio Name
 *
 * @param [in] ifname  wlan radio name (wifi0, wifi1..)
 *
 * @return Vendor enum soncmnVendors
 *
 */
enum soncmnVendors soncmnGetVendorFromRadioName(const char *radioName)
{
    int i;
    //Search this radio name in radioNameList[] and get index.
    for(i=0; i<(MAX_RADIOS); i++) {
        if (strcmp(radioNameList[i],radioName)==0) {
            //return Vendor for this index from radio-Vendor-map
            return radioNameVendorMap[i];
        }
    }

    //return vendor unknown
    return SONCMN_VENDOR_UNKNOWN;
}

/**
 * @brief Function to free up memory for DrvHandle
 *        and any private memory allocated by vendor
 *        by calling vendor specific deInit
 *
 * @param [in] drvHandle  vendor specific drvHandle
 *
 */
void soncmnDeinit(struct soncmnDrvHandle * drvhandle)
{
    if(!drvhandle) {
        return;
    }

    //Call vendor specific deInit API
    drvhandle->ops->deinit(drvhandle);
    free(drvhandle);
}

/**
 * @brief Function called by LBD to provide Event Handler
 *        pointer to SON Lib, used later to handle events
 *
 * @param [in] evHandler   opaque pointer to LBD event Handler
 *
 */
void soncmnSetLbdEventHandler(void *evHandler)
{
   soncmnLbdEventHandler = evHandler;
   dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s:soncmnLbdEventHandler:%p \n",__func__,soncmnLbdEventHandler);
}

void soncmnSetLbdLinkHandler(void *evHandler)
{
   soncmnLbdLinkHandler = evHandler;
   dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s:soncmnLbdLinkHandler:%p \n",__func__,soncmnLbdLinkHandler);
}

/**
 * @brief Function to obtain LBD Event handler opaque pointer
 *        Used by vendor modules for handling events from driver
 *
 */
void *soncmnGetLbdEventHandler(void)
{
    return soncmnLbdEventHandler;
}
void *soncmnGetLbdControlHandler(void)
{
    return soncmnLbdControlHandler;
}
void *soncmnGetLbdLinkHandler(void)
{
    return soncmnLbdLinkHandler;
}

