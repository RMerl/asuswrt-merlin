/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash engine	File: dev_flashop_engine.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "lib_physio.h"
#include "addrspace.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"

#include "bsp_config.h"
#include "dev_newflash.h"

int flashop_engine(flashinstr_t *prog);

/* Not relocatable */
int flashop_engine_len = 0;
void *flashop_engine_ptr = (void *) flashop_engine;

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define roundup(a,b) (((a)+((b)-1))/(b)*(b))

#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)
/* 
 * When the order of address bits matters (such as when writing
 * commands or reading magic registers), we want to use the address
 * fixup mechanism in lib_physio. When the order of data bits matters
 * (such as when copying data from host memory to flash or
 * vice-versa), we do not want to use the address fixup mechanism in
 * lib_physio. When both matter (such as when using a write buffer
 * mechanism that requires that consecutive addresses be presented to
 * the flash), macros can't help us (see intel_pgm8 and intel_pgm16).
 */
#ifdef	BCMHND74K
#define data_read8(a) phys_read8((a)^7)
#define data_read16(a) phys_read16((a)^6)
#define data_write8(a,v) phys_write8((a)^7,v)
#define data_write16(a,v) phys_write16((a)^6,v)
#else	/* Not 74K, bcm33xx */
#define data_read8(a) phys_read8((a)^3)
#define data_read16(a) phys_read16((a)^2)
#define data_write8(a,v) phys_write8((a)^3,v)
#define data_write16(a,v) phys_write16((a)^2,v)
#endif	/* BCMHND74K */
#else
#define data_read8(a) phys_read8(a)
#define data_read16(a) phys_read16(a)
#define data_write8(a,v) phys_write8(a,v)
#define data_write16(a,v) phys_write16(a,v)
#endif

/* Read, 8-bit mode */
static void flash_read8(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t src = base + offset;

    while (blen > 0) {
	*buf++ = data_read8(src++);
	blen--;
    }
}

/* Read, 16-bit mode */
static void flash_read16(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t src = base + offset;
    uint16_t val;
    int n;

    while (blen > 0) {
	offset = src & 1;
	val = data_read16(src - offset);
	n = min(blen, 2 - offset);
	memcpy(buf, (uint8_t *) &val + offset, n);
	buf += n;
	src = src - offset + 2;
	blen -= n;
    }
}

/* CFI Query 8-bit */
static void _cfi_query8(long base, uint8_t *buf, int offset, int blen, int buswidth)
{
    physaddr_t src;

    phys_write8(base + FLASH_CFI_QUERY_ADDR*buswidth, FLASH_CFI_QUERY_MODE);
    for (src = base + offset*buswidth; blen; src += buswidth, buf++, blen--)
	*buf = phys_read8(src);
    phys_write8(base + FLASH_CFI_QUERY_ADDR*buswidth, FLASH_CFI_QUERY_EXIT);
}

/* CFI Query 8-bit */
static void cfi_query8(long base, uint8_t *buf, int offset, int blen)
{
    _cfi_query8(base, buf, offset, blen, 1);
}

/* CFI Query 16-bit in byte mode */
static void cfi_query16b(long base, uint8_t *buf, int offset, int blen)
{
    _cfi_query8(base, buf, offset, blen, 2);
}

/* CFI Query 16-bit in word mode */
static void cfi_query16(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t src;

    phys_write16(base + FLASH_CFI_QUERY_ADDR*2, FLASH_CFI_QUERY_MODE);
    for (src = base + offset*2; blen; src += 2, buf++, blen--)
	*buf = (uint8_t) phys_read16(src);
    phys_write16(base + FLASH_CFI_QUERY_ADDR*2, FLASH_CFI_QUERY_EXIT);
}

#if (FLASH_DRIVERS & FLASH_DRIVER_AMD)

/* AMD erase (8-bit) */
static int _amd_erase8(long base, int offset, int buswidth)
{
    physaddr_t dst = base + offset;

    /* Do an "unlock write" sequence  (cycles 1-2) */
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_MAGIC_1);
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_2*buswidth, AMD_FLASH_MAGIC_2);

    /* send the erase command (cycle 3) */
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_ERASE_3);

    /* Do an "unlock write" sequence (cycles 4-5) */
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_MAGIC_1);
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_2*buswidth, AMD_FLASH_MAGIC_2);

    /* Send the "erase sector" qualifier (cycle 6) */
    phys_write8(dst, AMD_FLASH_ERASE_SEC_6);

    /* Wait for the erase to complete */
    while (phys_read8(dst) != 0xff);

    return 0;
}

/* AMD erase (8-bit) */
static int amd_erase8(long base, int offset)
{
    return _amd_erase8(base, offset, 1);
}

/* AMD erase (16-bit in byte mode) */
static int amd_erase16b(long base, int offset)
{
    return _amd_erase8(base, offset, 2);
}

/* AMD erase (16-bit in word mode) */
static int amd_erase16(long base, int offset)
{
    physaddr_t dst = base + offset;

    /* Do an "unlock write" sequence  (cycles 1-2) */
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_MAGIC_1);
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_2*2, AMD_FLASH_MAGIC_2);

    /* send the erase command (cycle 3) */
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_ERASE_3);

    /* Do an "unlock write" sequence (cycles 4-5) */
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_MAGIC_1);
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_2*2, AMD_FLASH_MAGIC_2);

    /* Send the "erase sector" qualifier (cycle 6) */
    phys_write16(dst, AMD_FLASH_ERASE_SEC_6);

    /* Wait for the erase to complete */
    while ((phys_read16(dst) & 0xff) != 0xff);

    return 0;
}

/* AMD 8-bit program */
static int _amd_pgm8(long base, uint8_t *buf, int offset, int blen, int buswidth)
{
    physaddr_t dst = base + offset;
    uint8_t *ptr = buf;
    uint8_t tempVal;

    while (blen > 0) {
	/* Do an "unlock write" sequence  (cycles 1-2) */
	phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_MAGIC_1);
	phys_write8(base + AMD_FLASH_MAGIC_ADDR_2*buswidth, AMD_FLASH_MAGIC_2);

	/* Send a program command (cycle 3) */
	phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_PROGRAM);

	/* Write a byte (cycle 4) */
	data_write8(dst, *ptr);

	/* Wait for write to complete or timeout */
	while (((tempVal = data_read8(dst)) & 0x80) != (*ptr & 0x80)) {
		if (tempVal & 0x20) {
			/* Bit DQ5 is set */
			tempVal = data_read8(dst);
			if ((tempVal & 0x80) != (*ptr & 0x80)) {
				xprintf("\nERROR. Flash device write error at address %x\n", dst);
				return CFE_ERR_IOERR;
			}
		}
	}

	ptr++;
	dst++;
	blen--;
    }

    return 0;
}

/* AMD 8-bit program */
static int amd_pgm8(long base, uint8_t *buf, int offset, int blen)
{
    return _amd_pgm8(base, buf, offset, blen, 1);
}

/* AMD 16-bit pgm in 8-bit mode */
static int amd_pgm16b(long base, uint8_t *buf, int offset, int blen)
{
    return _amd_pgm8(base, buf, offset, blen, 2);
}

/* AMD 16-bit program */
static int amd_pgm16(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t dst = base + offset;
    uint16_t *ptr = (uint16_t *) buf;
    uint16_t *src;
    uint16_t tempVal;
    uint8_t tempData[2];

    while (blen > 0) {
		/* Do an "unlock write" sequence  (cycles 1-2) */
		phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_MAGIC_1);
		phys_write16(base + AMD_FLASH_MAGIC_ADDR_2*2, AMD_FLASH_MAGIC_2);

		/* Send a program command (cycle 3) */
		phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_PROGRAM);

		/* If ptr is not 2-byte aligned, copy the data before writing */
		if ((int)ptr & 1) {
			if(blen == 1) {
				/* For the last byte, read the next byte to write first */
				flash_read16(dst, tempData, 0, 2);
			} else {
				tempData[1] = *((uint8_t *)ptr + 1);
			}
			tempData[0] = *(uint8_t *)ptr;
			src = (uint16_t *)tempData;
		} else {
			src = ptr;
		}

		/* Write a byte (cycle 4) */
		data_write16(dst, *src);

		/* Wait for write to complete or timeout */
		while (((tempVal = data_read16(dst)) & 0x80) != (*src & 0x80)) {
			if (tempVal & 0x20) {
				/* Bit DQ5 is set */
				tempVal = data_read16(dst);
				if ((tempVal & 0x80) != (*src & 0x80)) {
					xprintf("\nERROR. Flash device write error at address %x\n", dst);
					return CFE_ERR_IOERR;
				}
			}
		}

		ptr++;
		dst += 2;
		blen -= 2;
    }

    return 0;
}

static void _amd_autosel8(long base, uint8_t *buf, int offset, int buswidth)
{
    /* Do an "unlock write" sequence  (cycles 1-2) */
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_MAGIC_1);
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_2*buswidth, AMD_FLASH_MAGIC_2);

    /* Send an autoselect command (cycle 3) */
    phys_write8(base + AMD_FLASH_MAGIC_ADDR_1*buswidth, AMD_FLASH_AUTOSEL);

    /* Read code (cycle 4) */
    *buf = phys_read8(base+offset*buswidth);

    /* Reset */
    phys_write8(base, AMD_FLASH_RESET);
}

static void amd_manid8(long base, uint8_t *buf)
{
    _amd_autosel8(base, buf, 0, 1);
}

static void amd_manid16b(long base, uint8_t *buf)
{
    _amd_autosel8(base, buf, 0, 2);
}

static void amd_devcode8(long base, uint8_t *buf)
{
    _amd_autosel8(base, buf, 1, 1);
}

static void amd_devcode16b(long base, uint8_t *buf)
{
    _amd_autosel8(base, buf, 1, 2);
}

static void amd_autosel16(long base, uint8_t *buf, int offset)
{
    /* Do an "unlock write" sequence  (cycles 1-2) */
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_MAGIC_1);
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_2*2, AMD_FLASH_MAGIC_2);

    /* Send an autoselect command (cycle 3) */
    phys_write16(base + AMD_FLASH_MAGIC_ADDR_1*2, AMD_FLASH_AUTOSEL);

    /* Read code (cycle 4) */
    *buf = (uint8_t) phys_read16(base+offset*2);

    /* Reset */
    phys_write16(base, AMD_FLASH_RESET);
}

static void amd_manid16(long base, uint8_t *buf)
{
    amd_autosel16(base, buf, 0);
}

static void amd_devcode16(long base, uint8_t *buf)
{
    amd_autosel16(base, buf, 1);
}

#endif

#if (FLASH_DRIVERS & FLASH_DRIVER_INTEL)

/* Intel extended erase 8-bit */
static int intelext_erase8(long base, int offset)
{
    physaddr_t dst = base + offset;
    int res;

    phys_write8(dst, INTEL_FLASH_ERASE_BLOCK);
    phys_write8(dst, INTEL_FLASH_ERASE_CONFIRM);

    /* poll for SR.7 */
    while (!((res = phys_read8(dst)) & 0x80));

    /* check for SR.4 or SR.5 */
    return (res & 0x30);
}

/* Intel erase 8-bit */
static int intel_erase8(long base, int offset)
{
    physaddr_t dst = base + offset;

    /* unlock block */
    phys_write16(dst, INTEL_FLASH_CONFIG_SETUP);
    phys_write16(dst, INTEL_FLASH_UNLOCK);

    return intelext_erase8(base, offset);
}

/* Intel extended erase 16-bit */
static int intelext_erase16(long base, int offset)
{
    physaddr_t dst = base + offset;
    int res;

    phys_write16(dst, INTEL_FLASH_ERASE_BLOCK);
    phys_write16(dst, INTEL_FLASH_ERASE_CONFIRM);

    /* poll for SR.7 */
    while (!((res = phys_read16(dst)) & 0x80));

    /* check for SR.4 or SR.5 */
    return (res & 0x30);
}

/* Intel erase 16-bit */
static int intel_erase16(long base, int offset)
{
    physaddr_t dst = base + offset;

    /* unlock block */
    phys_write16(dst, INTEL_FLASH_CONFIG_SETUP);
    phys_write16(dst, INTEL_FLASH_UNLOCK);

    return intelext_erase16(base, offset);
}

/* Intel 8-bit prog */
static int _intel_pgm8(long base, uint8_t *buf, int offset, int blen, int cmdset)
{
    physaddr_t dst = base + offset;
    uint8_t *ptr = buf;
    int n, res = 0;

    while (blen > 0) {
	if (cmdset == FLASH_CFI_CMDSET_INTEL_ECS &&
	    (dst & (INTEL_FLASH_WRITE_BUFFER_SIZE-1)) == 0) {
	    data_write8(dst, INTEL_FLASH_WRITE_BUFFER);

	    /* poll for XSR.7 */
	    while (!(data_read8(dst) & 0x80));

#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)
	    /* 
	     * This write buffer mechanism appears to be
	     * sensitive to the order of the addresses hence we need
	     * to unscramble them. We may also need to pad the source
	     * with up to three bytes of 0xffff in case an unaligned
	     * number of bytes are presented.
	     */

	    n = roundup(min(blen, INTEL_FLASH_WRITE_BUFFER_SIZE), 4);

	    /* write (nwords-1) */
	    data_write8(dst, n-1);

	    /* write data (plus pad if necessary) */
	    while (n) {
		data_write8(dst+3, blen < 4 ? 0xff : *(ptr+3));
		data_write8(dst+2, blen < 3 ? 0xff : *(ptr+2));
		data_write8(dst+1, blen < 2 ? 0xff : *(ptr+1));
		data_write8(dst+0, *ptr);
		ptr += 4;
		dst += 4;
		blen -= 4;
		n -= 2;
	    }
#else
	    n = min(blen, INTEL_FLASH_WRITE_BUFFER_SIZE);

	    /* write (nbytes-1) */
	    data_write8(dst, n-1);

	    /* write data */
	    while (n--) {
		data_write8(dst, *ptr);
		ptr++;
		dst++;
		blen--;
	    }
#endif

	    /* write confirm */
	    phys_write8(base, INTEL_FLASH_WRITE_CONFIRM);
	} else {
	    data_write8(dst, INTEL_FLASH_PROGRAM);
	    data_write8(dst, *ptr);
	    ptr++;
	    dst++;
	    blen--;
	}

	/* poll for SR.7 */
	while (!((res = phys_read8(base)) & 0x80));

	/* check for SR.4 or SR.5 */
	if (res & 0x30)
	    break;
    }

    /* reset */
    phys_write8(base, INTEL_FLASH_READ_MODE);

    /* check for SR.4 or SR.5 */
    return (res & 0x30);
}

/* Intel 8-bit prog */
static int intel_pgm8(long base, uint8_t *buf, int offset, int blen)
{
    return _intel_pgm8(base, buf, offset, blen, FLASH_CFI_CMDSET_INTEL_STD);
}

/* Intel extended 8-bit prog */
static int intelext_pgm8(long base, uint8_t *buf, int offset, int blen)
{
    return _intel_pgm8(base, buf, offset, blen, FLASH_CFI_CMDSET_INTEL_ECS);
}

/* Intel 16-bit prog */
static int _intel_pgm16(long base, uint8_t *buf, int offset, int blen, int cmdset)
{
    physaddr_t dst = base + offset;
    uint16_t *ptr = (uint16_t *) buf;
    int n, res = 0;

    while (blen > 0) {
	if (cmdset == FLASH_CFI_CMDSET_INTEL_ECS &&
	    (dst & (INTEL_FLASH_WRITE_BUFFER_SIZE-1)) == 0) {
	    data_write16(dst, INTEL_FLASH_WRITE_BUFFER);

	    /* poll for XSR.7 */
	    while (!(data_read16(dst) & 0x80));

#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)
	    /* 
	     * This write buffer mechanism appears to be
	     * sensitive to the order of the addresses hence we need
	     * to unscramble them. We may also need to pad the source
	     * with up to three bytes of 0xffff in case an unaligned
	     * number of bytes are presented.
	     */

	    n = roundup(min(blen, INTEL_FLASH_WRITE_BUFFER_SIZE), 4)/2;

	    /* write (nwords-1) */
	    data_write16(dst, n-1);

	    /* write data (plus pad if necessary) */
	    while (n > 0) {
		data_write16(dst+2, blen < 4 ? 0xffff : *(ptr+1));
		data_write16(dst, *ptr);
		ptr += 2;
		dst += 4;
		blen -= 4;
		n -= 2;
	    }
#else
	    n = min(blen, INTEL_FLASH_WRITE_BUFFER_SIZE)/2;

	    /* write (nwords-1) */
	    data_write16(dst, n-1);

	    /* write data */
	    while (n > 0) {
		data_write16(dst, *ptr);
		ptr++;
		dst += 2;
		blen -= 2;
		n--;
	    }
#endif

	    /* write confirm */
	    phys_write16(base, INTEL_FLASH_WRITE_CONFIRM);
	} else {
	    data_write16(dst, INTEL_FLASH_PROGRAM);
	    data_write16(dst, *ptr);
	    ptr++;
	    dst += 2;
	    blen -= 2;
	}

	/* poll for SR.7 */
	while (!((res = phys_read16(base)) & 0x80));

	/* check for SR.4 or SR.5 */
	if (res & 0x30)
	    break;
    }

    /* reset */
    phys_write16(base, INTEL_FLASH_READ_MODE);

    /* check for SR.4 or SR.5 */
    return (res & 0x30);
}

/* Intel 16-bit prog */
static int intel_pgm16(long base, uint8_t *buf, int offset, int blen)
{
    return _intel_pgm16(base, buf, offset, blen, FLASH_CFI_CMDSET_INTEL_STD);
}

/* Intel extended 16-bit prog */
static int intelext_pgm16(long base, uint8_t *buf, int offset, int blen)
{
    return _intel_pgm16(base, buf, offset, blen, FLASH_CFI_CMDSET_INTEL_ECS);
}

#endif

#if (FLASH_DRIVERS & FLASH_DRIVER_SST)

/* SST CFI Query 16-bit in word mode */
static void sst_cfi_query16(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t src;

    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_MAGIC_1);
    phys_write16(base + SST_FLASH_MAGIC_ADDR_2*2, SST_FLASH_MAGIC_2);

    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, FLASH_CFI_QUERY_MODE);
    for (src = base + offset*2; blen; src += 2, buf++, blen--)
	*buf = (uint8_t) phys_read16(src);
    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_RESET);
}

/* SST erase (16-bit in word mode) */
static int sst_erase16(long base, int offset)
{
    physaddr_t dst = base + offset;

    /* Do an "unlock write" sequence  (cycles 1-2) */
    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_MAGIC_1);
    phys_write16(base + SST_FLASH_MAGIC_ADDR_2*2, SST_FLASH_MAGIC_2);

    /* send the erase command (cycle 3) */
    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_ERASE_3);

    /* Do an "unlock write" sequence (cycles 4-5) */
    phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_MAGIC_1);
    phys_write16(base + SST_FLASH_MAGIC_ADDR_2*2, SST_FLASH_MAGIC_2);

    /* Send the "erase sector" qualifier (cycle 6) */
    phys_write16(dst, SST_FLASH_ERASE_SEC_6);

    /* Wait for the erase to complete */
    while ((phys_read16(dst) & 0xff) != 0xff);

    return 0;
}

/* SST 16-bit program */
static int sst_pgm16(long base, uint8_t *buf, int offset, int blen)
{
    physaddr_t dst = base + offset;
    uint16_t *ptr = (uint16_t *) buf;

    while (blen > 0) {
	/* Do an "unlock write" sequence  (cycles 1-2) */
	phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_MAGIC_1);
	phys_write16(base + SST_FLASH_MAGIC_ADDR_2*2, SST_FLASH_MAGIC_2);

	/* Send a program command (cycle 3) */
	phys_write16(base + SST_FLASH_MAGIC_ADDR_1*2, SST_FLASH_PROGRAM);

	/* Write a byte (cycle 4) */
	data_write16(dst, *ptr);

	/* Wait for write to complete */
	while ((data_read16(dst) & 0x80) != (*ptr & 0x80));

	ptr++;
	dst += 2;
	blen -= 2;
    }

    return 0;
}

#endif

int flashop_engine(flashinstr_t *prog)
{
	int ret = 0;

    for (;; prog++) {
		if (ret) {
			break;
		}
	switch (prog->fi_op) {
	    case FEOP_RETURN:
		return 0;
	    case FEOP_REBOOT:
		/* Jump to boot vector */
		((void (*)(void)) 0xbfc00000)();
		break;
	    case FEOP_READ8:
		flash_read8(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_READ16:
		flash_read16(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_CFIQUERY8:
		cfi_query8(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_CFIQUERY16:
		cfi_query16(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_CFIQUERY16B:
		cfi_query16b(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_MEMCPY:
		memcpy((uint8_t *) prog->fi_dest, (uint8_t *) prog->fi_src, prog->fi_cnt);
		break;
#if (FLASH_DRIVERS & FLASH_DRIVER_AMD)
	    case FEOP_AMD_ERASE8:
		amd_erase8(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_AMD_ERASE16:
		amd_erase16(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_AMD_ERASE16B:
		amd_erase16b(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_AMD_PGM8:
		ret |= amd_pgm8(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_AMD_PGM16:
		ret |= amd_pgm16(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_AMD_PGM16B:
		ret |= amd_pgm16b(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_AMD_MANID8:
		amd_manid8(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
	    case FEOP_AMD_MANID16:
		amd_manid16(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
	    case FEOP_AMD_MANID16B:
		amd_manid16b(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
	    case FEOP_AMD_DEVCODE8:
		amd_devcode8(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
	    case FEOP_AMD_DEVCODE16:
		amd_devcode16(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
	    case FEOP_AMD_DEVCODE16B:
		amd_devcode16b(prog->fi_base, (uint8_t *) prog->fi_dest);
		break;
#endif
#if (FLASH_DRIVERS & FLASH_DRIVER_INTEL)
	    case FEOP_INTEL_ERASE8:
		intel_erase8(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_INTEL_ERASE16:
		intel_erase16(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_INTEL_PGM8:
		intel_pgm8(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_INTEL_PGM16:
		intel_pgm16(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_INTELEXT_ERASE8:
		intelext_erase8(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_INTELEXT_ERASE16:
		intelext_erase16(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_INTELEXT_PGM8:
		intelext_pgm8(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
	    case FEOP_INTELEXT_PGM16:
		intelext_pgm16(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
#endif
#if (FLASH_DRIVERS & FLASH_DRIVER_SST)
	    case FEOP_SST_CFIQUERY16:
		sst_cfi_query16(prog->fi_base, (uint8_t *) prog->fi_dest, prog->fi_src, prog->fi_cnt);
		break;
	    case FEOP_SST_ERASE16:
		sst_erase16(prog->fi_base, prog->fi_dest);
		break;
	    case FEOP_SST_PGM16:
		sst_pgm16(prog->fi_base, (uint8_t *) prog->fi_src, prog->fi_dest, prog->fi_cnt);
		break;
#endif
	    default:
		return -1;
	}
    }

    return ret;
}
