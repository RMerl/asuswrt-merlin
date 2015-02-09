/*
 * SD-SPI Protocol Conversion - BCMSDH->SPI Translation Layer
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmsdspi.h 294363 2011-11-06 23:02:20Z $
 */
#ifndef	_BCM_SD_SPI_H
#define	_BCM_SD_SPI_H

/* global msglevel for debug messages - bitvals come from sdiovar.h */

#ifdef BCMDBG
#define sd_err(x)	do { if (sd_msglevel & SDH_ERROR_VAL) printf x; } while (0)
#define sd_trace(x)	do { if (sd_msglevel & SDH_TRACE_VAL) printf x; } while (0)
#define sd_info(x)	do { if (sd_msglevel & SDH_INFO_VAL)  printf x; } while (0)
#define sd_debug(x)	do { if (sd_msglevel & SDH_DEBUG_VAL) printf x; } while (0)
#define sd_data(x)	do { if (sd_msglevel & SDH_DATA_VAL)  printf x; } while (0)
#define sd_ctrl(x)	do { if (sd_msglevel & SDH_CTRL_VAL)  printf x; } while (0)
#else
#define sd_err(x)
#define sd_trace(x)
#define sd_info(x)
#define sd_debug(x)
#define sd_data(x)
#define sd_ctrl(x)
#endif

#ifdef BCMPERFSTATS
#define sd_log(x)	do { if (sd_msglevel & SDH_LOG_VAL)	 bcmlog x; } while (0)
#else
#define sd_log(x)
#endif

#define SDIOH_ASSERT(exp) \
	do { if (!(exp)) \
		printf("!!!ASSERT fail: file %s lines %d", __FILE__, __LINE__); \
	} while (0)

#define BLOCK_SIZE_4318 64
#define BLOCK_SIZE_4328 512

/* internal return code */
#define SUCCESS	0
#undef ERROR
#define ERROR	1

/* private bus modes */
#define SDIOH_MODE_SPI		0

#define USE_BLOCKMODE		0x2	/* Block mode can be single block or multi */
#define USE_MULTIBLOCK		0x4

struct sdioh_info {
	uint cfg_bar;                   	/* pci cfg address for bar */
	uint32 caps;                    	/* cached value of capabilities reg */
	uint		bar0;			/* BAR0 for PCI Device */
	osl_t 		*osh;			/* osh handler */
	void		*controller;	/* Pointer to SPI Controller's private data struct */

	uint		lockcount; 		/* nest count of sdspi_lock() calls */
	bool		client_intr_enabled;	/* interrupt connnected flag */
	bool		intr_handler_valid;	/* client driver interrupt handler valid */
	sdioh_cb_fn_t	intr_handler;		/* registered interrupt handler */
	void		*intr_handler_arg;	/* argument to call interrupt handler */
	bool		initialized;		/* card initialized */
	uint32		target_dev;		/* Target device ID */
	uint32		intmask;		/* Current active interrupts */
	void		*sdos_info;		/* Pointer to per-OS private data */

	uint32		controller_type;	/* Host controller type */
	uint8		version;		/* Host Controller Spec Compliance Version */
	uint 		irq;			/* Client irq */
	uint32 		intrcount;		/* Client interrupts */
	uint32 		local_intrcount;	/* Controller interrupts */
	bool 		host_init_done;		/* Controller initted */
	bool 		card_init_done;		/* Client SDIO interface initted */
	bool 		polled_mode;		/* polling for command completion */

	bool		sd_use_dma;		/* DMA on CMD53 */
	bool 		sd_blockmode;		/* sd_blockmode == FALSE => 64 Byte Cmd 53s. */
						/*  Must be on for sd_multiblock to be effective */
	bool 		use_client_ints;	/* If this is false, make sure to restore */
	bool		got_hcint;		/* Host Controller interrupt. */
						/*  polling hack in wl_linux.c:wl_timer() */
	int 		adapter_slot;		/* Maybe dealing with multiple slots/controllers */
	int 		sd_mode;		/* SD1/SD4/SPI */
	int 		client_block_size[SDIOD_MAX_IOFUNCS];		/* Blocksize */
	uint32 		data_xfer_count;	/* Current register transfer size */
	uint32		cmd53_wr_data;		/* Used to pass CMD53 write data */
	uint32		card_response;		/* Used to pass back response status byte */
	uint32		card_rsp_data;		/* Used to pass back response data word */
	uint16 		card_rca;		/* Current Address */
	uint8 		num_funcs;		/* Supported funcs on client */
	uint32 		com_cis_ptr;
	uint32 		func_cis_ptr[SDIOD_MAX_IOFUNCS];
	void		*dma_buf;
	ulong		dma_phys;
	int 		r_cnt;			/* rx count */
	int 		t_cnt;			/* tx_count */
};

/************************************************************
 * Internal interfaces: per-port references into bcmsdspi.c
 */

/* Global message bits */
extern uint sd_msglevel;

/**************************************************************
 * Internal interfaces: bcmsdspi.c references to per-port code
 */

/* Register mapping routines */
extern uint32 *spi_reg_map(osl_t *osh, uintptr addr, int size);
extern void spi_reg_unmap(osl_t *osh, uintptr addr, int size);

/* Interrupt (de)registration routines */
extern int spi_register_irq(sdioh_info_t *sd, uint irq);
extern void spi_free_irq(uint irq, sdioh_info_t *sd);

/* OS-specific interrupt wrappers (atomic interrupt enable/disable) */
extern void spi_lock(sdioh_info_t *sd);
extern void spi_unlock(sdioh_info_t *sd);

/* Allocate/init/free per-OS private data */
extern int spi_osinit(sdioh_info_t *sd);
extern void spi_osfree(sdioh_info_t *sd);

#endif /* _BCM_SD_SPI_H */
