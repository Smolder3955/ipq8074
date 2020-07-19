/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  Operating System API Interface Header File

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  the Operating System API interface for QUIPC software.

Copyright (c) 2013 Qualcomm Atheros, Inc.
   All Rights Reserved.
   Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/

#ifndef OSAL_OS_API_H
#define OSAL_OS_API_H

#include "commontypes.h"
#ifdef __cplusplus
extern "C"
{
#endif

// Those are the macros that automatically records down file name and line number for the malloc.
// This portion may also need to be modified for Windows Mobile phone
#define QUIPC_MALLOC(s)             malloc(s)
#define QUIPC_FREE(p)               {if (p != NULL) {free(p);} p = NULL;}

/*=============================================================================================
                        Message queue module
 =============================================================================================*/
// The following constants defines the message queues to be used by QUIPC systems.
// For each message queue, only one thread in QUIPC will be reading messages from the queue,
// although there can be mutliple threads that could write to the queue.
// For example, all the messages intended for IMC mdoule should be put on the QUIPC_MSG_QUEUE_IMC,
// only one dedicated thread from IMC module will be listening on this message queue, but there are
// multiple modules (PE, WMM, IGSD) will be writing to this queue.
//
// Each msg ID should occupy a unique bit on the 32-bit int, As a result, we can support up to
// 32 message queues.
//
#define QUIPC_MSG_QUEUE_IMC  0x00000001 // pipe 0
#define QUIPC_MSG_QUEUE_IPE  0x00000002 // pipe 1
#define QUIPC_MSG_QUEUE_IWMM 0x00000004 // pipe 2
#define QUIPC_MSG_QUEUE_IGSN 0x00000008 // pipe 3
#define QUIPC_MSG_QUEUE_IGSD 0x00000010 // pipe 4
#define QUIPC_MSG_QUEUE_IWSS 0x00000020 // pipe 5
#define QUIPC_MSG_QUEUE_ULP  0x00000040 // pipe 6
#define QUIPC_MSG_QUEUE_MSAPM 0x0000080 // pipe 7
#define QUIPC_MSG_QUEUE_SDP  0x0000100  // pipe 8
#define QUIPC_MSG_QUEUE_SLIMCW 0x0000200 // pipe 9
//note: this is only temporary for testing purposes
// #define QUIPC_MSG_QUEUE_NEXT_QUEUE // Add next message queue ID here and increase the NUM_MSG_QUEUES by 1

// Total number of message queues, please update this number to be consistent with the number of
// message queues supported
#define NUM_MSG_QUEUES 10

// The following constants defines the src and dst module IDs of QUIPC messsages.
#define QUIPC_MODULE_IMC    0x00000001
#define QUIPC_MODULE_IPE    0x00000002
#define QUIPC_MODULE_IWMM   0x00000004
#define QUIPC_MODULE_IGSN   0x00000008
#define QUIPC_MODULE_IGSD   0x00000010
#define QUIPC_MODULE_IWSS   0x00000020
#define QUIPC_MODULE_ULP    0x00000040
#define QUIPC_MODULE_MSAPM  0x00000080 //note: this is only temporary for testing purposes0
#define QUIPC_MODULE_SDP    0x00000100
#define QUIPC_MODULE_SLIMCW 0x00000200
// PLEASE UPDATE the quipc_module_name_table in common\src\log.c

// QUIPC message enum type:
typedef enum
{
   // first message starts at index 1

  // PLEASE UPDATE the quipc_msg_name_table in common\src\quipc_log.c
  // Please write down the enum number
   MSG_TYPE_IGSD_LCI_LOOKUP_REQ               = 1,
   MSG_TYPE_IGSD_LCI_LOOKUP_MS_REQ            = 2,
   MSG_TYPE_IGSD_LCI_ASSIST_DATA_DOWNLOAD_REQ = 3,
   MSG_TYPE_IGSD_GET_CANDIDATE_LCI_RES        = 4,
   MSG_TYPE_IGSD_GET_CANDIDATE_LCI_BY_AP_SCAN_RES = 5,
   MSG_TYPE_IGSD_GET_ASSISTANCE_DATA_RES          = 6,
   MSG_TYPE_IGSD_GET_AP_LIST_RES                  = 7,
   MSG_TYPE_IGSD_GET_LCI_INFO_RES                 = 8,
   MSG_TYPE_IGSD_GET_CANDIDATE_LCI_MS_RES         = 9,
   MSG_TYPE_IGSD_DATABASE_RESET_REQ               = 10,
   MSG_TYPE_IGSD_MAINTENANCE                      = 11,
   MSG_TYPE_IGSD_FINISH                           = 12,
   MSG_TYPE_IGSD_CANCEL                           = 13,

  // PLEASE UPDATE the quipc_msg_name_table in common\src\quipc_log.c
   MSG_TYPE_IGSN_GET_CANDIDATE_LCI_REQ        = 100,
   MSG_TYPE_IGSN_GET_CANDIDATE_LCI_BY_APSCAN_REQ  = 101,
   MSG_TYPE_IGSN_GET_ASSISTANCE_DATA_REQ          = 102,
   MSG_TYPE_IGSN_GET_AP_LIST_REQ                  = 103,
   MSG_TYPE_IGSN_GET_LCI_INFO_REQ                 = 104,
   MSG_TYPE_IGSN_GET_CANDIDATE_LCI_MS_REQ         = 105,
   MSG_TYPE_IGSN_FINISH                           = 106,

   // PLEASE UPDATE the quipc_msg_name_table in common\src\quipc_log.c
   // MSG to IPE module starts at index of 200
   MSG_TYPE_IPE_RESET                        = 200,
   MSG_TYPE_IPE_INJECT_ASSISTANCE_DATA       = 201,
   MSG_TYPE_IPE_RUN_PF                       = 202,
   MSG_TYPE_IPE_DEBUG_REQUEST                = 203,
   MSG_TYPE_IPE_INJECT_LCI_INFO              = 204,
   MSG_TYPE_IPE_INJECT_PEDO_STATUS           = 205,
   MSG_TYPE_IPE_INJECT_PEDO_DATA             = 206,

  // PLEASE UPDATE the quipc_msg_name_table in common\src\quipc_log.c
   //MSG to IMC start at index 300
   MSG_TYPE_IMC_START_REQ                     = 301,
   MSG_TYPE_IMC_STOP_REQ                      = 302,
   MSG_TYPE_IMC_POS_RESP                      = 303,
   MSG_TYPE_IMC_WLAN_MEAS_RESP                = 304,
   MSG_TYPE_IMC_LCI_LOOKUP_RESP               = 305,
   MSG_TYPE_IMC_LCI_LOOKUP_MS_RESP            = 306,
   MSG_TYPE_IMC_LCI_ASSIST_DATA_DOWNLOAD_RESP = 307,
   MSG_TYPE_IMC_COARSE_POS_RESP               = 308,
   MSG_TYPE_IMC_HEARTBEAT_REQ                 = 309,
   MSG_TYPE_IMC_CANCEL_RESP                   = 310,
   MSG_TYPE_IMC_MSAP_UPDATE                   = 311,
   MSG_TYPE_IMC_DEBUG_REQUEST                 = 312,
   MSG_TYPE_IMC_DEBUG_INFO                    = 313,
   MSG_TYPE_IMC_SUB_LCI_ASSISTANCE_DATA_REQ   = 314,
   MSG_TYPE_IMC_LCI_LOOKUP_TIMEOUT            = 315,
   MSG_TYPE_IMC_RESTART_REQ                   = 316,
   MSG_TYPE_IMC_AP_STEERING_INFO              = 317,
   MSG_TYPE_IMC_WLAN_CAPABILITIES_RESP        = 318,
   MSG_TYPE_IMC_BARO_REPORT                   = 319,
   MSG_TYPE_IMC_BARO_STATUS                   = 320,
   MSG_TYPE_IMC_VENUE_STATUS_REQ              = 321,
   MSG_TYPE_IMC_VENUE_STATUS_REQ_TIMEOUT      = 322,

  // PLEASE UPDATE the quipc_msg_name_table in common\src\log.c
   //MSG to IWMM start at index 400
   MSG_TYPE_IWMM_SET_STRATEGY_DEADLINE_REQ        = 401,
   MSG_TYPE_IWMM_IMMEDIATE_PASSIVE_SCAN_REQ       = 402,
   MSG_TYPE_IWMM_LCI_AP_LIST                      = 403,
   MSG_TYPE_IWMM_RESET                            = 404,
   MSG_TYPE_IWSS_IWMM_PASSIVE_SCAN_MEAS           = 405,
   MSG_TYPE_IWSS_IWMM_RTS_CTS_SCAN_MEAS           = 406,
   MSG_TYPE_IWMM_IWMM_STRATEGY_DEADLINE_TIMER_EXP = 407,
   MSG_TYPE_IWMM_START                            = 408,
   MSG_TYPE_IWMM_AP_STEERING_INFO                 = 409,
   MSG_TYPE_IWMM_WLAN_CAPABILITIES_REQ            = 410,
   MSG_TYPE_IWMM_WLAN_CAPABILITIES_RESP           = 411,
   MSG_TYPE_IWMM_IWMM_ASYNC_DISC_SCAN_TIMER_EXP   = 412,

  // PLEASE UPDATE the quipc_msg_name_table in common\src\log.c
   //MSG to IWSS starts at index of 500
   MSG_TYPE_IWMM_IWSS_REQ_PASSIVE_SCAN = 500,
   MSG_TYPE_IWMM_IWSS_REQ_RTS_CTS_SCAN = 501,
   MSG_TYPE_IWMM_IWSS_REQ_START        = 502,
   MSG_TYPE_IWMM_IWSS_REQ_STOP         = 503,

  //TFQ Specific messages start at index of 600
   MSG_TYPE_TFQ_IMC_START = 600,
   MSG_TYPE_TFQ_IMC_STOP = 601,
   MSG_TYPE_TFQ_IMC_QUIPC_START = 602,
   MSG_TYPE_TFQ_IMC_QUIPC_STOP = 603,
   MSG_TYPE_TFQ_IWMM_STOP = 604,
   MSG_TYPE_TFQ_LCI_SWITCH = 605,
   MSG_TYPE_TFQ_IWMM_SIGNAL_WHEN_WLAN_ROUTE_COMPLETED = 606,
   MSG_TYPE_IMC_IWMM_CHANGE_WLAN_FILE = 607,
   MSG_TYPE_IMC_IWMM_CHANGE_WLAN_FILE_ACK = 608,

   // IPEPB specific messages start at index of 800
   MSG_TYPE_IPEPB_IMC_LCI_START = 700,
   MSG_TYPE_IPEPB_IWMM_CONFIG = 701,
   MSG_TYPE_IPEPB_IWMM_SET_STRATEGY_DEADLINE = 702,
   MSG_TYPE_IPEPB_IMC_MEAS_START = 703,

   // MSAPM specific messages start at index of 900
   MSG_TYPE_MSAPM_REQ = 800,
   MSG_TYPE_MSAPM_STOP = 801,

   // PLEASE UPDATE the quipc_msg_name_table in common\src\log.c
   // ULP specific messages start at index of 1000
   MSG_TYPE_ULP_POS_REPORT = 1000,
   MSG_TYPE_ULP_COARSE_POS_REQ = 1001,
   MSG_TYPE_ULP_VENUE_REPORT = 1002,

   // PLEASE UPDATE the quipc_msg_name_table in common\src\log.c
   // SDP specific messages start at index of 1100
   MSG_TYPE_SDP_BARO_REQ = 1100,
   MSG_TYPE_SDP_SLIM_STATUS_RESP = 1101,
   MSG_TYPE_SDP_SLIM_INJECT_SENSOR_DATA_IND = 1102,
   MSG_TYPE_SDP_SLIM_INJECT_SERVICE_STATUS_EVENT_IND = 1103,
   MSG_TYPE_SDP_PEDO_REQ = 1104,
   MSG_TYPE_SDP_SLIM_INJECT_PEDOMETER_DATA_IND = 1105,

   MSG_TYPE_E_SIZE = 0x10000000   // force enum to be 32-bit
}quipc_msg_e_type;

// QUIPC message structure
// Potential feature enhancement: additional field to aid detection of corrupted messages
typedef struct
{
   uint32           msg_size;            /* Total size of the whole message, including msg_size field */
   uint32           msg_payload_size;    /* Msg payload size */
   uint32           msg_id;              /* Unique global ID of messages */
   uint32           src_module_id;       /* Source Module where IPC msg is originated from */
   uint32           dst_module_id;       /* Destionation Module where IPC msg  is sent to*/
   quipc_msg_e_type msg_type;            /* Unique msg Type within quipc subsystem */
   uint64           msg_payload_padding; /* Force msg_payload to start at 8-byte boundary with this field,
                                            must proceed right before msg_payload. This field will also be used
                                            to watermarking the msg.
                                            NOTE: Please do not add any field between this field and msg_payload*/
   uint8           msg_payload[1];      /* data payload */
} quipc_msg_s_type;

/*=============================================================================================
 * Function description:
 *    This function will create the message queue specified in the parameter.
 *    Please note that this function shall be called only once for each msg queue.
 *    The module that will listen on the msg queue shall call this function. For example,
 *    the IGS-N message queue shall be created by the IGSN module that resides in process "Java",
 *    and the IMC message queue shall be created by the IMC module that resides in process "C".
 *
 * Parameters:
 *    msg_queue_id: the message queue id. This shall refer to single message queue id.
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int quipc_msg_queue_create (uint32 msg_queue_id);

/*=============================================================================================
 * Function description:
 *    This function will allocate the memory needed for specified payload_size. In addition to
 *    the message payload, it will also allocate the memory needed for message header and possible
 *    paddings added by OS.
 *
 *    After calling this function, the caller is *ONLY* expected to intialzie the *msg_payload*.
 *    All message header fields are expected to be *READ-ONLY* once returned from quipc_msg_alloc.
 *    In the returned msg, the following message header fields: the msg_size, msg_payload_size,
 *    msg_id field will be filled up by the QUIPC module, and the following message header fields:
 *    src_module_id, dst_module_id and msg_type will be initialized to the specified value passed
 *    via parameters.
 *
 *    In particular, the msg_payload_size should always match with the pass-in msg payload size
 *    with one exception that the msg_payload_size will be set to 1 if the payload_size in parameter
 *    is set to 0.
 *
 *    The msg id field is generated from a global counter that is getting
 *    incremented every time a new message is created. This field is intended to  be used to
 *    track the message flow within the QUIPC system if needed.
 *
 *    The msg_size field may be different than the message header size plus message payload size
 *    due to the padding added by OS abstraction layer.
 *
 *    Note: if the msg payload contains pointer, then the memory referred by the pointer
 *    shall be allocated separately outside of quipc_msg_alloc. This should be done regardless of
 *    the usage scenario, eg: whether variable length payload is at the end of message payload structure.
 *    Any attempt to ues quipc_msg_alloc to allocate the memory needed by
 *    the pointer that is part of the msg payload will have potentail illegal memory access issue.
 *
 *    Following is an example: the mesage payload is a pointer to a variable length char array.
 *    The proper way to do this is:
 *    Step 1: msg_ptr = QUIPC_MSG_ALLOC(src, dst, type, sizeof (ptr) );
 *    Step 2: ptr = QUIPC_MALLOC (variable length);
 *    Step 3: ((char*) msg_ptr->msg_payload) = ptr;
 *    Step 4: put the message on the queue via quipc_msg_send
 *    Step 5: QUIPC_MSG_FREE (msg_ptr);
 *    If the memory for ptr is allocated as part of msg_ptr, then the memory will be freed via QUIPC_MSG_FREE,
 *    and when the receiving end gets the message, the memory pointed by ptr is likely not to be valid any more.
 *
 * Parameters:
 *    src_module_id: ID of the module that calls this function, see defintion of QUIPC_MODULE_XXX
 *    dst_module_id: ID of the module that this message is intended for, see defintion of QUIPC_MODULE_XXX
 *    msg_type:      message type, please note that this is different than msg id
 *    msg_payload_size: msg payload size in bytes. This must match the size of the
 *                      msg payload data structure using sizeof (msg payload).
 *    file_name: file name that msg allocation comes from
 *    line_num:  line num that msg allocation comes from
 *
 * Assumpiton:
 *    The fields of msg_size, msg_payload_size, and msg_id will be filled up by message queue module
 *    implementation. It is *NOT* the responsibility of the caller to intialize those fields. Instead,
 *    those fields shall be treated as *READ-ONLY* to the caller.
 *
 *    The caller of this function must NOT call QUIPC_MSG_FREE after calling quipc_msg_send, since
 *    quipc_msg_send internally will call QUIPC_MSG_FREE for the client.
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern quipc_msg_s_type* quipc_msg_alloc (uint32           src_module_id,
                                          uint32           dst_module_id,
                                          quipc_msg_e_type msg_type,
                                          uint32           msg_payload_size,
                                          const char*      file_name,
                                          uint32           line_num);


// Please use this macro for message allocation so we can easily track where allocation comes from

#define QUIPC_MSG_ALLOC(src_module_id, dst_module_id, msg_type, msg_payload_size) \
quipc_msg_alloc (src_module_id, dst_module_id, msg_type, msg_payload_size, __FILE__, __LINE__)

/*=============================================================================================
 * Function description:
 *    This function will free the memory pointed by msg_ptr.
 *
 * Parameters:
 *    msg_ptr: ptr to QUIPC message
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int quipc_msg_free (quipc_msg_s_type*  msg_ptr,
                           const char*      file_name,
                           uint32           line_num);


// Please use this macro for message free so we can easily track where de-allocation comes from

#define QUIPC_MSG_FREE(msg_ptr) \
quipc_msg_free (msg_ptr, __FILE__, __LINE__)

/*=============================================================================================
 * Function description:
 *    This function will send the message to the specified message queue.
 *
 * Parameters:
 *    msg_queue_id: id of the destination message queue
 *    msg_ptr: ptr to the message to be sent
 *
 * Assumption:
 *    The message must be freed after calling quipc_msg_send.
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int quipc_msg_send (uint32 msg_queue_id, quipc_msg_s_type *msg_ptr);

/*=============================================================================================
 * Function description:
 *    This function will cause the calling thread to block until there is a message available
 *    on the message queue.
 *
 * Parameters:
 *    msg_queue_id: id of the message queue where the calling thread shall be blocked on
 *
 * Assumption;
 *    After the msg has been processed, the received msg must be freed by calling QUIPC_MSG_FREE.
 *
 * Return value:
 *    pointer to the first message that is available on the queue
 =============================================================================================*/
extern quipc_msg_s_type* quipc_msg_receive (uint32 msg_queue_id);




/*=============================================================================================
                                 Timer module
 =============================================================================================*/


/*=============================================================================================
 * Function description:
 *    This function will cause the message to be sent to the queue when the specified amount of
 *    milli-second passed.
 *
 *    Note: The memroy pointed by msg_ptr will only be de-allocated once the timer is expired or
 *          is cancelled. Since the memory pointed by msg_ptr will not be duplicated by this call,
 *          so the caller of this function shall guarantee that the memory pointed by msg_ptr
 *          remains valid.
 *
 * Parameters:
 *    msecs: number of milli-seconds to be passed
 *    msg_queue_id: id of the message queue where the message will be sent
 *    msg_ptr: pointer to the message that should be queued when time expired
 *
 * Return value:
 *    Pointer: a pointer that QUIPC OS layer will use to uniquely identify this timer
 =============================================================================================*/
extern void*  quipc_timer_start(uint32            msecs,
                                uint32            msg_queue_id,
                                quipc_msg_s_type *msg_ptr);

/*=============================================================================================
 * Function description:
 *    This function will cause the message to be sent to the queue when the absolute time is
 *    reached.
 *
 *    Note: The memroy pointed by msg_ptr will only be de-allocated once the timer is expired or
 *          is cancelled. Since the memory pointed by msg_ptr will not be duplicated by this call,
 *          so the caller of this function shall guarantee that the memory pointed by msg_ptr
 *          remains valid.
 *
 * Parameters:
 *    expire_time_msec: time when timer is expected to expire, it is specified as number of
                      milli-seconds since the Epoch
 *    msg_queue_id: id of the message queue where the message will be sent
 *    msg_ptr: pointer to the message that should be queued when time expired
 *
 * Return value:
 *    Pointer: a pointer that QUIPC OS layer will use to uniquely identify this timer
 =============================================================================================*/
extern void*  quipc_abs_timer_start(struct timespec*  p_expire_time,
                                    uint32            msg_queue_id,
                                    quipc_msg_s_type *msg_ptr);



/*=============================================================================================
 * Function description:
 *    This function will cancel the specified timer.
 *
 * Parameters:
 *    timer_info_ptr: timer returned from previous quipc_timer_start or quipc_timer_abs_start.
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int  quipc_timer_cancel(void* timer_info_ptr);

/*=============================================================================================
                                 Thread module
 =============================================================================================*/

/*=============================================================================================
 * Function description:
 *    the start routine for QUIPC thread.
 *
 * Parameters:
 *    arg: user passed in parameter when calling quipc_thread_create
 *
 * Return value:
 *    void *: definition can vary between OSes. In linux, NULL is returned when thread exits
 *            its start routine
 =============================================================================================*/
typedef void* (quipc_thread_start_routine) (void* arg);

/*=============================================================================================
 * Function description:
 *    This function creates a thread with the stack size specified in the input parameter.
 *    Subsequent call to quipc_thread_start need to be invoked to execute the thread start
 *    routine.
 *
 * Parameters:
 *    use_custom_stack_size: 1: the stack size for the thread will be specified
 *                           0: the stack size for the thread will not be specified,
 *                              use the default stack size provided by the OS
 *    stack_size: if use_custom_stack_size is set to 1, this will parameter will specify
 *                the stack size
 *    thread_name: name of the thread to be displayed with profile tool
 * Return value:
 *    void *: A unique value that will be used by QUIPC OS abstraction layer to identify the
 *            the OS thread
 =============================================================================================*/
extern void* quipc_thread_create (boolean  use_custom_stack_size, /* 1: Use custom stack size, 0: use default stack size */
                                  unsigned int stack_size, /* in bytes */
                                  const char*  thread_name
                                  );

/*=============================================================================================
 * Function description:
 *    This function causes the thread start routine to be executed.
 *
 * Parameters:
 *    thread_info_ptr: pointer to the quipc OS thread that is returned via quipc_thread_create.
 *    start_func: the start routine for the QUIPC OS thread
 *    client_arg: argument to be passed to the start routine of the QUIPC OS thread
 *
 * Return value:
 *    error code: 0: success
 *                non-0: error code
 =============================================================================================*/
extern int quipc_thread_start (void* thread_info_ptr,
                               quipc_thread_start_routine* start_func,
                               void * client_arg              /* Argument to be passed to thread_func */
                                  );


/*=============================================================================================
 * quipc_thread_check_stack
 *
 * Function description:
 *    This function checks the current stack depth. It is sort of expensive so it is
 *    recommended only for during debugging, profiling or test mode only.
 *
 * Parameters: None
 *
 * Return value: None
 *
 =============================================================================================*/
extern void quipc_thread_check_stack(void);


/*=============================================================================================
 * Function description:
 *    This function will exit the start routine of the calling thread. It will also free the
 *    resource used by OS abstration layer for the thread. This function does not need to be
 *    called for daemon thread that lasts as long as its owner. But for short-life thread, this
 *    function need to be called before the thread exists from its start routine.
 *    Please note that this function can only be called within the start routine of its own thread
 *    pointed by thread_info_ptr.
 *
 * Parameters:
 *    thread_info_ptr: pointer to the quipc OS thread
 *    value_ptr: value_ptr will be made available to any successful join with
 *               the terminating thread via quipc_thread_wait_for_exit
 *
 * Return value:
 *    none
 =============================================================================================*/
extern void quipc_thread_exit (void* thread_info_ptr, void* value_ptr);

/*=============================================================================================
 * Function description:
 *    This function will cause the calling thread to wait for the thread specified in
 *    thread_info_ptr to exit.
 *
 * Parameters:
 *    thread_info_ptr: pointer to the quipc OS thread
 *    exit_value_ptr: Upon successful join, this will be the pointer to the value_ptr that is made
 *                    available by the exiting thread
 *
 * Return value:
 *    error code: 0: success
 *                non-0: error code
 =============================================================================================*/
extern int quipc_thread_wait_for_exit (void* thread_info_ptr, void** exit_value_ptr);

/*=============================================================================================
 * Function description:
 *    This function will put the current thread to sleep for the specified amount of milli-seconds.
 *    Depending on the OS limitation, the supported resolution may not be 1 milli-seconds.
 *
 * Parameters:
 *    none
 *
 * Return value:
 *    none
 =============================================================================================*/
extern void quipc_thread_sleep_msecs (uint64 msecs);


/*=============================================================================================
                                 Utility module
 =============================================================================================*/


/*=============================================================================================
 * Function description:
 *    This function will remove a file from the filesystem
 *
 * Parameters:
 *    file_name: the name of the file to remove
 *
 * Return value:
 *    error code: 0: success
 *                non-0: error code
 =============================================================================================*/
extern int quipc_remove_file (const char* file_name);

/*=============================================================================================
 * Macro description for UUID related
 *
 =============================================================================================*/

/*=============================================================================================
 * MACRO: QUIPC_UUID_FORMAT, QUIPC_UUID_CONTENT
 * These two macros are just mapping of UUID content from unsigned char[16] to string format
 =============================================================================================*/
#define QUIPC_UUID_FORMAT() \
  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define QUIPC_UUID_CONTENT(uuid) \
  *(uuid) & 0x00ff, *((uuid)+1) & 0x00ff, *((uuid)+2) & 0x00ff, *((uuid)+3) & 0x00ff,     \
  *((uuid)+4) & 0x00ff, *((uuid)+5) & 0x00ff, *((uuid)+6) & 0x00ff, *((uuid)+7) & 0x00ff, \
  *((uuid)+8) & 0x00ff, *((uuid)+9) & 0x00ff, *((uuid)+10)& 0x00ff, *((uuid)+11)& 0x00ff, \
  *((uuid)+12)& 0x00ff, *((uuid)+13)& 0x00ff, *((uuid)+14)& 0x00ff, *((uuid)+15) & 0x00ff

/*=============================================================================================
 * MACRO: QUIPC_PRINT_UUID*
 * This macro prints the UUID with some user given string header
 * Parameters:
 *   s - user header
 *   c - the UUID to be printed
 =============================================================================================*/
#define QUIPC_PRINT_UUID(s,c) \
  QUIPC_DBG_MED("%s " QUIPC_UUID_FORMAT() "\n", \
                  s, QUIPC_UUID_CONTENT(c))

#define QUIPC_PRINT_UUID_LOW(s,c) \
  QUIPC_DBG_LOW("%s " QUIPC_UUID_FORMAT() "\n", \
                  s, QUIPC_UUID_CONTENT(c))

#define QUIPC_PRINT_UUID_MED(s,c) \
  QUIPC_DBG_MED("%s " QUIPC_UUID_FORMAT() "\n", \
                  s, QUIPC_UUID_CONTENT(c))

#define QUIPC_PRINT_UUID_HIGH(s,c) \
  QUIPC_DBG_HIGH("%s " QUIPC_UUID_FORMAT() "\n", \
                  s, QUIPC_UUID_CONTENT(c))

/*=============================================================================================
 * MACRO: QUIPC_CONVERT_UUID_TO_STRING
 * This macro converts UUID that is in unsigned char[16] into string.
 * Parameters:
 *   uuid - UUID byte array, has to be in unsigned char[16]
 *   str - buffer to hold the conversion, must be at least hold 36 characters, i.e. char[36]
 *   str_sz - the size of the buffer above
 * Return value: Number of bytes converted, -1 if error
 =============================================================================================*/
#define QUIPC_UUID_CONVERT_TO_STRING(uuid, str, str_sz) \
 snprintf(str, str_sz,                                  \
          QUIPC_UUID_FORMAT(),                          \
          QUIPC_UUID_CONTENT(uuid))

// MACRO to set UUID to UNKNOWN
#define QUIPC_SET_UUID_TO_UNKNOWN(c) \
   memset(c, 0, UUID_LENGTH); \
   *(c+8) = 0xC0; \
   *(c+15) = 0x46;


/*=============================================================================================
 * Function description:
 *    This function will return TRUE if the LCI ID is unknown.
 *
 * Parameters:
 *    lci_id_ptr: pointer to LCI ID
 *
 * Return value:
 *    TRUE: LCI ID is unknown
 *    FALSE: LCI ID is not unknown
 =============================================================================================*/
extern boolean quipc_uuid_is_unknown (unsigned char* lci_id_ptr);

/*=============================================================================================
 * Function description:
 *    This function will remove a file from the filesystem
 *
 * Parameters:
 *    file_name: the name of the file to remove
 *
 * Return value:
 *    error code: 0: success
 *                non-0: error code
 =============================================================================================*/
extern int quipc_remove_file (const char* file_name);

/*=============================================================================================
 * Function description:
 *    This function will print out the memguard for the memory.
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    None
 =============================================================================================*/
extern void quipc_print_mem_end_mark (const char* file_name, uint32 line_num, void* ptr, int size);
#define QUIPC_PRINT_MEM_END_MARK(ptr, size) quipc_print_mem_end_mark(__func__, __LINE__, ptr, size)

/*=============================================================================================
 * MACRO: QUIPC_MACADDR_FMT, QUIPC_MACADDR
 * These two macros are just mapping of Mac Address content from unsigned char[6] to string format
 =============================================================================================*/
#define QUIPC_MACADDR_FMT \
  "%02x:%02x:%02x:%02x:%02x:%02x"
#define QUIPC_MACADDR(mac_addr) \
  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]

/*=============================================================================================
 * Function description:
 *    This function will convert the mac_address to int64
 *
 * Parameters:
 *    Mac Address to be converted to int64
 *
 * Return value:
 *    int64 representation of Mac Address
 =============================================================================================*/
extern int64 quipc_mac_addr_to_i64(const uint8 * const mac_addr);

/*=============================================================================================
 * Function description:
 *    This function will check if mac_addr is part of range specified by
 *    [base_mac, base_mac + range -1]
 *
 * Parameters:
 *    Mac Addresses and range
 *
 * Return value:
 *    TRUE if mac_addr is part of address range
 =============================================================================================*/
extern boolean quipc_mac_addr_match_range(const uint8 * const mac_addr,
                                          const uint8 * const base_mac,
                                          const int16 range);
#ifdef __cplusplus
}
#endif

#endif /* OSAL_OS_API_H */
