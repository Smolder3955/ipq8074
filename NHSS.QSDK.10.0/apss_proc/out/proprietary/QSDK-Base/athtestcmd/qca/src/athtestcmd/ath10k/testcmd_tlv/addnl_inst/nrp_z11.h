/////////////////////////////////////////////////////////////////////////////
// Name:        nrp_z11.h
// Purpose:     Provide APIs for Power Sensor nrp_z11
//
// Description: It is based on demo_rsnrpzBufferedMode.
//              Three major APIs are provided.
//              nrp_open();
//              nrp_get();
//              nrp_close();
//
// Author:      Chen Wei
// Created:     13-JUN-2008
/////////////////////////////////////////////////////////////////////////////

#ifndef _NRP_Z11_H_
#define _NRP_Z11_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined (LINUX)
#include <unistd.h>
#else
#include <windows.h>
#endif

#define CH1 (1)
#define MAXBUFFERSIZE (1024)

ViSession nrp_open();
//double nrp_get( long lSesID, int avg_count, int central_freq, int indBm );
//double nrp_get_burst(long lSesID, int central_freq, int indBm);
double ContBurstMode( long lSesID );
void nrp_close(ViSession lSession );


#endif
