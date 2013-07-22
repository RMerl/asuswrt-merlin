/* 
 * p5064/sbd.h: Algorithmics P5064 board definition header file
 * Copyright (c) 1998 Algorithmics Ltd.
 */

#ifndef MHZ
/* fastest possible pipeline clock (83MHz * 3) */
#define MHZ		250
#endif

#define RAMCYCLE	60			/* ~60ns dram cycle */
#define ROMCYCLE	1500			/* ~1500ns rom cycle */
#define CACHECYCLE	(1000/MHZ) 		/* pipeline clock */
#define CYCLETIME	CACHECYCLE
#define CACHEMISS	(CYCLETIME * 6)

/*
 * rough scaling factors for 2 instruction DELAY loop to get 1ms and 1us delays
 */
#define ASMDELAY(ns,icycle)	\
	(((ns) + (icycle)) / ((icycle) * 2))

#define CACHENS(ns)	ASMDELAY((ns), CACHECYCLE)
#define RAMNS(ns)	ASMDELAY((ns), CACHEMISS+RAMCYCLE)
#define ROMNS(ns)	ASMDELAY((ns), CACHEMISS+ROMCYCLE)
#define CACHEUS(us)	ASMDELAY((us)*1000, CACHECYCLE)
#define RAMUS(us)	ASMDELAY((us)*1000, CACHEMISS+RAMCYCLE)
#define ROMUS(us)	ASMDELAY((us)*1000, CACHEMISS+ROMCYCLE)
#define CACHEMS(ms)	((ms) * ASMDELAY(1000000, CACHECYCLE))
#define RAMMS(ms)	((ms) * ASMDELAY(1000000, CACHEMISS+RAMCYCLE))
#define ROMMS(ms)	((ms) * ASMDELAY(1000000, CACHEMISS+ROMCYCLE))

#ifndef __ASSEMBLER__
#define nsdelay(ns)	mips_cycle (ASMDELAY (ns, CACHECYCLE))
#define usdelay(us)	mips_cycle (ASMDELAY ((us)*1000, CACHECYCLE))
#endif

/* Our local to PCI bus apertures are set up with the following sizes */
#define PCI_MEM_SPACE_SIZE	(128 * 1024 * 1024)
#define PCI_IO_SPACE_SIZE	( 16 * 1024 * 1024)
#define PCI_CONF_SPACE_SIZE	(  1 * 1024 * 1024)

#define PCI_IDSEL_ETHER		0
#define PCI_IDSEL_SCSI		1
#define PCI_IDSEL_I82371	2
#define PCI_IDSEL_SLOT1		3
#define PCI_IDSEL_SLOT2		4
#define PCI_IDSEL_SLOT3		5

#define DRAM_BASE	0x00000000
#define PCI_MEM_SPACE	0x10000000	/* 128MB: s/w configurable */
#define PCI_IO_SPACE	0x1d000000	/*  16MB: s/w configurable */
#define PCI_CONF_SPACE	0x1ee00000	/*   1MB: s/w configurable */
#define V96XPBC_BASE	0x1ef00000	/*  64KB: s/w configurable */
#define BOOTPROM_BASE	0x1fc00000
#define EPROM_BASE	0x1fd00000
#define FLASH_BASE	0x1fe00000

/* subtract this from prom/flash base for safe byte accesses */
#define PROGRAM_OFFS	0x00400000

#define LED_BASE	0x1ff20010
#define  LED(n)	((3-(n))*4)
#define LCD_BASE	0x1ff30000
#define ZPIO_BASE	0x1ff40000
#define ZPIO_IACK	0x1ff50000
#define ZPIO_RETI	0x1ff50000
#define DBGUART_BASE	0x1ff60000	/* S2681 uart on debug board */
#define ICU_BASE	0x1ff90000
#define BCR0_BASE	0x1ffa0000
#define ENDIAN_BASE	(BCR0_BASE + 6 * 4)
#define BCR1_BASE	0x1ffb0000
#define DCR_BASE	0x1ffc0000
#define OPTION_BASE	0x1ffd0000

/* ISA i/o ports */
#define ISAPORT_BASE(x)	(PCI_IO_SPACE + (x))

#define DMA1_PORT	0x000
#define ICU1_PORT	0x020
#define CTC_PORT	0x040
#define DIAG_PORT	0x061
#define RTC_ADDR_PORT	0x070
#define RTC_DATA_PORT	0x071
#define KEYBD_PORT	0x060
#define DMAPAGE_PORT	0x080
#define SYSC_PORT	0x092
#define ICU2_PORT	0x0a0
#define DMA2_PORT	0x0c0
#define IDE_PORT	0x1f0
#define UART1_PORT	0x2f8
#define UART0_PORT	0x3f8
#define ECP_PORT	0x378
#define CEN_LATCH_PORT	0x37c	/* P5064 special */
#define FDC_PORT	0x3f0

#define GPIO_PORT	0xff00

/* ISA interrupt numbers */
#define TIMER0_IRQ	0
#define KEYBOARD_IRQ	1
#define ICU2_IRQ	2
#define SERIAL2_IRQ	3
#define SERIAL1_IRQ	4
#define PARALLEL2_IRQ	5
#define FDC_IRQ		6
#define PARALLEL1_IRQ	7
#define RTC_IRQ		8
#define NET_IRQ		9
#define MATH_IRQ	13
#define IDE_IRQ		14


/* Define UART baud rate and register layout */
#define NS16550_HZ	(24000000/13)
#ifdef __ASSEMBLER__
#define NSREG(x)	((x)*1)
#else
#define nsreg(x)	unsigned char x
#endif
#define UART0_BASE	ISAPORT_BASE(UART0_PORT)
#define UART1_BASE	ISAPORT_BASE(UART1_PORT)

/* Other ISA device base addresses */
#define ECP_BASE	ISAPORT_BASE(ECP_PORT)
#define FDC_BASE	ISAPORT_BASE(FDC_PORT)
#define IDE_BASE	ISAPORT_BASE(IDE_PORT)

/* DRAM control register (8 bits, 1 word per bit) */
#define DCR_ROWS	0x03	/* number of rows */
#define  DCR_ROWS_11	 0x01
#define  DCR_ROWS_12	 0x02
#define  DCR_ROWS_13	 0x03
#define DCR_COLS	0x0c	/* number of cols */
#define  DCR_COLS_8	 0x00
#define  DCR_COLS_9	 0x04
#define  DCR_COLS_10	 0x08
#define  DCR_COLS_11	 0x0c
#define DCR_BLOCKS	0x10	/* number of blocks per dram */
#define  DCR_BLOCKS_2	 0x00
#define  DCR_BLOCKS_4	 0x10
#define DCR_SINGLE	0x20	/* single-sided (!double-sided) */
#define DCR_DIMM0	0x40	/* clock DIMM0 data into FPGA */
#define DCR_DIMM1	0x00
#define DCR_SLOW_CAS	0x80	/* high CAS latency */

/* Board control register #0 offsets (8 bits, 1 word per bit) */
#define BCR0_CEN_BUSY	0x04	/* Centronics busy generation */
#define  BCR0_CEN_BUSY_MANUAL	0
#define  BCR0_CEN_BUSY_AUTO	-1
#define BCR0_LED	0x08	/* LED control */
#define  BCR0_LED_OFF	 	0
#define  BCR0_LED_ON	 	-1
#define BCR0_PSU	0x0c	/* PSU control */
#define  BCR0_PSU_ON	 	0
#define  BCR0_PSU_OFF	 	-1
#define BCR0_ENDIAN	0x18	/* I/O system endianess */
#define  BCR0_ENDIAN_LE	0
#define  BCR0_ENDIAN_BE	-1
#define BCR0_SCSI_TERM	0x1c	/* Active SCSI termination */
#define  BCR0_SCSI_TERM_OFF	0
#define  BCR0_SCSI_TERM_ON	-1

#define BCR0(x)		PHYS_TO_K1 (BCR0_BASE + (x))

#ifndef __ASSEMBLER__
typedef struct p5064bcr0 {
    unsigned int	spare0;
    unsigned int	cen_busy;
    unsigned int	led;
    unsigned int	psu;
    unsigned int	spare4;
    unsigned int	spare5;
    unsigned int	endian;
    unsigned int	scsi_term;
} p5064bcr0;
#endif


/* Board control register #1 offsets (8 bits, 1 word per bit) */
#define BCR1_V96X	0x00	/* enable V96x PCI chip (!reset) */
#define BCR1_SCSI	0x04	/* scsi chip enable (!reset) */
#define BCR1_ETH	0x08 	/* ethernet chip enable (!reset) */
#define BCR1_ISA	0x0c 	/* ISA bridge chip enable (!reset) */
#define BCR1_PCMCIA	0x10 	/* PCMCIA bridge chip enable (!reset) */

#ifndef __ASSEMBLER__
typedef struct p5064bcr1 {
    unsigned int	v96x;
    unsigned int	scsi;
    unsigned int	eth;
    unsigned int	isa;
    unsigned int	pcmcia;
    unsigned int	spare5;
    unsigned int	spare6;
    unsigned int	spare7;
} p5064bcr1;
#endif

#define BCR1_RESET	0
#define BCR1_NRESET	-1
#define BCR1_ENABLE	-1

#define BCR1(x)		PA_TO_KVA1 (BCR1_BASE + (x))

/* option switches */

#define OPTION_REV	0xf0
#define OPTION_REV_SHIFT 4
#define OPTION_LE	0x08
#define OPTION_USER	0x07
#define OPTION_USER2	0x04
#define OPTION_USER1	0x02
#define OPTION_USER0	0x01

#define BOARD_REVISION ((((*(unsigned int *)PHYS_TO_K1(OPTION_BASE)) & OPTION_REV) >> OPTION_REV_SHIFT) + 'A')

/* board attributes */
#define _SBD_PANEL

#ifndef __ASSEMBLER__
/* safe programmatic interface to write-only board registers */
unsigned int _imask_get (void);
unsigned int _imask_set (unsigned int);
unsigned int _imask_bis (unsigned int);
unsigned int _imask_bic (unsigned int);
#endif

/* soft copies of the board registers are kept in low memory */
#define SOFTC_BASE	(0x180-16)
#define SOFTC_BCR0	0x0	/* bcr0 register */
#define SOFTC_BCR1	0x1	/* bcr1 register */
#define SOFTC_DCR0	0x2	/* dcr0 register */
#define SOFTC_DCR1	0x3	/* dcr1 register */
#define SOFTC_IMASK	0x4	/* icu mask registers (4b) */
#define SOFTC_MEMSZ	0x8	/* memory size in 4Mb units */

#ifndef __ASSEMBLER__
typedef struct p5064softc {
    unsigned char	bcr0;
    unsigned char	bcr1;
    unsigned char	dcr0;
    unsigned char	dcr1;
    unsigned int	imask;
    unsigned char	memsz;
} p5064softc;
#endif


#ifndef __ASSEMBLER__
typedef union {
    /* read-only registers */
    struct {
	unsigned int dev;	/* local devices */
	unsigned int err;	/* errors */
	unsigned int pci;	/* PCI devices */
	unsigned int isa;	/* ISA bus and IDE */
    } irr;
    struct {
	unsigned int _pad[15];
	unsigned int rev;	/* FPGA revision */
    } fpga;
    /* write-only registers */
    struct {
	unsigned int devmask;	/* local device mask */
	unsigned int clear;	/* clear error bits */
	unsigned int pcimask;	/* PCI device mask */
	unsigned int isamask;	/* ISA/IDE mask */
	unsigned int devxbar0;	/* local device crossbar #0 */
	unsigned int devxbar1;	/* local device crossbar #1 */
	unsigned int xpcixbar;	/* PCI expansion bus crossbar */
	unsigned int isaxbar;	/* ISA/IDE crossbar */
	unsigned int lpcixbar;	/* PCI local bus crossbar (>=RevB)*/
    } ctrl;
} p5064icu;
#else
/* read-only registers */
#define ICU_IRR_DEV		0x00
#define ICU_IRR_ERR		0x04
#define ICU_IRR_PCI		0x08
#define ICU_IRR_ISA		0x0c

#define ICU_FPGA_REV		0x3c

/* write-only registers */
#define ICU_CTRL_MASK		0x00
#define ICU_CTRL_CLEAR		0x04
#define ICU_CTRL_PCIMASK	0x08
#define ICU_CTRL_ISAMASK	0x0c
#define ICU_CTRL_DEVXBAR0	0x10
#define ICU_CTRL_DEVXBAR1	0x14
#define ICU_CTRL_XPCIXBAR	0x18
#define ICU_CTRL_ISAXBAR	0x1c
#define ICU_CTRL_LPCIXBAR	0x20
#endif

/* Device interrupt request and mask bits */
#define ICU_DEV_RTC	0x80	/* Real-Time Clock */
#define ICU_DEV_PIO_REVAB 0x40	/* Z80pio (<=RevB) */
#define ICU_DEV_CENT	0x20	/* LPT1 */
#define ICU_DEV_UART1	0x10	/* COM2 */
#define ICU_DEV_UART0	0x08	/* COM1 */
#define ICU_DEV_KBD	0x04	/* Keyboard/Mouse */
#define ICU_DEV_FDC	0x02	/* Floppy Disk  */
#define ICU_DEV_V96X	0x01	/* V962 general purpose i/u */

/* Error interrupt request and clear bits */
#define ICU_ERR_PHYBUG	0x80	/* Phy lockup status (>=RevC) */ 
#define ICU_ERR_WAKEUP	0x40	/* Ethernet wakeup (>=RevC) */
#define ICU_ERR_CENT	0x20	/* LPT1 clear */
#define ICU_ERR_V96XPE	0x10	/* V962 parity error */
#define ICU_ERR_ISANMI	0x08	/* ISA NMI */
#define ICU_ERR_BUSERR	0x04	/* Bus Timeout */
#define ICU_ERR_ACFAIL	0x02	/* AC Power Fail */
#define ICU_ERR_DEBUG	0x01	/* Debug Button */

/* PCI request and mask bits */
#define ICU_PCI_INT3	0x80	/* PCI bus IRQ3 */
#define ICU_PCI_INT2	0x40	/* PCI bus IRQ2 */
#define ICU_PCI_INT1	0x20	/* PCI bus IRQ1 */
#define ICU_PCI_ETH_REVA ICU_PCI_INT1	/* PCI onboard Ethernet (RevA)*/
#define ICU_PCI_INT0	0x10	/* PCI bus IRQ0 */
#define ICU_PCI_SCSI_REVA ICU_PCI_INT0	/* PCI onboard SCSI (RevA)*/
#define ICU_PCI_USB	0x08	/* PCI onboard USB (>=RevB) */
#define ICU_PCI_SCSI	0x04	/* PCI onboard SCSI (>=RevB) */
#define ICU_PCI_ETH	0x02	/* PCI onboard Ethernet (>=RevB) */
#define ICU_PCI_MII	0x01	/* MII bus (>=RevC) */

/* ISA request and mask bits */
#define ICU_ISA_IDE1	0x04	/* Onboard secondary IDE */
#define ICU_ISA_IDE0	0x02	/* Onboard primary IDE */
#define ICU_ISA_ICU	0x01	/* ISA bus i8259 */

/* Combine individual request/mask registers into one 32-bit value */
#define ICU_DEV_MASK	0x000000ff
#define ICU_DEV_SHIFT	0
#define ICU_ERR_MASK	0x0000ff00
#define ICU_ERR_SHIFT	8
#define ICU_PCI_MASK	0x00ff0000
#define ICU_PCI_SHIFT	16
#define ICU_ISA_MASK	0xff000000
#define ICU_ISA_SHIFT	24

#define ICU_MASK_RTC	(ICU_DEV_RTC << ICU_DEV_SHIFT)
#define ICU_MASK_PIO_REVAB (ICU_DEV_PIO_REVAB << ICU_DEV_SHIFT)
#define ICU_MASK_CENT	(ICU_DEV_CENT << ICU_DEV_SHIFT)
#define ICU_MASK_UART1	(ICU_DEV_UART1 << ICU_DEV_SHIFT)
#define ICU_MASK_UART0	(ICU_DEV_UART0 << ICU_DEV_SHIFT)
#define ICU_MASK_KBD	(ICU_DEV_KBD << ICU_DEV_SHIFT)
#define ICU_MASK_FDC	(ICU_DEV_FDC << ICU_DEV_SHIFT)
#define ICU_MASK_V96X	(ICU_DEV_V96X << ICU_DEV_SHIFT)
#define ICU_MASK_V96XPE (ICU_ERR_V96XPE << ICU_ERR_SHIFT)
#define ICU_MASK_ISANMI (ICU_ERR_ISANMI << ICU_ERR_SHIFT)
#define ICU_MASK_BUSERR (ICU_ERR_BUSERR << ICU_ERR_SHIFT)
#define ICU_MASK_ACFAIL (ICU_ERR_ACFAIL << ICU_ERR_SHIFT)
#define ICU_MASK_DEBUG	(ICU_ERR_DEBUG << ICU_ERR_SHIFT)
#define ICU_MASK_WAKEUP	(ICU_ERR_WAKEUP << ICU_ERR_SHIFT)
#define ICU_MASK_INT3  (ICU_PCI_INT3 << ICU_PCI_SHIFT)
#define ICU_MASK_INT2  (ICU_PCI_INT2 << ICU_PCI_SHIFT)
#define ICU_MASK_INT1  (ICU_PCI_INT1 << ICU_PCI_SHIFT)
#define ICU_MASK_ETH_REVA ICU_MASK_INT1
#define ICU_MASK_INT0  (ICU_PCI_INT0 << ICU_PCI_SHIFT)
#define ICU_MASK_SCSI_REVA ICU_MASK_INT0
#define ICU_MASK_USB   (ICU_PCI_USB << ICU_PCI_SHIFT)
#define ICU_MASK_SCSI  (ICU_PCI_SCSI << ICU_PCI_SHIFT)
#define ICU_MASK_ETH   (ICU_PCI_ETH  << ICU_PCI_SHIFT)
#define ICU_MASK_MII   (ICU_PCI_MII << ICU_PCI_SHIFT)
#define ICU_MASK_IDE1  (ICU_ISA_IDE1 << ICU_ISA_SHIFT)
#define ICU_MASK_IDE0  (ICU_ISA_IDE0 << ICU_ISA_SHIFT)
#define ICU_MASK_ICU   (ICU_ISA_ICU << ICU_ISA_SHIFT)

/* devxbar0 shifts */
#define ICU_DEVX0_V96X	0
#define ICU_DEVX0_FDC	2
#define ICU_DEVX0_KBD	4
#define ICU_DEVX0_UART0 6

/* devxbar1 shifts */
#define ICU_DEVX1_UART1 0
#define ICU_DEVX1_CENT	2
#define ICU_DEVX1_PIO_REVAB 4
#define ICU_DEVX1_RTC	6

/* xpcixbar shifts */
#define ICU_XPCIX_0	0
#define ICU_XPCIX_SCSI_REVA	ICU_XPCIX_0
#define ICU_XPCIX_1	2
#define ICU_XPCIX_ETH_REVA	ICU_XPCIX_1
#define ICU_XPCIX_2	4
#define ICU_XPCIX_3	6

/* isaxbar shifts */
#define ICU_ISAX_ICU	0
#define ICU_ISAX_IDE0	2
#define ICU_ISAX_IDE1	4

/* lpcixbar shifts */
#define ICU_LPCIX_ETH	2
#define ICU_LPCIX_SCSI	4
#define ICU_LPCIX_USB	6

/* crossbar interrupt output selects */
#define ICU_HWINT0	0x0	/* send to CPU Intr0 */
#define ICU_HWINT1	0x1	/* send to CPU Intr1 */
#define ICU_HWINT2	0x2	/* send to CPU Intr2 */
#define ICU_HWINT3	0x3	/* send to CPU Intr3 */
#define ICU_HWMASK	0x3
