// This is an auto-updated file when a command is added to tlv2p0 using cmdRspDictGenSrc.exe
// DON'T manually modify the file but use cmdRspDictGenSrc.exe.

#ifndef _TLV2_API_H_
#define _TLV2_API_H_


#ifdef _WINDOWS
    #ifdef TLV2P0_EXPORTS
        #define TLV2_API __declspec(dllexport)
    #else
        #define TLV2_API __declspec(dllimport)
    #endif //TLV2P0_EXPORTS
#else
    #define TLV2_API
#endif //#ifdef _WINDOWS

/*
 * Exposed APIs
 *     A_BOOL createCmdRsp(A_UINT32 cmdCode, int numArgs, ...);
 *   
 *     numArgs: the number of arguments following.
 *
 *     Singular format:
 *         parmCode(4B) parmRand(4B) parmValue(4B)
 *
 *     All data types are promoted either to A_UINT32 or A_INT32, depending on sign
 *
 *     Array format:
 *         parmCode(4B) parmRand(4B) numElem(4B) beginPos(4B) value1(4B) ... value(numElem-1)(4B)
 *
 *     beginPos starts from 0
 *
 * Examples
 *     createCmdRsp(CMD_TX, 4, PARM_FREQ, 5320, PARM_CHAINMASK, 0x7); 
 *     
 *     An array example, which sends tx gain table entries, 12, 19, 24
 *         A_UINT8 txGainTbl[32]={0, 6, 12, 19, 24, ....};
 *         createCmdRsp(CMD_TX, 6, PARM_FREQ, 2412, PARM_TXGAINTBL, 3, 2, &(txGainTble[0])); 
 *
 *
 */
TLV2_API TESTFLOW_CMD_STREAM_V2 *createCmdRsp(A_UINT32 cmdCode, int numArgs, ...);
TLV2_API A_BOOL addTLV2p0BinCmdParser(void);
TLV2_API A_BOOL addTLV2p0Encoder(void); 
TLV2_API A_BOOL cmdStreamParsingAndProcessing(A_UINT8 *cmdStreamBuf, A_UINT32 cmdStreamLen, A_UINT8 **pPayload, A_UINT16 *payloadLen); 
TLV2_API void tlv2CreateCmdHeader(A_UINT32 cmdCode);
TLV2_API void tlv2AddParms(int numArgs, ...);
TLV2_API TESTFLOW_CMD_STREAM_V2 *tlv2CompleteCmdRsp();

#include "cmdTPCCALHandler.h"
TLV2_API void registerTPCCALHandler(TPCCAL_OP_FUNC fp);
TLV2_API void registerTPCCALRSPHandler(TPCCALRSP_OP_FUNC fp);

#include "cmdTPCCALPWRHandler.h"
TLV2_API void registerTPCCALPWRHandler(TPCCALPWR_OP_FUNC fp);
TLV2_API void registerTPCCALDATAHandler(TPCCALDATA_OP_FUNC fp);

#include "cmdRxGainCalHandler.h"
TLV2_API void registerRXGAINCALHandler(RXGAINCAL_OP_FUNC fp);
TLV2_API void registerRXGAINCAL_SIGL_DONEHandler(RXGAINCAL_SIGL_DONE_OP_FUNC fp);

#include "cmdRegReadHandler.h"
TLV2_API void registerREGREADHandler(REGREAD_OP_FUNC fp);
TLV2_API void registerREGREADRSPHandler(REGREADRSP_OP_FUNC fp);

#include "cmdRegWriteHandler.h"
TLV2_API void registerREGWRITEHandler(REGWRITE_OP_FUNC fp);
TLV2_API void registerREGWRITERSPHandler(REGWRITERSP_OP_FUNC fp);

#include "cmdBasicRspHandler.h"
TLV2_API void registerBASICRSPHandler(BASICRSP_OP_FUNC fp);

#include "cmdTxHandler.h"
TLV2_API void registerTXHandler(TX_OP_FUNC fp);

#include "cmdTxStatusHandler.h"
TLV2_API void registerTXSTATUSHandler(TXSTATUS_OP_FUNC fp);
TLV2_API void registerTXSTATUSRSPHandler(TXSTATUSRSP_OP_FUNC fp);

#include "cmdRxHandler.h"
TLV2_API void registerRXHandler(RX_OP_FUNC fp);

#include "cmdRxStatusHandler.h"
TLV2_API void registerRXSTATUSHandler(RXSTATUS_OP_FUNC fp);
TLV2_API void registerRXSTATUSRSPHandler(RXSTATUSRSP_OP_FUNC fp);

#include "cmdHwCalHandler.h"
TLV2_API void registerHWCALHandler(HWCAL_OP_FUNC fp);

TLV2_API void registerRXRSPHandler(RXRSP_OP_FUNC fp);

#include "cmdXtalCalHandler.h"
TLV2_API void registerXTALCALPROCHandler(XTALCALPROC_OP_FUNC fp);
TLV2_API void registerXTALCALPROCRSPHandler(XTALCALPROCRSP_OP_FUNC fp);

#include "cmdCustOtpSpaceRead.h"
TLV2_API void registerREADCUSTOTPSPACEHandler(READCUSTOTPSPACE_OP_FUNC fp);
TLV2_API void registerREADCUSTOTPSPACERSPHandler(READCUSTOTPSPACERSP_OP_FUNC fp);

#include "cmdCustOtpSpaceWrite.h"
TLV2_API void registerWRITECUSTOTPSPACEHandler(WRITECUSTOTPSPACE_OP_FUNC fp);
TLV2_API void registerWRITECUSTOTPSPACERSPHandler(WRITECUSTOTPSPACERSP_OP_FUNC fp);

#include "cmdCustOtpSpaceGetSize.h"
TLV2_API void registerGETCUSTOTPSIZEHandler(GETCUSTOTPSIZE_OP_FUNC fp);
TLV2_API void registerGETCUSTOTPSIZERSPHandler(GETCUSTOTPSIZERSP_OP_FUNC fp);

#include "cmdDPDComplete.h"
TLV2_API void registerGETDPDCOMPLETEHandler(GETDPDCOMPLETE_OP_FUNC fp);
TLV2_API void registerGETDPDCOMPLETERSPHandler(GETDPDCOMPLETERSP_OP_FUNC fp);

#include "cmdGetTgtPwr.h"
TLV2_API void registerGETTGTPWRHandler(GETTGTPWR_OP_FUNC fp);
TLV2_API void registerGETTGTPWRRSPHandler(GETTGTPWRRSP_OP_FUNC fp);

#include "cmdSetPCIEConfigParam.h"
TLV2_API void registerSETPCIECONFIGPARAMSHandler(SETPCIECONFIGPARAMS_OP_FUNC fp);
TLV2_API void registerSETPCIECONFIGPARAMSRSPHandler(SETPCIECONFIGPARAMSRSP_OP_FUNC fp);

#include "cmdCommitOtpStream.h"
TLV2_API void registerCOMMITOTPSTREAMHandler(COMMITOTPSTREAM_OP_FUNC fp);
TLV2_API void registerCOMMITOTPSTREAMRSPHandler(COMMITOTPSTREAMRSP_OP_FUNC fp);

#include "cmdSetRegDmn.h"
TLV2_API void registerSETREGDMNHandler(SETREGDMN_OP_FUNC fp);
TLV2_API void registerSETREGDMNRSPHandler(SETREGDMNRSP_OP_FUNC fp);

#endif // _TLV2_API_H_ - This is the marker line. Don't modify or delete this line 
