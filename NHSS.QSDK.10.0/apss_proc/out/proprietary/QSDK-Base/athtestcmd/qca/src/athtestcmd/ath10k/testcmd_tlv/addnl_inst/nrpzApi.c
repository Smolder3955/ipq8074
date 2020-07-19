/////////////////////////////////////////////////////////////////////////////
// Name:        demo_rsnrpzContAv.h
// Purpose:     Demo to use remote control driver 'rsnrpz' to do 
//              measurements buffered PowerAverage mode
//
// Description: Sample application for 'rsnrpz' driver
//              This example demonstrates the NRP-Z Powersensor measuring
//              continuously average power
//
//              The main() function first tries to open a connected R&S
//              NRP-Z series Powersensor by the resource string.
//
//              The ContAvMode(..) function configures the Powersensor,
//              starts a measurement, fetches and displays the results.
//
// Author:      Juergen D. Geltinger
// Created:     12-MAR-2008
// Modified by: Juergen D. Geltinger
// Modified:    14-OCT-2008
// RCS-ID:
// Copyright:   (c) Rohde & Schwarz, Munich
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined (LINUX)
#include <unistd.h>
#else
#include <windows.h>
#include "nrpzApi.h"
#endif


#define CH1 (1)
#define NUM_MEAS    1


/*===========================================================================*/
/* Function: PrintError                                                      */
/* Purpose:  prints errormessage depending on error code to console          */
/*                                                                           */
/*===========================================================================*/
void PrintError( long lErr, long lSession, const char *cpFunc )
{
    if ( lErr < VI_SUCCESS ) 
    {
        ViChar  szErrorMessage[ 256 ];
        rsnrpz_error_message( lSession, lErr, szErrorMessage );
        printf( "%s() Error: %s\n", cpFunc, szErrorMessage );

        // Also fetching a possible error from the
        // device's error queue
        rsnrpz_error_query( lSession, &lErr, szErrorMessage );
        if ( lErr )
            printf( "Instrument-Error: %s\n", szErrorMessage );
    }
}

/*===========================================================================*/
/* Function: Watt2dBm                                                        */
/* Purpose:  convert power value from Watt to dBm                            */
/*                                                                           */
/*===========================================================================*/
double Watt2dBm( double dW )
{
	if ( dW < 1.0e-14 )
        return -110.0;
    
    return 10.0 * log10( dW ) + 30.0;
}


/*===========================================================================*/
/* Function: ContBurstMode                                                      */
/* Purpose:  configures ContAv measurement using buffered mode               */
/*                                                                           */
/*===========================================================================*/
NRPZAPI_API double ContBurstMode( long lSesID )
{
    unsigned short usComplete = 0;
    //long           lReadCount = -1;
    long           lAvgCount  = 1;
    double         dAperture  = 0;
    //unsigned int   ui         = 0;
    unsigned int   nMeas      = 0;
    double         adResult[ NUM_MEAS ];
    long           iWait      = 0;
    ViReal64       dropOutTol = (10 * 1e-6); 

    rsnrpz_setTimeout( 10000 );

    /* Reset the Sensor */
    PrintError( rsnrpz_chan_reset( lSesID, CH1 ), lSesID, "rsnrpz_chan_reset" );

    /* Switch to buffered mode */  
    PrintError( rsnrpz_chan_mode( lSesID, CH1,RSNRPZ_SENSOR_MODE_BURST ), lSesID, "rsnrpz_chan_mode" );
    //PrintError( rsnrpz_chan_mode( lSesID, CH1,RSNRPZ_SENSOR_MODE_CONTAV ), lSesID, "rsnrpz_chan_mode" );

    PrintError( rsnrpz_chan_setBurstDropoutTolerance (lSesID, CH1, dropOutTol), lSesID, "fail to set Drop");

    /* Set the trigger count. One 'init:imm' command causes trigger:count results
       if we set the trigger count to the buffer size, an 'init:immediate' will
       fill the sensor buffer with results and causes the sensor to transmit the results
    */
    PrintError( rsnrpz_trigger_setCount( lSesID, CH1, 1 ), lSesID, "rsnrpz_trigger_setCount" );  

    /* disable averaging for fast measurements */
    PrintError( rsnrpz_avg_setEnabled( lSesID, CH1, 0 ), lSesID, "rsnrpz_avg_setEnabled" );

    // If manual ranging of the NRP-Z11/Z21 series Power sensors
    // shall be checked, the following code could be enabled:
#if 0
    /* disable autoranging */
    PrintError( rsnrpz_range_setAutoEnabled( lSesID, CH1, 0 ), lSesID, "rsnrpz_range_setAutoEnabled" );
    PrintError( rsnrpz_range_setRange(       lSesID, CH1, 0 ), lSesID, "rsnrpz_range_setRange" );
#endif

    while ( nMeas != NUM_MEAS )
    {
        /* start the measurement */
        PrintError( rsnrpz_chan_initiate( lSesID, CH1 ), lSesID, "rsnrpz_chan_initiate" );

        //PrintError( rsnrpz_chan_setBurstDropoutTolerance (lSesID, CH1, dropOutTol), lSesID, "fail to set Drop");

        /* wait until result arrives */
        usComplete = 0;

        while ( usComplete == 0 )
        {
            PrintError( rsnrpz_chan_isMeasurementComplete( lSesID, CH1, &usComplete ), lSesID, "rsnrpz_chan_isMeasurementComplete" );
            if (usComplete == 0)
            {
            printf( "." );
            fflush( stdout );
            //iWait++;

            if (++iWait >= 5)
            {
                return -110.0;
            }
#if defined (LINUX)
            usleep( 100 * 1000 );
#else
            Sleep( 100 );
#endif
            }
        }

        /* fetch the result */
        PrintError( rsnrpz_meass_fetchMeasurement( lSesID, CH1, &adResult[nMeas] ), lSesID, "rsnrpz_meass_fetchMeasurement" );

        PrintError( rsnrpz_avg_getCount( lSesID, CH1, &lAvgCount ),           lSesID, "rsnrpz_avg_getCount" );
        PrintError( rsnrpz_chan_getContAvAperture( lSesID, CH1, &dAperture ), lSesID, "rsnrpz_chan_getContAvAperture" );

        adResult[nMeas] = fabs( adResult[nMeas] );
/*
        printf( " #%4d: %e W = %6.2f dBm, Avg = %ld, Aper = %e s\n",
                nMeas + 1,
                adResult[nMeas],
                Watt2dBm( adResult[nMeas] ),
                lAvgCount,
                dAperture );
*/
        nMeas++;
    }

#ifdef PRINT_SUMMARY
    /* print a summary... */
    for ( ui = 0; ui < nMeas; ui++ )
    {
        printf( "[%d]: \t %e Watt = %7.3f dBm\n", ui, adResult[ ui ], Watt2dBm( adResult[ ui ] ) );
    }
#endif

    /* Always return the 1st value */
    return Watt2dBm( adResult[0] );
}

/*===========================================================================*/
/* Function: BufferedMode                                                    */
/* Purpose:  configures ContAv measurement using buffered mode               */
/*                                                                           */
/*===========================================================================*/
NRPZAPI_API void BufferedMode(unsigned long sid, double *result)
{
#define MAXBUFFERSIZE 1024
  const int BUFFERSIZE = 8;

  unsigned short complete = 0;
  //double result[MAXBUFFERSIZE];
  long readcount = -1;
  int i= 0;

  /* Reset the Sensor */
  PrintError (
    rsnrpz_chan_reset(sid,CH1), sid, "rsnrpz_chan_reset");

  /* switch to buffered mode */  
  PrintError (
    rsnrpz_chan_mode (sid, CH1,RSNRPZ_SENSOR_MODE_BUF_CONTAV), sid, "rsnrpz_chan_mode");
  /* set buffersize */
  PrintError (
    rsnrpz_chan_setContAvBufferSize (sid, CH1, BUFFERSIZE), sid, "rsnrpz_chan_setContAvBufferSize");

  /* set the trigger count. one init:imm causes trigger:count results
     if we set the trigger count to the biffer size, one init immediate will
     fill the sensor buffer with results and causes the sensor to transmit the results
  */
  PrintError (
    rsnrpz_trigger_setCount (sid, CH1,BUFFERSIZE), sid, "rsnrpz_trigger_setCount");  

  /* disable averaging for fast measurements */
  PrintError (
    rsnrpz_avg_setEnabled (sid,CH1,0), sid, "rsnrpz_avg_setEnabled");

  /* start the measurement */
  PrintError (
    rsnrpz_chan_initiate (sid, CH1), sid, "rsnrpz_chan_initiate");

  /* wait until result arrives */    
  while (complete == 0)
  {
    PrintError (
      rsnrpz_chan_isMeasurementComplete (sid, CH1, &complete), sid, "rsnrpz_chan_isMeasurementComplete");
    Sleep(200);
    printf(".");
  }

  /* fetch the result */
  printf("\r\n");
    PrintError (
      rsnrpz_meass_fetchBufferMeasurement (sid, CH1, BUFFERSIZE, result, &readcount), sid, "rsnrpz_meass_fetchBufferMeasurement");

  /* and print it to stdout */
  for (i=0;i<readcount;i++)
  {
    printf("[%d]: \t %e\r\n",i,result[i]);
  }


}

#ifndef ADDNL_INST_EXPORTS
/*===========================================================================*/
/* Function: main()                                                          */
/* Purpose:  initiate sensor and wait until the rusult arrives               */
/*                                                                           */
/*===========================================================================*/
int main(int argc, char *argv[])
{
	/* please edit resource for your sown sensor ! */
	
  /*constant for Sensor Resource*/
  /*                             Z21   ser.No.*/
  char NRPZ21[] = "USB::0x0aad::0x0003::100000";
  /*                             Z11   ser No */
  char NRPZ11[] = "USB::0x0aad::0x000c::100000";
  /*                             Z51   ser No */
  char NRPZ51[] = "USB::0x0aad::0x0016::100000";

  /* The Driver can now open the first sensor on the Bus with the '*' descriptor
     This is useful if only one sensor is connected
  */
  char FirstSensor[] = "*"; 

  char* pSensor = &FirstSensor[0];

  /* it will be better that session is initialized with 0 */
  unsigned long session = 0;
  unsigned long err = 0;
  double result = 0.0;
  char DriverRev[256] = "";
  char firmwareRev[256] ="" ;

  /* open Driver with Sensor */
  err = (rsnrpz_init(pSensor, 1, 1, &session));
  PrintError(err,session);

  if (err != VI_SUCCESS)
  {
    /* no Sensor found: exit */
    printf ("Could not find Sensor: %s\n Please press any key to exit\n",pSensor);
    getch();
    return -1;
  }

  printf ("%s opened with session id: %ld\n",pSensor, session);

  /* get Driver revision */
  PrintError (rsnrpz_revision_query (session, DriverRev,firmwareRev), session);
  printf ("Driver Revision: %s; Firmware Revision: %s\n", DriverRev,firmwareRev);

  
  printf ("\r\n==============================================\r\n");
  printf ("Hit any key to start\r\n");
  getch();

  BufferedMode(session);

  printf ("Hit any key to quit\r\n");
  getch();

  /*close the session after usage */
  rsnrpz_close (session);

  return 0;
}
#endif //ADDNL_INST_EXPORTS

NRPZAPI_API ViSession nrp_open()
{
  ViSession lSession  = 0xFFFFFFFF;
  //long   lErr         = 0;
  long   lNumSensors  = 0;
  //double dResult      = 0.0;
  char   szDriverRev[   256 ];
  char   szFirmwareRev[ 256 ];
  char   szSensorName[  256 ];
  char   szSensorType[  256 ];
  char   szSensorSN[    256 ];


  strcpy( szSensorName, "*" );

  PrintError( rsnrpz_GetSensorCount( 0, &lNumSensors ), lSession, "rsnrpz_GetSensorCount" );
  if ( lNumSensors < 1 )
  {
    printf( "No Sensor found.\n" );
    return 0;
  }

  PrintError( rsnrpz_GetSensorInfo( 0, CH1, szSensorName, szSensorType, szSensorSN ), lSession, "rsnrpz_GetSensorInfo" );

  PrintError( rsnrpz_init( szSensorName, 1, 1, &lSession ), lSession, "rsnrpz_init" );

  printf( "%s opened. Session ID is: 0x%08x\n", szSensorName, (unsigned int)lSession );

  PrintError( rsnrpz_revision_query( lSession, szDriverRev, szFirmwareRev ), lSession, "rsnrpz_revision_query" );

  printf( "=======================================================\n" );
  printf( "Driver Revision: %s; Firmware Revision: %s\n", szDriverRev, szFirmwareRev );
  printf( "=======================================================\n" );
	
  return lSession;
}

NRPZAPI_API void nrp_close(ViSession lSession )
{
    PrintError( rsnrpz_chan_reset( lSession, CH1 ), lSession, "rsnrpz_chan_reset" );
    PrintError( rsnrpz_close( lSession ), lSession, "rsnrpz_close" );
}

