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
#endif

#define CH1         1
#define NUM_MEAS    1



/*===========================================================================*/
/* Function: PrintError                                                      */
/* Purpose:  prints error message depending on error code to console         */
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
double ContBurstMode( long lSesID )
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
#if defined (LINUX)
            usleep( 100 * 1000 );
#else
            Sleep( 100 );
#endif
//          printf( "." );
//          fflush( stdout );
            iWait++;

            if (iWait >= 5)
            {
                return -110.0;
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
/* Function: main()                                                          */
/* Purpose:  initiate sensor and wait until the result arrives               */
/*                                                                           */
/*===========================================================================*/

ViSession nrp_open()
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

  PrintError( rsnrpz_GetSensorInfo( 0, 0, szSensorName, szSensorType, szSensorSN ), lSession, "rsnrpz_GetSensorInfo" );

  PrintError( rsnrpz_init( szSensorName, 1, 1, &lSession ), lSession, "rsnrpz_init" );

  printf( "%s opened. Session ID is: 0x%08x\n", szSensorName, (unsigned int)lSession );

  PrintError( rsnrpz_revision_query( lSession, szDriverRev, szFirmwareRev ), lSession, "rsnrpz_revision_query" );

  printf( "=======================================================\n" );
  printf( "Driver Revision: %s; Firmware Revision: %s\n", szDriverRev, szFirmwareRev );
  printf( "=======================================================\n" );
	
  return lSession;
}

void nrp_close(ViSession lSession )
{
    PrintError( rsnrpz_chan_reset( lSession, CH1 ), lSession, "rsnrpz_chan_reset" );
    PrintError( rsnrpz_close( lSession ), lSession, "rsnrpz_close" );
}

