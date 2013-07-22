/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SREC Program Loader			File: cfe_ldr_srec.c
    *  
    *  This program reads Motorola S-record files into memory
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
#ifdef	CFG_LDR_SREC

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_error.h"
#include "cfe_devfuncs.h"

#include "cfe.h"
#include "cfe_fileops.h"

#include "cfe_boot.h"
#include "cfe_bootblock.h"

#include "cfe_loader.h"

#include "cfe_mem.h"

/*  *********************************************************************
    *  Externs
    ********************************************************************* */

static int cfe_srecload(cfe_loadargs_t *la);

const cfe_loader_t srecloader = {
    "srec",
    cfe_srecload,
    0};


typedef struct linebuf_s {
    char linebuf[256];
    int curidx;
    int buflen;
    int eof;
    void *ref;
    fileio_ctx_t *fsctx;
} linebuf_t;

#define initlinebuf(b,r,o) (b)->curidx = 0; (b)->buflen = 0; \
                           (b)->eof = 0 ; (b)->ref = r; (b)->fsctx = o


/*  *********************************************************************
    *  readchar(file)
    *  
    *  Read a character from a file.  It's kind of like getchar
    *  on "C" FILE devices, but not so fancy.
    *  
    *  Input parameters: 
    *  	   file - pointer to a linebuf_t containing reader state
    *  	   
    *  Return value:
    *  	   character, or -1 if at EOF
    ********************************************************************* */

static int readchar(linebuf_t *file)
{
    int ch;
    int res;

    if (file->eof) return -1;

    if (file->curidx == file->buflen) {
	for (;;) {
	    res = fs_read(file->fsctx,file->ref,(unsigned char *)file->linebuf,sizeof(file->linebuf));
	    if (res < 0) {
		file->eof = -1;
		return -1;
		}
	    if (res == 0) continue;
	    file->buflen = res;
	    file->curidx = 0;
	    break;
	    }
	}

    ch = file->linebuf[file->curidx];
    file->curidx++;
    return ch;
}


/*  *********************************************************************
    *  readline(file,buffer,maxlen)
    *  
    *  Read a line of text from a file using our crude file stream
    *  mechanism.
    *  
    *  Input parameters: 
    *  	   file - pointer to a linebuf_t containing reader state
    *  	   buffer - will receive line of text
    *  	   maxlen - number of bytes that will fit in buffer
    *  	   
    *  Return value:
    *  	   0 if ok, else <0 if at EOF
    ********************************************************************* */

static int readline(linebuf_t *file,char *buffer,int maxlen)
{
    int ch;
    char *ptr;

    ptr = buffer;
    maxlen--;			/* account for terminating null */

    while (maxlen) {
	ch = readchar(file);
	if (ch == -1) return -1;
	if (ch == 27) return -1;	/* ESC */
	if ((ch == '\n') || (ch == '\r')) break;
	*ptr++ = (char) ch;
	maxlen--;
	}

    *ptr = '\0';

    return 0;
}


/*  *********************************************************************
    *  getxdigit(c)
    *  
    *  Convert a hex digit into its numeric equivalent
    *  
    *  Input parameters: 
    *  	   c - character
    *  	   
    *  Return value:
    *  	   value
    ********************************************************************* */

static int getxdigit(char c)
{
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    return -1;
}

/*  *********************************************************************
    *  getbyte(line)
    *  
    *  Process two hex digits and return the value
    *  
    *  Input parameters: 
    *  	   line - pointer to pointer to characters (updated on exit)
    *  	   
    *  Return value:
    *  	   byte value, or <0 if bad hex digits
    ********************************************************************* */

static int getbyte(char **line)
{
    int res;
    int c1,c2;

    c1 = getxdigit(*(*(line)+0));
    if (c1 < 0) return -1;

    c2 = getxdigit(*(*(line)+1));
    if (c2 < 0) return -1;

    res = (c1*16+c2);
    (*line) += 2;

    return res;
}


/*  *********************************************************************
    *  procsrec(line,loadaddr,blklen,data)
    *  
    *  Process an S-record, reading the data into a local buffer
    *  and returning the block's address.
    *  
    *  Input parameters: 
    *  	   line - line of text (s-record line)
    *  	   loadaddr - will be filled with address where data should go
    *  	   blklen - will be filled in with size of record
    *  	   data - points to buffer to receive data
    *  	   
    *  Return value:
    *  	   <0 if error occured (not an s-record)
    ********************************************************************* */

static int procsrec(char *line,
		    unsigned int *loadaddr,
		    unsigned int *blklen,
		    unsigned char *data)
{
    char rectype;
    unsigned char b;
    unsigned int len;
    unsigned int minlen;
    unsigned int linelen;
    unsigned int addr;
    unsigned int chksum;

    int idx;
    int ret = 0;

    addr = 0;

    if (*line++ != 'S')
	return -1;				/* not an S record */

    rectype = *line++;

    minlen = 3;					/* type 1 record */
    switch (rectype) {
	case '0':
	    break;

	/*
	 * data bytes
	 */
	case '3':
	    minlen++;
	    /* fall through */
	case '2':
	    minlen++;
	    /* fall through */
	case '1':
	    chksum = 0;
	    linelen = getbyte(&line);
	    if (linelen < minlen) {
		xprintf("srec: line too short\n");
		return -1;
		}
	    chksum += (unsigned int)linelen;

	    /*
	     * There are two address bytes in a type 1 record, and three
	     * in a type 2 record.  The high-order byte is first, then
	     * one or two lower-order bytes.  Build up the adddress.
	     */
	    b = getbyte(&line);
	    chksum += (unsigned int)b;
	    addr = b;
	    b = getbyte(&line);
	    chksum += (unsigned int)b;
	    addr <<= 8;
	    addr += b;
	    if (rectype == '2') {
		b = getbyte(&line);
		chksum += (unsigned int)b;
		addr <<= 8;
		addr += b;
		}
	    if (rectype == '3') {
		b = getbyte(&line);
		chksum += (unsigned int)b;
		addr <<= 8;
		addr += b;
		b = getbyte(&line);
		chksum += (unsigned int)b;
		addr <<= 8;
		addr += b;
		}

#if	VERBOSE
	    printf("Addr: %08X Len: %3u(0x%x)\n", addr , linelen - minlen,
		   linelen-minlen);
#endif
    
	    *loadaddr = addr;
	    len = linelen - minlen;
	    *blklen = len;

	    for (idx = 0; idx < len; idx++) {
		b = getbyte(&line);
		chksum += (unsigned int) b;
		data[idx] = (unsigned char ) b;
		}

	    b = getbyte(&line);
	    chksum = (~chksum) & 0x000000FF;
	    if (chksum != b) {
		xprintf("Checksum error in s-record file\n");
		return -1;
		}
	    ret = 1;
	    break;

	case '9':
	    linelen = getbyte(&line);
	    b = getbyte(&line);
	    addr = b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    *loadaddr = addr;
	    ret = -2;
	    break;

	case '8':
	    linelen = getbyte(&line);
	    b = getbyte(&line);
	    addr = b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    *loadaddr = addr;
	    ret = -2;
	    break;

	case '7':
	    linelen = getbyte(&line);
	    b = getbyte(&line);
	    addr = b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    b = getbyte(&line);
	    addr <<= 8;
	    addr += b;
	    *loadaddr = addr;
	    ret = -2;
	    break;

	default:
	    xprintf("Unknown S-record type: %c\n",rectype);
	    return -1;
	    break;
	}

    return ret;
}


/*  *********************************************************************
    *  cfe_srecload(la)
    *  
    *  Read an s-record file
    *  
    *  Input parameters: 
    *      la - loader args
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */
static int cfe_srecload(cfe_loadargs_t *la)
{
    int res;
    fileio_ctx_t *fsctx;
    void *ref;
    int devinfo;
    int serial = FALSE;
    unsigned int loadaddr = 0;
    unsigned int blklen = 0;
    linebuf_t lb;
    char line[256];
    uint8_t data[256];
    int cnt;
    unsigned int specaddr;	/* current address if loading to a special address */
    int specflg;		/* true if in "special address" mode */
    int firstrec = 1;		/* true if we have not seen the first record */

    /*
     * We might want to know something about the boot device.
     */

    devinfo = la->la_device ? cfe_getdevinfo(la->la_device) : 0;

    /*
     * Figure out if we're loading to a "special address".  This lets
     * us load S-records into a temporary buffer, ignoring the
     * addresses in the records (but using them so we'll know where
     * they go relative to each other 
     */

    specflg = (la->la_flags & LOADFLG_SPECADDR) ? 1 : 0;
    specaddr = 0;

    /*	
     * If the device is a serial port, we want to know that.
     */

    if ((devinfo >= 0) && 
        ((devinfo & CFE_DEV_MASK) == CFE_DEV_SERIAL)) {
	serial = TRUE;
	}

    /*
     * Create a file system context
     */

    res = fs_init(la->la_filesys,&fsctx,la->la_device);
    if (res != 0) {
	return res;
	}

    /*
     * Turn on compression if we're doing that.
     */

    if (la->la_flags & LOADFLG_COMPRESSED) {
	res = fs_hook(fsctx,"z");
	if (res != 0) {
	    return res;
	    }
	}

    /*
     * Open the boot device
     */

    res = fs_open(fsctx,&ref,la->la_filename,FILE_MODE_READ);
    if (res != 0) {
	fs_uninit(fsctx);
	return res;
	}


    initlinebuf(&lb,ref,fsctx);

    cnt = 0;
    for (;;) {
	/*
	 * Read a line of text
	 */
	res = readline(&lb,line,sizeof(line));
	if (res < 0) break;		/* reached EOF */

	/*
	 * Process the S-record.  If at EOF, procsrec returns 0.
	 * Invalid s-records returns -1.
	 */

	if (line[0] == 0) continue;

	res = procsrec(line, &loadaddr, &blklen, data);

	if (res < 0) break;

	/*
	 * Handle "special address" mode.   All S-records will be 
	 * loaded into a buffer passed by the caller to the loader.
	 * We use the addresses in the S-records to determine 
	 * relative placement of the data, keying on the first
	 * S-record in the file.
	 */

	if ((res == 1) && (specflg)) {
	    if (firstrec) {
		/* First S-record seen sets the base for all that follow */
		specaddr = loadaddr;
		firstrec = 0;
		}
	    loadaddr = la->la_address + (intptr_t) (loadaddr - specaddr);
	    }

	cnt++;

	if (res == 1) {
	    if (!cfe_arena_loadcheck((intptr_t)loadaddr, blklen)) {
		xprintf("Invalid address: %P\n", loadaddr);
		res = -1;
		break;
		}
	    memcpy((uint8_t *)(intptr_t)(signed)loadaddr, data, blklen);
	    }
	}

    /*
     * We're done with the file.
     */

    fs_close(fsctx,ref);
    fs_uninit(fsctx);

    /*
     * Say something cute on the LEDs.
     * Don't do this for every s-record, because if logging the LED
     * changes out the serial port, that can take a Long Time.  Just
     * goes to show: too much cuteness is a _very_ bad thing.
     */
    xsprintf(line,"%04d",cnt);
    cfe_ledstr(line);

    if (res == -2) {
	la->la_entrypt = (intptr_t)(signed)loadaddr;
	res = 0;
	}

    return res;
}

#endif	/* CFG_LDR_SREC */
