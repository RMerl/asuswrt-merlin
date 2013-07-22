/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  API test program				File: test.c
    *
    *  Small program to test CFE's external API
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


#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_api.h"

int conhandle;


void appletmain(unsigned long handle,unsigned long vector,
		unsigned long reserved,unsigned long seal);




/*  *********************************************************************
    *  console_write(buffer,length)
    *  
    *  Write text to the console.  If the console is not open and
    *  we're "saving" text, put the text on the in-memory queue
    *  
    *  Input parameters: 
    *  	   buffer - pointer to data
    *  	   length - number of characters to write
    *  	   
    *  Return value:
    *  	   number of characters written or <0 if error
    ********************************************************************* */

static int console_write(unsigned char *buffer,int length)
{
    int res;

    /*
     * Do nothing if no console
     */

    if (conhandle == -1) return -1;

    /*
     * Write text to device
     */

    for (;;) {
	res = cfe_write(conhandle,buffer,length);
	if (res < 0) break;
	buffer += res;
	length -= res;
	if (length == 0) break;
	}

    if (res < 0) return -1;
    return 0;			 
}


/*  *********************************************************************
    *  console_xprint(str)
    *  
    *  printf callback for the console device.  the "xprintf"
    *  routine ends up calling this.  This routine also cooks the
    *  output, turning "\n" into "\r\n"
    *  
    *  Input parameters: 
    *  	   str - string to write
    *  	   
    *  Return value:
    *  	   number of characters written
    ********************************************************************* */

static int console_xprint(const char *str)
{
    int count = 0;	
    int len;
    const char *p;

    /* Convert CR to CRLF as we write things out */

    while ((p = strchr(str,'\n'))) {
	console_write((char *) str,p-str);
	console_write("\r\n",2);
	count += (p-str);
	str = p + 1;
	}

    len = strlen(str);
    console_write((char *) str, len);
    count += len;

    return count;
}



void appletmain(unsigned long handle,unsigned long vector,
		unsigned long ept,unsigned long seal)
{
    void (*reboot)(void) = (void *) (uintptr_t) (int) 0xBFC00000;
    char str[100];
    cfe_fwinfo_t info;
    int idx;
    int res;
    uint64_t start,length,type;

    xprinthook = console_xprint;

    cfe_init(handle,ept);

    conhandle = cfe_getstdhandle(CFE_STDHANDLE_CONSOLE);

    str[0] = 0;
    cfe_getenv("BOOT_CONSOLE",str,sizeof(str));

    xprintf("Hello, world.  Console = %s\n",str);
    xprintf("API Seal = %08X\n",(int)seal);
    xprintf("Entrypoint=%08X  Handle=%08X\n",(int)ept,(int)handle);
    idx = 0;

    cfe_getfwinfo(&info);
    xprintf("CFE version:    %08llX\n",info.fwi_version);
    xprintf("Flags:          %08llX\n",info.fwi_flags);
    xprintf("Total memory:   %08llX\n",info.fwi_totalmem);
    xprintf("Board ID:       %08llX\n",info.fwi_boardid);
    xprintf("Bootarea VA:    %08llX\n",info.fwi_bootarea_va);
    xprintf("Bootarea PA:    %08llX\n",info.fwi_bootarea_pa);
    xprintf("Bootarea Size:  %08llX\n",info.fwi_bootarea_size);

    xprintf("Memory map:\n");
    for (;;) {
	if ((res = cfe_enummem(idx,1,&start,&length,&type) != 0)) break;
	xprintf("Memory at %016llX length %016llX type %ld\n",
		start,length,type);
	idx++;
	}

    xprintf("Exiting to CFE\n\n");

    cfe_exit(CFE_FLG_WARMSTART,0);

    (*reboot)();

}
