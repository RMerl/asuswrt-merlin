/* $Id: bootstr.c,v 1.20 2000/02/08 20:24:23 davem Exp $
 * bootstr.c:  Boot string/argument acquisition from the PROM.
 *
 * Copyright(C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/string.h>
#include <asm/oplib.h>
#include <asm/sun4prom.h>
#include <linux/init.h>

#define BARG_LEN  256
static char barg_buf[BARG_LEN] = { 0 };
static char fetched __initdata = 0;

extern linux_sun4_romvec *sun4_romvec;

char * __init
prom_getbootargs(void)
{
	int iter;
	char *cp, *arg;

	/* This check saves us from a panic when bootfd patches args. */
	if (fetched) {
		return barg_buf;
	}

	switch(prom_vers) {
	case PROM_V0:
	case PROM_SUN4:
		cp = barg_buf;
		/* Start from 1 and go over fd(0,0,0)kernel */
		for(iter = 1; iter < 8; iter++) {
			arg = (*(romvec->pv_v0bootargs))->argv[iter];
			if(arg == 0) break;
			while(*arg != 0) {
				/* Leave place for space and null. */
				if(cp >= barg_buf + BARG_LEN-2){
					/* We might issue a warning here. */
					break;
				}
				*cp++ = *arg++;
			}
			*cp++ = ' ';
		}
		*cp = 0;
		break;
	case PROM_V2:
	case PROM_V3:
		/*
		 * V3 PROM cannot supply as with more than 128 bytes
		 * of an argument. But a smart bootstrap loader can.
		 */
		strlcpy(barg_buf, *romvec->pv_v2bootargs.bootargs, sizeof(barg_buf));
		break;
	default:
		break;
	}

	fetched = 1;
	return barg_buf;
}
