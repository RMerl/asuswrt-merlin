/*
 * BCM43XX PCI/E core sw API definitions.
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
 * $Id: nicpci.h 451514 2014-01-27 00:24:19Z $
 */

#ifndef	_NICPCI_H
#define	_NICPCI_H

#if defined(BCMDHDUSB) || (defined(BCMBUSTYPE) && (BCMBUSTYPE == SI_BUS))
#define pcicore_find_pci_capability(a, b, c, d) (0)
#define pcie_readreg(a, b, c, d) (0)
#define pcie_writereg(a, b, c, d, e) (0)

#define pcie_clkreq(a, b, c)	(0)
#define pcie_lcreg(a, b, c)	(0)
#define pcie_ltrenable(a, b, c)	(0)
#define pcie_obffenable(a, b, c)	(0)
#define pcie_ltr_reg(a, b, c, d)	(0)
#define pcieltrspacing_reg(a, b, c)	(0)
#define pcieltrhysteresiscnt_reg(a, b, c)	(0)

#define pcicore_init(a, b, c) (0x0dadbeef)
#define pcicore_deinit(a)	do { } while (0)
#define pcicore_attach(a, b, c)	do { } while (0)
#define pcicore_hwup(a)		do { } while (0)
#define pcicore_up(a, b)	do { } while (0)
#define pcicore_sleep(a)	do { } while (0)
#define pcicore_down(a, b)	do { } while (0)

#define pcie_war_ovr_aspm_update(a, b)	do { } while (0)
#define pcie_power_save_enable(a, b)	do { } while (0)

#define pcicore_pcieserdesreg(a, b, c, d, e) (0)
#define pcicore_pciereg(a, b, c, d, e) (0)
#if defined(WLTEST) || defined(BCMDBG)
#define pcicore_dump_pcieinfo(a, b) (0)
#endif 
#ifdef BCMDBG
#define pcie_lcreg(a, b, c) (0)
#define pcicore_dump(a, b)	do { } while (0)
#endif

#define pcicore_pmecap_fast(a)	(FALSE)
#define pcicore_pmeen(a)	do { } while (0)
#define pcicore_pmeclr(a)	do { } while (0)
#define pcicore_pmestat(a)	(FALSE)
#define pcicore_pmestatclr(a)	do { } while (0)
#define pcie_set_request_size(pch, size) do { } while (0)
#define pcie_get_request_size(pch) (0)
#define pcie_set_maxpayload_size(pch, size) do { } while (0)
#define pcie_get_maxpayload_size(pch) (0)
#define pcie_get_ssid(a) (0)
#define pcie_get_bar0(a) (0)
#define pcie_configspace_cache(a) (0)
#define pcie_configspace_restore(a) (0)
#define pcie_configspace_get(a, b, c) (0)
#define pcie_set_L1_entry_time(a, b) do { } while (0)
#define pcie_disable_TL_clk_gating(a) do { } while (0)
#define pcie_get_link_speed(a) (0)
#define pcie_set_error_injection(a, b) do { } while (0)
#define pcie_set_L1substate(a, b) do { } while (0)
#define pcie_get_L1substate(a) (0)
#define pcie_survive_perst(a, b, c) (0)
#define pcie_ltr_war(a, b)  do { } while (0)
#define pcie_hw_LTR_war(a)  do { } while (0)
#define pcie_hw_L1SS_war(a)  do { } while (0)
#define pciedev_crwlpciegen2(a)	do { } while (0)
#define pciedev_prep_D3(a, b)	do { } while (0)
#define pciedev_reg_pm_clk_period(a)  do { } while (0)
#define pcie_set_ctrlreg(a, b, c) (0)
#else
struct sbpcieregs;

extern uint8 pcicore_find_pci_capability(osl_t *osh, uint8 req_cap_id,
                                         uchar *buf, uint32 *buflen);
extern uint pcie_readreg(si_t *sih, struct sbpcieregs *pcieregs, uint addrtype, uint offset);
extern uint pcie_writereg(si_t *sih, struct sbpcieregs *pcieregs, uint addrtype, uint offset,
                          uint val);

extern uint8 pcie_clkreq(void *pch, uint32 mask, uint32 val);
extern uint32 pcie_lcreg(void *pch, uint32 mask, uint32 val);
extern void pcie_set_L1_entry_time(void *pch, uint32 val);
extern void pcie_disable_TL_clk_gating(void *pch);
extern void pcie_set_error_injection(void *pch, uint32 mode);
extern uint8 pcie_ltrenable(void *pch, uint32 mask, uint32 val);
extern uint8 pcie_obffenable(void *pch, uint32 mask, uint32 val);
extern void pcie_set_L1substate(void *pch, uint32 substate);
extern uint32 pcie_get_L1substate(void *pch);
extern uint32 pcie_ltr_reg(void *pch, uint32 reg, uint32 mask, uint32 val);
extern uint32 pcieltrspacing_reg(void *pch, uint32 mask, uint32 val);
extern uint32 pcieltrhysteresiscnt_reg(void *pch, uint32 mask, uint32 val);

extern void *pcicore_init(si_t *sih, osl_t *osh, void *regs);
extern void pcicore_deinit(void *pch);
extern void pcicore_attach(void *pch, char *pvars, int state);
extern void pcicore_hwup(void *pch);
extern void pcicore_up(void *pch, int state);
extern void pcicore_sleep(void *pch);
extern void pcicore_down(void *pch, int state);

extern void pcie_war_ovr_aspm_update(void *pch, uint8 aspm);
extern void pcie_power_save_enable(void *pch, bool enable);

extern uint32 pcicore_pcieserdesreg(void *pch, uint32 mdioslave, uint32 offset,
                                    uint32 mask, uint32 val);

extern uint32 pcicore_pciereg(void *pch, uint32 offset, uint32 mask, uint32 val, uint type);

#if defined(WLTEST) || defined(BCMDBG)
extern int pcicore_dump_pcieinfo(void *pch, struct bcmstrbuf *b);
#endif


#ifdef BCMDBG
extern void pcicore_dump(void *pch, struct bcmstrbuf *b);
#endif

extern bool pcicore_pmecap_fast(osl_t *osh);
extern void pcicore_pmeen(void *pch);
extern void pcicore_pmeclr(void *pch);
extern void pcicore_pmestatclr(void *pch);
extern bool pcicore_pmestat(void *pch);
extern void pcie_set_request_size(void *pch, uint16 size);
extern uint16 pcie_get_request_size(void *pch);
extern void pcie_set_maxpayload_size(void *pch, uint16 size);
extern uint16 pcie_get_maxpayload_size(void *pch);
extern uint16 pcie_get_ssid(void *pch);
extern uint32 pcie_get_bar0(void *pch);
extern int pcie_configspace_cache(void* pch);
extern int pcie_configspace_restore(void* pch);
extern int pcie_configspace_get(void* pch, uint8 *buf, uint size);
extern uint32 pcie_get_link_speed(void* pch);
extern uint32 pcie_survive_perst(void* pch, uint32 mask, uint32 val);
extern void pcie_ltr_war(void *pch, bool enable);
extern void pcie_hw_LTR_war(void *pch);
extern void pcie_hw_L1SS_war(void *pch);
extern void pciedev_crwlpciegen2(void *pch);
extern void pciedev_prep_D3(void *pch, bool enter_D3);
extern void pciedev_reg_pm_clk_period(void *pch);
extern uint32 pcie_set_ctrlreg(void* pch, uint32 mask, uint32 val);
#endif 

#define PCIE_MRRS_OVERRIDE(sih) \
	((pi->sih->boardvendor == VENDOR_APPLE) && \
	 ((pi->sih->boardtype == BCM94331X19) || \
	  (pi->sih->boardtype == BCM94331X28) || \
	  (pi->sih->boardtype == BCM94331X28B) || \
	  (pi->sih->boardtype == BCM94331X29B) || \
	  (pi->sih->boardtype == BCM94331X29D) || \
	  (pi->sih->boardtype == BCM94331X19C) || \
	  (pi->sih->boardtype == BCM94331X33)))

#define PCIE_DRIVE_STRENGTH_OVERRIDE(sih) \
	((CHIPID((sih)->chip) == BCM4331_CHIP_ID) && \
	 ((sih)->boardtype == BCM94331X19 || \
	  (sih)->boardtype == BCM94331X28 || \
	  (sih)->boardtype == BCM94331X29B || \
	  (sih)->boardtype == BCM94331X19C))
#endif	/* _NICPCI_H */
