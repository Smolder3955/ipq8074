/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Client

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Client

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <base_util/log.h>

#include <base_util/postcard.h>
#include <mq_client/mq_client.h>
#include <inc/lowi_client.h>
#include <inc/lowi_client_receiver.h>
#include <common/lowi_utils.h>
#include <base_util/config_file.h>

using namespace qc_loc_fw;

#define SOCKET_NAME "/usr/share/location/mq/location-mq-s"

// client thread will be blocked for following time while rtt capabilities are retrieved
#define CAPABILITIES_REQ_BLOCKING_TIMEOUT_SEC 5

#define STATUS_BLOCKING_TIMEOUT_SEC 5
#define CAPS_SUBCRIPTION_BLOCKING_TIMEOUT_SEC 60

const char * const LOWIClient::TAG = "LOWIClient";

LOWIClient* LOWIClient::createInstance (LOWIClientListener* const ptr,
    bool enableLogging, eLogLevel log_level)
{
  LOWIClient* client = NULL;

  // Check & enable logging
  if (true == enableLogging)
  {
    setLogLevel (log_level);
  }
  else
  {
    setLogLevel (LL_LOG_OFF);
  }

  log_info (TAG, "createInstance ()");

  if (NULL == ptr)
  {
    log_error (TAG, "createInstance () - Null parameter");
  }

  client = new (std::nothrow) LOWIClient (ptr);
  if (NULL != client)
  {
    bool success = client->init ();
    if (success == false)
    {
      log_error (TAG, "Unable to initialize the LOWIClient");
      delete client;
      client = NULL;
    }
  }
  return client;
}

bool LOWIClient::setLogLevel (eLogLevel log_level)
{
  bool retVal = true;
  // Set the local log level for all the TAGS
  qc_loc_fw::ERROR_LEVEL level = LOWIUtils::to_logLevel(log_level);
  if (0 != log_set_local_level_for_tag (LOWIClient::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIClientReceiver::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWISsid::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIMacAddress::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIRequest::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIResponse::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIScanMeasurement::TAG, level))
  {
    retVal = false;
  }
  if (0 != log_set_local_level_for_tag (LOWIUtils::TAG, level))
  {
    retVal = false;
  }
  return retVal;
}

LOWIClient::LOWIClient (LOWIClientListener* const ptr)
: mLOWIClientListener (ptr),
  mLOWIClientReceiver (NULL),
  mConn (NULL),
  mLocalMsgQueue (NULL),
  mLocalMsgQueueBg (NULL),
  mClientName (NULL),
  mBlock (0),
  mBlockBg (0)
{
  log_verbose (TAG, "LOWIClient ()");
}

LOWIClient::~LOWIClient ()
{
  log_verbose (TAG, "~LOWIClient ()");
  // Delete the memory allocated on heap
  delete mLOWIClientReceiver;
  delete mLocalMsgQueue;
  delete mLocalMsgQueueBg;
  delete [] mClientName;
}

bool LOWIClient::init ()
{
  bool success = false;
  log_verbose (TAG, "init ()");
  do
  {
    mConn = MessageQueueClient::createInstance();
    if (NULL == mConn)
    {
      log_error (TAG, "Unable to create MessageQueueClient");
      break;
    }

    mLocalMsgQueue = BlockingQueue::createInstance ("LOWIMsgQ");
    if (NULL == mLocalMsgQueue)
    {
      log_error (TAG, "Unable to create Local Message Queue!");
      break;
    }

    mLocalMsgQueueBg = BlockingQueue::createInstance ("LOWIMsgQBg");
    if (NULL == mLocalMsgQueueBg)
    {
      log_error (TAG, "Unable to create Local Message Queue Bg!");
      break;
    }

    // Compose the client name with the thread id and suffix
    long long unsigned int threadid =
        (long long unsigned int)pthread_self ();
    log_verbose (TAG, "Client thread id = %llu", threadid);

    char clientname [CLIENT_NAME_LEN] = {0};
    snprintf(clientname, sizeof(clientname), "%llu", threadid);
    strlcat (clientname, LOWI_CLIENT_SUFFIX, sizeof (clientname));
    log_debug (TAG, "LOWIClient name = %s", clientname);

    mClientName = new (std::nothrow) char [CLIENT_NAME_LEN];
    if (NULL == mClientName)
    {
      log_error (TAG, "Unable to allocate memory for Client name");
      break;
    }

    strlcpy (mClientName, clientname, CLIENT_NAME_LEN);
    mLOWIClientReceiver = new (std::nothrow) LOWIClientReceiver
        (SOCKET_NAME, mLocalMsgQueue, mConn, this, mClientName);
    if (NULL == mLOWIClientReceiver)
    {
      log_error (TAG, "Unable to allocate memory for Receiver");
      break;
    }

    success = true;
  }
  while (0);

  if (true == success)
  {
    success = mLOWIClientReceiver->init();
  }

  return success;
}

LOWIResponse* LOWIClient::processRequest(LOWIRequest* const request)
{
  if (NULL == request)
  {
    return NULL;
  }

  LOWIRequest::eRequestType reqType = request->getRequestType();
  // send the request
  if( LOWIClient::STATUS_OK != sendRequest(request) )
  {
    log_error(TAG, "@processRequest(): failed to send request");
    return NULL;
  }

  // Set the time out value for which the thread will block
  struct timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;
  struct timespec * pTimeout = 0;
  time_t now = time(0);

  pTimeout = &timeout;

  bool is_queue_closed = false;

  // Block until time out or until response msg is received
  // in the queue.
  void *pMsg = 0;

  // Set the blocking flag so that upon receiving the response, it can be returned back
  // to the caller and without going through the client listener. We do this because the
  // client listener doesn't have access to the blocking queue.
  mBlockBg = true;

  // now block...
  log_verbose(TAG, "@processRequest(): blocking the client...");
  mLocalMsgQueueBg->pop(&pMsg, pTimeout, &is_queue_closed);

  // blocking is over, let's check the message
  if( 0 != pMsg )
  {
    log_verbose(TAG, "@processRequest(): rsp in blocking q to request type(%s)",
                LOWIUtils::to_string(reqType));
    return(LOWIResponse*)pMsg;
  }
  else
  {
    // timeout or queue closed?
    if( is_queue_closed )
    {
      // queue closed case
      log_warning(TAG, "init () - " "queue closed");
    }
    else
    {
      // Most likely a timeout
      log_warning(TAG, "init () - " "Blocking Request Timeout");
    }
    // reset the flag in case of queue closed or timeout
    mBlockBg = false;
  }
  return NULL;
}

LOWICapabilityResponse* LOWIClient::getCapabilities(LOWIRequest* const request)
{
  // send the request
  if( LOWIClient::STATUS_OK != sendRequest(request) )
  {
    log_error(TAG, "@getCapabilities(): failed to send request");
    return NULL;
  }

  // Set the time out value for which the thread will block
  struct timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;
  struct timespec * pTimeout = 0;
  time_t now = time(0);
  timeout.tv_sec = now + CAPABILITIES_REQ_BLOCKING_TIMEOUT_SEC;
  pTimeout = &timeout;

  bool is_queue_closed = false;

  // Block until time out or until rtt capabilities msg is received
  // in the queue. This is to ensure that rtt the capabilities are
  // received and passed to the caller before this client thread
  // is able to send any requests.
  void *pMsg = 0;

  // Set the blocking flag so that upon receiving the response, it can be returned back
  // to the caller and without going through the client listener. We do this because the
  // client listener doesn't have access to the blocking queue.
  // The time out is set by the definition CAPABILITIES_REQ_BLOCKING_TIMEOUT_SEC which is
  // currently set to 5 sec.
  mBlock = true;

  // now block...
  log_verbose(TAG, "@getCapabilities(): blocking on q");
  mLocalMsgQueue->pop(&pMsg, pTimeout, &is_queue_closed);

  // blocking is over, let's check the message
  if( 0 != pMsg )
  {
    log_verbose(TAG, "@getCapabilities(): good ptr in blocking q");
    return (LOWICapabilityResponse*)pMsg;
  }
  else
  {
    // timeout or queue closed?
    if( is_queue_closed )
    {
      // queue closed case
      log_warning(TAG, "init () - " "queue closed");
    }
    else
    {
      // Most likely a timeout
      log_warning(TAG, "init () - " "RTT Capabilities Request Timeout");
    }
    // reset the flag in case of queue closed or timeout
    mBlock = false;
  }
  return NULL;
}

LOWIClient::eRequestStatus LOWIClient::sendRequest(LOWIRequest* const request)
{
  eRequestStatus status = BAD_PARAMS;
  log_verbose (TAG, "sendRequest ()");

  do
  {
    if (NULL == request) break;

    OutPostcard* card = composePostCard (request, mClientName);
    if (NULL == card) break;

    int ret = mConn->send(card->getEncodedBuffer());
    delete card;
    if (ret != 0)
    {
      status = SEND_FAILURE;
      break;
    }
    status = STATUS_OK;
  }
  while (0);
  return status;
}

OutPostcard* LOWIClient::composePostCard (LOWIRequest* const request,
    const char* const originatorId)
{
  log_verbose (TAG, "composePostCard ()");
  return LOWIUtils::requestToOutPostcard(request, originatorId);
}

LOWIResponse* LOWIClient::parsePostCard (InPostcard* const card)
{
  log_verbose (TAG, "parsePostCard ()");
  return LOWIUtils::inPostcardToResponse(card);
}

int LOWIClient::newMsg(InMemoryStream * new_buffer)
{
  int result = 1;
  InPostcard * card = NULL;
  do
  {
    log_verbose(TAG, "newMsg");
    if (NULL == new_buffer)
    {
      log_error (TAG, "NewMsg received without a memory stream!");
      break;
    }

    card = InPostcard::createInstance();
    if (NULL == card)
    {
      log_error (TAG, "Unable to create a InPostcard");
      break;
    }
    card->init(new_buffer->getBuffer(), new_buffer->getSize());

    LOWIResponse* resp = parsePostCard (card);
    if (NULL == resp)
    {
      log_error (TAG, "Unable to parse the InPostcard");
      break;
    }

    // Check to see if it's a blocking call
    // The client thread is waiting to receive confirmation
    // that the information requested has been received.
    // Unblock the client by posting a message in the blocking queue.
    // The caller is responsible for freeing the memory.
    log_verbose (TAG, "@newMsg: resp to blocking caller... rspType(%s) mBlock(%d) mBlockBg(%d)",
                 LOWIUtils::to_string(resp->getResponseType()), mBlock, mBlockBg);

    if (mBlock || mBlockBg)
    {
      // Use this variable to see if we got a response we were waiting for. If not,
      // the client will continue to be blocked until the correct response arrives
      // or a timeout occurs. We do this because, there are asynchrounous events that
      // show up while the client is blocked waiting for a response.
      bool gotCorrectRsp = false;

      switch(resp->getResponseType())
      {
        case LOWIResponse::CAPABILITY:
          {
            log_debug (TAG, "%s response received", LOWIUtils::to_string(resp->getResponseType()));
            mLocalMsgQueue->push((void *)resp);
            mBlock = false;
          }
          break;
        // intentional fall-through
        case LOWIResponse::LOWI_STATUS:
          {
            log_debug (TAG, "%s response received", LOWIUtils::to_string(resp->getResponseType()));
            mLocalMsgQueueBg->push((void*)resp);
            mBlockBg = false; // Unblock the client
          }
          break;
        default:
          log_warning (TAG, "Unknown response received while blocking the client");
          break;
      }
    }
    else
    {
      log_verbose (TAG, "newMsg - Sending response to the listener");
      this->mLOWIClientListener->responseReceived(resp);
      delete resp;
    }

    result = 0;
  } while(0);

  delete new_buffer;
  delete card;

  if(0 != result)
  {
    log_error(TAG, "newMsg failed %d", result);
  }
  // We do not want to terminate the loop in which
  // the receiver is running to retrieve the response from IPC hub
  // So keeping the result to 0 no matter what.
  result = 0;
  return result;
}

LOWIClientListener::~LOWIClientListener()
{
}
