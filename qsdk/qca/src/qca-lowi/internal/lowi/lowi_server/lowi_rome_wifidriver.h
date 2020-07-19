#ifndef __LOWI_ROME_WIFI_DRIVER_H__
#define __LOWI_ROME_WIFI_DRIVER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI ROME Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI ROME Wifi Driver

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <lowi_server/lowi_wifidriver_interface.h>
#include <lowi_server/lowi_cache_manager.h>
#include <base_util/sync.h>
#include "osal/wifi_scan.h"
#include "wlan_capabilities.h"
#include "lowi_rome_fsm.h"
#include "lowi_nl80211.h"

namespace qc_loc_fw
{
/**
 * This class implements the WifiDriverInterface
 * and provides implementation for ROME Atheros wifi driver
 */
class LOWIROMEWifiDriver : public LOWIWifiDriverInterface
{
private:
  static const char* const TAG;
  int32                 mRtsCtsNumMeas;
  int32                 mCfrCaptureMode;
  uint32                mRtsCtsTag;
  int32                 mRssiThresholdForRtt;
  bool                  mConnectedToDriver;
  uint32                mInternalMsgId;
  unsigned int*         mChList;
  LOWIRomeFSM*          mLowiRomeFsm;
  LOWIInternalMessageReceiverListener* mInternalMessageListener;
  /** The Cache Manager object through which the FSM can access
   *  the BSSID cache.
   */
  LOWICacheManager*                        mCacheManager;

  // private copy constructor and assignment operator so that the
  // the instance can not be copied.
  LOWIROMEWifiDriver( const LOWIROMEWifiDriver& rhs );
  LOWIROMEWifiDriver& operator=( const LOWIROMEWifiDriver& rhs );

  /**
   * This function passes in the newly arrived PIPE event to the
   * underlying Rome driver.
   *
   * @param mode: The listenning mode of the Driver thread
   *              - Discovery/Ranging.
   * @param newEvent: The newly arrived PIPE event from LOWI
   *                 controller.
   *
   * @return int: Error code - 0 for success, other values
   *              indicate failures.
   */
  int processPipeEvent(eListenMode mode, RomePipeEvents newEvent);

  /**
   * This function Sends out a Neighbor Report Request to the WLAn
   * driver to be sent out to the target Access Point.
   *
   * @param NONE.
   *
   * @return NONE.
   */
  void sendNeighborRprtReq();

  /**
   * This function Sends out a Fine Timing Measurement Report
   * Action Frame to the driver to be sent out to the target
   * Access Point.
   *
   * @param req: The Fine Timing Measurements to be sent out.
   *
   * @return NONE.
   */
  void SendFTMRRep(LOWIFTMRangeRprtMessage* req);

  /* The listener through which Rome Driverwill send Internal Requests */
  LOWIScanResultReceiverListener*          mListener;

public:

  /**
   * Gets the cached measurements from the Wifi driver which
   * are cached by the wifi driver
   *
   * @return Measurements
   */
  virtual LOWIMeasurementResult* getCacheMeasurements ();

  /**
   * Returns the Capabilities of the driver
   * @return Capabilities of the driver
   */
  virtual LOWICapabilities getCapabilities ();

  /**
   * Blocking call to listen for the scan measurements from the
   * wifi driver. It can also listen to additional file descriptor, if
   * initialized through initFileDescriptor
   * @param LOWIRequest*   Request
   * @param eListenMode Mode of listening, used only if the request is NULL
   * @return Measurement results
   */
  virtual LOWIMeasurementResult* getMeasurements (LOWIRequest* request,
      eListenMode mode);

  /**
   * Stops listening to the events from the Wifi Driver and unblocks the
   * getMeasurements call. This call unblocks the thread by
   * writing to the initialized file descriptor.
   * Note: This function is called in context of a different thread
   * Note: This function will only work, if initFileDescriptor was called
   *       before calling getMeasurements ()
   * @param eListenMode Mode of listening on which listening is to stop
   * @return 0 - success, other values otherwise
   */
  virtual int unBlock (eListenMode mode);

  /**
   * Terminates the getMeasurements call.
   * This call unblocks the thread by writing to the initialized
   * file descriptor. Note: This function is called in context of
   * a different thread Note: This function will only work, if
   * initFileDescriptor was called
   *       before calling getMeasurements ()
   * @param eListenMode Mode of listening on which listening is to stop
   * @return 0 - success, other values otherwise
   */
  virtual int terminate (eListenMode mode);

  /**
   * Initialize the file descriptor to which the thread will additionally
   * listen to during getMeasurements() in specified mode.
   * @param eListenMode Mode of listening
   * @return 0 - success, other values otherwise
   */
  virtual int initFileDescriptor (eListenMode mode);

  /**
   * Closes the file descriptor.
   * @param eListenMode Mode of listening
   * @return 0 - success, other values otherwise
   */
  virtual int closeFileDescriptor (eListenMode mode);

  /**
   * Sets the LOWI Request - this is called by LOWI controller to
   * pass in the new Request to the LOWI Wifi Driver. The purpose
   * of this call is so that the WiFi Driver can pick up the new
   * request without having to return from the "getMeasurements"
   * call.
   * @param [in] LOWIRequest*: The Current valid Request.
   * @param [in] eListenMode :Mode of Listening
   */
  virtual void setNewRequest(const LOWIRequest* r, eListenMode mode);

  /**
   * Parse and take action on the newly arrived WLAN frame
   * @return NONE
   */
  virtual void processWlanFrame();

  /**
   * Parse Neighbor Report Element
   *
   * @param [in] elemLen: The length of FTM Range Request
   *                      Element
   * @param [in] frameBody: The frame Body byte stream
   * @param [out] rangeReq: The location where the parsed neighbor
   *                        report will be stored
   *
   * @return uint8*: return pointer to the frame body after
   *                 parsing current element
   */
  uint8* parseNeighborReport(uint8  &elemLen,
                             uint8* frameBody,
                             FineTimingMeasRangeReq &rangeReq);

  /**
   * Parse Measurement Request Element
   *
   * @param [in] currentFrameLen: The current Length of WLAN frame
   * @param [in] frameBody: The frame Body byte stream
   * @param [in] dialogTok: The dialog Token of the current Radio
   *                        Measurement Frame
   * @param [in] sourceMac: The Source MAC address of the frame
   * @param [in] sourceMac: The local STA MAC address of the
   *                         frame
   * @param [in] freq: The channel frequency on which the frame
   *                   arrived
   *
   * @return uint8*: return pointer to the frame body after
   *                 parsing current element
   */
  uint8* parseMeasReqElem(uint32 &currentFrameLen,
                          uint8* frameBody,
                          uint8 dialogTok,
                          uint8 sourceMac[BSSID_SIZE],
                          uint8 staMac[BSSID_SIZE],
                          uint32 freq);

  /**
   * Constructor
   * Starts the WifiDriver
   * @param [in] config: Provides the configuration parameters
   *        provided by the user which allows the driver to behave
   *        according to user's needs, for example to run in CFR
   *        capture mode.
   * @param [in] scanResultListener: This is a pointer to the
   *        Listener object. The Driver uses this object to send
   *        the results from scans to the client.
   * @param [in] cacheManager: This is a pointer to the Cache
   *        Manager Object. The Driver uses this to access the
   *        Cache to retrieve BSSID related information.
   */
  LOWIROMEWifiDriver (ConfigFile* config,
                      LOWIScanResultReceiverListener* scanResultListener,
                      LOWICacheManager* cacheManager);

  /**
   * Constructor
   * Starts the WifiDriver
   * @param [in] config: Provides the configuration parameters
   *        provided by the user which allows the driver to behave
   *        according to user's needs, for example to run in CFR
   *        capture mode.
   * @param [in] scanResultListener: This is a pointer to the
   *        Listener object. The Driver uses this object to send
   *        the results from scans to the client.
   * @param [in] internalMessageListener: This is the pointer to
   *        the internal Message listerner Object. The Driver uses
   *        this object to send internal messages to the Client.
   * @param [in] cacheManager: This is a pointer to the Cache
   *        Manager Object. The Driver uses this to access the
   *        Cache to retrieve BSSID related information.
   */
  LOWIROMEWifiDriver (ConfigFile* config,
                      LOWIScanResultReceiverListener* scanResultListener,
                      LOWIInternalMessageReceiverListener* internalMessageListener,
                      LOWICacheManager* cacheManager);
  /**
   * Destructor
   * Stops the WifiDriver
   */
  virtual ~LOWIROMEWifiDriver ();
};
} // namespace
#endif //#ifndef __LOWI_ROME_WIFI_DRIVER_H__
