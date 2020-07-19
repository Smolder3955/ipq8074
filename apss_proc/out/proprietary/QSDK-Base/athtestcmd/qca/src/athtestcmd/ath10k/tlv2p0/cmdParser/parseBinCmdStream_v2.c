#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if !defined(_HOST_SIM_TESTING)
#include "osapi.h"
#endif
#include "wlantype.h"
#include "tsCommon.h"
#include "cmdStream.h"
#include "cmdRspParmsInternal.h"
#include "cmdRspParmsDict.h"
#include "parseBinCmdStream.h"
#include "tlvCmdRspInternal.h"
#include "tlvCmdRsp.h"
#include "tlv2Api.h"
#include "cmdHandlers.h"

#if defined(_DEBUG_TLV2)
_STATIC void prtCmdStream(A_UINT8 *stream, A_UINT32 streamLen) 
{
    A_UINT32 i,num=0;
    A_UINT32 *pt32=(A_UINT32 *)stream;
    A_PRINTF_ALWAYS("ver2..stream: ");
    for (i=0;i<streamLen;i+=4) {
        //printf("%d ", stream[i]);
        pt32=(A_UINT32 *)&(stream[i]);
        A_PRINTF_ALWAYS("%d ", (int)(*pt32));
        num++;
        if (!(num % 8)) A_PRINTF_ALWAYS("\n");
    }
    A_PRINTF_ALWAYS("\n");
    return;
}
#else
#define prtCmdStream(x,y)
#endif

#define copyArray(type)       \
{                             \
    type temp;                \
    for (i=0;i<numElem;i++) { \
        temp = (type) payload[payloadPos++]; \
        memcpy((void*)(((type *)((char*)(*pParmStruct)+offset)) +beginPos +i), (void*)&temp, sizeof(temp)); \
    } \
}

#define copyValue(type) \
{ \
    type temp=(type)value; \
    memcpy((void*)((char*)(*pParmStruct)+offset), (void*)&temp, sizeof(temp)); \
}

#if 0
#define copyBinData(type, len) \
{  \
    A_UINT8 *binData = (A_UINT8 *)&(payload[payloadPos]); 
    
    memcpy((void*)((A_UINT8 *)(*pParmStruct)+offset), (void*)binData, len); 
    payloadPos += (len/4);
#endif

// Note:
//   Both the parm struct and parm offset tbl need to be built for every cmd during run time
// Area to create a cmd specific parm struct
static CMD_ALL_PARMS ParmsCommon;

// Indexed by parm key hash, stores parmCode and offset within the parm struct
static PARM_OFFSET_TBL ParmsOffset[KEY_HASH_RANGE];

// TLV2p0 parser
A_BOOL parmParser2p0(A_UINT32 cmdCode, A_UINT32 numParms, A_UINT32 *payload, void **pParmStruct) 
{
    A_BOOL rc=TRUE;
    A_UINT32 payloadPos;
    A_UINT32 parmCode, semCode;
    //A_UINT32 dataType; 
    A_BOOL   offsetValid;
    A_UINT8  keyHash;
    PARM_OFFSET_FIELDS *pParmOffsetFields; 
    A_UINT32 offset;
    A_UINT32 numElem, beginPos;
    A_UINT32 value;
    A_UINT32 numParmsParsed;
    A_UINT32 i;

    // after header, it's the 4B cmdCode and randNum

    memset((void*)&ParmsCommon, 0, sizeof(ParmsCommon));
    memset((void*)ParmsOffset, 0, sizeof(ParmsOffset));
    //A_PRINTF_ALWAYS("pOfSt %d, %d\n", sizeof(ParmsOffset), (int)ParmsOffset[2].pt);
    if (NULL != CmdHandlerTbl[cmdCode].initParms ) {
        *pParmStruct = (CmdHandlerTbl[cmdCode].initParms)((A_UINT8 *)&ParmsCommon, &(ParmsOffset[0]), ParmDict);
    }
    else { A_PRINTF_ALWAYS("Null initParms\n"); return(FALSE);}

    if (NULL == *pParmStruct) {
    	if (numParms == 0)
    		return(TRUE);
    	else{
			A_PRINTF_ALWAYS("cannot build parm struct\n");
			return(FALSE);
    	}
    }
    numParmsParsed =0;
    payloadPos=0;
    while (numParmsParsed < numParms) {
        parmCode    = payload[payloadPos++];
        semCode     = payload[payloadPos++]; 
        numParmsParsed++;
        //A_PRINTF_ALWAYS("parmCode %d semCode %d\n", (int)parmCode, (int)semCode);

        if (!(parmCode < (A_UINT32)MaxParmDictEntries)) { A_PRINTF_ALWAYS("parmCode %d exceeds dict, dropped\n", (int)parmCode); continue; }
        if (semCode != ParmDict[parmCode].rand) { A_PRINTF_ALWAYS("parmRand mismatch %d %d %d, dropped\n", (int)parmCode, (int)semCode, (int)ParmDict[parmCode].rand); continue; }

        keyHash = ParmDict[parmCode].keyHash;
        pParmOffsetFields = ParmsOffset[keyHash].pt;
        offsetValid=FALSE;
        while (NULL != pParmOffsetFields) {
            if (pParmOffsetFields->parmCode == parmCode){
                offset = pParmOffsetFields->offset;
                offsetValid = TRUE;
                break;
            }
            else {
                pParmOffsetFields = pParmOffsetFields->next;
            }
        }
        if (!offsetValid) { A_PRINTF_ALWAYS("parmCode %d not in offset table, dropped\n", (int)parmCode); continue; }

        if (DATATYPE_IS_ARRAY(ParmDict[parmCode].dataType)) {
            // array
            numElem  = payload[payloadPos++];
            beginPos = payload[payloadPos++]; 
            switch (DATATYPE_TYPE(ParmDict[parmCode].dataType)) {
                #define COPY_ARRAY(parmType, dataType) \
                    case parmType:  \
                        copyArray(dataType)  \
                        break;
                #include "copyArray.def" 
            }
        }
        else if (DATATYPE_IS_DATA(ParmDict[parmCode].dataType)) {
            // binary data 
            A_UINT8 *binData;
            switch (DATATYPE_TYPE(ParmDict[parmCode].dataType)) {
#if 0
                #define COPY_BINDATA(parmType, dataType) \
                    case parmType: \
                        copyBinData(dataType)  \
                        break;
                #include "copyBinData.def"
#endif
                case PARM_DATA_64:
                    binData = (A_UINT8 *)&(payload[payloadPos]); 
                    memcpy((void*)((A_UINT8 *)(*pParmStruct)+offset), (void*)binData, 64); 
                    payloadPos += (64/4);
                    break;
                case PARM_DATA_128:
                    binData = (A_UINT8 *)&(payload[payloadPos]); 
                    memcpy((void*)((A_UINT8 *)(*pParmStruct)+offset), (void*)binData, 128); 
                    payloadPos += (128/4);
                    break;
                case PARM_DATA_256:
                    binData = (A_UINT8 *)&(payload[payloadPos]); 
                    memcpy((void*)((A_UINT8 *)(*pParmStruct)+offset), (void*)binData, 256); 
                    payloadPos += (256/4);
                    break;
                case PARM_DATA_512:
                    binData = (A_UINT8 *)&(payload[payloadPos]); 
                    memcpy((void*)((A_UINT8 *)(*pParmStruct)+offset), (void*)binData, 512); 
                    payloadPos += (512/4);
                    break;
                default:
                    break;
            }
        }
        else {
            // singular value
            value = payload[payloadPos++]; 
            // convert to proper size, from 4B
            // overwrite the parm structure corresponding field
            switch (DATATYPE_TYPE(ParmDict[parmCode].dataType)) {
                #define COPY_VALUE(parmType, dataType) \
                    case parmType:  \
                        copyValue(dataType)  \
                        break;
                #include "copyValue.def" 
            }
        }
    }
    //A_PRINTF_ALWAYS("d parmP %d %d\n", numParmsParsed, numParms);
    return(rc);
}

typedef enum {
    RSP_RESERVED=0,
    RSP_POSITIVE,
    RSP_NEGATIVE,
} RSP_TYPE;

typedef struct platformSpecificStuff {
    A_BOOL (*sendGenericRsp)(A_UINT32 cmdCode, void* pParmStruct, RSP_TYPE rsp); 
} PLATFORM_SPECIFIC;

static PLATFORM_SPECIFIC PlatformSpecific = {
    NULL,
};

// The simple one cmd one rsp parser
static A_BOOL binCmdStreamParserVer2(A_UINT8 *stream, A_UINT32 readStreamLen, A_UINT8 **pPayload, A_UINT16 *payloadLen)
{
    A_BOOL rc=TRUE;
    TESTFLOW_CMD_STREAM_V2 *cmdStream;
    A_UINT32 payloadPos;
    A_UINT32 *payload, *parmPayload;
    void *pParmStruct; 
    A_UINT32 cmdCode, semCode;
    A_UINT32 numParms;
    

    prtCmdStream(stream, readStreamLen);

    cmdStream=(TESTFLOW_CMD_STREAM_V2 *)stream;

    *payloadLen = (A_UINT16)(readStreamLen - sizeof(TESTFLOW_CMD_STREAM_HEADER_V2));
    if (cmdStream->cmdStreamHeader.length != *payloadLen) { A_PRINTF_ALWAYS("cmdStream len mismatch\n"); return(FALSE); }
    payload = (A_UINT32 *)cmdStream->payload;
    payloadPos =0;

    // after header, it's the 4B cmdCode and randNum
    cmdCode = payload[payloadPos++];
    semCode = payload[payloadPos++]; 
    numParms = payload[payloadPos++]; 
    parmPayload = (A_UINT32 *)&(payload[payloadPos]);

    if (!(cmdCode < (A_UINT32)MaxCmdDictEntries)) { A_PRINTF_ALWAYS("cmdCode > dict\n");  return(FALSE); }
    if (!(cmdCode < MaxCmdHandlers)) { A_PRINTF_ALWAYS("cmdCode > handlers\n"); return(FALSE); }
    if (semCode != CmdDict[cmdCode].rand) { A_PRINTF_ALWAYS("cmd rand mismatch code %d sc %d r %d\n", (int)cmdCode, (int)semCode, (int)CmdDict[cmdCode].rand); return(FALSE); }

    if (!(rc = parmParser2p0(cmdCode, numParms, parmPayload, &pParmStruct))) { A_PRINTF_ALWAYS("parm parsing error \n"); return(FALSE); }

    // call cmdHandler
    // How to pass in phyID? It should be a parameter in the parmStruct
    if (NULL != CmdHandlerTbl[cmdCode].cmdProcessing) {
        rc = CmdHandlerTbl[cmdCode].cmdProcessing((void*)pParmStruct);
    }

    // if cmdProcessing is succesful, 
    //     if CmdHandlerTbl.rsp is NULL, positive response
    //     else execute the rsp routine
    // else 
    //     negative response
    if (rc) {
        if (NULL == CmdHandlerTbl[cmdCode].rspGeneration) {
            // call bounded platform specific positive response, with cmd parmStruct for the eontext
            if (NULL != PlatformSpecific.sendGenericRsp) {
                if (!(rc = PlatformSpecific.sendGenericRsp(cmdCode, (void*)pParmStruct, RSP_POSITIVE))) {
                    A_PRINTF_ALWAYS("sendRsp failed\n");
                    // nothing else can be done 
                }
            }
        }
        else {
            // rspGeneration routine is provided with the cmd parmStruct to establish the context
            // likely it will fetch data on the target, and send response report back
            if (!(rc = CmdHandlerTbl[cmdCode].rspGeneration(cmdCode, (void*)pParmStruct))) {
                A_PRINTF_ALWAYS("rspGeneration failed\n");
            }
        }
    }
    else {
        if (NULL != PlatformSpecific.sendGenericRsp) {
            if (!(rc = PlatformSpecific.sendGenericRsp(cmdCode, (void*)pParmStruct, RSP_NEGATIVE))) {
                A_PRINTF_ALWAYS("sendRsp failed\n");
            }
        }
    }

    return(rc);
}

//
// Exposed API
TLV2_API A_BOOL addTLV2p0BinCmdParser(void)
{
    if (!(bindBinCmdStreamParser(binCmdStreamParserVer2, CMD_STREAM_VER2))) {
        return(FALSE);
    }

    // no separate cmdProcessing for TLV2p0
    return(TRUE);
}

