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
#include <common.h>
#include <net.h>
#include <asm-generic/errno.h>
#include <asm/io.h>
#include <malloc.h>
#include <phy.h>
#include "ipq_qca8081.h"
#include "ipq_phy.h"

extern int ipq_mdio_read(int mii_id,
		int regnum, ushort *data);

u16 qca8081_phy_reg_read(u32 dev_id, u32 phy_id, u32 reg_id)
{
	return ipq_mdio_read(phy_id, reg_id, NULL);
}

u8 qca8081_phy_get_link_status(u32 dev_id, u32 phy_id)
{
	u16 phy_data;
	phy_data = qca8081_phy_reg_read(dev_id,
			phy_id, QCA8081_PHY_SPEC_STATUS);
	if (phy_data & QCA8081_STATUS_LINK_PASS)
		return 0;

	return 1;
}

u32 qca8081_phy_get_duplex(u32 dev_id, u32 phy_id, fal_port_duplex_t *duplex)
{
	u16 phy_data;

	phy_data = qca8081_phy_reg_read(dev_id, phy_id,
				QCA8081_PHY_SPEC_STATUS);

	/*
	 * Read duplex
	 */
	if (phy_data & QCA8081_STATUS_FULL_DUPLEX)
		*duplex = FAL_FULL_DUPLEX;
	else
		*duplex = FAL_HALF_DUPLEX;

	return 0;
}

u32 qca8081_phy_get_speed(u32 dev_id, u32 phy_id, fal_port_speed_t *speed)
{
	u16 phy_data;

	phy_data = qca8081_phy_reg_read(dev_id,
			phy_id, QCA8081_PHY_SPEC_STATUS);

	switch (phy_data & QCA8081_STATUS_SPEED_MASK) {
	case QCA8081_STATUS_SPEED_2500MBS:
		*speed = FAL_SPEED_2500;
		break;
	case QCA8081_STATUS_SPEED_1000MBS:
		*speed = FAL_SPEED_1000;
		break;
	case QCA8081_STATUS_SPEED_100MBS:
		*speed = FAL_SPEED_100;
		break;
	case QCA8081_STATUS_SPEED_10MBS:
		*speed = FAL_SPEED_10;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int ipq_qca8081_phy_init(struct phy_ops **ops, u32 phy_id)
{
	u16 phy_data;
	struct phy_ops *qca8081_ops;
	qca8081_ops = (struct phy_ops *)malloc(sizeof(struct phy_ops));
	if (!qca8081_ops)
		return -ENOMEM;
	qca8081_ops->phy_get_link_status = qca8081_phy_get_link_status;
	qca8081_ops->phy_get_speed = qca8081_phy_get_speed;
	qca8081_ops->phy_get_duplex = qca8081_phy_get_duplex;
	*ops = qca8081_ops;

	phy_data = qca8081_phy_reg_read(0x0, phy_id, QCA8081_PHY_ID1);
	printf ("PHY ID1: 0x%x\n", phy_data);
	phy_data = qca8081_phy_reg_read(0x0, phy_id, QCA8081_PHY_ID2);
	printf ("PHY ID2: 0x%x\n", phy_data);

	return 0;
}
