/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef _QCA8081_PHY_H_
#define _QCA8081_PHY_H_

/* PHY Registers */
#define QCA8081_PHY_CONTROL			0
#define QCA8081_PHY_STATUS			1
#define QCA8081_PHY_SPEC_STATUS			17

#define QCA8081_PHY_ID1				2
#define QCA8081_PHY_ID2				3
#define QCA8081_AUTONEG_ADVERT			4
#define QCA8081_LINK_PARTNER_ABILITY		5
#define QCA8081_1000BASET_CONTROL		9
#define QCA8081_1000BASET_STATUS		10
#define QCA8081_MMD_CTRL_REG			13
#define QCA8081_MMD_DATA_REG			14
#define QCA8081_EXTENDED_STATUS			15
#define QCA8081_PHY_SPEC_CONTROL		16
#define QCA8081_PHY_INTR_MASK			18
#define QCA8081_PHY_INTR_STATUS			19
#define QCA8081_PHY_CDT_CONTROL			22
#define QCA8081_DEBUG_PORT_ADDRESS		29
#define QCA8081_DEBUG_PORT_DATA			30

#define QCA8081_STATUS_LINK_PASS		0x0400

#define QCA8081_STATUS_FULL_DUPLEX		0x2000

#define QCA8081_STATUS_SPEED_MASK		0x380
#define QCA8081_STATUS_SPEED_2500MBS		0x200
#define QCA8081_STATUS_SPEED_1000MBS		0x100
#define QCA8081_STATUS_SPEED_100MBS		0x80
#define QCA8081_STATUS_SPEED_10MBS		0x0000

#endif				/* _QCA8081_PHY_H_ */
