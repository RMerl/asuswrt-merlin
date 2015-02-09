/*
** Copyright 2000, 2001  Broadcom Corporation
** All Rights Reserved
**
** No portions of this material may be reproduced in any form 
** without the written permission of:
**
**   Broadcom Corporation
**   5300 California Avenue
**   Irvine, California 92617
**
** All information contained in this document is Broadcom 
** Corporation company private proprietary, and trade secret.
**
** ----------------------------------------------------------
**
** 
**
** $Id::                                                          $:
** $Rev::file =  : Global SVN Revision = 1780                     $:
**
*/

/* platform.c
 *
 * Use those header files for the northstar build
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <ddr_core.h>

#include <platform.h>

#define CACHE_LINE_SIZE   (32)

void flush_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long i;

	for (i = start; i < stop; i += CACHE_LINE_SIZE) {
		asm volatile("MCR  p15, 0, %0, c7, c10, 1"::"r"(i));
	}
	asm volatile("MCR  p15, 0, r0, c7, c10, 4":::); /* DSB */
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long i;

	for (i = start; i < stop; i += CACHE_LINE_SIZE) {
		asm volatile("MCR  p15, 0, %0, c7, c6, 1"::"r"(i));
	}

	asm volatile("MCR  p15, 0, r0, c7, c10, 4":::); /* DSB */
}

void SHMOO_FLUSH_DATA_TO_DRAM(ddr40_addr_t Address, unsigned int bytes)
{
	unsigned long const EndAddress = Address + bytes;
	flush_dcache_range(Address, EndAddress);
}

void SHMOO_INVALIDATE_DATA_FROM_DRAM(ddr40_addr_t Address, unsigned int bytes)
{
	unsigned long const EndAddress = Address + bytes;
	invalidate_dcache_range(Address, EndAddress);
}

void UART_OUT(unsigned int val)
{
	printf("%c", val);
}

void plot_dec_number(unsigned int val)
{
	printf("%08d", val);
}

void plot_hex_number(unsigned int val)
{
	printf("%08X", val);
}

unsigned int tb_r(ddr40_addr_t Address)
{
	unsigned int const Data = *(unsigned int *)Address;

	return Data;
}

void tb_w(ddr40_addr_t Address, unsigned int Data)
{
	volatile unsigned int dummy;

	*(unsigned int *)Address = Data;
	dummy = *(volatile unsigned int *)Address;
}

unsigned int SHMOO_DRAM_READ_32(ddr40_addr_t Address)
{
	return (*(unsigned int *)Address);
}

void SHMOO_DRAM_WRITE_32(ddr40_addr_t Address, unsigned int Data)
{
	*(unsigned int *)Address = Data;
}

int rewrite_dram_mode_regs(void *sih)
{
	return rewrite_mode_registers(sih);
}

/*
 * Functions for the PHY init
 */
void PrintfLog(char * const ptFormatStr, ...)
{
	char __tmp_buf[128] = {'L', 'o', 'g', ':', ' '};
	va_list arg;

	va_start(arg, ptFormatStr);
	vsprintf(&__tmp_buf[5], ptFormatStr, arg);
	va_end(arg);

	printf(__tmp_buf);
}

void PrintfErr(char * const ptFormatStr, ...)
{
	char __tmp_buf[128] = {'E', 'r', 'r', 'o', 'r', ':', ' '};
	va_list arg;

	va_start(arg, ptFormatStr);
	vsprintf(&__tmp_buf[7], ptFormatStr, arg);
	va_end(arg);

	printf(__tmp_buf);
}

void PrintfFatal(char * const ptFormatStr, ...)
{
	char __tmp_buf[128] = {'F', 'a', 't', 'a', 'l', ':', ' '};
	va_list arg;

	va_start(arg, ptFormatStr);
	vsprintf(&__tmp_buf[7], ptFormatStr, arg);
	va_end(arg);

	printf(__tmp_buf);
}
