
/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/  

#include <Windows.h>
//#include "anwiioctl.h"

#define QCA_WLAN_SUPPORT_ART2   1

void InitDevMsg(HANDLE hDevArrivalEvent);
void UninitDevMsg();
DWORD WaitForDeviceReady();
BOOL DeviceExists();

DWORD OpenArt2Device( PHANDLE OutputHandle );
DWORD CloseArt2Device( HANDLE DevHandle );

DWORD 
Art2DeviceIoctl(	
	HANDLE 	DevHandle, 
	DWORD 	IoctlCode, 
	PUCHAR 	CustomData, 
	ULONG 	CustomDataLength,
	PUCHAR 	OutputBuffer,
	ULONG 	OutputBufferLength,
	PULONG 	BytesRet
	);

BOOLEAN
OpenBusInterface (
    __in	HDEVINFO                    HardwareDeviceInfo,
    __in    PSP_DEVICE_INTERFACE_DATA   DeviceInterfaceData,
	__out	PHANDLE						OutputHandle
    );

#define DEVDRVIF_API 

// device interface id {A35AB82A-FDAF-4680-8595-4E2A4ACBF46E}
DEFINE_GUID(ART2_DEV_INF_GUID, 0xA35AB82A, 0xFDAF, 0x4680, 0x85, 0x95, 0x4E, 0x2A, 0x4A,0xCB, 0xF4, 0x6E);

//GUID_DEVINTERFACE_NET
DEFINE_GUID(NetGuid,0xCAC88484,0x7515,0x4C03, 0x82, 0xE6, 0x71, 0xA8, 0x7A, 0xBA, 0xC3, 0x61);
 



//
// Device type           -- in the "User Defined" range."
//
#define ART2_IOCTL_TYPE_START 	50000


//
// The IOCTL function codes from 0x800 to 0xFFF are for END USER use.
//

/* ART2 WMI test command */
#define ART2_DEV_IOCTL_REQUEST \
    CTL_CODE( ART2_IOCTL_TYPE_START, 0x901, METHOD_OUT_DIRECT, FILE_ANY_ACCESS )


#define ART2_REQ_ID_UTF_CMD 		0x00000001
#define ART2_REQ_ID_UTF_RSP			0x00000002
#define ART2_REQ_ID_WMI_CMD 		0x00000003
#define ART2_REQ_ID_WMI_RSP			0x00000004
#define ART2_REQ_ID_READ_MEM 		0x00000005
#define ART2_REQ_ID_WRITE_MEM		0x00000006
#define ART2_REQ_ID_READ_REG		0x00000007
#define ART2_REQ_ID_WRITE_REG 		0x00000008
#define ART2_REQ_ID_DL_FW 			0x00000009
#define ART2_REQ_ID_BMI_DONE		0x0000000A
#define ART2_REQ_ID_READ_CFG		0x0000000B
#define ART2_REQ_ID_WRITE_CFG		0x0000000C
#define ART2_REQ_ID_GET_INIT_STAT	0x0000000D



#define ART2_REQ_ID_OTHER			0x000000FF


//
// Firmware downloading flags
//
#define ART2_FW_DL_BOARD_DATA		0x00000001
#define ART2_FW_DL_ATH_WLAN			0x00000002
#define ART2_FW_DL_PATCH			0x00000004
#define ART2_FW_DL_UTF				0x00000008


typedef struct _ART2_IO_REQ_HEADER
{
	ULONG 	RequestId;
	ULONG 	InputContentLength;

}ART2_IO_REQ_HEADER, *PART2_IO_REQ_HEADER;
