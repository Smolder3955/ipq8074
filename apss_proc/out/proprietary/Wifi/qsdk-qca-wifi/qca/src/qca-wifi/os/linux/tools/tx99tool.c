/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
 
 /*
 * tx99tool set
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <err.h>


/*
 * Linux uses __BIG_ENDIAN and __LITTLE_ENDIAN while BSD uses _foo
 * and an explicit _BYTE_ORDER.  Sorry, BSD got there first--define
 * things in the BSD way...
 */
#define	_LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	_BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#include <asm/byteorder.h>
#if defined(__LITTLE_ENDIAN)
#define	_BYTE_ORDER	_LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define	_BYTE_ORDER	_BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif

/*
** Need to find proper references in UMAC code
*/

#include "ieee80211_external.h"
#include "tx99_wcmd.h"

/*
 * These frequency values are as per channel tags and regulatory domain
 * info. Please update them as database is updated.
 */
#define A_FREQ_MIN              4920
#define A_FREQ_MAX              5825

#define A_CHAN0_FREQ            5000
#define A_CHAN_MAX              ((A_FREQ_MAX - A_CHAN0_FREQ)/5)

#define BG_FREQ_MIN             2412
#define BG_FREQ_MAX             2484

#define BG_CHAN0_FREQ           2407
#define BG_CHAN_MIN             ((BG_FREQ_MIN - BG_CHAN0_FREQ)/5)
#define BG_CHAN_MAX             14      /* corresponding to 2484 MHz */

#define A_20MHZ_BAND_FREQ_MAX   5000
#define INVALID_FREQ    0
typedef unsigned short     A_UINT16;
typedef unsigned long      A_UINT32;

static A_UINT16
wmic_ieee2freq(A_UINT32 chan)
{
    if (chan == BG_CHAN_MAX) {
        return BG_FREQ_MAX;
    }
    if (chan < BG_CHAN_MAX) {    /* 0-13 */
        return (BG_CHAN0_FREQ + (chan*5));
    }
    if (chan <= A_CHAN_MAX) {
        return (A_CHAN0_FREQ + (chan*5));
    }
    else {
        return INVALID_FREQ;
    }
}


static A_UINT32 freqValid(A_UINT32 val)
{
    do {
        if (val <= A_CHAN_MAX) {
            A_UINT16 freq;

            if (val < BG_CHAN_MIN)
                break;

            freq = wmic_ieee2freq(val);
            if (INVALID_FREQ == freq)
                break;
            else
                return freq;
        }

        if ((val == BG_FREQ_MAX) || 
            ((val < BG_FREQ_MAX) && (val >= BG_FREQ_MIN) && !((val - BG_FREQ_MIN) % 5)))
            return val;
        else if ((val >= A_FREQ_MIN) && (val < A_20MHZ_BAND_FREQ_MAX) && !((val - A_FREQ_MIN) % 20))
            return val;
        else if ((val >= A_20MHZ_BAND_FREQ_MAX) && (val <= A_FREQ_MAX) && !((val - A_20MHZ_BAND_FREQ_MAX) % 5))
            return val;
    } while (0);

    fprintf(stderr,"Invalid channel or freq #: %lu !\n", val);

    return 0;
}

static void usage(void);

int
main(int argc, char *argv[])
{
	const char *ifname, *cmd;
	tx99_wcmd_t i_req;
	int s;
	struct ifreq ifr;
	int value = 0;

	if (argc < 3)
		usage();
	ifname = argv[1];
	cmd = argv[2];
	
    (void) memset(&ifr, 0, sizeof(ifr));
    if (strlcpy(ifr.ifr_name, ifname, IFNAMSIZ) >= IFNAMSIZ) {
        printf("source too long\n");
        usage();
    }
    (void) memset(&i_req, 0, sizeof(i_req));
    if (strlcpy(i_req.if_name, ifname, IFNAMSIZ) >= IFNAMSIZ) {
        printf("source too long\n");
        usage();
    }

    /* enable */
	if(!strcasecmp(cmd, "start")) {
		if(argc > 3) {
			fprintf(stderr,"Too many arguments\n");
			fprintf(stderr,"usage: tx99tool wifi0 start\n");
			goto ErrExit;
		}
		i_req.type = TX99_WCMD_ENABLE;
	}	
	/* disable */
	else if(!strcasecmp(cmd, "stop")) {
		if(argc > 3) {
			fprintf(stderr,"Too many arguments\n");
			fprintf(stderr,"usage: tx99tool wifi0 stop\n");
			goto ErrExit;
		}
		i_req.type = TX99_WCMD_DISABLE;
	} 
	/* get */
	else if (!strcasecmp(cmd, "get")) {
	    if(argc > 3) {
			fprintf(stderr,"Too many arguments\n");
			fprintf(stderr,"usage: tx99tool wifi0 get\n");
			goto ErrExit;
		}
        i_req.type = TX99_WCMD_GET;
    } 
    /* set */
    else if (!strcasecmp(cmd, "set")) {
        if (argc < 4)
		    usage();
    
    	/* Tx frequency, bandwidth and extension channel offset */
    	if(!strcasecmp(argv[3], "freq") && argc ==9) {
			
    		i_req.type = TX99_WCMD_SET_FREQ;
			value = freqValid(atoi(argv[4]));
			if(!value) {
				fprintf(stderr,"invalid the frequency \r\n");
				goto ErrExit;
			}
    		i_req.data.freq = value;

			if(!strncmp(argv[5], "band", strlen("band"))){
				value = atoi(argv[6]);
			    if(!((value >= 0) && (value <= 2))){
					fprintf(stderr,"The value of bandwidth is invalid, the valid value: 0: 20MHZ, 1:20+40MHZ,2:40 MHZ \r\n");
					goto ErrExit;
				}

	    		i_req.data.htmode = value;
			}
			if(!strncmp(argv[7], "ht", strlen("ht"))){
				value = atoi(argv[8]);
			    if(!(( value >= 0)&&( value <= 2))) {
					fprintf(stderr,"The value of htext is invalid, the valid value: 0: legacy, 1:when the extension is plus,2:when extension is minus \r\n");
					goto ErrExit;
				}
				i_req.data.htext = value;

			}
    	}
    	/* rate - Kbits/s */
    	else if(!strcasecmp(argv[3], "rate")) {
    		if(argc != 5) {
    			fprintf(stderr,"Wrong arguments\n");
    			fprintf(stderr,"usage: tx99tool wifi0 set rate [Tx rate]\n");
				goto ErrExit;
    		}
    		i_req.type = TX99_WCMD_SET_RATE;
			value =  atoi(argv[4]);
			if (value < 0){
    			fprintf(stderr,"Wrong arguments, the rate should > 0 \n");
				goto ErrExit;
			}
    		i_req.data.rate = value;
    	}
    	/* Tx power */
    	else if(!strcasecmp(argv[3], "pwr")) {
    		if(argc != 5) {
    			fprintf(stderr,"Wrong arguments\n");
    			fprintf(stderr,"usage: tx99tool wifi0 set pwr [Tx pwr]\n");
				goto ErrExit;
    		}
			
    		i_req.type = TX99_WCMD_SET_POWER;
			value =  atoi(argv[4]);
			
			if (value < 0) {
    			fprintf(stderr,"Wrong arguments, the power should > 0 \n");
				goto ErrExit;
			}
			i_req.data.power = value;
		
    	}
		
    	/* tx chain mask - 1, 2, 3 (for 2*2) */
    	else if(!strcasecmp(argv[3], "txchain")) {
    		if(argc != 5) {
    			fprintf(stderr,"Wrong arguments\n");
    			fprintf(stderr,"usage: tx99tool wifi0 set txchain [Tx chain mask]\n");
				goto ErrExit;
    		}
    		i_req.type = TX99_WCMD_SET_CHANMASK;
			value = atoi(argv[4]);
			if( !((value == 1) || (value ==3 )|| (value ==7))) {
    			fprintf(stderr,"The valid value of txchain: 1: 1x1, 3: 2x2, 7:3x3 \r\n");
				goto ErrExit;
			}
				
    		i_req.data.chanmask = value;
    	}
    	/* Tx type - 0: data modulated, 1:single carrier */
    	else if(!strcasecmp(argv[3], "type")) {
			#if 0
    		if(argc != 5) {
    			fprintf(stderr,"Wrong arguments\n");
    			fprintf(stderr,"usage: tx99tool wifi0 set type [Tx type]\n");
    			goto ErrExit;
    		}
    		i_req.type = TX99_WCMD_SET_TYPE;
    		i_req.data.type = atoi(argv[4]);
			#else
			fprintf(stderr,"usage: don't support configure the tx type ");
			goto ErrExit;
			#endif
    	}    else if(!strcasecmp(argv[3], "txmode")) {
            if(argc != 5) {
                fprintf(stderr,"Wrong arguments \n");
                fprintf(stderr,"usage: tx99tool wifi0 set txmode [0: 11A 1: 11B  2: 11G 5: 11NAHT20 6: 11NGHT20 7: 11NAHT40PLUS 8: 11NAHT40MINUS 9: 11NGHT40PLUS 10: 11NGHT40MINUS] \n");
                goto ErrExit;
            }
            i_req.type = TX99_WCMD_SET_TXMODE;
			value = atoi(argv[4]);
		    if(!(( value >= 0) && (value <= 10) && (value !=4 ) && (value !=3 ))) {
				fprintf(stderr,"usage: tx99tool wifi0 set txmode [0: 11A1: 11B 2: 11G 5: 11NAHT20 6: 11NGHT20 7: 11NAHT40PLUS 8: 11NAHT40MINUS 9: 11NGHT40PLUS 10: 11NGHT40MINUS] \n");
				goto ErrExit;
		   }
            i_req.data.txmode = value;
        }
        else if(!strcasecmp(argv[3], "testmode")) {
            if(argc != 5) {
                fprintf(stderr,"Wrong arguments\n");
                fprintf(stderr,"usage: tx99tool wifi0 set testmode [Test Mode]\n");
                goto ErrExit;
            }
            i_req.type = TX99_WCMD_SET_TESTMODE;
			value = atoi(argv[4]);
		    if(!(( value >= 0) && ( value <= 4))) {
	            fprintf(stderr,"usage: tx99tool wifi0 set testmode[0:TransMIT PN9 data 1:Transmit All zeros 2:Transmit All ones, 3:Receive only 4:Single Carrier mode\n");
				goto ErrExit;
			}

            i_req.data.testmode = value;
        }
    	else 
        	usage();

    } else
        usage();
    
    ifr.ifr_data = (void *) &i_req;
    
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket(SOCK_DRAGM)");
	
    if (ioctl(s, SIOCIOCTLTX99, &ifr) < 0)
		err(1, "ioctl");


    return 0;
ErrExit:
		exit(-1);


}

static void
usage(void)
{
	fprintf(stderr, "usage: tx99tool wifi0 [start|stop|set]\n");
	fprintf(stderr, "usage: tx99tool wifi0 set [freq||bandwidth|htext|rate|txchain|type|pwr|txmode|testmode]\n");
	exit(-1);
}

