/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
LOWI Internal Message

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Internal Message

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <base_util/log.h>
#include <common/lowi_utils.h>
#include <lowi_server/lowi_internal_message.h>

using namespace qc_loc_fw;

const char* const LOWIInternalMessage::TAG = "LOWIInternalMessage";

//////////////////////
// LOWIInternalMessage
//////////////////////
LOWIInternalMessage::LOWIInternalMessage(uint32 msgId)
: LOWIRequest(msgId)
{
  log_verbose (TAG, "LOWIInternalMessage");
}

LOWIInternalMessage::~LOWIInternalMessage()
{
  log_verbose (TAG, "~LOWIInternalMessage");
}

InPostcard* LOWIInternalMessage::createPostcard (LOWIInternalMessage* req)
{
  InPostcard* card = NULL;
  do
  {
    if (NULL == req)
    {
        break;
    }

    OutPostcard* out = OutPostcard::createInstance ();
    if (NULL == out)
    {
      break;
    }

    out->init ();

    const void* blob = (const void*) &req;
    size_t length = sizeof (req);
    out->addString("IMSG", "INTERNAL_MESSAGE");
    out->addBlob ("IMESSAGE", blob, length);
    out->finalize ();

    // Create InPostcard from the OutPostcard
    card = InPostcard::createInstance (out);
    delete out;
  } while (0);
  return card;
}

LOWIInternalMessage* LOWIInternalMessage::parseInternalMessage (InPostcard* card)
{
  LOWIInternalMessage * req = NULL;

  do
  {
    if (NULL == card)
    {
      break;
    }

    const void* blob = NULL;
    size_t length = 0;
    card->getBlob ("IMESSAGE", &blob, &length);
    req =  *(LOWIInternalMessage **) blob;
  } while (0);
  return req;
}

LOWIRequest::eRequestType LOWIInternalMessage::getRequestType () const
{
  return LOWI_INTERNAL_MESSAGE;
}

/////////////////////////////
// LOWIFTMRangeReqMessage
/////////////////////////////
LOWIFTMRangeReqMessage::LOWIFTMRangeReqMessage (uint32 msgId, vector<LOWIPeriodicNodeInfo> &v,
                                    LOWIMacAddress & bssid, LOWIMacAddress & selfBssid,
                                    uint32 freq, uint32 mDToken, uint32 mMToken)
: LOWIInternalMessage (msgId)
{
  log_verbose (TAG, "LOWIFTMRangeReqMessage");
  mNodeInfo       = v;
  mRequesterBssid = bssid;
  mSelfBssid      = selfBssid;
  mFrequency      = freq;
  mDiagToken      = mDToken;
  mMeasToken      = mMToken;
}

LOWIFTMRangeReqMessage::~LOWIFTMRangeReqMessage ()
{
  log_verbose (TAG, "~LOWIFTMRangeReqMessage");
}

vector <LOWIPeriodicNodeInfo> & LOWIFTMRangeReqMessage::getNodes(){
  return mNodeInfo;
}

LOWIMacAddress & LOWIFTMRangeReqMessage::getRequesterBssid()
{
  return mRequesterBssid;
}

LOWIMacAddress & LOWIFTMRangeReqMessage::getSelfBssid()
{
  return mSelfBssid;
}

uint32 LOWIFTMRangeReqMessage::getFrequency() const
{
  return mFrequency;
}

uint32 LOWIFTMRangeReqMessage::getDiagToken() const
{
  return mDiagToken;
}

uint32 LOWIFTMRangeReqMessage::getMeasToken() const
{
  return mMeasToken;
}

LOWIInternalMessage::eLowiInternalMessage
LOWIFTMRangeReqMessage::getInternalMessageType () const
{
  return LOWI_IMSG_FTM_RANGE_REQ;
}

//////////////////////////////////
// LOWIFTMRangeRprtMessage
//////////////////////////////////
LOWIFTMRangeRprtMessage::LOWIFTMRangeRprtMessage(uint32 msgId,
                                                 LOWIMacAddress & bssid,
                                                 LOWIMacAddress & selfBssid,
                                                 uint32 freq,
                                                 uint32 dToken,
                                                 uint32 mToken,
                                                 vector<LOWIRangeEntry> & vR,
                                                 vector<LOWIErrEntry> & vE)
: LOWIInternalMessage (msgId)
{
  log_verbose (TAG, "LOWIFTMRangeRprtMessage");
  mRequesterBssid = bssid;
  mSelfBssid      = selfBssid;
  mFrequency      = freq;
  mDiagToken      = dToken;
  mMeasToken      = mToken;
  measInfoSuccess = vR;
  measInfoErr     = vE;
}

/** Destructor*/
LOWIFTMRangeRprtMessage::~LOWIFTMRangeRprtMessage()
{
  log_verbose (TAG, "~LOWIFTMRangeRprtMessage");
}

vector<LOWIRangeEntry> & LOWIFTMRangeRprtMessage::getSuccessNodes()
{
  return measInfoSuccess;
}

vector<LOWIErrEntry> & LOWIFTMRangeRprtMessage::getErrNodes()
{
  return measInfoErr;
}

LOWIMacAddress & LOWIFTMRangeRprtMessage::getRequesterBssid()
{
  return mRequesterBssid;
}

LOWIMacAddress & LOWIFTMRangeRprtMessage::getSelfBssid()
{
  return mSelfBssid;
}

uint32 LOWIFTMRangeRprtMessage::getFrequency() const
{
  return mFrequency;
}

uint32 LOWIFTMRangeRprtMessage::getDiagToken() const
{
  return mDiagToken;
}

uint32 LOWIFTMRangeRprtMessage::getMeasToken() const
{
  return mMeasToken;
}

LOWIInternalMessage::eLowiInternalMessage
LOWIFTMRangeRprtMessage::getInternalMessageType () const
{
  return LOWI_IMSG_FTM_RANGE_RPRT;
}


