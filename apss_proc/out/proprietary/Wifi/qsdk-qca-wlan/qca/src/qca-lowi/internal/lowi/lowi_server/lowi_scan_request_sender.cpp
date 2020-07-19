/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Scan Request Sender

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Scan Request Sender

Copyright (c) 2015 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_scan_request_sender.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <unistd.h>

using namespace qc_loc_fw;

const char * const LOWIScanRequestSender::TAG = "LOWIScanRequestSender";

LOWIScanRequestSender::LOWIScanRequestSender
(
    LOWIScanResultReceiverListener* listener,
    LOWIWifiDriverInterface* interface
)
: LOWIScanResultReceiver (listener, interface)
{
  log_verbose (TAG, "LOWIScanRequestSender");
  mListenMode = LOWIWifiDriverInterface::REQUEST_SCAN;
}

LOWIScanRequestSender::~LOWIScanRequestSender ()
{
  log_verbose (TAG, "~LOWIScanRequestSender");
  if (mReceiverThread != NULL)
  {
    terminate ();
  }
}

bool LOWIScanRequestSender::init()
{
  bool retVal = true;

  // Create the Blocking queue
  mMsgQueue = BlockingQueue::createInstance ("LOWIScanRequestSenderQ");
  if( NULL == mMsgQueue )
  {
    log_error (TAG, "Unable to create Message Queue!");
    retVal = false;
  }
  else
  {
    // Call base class init
    retVal = LOWIScanResultReceiver::init();
  }
  return retVal;
}

bool LOWIScanRequestSender::execute(LOWIRequest *request)
{
  log_debug(TAG, "execute ()");
  bool retVal = false;

  if ( mReceiverThread != NULL )
  {
    log_verbose(TAG, "execute - push the request to blocking queue");
    // Post the new request to the blocking queue
    mMsgQueue->push((void *)request);
    retVal = true;
  }
  return retVal;
}

bool LOWIScanRequestSender::terminate ()
{
  bool retVal = false;

  log_info (TAG, "terminate ()");
  bool thread_running = false;

  {
    AutoLock autolock(mMutex);
    thread_running = mThreadAboutToBlock;
    // Thread is not running
    mThreadTerminateRequested = true;
    if( false == thread_running )
    {
      retVal = true;
    }
  }

  if( true == thread_running )
  {
    // Unblock the thread
    // The thread could be blocked on the msg queue. To unblock
    // write a string to the queue
    mMsgQueue->push((void*)"Terminate called");

    // The thread could also be blocked on a semaphore waiting for
    // signal from the FW upon the event socket. Call terminate
    // to generate an "on demand" signal.
    if( mWifiDriver->terminate (mListenMode) <= 0 )
    {
      log_error (TAG, "terminate () shutdown failed");
    }
    else
    {
      log_info (TAG, "terminate () - success");
      retVal = true;
    }
  }

  log_verbose (TAG, "About to join");
  mReceiverThread->join ();
  log_info (TAG, "After join complete");

  {
    AutoLock autolock(mMutex);
    mThreadTerminateRequested = false;
  }

  return retVal;
}

void LOWIScanRequestSender::run ()
{
  log_info (TAG, "run ()");
  do
  {
    log_debug (TAG, "run () - Block on Msg Q");

#ifndef __ANDROID__
    sleep (5);
#endif
    bool is_queue_closed = false;

    // Message to retrieve. Block until any message is received
    // in the queue. This thread will block as long as there is a
    // Request to execute.
    mThreadAboutToBlock = true;
    void * ptr = 0;
    mMsgQueue->pop (&ptr, TimeDiff(false), &is_queue_closed);
    if( 0 != ptr )
    {
      if (true == mThreadTerminateRequested)
      {
        log_info (TAG, "run () - Thread terminate was requested...");
        break;
      }

      LOWIRequest* req = reinterpret_cast<LOWIRequest*>(ptr);
      log_verbose(TAG, "run () - Message received in blocking q");
      LOWIMeasurementResult* result = mWifiDriver->getMeasurements
                                      (req, mListenMode);
      log_info (TAG, "result received");

      mThreadAboutToBlock = false;

      if( NULL == result )
      {
        log_info (TAG, "run () - NULL result");
        // No need to send callback to the listener as we do not have
        // any measurements. This might be the case if there was mem allocation failure
        // or possibly if the thread was waiting in the socket when it was terminated
        // by LOWIController.
      }
      else
      {
        // Report the results to the Listener
        if( NULL != mListener )
        {
          log_verbose (TAG, "run () - Sending scan measurements to the listener");
          mListener->scanResultsReceived (result);
        }
      }
    }
    else
    {
      mThreadAboutToBlock = false;
      // queue closed?
      if( is_queue_closed )
      {
        // queue closed case
        log_warning(TAG, "run () - queue closed. Exit thread");
        break;
      }
      else
      {
        log_verbose(TAG, "run () - Unknown state. Block again");
      }
    }
  } while( 1 );
}
