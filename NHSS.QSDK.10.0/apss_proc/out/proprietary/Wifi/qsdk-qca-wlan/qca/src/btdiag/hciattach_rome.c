/* 
 * Copyright (c) 2014 Qualcomm Atheros, Inc. 
 * All Rights Reserved.
 * Not a Contribution. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */
/*
 *
 *  Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *  Not a Contribution.
 *
 *  Copyright 2012 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you
 *  may not use this file except in compliance with the License. You may
 *  obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  permissions and limitations under the License.
 *
 */

/******************************************************************************
 *
 *  Filename:      hciattach_rome.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
//#include <bluetooth/bluetooth.h>
#include "hciattach_rome.h"
#include "connection.h"

#ifdef __cplusplus
}
#endif


/******************************************************************************
**  Variables
******************************************************************************/
FILE *file;
unsigned char *phdr_buffer;
unsigned char *pdata_buffer = NULL;
patch_info rampatch_patch_info;
int rome_ver = ROME_VER_UNKNOWN;
unsigned char gTlv_type;
unsigned char gTlv_dwndCfg;
char *rampatch_file_path;
char *nvm_file_path;
vnd_userial_cb_t vnd_userial;
unsigned char wait_vsc_evt = TRUE;
unsigned char wait_cc_evt = TRUE;
/******************************************************************************
**  Extern variables
******************************************************************************/
//extern unsigned char vnd_local_bd_addr[6];

/*****************************************************************************
**   Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_to_tcio_baud
**
** Description     helper function converts USERIAL baud rates into TCIO
**                  conforming baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
unsigned char userial_to_tcio_baud(unsigned char cfg_baud, unsigned int *baud)
{
    if (cfg_baud == USERIAL_BAUD_115200)
        *baud = B115200;
    else if (cfg_baud == USERIAL_BAUD_4M)
        *baud = B4000000;
    else if (cfg_baud == USERIAL_BAUD_3M)
        *baud = B3000000;
    else if (cfg_baud == USERIAL_BAUD_2M)
        *baud = B2000000;
    else if (cfg_baud == USERIAL_BAUD_1M)
        *baud = B1000000;
    else if (cfg_baud == USERIAL_BAUD_921600)
        *baud = B921600;
    else if (cfg_baud == USERIAL_BAUD_460800)
        *baud = B460800;
    else if (cfg_baud == USERIAL_BAUD_230400)
        *baud = B230400;
    else if (cfg_baud == USERIAL_BAUD_57600)
        *baud = B57600;
    else if (cfg_baud == USERIAL_BAUD_19200)
        *baud = B19200;
    else if (cfg_baud == USERIAL_BAUD_9600)
        *baud = B9600;
    else if (cfg_baud == USERIAL_BAUD_1200)
        *baud = B1200;
    else if (cfg_baud == USERIAL_BAUD_600)
        *baud = B600;
    else
    {
        fprintf(stderr,  "userial vendor open: unsupported baud idx %i\n", cfg_baud);
        *baud = B115200;
        return FALSE;
    }

    return TRUE;
}


/*******************************************************************************
**
** Function        userial_vendor_set_baud
**
** Description     Set new baud rate
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_set_baud(unsigned char userial_baud)
{
    unsigned int tcio_baud;
    fprintf(stderr, "## userial_vendor_set_baud: %d\n", userial_baud);

    userial_to_tcio_baud(userial_baud, &tcio_baud);

    cfsetospeed(&vnd_userial.termios, tcio_baud);
    cfsetispeed(&vnd_userial.termios, tcio_baud);
    tcsetattr(vnd_userial.fd, TCSADRAIN, &vnd_userial.termios); /* don't change speed until last write done */

}


/*******************************************************************************
**
** Function        userial_vendor_ioctl
**
** Description     ioctl inteface
**
** Returns         None
**
*******************************************************************************/
int userial_vendor_ioctl(int fd, userial_vendor_ioctl_op_t op, int *p_data)
{
    int err = -1;
    struct termios ti;

    if (tcgetattr(fd, &ti) < 0) {
            perror("Can't get port settings");
            return -1;
    }
    cfmakeraw(&ti);
    ti.c_cflag |= CLOCAL;
    ti.c_cflag |= CREAD;
    //ti.c_cflag |= PARENB;

    switch(op)
    {
#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
        case USERIAL_OP_ASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: Asserting BT_Wake ##");
            err = ioctl(fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
            break;

        case USERIAL_OP_DEASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: De-asserting BT_Wake ##");
            err = ioctl(fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
            break;

        case USERIAL_OP_GET_BT_WAKE_STATE:
            err = ioctl(fd, USERIAL_IOCTL_BT_WAKE_GET_ST, p_data);
            break;
#endif  //  (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
        case USERIAL_OP_FLOW_ON:
            fprintf(stderr, "## userial_vendor_ioctl: UART Flow On\n ");
            ti.c_cflag |= CRTSCTS;

            if (err = tcsetattr(fd, TCSANOW, &ti) < 0) {
                perror("Can't set port settings");
                return -1;
            }

            break;

        case USERIAL_OP_FLOW_OFF:
            fprintf(stderr, "## userial_vendor_ioctl: UART Flow Off\n ");
            ti.c_cflag &= ~CRTSCTS;
            if (err = tcsetattr(fd, TCSANOW, &ti) < 0) {
                fprintf(stderr, "Can't set port settings");
                return -1;
            }
            break;

        default:
            break;
    }

    return err;
}


int get_vs_hci_event(unsigned char *rsp)
{
    int err = 0, soc_id =0;
    unsigned char paramlen = 0;

    if( (rsp[EVENTCODE_OFFSET] == VSEVENT_CODE) || (rsp[EVENTCODE_OFFSET] == EVT_CMD_COMPLETE))
        fprintf(stderr, "%s: Received HCI-Vendor Specific event\n", __FUNCTION__);
    else {
        fprintf(stderr, "%s: Failed to receive HCI-Vendor Specific event\n", __FUNCTION__);
        err = -EIO;
        goto failed;
    }

    fprintf(stderr, "%s: Parameter Length: 0x%x\n", __FUNCTION__, paramlen = rsp[EVT_PLEN]);
    fprintf(stderr, "%s: Command response: 0x%x\n", __FUNCTION__, rsp[CMD_RSP_OFFSET]);
    fprintf(stderr, "%s: Response type   : 0x%x\n", __FUNCTION__, rsp[RSP_TYPE_OFFSET]);

    /* Check the status of the operation */
    switch ( rsp[CMD_RSP_OFFSET] )
    {
        case EDL_CMD_REQ_RES_EVT:
        fprintf(stderr, "%s: Command Request Response\n", __FUNCTION__);
        switch(rsp[RSP_TYPE_OFFSET])
        {
            case EDL_PATCH_VER_RES_EVT:
            case EDL_APP_VER_RES_EVT:
                fprintf(stderr, "\t Current Product ID\t\t: 0x%08x\n",
                    (unsigned int)(rsp[PATCH_PROD_ID_OFFSET +3] << 24 |
                                        rsp[PATCH_PROD_ID_OFFSET+2] << 16 |
                                        rsp[PATCH_PROD_ID_OFFSET+1] << 8 |
                                        rsp[PATCH_PROD_ID_OFFSET]  ));

                /* Patch Version indicates FW patch version */
                fprintf(stderr, "\t Current Patch Version\t\t: 0x%04x\n",
                    (unsigned short)(rsp[PATCH_PATCH_VER_OFFSET + 1] << 8 |
                                            rsp[PATCH_PATCH_VER_OFFSET] ));

                /* ROM Build Version indicates ROM build version like 1.0/1.1/2.0 */
                fprintf(stderr, "\t Current ROM Build Version\t: 0x%04x\n", rome_ver =
                    (int)(rsp[PATCH_ROM_BUILD_VER_OFFSET + 1] << 8 |
                                            rsp[PATCH_ROM_BUILD_VER_OFFSET] ));

                /* In case rome 1.0/1.1, there is no SOC ID version available */
                if (paramlen - 10)
                {
                    fprintf(stderr, "\t Current SOC Version\t\t: 0x%08x\n", soc_id =
                        (unsigned int)(rsp[PATCH_SOC_VER_OFFSET +3] << 24 |
                                                rsp[PATCH_SOC_VER_OFFSET+2] << 16 |
                                                rsp[PATCH_SOC_VER_OFFSET+1] << 8 |
                                                rsp[PATCH_SOC_VER_OFFSET]  ));
                }

                /* Rome Chipset Version can be decided by Patch version and SOC version,
                Upper 2 bytes will be used for Patch version and Lower 2 bytes will be
                used for SOC as combination for BT host driver */
                rome_ver = (rome_ver << 16) | (soc_id & 0x0000ffff);
                break;
            case EDL_TVL_DNLD_RES_EVT:
            case EDL_CMD_EXE_STATUS_EVT:
                switch (err = rsp[CMD_STATUS_OFFSET])
                    {
                    case HCI_CMD_SUCCESS:
                        fprintf(stderr, "%s: Download Packet successfully!\n", __FUNCTION__);
                        break;
                    case PATCH_LEN_ERROR:
                        fprintf(stderr, "%s: Invalid patch length argument passed for EDL PATCH "
                        "SET REQ cmd\n", __FUNCTION__);
                        break;
                    case PATCH_VER_ERROR:
                        fprintf(stderr, "%s: Invalid patch version argument passed for EDL PATCH "
                        "SET REQ cmd\n", __FUNCTION__);
                        break;
                    case PATCH_CRC_ERROR:
                        fprintf(stderr, "%s: CRC check of patch failed!!!\n", __FUNCTION__);
                        break;
                    case PATCH_NOT_FOUND:
                        fprintf(stderr, "%s: Invalid patch data!!!\n", __FUNCTION__);
                        break;
                    case TLV_TYPE_ERROR:
                        fprintf(stderr, "%s: TLV Type Error !!!\n", __FUNCTION__);
                        break;
                    default:
                        fprintf(stderr, "%s: Undefined error (0x%x)", __FUNCTION__, err);
                        break;
                    }
            break;
        }
        break;

        case NVM_ACCESS_CODE:
            fprintf(stderr, "%s: NVM Access Code!!!\n", __FUNCTION__);
            err = HCI_CMD_SUCCESS;
            break;
        case EDL_SET_BAUDRATE_RSP_EVT:
            /* Rome 1.1 has bug with the response, so it should ignore it. */
            if (rsp[BAUDRATE_RSP_STATUS_OFFSET] != BAUDRATE_CHANGE_SUCCESS)
            {
                fprintf(stderr, "%s: Set Baudrate request failed - 0x%x\n", __FUNCTION__,
                    rsp[CMD_STATUS_OFFSET]);
                err = -1;
            }
            break;
        default:
            fprintf(stderr, "%s: Not a valid status!!!\n", __FUNCTION__);
            err = -1;
            break;
    }

failed:
    return err;
}

int wait_for_data(int fd, int maxTimeOut)
{
    fd_set infids;
    struct timeval timeout;

    if (maxTimeOut <= 0) {
        fprintf(stderr, "%s: Invalid timeout value specified", __func__);
        return -EINVAL;
    }

    FD_ZERO (&infids);
    FD_SET (fd, &infids);
    timeout.tv_sec = maxTimeOut;
    timeout.tv_usec = 0;

    /* Check whether data is available in TTY buffer before calling read() */
    if (select (fd + 1, &infids, NULL, NULL, &timeout) < 1) {
        fprintf(stderr, "%s: Timing out on select for %d secs.\n", __FUNCTION__, maxTimeOut);
        return -1;
    }
    else
        fprintf(stderr, "%s: HCI-VS-EVENT available in TTY Serial buffer\n",
            __FUNCTION__);

    return 1;
}

/*
 * Read an VS HCI event from the given file descriptor.
 */
int read_vs_hci_event(int fd, unsigned char* buf, int size)
{
    int remain, r, retry = 0;
    int count = 0;

    if (size <= 0) {
        fprintf(stderr, "Invalid size arguement!\n");
        return -1;
    }

    fprintf(stderr, "%s: Wait for HCI-Vendor Specfic Event from SOC\n",
        __FUNCTION__);

    /* Check whether data is available in TTY buffer before calling read() */
    if (wait_for_data(fd, SELECT_TIMEOUT) < 1)
        return -1;

    /* The first byte identifies the packet type. For HCI event packets, it
     * should be 0x04, so we read until we get to the 0x04. */
    /* It will keep reading until find 0x04 byte */
    while (1) {
            /* Read UART Buffer for HCI-DATA */
            r = read(fd, buf, 1);
            if (r <= 0) {
                fprintf(stderr, "%s: read() failed. error: %d\n",
                    __FUNCTION__, r);
                return -1;
            }

            /* Check if received data is HCI-DATA or not.
             * If not HCI-DATA, then retry reading the UART Buffer once.
             * Sometimes there could be corruption on the UART lines and to
             * avoid that retry once reading the UART Buffer for HCI-DATA.
             */
            if (buf[0] == 0x04) { /* Recvd. HCI DATA */
                    retry = 0;
                    break;
            }
            else if (retry < MAX_RETRY_CNT){ /* Retry mechanism */
                retry++;
                fprintf(stderr, "%s: Not an HCI-VS-Event! buf[0]: %d",
                    __FUNCTION__, buf[0]);
                if (wait_for_data(fd, SELECT_TIMEOUT) < 1)
                    return -1;
                else /* Data available in UART Buffer: Continue to read */
                    continue;
            }
            else { /* RETRY failed : Exiting with failure */
                fprintf(stderr, "%s: RETRY failed!", __FUNCTION__);
                return -1;
            }
    }
    count++;

    fprintf(stderr, "%s: Wait for HCI-Vendor Specfic Event from SOC, buf[0] - 0x%x\n", __FUNCTION__, buf[0]);
    /* The next two bytes are the event code and parameter total length. */
    while (count < 3) {
            r = read(fd, buf + count, 3 - count);
            if ((r <= 0) || (buf[1] != 0xFF )) {
                fprintf(stderr, "It is not VS event !!\n");
                return -1;
            }
            count += r;
    }

    fprintf(stderr, "%s: Wait for HCI-Vendor Specfic Event from SOC, buf[1] - 0x%x\n", __FUNCTION__, buf[1]);
    /* Now we read the parameters. */
    if (buf[2] < (size - 3))
            remain = buf[2];
    else
            remain = size - 3;

    while ((count - 3) < remain) {
            r = read(fd, buf + count, remain - (count - 3));
            if (r <= 0)
                    return -1;
            count += r;
    }

     /* Check if the set patch command is successful or not */
    if(get_vs_hci_event(buf) != HCI_CMD_SUCCESS)
        return -1;

    fprintf(stderr, "%s: Wait for HCI-Vendor Specfic Event from SOC, count - 0x%x\n", __FUNCTION__, count);
    return count;
}


int hci_send_vs_cmd(int fd, unsigned char *cmd, unsigned char *rsp, int size)
{
    int ret = 0;

    /* Send the HCI command packet to UART for transmission */
    ret = write(fd, cmd, size);
    if (ret != size) {
        fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, ret);
        goto failed;
    }

    if (wait_vsc_evt) {
        /* Check for response from the Controller */
        if (read_vs_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE) < 0) {
            ret = -ETIMEDOUT;
            fprintf(stderr, "%s: Failed to get HCI-VS Event from SOC\n", __FUNCTION__);
            goto failed;
        }
        fprintf(stderr, "%s: Received HCI-Vendor Specific Event from SOC\n", __FUNCTION__);
    }

failed:
    return ret;
}

void frame_hci_cmd_pkt(
    unsigned char *cmd,
    int edl_cmd, unsigned int p_base_addr,
    int segtNo, int size
    )
{
    int offset = 0;
    hci_command_hdr *cmd_hdr;

    memset(cmd, 0x0, HCI_MAX_CMD_SIZE);

    cmd_hdr = (void *) (cmd + 1);

    cmd[0]      = HCI_COMMAND_PKT;
    cmd_hdr->opcode = cmd_opcode_pack(HCI_VENDOR_CMD_OGF, HCI_PATCH_CMD_OCF);
    cmd_hdr->plen   = size;
    cmd[4]      = edl_cmd;

    switch (edl_cmd)
    {
        case EDL_PATCH_SET_REQ_CMD:
            /* Copy the patch header info as CMD params */
            memcpy(&cmd[5], phdr_buffer, PATCH_HDR_LEN);
            fprintf(stderr, "%s: Sending EDL_PATCH_SET_REQ_CMD\n", __FUNCTION__);
            fprintf(stderr, "HCI-CMD %d:\t0x%x \t0x%x \t0x%x \t0x%x \t0x%x\n",
                segtNo, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);
            break;
        case EDL_PATCH_DLD_REQ_CMD:
            offset = ((segtNo - 1) * MAX_DATA_PER_SEGMENT);
            p_base_addr += offset;
            cmd_hdr->plen   = (size + 6);
            cmd[5]  = (size + 4);
            cmd[6]  = EXTRACT_BYTE(p_base_addr, 0);
            cmd[7]  = EXTRACT_BYTE(p_base_addr, 1);
            cmd[8]  = EXTRACT_BYTE(p_base_addr, 2);
            cmd[9]  = EXTRACT_BYTE(p_base_addr, 3);
            memcpy(&cmd[10], (pdata_buffer + offset), size);

            fprintf(stderr, "%s: Sending EDL_PATCH_DLD_REQ_CMD: size: %d bytes\n",
                __FUNCTION__, size);
            fprintf(stderr, "HCI-CMD %d:\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t"
                "0x%x\t0x%x\t0x%x\t\n", segtNo, cmd[0], cmd[1], cmd[2],
                cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
            break;
        case EDL_PATCH_ATCH_REQ_CMD:
            fprintf(stderr, "%s: Sending EDL_PATCH_ATTACH_REQ_CMD\n", __FUNCTION__);
            fprintf(stderr, "HCI-CMD %d:\t0x%x \t0x%x \t0x%x \t0x%x \t0x%x\n",
            segtNo, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);
            break;
        case EDL_PATCH_RST_REQ_CMD:
            fprintf(stderr, "%s: Sending EDL_PATCH_RESET_REQ_CMD\n", __FUNCTION__);
            fprintf(stderr, "HCI-CMD %d:\t0x%x \t0x%x \t0x%x \t0x%x \t0x%x\n",
            segtNo, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);
            break;
        case EDL_PATCH_VER_REQ_CMD:
            fprintf(stderr, "%s: Sending EDL_PATCH_VER_REQ_CMD\n", __FUNCTION__);
            fprintf(stderr, "HCI-CMD %d:\t0x%x \t0x%x \t0x%x \t0x%x \t0x%x\n",
            segtNo, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);
            break;
        case EDL_PATCH_TLV_REQ_CMD:
            fprintf(stderr, "%s: Sending EDL_PATCH_TLV_REQ_CMD\n", __FUNCTION__);
            /* Parameter Total Length */
            cmd[3] = size +2;

            /* TLV Segment Length */
            cmd[5] = size;
            fprintf(stderr, "HCI-CMD %d:\t0x%x \t0x%x \t0x%x \t0x%x \t0x%x \t0x%x\n",
            segtNo, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
            offset = (segtNo * MAX_SIZE_PER_TLV_SEGMENT);
            memcpy(&cmd[6], (pdata_buffer + offset), size);
            break;
        default:
            fprintf(stderr, "%s: Unknown EDL CMD !!!\n", __FUNCTION__);
    }
}

void rome_extract_patch_header_info(unsigned char *buf)
{
    int index;

    /* Extract patch id */
    for (index = 0; index < 4; index++)
        rampatch_patch_info.patch_id |=
            (LSH(buf[index + P_ID_OFFSET], (index * 8)));

    /* Extract (ROM and BUILD) version information */
    for (index = 0; index < 2; index++)
        rampatch_patch_info.patch_ver.rom_version |=
            (LSH(buf[index + P_ROME_VER_OFFSET], (index * 8)));

    for (index = 0; index < 2; index++)
        rampatch_patch_info.patch_ver.build_version |=
            (LSH(buf[index + P_BUILD_VER_OFFSET], (index * 8)));

    /* Extract patch base and entry addresses */
    for (index = 0; index < 4; index++)
        rampatch_patch_info.patch_base_addr |=
            (LSH(buf[index + P_BASE_ADDR_OFFSET], (index * 8)));

    /* Patch BASE & ENTRY addresses are same */
    rampatch_patch_info.patch_entry_addr = rampatch_patch_info.patch_base_addr;

    /* Extract total length of the patch payload */
    for (index = 0; index < 4; index++)
        rampatch_patch_info.patch_length |=
            (LSH(buf[index + P_LEN_OFFSET], (index * 8)));

    /* Extract the CRC checksum of the patch payload */
    for (index = 0; index < 4; index++)
        rampatch_patch_info.patch_crc |=
            (LSH(buf[index + P_CRC_OFFSET], (index * 8)));

    /* Extract patch control value */
    for (index = 0; index < 4; index++)
        rampatch_patch_info.patch_ctrl |=
            (LSH(buf[index + P_CONTROL_OFFSET], (index * 8)));

    fprintf(stderr, "PATCH_ID\t : 0x%x\n", rampatch_patch_info.patch_id);
    fprintf(stderr, "ROM_VERSION\t : 0x%x\n", rampatch_patch_info.patch_ver.rom_version);
    fprintf(stderr, "BUILD_VERSION\t : 0x%x\n", rampatch_patch_info.patch_ver.build_version);
    fprintf(stderr, "PATCH_LENGTH\t : 0x%x\n", rampatch_patch_info.patch_length);
    fprintf(stderr, "PATCH_CRC\t : 0x%x\n", rampatch_patch_info.patch_crc);
    fprintf(stderr, "PATCH_CONTROL\t : 0x%x\n", rampatch_patch_info.patch_ctrl);
    fprintf(stderr, "PATCH_BASE_ADDR\t : 0x%x\n", rampatch_patch_info.patch_base_addr);

}

int rome_edl_set_patch_request(int fd)
{
    int size, err;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

    /* Frame the HCI CMD to be sent to the Controller */
    frame_hci_cmd_pkt(cmd, EDL_PATCH_SET_REQ_CMD, 0,
        -1, PATCH_HDR_LEN + 1);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND	+ HCI_COMMAND_HDR_SIZE + cmd[PLEN]);

    /* Send HCI Command packet to Controller */
    err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
    if ( err != size) {
        fprintf(stderr, "Failed to set the patch info to the Controller!\n");
        goto error;
    }

    err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
    if ( err < 0) {
        fprintf(stderr, "%s: Failed to set patch info on Controller\n", __FUNCTION__);
        goto error;
    }
    fprintf(stderr, "%s: Successfully set patch info on the Controller\n", __FUNCTION__);
error:
    return err;
}

int rome_edl_patch_download_request(int fd)
{
    int no_of_patch_segment;
    int index = 1, err = 0, size = 0;
    unsigned int p_base_addr;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

    no_of_patch_segment = (rampatch_patch_info.patch_length /
        MAX_DATA_PER_SEGMENT);
    fprintf(stderr, "%s: %d patch segments to be d'loaded from patch base addr: 0x%x\n",
        __FUNCTION__, no_of_patch_segment,
    rampatch_patch_info.patch_base_addr);

    /* Initialize the patch base address from the one read from bin file */
    p_base_addr = rampatch_patch_info.patch_base_addr;

    /*
    * Depending upon size of the patch payload, download the patches in
    * segments with a max. size of 239 bytes
    */
    for (index = 1; index <= no_of_patch_segment; index++) {

        fprintf(stderr, "%s: Downloading patch segment: %d\n", __FUNCTION__, index);

        /* Frame the HCI CMD PKT to be sent to Controller*/
        frame_hci_cmd_pkt(cmd, EDL_PATCH_DLD_REQ_CMD, p_base_addr,
        index, MAX_DATA_PER_SEGMENT);

        /* Total length of the packet to be sent to the Controller */
        size = (HCI_CMD_IND	+ HCI_COMMAND_HDR_SIZE + cmd[PLEN]);

        /* Initialize the RSP packet everytime to 0 */
        memset(rsp, 0x0, HCI_MAX_EVENT_SIZE);

        /* Send HCI Command packet to Controller */
        err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
        if ( err != size) {
            fprintf(stderr, "Failed to send the patch payload to the Controller!\n");
            goto error;
        }

        /* Read Command Complete Event */
        err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
        if ( err < 0) {
            fprintf(stderr, "%s: Failed to downlaod patch segment: %d!\n",
            __FUNCTION__, index);
            goto error;
        }
        fprintf(stderr, "%s: Successfully downloaded patch segment: %d\n",
        __FUNCTION__, index);
    }

    /* Check if any pending patch data to be sent */
    size = (rampatch_patch_info.patch_length < MAX_DATA_PER_SEGMENT) ?
        rampatch_patch_info.patch_length :
        (rampatch_patch_info.patch_length  % MAX_DATA_PER_SEGMENT);

    if (size)
    {
        /* Frame the HCI CMD PKT to be sent to Controller*/
        frame_hci_cmd_pkt(cmd, EDL_PATCH_DLD_REQ_CMD, p_base_addr, index, size);

        /* Initialize the RSP packet everytime to 0 */
        memset(rsp, 0x0, HCI_MAX_EVENT_SIZE);

        /* Total length of the packet to be sent to the Controller */
        size = (HCI_CMD_IND	+ HCI_COMMAND_HDR_SIZE + cmd[PLEN]);

        /* Send HCI Command packet to Controller */
        err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
        if ( err != size) {
            fprintf(stderr, "Failed to send the patch payload to the Controller!\n");
            goto error;
        }

        /* Read Command Complete Event */
        err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
        if ( err < 0) {
            fprintf(stderr, "%s: Failed to downlaod patch segment: %d!\n",
                __FUNCTION__, index);
            goto error;
        }

        fprintf(stderr, "%s: Successfully downloaded patch segment: %d\n",
        __FUNCTION__, index);
    }

error:
    return err;
}

static int rome_download_rampatch(int fd)
{
    int c, size, index, ret = -1;

    fprintf(stderr, "%s:\n", __FUNCTION__);

    /* Get handle to the RAMPATCH binary file */
    fprintf(stderr, "%s: Getting handle to the RAMPATCH binary file from %s\n", __FUNCTION__, ROME_FW_PATH);
    file = fopen(ROME_FW_PATH, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: Failed to get handle to the RAMPATCH bin file!\n",
        __FUNCTION__);
        return -ENFILE;
    }

    /* Allocate memory for the patch headder info */
    fprintf(stderr, "%s: Allocating memory for the patch header\n", __FUNCTION__);
    phdr_buffer = (unsigned char *) malloc(PATCH_HDR_LEN + 1);
    if (phdr_buffer == NULL) {
        fprintf(stderr, "%s: Failed to allocate memory for patch header\n",
        __FUNCTION__);
        goto phdr_alloc_failed;
    }
    for (index = 0; index < PATCH_HDR_LEN + 1; index++)
        phdr_buffer[index] = 0x0;

    /* Read 28 bytes of patch header information */
    fprintf(stderr, "%s: Reading patch header info\n", __FUNCTION__);
    index = 0;
    do {
        c = fgetc (file);
        phdr_buffer[index++] = (unsigned char)c;
    } while (index != PATCH_HDR_LEN);

    /* Save the patch header info into local structure */
    fprintf(stderr, "%s: Saving patch hdr. info\n", __FUNCTION__);
    rome_extract_patch_header_info((unsigned char *)phdr_buffer);

    /* Set the patch header info onto the Controller */
    ret = rome_edl_set_patch_request(fd);
    if (ret < 0) {
        fprintf(stderr, "%s: Error setting the patchheader info!\n", __FUNCTION__);
        goto pdata_alloc_failed;
    }

    /* Allocate memory for the patch payload */
    fprintf(stderr, "%s: Allocating memory for patch payload\n", __FUNCTION__);
    size = rampatch_patch_info.patch_length;
    pdata_buffer = (unsigned char *) malloc(size+1);
    if (pdata_buffer == NULL) {
        fprintf(stderr, "%s: Failed to allocate memory for patch payload\n",
            __FUNCTION__);
        goto pdata_alloc_failed;
    }
    for (index = 0; index < size+1; index++)
        pdata_buffer[index] = 0x0;

    /* Read the patch data from Rampatch binary image */
    fprintf(stderr, "%s: Reading patch payload from RAMPATCH file\n", __FUNCTION__);
    index = 0;
    do {
        c = fgetc (file);
        pdata_buffer[index++] = (unsigned char)c;
    } while (c != EOF);

    /* Downloading patches in segments to controller */
    ret = rome_edl_patch_download_request(fd);
    if (ret < 0) {
        fprintf(stderr, "%s: Error downloading patch segments!\n", __FUNCTION__);
        goto cleanup;
    }
cleanup:
    free(pdata_buffer);
pdata_alloc_failed:
    free(phdr_buffer);
phdr_alloc_failed:
    fclose(file);

    return ret;
}

int rome_attach_rampatch(int fd)
{
    int size, err;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

    /* Frame the HCI CMD to be sent to the Controller */
    frame_hci_cmd_pkt(cmd, EDL_PATCH_ATCH_REQ_CMD, 0,
        -1, EDL_PATCH_CMD_LEN);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND	+ HCI_COMMAND_HDR_SIZE + cmd[PLEN]);

    /* Send HCI Command packet to Controller */
    err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
    if ( err != size) {
        fprintf(stderr, "Failed to attach the patch payload to the Controller!\n");
        goto error;
    }

    /* Read Command Complete Event */
    err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
    if ( err < 0) {
        fprintf(stderr, "%s: Failed to attach the patch segment(s)\n", __FUNCTION__);
        goto error;
    }
error:
    return err;
}

int rome_rampatch_reset(int fd)
{
    int size, err = 0;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    struct timespec tm = { 0, 100*1000*1000 }; /* 100 ms */

    /* Frame the HCI CMD to be sent to the Controller */
    frame_hci_cmd_pkt(cmd, EDL_PATCH_RST_REQ_CMD, 0,
                                        -1, EDL_PATCH_CMD_LEN);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND	+ HCI_COMMAND_HDR_SIZE + EDL_PATCH_CMD_LEN);

    /* Send HCI Command packet to Controller */
    err = write(fd, cmd, size);
    if (err != size) {
        fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, err);
        goto error;
    }

    /*
    * Controller doesn't sends any response for the patch reset
    * command. HOST has to wait for 100ms before proceeding.
    */
    nanosleep(&tm, NULL);

error:
    return err;
}

int rome_get_tlv_file(char *file_path)
{
    FILE * pFile;
    long fileSize;
    int readSize, nvm_length, nvm_index, i;
    unsigned short nvm_tag_len;
    tlv_patch_info *ptlv_header;
    tlv_nvm_hdr *nvm_ptr;
    unsigned char data_buf[PRINT_BUF_SIZE]={0,};
    unsigned char *nvm_byte_ptr;
    unsigned char bdaddr[6];

    fprintf(stderr, "File Open (%s)\n", file_path);
    pFile = fopen ( file_path , "r" );
    if (pFile==NULL) {;
        fprintf(stderr, "%s File Open Fail\n", file_path);
        return -1;
    }

    /* Get File Size */
    fseek (pFile , 0 , SEEK_END);
    fileSize = ftell (pFile);
    rewind (pFile);

    pdata_buffer = (unsigned char*) malloc (sizeof(char)*fileSize);
    if (pdata_buffer == NULL) {
        fprintf(stderr, "Allocated Memory failed\n");
        fclose (pFile);
        return -1;
    }

    /* Copy file into allocated buffer */
    readSize = fread (pdata_buffer,1,fileSize,pFile);

    /* File Close */
    fclose (pFile);

    if (readSize != fileSize) {
        fprintf(stderr, "Read file size(%d) not matched with actual file size (%ld bytes)\n",readSize,fileSize);
        return -1;
    }

    ptlv_header = (tlv_patch_info *) pdata_buffer;

    /* To handle different event between rampatch and NVM */
    gTlv_type = ptlv_header->tlv_type;
    gTlv_dwndCfg = ptlv_header->tlv.patch.dwnd_cfg;

    if(ptlv_header->tlv_type == TLV_TYPE_PATCH){
        fprintf(stderr, "====================================================\n");
        fprintf(stderr, "TLV Type\t\t\t : 0x%x\n", ptlv_header->tlv_type);
        fprintf(stderr, "Length\t\t\t : %d bytes\n", (ptlv_header->tlv_length1) |
                                                    (ptlv_header->tlv_length2 << 8) |
                                                    (ptlv_header->tlv_length3 << 16));
        fprintf(stderr, "Total Length\t\t\t : %d bytes\n", ptlv_header->tlv.patch.tlv_data_len);
        fprintf(stderr, "Patch Data Length\t\t\t : %d bytes\n",ptlv_header->tlv.patch.tlv_patch_data_len);
        fprintf(stderr, "Signing Format Version\t : 0x%x\n", ptlv_header->tlv.patch.sign_ver);
        fprintf(stderr, "Signature Algorithm\t\t : 0x%x\n", ptlv_header->tlv.patch.sign_algorithm);
        fprintf(stderr, "Event Handling\t\t\t : 0x%x", ptlv_header->tlv.patch.dwnd_cfg);
        fprintf(stderr, "Reserved\t\t\t : 0x%x\n", ptlv_header->tlv.patch.reserved1);
        fprintf(stderr, "Product ID\t\t\t : 0x%04x\n", ptlv_header->tlv.patch.prod_id);
        fprintf(stderr, "Rom Build Version\t\t : 0x%04x\n", ptlv_header->tlv.patch.build_ver);
        fprintf(stderr, "Patch Version\t\t : 0x%04x\n", ptlv_header->tlv.patch.patch_ver);
        fprintf(stderr, "Reserved\t\t\t : 0x%x\n", ptlv_header->tlv.patch.reserved2);
        fprintf(stderr, "Patch Entry Address\t\t : 0x%x\n", (ptlv_header->tlv.patch.patch_entry_addr));
        fprintf(stderr, "====================================================\n");

    } else if(ptlv_header->tlv_type == TLV_TYPE_NVM) {
        fprintf(stderr, "====================================================\n");
        fprintf(stderr, "TLV Type\t\t\t : 0x%x\n", ptlv_header->tlv_type);
        fprintf(stderr, "Length\t\t\t : %d bytes\n",  nvm_length = (ptlv_header->tlv_length1) |
                                                    (ptlv_header->tlv_length2 << 8) |
                                                    (ptlv_header->tlv_length3 << 16));

        if(nvm_length <= 0)
            return readSize;

       for(nvm_byte_ptr=(unsigned char *)(nvm_ptr = &(ptlv_header->tlv.nvm)), nvm_index=0;
             nvm_index < nvm_length ; nvm_ptr = (tlv_nvm_hdr *) nvm_byte_ptr)
       {
            fprintf(stderr, "TAG ID\t\t\t : %d\n", nvm_ptr->tag_id);
            fprintf(stderr, "TAG Length\t\t\t : %d\n", nvm_tag_len = nvm_ptr->tag_len);
            fprintf(stderr, "TAG Pointer\t\t\t : %d\n", nvm_ptr->tag_ptr);
            fprintf(stderr, "TAG Extended Flag\t\t : %d\n", nvm_ptr->tag_ex_flag);

            /* Increase nvm_index to NVM data */
            nvm_index+=sizeof(tlv_nvm_hdr);
            nvm_byte_ptr+=sizeof(tlv_nvm_hdr);

            /* Write BD Address */
            if(nvm_ptr->tag_id == TAG_NUM_2 && read_bd_address(&bdaddr) == 0) {
                memcpy(nvm_byte_ptr, bdaddr, 6);
                fprintf(stderr, "Overriding default BD ADDR with user"
                  " programmed BD Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    *nvm_byte_ptr, *(nvm_byte_ptr+1), *(nvm_byte_ptr+2),
                    *(nvm_byte_ptr+3), *(nvm_byte_ptr+4), *(nvm_byte_ptr+5));
            }

            for(i =0;(i<nvm_ptr->tag_len && (i*3 + 2) <PRINT_BUF_SIZE);i++)
                snprintf((char *) data_buf, PRINT_BUF_SIZE, "%s%.02x ", (char *)data_buf, *(nvm_byte_ptr + i));

            fprintf(stderr, "TAG Data\t\t\t : %s\n", data_buf);

            /* Clear buffer */
            memset(data_buf, 0x0, PRINT_BUF_SIZE);

            /* increased by tag_len */
            nvm_index+=nvm_ptr->tag_len;
            nvm_byte_ptr +=nvm_ptr->tag_len;
        }

        fprintf(stderr, "====================================================\n");

    } else {
        fprintf(stderr, "TLV Header type is unknown (%d) \n", ptlv_header->tlv_type);
    }

    return readSize;
}

int rome_tlv_dnld_segment(int fd, int index, int seg_size, unsigned char wait_cc_evt)
{
    int size=0, err = -1;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

    fprintf(stderr, "%s: Downloading TLV Patch segment no.%d, size:%d\n", __FUNCTION__, index, seg_size);

    /* Frame the HCI CMD PKT to be sent to Controller*/
    frame_hci_cmd_pkt(cmd, EDL_PATCH_TLV_REQ_CMD, 0, index, seg_size);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND + HCI_COMMAND_HDR_SIZE + cmd[PLEN]);

    /* Initialize the RSP packet everytime to 0 */
    memset(rsp, 0x0, HCI_MAX_EVENT_SIZE);

    /* Send HCI Command packet to Controller */
    err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
    if ( err != size) {
        fprintf(stderr, "Failed to send the patch payload to the Controller! 0x%x\n", err);
        return err;
    }

    if(wait_cc_evt) {
        err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
        if ( err < 0) {
            fprintf(stderr, "%s: Failed to downlaod patch segment: %d!\n",  __FUNCTION__, index);
            return err;
        }
    }

    fprintf(stderr, "%s: Successfully downloaded patch segment: %d\n", __FUNCTION__, index);
    return err;
}

int rome_tlv_dnld_req(int fd, int tlv_size)
{
    int  total_segment, remain_size, i, err = -1;

    total_segment = tlv_size/MAX_SIZE_PER_TLV_SEGMENT;
    remain_size = (tlv_size < MAX_SIZE_PER_TLV_SEGMENT)?\
        tlv_size: (tlv_size%MAX_SIZE_PER_TLV_SEGMENT);

    fprintf(stderr, "%s: TLV size: %d, Total Seg num: %d, remain size: %d\n",
        __FUNCTION__,tlv_size, total_segment, remain_size);

    if (gTlv_type == TLV_TYPE_PATCH) {
       /* Prior to Rome version 3.2(including inital few rampatch release of Rome 3.2), the event
        * handling mechanism is ROME_SKIP_EVT_NONE. After few release of rampatch for Rome 3.2, the
        * mechamism is changed to ROME_SKIP_EVT_VSE_CC. Rest of the mechanism is not used for now
        */
       switch(gTlv_dwndCfg)
       {
           case ROME_SKIP_EVT_NONE:
              wait_vsc_evt = TRUE;
              wait_cc_evt = TRUE;
              fprintf(stderr, "%s: Event handling type: ROME_SKIP_EVT_NONE", __func__);
              break;
           case ROME_SKIP_EVT_VSE_CC:
              wait_vsc_evt = FALSE;
              wait_cc_evt = FALSE;
              fprintf(stderr, "%s: Event handling type: ROME_SKIP_EVT_VSE_CC", __func__);
              break;
           /* Not handled for now */
           case ROME_SKIP_EVT_VSE:
           case ROME_SKIP_EVT_CC:
           default:
              fprintf(stderr, "%s: Unsupported Event handling: %d", __func__, gTlv_dwndCfg);
              break;
       }
    } else {
        wait_vsc_evt = TRUE;
        wait_cc_evt = TRUE;
    }

    for(i=0;i<total_segment ;i++){
        if ((i+1) == total_segment) {
             if ((rome_ver >= ROME_VER_1_1) && (rome_ver < ROME_VER_3_2) && (gTlv_type == TLV_TYPE_PATCH)) {
               /* If the Rome version is from 1.1 to 3.1
                * 1. No CCE for the last command segment but all other segment
                * 2. All the command segments get VSE including the last one
                */
                wait_cc_evt = !remain_size ? FALSE: TRUE;
             } else if ((rome_ver == ROME_VER_3_2) && (gTlv_type == TLV_TYPE_PATCH)) {
                /* If the Rome version is 3.2
                 * 1. None of the command segments receive CCE
                 * 2. No command segments receive VSE except the last one
                 * 3. If gTlv_dwndCfg is ROME_SKIP_EVT_NONE then the logic is
                 *    same as Rome 2.1, 2.2, 3.0
                 */
                 if (gTlv_dwndCfg == ROME_SKIP_EVT_NONE) {
                    wait_cc_evt = !remain_size ? FALSE: TRUE;
                 } else if (gTlv_dwndCfg == ROME_SKIP_EVT_VSE_CC) {
                    wait_vsc_evt = !remain_size ? TRUE: FALSE;
                 }
             }
        }

        if((err = rome_tlv_dnld_segment(fd, i, MAX_SIZE_PER_TLV_SEGMENT, wait_cc_evt )) < 0)
            goto error;
    }

    if ((rome_ver >= ROME_VER_1_1) && (rome_ver < ROME_VER_3_2) && (gTlv_type == TLV_TYPE_PATCH)) {
       /* If the Rome version is from 1.1 to 3.1
        * 1. No CCE for the last command segment but all other segment
        * 2. All the command segments get VSE including the last one
        */
        wait_cc_evt = remain_size ? FALSE: TRUE;
    } else if ((rome_ver == ROME_VER_3_2) && (gTlv_type == TLV_TYPE_PATCH)) {
        /* If the Rome version is 3.2
         * 1. None of the command segments receive CCE
         * 2. No command segments receive VSE except the last one
         * 3. If gTlv_dwndCfg is ROME_SKIP_EVT_NONE then the logic is
         *    same as Rome 2.1, 2.2, 3.0
         */
        if (gTlv_dwndCfg == ROME_SKIP_EVT_NONE) {
           wait_cc_evt = remain_size ? FALSE: TRUE;
        } else if (gTlv_dwndCfg == ROME_SKIP_EVT_VSE_CC) {
           wait_vsc_evt = remain_size ? TRUE: FALSE;
        }
    }

    if(remain_size) err =rome_tlv_dnld_segment(fd, i, remain_size, wait_cc_evt);

error:
    return err;
}

int rome_download_tlv_file(int fd)
{
    int tlv_size, err = -1;

    /* Rampatch TLV file Downloading */
    pdata_buffer = NULL;

    if((tlv_size = rome_get_tlv_file(rampatch_file_path)) < 0)
        goto error;

    if((err =rome_tlv_dnld_req(fd, tlv_size)) <0 )
        goto error;

    if (pdata_buffer != NULL){
        free (pdata_buffer);
        pdata_buffer = NULL;
    }

    /* NVM TLV file Downloading */
    if((tlv_size = rome_get_tlv_file(nvm_file_path)) < 0)
        goto error;

    if((err =rome_tlv_dnld_req(fd, tlv_size)) <0 )
        goto error;

error:
    if (pdata_buffer != NULL)
        free (pdata_buffer);

    return err;
}

int rome_1_0_nvm_tag_dnld(int fd)
{
    int i, size, err = 0;
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

#if (NVM_VERSION >= ROME_1_0_100019)
    unsigned char cmds[MAX_TAG_CMD][HCI_MAX_CMD_SIZE] =
    {
        /* Tag 2 */ /* BD Address */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     9,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     2,
            /* Tag Len */      6,
            /* Tag Value */   0x77,0x78,0x23,0x01,0x56,0x22
         },
        /* Tag 6 */ /* Bluetooth Support Features */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     11,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     6,
            /* Tag Len */      8,
            /* Tag Value */   0xFF,0xFE,0x8B,0xFE,0xD8,0x3F,0x5B,0x8B
         },
        /* Tag 17 */ /* HCI Transport Layer Setting */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     11,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     17,
            /* Tag Len */      8,
            /* Tag Value */   0x82,0x01,0x0E,0x08,0x04,0x32,0x0A,0x00
         },
        /* Tag 35 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     58,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     35,
            /* Tag Len */      55,
            /* Tag Value */   0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x58, 0x59,
                                      0x0E, 0x0E, 0x16, 0x16, 0x16, 0x1E, 0x26, 0x5F, 0x2F, 0x5F,
                                      0x0E, 0x0E, 0x16, 0x16, 0x16, 0x1E, 0x26, 0x5F, 0x2F, 0x5F,
                                      0x0C, 0x18, 0x14, 0x24, 0x40, 0x4C, 0x70, 0x80, 0x80, 0x80,
                                      0x0C, 0x18, 0x14, 0x24, 0x40, 0x4C, 0x70, 0x80, 0x80, 0x80,
                                      0x1B, 0x14, 0x01, 0x04, 0x48
         },
        /* Tag 36 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     15,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     36,
            /* Tag Len */      12,
            /* Tag Value */   0x0F,0x00,0x03,0x03,0x03,0x03,0x00,0x00,0x03,0x03,0x04,0x00
         },
        /* Tag 39 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     7,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     39,
            /* Tag Len */      4,
            /* Tag Value */   0x12,0x00,0x00,0x00
         },
        /* Tag 41 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     91,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     41,
            /* Tag Len */      88,
            /* Tag Value */   0x15, 0x00, 0x00, 0x00, 0xF6, 0x02, 0x00, 0x00, 0x76, 0x00,
                                      0x1E, 0x00, 0x29, 0x02, 0x1F, 0x00, 0x61, 0x00, 0x1A, 0x00,
                                      0x76, 0x00, 0x1E, 0x00, 0x7D, 0x00, 0x40, 0x00, 0x91, 0x00,
                                      0x06, 0x00, 0x92, 0x00, 0x03, 0x00, 0xA6, 0x01, 0x50, 0x00,
                                      0xAA, 0x01, 0x15, 0x00, 0xAB, 0x01, 0x0A, 0x00, 0xAC, 0x01,
                                      0x00, 0x00, 0xB0, 0x01, 0xC5, 0x00, 0xB3, 0x01, 0x03, 0x00,
                                      0xB4, 0x01, 0x13, 0x00, 0xB5, 0x01, 0x0C, 0x00, 0xC5, 0x01,
                                      0x0D, 0x00, 0xC6, 0x01, 0x10, 0x00, 0xCA, 0x01, 0x2B, 0x00,
                                      0xCB, 0x01, 0x5F, 0x00, 0xCC, 0x01, 0x48, 0x00
         },
        /* Tag 42 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     63,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     42,
            /* Tag Len */      60,
            /* Tag Value */   0xD7, 0xC0, 0x00, 0x00, 0x8F, 0x5C, 0x02, 0x00, 0x80, 0x47,
                                      0x60, 0x0C, 0x70, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x1F, 0x01,
                                      0x42, 0x01, 0x69, 0x01, 0x95, 0x01, 0xC7, 0x01, 0xFE, 0x01,
                                      0x3D, 0x02, 0x83, 0x02, 0xD1, 0x02, 0x29, 0x03, 0x00, 0x0A,
                                      0x10, 0x00, 0x1F, 0x00, 0x3F, 0x00, 0x7F, 0x00, 0xFD, 0x00,
                                      0xF9, 0x01, 0xF1, 0x03, 0xDE, 0x07, 0x00, 0x00, 0x9A, 0x01
         },
        /* Tag 84 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     153,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     84,
            /* Tag Len */      150,
            /* Tag Value */   0x7C, 0x6A, 0x59, 0x47, 0x19, 0x36, 0x35, 0x25, 0x25, 0x28,
                                      0x2C, 0x2B, 0x2B, 0x28, 0x2C, 0x28, 0x29, 0x28, 0x29, 0x28,
                                      0x29, 0x29, 0x2C, 0x29, 0x2C, 0x29, 0x2C, 0x28, 0x29, 0x28,
                                      0x29, 0x28, 0x29, 0x2A, 0x00, 0x00, 0x2C, 0x2A, 0x2C, 0x18,
                                      0x98, 0x98, 0x98, 0x98, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
                                      0x1E, 0x13, 0x1E, 0x1E, 0x1E, 0x1E, 0x13, 0x13, 0x11, 0x13,
                                      0x1E, 0x1E, 0x13, 0x12, 0x12, 0x12, 0x11, 0x12, 0x1F, 0x12,
                                      0x12, 0x12, 0x10, 0x0C, 0x18, 0x0D, 0x01, 0x01, 0x01, 0x01,
                                      0x01, 0x01, 0x01, 0x0C, 0x01, 0x01, 0x01, 0x01, 0x0D, 0x0D,
                                      0x0E, 0x0D, 0x01, 0x01, 0x0D, 0x0D, 0x0D, 0x0D, 0x0F, 0x0D,
                                      0x10, 0x0D, 0x0D, 0x0D, 0x0D, 0x10, 0x05, 0x10, 0x03, 0x00,
                                      0x7E, 0x7B, 0x7B, 0x72, 0x71, 0x50, 0x50, 0x50, 0x00, 0x40,
                                      0x60, 0x60, 0x30, 0x08, 0x02, 0x0F, 0x00, 0x01, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x08, 0x16, 0x16, 0x08, 0x08, 0x00,
                                      0x00, 0x00, 0x1E, 0x34, 0x2B, 0x1B, 0x23, 0x2B, 0x15, 0x0D
         },
        /* Tag 85 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     119,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     85,
            /* Tag Len */      116,
            /* Tag Value */   0x03, 0x00, 0x38, 0x00, 0x45, 0x77, 0x00, 0xE8, 0x00, 0x59,
                                      0x01, 0xCA, 0x01, 0x3B, 0x02, 0xAC, 0x02, 0x1D, 0x03, 0x8E,
                                      0x03, 0x00, 0x89, 0x01, 0x0E, 0x02, 0x5C, 0x02, 0xD7, 0x02,
                                      0xF8, 0x08, 0x01, 0x00, 0x1F, 0x00, 0x0A, 0x02, 0x55, 0x02,
                                      0x00, 0x35, 0x00, 0x00, 0x00, 0x00, 0x2A, 0xD7, 0x00, 0x00,
                                      0x00, 0x1E, 0xDE, 0x00, 0x00, 0x00, 0x14, 0x0F, 0x0A, 0x0F,
                                      0x0A, 0x0C, 0x0C, 0x0C, 0x0C, 0x04, 0x04, 0x04, 0x0C, 0x0C,
                                      0x0C, 0x0C, 0x06, 0x06, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02,
                                      0x01, 0x00, 0x02, 0x02, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00,
                                      0x06, 0x0F, 0x14, 0x05, 0x47, 0xCF, 0x77, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0xAC, 0x7C, 0xFF, 0x40, 0x00, 0x00, 0x00,
                                      0x12, 0x04, 0x04, 0x01, 0x04, 0x03
         },
        {TAG_END}
    };
#elif (NVM_VERSION == ROME_1_0_6002)
    unsigned char cmds[MAX_TAG_CMD][HCI_MAX_CMD_SIZE] =
    {
        /* Tag 2 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     9,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     2,
            /* Tag Len */      6,
            /* Tag Value */   0x77,0x78,0x23,0x01,0x56,0x22 /* BD Address */
         },
        /* Tag 6 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     11,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     6,
            /* Tag Len */      8,
            /* Tag Value */   0xFF,0xFE,0x8B,0xFE,0xD8,0x3F,0x5B,0x8B
         },
        /* Tag 17 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     11,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     17,
            /* Tag Len */      8,
            /* Tag Value */   0x82,0x01,0x0E,0x08,0x04,0x32,0x0A,0x00
         },
        /* Tag 36 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     15,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     36,
            /* Tag Len */      12,
            /* Tag Value */   0x0F,0x00,0x03,0x03,0x03,0x03,0x00,0x00,0x03,0x03,0x04,0x00
         },

        /* Tag 39 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     7,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     39,
            /* Tag Len */      4,
            /* Tag Value */   0x12,0x00,0x00,0x00
         },

        /* Tag 41 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     199,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     41,
            /* Tag Len */      196,
            /* Tag Value */   0x30,0x00,0x00,0x00,0xD5,0x00,0x0E,0x00,0xD6,0x00,0x0E,0x00,
                                      0xD7,0x00,0x16,0x00,0xD8,0x00,0x16,0x00,0xD9,0x00,0x16,0x00,
                                      0xDA,0x00,0x1E,0x00,0xDB,0x00,0x26,0x00,0xDC,0x00,0x5F,0x00,
                                      0xDD,0x00,0x2F,0x00,0xDE,0x00,0x5F,0x00,0xE0,0x00,0x0E,0x00,
                                      0xE1,0x00,0x0E,0x00,0xE2,0x00,0x16,0x00,0xE3,0x00,0x16,0x00,
                                      0xE4,0x00,0x16,0x00,0xE5,0x00,0x1E,0x00,0xE6,0x00,0x26,0x00,
                                      0xE7,0x00,0x5F,0x00,0xE8,0x00,0x2F,0x00,0xE9,0x00,0x5F,0x00,
                                      0xEC,0x00,0x0C,0x00,0xED,0x00,0x08,0x00,0xEE,0x00,0x14,0x00,
                                      0xEF,0x00,0x24,0x00,0xF0,0x00,0x40,0x00,0xF1,0x00,0x4C,0x00,
                                      0xF2,0x00,0x70,0x00,0xF3,0x00,0x80,0x00,0xF4,0x00,0x80,0x00,
                                      0xF5,0x00,0x80,0x00,0xF8,0x00,0x0C,0x00,0xF9,0x00,0x18,0x00,
                                      0xFA,0x00,0x14,0x00,0xFB,0x00,0x24,0x00,0xFC,0x00,0x40,0x00,
                                      0xFD,0x00,0x4C,0x00,0xFE,0x00,0x70,0x00,0xFF,0x00,0x80,0x00,
                                      0x00,0x01,0x80,0x00,0x01,0x01,0x80,0x00,0x04,0x01,0x1B,0x00,
                                      0x05,0x01,0x14,0x00,0x06,0x01,0x01,0x00,0x07,0x01,0x04,0x00,
                                      0x08,0x01,0x00,0x00,0x09,0x01,0x00,0x00,0x0A,0x01,0x03,0x00,
                                      0x0B,0x01,0x03,0x00
         },

        /* Tag 44 */
        {  /* Packet Type */HCI_COMMAND_PKT,
            /* Opcode */       0x0b,0xfc,
            /* Total Len */     44,
            /* NVM CMD */    NVM_ACCESS_SET,
            /* Tag Num */     44,
            /* Tag Len */      41,
            /* Tag Value */   0x6F,0x0A,0x00,0x00,0x00,0x00,0x00,0x50,0xFF,0x10,0x02,0x02,
                                      0x01,0x00,0x14,0x01,0x06,0x28,0xA0,0x62,0x03,0x64,0x01,0x01,
                                      0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0xFF,0x10,0x02,0x01,
                                      0x00,0x14,0x01,0x02,0x03
         },
        {TAG_END}
    };
#endif

    fprintf(stderr, "%s: Start sending NVM Tags (ver: 0x%x)\n", __FUNCTION__, (unsigned int) NVM_VERSION);

    for (i=0; (i < MAX_TAG_CMD) && (cmds[i][0] != TAG_END); i++)
    {
        /* Write BD Address */
        if(cmds[i][TAG_NUM_OFFSET] == TAG_NUM_2){
            memcpy(&cmds[i][TAG_BDADDR_OFFSET], vnd_local_bd_addr, 6);
            fprintf(stderr, "BD Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
                cmds[i][TAG_BDADDR_OFFSET ], cmds[i][TAG_BDADDR_OFFSET + 1],
                cmds[i][TAG_BDADDR_OFFSET + 2], cmds[i][TAG_BDADDR_OFFSET + 3],
                cmds[i][TAG_BDADDR_OFFSET + 4], cmds[i][TAG_BDADDR_OFFSET + 5]);
        }
        size = cmds[i][3] + HCI_COMMAND_HDR_SIZE + 1;
        /* Send HCI Command packet to Controller */
        err = hci_send_vs_cmd(fd, (unsigned char *)&cmds[i][0], rsp, size);
        if ( err != size) {
            fprintf(stderr, "Failed to attach the patch payload to the Controller!\n");
            goto error;
        }

        /* Read Command Complete Event - This is extra routine for ROME 1.0. From ROM 2.0, it should be removed. */
        err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
        if ( err < 0) {
            fprintf(stderr, "%s: Failed to get patch version(s)\n", __FUNCTION__);
            goto error;
        }
    }

error:
    return err;
}



int rome_patch_ver_req(int fd)
{
    int size, err = 0;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];

    /* Frame the HCI CMD to be sent to the Controller */
    frame_hci_cmd_pkt(cmd, EDL_PATCH_VER_REQ_CMD, 0,
    -1, EDL_PATCH_CMD_LEN);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND + HCI_COMMAND_HDR_SIZE + EDL_PATCH_CMD_LEN);

    /* Send HCI Command packet to Controller */
    err = hci_send_vs_cmd(fd, (unsigned char *)cmd, rsp, size);
    if ( err != size) {
        fprintf(stderr, "Failed to attach the patch payload to the Controller!\n");
        goto error;
    }

    /* Read Command Complete Event - This is extra routine for ROME 1.0. From ROM 2.0, it should be removed. */
    err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
    if ( err < 0) {
        fprintf(stderr, "%s: Failed to get patch version(s)\n", __FUNCTION__);
        goto error;
    }
error:
    return err;

}


int rome_set_baudrate_req(int fd)
{
#ifdef NOTREQUIRED 
   int size, err = 0;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];
    hci_command_hdr *cmd_hdr;
    int flags;

    memset(cmd, 0x0, HCI_MAX_CMD_SIZE);

    cmd_hdr = (void *) (cmd + 1);
    cmd[0]  = HCI_COMMAND_PKT;
    cmd_hdr->opcode = cmd_opcode_pack(HCI_VENDOR_CMD_OGF, EDL_SET_BAUDRATE_CMD_OCF);
    cmd_hdr->plen     = VSC_SET_BAUDRATE_REQ_LEN;
    cmd[4]  = BAUDRATE_3000000;

    /* Change Local UART baudrate to high speed UART */
    userial_vendor_set_baud(USERIAL_BAUD_115200);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND + HCI_COMMAND_HDR_SIZE + VSC_SET_BAUDRATE_REQ_LEN);
    /* Flow off during baudrate change */
    if ((err = userial_vendor_ioctl(fd, USERIAL_OP_FLOW_OFF , &flags)) < 0)
    {
      fprintf(stderr, "%s: HW Flow-off error: 0x%x\n", __FUNCTION__, err);
      goto error;
    }
    /* Send the HCI command packet to UART for transmission */
    fprintf(stderr, "%s: HCI CMD: 0x%x 0x%x 0x%x 0x%x 0x%x\n", __FUNCTION__, cmd[0], cmd[1], cmd[2], cmd[3],cmd[4]) ;
    err = write(fd, cmd, size);
    if (err != size) {
        fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, err);
        goto error;
    }

    /* Flow on after changing local uart baudrate */
    if ((err = userial_vendor_ioctl(fd, USERIAL_OP_FLOW_ON , &flags)) < 0)
    {
        fprintf(stderr, "%s: HW Flow-on error: 0x%x \n", __FUNCTION__, err);
        return err;
    }
    /* Check for response from the Controller */
    if ((err =read_vs_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE)) < 0) {
            fprintf(stderr, "%s: Failed to get HCI-VS Event from SOC\n", __FUNCTION__);
            goto error;
    }

    fprintf(stderr, "%s: Received HCI-Vendor Specific Event from SOC\n", __FUNCTION__);

    /* Wait for command complete event */
    err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
    if ( err < 0) {
        fprintf(stderr, "%s: Failed to set patch info on Controller\n", __FUNCTION__);
        goto error;
    }
        fprintf(stderr, "%s\n", __FUNCTION__);
error:
    return err;
#else 
    return 0;
#endif
}


int rome_hci_reset_req(int fd)
{
    int size, err = 0;
    unsigned char cmd[HCI_MAX_CMD_SIZE];
    unsigned char rsp[HCI_MAX_EVENT_SIZE];
    hci_command_hdr *cmd_hdr;
    int flags;

    fprintf(stderr, "%s: HCI RESET \n", __FUNCTION__);

    memset(cmd, 0x0, HCI_MAX_CMD_SIZE);

    cmd_hdr = (void *) (cmd + 1);
    cmd[0]  = HCI_COMMAND_PKT;
    cmd_hdr->opcode = HCI_RESET;
    cmd_hdr->plen   = 0;

    /* Change Local UART baudrate to high speed UART */
     //userial_vendor_set_baud(USERIAL_BAUD_115200);

    /* Total length of the packet to be sent to the Controller */
    size = (HCI_CMD_IND + HCI_COMMAND_HDR_SIZE);

#ifdef NOTREQUIRED 
    /* Flow off during baudrate change */
    if ((err = userial_vendor_ioctl(fd, USERIAL_OP_FLOW_OFF , &flags)) < 0)
    {
      fprintf(stderr, "%s: HW Flow-off error: 0x%x\n", __FUNCTION__, err);
      goto error;
    }
#endif
    /* Send the HCI command packet to UART for transmission */
    fprintf(stderr, "%s: HCI CMD: 0x%x 0x%x 0x%x 0x%x\n", __FUNCTION__, cmd[0], cmd[1], cmd[2], cmd[3]);
   err = write(fd, cmd, size);
   sleep(1);
   int err1 ; 
   err1 = write(fd, cmd, size);
   sleep(1);
   int err2 ; 
   err2 = write(fd, cmd, size);
   sleep(1);
   if (err2 != size) {
        fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, err2);
       goto error;
    }


#ifdef NOTREQUIRED 
    /* Flow on after changing local uart baudrate */
    if ((err = userial_vendor_ioctl(fd, USERIAL_OP_FLOW_ON , &flags)) < 0)
    {
        fprintf(stderr, "%s: HW Flow-on error: 0x%x \n", __FUNCTION__, err);
        return err;
    }
#endif
    /* Wait for command complete event */
    printf("waiting for read_HCI_EVENT %s\n",__FUNCTION__);
    err = read_hci_event(fd, rsp, HCI_MAX_EVENT_SIZE);
    if ( err < 0) {
       fprintf(stderr, "%s: Failed to set patch info on Controller\n", __FUNCTION__);
       goto error;
   }

   return 0;
error:
    return err;

}

int read_bd_address(unsigned char *bdaddr)
{
  int fd = -1;
  int readPtr = 0;
  unsigned char data[BD_ADDR_LEN];

  /* Open the persist file for reading device address*/
  fd = open("/etc/bluetooth/.bt_nv.bin", O_RDONLY);
  if(fd < 0)
  {
    fprintf(stderr, "%s: Open failed: Programming default BD ADDR\n", __func__);
    return -1;
  }

  /* Read the NVM Header : fp will be advanced by readPtr number of bytes */
  readPtr = read(fd, data, PERSIST_HEADER_LEN);
  if (readPtr > 0)
    fprintf(stderr, "%s: Persist header data: %02x \t %02x \t %02x\n", __func__,
      data[NVITEM], data[RDWR_PROT], data[NVITEM_SIZE]);
  else {
    fprintf(stderr, "%s: Read from persist memory failed : Programming default"
      " BD ADDR\n");
    close(fd);
    return -1;
  }

  /* Check for BD ADDR length before programming */
  if(data[NVITEM_SIZE] != BD_ADDR_LEN) {
    fprintf(stderr, "Invalid BD ADDR: Programming default BD ADDR!\n");
    close(fd);
    return -1;
  }

  /* Read the BD ADDR info */
  readPtr = read(fd, data, BD_ADDR_LEN);
  if (readPtr > 0)
    fprintf(stderr, "BD-ADDR: ==> %02x:%02x:%02x:%02x:%02x:%02x\n", data[0],
      data[1], data[2], data[3], data[4], data[5]);
  else {
    fprintf(stderr, "%s: Read from persist memory failed : Programming default"
      " BD ADDR\n");
    close(fd);
    return -1;
  }
  memcpy(bdaddr, data, BD_ADDR_LEN);
  close(fd);
  return 0;
}

int qca_soc_init(int fd, char *bdaddr)
{
    int err = -1;
    int size;

    fprintf(stderr, " %s \n", __FUNCTION__);
    vnd_userial.fd = fd;

    /* DK test code */
    err = rome_hci_reset_req(fd);
    printf ("**** err = %d\n", err);
    return err;



    /* Get Rome version information */
    if((err = rome_patch_ver_req(fd)) <0){
        fprintf(stderr, "%s: Fail to get Rome Version (0x%x)\n", __FUNCTION__, err);
        goto error;
    }

    fprintf(stderr, "%s: Rome Version (0x%08x)\n", __FUNCTION__, rome_ver);
    
    char rome_ver = ROME_VER_1_0;
    switch (rome_ver){
        case ROME_VER_1_0:
            {

                /* Send Reset */
                size = (HCI_CMD_IND + HCI_COMMAND_HDR_SIZE + EDL_PATCH_CMD_LEN);
                err = rome_rampatch_reset(fd);
                if ( err < 0 ) {
                    fprintf(stderr, "Failed to RESET after RAMPATCH upgrade!\n");
                    goto error;
                }

                fprintf(stderr, "HCI Reset is done\n");
            }
            break;
        case ROME_VER_1_1:
            rampatch_file_path = ROME_RAMPATCH_TLV_PATH;
            nvm_file_path = ROME_NVM_TLV_PATH;
            goto download;
        case ROME_VER_1_3:
            rampatch_file_path = ROME_RAMPATCH_TLV_1_0_3_PATH;
            nvm_file_path = ROME_NVM_TLV_1_0_3_PATH;
            goto download;
        case ROME_VER_2_1:
            rampatch_file_path = ROME_RAMPATCH_TLV_2_0_1_PATH;
            nvm_file_path = ROME_NVM_TLV_2_0_1_PATH;
            goto download;
        case ROME_VER_3_0:
        case TUFELLO_VER_1_0:
            rampatch_file_path = ROME_RAMPATCH_TLV_3_0_0_PATH;
            nvm_file_path = ROME_NVM_TLV_3_0_0_PATH;
            goto download;
        case ROME_VER_3_2:
            rampatch_file_path = ROME_RAMPATCH_TLV_3_0_2_PATH;
            nvm_file_path = ROME_NVM_TLV_3_0_2_PATH;

download:
            /* Change baud rate 115.2 kbps to 3Mbps*/
            err = rome_set_baudrate_req(fd);
            if (err < 0) {
                fprintf(stderr, "%s: Baud rate change failed!\n", __FUNCTION__);
                goto error;
            }
            fprintf(stderr, "%s: Baud rate changed successfully \n", __FUNCTION__);

            /* Donwload TLV files (rampatch, NVM) */
            err = rome_download_tlv_file(fd);
            if (err < 0) {
                fprintf(stderr, "%s: Download TLV file failed!\n", __FUNCTION__);
                goto error;
            }
            fprintf(stderr, "%s: Download TLV file successfully \n", __FUNCTION__);

            /* Perform HCI reset here*/
            err = rome_hci_reset_req(fd);
            if ( err <0 ) {
                fprintf(stderr, "HCI Reset Failed !!!\n");
                goto error;
            }
	    fprintf(stderr, "HCI Reset is done\n");

            break;
        case ROME_VER_UNKNOWN:
        default:
            fprintf(stderr, "%s: Detected unknown ROME version\n", __FUNCTION__);
            err = -1;
            break;
    }

error:
    return err;
}
