/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/

#include "global.h"

enum CmdCode
{
    version = 0x0,
    esn = 0x1,
    status = 0xC,
    nvItemRead = 0x26,
    phoneState = 0x3F,
    subSystemDispatch = 0x4B,
    featureQuery = 0x51,
    embeddedFileOperation = 0x59,
    diagProtLoopback = 0x7B,
    extendedBuildID = 0x7C,
    logMask = 0x73
};

enum SubsystemID
{
    FTMSubsystem = 0x0B
};

static BYTE verResponseMsg[] = {                                    
                                   0x0,  //(BYTE)CmdCode.version,                        //CMD_CODE (0)
								   0x4F,0x63,0x74,0x20,0x30,0x38, 
                                   0x20,0x32,0x30,0x31,0x31,       //COMP_DATE (Compilation date)
                                   0x31,0x30,0x3A,0x30,0x37,0x3A,
                                   0x30,0x30,                      //COMP_TIME
                                   0x4F,0x63,0x74,0x20,0x30,0x38,
                                   0x20,0x32,0x30,0x31,0x31,       //REL_DATE (Compilation date)
                                   0x31,0x30,0x3A,0x30,0x37,0x3A,
                                   0x30,0x30,                      //REL_TIME
                                   0x41,0x41,0x41,0x31,0x30,
                                   0x30,0x30,0x30,                 //VER_DIR
                                   0,                              //SCM
                                   1,                              //MOB_CAL_REV
                                   255,                            //MOB_MODEL
                                   1,0,                            //MOB_FIRM_REV
                                   0,                              //SLOT_CYCLE_INDEX
                                   1, 0                            //MSM_VER
                               };

	 						   
static BYTE logMaskResponseFirstMsg[] = { 
                                            0x73,00, 00, 00, 01, 00, 00, 00,00 ,00 ,00, 00, 00, 00, 00,
                                            00,0xA6,05,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x10,0x09,
                                            00, 00, 0x20, 04, 00 ,00,00, 00, 00, 00, 0x56,0x0B,00,00,00,00,
                                            00,00,00,00,00,00,0x8A,03,00,00,01,02, 00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00 
                                        };

static BYTE logMaskResponseSecondMsg[] = { 
              0x73,00,00,00,04,00,00,00,00,00,00,00,01,00,00,00,0xA6,05,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,
              00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,0x10,00,00, 00, 00, 00, 00, 00, 00, 00, 00, 00,00 ,00 ,00, 00, 00, 00, 00, 00, 00, 00,
              00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
              0x40, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
              00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 
                                         };
static BYTE esnResponseMsg[] = {                
                0x1,  //(BYTE)CmdCode.esn,                            //CMD_CODE (1)
				0xEF,0xBE,0xAD,0xDE             //ESN
                               };

static BYTE statusResponseMsg[] = {                     
                    0xC,  //(BYTE)CmdCode.status,                 //CMD_CODE (12) 1
					0, 0, 0,                //RESERVED 3
                    0xce, 0xfa, 0xbe, 0xba, //ESN 4
                    6, 0,                   //RF_MODE 2
                    0, 0, 0, 0,             //MIN1 (Analog) 4
                    0, 0, 0, 0,             //MIN1 (CDMA) 4
                    0, 0,                   //MIN2 (Analog) 2
                    0, 0,                   //MIN2 (CDMA) 2
                    0,                      //RESERVED 1
                    0, 0,                   //CDMA_RX_STATE 2
                    0xff,                   //CDMA_GOOD_FRAMES 1
                    0, 0,                   //ANALOG_CORRECTED_FRAMES 2
                    0, 0,                   //ANALOG_BAD_FRAMES 2
                    0, 0,                   //ANALOG_WORD_SYNCS 2 
                    0, 0,                   //ENTRY_REASON 2
                    0, 0,                   //CURRENT_CHAN 2
                    0,                      //CDMA_CODE_CHAN 1
                    0, 0,                   //PILOT_BASE 2
                    0, 0,                   //SID 2
                    0, 0,                   //NID 2
                    0, 0,                   //LOCAID 2
                    0, 0,                   //RSSI 2
                    0                       //POWER  1  
                                  };

static BYTE nvItemReadResponseMsg[] = {
                        0x26,  //(BYTE)CmdCode.nvItemRead,             // CMD_CODE (0x26)
						71, 0,                  //NV_ITEM
                        0x42,0x54,0x2E,        // ITEM_DATA
                        0x44,0x49,0x41,0x47,0x2E,
                        0x42,0x52,0x49,0x44,0x47,0x45,
                        0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0                     // STATUS
                                      };
static BYTE phoneStateResponseMsg[] = { 
                0x3F,  //(BYTE)CmdCode.phoneState,                     //CMD_CODE (0x3F)
                0x1,                            //PHONE_STATE
                0x0,0x0                         //EVENT_COUNT
                                      };

BYTE featureQueryRespMsg[] = {
                      0x51,  //(BYTE)CmdCode.featureQuery, 
                      0,0,
                      0
                             };
static BYTE extendedBuildIDRespMsg[] = {
                        0x7C,  //(BYTE)CmdCode.extendedBuildID,        // CMD_CODE (0x7C)
                        2,                      // Version
                        0,0,                    // Reserved
                        0xE8,0x3,0,0,           // MSM Revision (assigned to 1000)
                        0x10,0x27,0,0,             // Manufacturer model number (assigned to 10000)
                        0x31,0,                 // Softwareversion (ascii value of 1)
                        0x42,0x54,0x2E,         //BT.
                        0x44,0x49,0x41,0x47,0x2E,//DIAG.
                        0x42,0x52,0x49,0x44,0x47,0x45, //BRIDGE
                        0x0                // Model string (BT.DIAG.BRIDGE)
                                       };
