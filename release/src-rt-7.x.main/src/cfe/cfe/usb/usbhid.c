/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Human Interface Driver		File: usbhid.c
    *  
    *  This module deals with keyboards, mice, etc.  It's very simple,
    *  and only the "boot protocol" is supported.
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


#ifndef _CFE_
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <stdint.h>
#include "usbhack.h"
#else
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "cfe_console.h"
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define HID_BOOT_PROTOCOL	0
#define HID_REPORT_PROTOCOL	1

#define HID_DEVTYPE_UNKNOWN	0
#define HID_DEVTYPE_KBD		1
#define HID_DEVTYPE_MOUSE	2
#define HID_DEVTYPE_MAX		2

#define UBR_KBD_MODS	0
#define UBR_KBD_RSVD	1
#define UBR_KBD_KEYS	2
#define UBR_KBD_NUMKEYS	6
#define UBR_KBD_MAX	8

#define KBD_MOD_LCTRL	0x01
#define KBD_MOD_LSHIFT	0x02
#define KBD_MOD_LALT	0x04
#define KBD_MOD_LWIN	0x08

#define KBD_MOD_RCTRL	0x10
#define KBD_MOD_RSHIFT	0x20
#define KBD_MOD_RALT	0x40
#define KBD_MOD_RWIN	0x80

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define usbhid_set_protocol(dev,protocol,ifc) \
        usb_simple_request(dev,0x21,0x0B,0,ifc)


/*  *********************************************************************
    *  Forward Definitions
    ********************************************************************* */

static int usbhid_attach(usbdev_t *dev,usb_driver_t *drv);
static int usbhid_detach(usbdev_t *dev);

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct usbhid_softc_s {
    int uhid_ipipe;
    int uhid_ipipemps;
    int uhid_devtype;
    uint8_t uhid_imsg[UBR_KBD_MAX];
    uint8_t uhid_lastmsg[UBR_KBD_MAX];
    uint32_t uhid_shiftflags;
} usbhid_softc_t;

usb_driver_t usbhid_driver = {
    "Human-Interface Device",
    usbhid_attach,
    usbhid_detach
};

static char *usbhid_devtypes[3] = {
    "Unknown",
    "Keyboard",
    "Mouse"};

#ifdef CFG_VGACONSOLE
extern int pcconsole_enqueue(uint8_t ch);
#endif


/*  *********************************************************************
    *  Constants for keyboard table
    ********************************************************************* */

#define FLG_NUM             0x0001  /* Toggles: same as bits for LEDs */
#define FLG_CAPS            0x0002  
#define FLG_SCROLL          0x0004
#define FLG_SHIFT           0x0008  /* Shifts */
#define FLG_CTRL            0x0100  
#define FLG_ALT             0x0200  
#define FLG_FKEY            0x0400  /* function keys */
#define FLG_NKPD            0x0800  /* numeric keypad */ 
#define FLG_ASCII           0x1000  /* regular ASCII character */
#define FLG_NONE            0x2000  


/*  *********************************************************************
    *  Structures for keyboard table
    ********************************************************************* */

#define KC_RESPLEN 4
typedef struct keycode_s {
    int         kc_type;
    char        kc_normal[KC_RESPLEN];
    char        kc_shifted[KC_RESPLEN];
    char        kc_ctrl[KC_RESPLEN];
} keycode_t;


/*  *********************************************************************
    *  Scan code conversion table
    ********************************************************************* */

static keycode_t usbhid_scantable[] = {
    { FLG_NONE,   "",             "",             "" },           /* 0 */
    { FLG_NONE,   "",             "",             "" },           /* 1 */
    { FLG_NONE,   "",             "",             "" },           /* 2 */
    { FLG_NONE,   "",             "",             "" },           /* 3 */
    { FLG_ASCII,  "a",            "A",            "\001" },       /* 4 a */
    { FLG_ASCII,  "b",            "B",            "\002" },       /* 5 b */
    { FLG_ASCII,  "c",            "C",            "\003" },       /* 6 c */
    { FLG_ASCII,  "d",            "D",            "\004" },       /* 7 d */
    { FLG_ASCII,  "e",            "E",            "\005" },       /* 8 e */
    { FLG_ASCII,  "f",            "F",            "\006" },       /* 9 f */
    { FLG_ASCII,  "g",            "G",            "\007" },       /* 10 g */
    { FLG_ASCII,  "h",            "H",            "\010" },       /* 11 h */
    { FLG_ASCII,  "i",            "I",            "\011" },       /* 12 i */
    { FLG_ASCII,  "j",            "J",            "\n" },         /* 13 j */
    { FLG_ASCII,  "k",            "K",            "\013" },       /* 14 k */
    { FLG_ASCII,  "l",            "L",            "\014" },       /* 15 l */
    { FLG_ASCII,  "m",            "M",            "\r" },         /* 16 m */
    { FLG_ASCII,  "n",            "N",            "\016" },       /* 17 n */
    { FLG_ASCII,  "o",            "O",            "\017" },       /* 18 o */
    { FLG_ASCII,  "p",            "P",            "\020" },       /* 19 p */
    { FLG_ASCII,  "q",            "Q",            "\021" },       /* 20 q */
    { FLG_ASCII,  "r",            "R",            "\022" },       /* 21 r */
    { FLG_ASCII,  "s",            "S",            "\023" },       /* 22 s */
    { FLG_ASCII,  "t",            "T",            "\024" },       /* 23 t */
    { FLG_ASCII,  "u",            "U",            "\025" },       /* 24 u */
    { FLG_ASCII,  "v",            "V",            "\026" },       /* 25 v */
    { FLG_ASCII,  "w",            "W",            "\027" },       /* 26 w */
    { FLG_ASCII,  "x",            "X",            "\030" },       /* 27 x */
    { FLG_ASCII,  "y",            "Y",            "\031" },       /* 28 y */
    { FLG_ASCII,  "z",            "Z",            "\032" },       /* 29 z */

    { FLG_ASCII,  "1",            "!",            "!" },          /* 30 1 */
    { FLG_ASCII,  "2",            "@",            "\000" },       /* 31 2 */
    { FLG_ASCII,  "3",            "#",            "#" },          /* 32 3 */
    { FLG_ASCII,  "4",            "$",            "$" },          /* 33 4 */
    { FLG_ASCII,  "5",            "%",            "%" },          /* 34 5 */
    { FLG_ASCII,  "6",            "^",            "\036" },       /* 35 6 */
    { FLG_ASCII,  "7",            "&",            "&" },          /* 36 7 */
    { FLG_ASCII,  "8",            "*",            "\010" },       /* 37 8 */
    { FLG_ASCII,  "9",            "(",            "(" },          /* 38 9 */
    { FLG_ASCII,  "0",            ")",            ")" },          /* 39 0 */

    { FLG_ASCII,  "\r",           "\r",           "\n" },         /* 40 ENT */
    { FLG_ASCII,  "\033",         "\033",         "\033" },       /* 41 ESC */
    { FLG_ASCII,  "\177",         "\177",         "\010" },       /* 42 <- */
    { FLG_ASCII,  "\t",           "\177\t",       "\t" },         /* 43 ->| */
    { FLG_ASCII,  " ",            " ",            "\000" },       /* 44 SPC */

    { FLG_ASCII,  "-",            "_",            "\037" },       /* 45 - */
    { FLG_ASCII,  "=",            "+",            "+" },          /* 46 = */
    { FLG_ASCII,  "[",            "{",            "\033" },       /* 47 [ */
    { FLG_ASCII,  "]",            "}",            "\035" },       /* 48 ] */
    { FLG_ASCII,  "\\",           "|",            "\034" },       /* 49 \ */

    { FLG_NONE,   "",             "",             "" },           /* 50 pound */

    { FLG_ASCII,  ";",            ":",            ";" },          /* 51 ; */
    { FLG_ASCII,  "'",            "\"",           "'" },          /* 52 ' */
    { FLG_ASCII,  "`",            "~",            "`" },          /* 53 ` */
    { FLG_ASCII,  ",",            "<",            "<" },          /* 54 , */
    { FLG_ASCII,  ".",            ">",            ">" },          /* 55 . */
    { FLG_ASCII,  "/",            "?",            "\037" },       /* 56 / */
    { FLG_CAPS,   "",             "",             "" },           /* 57 CAPS */

    { FLG_FKEY,   "\033[M",       "\033[Y",       "\033[k" },     /* 58 f1 */
    { FLG_FKEY,   "\033[N",       "\033[Z",       "\033[l" },     /* 59 f2 */
    { FLG_FKEY,   "\033[O",       "\033[a",       "\033[m" },     /* 60 f3 */
    { FLG_FKEY,   "\033[P",       "\033[b",       "\033[n" },     /* 61 f4 */
    { FLG_FKEY,   "\033[Q",       "\033[c",       "\033[o" },     /* 62 f5 */
    { FLG_FKEY,   "\033[R",       "\033[d",       "\033[p" },     /* 63 f6 */
    { FLG_FKEY,   "\033[S",       "\033[e",       "\033[q" },     /* 64 f7 */
    { FLG_FKEY,   "\033[T",       "\033[f",       "\033[r" },     /* 65 f8 */
    { FLG_FKEY,   "\033[U",       "\033[g",       "\033[s" },     /* 66 f9 */
    { FLG_FKEY,   "\033[V",       "\033[h",       "\033[t" },     /* 67 f10 */
    { FLG_FKEY,   "\033[W",       "\033[i",       "\033[u" },     /* 68 f11 */
    { FLG_FKEY,   "\033[X",       "\033[j",       "\033[v" },     /* 69 f12 */

    { FLG_NONE,   "",             "",             "" },           /* 70 prtsc */
    { FLG_SCROLL, "",             "",             "" },           /* 71 SCRLK  */
    { FLG_NONE,   "",             "",             "" },           /* 72 pause */
    { FLG_NONE,   "",             "",             "" },           /* 73 KPins */
    { FLG_NONE,   "",             "",             "" },           /* 74 KPhome */
    { FLG_NONE,   "",             "",             "" },           /* 75 KPpgup */
    { FLG_NONE,   "",             "",             "" },           /* 76 KPdel */
    { FLG_NONE,   "",             "",             "" },           /* 77 KPend */
    { FLG_NONE,   "",             "",             "" },           /* 78 KPpgdn */

    { FLG_FKEY,   "\033[C",       "",             "" },           /* 79 KPright */
    { FLG_FKEY,   "\033[D",       "",             "" },           /* 80 KPleft */
    { FLG_FKEY,   "\033[B",       "",             "" },           /* 81 KPdown */
    { FLG_FKEY,   "\033[A",       "",             "" },           /* 82 KPup */

    { FLG_NUM,    "",             "",             "" },           /* 83 NUMLK */
    { FLG_NKPD,   "/",            "/",            "/" },          /* 84 KP/ */
    { FLG_NKPD,   "*",            "*",            "*" },          /* 85 KP* */
    { FLG_NKPD,   "-",            "-",            "-" },          /* 86 KP- */
    { FLG_NKPD,   "+",            "+",            "+" },          /* 87 KP+ */
    { FLG_NKPD,   "\r",           "\r",           "\n" },         /* 88 KPent */

    { FLG_NKPD,   "1",            "\033[F",       "1" },          /* 89 KP1 */
    { FLG_NKPD,   "2",            "\033[B",       "2" },          /* 90 KP2 */
    { FLG_NKPD,   "3",            "\033[G",       "3" },          /* 91 KP3 */
    { FLG_NKPD,   "4",            "\033[D",       "4" },          /* 92 KP4 */
    { FLG_NKPD,   "5",            "\033[E",       "5" },          /* 93 KP5 */
    { FLG_NKPD,   "6",            "\033[C",       "6" },          /* 94 KP6 */
    { FLG_NKPD,   "7",            "\033[H",       "7" },          /* 95 KP7 */
    { FLG_NKPD,   "8",            "\033[A",       "8" },          /* 96 KP8 */
    { FLG_NKPD,   "9",            "\033[I",       "9" },          /* 97 KP9 */
    { FLG_NKPD,   "0",            "\033[L",       "0" },          /* 98 KP0 */

    { FLG_NKPD,   ".",            "\177",         "." },          /* 99 KP. */

    { FLG_NONE,   "",             "",             "" },           /* 100 non\ */

};

#define usbhid_scantablesize (sizeof(usbhid_scantable)/sizeof(keycode_t))


/*  *********************************************************************
    *  usbhid_kbd_mod1(uhid)
    *  
    *  Process modifier key changes for the current USB event,
    *  which was stored in uhid_imsg.  Basically all this does
    *  is update uhid_shiftflags, converting the bits into the ones
    *  we use in our keyboard table.
    *  
    *  Input parameters: 
    *  	   uhid - the hid softc.
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbhid_kbd_mod1(usbhid_softc_t *uhid)
{
    uint8_t changed;
    uint8_t mod;

    /*
     * See if anything changed. 
     */

    changed = (uhid->uhid_imsg[UBR_KBD_MODS] ^ uhid->uhid_lastmsg[UBR_KBD_MODS]);
    if (changed == 0) return;

    /*
     * Something changed.  Reflect changes in our local copy of the
     * shift state.
     */

    mod = uhid->uhid_imsg[UBR_KBD_MODS];

    uhid->uhid_shiftflags &= ~(FLG_SHIFT|FLG_ALT|FLG_CTRL);

    if (mod & (KBD_MOD_LCTRL|KBD_MOD_RCTRL))   uhid->uhid_shiftflags |= FLG_CTRL;
    if (mod & (KBD_MOD_LSHIFT|KBD_MOD_RSHIFT)) uhid->uhid_shiftflags |= FLG_SHIFT;
    if (mod & (KBD_MOD_LALT|KBD_MOD_RALT)) uhid->uhid_shiftflags |= FLG_ALT;
}

/*  *********************************************************************
    *  usbhid_kbd_scan1(uhid,scan,breakflg)
    *  
    *  Handle a single keyboard event.  Using the scan code, look up
    *  the key in the table and convert it to one or more characters
    *  for the keyboard event queue.
    *  
    *  Input parameters: 
    *  	   uhid - the hid softc
    *  	   scan - scan code from keyboard report 
    *  	   breakflg - true if key is being released, false if pressed
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbhid_kbd_scan1(usbhid_softc_t *uhid,uint8_t scan,int breakflg)
{
    keycode_t *code = 0;
    char *str;

    /*
     * Check scan code for reality.  
     */

    if (scan >= usbhid_scantablesize) return;
    code = &usbhid_scantable[scan];

    /*
     * If the change is a toggle, handle the toggle.  These 
     * keys also deal with the LEDs on the keyboard.
     */

    if (code->kc_type & (FLG_CAPS|FLG_SCROLL|FLG_NUM)) {
	if (!breakflg) uhid->uhid_shiftflags ^= code->kc_type;
//	if (ks->ks_setleds) {
//	    (*(ks->ks_setleds))(ks,ks->ks_shiftflags & (FLG_CAPS|FLG_SCROLL|FLG_NUM));
//	    }
	}

    /*
     * Regular keys - just look up in table and
     * queue the characters to the upper layers.
     */

    if (code->kc_type & (FLG_ASCII | FLG_FKEY | FLG_NKPD)) {
	if (uhid->uhid_shiftflags & (FLG_SHIFT|FLG_CAPS)) str = code->kc_shifted;
	else if (uhid->uhid_shiftflags & FLG_CTRL) str = code->kc_ctrl;
	else str = code->kc_normal;
	if (!breakflg) {
#if CFG_VGACONSOLE
	    while (*str) {
		pcconsole_enqueue(*str++);
		}
#else
	    printf("%s",str); 
#endif
#ifndef _CFE_
	    fflush(stdout);
#endif
	    }
	}

}


/*  *********************************************************************
    *  usbhid_kbd_scan(uhid)
    *  
    *  Main processing routine for keyboard report messages.  Once
    *  we've determined that it is a keyboard mesage, we end up
    *  here.  The work involves seeing what new keys have arrived
    *  in the list (presses), and which ones are no longer there 
    *  (releases).  To do this, we us the current and previous
    *  report structure.
    *  
    *  Input parameters: 
    *  	   uhid - the hid softc
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbhid_kbd_scan(usbhid_softc_t *uhid)
{
    int n,o;

    /*
     * Modifier keys (shift, alt, control)
     */

    if (uhid->uhid_imsg[UBR_KBD_MODS] ^ uhid->uhid_lastmsg[UBR_KBD_MODS]) {
	usbhid_kbd_mod1(uhid);
	}

    /*
     * "Make" codes (keys pressed down)
     * Look for keys in 'uhid_imsg' that are not in 'uhid_lastmsg'
     */

    for (n = UBR_KBD_KEYS; n < (UBR_KBD_KEYS + UBR_KBD_NUMKEYS); n++) {
	if (uhid->uhid_imsg[n] == 0) break;	/* no more keys */
	for (o = UBR_KBD_KEYS; o < (UBR_KBD_KEYS + UBR_KBD_NUMKEYS); o++) {
	    if (uhid->uhid_imsg[n] == uhid->uhid_lastmsg[o]) break;
	    }
	if (o == (UBR_KBD_KEYS + UBR_KBD_NUMKEYS)) {	/* key not found, must be pressed */
	    usbhid_kbd_scan1(uhid,uhid->uhid_imsg[n],0);
	    }
	}

    /*
     * "Break" codes (keys released)
     * Look for keys in 'uhid_lastmsg' that are not in 'uhid_imsg'
     */


    for (n = UBR_KBD_KEYS; n < (UBR_KBD_KEYS + UBR_KBD_NUMKEYS); n++) {
	if (uhid->uhid_lastmsg[n] == 0) break;	/* no more keys */
	for (o = UBR_KBD_KEYS; o < (UBR_KBD_KEYS + UBR_KBD_NUMKEYS); o++) {
	    if (uhid->uhid_lastmsg[n] == uhid->uhid_imsg[o]) break;
	    }
	if (o == (UBR_KBD_KEYS + UBR_KBD_NUMKEYS)) {	/* key not found, must be released */
	    usbhid_kbd_scan1(uhid,uhid->uhid_lastmsg[n],1);
	    }
	}
}


/*  *********************************************************************
    *  usbhid_ireq_callback(ur)
    *  
    *  This routine is called when our interrupt transfer completes
    *  and there is report data to be processed.
    *  
    *  Input parameters: 
    *  	   ur - usb request
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int usbhid_ireq_callback(usbreq_t *ur)
{
    usbhid_softc_t *uhid = (ur->ur_dev->ud_private);

    /*
     * If the driver is unloaded, the request will be cancelled.
     */

    if (ur->ur_status == 0xFF) {
	usb_free_request(ur);
	return 0;
	}

    /*
     * What we do now depends on the type of device.
     */

    switch (uhid->uhid_devtype) {
	case HID_DEVTYPE_KBD:
	    /*
	     * Handle keyboard event
	     */
	    usbhid_kbd_scan(uhid);

	    /*
	     * Save old event to compare for next time.
	     */
	    memcpy(uhid->uhid_lastmsg,uhid->uhid_imsg,8);
	    break;

	case HID_DEVTYPE_MOUSE:
	    break;
	}

    /*
     * Re-queue request to get next keyboard event.
     */

    usb_queue_request(ur);

    return 0;
}


/*  *********************************************************************
    *  usbhid_queue_intreq(dev,softc)
    *  
    *  Queue an interrupt request for this usb device.  The
    *  driver will place this request on the queue that corresponds
    *  to the endpoint, and will call the callback routine when
    *  something happens.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   softc - the usb hid softc
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbhid_queue_intreq(usbdev_t *dev,usbhid_softc_t *softc)
{
    usbreq_t *ur;

    ur = usb_make_request(dev,
			  softc->uhid_ipipe,
			  softc->uhid_imsg,softc->uhid_ipipemps,
			  UR_FLAG_IN);

    ur->ur_callback = usbhid_ireq_callback;

    usb_queue_request(ur);
}


/*  *********************************************************************
    *  usbhid_attach(dev,drv)
    *  
    *  This routine is called when the bus scan stuff finds a HID
    *  device.  We finish up the initialization by configuring the
    *  device and allocating our softc here.
    *  
    *  Input parameters: 
    *  	   dev - usb device, in the "addressed" state.
    *  	   drv - the driver table entry that matched
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int usbhid_attach(usbdev_t *dev,usb_driver_t *drv)
{
    usb_config_descr_t *cfgdscr = dev->ud_cfgdescr;
    usb_endpoint_descr_t *epdscr;
    usb_interface_descr_t *ifdscr;
    usbhid_softc_t *softc;

    dev->ud_drv = drv;

    softc = KMALLOC(sizeof(usbhid_softc_t),0);
    memset(softc,0,sizeof(usbhid_softc_t));
    dev->ud_private = softc;

    epdscr = usb_find_cfg_descr(dev,USB_ENDPOINT_DESCRIPTOR_TYPE,0);
    ifdscr = usb_find_cfg_descr(dev,USB_INTERFACE_DESCRIPTOR_TYPE,0);

    if (!epdscr || !ifdscr) {
	/*
	 * Could not get descriptors, something is very wrong.
	 * Leave device addressed but not configured.
	 */
	return 0;
	}

    /*
     * Choose the standard configuration.
     */

    usb_set_configuration(dev,cfgdscr->bConfigurationValue);

    /*
     * Set the protocol to the "boot" protocol, so we don't
     * have to deal with fancy HID stuff.
     */

    usbhid_set_protocol(dev,HID_BOOT_PROTOCOL,ifdscr->bInterfaceNumber);

    /*
     * Open the interrupt pipe.
     */

    softc->uhid_ipipe = usb_open_pipe(dev,epdscr);
    softc->uhid_ipipemps = GETUSBFIELD(epdscr,wMaxPacketSize);

    /*
     * Figure out the device type from the protocol.  Keyboards,
     * mice use this field to distinguish themselves.
     */

    softc->uhid_devtype = ifdscr->bInterfaceProtocol;
    if (softc->uhid_devtype > HID_DEVTYPE_MAX) {
	softc->uhid_devtype = HID_DEVTYPE_UNKNOWN;
	}

    console_log("USBHID: %s Configured.\n",
	   usbhid_devtypes[softc->uhid_devtype]);

    /*
     * Queue a transfer on the interrupt endpoint to catch
     * our first characters.
     */

    usbhid_queue_intreq(dev,softc);

    return 0;
}

/*  *********************************************************************
    *  usbhid_detach(dev)
    *  
    *  This routine is called when the bus scanner notices that
    *  this device has been removed from the system.  We should
    *  do any cleanup that is required.  The pending requests
    *  will be cancelled automagically.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int usbhid_detach(usbdev_t *dev)
{
    return 0;
}
 
