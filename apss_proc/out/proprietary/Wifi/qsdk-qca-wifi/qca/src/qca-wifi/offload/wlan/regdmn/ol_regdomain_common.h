/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *
 * This module contains the common regulatory domain database tables:
 *
 * 	- reg domain enum constants
 *	- reg domain enum to reg domain pair mappings
 * 	- country to regdomain mappings
 *	- channel tag enums and the frequency-to-frequency band mappings
 *	  for all the modes
 *
 * "The country table and respective Regulatory Domain channel and power
 * settings are based on available knowledge as of software release. The
 * underlying global regulatory and spectrum rules change on a regular basis,
 * therefore, no warranty is given that the channel and power information
 * herein is complete, accurate or up to date.  Developers are responsible
 * for regulatory compliance of end-products developed using the enclosed
 * data per all applicable national requirements.  Furthermore, data in this
 * table does not guarantee that spectrum is available and that regulatory
 * approval is possible in every case. Knowldegable regulatory compliance
 * or government contacts should be consulted by the manufacturer to ensure
 * that the most current and accurate settings are used in each end-product.
 * This table was designed so that developers are able to update the country
 * table mappings as well as the Regulatory Domain definitions in order to
 * incorporate the most current channel and power settings in the end-product."
 *
 */

/* Enumerated Regulatory Domain Information 8 bit values indicate that
 * the regdomain is really a pair of unitary regdomains.  12 bit values
 * are the real unitary regdomains and are the only ones which have the
 * frequency bitmasks and flags set.
 */

#define WIRELESS_MODES_2G (WMI_HOST_REGDMN_MODE_11B | WMI_HOST_REGDMN_MODE_PUREG \
            | WMI_HOST_REGDMN_MODE_11G | WMI_HOST_REGDMN_MODE_108G \
            | WMI_HOST_REGDMN_MODE_11NG_HT20 | WMI_HOST_REGDMN_MODE_11NG_HT40PLUS \
            | WMI_HOST_REGDMN_MODE_11NG_HT40MINUS  | WMI_HOST_REGDMN_MODE_11AXG_HE20 \
            | WMI_HOST_REGDMN_MODE_11AXG_HE40PLUS | WMI_HOST_REGDMN_MODE_11AXG_HE40MINUS)

#define WIRELESS_MODES_5G (WMI_HOST_REGDMN_MODE_11A | WMI_HOST_REGDMN_MODE_TURBO \
            | WMI_HOST_REGDMN_MODE_108A \
            | WMI_HOST_REGDMN_MODE_11A_HALF_RATE | WMI_HOST_REGDMN_MODE_11A_QUARTER_RATE \
            | WMI_HOST_REGDMN_MODE_11NA_HT20 | WMI_HOST_REGDMN_MODE_11NA_HT40PLUS \
            | WMI_HOST_REGDMN_MODE_11NA_HT40MINUS | WMI_HOST_REGDMN_MODE_11AC_VHT20 \
            | WMI_HOST_REGDMN_MODE_11AC_VHT40PLUS | WMI_HOST_REGDMN_MODE_11AC_VHT40MINUS \
            | WMI_HOST_REGDMN_MODE_11AC_VHT80 | WMI_HOST_REGDMN_MODE_11AC_VHT160 \
            | WMI_HOST_REGDMN_MODE_11AC_VHT80_80 | WMI_HOST_REGDMN_MODE_11AXA_HE20 \
            | WMI_HOST_REGDMN_MODE_11AXA_HE40PLUS | WMI_HOST_REGDMN_MODE_11AXA_HE40MINUS \
            | WMI_HOST_REGDMN_MODE_11AXA_HE80 | WMI_HOST_REGDMN_MODE_11AXA_HE160 \
            | WMI_HOST_REGDMN_MODE_11AXA_HE80_80)
