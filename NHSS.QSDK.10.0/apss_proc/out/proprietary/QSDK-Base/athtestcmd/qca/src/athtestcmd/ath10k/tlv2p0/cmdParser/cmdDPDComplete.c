// This is an auto-generated file from input/cmdDPDComplete.s
#include "tlv2Inc.h"
#include "cmdDPDComplete.h"

void* initGETDPDCOMPLETEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    return(NULL);
}

static GETDPDCOMPLETE_OP_FUNC GETDPDCOMPLETEOpFunc = NULL;

TLV2_API void registerGETDPDCOMPLETEHandler(GETDPDCOMPLETE_OP_FUNC fp)
{
    GETDPDCOMPLETEOpFunc = fp;
}

A_BOOL GETDPDCOMPLETEOp(void *pParms)
{
    if (NULL != GETDPDCOMPLETEOpFunc) {
        (*GETDPDCOMPLETEOpFunc)(NULL);
    }
    return(TRUE);
}

void* initGETDPDCOMPLETERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i; 	//for initializing array parameter
    CMD_GETDPDCOMPLETERSP_PARMS  *pGETDPDCOMPLETERSPParms = (CMD_GETDPDCOMPLETERSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = 0;	//assign a number to avoid warning in case i is not used

    // Populate the parm structure with initial values
    pGETDPDCOMPLETERSPParms->dpdComplete = pParmDict[PARM_DPDCOMPLETE].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_DPDCOMPLETE, (A_UINT32)(((A_UINT32)&(pGETDPDCOMPLETERSPParms->dpdComplete)) - (A_UINT32)pGETDPDCOMPLETERSPParms), pParmsOffset);
    return((void*) pGETDPDCOMPLETERSPParms);
}

static GETDPDCOMPLETERSP_OP_FUNC GETDPDCOMPLETERSPOpFunc = NULL;

TLV2_API void registerGETDPDCOMPLETERSPHandler(GETDPDCOMPLETERSP_OP_FUNC fp)
{
    GETDPDCOMPLETERSPOpFunc = fp;
}

A_BOOL GETDPDCOMPLETERSPOp(void *pParms)
{
    CMD_GETDPDCOMPLETERSP_PARMS *pGETDPDCOMPLETERSPParms = (CMD_GETDPDCOMPLETERSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("GETDPDCOMPLETERSPOp: dpdComplete %u\n", pGETDPDCOMPLETERSPParms->dpdComplete);
#endif //_DEBUG

    if (NULL != GETDPDCOMPLETERSPOpFunc) {
        (*GETDPDCOMPLETERSPOpFunc)(pGETDPDCOMPLETERSPParms);
    }
    return(TRUE);
}
