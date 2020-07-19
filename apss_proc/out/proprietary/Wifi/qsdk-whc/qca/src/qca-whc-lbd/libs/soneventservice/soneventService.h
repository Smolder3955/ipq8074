/*
 * @File: soneventService.h
 *
 * @Abstract: lbd son event services
 *
 * @Notes: Provides registered steering events to
 *         user application via son extension library
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef soneventservice__h
#define soneventservice__h

#include "lbd_types.h"
#include "lb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief son event service events used to form reports
 */
enum soneventserviceEvent {
    soneventservice_event_blklist_steer_cmd,
    soneventservice_event_bstm_req_cmd,
    soneventservice_event_blklist_steer_cmd_result,
    soneventservice_event_bstm_query,
    soneventservice_event_bstm_resp,

    soneventservice_max_event
};

/**
 * @brief structure used to convey blklist cmd event
 */
typedef struct soneventserviceBlklistSteerCmdEvent {
    struct ether_addr staAddr;
    u_int32_t blklistType;
    u_int32_t blklistTime;
} soneventserviceBlklistSteerCmdEvent;

/**
 * @brief structure used to convey blklist cmd result event
 */
typedef struct soneventserviceBlklistSteerCmdResultEvent {
    struct ether_addr staAddr;
    LBD_BOOL blklistCmdResult;
} soneventserviceBlklistSteerCmdResultEvent;

/**
 * @brief structure used to convey BTM req event
 */
typedef struct soneventserviceBSTMReqCmdEvent {
    struct ether_addr staAddr;
} soneventserviceBSTMReqCmdEvent;

/**
 * @brief structure used to convey BTM query event
 */
typedef struct soneventserviceBSTMQueryEvent {
    struct ether_addr staAddr;
    u_int32_t reason;
} soneventserviceBSTMQueryEvent; 

/**
 * @brief structure used to convey BTM response event
 */
typedef struct soneventserviceBSTMRespEvent {
    struct ether_addr staAddr;
    u_int32_t status;
} soneventserviceBSTMRespEvent; 

/**
 * @brief Perform son event service initialisation
 *
 * @return LBD_OK if successful initialisation;
 *         otherwise return LBD_NOK
 */
LBD_STATUS soneventservice_init(void);

/**
 * @brief Perform son event service cleanup
 */
void soneventservice_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* soneventService__h */
