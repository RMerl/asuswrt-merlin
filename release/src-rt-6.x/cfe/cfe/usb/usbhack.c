/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Main Module				File: usbhack.c       
    *  
    *  A crude test program to let us tinker with a USB controller
    *  installed in an X86 Linux PC.  Eventually we'll clean up
    *  this stuff and incorporate it into CFE.
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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <memory.h>
#include <time.h>

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"
#include "ohci.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define OHCI_WRITECSR(softc,x,y) \
    *((volatile uint32_t *) ((softc)->ohci_regs + ((x)/sizeof(uint32_t)))) = (y)
#define OHCI_READCSR(softc,x) \
    *((volatile uint32_t *) ((softc)->ohci_regs + ((x)/sizeof(uint32_t))))

/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern int usb_noisy;
extern int ohcidebug;

extern usbdev_t *usbmass_dev;
extern int usbmass_read_sector(usbdev_t *dev,uint32_t sectornum,uint32_t seccnt,
			uint8_t *buffer);


/*  *********************************************************************
    *  Play Area definitions
    *  
    *  We use /dev/mem to map two areas - the "play" area and the
    *  "device" area.
    *  
    *  The "play" area maps to the upper 1MB of physical memory.
    *  You need to calculate this address yourself based on your system
    *  setup.   If you have 256MB of memory, in your lilo.conf file put
    *  the following line:
    *  
    *       append="mem=255M"
    *  
    *  where '255' is one less than your system memory size.  this
    *  leaves 1MB out of Linux's physical memory map and therefore
    *  leaves it to you to play with.  /dev/mem will map this
    *  uncached using the Pentium MTRR registers, but for playing 
    *  around this will be fine.
    *  
    *  the second area is the "device area" - this is the address
    *  the PCI USB controller's BARs were mapped to.  You can find
    *  this by looking through /proc/pci until you find:
    *  
    *  Bus  0, device   8, function  0:
    *    USB Controller: OPTi Unknown device (rev 16).
    *    Vendor id=1045. Device id=c861.
    *     Medium devsel.  Fast back-to-back capable.  IRQ 3.  
    *     Master Capable.  Latency=32.  
    *  Non-prefetchable 32 bit memory at 0xd9100000 [0xd9100000].
    *  
    *  The 0xd9100000 will probably be different on your system.
    *  
    *  Of course, to make this work you'll need to rebuild the kernel
    *  without USB support, if you're running a recent kernel.
    *  Fortunately(?), I've been using RH 6.2, no USB support there
    *  in the old 2.2 kernels.
    *  
    *  Finally, you'll need to run this program as root.  Even if
    *  you mess with the permissions on /dev/mem, there are additional
    *  checks in the kernel, so you will lose.
    *  
    *  But, the good news is that it works well - I've never crashed
    *  my Linux box, and I can use gdb to debug programs.  
    *  You will NOT be able to use GDB to display things in the
    *  play area - I believe GDB doesn't know how to deal with 
    *  the uncached nature of the memory there.  You can see stuff
    *  in the area by tracing through instructions that read the play
    *  area, and viewing the register contents.
    ********************************************************************* */

#define PLAY_AREA_ADDR (255*1024*1024)		/* EDIT ME */
#define PLAY_AREA_SIZE (1024*1024)
int play_fd = -1;
uint8_t *play_area = MAP_FAILED;

#define DEVICE_AREA_ADDR 0xd9100000		/* EDIT ME */
#define DEVICE_AREA_SIZE 4096
int dev_fd = -1;
uint8_t *device_area = MAP_FAILED;

/*  *********************************************************************
    *  Globals
    ********************************************************************* */


usbbus_t *bus = NULL;
ohci_softc_t *ohci = NULL;
int running = 1;

/*  *********************************************************************
    *  vtop(v)
    *  
    *  Given a virtual address in the play area, return its physical
    *  address.
    *  
    *  Input parameters: 
    *  	   v - virtual address
    *  	   
    *  Return value:
    *  	   physical address
    ********************************************************************* */


uint32_t vtop(void *v)
{
    uint32_t p = (uint32_t) v;

    if (v == 0) return 0;

    p -= (uint32_t) play_area;
    p += PLAY_AREA_ADDR;

    return p;
}

/*  *********************************************************************
    *  ptov(v)
    *  
    *  Given a phyiscal address in the play area, return the virtual
    *  address.
    *  
    *  Input parameters: 
    *  	   p - physical address
    *  	   
    *  	   
    *  Return value:
    *  	   virtual address (void pointer)
    ********************************************************************* */

void *ptov(uint32_t p)
{
    if (p == 0) return 0;

    p -= PLAY_AREA_ADDR;
    p += (uint32_t) play_area;

    return (void *) p;
}

/*  *********************************************************************
    *  mydelay(x)
    *  
    *  delay for 'x' milliseconds.
    *  
    *  Input parameters: 
    *  	   x - milliseconds
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void mydelay(int x)
{
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = x * 1000000;	/* milliseconds */
    nanosleep(&ts,NULL);
}

/*  *********************************************************************
    *  console_log(tmplt,...)
    *  
    *  Display a console log message - this is a CFE function
    *  transplanted here.
    *  
    *  Input parameters: 
    *  	   tmplt - printf string args...
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void console_log(const char *tmplt,...)
{
    char buffer[256];
    va_list marker;

    va_start(marker,tmplt);
    vsprintf(buffer,tmplt,marker);
    va_end(marker);
    printf("%s\n",buffer);
}

/*  *********************************************************************
    *  init_devaccess()
    *  
    *  Open /dev/mem and create the play area
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok, else error
    ********************************************************************* */

int init_devaccess(void)
{
    int idx;

   play_fd = open("/dev/mem",O_RDWR);

    if (play_fd < 0) {
	perror("open");
	return -1;
	}

    dev_fd = open("/dev/mem",O_RDWR | O_SYNC);

    if (dev_fd < 0) {
	perror("open");
	close(play_fd);
	play_fd = -1;
	return -1;
	}

    play_area = mmap(NULL,
		     PLAY_AREA_SIZE,
		     PROT_READ|PROT_WRITE,MAP_SHARED,
		     play_fd,
		     PLAY_AREA_ADDR);

    if (play_area != MAP_FAILED) {
	printf("Play area mapped ok at address %p to %p\n",play_area,play_area+PLAY_AREA_SIZE-1);
	for (idx = 0; idx < PLAY_AREA_SIZE; idx++) {
	    play_area[idx] = 0x55;
	    if (play_area[idx] != 0x55) printf("Offset %x doesn't work\n",idx);
	    play_area[idx] = 0xaa;
	    if (play_area[idx] != 0xaa) printf("Offset %x doesn't work\n",idx);
	    play_area[idx] = 0x0;
	    if (play_area[idx] != 0x0) printf("Offset %x doesn't work\n",idx);
	    }
	}
    else {
	perror("mmap");
	close(play_fd);
	close(dev_fd);
	play_fd = -1;
	dev_fd = -1;
	return -1;
	}

    device_area = mmap(NULL,
		       DEVICE_AREA_SIZE,
		       PROT_READ|PROT_WRITE,MAP_SHARED,
		       dev_fd,
		       DEVICE_AREA_ADDR);

    if (device_area != MAP_FAILED) {
	printf("Device area mapped ok at address %p\n",device_area);
	}
    else {
	perror("mmap");
	munmap(play_area,PLAY_AREA_SIZE);
	play_area = MAP_FAILED;
	close(play_fd);
	close(dev_fd);
	play_fd = -1;
	dev_fd = -1;
	return -1;
	}

    return 0;
}


/*  *********************************************************************
    *  uninit_devaccess()
    *  
    *  Turn off access to the /dev/mem area.  this will also
    *  set the OHCI controller's state to reset if we were playing
    *  with it.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void uninit_devaccess(void)
{
    if (ohci->ohci_regs) {
	OHCI_WRITECSR(ohci,R_OHCI_CONTROL,V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_RESET));
	}

    if (play_area != MAP_FAILED) munmap(play_area,PLAY_AREA_SIZE);
    if (device_area != MAP_FAILED) munmap(device_area,DEVICE_AREA_SIZE);

    if (play_fd > 0) close(play_fd);
    if (dev_fd > 0) close(dev_fd);

    device_area = MAP_FAILED;
    play_area = MAP_FAILED;

    dev_fd = -1;
    play_fd = -1;
}

/*  *********************************************************************
    *  sighandler()
    *  
    *  ^C handler - switch off OHCI controller
    *  
    *  Input parameters: 
    *  	   sig - signal
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void sighandler(int sig)
{
    signal(SIGINT,SIG_DFL);
    printf("Interrupted, controller reset\n");
    if (ohci->ohci_regs) {
	OHCI_WRITECSR(ohci,R_OHCI_CONTROL,V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_RESET));
	}
    running = 0;
}


extern usb_hcdrv_t ohci_driver;

/*  *********************************************************************
    *  xprintf(str)
    *  
    *  Called by lib_malloc, we need to supply it.
    *  
    *  Input parameters: 
    *  	   str - string
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

int xprintf(char *str)
{
    printf("%s",str);
    return 0;
}

/*  *********************************************************************
    *  main(argc,argv)
    *  
    *  Main test program
    *  
    *  Input parameters: 
    *  	   argc,argv - guess.
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


int main(int argc,char *argv[])
{
    int res;
    memstats_t stats;
    uint8_t *buffer;

    /*
     * Parse command line args
     */

    for (res = 1; res < argc; res++) {
	if (strcmp(argv[res],"-o") == 0) ohcidebug++;
	if (strcmp(argv[res],"-u") == 0) usb_noisy++;
	}

    /*
     * Open the play area.
     */

    if (init_devaccess() < 0) {
	printf("Could not map USB controller\n");
	}

    /*
     * Establish signal and exit handlers 
     */

    signal(SIGINT,sighandler);
    atexit(uninit_devaccess);

    /*
     * Initialize a buffer pool to point at the play area.
     * the 'malloc' calls inside our driver will therefore
     * allocate memory suitable for DMA, just like on real
     * hardware.
     */

    KMEMINIT(play_area,PLAY_AREA_SIZE);

    buffer = KMALLOC(512,32);

    printf("-------------------------------------------\n\n");

    /*	
     * Create the OHCI driver instance.
     */


    bus = UBCREATE(&ohci_driver,(void *) device_area);

    /*
     * Hack: retrieve copy of softc for our exception handler
     */

    ohci = (ohci_softc_t *) bus->ub_hwsoftc;

    /*
     * Start the controller.
     */

    res = UBSTART(bus);

    if (res != 0) {
	printf("Could not init hardware\n");
	UBSTOP(bus);
	exit(1);
	}

    /*
     * Init the root hub
     */

    usb_initroot(bus);

    /*
     * Main loop - just call interrupt routine to poll
     */

    while (usbmass_dev== NULL) {
	usb_poll(bus);
	usb_daemon(bus);
	}

    for (res = 0; res < 1000; res++) {
	usbmass_read_sector(usbmass_dev,0,1,buffer);
	}

    printf("----- finished reading all sectors ----\n");

    while (running) {
	usb_poll(bus);
	usb_daemon(bus);
	}

    /*
     * Clean up - get heap statistics to see if we
     * screwed up.
     */

    res = KMEMSTATS(&stats);
    if (res < 0) printf("Warning: heap is not consistent\n");
    else printf("Heap is ok\n");

    exit(0);

}
