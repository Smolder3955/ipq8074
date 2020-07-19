// This file is auto-updated when a command is added to the dictionary by running cmdRspDictGenSrc.exe

#include "tlv2Inc.h"
#include "cmdHandlers.h"

// Notes:
// 
// TBD??? "dynamic" loading of the modules?
// Potentially we could allocate enough space for "dynamic" binding of the handlers, modularize them
//
// cmdHandlerTbl
// Note:
//     1. rspGeneration of the RSP should be NULL, otherwise, it will be response for RSP, an infinite loop

CMD_HANDLER_ENTRY CmdHandlerTbl[] = {
    {NULL, NULL, NULL},                       
    {initTPCCALOpParms, TPCCALOp, NULL},
    {initTPCCALRSPOpParms, TPCCALRSPOp, NULL},
    {initTPCCALPWROpParms, TPCCALPWROp, NULL},
    {initTPCCALDATAOpParms, TPCCALDATAOp, NULL},
    {initRXGAINCALOpParms, RXGAINCALOp, NULL},
    {initRXGAINCALRSPOpParms, RXGAINCALRSPOp, NULL},
    {initRXGAINCAL_SIGL_DONEOpParms, RXGAINCAL_SIGL_DONEOp, NULL},
    {initRXGAINCALRSP_DONEOpParms, RXGAINCALRSP_DONEOp, NULL},
    {initREGREADOpParms, REGREADOp, NULL},
    {initREGREADRSPOpParms, REGREADRSPOp, NULL},
    {initREGWRITEOpParms, REGWRITEOp, NULL},
    {initREGWRITERSPOpParms, REGWRITERSPOp, NULL},
    {initBASICRSPOpParms, BASICRSPOp, NULL},
    {initTXOpParms, TXOp, NULL},
    {initTXSTATUSOpParms, TXSTATUSOp, NULL},
    {initTXSTATUSRSPOpParms, TXSTATUSRSPOp, NULL},
    {initRXOpParms, RXOp, NULL},
    {initRXSTATUSOpParms, RXSTATUSOp, NULL},
    {initRXSTATUSRSPOpParms, RXSTATUSRSPOp, NULL},
    {initHWCALOpParms, HWCALOp, NULL},
    {initRXRSPOpParms, RXRSPOp, NULL},
    {initXTALCALPROCOpParms, XTALCALPROCOp, NULL},
    {initXTALCALPROCRSPOpParms, XTALCALPROCRSPOp, NULL},
    {initREADCUSTOTPSPACEOpParms, READCUSTOTPSPACEOp, NULL},
    {initREADCUSTOTPSPACERSPOpParms, READCUSTOTPSPACERSPOp, NULL},
    {initWRITECUSTOTPSPACEOpParms, WRITECUSTOTPSPACEOp, NULL},
    {initWRITECUSTOTPSPACERSPOpParms, WRITECUSTOTPSPACERSPOp, NULL},
    {initGETCUSTOTPSIZEOpParms, GETCUSTOTPSIZEOp, NULL},
    {initGETCUSTOTPSIZERSPOpParms, GETCUSTOTPSIZERSPOp, NULL},
    {initGETDPDCOMPLETEOpParms, GETDPDCOMPLETEOp, NULL},
    {initGETDPDCOMPLETERSPOpParms, GETDPDCOMPLETERSPOp, NULL},
    {initGETTGTPWROpParms, GETTGTPWROp, NULL},
    {initGETTGTPWRRSPOpParms, GETTGTPWRRSPOp, NULL},
    {initSETPCIECONFIGPARAMSOpParms, SETPCIECONFIGPARAMSOp, NULL},
    {initSETPCIECONFIGPARAMSRSPOpParms, SETPCIECONFIGPARAMSRSPOp, NULL},
    {initCOMMITOTPSTREAMOpParms, COMMITOTPSTREAMOp, NULL},
    {initCOMMITOTPSTREAMRSPOpParms, COMMITOTPSTREAMRSPOp, NULL},
    {initSETREGDMNOpParms, SETREGDMNOp, NULL},
    {initSETREGDMNRSPOpParms, SETREGDMNRSPOp, NULL},
//auto-updated marker - this line should be the last in the table (DON'T REMOVE THIS LINE)	
};

A_UINT32 MaxCmdHandlers = sizeof(CmdHandlerTbl) / sizeof(CMD_HANDLER_ENTRY);


