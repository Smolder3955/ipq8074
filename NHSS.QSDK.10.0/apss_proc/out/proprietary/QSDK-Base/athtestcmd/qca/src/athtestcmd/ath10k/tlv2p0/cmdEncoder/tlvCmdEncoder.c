/*
 *   TLV cmd/rsp encoder
 *   -------------------
 *
 *   APIs:
 *       createCmdRsp()
 *
 *   How to ensure codes are consistent between two entities: random number as semantics code
 *   ----------------------------------------------------------------------------------------
 *
 *   The following command/response, and paramter dictionaries are not required to be
 *   always consistent between two entities.
 *   Though we could use build script to check duplicates, it's not the best guarantee 
 *   in cases the builds come from different branches. We do want to work towards a single
 *   branch, inevitably branching may happen, for example, a customer branch.
 *   
 *   Command/response code in the stream, total 4B
 *       cmdCode(4B)
 *       randomNumber(4B)
 *
 *   Command/Response code dictionary
 *   --------------------------------
 *       Refer to cmdRspParmsDic.h
 *       May later add distinction between FTM and MM exposure, i.e. some only for FTM
 *
 *   Parameter code dictionary 
 *   -------------------------
 *   Parameter codes have scope within a command/response. Across commands/responses, the parameter
 *   codes can be the same.
 *   The same random semantics codes are used to ensure the consistency between two entities.
 *
 *   The parameter code in the stream, total 4B
 *       parmCode(4B)
 *       randomNumber(4B)
 *
 *  The parameter type
 *      Singular vs. array
 *
 *  The basic parameter value type (either singular or array element)
 *      PARM_U32
 *      PARM_U16
 *      PARM_U8
 *      PARM_S32
 *      PARM_S16
 *      PARM_S8
 * 
 * Command/Response format
 * -----------------------
 *
 * The stream header is a shorter version of version 1 (TS), keeping only
 *     commandID and version.
 *
 * Each command/response stream contains ONLY ONE command or response.
 * It's expected that after sending command, it's blocked for response.
 *
 * All parameter values are promoted to A_UINT32 or A_INT32.
 * The parser needs to handle accordingly.
 *
 * Examples of stream
 *     - A single value cmd stream:
 * |cmdID(4B)|version(4B)|cmdCode(4B)|cmdRand(4B)|(parmCode1)(4B)|parmRand1(4B)|parmValue1(4B)|
 * 
 *     - An array value cmd stream:
 * |cmdID(4B)|version(4B)|cmdCode(4B)|cmdRand(4B)|(parmCode1)(4B)|parmRand1(4B)|numElem(4B)|beginPos(4B)|value1(4B)|value2(4B)|...value(numElem-1)(4B)|
 *
 *     Of course, the aggregation of the above two basic examples.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#if !defined(_HOST_SIM_TESTING)
#include "osapi.h"
#endif
#include "wlantype.h"
#include "tsCommon.h"
#include "cmdRspParmsInternal.h"
#include "cmdRspParmsDict.h"
#include "testcmd.h"
#include "tlv2Api.h"

_STATIC TESTFLOW_CMD_STREAM_V2  CmdStreamV2;
_STATIC A_UINT32 streamPos = 0;
_STATIC A_UINT32 numParmsPos = 0;
_STATIC A_UINT32 numParms = 0;

#define encode_array(arrayType, dataInStreamType) \
{\
    arrayType *arr = (arrayType *)arrayLoc; \
    dataInStreamType temp; \
    for (i=0;i<numElem;i++) { \
        temp = (dataInStreamType) arr[beginPos++]; \
        memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&temp, sizeof(temp)); \
        streamPos +=4; \
    }\
}

#define ENCODE_VAL_UNSIGNED(type) \
{ \
    type temp = va_arg(args, type); \
    numArgs--; \
    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&temp, sizeof(temp)); \
    streamPos +=4; \
}

#ifdef _HOST_SIM_TESTING
static void prtCmdStream(A_UINT8 *stream, A_UINT32 streamLen) 
{
    int i,num=0;
    A_UINT32 *pt32=(A_UINT32 *)stream;
    printf("ver2..stream: ");
    for (i=0;i<(int)streamLen;i+=4) {
        //printf("%d ", stream[i]);
        pt32=(A_UINT32 *)&(stream[i]);
        printf("%d ", (int)(*pt32));
        num++;
        if (!(num % 40)) printf("\n");
    }
    printf("\n");
    return;
}
#endif //_HOST_SIM_TESTING

static A_BOOL tlv2p0Enabled=FALSE;

A_BOOL addTLV2p0Encoder(void) 
{
    tlv2p0Enabled=TRUE;
    return TRUE;
}

TLV2_API void tlv2CreateCmdHeader(A_UINT32 cmdCode)
{
    CMD_DICT  *pCmdDict=CmdDict;

    if (!tlv2p0Enabled) {
        A_PRINTF_ALWAYS("TLV 2.0 encoder not on\n");
        return;
    }

    if (streamPos != 0)
    {
        A_PRINTF_ALWAYS("A TLV2 command contruction is already in progress.\n");
        return;
    }
    if (!(cmdCode < (A_UINT32)MaxCmdDictEntries)) {A_PRINTF_ALWAYS("cmdCode exceeds dict\n"); return;}

    // Populate header
    memset((void*)&CmdStreamV2.cmdStreamHeader, 0, sizeof(CmdStreamV2.cmdStreamHeader));
    CmdStreamV2.cmdStreamHeader.cmdId   = TC_CMD_TLV_ID;
    CmdStreamV2.cmdStreamHeader.version = CMD_STREAM_VER2;

    // start payload
    streamPos = 0;
    // Add command code
    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&cmdCode, sizeof(cmdCode));
    streamPos +=4;
    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&(pCmdDict[cmdCode].rand), sizeof(pCmdDict[cmdCode].rand));
    streamPos +=4;
    numParmsPos = streamPos;
    //memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&numOfParms, sizeof(numOfParms));
    streamPos +=4;
    numParms = 0;
}

TLV2_API void tlv2AddParms(int numArgs, ...)
{
    va_list args;
    A_UINT32 parmCode;
    PARM_DICT *pParmDict=ParmDict;
    A_UINT32 numElem, beginPos;
    A_UINT32 i;
    A_UINT32 valueType, arrayLoc, binDataLoc;

    if (!tlv2p0Enabled) {
        A_PRINTF_ALWAYS("TLV 2.0 encoder not on\n");
        return;
    }
    if (streamPos == 0)
    {
    	A_PRINTF_ALWAYS("No TLV2 command has been constructed.\n");
    	return;
    }

    // Add parameters one at a time
    va_start(args, numArgs);
    while(--numArgs >=0) {
        parmCode = va_arg(args, A_UINT32);
        numParms++;
        if (!(parmCode < (A_UINT32)MaxParmDictEntries)) {
            va_end(args);
            A_PRINTF_ALWAYS("parmCode exceeds dict\n");
            return;
        }
        // encode parmCode and its rand
        memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&parmCode, sizeof(parmCode));
        streamPos +=4;
        memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&(pParmDict[parmCode].rand), sizeof(pParmDict[parmCode].rand));
        streamPos +=4;

        // go through value(s): singular or array
        valueType = pParmDict[parmCode].dataType;
        // array
        if (DATATYPE_IS_ARRAY(valueType)) {
            numElem   = va_arg(args, A_UINT32);
            beginPos  = va_arg(args, A_UINT32);
            arrayLoc  = va_arg(args, A_UINT32);
            numArgs -= 3;

            // encode array numElem and beginPos
            memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&numElem, sizeof(numElem));
            streamPos +=4;
            memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&beginPos, sizeof(beginPos));
            streamPos +=4;
            // encode array elements one by one
            switch (DATATYPE_TYPE(valueType)) {
              #define ENCODE_ARRAY(parmType, dataType) \
                case parmType: \
                    encode_array(dataType, A_UINT32)  \
                    break;
              #include "encodeArray.def"
            }
        }
        else if (DATATYPE_IS_DATA(valueType)) {
            // encode binary data block
            binDataLoc = va_arg(args, A_UINT32);
            numArgs--;
            switch (DATATYPE_TYPE(valueType)) {
                case PARM_DATA_64:
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 64);
                    streamPos +=64;
                    break;
                case PARM_DATA_128:
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 128);
                    streamPos +=128;
                    break;
                case PARM_DATA_256:
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 256);
                    streamPos +=256;
                    break;
                case PARM_DATA_512:
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 512);
                    streamPos +=512;
                    break;
                default:
                    break;
            }
        }
        // singular
        else {
            switch (DATATYPE_TYPE(valueType)) {
                case PARM_U32:
                case PARM_U16:
                case PARM_U8:
                    // all unsigned types are promoted to A_UINT32, parser needs to convert correctly
                    ENCODE_VAL_UNSIGNED(A_UINT32)
                    break;
                case PARM_S32:
                case PARM_S16:
                case PARM_S8:
                    // all signed types are promoted to A_INT32, parser needs to convert correctly
                    ENCODE_VAL_UNSIGNED(A_INT32)
                    break;
            }
        }
    }
    // done
    va_end(args);
}

TLV2_API TESTFLOW_CMD_STREAM_V2 *tlv2CompleteCmdRsp()
{
    if (!tlv2p0Enabled) {
        A_PRINTF_ALWAYS("TLV 2.0 encoder not on\n");
        return(NULL);
    }
    if (streamPos == 0)
    {
    	A_PRINTF_ALWAYS("No TLV2 command has been constructed.\n");
    	return (NULL);
    }
    // add payload length
    CmdStreamV2.cmdStreamHeader.length = streamPos;
    // add numParms back to a location after cmdCode
    memcpy((void*)&(CmdStreamV2.payload[numParmsPos]), (void*)&numParms, sizeof(numParms));
    streamPos = 0;
    numParmsPos = 0;
    numParms = 0;
    return(&CmdStreamV2);
}

TLV2_API TESTFLOW_CMD_STREAM_V2 *createCmdRsp(A_UINT32 cmdCode, int numArgs, ...)
{
    va_list args;
    A_UINT32 parmCode;
    CMD_DICT  *pCmdDict=CmdDict;
    PARM_DICT *pParmDict=ParmDict;
    A_UINT32 numElem, beginPos;
    A_UINT32 i;
    A_UINT32 valueType, arrayLoc, binDataLoc;

    if (!tlv2p0Enabled) {
        A_PRINTF_ALWAYS("TLV 2.0 encoder not on\n");
        return(NULL);
    }

    if (!(cmdCode < (A_UINT32)MaxCmdDictEntries)) {A_PRINTF_ALWAYS("cmdCode exceeds dict\n"); return(NULL);}

    // Populate header
    memset((void*)&CmdStreamV2.cmdStreamHeader, 0, sizeof(CmdStreamV2.cmdStreamHeader));
    CmdStreamV2.cmdStreamHeader.cmdId   = TC_CMD_TLV_ID;
    CmdStreamV2.cmdStreamHeader.version = CMD_STREAM_VER2;

    // start payload
    streamPos = 0;
    // Add command code
    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&cmdCode, sizeof(cmdCode));
    streamPos +=4;
    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&(pCmdDict[cmdCode].rand), sizeof(pCmdDict[cmdCode].rand));
    streamPos +=4;
    numParmsPos = streamPos;
    //memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&numOfParms, sizeof(numOfParms));
    streamPos +=4;

    // Add parameters one at a time
    va_start(args, numArgs);
    numParms=0;
    while(--numArgs >=0) {
        parmCode = va_arg(args, A_UINT32);
        numParms++;
        if (!(parmCode < (A_UINT32)MaxParmDictEntries)) {
            va_end(args);
            A_PRINTF_ALWAYS("parmCode exceeds dict\n");
            return(NULL);
        }
        // encode parmCode and its rand
        memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&parmCode, sizeof(parmCode));
        streamPos +=4;
        memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&(pParmDict[parmCode].rand), sizeof(pParmDict[parmCode].rand));
        streamPos +=4;

        // go through value(s): singular or array
        valueType = pParmDict[parmCode].dataType;
        // array
        if (DATATYPE_IS_ARRAY(valueType)) { 
            numElem   = va_arg(args, A_UINT32);
            beginPos  = va_arg(args, A_UINT32);
            arrayLoc  = va_arg(args, A_UINT32);
            numArgs -= 3;

            // encode array numElem and beginPos
            memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&numElem, sizeof(numElem));
            streamPos +=4;
            memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)&beginPos, sizeof(beginPos));
            streamPos +=4;

            // encode array elements one by one
            switch (DATATYPE_TYPE(valueType)) {
              #define ENCODE_ARRAY(parmType, dataType) \
                case parmType: \
                    encode_array(dataType, A_UINT32)  \
                    break;
              #include "encodeArray.def"
            }
        }
        else if (DATATYPE_IS_DATA(valueType)) { 
            // encode binary data block 
            binDataLoc = va_arg(args, A_UINT32);
            numArgs--;
            switch (DATATYPE_TYPE(valueType)) {
                case PARM_DATA_64: 
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 64);
                    streamPos +=64;
                    break;
                case PARM_DATA_128: 
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 128);
                    streamPos +=128;
                    break;
                case PARM_DATA_256: 
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 256);
                    streamPos +=256;
                    break;
                case PARM_DATA_512: 
                    memcpy((void*)&(CmdStreamV2.payload[streamPos]), (void*)((A_UINT8 *)binDataLoc), 512);
                    streamPos +=512;
                    break;
                default:
                    break;
            }
        }
        // singular
        else {
            switch (DATATYPE_TYPE(valueType)) {
                case PARM_U32:
                case PARM_U16:
                case PARM_U8:
                    // all unsigned types are promoted to A_UINT32, parser needs to convert correctly
                    ENCODE_VAL_UNSIGNED(A_UINT32)
                    break;
                case PARM_S32:
                case PARM_S16:
                case PARM_S8:
                    // all signed types are promoted to A_INT32, parser needs to convert correctly
                    ENCODE_VAL_UNSIGNED(A_INT32)
                    break;
            }
        }
    }
    // done
    va_end(args);
    // add payload length
    CmdStreamV2.cmdStreamHeader.length = streamPos;
    // add numParms back to a location after cmdCode
    memcpy((void*)&(CmdStreamV2.payload[numParmsPos]), (void*)&numParms, sizeof(numParms));

    streamPos = 0;
    numParmsPos = 0;
    numParms = 0;
    return(&CmdStreamV2);
}

