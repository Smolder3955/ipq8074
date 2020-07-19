/*============================================================================*/
/*                          N r p C o n t r o l                               */
/*----------------------------------------------------------------------------*/
/*      (C) Copyright 2003-2010 Rohde & Schwarz  All Rights Reserved.         */
/*----------------------------------------------------------------------------*/
/*                                                                            */
/* Title:       NrpControl2.h                                                 */
/* Purpose:     Declarations for the Nrp Control Library                      */
/*============================================================================*/
#ifndef __NRPCONTROL_H__
#define __NRPCONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Version Informations
 */
#define NRP_VAL_MAJOR_VERSION                   3
#define NRP_VAL_MINOR_VERSION                   610

/** Pascal calling convention for exported functions from DDL */
#define _NRP_FUNC               __stdcall

/** Error code offset 0x80000000 */
#define     NRP_ERROR_OFFSET                ( 0x80000000L )
#define     NRP_ERROR_USB_OFFSET            ( NRP_ERROR_OFFSET + 0x60000000L ) /* 0xE000000 */
#define     NRP_ERROR_DEVICE_OFFSET         ( NRP_ERROR_OFFSET + 0x50000000L ) /* 0xD000000 */
#define     NRP_ERROR_CONTROL_OFFSET        ( NRP_ERROR_OFFSET + 0x40000000L ) /* 0xC000000 */

/*
 * ERROR CODES
 */
#define NRP_SUCCESS                         ( 0x0L )
#define NRP_IN_PROGRESS                     ( NRP_SUCCESS + 0x01L )

#define NRP_ERROR_READ_BUFFER_ALLOC         ( NRP_ERROR_USB_OFFSET + 0x0FFF0001L ) /* 0xEFFF0001 */ 
#define NRP_ERROR_START_READ_THREAD         ( NRP_ERROR_USB_OFFSET + 0x0FFF0002L ) /* 0xEFFF0002 */ 
#define NRP_ERROR_NULL_DEVICE_LIST          ( NRP_ERROR_USB_OFFSET + 0x0FFF0003L ) /* 0xEFFF0003 */ 

#define NRP_ERROR_USB_DISCONNECTED          ( NRP_ERROR_CONTROL_OFFSET +  1 ) /* 0xC0000001 */
#define NRP_ERROR_INVALID_RESOURCE          ( NRP_ERROR_CONTROL_OFFSET +  2 ) /* 0xC0000002 */
#define NRP_ERROR_NOT_SUPPORTED_CALLBACK    ( NRP_ERROR_CONTROL_OFFSET +  3 ) /* 0xC0000003 */
#define NRP_ERROR_TIMEOUT                   ( NRP_ERROR_CONTROL_OFFSET +  4 ) /* 0xC0000004 */
#define NRP_ERROR_DEVICE_DISCONNECTED       ( NRP_ERROR_CONTROL_OFFSET +  5 ) /* 0xC0000005 */
#define NRP_ERROR_INVALID_SESSION           ( NRP_ERROR_CONTROL_OFFSET +  6 ) /* 0xC0000006 */
#define NRP_ERROR_CONFIGURATION_STORE       ( NRP_ERROR_CONTROL_OFFSET +  7 ) /* 0xC0000007 */
#define NRP_ERROR_DESCRIPTOR_INDEX          ( NRP_ERROR_CONTROL_OFFSET +  8 ) /* 0xC0000008 */
#define NRP_ERROR_LOGICAL_NAME_EXIST        ( NRP_ERROR_CONTROL_OFFSET +  9 ) /* 0xC0000009 */
#define NRP_ERROR_DATA_QUEUE_EMPTY          ( NRP_ERROR_CONTROL_OFFSET + 10 ) /* 0xC000000A */
#define NRP_ERROR_STRING_QUEUE_EMPTY        ( NRP_ERROR_CONTROL_OFFSET + 11 ) /* 0xC000000B */
#define NRP_ERROR_FLOAT_QUEUE_EMPTY         ( NRP_ERROR_CONTROL_OFFSET + 12 ) /* 0xC000000C */
#define NRP_ERROR_LONG_QUEUE_EMPTY          ( NRP_ERROR_CONTROL_OFFSET + 13 ) /* 0xC000000D */
#define NRP_ERROR_BITFIELD_QUEUE_EMPTY      ( NRP_ERROR_CONTROL_OFFSET + 14 ) /* 0xC000000E */
#define NRP_ERROR_BINARY_QUEUE_EMPTY        ( NRP_ERROR_CONTROL_OFFSET + 15 ) /* 0xC000000F */
#define NRP_ERROR_FLOAT_ARRAY_QUEUE_EMPTY   ( NRP_ERROR_CONTROL_OFFSET + 16 ) /* 0xC0000010 */
#define NRP_ERROR_PARAMETER1                ( NRP_ERROR_CONTROL_OFFSET + 17 ) /* 0xC0000011 */
#define NRP_ERROR_PARAMETER2                ( NRP_ERROR_CONTROL_OFFSET + 18 ) /* 0xC0000012 */
#define NRP_ERROR_PARAMETER3                ( NRP_ERROR_CONTROL_OFFSET + 19 ) /* 0xC0000013 */
#define NRP_ERROR_PARAMETER4                ( NRP_ERROR_CONTROL_OFFSET + 20 ) /* 0xC0000014 */
#define NRP_ERROR_PARAMETER5                ( NRP_ERROR_CONTROL_OFFSET + 21 ) /* 0xC0000015 */
#define NRP_ERROR_PARAMETER6                ( NRP_ERROR_CONTROL_OFFSET + 22 ) /* 0xC0000016 */
#define NRP_ERROR_SMALL_BUFFER_SIZE         ( NRP_ERROR_CONTROL_OFFSET + 23 ) /* 0xC0000017 */
#define NRP_ERROR_UNKNOWN_TRIGGER_STATE     ( NRP_ERROR_CONTROL_OFFSET + 24 ) /* 0xC0000018 */
#define NRP_ERROR_INVALID_SERIAL_NUMBER     ( NRP_ERROR_CONTROL_OFFSET + 25 ) /* 0xC0000019 */
#define NRP_ERROR_INVALID_SENSOR_TYPE       ( NRP_ERROR_CONTROL_OFFSET + 26 ) /* 0xC000001A */
#define NRP_ERROR_INCORRECT_DATA_ORDER      ( NRP_ERROR_CONTROL_OFFSET + 27 ) /* 0xC000001B */
#define NRP_ERROR_UNKNOWN_ATTRIBUTE         ( NRP_ERROR_CONTROL_OFFSET + 28 ) /* 0xC000001C */
#define NRP_ERROR_RESTRICTED_FOR_ONE_SENSOR ( NRP_ERROR_CONTROL_OFFSET + 29 ) /* 0xC000001D */
#define NRP_ERROR_BOOTLOADER_FAIL           ( NRP_ERROR_CONTROL_OFFSET + 30 ) /* 0xC000001E */
#define NRP_ERROR_SESSION_DESTROY           ( NRP_ERROR_CONTROL_OFFSET + 31 ) /* 0xC000001F */
#define NRP_ERROR_NOT_CONNECTED_SENSOR      ( NRP_ERROR_CONTROL_OFFSET + 32 ) /* 0xC0000020 */
#define NRP_ERROR_INVALID_FILENAME          ( NRP_ERROR_CONTROL_OFFSET + 33 ) /* 0xC0000021 */

#define NRP_ERROR_HOST_CONNECTION           ( NRP_ERROR_CONTROL_OFFSET + 129 )/* 0xC0000081 */
#define NRP_ERROR_STILL_CONNECTED           ( NRP_ERROR_CONTROL_OFFSET + 130 )/* 0xC0000082 */
#define NRP_ERROR_CLIENT_DENIED             ( NRP_ERROR_CONTROL_OFFSET + 132 )/* 0xC0000084 */

// ....
/****************************************************************/
/* Sensor specific errors                                       */
/****************************************************************/
#define NRP_ERROR_DEVICE_CALDATA_FORMAT      ( NRP_ERROR_DEVICE_OFFSET + 0x01 )
#define NRP_ERROR_DEVICE_OVERRANGE           ( NRP_ERROR_DEVICE_OFFSET + 0x02 )
#define NRP_ERROR_DEVICE_NOTINSERVICEMODE    ( NRP_ERROR_DEVICE_OFFSET + 0x03 )
#define NRP_ERROR_DEVICE_CALZERO             ( NRP_ERROR_DEVICE_OFFSET + 0x04 )
#define NRP_ERROR_DEVICE_TRIGGERQUEUEFULL    ( NRP_ERROR_DEVICE_OFFSET + 0x05 )
#define NRP_ERROR_DEVICE_EVENTQUEUEFULL      ( NRP_ERROR_DEVICE_OFFSET + 0x06 )
#define NRP_ERROR_DEVICE_SAMPLEERROR         ( NRP_ERROR_DEVICE_OFFSET + 0x07 )
#define NRP_ERROR_DEVICE_OVERLOAD            ( NRP_ERROR_DEVICE_OFFSET + 0x08 )
#define NRP_ERROR_DEVICE_HARDWARE            ( NRP_ERROR_DEVICE_OFFSET + 0x09 )
#define NRP_ERROR_DEVICE_CHECKSUM            ( NRP_ERROR_DEVICE_OFFSET + 0x0a )
#define NRP_ERROR_DEVICE_ILLEGALSERIAL       ( NRP_ERROR_DEVICE_OFFSET + 0x0b )
#define NRP_ERROR_DEVICE_FILTERTRUNCATED     ( NRP_ERROR_DEVICE_OFFSET + 0x0c )
#define NRP_ERROR_DEVICE_BURST_TOO_SHORT     ( NRP_ERROR_DEVICE_OFFSET + 0x0d )
#define NRP_ERROR_DEVICE_COMMUNICATION_ERROR ( NRP_ERROR_DEVICE_OFFSET + 0x0e )
#define NRP_ERROR_DEVICE_RESULT_QUESTIONABLE ( NRP_ERROR_DEVICE_OFFSET + 0x40 )

/* fatal errors */
#define NRP_ERROR_DEVICE_GENERIC             ( NRP_ERROR_DEVICE_OFFSET + 0x80 )
#define NRP_ERROR_DEVICE_OVERMAX             ( NRP_ERROR_DEVICE_OFFSET + 0x81 )
#define NRP_ERROR_DEVICE_UNDERMIN            ( NRP_ERROR_DEVICE_OFFSET + 0x82 )
#define NRP_ERROR_DEVICE_VOLTAGE             ( NRP_ERROR_DEVICE_OFFSET + 0x83 )  /* +/-5 Volt not correct */
#define NRP_ERROR_DEVICE_SYNTAX              ( NRP_ERROR_DEVICE_OFFSET + 0x84 )
#define NRP_ERROR_DEVICE_MEMORY              ( NRP_ERROR_DEVICE_OFFSET + 0x85 )
#define NRP_ERROR_DEVICE_PARAMETER           ( NRP_ERROR_DEVICE_OFFSET + 0x86 )
#define NRP_ERROR_DEVICE_TIMING              ( NRP_ERROR_DEVICE_OFFSET + 0x87 )
#define NRP_ERROR_DEVICE_NOTIDLE             ( NRP_ERROR_DEVICE_OFFSET + 0x88 )
#define NRP_ERROR_DEVICE_UNKNOWNCOMMAND      ( NRP_ERROR_DEVICE_OFFSET + 0x89 )
#define NRP_ERROR_DEVICE_OUTBUFFERFULL       ( NRP_ERROR_DEVICE_OFFSET + 0x8a )
#define NRP_ERROR_DEVICE_FLASHPROG           ( NRP_ERROR_DEVICE_OFFSET + 0x8b )
#define NRP_ERROR_DEVICE_CALDATANOTPRESENT   ( NRP_ERROR_DEVICE_OFFSET + 0x8c )


/*
 * All data types available for read from Nrp sensors
 */
typedef enum
{
  DATA_BINARYBLOCK,    /*  0 */
  DATA_BITFIELDLIMIT,  /*  1 */
  DATA_BITFIELDPARAM,  /*  2 */
  DATA_BITFIELDFEATURE,/*  3 */
  DATA_FLOATARRAY,     /*  4 */
  DATA_FLOATLIMIT,     /*  5 */
  DATA_FLOATPARAM,     /*  6 */
  DATA_FLOATRESULT,    /*  7 */
  DATA_LONGLIMIT,      /*  8 */
  DATA_LONGPARAM,      /*  9 */
  DATA_STRING,         /* 10 */
  DATA_AUXFLOATARRAY,  /* 11 */
  DATA_NROF            /* 12 */
} EDataType;

/*
 * Trigger states
 */
#define NRP_TRIGGER_UNKNOWN            ( -1L )
#define NRP_TRIGGER_IDLE               (  0L )
#define NRP_TRIGGER_RESERVED           (  1L )
#define NRP_TRIGGER_WAIT_FOR_TRIGGER   (  2L )
#define NRP_TRIGGER_MEASURING          (  3L )

/*
 * Session handle & user argument
 */
#ifdef _WIN64	// _M_IA64
typedef void* NRP_SESSION;
typedef void* NRP_USERARG;
#else
typedef long NRP_SESSION;
typedef long NRP_USERARG;
#endif

/*
 * Callback definition.
 * These callbacks can be called when some event has been occured.
 */
typedef void (_NRP_FUNC *Nrp_CommandAcceptedFuncPtr)( NRP_SESSION session,
                                                      long groupParam,
                                                      NRP_USERARG userArgument );

typedef void (_NRP_FUNC *Nrp_DataAvailableFuncPtr)( NRP_SESSION session,
                                                    long dataType,
                                                    NRP_USERARG userArgument );

typedef void (_NRP_FUNC *Nrp_ErrorOccurredFuncPtr)( NRP_SESSION session,
                                                    long errorCode,
                                                    NRP_USERARG userArgument );

typedef void (_NRP_FUNC *Nrp_StateChangedFuncPtr)( NRP_SESSION session,
                                                   long sensorTriggerState,
                                                   NRP_USERARG userArgument );

typedef void (_NRP_FUNC *Nrp_DeviceChangedFuncPtr)( NRP_USERARG userArgument );

typedef void (_NRP_FUNC *Nrp_StillAliveFuncPtr)( NRP_SESSION session,
                                                 long sensorTriggerState,
                                                 NRP_USERARG userArgument );

/**************************************************************************/
/*= Nrp Attributes =======================================================*/
/**************************************************************************/
#define NRP_ATTR_BOOTLOADER_VERSION         ( 0x3FFF0000L )  /* string */
#define NRP_ATTR_MAJOR_VERSION              ( 0x3FFF0001L )
#define NRP_ATTR_MINOR_VERSION              ( 0x3FFF0002L )
#define NRP_ATTR_USER_DATA                  ( 0x3FFF0007L )
#define NRP_ATTR_FORCE_USER_DATA            ( 0x3FFF0008L )

#define NRP_ATTR_DISTANCE_DEFAULT           ( 0x3FFF0010L )
#define NRP_ATTR_DISTANCE_LONG              ( 0x3FFF0011L )
#define NRP_ATTR_DISTANCE_ANYWHERE_USB      ( 0x3FFF0012L )

/**************************************************************************/
/*= Exported functions ===================================================*/
/**************************************************************************/

/** Connects to the USB kernel mode driver */
long _NRP_FUNC  NrpOpenDriver(void);

/** Disconnects from the USB kernel mode driver */
long _NRP_FUNC  NrpCloseDriver(void);

/** Builds the current device list and freezes this state */
long _NRP_FUNC  NrpLockDeviceList(void);

/** Commits device list and store it to system registry */
long _NRP_FUNC  NrpCommitDeviceList(void);

/** Gets number of devices in locked list */
long _NRP_FUNC  NrpGetDeviceListLength(
    long* pLength
);

/** Gets device on index position in device list */
long _NRP_FUNC  NrpGetDeviceInfo(
    long    index,
    char    logicalName[],
    char    sensorType[],
    char    serial[],
    long*   pIsConnected
);

/** Sets logical name on index position in device list */
long _NRP_FUNC  NrpSetDeviceInfo(
    long        index,
    const char* logicalName,
    const char* sensorType,
    const char* serial
);

/** Adds an entry to the device list */
long _NRP_FUNC  NrpAddDeviceInfo(
    const char* logicalName,
    const char* sensorType,
    const char* serial
);

/** Removes an entry from the device list */
long _NRP_FUNC  NrpDeleteDeviceInfo(
    long    index
);

long _NRP_FUNC NrpGetResourceName(
    long         lIndex,
    char         *cpResource,
    unsigned int uiMaxlen
);

/*==========================================================================*
 * Communication with a selected sensor
 *==========================================================================*/
/** Opens the named sensor and returns its session */
long _NRP_FUNC  NrpOpenSensor(
    const char  *resourceDescriptor,
    NRP_SESSION *pSession
);

/** Close the specifies sensor */
long _NRP_FUNC  NrpCloseSensor(
    NRP_SESSION  session
);


/** Gets the logical name, sensor type and serial number. */
long _NRP_FUNC  NrpGetSensorInfo(
    NRP_SESSION  session,
    char         name[128],
    char         type[128],
    char         serial[128]
);

/** Sends a Vendor In Request to the specified sensor */
long _NRP_FUNC NrpSendVendorInRequest(
    NRP_SESSION  session,
    char*        pBuffer,
    long         count,
    long         request,
    long         index
);

/** Sends a Vendor Out Request to the specified sensor */
long _NRP_FUNC NrpSendVendorOutRequest(
    NRP_SESSION  session,
    long         request,
    long         index,
    long         value
);

/** Use for USB Standard Device Request Get Descriptor */
long _NRP_FUNC NrpSDRGetDescriptor(
    NRP_SESSION  session,
    void*        pBuffer,
    long*        pByteCount,
    long         recipient,
    long         descrType,
    long         descrIndex,
    long         langID
);

/** Sends a command to the sensor */
long _NRP_FUNC  NrpSendCommand(
    NRP_SESSION      session,
    const char*      command,
    long             timeout
);

/** Indicates if the data are available to be fetched. */
long _NRP_FUNC  NrpDataAvailable(
    NRP_SESSION  session,
    long*        pDataCount
);

/** Gets information about the next data type */
long _NRP_FUNC  NrpGetData(
    NRP_SESSION  session,
    long*        pBlockType,
    long*        pGroupNumber,
    long*        pParamNumber
);

/** Fetchs a single float result with its 2 aux values */
long _NRP_FUNC  NrpGetFloatResult(
    NRP_SESSION  session,
    float*       pValue1,
    float*       pValue2,
    float*       pValue3
);

/** Fetchs a single long result */
long _NRP_FUNC  NrpGetLongResult(
    NRP_SESSION  session,
    long*        pValue1,
    long*        pValue2,
    long*        pValue3
);

/** Fetchs a single bitfield result */
long _NRP_FUNC  NrpGetBitfieldResult(
    NRP_SESSION  session,
    long*        pValue1,
    long*        pValue2,
    long*        pValue3
);

/** Fetchs a single string result */
long _NRP_FUNC  NrpGetStringResult(
    NRP_SESSION  session,
    char*        pBuffer,
    long         size
);

/** Gets size of the bianry data */
long _NRP_FUNC  NrpGetBinaryResultLength(
    NRP_SESSION  session,
    long*        pSize
);

/** Fetchs a binary data */
long _NRP_FUNC  NrpGetBinaryResult(
    NRP_SESSION     session,
    unsigned char*  pBuffer,
    long            bufferSize,
    long*           pReadCount
);

/** Gets size of the float array data */
long _NRP_FUNC  NrpGetFloatArrayLength(
    NRP_SESSION  session,
    long*        pLength
);

/** Gets size of the aux float array data */
long _NRP_FUNC  NrpGetAuxFloatArrayLength(
    NRP_SESSION  session,
    long*        pLength
);

/** Fetchs a float array data */
long _NRP_FUNC  NrpGetFloatArray(
    NRP_SESSION  session,
    float*       pArray,
    long         arrayLength,
    long*        pReadCount
);

/** Fetches a float array data with aux values */
long _NRP_FUNC  NrpGetAuxFloatArray(
    NRP_SESSION  session,
    float*       pResult,
    float*       pAux1,
    float*       pAux2,
    long         arrayLength,
    long*   pReadCount
);

/** Discards the result which comes next in the queue */
long _NRP_FUNC  NrpDiscardResult(
    NRP_SESSION  session
);

/** Removes all internal buffers */
long _NRP_FUNC  NrpEmptyAllBuffers(
    NRP_SESSION  session
);

/** Gets the current state of the sensor trigger system. */
long _NRP_FUNC  NrpGetTriggerState(
    NRP_SESSION  session,
    long*        pState
);

/** Gets the current state of the sensor trigger system into readable text. */
long _NRP_FUNC  NrpGetTriggerStateText(
    long    triggerState,
    char    pBuffer[],
    long    size
);

/** Registers the callback for command accepted event. */
long _NRP_FUNC  NrpSetNotifyCallbackCommandAccepted(
    Nrp_CommandAcceptedFuncPtr  callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Registers the callback for data available event. */
long _NRP_FUNC  NrpSetNotifyCallbackDataAvailable(
    Nrp_DataAvailableFuncPtr    callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Registers the callback for error occured event. */
long _NRP_FUNC  NrpSetNotifyCallbackErrorOccurred(
    Nrp_ErrorOccurredFuncPtr    callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Registers the callback for state changed event. */
long _NRP_FUNC  NrpSetNotifyCallbackStateChanged(
    Nrp_StateChangedFuncPtr     callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Registers the callback for device changed event. */
long _NRP_FUNC  NrpSetNotifyCallbackDeviceChanged(
    Nrp_DeviceChangedFuncPtr    callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Registers the callback for still alive event. */
long _NRP_FUNC  NrpSetNotifyCallbackStillAlive(
    Nrp_StillAliveFuncPtr       callbackPtr,
    NRP_USERARG                 usrArgument
);

/** Returns the oldest error code */
long _NRP_FUNC  NrpGetError(
    NRP_SESSION  session,
    long*        pErrorCode
);

/** Clears internal error queue.*/
long _NRP_FUNC  NrpEmptyErrorQueue(
    NRP_SESSION  session
);

/** Converts a status code into user-readable string. */
long _NRP_FUNC  NrpGetErrorText(
    long    errorCode,
    char    pBuffer[],
    long    size
);

/** high level abort with highest priority over USB */
long _NRP_FUNC NrpClearDevice(
    NRP_SESSION  session
);

/** Sets a specified attribute for given object. */
long _NRP_FUNC NrpSetAttribute(
    NRP_SESSION     session,
    unsigned long   attrName,
    NRP_USERARG     attrValue
);

/** Queries the specified attribute for a given object. */
long _NRP_FUNC NrpGetAttribute(
    NRP_SESSION     session,
    unsigned long   attrName,
    NRP_USERARG     *pAttrValue
);
        
/** Sends a binary data for SCPI command by using a "define length block" to the sensor. */
long _NRP_FUNC  NrpSendBinaryBlock(
    NRP_SESSION     session,
    const char*     command,
    void*           pBinaryData,
    long            size,
    long            timeout
);

/*==========================================================================*
 * Firmware update interface
 *==========================================================================*/
typedef enum tag_FwuState
{
  FWUST_IDLE = 0,         // Idle -- Nothing to do
  FWUST_PREPARE,          // Search for sensor with given resource name
  FWUST_OPENDEV,          // Opening "normal" NRP sensor 
  FWUST_CHECK,            // Check for correct sensor type etc.
  FWUST_SETUP,            // Send FWU vendor requests
  FWUST_SETUPCLOSE,       // Close "normal" NRP sensor
  FWUST_WAIT4UPDATEDEVICE,// Wait for re-enumeration as "Boot"/FwUpdate device
  FWUST_OPENUPDATEDEVICE, // Open FwUpdate device
  FWUST_PREPDOWNLOAD,     // Send required vendor requests
  FWUST_DOWNLOAD,         // Download data
  FWUST_READDLSTATUS,     // Checking for Download okay
  FWUST_CHECKDLCS,        // Checking for correct checksum
  FWUST_INITPROCESS,      // Start erasing
  FWUST_ERASING,          // Wait for erasing finished
  FWUST_PROGRAMMING,      // Wait for programming finished
  FWUST_DONEPROCESS,      // Wait for process finished
  FWUST_TIMO,             // Handling timeout
  FWUST_CANCEL,           // Canceling FwUpdate
  FWUST_DORESET,          // send a VRT_RESET_ALL
  FWUST_DONE,             // function done (continue with next file {=FWUST_PREPARE} or goto CLEANUP)
  FWUST_CLEANUP           // clean up and goto IDLE
} eFwuState;

// NrpUpdateOpen() sets up a firmware-update thread for a given
//    NRP-Zxx sensor (resource name might be "*") and with a given
//    *.NRP file. The function initializes a session parameter for
//    subsequent status- and progress requests
//    Function returns NRP_SUCCESS or a NRP_ERROR_xxxx code
long _NRP_FUNC NrpUpdateOpen(
    const char*   cpResourceName,
    const char*   cpUpdateFileNRP,
    NRP_SESSION*  plSession
);

// NrpUpdateGetState() returns the internal states of the
//    firmware-update process. This function can be used to
//    trigger updates of GUI elements whenever the update-
//    thread state changes. The states are defined as enums
//    with names as FWUST_xxxx as found above
long _NRP_FUNC NrpUpdateGetState(
    NRP_SESSION lSession,
    int*        piState
);

// NrpUpdateGetError() returns NRP_SUCCESS or an error code, i. e.
//    NRP_ERROR_INVALID_SESSION  when lSession is invalid
//    NRP_SUCCESS                when the firmware-update finished successfully
//    NRP_ERROR_xxxxx            in case of an error
long _NRP_FUNC NrpUpdateGetError(
    NRP_SESSION lSession
);

// NrpUpdateGetProgress()  returns a percentage of the update progress
//    in the range of 0...100.  This can be used to visualize the
//    update in a progress-bar
int  _NRP_FUNC NrpUpdateGetProgress(
    NRP_SESSION lSession
);

// NrpUpdateGetInfoText() return textual information about the current
//    update progress. This can be used to show status
//    information to the user in a text-field
const char* _NRP_FUNC NrpUpdateGetInfoText(
    NRP_SESSION lSession
);

// NrpUpdateCancel() can be used to abort an active
//    update-process
long _NRP_FUNC NrpUpdateCancel(
    NRP_SESSION lSession
);

// NrpUpdateClose()  This function shall be used to close a
//    firmware-update session which was created by a previous
//    call of NrpUpdateOpen();
long _NRP_FUNC NrpUpdateClose(
    NRP_SESSION* plSession
);


// NrpGetDeviceStatusZ5()
//   This functions provides information about whether an
//   NRP-Z5 is connected to the PC.
//   The function return
//     NRP_SUCCESS                    if an NRP-Z5 was found
//     NRP_ERROR_DEVICE_DISCONNECTED  No NRP-Z5 is connected
long _NRP_FUNC NrpGetDeviceStatusZ5( void );

// NrpGetDeviceInfoZ5()
//   If the above status function (= NrpGetDeviceStatusZ5() )
//   indicates that there is an NRP-Z5, this function supplies
//   information about the connected devices at its ports A...D
//   (using 'index' = 0...3)
//   The function returns
//     NRP_SUCCESS                 if an NRP-Z5 was found and
//                                 'index' is in the range [0..3]
//     NRP_ERROR_DESCRIPTOR_INDEX  if no NRP-Z5 is connected or
//                                 'index' is not within [0..3]
long _NRP_FUNC NrpGetDeviceInfoZ5( long index, char *cpName, char *cpType, char *cpSerial, long *pIsConnected );

// NrpOpenSensorOnZ5()
//   Open a dedicated sensor on Port 'A', 'B', 'C' or 'D' of an NRP-Z5
//   The function returns
//     NRP_SUCCESS                 if an NRP-Z5 was found and 'cPort' is in
//                                 the range ['A'..'D'] and a power sensor is
//                                 physically connected to the selected channel
//                                 and could have been opened
//                                 The variable pointed to by 'pSession' then
//                                 contains the session ID of the opened sensor
//     NRP_ERROR_INVALID_RESOURCE  if at least one of the above pre-requisites
//                                 is not met
long _NRP_FUNC NrpOpenSensorOnZ5( char cPort, NRP_SESSION *pSession );


#ifdef __cplusplus
}
#endif

#endif //__NRPCONTROL_H__
