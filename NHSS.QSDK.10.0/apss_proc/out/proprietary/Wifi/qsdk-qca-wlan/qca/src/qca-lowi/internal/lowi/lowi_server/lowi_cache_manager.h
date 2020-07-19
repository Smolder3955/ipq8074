#ifndef __LOWI_CACHE_MANAGER_H__
#define __LOWI_CACHE_MANAGER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Cache Manager Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Cache manager

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <inc/lowi_scan_measurement.h>
#include <inc/lowi_request.h>
#include <base_util/vector.h>
#include <base_util/list.h>
namespace qc_loc_fw
{

/**
 * This class abstracts & manages the underlying cache implementation.
 * Should only be accessed from the MsgHandler thread.
 *
 * Primary Key is BSSID. Keeps only one record per BSSID.
 * Updates every time new measurements are received.
 *
 * Key to extract the information is time stamp only.
 *
 */
class LOWICacheManager
{
private:
  static const char * const TAG;
  /** Max number of records the cache should have available at any time*/
  uint32 mLimit;
  /** List of APs discovered, the count is determined by the
   *  module that instantiates an object of the cache */
  List<LOWIScanMeasurement> mMeasurementCache;
  /** Lowest Age in Cache */
  int32 mLowestAgeMs;
  /** Timestamp of the last time APs were loaded into cache */
  int64 mLastMeasurementTs;

  /** Flag indicating if STA is associated */
  bool associatedToAp;
  /** BSSID of AP STA is associated to */
  LOWIScanMeasurement associatedApMeas;

  /** Flag indicating if local STA's MAC is valid */
  bool localStaMacValid;
  /** Local STA Mac address */
  LOWIMacAddress localStaMac;
private:
  /**
   * Updates Measurements for BSSIDs already in the Cache.
   *
   * @param List<LOWIScanMeasurement*> Measurements to be kept in
   *         cache
   *
   * @return bool true - success, false - otherwise
   */
  bool updateCachedMeas (List<LOWIScanMeasurement> &measurements);

  /**
   * Adds new BSSIDs and their measurement info to the Cache.
   *
   * @param List<LOWIScanMeasurement*> Measurements to be
   *         added to cache.
   *
   * @return bool true - success, false - otherwise
   *
   */
  bool addToCache (List<LOWIScanMeasurement> &newMeasurements);

public:
  /**
   * Constructor
   * @param uint32 Max number of records the cache should support
   */
  LOWICacheManager(uint32 limit);
  /** Destructor*/
  ~LOWICacheManager();

  /**
   * Resets the cache. All cached data is lost.
   * @return bool true - success, false - otherwise
   */
  bool resetCache ();

  /**
   * Stores the local STA's Mac address in the cache.
   * @param LOWIMacAddress - STA Mac address to store in cache
   *
   * @return NONE
   */
  void storeStaMacInCache (LOWIMacAddress staMac);

  /**
   * Retrieves the local STA's Mac address from the cache.
   * @param LOWIMacAddress - location to store STA Mac address for
   *                       caller
   *
   * @return bool true - if returned MAC address is valid
   *              false - otherwise
   */
  bool getStaMacInCache (LOWIMacAddress &staMac);

  /**
   * Puts the ScanMeasurement in the cache
   * If the records are running to the limit it will
   * delete oldest records.
   * @param vector<LOWIScanMeasurement*> Measurements to be kept in cache
   *
   * @return bool true - success, false - otherwise
   */
  bool putInCache (vector<LOWIScanMeasurement*> & measurements);

  /**
   * Gets all the ScanMeasurement from the cache related
   * to the time stamp. All the returned ScanMeasurement will
   * have time stamp equal to or greater than the time stamp in
   * the request.
   *
   * @param [in] timestamp   Requested measurements must have
   *                          timestamp equal to this or greater.
   * @param [out] vector<LOWIScanMeasurement*> Vector containing
   *                          ScanMeasurement
   *
   * @return error code
   */
  int32 getFromCache (int64 timestamp,
      vector<LOWIScanMeasurement*> & v);

  /**
   * Gets all the ScanMeasurement from the cache related
   * to the time stamp & band. All the returned ScanMeasurement will
   * have time stamp equal to or greater than the time stamp in
   * the request and have the band specified.
   *
   * Note: Will provide the measurements for available band
   * but also generate error if the criteria is failed for any
   * band i.e. Channels in one band are not searched before
   *
   * @param [in] int64   Requested measurements must have
   *                          timestamp equal to this or greater.
   * @param [in] int64 Fallback timestamp, if valid (other than 0) this
   *                  will be checked against the all the requested channel's
   *                  last measurement timestamp first. If this is less than
   *                  the requested channel timestamp, required data from Cache
   *                  is provided else error.
   * @param [in] LOWIDiscoveryScanRequest::eBand Measurements required for
   *                           specified band only
   * @param [out] vector<LOWIScanMeasurement*> Vector containing
   *                     ScanMeasurement
   * @param [out] int64& Latest time stamp of the cached measurements
   *                      related to the request
   *
   * @return error code - 0 (success)
   */
  int32 getFromCache (int64 cache_timestamp,
      int64 fallback_timestamp,
      LOWIDiscoveryScanRequest::eBand band,
      vector<LOWIScanMeasurement*> & v,
      int64& latest_cached_timestamp);

  /**
   * Gets all the ScanMeasurement from the cache related
   * to the time stamp & channels. All the returned ScanMeasurement
   * will have time stamp equal to or greater than the time stamp in
   * the request for only the corresponding channels.
   *
   * Note: Will provide the measurements for available channels
   * but also generate error if the criteria is failed for any
   * channel i.e. A channel is not searched before
   *
   * @param [in] int64   Requested measurements must have
   *                          timestamp equal to this or greater.
   * @param [in] int64 Fallback timestamp, if valid (other than 0) this
   *                  will be checked against the all the requested channel's
   *                  last measurement timestamp first. If this is less than
   *                  the requested channel timestamp, required data from Cache
   *                  is provided else error.
   * @param [in] vector <LOWIChannelInfo> Channels for which measurements
   *                  are requested
   * @param [out] vector<LOWIScanMeasurement*> Vector containing
   *                  ScanMeasurement
   * @param [out] int64& Latest time stamp of the cached measurements
   *                      related to the request
   *
   * @return error code - 0 (success)
   */
  int32 getFromCache (int64 cache_timestamp,
      int64 fallback_timestamp,
      vector <LOWIChannelInfo> & chanInfo,
      vector<LOWIScanMeasurement*> & v,
      int64& latest_cached_timestamp);

  /**
   * Gets the cached measurement for the BSSID specified in the
   * argument.
   *
   * Note: Will generate error if the cache does not have scan
   * measurement for the requested BSSID operating on the
   * specified channel.
   *
   * @param [in] LOWIMacAddress: The BSSID for which scan
   *                             measurements are requested.
   * @param [out] LOWIScanMeasurement&: Destination to store
   *                                    retrieved ScanMeasurement
   *
   * @return false (failure), true (success).
   */
  boolean getFromCache (LOWIMacAddress bssid,
                        LOWIScanMeasurement &scanMeasurement);

  /**
   * Returns the BSSID the STA is associated with.
   *
   * Note: Will generate false if STA is not associated with any
   * BSSID.
   *
   * @param [out] LOWIScanMeasurement: The scan measurement
   *                                   for the BSSID with which
   *                                   the STA is associated.
   *
   * @return false (not Associated), true (Associated).
   */
  bool getAssociatedAP (LOWIScanMeasurement &outBssidMeas);
  /**
   * Checks the measurement validity for a particular channel from
   * a particular time stamp.
   * @param int64 timestamp from which the last measurement of the
   *              channel is to be compared
   * @param LOWIChannelInfo*  Pointer to the channel for which the check
   *                          has to be done.
   * @result int  -1 (Last measurement of the channel is before
   *                    the requested time stamp)
   *              0 (The channel does not have a Last measurement
   *                    time stamp)
   *              1 (Last measurement of the channel is after the
   *                    requested time stamp)
   * @usage Client can specify a measurement time (Current time - threshold)
   *            to understand if the channel has a relatively fresh scan
   *            or older scan or no scan.
   */
  int32 checkMeasurementValidity (int64 timestamp,
      LOWIChannelInfo* p_channel);

protected:
};
} // namespace
#endif //#ifndef __LOWI_CACHE_MANAGER_H__
