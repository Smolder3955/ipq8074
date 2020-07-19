// This file is auto-updated when a command is added to the dictionary by running cmdRspDictGenSrc.exe

#if !defined(__CMD_HAHNDLERS_H)
#define __CMD_HAHNDLERS_H

//
// Function prototypes from individual cmd source
//

#include "cmdTPCCALHandler.h"
#include "cmdTPCCALPWRHandler.h"
#include "cmdRxGainCalHandler.h"
#include "cmdRegReadHandler.h"
#include "cmdRegWriteHandler.h"
#include "cmdBasicRspHandler.h"
#include "cmdTxHandler.h"
#include "cmdTxStatusHandler.h"
#include "cmdRxHandler.h"
#include "cmdRxStatusHandler.h"
#include "cmdHwCalHandler.h"
#include "cmdXtalCalHandler.h"
#include "cmdCustOtpSpaceRead.h"
#include "cmdCustOtpSpaceWrite.h"
#include "cmdCustOtpSpaceGetSize.h"
#include "cmdDPDComplete.h"
#include "cmdGetTgtPwr.h"
#include "cmdSetPCIEConfigParam.h"
#include "cmdCommitOtpStream.h"
#include "cmdSetRegDmn.h"
//auto-updated marker - no #include statement after this line (DON'T REMOVE THIS LINE)

// union of all cmd parm structure
typedef struct cmdAllParms {
    union {
        CMD_TPCCAL_PARMS            cmdTPCCALParms;
        CMD_TPCCALRSP_PARMS         rspTPCCALParms;
        CMD_TPCCALPWR_PARMS         cmdTPCCALPWRParms;
        CMD_TPCCALDATA_PARMS        rspTPCCALDATAParms;
        CMD_RXGAINCAL_PARMS		 cmdRXGAINCALParms;
        CMD_RXGAINCALRSP_PARMS		 rspRXGAINCALRSPParms;
        CMD_RXGAINCAL_SIGL_DONE_PARMS	 cmdRXGAINCAL_SIGL_DONEParms;
        CMD_RXGAINCALRSP_DONE_PARMS	 rspRXGAINCALRSP_DONEParms;
        CMD_REGREAD_PARMS		 cmdREGREADParms;
        CMD_REGREADRSP_PARMS		 rspREGREADRSPParms;
        CMD_REGWRITE_PARMS		 cmdREGWRITEParms;
        CMD_REGWRITERSP_PARMS		 rspREGWRITERSPParms;
        CMD_BASICRSP_PARMS		 rspBASICRSPParms;
        CMD_TX_PARMS		 cmdTXParms;
        CMD_TXSTATUS_PARMS		 cmdTXSTATUSParms;
        CMD_TXSTATUSRSP_PARMS		 rspTXSTATUSRSPParms;
        CMD_RX_PARMS		 cmdRXParms;
        CMD_RXSTATUS_PARMS		 cmdRXSTATUSParms;
        CMD_RXSTATUSRSP_PARMS		 rspRXSTATUSRSPParms;
        CMD_HWCAL_PARMS		 cmdHWCALParms;
        CMD_RXRSP_PARMS		 rspRXRSPParms;
        CMD_XTALCALPROC_PARMS		 cmdXTALCALPROCParms;
        CMD_XTALCALPROCRSP_PARMS		 rspXTALCALPROCRSPParms;
        CMD_READCUSTOTPSPACERSP_PARMS		 rspREADCUSTOTPSPACERSPParms;
        CMD_WRITECUSTOTPSPACE_PARMS		 cmdWRITECUSTOTPSPACEParms;
        CMD_WRITECUSTOTPSPACERSP_PARMS		 rspWRITECUSTOTPSPACERSPParms;
        CMD_GETCUSTOTPSIZERSP_PARMS		 rspGETCUSTOTPSIZERSPParms;
        CMD_GETDPDCOMPLETERSP_PARMS		 rspGETDPDCOMPLETERSPParms;
        CMD_GETTGTPWR_PARMS		 cmdGETTGTPWRParms;
        CMD_GETTGTPWRRSP_PARMS		 rspGETTGTPWRRSPParms;
        CMD_SETPCIECONFIGPARAMS_PARMS		 cmdSETPCIECONFIGPARAMSParms;
        CMD_SETREGDMN_PARMS		 cmdSETREGDMNParms;
    } __ATTRIB_PACK cmdParmU;

} __ATTRIB_PACK CMD_ALL_PARMS;

#endif //#if !defined(__CMD_HAHNDLERS_H)
