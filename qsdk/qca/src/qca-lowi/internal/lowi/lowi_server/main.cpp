/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Starting module

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Starting module

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

#include <base_util/log.h>

#include <base_util/postcard.h>
#include <mq_client/mq_client.h>

using namespace qc_loc_fw;

#include <lowi_server/lowi_controller.h>
#include <lowi_server/lowi_version.h>

#include <mq_server/mq_server.h>

#define SOCKET_NAME "/usr/share/location/mq/location-mq-s"

int main(int argc, char * argv[])
{
  qc_loc_fw::log_set_global_tag(LOWI_VERSION);
  qc_loc_fw::log_set_global_level(qc_loc_fw::EL_LOG_ALL);
  int log_level = qc_loc_fw::EL_LOG_OFF;
  mq_server_launch(LOWIUtils::to_logLevel(log_level));
  LOWIController* controller = new (std::nothrow)
             LOWIController (SOCKET_NAME, CONFIG_NAME);

  if ( (NULL != controller) && (0 == controller->init ()) )
  {
    controller->launch();
    controller->join ();
  }
  else
  {
    log_error ("main", "Unable to initialize the LOWIController");
  }
  delete controller;
  controller = 0;
  return 0;
}
