
/* include/asm-m68knommu/MC68332.h: '332 control registers
 *
 * Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *
 */

#ifndef _MC68332_H_
#define _MC68332_H_

#define BYTE_REF(addr) (*((volatile unsigned char*)addr))
#define WORD_REF(addr) (*((volatile unsigned short*)addr))

#define PORTE_ADDR	0xfffa11
#define PORTE	BYTE_REF(PORTE_ADDR)
#define DDRE_ADDR	0xfffa15
#define DDRE	BYTE_REF(DDRE_ADDR)
#define PEPAR_ADDR	0xfffa17
#define PEPAR	BYTE_REF(PEPAR_ADDR)

#define PORTF_ADDR	0xfffa19
#define PORTF	BYTE_REF(PORTF_ADDR)
#define DDRF_ADDR	0xfffa1d
#define DDRF	BYTE_REF(DDRF_ADDR)
#define PFPAR_ADDR	0xfffa1f
#define PFPAR	BYTE_REF(PFPAR_ADDR)

#define PORTQS_ADDR	0xfffc15
#define PORTQS	BYTE_REF(PORTQS_ADDR)
#define DDRQS_ADDR	0xfffc17
#define DDRQS	BYTE_REF(DDRQS_ADDR)
#define PQSPAR_ADDR	0xfffc16
#define PQSPAR	BYTE_REF(PQSPAR_ADDR)

#define CSPAR0_ADDR 0xFFFA44
#define CSPAR0 WORD_REF(CSPAR0_ADDR)
#define CSPAR1_ADDR 0xFFFA46
#define CSPAR1 WORD_REF(CSPAR1_ADDR)
#define CSARBT_ADDR 0xFFFA48
#define CSARBT WORD_REF(CSARBT_ADDR)
#define CSOPBT_ADDR 0xFFFA4A
#define CSOPBT WORD_REF(CSOPBT_ADDR)
#define CSBAR0_ADDR 0xFFFA4C
#define CSBAR0 WORD_REF(CSBAR0_ADDR)
#define CSOR0_ADDR 0xFFFA4E
#define CSOR0 WORD_REF(CSOR0_ADDR)
#define CSBAR1_ADDR 0xFFFA50
#define CSBAR1 WORD_REF(CSBAR1_ADDR)
#define CSOR1_ADDR 0xFFFA52
#define CSOR1 WORD_REF(CSOR1_ADDR)
#define CSBAR2_ADDR 0xFFFA54
#define CSBAR2 WORD_REF(CSBAR2_ADDR)
#define CSOR2_ADDR 0xFFFA56
#define CSOR2 WORD_REF(CSOR2_ADDR)
#define CSBAR3_ADDR 0xFFFA58
#define CSBAR3 WORD_REF(CSBAR3_ADDR)
#define CSOR3_ADDR 0xFFFA5A
#define CSOR3 WORD_REF(CSOR3_ADDR)
#define CSBAR4_ADDR 0xFFFA5C
#define CSBAR4 WORD_REF(CSBAR4_ADDR)
#define CSOR4_ADDR 0xFFFA5E
#define CSOR4 WORD_REF(CSOR4_ADDR)
#define CSBAR5_ADDR 0xFFFA60
#define CSBAR5 WORD_REF(CSBAR5_ADDR)
#define CSOR5_ADDR 0xFFFA62
#define CSOR5 WORD_REF(CSOR5_ADDR)
#define CSBAR6_ADDR 0xFFFA64
#define CSBAR6 WORD_REF(CSBAR6_ADDR)
#define CSOR6_ADDR 0xFFFA66
#define CSOR6 WORD_REF(CSOR6_ADDR)
#define CSBAR7_ADDR 0xFFFA68
#define CSBAR7 WORD_REF(CSBAR7_ADDR)
#define CSOR7_ADDR 0xFFFA6A
#define CSOR7 WORD_REF(CSOR7_ADDR)
#define CSBAR8_ADDR 0xFFFA6C
#define CSBAR8 WORD_REF(CSBAR8_ADDR)
#define CSOR8_ADDR 0xFFFA6E
#define CSOR8 WORD_REF(CSOR8_ADDR)
#define CSBAR9_ADDR 0xFFFA70
#define CSBAR9 WORD_REF(CSBAR9_ADDR)
#define CSOR9_ADDR 0xFFFA72
#define CSOR9 WORD_REF(CSOR9_ADDR)
#define CSBAR10_ADDR 0xFFFA74
#define CSBAR10 WORD_REF(CSBAR10_ADDR)
#define CSOR10_ADDR 0xFFFA76
#define CSOR10 WORD_REF(CSOR10_ADDR)

#define CSOR_MODE_ASYNC	0x0000
#define CSOR_MODE_SYNC	0x8000
#define CSOR_MODE_MASK	0x8000
#define CSOR_BYTE_DISABLE	0x0000
#define CSOR_BYTE_UPPER		0x4000
#define CSOR_BYTE_LOWER		0x2000
#define CSOR_BYTE_BOTH		0x6000
#define CSOR_BYTE_MASK		0x6000
#define CSOR_RW_RSVD		0x0000
#define CSOR_RW_READ		0x0800
#define CSOR_RW_WRITE		0x1000
#define CSOR_RW_BOTH		0x1800
#define CSOR_RW_MASK		0x1800
#define CSOR_STROBE_DS		0x0400
#define CSOR_STROBE_AS		0x0000
#define CSOR_STROBE_MASK	0x0400
#define CSOR_DSACK_WAIT(x)	(wait << 6)
#define CSOR_DSACK_FTERM	(14 << 6)
#define CSOR_DSACK_EXTERNAL	(15 << 6)
#define CSOR_DSACK_MASK		0x03c0
#define CSOR_SPACE_CPU		0x0000
#define CSOR_SPACE_USER		0x0010
#define CSOR_SPACE_SU		0x0020
#define CSOR_SPACE_BOTH		0x0030
#define CSOR_SPACE_MASK		0x0030
#define CSOR_IPL_ALL		0x0000
#define CSOR_IPL_PRIORITY(x)	(x << 1)
#define CSOR_IPL_MASK		0x000e
#define CSOR_AVEC_ON		0x0001
#define CSOR_AVEC_OFF		0x0000
#define CSOR_AVEC_MASK		0x0001

#define CSBAR_ADDR(x)		((addr >> 11) << 3) 
#define CSBAR_ADDR_MASK		0xfff8
#define CSBAR_BLKSIZE_2K	0x0000
#define CSBAR_BLKSIZE_8K	0x0001
#define CSBAR_BLKSIZE_16K	0x0002
#define CSBAR_BLKSIZE_64K	0x0003
#define CSBAR_BLKSIZE_128K	0x0004
#define CSBAR_BLKSIZE_256K	0x0005
#define CSBAR_BLKSIZE_512K	0x0006
#define CSBAR_BLKSIZE_1M	0x0007
#define CSBAR_BLKSIZE_MASK	0x0007

#define CSPAR_DISC	0
#define CSPAR_ALT	1
#define CSPAR_CS8	2
#define CSPAR_CS16	3
#define CSPAR_MASK	3

#define CSPAR0_CSBOOT(x) (x << 0)
#define CSPAR0_CS0(x)	(x << 2)
#define CSPAR0_CS1(x)	(x << 4)
#define CSPAR0_CS2(x)	(x << 6)
#define CSPAR0_CS3(x)	(x << 8)
#define CSPAR0_CS4(x)	(x << 10)
#define CSPAR0_CS5(x)	(x << 12)

#define CSPAR1_CS6(x)	(x << 0)
#define CSPAR1_CS7(x)	(x << 2)
#define CSPAR1_CS8(x)	(x << 4)
#define CSPAR1_CS9(x)	(x << 6)
#define CSPAR1_CS10(x)	(x << 8)

#endif
