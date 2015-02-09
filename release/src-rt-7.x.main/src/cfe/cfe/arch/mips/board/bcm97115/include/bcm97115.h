#define DDR_BASE_ADR_REG    0xFFE00000


#define UARTA_BASE          0xFFFE00B0       /* UART A registers */
#define UARTB_BASE          0xFFFE00C0       /* UART B registers */
#define UARTC_BASE          0xFFFE01A0       /* UART C registers */
#define UARTD_BASE          0xFFFE01A8       /* UART D registers */
#define TIMR_ADR_BASE       0xFFFE0700       /* timer registers  */
#define EBI_ADR_BASE        0xFFFE7000       /* EBI control registers */

#define PHYS_ROM_BASE       0x1FC00000
#define PHYS_FLASH_BASE     0x1A000000       /* Flash Memory */
#define PHYS_FLASH1_BASE    PHYS_FLASH_BASE  /* Flash Memory */
#define PHYS_FLASH2_BASE    0x1C000000       /* Flash Memory */
#define PHYS_BCM44XX_BASE   0x1B400000       /* HPNA */


#define EBI_SIZE_8K        0
#define EBI_SIZE_2M        8
#define EBI_SIZE_4M        9
#define EBI_SIZE_16M      11

#define CS0BASE         0x00
#define CS0CNTL         0x04
#define CS1BASE         0x08
#define CS1CNTL         0x0c
#define CS2BASE         0x10
#define CS2CNTL         0x14
#define CS8BASE         0x40
#define CS8CNTL         0x44

#define EBI_ENABLE          0x00000001     /* .. enable this range */
#define EBI_WORD_WIDE       0x00000010     /* .. 16-bit peripheral, else 8 */
#define EBI_TS_TA_MODE      0x00000080     /* .. use TS/TA mode */
#define SEVENWT             0x07000000     /* ..  7 WS */
#define EBI_REV_END         0x00000400     /* .. Reverse Endian */

#define EBI_MASTER_ENABLE   0x80000000  /* allow external masters */
#define EBICONFIG           0x100


#define TIMER_0_CTL		0x04
#define TIMER_1_CTL		0x08
#define TIMER_2_CTL		0x0c
#define TIMER_3_CTL		0x10

#define TIMER_MASK      2
#define TIMER_INTS      3
