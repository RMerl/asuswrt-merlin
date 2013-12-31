/*
 * Code to operate on PCI/E core, in NIC mode
 * Implements pci_api.h
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: nicpci.c 419467 2013-08-21 09:19:48Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <bcmdevs.h>
#include <sbchipc.h>
#include <pci_core.h>
#include <pcie_core.h>
#include <nicpci.h>
#include <pcicfg.h>

typedef struct {
	union {
		sbpcieregs_t *pcieregs;
		sbpciregs_t *pciregs;
	} regs;                         /* Memory mapped register to the core */

	si_t 	*sih;					/* System interconnect handle */
	osl_t 	*osh;					/* OSL handle */
	uint8	pciecap_lcreg_offset;	/* PCIE capability LCreg offset in the config space */
	uint8	pciecap_devctrl_offset;	/* PCIE DevControl reg offset in the config space */
	bool	pcie_pr42767;
	uint8	pcie_polarity;
	uint8	pcie_war_aspm_ovr;	/* Override ASPM/Clkreq settings */
	uint8	pmecap_offset;		/* PM Capability offset in the config space */
	bool 	pmecap;				/* Capable of generating PME */
	bool	pcie_power_save;
	uint16	pmebits;
	uint16	pcie_reqsize;
	uint16	pcie_mps;
	uint8	pciecap_devctrl2_offset; /* PCIE DevControl2 reg offset in the config space */
	uint32	pciecap_ltr0_reg_offset; /* PCIE LTR0 reg offset in the config space */
	uint32	pciecap_ltr1_reg_offset; /* PCIE LTR1 reg offset in the config space */
	uint32	pciecap_ltr2_reg_offset; /* PCIE LTR2 reg offset in the config space */
	uint8	pcie_configspace[PCI_CONFIG_SPACE_SIZE];
} pcicore_info_t;

/* debug/trace */
#ifdef BCMDBG_ERR
#define	PCI_ERROR(args)	printf args
#else
#define	PCI_ERROR(args)
#endif	/* BCMDBG_ERR */

/* routines to access mdio slave device registers */
static bool pcie_mdiosetblock(pcicore_info_t *pi,  uint blk);
static int pcie_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write, uint *val);
static int pciegen1_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write,
	uint *val);
static int pciegen2_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write,
	uint *val, bool slave_bypass);
static int pcie_mdiowrite(pcicore_info_t *pi, uint physmedia, uint readdr, uint val);
static int pcie_mdioread(pcicore_info_t *pi, uint physmedia, uint readdr, uint *ret_val);

static void pcie_extendL1timer(pcicore_info_t *pi, bool extend);
static void pcie_clkreq_upd(pcicore_info_t *pi, uint state);

static void pcie_war_aspm_clkreq(pcicore_info_t *pi);
static void pcie_war_serdes(pcicore_info_t *pi);
static void pcie_war_noplldown(pcicore_info_t *pi);
static void pcie_war_polarity(pcicore_info_t *pi);
static void pcie_war_pci_setup(pcicore_info_t *pi);
static void pcie_power_save_upd(pcicore_info_t *pi, bool up);

static bool pcicore_pmecap(pcicore_info_t *pi);
static void pcicore_fixlatencytimer(pcicore_info_t* pch, uint8 timer_val);

#define PCIE_GEN1(sih) ((BUSTYPE((sih)->bustype) == PCI_BUS) && \
	((sih)->buscoretype == PCIE_CORE_ID))
#define PCIE_GEN2(sih) ((BUSTYPE((sih)->bustype) == PCI_BUS) &&	\
	((sih)->buscoretype == PCIE2_CORE_ID))
#define PCIE(sih) (PCIE_GEN1(sih) || PCIE_GEN2(sih))

#define PCIEGEN1_ASPM(sih)	((PCIE_GEN1(sih)) &&	\
	(((sih)->buscorerev >= 3) && ((sih)->buscorerev <= 5)))

#define DWORD_ALIGN(x)  (x & ~(0x03))
#define BYTE_POS(x) (x & 0x3)
#define WORD_POS(x) (x & 0x1)

#define BYTE_SHIFT(x)  (8 * BYTE_POS(x))
#define WORD_SHIFT(x)  (16 * WORD_POS(x))

#define BYTE_VAL(a, x) ((a >> BYTE_SHIFT(x)) & 0xFF)
#define WORD_VAL(a, x) ((a >> WORD_SHIFT(x)) & 0xFFFF)

#define read_pci_cfg_byte(a) \
	(BYTE_VAL(OSL_PCI_READ_CONFIG(osh, DWORD_ALIGN(a), 4), a) & 0xff)

#define read_pci_cfg_word(a) \
	(WORD_VAL(OSL_PCI_READ_CONFIG(osh, DWORD_ALIGN(a), 4), a) & 0xffff)

#define write_pci_cfg_byte(a, val) do { \
	uint32 tmpval; \
	tmpval = (OSL_PCI_READ_CONFIG(osh, DWORD_ALIGN(a), 4) & ~0xFF << BYTE_POS(a)) | \
	        val << BYTE_POS(a); \
	OSL_PCI_WRITE_CONFIG(osh, DWORD_ALIGN(a), 4, tmpval); \
	} while (0)

#define write_pci_cfg_word(a, val) do { \
	uint32 tmpval; \
	tmpval = (OSL_PCI_READ_CONFIG(osh, DWORD_ALIGN(a), 4) & ~0xFFFF << WORD_POS(a)) | \
	        val << WORD_POS(a); \
	OSL_PCI_WRITE_CONFIG(osh, DWORD_ALIGN(a), 4, tmpval); \
	} while (0)

/* delay needed between the mdio control/ mdiodata register data access */
#define PR28829_DELAY() OSL_DELAY(10)

/**
 * Initialize the PCI core. It's caller's responsibility to make sure that this is done
 * only once
 */
void *
pcicore_init(si_t *sih, osl_t *osh, void *regs)
{
	pcicore_info_t *pi;
	uint8 cap_ptr;

	ASSERT(sih->bustype == PCI_BUS);

	/* alloc pcicore_info_t */
	if ((pi = MALLOC(osh, sizeof(pcicore_info_t))) == NULL) {
		PCI_ERROR(("pci_attach: malloc failed! malloced %d bytes\n", MALLOCED(osh)));
		return (NULL);
	}

	bzero(pi, sizeof(pcicore_info_t));

	pi->sih = sih;
	pi->osh = osh;

	if (sih->buscoretype == PCIE2_CORE_ID) {
		pi->regs.pcieregs = (sbpcieregs_t*)regs;
		cap_ptr = pcicore_find_pci_capability(pi->osh, PCI_CAP_PCIECAP_ID, NULL, NULL);
		ASSERT(cap_ptr);
		pi->pciecap_devctrl_offset = cap_ptr + PCIE_CAP_DEVCTRL_OFFSET;
		pi->pciecap_lcreg_offset = cap_ptr + PCIE_CAP_LINKCTRL_OFFSET;
		pi->pciecap_devctrl2_offset = cap_ptr + PCIE_CAP_DEVCTRL2_OFFSET;
		pi->pciecap_ltr0_reg_offset = cap_ptr + PCIE_CAP_LTR0_REG_OFFSET;
		pi->pciecap_ltr1_reg_offset = cap_ptr + PCIE_CAP_LTR1_REG_OFFSET;
		pi->pciecap_ltr2_reg_offset = cap_ptr + PCIE_CAP_LTR2_REG_OFFSET;
	} else if (sih->buscoretype == PCIE_CORE_ID) {
		pi->regs.pcieregs = (sbpcieregs_t*)regs;
		cap_ptr = pcicore_find_pci_capability(pi->osh, PCI_CAP_PCIECAP_ID, NULL, NULL);
		ASSERT(cap_ptr);
		pi->pciecap_lcreg_offset = cap_ptr + PCIE_CAP_LINKCTRL_OFFSET;
		pi->pciecap_devctrl_offset = cap_ptr + PCIE_CAP_DEVCTRL_OFFSET;
		pi->pciecap_devctrl2_offset = cap_ptr + PCIE_CAP_DEVCTRL2_OFFSET;
		pi->pciecap_ltr0_reg_offset = cap_ptr + PCIE_CAP_LTR0_REG_OFFSET;
		pi->pciecap_ltr1_reg_offset = cap_ptr + PCIE_CAP_LTR1_REG_OFFSET;
		pi->pciecap_ltr2_reg_offset = cap_ptr + PCIE_CAP_LTR2_REG_OFFSET;
		pi->pcie_power_save = TRUE; /* Enable pcie_power_save by default */
	} else
		pi->regs.pciregs = (sbpciregs_t*)regs;

	return pi;
}

void
pcicore_deinit(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;


	if (pi == NULL)
		return;
	MFREE(pi->osh, pi, sizeof(pcicore_info_t));
}

/** return cap_offset if requested capability exists in the PCI config space */
/* Note that it's caller's responsibility to make sure it's a pci bus */
uint8
pcicore_find_pci_capability(osl_t *osh, uint8 req_cap_id, uchar *buf, uint32 *buflen)
{
	uint8 cap_id;
	uint8 cap_ptr = 0;
	uint32 bufsize;
	uint8 byte_val;

	/* check for Header type 0 */
	byte_val = read_pci_cfg_byte(PCI_CFG_HDR);
	if ((byte_val & 0x7f) != PCI_HEADER_NORMAL)
		goto end;

	/* check if the capability pointer field exists */
	byte_val = read_pci_cfg_byte(PCI_CFG_STAT);
	if (!(byte_val & PCI_CAPPTR_PRESENT))
		goto end;

	cap_ptr = read_pci_cfg_byte(PCI_CFG_CAPPTR);
	/* check if the capability pointer is 0x00 */
	if (cap_ptr == 0x00)
		goto end;

	/* loop thr'u the capability list and see if the pcie capabilty exists */

	cap_id = read_pci_cfg_byte(cap_ptr);

	while (cap_id != req_cap_id) {
		cap_ptr = read_pci_cfg_byte((cap_ptr+1));
		if (cap_ptr == 0x00) break;
		cap_id = read_pci_cfg_byte(cap_ptr);
	}
	if (cap_id != req_cap_id) {
		goto end;
	}
	/* found the caller requested capability */
	if ((buf != NULL) && (buflen != NULL)) {
		uint8 cap_data;

		bufsize = *buflen;
		if (!bufsize) goto end;
		*buflen = 0;
		/* copy the cpability data excluding cap ID and next ptr */
		cap_data = cap_ptr + 2;
		if ((bufsize + cap_data)  > SZPCR)
			bufsize = SZPCR - cap_data;
		*buflen = bufsize;
		while (bufsize--) {
			*buf = read_pci_cfg_byte(cap_data);
			cap_data++;
			buf++;
		}
	}
end:
	return cap_ptr;
}

/** Register Access API */
uint
pcie_readreg(si_t *sih, sbpcieregs_t *pcieregs, uint addrtype, uint offset)
{
	uint retval = 0xFFFFFFFF;
	osl_t   *osh = si_osh(sih);

	ASSERT(pcieregs != NULL);
	BCM_REFERENCE(osh);

	if ((BUSTYPE(sih->bustype) == SI_BUS) || PCIE_GEN1(sih)) {
		switch (addrtype) {
			case PCIE_CONFIGREGS:
				W_REG(osh, (&pcieregs->configaddr), offset);
				(void)R_REG(osh, (&pcieregs->configaddr));
				retval = R_REG(osh, &(pcieregs->configdata));
				break;
			case PCIE_PCIEREGS:
				W_REG(osh, &(pcieregs->u.pcie1.pcieindaddr), offset);
				(void)R_REG(osh, (&pcieregs->u.pcie1.pcieindaddr));
				retval = R_REG(osh, &(pcieregs->u.pcie1.pcieinddata));
				break;
			default:
				ASSERT(0);
				break;
		}
	}
	else if (PCIE_GEN2(sih)) {
		W_REG(osh, (&pcieregs->configaddr), offset);
		(void)R_REG(osh, (&pcieregs->configaddr));
		retval = R_REG(osh, &(pcieregs->configdata));
	}

	return retval;
}

uint
pcie_writereg(si_t *sih, sbpcieregs_t *pcieregs, uint addrtype, uint offset, uint val)
{
	osl_t   *osh = si_osh(sih);

	ASSERT(pcieregs != NULL);
	BCM_REFERENCE(osh);

	if ((BUSTYPE(sih->bustype) == SI_BUS) || PCIE_GEN1(sih)) {
		switch (addrtype) {
			case PCIE_CONFIGREGS:
				W_REG(osh, (&pcieregs->configaddr), offset);
				W_REG(osh, (&pcieregs->configdata), val);
				break;
			case PCIE_PCIEREGS:
				W_REG(osh, (&pcieregs->u.pcie1.pcieindaddr), offset);
				W_REG(osh, (&pcieregs->u.pcie1.pcieinddata), val);
				break;
			default:
				ASSERT(0);
				break;
		}
	}
	else if (PCIE_GEN2(sih)) {
		W_REG(osh, (&pcieregs->configaddr), offset);
		W_REG(osh, (&pcieregs->configdata), val);
	}
	return 0;
}

static bool
pcie_mdiosetblock(pcicore_info_t *pi, uint blk)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint mdiodata, i = 0;
	uint pcie_serdes_spinwait = 200;

	mdiodata = MDIODATA_START | MDIODATA_WRITE | (MDIODATA_DEV_ADDR << MDIODATA_DEVADDR_SHF) |
	        (MDIODATA_BLK_ADDR << MDIODATA_REGADDR_SHF) | MDIODATA_TA | (blk << 4);
	W_REG(pi->osh, &pcieregs->u.pcie1.mdiodata, mdiodata);

	PR28829_DELAY();
	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		if (R_REG(pi->osh, &(pcieregs->u.pcie1.mdiocontrol)) & MDIOCTL_ACCESS_DONE) {
			break;
		}
		OSL_DELAY(1000);
		i++;
	}

	if (i >= pcie_serdes_spinwait) {
		PCI_ERROR(("pcie_mdiosetblock: timed out\n"));
		return FALSE;
	}

	return TRUE;
}

static bool
pcie2_mdiosetblock(pcicore_info_t *pi, uint blk)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint mdiodata, mdioctrl, i = 0;
	uint pcie_serdes_spinwait = 200;

	mdioctrl = MDIOCTL2_DIVISOR_VAL | (0x1F << MDIOCTL2_REGADDR_SHF);
	W_REG(pi->osh, &pcieregs->u.pcie2.mdiocontrol, mdioctrl);

	mdiodata = (blk << MDIODATA2_DEVADDR_SHF) | MDIODATA2_DONE;
	W_REG(pi->osh, &pcieregs->u.pcie2.mdiowrdata, mdiodata);

	PR28829_DELAY();
	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		if (!(R_REG(pi->osh, &(pcieregs->u.pcie2.mdiowrdata)) & MDIODATA2_DONE)) {
			break;
		}
		OSL_DELAY(1000);
		i++;
	}

	if (i >= pcie_serdes_spinwait) {
		PCI_ERROR(("pcie_mdiosetblock: timed out\n"));
		return FALSE;
	}

	return TRUE;
}

static int
pcie_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write, uint *val)
{
	if (PCIE_GEN1(pi->sih))
		return (pciegen1_mdioop(pi, physmedia, regaddr, write, val));
	else if (PCIE_GEN2(pi->sih))
		return (pciegen2_mdioop(pi, physmedia, regaddr, write, val, 0));
	else
		return 0xFFFFFFFF;
}

static int
pciegen2_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write, uint *val,
	bool slave_bypass)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint pcie_serdes_spinwait = 200, i = 0, mdio_ctrl;
	uint32 *reg32;

	if (!PCIE_GEN2(pi->sih))
		ASSERT(0);

	pcie2_mdiosetblock(pi, physmedia);

	/* enable mdio access to SERDES */
	mdio_ctrl = MDIOCTL2_DIVISOR_VAL;
	mdio_ctrl |= (regaddr << MDIOCTL2_REGADDR_SHF);

	if (slave_bypass)
		mdio_ctrl |= MDIOCTL2_SLAVE_BYPASS;

	if (!write)
		mdio_ctrl |= MDIOCTL2_READ;

	W_REG(pi->osh, (&pcieregs->u.pcie2.mdiocontrol), mdio_ctrl);
	if (write) {
		reg32 =  (uint32 *)&(pcieregs->u.pcie2.mdiowrdata);
		W_REG(pi->osh, reg32, *val | MDIODATA2_DONE);
	}
	else
		reg32 =  (uint32 *)&(pcieregs->u.pcie2.mdiorddata);

	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		if (!(R_REG(pi->osh, reg32) & MDIODATA2_DONE)) {
			if (!write)
				*val = (R_REG(pi->osh, reg32) & MDIODATA2_MASK);
			return 0;
		}
		OSL_DELAY(1000);
		i++;
	}
	return 0;
}

static int
pciegen1_mdioop(pcicore_info_t *pi, uint physmedia, uint regaddr, bool write, uint *val)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint mdiodata;
	uint i = 0;
	uint pcie_serdes_spinwait = 10;

	if (!PCIE_GEN1(pi->sih))
		ASSERT(0);

	/* enable mdio access to SERDES */
	W_REG(pi->osh, (&pcieregs->u.pcie1.mdiocontrol), MDIOCTL_PREAM_EN | MDIOCTL_DIVISOR_VAL);

	if (pi->sih->buscorerev >= 10) {
		/* new serdes is slower in rw, using two layers of reg address mapping */
		if (!pcie_mdiosetblock(pi, physmedia))
			return 1;
		mdiodata = (MDIODATA_DEV_ADDR << MDIODATA_DEVADDR_SHF) |
			(regaddr << MDIODATA_REGADDR_SHF);
		pcie_serdes_spinwait *= 20;
	} else {
		mdiodata = (physmedia << MDIODATA_DEVADDR_SHF_OLD) |
			(regaddr << MDIODATA_REGADDR_SHF_OLD);
	}

	if (!write)
		mdiodata |= (MDIODATA_START | MDIODATA_READ | MDIODATA_TA);
	else
		mdiodata |= (MDIODATA_START | MDIODATA_WRITE | MDIODATA_TA | *val);

	W_REG(pi->osh, &pcieregs->u.pcie1.mdiodata, mdiodata);

	PR28829_DELAY();

	/* retry till the transaction is complete */
	while (i < pcie_serdes_spinwait) {
		if (R_REG(pi->osh, &(pcieregs->u.pcie1.mdiocontrol)) & MDIOCTL_ACCESS_DONE) {
			if (!write) {
				PR28829_DELAY();
				*val = (R_REG(pi->osh, &(pcieregs->u.pcie1.mdiodata)) &
					MDIODATA_MASK);
			}
			/* Disable mdio access to SERDES */
			W_REG(pi->osh, (&pcieregs->u.pcie1.mdiocontrol), 0);
			return 0;
		}
		OSL_DELAY(1000);
		i++;
	}

	PCI_ERROR(("pcie_mdioop: timed out op: %d\n", write));
	/* Disable mdio access to SERDES */
	W_REG(pi->osh, (&pcieregs->u.pcie1.mdiocontrol), 0);
	return 1;
}

/** use the mdio interface to read from mdio slaves */
static int
pcie_mdioread(pcicore_info_t *pi, uint physmedia, uint regaddr, uint *regval)
{
	return pcie_mdioop(pi, physmedia, regaddr, FALSE, regval);
}

/** use the mdio interface to write to mdio slaves */
static int
pcie_mdiowrite(pcicore_info_t *pi, uint physmedia, uint regaddr, uint val)
{
	return pcie_mdioop(pi, physmedia, regaddr, TRUE, &val);
}

/* ***** Support functions ***** */

/**
 * By default, PCIe devices are not allowed to create payloads of greater than 128 bytes.
 * Maximum Read Request Size is a PCIe parameter that is advertized to the host, so the host can
 * choose a balance between high throughput and low 'chunkiness' on the bus. Regardless of the
 * setting of this (hardware) field, the core does not initiate read requests larger than 512 bytes.
 */
static uint32
pcie_devcontrol_mrrs(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint8 offset;

	offset = pi->pciecap_devctrl_offset;
	if (!offset)
		return 0;

	reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	/* set operation */
	if (mask) {
		if (val > PCIE_CAP_DEVCTRL_MRRS_128B) {
			if (PCIE_GEN1(pi->sih) && (pi->sih->buscorerev < 18)) {
				PCI_ERROR(("%s pcie corerev %d doesn't support >128B MRRS",
					__FUNCTION__, pi->sih->buscorerev));
				val = PCIE_CAP_DEVCTRL_MRRS_128B;
			}
		}

		reg_val &= ~PCIE_CAP_DEVCTRL_MRRS_MASK;
		reg_val |= (val << PCIE_CAP_DEVCTRL_MRRS_SHIFT) & PCIE_CAP_DEVCTRL_MRRS_MASK;

		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	}
	return reg_val;
}

static uint32
pcie_devcontrol_mps(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint8 offset;

	offset = pi->pciecap_devctrl_offset;
	if (!offset)
		return 0;

	reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	/* set operation */
	if (mask) {
		reg_val &= ~PCIE_CAP_DEVCTRL_MPS_MASK;
		reg_val |= (val << PCIE_CAP_DEVCTRL_MPS_SHIFT) & PCIE_CAP_DEVCTRL_MPS_MASK;

		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	}
	return reg_val;
}

uint8
pcie_clkreq(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint8 offset;

	offset = pi->pciecap_lcreg_offset;
	if (!offset)
		return 0;

	reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	/* set operation */
	if (mask) {
		if (val)
			reg_val |= PCIE_CLKREQ_ENAB;
		else
			reg_val &= ~PCIE_CLKREQ_ENAB;
		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	}
	if (reg_val & PCIE_CLKREQ_ENAB)
		return 1;
	else
		return 0;
}

uint8
pcie_ltrenable(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint8 offset;

	offset = pi->pciecap_devctrl2_offset;
	if (!offset)
		return 0;

	reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));

	/* set operation */
	if (mask) {
		if (val)
			reg_val |= PCIE_CAP_DEVCTRL2_LTR_ENAB_MASK;
		else
			reg_val &= ~PCIE_CAP_DEVCTRL2_LTR_ENAB_MASK;
		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	}
	if (reg_val & PCIE_CAP_DEVCTRL2_LTR_ENAB_MASK)
		return 1;
	else
		return 0;
}

/* JIRA:SWWLAN-28745
    val and return value:
	0  Disabled
	1  Enable using Message signaling[Var A]
	2  Enable using Message signaling[Var B]
	3  Enable using WAKE# signaling
*/
uint8
pcie_obffenable(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint8 offset;

	offset = pi->pciecap_devctrl2_offset;
	if (!offset)
		return 0;

	reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));

	/* set operation */
	if (mask) {
		reg_val = (reg_val & ~PCIE_CAP_DEVCTRL2_OBFF_ENAB_MASK) |
			((val << PCIE_CAP_DEVCTRL2_OBFF_ENAB_SHIFT) &
			PCIE_CAP_DEVCTRL2_OBFF_ENAB_MASK);
		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), reg_val);
		reg_val = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
	}

	return  (reg_val & PCIE_CAP_DEVCTRL2_OBFF_ENAB_MASK) >> PCIE_CAP_DEVCTRL2_OBFF_ENAB_SHIFT;
}

uint32
pcie_ltr_reg(void *pch, uint32 reg, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 reg_val;
	uint32 offset;

	if (PCIE_GEN1(pi->sih))
		return 0;

	if (reg == PCIE_CAP_LTR0_REG)
		offset = pi->pciecap_ltr0_reg_offset;
	else if (reg == PCIE_CAP_LTR1_REG)
		offset = pi->pciecap_ltr1_reg_offset;
	else if (reg == PCIE_CAP_LTR2_REG)
		offset = pi->pciecap_ltr2_reg_offset;
	else {
		PCI_ERROR(("pcie_ltr_reg: unsupported LTR register offset %d\n",
			reg));
		return 0;
	}

	if (!offset)
		return 0;

	if (mask) { /* set operation */
		reg_val = val;
		pcie_writereg(pi->sih, pi->regs.pcieregs, PCIE_CONFIGREGS, offset, reg_val);
	}
	else { /* get operation */
		reg_val = pcie_readreg(pi->sih, pi->regs.pcieregs, PCIE_CONFIGREGS, offset);
	}

	return reg_val;
}

uint32
pcieltrspacing_reg(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 retval;

	if (PCIE_GEN1(sih))
		return 0;

	ASSERT(pcieregs != NULL);

	if (mask) { /* set operation */
		retval = val;
		W_REG(pi->osh, &(pcieregs->ltrspacing), val);
	}
	else { /* get operation */
		retval = R_REG(pi->osh, &(pcieregs->ltrspacing));
	}

	return retval;
}

uint32
pcieltrhysteresiscnt_reg(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 retval;

	if (PCIE_GEN1(sih))
		return 0;

	ASSERT(pcieregs != NULL);

	if (mask) { /* set operation */
		retval = val;
		W_REG(pi->osh, &(pcieregs->ltrhysteresiscnt), val);
	}
	else { /* get operation */
		retval = R_REG(pi->osh, &(pcieregs->ltrhysteresiscnt));
	}

	return retval;
}

static void
pcie_extendL1timer(pcicore_info_t *pi, bool extend)
{
	uint32 w;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;

	if (!PCIE_GEN1(sih))
		return;

	w = pcie_readreg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_PMTHRESHREG);

	if (extend && sih->buscorerev >= 7)
		w |= PCIE_ASPMTIMER_EXTEND;
	else
		w &= ~PCIE_ASPMTIMER_EXTEND;
	pcie_writereg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_PMTHRESHREG, w);
	w = pcie_readreg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_PMTHRESHREG);
}

/** centralized clkreq control policy */
static void
pcie_clkreq_upd(pcicore_info_t *pi, uint state)
{
	si_t *sih = pi->sih;
	ASSERT(PCIE(sih));

	if (!PCIE_GEN1(sih))
		return;

	switch (state) {
	case SI_DOATTACH:
		if (PCIEGEN1_ASPM(sih))
			pcie_clkreq((void *)pi, 1, 0);
		break;
	case SI_PCIDOWN:
		if (sih->buscorerev == 6) {	/* turn on serdes PLL down */
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_addr),
			           ~0, 0);
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data),
			           ~0x40, 0);
		} else if (pi->pcie_pr42767) {
			pcie_clkreq((void *)pi, 1, 1);
		}
		break;
	case SI_PCIUP:
		if (sih->buscorerev == 6) {	/* turn off serdes PLL down */
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_addr),
			           ~0, 0);
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data),
			           ~0x40, 0x40);
		} else if (PCIEGEN1_ASPM(sih)) {		/* disable clkreq */
			pcie_clkreq((void *)pi, 1, 0);
		}
		break;
	default:
		ASSERT(0);
		break;
	}
}

/* ***** PCI core WARs ***** */
/* Done only once at attach time */
static void
pcie_war_polarity(pcicore_info_t *pi)
{
	uint32 w;

	if (pi->pcie_polarity != 0)
		return;

	w = pcie_readreg(pi->sih, pi->regs.pcieregs, PCIE_PCIEREGS, PCIE_PLP_STATUSREG);

	/* Detect the current polarity at attach and force that polarity and
	 * disable changing the polarity
	 */
	if ((w & PCIE_PLP_POLARITYINV_STAT) == 0)
		pi->pcie_polarity = (SERDES_RX_CTRL_FORCE);
	else
		pi->pcie_polarity = (SERDES_RX_CTRL_FORCE | SERDES_RX_CTRL_POLARITY);
}

/**
 * enable ASPM and CLKREQ if srom doesn't have it.
 * Needs to happen when update to shadow SROM is needed
 *   : Coming out of 'standby'/'hibernate'
 *   : If pcie_war_aspm_ovr state changed
 */
static void
pcie_war_aspm_clkreq(pcicore_info_t *pi)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	si_t *sih = pi->sih;
	uint16 val16, *reg16;
	uint32 w;

	if (!PCIEGEN1_ASPM(sih))
		return;

	/* bypass this on QT or VSIM */
	if (!ISSIM_ENAB(sih)) {

		reg16 = &pcieregs->sprom[SRSH_ASPM_OFFSET];
		val16 = R_REG(pi->osh, reg16);

		val16 &= ~SRSH_ASPM_ENB;
		if (pi->pcie_war_aspm_ovr == PCIE_ASPM_ENAB)
			val16 |= SRSH_ASPM_ENB;
		else if (pi->pcie_war_aspm_ovr == PCIE_ASPM_L1_ENAB)
			val16 |= SRSH_ASPM_L1_ENB;
		else if (pi->pcie_war_aspm_ovr == PCIE_ASPM_L0s_ENAB)
			val16 |= SRSH_ASPM_L0s_ENB;

		W_REG(pi->osh, reg16, val16);

		w = OSL_PCI_READ_CONFIG(pi->osh, pi->pciecap_lcreg_offset, sizeof(uint32));
		w &= ~PCIE_ASPM_ENAB;
		w |= pi->pcie_war_aspm_ovr;
		OSL_PCI_WRITE_CONFIG(pi->osh, pi->pciecap_lcreg_offset, sizeof(uint32), w);
	}

	reg16 = &pcieregs->sprom[SRSH_CLKREQ_OFFSET_REV5];
	val16 = R_REG(pi->osh, reg16);

	if (pi->pcie_war_aspm_ovr != PCIE_ASPM_DISAB) {
		val16 |= SRSH_CLKREQ_ENB;
		pi->pcie_pr42767 = TRUE;
	} else
		val16 &= ~SRSH_CLKREQ_ENB;

	W_REG(pi->osh, reg16, val16);
}

static void
pcie_war_pmebits(pcicore_info_t *pi)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint16 val16, *reg16;

	if (pi->sih->buscorerev != 18 && pi->sih->buscorerev != 19)
		return;

	reg16 = &pcieregs->sprom[SRSH_CLKREQ_OFFSET_REV8];
	val16 = R_REG(pi->osh, reg16);
	if (val16 != pi->pmebits) {
		PCI_ERROR(("pcie_war_pmebits: pmebits mismatch 0x%x (was 0x%x)\n",
			val16, pi->pmebits));
		pi->pmebits = 0x1f30;
		W_REG(pi->osh, reg16, pi->pmebits);
		val16 = R_REG(pi->osh, reg16);
		PCI_ERROR(("pcie_war_pmebits: update pmebits to 0x%x\n", val16));
	}
}

/** Apply the polarity determined at the start */
/* Needs to happen when coming out of 'standby'/'hibernate' */
static void
pcie_war_serdes(pcicore_info_t *pi)
{
	uint32 w = 0;

	if (pi->pcie_polarity != 0)
		pcie_mdiowrite(pi, MDIODATA_DEV_RX, SERDES_RX_CTRL, pi->pcie_polarity);

	pcie_mdioread(pi, MDIODATA_DEV_PLL, SERDES_PLL_CTRL, &w);
	if (w & PLL_CTRL_FREQDET_EN) {
		w &= ~PLL_CTRL_FREQDET_EN;
		pcie_mdiowrite(pi, MDIODATA_DEV_PLL, SERDES_PLL_CTRL, w);
	}
}

/** Fix MISC config to allow coming out of L2/L3-Ready state w/o PRST */
/* Needs to happen when coming out of 'standby'/'hibernate' */
static void
BCMINITFN(pcie_misc_config_fixup)(pcicore_info_t *pi)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint16 val16, *reg16;

	reg16 = &pcieregs->sprom[SRSH_PCIE_MISC_CONFIG];
	val16 = R_REG(pi->osh, reg16);

	if ((val16 & SRSH_L23READY_EXIT_NOPERST) == 0) {
		val16 |= SRSH_L23READY_EXIT_NOPERST;
		W_REG(pi->osh, reg16, val16);
	}
}

/* Needs to happen when coming out of 'standby'/'hibernate' */
static void
pcie_war_noplldown(pcicore_info_t *pi)
{
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint16 *reg16;

	ASSERT(pi->sih->buscorerev == 7);

	/* turn off serdes PLL down */
	si_corereg(pi->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
	           CHIPCTRL_4321_PLL_DOWN, CHIPCTRL_4321_PLL_DOWN);

	/*  clear srom shadow backdoor */
	reg16 = &pcieregs->sprom[SRSH_BD_OFFSET];
	W_REG(pi->osh, reg16, 0);
}

/** Needs to happen when coming out of 'standby'/'hibernate' */
static void
pcie_war_pci_setup(pcicore_info_t *pi)
{
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 w;

	if ((sih->buscorerev == 0) || (sih->buscorerev == 1)) {
		w = pcie_readreg(sih, pcieregs, PCIE_PCIEREGS, PCIE_TLP_WORKAROUNDSREG);
		w |= 0x8;
		pcie_writereg(sih, pcieregs, PCIE_PCIEREGS, PCIE_TLP_WORKAROUNDSREG, w);
	}

	if (sih->buscorerev == 1) {
		w = pcie_readreg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_LCREG);
		w |= (0x40);
		pcie_writereg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_LCREG, w);
	}

	if (sih->buscorerev == 0) {
		pcie_mdiowrite(pi, MDIODATA_DEV_RX, SERDES_RX_TIMER1, 0x8128);
		pcie_mdiowrite(pi, MDIODATA_DEV_RX, SERDES_RX_CDR, 0x0100);
		pcie_mdiowrite(pi, MDIODATA_DEV_RX, SERDES_RX_CDRBW, 0x1466);
	} else if (PCIEGEN1_ASPM(sih)) {
		/* Change the L1 threshold for better performance */
		w = pcie_readreg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_PMTHRESHREG);
		w &= ~(PCIE_L1THRESHOLDTIME_MASK);
		w |= (PCIE_L1THRESHOLD_WARVAL << PCIE_L1THRESHOLDTIME_SHIFT);
		pcie_writereg(sih, pcieregs, PCIE_PCIEREGS, PCIE_DLLP_PMTHRESHREG, w);

		pcie_war_serdes(pi);

		pcie_war_aspm_clkreq(pi);
	} else if (pi->sih->buscorerev == 7)
		pcie_war_noplldown(pi);

	/* Note that the fix is actually in the SROM, that's why this is open-ended */
	if (pi->sih->buscorerev >= 6)
		pcie_misc_config_fixup(pi);
}

void
pcie_war_ovr_aspm_update(void *pch, uint8 aspm)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!PCIE_GEN1(pi->sih))
		return;

	if (!PCIEGEN1_ASPM(pi->sih))
		return;

	/* Validate */
	if (aspm > PCIE_ASPM_ENAB)
		return;

	pi->pcie_war_aspm_ovr = aspm;

	/* Update the current state */
	pcie_war_aspm_clkreq(pi);
}


void
pcie_power_save_enable(void *pch, bool enable)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;


	if (!pi)
		return;

	pi->pcie_power_save = enable;
}

static void
pcie_power_save_upd(pcicore_info_t *pi, bool up)
{
	si_t *sih = pi->sih;

	if (!pi->pcie_power_save)
		return;


	if ((sih->buscorerev >= 15) && (sih->buscorerev <= 20)) {

		pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT1, 1, 0x7F64);

		if (up)
			pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT3, 1, 0x74);
		else
			pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT3, 1, 0x7C);

	} else if ((sih->buscorerev >= 21) && (sih->buscorerev <= 22)) {

		pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT1, 1, 0x7E65);

		if (up)
			pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT3, 1, 0x175);
		else
			pcicore_pcieserdesreg(pi, MDIO_DEV_BLK1, BLK1_PWR_MGMT3, 1, 0x17D);
	}
}

void
pcie_set_request_size(void *pch, uint16 size)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih;

	if (!pi)
		return;

	sih = pi->sih;

	if (size == 128)
		pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_128B;
	else if (size == 256)
		pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_256B;
	else if (size == 512)
		pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_512B;
	else if (size == 1024)
		pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_1024B;
	else
		return;

	if (PCIE_GEN1(sih)) {
		if (pi->sih->buscorerev == 18 || pi->sih->buscorerev == 19)
			pcie_devcontrol_mrrs(pi, PCIE_CAP_DEVCTRL_MRRS_MASK,
				(uint32)pi->pcie_reqsize);
	}
	else if (PCIE_GEN2(sih)) {
		pcie_devcontrol_mrrs(pi, PCIE_CAP_DEVCTRL_MRRS_MASK, (uint32)pi->pcie_reqsize);
	}
	else
		ASSERT(0);
}

uint16
pcie_get_request_size(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi)
		return (0);

	if (pi->pcie_reqsize == PCIE_CAP_DEVCTRL_MRRS_128B)
		return (128);
	else if (pi->pcie_reqsize == PCIE_CAP_DEVCTRL_MRRS_256B)
		return (256);
	else if (pi->pcie_reqsize == PCIE_CAP_DEVCTRL_MRRS_512B)
		return (512);
	return (0);
}

void
pcie_set_maxpayload_size(void *pch, uint16 size)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi)
		return;

	if (size == 128)
		pi->pcie_mps = PCIE_CAP_DEVCTRL_MPS_128B;
	else if (size == 256)
		pi->pcie_mps = PCIE_CAP_DEVCTRL_MPS_256B;
	else if (size == 512)
		pi->pcie_mps = PCIE_CAP_DEVCTRL_MPS_512B;
	else if (size == 1024)
		pi->pcie_mps = PCIE_CAP_DEVCTRL_MPS_1024B;
	else
		return;

	pcie_devcontrol_mps(pi, PCIE_CAP_DEVCTRL_MPS_MASK, (uint32)pi->pcie_mps);
}

uint16
pcie_get_maxpayload_size(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi)
		return (0);

	if (pi->pcie_mps == PCIE_CAP_DEVCTRL_MPS_128B)
		return (128);
	else if (pi->pcie_mps == PCIE_CAP_DEVCTRL_MPS_256B)
		return (256);
	else if (pi->pcie_mps == PCIE_CAP_DEVCTRL_MPS_512B)
		return (512);
	else if (pi->pcie_mps == PCIE_CAP_DEVCTRL_MPS_1024B)
		return (1024);
	return (0);
}

void
pcie_disable_TL_clk_gating(void *pch)
{
	/* disable TL clk gating is located in bit 4 of PCIEControl (Offset 0x000) */
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;

	if (!PCIE_GEN1(sih) && !PCIE_GEN2(sih))
		return;

	si_corereg(sih, sih->buscoreidx, 0, 0x10, 0x10);
}

void
pcie_set_L1_entry_time(void *pch, uint32 val)
{
	/* L1 entry time is located in bits [22:16] of register 0x1004 (pdl_control_1) */
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 data;

	if (!PCIE_GEN1(sih) && !PCIE_GEN2(sih))
		return;

	if (val > 0x7F)
		return;

	data = pcie_readreg(sih, pcieregs, PCIE_CONFIGREGS, PCIECFGREG_PDL_CTRL1);
	pcie_writereg(pch, pcieregs, PCIE_CONFIGREGS,
		PCIECFGREG_PDL_CTRL1, (data & ~0x7F0000) | (val << 16));
}

/** mode : 0 -- reset, 1 -- tx, 2 -- rx */
void
pcie_set_error_injection(void *pch, uint32 mode)
{
	/* through reg_phy_ctl_7 - 0x181c */
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;

	if (!PCIE_GEN1(sih) && !PCIE_GEN2(sih))
		return;

	if (mode == 0)
		pcie_writereg(pch, pcieregs, PCIE_CONFIGREGS, PCIECFGREG_REG_PHY_CTL7, 0);
	else if (mode == 1)
		pcie_writereg(pch, pcieregs, PCIE_CONFIGREGS, PCIECFGREG_REG_PHY_CTL7, 0x14031);
	else
		pcie_writereg(pch, pcieregs, PCIE_CONFIGREGS, PCIECFGREG_REG_PHY_CTL7, 0x2c031);
}

void
pcie_set_L1substate(void *pch, uint32 substate)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 data;

	ASSERT(PCIE_GEN2(sih));
	ASSERT(substate <= 3);

	if (substate != 0) {
		/* turn on ASPM L1 */
		data = pcie_readreg(sih, pcieregs, PCIE_CONFIGREGS, pi->pciecap_lcreg_offset);
		pcie_writereg(sih, pcieregs, PCIE_CONFIGREGS, pi->pciecap_lcreg_offset, data | 2);

		/* enable LTR */
		pcie_ltrenable(pch, 1, 1);
	}

	/* PML1_sub_control1 can only be accessed by OSL_PCI_xxxx_CONFIG */
	data = OSL_PCI_READ_CONFIG(pi->osh, PCIECFGREG_PML1_SUB_CTRL1, sizeof(uint32)) & 0xfffffff0;

	/* JIRA:SWWLAN-28455 */
	if (substate & 1)
		data |= PCI_PM_L1_2_ENA_MASK | ASPM_L1_2_ENA_MASK;

	if (substate & 2)
		data |= PCI_PM_L1_1_ENA_MASK | ASPM_L1_1_ENA_MASK;

	OSL_PCI_WRITE_CONFIG(pi->osh, PCIECFGREG_PML1_SUB_CTRL1, sizeof(uint32), data);
}

uint32
pcie_get_L1substate(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;
	uint32 data, substate = 0;

	ASSERT(PCIE_GEN2(sih));
	UNUSED_PARAMETER(sih);

	data = OSL_PCI_READ_CONFIG(pi->osh, PCIECFGREG_PML1_SUB_CTRL1, sizeof(uint32));

	/* JIRA:SWWLAN-28455 */
	if (data & (PCI_PM_L1_2_ENA_MASK | ASPM_L1_2_ENA_MASK))
		substate |= 1;

	if (data & (PCI_PM_L1_1_ENA_MASK | ASPM_L1_1_ENA_MASK))
		substate |= 2;

	return substate;
}

/* ***** Functions called during driver state changes ***** */
void
BCMATTACHFN(pcicore_attach)(void *pch, char *pvars, int state)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	si_t *sih = pi->sih;

	if (!PCIE_GEN1(sih)) {
		if ((BCM4360_CHIP_ID == CHIPID(sih->chip)) ||
		    (BCM43460_CHIP_ID == CHIPID(sih->chip)) ||
		    (BCM4350_CHIP_ID == CHIPID(sih->chip)) ||
		    (BCM4352_CHIP_ID == CHIPID(sih->chip)) ||
		    (BCM4335_CHIP_ID == CHIPID(sih->chip)))
			pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_1024B;
		return;
	}

	if (PCIEGEN1_ASPM(sih)) {
		if (((sih->boardvendor == VENDOR_APPLE) &&
		     ((uint8)getintvar(pvars, "sromrev") == 4) &&
		     ((uint8)getintvar(pvars, "boardrev") <= 0x71)) ||
		    ((uint32)getintvar(pvars, "boardflags2") & BFL2_PCIEWAR_OVR)) {
			pi->pcie_war_aspm_ovr = PCIE_ASPM_DISAB;
		} else {
			pi->pcie_war_aspm_ovr = PCIE_ASPM_ENAB;
		}
	}

	pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_128B;
	if (BCM4331_CHIP_ID == CHIPID(sih->chip))
	    pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_512B;

	bzero(pi->pcie_configspace, PCI_CONFIG_SPACE_SIZE);

	/* These need to happen in this order only */
	pcie_war_polarity(pi);

	pcie_war_serdes(pi);

	pcie_war_aspm_clkreq(pi);

	pcie_clkreq_upd(pi, state);

	pcie_war_pmebits(pi);

	/* Alter default TX drive strength setting */
	if (sih->boardvendor == VENDOR_APPLE) {
		if (sih->boardtype == 0x8d)
			/* change the TX drive strength to max */
			pcicore_pcieserdesreg(pch, MDIO_DEV_TXCTRL0, 0x18, 0xff, 0x7f);
		else if (PCIE_DRIVE_STRENGTH_OVERRIDE(sih))
			/* change the drive strength to 700mv */
			pcicore_pcieserdesreg(pch, MDIO_DEV_TXCTRL0, 0x18, 0xff, 0x70);
	}
}

void
pcicore_hwup(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi || !PCIE_GEN1(pi->sih))
		return;

	pcie_power_save_upd(pi, TRUE);

	if (pi->sih->boardtype == CB2_4321_BOARD || pi->sih->boardtype == CB2_4321_AG_BOARD)
		pcicore_fixlatencytimer(pch, 0x20);

	pcie_war_pci_setup(pi);

	/* Alter default TX drive strength setting */
	if (pi->sih->boardvendor == VENDOR_APPLE) {
		if (pi->sih->boardtype == 0x8d)
			/* change the TX drive strength to max */
			pcicore_pcieserdesreg(pch, MDIO_DEV_TXCTRL0, 0x18, 0xff, 0x7f);
		else if (PCIE_DRIVE_STRENGTH_OVERRIDE(pi->sih))
			/* change the drive strength to 700mv */
			pcicore_pcieserdesreg(pch, MDIO_DEV_TXCTRL0, 0x18, 0xff, 0x70);
	}
}

void
pcicore_up(void *pch, int state)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi)
		return;

	if (PCIE_GEN2(pi->sih)) {
		pcie_devcontrol_mrrs(pi, PCIE_CAP_DEVCTRL_MRRS_MASK, pi->pcie_reqsize);
		return;
	}

	pcie_power_save_upd(pi, TRUE);

	/* Restore L1 timer for better performance */
	pcie_extendL1timer(pi, TRUE);

	pcie_clkreq_upd(pi, state);

	if (pi->sih->buscorerev == 18 ||
	    (pi->sih->buscorerev == 19 && !PCIE_MRRS_OVERRIDE(sih)))
		pi->pcie_reqsize = PCIE_CAP_DEVCTRL_MRRS_128B;

	pcie_devcontrol_mrrs(pi, PCIE_CAP_DEVCTRL_MRRS_MASK, pi->pcie_reqsize);
}

/** When the device is going to enter D3 state (or the system is going to enter S3/S4 states */
void
pcicore_sleep(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 w;

	if (!pi || !PCIE_GEN1(pi->sih))
		return;

	pcie_power_save_upd(pi, FALSE);


	if (!PCIEGEN1_ASPM(pi->sih))
		return;


	w = OSL_PCI_READ_CONFIG(pi->osh, pi->pciecap_lcreg_offset, sizeof(uint32));
	w &= ~PCIE_CAP_LCREG_ASPML1;
	OSL_PCI_WRITE_CONFIG(pi->osh, pi->pciecap_lcreg_offset, sizeof(uint32), w);


	pi->pcie_pr42767 = FALSE;
}

void
pcicore_down(void *pch, int state)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!pi || !PCIE_GEN1(pi->sih))
		return;

	pcie_clkreq_upd(pi, state);

	/* Reduce L1 timer for better power savings */
	pcie_extendL1timer(pi, FALSE);

	pcie_power_save_upd(pi, FALSE);
}

/* ***** Wake-on-wireless-LAN (WOWL) support functions ***** */
/** Just uses PCI config accesses to find out, when needed before sb_attach is done */
bool
pcicore_pmecap_fast(osl_t *osh)
{
	uint8 cap_ptr;
	uint32 pmecap;

	cap_ptr = pcicore_find_pci_capability(osh, PCI_CAP_POWERMGMTCAP_ID, NULL, NULL);

	if (!cap_ptr)
		return FALSE;

	pmecap = OSL_PCI_READ_CONFIG(osh, cap_ptr, sizeof(uint32));

	return ((pmecap & PME_CAP_PM_STATES) != 0);
}

/**
 * return TRUE if PM capability exists in the pci config space
 * Uses and caches the information using core handle
 */
static bool
pcicore_pmecap(pcicore_info_t *pi)
{
	uint8 cap_ptr;
	uint32 pmecap;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint16*reg16;

	if (!pi->pmecap_offset) {
		cap_ptr = pcicore_find_pci_capability(pi->osh, PCI_CAP_POWERMGMTCAP_ID, NULL, NULL);
		if (!cap_ptr)
			return FALSE;

		pi->pmecap_offset = cap_ptr;

		reg16 = &pcieregs->sprom[SRSH_CLKREQ_OFFSET_REV8];
		pi->pmebits = R_REG(pi->osh, reg16);

		pmecap = OSL_PCI_READ_CONFIG(pi->osh, pi->pmecap_offset, sizeof(uint32));

		/* At least one state can generate PME */
		pi->pmecap = (pmecap & PME_CAP_PM_STATES) != 0;
	}

	return (pi->pmecap);
}

/** Enable PME generation */
void
pcicore_pmeen(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 w;

	/* if not pmecapable return */
	if (!pcicore_pmecap(pi))
		return;

	pcie_war_pmebits(pi);

	w = OSL_PCI_READ_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32));
	w |= (PME_CSR_PME_EN);
	OSL_PCI_WRITE_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32), w);
}

/** Return TRUE if PME status set */
bool
pcicore_pmestat(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 w;

	if (!pcicore_pmecap(pi))
		return FALSE;

	w = OSL_PCI_READ_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32));

	return (w & PME_CSR_PME_STAT) == PME_CSR_PME_STAT;
}

void
pcicore_pmestatclr(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 w;

	if (!pcicore_pmecap(pi))
		return;

	pcie_war_pmebits(pi);
	w = OSL_PCI_READ_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32));

	PCI_ERROR(("pcicore_pmestatclr PMECSR : 0x%x\n", w));

	/* Writing a 1 to PMESTAT will clear it */
	if ((w & PME_CSR_PME_STAT) == PME_CSR_PME_STAT) {
		OSL_PCI_WRITE_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32),
			w);
	}
}

/** Disable PME generation, clear the PME status bit if set */
void
pcicore_pmeclr(void *pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint32 w;

	if (!pcicore_pmecap(pi))
		return;

	pcie_war_pmebits(pi);

	w = OSL_PCI_READ_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32));

	PCI_ERROR(("pcicore_pci_pmeclr PMECSR : 0x%x\n", w));

	/* PMESTAT is cleared by writing 1 to it */
	w &= ~(PME_CSR_PME_EN);

	OSL_PCI_WRITE_CONFIG(pi->osh, pi->pmecap_offset + PME_CSR_OFFSET, sizeof(uint32), w);
}

static void
pcicore_fixlatencytimer(pcicore_info_t* pch, uint8 timer_val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	osl_t *osh;
	uint8 lattim;

	osh = pi->osh;
	lattim = read_pci_cfg_byte(PCI_CFG_LATTIM);

	if (!lattim) {
		PCI_ERROR(("%s: Modifying PCI_CFG_LATTIM from 0x%x to 0x%x\n",
		           __FUNCTION__, lattim, timer_val));
		write_pci_cfg_byte(PCI_CFG_LATTIM, timer_val);
	}
}

uint32
pcie_lcreg(void *pch, uint32 mask, uint32 val)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint8 offset;

	offset = pi->pciecap_lcreg_offset;
	if (!offset)
		return 0;

	/* set operation */
	if (mask)
		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), val);

	return OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
}

#ifdef BCMDBG
void
pcicore_dump(void *pch, struct bcmstrbuf *b)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	bcm_bprintf(b, "FORCEHT %d pcie_polarity 0x%x pcie_aspm_ovr 0x%x\n",
	            pi->sih->pci_pr32414, pi->pcie_polarity, pi->pcie_war_aspm_ovr);
}
#endif /* BCMDBG */

uint32
pcicore_pciereg(void *pch, uint32 offset, uint32 mask, uint32 val, uint type)
{
	uint32 reg_val = 0;
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;

	if (mask) {
		PCI_ERROR(("PCIEREG: 0x%x writeval  0x%x\n", offset, val));
		pcie_writereg(pi->sih, pcieregs, type, offset, val);
	}

	/* Should not read register 0x154 */
	if (PCIE_GEN1(pi->sih) &&
		pi->sih->buscorerev <= 5 && offset == PCIE_DLLP_PCIE11 && type == PCIE_PCIEREGS)
		return reg_val;

	reg_val = pcie_readreg(pi->sih, pcieregs, type, offset);
	PCI_ERROR(("PCIEREG: 0x%x readval is 0x%x\n", offset, reg_val));

	return reg_val;
}

uint32
pcicore_pcieserdesreg(void *pch, uint32 mdioslave, uint32 offset, uint32 mask, uint32 val)
{
	uint32 reg_val = 0;
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (mask) {
		pcie_mdiowrite(pi, mdioslave, offset, val);
	}

	if (pcie_mdioread(pi, mdioslave, offset, &reg_val))
		reg_val = 0xFFFFFFFF;

	return reg_val;
}

uint16
pcie_get_ssid(void* pch)
{
	uint32 ssid =
		OSL_PCI_READ_CONFIG(((pcicore_info_t *)pch)->osh, PCI_CFG_SVID, sizeof(uint32));
	return (uint16)(ssid >> 16);
}

uint32
pcie_get_bar0(void* pch)
{
	return OSL_PCI_READ_CONFIG(((pcicore_info_t *)pch)->osh, PCI_CFG_BAR0, sizeof(uint32));
}

int
pcie_configspace_cache(void* pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint offset = 0;
	uint32 *tmp = (uint32 *)pi->pcie_configspace;

	while (offset < PCI_CONFIG_SPACE_SIZE) {
		*tmp++ = OSL_PCI_READ_CONFIG(pi->osh, offset, sizeof(uint32));
		offset += 4;
	}
	return 0;
}

int
pcie_configspace_restore(void* pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	uint offset = 0;
	uint32 *tmp = (uint32 *)pi->pcie_configspace;

	/* if config space was not buffered, than abort restore */
	if (*tmp == 0)
		return -1;

	while (offset < PCI_CONFIG_SPACE_SIZE) {
		OSL_PCI_WRITE_CONFIG(pi->osh, offset, sizeof(uint32), *tmp);
		tmp++;
		offset += 4;
	}
	return 0;
}

int
pcie_configspace_get(void* pch, uint8 *buf, uint size)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	memcpy(buf, pi->pcie_configspace, size);
	return 0;
}

uint32
pcie_get_link_speed(void* pch)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 data;

	data = pcie_readreg(pi->sih, pcieregs, PCIE_CONFIGREGS, pi->pciecap_lcreg_offset);
	return (data & PCIE_LINKSPEED_MASK) >> PCIE_LINKSPEED_SHIFT;
}

uint32
pcie_survive_perst(void* pch, uint32 mask, uint32 val)
{
#ifdef SURVIVE_PERST_ENAB
	pcicore_info_t *pi = (pcicore_info_t *)pch;
	sbpcieregs_t *pcieregs = pi->regs.pcieregs;
	uint32 w;

	/* mask and set */
	if (mask || val) {
		w = (R_REG(pi->osh, (&pcieregs->control)) & ~mask) | val;
		W_REG(pi->osh, (&pcieregs->control), w);
	}
	/* readback */
	return R_REG(pi->osh, (&pcieregs->control));
#else
	return 0;
#endif /* SURVIVE_PERST_ENAB */
}


#if defined(WLTEST) || defined(BCMDBG)
/* Dump PCIE Info */
int
pcicore_dump_pcieinfo(void *pch, struct bcmstrbuf *b)
{
	pcicore_info_t *pi = (pcicore_info_t *)pch;

	if (!PCIE_GEN1(pi->sih) && !PCIE_GEN2(pi->sih))
		return BCME_ERROR;

	bcm_bprintf(b, "PCIE link speed: %d\n", pcie_get_link_speed(pch));
	return 0;
}
#endif
