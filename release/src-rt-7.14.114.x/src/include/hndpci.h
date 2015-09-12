/*
 * HND SiliconBackplane PCI core software interface.
 *
 * $Id: hndpci.h 270062 2011-07-01 00:27:12Z $
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
 */

#ifndef _hndpci_h_
#define _hndpci_h_

#define SI_PCI_MAXCORES		2

extern int hndpci_read_config(si_t *sih, uint bus, uint dev, uint func,
                              uint off, void *buf, int len);
extern int extpci_read_config(si_t *sih, uint bus, uint dev, uint func,
                              uint off, void *buf, int len);
extern int hndpci_write_config(si_t *sih, uint bus, uint dev, uint func,
                               uint off, void *buf, int len);
extern int extpci_write_config(si_t *sih, uint bus, uint dev, uint func,
                               uint off, void *buf, int len);
extern uint8 hndpci_find_pci_capability(si_t *sih, uint bus, uint dev, uint func,
                                        uint8 req_cap_id, uchar *buf, uint32 *buflen);
extern void hndpci_ban(uint16 core);
extern int hndpci_init(si_t *sih);
extern int hndpci_init_pci(si_t *sih, uint coreunit);
extern void hndpci_init_cores(si_t *sih);
extern void hndpci_arb_park(si_t *sih, uint parkid);
extern bool hndpci_is_hostbridge(uint bus, uint dev);
extern uint32 hndpci_get_membase(uint bus);
extern void hndpci_deinit(si_t *sih);
extern int hndpci_deinit_pci(si_t *sih, uint coreunit);

#define PCI_PARK_NVRAM    0xff

#endif /* _hndpci_h_ */
