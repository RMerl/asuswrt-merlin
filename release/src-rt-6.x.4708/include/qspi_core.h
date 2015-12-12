/*
 * qspi - Broadcom QSPI specific definitions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: $
 */

#ifndef	_qspi_core_h_
#define	_qspi_core_h_

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct {
	/* BSPI */
	uint32 bspi_revision_id;		/* 0x0 */
	uint32 bspi_scratch;			/* 0x4 */
	uint32 bspi_mast_n_boot_ctrl;		/* 0x8 */
	uint32 bspi_busy_status;		/* 0xc */
	uint32 bspi_intr_status;		/* 0x10 */
	uint32 bspi_b0_status;			/* 0x14 */
	uint32 bspi_b0_ctrl;			/* 0x18 */
	uint32 bspi_b1_status;			/* 0x1c */
	uint32 bspi_b1_ctrl;			/* 0x20 */
	uint32 bspi_strap_override_ctrl;	/* 0x24 */
	uint32 bspi_flex_mode_enable;		/* 0x28 */
	uint32 bspi_bits_per_cycle;		/* 0x2c */
	uint32 bspi_bits_per_phase;		/* 0x30 */
	uint32 bspi_cmd_and_mode_byte;		/* 0x34 */
	uint32 bspi_bspi_flash_upper_addr_byte;	/* 0x38 */
	uint32 bspi_bspi_xor_value;		/* 0x3c */
	uint32 bspi_bspi_xor_enable;		/* 0x40 */
	uint32 bspi_bspi_pio_mode_enable;	/* 0x44 */
	uint32 bspi_bspi_pio_iodir;		/* 0x48 */
	uint32 bspi_bspi_pio_data;		/* 0x4c */
	uint32 PAD[44];				/* 0x50 ~ 0xfc */

	/* RAF */
	uint32 raf_start_addr;			/* 0x100 */
	uint32 raf_num_words;			/* 0x104 */
	uint32 raf_ctrl;			/* 0x108 */
	uint32 raf_fullness;			/* 0x10c */
	uint32 raf_watermark;			/* 0x110 */
	uint32 raf_status;			/* 0x114 */
	uint32 raf_read_data;			/* 0x118 */
	uint32 raf_word_cnt;			/* 0x11c */
	uint32 raf_curr_addr;			/* 0x120 */
	uint32 PAD[55];				/* 0x124 ~ 0x1fc */

	/* MSPI */
	uint32 mspi_spcr0_lsb;			/* 0x200 */
	uint32 mspi_spcr0_msb;			/* 0x204 */
	uint32 mspi_spcr1_lsb;			/* 0x208 */
	uint32 mspi_spcr1_msb;			/* 0x20c */
	uint32 mspi_newqp;			/* 0x210 */
	uint32 mspi_endqp;			/* 0x214 */
	uint32 mspi_spcr2;			/* 0x218 */
	uint32 PAD;				/* 0x21c */
	uint32 mspi_mspi_status;		/* 0x220 */
	uint32 mspi_cptqp;			/* 0x224 */
	uint32 PAD[6];				/* 0x228 ~ 0x23c */
	uint32 mspi_txram[32];			/* 0x240 ~ 0x2b8 */
	uint32 mspi_rxram[32];			/* 0x2c0 ~ 0x33c */
	uint32 mspi_cdram[16];			/* 0x340 ~ 0x37c */
	uint32 mspi_write_lock;			/* 0x380 */
	uint32 mspi_disable_flush_gen;		/* 0x384 */
	uint32 PAD[6];				/* 0x388 ~ 0x39C */

	/* Interrupt */
	uint32 intr_raf_lr_fullness_reached;	/* 0x3a0 */
	uint32 intr_raf_lr_truncated;		/* 0x3a4 */
	uint32 intr_raf_lr_impatient;		/* 0x3a8 */
	uint32 intr_raf_lr_session_done;	/* 0x3ac */
	uint32 intr_raf_lr_overread;		/* 0x3b0 */
	uint32 intr_mspi_done;			/* 0x3b4 */
	uint32 intr_mspi_halt_set_transaction_done;	/* 0x3b8 */
} qspiregs_t;

/*
 * SPCR0_LSB - SPCR0_LSB REGISTER
 */
/* HIF_MSPI :: SPCR0_LSB :: reserved0 [31:08] */
#define MSPI_SPCR0_LSB_reserved0_MASK                     0xffffff00
#define MSPI_SPCR0_LSB_reserved0_SHIFT                    8

/* HIF_MSPI :: SPCR0_LSB :: SPBR [07:00] */
#define MSPI_SPCR0_LSB_SPBR_MASK                          0x000000ff
#define MSPI_SPCR0_LSB_SPBR_SHIFT                         0
#define MSPI_SPCR0_LSB_SPBR_DEFAULT                       0

/*
 * SPCR0_MSB - SPCR0_MSB Register
 */
/* HIF_MSPI :: SPCR0_MSB :: reserved0 [31:08] */
#define MSPI_SPCR0_MSB_reserved0_MASK                     0xffffff00
#define MSPI_SPCR0_MSB_reserved0_SHIFT                    8

/* HIF_MSPI :: SPCR0_MSB :: MSTR [07:07] */
#define MSPI_SPCR0_MSB_MSTR_MASK                          0x00000080
#define MSPI_SPCR0_MSB_MSTR_SHIFT                         7
#define MSPI_SPCR0_MSB_MSTR_DEFAULT                       1

/* HIF_MSPI :: SPCR0_MSB :: StartTransDelay [06:06] */
#define MSPI_SPCR0_MSB_StartTransDelay_MASK               0x00000040
#define MSPI_SPCR0_MSB_StartTransDelay_SHIFT              6
#define MSPI_SPCR0_MSB_StartTransDelay_DEFAULT            0

/* HIF_MSPI :: SPCR0_MSB :: BitS [05:02] */
#define MSPI_SPCR0_MSB_BitS_MASK                          0x0000003c
#define MSPI_SPCR0_MSB_BitS_SHIFT                         2
#define MSPI_SPCR0_MSB_BitS_DEFAULT                       0

/* HIF_MSPI :: SPCR0_MSB :: CPOL [01:01] */
#define MSPI_SPCR0_MSB_CPOL_MASK                          0x00000002
#define MSPI_SPCR0_MSB_CPOL_SHIFT                         1
#define MSPI_SPCR0_MSB_CPOL_DEFAULT                       0

/* HIF_MSPI :: SPCR0_MSB :: CPHA [00:00] */
#define MSPI_SPCR0_MSB_CPHA_MASK                          0x00000001
#define MSPI_SPCR0_MSB_CPHA_SHIFT                         0
#define MSPI_SPCR0_MSB_CPHA_DEFAULT                       0

/*
 * SPCR1_LSB - SPCR1_LSB REGISTER
 */
/* HIF_MSPI :: SPCR1_LSB :: reserved0 [31:08] */
#define MSPI_SPCR1_LSB_reserved0_MASK                     0xffffff00
#define MSPI_SPCR1_LSB_reserved0_SHIFT                    8

/* HIF_MSPI :: SPCR1_LSB :: DTL [07:00] */
#define MSPI_SPCR1_LSB_DTL_MASK                           0x000000ff
#define MSPI_SPCR1_LSB_DTL_SHIFT                          0
#define MSPI_SPCR1_LSB_DTL_DEFAULT                        0

/*
 * SPCR1_MSB - SPCR1_MSB REGISTER
 */
/* HIF_MSPI :: SPCR1_MSB :: reserved0 [31:08] */
#define MSPI_SPCR1_MSB_reserved0_MASK                     0xffffff00
#define MSPI_SPCR1_MSB_reserved0_SHIFT                    8

/* HIF_MSPI :: SPCR1_MSB :: RDSCLK [07:00] */
#define MSPI_SPCR1_MSB_RDSCLK_MASK                        0x000000ff
#define MSPI_SPCR1_MSB_RDSCLK_SHIFT                       0
#define MSPI_SPCR1_MSB_RDSCLK_DEFAULT                     0

/*
 * NEWQP - NEWQP REGISTER
 */
/* HIF_MSPI :: NEWQP :: reserved0 [31:04] */
#define MSPI_NEWQP_reserved0_MASK                         0xfffffff0
#define MSPI_NEWQP_reserved0_SHIFT                        4

/* HIF_MSPI :: NEWQP :: newqp [03:00] */
#define MSPI_NEWQP_newqp_MASK                             0x0000000f
#define MSPI_NEWQP_newqp_SHIFT                            0
#define MSPI_NEWQP_newqp_DEFAULT                          0

/*
 * ENDQP - ENDQP REGISTER
 */
/* HIF_MSPI :: ENDQP :: reserved0 [31:04] */
#define MSPI_ENDQP_reserved0_MASK                         0xfffffff0
#define MSPI_ENDQP_reserved0_SHIFT                        4

/* HIF_MSPI :: ENDQP :: endqp [03:00] */
#define MSPI_ENDQP_endqp_MASK                             0x0000000f
#define MSPI_ENDQP_endqp_SHIFT                            0
#define MSPI_ENDQP_endqp_DEFAULT                          0

/*
 * SPCR2 - SPCR2 REGISTER
 */
/* HIF_MSPI :: SPCR2 :: reserved0 [31:08] */
#define MSPI_SPCR2_reserved0_MASK                         0xffffff00
#define MSPI_SPCR2_reserved0_SHIFT                        8

/* HIF_MSPI :: SPCR2 :: cont_after_cmd [07:07] */
#define MSPI_SPCR2_cont_after_cmd_MASK                    0x00000080
#define MSPI_SPCR2_cont_after_cmd_SHIFT                   7
#define MSPI_SPCR2_cont_after_cmd_DEFAULT                 0

/* HIF_MSPI :: SPCR2 :: spe [06:06] */
#define MSPI_SPCR2_spe_MASK                               0x00000040
#define MSPI_SPCR2_spe_SHIFT                              6
#define MSPI_SPCR2_spe_DEFAULT                            0

/* HIF_MSPI :: SPCR2 :: spifie [05:05] */
#define MSPI_SPCR2_spifie_MASK                            0x00000020
#define MSPI_SPCR2_spifie_SHIFT                           5
#define MSPI_SPCR2_spifie_DEFAULT                         0

/* HIF_MSPI :: SPCR2 :: wren [04:04] */
#define MSPI_SPCR2_wren_MASK                              0x00000010
#define MSPI_SPCR2_wren_SHIFT                             4
#define MSPI_SPCR2_wren_DEFAULT                           0

/* HIF_MSPI :: SPCR2 :: wrt0 [03:03] */
#define MSPI_SPCR2_wrt0_MASK                              0x00000008
#define MSPI_SPCR2_wrt0_SHIFT                             3
#define MSPI_SPCR2_wrt0_DEFAULT                           0

/* HIF_MSPI :: SPCR2 :: loopq [02:02] */
#define MSPI_SPCR2_loopq_MASK                             0x00000004
#define MSPI_SPCR2_loopq_SHIFT                            2
#define MSPI_SPCR2_loopq_DEFAULT                          0

/* HIF_MSPI :: SPCR2 :: hie [01:01] */
#define MSPI_SPCR2_hie_MASK                               0x00000002
#define MSPI_SPCR2_hie_SHIFT                              1
#define MSPI_SPCR2_hie_DEFAULT                            0

/* HIF_MSPI :: SPCR2 :: halt [00:00] */
#define MSPI_SPCR2_halt_MASK                              0x00000001
#define MSPI_SPCR2_halt_SHIFT                             0
#define MSPI_SPCR2_halt_DEFAULT                           0

/*
 * MSPI_STATUS - MSPI STATUS REGISTER
 */
/* HIF_MSPI :: MSPI_STATUS :: reserved0 [31:02] */
#define MSPI_MSPI_STATUS_reserved0_MASK                   0xfffffffc
#define MSPI_MSPI_STATUS_reserved0_SHIFT                  2

/* HIF_MSPI :: MSPI_STATUS :: HALTA [01:01] */
#define MSPI_MSPI_STATUS_HALTA_MASK                       0x00000002
#define MSPI_MSPI_STATUS_HALTA_SHIFT                      1
#define MSPI_MSPI_STATUS_HALTA_DEFAULT                    0

/* HIF_MSPI :: MSPI_STATUS :: SPIF [00:00] */
#define MSPI_MSPI_STATUS_SPIF_MASK                        0x00000001
#define MSPI_MSPI_STATUS_SPIF_SHIFT                       0
#define MSPI_MSPI_STATUS_SPIF_DEFAULT                     0

/*
 * CPTQP - CPTQP REGISTER
 */
/* HIF_MSPI :: CPTQP :: reserved0 [31:04] */
#define MSPI_CPTQP_reserved0_MASK                         0xfffffff0
#define MSPI_CPTQP_reserved0_SHIFT                        4

/* HIF_MSPI :: CPTQP :: cptqp [03:00] */
#define MSPI_CPTQP_cptqp_MASK                             0x0000000f
#define MSPI_CPTQP_cptqp_SHIFT                            0
#define MSPI_CPTQP_cptqp_DEFAULT                          0

/*
 * TXRAM00 - MSbyte for bit 16 or bit 8 operation (queue pointer = 0)
 */
/* HIF_MSPI :: TXRAM00 :: reserved0 [31:08] */
#define MSPI_TXRAM00_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM00_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM00 :: txram [07:00] */
#define MSPI_TXRAM00_txram_MASK                           0x000000ff
#define MSPI_TXRAM00_txram_SHIFT                          0

/*
 * TXRAM01 - LSbyte for bit 16 operation only (queue pointer = 0)
 */
/* HIF_MSPI :: TXRAM01 :: reserved0 [31:08] */
#define MSPI_TXRAM01_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM01_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM01 :: txram [07:00] */
#define MSPI_TXRAM01_txram_MASK                           0x000000ff
#define MSPI_TXRAM01_txram_SHIFT                          0

/*
 * TXRAM02 - MSbyte for bit 16 or bit 8 operation (queue pointer = 1)
 */
/* HIF_MSPI :: TXRAM02 :: reserved0 [31:08] */
#define MSPI_TXRAM02_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM02_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM02 :: txram [07:00] */
#define MSPI_TXRAM02_txram_MASK                           0x000000ff
#define MSPI_TXRAM02_txram_SHIFT                          0

/*
 * TXRAM03 - LSbyte for bit 16 operation only (queue pointer = 1)
 */
/* HIF_MSPI :: TXRAM03 :: reserved0 [31:08] */
#define MSPI_TXRAM03_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM03_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM03 :: txram [07:00] */
#define MSPI_TXRAM03_txram_MASK                           0x000000ff
#define MSPI_TXRAM03_txram_SHIFT                          0

/*
 * TXRAM04 - MSbyte for bit 16 or bit 8 operation (queue pointer = 2)
 */
/* HIF_MSPI :: TXRAM04 :: reserved0 [31:08] */
#define MSPI_TXRAM04_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM04_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM04 :: txram [07:00] */
#define MSPI_TXRAM04_txram_MASK                           0x000000ff
#define MSPI_TXRAM04_txram_SHIFT                          0

/*
 * TXRAM05 - LSbyte for bit 16 operation only (queue pointer = 2)
 */
/* HIF_MSPI :: TXRAM05 :: reserved0 [31:08] */
#define MSPI_TXRAM05_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM05_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM05 :: txram [07:00] */
#define MSPI_TXRAM05_txram_MASK                           0x000000ff
#define MSPI_TXRAM05_txram_SHIFT                          0

/*
 * TXRAM06 - MSbyte for bit 16 or bit 8 operation (queue pointer = 3)
 */
/* HIF_MSPI :: TXRAM06 :: reserved0 [31:08] */
#define MSPI_TXRAM06_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM06_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM06 :: txram [07:00] */
#define MSPI_TXRAM06_txram_MASK                           0x000000ff
#define MSPI_TXRAM06_txram_SHIFT                          0

/*
 * TXRAM07 - LSbyte for bit 16 operation only (queue pointer = 3)
 */
/* HIF_MSPI :: TXRAM07 :: reserved0 [31:08] */
#define MSPI_TXRAM07_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM07_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM07 :: txram [07:00] */
#define MSPI_TXRAM07_txram_MASK                           0x000000ff
#define MSPI_TXRAM07_txram_SHIFT                          0

/*
 * TXRAM08 - MSbyte for bit 16 or bit 8 operation (queue pointer = 4)
 */
/* HIF_MSPI :: TXRAM08 :: reserved0 [31:08] */
#define MSPI_TXRAM08_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM08_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM08 :: txram [07:00] */
#define MSPI_TXRAM08_txram_MASK                           0x000000ff
#define MSPI_TXRAM08_txram_SHIFT                          0

/*
 * TXRAM09 - LSbyte for bit 16 operation only (queue pointer = 4)
 */
/* HIF_MSPI :: TXRAM09 :: reserved0 [31:08] */
#define MSPI_TXRAM09_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM09_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM09 :: txram [07:00] */
#define MSPI_TXRAM09_txram_MASK                           0x000000ff
#define MSPI_TXRAM09_txram_SHIFT                          0

/*
 * TXRAM10 - MSbyte for bit 16 or bit 8 operation (queue pointer = 5)
 */
/* HIF_MSPI :: TXRAM10 :: reserved0 [31:08] */
#define MSPI_TXRAM10_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM10_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM10 :: txram [07:00] */
#define MSPI_TXRAM10_txram_MASK                           0x000000ff
#define MSPI_TXRAM10_txram_SHIFT                          0

/*
 * TXRAM11 - LSbyte for bit 16 operation only (queue pointer = 5)
 */
/* HIF_MSPI :: TXRAM11 :: reserved0 [31:08] */
#define MSPI_TXRAM11_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM11_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM11 :: txram [07:00] */
#define MSPI_TXRAM11_txram_MASK                           0x000000ff
#define MSPI_TXRAM11_txram_SHIFT                          0

/*
 * TXRAM12 - MSbyte for bit 16 or bit 8 operation (queue pointer = 6)
 */
/* HIF_MSPI :: TXRAM12 :: reserved0 [31:08] */
#define MSPI_TXRAM12_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM12_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM12 :: txram [07:00] */
#define MSPI_TXRAM12_txram_MASK                           0x000000ff
#define MSPI_TXRAM12_txram_SHIFT                          0

/*
 * TXRAM13 - LSbyte for bit 16 operation only (queue pointer = 6)
 */
/* HIF_MSPI :: TXRAM13 :: reserved0 [31:08] */
#define MSPI_TXRAM13_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM13_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM13 :: txram [07:00] */
#define MSPI_TXRAM13_txram_MASK                           0x000000ff
#define MSPI_TXRAM13_txram_SHIFT                          0

/*
 * TXRAM14 - MSbyte for bit 16 or bit 8 operation (queue pointer = 7)
 */
/* HIF_MSPI :: TXRAM14 :: reserved0 [31:08] */
#define MSPI_TXRAM14_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM14_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM14 :: txram [07:00] */
#define MSPI_TXRAM14_txram_MASK                           0x000000ff
#define MSPI_TXRAM14_txram_SHIFT                          0

/*
 * TXRAM15 - LSbyte for bit 16 operation only (queue pointer = 7)
 */
/* HIF_MSPI :: TXRAM15 :: reserved0 [31:08] */
#define MSPI_TXRAM15_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM15_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM15 :: txram [07:00] */
#define MSPI_TXRAM15_txram_MASK                           0x000000ff
#define MSPI_TXRAM15_txram_SHIFT                          0

/*
 * TXRAM16 - MSbyte for bit 16 or bit 8 operation (queue pointer = 8)
 */
/* HIF_MSPI :: TXRAM16 :: reserved0 [31:08] */
#define MSPI_TXRAM16_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM16_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM16 :: txram [07:00] */
#define MSPI_TXRAM16_txram_MASK                           0x000000ff
#define MSPI_TXRAM16_txram_SHIFT                          0

/*
 * TXRAM17 - LSbyte for bit 16 operation only (queue pointer = 8)
 */
/* HIF_MSPI :: TXRAM17 :: reserved0 [31:08] */
#define MSPI_TXRAM17_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM17_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM17 :: txram [07:00] */
#define MSPI_TXRAM17_txram_MASK                           0x000000ff
#define MSPI_TXRAM17_txram_SHIFT                          0

/*
 * TXRAM18 - MSbyte for bit 16 or bit 8 operation (queue pointer = 9)
 */
/* HIF_MSPI :: TXRAM18 :: reserved0 [31:08] */
#define MSPI_TXRAM18_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM18_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM18 :: txram [07:00] */
#define MSPI_TXRAM18_txram_MASK                           0x000000ff
#define MSPI_TXRAM18_txram_SHIFT                          0

/*
 * TXRAM19 - LSbyte for bit 16 operation only (queue pointer = 9)
 */
/* HIF_MSPI :: TXRAM19 :: reserved0 [31:08] */
#define MSPI_TXRAM19_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM19_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM19 :: txram [07:00] */
#define MSPI_TXRAM19_txram_MASK                           0x000000ff
#define MSPI_TXRAM19_txram_SHIFT                          0

/*
 * TXRAM20 - MSbyte for bit 16 or bit 8 operation (queue pointer = a)
 */
/* HIF_MSPI :: TXRAM20 :: reserved0 [31:08] */
#define MSPI_TXRAM20_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM20_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM20 :: txram [07:00] */
#define MSPI_TXRAM20_txram_MASK                           0x000000ff
#define MSPI_TXRAM20_txram_SHIFT                          0

/*
 * TXRAM21 - LSbyte for bit 16 operation only (queue pointer = a)
 */
/* HIF_MSPI :: TXRAM21 :: reserved0 [31:08] */
#define MSPI_TXRAM21_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM21_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM21 :: txram [07:00] */
#define MSPI_TXRAM21_txram_MASK                           0x000000ff
#define MSPI_TXRAM21_txram_SHIFT                          0

/*
 * TXRAM22 - MSbyte for bit 16 or bit 8 operation (queue pointer = b)
 */
/* HIF_MSPI :: TXRAM22 :: reserved0 [31:08] */
#define MSPI_TXRAM22_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM22_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM22 :: txram [07:00] */
#define MSPI_TXRAM22_txram_MASK                           0x000000ff
#define MSPI_TXRAM22_txram_SHIFT                          0

/*
 * TXRAM23 - LSbyte for bit 16 operation only (queue pointer = b)
 */
/* HIF_MSPI :: TXRAM23 :: reserved0 [31:08] */
#define MSPI_TXRAM23_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM23_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM23 :: txram [07:00] */
#define MSPI_TXRAM23_txram_MASK                           0x000000ff
#define MSPI_TXRAM23_txram_SHIFT                          0

/*
 * TXRAM24 - MSbyte for bit 16 or bit 8 operation (queue pointer = c)
 */
/* HIF_MSPI :: TXRAM24 :: reserved0 [31:08] */
#define MSPI_TXRAM24_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM24_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM24 :: txram [07:00] */
#define MSPI_TXRAM24_txram_MASK                           0x000000ff
#define MSPI_TXRAM24_txram_SHIFT                          0

/*
 * TXRAM25 - LSbyte for bit 16 operation only (queue pointer = c)
 */
/* HIF_MSPI :: TXRAM25 :: reserved0 [31:08] */
#define MSPI_TXRAM25_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM25_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM25 :: txram [07:00] */
#define MSPI_TXRAM25_txram_MASK                           0x000000ff
#define MSPI_TXRAM25_txram_SHIFT                          0

/*
 * TXRAM26 - MSbyte for bit 16 or bit 8 operation (queue pointer = d)
 */
/* HIF_MSPI :: TXRAM26 :: reserved0 [31:08] */
#define MSPI_TXRAM26_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM26_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM26 :: txram [07:00] */
#define MSPI_TXRAM26_txram_MASK                           0x000000ff
#define MSPI_TXRAM26_txram_SHIFT                          0

/*
 * TXRAM27 - LSbyte for bit 16 operation only (queue pointer = d)
 */
/* HIF_MSPI :: TXRAM27 :: reserved0 [31:08] */
#define MSPI_TXRAM27_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM27_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM27 :: txram [07:00] */
#define MSPI_TXRAM27_txram_MASK                           0x000000ff
#define MSPI_TXRAM27_txram_SHIFT                          0

/*
 * TXRAM28 - MSbyte for bit 16 or bit 8 operation (queue pointer = e)
 */
/* HIF_MSPI :: TXRAM28 :: reserved0 [31:08] */
#define MSPI_TXRAM28_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM28_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM28 :: txram [07:00] */
#define MSPI_TXRAM28_txram_MASK                           0x000000ff
#define MSPI_TXRAM28_txram_SHIFT                          0

/*
 * TXRAM29 - LSbyte for bit 16 operation only (queue pointer = e)
 */
/* HIF_MSPI :: TXRAM29 :: reserved0 [31:08] */
#define MSPI_TXRAM29_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM29_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM29 :: txram [07:00] */
#define MSPI_TXRAM29_txram_MASK                           0x000000ff
#define MSPI_TXRAM29_txram_SHIFT                          0

/*
 * TXRAM30 - MSbyte for bit 16 or bit 8 operation (queue pointer = f)
 */
/* HIF_MSPI :: TXRAM30 :: reserved0 [31:08] */
#define MSPI_TXRAM30_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM30_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM30 :: txram [07:00] */
#define MSPI_TXRAM30_txram_MASK                           0x000000ff
#define MSPI_TXRAM30_txram_SHIFT                          0

/*
 * TXRAM31 - LSbyte for bit 16 operation only (queue pointer = f)
 */
/* HIF_MSPI :: TXRAM31 :: reserved0 [31:08] */
#define MSPI_TXRAM31_reserved0_MASK                       0xffffff00
#define MSPI_TXRAM31_reserved0_SHIFT                      8

/* HIF_MSPI :: TXRAM31 :: txram [07:00] */
#define MSPI_TXRAM31_txram_MASK                           0x000000ff
#define MSPI_TXRAM31_txram_SHIFT                          0

/*
 * RXRAM00 - MSbyte for bit 16 or bit 8 operation (queue pointer = 0)
 */
/* HIF_MSPI :: RXRAM00 :: reserved0 [31:08] */
#define MSPI_RXRAM00_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM00_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM00 :: rxram [07:00] */
#define MSPI_RXRAM00_rxram_MASK                           0x000000ff
#define MSPI_RXRAM00_rxram_SHIFT                          0

/*
 * RXRAM01 - LSbyte for bit 16 operation only (queue pointer = 0)
 */
/* HIF_MSPI :: RXRAM01 :: reserved0 [31:08] */
#define MSPI_RXRAM01_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM01_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM01 :: rxram [07:00] */
#define MSPI_RXRAM01_rxram_MASK                           0x000000ff
#define MSPI_RXRAM01_rxram_SHIFT                          0

/*
 * RXRAM02 - MSbyte for bit 16 or bit 8 operation (queue pointer = 1)
 */
/* HIF_MSPI :: RXRAM02 :: reserved0 [31:08] */
#define MSPI_RXRAM02_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM02_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM02 :: rxram [07:00] */
#define MSPI_RXRAM02_rxram_MASK                           0x000000ff
#define MSPI_RXRAM02_rxram_SHIFT                          0

/*
 * RXRAM03 - LSbyte for bit 16 operation only (queue pointer = 1)
 */
/* HIF_MSPI :: RXRAM03 :: reserved0 [31:08] */
#define MSPI_RXRAM03_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM03_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM03 :: rxram [07:00] */
#define MSPI_RXRAM03_rxram_MASK                           0x000000ff
#define MSPI_RXRAM03_rxram_SHIFT                          0

/*
 * RXRAM04 - MSbyte for bit 16 or bit 8 operation (queue pointer = 2)
 */
/* HIF_MSPI :: RXRAM04 :: reserved0 [31:08] */
#define MSPI_RXRAM04_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM04_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM04 :: rxram [07:00] */
#define MSPI_RXRAM04_rxram_MASK                           0x000000ff
#define MSPI_RXRAM04_rxram_SHIFT                          0

/*
 * RXRAM05 - LSbyte for bit 16 operation only (queue pointer = 2)
 */
/* HIF_MSPI :: RXRAM05 :: reserved0 [31:08] */
#define MSPI_RXRAM05_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM05_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM05 :: rxram [07:00] */
#define MSPI_RXRAM05_rxram_MASK                           0x000000ff
#define MSPI_RXRAM05_rxram_SHIFT                          0

/*
 * RXRAM06 - MSbyte for bit 16 or bit 8 operation (queue pointer = 3)
 */
/* HIF_MSPI :: RXRAM06 :: reserved0 [31:08] */
#define MSPI_RXRAM06_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM06_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM06 :: rxram [07:00] */
#define MSPI_RXRAM06_rxram_MASK                           0x000000ff
#define MSPI_RXRAM06_rxram_SHIFT                          0

/*
 * RXRAM07 - LSbyte for bit 16 operation only (queue pointer = 3)
 */
/* HIF_MSPI :: RXRAM07 :: reserved0 [31:08] */
#define MSPI_RXRAM07_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM07_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM07 :: rxram [07:00] */
#define MSPI_RXRAM07_rxram_MASK                           0x000000ff
#define MSPI_RXRAM07_rxram_SHIFT                          0

/*
 * RXRAM08 - MSbyte for bit 16 or bit 8 operation (queue pointer = 4)
 */
/* HIF_MSPI :: RXRAM08 :: reserved0 [31:08] */
#define MSPI_RXRAM08_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM08_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM08 :: rxram [07:00] */
#define MSPI_RXRAM08_rxram_MASK                           0x000000ff
#define MSPI_RXRAM08_rxram_SHIFT                          0

/*
 * RXRAM09 - LSbyte for bit 16 operation only (queue pointer = 4)
 */
/* HIF_MSPI :: RXRAM09 :: reserved0 [31:08] */
#define MSPI_RXRAM09_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM09_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM09 :: rxram [07:00] */
#define MSPI_RXRAM09_rxram_MASK                           0x000000ff
#define MSPI_RXRAM09_rxram_SHIFT                          0

/*
 * RXRAM10 - MSbyte for bit 16 or bit 8 operation (queue pointer = 5)
 */
/* HIF_MSPI :: RXRAM10 :: reserved0 [31:08] */
#define MSPI_RXRAM10_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM10_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM10 :: rxram [07:00] */
#define MSPI_RXRAM10_rxram_MASK                           0x000000ff
#define MSPI_RXRAM10_rxram_SHIFT                          0

/*
 * RXRAM11 - LSbyte for bit 16 operation only (queue pointer = 5)
 */
/* HIF_MSPI :: RXRAM11 :: reserved0 [31:08] */
#define MSPI_RXRAM11_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM11_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM11 :: rxram [07:00] */
#define MSPI_RXRAM11_rxram_MASK                           0x000000ff
#define MSPI_RXRAM11_rxram_SHIFT                          0

/*
 * RXRAM12 - MSbyte for bit 16 or bit 8 operation (queue pointer = 6)
 */
/* HIF_MSPI :: RXRAM12 :: reserved0 [31:08] */
#define MSPI_RXRAM12_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM12_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM12 :: rxram [07:00] */
#define MSPI_RXRAM12_rxram_MASK                           0x000000ff
#define MSPI_RXRAM12_rxram_SHIFT                          0

/*
 * RXRAM13 - LSbyte for bit 16 operation only (queue pointer = 6)
 */
/* HIF_MSPI :: RXRAM13 :: reserved0 [31:08] */
#define MSPI_RXRAM13_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM13_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM13 :: rxram [07:00] */
#define MSPI_RXRAM13_rxram_MASK                           0x000000ff
#define MSPI_RXRAM13_rxram_SHIFT                          0

/*
 * RXRAM14 - MSbyte for bit 16 or bit 8 operation (queue pointer = 7)
 */
/* HIF_MSPI :: RXRAM14 :: reserved0 [31:08] */
#define MSPI_RXRAM14_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM14_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM14 :: rxram [07:00] */
#define MSPI_RXRAM14_rxram_MASK                           0x000000ff
#define MSPI_RXRAM14_rxram_SHIFT                          0

/*
 * RXRAM15 - LSbyte for bit 16 operation only (queue pointer = 7)
 */
/* HIF_MSPI :: RXRAM15 :: reserved0 [31:08] */
#define MSPI_RXRAM15_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM15_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM15 :: rxram [07:00] */
#define MSPI_RXRAM15_rxram_MASK                           0x000000ff
#define MSPI_RXRAM15_rxram_SHIFT                          0

/*
 * RXRAM16 - MSbyte for bit 16 or bit 8 operation (queue pointer = 8)
 */
/* HIF_MSPI :: RXRAM16 :: reserved0 [31:08] */
#define MSPI_RXRAM16_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM16_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM16 :: rxram [07:00] */
#define MSPI_RXRAM16_rxram_MASK                           0x000000ff
#define MSPI_RXRAM16_rxram_SHIFT                          0

/*
 * RXRAM17 - LSbyte for bit 16 operation only (queue pointer = 8)
 */
/* HIF_MSPI :: RXRAM17 :: reserved0 [31:08] */
#define MSPI_RXRAM17_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM17_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM17 :: rxram [07:00] */
#define MSPI_RXRAM17_rxram_MASK                           0x000000ff
#define MSPI_RXRAM17_rxram_SHIFT                          0

/*
 * RXRAM18 - MSbyte for bit 16 or bit 8 operation (queue pointer = 9)
 */
/* HIF_MSPI :: RXRAM18 :: reserved0 [31:08] */
#define MSPI_RXRAM18_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM18_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM18 :: rxram [07:00] */
#define MSPI_RXRAM18_rxram_MASK                           0x000000ff
#define MSPI_RXRAM18_rxram_SHIFT                          0

/*
 * RXRAM19 - LSbyte for bit 16 operation only (queue pointer = 9)
 */
/* HIF_MSPI :: RXRAM19 :: reserved0 [31:08] */
#define MSPI_RXRAM19_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM19_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM19 :: rxram [07:00] */
#define MSPI_RXRAM19_rxram_MASK                           0x000000ff
#define MSPI_RXRAM19_rxram_SHIFT                          0

/*
 * RXRAM20 - MSbyte for bit 16 or bit 8 operation (queue pointer = a)
 */
/* HIF_MSPI :: RXRAM20 :: reserved0 [31:08] */
#define MSPI_RXRAM20_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM20_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM20 :: rxram [07:00] */
#define MSPI_RXRAM20_rxram_MASK                           0x000000ff
#define MSPI_RXRAM20_rxram_SHIFT                          0

/*
 * RXRAM21 - LSbyte for bit 16 operation only (queue pointer = a)
 */
/* HIF_MSPI :: RXRAM21 :: reserved0 [31:08] */
#define MSPI_RXRAM21_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM21_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM21 :: rxram [07:00] */
#define MSPI_RXRAM21_rxram_MASK                           0x000000ff
#define MSPI_RXRAM21_rxram_SHIFT                          0

/*
 * RXRAM22 - MSbyte for bit 16 or bit 8 operation (queue pointer = b)
 */
/* HIF_MSPI :: RXRAM22 :: reserved0 [31:08] */
#define MSPI_RXRAM22_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM22_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM22 :: rxram [07:00] */
#define MSPI_RXRAM22_rxram_MASK                           0x000000ff
#define MSPI_RXRAM22_rxram_SHIFT                          0

/*
 * RXRAM23 - LSbyte for bit 16 operation only (queue pointer = b)
 */
/* HIF_MSPI :: RXRAM23 :: reserved0 [31:08] */
#define MSPI_RXRAM23_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM23_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM23 :: rxram [07:00] */
#define MSPI_RXRAM23_rxram_MASK                           0x000000ff
#define MSPI_RXRAM23_rxram_SHIFT                          0

/*
 * RXRAM24 - MSbyte for bit 16 or bit 8 operation (queue pointer = c)
 */
/* HIF_MSPI :: RXRAM24 :: reserved0 [31:08] */
#define MSPI_RXRAM24_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM24_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM24 :: rxram [07:00] */
#define MSPI_RXRAM24_rxram_MASK                           0x000000ff
#define MSPI_RXRAM24_rxram_SHIFT                          0

/*
 * RXRAM25 - LSbyte for bit 16 operation only (queue pointer = c)
 */
/* HIF_MSPI :: RXRAM25 :: reserved0 [31:08] */
#define MSPI_RXRAM25_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM25_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM25 :: rxram [07:00] */
#define MSPI_RXRAM25_rxram_MASK                           0x000000ff
#define MSPI_RXRAM25_rxram_SHIFT                          0

/*
 * RXRAM26 - MSbyte for bit 16 or bit 8 operation (queue pointer = d)
 */
/* HIF_MSPI :: RXRAM26 :: reserved0 [31:08] */
#define MSPI_RXRAM26_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM26_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM26 :: rxram [07:00] */
#define MSPI_RXRAM26_rxram_MASK                           0x000000ff
#define MSPI_RXRAM26_rxram_SHIFT                          0

/*
 * RXRAM27 - LSbyte for bit 16 operation only (queue pointer = d)
 */
/* HIF_MSPI :: RXRAM27 :: reserved0 [31:08] */
#define MSPI_RXRAM27_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM27_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM27 :: rxram [07:00] */
#define MSPI_RXRAM27_rxram_MASK                           0x000000ff
#define MSPI_RXRAM27_rxram_SHIFT                          0

/*
 * RXRAM28 - MSbyte for bit 16 or bit 8 operation (queue pointer = e)
 */
/* HIF_MSPI :: RXRAM28 :: reserved0 [31:08] */
#define MSPI_RXRAM28_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM28_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM28 :: rxram [07:00] */
#define MSPI_RXRAM28_rxram_MASK                           0x000000ff
#define MSPI_RXRAM28_rxram_SHIFT                          0

/*
 * RXRAM29 - LSbyte for bit 16 operation only (queue pointer = e)
 */
/* HIF_MSPI :: RXRAM29 :: reserved0 [31:08] */
#define MSPI_RXRAM29_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM29_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM29 :: rxram [07:00] */
#define MSPI_RXRAM29_rxram_MASK                           0x000000ff
#define MSPI_RXRAM29_rxram_SHIFT                          0

/*
 * RXRAM30 - MSbyte for bit 16 or bit 8 operation (queue pointer = f)
 */
/* HIF_MSPI :: RXRAM30 :: reserved0 [31:08] */
#define MSPI_RXRAM30_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM30_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM30 :: rxram [07:00] */
#define MSPI_RXRAM30_rxram_MASK                           0x000000ff
#define MSPI_RXRAM30_rxram_SHIFT                          0

/*
 * RXRAM31 - LSbyte for bit 16 operation only (queue pointer = f)
 */
/* HIF_MSPI :: RXRAM31 :: reserved0 [31:08] */
#define MSPI_RXRAM31_reserved0_MASK                       0xffffff00
#define MSPI_RXRAM31_reserved0_SHIFT                      8

/* HIF_MSPI :: RXRAM31 :: rxram [07:00] */
#define MSPI_RXRAM31_rxram_MASK                           0x000000ff
#define MSPI_RXRAM31_rxram_SHIFT                          0

/*
 * CDRAM00 - 8-bit command (queue pointer = 0)
 */
/* HIF_MSPI :: CDRAM00 :: reserved0 [31:08] */
#define MSPI_CDRAM00_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM00_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM00 :: cdram [07:00] */
#define MSPI_CDRAM00_cdram_MASK                           0x000000ff
#define MSPI_CDRAM00_cdram_SHIFT                          0

/*
 * CDRAM01 - 8-bit command (queue pointer = 1)
 */
/* HIF_MSPI :: CDRAM01 :: reserved0 [31:08] */
#define MSPI_CDRAM01_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM01_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM01 :: cdram [07:00] */
#define MSPI_CDRAM01_cdram_MASK                           0x000000ff
#define MSPI_CDRAM01_cdram_SHIFT                          0

/*
 * CDRAM02 - 8-bit command (queue pointer = 2)
 */
/* HIF_MSPI :: CDRAM02 :: reserved0 [31:08] */
#define MSPI_CDRAM02_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM02_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM02 :: cdram [07:00] */
#define MSPI_CDRAM02_cdram_MASK                           0x000000ff
#define MSPI_CDRAM02_cdram_SHIFT                          0

/*
 * CDRAM03 - 8-bit command (queue pointer = 3)
 */
/* HIF_MSPI :: CDRAM03 :: reserved0 [31:08] */
#define MSPI_CDRAM03_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM03_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM03 :: cdram [07:00] */
#define MSPI_CDRAM03_cdram_MASK                           0x000000ff
#define MSPI_CDRAM03_cdram_SHIFT                          0

/*
 * CDRAM04 - 8-bit command (queue pointer = 4)
 */
/* HIF_MSPI :: CDRAM04 :: reserved0 [31:08] */
#define MSPI_CDRAM04_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM04_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM04 :: cdram [07:00] */
#define MSPI_CDRAM04_cdram_MASK                           0x000000ff
#define MSPI_CDRAM04_cdram_SHIFT                          0

/*
 * CDRAM05 - 8-bit command (queue pointer = 5)
 */
/* HIF_MSPI :: CDRAM05 :: reserved0 [31:08] */
#define MSPI_CDRAM05_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM05_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM05 :: cdram [07:00] */
#define MSPI_CDRAM05_cdram_MASK                           0x000000ff
#define MSPI_CDRAM05_cdram_SHIFT                          0

/*
 * CDRAM06 - 8-bit command (queue pointer = 6)
 */
/* HIF_MSPI :: CDRAM06 :: reserved0 [31:08] */
#define MSPI_CDRAM06_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM06_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM06 :: cdram [07:00] */
#define MSPI_CDRAM06_cdram_MASK                           0x000000ff
#define MSPI_CDRAM06_cdram_SHIFT                          0

/*
 * CDRAM07 - 8-bit command (queue pointer = 7)
 */
/* HIF_MSPI :: CDRAM07 :: reserved0 [31:08] */
#define MSPI_CDRAM07_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM07_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM07 :: cdram [07:00] */
#define MSPI_CDRAM07_cdram_MASK                           0x000000ff
#define MSPI_CDRAM07_cdram_SHIFT                          0

/*
 * CDRAM08 - 8-bit command (queue pointer = 8)
 */
/* HIF_MSPI :: CDRAM08 :: reserved0 [31:08] */
#define MSPI_CDRAM08_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM08_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM08 :: cdram [07:00] */
#define MSPI_CDRAM08_cdram_MASK                           0x000000ff
#define MSPI_CDRAM08_cdram_SHIFT                          0

/*
 * CDRAM09 - 8-bit command (queue pointer = 9)
 */
/* HIF_MSPI :: CDRAM09 :: reserved0 [31:08] */
#define MSPI_CDRAM09_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM09_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM09 :: cdram [07:00] */
#define MSPI_CDRAM09_cdram_MASK                           0x000000ff
#define MSPI_CDRAM09_cdram_SHIFT                          0

/*
 * CDRAM10 - 8-bit command (queue pointer = a)
 */
/* HIF_MSPI :: CDRAM10 :: reserved0 [31:08] */
#define MSPI_CDRAM10_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM10_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM10 :: cdram [07:00] */
#define MSPI_CDRAM10_cdram_MASK                           0x000000ff
#define MSPI_CDRAM10_cdram_SHIFT                          0

/*
 * CDRAM11 - 8-bit command (queue pointer = b)
 */
/* HIF_MSPI :: CDRAM11 :: reserved0 [31:08] */
#define MSPI_CDRAM11_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM11_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM11 :: cdram [07:00] */
#define MSPI_CDRAM11_cdram_MASK                           0x000000ff
#define MSPI_CDRAM11_cdram_SHIFT                          0

/*
 * CDRAM12 - 8-bit command (queue pointer = c)
 */
/* HIF_MSPI :: CDRAM12 :: reserved0 [31:08] */
#define MSPI_CDRAM12_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM12_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM12 :: cdram [07:00] */
#define MSPI_CDRAM12_cdram_MASK                           0x000000ff
#define MSPI_CDRAM12_cdram_SHIFT                          0

/*
 * CDRAM13 - 8-bit command (queue pointer = d)
 */
/* HIF_MSPI :: CDRAM13 :: reserved0 [31:08] */
#define MSPI_CDRAM13_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM13_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM13 :: cdram [07:00] */
#define MSPI_CDRAM13_cdram_MASK                           0x000000ff
#define MSPI_CDRAM13_cdram_SHIFT                          0

/*
 * CDRAM14 - 8-bit command (queue pointer = e)
 */
/* HIF_MSPI :: CDRAM14 :: reserved0 [31:08] */
#define MSPI_CDRAM14_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM14_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM14 :: cdram [07:00] */
#define MSPI_CDRAM14_cdram_MASK                           0x000000ff
#define MSPI_CDRAM14_cdram_SHIFT                          0

/*
 * CDRAM15 - 8-bit command (queue pointer = f)
 */
/* HIF_MSPI :: CDRAM15 :: reserved0 [31:08] */
#define MSPI_CDRAM15_reserved0_MASK                       0xffffff00
#define MSPI_CDRAM15_reserved0_SHIFT                      8

/* HIF_MSPI :: CDRAM15 :: cdram [07:00] */
#define MSPI_CDRAM15_cdram_MASK                           0x000000ff
#define MSPI_CDRAM15_cdram_SHIFT                          0

/*
 * WRITE_LOCK - Control bit to lock group of write commands
 */
/* HIF_MSPI :: WRITE_LOCK :: reserved0 [31:01] */
#define MSPI_WRITE_LOCK_reserved0_MASK                    0xfffffffe
#define MSPI_WRITE_LOCK_reserved0_SHIFT                   1

/* HIF_MSPI :: WRITE_LOCK :: WriteLock [00:00] */
#define MSPI_WRITE_LOCK_WriteLock_MASK                    0x00000001
#define MSPI_WRITE_LOCK_WriteLock_SHIFT                   0
#define MSPI_WRITE_LOCK_WriteLock_DEFAULT                 0

/*
 * DISABLE_FLUSH_GEN - Debug bit to mask the generation of flush signals from Mspi
 */
/* HIF_MSPI :: DISABLE_FLUSH_GEN :: reserved0 [31:01] */
#define MSPI_DISABLE_FLUSH_GEN_reserved0_MASK             0xfffffffe
#define MSPI_DISABLE_FLUSH_GEN_reserved0_SHIFT            1

/* HIF_MSPI :: DISABLE_FLUSH_GEN :: DisableFlushGen [00:00] */
#define MSPI_DISABLE_FLUSH_GEN_DisableFlushGen_MASK       0x00000001
#define MSPI_DISABLE_FLUSH_GEN_DisableFlushGen_SHIFT      0
#define MSPI_DISABLE_FLUSH_GEN_DisableFlushGen_DEFAULT    0

/* BSPI register fields */
#define BSPI_BITS_PER_PHASE_ADDR_MARK	0x00010000

#define SPI_PP_CMD		(0x02)
#define SPI_READ_CMD		(0x03)
#define SPI_WRDI_CMD		(0x04)
#define SPI_RDSR_CMD		(0x05)
#define SPI_WREN_CMD		(0x06)
#define SPI_SSE_CMD		(0x20)
#define SPI_READ_ID_CMD		(0x90)
#define SPI_RDID_CMD		(0x9F)
#define SPI_SE_CMD		(0xD8)
#define SPI_EN4B_CMD		(0xB7)
#define SPI_EX4B_CMD		(0xE9)

#define SPI_AT_BUF1_LOAD	0x53
#define SPI_AT_PAGE_ERASE	0x81
#define SPI_AT_BUF1_WRITE	0x84
#define SPI_AT_BUF1_PROGRAM	0x88
#define SPI_AT_STATUS		0xD7
#define SPI_AT_READY		0x80


#define SPI_POLLING_INTERVAL		10		/* in usecs */
#define SPI_CDRAM_CONT			0x80

#define SPI_CDRAM_PCS_PCS0		0x01
#define SPI_CDRAM_PCS_PCS1		0x02
#define SPI_CDRAM_PCS_PCS2		0x04
#define SPI_CDRAM_PCS_PCS3		0x08
#define SPI_CDRAM_PCS_DSCK		0x10
#define SPI_CDRAM_PCS_DISABLE_ALL	(SPI_CDRAM_PCS_PCS0 | SPI_CDRAM_PCS_PCS1 | \
					 SPI_CDRAM_PCS_PCS2 | SPI_CDRAM_PCS_PCS3)
#define SPI_CDRAM_BITSE   		0x40

#define SPI_SYSTEM_CLK			216000000	/* 216 MHz */
#define MAX_SPI_BAUD			13500000	/* SPBR = 8 (minimum value), 216MHZ */

#define BSPI_Pcs_eUpgSpiPcs2		0
#define FLASH_SPI_BYTE_ORDER_FIX	1

#endif	/* _qspi_core_h_ */
