/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Character set conversion Extensions
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#include "includes.h"
#define CTRLZ 26
extern int DEBUGLEVEL;

static char cvtbuf[sizeof(pstring)];

static BOOL mapsinited = 0;

static char unix2dos[256];
static char dos2unix[256];

static void initmaps(void) {
    int k;

    for (k = 0; k < 256; k++) unix2dos[k] = k;
    for (k = 0; k < 256; k++) dos2unix[k] = k;

    mapsinited = True;
}

static void update_map(char *str) {
    char *p;

    for (p = str; *p; p++) {
        if (p[1]) {
            unix2dos[(unsigned char)*p] = p[1];
            dos2unix[(unsigned char)p[1]] = *p;
            p++;
        }
    }
}

static void setupmaps(void)
{
    int i;
    if (!mapsinited) initmaps();

    /* Do not map undefined characters to some accidental code */
    for (i = 128; i < 256; i++)
    {
       unix2dos[i] = CTRLZ;
       dos2unix[i] = CTRLZ;
    }
}

static void init_iso8859_1(int codepage) {

	setupmaps();

    if (codepage == 437) {
        /* MSDOS Code Page 437 -> ISO-8859-1 */
        update_map("\xA1\xAD\xA2\x98\xA3\x9C\xA4\xED\xA5\x9D\xA6\xB3\xA7\xEE");
        update_map("\xAA\xA6\xAB\xAE\xAC\xAA\xAE\xE9\xAF\xC4");
        update_map("\xB0\xF8\xB1\xF1\xB2\xFD\xB5\xE6\xB7\xFA\xBA\xA7\xBC\xAC\xBD\xAB\xBF\xA8");
        update_map("\xC0\x85\xC1\xA0\xC2\x83\xC4\x8E\xC5\x8F\xC6\x92\xC7\x80\xC8\x8A");
        update_map("\xC9\x90\xCA\x88\xCB\x89\xCC\x8D\xCD\xA1\xCE\x8C\xCF\x8B");
        update_map("\xD1\xA5\xD2\x96\xD3\xA2\xD4\x93\xD6\x99\xD9\x97\xDA\xA3\xDB\x96\xDC\x9A\xDF\xE1");
        update_map("\xE0\x85\xE1\xA0\xE2\x83\xE4\x84\xE5\x86\xE6\x91\xE7\x87\xE8\x8A\xE9\x82\xEA\x88\xEB\x89\xEC\x8D\xED\xA1\xEE\x8C\xEF\x8B");
        update_map("\xF0\xEB\xF1\xA4\xF2\x95\xF3\xA2\xF4\x93\xF6\x94\xF7\xF6\xF8\xED\xF9\x97\xFA\xA3\xFB\x96\xFC\x81\xFF\x98");
    } else {
        /* MSDOS Code Page 850 -> ISO-8859-1 */
        update_map("\240\377\241\255\242\275\243\234\244\317\245\276\246\335\247\365");
        update_map("\250\371\251\270\252\246\253\256\254\252\255\360\256\251\257\356");
        update_map("\260\370\261\361\262\375\263\374\264\357\265\346\266\364\267\372");
        update_map("\270\367\271\373\272\247\273\257\274\254\275\253\276\363\277\250");
        update_map("\300\267\301\265\302\266\303\307\304\216\305\217\306\222\307\200");
        update_map("\310\324\311\220\312\322\313\323\314\336\315\326\316\327\317\330");
        update_map("\320\321\321\245\322\343\323\340\324\342\325\345\326\231\327\236");
        update_map("\330\235\331\353\332\351\333\352\334\232\335\355\336\350\337\341");
        update_map("\340\205\341\240\342\203\343\306\344\204\345\206\346\221\347\207");
        update_map("\350\212\351\202\352\210\353\211\354\215\355\241\356\214\357\213");
        update_map("\360\320\361\244\362\225\363\242\364\223\365\344\366\224\367\366");
        update_map("\370\233\371\227\372\243\373\226\374\201\375\354\376\347\377\230");
    }
}

/* Init for eastern european languages. */

static void init_iso8859_2(void) {

	setupmaps();

/*
 * Tranlation table created by Petr Hubeny <psh@capitol.cz>
 * Requires client code page = 852
 * and character set = ISO8859-2 in smb.conf
 */

/* MSDOS Code Page 852 -> ISO-8859-2 */
update_map("\240\377"); /* Fix for non-breaking space */
update_map("\241\244\242\364\243\235\244\317\245\225\246\227\247\365");
update_map("\250\371\251\346\252\270\253\233\254\215\256\246\257\275");
update_map("\261\245\262\362\263\210\264\357\265\226\266\230\267\363");
update_map("\270\367\271\347\272\255\273\234\274\253\275\361\276\247\277\276");
update_map("\300\350\301\265\302\266\303\306\304\216\305\221\306\217\307\200");
update_map("\310\254\311\220\312\250\313\323\314\267\315\326\316\327\317\322");
update_map("\320\321\321\343\322\325\323\340\324\342\325\212\326\231\327\236");
update_map("\330\374\331\336\332\351\333\353\334\232\335\355\336\335\337\341");
update_map("\340\352\341\240\342\203\343\307\344\204\345\222\346\206\347\207");
update_map("\350\237\351\202\352\251\353\211\354\330\355\241\356\214\357\324");
update_map("\360\320\361\344\362\345\363\242\364\223\365\213\366\224\367\366");
update_map("\370\375\371\205\372\243\373\373\374\201\375\354\376\356\377\372");
}

/* Init for russian language (iso8859-5) */

/* Added by Max Khon <max@iclub.nsu.ru> */

static void init_iso8859_5(void)
{
	setupmaps();

/* MSDOS Code Page 866 -> ISO8859-5 */
update_map("\260\200\261\201\262\202\263\203\264\204\265\205\266\206\267\207");
update_map("\270\210\271\211\272\212\273\213\274\214\275\215\276\216\277\217");
update_map("\300\220\301\221\302\222\303\223\304\224\305\225\306\226\307\227");
update_map("\310\230\311\231\312\232\313\233\314\234\315\235\316\236\317\237");
update_map("\320\240\321\241\322\242\323\243\324\244\325\245\326\246\327\247");
update_map("\330\250\331\251\332\252\333\253\334\254\335\255\336\256\337\257");
update_map("\340\340\341\341\342\342\343\343\344\344\345\345\346\346\347\347");
update_map("\350\350\351\351\352\352\353\353\354\354\355\355\356\356\357\357");
update_map("\241\360\361\361\244\362\364\363\247\364\367\365\256\366\376\367");
update_map("\360\374\240\377");
}

/* Added by Antonios Kavarnos (Antonios.Kavarnos@softlab.ece.ntua.gr */

static void init_iso8859_7(void)
{
	setupmaps();

/* MSDOS Code Page 737 -> ISO-8859-7 (Greek-Hellenic) */

update_map("\301\200\302\201\303\202\304\203\305\204\306\205\307\206");
update_map("\310\207\311\210\312\211\313\212\314\213\315\214\316\215\317\216");
update_map("\320\217\321\220\323\221\324\222\325\223\326\224\327\225");
update_map("\330\226\331\227");
update_map("\341\230\342\231\343\232\344\233\345\234\346\235\347\236");
update_map("\350\237\351\240\352\241\353\242\354\243\355\244\356\245\357\246");
update_map("\360\247\361\250\362\252\363\251\364\253\365\254\366\255\367\256");
update_map("\370\257\371\340");
update_map("\332\364\333\365\334\341\335\342\336\343\337\345");
update_map("\372\344\373\350\374\346\375\347\376\351");
update_map("\266\352");
update_map("\270\353\271\354\272\355\274\356\276\357\277\360");
}

/* Init for russian language (koi8) */

static void init_koi8_r(void)
{
	setupmaps();

/* MSDOS Code Page 866 -> KOI8-R */
update_map("\200\304\201\263\202\332\203\277\204\300\205\331\206\303\207\264");
update_map("\210\302\211\301\212\305\213\337\214\334\215\333\216\335\217\336");
update_map("\220\260\221\261\222\262\223\364\224\376\225\371\226\373\227\367");
update_map("\230\363\231\362\232\377\233\365\234\370\235\375\236\372\237\366");
update_map("\240\315\241\272\242\325\243\361\244\326\245\311\246\270\247\267");
update_map("\250\273\251\324\252\323\253\310\254\276\255\275\256\274\257\306");
update_map("\260\307\261\314\262\265\263\360\264\266\265\271\266\321\267\322");
update_map("\270\313\271\317\272\320\273\312\274\330\275\327\276\316\277\374");
update_map("\300\356\301\240\302\241\303\346\304\244\305\245\306\344\307\243");
update_map("\310\345\311\250\312\251\313\252\314\253\315\254\316\255\317\256");
update_map("\320\257\321\357\322\340\323\341\324\342\325\343\326\246\327\242");
update_map("\330\354\331\353\332\247\333\350\334\355\335\351\336\347\337\352");
update_map("\340\236\341\200\342\201\343\226\344\204\345\205\346\224\347\203");
update_map("\350\225\351\210\352\211\353\212\354\213\355\214\356\215\357\216");
update_map("\360\217\361\237\362\220\363\221\364\222\365\223\366\206\367\202");
update_map("\370\234\371\233\372\207\373\230\374\235\375\231\376\227\377\232");
}


/* Init for ROMAN-8 (HP-UX) */

static void init_roman8(void) {

	setupmaps();

/* MSDOS Code Page 850 -> ROMAN8 */
update_map("\240\377\241\267\242\266\243\324\244\322\245\323\246\327\247\330");
update_map("\250\357\253\371\255\353\256\352\257\234");
update_map("\260\356\261\355\262\354\263\370\264\200\265\207\266\245\267\244");
update_map("\270\255\271\250\272\317\273\234\274\276\275\365\276\237\277\275");
update_map("\300\203\301\210\302\223\303\226\304\240\305\202\306\242\307\243");
update_map("\310\205\311\212\312\225\313\227\314\204\315\211\316\224\317\201");
update_map("\320\217\321\214\322\235\323\222\324\206\325\241\326\233\327\221");
update_map("\330\216\331\215\332\231\333\232\334\220\335\213\336\341\337\342");
update_map("\340\265\341\307\342\306\343\321\344\320\345\326\346\336\347\340");
update_map("\350\343\351\345\352\344\355\351\357\230");
update_map("\360\350\361\347\362\372\363\346\364\364\365\363\366\360\367\254");
update_map("\370\253\371\246\372\247\373\256\374\376\375\257\376\361");
}

static void init_1251(void)
{
  int i;
  if (!mapsinited) initmaps();

  /* Do not map undefined characters to some accidental code */
  for (i = 128; i < 256; i++) 
  {
     unix2dos[i] = CTRLZ;
     dos2unix[i] = CTRLZ;
  }

/* MSDOS Code Page 866 -> 1251 */

update_map ("\240\377\241\366\242\367\244\375");
update_map ("\250\360\252\362\257\364");
update_map ("\260\370\267\372");
update_map ("\270\361\271\374\272\363\277\365");
update_map ("\300\200\301\201\302\202\303\203\304\204\305\205\306\206\307\207");
update_map ("\310\210\311\211\312\212\313\213\314\214\315\215\316\216\317\217");
update_map ("\320\220\321\221\322\222\323\223\324\224\325\225\326\226\327\227");
update_map ("\330\230\331\231\332\232\333\233\334\234\335\235\336\236\337\237");
update_map ("\340\240\341\241\342\242\343\243\344\244\345\245\346\246\347\247");
update_map ("\350\250\351\251\352\252\353\253\354\254\355\255\356\256\357\257");
update_map ("\360\340\361\341\362\342\363\343\364\344\365\345\366\346\367\347");
update_map ("\370\350\371\351\372\352\373\353\374\354\375\355\376\356\377\357");
}

/*
 * Convert unix to dos
 */
char *unix2dos_format(char *str,BOOL overwrite)
{
  char *p;
  char *dp;

  if (!mapsinited)
    initmaps();

  if (overwrite) {
    for (p = str; *p; p++)
      *p = unix2dos[(unsigned char)*p];
    return str;
  } else {
    for (p = str, dp = cvtbuf;*p && (dp - cvtbuf < sizeof(cvtbuf) - 1); p++,dp++)
      *dp = unix2dos[(unsigned char)*p];
    *dp = 0;
    return cvtbuf;
  }
}

/*
 * Convert dos to unix
 */
char *dos2unix_format(char *str, BOOL overwrite)
{
  char *p;
  char *dp;

  if (!mapsinited)
    initmaps();

  if (overwrite) {
    for (p = str; *p; p++)
      *p = dos2unix[(unsigned char)*p];
    return str;
  } else {
    for (p = str, dp = cvtbuf;*p && (dp - cvtbuf < sizeof(cvtbuf) - 1); p++,dp++)
      *dp = dos2unix[(unsigned char)*p];
    *dp = 0;
    return cvtbuf;
  }
}


/*
 * Interpret character set.
 */
void interpret_character_set(char *str, int codepage)
{
    if (strequal (str, "iso8859-1")) {
        init_iso8859_1(codepage);
    } else if (strequal (str, "iso8859-2")) {
        init_iso8859_2();
    } else if (strequal (str, "iso8859-5")) {
        init_iso8859_5();
    } else if (strequal (str, "iso8859-7")) {
        init_iso8859_7();
    } else if (strequal (str, "koi8-r")) {
        init_koi8_r();
    } else if (strequal (str, "1251")) {
        init_1251();
    } else if (strequal (str, "roman8")) {
        init_roman8();
    } else {
        DEBUG(0,("unrecognized character set %s\n", str));
    }

    load_unix_unicode_map(str);
}
