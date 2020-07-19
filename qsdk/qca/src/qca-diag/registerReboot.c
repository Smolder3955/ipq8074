/*
 * Copyright (c) 2012, 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include "msg.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "string.h"
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#define DEFAULT_SMD_PORT QMI_PORT_RMNET_0
#define LOGI(...) fprintf(stderr, "I:" __VA_ARGS__)
#define DIAG_CONTROL_F 41
#define EDL_RESET_CMD_CODE 1

#define PACKED __attribute__ ((__packed__))

#ifdef USE_GLIB
#include <glib.h>
#define strlcat g_strlcat
#define strlcpy g_strlcpy
#endif

/* string to send to reboot_daemon to reboot */
#define REBOOT_STR "REBOOT"
/* string to send to reboot_daemon to edl-reboot */
#define EDL_REBOOT_STR "EDL-REBOOT"
/* size to make write buffer */
#define MAX_BUF 64
/* name of pipe to write to */
#define FIFO_NAME "/dev/rebooterdev"

/* ID's for diag */
#define DIAG_UID 53
#define DIAG_GID 53
#define REBOOTERS_GID 1301
#define SDCARD_GID 1015

/* QDSS defines */
#define QDSSDIAG_PROCESSOR_APPS   0x0100
#define QDSS_DIAG_PROC_ID QDSSDIAG_PROCESSOR_APPS

#define QDSS_QUERY_STATUS              0x00
#define QDSS_TRACE_SINK                0x01
#define QDSS_FILTER_ETM                0x02
#define QDSS_FILTER_STM                0x03
#define QDSS_FILTER_HWEVENT_ENABLE     0x04

#define QDSS_FILTER_HWEVENT_CONFIGURE  0x31
#define QDSS_QTIMER_TS_SYNC            0x60

#define   TMC_TRACESINK_ETB   0
#define   TMC_TRACESINK_RAM   1
#define   TMC_TRACESINK_TPIU  2
#define   TMC_TRACESINK_USB   3
#define   TMC_TRACESINK_USB_BUFFERED   4
#define   TMC_TRACESINK_SD    6

#define QDSS_RSP_SUCCESS  0
#define QDSS_RSP_FAIL  1

#define QDSS_CLK_FREQ_HZ 19200000
#define QTIMER_CLK_FREQ_HZ 19200000

#define QDSS_ETB_SINK_FILE "/sys/bus/coresight/devices/coresight-tmc-etf/curr_sink"
#define QDSS_ETB_SINK_FILE_2 "/sys/bus/coresight/devices/coresight-tmc-etf/enable_sink"
#define QDSS_ETR_SINK_FILE "/sys/bus/coresight/devices/coresight-tmc-etr/curr_sink"
#define QDSS_ETR_SINK_FILE_2 "/sys/bus/coresight/devices/coresight-tmc-etr/enable_sink"
#define QDSS_ETR_OUTMODE_FILE "/sys/bus/coresight/devices/coresight-tmc-etr/out_mode"
#define QDSS_TPIU_SINK_FILE "/sys/bus/coresight/devices/coresight-tpiu/curr_sink"
#define QDSS_TPIU_OUTMODE_FILE "/sys/bus/coresight/devices/coresight-tpiu/out_mode"
#define QDSS_STM_FILE "/sys/bus/coresight/devices/coresight-stm/enable"
#define QDSS_STM_FILE_2 "/sys/bus/coresight/devices/coresight-stm/enable_source"
#define QDSS_HWEVENT_FILE "/sys/bus/coresight/devices/coresight-hwevent/enable"
#define QDSS_STM_HWEVENT_FILE "/sys/bus/coresight/devices/coresight-stm/hwevent_enable"
#define QDSS_HWEVENT_SET_REG_FILE "/sys/bus/coresight/devices/coresight-hwevent/setreg"
#define QDSS_SWAO_CSR_TIMESTAMP "/sys/bus/coresight/devices/coresight-swao-csr/timestamp"

/* QDSS */
typedef PACKED struct
{
  uint8 cmdCode;        // Diag Message ID
  uint8 subsysId;       // Subsystem ID (DIAG_SUBSYS_QDSS)
  uint16 subsysCmdCode; // Subsystem command code
}__attribute__((packed))qdss_diag_pkt_hdr;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_diag_pkt_req;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr; // Header
  uint8 result;          //See QDSS_CMDRESP_... definitions
}__attribute__((packed))qdss_diag_pkt_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint8 trace_sink;
}__attribute__((packed))qdss_trace_sink_req;

typedef qdss_diag_pkt_rsp qdss_trace_sink_rsp; //generic response

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_etm_req;

typedef qdss_diag_pkt_rsp qdss_filter_etm_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_stm_req;

typedef qdss_diag_pkt_rsp qdss_filter_stm_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_hwevents_req;

typedef qdss_diag_pkt_rsp qdss_filter_hwevents_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint32 register_addr;
  uint32 register_value;
}__attribute__((packed))qdss_filter_hwevents_configure_req;

typedef qdss_diag_pkt_rsp qdss_filter_hwevents_configure_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_query_status_req;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint8 trace_sink;
  uint8 stm_enabled;
  uint8 hw_events_enabled;
}__attribute__((packed))qdss_query_status_rsp;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_qtimer_ts_sync_req;

typedef PACKED struct
{
  qdss_diag_pkt_hdr hdr;
  uint32 status;
  uint64 qdss_ticks;
  uint64 qtimer_ticks;
  uint64 qdss_freq;
  uint64 qtimer_freq;
}__attribute__((packed))qdss_qtimer_ts_sync_rsp;

//enable the line below if you want to turn on debug messages
//#define DIAG_REBOOT_DEBUG

PACK(void *) qdss_diag_pkt_handler(PACK(void *) pReq, uint16 pkt_len);

/*qdss dispatch table*/
static const diagpkt_user_table_entry_type qdss_diag_pkt_tbl[] =
{
    {QDSS_DIAG_PROC_ID | QDSS_QUERY_STATUS,
    QDSS_DIAG_PROC_ID | QDSS_QTIMER_TS_SYNC,
    qdss_diag_pkt_handler}
};

static char qdss_sink = 0, qdss_hwevent = 0, qdss_stm = 0;

void drop_privileges() {

    /* Start as root */
    /* Update primary group */
    setgid(DIAG_GID);

    /* Update secondary groups */
    gid_t *groups;
    int numGroups = 2;
    groups = (gid_t *) malloc(numGroups * sizeof(gid_t));
    if (groups){
        groups[0] = REBOOTERS_GID;
        groups[1] = SDCARD_GID;
    }
    setgroups(numGroups, groups);

    /* Update UID -- from root to diag */
    setuid(DIAG_UID);

    free(groups);
}

void setup_qdss_sysfs_nodes() {
    chown(QDSS_ETB_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_ETB_SINK_FILE_2, DIAG_UID, -1);
    chown(QDSS_ETR_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_ETR_SINK_FILE_2, DIAG_UID, -1);
    chown(QDSS_ETR_OUTMODE_FILE, DIAG_UID, -1);
    chown(QDSS_TPIU_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_TPIU_OUTMODE_FILE, DIAG_UID, -1);
    chown(QDSS_STM_FILE, DIAG_UID, -1);
    chown(QDSS_STM_FILE_2, DIAG_UID, -1);
    chown(QDSS_HWEVENT_FILE, DIAG_UID, -1);
    chown(QDSS_STM_HWEVENT_FILE, DIAG_UID, -1);
    chown(QDSS_HWEVENT_SET_REG_FILE, DIAG_UID, -1);
}

int main (void) {

    boolean bInit_Success = FALSE;
    bInit_Success = Diag_LSM_Init(NULL);

    if (!bInit_Success) {
        printf("Diag_LSM_Init() failed.\n");
        return -1;
    }

    DIAGPKT_DISPATCH_TABLE_REGISTER (DIAG_SUBSYS_QDSS, qdss_diag_pkt_tbl);

    do {
        sleep (1);
    } while (1);

    Diag_LSM_DeInit();

    return 0;

}

static int qdss_file_write_str(const char *qdss_file_path, const char *str)
{
    int qdss_fd, ret;

    if (!qdss_file_path || !str) {
        return QDSS_RSP_FAIL;
    }

    qdss_fd = open(qdss_file_path, O_WRONLY);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", qdss_file_path, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, str, strlen(str));
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", qdss_file_path, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    return QDSS_RSP_SUCCESS;
}

static int qdss_file_write_byte(const char *qdss_file_path, unsigned char val)
{
    int qdss_fd, ret;

    if (!qdss_file_path) {
        return QDSS_RSP_FAIL;
    }

    qdss_fd = open(qdss_file_path, O_WRONLY);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", qdss_file_path, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, &val, 1);
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", qdss_file_path, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    return QDSS_RSP_SUCCESS;
}

/* Sets the Trace Sink */
static int qdss_trace_sink_handler(qdss_trace_sink_req *pReq, int req_len, qdss_trace_sink_rsp *pRsp, int rsp_len)
{
    int ret = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;
    if (pReq->trace_sink == TMC_TRACESINK_ETB) {
        /* For enabling writing ASCII value of 1 i.e. 0x31 */
        ret = qdss_file_write_byte(QDSS_ETB_SINK_FILE, 0x31);
        if (ret) {
            ret = qdss_file_write_byte(QDSS_ETB_SINK_FILE_2, 0x31);
            if (ret)
                return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_RAM) {
        ret = qdss_file_write_str(QDSS_ETR_OUTMODE_FILE, "mem");
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_ETR_SINK_FILE, 0x31);
        if (ret) {
            ret = qdss_file_write_byte(QDSS_ETR_SINK_FILE_2, 0x31);
            if (ret)
                return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_USB) {
        ret = qdss_file_write_str(QDSS_ETR_OUTMODE_FILE, "usb");
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_ETR_SINK_FILE, 0x31);
        if (ret) {
            ret = qdss_file_write_byte(QDSS_ETR_SINK_FILE_2, 0x31);
            if (ret)
                return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_TPIU) {
        ret = qdss_file_write_byte(QDSS_TPIU_SINK_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_str(QDSS_TPIU_OUTMODE_FILE, "mictor");
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_SD) {
        ret = qdss_file_write_byte(QDSS_TPIU_SINK_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_str(QDSS_TPIU_OUTMODE_FILE, "sdc");
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    } else {
        qdss_sink = 0;
        return QDSS_RSP_FAIL;
    }

    qdss_sink = pReq->trace_sink;
    pRsp->result = QDSS_RSP_SUCCESS;

    return QDSS_RSP_SUCCESS;
}

/* Enable/Disable STM */
static int qdss_filter_stm_handler(qdss_filter_stm_req *pReq, int req_len, qdss_filter_stm_rsp *pRsp, int rsp_len)
{
    char ret = 0, stm_state = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    if (pReq->state) {
        stm_state = 1;
    } else {
        stm_state = 0;
    }

    ret = qdss_file_write_byte(QDSS_STM_FILE, stm_state + 0x30);
    if (ret) {
        ret = qdss_file_write_byte(QDSS_STM_FILE_2, stm_state + 0x30);
        if (ret)
            return QDSS_RSP_FAIL;
    }

    qdss_stm = stm_state;
    pRsp->result = QDSS_RSP_SUCCESS;

    return QDSS_RSP_SUCCESS;
}

/* Enable/Disable HW Events */
static int qdss_filter_hwevents_handler(qdss_filter_hwevents_req *pReq, int req_len, qdss_filter_hwevents_rsp *pRsp, int rsp_len)
{
    int ret = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    if (pReq->state) {

        qdss_hwevent = 1;
        /* For disabling writing ASCII value of 0 i.e. 0x30 */
        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

    } else {

        qdss_hwevent = 0;

        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    }

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;
}

/* Programming registers to generate HW events */
static int qdss_filter_hwevents_configure_handler(qdss_filter_hwevents_configure_req *pReq, int req_len, qdss_filter_hwevents_configure_rsp *pRsp, int rsp_len)
{
    char reg_buf[100];
    int ret = 0, qdss_fd;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    snprintf(reg_buf, sizeof(reg_buf), "%x %x", pReq->register_addr, pReq->register_value);

    qdss_fd = open(QDSS_HWEVENT_SET_REG_FILE, O_WRONLY);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", QDSS_HWEVENT_SET_REG_FILE, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, reg_buf, strlen(reg_buf));
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", QDSS_HWEVENT_SET_REG_FILE, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;
}

/* Get the status of sink, stm and HW events */
static int qdss_query_status_handler(qdss_query_status_req *pReq, int req_len, qdss_query_status_rsp *pRsp, int rsp_len)
{

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->trace_sink = qdss_sink;
    pRsp->stm_enabled = qdss_stm;
    pRsp->hw_events_enabled = qdss_hwevent;

    return QDSS_RSP_SUCCESS;
}

/* Handling of qdss qtimer ts sync */
static int qdss_qtimer_ts_sync_handler(qdss_qtimer_ts_sync_req *pReq, int req_len, qdss_qtimer_ts_sync_rsp *pRsp, int rsp_len)
{
    int ret = 0, qdss_ts_fd = 0;
    uint64_t qdss_ticks = 0, qtimer_ticks = 0;
    char qdss_ts_val[17];

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    memset(qdss_ts_val, '\0', sizeof(qdss_ts_val));

    qdss_ts_fd = open(QDSS_SWAO_CSR_TIMESTAMP, O_RDONLY);
    if (qdss_ts_fd < 0) {
        LOGI("qdss open file: %s error: %s", QDSS_SWAO_CSR_TIMESTAMP, strerror(errno));
        goto fail;
    }

    ret = read(qdss_ts_fd, qdss_ts_val, sizeof(qdss_ts_val)-1);
    if (ret < 0) {
        LOGI("qdss read file: %s error: %s", QDSS_SWAO_CSR_TIMESTAMP, strerror(errno));
        close(qdss_ts_fd);
        goto fail;
    }

    qdss_ticks = atoll(qdss_ts_val);

    close(qdss_ts_fd);

#if defined __aarch64__ && __aarch64__ == 1
    asm volatile("mrs %0, cntvct_el0" : "=r" (qtimer_ticks));
#else
    asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (qtimer_ticks));
#endif

    pRsp->status = 0;
    pRsp->qdss_ticks = qdss_ticks;
    pRsp->qtimer_ticks = qtimer_ticks;
    pRsp->qdss_freq = QDSS_CLK_FREQ_HZ;
    pRsp->qtimer_freq = QTIMER_CLK_FREQ_HZ;
    return QDSS_RSP_SUCCESS;
fail:
    pRsp->status = 1;
    pRsp->qdss_ticks = 0;
    pRsp->qtimer_ticks = 0;
    pRsp->qdss_freq = 0;
    pRsp->qtimer_freq = 0;
    return QDSS_RSP_FAIL;
}

/* QDSS commands handler */
PACK(void *) qdss_diag_pkt_handler(PACK(void *) pReq, uint16 pkt_len)
{
/*
 * 1) Checks the request command size. If it fails send error response.
 * 2) If request command size is valid then allocates response packet
 *    based on request.
 * 3) Invokes the respective command handler
 */

#define QDSS_HANDLE_DIAG_CMD(cmd)                              \
   if (pkt_len < sizeof(cmd##_req)) {                          \
      pRsp = diagpkt_err_rsp(DIAG_BAD_LEN_F, pReq, pkt_len);   \
   }                                                           \
   else {                                                      \
      pRsp =  diagpkt_subsys_alloc(DIAG_SUBSYS_QDSS,           \
                                   pHdr->subsysCmdCode,        \
                                   sizeof(cmd##_rsp));         \
      if (NULL != pRsp) {                                      \
         cmd##_handler((cmd##_req *)pReq,                      \
                       pkt_len,                                \
                       (cmd##_rsp *)pRsp,                      \
                       sizeof(cmd##_rsp));                     \
      }                                                        \
   }

    qdss_diag_pkt_hdr *pHdr;
    PACK(void *)pRsp = NULL;

    if (NULL != pReq) {
        pHdr = (qdss_diag_pkt_hdr *)pReq;

        switch (pHdr->subsysCmdCode & 0x0FF) {
        case QDSS_QUERY_STATUS:
            QDSS_HANDLE_DIAG_CMD(qdss_query_status);
            break;
        case QDSS_TRACE_SINK:
            QDSS_HANDLE_DIAG_CMD(qdss_trace_sink);
            break;
        case QDSS_FILTER_STM:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_stm);
            break;
        case QDSS_FILTER_HWEVENT_ENABLE:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_hwevents);
            break;
        case QDSS_FILTER_HWEVENT_CONFIGURE:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_hwevents_configure);
            break;
        case QDSS_QTIMER_TS_SYNC:
            QDSS_HANDLE_DIAG_CMD(qdss_qtimer_ts_sync);
            break;
        default:
            pRsp = diagpkt_err_rsp(DIAG_BAD_CMD_F, pReq, pkt_len);
            break;
        }

        if (NULL != pRsp) {
            diagpkt_commit(pRsp);
            pRsp = NULL;
        }
    }
    return (pRsp);
}
