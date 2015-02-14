/*
 * mips/prid.h: MIPS processor ID values (cp_imp field).
 *
 * Copyright (c) 1998-1999, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

/* 
 * MIPS CPU types 
 */
#define	PRID_R2000	0x01	/* MIPS R2000 CPU		ISA I   */
#define	PRID_R3000	0x02	/* MIPS R3000 CPU		ISA I   */
#define	PRID_R6000	0x03	/* MIPS R6000 CPU		ISA II	*/
#define	PRID_R4000	0x04	/* MIPS R4000/4400 CPU		ISA III	*/
#define PRID_LR33K	0x05	/* LSI Logic R3000 derivate	ISA I	*/
#define	PRID_R6000A	0x06	/* MIPS R6000A CPU		ISA II	*/
#define	PRID_R3IDT	0x07	/* IDT R3000 derivates		ISA I	*/
#define	 PRID_R3IDT_R3041  0x07	  /* R3041 (cp_rev field) */
#define	 PRID_R3IDT_R36100 0x10	  /* R36100 (cp_rev field) */
#define	PRID_R10000	0x09	/* MIPS R10000/T5 CPU		ISA IV  */
#define	PRID_R4200	0x0a	/* MIPS R4200 CPU (ICE)		ISA III */
#define PRID_R4300	0x0b	/* NEC VR4300 CPU		ISA III */
#define PRID_R4100	0x0c	/* NEC VR4100 CPU		ISA III */
#define	PRID_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	PRID_RC6457X	0x15	/* IDT RC6457X CPU		ISA IV  */
#define	PRID_RC3233X	0x18	/* IDT RC3233X CPU		ISA MIPS32 */
#define	PRID_R4600	0x20	/* QED R4600 Orion		ISA III */
#define	PRID_R4700	0x21	/* QED R4700 Orion		ISA III */
#define	PRID_R3900	0x22	/* Toshiba/Philips R3900 CPU	ISA I	*/
#define	PRID_R4650	0x22	/* QED R4650/R4640 CPU		ISA III */
#define	PRID_R5000	0x23	/* MIPS R5000 CPU		ISA IV  */
#define	PRID_RC3236X	0x26	/* IDT RC3236X CPU		ISA MIPS32 */
#define	PRID_RM7000	0x27	/* QED RM7000 CPU		ISA IV  */
#define	PRID_RM52XX	0x28	/* QED RM52XX CPU		ISA IV  */
#define	PRID_RC6447X	0x30	/* IDT RC6447X CPU		ISA III */
#define	PRID_R5400	0x54	/* NEC Vr5400 CPU		ISA IV  */
#define PRID_JADE	0x80	/* MIPS JADE			ISA MIPS32 */

/*
 * MIPS FPU types
 */
#define	PRID_SOFT	0x00	/* Software emulation		ISA I   */
#define	PRID_R2360	0x01	/* MIPS R2360 FPC		ISA I   */
#define	PRID_R2010	0x02	/* MIPS R2010 FPC		ISA I   */
#define	PRID_R3010	0x03	/* MIPS R3010 FPC		ISA I   */
#define	PRID_R6010	0x04	/* MIPS R6010 FPC		ISA II  */
#define	PRID_R4010	0x05	/* MIPS R4000/R4400 FPC		ISA II  */
#define PRID_LR33010	0x06	/* LSI Logic derivate		ISA I	*/
#define	PRID_R10010	0x09	/* MIPS R10000/T5 FPU		ISA IV  */
#define	PRID_R4210	0x0a	/* MIPS R4200 FPC (ICE)		ISA III */
#define PRID_UNKF1	0x0b	/* unnanounced product cpu	ISA III */
#define	PRID_R8010	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	PRID_RC6457XF	0x15	/* IDT RC6457X FPU		ISA IV  */
#define	PRID_R4610	0x20	/* QED R4600 Orion		ISA III */
#define	PRID_R3SONY	0x21	/* Sony R3000 based FPU		ISA I   */
#define	PRID_R3910	0x22	/* Toshiba/Philips R3900 FPU	ISA I	*/
#define	PRID_R5010	0x23	/* MIPS R5000 FPU		ISA IV  */
#define	PRID_RM7000F	0x27	/* QED RM7000 FPU		ISA IV  */
#define	PRID_RM52XXF	0x28	/* QED RM52X FPU		ISA IV  */
#define	PRID_RC6447XF	0x30	/* IDT RC6447X FPU		ISA III */
#define	PRID_R5400F	0x54	/* NEC Vr5400 FPU		ISA IV  */
