/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IDE disk driver				File: dev_ide.h
    *  
    *  Probe constants for the IDE disk device.  Various flags
    *  can be passed to the probe routine to configure various
    *  things.  This is where they are defined.
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

/*
 * Use the macros below to set the type of the master and slave devices
 * on the IDE bus.  If you want automatic probing, then you need
 * to specify that IDE_DEV_TYPE_AUTO as the device type.  
 */




#define IDE_PROBE_TYPE_MASK	0x0F
#define IDE_PROBE_MASTER_SHIFT	0
#define IDE_PROBE_SLAVE_SHIFT	4

#define IDE_PROBE_MASTER_TYPE(x) ((x) << IDE_PROBE_MASTER_SHIFT)
#define IDE_PROBE_SLAVE_TYPE(x) ((x) << IDE_PROBE_SLAVE_SHIFT)

#define IDE_PROBE_GET_TYPE(pb,unit) (((pb) >> (unit*4)) & IDE_PROBE_TYPE_MASK)

/*
 * Device types.
 */

#define IDE_DEVTYPE_NOPROBE	0	/* none */
#define IDE_DEVTYPE_AUTO	0x0F	/* automatically probe */

#define IDE_DEVTYPE_DISK	1	/* hard drives */
#define IDE_DEVTYPE_CDROM	2	/* CD-ROMs */
#define IDE_DEVTYPE_ATAPIDISK	3	/* ZIP disks, etc. */
