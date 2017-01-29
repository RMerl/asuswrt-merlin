/*
 * Copyright (C) 2004 Intracom S.A.
 * Pantelis Antoniou <panto@intracom.gr>
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "internal.h"

struct fec {
	uint32_t	addr_low;	/* lower 32 bits of station address	*/
	uint32_t	addr_high;	/* upper 16 bits of station address||0  */
	uint32_t	hash_table_high;/* upper 32-bits of hash table		*/
	uint32_t	hash_table_low;	/* lower 32-bits of hash table		*/
	uint32_t	r_des_start;	/* beginning of Rx descriptor ring	*/
	uint32_t	x_des_start;	/* beginning of Tx descriptor ring	*/
	uint32_t	r_buff_size;	/* Rx buffer size			*/
	uint32_t	res2[9];	/* reserved				*/
	uint32_t	ecntrl;		/* ethernet control register		*/
	uint32_t	ievent;		/* interrupt event register		*/
	uint32_t	imask;		/* interrupt mask register		*/
	uint32_t	ivec;		/* interrupt level and vector status	*/
	uint32_t	r_des_active;	/* Rx ring updated flag			*/
	uint32_t	x_des_active;	/* Tx ring updated flag			*/
	uint32_t	res3[10];	/* reserved				*/
	uint32_t	mii_data;	/* MII data register			*/
	uint32_t	mii_speed;	/* MII speed control register		*/
	uint32_t	res4[17];	/* reserved				*/
	uint32_t	r_bound;	/* end of RAM (read-only)		*/
	uint32_t	r_fstart;	/* Rx FIFO start address		*/
	uint32_t	res5[6];	/* reserved				*/
	uint32_t	x_fstart;	/* Tx FIFO start address		*/
	uint32_t	res6[17];	/* reserved				*/
	uint32_t	fun_code;	/* fec SDMA function code		*/
	uint32_t	res7[3];	/* reserved				*/
	uint32_t	r_cntrl;	/* Rx control register			*/
	uint32_t	r_hash;		/* Rx hash register			*/
	uint32_t	res8[14];	/* reserved				*/
	uint32_t	x_cntrl;	/* Tx control register			*/
	uint32_t	res9[0x1e];	/* reserved				*/
};

#define DUMP_REG(f, x)	fprintf(stdout, \
			"0x%04lx: %-16s 0x%08x\n", \
				(unsigned long)(offsetof(struct fec, x)), \
				#x, f->x)

int fec_8xx_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	struct fec *f = (struct fec *)regs->data;

	fprintf(stdout, "Descriptor Registers\n");
	fprintf(stdout, "---------------------\n");

	DUMP_REG(f, addr_low);
	DUMP_REG(f, addr_high);
	DUMP_REG(f, hash_table_high);
	DUMP_REG(f, hash_table_low);
	DUMP_REG(f, r_des_start);
	DUMP_REG(f, x_des_start);
	DUMP_REG(f, r_buff_size);
	DUMP_REG(f, ecntrl);
	DUMP_REG(f, ievent);
	DUMP_REG(f, imask);
	DUMP_REG(f, ivec);
	DUMP_REG(f, r_des_active);
	DUMP_REG(f, x_des_active);
	DUMP_REG(f, mii_data);
	DUMP_REG(f, mii_speed);
	DUMP_REG(f, r_bound);
	DUMP_REG(f, r_fstart);
	DUMP_REG(f, x_fstart);
	DUMP_REG(f, fun_code);
	DUMP_REG(f, r_cntrl);
	DUMP_REG(f, r_hash);
	DUMP_REG(f, x_cntrl);

	return 0;
}
