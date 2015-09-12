/*
 * Broadcom SPI Low-Level Hardware Driver API
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmspi.h 241182 2011-02-17 21:50:03Z $
 */
#ifndef	_BCM_SPI_H
#define	_BCM_SPI_H

extern void spi_devintr_off(sdioh_info_t *sd);
extern void spi_devintr_on(sdioh_info_t *sd);
extern bool spi_start_clock(sdioh_info_t *sd, uint16 new_sd_divisor);
extern bool spi_controller_highspeed_mode(sdioh_info_t *sd, bool hsmode);
extern bool spi_check_client_intr(sdioh_info_t *sd, int *is_dev_intr);
extern bool spi_hw_attach(sdioh_info_t *sd);
extern bool spi_hw_detach(sdioh_info_t *sd);
extern void spi_sendrecv(sdioh_info_t *sd, uint8 *msg_out, uint8 *msg_in, int msglen);
extern void spi_spinbits(sdioh_info_t *sd);
extern void spi_waitbits(sdioh_info_t *sd, bool yield);

#endif /* _BCM_SPI_H */
