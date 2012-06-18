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
#ifndef _RTL8367R_H_
#define _RTL8367R_H_

int config8367r(int argc, char *argv[]);

int ralink_gpio_write_bit(int idx, int value);
int ralink_gpio_read_bit(int idx);
int ralink_gpio_init(unsigned int idx, int dir);
uint32_t ralink_gpio_read(void);
int ralink_gpio_write(uint32_t bit, int en);

int rtl8367r_LanPort_linkUp();
int rtl8367r_LanPort_linkDown();
int rtl8367r_AllPort_linkUp();
int rtl8367r_AllPort_linkDown();
void rtl8367r_AllPort_phyState();

//
int dsl_wanPort_phyStatus();


#endif
