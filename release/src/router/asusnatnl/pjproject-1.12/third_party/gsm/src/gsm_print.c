/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/gsm_print.c,v 1.1 1992/10/28 00:15:50 jutta Exp $ */

#include	<stdio.h>

#include "private.h"

#include "gsm.h"
#include "proto.h"

int gsm_print P3((f, s, c), FILE * f, gsm s, gsm_byte * c)
{
	word  	LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13*4];

	/* GSM_MAGIC  = (*c >> 4) & 0xF; */

	if (((*c >> 4) & 0x0F) != GSM_MAGIC) return -1;

	LARc[0]  = (*c++ & 0xF) << 2;		/* 1 */
	LARc[0] |= (*c >> 6) & 0x3;
	LARc[1]  = *c++ & 0x3F;
	LARc[2]  = (*c >> 3) & 0x1F;
	LARc[3]  = (*c++ & 0x7) << 2;
	LARc[3] |= (*c >> 6) & 0x3;
	LARc[4]  = (*c >> 2) & 0xF;
	LARc[5]  = (*c++ & 0x3) << 2;
	LARc[5] |= (*c >> 6) & 0x3;
	LARc[6]  = (*c >> 3) & 0x7;
	LARc[7]  = *c++ & 0x7;


	Nc[0]  = (*c >> 1) & 0x7F;
	bc[0]  = (*c++ & 0x1) << 1;
	bc[0] |= (*c >> 7) & 0x1;
	Mc[0]  = (*c >> 5) & 0x3;
	xmaxc[0]  = (*c++ & 0x1F) << 1;
	xmaxc[0] |= (*c >> 7) & 0x1;
	xmc[0]  = (*c >> 4) & 0x7;
	xmc[1]  = (*c >> 1) & 0x7;
	xmc[2]  = (*c++ & 0x1) << 2;
	xmc[2] |= (*c >> 6) & 0x3;
	xmc[3]  = (*c >> 3) & 0x7;
	xmc[4]  = *c++ & 0x7;
	xmc[5]  = (*c >> 5) & 0x7;
	xmc[6]  = (*c >> 2) & 0x7;
	xmc[7]  = (*c++ & 0x3) << 1;		/* 10 */
	xmc[7] |= (*c >> 7) & 0x1;
	xmc[8]  = (*c >> 4) & 0x7;
	xmc[9]  = (*c >> 1) & 0x7;
	xmc[10]  = (*c++ & 0x1) << 2;
	xmc[10] |= (*c >> 6) & 0x3;
	xmc[11]  = (*c >> 3) & 0x7;
	xmc[12]  = *c++ & 0x7;

	Nc[1]  = (*c >> 1) & 0x7F;
	bc[1]  = (*c++ & 0x1) << 1;
	bc[1] |= (*c >> 7) & 0x1;
	Mc[1]  = (*c >> 5) & 0x3;
	xmaxc[1]  = (*c++ & 0x1F) << 1;
	xmaxc[1] |= (*c >> 7) & 0x1;
	xmc[13]  = (*c >> 4) & 0x7;
	xmc[14]  = (*c >> 1) & 0x7;
	xmc[15]  = (*c++ & 0x1) << 2;
	xmc[15] |= (*c >> 6) & 0x3;
	xmc[16]  = (*c >> 3) & 0x7;
	xmc[17]  = *c++ & 0x7;
	xmc[18]  = (*c >> 5) & 0x7;
	xmc[19]  = (*c >> 2) & 0x7;
	xmc[20]  = (*c++ & 0x3) << 1;
	xmc[20] |= (*c >> 7) & 0x1;
	xmc[21]  = (*c >> 4) & 0x7;
	xmc[22]  = (*c >> 1) & 0x7;
	xmc[23]  = (*c++ & 0x1) << 2;
	xmc[23] |= (*c >> 6) & 0x3;
	xmc[24]  = (*c >> 3) & 0x7;
	xmc[25]  = *c++ & 0x7;


	Nc[2]  = (*c >> 1) & 0x7F;
	bc[2]  = (*c++ & 0x1) << 1;		/* 20 */
	bc[2] |= (*c >> 7) & 0x1;
	Mc[2]  = (*c >> 5) & 0x3;
	xmaxc[2]  = (*c++ & 0x1F) << 1;
	xmaxc[2] |= (*c >> 7) & 0x1;
	xmc[26]  = (*c >> 4) & 0x7;
	xmc[27]  = (*c >> 1) & 0x7;
	xmc[28]  = (*c++ & 0x1) << 2;
	xmc[28] |= (*c >> 6) & 0x3;
	xmc[29]  = (*c >> 3) & 0x7;
	xmc[30]  = *c++ & 0x7;
	xmc[31]  = (*c >> 5) & 0x7;
	xmc[32]  = (*c >> 2) & 0x7;
	xmc[33]  = (*c++ & 0x3) << 1;
	xmc[33] |= (*c >> 7) & 0x1;
	xmc[34]  = (*c >> 4) & 0x7;
	xmc[35]  = (*c >> 1) & 0x7;
	xmc[36]  = (*c++ & 0x1) << 2;
	xmc[36] |= (*c >> 6) & 0x3;
	xmc[37]  = (*c >> 3) & 0x7;
	xmc[38]  = *c++ & 0x7;

	Nc[3]  = (*c >> 1) & 0x7F;
	bc[3]  = (*c++ & 0x1) << 1;
	bc[3] |= (*c >> 7) & 0x1;
	Mc[3]  = (*c >> 5) & 0x3;
	xmaxc[3]  = (*c++ & 0x1F) << 1;
	xmaxc[3] |= (*c >> 7) & 0x1;

	xmc[39]  = (*c >> 4) & 0x7;
	xmc[40]  = (*c >> 1) & 0x7;
	xmc[41]  = (*c++ & 0x1) << 2;
	xmc[41] |= (*c >> 6) & 0x3;
	xmc[42]  = (*c >> 3) & 0x7;
	xmc[43]  = *c++ & 0x7;			/* 30  */
	xmc[44]  = (*c >> 5) & 0x7;
	xmc[45]  = (*c >> 2) & 0x7;
	xmc[46]  = (*c++ & 0x3) << 1;
	xmc[46] |= (*c >> 7) & 0x1;
	xmc[47]  = (*c >> 4) & 0x7;
	xmc[48]  = (*c >> 1) & 0x7;
	xmc[49]  = (*c++ & 0x1) << 2;
	xmc[49] |= (*c >> 6) & 0x3;
	xmc[50]  = (*c >> 3) & 0x7;
	xmc[51]  = *c & 0x7;			/* 33 */

	fprintf(f,
	      "LARc:\t%2.2d  %2.2d  %2.2d  %2.2d  %2.2d  %2.2d  %2.2d  %2.2d\n",
	       LARc[0],LARc[1],LARc[2],LARc[3],LARc[4],LARc[5],LARc[6],LARc[7]);

	fprintf(f, "#1: 	Nc %4.4d    bc %d    Mc %d    xmaxc %d\n",
		Nc[0], bc[0], Mc[0], xmaxc[0]);
	fprintf(f,
"\t%.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d\n",
		xmc[0],xmc[1],xmc[2],xmc[3],xmc[4],xmc[5],xmc[6],
		xmc[7],xmc[8],xmc[9],xmc[10],xmc[11],xmc[12] );

	fprintf(f, "#2: 	Nc %4.4d    bc %d    Mc %d    xmaxc %d\n",
		Nc[1], bc[1], Mc[1], xmaxc[1]);
	fprintf(f,
"\t%.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d\n",
		xmc[13+0],xmc[13+1],xmc[13+2],xmc[13+3],xmc[13+4],xmc[13+5],
		xmc[13+6], xmc[13+7],xmc[13+8],xmc[13+9],xmc[13+10],xmc[13+11],
		xmc[13+12] );

	fprintf(f, "#3: 	Nc %4.4d    bc %d    Mc %d    xmaxc %d\n",
		Nc[2], bc[2], Mc[2], xmaxc[2]);
	fprintf(f,
"\t%.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d\n",
		xmc[26+0],xmc[26+1],xmc[26+2],xmc[26+3],xmc[26+4],xmc[26+5],
		xmc[26+6], xmc[26+7],xmc[26+8],xmc[26+9],xmc[26+10],xmc[26+11],
		xmc[26+12] );

	fprintf(f, "#4: 	Nc %4.4d    bc %d    Mc %d    xmaxc %d\n",
		Nc[3], bc[3], Mc[3], xmaxc[3]);
	fprintf(f,
"\t%.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d\n",
		xmc[39+0],xmc[39+1],xmc[39+2],xmc[39+3],xmc[39+4],xmc[39+5],
		xmc[39+6], xmc[39+7],xmc[39+8],xmc[39+9],xmc[39+10],xmc[39+11],
		xmc[39+12] );

	return 0;
}
