/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

 mq-server launcher

 GENERAL DESCRIPTION
 This component launches the mq server thread.

 Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc
 =============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <base_util/log.h>
#include "mq_server.h"
#include <pthread.h>

using namespace qc_loc_fw;

#ifdef __ANDROID__
#ifdef __ANDROID_NDK__
#else // #ifdef __ANDROID_NDK__
#define SOCKET_NAME "/data/misc/location/mq/location-mq-s"
#endif // #ifdef __ANDROID_NDK__
#else
#ifdef LOWI_ON_ACCESS_POINT
#define SOCKET_NAME "/usr/share/location/mq/location-mq-s"
#else
#define SOCKET_NAME "location-mq-s"
#endif //#ifdef LOWI_ON_ACCESS_POINT
#endif // #ifdef __ANDROID__
#define BREAK_IF_ZERO(ERR,X) if(0==(X)) {result = (ERR); break;}
#define BREAK_IF_NON_ZERO(ERR,X) if(0!=(X)) {result = (ERR); break;}
#define BREAK_IF_NON_ZERO_RC(ERR,RC,X) if(0!=(RC=(X))) {result = (ERR); break;}

typedef void* (*pthread_func) (void*);

void location_mq_run(void *arg)
{
  int result = 1;

  MessageQueueServer * service = 0;
  ERROR_LEVEL error_level = qc_loc_fw::EL_LOG_OFF;
  if (NULL != arg)
  {
    error_level = *((ERROR_LEVEL *)arg);
  }
  do
  {
    BREAK_IF_NON_ZERO(5, log_set_local_level_for_tag("MessageQ", error_level));
    BREAK_IF_NON_ZERO(6, log_set_local_level_for_tag("MessageQueueClientInfo", error_level));

    service = MessageQueueServer::createInstance();
    BREAK_IF_ZERO(7, service);

    BREAK_IF_NON_ZERO(8, service->setServerNameDup(SOCKET_NAME));
    BREAK_IF_NON_ZERO(9, service->openListenerSocket());
    service->run();

    BREAK_IF_NON_ZERO(20, log_flush_local_level_for_tag("MessageQ"));
    BREAK_IF_NON_ZERO(21, log_flush_local_level_for_tag("MessageQueueClientInfo"));

    result = 0;
  } while (0);

  delete service;
  service = 0;

  if(0 != result)
  {
    log_error("location-mq", "queue server creation failed %d", result);
  }
  log_error("location-mq", "terminated");
}

int mq_server_launch(ERROR_LEVEL error_level)
{
  (void)log_set_global_level(error_level);

  pthread_t threadId;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int result = pthread_create(&threadId, &attr,
                              reinterpret_cast<pthread_func>(location_mq_run), &error_level);
  if (result != 0)
  {
    log_error("location-mq", "pthread_create failed error = %d", result);
  }
  return result;
}

