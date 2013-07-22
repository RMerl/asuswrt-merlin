/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SWARM toy clock commands			File: ui_toyclock.c
    *  
    *  time-of-year clock
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
#include "cfe_timer.h"
#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_toyclockcmds(void);

static int ui_cmd_showtime(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_settime(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_setdate(ui_cmdline_t *cmd,int argc,char *argv[]);

/*  *********************************************************************
    *  ui_init_toyclockcmds()
    *  
    *  Add toy clock commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */
int ui_init_toyclockcmds(void)
{

    cmd_addcmd("show time",
	       ui_cmd_showtime,
	       NULL,
	       "Display current time according to RTC",
	       "show time",
	       "");

    cmd_addcmd("set time",
	       ui_cmd_settime,
	       NULL,
	       "Set current time",
	       "set time hh:mm:ss",
	       "");

    cmd_addcmd("set date",
	       ui_cmd_setdate,
	       NULL,
	       "Set current date",
	       "set date mm/dd/yyyy",
	       "");

    return 0;
}

/*  *********************************************************************
    *  User interface commands
    ********************************************************************* */

static int ui_cmd_settime(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *line;
    char *p;
    int hr,min,sec;
    int fd;
    uint8_t buf[12];
    int res=0;

    if ((line = cmd_getarg(cmd,0)) == NULL) {
	return ui_showusage(cmd);
	}

    /* convert colons to spaces for the gettoken routine */
    while ((p = strchr(line,':'))) *p = ' ';

    /* parse and check command-line args */
    hr = -1; min = -1; sec = -1;

    p = gettoken(&line);
    if (p) hr = atoi(p);
    p = gettoken(&line);
    if (p) min = atoi(p);
    p = gettoken(&line);
    if (p) sec = atoi(p);

    if ((hr < 0) || (hr > 23) ||
	(min < 0) || (min >= 60) ||
	(sec < 0) || (sec >= 60)) {
	return ui_showusage(cmd);
	}

    /*
     * hour-minute-second-month-day-year1-year2-(time/date flag)
     * time/date flag (offset 7) is used to let device know what
     * is being set.
     */
    buf[0] = hr;
    buf[1] = min;
    buf[2] = sec;
    buf[7] = 0x00;	/*SET_TIME = 0x00, SET_DATE = 0x01*/

    fd = cfe_open("clock0");
    if (fd < 0) {
	ui_showerror(fd,"could not open clock device");
	return fd;
	}
    
    res = cfe_write(fd,buf,8);
    if (res < 0) {
	ui_showerror(res,"could not set time");
	}
    cfe_close(fd);
    return 0;
}

static int ui_cmd_setdate(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *line;
    char *p;
    int dt,mo,yr,y2k;
    int fd;
    uint8_t buf[12];
    int res=0;

    if ((line = cmd_getarg(cmd,0)) == NULL) {
	return ui_showusage(cmd);
	}

    /* convert colons to spaces for the gettoken routine */

    while ((p = strchr(line,'/'))) *p = ' ';

    /* parse and check command-line args */

    dt = -1; mo = -1; yr = -1;

    p = gettoken(&line);
    if (p) mo = atoi(p);
    p = gettoken(&line);
    if (p) dt = atoi(p);
    p = gettoken(&line);
    if (p) yr = atoi(p);

    if ((mo <= 0) || (mo > 12) ||
	(dt <= 0) || (dt > 31) ||
	(yr < 1900) || (yr > 2099)) {
	return ui_showusage(cmd);
	}

    y2k = (yr >= 2000) ? 0x20 : 0x19;
    yr %= 100;

    /*
     * hour-minute-second-month-day-year1-year2-(time/date flag)
     * time/date flag (offset 7) is used to let device know what
     * is being set.
     */
    buf[3] = mo;
    buf[4] = dt;
    buf[5] = yr;
    buf[6] = y2k;
    buf[7] = 0x01; 	/*SET_TIME = 0x00, SET_DATE = 0x01*/

    fd = cfe_open("clock0");
    if (fd < 0) {
	ui_showerror(fd,"could not open clock device");
	return fd;
	}

    res = cfe_write(fd,buf,8);
    if (res < 0) {
	ui_showerror(res,"could not set date");
	}

    cfe_close(fd);
    return 0;
}


static int ui_cmd_showtime(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint8_t hr,min,sec;
    uint8_t mo,day,yr,y2k;
    int res=0;
    int fd;
    uint8_t buf[12];
    
    fd = cfe_open("clock0");
    if (fd < 0) {
	ui_showerror(fd,"could not open clock device");
	return fd;
	}
    res = cfe_read(fd,buf,8);
    if (res < 0) {
	ui_showerror(res,"could not get time/date");
	}
    cfe_close(fd);

    hr = buf[0];
    min = buf[1];
    sec = buf[2];
    mo = buf[3];
    day = buf[4];
    yr = buf[5];
    y2k = buf[6];
    
    printf("Current date & time is: ");
    printf("%02X/%02X/%02X%02X  %02X:%02X:%02X\n",mo,day,y2k,yr,hr,min,sec);

    return 0;
}
