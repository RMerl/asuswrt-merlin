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
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#ifndef _NETDEV_LED_H_
#define _NETDEV_LED_H_

/* check-netdev.c */
extern unsigned int ndev_check_traffic(struct bled_priv *bled_priv);
extern int ndev_reset_check_traffic(struct bled_priv *bp);
extern void netdev_led_printer(struct bled_priv *b, struct seq_file *m);

/* check platform-specific switch port traffic. e.g., check-ar8216-qca8337.c */
extern unsigned int swports_check_traffic(struct bled_priv *bp);
extern int swports_reset_check_traffic(struct bled_priv *bp);
extern void swports_led_printer(struct bled_priv *bp, struct seq_file *m);

/* check-usbbus.c */
#if defined(RTCONFIG_USB)
extern unsigned int usbbus_check_traffic(struct bled_priv *bp);
extern int usbbus_reset_check_traffic(struct bled_priv *bp);
extern void usbbus_led_printer(struct bled_priv *bp, struct seq_file *m);
#else
static inline int handle_add_usbbus_bled(unsigned long arg) { return -EINVAL; }
static inline void usbbus_led_printer(struct bled_priv *bp, struct seq_file *m) { }
#endif

/* check-interrupt.c */
extern unsigned int interrupt_check_traffic(struct bled_priv *bled_priv);
extern int interrupt_reset_check_traffic(struct bled_priv *bp);
extern void interrupt_led_printer(struct bled_priv *b, struct seq_file *m);

#endif	/* !_NETDEV_LED_H_ */
