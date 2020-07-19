/*
 * @File: soneventService.c
 *
 * @Abstract: Implementation of son event service APIs
 *
 * @Notes:
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "module.h"
#include "profile.h"
#include "wifison_event.h"
#include "soneventService.h"
#include "steerexec.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <dbg.h>
#include <limits.h>
#include <evloop.h>
#include <bufrd.h>
#include <bufwr.h>

#define soneventserviceReadBufSize       1600

/**
 * @brief Internal structure for son event service state
 */
struct soneventserviceState {
    int isInit;         /* overall initialization done */
    struct dbgModule *dbgModule;  /* debug message context */
    int eventSock;         /* Socket Used for communication with customer Daemon */
    struct bufrd readBuf;   /* for reading from */
    bool isServerStart; /* flag to check if son ext-lib server started */
    bool eventRegFlag[SON_MAX_EVENT]; /* event registration flags */
} soneventserviceState;

#define soneventserviceDebug(level, ...) \
                 dbgf(soneventserviceState.dbgModule,(level),__VA_ARGS__)

/* forward declaration */
static void soneventserviceReadBufCB(void *Cookie);

/**
 * @brief Creates client event socket for communication with
 *        son ext-lib
 *
 * @return socket handle if successful;
 *         otherwise return -1
 */
static int soneventserviceCreateEventSock(void)
{
    int sock;
    int sock_len;
    struct sockaddr_un clientAddr = {
        AF_UNIX,
        SONE_EVENT_SOCKET_LBD
    };

    if ((sock = socket (AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        soneventserviceDebug(DBGERR, "%s:Socket() failed.", __func__);
        goto err;
    }
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sun_family = AF_UNIX;
    strlcpy(clientAddr.sun_path, SONE_EVENT_SOCKET_LBD, sizeof(clientAddr.sun_path));
    sock_len = strlen(SONE_EVENT_SOCKET_LBD);
    clientAddr.sun_path[sock_len] = '\0';
    unlink(clientAddr.sun_path);
    if (bind(sock, (struct sockaddr *)(&clientAddr), sizeof (clientAddr)) == -1)
    {
        soneventserviceDebug(DBGERR, "%s:Bind() failed.", __func__);
        close(sock);
        goto err;
    }
    /* Set nonblock. */
    if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK))
    {
        soneventserviceDebug(DBGERR, "%s failed to set fd NONBLOCK", __func__);
        goto err;
    }

    return sock;
err:
    return -1;
}

/**
 * @brief Send event msg to son ext-lib which will be later
 *        delivered to user application
 *
 * @param [in] msg pointer to steer report
 * @param [in] msgLen steer report len
 */
static void soneventserviceEventMsgTx(u_int8_t *msg, u_int32_t msgLen)
{
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;
    struct sockaddr_un serverAddr = {
        AF_UNIX,
        SONE_EVENT_SOCKET_SERVER
    };

    if (!msgLen && msgLen > MESSAGE_FRAME_LEN_MAX - sizeof(struct service_message))
    {
        soneventserviceDebug(DBGERR, "%s:Invaild msg len(%d)!", __func__, msgLen);
        return;
    }
    message->cmd = EVENT_NOTIFICATION;
    message->len = msgLen;
    memcpy(message->data, msg, message->len);

    if (sendto(soneventserviceState.eventSock, buffer, sizeof(buffer), MSG_DONTWAIT,
            (const struct sockaddr *)(&serverAddr),
            (socklen_t) (sizeof(serverAddr))) < 0) {
        int err = errno;
        if ((err != ECONNREFUSED) && (err != ENOENT) && (err != EAGAIN))
            soneventserviceDebug(DBGERR, "%s:Sendto failed. Error=%d", __func__, errno);
        return;
    }
    soneventserviceDebug(DBGINFO, "%s:Sucessfully sent event msg", __func__);
}

/**
 * @brief Form steer report from the event received
 *
 * @param [in] event pointer to event buffer
 * @param [in] eventID event ID
 */
void soneventserviceSendSteerReport(struct mdEventNode *event, u_int32_t eventID)
{
    struct sonEventInfo info;
    u_int32_t infoLen;
    struct soneventserviceBSTMQueryEvent *bstmQueryEvent;
    struct soneventserviceBlklistSteerCmdEvent *blklistSteerEvent;
    struct soneventserviceBlklistSteerCmdResultEvent *blklistSteerResultEvent;
    struct soneventserviceBSTMReqCmdEvent *bstmReqEvent;
    struct soneventserviceBSTMRespEvent *bstmRespEvent;

    if (!event || !event->Data || !event->DataLen) {
        soneventserviceDebug(DBGERR, "%s Failed to send steer event, No event data", __func__);
        return;
    }

    soneventserviceDebug(DBGERR, "%s Received event %u", __func__, eventID);
    if (soneventserviceState.eventRegFlag[eventID]) {
        memset(&info, 0, sizeof(info));
        info.eventMsg = eventID;
        info.length = sizeof(struct steerReport);
        gettimeofday(&info.data.sReport.timeStamp, NULL);

        switch(eventID) {
            case SON_BSTM_QUERY_EVENT:
                bstmQueryEvent = (struct soneventserviceBSTMQueryEvent *)event->Data;
                lbCopyMACAddr(bstmQueryEvent->staAddr.ether_addr_octet, info.data.sReport.mac);
                info.data.sReport.reportType = BSTM_QUERY;
                info.data.sReport.reportData.bstmQueryReason = bstmQueryEvent->reason;
                soneventserviceDebug(DBGINFO, "%s Received QUERY event", __func__);
                soneventserviceDebug(DBGINFO, "%s MAC:" lbMACAddFmt(":"),__func__, lbMACAddData(info.data.sReport.mac));
                soneventserviceDebug(DBGINFO, "%s report type =  %u", __func__, info.data.sReport.reportType);
                soneventserviceDebug(DBGINFO, "%s query reason = %u", __func__, info.data.sReport.reportData.bstmQueryReason);
                break;
            case SON_BLKLIST_STEER_CMD_EVENT:
                blklistSteerEvent = (struct soneventserviceBlklistSteerCmdEvent *)event->Data;
                lbCopyMACAddr(blklistSteerEvent->staAddr.ether_addr_octet, info.data.sReport.mac);
                info.data.sReport.reportType = BLKLIST_STEER_CMD;
                info.data.sReport.reportData.blkListCmd.blkListType = blklistSteerEvent->blklistType;
                info.data.sReport.reportData.blkListCmd.timeout = blklistSteerEvent->blklistTime;
                soneventserviceDebug(DBGINFO, "%s Received BLKLIST CMD event", __func__);
                soneventserviceDebug(DBGINFO, "%s MAC:" lbMACAddFmt(":"),__func__, lbMACAddData(info.data.sReport.mac));
                soneventserviceDebug(DBGINFO, "%s report type = %u", __func__,info.data.sReport.reportType);
                soneventserviceDebug(DBGINFO, "%s blklist type = %u", __func__,info.data.sReport.reportData.blkListCmd.blkListType);
                soneventserviceDebug(DBGINFO, "%s blklist timeout = %u", __func__,info.data.sReport.reportData.blkListCmd.timeout);
                break;
            case SON_BSTM_REQ_CMD_EVENT:
                bstmReqEvent = (struct soneventserviceBSTMReqCmdEvent *)event->Data;
                lbCopyMACAddr(bstmReqEvent->staAddr.ether_addr_octet, info.data.sReport.mac);
                info.data.sReport.reportType = BSTM_REQ_CMD;
                soneventserviceDebug(DBGINFO, "%s Received BTM REQ CMD event", __func__);
                soneventserviceDebug(DBGINFO, "%s MAC:" lbMACAddFmt(":"),__func__, lbMACAddData(info.data.sReport.mac));
                soneventserviceDebug(DBGINFO, "%s report type = %u", __func__,info.data.sReport.reportType);
                break;
            case SON_BLKLIST_STEER_CMD_RESULT_EVENT:
                blklistSteerResultEvent = (struct soneventserviceBlklistSteerCmdResultEvent *)event->Data;
                lbCopyMACAddr(blklistSteerResultEvent->staAddr.ether_addr_octet, info.data.sReport.mac);
                info.data.sReport.reportType = BLKLIST_STEER_CMD_RESULT;
                info.data.sReport.reportData.blkListCmdResult = blklistSteerResultEvent->blklistCmdResult;
                soneventserviceDebug(DBGINFO, "%s Received BLKLIST CMD RES event", __func__);
                soneventserviceDebug(DBGINFO, "%s MAC:" lbMACAddFmt(":"),__func__, lbMACAddData(info.data.sReport.mac));
                soneventserviceDebug(DBGINFO, "%s report type = %u", __func__,info.data.sReport.reportType);
                soneventserviceDebug(DBGINFO, "%s cmd result = %u", __func__,info.data.sReport.reportData.blkListCmdResult);
                break;
            case SON_BSTM_RESPONSE_EVENT:
                bstmRespEvent = (struct soneventserviceBSTMRespEvent *)event->Data;
                lbCopyMACAddr(bstmRespEvent->staAddr.ether_addr_octet, info.data.sReport.mac);
                info.data.sReport.reportType = BSTM_RESPONSE;
                info.data.sReport.reportData.bstmResponseStatus = bstmRespEvent->status;
                soneventserviceDebug(DBGINFO, "%s Received BRM RESP event", __func__);
                soneventserviceDebug(DBGINFO, "%s MAC:" lbMACAddFmt(":"),__func__, lbMACAddData(info.data.sReport.mac));
                soneventserviceDebug(DBGINFO, "%s report type = %u", __func__,info.data.sReport.reportType);
                soneventserviceDebug(DBGINFO, "%s status = %u", __func__,info.data.sReport.reportData.bstmResponseStatus);
                break;
            default:
                soneventserviceDebug(DBGINFO, "%s Unknown eventID", __func__);
                return;
        }
        infoLen = info.length + sizeof(info.eventMsg) + sizeof(info.length);
        soneventserviceEventMsgTx((u_int8_t *)&info, infoLen);
    }
}

/**
 * @brief Blklist steer cmd event callback function
 *
 * @param [in] event pointer to event buffer
 */
void soneventserviceBlkListSteerCmdCB(struct mdEventNode *event)
{
    soneventserviceSendSteerReport(event, SON_BLKLIST_STEER_CMD_EVENT);
}

/**
 * @brief BTM request event callback function
 *
 * @param [in] event pointer to event buffer
 */
void soneventserviceBSTMReqCmdCB(struct mdEventNode *event)
{
    soneventserviceSendSteerReport(event, SON_BSTM_REQ_CMD_EVENT);
}

/**
 * @brief Blklist steer cmd result event callback function
 *
 * @param [in] event pointer to event buffer
 */
void soneventserviceBlkListSteerCmdResultCB(struct mdEventNode *event)
{
    soneventserviceSendSteerReport(event, SON_BLKLIST_STEER_CMD_RESULT_EVENT);
}

/**
 * @brief BTM query event callback function
 *
 * @param [in] event pointer to event buffer
 */
void soneventserviceBSTMQueryCB(struct mdEventNode *event)
{
    soneventserviceSendSteerReport(event, SON_BSTM_QUERY_EVENT);
}

/**
 * @brief BTM response event callback function
 *
 * @param [in] event pointer to event buffer
 */
void soneventserviceBSTMRespCB(struct mdEventNode *event)
{
    soneventserviceSendSteerReport(event, SON_BSTM_RESPONSE_EVENT);
}

/**
 * @brief Creates event to notify activation of son event service
 *
 * @param [in] enable LBD_TRUE or LBD_FALSE
 */
static void soneventserviceEnable(int enable)
{
    if (LBD_NOK == steerexecSonEventServiceStart(enable)) {
        soneventserviceDebug(DBGINFO, "%s: Failed to create event", __func__);
        return;
    }
    soneventserviceDebug(DBGINFO, "%s: son event service enabled event sent",__func__);
}

/**
 * @brief Receives messages from son ext-lib
 *
 * @param [in] msg pointer to msg buffer
 * @param [in] msgLen msg buffer len
 */
static void soneventserviceEventMsgRx(void *msg, u_int32_t msgLen)
{
    struct service_message *message = (struct service_message *)msg;

    if(!message || !msgLen) {
        soneventserviceDebug(DBGERR, "%s Empty message", __func__);
        return;
    }
    soneventserviceDebug(DBGERR, "%s Event msg RX", __func__);
    switch(message->cmd) {
        case SERVER_START:
            soneventserviceState.isServerStart = LBD_TRUE;
            soneventserviceEnable(LBD_TRUE);
            soneventserviceDebug(DBGINFO, "%s Server started", __func__);
            break;
        case SERVER_STOP:
            soneventserviceState.isServerStart = LBD_FALSE;
            soneventserviceEnable(LBD_FALSE);
            soneventserviceDebug(DBGINFO, "%s Server stopped", __func__);
            break;
        case EVENT_REGISTER:
            switch (message->data[0]) {
                case SON_BSTM_QUERY_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_QUERY_EVENT] = LBD_TRUE;
                    soneventserviceDebug(DBGINFO, "%s Registered Query event", __func__);
                    break;
                case SON_BLKLIST_STEER_CMD_EVENT:
                    soneventserviceState.eventRegFlag[SON_BLKLIST_STEER_CMD_EVENT] = LBD_TRUE;
                    soneventserviceDebug(DBGINFO, "%s Registered blk list steer cmd event", __func__);
                    break;
                case SON_BLKLIST_STEER_CMD_RESULT_EVENT:
                    soneventserviceState.eventRegFlag[SON_BLKLIST_STEER_CMD_RESULT_EVENT] = LBD_TRUE;
                    soneventserviceDebug(DBGINFO, "%s Registered blk list steer cmd result event", __func__);
                    break;
                case SON_BSTM_REQ_CMD_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_REQ_CMD_EVENT] = LBD_TRUE;
                    soneventserviceDebug(DBGINFO, "%s Registered bstm req cmd event", __func__);
                    break;
                case SON_BSTM_RESPONSE_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_RESPONSE_EVENT] = LBD_TRUE;
                    soneventserviceDebug(DBGINFO, "%s Registered bstm resp event", __func__);
                    break;
                default:
                    soneventserviceDebug(DBGINFO, "%s Reg event Ignored", __func__);
                    break;
            }
            break;
        case EVENT_DEREGISTER:
            switch (message->data[0]) {
                case SON_BSTM_QUERY_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_QUERY_EVENT] = LBD_FALSE;
                    soneventserviceDebug(DBGINFO, "%s DeRegistered Query event", __func__);
                    break;
                case SON_BLKLIST_STEER_CMD_EVENT:
                    soneventserviceState.eventRegFlag[SON_BLKLIST_STEER_CMD_EVENT] = LBD_FALSE;
                    soneventserviceDebug(DBGINFO, "%s DeRegistered blk list steer cmd event", __func__);
                    break;
                case SON_BLKLIST_STEER_CMD_RESULT_EVENT:
                    soneventserviceState.eventRegFlag[SON_BLKLIST_STEER_CMD_RESULT_EVENT] = LBD_FALSE;
                    soneventserviceDebug(DBGINFO, "%s DeRegistered blk list steer cmd result event", __func__);
                    break;
                case SON_BSTM_REQ_CMD_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_REQ_CMD_EVENT] = LBD_FALSE;
                    soneventserviceDebug(DBGINFO, "%s DeRegistered bstm req cmd event", __func__);
                    break;
                case SON_BSTM_RESPONSE_EVENT:
                    soneventserviceState.eventRegFlag[SON_BSTM_RESPONSE_EVENT] = LBD_FALSE;
                    soneventserviceDebug(DBGINFO, "%s DeRegistered bstm resp event", __func__);
                    break;
                default:
                    soneventserviceDebug(DBGINFO, "%s Dereg event Ignored", __func__);
                    break;
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Notifies son ext-lib that this client has started
 */
static void soneventserviceEventstart(void)
{
    unsigned int id = SON_EVENT_CLIENT_LBD;
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;
    struct sockaddr_un serverAddr = {
        AF_UNIX,
        SONE_EVENT_SOCKET_SERVER
    };

    message->cmd = CLIENT_START;
    message->len = sizeof(id);
    memcpy(message->data, &id, message->len);

    if (sendto(soneventserviceState.eventSock, buffer, sizeof(buffer), MSG_DONTWAIT,
            (const struct sockaddr *)(&serverAddr),
            (socklen_t) (sizeof(serverAddr))) < 0) {
        int err = errno;
        if ((err != ECONNREFUSED) && (err != ENOENT) && (err != EAGAIN))
            soneventserviceDebug(DBGERR, "%s:Sendto failed. Error=%d", __func__, errno);
    }
}

/**
 * @brief son event service registration function
 */
static void soneventserviceEventRegister(void)
{
    soneventserviceState.eventSock = soneventserviceCreateEventSock();

    if (soneventserviceState.eventSock < 0) {
        soneventserviceDebug(DBGERR, "%s: Unix domain socket create failed", __func__);
        return;
    }

    /* Initialize input context */
    bufrdCreate(&soneventserviceState.readBuf, "soneventservice-rd",
        soneventserviceState.eventSock,
        soneventserviceReadBufSize  /* Read buf size */,
        soneventserviceReadBufCB, /* callback */
        NULL);

    soneventserviceEventstart();
}

/**
 * @brief son event service un-registration function
 */
static void soneventserviceEventUnRegister(void)
{
    if (close(soneventserviceState.eventSock) != 0) {
        soneventserviceDebug(DBGERR, "%s: Socket close failed", __func__);
    }

    soneventserviceState.eventSock = -1;
    bufrdDestroy(&soneventserviceState.readBuf);
}

/**
 * @brief Socket buffer read callback function
 *
 * @param [in] Cookie unused
 */
static void soneventserviceReadBufCB(void *Cookie)
{
    struct bufrd *readBuf =  &soneventserviceState.readBuf;
    u_int32_t numBytes = bufrdNBytesGet(readBuf);
    u_int8_t *msg = bufrdBufGet(readBuf);

    /* Error check. */
    if (bufrdErrorGet(readBuf)) {
        soneventserviceDebug(DBGERR, "%s: Read error!", __func__);

        soneventserviceEventUnRegister();
        soneventserviceEventRegister();

        if (soneventserviceState.eventSock == -1) {
            soneventserviceDebug(DBGERR, "%s: Failed to recover from fatal error!", __func__);
            exit(1);
        }
        return;
    }

    if(!numBytes)
        return;

    soneventserviceEventMsgRx(msg, numBytes);
    bufrdConsume(readBuf, numBytes);
}

/**
 * @brief son event service listen init callback function
 */
void soneventserviceListenInitCB(void)
{
    mdListenTableRegister(mdModuleID_SonEventService, soneventservice_event_blklist_steer_cmd,
                          soneventserviceBlkListSteerCmdCB);
    mdListenTableRegister(mdModuleID_SonEventService, soneventservice_event_bstm_req_cmd,
                          soneventserviceBSTMReqCmdCB);
    mdListenTableRegister(mdModuleID_SonEventService, soneventservice_event_blklist_steer_cmd_result,
                          soneventserviceBlkListSteerCmdResultCB);
    mdListenTableRegister(mdModuleID_SonEventService, soneventservice_event_bstm_query,
                          soneventserviceBSTMQueryCB);
    mdListenTableRegister(mdModuleID_SonEventService, soneventservice_event_bstm_resp,
                          soneventserviceBSTMRespCB);
}

/**
 * @brief son event service initialisation function
 */
LBD_STATUS soneventservice_init(void)
{
    if(soneventserviceState.isInit)
        return LBD_OK;

    memset(&soneventserviceState, 0, sizeof(soneventserviceState));
    soneventserviceState.isInit = 1;
    soneventserviceState.eventSock = -1;
    soneventserviceState.dbgModule = dbgModuleFind("seservice");

    soneventserviceEventRegister();
    mdEventTableRegister(mdModuleID_SonEventService, soneventservice_max_event);
    mdListenInitCBRegister(mdModuleID_SonEventService, soneventserviceListenInitCB);

    return LBD_OK;
}

/**
 * @brief son event service destruction function
 */
void soneventservice_fini(void)
{
    soneventserviceState.isInit = 0;
    soneventserviceEventUnRegister();
}
