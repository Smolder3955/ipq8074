/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Cache Manager

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Cache Manager

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <sys/param.h>
#include <osal/os_api.h>
#include <lowi_server/lowi_cache_manager.h>
#include <base_util/log.h>
#include <common/lowi_utils.h>
#include <lowi_server/lowi_measurement_result.h>

using namespace qc_loc_fw;

const char * const LOWICacheManager::TAG = "LOWICacheManager";

namespace qc_loc_fw
{
int list_comparator(const LOWIScanMeasurement lhs, const LOWIScanMeasurement rhs)
{
  if (lhs.measurementsInfo[0]->rssi_timestamp > rhs.measurementsInfo[0]->rssi_timestamp)
  {
    return -1;
  }
  else if (lhs.measurementsInfo[0]->rssi_timestamp < rhs.measurementsInfo[0]->rssi_timestamp)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
}

LOWICacheManager::LOWICacheManager(uint32 limit)
{
  log_verbose (TAG, "LOWICacheManager");
  mLimit = limit;
  mLowestAgeMs = -1;
  mLastMeasurementTs = -1;
  associatedToAp = false;
  localStaMacValid = false;
}

LOWICacheManager::~LOWICacheManager()
{
  log_verbose (TAG, "~LOWICacheManager");
}

bool LOWICacheManager::resetCache()
{
  bool retVal = true;
  log_verbose (TAG, "resetCache");
  return retVal;
}

void LOWICacheManager::storeStaMacInCache (LOWIMacAddress staMac)
{
  /** Store flag indicating if local STA's MAC is valid */
  localStaMacValid = true;
  /** Store Local STA Mac address */
  localStaMac = staMac;
  log_verbose(TAG, "%s - Stored the following Local STA MAC address: " QUIPC_MACADDR_FMT ,
              __FUNCTION__, QUIPC_MACADDR(localStaMac));
}

bool LOWICacheManager::getStaMacInCache (LOWIMacAddress &staMac)
{
  staMac = localStaMac;
  log_verbose(TAG, "%s - The stored Local STA MAC address is: " QUIPC_MACADDR_FMT ,
              __FUNCTION__, QUIPC_MACADDR(localStaMac));
  return localStaMacValid;
}

bool LOWICacheManager::putInCache (vector<LOWIScanMeasurement*> & measurements)
{
  bool retVal = true;
  log_verbose (TAG, "putInCache");
  if (measurements.getNumOfElements() != 0)
  {
    List<LOWIScanMeasurement> newMeasurements;
    log_debug(TAG, "Meas to Put in Cache: %u", measurements.getNumOfElements());
    for (unsigned int i = 0; i< measurements.getNumOfElements(); ++i)
    {
      log_verbose(TAG, "Adding to new Meas List - %u", i+1);
      newMeasurements.add(*(measurements[i]));
    }
    if (mMeasurementCache.getSize() != 0)
    {
      /** Update BSSIDs in the cache with new measurements if
       *  new measurements have arrived for them.
       */
      updateCachedMeas(newMeasurements);
    }
    /** Add new BSSIDs to the Cache */
    addToCache(newMeasurements);
  }
  else
  {
    log_info(TAG, "No measurements to put in cache");
  }
  return retVal;
}

bool LOWICacheManager::getAssociatedAP (LOWIScanMeasurement &outBssidMeas)
{
  if (associatedToAp)
  {
    outBssidMeas = associatedApMeas;
    log_verbose(TAG, "%s - STA is Associated to: " QUIPC_MACADDR_FMT,
                __FUNCTION__,QUIPC_MACADDR(outBssidMeas.bssid));
  }
  else
  {
    log_verbose(TAG, "%s - STA is NOT Associated", __FUNCTION__);
  }

  return associatedToAp;
}

bool LOWICacheManager::updateCachedMeas (List<LOWIScanMeasurement> &newMeasurements)
{
  bool retVal = true;
  log_verbose (TAG, "updateCachedMeas");

  for (List<LOWIScanMeasurement>::Iterator cacheItr = mMeasurementCache.begin();
       cacheItr != mMeasurementCache.end();
       ++cacheItr)
  {
    LOWIScanMeasurement scanMeas = *cacheItr;
    for (List<LOWIScanMeasurement>::Iterator newMeasItr = newMeasurements.begin(); newMeasItr != newMeasurements.end(); ++newMeasItr)
    {
      LOWIScanMeasurement newScanMeas = *newMeasItr;
      if (scanMeas.bssid.compareTo(newScanMeas.bssid) == 0)
      {
        log_info(TAG, "BSSID found in cache, updating...");
        /** Found BSSID in Cache, update it and move on
         *  Note: Add further checks here if needed before updating the
         *  Cached BSSID */
        LOWIScanMeasurement *scanMeasPtr = cacheItr.ptr();
        if (scanMeasPtr != NULL)
        {
          *(scanMeasPtr) = newScanMeas;
          /* Check to see if we are now associated with this AP and update associated Status*/
          if (scanMeasPtr->associatedToAp == true)
          {
            associatedToAp = true;
            associatedApMeas = *(scanMeasPtr);
            log_verbose(TAG, "%s - Associated to AP: " QUIPC_MACADDR_FMT,
                        __FUNCTION__,QUIPC_MACADDR(associatedApMeas.bssid));
          }
        }
        /* Remove the BSSID from the new scan measurement list */
        newMeasurements.erase(newMeasItr);
        break;
      }
    }
  }
  return retVal;
}

bool LOWICacheManager::addToCache (List<LOWIScanMeasurement> &newMeasurements)
{
  bool retVal = true;
  log_verbose(TAG, "%s", __FUNCTION__);


  if (newMeasurements.getSize() > (mLimit - mMeasurementCache.getSize()))
  {
    log_verbose(TAG, "Cache Full, Purging...");

    unsigned int excessMeas = newMeasurements.getSize() - (mLimit - mMeasurementCache.getSize());

    /* Purge 1/2 of the cache */
    unsigned int purgeCount = mLimit/2;

    /** If the purgint 1/2 the cache is not enought make it
     *  equal to the amount of space needed
     */
    purgeCount = MAX(purgeCount, excessMeas);

    /** If the space needed exceeds the cache size, make it equal
     *  to the cache Size.
     */
    purgeCount = MIN(purgeCount, mLimit);

    unsigned int cacheElemetsToKeep = mLimit - purgeCount;

    log_debug(TAG, "%s - Cache Full, Purging... Cache Limit: %u, Purging: %u", __FUNCTION__, mLimit, purgeCount);

    mMeasurementCache.sort();

    List<LOWIScanMeasurement>::Iterator it = mMeasurementCache.begin();

    while (it != mMeasurementCache.end())
    {
      if (cacheElemetsToKeep)
      {
        cacheElemetsToKeep--;
        ++it;
      }
      else
      {
        it = mMeasurementCache.erase(it);
      }
    }

    log_verbose(TAG, "Purging Done");
  }
  else
  {
    log_verbose(TAG,"Cache Not Full");
  }

  for (List<LOWIScanMeasurement>::Iterator pIt = newMeasurements.begin(); pIt != newMeasurements.end(); ++pIt)
  {
    if (mLimit > mMeasurementCache.getSize())
    {
      LOWIScanMeasurement newScanMeas = *pIt;
      mMeasurementCache.add(newScanMeas);
      /* Check to see if we are now associated with this AP and update associated Status*/
      if (newScanMeas.associatedToAp)
      {
        associatedToAp = true;
        associatedApMeas = newScanMeas;
        log_verbose(TAG, "%s - Associated to AP: " QUIPC_MACADDR_FMT,
                    __FUNCTION__,QUIPC_MACADDR(associatedApMeas.bssid));
      }
      log_verbose(TAG, "%s - Size of Cache: %u, Size of Element being added to Cache: %u, New size of Cache: %u",
                  __FUNCTION__,
                  sizeof(mMeasurementCache),
                  sizeof(newScanMeas),
                  mMeasurementCache.getSize());
    }
    else
    {
      log_warning(TAG, "%s - Cache Full! - this should never happen", __FUNCTION__);
      retVal = false;
      break;
    }
  }

  return retVal;
}

int32 LOWICacheManager::getFromCache (int64 timestamp,
    vector<LOWIScanMeasurement*> & v)
{
  int32 retVal = -1;
  log_verbose (TAG, "getFromCache");
  return retVal;
}

int32 LOWICacheManager::getFromCache (int64 cache_timestamp,
    int64 fallback_timestamp,
    LOWIDiscoveryScanRequest::eBand band,
    vector<LOWIScanMeasurement*> & v,
    int64& latest_cached_timestamp)
{
  int32 retVal = -1;
  log_verbose (TAG, "getFromCache");
  return retVal;
}

int32 LOWICacheManager::getFromCache (int64 cache_timestamp,
    int64 fallback_timestamp,
    vector <LOWIChannelInfo> & chanInfo,
    vector<LOWIScanMeasurement*> & v,
    int64& latest_cached_timestamp)
{
  int32 retVal = -1;
  log_verbose (TAG, "getFromCache");
  return retVal;
}

boolean LOWICacheManager::getFromCache (LOWIMacAddress bssid,
                                        LOWIScanMeasurement &scanMeasurement)
{
  boolean retVal = false;
  log_verbose(TAG, "%s - BSSID: " QUIPC_MACADDR_FMT, __FUNCTION__,QUIPC_MACADDR(bssid));
  for (List<LOWIScanMeasurement>::Iterator it = mMeasurementCache.begin(); it != mMeasurementCache.end(); ++it)
  {
    LOWIScanMeasurement scanMeas = *it;
    /** If the BSSID and operating channel match, return the
     *  scan Measurement from Cache */
    if (scanMeas.bssid.compareTo(bssid) == 0)
    {
      log_verbose(TAG, "%s: Found BSSID" QUIPC_MACADDR_FMT " in Cache - Freq: %u, secondary Freq: %u, info: %u tsfDelta: %u",
                  __FUNCTION__,
                  QUIPC_MACADDR(scanMeas.bssid),
                  scanMeas.frequency,
                  scanMeas.band_center_freq1,
                  scanMeas.info,
                  scanMeas.tsfDelta);
      scanMeasurement = scanMeas;
      retVal = true;
      break;
    }
  }
  return retVal;
}

int32 LOWICacheManager::checkMeasurementValidity (int64 timestamp,
    LOWIChannelInfo* p_channel)
{
  int32 retVal = -1;
  log_verbose (TAG, "checkMeasurementValidity");
  return retVal;
}
