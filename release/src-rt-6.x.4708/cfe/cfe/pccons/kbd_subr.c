/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PC-style keyboard interface		File: KBD_SUBR.C
    *  
    *  This module converts a stream of scancodes into ASCII
    *  characters.  The scan codes come from a PC-style
    *  keyboard.
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

#include "kbd_subr.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define FLG_SCROLL          0x0001  /* Toggles: same as bit positions for LEDs! */
#define FLG_NUM             0x0002  
#define FLG_CAPS            0x0004  
#define FLG_SHIFT           0x0008  /* Shifts */
#define FLG_CTRL            0x0100  
#define FLG_ALT             0x0200  
#define FLG_FKEY            0x0400  /* function keys */
#define FLG_NKPD            0x0800  /* numeric keypad */ 
#define FLG_ASCII           0x1000  /* regular ASCII character */
#define FLG_NONE            0x2000  
#define FLG_BREAKBIT        0x80    


/*  *********************************************************************
    *  Structures
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

static keycode_t scantable[] = {
    { FLG_NONE,   "",             "",             "" },           /* 0 */
    { FLG_ASCII,  "\033",         "\033",         "\033" },       /* 1 ESC */
    { FLG_ASCII,  "1",            "!",            "!" },          /* 2 1 */
    { FLG_ASCII,  "2",            "@",            "\000" },       /* 3 2 */
    { FLG_ASCII,  "3",            "#",            "#" },          /* 4 3 */
    { FLG_ASCII,  "4",            "$",            "$" },          /* 5 4 */
    { FLG_ASCII,  "5",            "%",            "%" },          /* 6 5 */
    { FLG_ASCII,  "6",            "^",            "\036" },       /* 7 6 */
    { FLG_ASCII,  "7",            "&",            "&" },          /* 8 7 */
    { FLG_ASCII,  "8",            "*",            "\010" },       /* 9 8 */
    { FLG_ASCII,  "9",            "(",            "(" },          /* 10 9 */
    { FLG_ASCII,  "0",            ")",            ")" },          /* 11 0 */
    { FLG_ASCII,  "-",            "_",            "\037" },       /* 12 - */
    { FLG_ASCII,  "=",            "+",            "+" },          /* 13 = */
    { FLG_ASCII,  "\177",         "\177",         "\010" },       /* 14 <- */
    { FLG_ASCII,  "\t",           "\177\t",       "\t" },         /* 15 ->| */
    { FLG_ASCII,  "q",            "Q",            "\021" },       /* 16 q */
    { FLG_ASCII,  "w",            "W",            "\027" },       /* 17 w */
    { FLG_ASCII,  "e",            "E",            "\005" },       /* 18 e */
    { FLG_ASCII,  "r",            "R",            "\022" },       /* 19 r */
    { FLG_ASCII,  "t",            "T",            "\024" },       /* 20 t */
    { FLG_ASCII,  "y",            "Y",            "\031" },       /* 21 y */
    { FLG_ASCII,  "u",            "U",            "\025" },       /* 22 u */
    { FLG_ASCII,  "i",            "I",            "\011" },       /* 23 i */
    { FLG_ASCII,  "o",            "O",            "\017" },       /* 24 o */
    { FLG_ASCII,  "p",            "P",            "\020" },       /* 25 p */
    { FLG_ASCII,  "[",            "{",            "\033" },       /* 26 [ */
    { FLG_ASCII,  "]",            "}",            "\035" },       /* 27 ] */
    { FLG_ASCII,  "\r",           "\r",           "\n" },         /* 28 ENT */
    { FLG_CTRL,    "",             "",             "" },          /* 29 CTRL */
    { FLG_ASCII,  "a",            "A",            "\001" },       /* 30 a */
    { FLG_ASCII,  "s",            "S",            "\023" },       /* 31 s */
    { FLG_ASCII,  "d",            "D",            "\004" },       /* 32 d */
    { FLG_ASCII,  "f",            "F",            "\006" },       /* 33 f */
    { FLG_ASCII,  "g",            "G",            "\007" },       /* 34 g */
    { FLG_ASCII,  "h",            "H",            "\010" },       /* 35 h */
    { FLG_ASCII,  "j",            "J",            "\n" },         /* 36 j */
    { FLG_ASCII,  "k",            "K",            "\013" },       /* 37 k */
    { FLG_ASCII,  "l",            "L",            "\014" },       /* 38 l */
    { FLG_ASCII,  ";",            ":",            ";" },          /* 39 ; */
    { FLG_ASCII,  "'",            "\"",           "'" },          /* 40 ' */
    { FLG_ASCII,  "`",            "~",            "`" },          /* 41 ` */
    { FLG_SHIFT,  "",             "",             "" },           /* 42 SHIFT */
    { FLG_ASCII,  "\\",           "|",            "\034" },       /* 43 \ */
    { FLG_ASCII,  "z",            "Z",            "\032" },       /* 44 z */
    { FLG_ASCII,  "x",            "X",            "\030" },       /* 45 x */
    { FLG_ASCII,  "c",            "C",            "\003" },       /* 46 c */
    { FLG_ASCII,  "v",            "V",            "\026" },       /* 47 v */
    { FLG_ASCII,  "b",            "B",            "\002" },       /* 48 b */
    { FLG_ASCII,  "n",            "N",            "\016" },       /* 49 n */
    { FLG_ASCII,  "m",            "M",            "\r" },         /* 50 m */
    { FLG_ASCII,  ",",            "<",            "<" },          /* 51 , */
    { FLG_ASCII,  ".",            ">",            ">" },          /* 52 . */
    { FLG_ASCII,  "/",            "?",            "\037" },       /* 53 / */
    { FLG_SHIFT,  "",             "",             "" },           /* 54 SHIFT */
    { FLG_NKPD,   "*",            "*",            "*" },          /* 55 KP* */
    { FLG_ALT,    "",             "",             "" },           /* 56 ALT */
    { FLG_ASCII,  " ",            " ",            "\000" },       /* 57 SPC */
    { FLG_CAPS,   "",             "",             "" },           /* 58 CAPS */
    { FLG_FKEY,   "\033[M",       "\033[Y",       "\033[k" },     /* 59 f1 */
    { FLG_FKEY,   "\033[N",       "\033[Z",       "\033[l" },     /* 60 f2 */
    { FLG_FKEY,   "\033[O",       "\033[a",       "\033[m" },     /* 61 f3 */
    { FLG_FKEY,   "\033[P",       "\033[b",       "\033[n" },     /* 62 f4 */
    { FLG_FKEY,   "\033[Q",       "\033[c",       "\033[o" },     /* 63 f5 */
    { FLG_FKEY,   "\033[R",       "\033[d",       "\033[p" },     /* 64 f6 */
    { FLG_FKEY,   "\033[S",       "\033[e",       "\033[q" },     /* 65 f7 */
    { FLG_FKEY,   "\033[T",       "\033[f",       "\033[r" },     /* 66 f8 */
    { FLG_FKEY,   "\033[U",       "\033[g",       "\033[s" },     /* 67 f9 */
    { FLG_FKEY,   "\033[V",       "\033[h",       "\033[t" },     /* 68 f10 */
    { FLG_NUM,    "",             "",             "" },           /* 69 NUMLK */
    { FLG_SCROLL, "",             "",             "" },           /* 70 SCRLK  */
    { FLG_NKPD,   "7",            "\033[H",       "7" },          /* 71 KP7 */
    { FLG_NKPD,   "8",            "\033[A",       "8" },          /* 72 KP8 */
    { FLG_NKPD,   "9",            "\033[I",       "9" },          /* 73 KP9 */
    { FLG_NKPD,   "-",            "-",            "-" },          /* 74 KP- */
    { FLG_NKPD,   "4",            "\033[D",       "4" },          /* 75 KP4 */
    { FLG_NKPD,   "5",            "\033[E",       "5" },          /* 76 KP5 */
    { FLG_NKPD,   "6",            "\033[C",       "6" },          /* 77 KP6 */
    { FLG_NKPD,   "+",            "+",            "+" },          /* 78 KP+ */
    { FLG_NKPD,   "1",            "\033[F",       "1" },          /* 79 KP1 */
    { FLG_NKPD,   "2",            "\033[B",       "2" },          /* 80 KP2 */
    { FLG_NKPD,   "3",            "\033[G",       "3" },          /* 81 KP3 */
    { FLG_NKPD,   "0",            "\033[L",       "0" },          /* 82 KP0 */
    { FLG_NKPD,   ".",            "\177",         "." },          /* 83 KP. */
    { FLG_NONE,   "",             "",             "" },           /* 84 0 */
    { FLG_NONE,   "100",          "",             "" },           /* 85 0 */
    { FLG_NONE,   "101",          "",             "" },           /* 86 0 */
    { FLG_FKEY,   "\033[W",       "\033[i",       "\033[u" },     /* 87 f11 */
    { FLG_FKEY,   "\033[X",       "\033[j",       "\033[v" },     /* 88 f12 */
    { FLG_NONE,   "102",          "",             "" },           /* 89 0 */
    { FLG_NONE,   "103",          "",             "" },           /* 90 0 */
    { FLG_NONE,   "",             "",             "" },           /* 91 0 */
    { FLG_NONE,   "",             "",             "" },           /* 92 0 */
    { FLG_NONE,   "",             "",             "" },           /* 93 0 */
    { FLG_NONE,   "",             "",             "" },           /* 94 0 */
    { FLG_NONE,   "",             "",             "" },           /* 95 0 */
    { FLG_NONE,   "",             "",             "" },           /* 96 0 */
    { FLG_NONE,   "",             "",             "" },           /* 97 0 */
    { FLG_NONE,   "",             "",             "" },           /* 98 0 */
    { FLG_NONE,   "",             "",             "" },           /* 99 0 */
    { FLG_NONE,   "",             "",             "" },           /* 100 */
    { FLG_NONE,   "",             "",             "" },           /* 101 */
    { FLG_NONE,   "",             "",             "" },           /* 102 */
    { FLG_NONE,   "",             "",             "" },           /* 103 */
    { FLG_NONE,   "",             "",             "" },           /* 104 */
    { FLG_NONE,   "",             "",             "" },           /* 105 */
    { FLG_NONE,   "",             "",             "" },           /* 106 */
    { FLG_NONE,   "",             "",             "" },           /* 107 */
    { FLG_NONE,   "",             "",             "" },           /* 108 */
    { FLG_NONE,   "",             "",             "" },           /* 109 */
    { FLG_NONE,   "",             "",             "" },           /* 110 */
    { FLG_NONE,   "",             "",             "" },           /* 111 */
    { FLG_NONE,   "",             "",             "" },           /* 112 */
    { FLG_NONE,   "",             "",             "" },           /* 113 */
    { FLG_NONE,   "",             "",             "" },           /* 114 */
    { FLG_NONE,   "",             "",             "" },           /* 115 */
    { FLG_NONE,   "",             "",             "" },           /* 116 */
    { FLG_NONE,   "",             "",             "" },           /* 117 */
    { FLG_NONE,   "",             "",             "" },           /* 118 */
    { FLG_NONE,   "",             "",             "" },           /* 119 */
    { FLG_NONE,   "",             "",             "" },           /* 120 */
    { FLG_NONE,   "",             "",             "" },           /* 121 */
    { FLG_NONE,   "",             "",             "" },           /* 122 */
    { FLG_NONE,   "",             "",             "" },           /* 123 */
    { FLG_NONE,   "",             "",             "" },           /* 124 */
    { FLG_NONE,   "",             "",             "" },           /* 125 */
    { FLG_NONE,   "",             "",             "" },           /* 126 */
    { FLG_NONE,   "",             "",             "" },           /* 127 */
};


/*  *********************************************************************
    *  KBD_ENQUEUECHAR(ks,ch)
    *  
    *  Put a character on the queue
    *  
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   ch - character to enqueue
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void kbd_enqueuechar(keystate_t *ks,char ch)
{
    if (((ks->ks_head+1) & (KEYQUEUELEN-1)) == ks->ks_tail) {
	/* queue is full */
	return;
	}
    ks->ks_queue[ks->ks_head] = ch;
    ks->ks_head = (ks->ks_head+1) & (KEYQUEUELEN-1);
}


/*  *********************************************************************
    *  KBD_DEQUEUECHAR(ks)
    *  
    *  Remove a character from the queue
    *  
    *  Input parameters: 
    *  	   ks - keystate
    *  	   
    *  Return value:
    *  	   0 if no characters in queue
    *  	   else character from queue
    ********************************************************************* */
static int kbd_dequeuechar(keystate_t *ks)
{
    char ch;

    if (ks->ks_head == ks->ks_tail) return 0;

    ch = ks->ks_queue[ks->ks_tail];
    ks->ks_tail = (ks->ks_tail+1) & (KEYQUEUELEN-1);
    return ch;
}

/*  *********************************************************************
    *  KBD_READ(ks)
    *  
    *  User call to kbd_dequeuechar - remove a character from
    *  the queue.
    *  	   
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   
    *  Return value:
    *  	   character from queue or 0 if no chars
    ********************************************************************* */

int kbd_read(keystate_t *ks)
{
    return kbd_dequeuechar(ks);
}

/*  *********************************************************************
    *  KBD_INPSTAT(ks)
    *  
    *  Test input status (see if a character is waiting)
    *  
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   
    *  Return value:
    *  	   0 if no chars waiting, 1 if characters are waiting
    ********************************************************************* */

int kbd_inpstat(keystate_t *ks)
{
    return (ks->ks_head != ks->ks_tail);
}


/*  *********************************************************************
    *  KBD_DOSCAN(ks,scan)
    *  
    *  Process a scan code from the keyboard.
    *  
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   scan - scan code from the keyboard
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void kbd_doscan(keystate_t *ks,uint8_t scan)
{
    int breakflg;
    keycode_t *code = 0;
    char *str;

    breakflg = (scan & FLG_BREAKBIT);
    scan &= ~FLG_BREAKBIT;
    code = &scantable[scan];

    if (code->kc_type & (FLG_SHIFT|FLG_CTRL|FLG_ALT)) {
	if (breakflg) ks->ks_shiftflags &= ~code->kc_type;
	else ks->ks_shiftflags |= code->kc_type;
	}
    if (code->kc_type & (FLG_CAPS|FLG_SCROLL|FLG_NUM)) {
	if (!breakflg) ks->ks_shiftflags ^= code->kc_type;
	if (ks->ks_setleds) {
	    (*(ks->ks_setleds))(ks,ks->ks_shiftflags & (FLG_CAPS|FLG_SCROLL|FLG_NUM));
	    }
	}
    if (code->kc_type & (FLG_ASCII | FLG_FKEY | FLG_NKPD)) {
	if (ks->ks_shiftflags & (FLG_SHIFT|FLG_CAPS)) str = code->kc_shifted;
	else if (ks->ks_shiftflags & FLG_CTRL) str = code->kc_ctrl;
	else str = code->kc_normal;
	if (!breakflg) {
	    while (*str) {
		kbd_enqueuechar(ks,*str++);
		}
	    }
	}
}


/*  *********************************************************************
    *  KBD_INIT(ks,setleds,ref)
    *  
    *  Initialize a keyboard state object.  
    *  
    *  Input parameters: 
    *  	   ks - keyboard state
    *  	   setleds - routine to call when we want to set the state
    *  	             of the keyboard's LEDs
    *  	   ref - data to store in the keyboard state object
    *  
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void kbd_init(keystate_t *ks,int (*setleds)(keystate_t *,int),void *ref)
{
    memset(ks,0,sizeof(keystate_t));
    ks->ks_setleds = setleds;
    ks->ks_ref = ref;
}
