/* 
 * p6032/hd2532.h: HD2532 alphanumeric display definitions
 *
 * Copyright (c) 2000-2001, Algorithmics Ltd.  All rights reserved.
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

#ifndef HD2532_STRIDE
#define HD2532_STRIDE		4
#endif
#ifndef HD2532_FLASH_OFFSET
#define HD2532_FLASH_OFFSET	0
#define HD2532_NFLASH_OFFSET	0x80
#endif

#define HD2532_FLASH(x)		(HD2532_FLASH_OFFSET+((x)*HD2532_STRIDE)
#define HD2532_UDCAR		(HD2532_NFLASH_OFFSET
#define HD2532_UDCRAM(x)	(HD2532_NFLASH_OFFSET+(0x08*HD2532_STRIDE)+((x)*HD2532_STRIDE))
#define HD2532_CW		(HD2532_NFLASH_OFFSET+(0x10*HD2532_STRIDE))
#define HD2532_CRAM		(HD2532_NFLASH_OFFSET+(0x18*HD2532_STRIDE))
#define HD2532_CHAR(x)		((x)*HD2532_STRIDE)

#define HD2532_CW_C	0x80	/* clear FLASH and CRAM */
#define HD2532_CW_S	0x40	/* start self test */
#define HD2532_CW_OK	0x20	/* self test passed */
#define HD2532_CW_BL	0x10	/* blink enable */
#define HD2532_CW_F	0x08	/* flash enable */
#define HD2532_CW_B(x)	((x)&7)	/* Brightness */
#define HD2532_CW_BMASK	0x07	/* Brightness 100% */
#define HD2532_CW_B100	0x00	/* Brightness 100% */
#define HD2532_CW_B80	0x01	/* Brightness 80% */
#define HD2532_CW_B53	0x02	/* Brightness 53% */
#define HD2532_CW_B40	0x03	/* Brightness 40% */
#define HD2532_CW_B27	0x04	/* Brightness 27% */
#define HD2532_CW_B20	0x05	/* Brightness 20% */
#define HD2532_CW_B13	0x06	/* Brightness 13% */
#define HD2532_CW_B0	0x07	/* Brightness 0% */
