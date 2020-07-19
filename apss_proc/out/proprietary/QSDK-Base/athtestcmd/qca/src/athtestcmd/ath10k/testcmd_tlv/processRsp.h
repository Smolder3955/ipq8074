#ifndef _PROCESS_RSP_H__
#define _PROCESS_RSP_H__

#define A_RATE_NUM      183
#define G_RATE_NUM      183
#define TCMD_MAX_RATES_11AC_3x3 153

#define RATE_STR_LEN             30

typedef const char RATE_STR[RATE_STR_LEN];

extern void cmdReplyFunc_v2(void *buf);
extern void cmdReplyFunc(void *buf);
extern void handleTPCCALRsp(CMD_TPCCALRSP_PARMS *pParms);
extern void handleTPCCALDATA(CMD_TPCCALDATA_PARMS *pParms);
extern void handleREGREADRSP(void *pParms);
extern void handleREGWRITERSP(void *pParms);
extern void handleBASICRSP (void *parms);
extern void handleTXSTATUSRSP (void *parms);
extern void handleRXSTATUSRSP (void *parms);
extern void handleRXRSP (void *parms);

#endif //_PROCESS_RSP_H__
