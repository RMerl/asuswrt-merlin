/*
 * ADMtek switch setup functions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: etc_adm.c 286404 2011-09-27 19:29:08Z $
 */

#include <et_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <bcmparams.h>
#include <etc_adm.h>
#include <et_dbg.h>

/* Private state per ADM switch */
struct adm_info_s {
	si_t *sih;			/* SiliconBackplane handle */
	void *vars;			/* variables handle */
	uint coreidx;			/* Current core index */
	uint32 eecs, eesk, eedi;	/* GPIO mapping */
};

/* Minimum timing constants */
#define EECK_EDGE_TIME	3	/* 3us - max(adm 2.5us, 93c 1us) */
#define EEDI_SETUP_TIME	1	/* 1us - max(adm 10ns, 93c 400ns) */
#define EECS_SETUP_TIME	1	/* 1us - max(adm no, 93c 200ns) */

/* Allocate private resource */
adm_info_t *
adm_attach(si_t *sih, char *vars)
{
	adm_info_t *adm;
	int gpio;

	/* Allocate private data */
	if (!(adm = MALLOC(si_osh(sih), sizeof(adm_info_t)))) {
		ET_ERROR(("adm_attach: out of memory, malloc %d bytes", MALLOCED(si_osh(sih))));
		return NULL;
	}
	bzero((char *) adm, sizeof(adm_info_t));
	adm->sih = sih;
	adm->vars = vars;

	/* Init GPIO mapping. Default GPIO: 2, 3, 4 */
	gpio = getgpiopin(vars, "adm_eecs", 2);
	ET_ERROR(("adm_attach: got %d as adm_eecs", gpio));
	if (gpio == GPIO_PIN_NOTDEFINED) {
		ET_ERROR(("adm_attach: adm_eecs gpio fail: GPIO 2 in use"));
		goto error;
	}
	adm->eecs = 1 << gpio;

	gpio = getgpiopin(vars, "adm_eesk", 3);
	ET_ERROR(("adm_attach: got %d as adm_eesk", gpio));
	if (gpio == GPIO_PIN_NOTDEFINED) {
		ET_ERROR(("adm_attach: adm_eesk gpio fail: GPIO 3 in use"));
		goto error;
	}
	adm->eesk = 1 << gpio;

	gpio = getgpiopin(vars, "adm_eedi", 4);
	ET_ERROR(("adm_attach: got %d as adm_eedi", gpio));
	if (gpio == GPIO_PIN_NOTDEFINED) {
		ET_ERROR(("adm_attach: adm_eedi gpio fail: GPIO 4 in use"));
		goto error;
	}
	adm->eedi = 1 << gpio;

	return adm;

error:
	adm_detach(adm);
	return NULL;
}

/* Release private resource */
void
adm_detach(adm_info_t *adm)
{
	/* Free private data */
	MFREE(si_osh(adm->sih), adm, sizeof(adm_info_t));
}

/*
* The following local functions provide chip access control. The
* general rules in writing these supporting routines are:
*
*   1. EECS should be kept low after each routine.
*   2. EESK should be kept low after each routine.
*/
/* Enable register access to the chip */
static void
adm_enable(adm_info_t *adm)
{
	void *regs;

	/* Save current core index */
	adm->coreidx = si_coreidx(adm->sih);

	/* Switch to GPIO core for faster access */
	regs = si_gpiosetcore(adm->sih);
	ASSERT(regs);
}

/* Disable register access to the chip */
static void
adm_disable(adm_info_t *adm)
{
	/* Switch back to original core */
	si_setcoreidx(adm->sih, adm->coreidx);
}

/* Enable outputs with specified value to the chip */
static void
adm_enout(adm_info_t *adm, uint32 pins, uint val)
{
	/* Prepare GPIO output value */
	si_gpioout(adm->sih, pins, val, GPIO_DRV_PRIORITY);
	/* Enable GPIO outputs */
	si_gpioouten(adm->sih, pins, pins, GPIO_DRV_PRIORITY);
	OSL_DELAY(EECK_EDGE_TIME);
}

/* Disable outputs to the chip */
static void
adm_disout(adm_info_t *adm, uint32 pins)
{
	/* Disable GPIO outputs */
	si_gpioouten(adm->sih, pins, 0, GPIO_DRV_PRIORITY);
	OSL_DELAY(EECK_EDGE_TIME);
}

/* Advance clock(s) */
static void
adm_adclk(adm_info_t *adm, int clocks)
{
	int i;
	for (i = 0; i < clocks; i ++) {
		/* Clock high */
		si_gpioout(adm->sih, adm->eesk, adm->eesk, GPIO_DRV_PRIORITY);
		OSL_DELAY(EECK_EDGE_TIME);
		/* Clock low */
		si_gpioout(adm->sih, adm->eesk, 0, GPIO_DRV_PRIORITY);
		OSL_DELAY(EECK_EDGE_TIME);
	}
}

/* Write a bit stream to the chip */
static void
adm_write(adm_info_t *adm, int cs, uint8 *buf, uint bits)
{
	uint i, len = (bits + 7) / 8;
	uint8 mask;

	/* CS high/low */
	if (cs)
		si_gpioout(adm->sih, adm->eecs, adm->eecs, GPIO_DRV_PRIORITY);
	else
		si_gpioout(adm->sih, adm->eecs, 0, GPIO_DRV_PRIORITY);
	OSL_DELAY(EECK_EDGE_TIME);

	/* Byte assemble from MSB to LSB */
	for (i = 0; i < len; i++) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80; mask && bits > 0; mask >>= 1, bits --) {
			/* Clock low */
			si_gpioout(adm->sih, adm->eesk, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(EECK_EDGE_TIME);

			/* Output on rising edge */
			if (mask & buf[i])
				si_gpioout(adm->sih, adm->eedi, adm->eedi, GPIO_DRV_PRIORITY);
			else
				si_gpioout(adm->sih, adm->eedi, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(EEDI_SETUP_TIME);

			/* Clock high */
			si_gpioout(adm->sih, adm->eesk, adm->eesk, GPIO_DRV_PRIORITY);
			OSL_DELAY(EECK_EDGE_TIME);
		}
	}

	/* Clock low */
	si_gpioout(adm->sih, adm->eesk, 0, GPIO_DRV_PRIORITY);
	OSL_DELAY(EECK_EDGE_TIME);

	/* CS low */
	if (cs)
		si_gpioout(adm->sih, adm->eecs, 0, GPIO_DRV_PRIORITY);
}

/* Handy macros for writing fixed length values */
#define adm_write8(adm, cs, b) { uint8 val = (uint8) (b); adm_write(adm, cs, &val, sizeof(val)*8); }
#define adm_write16(adm, cs, w) { uint16 val = hton16(w); \
		adm_write(adm, cs, (uint8 *)&val, sizeof(val)*8); }
#define adm_write32(adm, cs, i) { uint32 val = hton32(i); \
		adm_write(adm, cs, (uint8 *)&val, sizeof(val)*8); }

/* Write chip configuration register */
/* Follow 93c66 timing and chip's min EEPROM timing requirement */
static void
adm_wreg(adm_info_t *adm, uint8 addr, uint16 val)
{
	/* cmd(27bits): sb(1) + opc(01) + addr(bbbbbbbb) + data(bbbbbbbbbbbbbbbb) */
	uint8 bits[4] = {
		(0x05 << 5) | (addr >> 3),
		(addr << 5) | (uint8)(val >> 11),
		(uint8)(val >> 3),
		(uint8)(val << 5)
	};

	ET_TRACE(("adm_wreg: addr %02x val %04x (%02X%02X%02X%02X)\n",
		addr, val, bits[0], bits[1], bits[2], bits[3]));

	/* Enable GPIO outputs with all pins to 0 */
	adm_enout(adm, adm->eecs | adm->eesk | adm->eedi, 0);

	/* Write cmd. Total 27 bits */
	adm_write(adm, 1, bits, 27);

	/* Extra clock(s) required per datasheet */
	adm_adclk(adm, 2);

	/* Disable GPIO outputs */
	adm_disout(adm, adm->eecs | adm->eesk | adm->eedi);
}

/* Configure the chip based on nvram settings */
int
adm_config_vlan(adm_info_t *adm)
{
	/* Port configuration */
	struct {
		uint8 addr;	/* port configuration register */
		uint16 vlan;	/* vlan port mapping */
		uint8 tagged;	/* output tagging */
		uint8 cpu;	/* cpu port? 1 - yes, 0 - no */
		uint16 pvid;	/* cpu port pvid */
	} port_cfg_tab[] = {
		{1, 1<<0, 0, 0, -1},
		{3, 1<<2, 0, 0, -1},
		{5, 1<<4, 0, 0, -1},
		{7, 1<<6, 0, 0, -1},
		{8, 1<<7, 0, 0, -1},
#if defined(PMON) || defined(_CFE_)
		{9, 1<<8, 0, 1, -1}	/* no output tagging for pmon/cfe */
#else	/* #if defined(PMON) || defined(CFE) */
		{9, 1<<8, 1, 1, -1}	/* output tagging for linux... */
#endif	/* #if defined(PMON) || defined(CFE) */
	};
	/* Vlan ports bitmap */
	struct {
		uint8 addr;	/* vlan port map register */
	} vlan_cfg_tab[] = {
		{0x13},
		{0x14},
		{0x15},
		{0x16},
		{0x17},
		{0x18},
		{0x19},
		{0x1a},
		{0x1b},
		{0x1c},
		{0x1d},
		{0x1e},
		{0x1f},
		{0x20},
		{0x21},
		{0x22}
	};
	uint16 vid, i;

	/* Enable access to the switch */
	adm_enable(adm);

	/* vlan mode select register (0x11): vlan on, mac clone */
	adm_wreg(adm, 0x11, 0xff30);

	/* vlan port group: port configuration, vlan port map */
	/* VLAN ID is equal to vlan number, max 16 vlans */
	for (vid = 0; vid < 16; vid ++) {
		char port[] = "XXXX", *ports, *next, *cur;
		char vlanports[] = "vlanXXXXports";
		uint16 vlan_map = 0;
		int port_num, len;
		uint16 port_cfg;

		/* no members if VLAN id is out of limitation */
		if (vid > VLAN_MAXVID)
			goto vlan_setup;

		/* get nvram port settings */
		sprintf(vlanports, "vlan%dports", vid);
		ports = getvar(adm->vars, vlanports);

		/* disable this vlan if not defined */
		if (!ports)
			goto vlan_setup;

		/*
		* port configuration register (0x01, 0x03, 0x05, 0x07, 0x08, 0x09):
		*   input/output tagging, pvid, auto mdix, auto negotiation, ...
		* cpu port needs special handing to support pmon/cfe/linux...
		*/
		for (cur = ports; cur; cur = next) {
			/* tokenize the port list */
			while (*cur == ' ')
				cur ++;
			next = bcmstrstr(cur, " ");
			len = next ? next - cur : strlen(cur);
			if (!len)
				break;
			if (len > sizeof(port) - 1)
				len = sizeof(port) - 1;
			strncpy(port, cur, len);
			port[len] = 0;

			/* make sure port # is within the range */
			port_num = bcm_atoi(port);
			if (port_num >= sizeof(port_cfg_tab) / sizeof(port_cfg_tab[0])) {
				ET_ERROR(("port number %d is out of range\n", port_num));
				continue;
			}

			/* build vlan port map */
			vlan_map |= port_cfg_tab[port_num].vlan;

			/* cpu port needs special care */
			if (port_cfg_tab[port_num].cpu) {
				/* cpu port's default VLAN is lan! */
				if (strchr(port, '*'))
					port_cfg_tab[port_num].pvid = vid;
				/* will be done later */
				continue;
			}

			/* configure port */
			port_cfg = 0x8000 |	/* auto mdix */
				(vid << 10) | 	/* pvid */
				0x000f;		/* full duplex, 100Mbps, auto neg, flow ctrl */
			adm_wreg(adm, port_cfg_tab[port_num].addr, port_cfg);
		}
vlan_setup:
		/* vlan port map register (0x13 - 0x22) */
		adm_wreg(adm, vlan_cfg_tab[vid].addr, vlan_map);
	}

	/* cpu port config: auto mdix, pvid, output tagging, ... */
	for (i = 0; i < sizeof(port_cfg_tab)/sizeof(port_cfg_tab[0]); i ++) {
		uint16 tagged, pvid;
		uint16 port_cfg;

		/* cpu port only */
		if (port_cfg_tab[i].cpu == 0 || port_cfg_tab[i].pvid == 0xffff)
			continue;

		/* configure port */
		tagged = port_cfg_tab[i].tagged ? 1 : 0;
		pvid = port_cfg_tab[i].pvid;
		port_cfg = 0x8000 |	/* auto mdix */
			(pvid << 10) | 	/* pvid */
			(tagged << 4) |	/* output tagging */
			0x000f;		/* full duplex, 100Mbps, auto neg, flow ctrl */
		adm_wreg(adm, port_cfg_tab[i].addr, port_cfg);
	}

	/* Disable access to the switch */
	adm_disable(adm);

	return 0;
}

/*
* Enable the chip with preset default configuration:
*
*  TP Auto MDIX (EESK/GPIO = 1)
*  Single Color LED (EEDI/GPIO = 0)
*  EEPROM Disable (H/W pull down)
*/
int
adm_enable_device(adm_info_t *adm)
{
	uint32 rc;
	int i;

	/* Check nvram override existance */
	if ((rc = getgpiopin(adm->vars, "adm_rc", GPIO_PIN_NOTDEFINED)) == GPIO_PIN_NOTDEFINED)
		return 0;
	rc = 1 << rc;

	/* Enable access to the switch */
	adm_enable(adm);
	/*
	* Reset sequence: RC high->low(100ms)->high(30ms)
	*
	* WAR: Certain boards don't have the correct power on
	* reset logic therefore we must explicitly perform the
	* sequece in software.
	*/
	/* Keep RC high for at least 20ms */
	adm_enout(adm, rc, rc);
	for (i = 0; i < 20; i ++)
		OSL_DELAY(1000);
	/* Keep RC low for at least 100ms */
	adm_enout(adm, rc, 0);
	for (i = 0; i < 100; i++)
		OSL_DELAY(1000);
	/* Set default configuration */
	adm_enout(adm, adm->eesk | adm->eedi, adm->eesk);
	/* Keep RC high for at least 30ms */
	adm_enout(adm, rc, rc);
	for (i = 0; i < 30; i++)
		OSL_DELAY(1000);
	/* Leave RC high and disable GPIO outputs */
	adm_disout(adm, adm->eecs | adm->eesk | adm->eedi);
	/* Disable access to the switch */
	adm_disable(adm);
	return 0;
}

#ifdef BCMDBG
/* Read a bit stream from the chip */
static void
adm_read(adm_info_t *adm, int cs, uint32 pin, uint8 *buf, uint bits)
{
	uint i, len = (bits + 7) / 8;

	/* CS high/low */
	if (cs)
		si_gpioout(adm->sih, adm->eecs, adm->eecs, GPIO_DRV_PRIORITY);
	else
		si_gpioout(adm->sih, adm->eecs, 0, GPIO_DRV_PRIORITY);
	OSL_DELAY(EECK_EDGE_TIME);

	/* Byte assemble from MSB to LSB */
	for (i = 0; i < len; i ++) {
		uint8 mask, byte = 0;
		/* Bit bang from MSB to LSB */
		for (mask = 0x80; mask && bits > 0; mask >>= 1, bits --) {
			/* Clock high */
			si_gpioout(adm->sih, adm->eesk, adm->eesk, GPIO_DRV_PRIORITY);
			OSL_DELAY(EECK_EDGE_TIME);

			/* Sample on rising edge */
			if (si_gpioin(adm->sih) & pin)
				byte |= mask;

			/* Clock low */
			si_gpioout(adm->sih, adm->eesk, 0, GPIO_DRV_PRIORITY);
			OSL_DELAY(EECK_EDGE_TIME);
		}
		buf[i] = byte;
	}

	/* CS low */
	if (cs)
		si_gpioout(adm->sih, adm->eecs, 0, GPIO_DRV_PRIORITY);
}

/* Handy macros for reading fixed length values */
#define adm_read8(adm, cs, pin, b) { uint8 val; \
		adm_read(adm, cs, pin, &val, sizeof(val) * 8); *(b) = val; }
#define adm_read16(adm, cs, pin, w) { uint16 val; \
		adm_read(adm, cs, pin, (uint8 *)&val, sizeof(val) * 8); *(w) = ntoh16(val); }
#define adm_read32(adm, cs, pin, i) { uint32 val; \
		adm_read(adm, cs, pin, (uint8 *)&val, sizeof(val) * 8); *(i) = ntoh32(val); }

/* Read counter/config register. table 0 - config registers, 1 - internal counters */
/* Follow chip's SMI timing */
static uint32
adm_rreg(adm_info_t *adm, int addr, int table)
{
	/* Command: preamble(11) + start(01) + opcode(10) + table(b) +
	* device(00) + register(bbbbbbb)
	*/
	uint16 cmd = (3 << 14) | (1 << 12) | (2 << 10) | (table << 9) | (0 << 7) | addr;
	uint32 data;

	/* Enable GPIO outputs */
	adm_enout(adm, adm->eecs | adm->eesk | adm->eedi, 0);

	/* Preamble: at lease 32 bit 1s */
	adm_write32(adm, 0, 0xffffffff);

	/* Command: 2 extra preamble bits plus 14 command bits */
	adm_write16(adm, 0, cmd);

	/* Z EEDI: the switch will drive it */
	adm_disout(adm, adm->eedi);

	/* Turn around: 1 bit */
	adm_adclk(adm, 1);

	/* Register value: 32 bits */
	adm_read32(adm, 0, adm->eedi, &data);

	/* Idle: at least 1 extra clock */
	adm_adclk(adm, 2);

	/* Disable GPIO outputs */
	adm_disout(adm, adm->eecs | adm->eesk);

	return data;
}

char*
adm_dump_regs(adm_info_t *adm, char *buf)
{
	uint32 v;
	int i;

	/* enable access to the switch */
	adm_enable(adm);

	/* dump admtek switch registers */
	buf += sprintf(buf, "admtek:\n");
	for (i = 0; i <= 0x2d; i++) {
		v = adm_rreg(adm, i, 0);
		buf += sprintf(buf, "%04x ",
			((i % 2) ? ((v >> 16) & 0xffff) :  (v & 0xffff)));
		if ((i % 8) == 7)
			buf += sprintf(buf, "\n");
	}
	buf += sprintf(buf, "\n");

	/* disable access to the switch */
	adm_disable(adm);

	return (buf);
}
#endif /* BCMDBG */
