/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* C and system library includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <fcntl.h>
#include <wifison_event.h>
#include <time.h>

#define MAC_ADDR_LEN    6
#define MAX_HEX_STR_LEN 2

static inline bool
is_hex(char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F'))
        return true;

    return false;
}

int main(int argc, char **argv)
{
    int socket = 0, error = 0;
    struct sonEventInfo info;
    char *cmdstr;

    socket = wifison_event_init();
    if (socket == -1)
    {
        printf("%s: socket create failed\r\n", __func__);
        return 0;
    }

    wifison_event_register(SON_RE_JOIN_EVENT);
    wifison_event_register(SON_RE_LEAVE_EVENT);
    wifison_event_register(SON_DATA_EVENT);
    wifison_event_register(SON_BSTM_QUERY_EVENT);
    wifison_event_register(SON_BLKLIST_STEER_CMD_EVENT);
    wifison_event_register(SON_BLKLIST_STEER_CMD_RESULT_EVENT);
    wifison_event_register(SON_BSTM_REQ_CMD_EVENT);
    wifison_event_register(SON_BSTM_RESPONSE_EVENT);

    cmdstr = argv[1];

    if (!cmdstr)
    {
        printf("Input commands like:\r\n");
        printf("qca_event_sample recvevent\r\n");
        printf("qca_event_sample senddata XX:XX:XX:XX:XX:XX AA:BB:CC\r\n");
        goto err;
    }
    else if (!strcmp(cmdstr, "recvevent"))
    {
        while (1)
        {
            error = wifison_event_get(&info);

            if (error == EVENT_SOCKET_ERROR)
                goto err;

            if (error == EVENT_OK)
            {
                switch (info.eventMsg)
                {
                    case SON_CLIENT_RESTART_EVENT:
                        printf("Client restart, td database information is disappear (RE maybe leave and wait for new Database update)\r\n");
                        break;

                    case SON_RE_JOIN_EVENT:
                        printf("RE MAC %x:%x:%x:%x:%x:%x is Join as %s\r\n",info.data.re.macaddress[0],info.data.re.macaddress[1],info.data.re.macaddress[2],
                                   info.data.re.macaddress[3],info.data.re.macaddress[4],info.data.re.macaddress[5],info.data.re.isDistantNeighbor?"Distant Neighbor":"Direct Neighbor");
                        break;

                    case SON_RE_LEAVE_EVENT:
                        printf("RE MAC %x:%x:%x:%x:%x:%x is leave\r\n",info.data.re.macaddress[0],info.data.re.macaddress[1],info.data.re.macaddress[2],
                                   info.data.re.macaddress[3],info.data.re.macaddress[4],info.data.re.macaddress[5]);
                        break;

                    case SON_DATA_EVENT:
                    {
                        int i, dataLen;
                        unsigned char *ptr;

                        dataLen = info.length - sizeof(info.data.vdata.macaddress);
                        ptr = info.data.vdata.cus_data;

                        printf("Receive data from MAC %x:%x:%x:%x:%x:%x len:%d \n",
                                info.data.vdata.macaddress[0],info.data.vdata.macaddress[1],info.data.vdata.macaddress[2],
                                info.data.vdata.macaddress[3],info.data.vdata.macaddress[4],info.data.vdata.macaddress[5],
                                dataLen);

                        for (i=0; i<dataLen && i<VENDOR_DATA_LEN_MAX; i++)
                        {
                            printf("%02x:", ptr[i]);
                        }
                        printf("\n");
                    }
                        break;

                    case SON_BSTM_QUERY_EVENT:
                        printf("\nReceived BTM Query event:\n");
                        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",info.data.sReport.mac[0],info.data.sReport.mac[1],
                                info.data.sReport.mac[2],info.data.sReport.mac[3],info.data.sReport.mac[4],
                                info.data.sReport.mac[5]);
                        printf("Timestamp: %s",ctime(&info.data.sReport.timeStamp.tv_sec));
                        printf("Report Type: %u\n",info.data.sReport.reportType);
                        printf("Query reason: %u\n",info.data.sReport.reportData.bstmQueryReason);
                        break;

                    case SON_BLKLIST_STEER_CMD_EVENT:
                        printf("\nReceived blacklist steer cmd event:\n");
                        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",info.data.sReport.mac[0],info.data.sReport.mac[1],
                                info.data.sReport.mac[2],info.data.sReport.mac[3],info.data.sReport.mac[4],
                                info.data.sReport.mac[5]);
                        printf("Timestamp: %s",ctime(&info.data.sReport.timeStamp.tv_sec));
                        printf("Report Type: %u\n",info.data.sReport.reportType);
                        printf("Blacklist Type: %u\n",info.data.sReport.reportData.blkListCmd.blkListType);
                        printf("Blacklist Timeout: %u\n",info.data.sReport.reportData.blkListCmd.timeout);
                        break;

                    case SON_BLKLIST_STEER_CMD_RESULT_EVENT:
                        printf("\nReceived blacklist steer cmd result event:\n");
                        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",info.data.sReport.mac[0],info.data.sReport.mac[1],
                                info.data.sReport.mac[2],info.data.sReport.mac[3],info.data.sReport.mac[4],
                                info.data.sReport.mac[5]);
                        printf("Timestamp: %s",ctime(&info.data.sReport.timeStamp.tv_sec));
                        printf("Report Type: %u\n",info.data.sReport.reportType);
                        printf("Result: %u\n",info.data.sReport.reportData.blkListCmdResult);
                        break;

                    case SON_BSTM_REQ_CMD_EVENT:
                        printf("\nReceived BTM Request event:\n");
                        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",info.data.sReport.mac[0],info.data.sReport.mac[1],
                                info.data.sReport.mac[2],info.data.sReport.mac[3],info.data.sReport.mac[4],
                                info.data.sReport.mac[5]);
                        printf("Timestamp: %s",ctime(&info.data.sReport.timeStamp.tv_sec));
                        printf("Report Type: %u\n",info.data.sReport.reportType);
                        break;

                    case SON_BSTM_RESPONSE_EVENT:
                        printf("\nReceived BTM Response event:\n");
                        printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",info.data.sReport.mac[0],info.data.sReport.mac[1],
                                info.data.sReport.mac[2],info.data.sReport.mac[3],info.data.sReport.mac[4],
                                info.data.sReport.mac[5]);
                        printf("Timestamp: %s",ctime(&info.data.sReport.timeStamp.tv_sec));
                        printf("Report Type: %u\n",info.data.sReport.reportType);
                        printf("Status: %u\n",info.data.sReport.reportData.bstmResponseStatus);
                        break;

                    default:
                        printf("Unknown event %d\n",info.eventMsg);
                }
            }
        }
    }
    else if (!strcmp(cmdstr, "recvcmd"))
    {
        while (1)
        {
            error = wifison_event_get(&info);

            if (error == EVENT_SOCKET_ERROR)
                goto err;

            if (error == EVENT_OK)
            {
                switch (info.eventMsg)
                {
                    case SON_DATA_EVENT:
                    {
                        int dataLen;

                        dataLen = info.length - sizeof(info.data.vdata.macaddress);

                        printf("Receive data from MAC %x:%x:%x:%x:%x:%x len:%d \n",
                                info.data.vdata.macaddress[0],info.data.vdata.macaddress[1],info.data.vdata.macaddress[2],
                                info.data.vdata.macaddress[3],info.data.vdata.macaddress[4],info.data.vdata.macaddress[5],
                                dataLen);

                        printf("%s\n", info.data.vdata.cus_data);
                    }
                        break;
                }
            }
        }
    }
    else if (!strcmp(cmdstr, "senddata"))
    {
        struct vendorData vData;
        char *mac_str, *data_str, *tmp, *str_save;
        unsigned int addr;
        int vDataLen;
        int i, j;

        mac_str = argv[2];
        data_str = argv[3];

        if(!mac_str || !data_str)
        {
            printf("mac (%p) or data (%p) is NULL\n", mac_str, data_str);
            goto err;
        }

        i = j = 0;
        tmp = (void *) strtok_r(mac_str, ":", &str_save);
        while (tmp)
        {
            if (MAC_ADDR_LEN <= i)
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            if ((MAX_HEX_STR_LEN < strlen(tmp)) || (0 == strlen(tmp)))
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            for (j = 0; j < strlen(tmp); j++)
            {
                if (false == is_hex(tmp[j]))
                {
                    printf("Bad value for mac address\n");
                    goto err;
                }
            }

            sscanf(tmp, "%x", &addr);
            if (0xff < addr)
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            vData.macaddress[i++] = addr;
            tmp = (void *) strtok_r(NULL, ":", &str_save);
        }

        if (MAC_ADDR_LEN != i)
        {
            printf("Bad value for mac address\n");
            goto err;
        }

        i = j = 0;
        tmp = (void *) strtok_r(data_str, ":", &str_save);
        while (tmp)
        {
            if (VENDOR_DATA_LEN_MAX <= i)
            {
                printf("Bad value for data\n");
                goto err;
            }

            if ((MAX_HEX_STR_LEN < strlen(tmp)) || (0 == strlen(tmp)))
            {
                printf("Bad value for data\n");
                goto err;
            }

            for (j = 0; j < strlen(tmp); j++)
            {
                if (false == is_hex(tmp[j]))
                {
                    printf("Bad value for data\n");
                    goto err;
                }
            }

            sscanf(tmp, "%x", &addr);
            if (0xff < addr)
            {
                printf("Bad value for data\n");
                goto err;
            }

            vData.cus_data[i++] = addr;
            tmp = (void *) strtok_r(NULL, ":", &str_save);
        }

        vDataLen = sizeof(vData.macaddress) + i;

        wifison_data_send(&vData, vDataLen);
    }
    else if (!strcmp(cmdstr, "sendcmd"))
    {
        struct vendorData vData;
        char *mac_str, *data_str, *tmp, *str_save;
        unsigned int addr;
        int vDataLen;
        int i, j;

        mac_str = argv[2];
        data_str = argv[3];

        if(!mac_str || !data_str)
        {
            printf("mac (%p) or data (%p) is NULL\n", mac_str, data_str);
            goto err;
        }

        i = j = 0;
        tmp = (void *) strtok_r(mac_str, ":", &str_save);
        while (tmp)
        {
            if (MAC_ADDR_LEN <= i)
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            if ((MAX_HEX_STR_LEN < strlen(tmp)) || (0 == strlen(tmp)))
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            for (j = 0; j < strlen(tmp); j++)
            {
                if (false == is_hex(tmp[j]))
                {
                    printf("Bad value for mac address\n");
                    goto err;
                }
            }

            sscanf(tmp, "%x", &addr);
            if (0xff < addr)
            {
                printf("Bad value for mac address\n");
                goto err;
            }

            vData.macaddress[i++] = addr;
            tmp = (void *) strtok_r(NULL, ":", &str_save);
        }

        if (MAC_ADDR_LEN != i)
        {
            printf("Bad value for mac address\n");
            goto err;
        }

        i = (strlen(data_str) > VENDOR_DATA_LEN_MAX) ? VENDOR_DATA_LEN_MAX : strlen(data_str);
        memcpy(vData.cus_data, data_str, i);
        vDataLen = sizeof(vData.macaddress) + i;
        wifison_data_send(&vData, vDataLen);
    }
    else
    {
        printf("Unsupport argv:%s\n", cmdstr);
    }

err:
    wifison_event_deregister(SON_RE_JOIN_EVENT);
    wifison_event_deregister(SON_RE_LEAVE_EVENT);
    wifison_event_deregister(SON_DATA_EVENT);
    wifison_event_deregister(SON_BSTM_QUERY_EVENT);
    wifison_event_deregister(SON_BLKLIST_STEER_CMD_EVENT);
    wifison_event_deregister(SON_BLKLIST_STEER_CMD_RESULT_EVENT);
    wifison_event_deregister(SON_BSTM_REQ_CMD_EVENT);
    wifison_event_deregister(SON_BSTM_RESPONSE_EVENT);
    wifison_event_deinit();

    return 0;
}
