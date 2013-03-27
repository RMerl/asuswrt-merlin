/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _RTKSWITCH_H_
#define _RTKSWITCH_H_

extern int config_rtkswitch(int argc, char *argv[]);

extern int ralink_gpio_write_bit(int idx, int value);
extern int ralink_gpio_read_bit(int idx);
extern int ralink_gpio_init(unsigned int idx, int dir);
extern uint32_t ralink_gpio_read(void);
extern int ralink_gpio_write(uint32_t bit, int en);

extern int rtkswitch_LanPort_linkUp();
extern int rtkswitch_LanPort_linkDown();
extern int rtkswitch_AllPort_linkUp();
extern int rtkswitch_AllPort_linkDown();
extern void rtkswitch_AllPort_phyState();

#endif
