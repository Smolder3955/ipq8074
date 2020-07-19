/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#ifndef _DEV_ATH_AR5311REG_H_
#define _DEV_ATH_AR5311REG_H_

/*
 * Definitions for the Atheros 5311 chipset.
 */
#define	AR5311_QDCLKGATE	0x005c	/* MAC QCU/DCU clock gating control */
#define	AR5311_QDCLKGATE_QCU_M	0x0000FFFF /* QCU clock disable */
#define	AR5311_QDCLKGATE_DCU_M	0x07FF0000 /* DCU clock disable */

#define	AR5311_RXCFG_DEF_RX_ANTENNA	0x00000008 /* Default Receive Antenna */

/*
 * NOTE: MAC_5211/MAC_5311 difference
 * On Oahu the TX latency field has increased from 6 bits to 9 bits.
 * The RX latency field is unchanged but is shifted over 3 bits.
 */
#define	AR5311_USEC_TX_LAT_M	0x000FC000 /* tx latency (usec) */
#define	AR5311_USEC_TX_LAT_S	14
#define	AR5311_USEC_RX_LAT_M	0x03F00000 /* rx latency (usec) */
#define	AR5311_USEC_RX_LAT_S	20

/*
 * NOTE: MAC_5211/MAC_5311 difference
 * On Maui2/Spirit the frame sequence number is controlled per DCU.
 * On Oahu the frame sequence number is global across all DCUs and
 * is controlled
 */
#define	AR5311_D_MISC_SEQ_NUM_CONTROL	0x01000000 /* seq num local or global */
#define	AR5311_DIAG_USE_ECO	0x00000400	/* "super secret" enable ECO */

#endif /* _DEV_ATH_AR5311REG_H_ */
