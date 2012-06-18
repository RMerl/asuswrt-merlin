/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on Samsung CMC-730 chip.
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _TUN_DEV_H
#define _TUN_DEV_H

int tap_open(char *dev);
int tap_close(int fd, char *dev);

int tap_write(int fd, const void *buf, int len);
int tap_read(int fd, void *buf, int len);

int tap_set_hwaddr(int fd, const char *dev, unsigned char *hwaddr);
int tap_set_mtu(int fd, const char *dev, int mtu);

int tap_bring_up(int fd, const char *dev);
int tap_bring_down(int fd, const char *dev);

#endif // _TUN_DEV_H

