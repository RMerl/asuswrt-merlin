/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  NVRAM subroutines			File: nvram_subr.c
    *  
    *  High-level routines to read/write the NVRAM device.
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
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"

#include "cfe_error.h"
#include "env_subr.h"
#include "nvram_subr.h"
#include "cfe.h"

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

static int nvram_handle = -1;
static nvram_info_t nvram_info;
static char *nvram_devname = NULL;

/*  *********************************************************************
    *  nvram_getinfo(info)
    *  
    *  Obtain information about the NVRAM device from the device
    *  driver.  A flash device might only dedicate a single sector
    *  to the environment, so we need to ask the driver first.
    *  
    *  Input parameters: 
    *  	   info - nvram info
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   <0 = error code
    ********************************************************************* */

static int nvram_getinfo(nvram_info_t *info)
{
    int retlen;

    if (nvram_handle == -1) return -1;

    cfe_ioctl(nvram_handle,IOCTL_NVRAM_UNLOCK,NULL,0,NULL,0);

    if (cfe_ioctl(nvram_handle,IOCTL_NVRAM_GETINFO,
		  (unsigned char *) info,
		  sizeof(nvram_info_t),
		  &retlen,0) != 0) {
	return -1;
	}

    return 0;

}

/*  *********************************************************************
    *  nvram_open()
    *  
    *  Open the default NVRAM device and get the information from the
    *  device driver.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int nvram_open(void)
{
    if (nvram_handle != -1) {
	nvram_close();
	}

    if (nvram_devname == NULL) {
	return CFE_ERR_DEVNOTFOUND;
	}

    nvram_handle = cfe_open(nvram_devname);

    if (nvram_handle < 0) {
	return CFE_ERR_DEVNOTFOUND;
	}

    if (nvram_getinfo(&nvram_info) < 0) {
	nvram_close();
	return CFE_ERR_IOERR;
	}

    return 0;
}

/*  *********************************************************************
    *  nvram_close()
    *  
    *  Close the NVRAM device
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

int nvram_close(void)
{
    if (nvram_handle != -1) {
	cfe_close(nvram_handle);
	nvram_handle = -1;
	}

    return 0;
}


/*  *********************************************************************
    *  nvram_getsize()
    *  
    *  Return the total size of the NVRAM device.  Note that
    *  this is the total size that is used for the NVRAM functions,
    *  not the size of the underlying media.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   size.  <0 if error
    ********************************************************************* */

int nvram_getsize(void)
{
    if (nvram_handle < 0) return 0;
    return nvram_info.nvram_size;
}


/*  *********************************************************************
    *  nvram_read(buffer,offset,length)
    *  
    *  Read data from the NVRAM device
    *  
    *  Input parameters: 
    *  	   buffer - destination buffer
    *  	   offset - offset of data to read
    *  	   length - number of bytes to read
    *  	   
    *  Return value:
    *  	   number of bytes read, or <0 if error occured
    ********************************************************************* */
int nvram_read(char *buffer,int offset,int length)
{
    if (nvram_handle == -1) return -1;

    return cfe_readblk(nvram_handle,
		       (cfe_offset_t) (offset+nvram_info.nvram_offset),
		       (unsigned char *)buffer,
		       length);
}

/*  *********************************************************************
    *  nvram_write(buffer,offset,length)
    *  
    *  Write data to the NVRAM device
    *  
    *  Input parameters: 
    *  	   buffer - source buffer
    *  	   offset - offset of data to write
    *  	   length - number of bytes to write
    *  	   
    *  Return value:
    *  	   number of bytes written, or -1 if error occured
    ********************************************************************* */
int nvram_write(char *buffer,int offset,int length)
{
    if (nvram_handle == -1) return -1;

    return cfe_writeblk(nvram_handle,
			(cfe_offset_t) (offset+nvram_info.nvram_offset),
			(unsigned char *)buffer,
			length);
}


/*  *********************************************************************
    *  nvram_erase()
    *  
    *  Erase the NVRAM device.  Not all devices need to be erased,
    *  but flash memory does.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int nvram_erase(void)
{
    int retlen;

    if (nvram_handle < 0) {
	return -1;
	}

    if (nvram_info.nvram_eraseflg == FALSE) return 0;

    if (cfe_ioctl(nvram_handle,IOCTL_NVRAM_ERASE,
		  (unsigned char *) &nvram_info,
		  sizeof(nvram_info_t),
		  &retlen,0) != 0) {
	return -1;
	}

    return 0;
}


/*  *********************************************************************
    *  cfe_set_envdevice(name)
    *  
    *  Set the environment NVRAM device name
    *  
    *  Input parameters: 
    *  	   name - name of device to use for NVRAM
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int cfe_set_envdevice(char *name)
{
    int res;

    nvram_devname = strdup(name);

    res = nvram_open();

    if (res != 0) {
	xprintf("!! Could not open NVRAM device %s\n",nvram_devname);
	return res;
	}

    nvram_close();

    res = env_load();

    return res;
}
