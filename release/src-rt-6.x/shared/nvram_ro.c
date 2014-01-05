/*
 * Read-only support for NVRAM on flash and otp.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nvram_ro.c 290190 2011-10-17 18:13:26Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <sbchipc.h>
#include <bcmsrom.h>
#include <bcmotp.h>
#include <bcmdevs.h>
#include <sflash.h>
#include <hndsoc.h>

#ifdef BCMDBG_ERR
#define NVR_MSG(x) printf x
#else
#define NVR_MSG(x)
#endif	/* BCMDBG_ERR */

typedef struct _vars {
	struct _vars *next;
	int bufsz;	/* allocated size */
	int size;	/* actual vars size */
	char *vars;
} vars_t;

#define	VARS_T_OH	sizeof(vars_t)

static vars_t *vars = NULL;

#if !defined(DONGLEBUILD) && !defined(BCM_BOOTLOADER)
#define NVRAM_FILE	1
#endif

#ifdef NVRAM_FILE
static int nvram_file_init(void* sih);
static int initvars_file(si_t *sih, osl_t *osh, char **nvramp, int *nvraml);
#endif

static char *findvar(char *vars_arg, char *lim, const char *name);
extern void nvram_get_global_vars(char **varlst, uint *varsz);

#if defined(FLASH)
/* copy flash to ram */
static void
BCMINITFN(get_flash_nvram)(si_t *sih, struct nvram_header *nvh)
{
	osl_t *osh;
	uint nvs, bufsz;
	vars_t *new;

	osh = si_osh(sih);

	nvs = R_REG(osh, &nvh->len) - sizeof(struct nvram_header);
	bufsz = nvs + VARS_T_OH;

	if ((new = (vars_t *)MALLOC(osh, bufsz)) == NULL) {
		NVR_MSG(("Out of memory for flash vars\n"));
		return;
	}
	new->vars = (char *)new + VARS_T_OH;

	new->bufsz = bufsz;
	new->size = nvs;
	new->next = vars;
	vars = new;

#ifdef BCMJTAG
	if (BUSTYPE(sih->bustype) == JTAG_BUS) {
		uint32 *s, *d;
		uint sz = nvs;

		s = (uint32 *)(&nvh[1]);
		d = (uint32 *)new->vars;

		ASSERT(ISALIGNED((uintptr)s, sizeof(uint32)));
		ASSERT(ISALIGNED((uintptr)d, sizeof(uint32)));

		while (sz >= sizeof(uint32)) {
			*d++ = ltoh32(R_REG(osh, s++));
			sz -= sizeof(uint32);
		}
		if (sz) {
			union {
				uint32	w;
				char	b[sizeof(uint32)];
			} data;
			uint i;
			char *dst = (char *)d;

			data.w =  ltoh32(R_REG(osh, s));
			for (i = 0; i < sz; i++)
				*dst++ = data.b[i];
		}
	} else
#endif	/* BCMJTAG */
		bcopy((char *)(&nvh[1]), new->vars, nvs);

	NVR_MSG(("%s: flash nvram @ %p, copied %d bytes to %p\n", __FUNCTION__,
	         nvh, nvs, new->vars));
}
#endif /* FLASH */

#if defined(BCMUSBDEV) || defined(BCMHOSTVARS)
#if defined(BCMHOSTVARS)
extern char *_vars;
extern uint _varsz;
#endif
#ifndef CONFIG_XIP
extern uint8 embedded_nvram[];
#endif
#endif	

int
BCMATTACHFN(nvram_init)(void *si)
{
#if defined(FLASH)
	uint idx;
	chipcregs_t *cc;
	si_t *sih;
	osl_t *osh;
	void *oh;
	struct nvram_header *nvh = NULL;
	uintptr flbase;
	struct sflash *info;
	uint32 cap = 0, off, flsz;
#endif /* FLASH */

	/* Make sure we read nvram in flash just once before freeing the memory */
	if (vars != NULL) {
		NVR_MSG(("nvram_init: called again without calling nvram_exit()\n"));
		return 0;
	}

#if defined(FLASH)
	sih = (si_t *)si;
	osh = si_osh(sih);

	/* Check for flash */
	idx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	flbase = (uintptr)OSL_UNCACHED((void *)SI_FLASH2);
	flsz = 0;
	cap = R_REG(osh, &cc->capabilities);

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (sih->chiprev == 0))
		cap &= ~CC_CAP_FLASH_MASK;

	switch (cap & CC_CAP_FLASH_MASK) {
	case PFLASH:
		flsz = SI_FLASH2_SZ;
		break;

	case SFLASH_ST:
	case SFLASH_AT:
		if ((info = sflash_init(sih, cc)) == NULL)
			break;
		flsz = info->size;
		break;

	case FLASH_NONE:
	default:
		break;
	}

	/* If we found flash, see if there is nvram there */
	if (flsz != 0) {
		off = FLASH_MIN;
		nvh = NULL;
		while (off <= flsz) {
			nvh = (struct nvram_header *)(flbase + off - NVRAM_SPACE);
			if (R_REG(osh, &nvh->magic) == NVRAM_MAGIC)
				break;
			off <<= 1;
			nvh = NULL;
		};

		if (nvh != NULL)
			get_flash_nvram(sih, nvh);
	}

	/* Check for otp */
	if ((oh = otp_init(sih)) != NULL) {
		uint sz = otp_size(oh);
		uint bufsz = sz + VARS_T_OH;
		vars_t *new = (vars_t *)MALLOC(osh, bufsz);
		if (new != NULL)
			new->vars = (char *)new + VARS_T_OH;
		if (new == NULL) {
			NVR_MSG(("Out of memory for otp\n"));
		} else if (otp_nvread(oh, new->vars, &sz)) {
			NVR_MSG(("otp_nvread error\n"));
			MFREE(osh, new, bufsz);
		} else {
			new->bufsz = bufsz;
			new->size = sz;
			new->next = vars;
			vars = new;
		}
	}

	/* Last, if we do have flash but no regular nvram was found in it,
	 * try for embedded nvram.
	 * Note that since we are doing this last, embedded nvram will override
	 * otp, a change from the normal precedence in the designs that use
	 * the full read/write nvram support.
	 */
	if ((flsz != 0) && (nvh == NULL)) {
		nvh = (struct nvram_header *)(flbase + 1024);
		if (R_REG(osh, &nvh->magic) == NVRAM_MAGIC)
			get_flash_nvram(sih, nvh);
		else {
			nvh = (struct nvram_header *)(flbase + 4096);
			if (R_REG(osh, &nvh->magic) == NVRAM_MAGIC)
				get_flash_nvram(sih, nvh);
		}
	}

	/* All done */
	si_setcoreidx(sih, idx);
#endif /* FLASH */

#if defined(BCMUSBDEV) || defined(BCMHOSTVARS)
#if defined(BCMHOSTVARS)
	/* Honor host supplied variables and make them global */
	if (_vars != NULL && _varsz != 0)
		nvram_append(si, _vars, _varsz);
#endif	
#ifndef CONFIG_XIP
#endif	/* CONFIG_XIP */
#endif	

#ifdef NVRAM_FILE
	if (BUSTYPE(((si_t *)si)->bustype) == PCI_BUS) {
		if (nvram_file_init(si) != 0)
			return BCME_ERROR;
	}
#endif /* NVRAM_FILE */

	return 0;
}

#ifdef NVRAM_FILE
static int
nvram_file_init(void* sih)
{
	char *base = NULL, *nvp = NULL, *flvars = NULL;
	int err = 0, nvlen = 0;

	base = nvp = MALLOC(si_osh((si_t *)sih), MAXSZ_NVRAM_VARS);
	if (base == NULL)
		return BCME_NOMEM;

	/* Init nvram from nvram file if they exist */
	err = initvars_file(sih, si_osh((si_t *)sih), &nvp, (int*)&nvlen);
	if (err != 0) {
		NVR_MSG(("No NVRAM file present!!!\n"));
		goto exit;
	}
	if (nvlen) {
		flvars = MALLOC(si_osh((si_t *)sih), nvlen);
		if (flvars == NULL)
			goto exit;
	}
	else
		goto exit;

	bcopy(base, flvars, nvlen);
	err = nvram_append(sih, flvars, nvlen);

exit:
	MFREE(si_osh((si_t *)sih), base, MAXSZ_NVRAM_VARS);

	return err;
}

/* NVRAM file read for pcie NIC's */
static int
initvars_file(si_t *sih, osl_t *osh, char **nvramp, int *nvraml)
{
#if defined(BCMDRIVER)
	/* Init nvram from nvram file if they exist */
	char *nvram_buf = *nvramp;
	void	*nvram_fp = NULL;
	int nv_len = 0, ret = 0, i = 0, len = 0;

	nvram_fp = (void*)osl_os_open_image("nvram.txt");
	if (nvram_fp != NULL) {
		while ((nv_len = osl_os_get_image_block(nvram_buf, MAXSZ_NVRAM_VARS, nvram_fp)))
			len = nv_len;
	}
	else {
		NVR_MSG(("Could not open nvram.txt file\n"));
		ret = -1;
		goto exit;
	}

	/* Make array of strings */
	for (i = 0; i < len; i++) {
		if ((*nvram_buf == ' ') || (*nvram_buf == '\t') || (*nvram_buf == '\n') ||
			(*nvram_buf == '\0')) {
			*nvram_buf = '\0';
			nvram_buf++;
		}
		else
			nvram_buf++;
	}
	*nvram_buf++ = '\0';
	*nvramp = nvram_buf;
	*nvraml = len+1; /* add one for the null character */

exit:
	if (nvram_fp)
		osl_os_close_image(nvram_fp);

	return ret;
#else /* BCMDRIVER */
	return BCME_ERROR
#endif /* BCMDRIVER */
}
#endif /* NVRAM_FILE */

int
BCMATTACHFN(nvram_append)(void *si, char *varlst, uint varsz)
{
	uint bufsz = VARS_T_OH;
	vars_t *new;

	if ((new = MALLOC(si_osh((si_t *)si), bufsz)) == NULL)
		return BCME_NOMEM;

	new->vars = varlst;
	new->bufsz = bufsz;
	new->size = varsz;
	new->next = vars;
	vars = new;

	return BCME_OK;
}

void
nvram_get_global_vars(char **varlst, uint *varsz)
{
	*varlst = vars->vars;
	*varsz = vars->size;
}

void
BCMUNINITFN(nvram_exit)(void *si)
{
	vars_t *this, *next;
	si_t *sih;

	sih = (si_t *)si;
	this = vars;

#ifndef DONGLEBUILD
	if (this)
		MFREE(si_osh(sih), this->vars, this->size);
#endif /* DONGLEBUILD */

	while (this) {
		next = this->next;
		MFREE(si_osh(sih), this, this->bufsz);
		this = next;
	}
	vars = NULL;
}

static char *
findvar(char *vars_arg, char *lim, const char *name)
{
	char *s;
	int len;

	len = strlen(name);

	for (s = vars_arg; (s < lim) && *s;) {
		if ((bcmp(s, name, len) == 0) && (s[len] == '='))
			return (&s[len+1]);

		while (*s++)
			;
	}

	return NULL;
}

#ifdef BCMSPACE
char *defvars = "il0macaddr=00:11:22:33:44:55\0"
		"boardtype=0xffff\0"
		"boardrev=0x10\0"
		"boardflags=8\0"
		"aa0=3\0"
		"sromrev=2";
#define	DEFVARSLEN	89	/* Length of *defvars */
#else /* !BCMSPACE */
char *defvars = "";
#endif	/* BCMSPACE */

char *
nvram_get(const char *name)
{
	char *v = NULL;
	vars_t *cur;

	for (cur = vars; cur; cur = cur->next)
		if ((v = findvar(cur->vars, cur->vars + cur->size, name)))
			break;

#ifdef BCMSPACE
	if (v == NULL) {
		v = findvar(defvars, defvars + DEFVARSLEN, name);
		if (v)
			NVR_MSG(("%s: variable %s defaulted to %s\n",
			         __FUNCTION__, name, v));
	}
#endif	/* BCMSPACE */

	return v;
}

int
BCMATTACHFN(nvram_set)(const char *name, const char *value)
{
	return 0;
}

int
BCMATTACHFN(nvram_unset)(const char *name)
{
	return 0;
}

int
BCMATTACHFN(nvram_reset)(void *si)
{
	return 0;
}

int
BCMATTACHFN(nvram_commit)(void)
{
	return 0;
}

int
nvram_getall(char *buf, int count)
{
	int len, resid = count;
	vars_t *this;

	this = vars;
	while (this) {
		char *from, *lim, *to;
		int acc;

		from = this->vars;
		lim = (char *)((uintptr)this->vars + this->size);
		to = buf;
		acc = 0;
		while ((from < lim) && (*from)) {
			len = strlen(from) + 1;
			if (resid < (acc + len))
				return BCME_BUFTOOSHORT;
			bcopy(from, to, len);
			acc += len;
			from += len;
			to += len;
		}

		resid -= acc;
		buf += acc;
		this = this->next;
	}
	if (resid < 1)
		return BCME_BUFTOOSHORT;
	*buf = '\0';
	return 0;
}

#ifdef BCMQT
extern void nvram_printall(void);
/* QT: print nvram w/o a big buffer - helps w/memory consumption evaluation of USB bootloader */
void
nvram_printall(void)
{
	vars_t *this;

	this = vars;
	while (this) {
		char *from, *lim;

		from = this->vars;
		lim = (char *)((uintptr)this->vars + this->size);
		while ((from < lim) && (*from)) {
			printf("%s\n", from);
			from += strlen(from) + 1;
		}
		this = this->next;
	}
}
#endif /* BCMQT */
