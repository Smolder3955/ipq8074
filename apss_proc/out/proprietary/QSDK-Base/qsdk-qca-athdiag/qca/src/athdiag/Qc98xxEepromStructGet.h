/*
 * Copyright (c) 2017 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _QC98XX_EEPROM_STRUCT_GET_H_
#define _QC98XX_EEPROM_STRUCT_GET_H_

extern QC98XX_EEPROM *Qc98xxEepromStructGet(void);

extern A_UINT8 Qc98xxEepromGetCckTrgtPwr(A_UINT16 targetPowerRateIndex, int freq);
extern A_UINT8 Qc98xxEepromGetLegacyTrgtPwr(A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz);
extern A_UINT8 Qc98xxEepromGetHT20TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz);
extern A_UINT8 Qc98xxEepromGetHT40TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz);
extern A_UINT8 Qc98xxEepromGetHT80TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz);

extern A_INT32 Qc98xxCustomerDataGet(A_UCHAR *data, int maxlength);
extern int Qc98xxEepromCalPierGet(int mode, int ipier, int ichain, 
                       int *pfrequency, int *pcorrection, int *ptemperature, int *pvoltage);
extern int Qc98xxReconfigMiscGet();
extern int Qc98xxReconfigDriveStrengthGet();
extern int Qc98xxChainMaskReduceGet();
extern int Qc98xxReconfigQuickDropGet();
extern int Qc98xxEepromWriteEnableGpioGet();
extern int Qc98xxWlanDisableGpioGet();
extern int Qc98xxWlanLedGpioGet();
extern int Qc98xxSpurBaseAGet();
extern int Qc98xxSpurBaseBGet();
extern int Qc98xxSpurRssiThreshGet();
extern int Qc98xxSpurRssiThreshCckGet();
extern int Qc98xxSpurMitFlagGet();
extern int Qc98xxRxBandSelectGpioGet();

extern int Qc98xxEnableFeatureGet();
extern int Qc98xxEnableTempCompensationGet();
extern int Qc98xxEnableVoltCompensationGet();
//extern int Qc98xxEnableFastClockGet();
extern int Qc98xxEnableDoublingGet();
extern int Qc98xxInternalRegulatorGet();
extern int Qc98xxRbiasGet(void);
extern int Qc98xxPapdGet(void);
extern int Qc98xxEnableTuningCapsGet();
extern int Qc98xxPapdRateMaskHt20Get(int iBand);
extern int Qc98xxPapdRateMaskHt40Get(int iBand);

extern int Qc98xxFutureGet(int *value, int ix, int *num, int iBand);
extern int Qc98xxAntDivCtrlGet();

extern int Qc98xxSWREGGet();

extern int Qc98xxEepromVersionGet();
extern int Qc98xxTemplateVersionGet();
extern int Qc98xxRegDmnGet(int *value, int ix, int *num);
extern int Qc98xxMacAddressGet(unsigned char mac[6]);
extern int Qc98xxBdAddressGet(unsigned char bdAddr[6]);
extern int Qc98xxTxRxMaskGet();
extern int Qc98xxTxMaskGet(void);
extern int Qc98xxRxMaskGet(void);
extern int Qc98xxOpFlagsGet(void);
extern int Qc98xxOpFlags2Get(void);
extern int Qc98xxBoardFlagsGet(void);
extern int Qc98xxEepMiscGet();
extern int Qc98xxRfSilentGet();
extern int Qc98xxRfSilentB0Get();
extern int Qc98xxRfSilentB1Get();
extern int Qc98xxRfSilentGPIOGet();
extern int Qc98xxBlueToothOptionsGet();
extern int Qc98xxDeviceCapGet();
extern int Qc98xxDeviceTypeGet();
extern int Qc98xxPwrTableOffsetGet();
extern int Qc98xxPwrTuningCapsParamsGet();
extern int Qc98xxThermometerGet();
extern int Qc98xxTxRxGainGet(int iBand);
extern int Qc98xxTxGainGet(int iBand);
extern int Qc98xxRxGainGet(int iBand);
extern int Qc98xxSWREGGet();

extern int Qc98xxTempSlopeGet(int *value, int ix, int *num, int iBand);
extern int Qc98xxTempSlopeLowGet(int *value);
extern int Qc98xxTempSlopeHighGet(int *value);
extern int Qc98xxVoltSlopeGet(int *value, int ix, int *num, int iBand);
extern int Qc98xxXpaBiasLvlGet(int iBand);
extern int Qc98xxTxFrameToDataStartGet(int iBand);
extern int Qc98xxTxFrameToPaOnGet(int iBand);
extern int Qc98xxTxClipGet(int iBand);
extern int Qc98xxDacScaleCckGet(int iBand);
extern int Qc98xxAntennaGainGet(int iBand);
extern int Qc98xxAdcDesiredSizeGet(int iBand);
extern int Qc98xxSwitchSettlingGet(int iBand);
extern int Qc98xxSwitchSettlingHt40Get(int iBand);
extern int Qc98xxTxEndToXpaOffGet(int iBand);
extern int Qc98xxTxEndToRxOnGet(int iBand);
extern int Qc98xxTxFrameToXpaOnGet(int iBand);
extern int Qc98xxThresh62Get(int iBand);

extern int Qc98xxDeltaCck20Get();
extern int Qc98xxDelta4020Get();
extern int Qc98xxDelta8020Get();

extern A_INT32 Qc98xxCalTgtPwrGet(int *pwrArr, int band, int htMode, int iFreqNum);
extern A_INT32 Qc98xxCalTgtFreqGet(int *freqArr, int band, int htMode);

extern A_INT32 Qc98xxCalFreqTGTCckGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalFreqTGTLegacyOFDMGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalFreqTGTHT20Get(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalFreqTGTHT40Get(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalFreqTGTHT80Get(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalTGTPwrCCKGet(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalTGTPwrLegacyOFDMGet(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalTGTPwrHT20Get(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalTGTPwrHT40Get(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalTGTPwrHT80Get(double *value, int ix, int iy, int iz, int *num, int iBand);

extern A_INT32 Qc98xxThermAdcScaledGainGet();
extern A_INT32 Qc98xxThermAdcOffsetGet();

extern A_INT32 Qc98xxCalFreqPierGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalPointTxGainIdxGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalPointDacGainGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalPointPowerGet(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalPierDataVoltMeasGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCalPierDataTempMeasGet(int *value, int ix, int iy, int iz, int *num, int iBand);

extern A_INT32 Qc98xxCtlIndexGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCtlFreqGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCtlPowerGet(double *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxCtlFlagGet(int *value, int ix, int iy, int iz, int *num, int iBand);

extern A_INT32 Qc98xxObGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxNoiseFloorThreshChGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxSpurChansGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxSpurAPrimSecChooseGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxSpurBPrimSecChooseGet(int *value, int ix, int *num, int iBand);

extern int Qc98xxQuickDropGet(int iBand);
extern int Qc98xxQuickDropLowGet();
extern int Qc98xxQuickDropHighGet();
extern int Qc98xxAntCtrlCommonGet(int iBand);
extern int Qc98xxAntCtrlCommon2Get(int iBand);
extern int Qc98xxRxFilterCapGet(int iBand);
extern int Qc98xxRxGainCapGet(int iBand);
extern A_INT32 Qc98xxAntCtrlChainGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1DBGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1MarginGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1HystGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten2DBGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten2MarginGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten2HystGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1DBLowGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1DBHighGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1MarginLowGet(int *value, int ix, int *num, int iBand);
extern A_INT32 Qc98xxXatten1MarginHighGet(int *value, int ix, int *num, int iBand);

//extern int Qc98xxEepromPaPredistortionGet(void);
extern int Qc98xxEepromCalibrationValid(void);

extern int Qc98xxRegulatoryDomainGet(void);
extern int Qc98xxRegulatoryDomain1Get(void);
extern int Qc98xxRegulatoryDomainOverride(unsigned int regdmn);

extern void Qc98xxPrintOffset();

extern int Qc98xxEepromLengthGet();
extern A_INT32 Qc98xxAlphaThermTableGet(int *value, int ix, int iy, int iz, int *num, int iBand);
extern A_INT32 Qc98xxConfigAddrGet(int *value, int ix, int *num);


#endif //_QC98XX_EEPROM_STRUCT_GET_H_
