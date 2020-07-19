/////////////////////////////////////////////////////////////////////////////
// Name:        nrpzAPI.h
// Purpose:     Provide APIs for Power Sensor nrpz
//
// Description: Three major APIs are provided.
//              nrp_open();
//              ContBurstMode();
//              nrp_close();
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _NRPZ_API_H_
#define _NRPZ_API_H_


#if defined (LINUX)
#define NRPZAPI_API
#else
#ifdef ADDNL_INST_EXPORTS
#define NRPZAPI_API __declspec(dllexport)
#else
#define NRPZAPI_API __declspec(dllimport)
#endif //ADDNL_INST_EXPORTS
#endif //LINUX

ViSession NRPZAPI_API nrp_open();
double NRPZAPI_API ContBurstMode( long lSesID );
void NRPZAPI_API BufferedMode(unsigned long sid, double *result);
void NRPZAPI_API nrp_close(ViSession lSession );

#endif //_NRPZ_API_H_

