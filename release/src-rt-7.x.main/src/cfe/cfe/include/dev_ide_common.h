/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Generic IDE disk driver			File: dev_ide_common.c
    *  
    *  This file contains common constants and structures for IDE
    *  disks and CFE drivers for them.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define IDE_REG_DATA	0x0
#define IDE_REG_ERROR	0x1
#define IDE_REG_PRECOMP	0x1
#define IDE_REG_SECCNT	0x2
#define IDE_REG_IR	0x2	/* ATAPI */
#define IDE_REG_SECNUM	0x3
#define IDE_REG_BCLSB	0x4	/* ATAPI */
#define IDE_REG_BCMSB	0x5	/* ATAPI */
#define IDE_REG_CYLLSB	0x4
#define IDE_REG_CYLMSB	0x5
#define IDE_REG_DRVHD	0x6
#define IDE_REG_STATUS	0x7
#define IDE_REG_COMMAND	0x7
#define IDE_REG_ALTSTAT	0x6	/* Note: ALTSTAT is really 0x3F6, what do we do? */
#define IDE_REG_DIGOUT	0x6

#define IDE_IR_CD	0x01	/* 1 = command, 0 = data */
#define IDE_IR_IO	0x02	/* 1 = from device, 0 = to device */
#define IDE_IR_REL	0x04

#define IDE_ERR_BBK	0x80	/* sector marked bad by host */
#define IDE_ERR_UNC	0x40	/* uncorrectable error */
#define IDE_ERR_MC	0x20	/* medium changed */
#define IDE_ERR_NID	0x10	/* no ID mark found */
#define IDE_ERR_MCR	0x08	/* medium change required */
#define IDE_ERR_ABT	0x04	/* command aborted */
#define IDE_ERR_NT0	0x02	/* track 0 not found */
#define IDE_ERR_NDM	0x01	/* address mark not found */

#define IDE_DRV_SLAVE	0x10
#define IDE_DRV_LBA	0x40
#define IDE_DRV_MBO	0xA0
#define IDE_DRV_HDMASK	0x0F

#define IDE_STS_BSY	0x80	/* drive is busy */
#define IDE_STS_RDY	0x40	/* drive is ready */
#define IDE_STS_WFT	0x20	/* write fault */
#define IDE_STS_SKC	0x10	/* seek complete */
#define IDE_STS_DRQ	0x08	/* data can be transferred */
#define IDE_STS_CORR	0x04	/* correctable data error */
#define IDE_STS_IDX	0x02	/* index mark just passed */
#define IDE_STS_ERR	0x01	/* Error register contains info */

#define IDE_CMD_RECAL		0x10
#define IDE_CMD_READ		0x20
#define IDE_CMD_READRETRY	0x21
#define IDE_CMD_WRITE		0x30
#define IDE_CMD_READVERIFY	0x40
#define IDE_CMD_DIAGNOSTIC	0x90
#define IDE_CMD_INITPARAMS	0x91
#define IDE_CMD_SETMULTIPLE	0xC6
#define IDE_CMD_POWER_MODE	0xE5
#define IDE_CMD_DRIVE_INFO	0xEC

#define IDE_CMD_ATAPI_SOFTRESET	0x08
#define IDE_CMD_ATAPI_PACKET	0xA0
#define IDE_CMD_ATAPI_IDENTIFY	0xA1
#define IDE_CMD_ATAPI_SERVICE	0xA2

#define IDE_DOR_SRST		0x04
#define IDE_DOR_IEN		0x02

#define DISK_SECTORSIZE		512
#define CDROM_SECTORSIZE	2048
#define MAX_SECTORSIZE		2048

#define ATAPI_SENSE_MASK	0xF0
#define ATAPI_SENSE_NONE	0x00
#define ATAPI_SENSE_RECOVERED	0x10
#define ATAPI_SENSE_NOTREADY	0x20
#define ATAPI_SENSE_MEDIUMERROR	0x30
#define ATAPI_SENSE_HWERROR	0x40
#define ATAPI_SENSE_ILLREQUEST	0x50
#define ATAPI_SENSE_ATTENTION	0x60
#define ATAPI_SENSE_PROTECT	0x70
#define ATAPI_SENSE_BLANKCHECK	0x80
#define ATAPI_SENSE_VSPECIFIC	0x90
#define ATAPI_SENSE_COPYABORT	0xA0
#define ATAPI_SENSE_CMDABORT	0xB0
#define ATAPI_SENSE_EQUAL	0xC0
#define ATAPI_SENSE_VOLOVERFLOW	0xD0
#define ATAPI_SENSE_MISCOMPARE	0xE0
#define ATAPI_SENSE_RESERVED	0xF0

#define ATAPI_SIG_LSB		0x14
#define ATAPI_SIG_MSB		0xEB

#define CDB_CMD_READ		0x28
#define CDB_CMD_WRITE		0x2A
#define CDB_CMD_REQSENSE	0x03


/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct idecommon_dispatch_s idecommon_dispatch_t;

struct idecommon_dispatch_s {
    void *ref;
    uint32_t baseaddr;
    uint8_t (*inb)(idecommon_dispatch_t *disp,uint32_t reg);
    uint16_t (*inw)(idecommon_dispatch_t *disp,uint32_t reg);
    void (*ins)(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len);

    void (*outb)(idecommon_dispatch_t *disp,uint32_t reg,uint8_t val);
    void (*outw)(idecommon_dispatch_t *disp,uint32_t reg,uint16_t val);
    void (*outs)(idecommon_dispatch_t *disp,uint32_t reg,uint8_t *buf,int len);
};

#define IDEDISP_WRITEREG8(ide,reg,val)    (*((ide)->outb))(ide,reg,val)
#define IDEDISP_WRITEREG16(ide,reg,val)   (*((ide)->outw))(ide,reg,val)
#define IDEDISP_WRITEBUF(ide,reg,buf,len) (*((ide)->outs))(ide,reg,buf,len)
#define IDEDISP_READREG8(ide,reg)         (*((ide)->inb))(ide,reg)
#define IDEDISP_READREG16(ide,reg)        (*((ide)->inw))(ide,reg)
#define IDEDISP_READBUF(ide,reg,buf,len)  (*((ide)->ins))(ide,reg,buf,len)

typedef struct idecommon_s idecommon_t;

struct idecommon_s {
    idecommon_dispatch_t *idecommon_dispatch;
    unsigned long idecommon_addr;	/* physical address */
    int idecommon_unit;			/* 0 or 1 master/slave */
    int idecommon_sectorsize;		/* size of each sector */
    long long idecommon_ttlsect;	/* total sectors */
    int idecommon_atapi;		/* 1 if ATAPI device */
    int idecommon_devtype;		/* device type */
    uint64_t idecommon_deferprobe;	/* Defer probe to open */
    uint32_t idecommon_flags;		/* flags for underlying driver */
    int (*idecommon_readfunc)(idecommon_t *ide,uint64_t lba,int numsec,uint8_t *buffer);
    int (*idecommon_writefunc)(idecommon_t *ide,uint64_t lba,int numsec,uint8_t *buffer);

    uint32_t timer;
};



/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

extern void idecommon_init(idecommon_t *ide,int devtype);
extern int idecommon_open(cfe_devctx_t *ctx);
extern int idecommon_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
extern int idecommon_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
extern int idecommon_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
extern int idecommon_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
extern int idecommon_identify(idecommon_t *ide,unsigned char *buffer);
extern int idecommon_close(cfe_devctx_t *ctx);
extern int idecommon_devprobe(idecommon_t *ide,int noisy);
void idecommon_attach(cfe_devdisp_t *disp);
