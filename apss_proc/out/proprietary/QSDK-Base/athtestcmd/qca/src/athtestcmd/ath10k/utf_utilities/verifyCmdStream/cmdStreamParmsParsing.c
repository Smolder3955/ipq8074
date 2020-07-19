#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_HOST_SIM_TESTING)
#include "otaHostCommon.h"
#else
#include "osapi.h"
#endif
#include "wlantype.h"
#include "tcmdHostInternal.h"
#include "cmdStream.h"
#include "parmBinTemplate.h"
#include "parseBinCmdStream.h"
#include "cmdAllParms.h"
#include "cmdProcessingSM.h"

extern char dbgFile[KEY_SIZE_MAX];
extern FILE *fpDbg;

// ----------------- Some Notes -------------
    // cmd processing
    // cmdqueue 1 cmd at a time,
    //    cmdopcode -> cmdParmParser
    //    cmdParmParser knows the exact cmdParm structure
    //    based on cmd parm template that contains offset and size, update the cmd parm structure
    //    finally calls cmd processing routine with the cmd parm structure.
    //
    // based on cmd stream execution mode: (i.e. repeat n/indefinite times )
    //                     tester response, (no need, if we insert RX SYNC cmd
    //                     host respons (no need, since host can control from its end)
    //
    // cmd execution failure handling: back 2 steps
    // For failed test mode, copy 2 last entries from executedCmdQueue over to curCmdQueue.
    //
    // extream case of timeout, abort testing sequence ... and send back failure
    //
    // curCmdQueue, executedCmdQueue
    // once curCmdQueue is empty, depending on repeated test or not, copy all executeCmdQueue over to curCmdQueue
    //
    //
// ------------------------------------------

#if 0
static A_BOOL txCmdParmsParser(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms)
{
    _PARM_ONEOF   *oneParm;
    size_t   size;
    A_UINT32 pos, offset;
    A_UINT8  parmCode, parmType, *parmValStart;
    A_INT32  i,j;

    //A_PRINTF("Tx cmd parms parser\n");
    pos=0;
    for (i=0;i<numOfParms;i++) {
        oneParm = (_PARM_ONEOF *)(&cmdParmBuf[pos]);
        parmCode = oneParm->parmCode;
        parmType = oneParm->parmType;

        fprintf(fpDbg, "code %d = ", parmCode);

        offset = _txParm_bin_template[parmCode].offset;
        size   = _txParm_bin_template[parmCode].len;
        //memcpy((void*)(txCmdParms+offset), (void*)&(oneParm->parmValue), size);
        if ((A_UINT8)_PARM_U8 == parmType || _PARM_S8 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.addr[0]);
            for (j=0;j<size;j++) {
                fprintf(fpDbg, "0x%x ", oneParm->parmValue.addr[j]);
            }
            fprintf(fpDbg, "\n");
        }
        else if ((A_UINT8)_PARM_U16 == parmType || _PARM_S16 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val16);
            fprintf(fpDbg, "%d \n", oneParm->parmValue.value.val16);
        }
        else {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val32);
            fprintf(fpDbg, "%d \n", oneParm->parmValue.value.val32);
        }

        pos += sizeof(_PARM_ONEOF);
    }
    
    return(TRUE);
}
#endif

static _CMD_ALL_PARMS _cmdParms;

// tx cmd parms template
static _CMD_TX_PARMS _txCmdParms =
{
    2412,                // channel;
    //TCMD_CONT_TX_SINE,
    //TCMD_CONT_TX_TX99,   // txMode;
    2, //TCMD_CONT_TX_FRAME,   // txMode;
   {0x00008000,          // rateMask0;
    0x0},                 // rateMask1;

    // start power/gain
   {0x1c1c1c1c,          // pwrGainStart0;       // cck 
    0x1c1c1c1c,          // pwrGainStart0;       // cck 
    0x1c1c1c1c,          // pwrGainStart1;       // ofdm
    0x1a1a1c1c,          // pwrGainStart1;       // ofdm
    0x20202020,          // pwrGainStart2;       // ht20-1
    0x181c1e20,          // pwrGainStart2;       // ht20-1
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-1
    0x181a1c1e,          // pwrGainStart3;       // ht40-1
    0x20202020,          // pwrGainStart2;       // ht20-2
    0x181c1e20,          // pwrGainStart2;       // ht20-2
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-2
    0x181a1c1e,          // pwrGainStart3;       // ht40-2
    0x20202020,          // pwrGainStart2;       // ht20-3
    0x181c1e20,          // pwrGainStart2;       // ht20-3
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-3
    0x181a1c1e},          // pwrGainStart3;       // ht40-3

    // end power/gain
   {0x1c1c1c1c,          // pwrGainStart0;       // cck 
    0x1c1c1c1c,          // pwrGainStart0;       // cck 
    0x1c1c1c1c,          // pwrGainStart1;       // ofdm
    0x1a1a1c1c,          // pwrGainStart1;       // ofdm
    0x20202020,          // pwrGainStart2;       // ht20-1
    0x181c1e20,          // pwrGainStart2;       // ht20-1
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-1
    0x181a1c1e,          // pwrGainStart3;       // ht40-1
    0x20202020,          // pwrGainStart2;       // ht20-2
    0x181c1e20,          // pwrGainStart2;       // ht20-2
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-2
    0x181a1c1e,          // pwrGainStart3;       // ht40-2
    0x20202020,          // pwrGainStart2;       // ht20-3
    0x181c1e20,          // pwrGainStart2;       // ht20-3
    0x1e1e1e1e,          // pwrGainStart3;       // ht40-3
    0x181a1c1e},          // pwrGainStart3;       // ht40-3

    // pwrGain step
   {0x01010101,          // pwrGainStep0;        // cck 
    0x01010101,          // pwrGainStep1;        // ofdm
    0x01010101,          // pwrGainStep0;        // cck 
    0x01010101,          // pwrGainStep1;        // ofdm
    0x01010101,          // pwrGainStep2;        // ht20-1
    0x01010101,          // pwrGainStep3;        // ht40-1
    0x01010101,          // pwrGainStep4;        // ht20-2
    0x01010101,          // pwrGainStep5;        // ht40-2
    0x01010101,          // antenna;
    0x01010101,          // pwrGainStep0;        // cck 
    0x01010101,          // pwrGainStep1;        // ofdm
    0x01010101,          // pwrGainStep2;        // ht20-1
    0x01010101,          // pwrGainStep3;        // ht40-1
    0x01010101,          // pwrGainStep4;        // ht20-2
    0x01010101,          // pwrGainStep5;        // ht40-2
    0x01010101},          

    0,                   // antenna;
    0,                   // enANI;
    0,                   // scramblerOff;
    0,                   // aifsn;
    0,                   // pktSz;
    0,                   // txPattern;
    0,                   // shortGuard;
    0,                   // numPackets;
    1, //MODE_11G,            // wlanMode;
};

// rx cmd parms template
// This is only needed because whalSetMac doesn't seem to work ...
#if 0
static _CMD_RX_PARMS _rxSyncCmdParms={
    2412, //A_UINT32  channel;
    1, //TCMD_CONT_RX_FILTER,//1, //TCMD_CONT_RX_FILTER, //A_UINT32  rxMode; //act;
    //TCMD_CONT_RX_PROMIS,//1, //TCMD_CONT_RX_FILTER, //A_UINT32  rxMode; //act;
    0, //A_UINT32  enANI;
    0, //A_UINT32  antenna;
    1, //MODE_11G, //A_UINT32  wlanMode;
    1, // expectedPkts
    0, //A_UINT32  totalPkt;
    0, //A_INT32   rssiInDBm;
    0, //A_UINT32  crcErrPkt;
    0, //A_UINT32  secErrPkt;
    0, //A_UINT32  antswitch1;
    0, //A_UINT32  antswitch2;
    {0x01, 0x00, 0x00, 0xCA, 0xFF, 0xEE}, //A_UCHAR   addr[ATH_MAC_LEN];
};
#endif

static _CMD_RX_PARMS _rxCmdParms={
    2412, //A_UINT32  channel;
    1, //TCMD_CONT_RX_FILTER,//1, //TCMD_CONT_RX_FILTER, //A_UINT32  rxMode; //act;
    //TCMD_CONT_RX_PROMIS,//1, //TCMD_CONT_RX_FILTER, //A_UINT32  rxMode; //act;
    0, //A_UINT32  enANI;
    0, //A_UINT32  antenna;
    1, //MODE_11G, //A_UINT32  wlanMode;
    1, // expectedPkts
    0, //A_UINT32  totalPkt;
    0, //A_INT32   rssiInDBm;
    0, //A_UINT32  crcErrPkt;
    0, //A_UINT32  secErrPkt;
    0, //A_UINT32  antswitch1;
    0, //A_UINT32  antswitch2;
    {0x01, 0x00, 0x00, 0xC0, 0xFF, 0xEE}, //A_UCHAR   addr[ATH_MAC_LEN];
};


static A_BOOL cmdParmsParser_common(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms, void*cmdParms, _PARM_BIN_TEMPLATE *_parm_bin_template)
{
    _PARM_ONEOF   *oneParm;
    A_UINT32 pos, offset;
    size_t   size;
    A_UINT8  parmCode, parmType, *parmValStart;
    A_INT32  i;

    //A_PRINTF("Tx cmd parms parser\n");
    pos=0;
    for (i=0;i<numOfParms;i++) {
        oneParm = (_PARM_ONEOF *)(&cmdParmBuf[pos]);
        parmCode = oneParm->parmCode;
        parmType = oneParm->parmType;

        offset = _parm_bin_template[parmCode].offset;
        size   = _parm_bin_template[parmCode].len;
        //memcpy((void*)(txCmdParms+offset), (void*)&(oneParm->parmValue), size);
        //if ((A_UINT8)_PARM_U8 == parmType)
        if ((A_UINT8)_PARM_U8 == parmType || _PARM_S8 == parmType)
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.addr[0]);
        else if ((A_UINT8)_PARM_U16 == parmType || _PARM_S16 == parmType)
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val16);
        else 
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val32);

        memcpy((void*)(((A_UINT8 *)cmdParms)+offset), (void*)parmValStart, size);
        //A_PRINTF("tx pC %d pT %d of %d si %d ch %d rateM %x %x numPk %d\n", parmCode, parmType, offset, size, txCmdParms->channel, txCmdParms->rateMask[0], txCmdParms->rateMask[1], txCmdParms->numPackets);  

        pos += sizeof(_PARM_ONEOF);
    }
    
    *parms = (_CMD_ALL_PARMS *)&_cmdParms;

    return(TRUE);
}
#if 0
static A_BOOL cmdParmsParser_common(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms,
_PARM_BIN_TEMPLATE *_parm_bin_template)
{
    _PARM_ONEOF   *oneParm;
    size_t   size;
    A_UINT32 pos, offset;
    A_UINT8  parmCode, parmType, *parmValStart;
    A_INT32  i,j;

    pos=0;
    for (i=0;i<numOfParms;i++) {
        oneParm = (_PARM_ONEOF *)(&cmdParmBuf[pos]);
        parmCode = oneParm->parmCode;
        parmType = oneParm->parmType;

        fprintf(fpDbg, "code %d = ", parmCode);

        offset = _parm_bin_template[parmCode].offset;
        size   = _parm_bin_template[parmCode].len;
        //memcpy((void*)(rxCmdParms+offset), (void*)&(oneParm->parmValue), size);
        if ((A_UINT8)_PARM_U8 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.addr[0]);
            for (j=0;j<size;j++) {
                fprintf(fpDbg, "0x%x ", oneParm->parmValue.addr[j]);
            }
            fprintf(fpDbg, "\n");
        }
        else if ((A_UINT8)_PARM_U16 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val16);
            fprintf(fpDbg, "0x%x \n", oneParm->parmValue.value.val16);
        }
        else { 
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val32);
            fprintf(fpDbg, "0x%x \n", (unsigned int)oneParm->parmValue.value.val32);
        }

        pos += sizeof(_PARM_ONEOF);
    }
    
    return(TRUE);
}
#endif

static A_BOOL txCmdParmsParser(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms)
{
    _CMD_TX_PARMS *txCmdParms = (_CMD_TX_PARMS *)&(_cmdParms._cmdParmU._cmdTxParms);
    memset((void*)txCmdParms,0, sizeof(_CMD_TX_PARMS));
    memcpy((void*)txCmdParms, (void*)&_txCmdParms, sizeof(_CMD_TX_PARMS));
    return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, (void*)txCmdParms, _txParm_bin_template));
    //return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, _txParm_bin_template));
}

static A_BOOL rxCmdParmsParser(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms)
{
    _CMD_RX_PARMS *rxCmdParms = (_CMD_RX_PARMS *)&(_cmdParms._cmdParmU._cmdRxParms);
    memset((void*)rxCmdParms,0, sizeof(_CMD_RX_PARMS));
    memcpy((void*)rxCmdParms, (void*)&_rxCmdParms, sizeof(_CMD_RX_PARMS));
    return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, (void*)rxCmdParms, _rxParm_bin_template));
    //return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, _rxParm_bin_template));
}

static A_BOOL calCmdParmsParser(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms)
{
    _CMD_CAL_PARMS *calParms=(_CMD_CAL_PARMS*)&(_cmdParms._cmdParmU._cmdCalParms);
    return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, (void*)calParms, _calParm_bin_template));
}
#if 0
static A_BOOL calDoneCmdParmsParser(A_UINT8 *cmdParmBuf, _CMD_ALL_PARMS **parms, A_UINT8 numOfParms)
{
    return(cmdParmsParser_common(cmdParmBuf, parms, numOfParms, _calDoneParm_bin_template));
}
#endif
#if 0
    _PARM_ONEOF   *oneParm;
    size_t   size;
    A_UINT32 pos, offset;
    A_UINT8  parmCode, parmType, *parmValStart;
    A_INT32  i,j;

    //A_PRINTF("Tx cmd parms parser\n");
    pos=0;
    for (i=0;i<numOfParms;i++) {
        oneParm = (_PARM_ONEOF *)(&cmdParmBuf[pos]);
        parmCode = oneParm->parmCode;
        parmType = oneParm->parmType;

        fprintf(fpDbg, "code %d = ", parmCode);

        offset = _calParm_bin_template[parmCode].offset;
        size   = _calParm_bin_template[parmCode].len;
        //memcpy((void*)(txCmdParms+offset), (void*)&(oneParm->parmValue), size);
        if ((A_UINT8)_PARM_U8 == parmType || _PARM_S8 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.addr[0]);
            for (j=0;j<size;j++) {
                fprintf(fpDbg, "0x%x ", oneParm->parmValue.addr[j]);
            }
            fprintf(fpDbg, "\n");
        }
        else if ((A_UINT8)_PARM_U16 == parmType || _PARM_S16 == parmType) {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val16);
            fprintf(fpDbg, "%d \n", oneParm->parmValue.value.val16);
        }
        else {
            parmValStart = (A_UINT8 *)&(oneParm->parmValue.value.val32);
            fprintf(fpDbg, "%d \n", oneParm->parmValue.value.val32);
        }

        pos += sizeof(_PARM_ONEOF);
    }
    
    return(TRUE);
}
#endif

_CMD_HANDLERS CmdHandlers[] = 
{
    /*_OP_RESERVED*/     {NULL, rxCmdParmsParser},
    /*_OP_TX*/           {NULL, txCmdParmsParser},
    /*_OP_RX*/           {NULL, rxCmdParmsParser},
    /*_OP_CAL*/          {NULL, calCmdParmsParser},
    /*_OP_CALDONE*/      {NULL, NULL},
};

A_BOOL startParseCmdStream(A_UINT8 header, A_UINT16 headerDepValue, A_UINT32 headerExtended)
{
    A_BOOL rc=TRUE;

    fprintf(fpDbg, "op = 255\n");
    fprintf(fpDbg, "header = %d\n", header);
    fprintf(fpDbg, "headerDepValue = %d\n", headerDepValue);
    fprintf(fpDbg, "headerExtended = %d\n", (int)headerExtended);
 
    //if (fp) fclose(fp);

    return(rc);
    
}

#define _MAX_CAL_TX_CHANNELS 48
int _txChansIdx=0;
_CMD_TX_PARMS _txChans[_MAX_CAL_TX_CHANNELS]; 
    
A_BOOL parseCmdParms(_CMD_OPCODES opCode, A_UINT8 *cmdParmBuf, A_UINT8 numOfParms)
{
    A_BOOL rc=TRUE;
    _CMD_ALL_PARMS parms;
    _CMD_ALL_PARMS *pParms = &parms;

    fprintf(fpDbg, "\n\nop = %d, numParms %d\n", opCode, numOfParms);
    rc = (CmdHandlers[opCode]._CmdParmsParser)(cmdParmBuf, &pParms, numOfParms);
    if (!rc) {
        printf("Err parsing parms for cmd %d\n", opCode);
        return(rc);
    }

    // kludge part for now, storing tx info for CAL
    if (opCode == _OP_TX) {
        _CMD_TX_PARMS *txParms =(_CMD_TX_PARMS*)&(pParms->_cmdParmU._cmdTxParms);
        memcpy((void*)&(_txChans[_txChansIdx]), (void*)txParms, sizeof(_CMD_TX_PARMS));
        _txChansIdx++;
    }
    
    return(rc);
    //
}


