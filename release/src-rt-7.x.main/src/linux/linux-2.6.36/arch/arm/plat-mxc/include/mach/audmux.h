#ifndef __MACH_AUDMUX_H
#define __MACH_AUDMUX_H

#define MX27_AUDMUX_HPCR1_SSI0		0
#define MX27_AUDMUX_HPCR2_SSI1		1
#define MX27_AUDMUX_HPCR3_SSI_PINS_4	2
#define MX27_AUDMUX_PPCR1_SSI_PINS_1	3
#define MX27_AUDMUX_PPCR2_SSI_PINS_2	4
#define MX27_AUDMUX_PPCR3_SSI_PINS_3	5

#define MX31_AUDMUX_PORT1_SSI0		0
#define MX31_AUDMUX_PORT2_SSI1		1
#define MX31_AUDMUX_PORT3_SSI_PINS_3	2
#define MX31_AUDMUX_PORT4_SSI_PINS_4	3
#define MX31_AUDMUX_PORT5_SSI_PINS_5	4
#define MX31_AUDMUX_PORT6_SSI_PINS_6	5

/* Register definitions for the i.MX21/27 Digital Audio Multiplexer */
#define MXC_AUDMUX_V1_PCR_INMMASK(x)	((x) & 0xff)
#define MXC_AUDMUX_V1_PCR_INMEN		(1 << 8)
#define MXC_AUDMUX_V1_PCR_TXRXEN	(1 << 10)
#define MXC_AUDMUX_V1_PCR_SYN		(1 << 12)
#define MXC_AUDMUX_V1_PCR_RXDSEL(x)	(((x) & 0x7) << 13)
#define MXC_AUDMUX_V1_PCR_RFCSEL(x)	(((x) & 0xf) << 20)
#define MXC_AUDMUX_V1_PCR_RCLKDIR	(1 << 24)
#define MXC_AUDMUX_V1_PCR_RFSDIR	(1 << 25)
#define MXC_AUDMUX_V1_PCR_TFCSEL(x)	(((x) & 0xf) << 26)
#define MXC_AUDMUX_V1_PCR_TCLKDIR	(1 << 30)
#define MXC_AUDMUX_V1_PCR_TFSDIR	(1 << 31)

/* Register definitions for the i.MX25/31/35 Digital Audio Multiplexer */
#define MXC_AUDMUX_V2_PTCR_TFSDIR	(1 << 31)
#define MXC_AUDMUX_V2_PTCR_TFSEL(x)	(((x) & 0xf) << 27)
#define MXC_AUDMUX_V2_PTCR_TCLKDIR	(1 << 26)
#define MXC_AUDMUX_V2_PTCR_TCSEL(x)	(((x) & 0xf) << 22)
#define MXC_AUDMUX_V2_PTCR_RFSDIR	(1 << 21)
#define MXC_AUDMUX_V2_PTCR_RFSEL(x)	(((x) & 0xf) << 17)
#define MXC_AUDMUX_V2_PTCR_RCLKDIR	(1 << 16)
#define MXC_AUDMUX_V2_PTCR_RCSEL(x)	(((x) & 0xf) << 12)
#define MXC_AUDMUX_V2_PTCR_SYN		(1 << 11)

#define MXC_AUDMUX_V2_PDCR_RXDSEL(x)	(((x) & 0x7) << 13)
#define MXC_AUDMUX_V2_PDCR_TXRXEN	(1 << 12)
#define MXC_AUDMUX_V2_PDCR_MODE(x)	(((x) & 0x3) << 8)
#define MXC_AUDMUX_V2_PDCR_INMMASK(x)	((x) & 0xff)

int mxc_audmux_v1_configure_port(unsigned int port, unsigned int pcr);

int mxc_audmux_v2_configure_port(unsigned int port, unsigned int ptcr,
		unsigned int pdcr);

#endif /* __MACH_AUDMUX_H */
