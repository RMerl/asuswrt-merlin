/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Automatic OS bootstrap			File: cfe_autoboot.c
    *  
    *  This module handles OS bootstrap stuff.  We use this version
    *  to do "automatic" booting; walking down a list of possible boot
    *  options, trying them until something good happens.
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
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"

#include "cfe_error.h"

#include "env_subr.h"
#include "cfe.h"
#include "bsp_config.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_api.h"
#include "cfe_fileops.h"
#include "cfe_bootblock.h"
#include "cfe_boot.h"

#include "cfe_loader.h"

#include "cfe_autoboot.h"

#if CFG_AUTOBOOT
/*  *********************************************************************
    *  data
    ********************************************************************* */

queue_t cfe_autoboot_list = {&cfe_autoboot_list,&cfe_autoboot_list};

extern cfe_loadargs_t cfe_loadargs;		/* from cfe_boot.c */
extern const char *bootvar_device;
extern const char *bootvar_file;
extern const char *bootvar_flags;

extern char *varchars;

/*  *********************************************************************
    *  macroexpand(instr,outstr)
    *  
    *  Expand environment variables in "instr" to "outstr"
    *  
    *  Input parameters: 
    *  	   instr - to be expanded
    *	   outstr - expanded.
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void macroexpand(char *instr,char *outstr)
{
    char macroname[50];
    char *m;

    while (*instr) {
	if (*instr == '$') {
	    instr++;
	    m = macroname;
	    while (*instr && strchr(varchars,*instr)) {
		*m++ = *instr++;
		}
	    *m = '\0';
	    m = env_getenv(macroname);
	    if (m) {
		while (*m) *outstr++ = *m++;
		}
	    }
	else {
	    *outstr++ = *instr++;
	    }
	}

    *outstr = '\0';
}

/*  *********************************************************************
    *  cfe_tryauto_common(method,loadargs)
    *  
    *  Common portion of autoboot
    *  
    *  Input parameters: 
    *  	   method - details on device to boot from
    *      filename - canonical name of file to load
    *  	   loadargs - cfe_loadargs_t of digested load parameters.
    *  	   
    *  Return value:
    *  	   does not return if successful
    ********************************************************************* */

static int cfe_tryauto_common(cfe_autoboot_method_t *method,
			      char *filename,
			      cfe_loadargs_t *la)
{
    int res;

    la->la_filename = filename;
    la->la_options = env_getenv(bootvar_flags);
    la->la_filesys = method->ab_filesys;
    la->la_device = method->ab_dev;
    la->la_flags = method->ab_flags | LOADFLG_NOISY | LOADFLG_EXECUTE;
    la->la_address = 0;
    la->la_maxsize = 0;
    la->la_entrypt = 0;

    /* okay, go for it. */

    xprintf("Loader:%s Filesys:%s Dev:%s File:%s Options:%s\n",
	    method->ab_loader,la->la_filesys,la->la_device,la->la_filename,la->la_options);

    res = cfe_boot(method->ab_loader,&cfe_loadargs);

    return res;

}
			      

/*  *********************************************************************
    *  cfe_tryauto_network(method)
    *  
    *  Try to autoboot from a network device.
    *  
    *  Input parameters: 
    *  	   method - details on device to boot from
    *  	   
    *  Return value:
    *  	   does not return unless there is an error
    ********************************************************************* */

#if CFG_NETWORK
#if CFG_DHCP
static int cfe_tryauto_network(cfe_autoboot_method_t *method)
{
    int res;
    dhcpreply_t *reply = NULL;
    cfe_loadargs_t *la = &cfe_loadargs;
    char buffer[512];
    char *x;

    /*
     * First turn off any network interface that is currently active.
     */

    net_uninit();

    /*
     * Now activate the network hardware.
     */

    res = net_init(method->ab_dev);
    if (res < 0) {
	printf("Could not initialize network device %s: %s\n",
	       method->ab_dev,
	       cfe_errortext(res));
	return res;
	}

    /*
     * Do a DHCP query.
     */

    res = dhcp_bootrequest(&reply);
    if (res < 0) {
	printf("DHCP request failed on device %s: %s\n",method->ab_dev,
	       cfe_errortext(res));
	net_uninit();
	return res;
	}

    net_setparam(NET_IPADDR,reply->dr_ipaddr);
    net_setparam(NET_NETMASK,reply->dr_netmask);
    net_setparam(NET_GATEWAY,reply->dr_gateway);
    net_setparam(NET_NAMESERVER,reply->dr_nameserver);
    net_setparam(NET_DOMAIN,(uint8_t *)reply->dr_domainname);

    /* Set all our environment variables. */
    net_setnetvars();
    dhcp_set_envvars(reply);

    if (reply) dhcp_free_reply(reply);

    /*
     * Interface is now configured and ready, we can 
     * load programs now.
     */

    /*
     * Construct the file name to load.  If the method does not
     * specify a file name directly, get it from DHCP.
     *
     * For network booting, the format of the file name
     * is 'hostname:filename'
     *
     * If the method's filename includes a hostname, ignore
     * BOOT_SERVER.  Otherwise, combine BOOT_SERVER with the
     * filename.  This way we can provide the name here
     * but have the file come off the server specified in the
     * DHCP message.
     */

    if (method->ab_file && strchr(method->ab_file,':')) {
	macroexpand(method->ab_file,buffer);
	}
    else {
	buffer[0] = '\0';
	x = env_getenv("BOOT_SERVER");
	if (x) {
	    strcat(buffer,x);
	    strcat(buffer,":");
	    }

	x = method->ab_file;
	if (!x) x = env_getenv(bootvar_file);
	if (x) {
	    strcat(buffer,x);
	    }
	}

    /* Okay, do it. */

    cfe_tryauto_common(method,buffer,la);

    /* If we get here, something bad happened. */

    net_uninit();

    return res;

}

#endif	/* CFG_DHCP */
#endif


/*  *********************************************************************
    *  cfe_tryauto_disk(method)
    *  
    *  Try to autoboot from a disk device.
    *  
    *  Input parameters: 
    *  	   method - details on device to boot from
    *  	   
    *  Return value:
    *  	   does not return unless there is an error
    ********************************************************************* */
static int cfe_tryauto_disk(cfe_autoboot_method_t *method)
{
    int res;			
    cfe_loadargs_t *la = &cfe_loadargs;
    char buffer[512];
    char *filename;

    buffer[0] = '\0';

    if (method->ab_file) {
	macroexpand(method->ab_file,buffer);
	filename = buffer;
	}
    else {
	filename = env_getenv("BOOT_FILE");
	if (filename) strcpy(buffer,filename);
	}

    res = cfe_tryauto_common(method,filename,la);

    return res;
}

/*  *********************************************************************
    *  cfe_tryauto(method)
    *  
    *  Try a single autoboot method.
    *  
    *  Input parameters: 
    *  	   method - autoboot method to try
    *  	   
    *  Return value:
    *  	   does not return if bootstrap is successful
    *  	   else error code
    ********************************************************************* */

static int cfe_tryauto(cfe_autoboot_method_t *method)
{
    switch (method->ab_type) {
#if CFG_NETWORK
#if CFG_DHCP
	case CFE_AUTOBOOT_NETWORK:
	    return cfe_tryauto_network(method);
	    break;
#endif	/* CFG_DHCP */
#endif

	case CFE_AUTOBOOT_DISK:
	case CFE_AUTOBOOT_RAW:
	    return cfe_tryauto_disk(method);
	    break;
	}

    return -1;
}

/*  *********************************************************************
    *  cfe_autoboot(dev,flags)
    *  
    *  Try to perform an automatic system bootstrap
    *  
    *  Input parameters: 
    *  	   dev - if not NULL, restrict bootstrap to named device
    *  	   flags - controls behaviour of cfe_autoboot
    *  	   
    *  Return value:
    *  	   should not return if bootstrap is successful, otherwise
    *  	   error code
    ********************************************************************* */
int cfe_autoboot(char *dev,int flags)
{
    queue_t *qb;
    cfe_autoboot_method_t *method;
    int res;
    cfe_timer_t timer;
    int forever;
    int pollconsole;

    forever = (flags & CFE_AUTOFLG_TRYFOREVER) ? 1 : 0;
    pollconsole = (flags & CFE_AUTOFLG_POLLCONSOLE) ? 1 : 0;

    do {	/* potentially forever */
	for (qb = cfe_autoboot_list.q_next; qb != &cfe_autoboot_list; qb = qb->q_next) {

	    method = (cfe_autoboot_method_t *) qb;

	    /* 
	     * Skip other devices if we passed in a specific one.
	     */

	    if (dev && (strcmp(dev,method->ab_dev) != 0)) continue;

	    printf("\n*** Autoboot: Trying device '%s' ", method->ab_dev);
	    if (method->ab_file) printf("file %s ",method->ab_file);
	    printf("(%s,%s)\n\n",method->ab_dev,method->ab_filesys,method->ab_loader);

	    /*
	     * Scan keyboard if requested.
	     */
	    if (pollconsole) {
		TIMER_SET(timer,CFE_HZ);
		while (!TIMER_EXPIRED(timer)) {
		    POLL();
		    if (console_status()) goto done;
		    }
		}

	    /*
	     * Try something.  may not return.
	     */

	    res = cfe_tryauto(method);
	    }
	} while (forever);

    /* bail. */
done:

    printf("\n*** Autoboot failed.\n\n");

    return -1;
}


/*  *********************************************************************
    *  cfe_add_autoboot(type,flags,dev,loader,filesys,file)
    *  
    *  Add an autoboot method to the table.
    *  This is typically called in the board_finalinit one or more
    *  times to provide a list of bootstrap methods to try for autoboot
    *  
    *  Input parameters: 
    *  	   type - CFE_AUTOBOOT_xxx (disk,network,raw)
    *  	   flags - loader flags (LOADFLG_xxx)
    *  	   loader - loader string (elf, raw, srec)
    *  	   filesys - file system string (raw, tftp, fat)
    *  	   file - name of file to load (if null, will try to guess)
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int cfe_add_autoboot(int type,int flags,char *dev,char *loader,char *filesys,char *file)
{
    cfe_autoboot_method_t *method;

    method = (cfe_autoboot_method_t *) KMALLOC(sizeof(cfe_autoboot_method_t),0);

    if (!method) return CFE_ERR_NOMEM;

    method->ab_type = type;
    method->ab_flags = flags;
    method->ab_dev = dev;
    method->ab_loader = loader;
    method->ab_filesys = filesys;
    method->ab_file = file;

    q_enqueue(&cfe_autoboot_list,(queue_t *)method);
    return 0;
}
#endif /* CFG_AUTOBOOT */
