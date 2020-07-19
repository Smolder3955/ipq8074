/*
Copyright (c) 2013, 2017 Qualcomm Technologies, Inc.

All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.
*/

/* tlvCmd_if_Qdart.c - Interface to DevdrvIf.DLL ( DevdrvIf.DLL access device driver ) */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <errno.h>
#include "Qcmbr.h"
#include <unistd.h>
#define FLASH_SECTOR_SIZE 0x10000  // Only use in NOR flash.
#define CALDATA_SIZE 12500
#define QC9887_DEVICE_ID 0x50
#define QC9888_DEVICE_ID 0x3c

static char FlashCalData[CALDATA_SIZE];
static int BoardDataSize;
static char *interface=NULL;
static int pcie=0;
static int instance=0;
static int BDCapture=0;
static int start_index;

// holds the cmd replies sent over channel
static CMD_REPLY cmdReply;
static A_BOOL cmdInitCalled = FALSE;

/* CALDATA FILE STRUCT
*
* cal_check: unsigned int, for caldata valid check
* cal_start[pcie0], unsigned int, for start address of pcie0 caldata
* cal_length[pcie0], unsigned int, for length of pcie0 caldata
* cal_start[pcie1], unsigned int, for start address of pcie1 caldata
* cal_length[pcie1], unsigned int, for length of pcie1 caldata
*
*/

struct caldata_attributes
{
    unsigned int cal_index;  /* file index, caldata_idx */
    unsigned int cal_length; /* number of bytes */
};

#define CALDATA_IN_FILE_PCIE_NUMBER 2
#define CALDATA_IN_FILE_CHECK_PCIE0 0x01
#define CALDATA_IN_FILE_CHECK_PCIE1 0x02
#define CALDATA_IN_FILE_HDR_INIT_FLAG  0x10

struct caldata_table_header
{
    unsigned int cal_check; /* bit0 for pcie 0, bit1 for pcie 1, bit 4 for file initializing flag
                                           0 indicates invalid data,
                                           1 indicates valid data */

    struct caldata_attributes caldata_attr[CALDATA_IN_FILE_PCIE_NUMBER];  /* index 0 for the caldata of the card in pcie 0,
                                                                     index 1 for the caldata of the card in pcie 1,
                                                                     currently only design for 2 RFs */
};

struct caldata_table_header caldata_header;

/**************************************************************************
* receiveCmdReturn - Callback function for calling cmd_init().
*       Note: We are keeping the calling convention.
*       Note: cmd_init() is a library function from DevdrvIf.DLL.
*       Note: Qcmbr does not ( need to ) do anything to the data or care
*             what is in the data.
*/
void receiveCmdReturn(void *buf)
{
    DbgPrintf("CallBack-receiveCmdReturn bufAddr[%8.8X]\n",(unsigned int)buf);
    if ( buf == NULL )
    {
    }
    // Dummy call back function
}

/**************************************************************************
* artSendCmd2 - This function sends the TLV command passing from host (caller)
*               to the device interface function.
*
*/

A_BOOL artSendCmd2( A_UINT8 *pCmdStruct, A_UINT32 cmdSize, unsigned char* responseCharBuf, unsigned int *responseSize, unsigned int devid )
{
    int     errorNo;
    char buf[2048 + 8];
    extern void receiveCmdReturn1(void *buf);
    extern void DispHexString(A_UINT8 *pCmdStruct,A_UINT32 cmdSize);
    DispHexString(pCmdStruct,cmdSize);

    memset(buf, 0, sizeof(buf));

    if (cmdInitCalled == FALSE)
    {
	errorNo = cmd_init(interface,receiveCmdReturn1);
	cmdInitCalled = TRUE;
    }

    memcpy(&buf[8],pCmdStruct,cmdSize);
    DbgPrintf( "arSendCmd2->cmd_send2 RspLen [%d]\n", *responseSize );
    cmd_send2( buf, cmdSize, responseCharBuf, responseSize, devid);
    errorNo = (A_UINT16) (cmdReply.status & COMMS_ERR_MASK) >> COMMS_ERR_SHIFT;
    if (errorNo == COMMS_ERR_MDK_ERROR)
    {
        DbgPrintf("Error: COMMS error MDK error for command DONT_CARE\n" );
        return TRUE;
    }

    // check for a bad status in the command reply
    if (errorNo != CMD_OK)
    {
        DbgPrintf("Error: Bad return status (%d) in client command DONT_CARE response!\n", errorNo);
        return FALSE;
    }

    return TRUE;
}
int setInterface(char *Interface)
{
	printf("setting interface to %s\n",Interface);
	interface = Interface;
	return 0;
}
int setPcie(int Pcie)
{
    printf("setting pcie to %d\n",Pcie);
    pcie = Pcie;
    return 0;
}

int setInstance(int Instance)
{
    printf("setting instance to %d\n",Instance);
    instance = Instance;
    return 0;
}
int setBordDataCaptureFlag (int flag)
{
    DbgPrintf("setting BDCaptureFlag to %d\n",flag);
    BDCapture=flag;
    if (flag==1){
        //clear BoardDataSize
        BoardDataSize=0;
        start_index=-1;
    }
    return 0;
}


unsigned int fileUpdate()
{
    int caldata_hdr_fd = -1;
    int caldata_fd = -1;
    unsigned int counter = 0;

    printf("Save calibration data on host file system\n");

    memset(&caldata_header, 0, sizeof(caldata_header));

    /* open caldata header file */
    caldata_hdr_fd = open(CALDATA_HEADER_PATH, O_RDWR|O_CREAT,0);
    if (-1 == caldata_hdr_fd)
    {
        DbgPrintf("Failed to open file %s\n", CALDATA_HEADER_PATH);
        return -1;
    }

    /* read caldata header, or initialize caldata file */
    counter = read(caldata_hdr_fd, &caldata_header, sizeof(caldata_header));
    if (0 == (CALDATA_IN_FILE_HDR_INIT_FLAG & caldata_header.cal_check))
    {
        DbgPrintf("%s is new or header is invalid, whatever initialize caldata file\n", CALDATA_HEADER_PATH);

        counter = write(caldata_hdr_fd, &caldata_header, sizeof(caldata_header));
        if (sizeof(caldata_header) != counter)
        {
            DbgPrintf("Failed to initialize header.\n");
            close(caldata_hdr_fd);
            return -1;
        }

        caldata_header.cal_check |= CALDATA_IN_FILE_HDR_INIT_FLAG;

    }

    /* caldata file create */
    if (0 == instance)
    {
        caldata_header.cal_check &= (~CALDATA_IN_FILE_CHECK_PCIE0);
	caldata_fd = open(CALDATA0_FILE_PATH, O_RDWR|O_CREAT,0);
    }
    else if (1 == instance)
    {
        caldata_header.cal_check &= (~CALDATA_IN_FILE_CHECK_PCIE1);
        caldata_fd = open(CALDATA1_FILE_PATH, O_RDWR|O_CREAT,0);
    }
    else
    {
        DbgPrintf("invalid pcie slot number, please confirm how many PCIe devices you configured.\n");
        close(caldata_hdr_fd);
        return -1;
    }

    if(-1 == caldata_fd)
    {
        DbgPrintf("Failed to open caldata file.\n");
        close(caldata_hdr_fd);
        return -1;
    }

    ftruncate(caldata_fd, 0);
    lseek(caldata_fd, 0, SEEK_SET);

    counter = write(caldata_fd, FlashCalData, BoardDataSize);
    if(BoardDataSize != counter)
    {
        DbgPrintf("Failed to write pcie%d caldata\n", pcie);
        close(caldata_hdr_fd);
        close(caldata_fd);
        return -1;
    }

    close(caldata_fd);

    /* caldata header update */
    caldata_header.caldata_attr[pcie].cal_index = pcie;
    caldata_header.caldata_attr[pcie].cal_length = counter;

    if(0 == instance)
    {
        caldata_header.cal_check |= CALDATA_IN_FILE_CHECK_PCIE0;
    }

    if(1 == instance)
    {
        caldata_header.cal_check |= CALDATA_IN_FILE_CHECK_PCIE1;
    }

    lseek(caldata_hdr_fd, 0, SEEK_SET);
    counter = write(caldata_hdr_fd, &caldata_header, sizeof(caldata_header));
    if (sizeof(caldata_header) != counter)
    {
        DbgPrintf("Failed to update header\n");
        close(caldata_hdr_fd);
        return -1;
    }

    close(caldata_hdr_fd);

    return 0;

}

#ifdef SUPPORT_VPD
static A_BOOL WriteBoardDataToVPD(char *pEeprom, unsigned int eepromSize)
{
    unsigned int *pData;
    unsigned int address;
    char fileName[256];
    char fullFileName[256];
    FILE *fStream;
    A_BOOL rc = FALSE;

    DbgPrintf(" Entered into WriteBoardDataToVPD function\n");
    // save to text file.
    snprintf(fullFileName, sizeof(fullFileName), "/tmp/Wifi%dCal.txt",instance);
    if( (fStream = fopen(fullFileName, "w")) == NULL)
    {
        DbgPrintf("Could not open calDataFile %s\n", fullFileName);
        return FALSE;
    }
    pData = (unsigned int *)pEeprom;
    for (address = 0; address < eepromSize/4;  address++)
    {
        fprintf(fStream, "Addr0x%08X, value=0x%08X\n", address*4, pData[address]);
    }
    fclose(fStream);

    // Write to a binary eeprom
    snprintf(fullFileName, sizeof(fullFileName), "/tmp/Wifi%dCal.bin",instance);
    if( (fStream = fopen(fullFileName, "wb")) == NULL)
    {
        DbgPrintf("Could not open calDataFile %s to write\n", fullFileName);
        return FALSE;
    }

    if (eepromSize != fwrite(pEeprom, sizeof(char), eepromSize, fStream))
    {
        DbgPrintf("Error writing to %s\n", fullFileName);
        rc = FALSE;
    }
    else
    {
        DbgPrintf("... written to %s\n", fullFileName);
        rc = TRUE;
    }
    fclose(fStream);

    if (rc == TRUE)
    {
        // call VPD command.
        snprintf(fullFileName, sizeof(fullFileName), "base64 /tmp/Wifi%dCal.bin > /tmp/wifi%d.base64.txt",instance, instance);
        system (fullFileName);
        snprintf(fullFileName, sizeof(fullFileName), "vpd -S wifi_base64_calibration%d=/tmp/wifi%d.base64.txt",instance, instance);
        system (fullFileName);
    }
    return rc;
}
#endif // #ifdef SUPPORT_VPD

#define MAX_EEPROM_SIZE 0x4000
#define FLASH_BASE_CALDATA_OFFSET_RADIO_0 0x1000
#define FLASH_BASE_CALDATA_OFFSET_RADIO_1 0x5000
#define FLASH_BASE_CALDATA_OFFSET_RADIO_2 0x9000

void flashWrite(int flashSupport, unsigned int devid)
{
#ifdef SUPPORT_VPD

    DbgPrintf("WriteBoardDataToVPD start.\n");
    WriteBoardDataToVPD(FlashCalData, BoardDataSize);
    DbgPrintf("Caldata written into VDP successfully ! instance=%d, Size=%d\n", instance, BoardDataSize);
    return;

#else
    int fd;
    int offset;
    mtd_info_t mtdInfo;           // MTD structure for NOR flash
    erase_info_t eraseInfo;       // erase block structure for NOR flash
    int norSectorStart;
    int norSectorEnd;
    int norTotalSize;
    int norSectorOffset;
    char *norSectorData;
    int norSector;
    int norBufIndex;

    if((fd = open("/dev/caldata", O_RDWR)) < 0) {
        DbgPrintf("Could not open flash. Returning without write\n");
        return;
    }
    if((devid == QC9887_DEVICE_ID) || (devid == QC9888_DEVICE_ID)) {
        Sleep(3);
    }
    if(flashSupport) {
        // NOR flash needs sector etase before writing into a sector
        if (ioctl(fd, MEMGETINFO, &mtdInfo) == 0){   // get /dev/caldata MTD info; return 0 for NOR flash and -1 for NAND
            // Take backup of norflash partition.
            printf("NOR flash mtdInfo type=%d, size=0x%X, erasesize = 0x%X\n", mtdInfo.type, mtdInfo.size, mtdInfo.erasesize);
            if ( FLASH_SECTOR_SIZE != mtdInfo.erasesize )
            {
                printf("NOR flash sector is not 64KB ! Not support now.\n");
                return;
            }

            offset=instance*MAX_EEPROM_SIZE+FLASH_BASE_CALDATA_OFFSET_RADIO_0;
            norSectorStart = offset/FLASH_SECTOR_SIZE;
            norSectorEnd = (offset + MAX_EEPROM_SIZE)/FLASH_SECTOR_SIZE;
            norSectorOffset = (norSectorStart*FLASH_SECTOR_SIZE);
            offset = offset - norSectorOffset;
            norTotalSize = (norSectorEnd > norSectorStart) ? (2*FLASH_SECTOR_SIZE) : FLASH_SECTOR_SIZE;
            printf("NOR flash norSectorStart = %d, norSectorEnd=%d, norTotalSize=0x%X.\n", norSectorStart, norSectorEnd, norTotalSize);
            printf("NOR flash norSectorOffset=0x%X, offset=0x%X.\n", norSectorOffset, offset);

            norSectorData = (char *) malloc(norTotalSize);
            if (!norSectorData)
            {
                printf("NOR malloc fail.\n");
                return;
            }

            lseek(fd, norSectorOffset, SEEK_SET);
            if (read(fd, norSectorData, norTotalSize) < 1) {
                printf("backup NOR flash fail.\n");
                free(norSectorData);
                return;
            }

            printf("NOR flash backup to RAM success.\n");
            // clear RAM data to 0xFF.
            memset(&norSectorData[offset], 0xFF, MAX_EEPROM_SIZE);
            // overwrite with new caldata.
            memcpy(&norSectorData[offset], FlashCalData, BoardDataSize);

            for (norSector=norSectorStart, norBufIndex=0; norSector<=norSectorEnd; norSector++, norBufIndex++)
            {
                printf("NOR flash erase sector = %d.\n", norSector);
                eraseInfo.start = norSector*FLASH_SECTOR_SIZE;
                eraseInfo.length = FLASH_SECTOR_SIZE;
                ioctl(fd, MEMUNLOCK, &eraseInfo);
                if (ioctl(fd, MEMERASE, &eraseInfo) < 0) {
                    printf("NOR flash erase error.. Returning\n");
                    free(norSectorData);
                    return;
                }

                printf("NOR flash write sector = %d, norBufIndex = %d.\n", norSector, norBufIndex);
                lseek(fd, (norSector*FLASH_SECTOR_SIZE), SEEK_SET);
                if (write(fd, &norSectorData[norBufIndex*FLASH_SECTOR_SIZE], FLASH_SECTOR_SIZE) < 1) {
                    printf("NOR flash write error. Returning\n");
                    free(norSectorData);
                    return;
                }
            }

            free(norSectorData);
            close(fd);
            printf("NOR flash write ok ! norSectorOffset=0x%X, offset=0x%X, Size=%d\n", norSectorOffset, offset, BoardDataSize);
            return;
        }

        // instance0-> radio 0-> offset 0x1000; instance1-> radio 1 -> offset 0x5000 ; instance2-> radio 2 -> offset 0x9000
        // pcie value cannot indicate wifi index, because board can have an onboard radio (ahb bus shared) where -pcie option cannot be used

        offset=instance*MAX_EEPROM_SIZE+FLASH_BASE_CALDATA_OFFSET_RADIO_0;
        lseek(fd, offset, SEEK_SET);
        printf("NAND flash write instance=%d.\n", instance);
        if (write(fd, FlashCalData, BoardDataSize) < 1) {
            printf("NAND flash write error. Returning\n");
            close(fd);
            return;
        }

        close(fd);
        printf("Caldata written into Flash successfully @ offset %0x Size %d\n",offset,BoardDataSize);
    }
#endif /* #ifdef SUPPORT_VPD */
}

int WriteBtMacToFlash(int flashsupport, unsigned char *mac, unsigned int len)
{
    int fd;
    int offset;
    erase_info_t eraseInfo;
    mtd_info_t mtdInfo;
    int SectorStart;
    int SectorEnd;
    int TotalSize;
    int SectorOffset;
    char *SectorData;
    //int Sector;
    int i;

    if((fd = open("/dev/caldata", O_RDWR)) < 0) {
        perror("Could not open flash. Returning without write\n");
        return -1;
    }
    printf("\nNumber of bytes = %d",len);
    if(flashsupport) {
                offset = 0x40;
                SectorStart = offset/FLASH_SECTOR_SIZE;
		printf("\nSector start = %d\n",SectorStart);
                SectorEnd = (offset + 0x40)/FLASH_SECTOR_SIZE;
		printf("\nSector end = %d\n",SectorEnd);
                SectorOffset = (SectorStart*FLASH_SECTOR_SIZE);
		printf("\nSectorOffset = %d\n",SectorOffset);
                offset = offset - SectorOffset;
		printf("\nfinal offset = %d\n",offset);
                TotalSize = FLASH_SECTOR_SIZE;
		printf("\nTotal size = %d\n",TotalSize);

                SectorData = (char *) malloc(TotalSize);
                if (!SectorData)
                {
                        printf("malloc fail.\n");
                        return -1;
                }
		printf("\nSeek pointer to %d\n",SectorOffset);
                lseek(fd, SectorOffset, SEEK_SET);
                if (read(fd, SectorData, TotalSize) < 1) {
                        printf("backup flash fail.\n");
                        free(SectorData);
                        return -1;
                }
                printf("flash backup to RAM success.\n");

		//clear RAM content with 0x00 from the bt offset
		//memset(&SectorData[offset], 0x00, 0x40);
		//aply the bt data to buffer
		for(i=0;i<len;i++) {
			SectorData[offset+i]=mac[i];
			printf("mac = 0x%x SectorData = 0x%x",mac[i],SectorData[offset+i]);
		}
		printf("\n");
		if (ioctl(fd, MEMGETINFO, &mtdInfo) == 0){
			if ( FLASH_SECTOR_SIZE != mtdInfo.erasesize ) {
				printf("NOR flash sector is not 64KB ! Not support now.\n");
				return -1;
			}
			eraseInfo.start = SectorStart;
			eraseInfo.length = FLASH_SECTOR_SIZE;
			ioctl(fd, MEMUNLOCK, &eraseInfo);
			if (ioctl(fd, MEMERASE, &eraseInfo) < 0) {
				printf("flash erase error.. Returning\n");
				free(SectorData);
				return -1;
			}
			lseek(fd, SectorStart, SEEK_SET);
			if (write(fd, &SectorData[SectorStart], FLASH_SECTOR_SIZE) < 1) {
				printf("flash write error. Returning\n");
				free(SectorData);
				return -1;
			}
			free(SectorData);
			close(fd);
			printf("flash write ok ! SectorOffset=0x%X, offset=0x%X\n", SectorOffset, offset);
			return 1;
		}
		lseek(fd, SectorStart, SEEK_SET);
		if (write(fd, &SectorData[SectorStart], FLASH_SECTOR_SIZE) < 1) {
			printf("flash write error. Returning\n");
			free(SectorData);
			return -1;
		}
		free(SectorData);
		close(fd);
		printf("flash write ok ! SectorOffset=0x%X, offset=0x%X\n", SectorOffset, offset);
		return 1;
   }
   return -1;
}

#define BD_BLOCK_SIZE 256
#define QC98XX_BLOCK_SIZE 512

void BoardDataCapture(unsigned char * respdata,unsigned int devid)
{

    if (BDCapture==0){
        return;
    }else if (BDCapture==1){
        if ((devid == 0x50) || (devid == 0x3c)) {
            int dataSize=(respdata[169]<<8) + respdata[168];
            BoardDataSize+=dataSize;
            start_index = start_index + 1;
            DbgPrintf("Capturing Caldata for QC98xx by Qcmbr :: Size %d\n",dataSize);
            DbgPrintf("FlashCaldata Address start : %d \n",(start_index*QC98XX_BLOCK_SIZE));
            memcpy(&(FlashCalData[start_index*QC98XX_BLOCK_SIZE]),(unsigned char *)&(respdata[204]), dataSize);
        } else {
            int dataSize=(respdata[105]<<8) + respdata[104];
            BoardDataSize+=dataSize;
            DbgPrintf("Capturing Caldata by Qcmbr:: Block number %d Size %d\n",respdata[107],dataSize);
            memcpy(&(FlashCalData[respdata[107]*BD_BLOCK_SIZE]),(unsigned char *)&(respdata[108]), dataSize);
        }

    }
}
