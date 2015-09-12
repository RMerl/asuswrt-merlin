/*
 * Broadcom QSPI serial flash interface
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
 * $Id: $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <bcmdevs.h>
#include <qspi_core.h>
#include <hndsflash.h>
#include <chipcommonb.h>

#ifndef FLASH_SPI_MAX_PAGE_SIZE
#define FLASH_SPI_MAX_PAGE_SIZE   (256)
#endif

#ifdef BCMDBG
#define	SPIFL_MSG(args)	printf args
#else
#define	SPIFL_MSG(args)
#endif	/* BCMDBG */

#define MSPI_CALC_TIMEOUT(bytes, baud)	((((bytes * 9000)/baud) * 110)/100 + 1)

/* Private global state */
static hndsflash_t spiflash;

static int bspi_enabled = 1;
static bool firsttime = TRUE;

/* Prototype */
static int spiflash_read(hndsflash_t *spifl, uint offset, uint len, const uchar *buf);
static int spiflash_write(hndsflash_t *spifl, uint offset, uint len, const uchar *buf);
static int spiflash_erase(hndsflash_t *spifl, uint offset);
static int spiflash_poll(hndsflash_t *spifl, uint offset);
static int spiflash_commit(hndsflash_t *spifl, uint offset, uint len, const uchar *buf);
static int spiflash_open(si_t *sih, qspiregs_t *qspi);

/* Disable Boot SPI */
static void
mspi_disable_bspi(osl_t *osh, qspiregs_t *qspi)
{
	int i, j;
	unsigned int lval;

	if (bspi_enabled == 0)
		return;

	lval = R_REG(osh, &qspi->bspi_mast_n_boot_ctrl);
	if ((lval & 1) == 1)
		return;

	for (i = 0; i < 1000; i++) {
		lval = R_REG(osh, &qspi->bspi_busy_status);
		if ((lval & 1) == 0) {
			W_REG(osh, &qspi->bspi_mast_n_boot_ctrl, 0x1);
			bspi_enabled = 0;
			for (j = 0; j < 1000; j++);
				return;
		}
	}
}

/* Enable Boot SPI */
static void
mspi_enable_bspi(osl_t *osh, qspiregs_t *qspi)
{
	unsigned int lval;

	if (bspi_enabled == 1)
		return;

	lval = R_REG(osh, &qspi->bspi_mast_n_boot_ctrl);
	if ((lval & 1) == 0)
		return;

	W_REG(osh, &qspi->bspi_mast_n_boot_ctrl, 0x0);

	bspi_enabled = 1;
	return;
}

static int
mspi_wait(osl_t *osh, qspiregs_t *qspi, unsigned int timeout_ms)
{
	unsigned int lval;
	unsigned int loopCnt = ((timeout_ms * 1000) / 10) + 1;
	unsigned int count;
	int rc = -1;

	/* We must wait mspi_spcr2 spe bit clear before reading mspi_mspi_status */
	while (1) {
		lval = R_REG(osh, &qspi->mspi_spcr2);
		if ((lval & MSPI_SPCR2_spe_MASK) == 0)
			break;
	}

	/* Check status */
	for (count = 0; count < loopCnt; count++) {
		lval = R_REG(osh, &qspi->mspi_mspi_status);
		if (lval & MSPI_MSPI_STATUS_SPIF_MASK) {
			rc = 0;
			break;
		}

		/* Create some delay 5 times bigger */
		OSL_DELAY(100);
	}

	W_REG(osh, &qspi->mspi_mspi_status, 0);
	if (rc) {
		SPIFL_MSG(("Wait timeout\n"));
	}

	return rc;
}

static int
mspi_writeread(osl_t *osh, qspiregs_t *qspi, unsigned char *w_buf,
	unsigned char write_len, unsigned char *r_buf, unsigned char read_len)
{
	unsigned int lval;
	unsigned char i, len;

	len = write_len + read_len;
	for (i = 0; i < len; i++) {
		if (i < write_len) {
			/* Transmit Register File MSB */
			W_REG(osh, &qspi->mspi_txram[i * 2], (unsigned int)w_buf[i]);
		}

		lval = SPI_CDRAM_CONT | SPI_CDRAM_PCS_DISABLE_ALL | SPI_CDRAM_PCS_DSCK;
		lval &= ~(1 << BSPI_Pcs_eUpgSpiPcs2);
		/* Command Register File */
		W_REG(osh, &qspi->mspi_cdram[i], lval);
	}

	lval = SPI_CDRAM_PCS_DISABLE_ALL | SPI_CDRAM_PCS_DSCK;
	lval &= ~(1 << BSPI_Pcs_eUpgSpiPcs2);
	/* Command Register File */
	W_REG(osh, &qspi->mspi_cdram[len - 1], lval);

	/* Set queue pointers */
	W_REG(osh, &qspi->mspi_newqp, 0);
	W_REG(osh, &qspi->mspi_endqp, len - 1);

	/* Start SPI transfer */
	lval = R_REG(osh, &qspi->mspi_spcr2);
	lval |= MSPI_SPCR2_spe_MASK;
	W_REG(osh, &qspi->mspi_spcr2, lval);

	/* Wait for SPI to finish */
	if (mspi_wait(osh, qspi, MSPI_CALC_TIMEOUT(len, MAX_SPI_BAUD)) != 0)
		return 0;

	W_REG(osh, &qspi->mspi_write_lock, 0);

	for (i = write_len; i < len; ++i) {
		/* Data stored in the transmit register file LSB */
		r_buf[i-write_len] = (unsigned char)R_REG(osh, &qspi->mspi_rxram[1 + (i * 2)]);
	}

	return 1;
}

static int
mspi_writeread_continue(osl_t *osh, qspiregs_t *qspi, unsigned char *w_buf,
	unsigned char write_len, unsigned char *r_buf, unsigned char read_len)
{
	unsigned int lval;
	unsigned char i, len;

	len = write_len + read_len;
	for (i = 0; i < len; i++) {
		if (i < write_len)
			W_REG(osh, &qspi->mspi_txram[i * 2], (unsigned int)w_buf[i]);

		lval = SPI_CDRAM_CONT | SPI_CDRAM_PCS_DISABLE_ALL | SPI_CDRAM_PCS_DSCK;
		lval &= ~(1 << BSPI_Pcs_eUpgSpiPcs2);
		W_REG(osh, &qspi->mspi_cdram[i], lval);
	}

	/* Set queue pointers */
	W_REG(osh, &qspi->mspi_newqp, 0);
	W_REG(osh, &qspi->mspi_endqp, len - 1);

	W_REG(osh, &qspi->mspi_write_lock, 1);

	/* Start SPI transfer */
	lval = R_REG(osh, &qspi->mspi_spcr2);
	lval |= MSPI_SPCR2_cont_after_cmd_MASK | MSPI_SPCR2_spe_MASK;
	W_REG(osh, &qspi->mspi_spcr2, lval);

	/* Wait for SPI to finish */
	if (mspi_wait(osh, qspi, MSPI_CALC_TIMEOUT(len, MAX_SPI_BAUD)) != 0)
		return 0;

	for (i = write_len; i < len; ++i)
		r_buf[i-write_len] = (unsigned char)R_REG(osh, &qspi->mspi_rxram[1 + (i * 2)]);

	return 1;
}

/* Command codes for the flash_command routine */
#define FLASH_READ          0x03    /* read data from memory array */
#define FLASH_READ_FAST     0x0B    /* read data from memory array */
#define FLASH_PROG          0x02    /* program data into memory array */
#define FLASH_WREN          0x06    /* set write enable latch */
#define FLASH_WRDI          0x04    /* reset write enable latch */
#define FLASH_RDSR          0x05    /* read status register */
#define FLASH_WRST          0x01    /* write status register */
#define FLASH_EWSR          0x50    /* enable write status */
#define FLASH_WORD_AAI      0xAD    /* auto address increment word program */
#define FLASH_AAI           0xAF    /* auto address increment program */

#define SST_FLASH_CERASE    0x60    /* erase all sectors in memory array */
#define SST_FLASH_SERASE    0x20    /* erase one sector in memroy array */
#define SST_FLASH_RDID      0x90    /* read manufacturer and product id */

#define ATMEL_FLASH_CERASE  0x62    /* erase all sectors in memory array */
#define ATMEL_FLASH_SERASE  0x52    /* erase one sector in memroy array */
#define ATMEL_FLASH_RDID    0x15    /* read manufacturer and product id */

#define ATMEL_FLASH_PSEC    0x36    /* protect sector */
#define ATMEL_FLASH_UNPSEC  0x39    /* unprotect sector */
#define ATMEL_FLASH_RDPREG  0x3C    /* read protect sector registers */

#define AMD_FLASH_CERASE    0xC7    /* erase all sectors in memory array */
#define AMD_FLASH_SERASE    0xD8    /* erase one sector in memroy array */
#define AMD_FLASH_RDID      0xAB    /* read manufacturer and product id */

#define SPAN_FLASH_CERASE   0xC7    /* erase all sectors in memory array */
#define SPAN_FLASH_SERASE   0xD8    /* erase one sector in memory array */
#define SPAN_FLASH_RDID     0x9F    /* read manufacturer and product id */

/* Spansion commands for 4-byte addressing */
#define SPAN_FLASH_BRWR     0x17    /* bank register write */
#define SPAN_FLASH_4PP      0x12    /* page program with 4-byte address */
#define SPAN_FLASH_4P4E     0x21    /* parameter 4-kB secotor erase with 4-byte address */
#define SPAN_FLASH_4SE      0xDC    /* sectore erase with 4-byte address */

#define ST_FLASH_RDID       0x9F   /* read manufacturer and product id */
#define ST_FLASH_RDFSR      0x70   /* read flag status register */

/* ATMEL's manufacturer ID */
#define ATMELPART           0x1F

/* A list of ATMEL device ID's - add others as needed */
#define ID_AT25F512         0x60
#define ID_AT25F512A        0x65
#define ID_AT25F2048        0x63
#define ID_AT26F004         0x04
#define ID_AT25DF041A       0x44

/* AMD's device ID */
#define AMD_S25FL002D       0x11

/* SST's manufacturer ID */
#define SSTPART             0xBF

/* A list of SST device ID's - add others as needed */
#define ID_SST25VF016B      0x41
#define ID_SST25VF020       0x43
#define ID_SST25VF040       0x44
#define ID_SST25VF080       0x80

/* Winbond/NexFlash's manufacturer ID */
#define NXPART              0xEF

/* A list of NexFlash device ID's - add others as needed */
#define ID_NX25P20          0x11
#define ID_NX25P40          0x12
#define ID_W25X16           0x14
#define ID_W25X32           0x15
#define ID_W25X64           0x16

/* StFlash's manufacturer ID */
#define STPART              0x12

/* A list of StFlash device ID's - add others as needed */
#define ID_M25P40           0x12

/* SPANSION manufacturer ID */
#define SPANPART            0x01

/* SPANSION device ID's */
#define ID_SPAN25FL002A     0x11
#define ID_SPAN25FL004A     0x12
#define ID_SPAN25FL008A     0x13
#define ID_SPAN25FL016A     0x14
#define ID_SPAN25FL032A     0x15
#define ID_SPAN25FL064A     0x16
#define ID_SPAN25FL128A     0x17

/* EON manufacturer ID */
#define EONPART             0x1C
/* NUMONYX manufacturer ID */
#define NUMONYXPART         0x20
/* Macronix manufacturer ID */
#define MACRONIXPART        0xC2

/* JEDEC device ID */
#define ID_M25P64           0x17

/* Use READ_ID (0x90) command
 * Manufacturer Identification - 1 byte
 * Device Identification - 2 bytes
 * Extended Device Identification - 2 bytes
 */
#define MSPI_IDS_READLEN	5

/* Enable/disable BSPI 4-byte mode read */
static void
spiflash_set_4byte_mode(hndsflash_t *spifl, int enable)
{
	osl_t *osh = si_osh(spifl->sih);
	qspiregs_t *qspi = (qspiregs_t *)spifl->core;
	unsigned char cmd[1];

	/* Use MSPI to configure flash for entering/exiting 4-byte mode */
	mspi_disable_bspi(osh, qspi);
	cmd[0] = SPI_WREN_CMD;
	mspi_writeread(osh, qspi, cmd, 1, NULL, 0);
	if (spifl->vendor_id == SPANPART)
		cmd[0] = enable? SPAN_FLASH_BRWR : SPI_EX4B_CMD;
	else
		cmd[0] = enable? SPI_EN4B_CMD : SPI_EX4B_CMD;
	mspi_writeread(osh, qspi, cmd, 1, NULL, 0);
	cmd[0] = SPI_WRDI_CMD;
	mspi_writeread(osh, qspi, cmd, 1, NULL, 0);

	if (enable) {
		/* Enable 32-bit address */
		OR_REG(osh, &qspi->bspi_bits_per_phase, BSPI_BITS_PER_PHASE_ADDR_MARK);
	} else {
		/* Disable 32-bit address */
		AND_REG(osh, &qspi->bspi_bits_per_phase, ~BSPI_BITS_PER_PHASE_ADDR_MARK);
	}

	/* BSPI by default */
	mspi_enable_bspi(osh, qspi);
}

static uint16
NTOS(unsigned char *a)
{
	uint16 v;
	v = (a[0]*256) + a[1];
	return v;
}

static unsigned short
mspi_read_id(osl_t *osh, qspiregs_t *qspi)
{
	unsigned char cmd[4];
	unsigned char data[5];

	/* Try SST flashes read product id command */
	cmd[0] = SST_FLASH_RDID;
	cmd[1] = 0;
	cmd[2] = 0;
	cmd[3] = 0;
	if (mspi_writeread(osh, qspi, cmd, 4, data, 2)) {
		if (data[0] == SSTPART || data[0] == NXPART) {
			return NTOS(data);
		}
	}

	/* Try ATMEL flashes read product id command */
	cmd[0] = ATMEL_FLASH_RDID;
	if (mspi_writeread(osh, qspi, cmd, 1, data, 2)) {
		if (data[0] == ATMELPART) {
			return NTOS(data);
		}
	}

	/* Try SPANSION flashes read product id command */
	cmd[0] = SPAN_FLASH_RDID;
	if (mspi_writeread(osh, qspi, cmd, 1, data, 3)) {
		if (data[0] == ATMELPART) {
			return NTOS(data);
		}

		if ((data[0] == NUMONYXPART) || (data[0] == SPANPART) ||
		    (data[0] == EONPART) || (data[0] == MACRONIXPART)) {
			data[1] = data[2];
			return NTOS(data);
		}
	}

	/*
	 * AMD_FLASH_RDID is the same as RES command for SPAN,
	 * so it has to be the last one.
	 */
	cmd[0] = AMD_FLASH_RDID;
	cmd[1] = 0;
	cmd[2] = 0;
	cmd[3] = 0;
	if (mspi_writeread(osh, qspi, cmd, 4, data, 2)) {
		if (data[0] == AMD_S25FL002D || data[0] == STPART) {
			return NTOS(data);
		}
	}

	return 0;
}

static int
bspi_sector_erase(hndsflash_t *spifl, qspiregs_t *qspi, unsigned int offset)
{
	si_t *sih = spifl->sih;
	osl_t *osh;
	int result = 0;
	unsigned char cmd[5];
	unsigned char data;
	int idx;

	ASSERT(sih);
	osh = si_osh(sih);
	switch (spifl->type) {
	case QSPIFLASH_ST:
		cmd[0] = SPI_WREN_CMD;
		if ((result = mspi_writeread(osh, qspi, cmd, 1, NULL, 0)) != 1)
			break;

		idx = 0;
		if (spifl->vendor_id == SPANPART && (spifl->device_id & 0xff) >= 0x19)
			cmd[idx++] = (spifl->blocksize < (64 * 1024)) ?
				SPAN_FLASH_4P4E : SPAN_FLASH_4SE;
		else
			cmd[idx++] = (spifl->blocksize < (64 * 1024)) ? SPI_SSE_CMD : SPI_SE_CMD;
		if (spifl->size > 0x1000000) {
			cmd[idx++] = ((offset & 0xFF000000) >> 24);
		}
		cmd[idx++] = ((offset & 0x00FF0000) >> 16);
		cmd[idx++] = ((offset & 0x0000FF00) >> 8);
		cmd[idx++] = (offset & 0x000000FF);
		if ((result = mspi_writeread(osh, qspi, cmd, idx, NULL, 0)) != 1)
			break;

		/* Check for ST Write In Progress bit */
		do {
			cmd[0] = SPI_RDSR_CMD;
			if ((result = mspi_writeread(osh, qspi, cmd, 1, &data, 1)) != 1)
				break;
		} while(data & 0x01); /* busy check */

		if (result != 1)
			break;

		/* disable the write */
		cmd[0] = SPI_WRDI_CMD;
		result = mspi_writeread(osh, qspi, cmd, 1, NULL, 0);
		break;

	case QSPIFLASH_AT:
		cmd[0] = SPI_AT_PAGE_ERASE;
		offset = offset << 1;
		cmd[1] = ((offset & 0x00FF0000) >> 16);
		cmd[2] = ((offset & 0x0000FF00) >> 8);
		cmd[3] = (offset & 0x000000FF);
		if ((result = mspi_writeread(osh, qspi, cmd, 4, NULL, 0)) != 1)
			break;

		/* Check for Atmel Ready bit */
		do {
			cmd[0] = SPI_AT_STATUS;
			if ((result = mspi_writeread(osh, qspi, cmd, 1, &data, 1)) != 1)
				break;
		} while(data & 0x80); /* busy check */

		if (result != 1)
			break;

		/* disable the write */
		cmd[0] = SPI_WRDI_CMD;
		result = mspi_writeread(osh, qspi, cmd, 1, NULL, 0);
		break;
	}

	return result;
}

static int
bspi2_st_page_program(hndsflash_t *spifl, qspiregs_t *qspi, unsigned int offset,
	unsigned char *buf, int len)
{
	si_t *sih = spifl->sih;
	osl_t *osh;
	int result = 0;
	int i, len_total;
	static unsigned char cmd[FLASH_SPI_MAX_PAGE_SIZE+5];
	unsigned char data;
	int idx;

	ASSERT(sih);
	osh = si_osh(sih);
	if (len >(FLASH_SPI_MAX_PAGE_SIZE+4)) /* Max bytes per transaction */
		goto done;

	cmd[0] = SPI_WREN_CMD;
	if ((result = mspi_writeread(osh, qspi, cmd, 1, NULL, 0)) != 1)
		goto done;

	idx = 0;
	if (spifl->vendor_id == SPANPART && (spifl->device_id & 0xff) >= 0x19)
		cmd[idx++] = SPAN_FLASH_4PP;
	else
		cmd[idx++] = SPI_PP_CMD;
	if (spifl->size > 0x1000000) {
		cmd[idx++] = ((offset & 0xFF000000) >> 24);
	}
	cmd[idx++] = ((offset & 0x00FF0000) >> 16);
	cmd[idx++] = ((offset & 0x0000FF00) >> 8);
	cmd[idx++] = (offset & 0x000000FF);

	/*
	 * If new SPI has fix for byte orderig, define FLASH_SPI_BYTE_ORDER_FIX
	 * to 1 in qspi_core.h
	 */
#if defined(IL_BIGENDIAN) && !defined(FLASH_SPI_BYTE_ORDER_FIX)
	/*
	 * mspi does not handle byte ordering for be mode. Handle it here.
	 * Let's assume len is multiple of 4
	 */
	for (i = 0; i < len; i = i+4) {
		cmd[i + idx + 0] = buf[i + 3];
		cmd[i + idx + 1] = buf[i + 2];
		cmd[i + idx + 2] = buf[i + 1];
		cmd[i + idx + 3] = buf[i + 0];
	}
#else
	for (i = 0; i < len; ++i)
		cmd[i + idx] = buf[i];
#endif

	i = 0;
	len_total = len + idx;

	while (len_total > 16) {
		if ((result = mspi_writeread_continue(osh, qspi, cmd + i, 16, NULL, 0)) != 1)
			goto done;
		i += 16;
		len_total -= 16;
	}

	if (len_total <= 16 && len_total > 0) {
		if ((result = mspi_writeread(osh, qspi, cmd+i, len_total, NULL, 0)) != 1)
			goto done;
	}

	do {
		cmd[0] = SPI_RDSR_CMD;
		if ((result = mspi_writeread(osh, qspi, cmd, 1, &data, 1)) != 1)
			goto done;
	} while(data & 0x01 /* busy*/);

	/* disable the write */
	cmd[0] = SPI_WRDI_CMD;
	if ((result = mspi_writeread(osh, qspi, cmd, 1, NULL, 0)) != 1)
		goto done;

done:
	return result;
}

static int
bspi2_at_page_program(osl_t *osh, qspiregs_t *qspi, hndsflash_t *spifl, unsigned int offset,
	unsigned char *buf, int len)
{
	int result = 0;
	int i, len_total;
	static unsigned char cmd[FLASH_SPI_MAX_PAGE_SIZE+4];
	unsigned char data;
	uint32 page, byte, mask;

	mask = spifl->blocksize - 1;
	page = (offset & ~mask) << 1;
	byte = offset & mask;

	/* Read main memory page into buffer 1 */
	if (byte || (len < spifl->blocksize)) {
		cmd[0] = SPI_AT_BUF1_LOAD;
		cmd[1] = ((page & 0x00FF0000) >> 16);
		cmd[2] = ((page & 0x0000FF00) >> 8);
		cmd[3] = (page & 0x000000FF);

		if ((result = mspi_writeread(osh, qspi, cmd, 4, NULL, 0)) != 1)
			goto done;

		do {
			cmd[0] = SPI_AT_STATUS;
			if ((result = mspi_writeread(osh, qspi, cmd, 1, &data, 1)) != 1)
				goto done;
		} while(!(data & SPI_AT_READY) /* not ready */);
	}

	/* Write into buffer 1 */
	cmd[0] = SPI_AT_BUF1_WRITE;
	cmd[1] = ((byte & 0x00FF0000) >> 16);
	cmd[2] = ((byte & 0x0000FF00) >> 8);
	cmd[3] = (byte & 0x000000FF);
	/*
	 * If new SPI has fix for byte orderig, define FLASH_SPI_BYTE_ORDER_FIX
	 * to 1 in qspi_core.h
	 */
#if defined(IL_BIGENDIAN) && !defined(FLASH_SPI_BYTE_ORDER_FIX)
	/*
	 * mspi does not handle byte ordering for be mode. Handle it here.
	 * Let's assume len is multiple of 4
	 */
	for (i = 0; i < len; i = i+4) {
		cmd[i+4] = buf[i+3];
		cmd[i+5] = buf[i+2];
		cmd[i+6] = buf[i+1];
		cmd[i+7] = buf[i];
	}
#else
	for (i = 0; i < len; ++i)
		cmd[i+4] = buf[i];
#endif

	i = 0;
	len_total = len + 4;

	while (len_total > 16) {
		if ((result = mspi_writeread_continue(osh, qspi, cmd + i, 16, NULL, 0)) != 1)
			goto done;
		i += 16;
		len_total -= 16;
	}

	if (len_total <= 16 && len_total > 0) {
		if ((result = mspi_writeread(osh, qspi, cmd+i, len_total, NULL, 0)) != 1)
			goto done;
	}

	/* Write buffer 1 into main memory page */
	cmd[0] = SPI_AT_BUF1_PROGRAM;
	cmd[1] = ((page & 0x00FF0000) >> 16);
	cmd[2] = ((page & 0x0000FF00) >> 8);
	cmd[3] = (page & 0x000000FF);
	result = mspi_writeread(osh, qspi, cmd, 4, NULL, 0);

done:
	return result;
}

static int bspi_poll(hndsflash_t *spifl, qspiregs_t *qspi, unsigned int offset)
{
	si_t *sih = spifl->sih;
	osl_t *osh;
	int result = 1;
	unsigned char cmd[4];
	unsigned char data;

	ASSERT(sih);

	osh = si_osh(sih);

	switch (spifl->device_id & 0x00ff)
	{
	case 0x20:
		do {
			cmd[0] = ST_FLASH_RDFSR;
			if ((result = mspi_writeread(osh, qspi, cmd, 1, &data, 1)) != 1) {
				result = 0;
				break;
			}
		} while ((data & 0x80) == 0);
		break;
	default:
		break;
	}

	return result;
}

/* Initialize serial flash access */
hndsflash_t *
spiflash_init(si_t *sih)
{
	qspiregs_t *qspi;
	uint16 device_id;
	const char *name = "";
	osl_t *osh;
	uint8 vendor_id;
	int force_3byte_mode = 0;

	ASSERT(sih);

	/* Only support chipcommon revision == 42 for now */
	if (sih->ccrev != 42)
		return NULL;

	if ((qspi = (qspiregs_t *)si_setcore(sih, NS_QSPI_CORE_ID, 0)) == NULL)
		return NULL;

	if (!firsttime && spiflash.size != 0)
		return &spiflash;

	osh = si_osh(sih);

	bzero(&spiflash, sizeof(spiflash));
	spiflash.sih = sih;
	spiflash.core = (void *)qspi;
	spiflash.read = spiflash_read;
	spiflash.write = spiflash_write;
	spiflash.erase = spiflash_erase;
	spiflash.commit = spiflash_commit;

	spiflash.device_id = device_id = mspi_read_id(osh, qspi);
	spiflash.vendor_id = vendor_id = (device_id >> 8);
	switch (vendor_id) {
	case SPANPART:
	case MACRONIXPART:
	case NUMONYXPART:
	case NXPART:
		/* ST compatible */
		if (vendor_id == SPANPART)
			name = "ST compatible";
		else if (vendor_id == MACRONIXPART)
			name = "ST compatible (Marconix)";
		else if (vendor_id == NXPART)
			name = "ST compatible (Winbond/NexFlash)";
		else
			name = "ST compatible (Micron)";

		spiflash.type = QSPIFLASH_ST;
		spiflash.blocksize = 64 * 1024;

		switch ((unsigned short)(device_id & 0x00ff)) {
		case 0x11:
			/* ST M25P20 2 Mbit Serial Flash */
			spiflash.numblocks = 4;
			break;
		case 0x12:
			/* ST M25P40 4 Mbit Serial Flash */
			spiflash.numblocks = 8;
			break;
		case 0x13:
			spiflash.numblocks = 16;
			break;
		case 0x14:
			/* ST M25P16 16 Mbit Serial Flash */
			spiflash.numblocks = 32;
			break;
		case 0x15:
			/* ST M25P32 32 Mbit Serial Flash */
			spiflash.numblocks = 64;
			break;
		case 0x16:
			/* ST M25P64 64 Mbit Serial Flash */
			spiflash.numblocks = 128;
			break;
		case 0x17:
		case 0x18:
			/* ST M25FL128 128 Mbit Serial Flash */
			spiflash.numblocks = 256;
			break;
		case 0x19:
			spiflash.numblocks = 512;
			break;
		case 0x20:
			spiflash.numblocks = 1024;
			/* Special poll requirement for Micron N25Q512 */
			spiflash.poll = spiflash_poll;
			break;
		}
		break;

	case SSTPART:
		name = "SST";
		spiflash.type = QSPIFLASH_ST;
		spiflash.blocksize = 4 * 1024;
		switch ((unsigned char)(device_id & 0x00ff)) {
		case 1:
			/* SST25WF512 512 Kbit Serial Flash */
			spiflash.numblocks = 16;
			break;
		case 0x48:
			/* SST25VF512 512 Kbit Serial Flash */
			spiflash.numblocks = 16;
			break;
		case 2:
			/* SST25WF010 1 Mbit Serial Flash */
			spiflash.numblocks = 32;
			break;
		case 0x49:
			/* SST25VF010 1 Mbit Serial Flash */
			spiflash.numblocks = 32;
			break;
		case 3:
			/* SST25WF020 2 Mbit Serial Flash */
			spiflash.numblocks = 64;
			break;
		case 0x43:
			/* SST25VF020 2 Mbit Serial Flash */
			spiflash.numblocks = 64;
			break;
		case 4:
			/* SST25WF040 4 Mbit Serial Flash */
			spiflash.numblocks = 128;
			break;
		case 0x44:
			/* SST25VF040 4 Mbit Serial Flash */
			spiflash.numblocks = 128;
			break;
		case 0x8d:
			/* SST25VF040B 4 Mbit Serial Flash */
			spiflash.numblocks = 128;
			break;
		case 5:
			/* SST25WF080 8 Mbit Serial Flash */
			spiflash.numblocks = 256;
			break;
		case 0x8e:
			/* SST25VF080B 8 Mbit Serial Flash */
			spiflash.numblocks = 256;
			break;
		case 0x41:
			/* SST25VF016 16 Mbit Serial Flash */
			spiflash.numblocks = 512;
			break;
		case 0x4a:
			/* SST25VF032 32 Mbit Serial Flash */
			spiflash.numblocks = 1024;
			break;
		case 0x4b:
			/* SST25VF064 64 Mbit Serial Flash */
			spiflash.numblocks = 2048;
			break;
		}
		break;

	case ATMELPART:
		/* SFLASH_AT_STATUS not implement */
		name = "Atmel";
		spiflash.type = QSPIFLASH_AT;
		switch ((char)(device_id & 0x00ff)) {
		case ID_AT25F512:
			spiflash.numblocks = 2;
			spiflash.blocksize = 32 * 1024;
			break;

		case ID_AT25F2048:
			spiflash.numblocks = 4;
			spiflash.blocksize = 64 * 1024;
			break;
		default:
			break;
		}
		break;

	default:
		spiflash.numblocks = 0;
		SPIFL_MSG(("Unknown flash, device_id:0x%02X\n", device_id));
		return NULL;
	}

	/* Open device here */
	spiflash_open(sih, qspi);

	spiflash.size = spiflash.blocksize * spiflash.numblocks;

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		uint32 chip_rev, straps_ctrl;
		uint32 *srab_base, *dmu_base;
		/* Get chip revision */
		srab_base = (uint32 *)REG_MAP(CHIPCB_SRAB_BASE, SI_CORE_SIZE);
		W_REG(osh, (uint32 *)((uint32)srab_base + CHIPCB_SRAB_CMDSTAT_OFFSET), 0x02400001);
		chip_rev = R_REG(osh,
			(uint32 *)((uint32)srab_base + CHIPCB_SRAB_RDL_OFFSET)) & 0xff;
		REG_UNMAP(srab_base);
		if (CHIPID(sih->chip) == BCM4707_CHIP_ID && chip_rev < 2) {
			force_3byte_mode = 1;
		}
		/* Check 4BYTE_MODE strap */
		dmu_base = (uint32 *)REG_MAP(CHIPCB_DMU_BASE, SI_CORE_SIZE);
		straps_ctrl = R_REG(osh,
			(uint32 *)((uint32)dmu_base + CHIPCB_CRU_STRAPS_CTRL_OFFSET));
		REG_UNMAP(dmu_base);
		if (!(straps_ctrl & CHIPCB_CRU_STRAPS_4BYTE))
			force_3byte_mode = 1;
	}
	/* NOR flash size check. */
	if (force_3byte_mode && (spiflash.size > SI_FLASH_WINDOW)) {
		SPIFL_MSG(("NOR flash size %dMB is bigger than %dMB, limit it to %dMB\n",
			(spiflash.size >> 20), (SI_FLASH_WINDOW >> 20),
			(SI_FLASH_WINDOW >> 20)));
		spiflash.size = SI_FLASH_WINDOW;
	} else if (spiflash.size > SI_NS_FLASH_WINDOW) {
		SPIFL_MSG(("NOR flash size %dMB is bigger than %dMB, limit it to %dMB\n",
			(spiflash.size >> 20), (SI_NS_FLASH_WINDOW >> 20),
			(SI_NS_FLASH_WINDOW >> 20)));
		spiflash.size = SI_NS_FLASH_WINDOW;
	}

	spiflash.phybase = SI_NS_NORFLASH;
	if (spiflash.size)
		spiflash.base = (uint32)REG_MAP(SI_NS_NORFLASH, spiflash.size);

	if (firsttime) {
		if (spiflash.size == 0)
			printf("ERROR: Unknown flash, device_id:0x%02X\n", device_id);
		else {
			/* Enter 4-byte mode if size > 16MB */
			if (spiflash.size > 0x1000000) {
				spiflash_set_4byte_mode(&spiflash, 1);
			} else {
				spiflash_set_4byte_mode(&spiflash, 0);
			}
			printf("Found a %s serial flash with %d %dKB blocks; total size %dMB\n",
				name, spiflash.numblocks, spiflash.blocksize / 1024,
				spiflash.size / (1024 * 1024));
		}
	}

	firsttime = FALSE;
	return spiflash.size ? &spiflash : NULL;
}

static int
spiflash_open(si_t *sih, qspiregs_t *qspi)
{
	osl_t *osh = NULL;
	unsigned int lval;

	ASSERT(sih);

	osh = si_osh(sih);

	lval = SPI_SYSTEM_CLK / (2 * MAX_SPI_BAUD);
	W_REG(osh, &qspi->mspi_spcr0_lsb, lval);
	lval = R_REG(osh, &qspi->mspi_spcr0_msb);
	lval &= ~(MSPI_SPCR0_MSB_CPOL_MASK | MSPI_SPCR0_MSB_CPHA_MASK | MSPI_SPCR0_MSB_BitS_MASK);
	lval |= (MSPI_SPCR0_MSB_MSTR_MASK | (MSPI_SPCR0_MSB_CPOL_MASK | MSPI_SPCR0_MSB_CPHA_MASK |
		(0x8 << MSPI_SPCR0_MSB_BitS_SHIFT)));
	W_REG(osh, &qspi->mspi_spcr0_msb, lval);

	/* add delay if it is 7422A0 */
	W_REG(osh, &qspi->mspi_spcr1_msb, 128);

	return 0;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
static int
spiflash_read(hndsflash_t *spifl, uint offset, uint len, const uchar *buf)
{
	si_t *sih = spifl->sih;
	qspiregs_t *qspi = (qspiregs_t *)spifl->core;
	osl_t *osh;
	uint8 *from, *to;
	int cnt, i;

	ASSERT(sih);

	osh = si_osh(sih);

	if (!len)
		return 0;

	if ((offset + len) > spifl->size)
		return -22;

	if ((len >= 4) && (offset & 3))
		cnt = 4 - (offset & 3);
	else if ((len >= 4) && ((uintptr)buf & 3))
		cnt = 4 - ((uintptr)buf & 3);
	else
		cnt = len;

	from = (uint8 *)((void *)spifl->base + offset);
	to = (uint8 *)buf;

	mspi_enable_bspi(osh, qspi);
	if (cnt < 4) {
		for (i = 0; i < cnt; i ++) {
			/* Cannot use R_REG because in bigendian that will
			 * xor the address and we don't want that here.
			 */
			*to = *from;
			from ++;
			to ++;
		}
		return cnt;
	}

	while (cnt >= 4) {
		*(uint32 *)to = *(uint32 *)from;
		from += 4;
		to += 4;
		cnt -= 4;
	}

	return (len - cnt);
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written
 */
static int
spiflash_write(hndsflash_t *spifl, uint offset, uint length, const uchar *buffer)
{
	si_t *sih = spifl->sih;
	qspiregs_t *qspi = (qspiregs_t *)spifl->core;
	const uint8 *buf = buffer;
	int ret = 0;
	osl_t *osh;
	unsigned i, j, number_byte_to_write;
	uchar temp[FLASH_SPI_MAX_PAGE_SIZE+28];

	ASSERT(sih);

	osh = si_osh(sih);

	if (!length)
		return 0;

	if ((offset + length) > spifl->size)
		return -22;

	mspi_disable_bspi(osh, qspi);

	switch (spifl->type) {
	case QSPIFLASH_ST:
		for (i = 0; i < length; i += number_byte_to_write) {
			if (i + FLASH_SPI_MAX_PAGE_SIZE >= length)
				number_byte_to_write = length - i;
			else
				number_byte_to_write = FLASH_SPI_MAX_PAGE_SIZE;

			/* to prevent address cross the page boundary of 256 bytes. */
			if (number_byte_to_write > (256 - (i&0xFF)))
				number_byte_to_write = 256 - (i&0xFF);

			for (j = 0; j < number_byte_to_write; j++) {
				temp[j] = *buf;
				buf += 1;
			}
			if (!bspi2_st_page_program(spifl, qspi, offset + i, &temp[0],
				number_byte_to_write)) {
				SPIFL_MSG(("Program fail\n"));
				ret = -11;
				break;
			}

			if (spifl->poll && !bspi_poll(spifl, qspi, offset + i)) {
				SPIFL_MSG(("Poll fail\n"));
				ret = -33;
				break;
			}

			ret += number_byte_to_write;
		}
		break;

	case QSPIFLASH_AT:
		for (i = 0; i < length; i += number_byte_to_write) {
			if (i + FLASH_SPI_MAX_PAGE_SIZE >= length)
				number_byte_to_write = length - i;
			else
				number_byte_to_write = FLASH_SPI_MAX_PAGE_SIZE;

			/* to prevent address cross the page boundary of 256 bytes. */
			if (number_byte_to_write > (256 - (i&0xFF)))
				number_byte_to_write = 256 - (i&0xFF);

			for (j = 0; j < number_byte_to_write; j++) {
				temp[j] = *buf;
				buf += 1;
			}
			if (!bspi2_at_page_program(osh, qspi, spifl, offset + i, &temp[0],
				number_byte_to_write)) {
				SPIFL_MSG(("Program fail\n"));
				ret = -11;
				break;
			}
			ret += number_byte_to_write;
		}
		break;
	}

	mspi_enable_bspi(osh, qspi);
	return ret;
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
static int
spiflash_erase(hndsflash_t *spifl, uint offset)
{
	si_t *sih = spifl->sih;
	qspiregs_t *qspi = (qspiregs_t *)spifl->core;
	int erase_size = 0;
	osl_t *osh = NULL;

	ASSERT(sih);

	osh = si_osh(sih);

	if (offset >= spifl->size)
		return -22;

	mspi_disable_bspi(osh, qspi);

	if (bspi_sector_erase(spifl, qspi, offset))
		erase_size = spifl->blocksize;

	mspi_enable_bspi(osh, qspi);

	return erase_size;
}

/*
 * Poll function called after write/erase operations. Returns 0 when poll
 * completes.
 */
static int
spiflash_poll(hndsflash_t *spifl, uint offset)
{
	int result = 0;
	si_t *sih = spifl->sih;
	qspiregs_t *qspi = (qspiregs_t *)spifl->core;
	osl_t *osh = NULL;

	ASSERT(sih);

	osh = si_osh(sih);

	if (offset >= spifl->size)
		return -22;

	mspi_disable_bspi(osh, qspi);

	switch (spifl->type) {
	case QSPIFLASH_ST:
		/* Larger part such as the Micron 25Q512A (64MB) requires the flash status register
		 * (FSR) polled after write/erase.
		 */
		if (!bspi_poll(spifl, qspi, offset)) {
			result = 1;	/* Poll failed */
		}
		break;

	default:
		/* Nothing to do */
		break;
	}

	mspi_enable_bspi(osh, qspi);

	return result;
}

/*
 * writes the appropriate range of flash, a NULL buf simply erases
 * the region of flash
 */
static int
spiflash_commit(hndsflash_t *spifl, uint offset, uint len, const uchar *buf)
{
	si_t *sih = spifl->sih;
	uchar *block = NULL, *cur_ptr, *blk_ptr;
	uint blocksize = 0, mask, cur_offset, cur_length, cur_retlen, remainder;
	uint blk_offset, blk_len, copied;
	int bytes, ret = 0;
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	/* Check address range */
	if (len <= 0)
		return 0;

	if ((offset + len) > spifl->size)
		return -1;

	blocksize = spifl->blocksize;
	mask = blocksize - 1;

	/* Allocate a block of mem */
	if (!(block = MALLOC(osh, blocksize)))
		return -1;

	while (len) {
		/* Align offset */
		cur_offset = offset & ~mask;
		cur_length = blocksize;
		cur_ptr = block;

		remainder = blocksize - (offset & mask);
		if (len < remainder)
			cur_retlen = len;
		else
			cur_retlen = remainder;

		/* buf == NULL means erase only */
		if (buf) {
			/* Copy existing data into holding block if necessary */
			if ((offset & mask) || (len < blocksize)) {
				blk_offset = cur_offset;
				blk_len = cur_length;
				blk_ptr = cur_ptr;

				/* Copy entire block */
				while (blk_len) {
					copied = spiflash_read(spifl, blk_offset, blk_len,
						blk_ptr);
					blk_offset += copied;
					blk_len -= copied;
					blk_ptr += copied;
				}
			}

			/* Copy input data into holding block */
			memcpy(cur_ptr + (offset & mask), buf, cur_retlen);
		}

		/* Erase block */
		if ((ret = spiflash_erase(spifl, (uint)cur_offset)) < 0)
			goto done;

		/* buf == NULL means erase only */
		if (!buf) {
			offset += cur_retlen;
			len -= cur_retlen;
			continue;
		}

		/* Write holding block */
		while (cur_length > 0) {
			if ((bytes = spiflash_write(spifl,
			                          (uint)cur_offset,
			                          (uint)cur_length,
			                          (uchar *)cur_ptr)) < 0) {
				ret = bytes;
				goto done;
			}
			cur_offset += bytes;
			cur_length -= bytes;
			cur_ptr += bytes;
		}

		offset += cur_retlen;
		len -= cur_retlen;
		buf += cur_retlen;
	}

	ret = len;
done:
	if (block)
		MFREE(osh, block, blocksize);
	return ret;
}
