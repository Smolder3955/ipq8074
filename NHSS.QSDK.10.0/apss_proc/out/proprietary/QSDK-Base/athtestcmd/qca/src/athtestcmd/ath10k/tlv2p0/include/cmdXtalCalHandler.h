// This is an auto-generated file from input/cmdXtalCalHandler.s
#ifndef _CMDXTALCALHANDLER_H_
#define _CMDXTALCALHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct xtalcalproc_parms {
    A_UINT8	capIn;
    A_UINT8	capOut;
    A_UINT8	ctrlFlag;
    A_UINT8	pad[1];
} __ATTRIB_PACK CMD_XTALCALPROC_PARMS;

typedef struct xtalcalprocrsp_parms {
    A_UINT8	status;
    A_UINT8	capValMin;
    A_UINT8	capValMax;
    A_UINT8	capIn;
    A_UINT8	capOut;
    A_UINT8	pllLocked;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_XTALCALPROCRSP_PARMS;

typedef enum {
    CTRL_CAL = 0,              // 0  xtal calibration
    CTRL_R_RAM_BUF,            // 1  get the capin/capout value from BDF/EEPROM buf
    CTRL_W_RAM_BUF,            // 2  save the capin/capout value to BDF/EEPROM buf
    CTRL_W_OTP,                // 3  save the capin/capout value to OTP
    CTRL_GET_CAP_RANGE,        // 4  quary the range of capin/capout in decimal value
    CTRL_R_OTP,                // 5  get the capin/capout value from OTP
} XTALCAL_CTRLFLAG;

// xtal cal error code
typedef enum {
    XTAL_CAL_OK                 = 0,//                 0      // ok
    XTAL_OTP_WRITE_ERR_LD       = 1,//      // locked down already
    XTAL_OTP_WRITE_FAILURE      = 2,//      // OTP write failure
    XTAL_OTP_WRITE_ERR_NOT_ATE  = 3,//      // not ATEed device
    XTAL_CAL_NOT_LOCKED         = 4,
}XTALCALERRORCODE;

typedef void (*XTALCALPROC_OP_FUNC)(void *pParms);
typedef void (*XTALCALPROCRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initXTALCALPROCOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL XTALCALPROCOp(void *pParms);

void* initXTALCALPROCRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL XTALCALPROCRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDXTALCALHANDLER_H_
