/*
 * This file is part of TSC2005 touchscreen driver
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Contact: Aaro Koskinen <aaro.koskinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef _LINUX_SPI_TSC2005_H
#define _LINUX_SPI_TSC2005_H

#include <linux/types.h>

struct tsc2005_platform_data {
	int		ts_pressure_max;
	int		ts_pressure_fudge;
	int		ts_x_max;
	int		ts_x_fudge;
	int		ts_y_max;
	int		ts_y_fudge;
	int		ts_x_plate_ohm;
	unsigned int	esd_timeout_ms;
	void		(*set_reset)(bool enable);
};

#endif
