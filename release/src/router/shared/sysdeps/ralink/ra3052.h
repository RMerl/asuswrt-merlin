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
#ifndef RA3052H
#define RA3052H

#define LED_CONTROL(led,flag)   ra_gpio_write_spec(led, flag)

#define LED_POWER       RTN12_PWRLED_GPIO_IRQ
#define BTN_RESET       RTN12_RESETDF_GPIO_IRQ

#define RTN12_PWRLED_GPIO_IRQ   7
#define RTN12_RESETDF_GPIO_IRQ  10
#define RTN12_EZSETUP_GPIO_IRQ  0
#define RTN12_SW1		9
#define RTN12_SW2		13
#define RTN12_SW3		11

#define LED_ON  0       // low active (all 5xx series)
#define LED_OFF 1

unsigned long task_mask;

int switch_init(void);

void switch_fini(void);

int ra3052_reg_read(int offset, int *value);

int ra3052_reg_write(int offset, int value);

int config_3052(int type);

int restore_3052();

void ra_gpio_write_spec(bit_idx, flag);

int check_all_tasks();

#endif
