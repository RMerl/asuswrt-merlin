/*
 * Generic GPIO Interface
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: gpio.h,v 1.1 2009/11/02 19:10:00 Exp $
 */

int gpio_direction_input(unsigned pin);
int gpio_direction_output(unsigned pin, int value);

int gpio_get_value(unsigned int gpio);
void gpio_set_value(unsigned int gpio, int value);

int gpio_request(unsigned int gpio, const char *label);
void gpio_free(unsigned int gpio);
