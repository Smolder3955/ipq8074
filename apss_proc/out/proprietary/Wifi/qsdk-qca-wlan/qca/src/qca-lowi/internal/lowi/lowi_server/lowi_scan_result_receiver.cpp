/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Scan Result Receiver

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Scan Result Receiver

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_scan_result_receiver.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <unistd.h>

using namespace qc_loc_fw;

const char * const LOWIScanResultReceiver::TAG = "LOWIScanResultReceiver";

LOWIScanResultReceiver::LOWIScanResultReceiver
(
    LOWIScanResultReceiverListener* listener,
    LOWIWifiDriverInterface* interface
)
: mListener (listener), mWifiDriver (interface), mRequest (NULL),
  mReceiverThread (NULL), mMutex (NULL)
{
  log_verbose (TAG, "LOWIScanResultReceiver");
  mMutex = Mutex::createInstance("LOWIScanResultReceiver",false);
  if(0 == mMutex)
  {
    log_error(TAG, "Could not create mutex!");
  }
  mThreadAboutToBlock = false;
  mThreadTerminateRequested = false;
  mNewRequestArrived = false;
}

LOWIScanResultReceiver::~LOWIScanResultReceiver ()
{
  log_verbose (TAG, "~LOWIScanResultReceiver");
  if (mReceiverThread != NULL)
  {
    terminate ();
  }
  delete mMutex;
  delete mReceiverThread;
}

bool LOWIScanResultReceiver::init ()
{
  bool retVal = true;
  log_debug(TAG, "init ()");

  // If the thread is not already running start it.
  if (NULL == mReceiverThread)
  {
    // Re-start the Receiver thread. No need to delete the runnable
    // at destruction.
    mReceiverThread = Thread::createInstance(TAG, this, false);
    if (mReceiverThread == NULL)
    {
      log_warning (TAG, "execute() - Unable to create receiver"
          " thread instance");
      retVal = false;
    }
    else
    {
      int ret = mReceiverThread->launch ();
      log_debug (TAG, "execute() - Launch thread returned = %d", ret);
      if (0 != ret)
      {
        retVal = false;
      }
    }
  }
  return retVal;
}

bool LOWIScanResultReceiver::execute (LOWIRequest* request)
{
  bool retVal = true;
  log_debug(TAG, "execute ()");

  {
    AutoLock autolock(mMutex);
    mRequest = request;
    mWifiDriver->setNewRequest(request, mListenMode);
    mNewRequestArrived = true;
  }

  // unblock the thread
  if (mReceiverThread != NULL)
  {
    retVal = unblockThread ();
  }
  return retVal;
}

bool LOWIScanResultReceiver::unblockThread ()
{
  bool retVal = true;
  log_debug (TAG, "unblockThread ()");
  bool thread_running = false;

  {
    AutoLock autolock(mMutex);
    thread_running = mThreadAboutToBlock;
  }

  if (true == thread_running)
  {
    if (mWifiDriver->unBlock (mListenMode) <= 0)
    {
      log_error (TAG, "unblockThread () shutdown failed");
      retVal = false;
    }
    else
    {
      log_debug (TAG, "unblockThread () - success");
    }
  }

  return retVal;
}

bool LOWIScanResultReceiver::terminate ()
{
  bool retVal = false;
  log_info (TAG, "terminate ()");
  bool thread_running = false;

  {
    AutoLock autolock(mMutex);
    thread_running = mThreadAboutToBlock;
    // Thread is not running
    mThreadTerminateRequested = true;
    if (false == thread_running)
    {
      retVal = true;
    }
  }

  if (true == thread_running)
  {
    if (mWifiDriver->terminate (mListenMode) <= 0)
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

void LOWIScanResultReceiver::run ()
{
  log_verbose (TAG, "run ()");
  LOWIRequest* req = NULL;
  do
  {
    {
      AutoLock autolock(mMutex);
      if (true == mThreadTerminateRequested)
      {
        log_info (TAG, "Thread terminate was requested");
        break;
      }
      // Initialize the file descriptor.
      // This is required to initialize the communication
      // pipe between this thread and the main thread so that the
      // main thread could unblock this thread at any point
      // by writing to the file descriptor after this thread
      // is blocked listening to the events from wifi driver and
      // the file descriptor.
      mWifiDriver->initFileDescriptor (mListenMode);
      mThreadAboutToBlock = true;

      // Check if a new request has arrived
      if (true == mNewRequestArrived)
      {
        req = mRequest;
        log_debug (TAG, "Execute new request");
        mNewRequestArrived = false;
      }
      else
      {
        req = NULL;
        log_debug (TAG, "Enter passive listening mode");
      }
    }
  #ifndef __ANDROID__
    sleep (5);
  #endif
    // This is a blocking call and waits on measurements.
    LOWIMeasurementResult* result = mWifiDriver->block(req, mListenMode);

    log_info (TAG, "result received");

    {
      AutoLock autolock(mMutex);
      // Close the communication pipe created by this thread.
      mWifiDriver->closeFileDescriptor(mListenMode);
      mThreadAboutToBlock = false;
    }

    if (NULL == result)
    {
      log_debug (TAG, "No result, because : 1) Internal Message was processed, 2) Terminate or 3) No memory");
      // No need to send call back to the listener as we do not have
      // any measurements. This might be the case that the passive
      // scanning mode was terminated by LOWIController or there was
      // mem allocation failure.
    }
    else
    {
      // Report the results to the Listener
      if (NULL != mListener)
      {
        log_debug (TAG, "Sending scan measurements to the listener");
        mListener->scanResultsReceived (result);
      }
      else
      {
        log_warning (TAG, "No listener. Unexpected!");
      }
    }
  } while (1);
}
