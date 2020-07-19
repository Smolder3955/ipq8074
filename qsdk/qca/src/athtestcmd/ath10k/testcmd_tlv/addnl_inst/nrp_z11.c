/////////////////////////////////////////////////////////////////////////////
// Name:        nrp_z11.c
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

#include "nrp_z11.h"

double calculate_average(double p[], int count);
ViReal64 watt2dbm( ViReal64 watt );

/*===========================================================================*/
/* Function: Watt2dBm                                                        */
/* Purpose:  convert power value from Watt to dBm                            */
/*                                                                           */
/*===========================================================================*/
static double Watt2dBm( double dW )
{
	if ( dW < 1.0e-14 )
        return -110.0;
    
    return 10.0 * log10( dW ) + 30.0;
}

/*===========================================================================*/
/* Function: PrintError                                                      */
/* Purpose:  prints errormessage depending on error code to console          */
/*                                                                           */
/*===========================================================================*/
static void PrintError( long lErr, long lSession )
{
        if ( lErr < VI_SUCCESS ) 
        {
                ViChar  szErrorMessage[ 256 ];
                rsnrpz_error_message( lSession, lErr, szErrorMessage );
                printf( "Error: %s\n", szErrorMessage );
        }
}

/*===========================================================================*/
/* Function: nrp_open                                                        */
/* Purpose:  initialize the power sensor and return the session ID           */
/*           if successful, return 0xFFFFFFFF if failed.                     */
/*                                                                           */
/*===========================================================================*/
ViSession nrp_open()
{
        ViSession lSession  = 0xFFFFFFFF;
        long   lNumSensors  = 0;
        //char   szDriverRev[   256 ];
        //char   szFirmwareRev[ 256 ];
        char   szSensorName[  256 ];
        char   szSensorType[  256 ];
        char   szSensorSN[    256 ];
        
        PrintError( rsnrpz_GetSensorCount( 0, &lNumSensors ), lSession );
        if ( lNumSensors < 1 )
        {
                printf( "No Sensor found.\n" );
                return lSession;
        }

        PrintError( rsnrpz_GetSensorInfo( 0, 0, szSensorName, szSensorType, szSensorSN ), lSession );
        PrintError( rsnrpz_init( szSensorName, 1, 1, &lSession ), lSession );
        printf( "%s opened. Session id: %ld\n", szSensorName, lSession );

        /*
        PrintError( rsnrpz_revision_query( lSession, szDriverRev, szFirmwareRev ), lSession );
        printf( "=======================================================\n" );
        printf( "Driver Revision: %s; Firmware Revision: %s\n", szDriverRev, szFirmwareRev );
        printf( "=======================================================\n" );
        */

        return lSession;
}

/*===========================================================================*/
/* Function: nrp_close                                                       */
/* Purpose:  close the power sensor and release the session ID               */
/*                                                                           */
/*===========================================================================*/
void nrp_close( long lSession )
{
        PrintError( rsnrpz_chan_reset( lSession, CH1 ), lSession );
        rsnrpz_close( lSession );
}

/*===========================================================================*/
/* Function: nrp_get                                                         */
/* Purpose:  get the power value from the sensor, it takes four parameters   */
/*           lSesID - Session ID                                           */
/*           avg_count - the counting number for averaging                   */
/*           central_freq - the central frequency of RF signal in MHz        */
/*           indBm - the returned value in dBm, otherwise in mW              */
/*                                                                           */
/*===========================================================================*/
double nrp_get( long lSesID, int avg_count, int central_freq, int indBm )
{
        unsigned short usComplete = 0;
        long           lReadCount = -1;
        int            i          = 0;
        int count;
        double         adResult[ MAXBUFFERSIZE ], adPow, adPowdB;

        /* Reset the Sensor */
        PrintError( rsnrpz_chan_reset( lSesID, CH1 ), lSesID );

        /* Set the central frequency */
        PrintError( rsnrpz_chan_setCorrectionFrequency( lSesID, CH1, central_freq * 1e6 ), lSesID );

        /* Switch to buffered mode */  
        PrintError( rsnrpz_chan_mode( lSesID, CH1,RSNRPZ_SENSOR_MODE_BUF_CONTAV ), lSesID );

        /* Set buffersize */
        PrintError( rsnrpz_chan_setContAvBufferSize( lSesID, CH1, avg_count), lSesID );

        /* Set the trigger count. One 'init:imm' command causes trigger:count results
           if we set the trigger count to the buffer size, an 'init:immediate' will
           fill the sensor buffer with results and causes the sensor to transmit the results
           */
        PrintError( rsnrpz_trigger_setCount( lSesID, CH1, avg_count ), lSesID );  

        /* disable averaging for fast measurements */
        PrintError( rsnrpz_avg_setEnabled( lSesID, CH1, 0 ), lSesID );

        /* start the measurement */
        PrintError( rsnrpz_chan_initiate( lSesID, CH1 ), lSesID );

        /* wait until result arrives */
        count = avg_count + 10;
        while ( usComplete == 0 && count > 0 )
        {
                PrintError( rsnrpz_chan_isMeasurementComplete( lSesID, CH1, &usComplete ), lSesID );
#if defined (LINUX)
                usleep( 200 * 1000 );
#else
                Sleep( 200 );
#endif
                //printf( "." );
                fflush( stdout );
                count--;
        }
        /* timeout, some error here */
        if ( count == 0 )       return 999.0; 

        /* fetch the result */
//        printf( "\n\n" );
        PrintError( rsnrpz_meass_fetchBufferMeasurement( lSesID, CH1, avg_count, adResult, &lReadCount ), lSesID );

        /* and print it to stdout */
        adPow = 0.0;
        for ( i = 0; i < lReadCount; i++ )
        {
                adPow += adResult[ i ];
        }
        adPow = adPow / lReadCount;
        adPowdB = Watt2dBm( adPow );

        printf("Read adPow = %f (%f dBm)\n", adPow, adPowdB);
//        printf( "The average power is %.4f mW, %.2f dBm\n", adPow*1000, adPowdB );

        if ( indBm )
                return adPowdB;
        else
                return adPow;
}



double nrp_get_burst(long lSesID, int central_freq, int indBm)
{
        ViReal64 readResult = 0;
	double   adPowdB;
	ViReal64 dropOutTol = (10 * 1e-6); /* Burst is ~200us, so tollerance = 200x5% = 10us (2% in dB) */
	ViInt32  triggerSource = RSNRPZ_TRIGGER_SOURCE_INTERNAL;
	//ViReal64 triggerLevel = 1e-4; /* -40dBm */

	ViStatus ret;

        /* Reset the Sensor */
        PrintError( rsnrpz_chan_reset( lSesID, CH1 ), lSesID );

        /* Set the central frequency */
        PrintError( rsnrpz_chan_setCorrectionFrequency( lSesID, CH1, central_freq * 1e6 ), lSesID );

        /* Switch to burst mode */
        PrintError( rsnrpz_chan_mode( lSesID, CH1, RSNRPZ_SENSOR_MODE_BURST ), lSesID );

        PrintError( rsnrpz_chan_setBurstDropoutTolerance (lSesID, CH1, dropOutTol), lSesID);

	//rsnrpz_chan_setBurstChopperEnabled (lSesID, CH1, 0);
        
	//PrintError( rsnrpz_chan_getBurstDropoutTolerance (lSesID, CH1, &dropOutTol), lSesID);

	//printf("DropOutTol = %f seconds.\n", dropOutTol);
	//
	//rsnrpz_trigger_setLevel (lSesID, CH1, triggerLevel);
	

	rsnrpz_trigger_setSource (lSesID, CH1, triggerSource);


	rsnrpz_trigger_setAutoTriggerEnabled (lSesID, CH1, 1);


	ret = rsnrpz_meass_readMeasurement ( lSesID, CH1, 500, &readResult);

	if (ret == 0){
		printf("Power = %e ", readResult);
	} else {
		readResult = 0.0;
	}

	//printf("ret=%d, value=%f\n", ret, readResult);

	//avgPower = calculate_average(&readResult[0], count);
        if (indBm){
                /* Translate into dBm */
                adPowdB = watt2dbm(readResult);

		printf("(%f dBm)\n", adPowdB);

                return adPowdB;
        } else {
                return readResult;
        }
}

double calculate_average(double pSample[], int count)
{
	int i = 0, j = 0;
	int good_sample_count = 0;
	double raw_average = 0;
	double sum = 0;

	for (i = 0; i < count; i++)
	{
		//printf("Sample[%d]=%f\n", i, pSample[i]);
		raw_average += pSample[i];
	}

	raw_average = (raw_average / count);
	//printf("Raw Average = %f\n", raw_average);

	for (j = 0; j < count; j++)
	{
		/* Drop 10% from average is acceptable */
		if (pSample[j] > (raw_average * 0.9))
		{
			//printf("Sample[%d] = %f. Good.\n", j, pSample[j]);

			sum += pSample[j];
			good_sample_count++;
		}
#if 0
		else
		{
			printf("Sample[%d] = %f. Bad\n", j, pSample[j]);
		}
#endif
	}

	if (good_sample_count == 0)
	{
		return raw_average;
	}

	printf("Good Samples = %d\n", good_sample_count);

	return (sum / good_sample_count);
}


ViReal64 watt2dbm( ViReal64 watt )
{
        if ( watt < 1e-9 ){
		return (-110.0);
	}
//printf("f     %e\n", log10(watt));
//printf("f     %e\n", 10.0*log10(watt));
//printf("f     %e\n", 10.0*log10(watt)+30.0);
        double tmp;
        tmp = log10(watt);
        tmp *= 10.0;
        tmp += 30.0;

    	return ((10.0 * log10((double)watt)) + 30.0);
}

