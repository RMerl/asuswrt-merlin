/*
 * i82371sb.h: Intel PCI to ISA bridge
 */

#define I82371_IDETIM		0x40
#define I82371_IDETIM_IDEENA	0x8000

#define I82371_IORT_XBCS	0x4c
#define  I82371_IORT_16BIT(x)	 0x04+(((x)&3)<<0)
#define  I82371_IORT_8BIT(x)	 0x40+(((x)&7)<<3)
#define  I82371_IORT_DMAAC	 0x80

#define I82371_XBCS		0x4e
#define  I82371_XBCS_RTCEN	 0x0001
#define  I82371_XBCS_KBDEN	 0x0002
#define  I82371_XBCS_BIOSWP	 0x0004
#define  I82371_XBCS_MOUSE	 0x0010
#define  I82371_XBCS_CPERR	 0x0020
#define  I82371_XBCS_LBIOS	 0x0040
#define  I82371_XBCS_XBIOS	 0x0080
#define  I82371_XBCS_APIC	 0x0100

#define I82371_PIRQRCA		0x60
#define I82371_PIRQRCB		0x61
#define I82371_PIRQRCC		0x62
#define I82371_PIRQRCD		0x63
#define  I82371_PIRQRC(d)	(0x80 | (d & 0xf))

#define I82371_TOM		0x69
#define  I82371_TOM_FWD_89	 0x02
#define  I82371_TOM_FWD_AB	 0x04
#define  I82371_TOM_FWD_LBIOS	 0x08
#define  I82371_TOM_TOM(mb)	 (((mb)-1) << 4)

#define I82371_MSTAT		0x6a
#define  I82371_MSTAT_ISADIV_3	 0x0001
#define  I82371_MSTAT_ISADIV_4	 0x0000
#define  I82371_MSTAT_USBE	 0x0010
#define  I82371_MSTAT_ESMIME	 0x0040
#define  I82371_MSTAT_NBRE	 0x0080
#define  I82371_MSTAT_SEDT	 0x8000

#define I82371_MBIRQ0		0x70

#define I82371_MBDMA0		0x76
#define I82371_MBDMA1		0x77
#define  I82371_MBDMA_CHNL(x)	((x) & 7)
#define  I82371_MBDMA_FAST	 0x80

#define I82371_PCSC		0x78
#define  I82371_PCSC_AMASK	 0xfffc
#define  I82371_PCSC_SIZE_4	 0x0000
#define  I82371_PCSC_SIZE_8	 0x0001
#define  I82371_PCSC_DISABLED	 0x0002
#define  I82371_PCSC_SIZE_16	 0x0003

#define I82371_APICBASE		0x80

#define I82371_DLC		0x82
#define  I82371_DLC_DT		 0x01 /* delayed transaction enb */
#define  I82371_DLC_PR		 0x02 /* passive release enb */
#define  I82371_DLC_USBPR	 0x04 /* USB passive release enb */
#define  I82371_DLC_DTTE	 0x08 /* SERR on delayed timeout */

#define I82371_SMICNTL		0xa0
#define I82371_SMIEN		0xa2

#define I82371_SEE		0xa4

#define I82371_FTMR		0xa8
#define I82371_SMIREQ		0xaa

#define I82371_CTLTMR		0xac
#define I82371_CTHTMR		0xae
