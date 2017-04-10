/**************************************************************************
 *   winio.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014, 2015, 2016, 2017 Benno Schulenberg               *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "proto.h"
#include "revision.h"

#ifdef __linux__
#include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifdef REVISION
#define BRANDING REVISION
#else
#define BRANDING PACKAGE_STRING
#endif

static int *key_buffer = NULL;
	/* The keystroke buffer, containing all the keystrokes we
	 * haven't handled yet at a given point. */
static size_t key_buffer_len = 0;
	/* The length of the keystroke buffer. */
static bool solitary = FALSE;
	/* Whether an Esc arrived by itself -- not as leader of a sequence. */
static int statusblank = 0;
	/* The number of keystrokes left before we blank the statusbar. */
static bool suppress_cursorpos = FALSE;
	/* Should we skip constant position display for one keystroke? */
#ifdef USING_OLD_NCURSES
static bool seen_wide = FALSE;
	/* Whether we've seen a multicolumn character in the current line. */
#endif

/* Control character compatibility:
 *
 * - Ctrl-H is Backspace under ASCII, ANSI, VT100, and VT220.
 * - Ctrl-I is Tab under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-M is Enter under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-Q is XON under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-S is XOFF under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-8 (Ctrl-?) is Delete under ASCII, ANSI, VT100, and VT220,
 *          but is Backspace under VT320.
 *
 * Note: VT220 and VT320 also generate Esc [ 3 ~ for Delete.  By
 * default, xterm assumes it's running on a VT320 and generates Ctrl-8
 * (Ctrl-?) for Backspace and Esc [ 3 ~ for Delete.  This causes
 * problems for VT100-derived terminals such as the FreeBSD console,
 * which expect Ctrl-H for Backspace and Ctrl-8 (Ctrl-?) for Delete, and
 * on which the VT320 sequences are translated by the keypad to KEY_DC
 * and [nothing].  We work around this conflict via the REBIND_DELETE
 * flag: if it's not set, we assume VT320 compatibility, and if it is,
 * we assume VT100 compatibility.  Thanks to Lee Nelson and Wouter van
 * Hemel for helping work this conflict out.
 *
 * Escape sequence compatibility:
 *
 * We support escape sequences for ANSI, VT100, VT220, VT320, the Linux
 * console, the FreeBSD console, the Mach console, xterm, rxvt, Eterm,
 * and Terminal, and some for iTerm2.  Among these, there are several
 * conflicts and omissions, outlined as follows:
 *
 * - Tab on ANSI == PageUp on FreeBSD console; the former is omitted.
 *   (Ctrl-I is also Tab on ANSI, which we already support.)
 * - PageDown on FreeBSD console == Center (5) on numeric keypad with
 *   NumLock off on Linux console; the latter is omitted.  (The editing
 *   keypad key is more important to have working than the numeric
 *   keypad key, because the latter has no value when NumLock is off.)
 * - F1 on FreeBSD console == the mouse key on xterm/rxvt/Eterm; the
 *   latter is omitted.  (Mouse input will only work properly if the
 *   extended keypad value KEY_MOUSE is generated on mouse events
 *   instead of the escape sequence.)
 * - F9 on FreeBSD console == PageDown on Mach console; the former is
 *   omitted.  (The editing keypad is more important to have working
 *   than the function keys, because the functions of the former are not
 *   arbitrary and the functions of the latter are.)
 * - F10 on FreeBSD console == PageUp on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F13 on FreeBSD console == End on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F15 on FreeBSD console == Shift-Up on rxvt/Eterm; the former is
 *   omitted.  (The arrow keys, with or without modifiers, are more
 *   important to have working than the function keys, because the
 *   functions of the former are not arbitrary and the functions of the
 *   latter are.)
 * - F16 on FreeBSD console == Shift-Down on rxvt/Eterm; the former is
 *   omitted.  (Same as above.) */

/* Read in a sequence of keystrokes from win and save them in the
 * keystroke buffer.  This should only be called when the keystroke
 * buffer is empty. */
void get_key_buffer(WINDOW *win)
{
    int input;
    size_t errcount = 0;

    /* If the keystroke buffer isn't empty, get out. */
    if (key_buffer != NULL)
	return;

    /* Just before reading in the first character, display any pending
     * screen updates. */
    doupdate();

    /* Read in the first character using whatever mode we're in. */
    input = wgetch(win);

#ifndef NANO_TINY
    if (the_window_resized) {
	ungetch(input);
	regenerate_screen();
	input = KEY_WINCH;
    }
#endif

    if (input == ERR && nodelay_mode)
	return;

    while (input == ERR) {
	/* If we've failed to get a character MAX_BUF_SIZE times in a row,
	 * assume our input source is gone and die gracefully.  We could
	 * check if errno is set to EIO ("Input/output error") and die in
	 * that case, but it's not always set properly.  Argh. */
	if (++errcount == MAX_BUF_SIZE)
	    handle_hupterm(0);

#ifndef NANO_TINY
	if (the_window_resized) {
	    regenerate_screen();
	    input = KEY_WINCH;
	    break;
	}
#endif
	input = wgetch(win);
    }

    /* Increment the length of the keystroke buffer, and save the value
     * of the keystroke at the end of it. */
    key_buffer_len++;
    key_buffer = (int *)nmalloc(sizeof(int));
    key_buffer[0] = input;

#ifndef NANO_TINY
    /* If we got SIGWINCH, get out immediately since the win argument is
     * no longer valid. */
    if (input == KEY_WINCH)
	return;
#endif

    /* Read in the remaining characters using non-blocking input. */
    nodelay(win, TRUE);

    while (TRUE) {
	input = wgetch(win);

	/* If there aren't any more characters, stop reading. */
	if (input == ERR)
	    break;

	/* Otherwise, increment the length of the keystroke buffer, and
	 * save the value of the keystroke at the end of it. */
	key_buffer_len++;
	key_buffer = (int *)nrealloc(key_buffer, key_buffer_len *
		sizeof(int));
	key_buffer[key_buffer_len - 1] = input;
    }

    /* Restore waiting mode if it was on. */
    if (!nodelay_mode)
	nodelay(win, FALSE);

#ifdef DEBUG
    {
	size_t i;
	fprintf(stderr, "\nget_key_buffer(): the sequence of hex codes:");
	for (i = 0; i < key_buffer_len; i++)
	    fprintf(stderr, " %3x", key_buffer[i]);
	fprintf(stderr, "\n");
    }
#endif
}

/* Return the length of the keystroke buffer. */
size_t get_key_buffer_len(void)
{
    return key_buffer_len;
}

/* Add the keystrokes in input to the keystroke buffer. */
void unget_input(int *input, size_t input_len)
{
    /* If input is empty, get out. */
    if (input_len == 0)
	return;

    /* If adding input would put the keystroke buffer beyond maximum
     * capacity, only add enough of input to put it at maximum
     * capacity. */
    if (key_buffer_len + input_len < key_buffer_len)
	input_len = (size_t)-1 - key_buffer_len;

    /* Add the length of input to the length of the keystroke buffer,
     * and reallocate the keystroke buffer so that it has enough room
     * for input. */
    key_buffer_len += input_len;
    key_buffer = (int *)nrealloc(key_buffer, key_buffer_len *
	sizeof(int));

    /* If the keystroke buffer wasn't empty before, move its beginning
     * forward far enough so that we can add input to its beginning. */
    if (key_buffer_len > input_len)
	memmove(key_buffer + input_len, key_buffer,
		(key_buffer_len - input_len) * sizeof(int));

    /* Copy input to the beginning of the keystroke buffer. */
    memcpy(key_buffer, input, input_len * sizeof(int));
}

/* Put the character given in kbinput back into the input stream.  If it
 * is a Meta key, also insert an Escape character in front of it. */
void unget_kbinput(int kbinput, bool metakey)
{
    unget_input(&kbinput, 1);

    if (metakey) {
	kbinput = ESC_CODE;
	unget_input(&kbinput, 1);
    }
}

/* Try to read input_len codes from the keystroke buffer.  If the
 * keystroke buffer is empty and win isn't NULL, try to read in more
 * codes from win and add them to the keystroke buffer before doing
 * anything else.  If the keystroke buffer is (still) empty, return NULL. */
int *get_input(WINDOW *win, size_t input_len)
{
    int *input;

    if (key_buffer_len == 0 && win != NULL)
	get_key_buffer(win);

    if (key_buffer_len == 0)
	return NULL;

    /* Limit the request to the number of available codes in the buffer. */
    if (input_len > key_buffer_len)
	input_len = key_buffer_len;

    /* Copy input_len codes from the head of the keystroke buffer. */
    input = (int *)nmalloc(input_len * sizeof(int));
    memcpy(input, key_buffer, input_len * sizeof(int));
    key_buffer_len -= input_len;

    /* If the keystroke buffer is now empty, mark it as such. */
    if (key_buffer_len == 0) {
	free(key_buffer);
	key_buffer = NULL;
    } else {
	/* Trim from the buffer the codes that were copied. */
	memmove(key_buffer, key_buffer + input_len, key_buffer_len *
		sizeof(int));
	key_buffer = (int *)nrealloc(key_buffer, key_buffer_len *
		sizeof(int));
    }

    return input;
}

/* Read in a single keystroke, ignoring any that are invalid. */
int get_kbinput(WINDOW *win)
{
    int kbinput = ERR;

    /* Extract one keystroke from the input stream. */
    while (kbinput == ERR)
	kbinput = parse_kbinput(win);

#ifdef DEBUG
    fprintf(stderr, "after parsing:  kbinput = %d, meta_key = %s\n",
	kbinput, meta_key ? "TRUE" : "FALSE");
#endif

    /* If we read from the edit window, blank the statusbar if needed. */
    if (win == edit)
	check_statusblank();

    return kbinput;
}

/* Extract a single keystroke from the input stream.  Translate escape
 * sequences and extended keypad codes into their corresponding values.
 * Set meta_key to TRUE when appropriate.  Supported extended keypad values
 * are: [arrow key], Ctrl-[arrow key], Shift-[arrow key], Enter, Backspace,
 * the editing keypad (Insert, Delete, Home, End, PageUp, and PageDown),
 * the function keys (F1-F16), and the numeric keypad with NumLock off. */
int parse_kbinput(WINDOW *win)
{
    static int escapes = 0, byte_digits = 0;
    static bool double_esc = FALSE;
    int *kbinput, keycode, retval = ERR;

    meta_key = FALSE;
    shift_held = FALSE;

    /* Read in a character. */
    kbinput = get_input(win, 1);

    if (kbinput == NULL && nodelay_mode)
	return 0;

    while (kbinput == NULL)
	kbinput = get_input(win, 1);

    keycode = *kbinput;
    free(kbinput);

#ifdef DEBUG
    fprintf(stderr, "before parsing:  keycode = %d, escapes = %d, byte_digits = %d\n",
	keycode, escapes, byte_digits);
#endif

    if (keycode == ERR)
	return ERR;

    if (keycode == ESC_CODE) {
	/* Increment the escape counter, but trim an overabundance. */
	escapes++;
	if (escapes > 3)
	    escapes = 1;
	/* Take note when an Esc arrived by itself. */
	solitary = (escapes == 1 && key_buffer_len == 0);
	return ERR;
    }

    switch (escapes) {
	case 0:
	    /* One non-escape: normal input mode. */
	    retval = keycode;
	    break;
	case 1:
	    if (keycode >= 0x80)
		retval = keycode;
	    else if ((keycode != 'O' && keycode != 'o' && keycode != '[') ||
			key_buffer_len == 0 || *key_buffer == ESC_CODE) {
		/* One escape followed by a single non-escape:
		 * meta key sequence mode. */
		if (!solitary || (keycode >= 0x20 && keycode < 0x7F))
		    meta_key = TRUE;
		retval = tolower(keycode);
	    } else
		/* One escape followed by a non-escape, and there
		 * are more codes waiting: escape sequence mode. */
		retval = parse_escape_sequence(win, keycode);
	    escapes = 0;
	    break;
	case 2:
	    if (double_esc) {
		/* An "ESC ESC [ X" sequence from Option+arrow, or
		 * an "ESC ESC [ x" sequence from Shift+Alt+arrow. */
		switch (keycode) {
		    case 'A':
			retval = KEY_HOME;
			break;
		    case 'B':
			retval = KEY_END;
			break;
		    case 'C':
			retval = CONTROL_RIGHT;
			break;
		    case 'D':
			retval = CONTROL_LEFT;
			break;
#ifndef NANO_TINY
		    case 'a':
			retval = shiftaltup;
			break;
		    case 'b':
			retval = shiftaltdown;
			break;
		    case 'c':
			retval = shiftaltright;
			break;
		    case 'd':
			retval = shiftaltleft;
			break;
#endif
		}
		double_esc = FALSE;
		escapes = 0;
	    } else if (key_buffer_len == 0) {
		if (('0' <= keycode && keycode <= '2' &&
			byte_digits == 0) || ('0' <= keycode &&
			keycode <= '9' && byte_digits > 0)) {
		    /* Two escapes followed by one or more decimal
		     * digits, and there aren't any other codes
		     * waiting: byte sequence mode.  If the range
		     * of the byte sequence is limited to 2XX (the
		     * first digit is between '0' and '2' and the
		     * others between '0' and '9', interpret it. */
		    int byte;

		    byte_digits++;
		    byte = get_byte_kbinput(keycode);

		    /* If the decimal byte value is complete, convert it and
		     * put the obtained byte(s) back into the input buffer. */
		    if (byte != ERR) {
			char *byte_mb;
			int byte_mb_len, *seq, i;

			/* Convert the decimal code to one or two bytes. */
			byte_mb = make_mbchar((long)byte, &byte_mb_len);

			seq = (int *)nmalloc(byte_mb_len * sizeof(int));

			for (i = 0; i < byte_mb_len; i++)
			    seq[i] = (unsigned char)byte_mb[i];

			/* Insert the byte(s) into the input buffer. */
			unget_input(seq, byte_mb_len);

			free(byte_mb);
			free(seq);

			byte_digits = 0;
			escapes = 0;
		    }
		} else {
		    if (byte_digits == 0)
			/* Two escapes followed by a non-decimal
			 * digit (or a decimal digit that would
			 * create a byte sequence greater than 2XX)
			 * and there aren't any other codes waiting:
			 * control character sequence mode. */
			retval = get_control_kbinput(keycode);
		    else {
			/* An invalid digit in the middle of a byte
			 * sequence: reset the byte sequence counter
			 * and save the code we got as the result. */
			byte_digits = 0;
			retval = keycode;
		    }
		    escapes = 0;
		}
	    } else if (keycode == '[' && key_buffer_len > 0 &&
			(('A' <= *key_buffer && *key_buffer <= 'D') ||
			('a' <= *key_buffer && *key_buffer <= 'd'))) {
		/* An iTerm2/Eterm/rxvt sequence: ^[ ^[ [ X. */
		double_esc = TRUE;
	    } else {
		/* Two escapes followed by a non-escape, and there are more
		 * codes waiting: combined meta and escape sequence mode. */
		retval = parse_escape_sequence(win, keycode);
		meta_key = TRUE;
		escapes = 0;
	    }
	    break;
	case 3:
	    if (key_buffer_len == 0)
		/* Three escapes followed by a non-escape, and no
		 * other codes are waiting: normal input mode. */
		retval = keycode;
	    else
		/* Three escapes followed by a non-escape, and more
		 * codes are waiting: combined control character and
		 * escape sequence mode.  First interpret the escape
		 * sequence, then the result as a control sequence. */
		retval = get_control_kbinput(
			parse_escape_sequence(win, keycode));
	    escapes = 0;
	    break;
    }

    if (retval == ERR)
	return ERR;

    if (retval == controlleft)
	return CONTROL_LEFT;
    else if (retval == controlright)
	return CONTROL_RIGHT;
    else if (retval == controlup)
	return CONTROL_UP;
    else if (retval == controldown)
	return CONTROL_DOWN;
#ifndef NANO_TINY
    else if (retval == shiftcontrolleft) {
	shift_held = TRUE;
	return CONTROL_LEFT;
    } else if (retval == shiftcontrolright) {
	shift_held = TRUE;
	return CONTROL_RIGHT;
    } else if (retval == shiftcontrolup) {
	shift_held = TRUE;
	return CONTROL_UP;
    } else if (retval == shiftcontroldown) {
	shift_held = TRUE;
	return CONTROL_DOWN;
    } else if (retval == shiftaltleft) {
	shift_held = TRUE;
	return KEY_HOME;
    } else if (retval == shiftaltright) {
	shift_held = TRUE;
	return KEY_END;
    } else if (retval == shiftaltup) {
	shift_held = TRUE;
	return KEY_PPAGE;
    } else if (retval == shiftaltdown) {
	shift_held = TRUE;
	return KEY_NPAGE;
    }
#endif

#ifdef __linux__
    /* When not running under X, check for the bare arrow keys whether
     * Shift/Ctrl/Alt are being held together with them. */
    unsigned char modifiers = 6;

    if (console && ioctl(0, TIOCLINUX, &modifiers) >= 0) {
#ifndef NANO_TINY
	/* Is Shift being held? */
	if (modifiers & 0x01)
	    shift_held = TRUE;
#endif
	/* Is Ctrl being held? */
	if (modifiers & 0x04) {
	    if (retval == KEY_UP)
		return CONTROL_UP;
	    else if (retval == KEY_DOWN)
		return CONTROL_DOWN;
	    else if (retval == KEY_LEFT)
		return CONTROL_LEFT;
	    else if (retval == KEY_RIGHT)
		return CONTROL_RIGHT;
	}
#ifndef NANO_TINY
	/* Are both Shift and Alt being held? */
	if ((modifiers & 0x09) == 0x09) {
	    if (retval == KEY_UP)
		return KEY_PPAGE;
	    else if (retval == KEY_DOWN)
		return KEY_NPAGE;
	    else if (retval == KEY_LEFT)
		return KEY_HOME;
	    else if (retval == KEY_RIGHT)
		return KEY_END;
	}
#endif
    }
#endif /* __linux__ */

    switch (retval) {
#ifdef KEY_SLEFT
	/* Slang doesn't support KEY_SLEFT. */
	case KEY_SLEFT:
	    shift_held = TRUE;
	    return KEY_LEFT;
#endif
#ifdef KEY_SRIGHT
	/* Slang doesn't support KEY_SRIGHT. */
	case KEY_SRIGHT:
	    shift_held = TRUE;
	    return KEY_RIGHT;
#endif
#ifdef KEY_SR
#ifdef KEY_SUP
	/* ncurses and Slang don't support KEY_SUP. */
	case KEY_SUP:
#endif
	case KEY_SR:	/* Scroll backward, on Xfce4-terminal. */
	    shift_held = TRUE;
	    return KEY_UP;
#endif
#ifdef KEY_SF
#ifdef KEY_SDOWN
	/* ncurses and Slang don't support KEY_SDOWN. */
	case KEY_SDOWN:
#endif
	case KEY_SF:	/* Scroll forward, on Xfce4-terminal. */
	    shift_held = TRUE;
	    return KEY_DOWN;
#endif
#ifdef KEY_SHOME
	/* HP-UX 10-11 and Slang don't support KEY_SHOME. */
	case KEY_SHOME:
#endif
	case SHIFT_HOME:
	    shift_held = TRUE;
	case KEY_A1:	/* Home (7) on keypad with NumLock off. */
	    return KEY_HOME;
#ifdef KEY_SEND
	/* HP-UX 10-11 and Slang don't support KEY_SEND. */
	case KEY_SEND:
#endif
	case SHIFT_END:
	    shift_held = TRUE;
	case KEY_C1:	/* End (1) on keypad with NumLock off. */
	    return KEY_END;
#ifndef NANO_TINY
#ifdef KEY_SPREVIOUS
	case KEY_SPREVIOUS:
#endif
	case SHIFT_PAGEUP:	/* Fake key, from Shift+Alt+Up. */
	    shift_held = TRUE;
#endif
	case KEY_A3:	/* PageUp (9) on keypad with NumLock off. */
	    return KEY_PPAGE;
#ifndef NANO_TINY
#ifdef KEY_SNEXT
	case KEY_SNEXT:
#endif
	case SHIFT_PAGEDOWN:	/* Fake key, from Shift+Alt+Down. */
	    shift_held = TRUE;
#endif
	case KEY_C3:	/* PageDown (3) on keypad with NumLock off. */
	    return KEY_NPAGE;
#ifdef KEY_SDC
	/* Slang doesn't support KEY_SDC. */
	case KEY_SDC:
#endif
	case DEL_CODE:
	    if (ISSET(REBIND_DELETE))
		return the_code_for(do_delete, KEY_DC);
	    else
		return KEY_BACKSPACE;
#ifdef KEY_SIC
	/* Slang doesn't support KEY_SIC. */
	case KEY_SIC:
	    return the_code_for(do_insertfile_void, KEY_IC);
#endif
#ifdef KEY_SBEG
	/* Slang doesn't support KEY_SBEG. */
	case KEY_SBEG:
#endif
#ifdef KEY_BEG
	/* Slang doesn't support KEY_BEG. */
	case KEY_BEG:
#endif
	case KEY_B2:	/* Center (5) on keypad with NumLock off. */
	    return ERR;
#ifdef KEY_CANCEL
#ifdef KEY_SCANCEL
	/* Slang doesn't support KEY_SCANCEL. */
	case KEY_SCANCEL:
#endif
	/* Slang doesn't support KEY_CANCEL. */
	case KEY_CANCEL:
	    return the_code_for(do_cancel, 0x03);
#endif
#ifdef KEY_SUSPEND
#ifdef KEY_SSUSPEND
	/* Slang doesn't support KEY_SSUSPEND. */
	case KEY_SSUSPEND:
#endif
	/* Slang doesn't support KEY_SUSPEND. */
	case KEY_SUSPEND:
	    return the_code_for(do_suspend_void, KEY_SUSPEND);
#endif
#ifdef PDCURSES
	case KEY_SHIFT_L:
	case KEY_SHIFT_R:
	case KEY_CONTROL_L:
	case KEY_CONTROL_R:
	case KEY_ALT_L:
	case KEY_ALT_R:
	    return ERR;
#endif
#ifdef KEY_RESIZE
	/* Slang and SunOS 5.7-5.9 don't support KEY_RESIZE. */
	case KEY_RESIZE:
	    return ERR;
#endif
    }

    return retval;
}

/* Translate escape sequences, most of which correspond to extended
 * keypad values, into their corresponding key values.  These sequences
 * are generated when the keypad doesn't support the needed keys.
 * Assume that Escape has already been read in. */
int convert_sequence(const int *seq, size_t seq_len)
{
    if (seq_len > 1) {
	switch (seq[0]) {
	    case 'O':
		switch (seq[1]) {
		    case '1':
			if (seq_len > 4  && seq[2] == ';') {

	switch (seq[3]) {
	    case '2':
		switch (seq[4]) {
		    case 'A': /* Esc O 1 ; 2 A == Shift-Up on Terminal. */
		    case 'B': /* Esc O 1 ; 2 B == Shift-Down on Terminal. */
		    case 'C': /* Esc O 1 ; 2 C == Shift-Right on Terminal. */
		    case 'D': /* Esc O 1 ; 2 D == Shift-Left on Terminal. */
			shift_held = TRUE;
			return arrow_from_abcd(seq[4]);
		    case 'P': /* Esc O 1 ; 2 P == F13 on Terminal. */
			return KEY_F(13);
		    case 'Q': /* Esc O 1 ; 2 Q == F14 on Terminal. */
			return KEY_F(14);
		    case 'R': /* Esc O 1 ; 2 R == F15 on Terminal. */
			return KEY_F(15);
		    case 'S': /* Esc O 1 ; 2 S == F16 on Terminal. */
			return KEY_F(16);
		}
		break;
	    case '5':
		switch (seq[4]) {
		    case 'A': /* Esc O 1 ; 5 A == Ctrl-Up on Terminal. */
			return CONTROL_UP;
		    case 'B': /* Esc O 1 ; 5 B == Ctrl-Down on Terminal. */
			return CONTROL_DOWN;
		    case 'C': /* Esc O 1 ; 5 C == Ctrl-Right on Terminal. */
			return CONTROL_RIGHT;
		    case 'D': /* Esc O 1 ; 5 D == Ctrl-Left on Terminal. */
			return CONTROL_LEFT;
		}
		break;
	}

			}
			break;
		    case '2':
			if (seq_len >= 3) {
			    switch (seq[2]) {
				case 'P': /* Esc O 2 P == F13 on xterm. */
				    return KEY_F(13);
				case 'Q': /* Esc O 2 Q == F14 on xterm. */
				    return KEY_F(14);
				case 'R': /* Esc O 2 R == F15 on xterm. */
				    return KEY_F(15);
				case 'S': /* Esc O 2 S == F16 on xterm. */
				    return KEY_F(16);
			    }
			}
			break;
		    case 'A': /* Esc O A == Up on VT100/VT320/xterm. */
		    case 'B': /* Esc O B == Down on VT100/VT320/xterm. */
		    case 'C': /* Esc O C == Right on VT100/VT320/xterm. */
		    case 'D': /* Esc O D == Left on VT100/VT320/xterm. */
			return arrow_from_abcd(seq[1]);
		    case 'E': /* Esc O E == Center (5) on numeric keypad
			       * with NumLock off on xterm. */
			return KEY_B2;
		    case 'F': /* Esc O F == End on xterm/Terminal. */
			return KEY_END;
		    case 'H': /* Esc O H == Home on xterm/Terminal. */
			return KEY_HOME;
		    case 'M': /* Esc O M == Enter on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm. */
			return KEY_ENTER;
		    case 'P': /* Esc O P == F1 on VT100/VT220/VT320/Mach
			       * console. */
			return KEY_F(1);
		    case 'Q': /* Esc O Q == F2 on VT100/VT220/VT320/Mach
			       * console. */
			return KEY_F(2);
		    case 'R': /* Esc O R == F3 on VT100/VT220/VT320/Mach
			       * console. */
			return KEY_F(3);
		    case 'S': /* Esc O S == F4 on VT100/VT220/VT320/Mach
			       * console. */
			return KEY_F(4);
		    case 'T': /* Esc O T == F5 on Mach console. */
			return KEY_F(5);
		    case 'U': /* Esc O U == F6 on Mach console. */
			return KEY_F(6);
		    case 'V': /* Esc O V == F7 on Mach console. */
			return KEY_F(7);
		    case 'W': /* Esc O W == F8 on Mach console. */
			return KEY_F(8);
		    case 'X': /* Esc O X == F9 on Mach console. */
			return KEY_F(9);
		    case 'Y': /* Esc O Y == F10 on Mach console. */
			return KEY_F(10);
		    case 'a': /* Esc O a == Ctrl-Up on rxvt. */
			return CONTROL_UP;
		    case 'b': /* Esc O b == Ctrl-Down on rxvt. */
			return CONTROL_DOWN;
		    case 'c': /* Esc O c == Ctrl-Right on rxvt. */
			return CONTROL_RIGHT;
		    case 'd': /* Esc O d == Ctrl-Left on rxvt. */
			return CONTROL_LEFT;
		    case 'j': /* Esc O j == '*' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm/Terminal. */
			return '*';
		    case 'k': /* Esc O k == '+' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm/Terminal. */
			return '+';
		    case 'l': /* Esc O l == ',' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm/Terminal. */
			return ',';
		    case 'm': /* Esc O m == '-' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm/Terminal. */
			return '-';
		    case 'n': /* Esc O n == Delete (.) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * xterm/rxvt/Eterm/Terminal. */
			return KEY_DC;
		    case 'o': /* Esc O o == '/' on numeric keypad with
			       * NumLock off on VT100/VT220/VT320/xterm/
			       * rxvt/Eterm/Terminal. */
			return '/';
		    case 'p': /* Esc O p == Insert (0) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_IC;
		    case 'q': /* Esc O q == End (1) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_END;
		    case 'r': /* Esc O r == Down (2) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_DOWN;
		    case 's': /* Esc O s == PageDown (3) on numeric
			       * keypad with NumLock off on VT100/VT220/
			       * VT320/rxvt/Eterm/Terminal. */
			return KEY_NPAGE;
		    case 't': /* Esc O t == Left (4) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_LEFT;
		    case 'u': /* Esc O u == Center (5) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm. */
			return KEY_B2;
		    case 'v': /* Esc O v == Right (6) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_RIGHT;
		    case 'w': /* Esc O w == Home (7) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_HOME;
		    case 'x': /* Esc O x == Up (8) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_UP;
		    case 'y': /* Esc O y == PageUp (9) on numeric keypad
			       * with NumLock off on VT100/VT220/VT320/
			       * rxvt/Eterm/Terminal. */
			return KEY_PPAGE;
		}
		break;
	    case 'o':
		switch (seq[1]) {
		    case 'a': /* Esc o a == Ctrl-Up on Eterm. */
			return CONTROL_UP;
		    case 'b': /* Esc o b == Ctrl-Down on Eterm. */
			return CONTROL_DOWN;
		    case 'c': /* Esc o c == Ctrl-Right on Eterm. */
			return CONTROL_RIGHT;
		    case 'd': /* Esc o d == Ctrl-Left on Eterm. */
			return CONTROL_LEFT;
		}
		break;
	    case '[':
		switch (seq[1]) {
		    case '1':
			if (seq_len > 3 && seq[3] == '~') {
			    switch (seq[2]) {
				case '1': /* Esc [ 1 1 ~ == F1 on rxvt/Eterm. */
				    return KEY_F(1);
				case '2': /* Esc [ 1 2 ~ == F2 on rxvt/Eterm. */
				    return KEY_F(2);
				case '3': /* Esc [ 1 3 ~ == F3 on rxvt/Eterm. */
				    return KEY_F(3);
				case '4': /* Esc [ 1 4 ~ == F4 on rxvt/Eterm. */
				    return KEY_F(4);
				case '5': /* Esc [ 1 5 ~ == F5 on xterm/
					   * rxvt/Eterm. */
				    return KEY_F(5);
				case '7': /* Esc [ 1 7 ~ == F6 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    return KEY_F(6);
				case '8': /* Esc [ 1 8 ~ == F7 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    return KEY_F(7);
				case '9': /* Esc [ 1 9 ~ == F8 on
					   * VT220/VT320/Linux console/
					   * xterm/rxvt/Eterm. */
				    return KEY_F(8);
			    }
			} else if (seq_len > 4 && seq[2] == ';') {

	switch (seq[3]) {
	    case '2':
		switch (seq[4]) {
		    case 'A': /* Esc [ 1 ; 2 A == Shift-Up on xterm. */
		    case 'B': /* Esc [ 1 ; 2 B == Shift-Down on xterm. */
		    case 'C': /* Esc [ 1 ; 2 C == Shift-Right on xterm. */
		    case 'D': /* Esc [ 1 ; 2 D == Shift-Left on xterm. */
			shift_held = TRUE;
			return arrow_from_abcd(seq[4]);
		}
		break;
#ifndef NANO_TINY
	    case '4':
		/* When the arrow keys are held together with Shift+Meta,
		 * act as if they are Home/End/PgUp/PgDown with Shift. */
		switch (seq[4]) {
		    case 'A': /* Esc [ 1 ; 4 A == Shift-Alt-Up on xterm. */
			return SHIFT_PAGEUP;
		    case 'B': /* Esc [ 1 ; 4 B == Shift-Alt-Down on xterm. */
			return SHIFT_PAGEDOWN;
		    case 'C': /* Esc [ 1 ; 4 C == Shift-Alt-Right on xterm. */
			return SHIFT_END;
		    case 'D': /* Esc [ 1 ; 4 D == Shift-Alt-Left on xterm. */
			return SHIFT_HOME;
		}
		break;
#endif
	    case '5':
		switch (seq[4]) {
		    case 'A': /* Esc [ 1 ; 5 A == Ctrl-Up on xterm. */
			return CONTROL_UP;
		    case 'B': /* Esc [ 1 ; 5 B == Ctrl-Down on xterm. */
			return CONTROL_DOWN;
		    case 'C': /* Esc [ 1 ; 5 C == Ctrl-Right on xterm. */
			return CONTROL_RIGHT;
		    case 'D': /* Esc [ 1 ; 5 D == Ctrl-Left on xterm. */
			return CONTROL_LEFT;
		}
		break;
#ifndef NANO_TINY
	    case '6':
		switch (seq[4]) {
		    case 'A': /* Esc [ 1 ; 6 A == Shift-Ctrl-Up on xterm. */
			return shiftcontrolup;
		    case 'B': /* Esc [ 1 ; 6 B == Shift-Ctrl-Down on xterm. */
			return shiftcontroldown;
		    case 'C': /* Esc [ 1 ; 6 C == Shift-Ctrl-Right on xterm. */
			return shiftcontrolright;
		    case 'D': /* Esc [ 1 ; 6 D == Shift-Ctrl-Left on xterm. */
			return shiftcontrolleft;
		}
		break;
#endif
	}

			} else if (seq_len > 2 && seq[2] == '~')
			    /* Esc [ 1 ~ == Home on VT320/Linux console. */
			    return KEY_HOME;
			break;
		    case '2':
			if (seq_len > 3 && seq[3] == '~') {
			    switch (seq[2]) {
				case '0': /* Esc [ 2 0 ~ == F9 on VT220/VT320/
					   * Linux console/xterm/rxvt/Eterm. */
				    return KEY_F(9);
				case '1': /* Esc [ 2 1 ~ == F10 on VT220/VT320/
					   * Linux console/xterm/rxvt/Eterm. */
				    return KEY_F(10);
				case '3': /* Esc [ 2 3 ~ == F11 on VT220/VT320/
					   * Linux console/xterm/rxvt/Eterm. */
				    return KEY_F(11);
				case '4': /* Esc [ 2 4 ~ == F12 on VT220/VT320/
					   * Linux console/xterm/rxvt/Eterm. */
				    return KEY_F(12);
				case '5': /* Esc [ 2 5 ~ == F13 on VT220/VT320/
					   * Linux console/rxvt/Eterm. */
				    return KEY_F(13);
				case '6': /* Esc [ 2 6 ~ == F14 on VT220/VT320/
					   * Linux console/rxvt/Eterm. */
				    return KEY_F(14);
				case '8': /* Esc [ 2 8 ~ == F15 on VT220/VT320/
					   * Linux console/rxvt/Eterm. */
				    return KEY_F(15);
				case '9': /* Esc [ 2 9 ~ == F16 on VT220/VT320/
					   * Linux console/rxvt/Eterm. */
				    return KEY_F(16);
			    }
			} else if (seq_len > 2 && seq[2] == '~')
			    /* Esc [ 2 ~ == Insert on VT220/VT320/
			     * Linux console/xterm/Terminal. */
			    return KEY_IC;
			break;
		    case '3': /* Esc [ 3 ~ == Delete on VT220/VT320/
			       * Linux console/xterm/Terminal. */
			if (seq_len > 2 && seq[2] == '~')
			    return KEY_DC;
			break;
		    case '4': /* Esc [ 4 ~ == End on VT220/VT320/Linux
			       * console/xterm. */
			if (seq_len > 2 && seq[2] == '~')
			    return KEY_END;
			break;
		    case '5': /* Esc [ 5 ~ == PageUp on VT220/VT320/
			       * Linux console/xterm/Terminal;
			       * Esc [ 5 ^ == PageUp on Eterm. */
			if (seq_len > 2 && (seq[2] == '~' || seq[2] == '^'))
			    return KEY_PPAGE;
			break;
		    case '6': /* Esc [ 6 ~ == PageDown on VT220/VT320/
			       * Linux console/xterm/Terminal;
			       * Esc [ 6 ^ == PageDown on Eterm. */
			if (seq_len > 2 && (seq[2] == '~' || seq[2] == '^'))
			    return KEY_NPAGE;
			break;
		    case '7': /* Esc [ 7 ~ == Home on Eterm/rxvt,
			       * Esc [ 7 $ == Shift-Home on Eterm/rxvt. */
			if (seq_len > 2 && seq[2] == '~')
			    return KEY_HOME;
			else if (seq_len > 2 && seq[2] == '$')
			    return SHIFT_HOME;
			break;
		    case '8': /* Esc [ 8 ~ == End on Eterm/rxvt.
			       * Esc [ 8 $ == Shift-End on Eterm/rxvt. */
			if (seq_len > 2 && seq[2] == '~')
			    return KEY_END;
			else if (seq_len > 2 && seq[2] == '$')
			    return SHIFT_END;
			break;
		    case '9': /* Esc [ 9 == Delete on Mach console. */
			return KEY_DC;
		    case '@': /* Esc [ @ == Insert on Mach console. */
			return KEY_IC;
		    case 'A': /* Esc [ A == Up on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm/Terminal. */
		    case 'B': /* Esc [ B == Down on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm/Terminal. */
		    case 'C': /* Esc [ C == Right on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm/Terminal. */
		    case 'D': /* Esc [ D == Left on ANSI/VT220/Linux
			       * console/FreeBSD console/Mach console/
			       * rxvt/Eterm/Terminal. */
			return arrow_from_abcd(seq[1]);
		    case 'E': /* Esc [ E == Center (5) on numeric keypad
			       * with NumLock off on FreeBSD console/
			       * Terminal. */
			return KEY_B2;
		    case 'F': /* Esc [ F == End on FreeBSD console/Eterm. */
			return KEY_END;
		    case 'G': /* Esc [ G == PageDown on FreeBSD console. */
			return KEY_NPAGE;
		    case 'H': /* Esc [ H == Home on ANSI/VT220/FreeBSD
			       * console/Mach console/Eterm. */
			return KEY_HOME;
		    case 'I': /* Esc [ I == PageUp on FreeBSD console. */
			return KEY_PPAGE;
		    case 'L': /* Esc [ L == Insert on ANSI/FreeBSD console. */
			return KEY_IC;
		    case 'M': /* Esc [ M == F1 on FreeBSD console. */
			return KEY_F(1);
		    case 'N': /* Esc [ N == F2 on FreeBSD console. */
			return KEY_F(2);
		    case 'O':
			if (seq_len > 2) {
			    switch (seq[2]) {
				case 'P': /* Esc [ O P == F1 on xterm. */
				    return KEY_F(1);
				case 'Q': /* Esc [ O Q == F2 on xterm. */
				    return KEY_F(2);
				case 'R': /* Esc [ O R == F3 on xterm. */
				    return KEY_F(3);
				case 'S': /* Esc [ O S == F4 on xterm. */
				    return KEY_F(4);
			    }
			} else
			    /* Esc [ O == F3 on FreeBSD console. */
			    return KEY_F(3);
			break;
		    case 'P': /* Esc [ P == F4 on FreeBSD console. */
			return KEY_F(4);
		    case 'Q': /* Esc [ Q == F5 on FreeBSD console. */
			return KEY_F(5);
		    case 'R': /* Esc [ R == F6 on FreeBSD console. */
			return KEY_F(6);
		    case 'S': /* Esc [ S == F7 on FreeBSD console. */
			return KEY_F(7);
		    case 'T': /* Esc [ T == F8 on FreeBSD console. */
			return KEY_F(8);
		    case 'U': /* Esc [ U == PageDown on Mach console. */
			return KEY_NPAGE;
		    case 'V': /* Esc [ V == PageUp on Mach console. */
			return KEY_PPAGE;
		    case 'W': /* Esc [ W == F11 on FreeBSD console. */
			return KEY_F(11);
		    case 'X': /* Esc [ X == F12 on FreeBSD console. */
			return KEY_F(12);
		    case 'Y': /* Esc [ Y == End on Mach console. */
			return KEY_END;
		    case 'Z': /* Esc [ Z == F14 on FreeBSD console. */
			return KEY_F(14);
		    case 'a': /* Esc [ a == Shift-Up on rxvt/Eterm. */
		    case 'b': /* Esc [ b == Shift-Down on rxvt/Eterm. */
		    case 'c': /* Esc [ c == Shift-Right on rxvt/Eterm. */
		    case 'd': /* Esc [ d == Shift-Left on rxvt/Eterm. */
			shift_held = TRUE;
			return arrow_from_abcd(seq[1]);
		    case '[':
			if (seq_len > 2 ) {
			    switch (seq[2]) {
				case 'A': /* Esc [ [ A == F1 on Linux
					   * console. */
				    return KEY_F(1);
				case 'B': /* Esc [ [ B == F2 on Linux
					   * console. */
				    return KEY_F(2);
				case 'C': /* Esc [ [ C == F3 on Linux
					   * console. */
				    return KEY_F(3);
				case 'D': /* Esc [ [ D == F4 on Linux
					   * console. */
				    return KEY_F(4);
				case 'E': /* Esc [ [ E == F5 on Linux
					   * console. */
				    return KEY_F(5);
			    }
			}
			break;
		}
		break;
	}
    }

    return ERR;
}

/* Return the equivalent arrow-key value for the first four letters
 * in the alphabet, common to many escape sequences. */
int arrow_from_abcd(int kbinput)
{
    switch (tolower(kbinput)) {
	case 'a':
	    return KEY_UP;
	case 'b':
	    return KEY_DOWN;
	case 'c':
	    return KEY_RIGHT;
	case 'd':
	    return KEY_LEFT;
	default:
	    return ERR;
    }
}

/* Interpret the escape sequence in the keystroke buffer, the first
 * character of which is kbinput.  Assume that the keystroke buffer
 * isn't empty, and that the initial escape has already been read in. */
int parse_escape_sequence(WINDOW *win, int kbinput)
{
    int retval, *seq;
    size_t seq_len;

    /* Put back the non-escape character, get the complete escape
     * sequence, translate the sequence into its corresponding key
     * value, and save that as the result. */
    unget_input(&kbinput, 1);
    seq_len = key_buffer_len;
    seq = get_input(NULL, seq_len);
    retval = convert_sequence(seq, seq_len);

    free(seq);

    /* If we got an unrecognized escape sequence, notify the user. */
    if (retval == ERR) {
	if (win == edit) {
	    /* TRANSLATORS: This refers to a sequence of escape codes
	     * (from the keyboard) that nano does not know about. */
	    statusline(ALERT, _("Unknown sequence"));
	    suppress_cursorpos = FALSE;
	    lastmessage = HUSH;
	    if (currmenu == MMAIN) {
		reset_cursor();
		curs_set(1);
	    }
	}
    }

#ifdef DEBUG
    fprintf(stderr, "parse_escape_sequence(): kbinput = %d, seq_len = %lu, retval = %d\n",
		kbinput, (unsigned long)seq_len, retval);
#endif

    return retval;
}

/* Translate a byte sequence: turn a three-digit decimal number (from
 * 000 to 255) into its corresponding byte value. */
int get_byte_kbinput(int kbinput)
{
    static int byte_digits = 0, byte = 0;
    int retval = ERR;

    /* Increment the byte digit counter. */
    byte_digits++;

    switch (byte_digits) {
	case 1:
	    /* First digit: This must be from zero to two.  Put it in
	     * the 100's position of the byte sequence holder. */
	    if ('0' <= kbinput && kbinput <= '2')
		byte = (kbinput - '0') * 100;
	    else
		/* This isn't the start of a byte sequence.  Return this
		 * character as the result. */
		retval = kbinput;
	    break;
	case 2:
	    /* Second digit: This must be from zero to five if the first
	     * was two, and may be any decimal value if the first was
	     * zero or one.  Put it in the 10's position of the byte
	     * sequence holder. */
	    if (('0' <= kbinput && kbinput <= '5') || (byte < 200 &&
		'6' <= kbinput && kbinput <= '9'))
		byte += (kbinput - '0') * 10;
	    else
		/* This isn't the second digit of a byte sequence.
		 * Return this character as the result. */
		retval = kbinput;
	    break;
	case 3:
	    /* Third digit: This must be from zero to five if the first
	     * was two and the second was five, and may be any decimal
	     * value otherwise.  Put it in the 1's position of the byte
	     * sequence holder. */
	    if (('0' <= kbinput && kbinput <= '5') || (byte < 250 &&
		'6' <= kbinput && kbinput <= '9')) {
		byte += kbinput - '0';
		/* The byte sequence is complete. */
		retval = byte;
	    } else
		/* This isn't the third digit of a byte sequence.
		 * Return this character as the result. */
		retval = kbinput;
	    break;
	default:
	    /* If there are more than three digits, return this
	     * character as the result.  (Maybe we should produce an
	     * error instead?) */
	    retval = kbinput;
	    break;
    }

    /* If we have a result, reset the byte digit counter and the byte
     * sequence holder. */
    if (retval != ERR) {
	byte_digits = 0;
	byte = 0;
    }

#ifdef DEBUG
    fprintf(stderr, "get_byte_kbinput(): kbinput = %d, byte_digits = %d, byte = %d, retval = %d\n", kbinput, byte_digits, byte, retval);
#endif

    return retval;
}

#ifdef ENABLE_UTF8
/* If the character in kbinput is a valid hexadecimal digit, multiply it
 * by factor and add the result to uni, and return ERR to signify okay. */
long add_unicode_digit(int kbinput, long factor, long *uni)
{
    if ('0' <= kbinput && kbinput <= '9')
	*uni += (kbinput - '0') * factor;
    else if ('a' <= tolower(kbinput) && tolower(kbinput) <= 'f')
	*uni += (tolower(kbinput) - 'a' + 10) * factor;
    else
	/* The character isn't hexadecimal; give it as the result. */
	return (long)kbinput;

    return ERR;
}

/* Translate a Unicode sequence: turn a six-digit hexadecimal number
 * (from 000000 to 10FFFF, case-insensitive) into its corresponding
 * multibyte value. */
long get_unicode_kbinput(WINDOW *win, int kbinput)
{
    static int uni_digits = 0;
    static long uni = 0;
    long retval = ERR;

    /* Increment the Unicode digit counter. */
    uni_digits++;

    switch (uni_digits) {
	case 1:
	    /* The first digit must be zero or one.  Put it in the
	     * 0x100000's position of the Unicode sequence holder.
	     * Otherwise, return the character itself as the result. */
	    if (kbinput == '0' || kbinput == '1')
		uni = (kbinput - '0') * 0x100000;
	    else
		retval = kbinput;
	    break;
	case 2:
	    /* The second digit must be zero if the first was one, but
	     * may be any hexadecimal value if the first was zero. */
	    if (kbinput == '0' || uni == 0)
		retval = add_unicode_digit(kbinput, 0x10000, &uni);
	    else
		retval = kbinput;
	    break;
	case 3:
	    /* Later digits may be any hexadecimal value. */
	    retval = add_unicode_digit(kbinput, 0x1000, &uni);
	    break;
	case 4:
	    retval = add_unicode_digit(kbinput, 0x100, &uni);
	    break;
	case 5:
	    retval = add_unicode_digit(kbinput, 0x10, &uni);
	    break;
	case 6:
	    retval = add_unicode_digit(kbinput, 0x1, &uni);
	    /* If also the sixth digit was a valid hexadecimal value, then
	     * the Unicode sequence is complete, so return it. */
	    if (retval == ERR)
		retval = uni;
	    break;
    }

    /* Show feedback only when editing, not when at a prompt. */
    if (retval == ERR && win == edit) {
	char partial[7] = "......";

	/* Construct the partial result, right-padding it with dots. */
	snprintf(partial, uni_digits + 1, "%06lX", uni);
	partial[uni_digits] = '.';

	/* TRANSLATORS: This is shown while a six-digit hexadecimal
	 * Unicode character code (%s) is being typed in. */
	statusline(HUSH, _("Unicode Input: %s"), partial);
    }

#ifdef DEBUG
    fprintf(stderr, "get_unicode_kbinput(): kbinput = %d, uni_digits = %d, uni = %ld, retval = %ld\n",
						kbinput, uni_digits, uni, retval);
#endif

    /* If we have an end result, reset the Unicode digit counter. */
    if (retval != ERR)
	uni_digits = 0;

    return retval;
}
#endif /* ENABLE_UTF8 */

/* Translate a control character sequence: turn an ASCII non-control
 * character into its corresponding control character. */
int get_control_kbinput(int kbinput)
{
    int retval;

    /* Ctrl-Space (Ctrl-2, Ctrl-@, Ctrl-`) */
    if (kbinput == ' ' || kbinput == '2')
	retval = 0;
    /* Ctrl-/ (Ctrl-7, Ctrl-_) */
    else if (kbinput == '/')
	retval = 31;
    /* Ctrl-3 (Ctrl-[, Esc) to Ctrl-7 (Ctrl-/, Ctrl-_) */
    else if ('3' <= kbinput && kbinput <= '7')
	retval = kbinput - 24;
    /* Ctrl-8 (Ctrl-?) */
    else if (kbinput == '8' || kbinput == '?')
	retval = DEL_CODE;
    /* Ctrl-@ (Ctrl-Space, Ctrl-2, Ctrl-`) to Ctrl-_ (Ctrl-/, Ctrl-7) */
    else if ('@' <= kbinput && kbinput <= '_')
	retval = kbinput - '@';
    /* Ctrl-` (Ctrl-2, Ctrl-Space, Ctrl-@) to Ctrl-~ (Ctrl-6, Ctrl-^) */
    else if ('`' <= kbinput && kbinput <= '~')
	retval = kbinput - '`';
    else
	retval = kbinput;

#ifdef DEBUG
    fprintf(stderr, "get_control_kbinput(): kbinput = %d, retval = %d\n", kbinput, retval);
#endif

    return retval;
}

/* Put the output-formatted characters in output back into the keystroke
 * buffer, so that they can be parsed and displayed as output again. */
void unparse_kbinput(char *output, size_t output_len)
{
    int *input;
    size_t i;

    if (output_len == 0)
	return;

    input = (int *)nmalloc(output_len * sizeof(int));

    for (i = 0; i < output_len; i++)
	input[i] = (int)output[i];

    unget_input(input, output_len);

    free(input);
}

/* Read in a stream of characters verbatim, and return the length of the
 * string in kbinput_len.  Assume nodelay(win) is FALSE. */
int *get_verbatim_kbinput(WINDOW *win, size_t *kbinput_len)
{
    int *retval;

    /* Turn off flow control characters if necessary so that we can type
     * them in verbatim, and turn the keypad off if necessary so that we
     * don't get extended keypad values. */
    if (ISSET(PRESERVE))
	disable_flow_control();
    if (!ISSET(REBIND_KEYPAD))
	keypad(win, FALSE);

    /* Read in one keycode, or one or two escapes. */
    retval = parse_verbatim_kbinput(win, kbinput_len);

    /* If the code is invalid in the current mode, discard it. */
    if ((*retval == '\n' && as_an_at) || (*retval == '\0' && !as_an_at)) {
	*kbinput_len = 0;
	beep();
    }

    /* Turn flow control characters back on if necessary and turn the
     * keypad back on if necessary now that we're done. */
    if (ISSET(PRESERVE))
	enable_flow_control();
    /* Use the global window pointers, because a resize may have freed
     * the data that the win parameter points to. */
    if (!ISSET(REBIND_KEYPAD)) {
	keypad(edit, TRUE);
	keypad(bottomwin, TRUE);
    }

    return retval;
}

/* Read in one control character (or an iTerm/Eterm/rxvt double Escape),
 * or convert a series of six digits into a Unicode codepoint.  Return
 * in count either 1 (for a control character or the first byte of a
 * multibyte sequence), or 2 (for an iTerm/Eterm/rxvt double Escape). */
int *parse_verbatim_kbinput(WINDOW *win, size_t *count)
{
    int *kbinput;

    /* Read in the first code. */
    while ((kbinput = get_input(win, 1)) == NULL)
	;

#ifndef NANO_TINY
    /* When the window was resized, abort and return nothing. */
    if (*kbinput == KEY_WINCH) {
	free(kbinput);
	*count = 0;
	return NULL;
    }
#endif

    *count = 1;

#ifdef ENABLE_UTF8
    if (using_utf8()) {
	/* Check whether the first code is a valid starter digit: 0 or 1. */
	long unicode = get_unicode_kbinput(win, *kbinput);

	/* If the first code isn't the digit 0 nor 1, put it back. */
	if (unicode != ERR)
	    unget_input(kbinput, 1);
	/* Otherwise, continue reading in digits until we have a complete
	 * Unicode value, and put back the corresponding byte(s). */
	else {
	    char *multibyte;
	    int onebyte, i;

	    while (unicode == ERR) {
		free(kbinput);
		while ((kbinput = get_input(win, 1)) == NULL)
		    ;
		unicode = get_unicode_kbinput(win, *kbinput);
	    }

	    /* Convert the Unicode value to a multibyte sequence. */
	    multibyte = make_mbchar(unicode, (int *)count);

	    /* Insert the multibyte sequence into the input buffer. */
	    for (i = *count; i > 0 ; i--) {
		onebyte = (unsigned char)multibyte[i - 1];
		unget_input(&onebyte, 1);
	    }

	    free(multibyte);
	}
    } else
#endif /* ENABLE_UTF8 */
	/* Put back the first code. */
	unget_input(kbinput, 1);

    free(kbinput);

    /* If this is an iTerm/Eterm/rxvt double escape, take both Escapes. */
    if (key_buffer_len > 3 && *key_buffer == ESC_CODE &&
		key_buffer[1] == ESC_CODE && key_buffer[2] == '[')
	*count = 2;

    return get_input(NULL, *count);
}

#ifndef DISABLE_MOUSE
/* Handle any mouse event that may have occurred.  We currently handle
 * releases/clicks of the first mouse button.  If allow_shortcuts is
 * TRUE, releasing/clicking on a visible shortcut will put back the
 * keystroke associated with that shortcut.  If NCURSES_MOUSE_VERSION is
 * at least 2, we also currently handle presses of the fourth mouse
 * button (upward rolls of the mouse wheel) by putting back the
 * keystrokes to move up, and presses of the fifth mouse button
 * (downward rolls of the mouse wheel) by putting back the keystrokes to
 * move down.  We also store the coordinates of a mouse event that needs
 * to be handled in mouse_x and mouse_y, relative to the entire screen.
 * Return -1 on error, 0 if the mouse event needs to be handled, 1 if
 * it's been handled by putting back keystrokes that need to be handled.
 * or 2 if it's been ignored.  Assume that KEY_MOUSE has already been
 * read in. */
int get_mouseinput(int *mouse_x, int *mouse_y, bool allow_shortcuts)
{
    MEVENT mevent;
    bool in_bottomwin;
    subnfunc *f;

    *mouse_x = -1;
    *mouse_y = -1;

    /* First, get the actual mouse event. */
    if (getmouse(&mevent) == ERR)
	return -1;

    /* Save the screen coordinates where the mouse event took place. */
    *mouse_x = mevent.x - margin;
    *mouse_y = mevent.y;

    in_bottomwin = wenclose(bottomwin, *mouse_y, *mouse_x);

    /* Handle releases/clicks of the first mouse button. */
    if (mevent.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
	/* If we're allowing shortcuts, the current shortcut list is
	 * being displayed on the last two lines of the screen, and the
	 * first mouse button was released on/clicked inside it, we need
	 * to figure out which shortcut was released on/clicked and put
	 * back the equivalent keystroke(s) for it. */
	if (allow_shortcuts && !ISSET(NO_HELP) && in_bottomwin) {
	    int i;
		/* The width of all the shortcuts, except for the last
		 * two, in the shortcut list in bottomwin. */
	    int j;
		/* The calculated index number of the clicked item. */
	    size_t number;
		/* The number of available shortcuts in the current menu. */

	    /* Translate the mouse event coordinates so that they're
	     * relative to bottomwin. */
	    wmouse_trafo(bottomwin, mouse_y, mouse_x, FALSE);

	    /* Handle releases/clicks of the first mouse button on the
	     * statusbar elsewhere. */
	    if (*mouse_y == 0) {
		/* Restore the untranslated mouse event coordinates, so
		 * that they're relative to the entire screen again. */
		*mouse_x = mevent.x - margin;
		*mouse_y = mevent.y;

		return 0;
	    }

	    /* Determine how many shortcuts are being shown. */
	    number = length_of_list(currmenu);

	    if (number > MAIN_VISIBLE)
		number = MAIN_VISIBLE;

	    /* Calculate the width of all of the shortcuts in the list
	     * except for the last two, which are longer by (COLS % i)
	     * columns so as to not waste space. */
	    if (number < 2)
		i = COLS / (MAIN_VISIBLE / 2);
	    else
		i = COLS / ((number / 2) + (number % 2));

	    /* Calculate the one-based index in the shortcut list. */
	    j = (*mouse_x / i) * 2 + *mouse_y;

	    /* Adjust the index if we hit the last two wider ones. */
	    if ((j > number) && (*mouse_x % i < COLS % i))
		j -= 2;
#ifdef DEBUG
	    fprintf(stderr, "Calculated %i as index in shortcut list, currmenu = %x.\n", j, currmenu);
#endif
	    /* Ignore releases/clicks of the first mouse button beyond
	     * the last shortcut. */
	    if (j > number)
		return 2;

	    /* Go through the list of functions to determine which
	     * shortcut in the current menu we released/clicked on. */
	    for (f = allfuncs; f != NULL; f = f->next) {
		if ((f->menus & currmenu) == 0)
		    continue;
		if (first_sc_for(currmenu, f->scfunc) == NULL)
		    continue;
		/* Tick off an actually shown shortcut. */
		j -= 1;
		if (j == 0)
		    break;
	    }
#ifdef DEBUG
	    fprintf(stderr, "Stopped on func %ld present in menus %x\n", (long)f->scfunc, f->menus);
#endif

	    /* And put the corresponding key into the keyboard buffer. */
	    if (f != NULL) {
		const sc *s = first_sc_for(currmenu, f->scfunc);
		unget_kbinput(s->keycode, s->meta);
	    }
	    return 1;
	} else
	    /* Handle releases/clicks of the first mouse button that
	     * aren't on the current shortcut list elsewhere. */
	    return 0;
    }
#if NCURSES_MOUSE_VERSION >= 2
    /* Handle presses of the fourth mouse button (upward rolls of the
     * mouse wheel) and presses of the fifth mouse button (downward
     * rolls of the mouse wheel) . */
    else if (mevent.bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED)) {
	bool in_edit = wenclose(edit, *mouse_y, *mouse_x);

	if (in_bottomwin)
	    /* Translate the mouse event coordinates so that they're
	     * relative to bottomwin. */
	    wmouse_trafo(bottomwin, mouse_y, mouse_x, FALSE);

	if (in_edit || (in_bottomwin && *mouse_y == 0)) {
	    int i;

	    /* One upward roll of the mouse wheel is equivalent to
	     * moving up three lines, and one downward roll of the mouse
	     * wheel is equivalent to moving down three lines. */
	    for (i = 0; i < 3; i++)
		unget_kbinput((mevent.bstate & BUTTON4_PRESSED) ?
				KEY_PPAGE : KEY_NPAGE, FALSE);

	    return 1;
	} else
	    /* Ignore presses of the fourth mouse button and presses of
	     * the fifth mouse buttons that aren't on the edit window or
	     * the statusbar. */
	    return 2;
    }
#endif

    /* Ignore all other mouse events. */
    return 2;
}
#endif /* !DISABLE_MOUSE */

/* Return the shortcut that corresponds to the values of kbinput (the
 * key itself) and meta_key (whether the key is a meta sequence).  The
 * returned shortcut will be the first in the list that corresponds to
 * the given sequence. */
const sc *get_shortcut(int *kbinput)
{
    sc *s;

#ifdef DEBUG
    fprintf(stderr, "get_shortcut(): kbinput = %d, meta_key = %s -- ",
				*kbinput, meta_key ? "TRUE" : "FALSE");
#endif

    for (s = sclist; s != NULL; s = s->next) {
	if ((s->menus & currmenu) && *kbinput == s->keycode &&
					meta_key == s->meta) {
#ifdef DEBUG
	    fprintf (stderr, "matched seq '%s'  (menu is %x from %x)\n",
				s->keystr, currmenu, s->menus);
#endif
	    return s;
	}
    }
#ifdef DEBUG
    fprintf (stderr, "matched nothing\n");
#endif

    return NULL;
}

/* Move to (x, y) in win, and display a line of n spaces with the
 * current attributes. */
void blank_row(WINDOW *win, int y, int x, int n)
{
    wmove(win, y, x);

    for (; n > 0; n--)
	waddch(win, ' ');
}

/* Blank the first line of the top portion of the window. */
void blank_titlebar(void)
{
    blank_row(topwin, 0, 0, COLS);
}

/* Blank all the lines of the middle portion of the window, i.e. the
 * edit window. */
void blank_edit(void)
{
    int row;

    for (row = 0; row < editwinrows; row++)
	blank_row(edit, row, 0, COLS);
}

/* Blank the first line of the bottom portion of the window. */
void blank_statusbar(void)
{
    blank_row(bottomwin, 0, 0, COLS);
}

/* If the NO_HELP flag isn't set, blank the last two lines of the bottom
 * portion of the window. */
void blank_bottombars(void)
{
    if (!ISSET(NO_HELP) && LINES > 4) {
	blank_row(bottomwin, 1, 0, COLS);
	blank_row(bottomwin, 2, 0, COLS);
    }
}

/* Check if the number of keystrokes needed to blank the statusbar has
 * been pressed.  If so, blank the statusbar, unless constant cursor
 * position display is on and we are in the editing screen. */
void check_statusblank(void)
{
    if (statusblank == 0)
	return;

    statusblank--;

    /* When editing and 'constantshow' is active, skip the blanking. */
    if (currmenu == MMAIN && ISSET(CONST_UPDATE))
	return;

    if (statusblank == 0) {
	blank_statusbar();
	wnoutrefresh(bottomwin);
	reset_cursor();
	wnoutrefresh(edit);
    }

    /* If the subwindows overlap, make sure to show the edit window now. */
    if (LINES == 1)
	edit_refresh();
}

/* Convert buf into a string that can be displayed on screen.  The
 * caller wants to display buf starting with column start_col, and
 * extending for at most span columns.  start_col is zero-based.  span
 * is one-based, so span == 0 means you get "" returned.  The returned
 * string is dynamically allocated, and should be freed.  If isdata is
 * TRUE, the caller might put "$" at the beginning or end of the line if
 * it's too long. */
char *display_string(const char *buf, size_t start_col, size_t span,
	bool isdata)
{
    size_t start_index;
	/* Index in buf of the first character shown. */
    size_t column;
	/* Screen column that start_index corresponds to. */
    char *converted;
	/* The string we return. */
    size_t index;
	/* Current position in converted. */
    size_t beyond = start_col + span;
	/* The column number just beyond the last shown character. */

    start_index = actual_x(buf, start_col);
    column = strnlenpt(buf, start_index);

    assert(column <= start_col);

    index = 0;
#ifdef USING_OLD_NCURSES
    seen_wide = FALSE;
#endif
    buf += start_index;

    /* Allocate enough space for converting the relevant part of the line. */
    converted = charalloc(strlen(buf) * (mb_cur_max() + tabsize) + 1);

    /* If the first character starts before the left edge, or would be
     * overwritten by a "$" token, then show spaces instead. */
    if (*buf != '\0' && *buf != '\t' && (column < start_col ||
				(column > 0 && isdata && !ISSET(SOFTWRAP)))) {
	if (is_cntrl_mbchar(buf)) {
	    if (column < start_col) {
		converted[index++] = control_mbrep(buf, isdata);
		start_col++;
		buf += parse_mbchar(buf, NULL, NULL);
	    }
	}
#ifdef ENABLE_UTF8
	else if (using_utf8() && mbwidth(buf) == 2) {
	    if (column >= start_col) {
		converted[index++] = ' ';
		start_col++;
	    }

	    converted[index++] = ' ';
	    start_col++;

	    buf += parse_mbchar(buf, NULL, NULL);
	}
#endif
    }

    while (*buf != '\0' && start_col < beyond) {
	int charlength, charwidth = 1;

	if (*buf == ' ') {
	    /* Show a space as a visible character, or as a space. */
#ifndef NANO_TINY
	    if (ISSET(WHITESPACE_DISPLAY)) {
		int i = whitespace_len[0];

		while (i < whitespace_len[0] + whitespace_len[1])
		    converted[index++] = whitespace[i++];
	    } else
#endif
		converted[index++] = ' ';
	    start_col++;
	    buf++;
	    continue;
	} else if (*buf == '\t') {
	    /* Show a tab as a visible character, or as as a space. */
#ifndef NANO_TINY
	    if (ISSET(WHITESPACE_DISPLAY)) {
		int i = 0;

		while (i < whitespace_len[0])
		    converted[index++] = whitespace[i++];
	    } else
#endif
		converted[index++] = ' ';
	    start_col++;
	    /* Fill the tab up with the required number of spaces. */
	    while (start_col % tabsize != 0) {
		converted[index++] = ' ';
		start_col++;
	    }
	    buf++;
	    continue;
	}

	charlength = length_of_char(buf, &charwidth);

	/* If buf contains a control character, represent it. */
	if (is_cntrl_mbchar(buf)) {
	    converted[index++] = '^';
	    converted[index++] = control_mbrep(buf, isdata);
	    start_col += 2;
	    buf += charlength;
	    continue;
	}

	/* If buf contains a valid non-control character, simply copy it. */
	if (charlength > 0) {
	    for (; charlength > 0; charlength--)
		converted[index++] = *(buf++);

	    start_col += charwidth;
#ifdef USING_OLD_NCURSES
	    if (charwidth > 1)
		seen_wide = TRUE;
#endif
	    continue;
	}

	/* Represent an invalid sequence with the Replacement Character. */
	converted[index++] = '\xEF';
	converted[index++] = '\xBF';
	converted[index++] = '\xBD';

	start_col += 1;
	buf++;

	/* For invalid codepoints, skip extra bytes. */
	if (charlength < -1)
	   buf += charlength + 7;
    }

    /* If there is more text than can be shown, make room for the $. */
    if (*buf != '\0' && isdata && !ISSET(SOFTWRAP))
	index = move_mbleft(converted, index);

    /* Null-terminate the converted string. */
    converted[index] = '\0';

    return converted;
}

/* If path is NULL, we're in normal editing mode, so display the current
 * version of nano, the current filename, and whether the current file
 * has been modified on the titlebar.  If path isn't NULL, we're in the
 * file browser, and path contains the directory to start the file
 * browser in, so display the current version of nano and the contents
 * of path on the titlebar. */
void titlebar(const char *path)
{
    size_t verlen, prefixlen, pathlen, statelen;
	/* The width of the different titlebar elements, in columns. */
    size_t pluglen = 0;
	/* The width that "Modified" would take up. */
    size_t offset = 0;
	/* The position at which the center part of the titlebar starts. */
    const char *prefix = "";
	/* What is shown before the path -- "File:", "DIR:", or "". */
    const char *state = "";
	/* The state of the current buffer -- "Modified", "View", or "". */
    char *caption;
	/* The presentable form of the pathname. */

    /* If the screen is too small, there is no titlebar. */
    if (topwin == NULL)
	return;

    assert(path != NULL || openfile->filename != NULL);

    wattron(topwin, interface_color_pair[TITLE_BAR]);

    blank_titlebar();
    as_an_at = FALSE;

    /* Do as Pico: if there is not enough width available for all items,
     * first sacrifice the version string, then eat up the side spaces,
     * then sacrifice the prefix, and only then start dottifying. */

    /* Figure out the path, prefix and state strings. */
#ifndef DISABLE_BROWSER
    if (path != NULL)
	prefix = _("DIR:");
    else
#endif
    {
	if (openfile->filename[0] == '\0')
	    path = _("New Buffer");
	else {
	    path = openfile->filename;
	    prefix = _("File:");
	}

	if (openfile->modified)
	    state = _("Modified");
	else if (ISSET(VIEW_MODE))
	    state = _("View");

	pluglen = strlenpt(_("Modified")) + 1;
    }

    /* Determine the widths of the four elements, including their padding. */
    verlen = strlenpt(BRANDING) + 3;
    prefixlen = strlenpt(prefix);
    if (prefixlen > 0)
	prefixlen++;
    pathlen= strlenpt(path);
    statelen = strlenpt(state) + 2;
    if (statelen > 2) {
	pathlen++;
	pluglen = 0;
    }

    /* Only print the version message when there is room for it. */
    if (verlen + prefixlen + pathlen + pluglen + statelen <= COLS)
	mvwaddstr(topwin, 0, 2, BRANDING);
    else {
	verlen = 2;
	/* If things don't fit yet, give up the placeholder. */
	if (verlen + prefixlen + pathlen + pluglen + statelen > COLS)
	    pluglen = 0;
	/* If things still don't fit, give up the side spaces. */
	if (verlen + prefixlen + pathlen + pluglen + statelen > COLS) {
	    verlen = 0;
	    statelen -= 2;
	}
    }

    /* If we have side spaces left, center the path name. */
    if (verlen > 0)
	offset = verlen + (COLS - (verlen + pluglen + statelen) -
					(prefixlen + pathlen)) / 2;

    /* Only print the prefix when there is room for it. */
    if (verlen + prefixlen + pathlen + pluglen + statelen <= COLS) {
	mvwaddstr(topwin, 0, offset, prefix);
	if (prefixlen > 0)
	    waddstr(topwin, " ");
    } else
	wmove(topwin, 0, offset);

    /* Print the full path if there's room; otherwise, dottify it. */
    if (pathlen + pluglen + statelen <= COLS) {
	caption = display_string(path, 0, pathlen, FALSE);
	waddstr(topwin, caption);
	free(caption);
    } else if (5 + statelen <= COLS) {
	waddstr(topwin, "...");
	caption = display_string(path, 3 + pathlen - COLS + statelen,
					COLS - statelen, FALSE);
	waddstr(topwin, caption);
	free(caption);
    }

    /* Right-align the state if there's room; otherwise, trim it. */
    if (statelen > 0 && statelen <= COLS)
	mvwaddstr(topwin, 0, COLS - statelen, state);
    else if (statelen > 0)
	mvwaddnstr(topwin, 0, 0, state, actual_x(state, COLS));

    wattroff(topwin, interface_color_pair[TITLE_BAR]);

    wnoutrefresh(topwin);
    reset_cursor();
    wnoutrefresh(edit);
}

/* Display a normal message on the statusbar, quietly. */
void statusbar(const char *msg)
{
    statusline(HUSH, msg);
}

/* Warn the user on the statusbar and pause for a moment, so that the
 * message can be noticed and read. */
void warn_and_shortly_pause(const char *msg)
{
    statusbar(msg);
    beep();
    napms(1800);
}

/* Display a message on the statusbar, and set suppress_cursorpos to
 * TRUE, so that the message won't be immediately overwritten if
 * constant cursor position display is on. */
void statusline(message_type importance, const char *msg, ...)
{
    va_list ap;
    static int alerts = 0;
    char *compound, *message;
    size_t start_col;
    bool bracketed;
#ifndef NANO_TINY
    bool old_whitespace = ISSET(WHITESPACE_DISPLAY);

    UNSET(WHITESPACE_DISPLAY);
#endif

    va_start(ap, msg);

    /* Curses mode is turned off.  If we use wmove() now, it will muck
     * up the terminal settings.  So we just use vfprintf(). */
    if (isendwin()) {
	vfprintf(stderr, msg, ap);
	va_end(ap);
	return;
    }

    /* If there already was an alert message, ignore lesser ones. */
    if ((lastmessage == ALERT && importance != ALERT) ||
		(lastmessage == MILD && importance == HUSH))
	return;

    /* If the ALERT status has been reset, reset the counter. */
    if (lastmessage == HUSH)
	alerts = 0;

    /* Shortly pause after each of the first three alert messages,
     * to give the user time to read them. */
    if (lastmessage == ALERT && alerts < 4 && !ISSET(NO_PAUSES))
	napms(1200);

    if (importance == ALERT) {
	if (++alerts > 3 && !ISSET(NO_PAUSES))
	    msg = _("Further warnings were suppressed");
	beep();
    }

    lastmessage = importance;

    /* Turn the cursor off while fiddling in the statusbar. */
    curs_set(0);

    blank_statusbar();

    /* Construct the message out of all the arguments. */
    compound = charalloc(mb_cur_max() * (COLS + 1));
    vsnprintf(compound, mb_cur_max() * (COLS + 1), msg, ap);
    va_end(ap);
    message = display_string(compound, 0, COLS, FALSE);
    free(compound);

    start_col = (COLS - strlenpt(message)) / 2;
    bracketed = (start_col > 1);

    wmove(bottomwin, 0, (bracketed ? start_col - 2 : start_col));
    wattron(bottomwin, interface_color_pair[STATUS_BAR]);
    if (bracketed)
	waddstr(bottomwin, "[ ");
    waddstr(bottomwin, message);
    free(message);
    if (bracketed)
	waddstr(bottomwin, " ]");
    wattroff(bottomwin, interface_color_pair[STATUS_BAR]);

    /* Push the message to the screen straightaway. */
    wnoutrefresh(bottomwin);
    doupdate();

    suppress_cursorpos = TRUE;

#ifndef NANO_TINY
    if (old_whitespace)
	SET(WHITESPACE_DISPLAY);

    /* If doing quick blanking, blank the statusbar after just one keystroke.
     * Otherwise, blank it after twenty-six keystrokes, as Pico does. */
    if (ISSET(QUICK_BLANK))
	statusblank = 1;
    else
#endif
	statusblank = 26;
}

/* Display the shortcut list corresponding to menu on the last two rows
 * of the bottom portion of the window. */
void bottombars(int menu)
{
    size_t number, itemwidth, i;
    subnfunc *f;
    const sc *s;

    /* Set the global variable to the given menu. */
    currmenu = menu;

    if (ISSET(NO_HELP) || LINES < 5)
	return;

    /* Determine how many shortcuts there are to show. */
    number = length_of_list(menu);

    if (number > MAIN_VISIBLE)
	number = MAIN_VISIBLE;

    /* Compute the width of each keyname-plus-explanation pair. */
    itemwidth = COLS / ((number / 2) + (number % 2));

    /* If there is no room, don't print anything. */
    if (itemwidth == 0)
	return;

    blank_bottombars();

#ifdef DEBUG
    fprintf(stderr, "In bottombars, number of items == \"%d\"\n", (int) number);
#endif

    for (f = allfuncs, i = 0; i < number && f != NULL; f = f->next) {
#ifdef DEBUG
	fprintf(stderr, "Checking menu items....");
#endif
	if ((f->menus & menu) == 0)
	    continue;

#ifdef DEBUG
	fprintf(stderr, "found one! f->menus = %x, desc = \"%s\"\n", f->menus, f->desc);
#endif
	s = first_sc_for(menu, f->scfunc);
	if (s == NULL) {
#ifdef DEBUG
	    fprintf(stderr, "Whoops, guess not, no shortcut key found for func!\n");
#endif
	    continue;
	}

	wmove(bottomwin, 1 + i % 2, (i / 2) * itemwidth);
#ifdef DEBUG
	fprintf(stderr, "Calling onekey with keystr \"%s\" and desc \"%s\"\n", s->keystr, f->desc);
#endif
	onekey(s->keystr, _(f->desc), itemwidth + (COLS % itemwidth));
	i++;
    }

    /* Defeat a VTE bug by moving the cursor and forcing a screen update. */
    wmove(bottomwin, 0, 0);
    wnoutrefresh(bottomwin);
    doupdate();

    reset_cursor();
    wnoutrefresh(edit);
}

/* Write a shortcut key to the help area at the bottom of the window.
 * keystroke is e.g. "^G" and desc is e.g. "Get Help".  We are careful
 * to write at most length characters, even if length is very small and
 * keystroke and desc are long.  Note that waddnstr(,,(size_t)-1) adds
 * the whole string!  We do not bother padding the entry with blanks. */
void onekey(const char *keystroke, const char *desc, int length)
{
    assert(keystroke != NULL && desc != NULL);

    wattron(bottomwin, interface_color_pair[KEY_COMBO]);
    waddnstr(bottomwin, keystroke, actual_x(keystroke, length));
    wattroff(bottomwin, interface_color_pair[KEY_COMBO]);

    length -= strlenpt(keystroke) + 1;

    if (length > 0) {
	waddch(bottomwin, ' ');
	wattron(bottomwin, interface_color_pair[FUNCTION_TAG]);
	waddnstr(bottomwin, desc, actual_x(desc, length));
	wattroff(bottomwin, interface_color_pair[FUNCTION_TAG]);
    }
}

/* Redetermine current_y from the position of current relative to edittop,
 * and put the cursor in the edit window at (current_y, "current_x"). */
void reset_cursor(void)
{
    ssize_t row = 0;
    size_t col, xpt = xplustabs();

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	filestruct *line = openfile->edittop;

	row -= (openfile->firstcolumn / editwincols);

	/* Calculate how many rows the lines from edittop to current use. */
	while (line != NULL && line != openfile->current) {
	    row += strlenpt(line->data) / editwincols + 1;
	    line = line->next;
	}

	/* Add the number of wraps in the current line before the cursor. */
	row += xpt / editwincols;
	col = xpt % editwincols;
    } else
#endif
    {
	row = openfile->current->lineno - openfile->edittop->lineno;
	col = xpt - get_page_start(xpt);
    }

    if (row < editwinrows)
	wmove(edit, row, margin + col);

    openfile->current_y = row;
}

/* edit_draw() takes care of the job of actually painting a line into
 * the edit window.  fileptr is the line to be painted, at row row of
 * the window.  converted is the actual string to be written to the
 * window, with tabs and control characters replaced by strings of
 * regular characters.  from_col is the column number of the first
 * character of this page.  That is, the first character of converted
 * corresponds to character number actual_x(fileptr->data, from_col) of the
 * line. */
void edit_draw(filestruct *fileptr, const char *converted,
	int row, size_t from_col)
{
#if !defined(NANO_TINY) || !defined(DISABLE_COLOR)
    size_t from_x = actual_x(fileptr->data, from_col);
	/* The position in fileptr->data of the leftmost character
	 * that displays at least partially on the window. */
    size_t till_x = actual_x(fileptr->data, from_col + editwincols - 1) + 1;
	/* The position in fileptr->data of the first character that is
	 * completely off the window to the right.  Note that till_x
	 * might be beyond the null terminator of the string. */
#endif

    assert(openfile != NULL && fileptr != NULL && converted != NULL);
    assert(strlenpt(converted) <= editwincols);

#ifdef ENABLE_LINENUMBERS
    /* If line numbering is switched on, put a line number in front of
     * the text -- but only for the parts that are not softwrapped. */
    if (margin > 0) {
	wattron(edit, interface_color_pair[LINE_NUMBER]);
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP) && from_x != 0)
	    mvwprintw(edit, row, 0, "%*s", margin - 1, " ");
	else
#endif
	    mvwprintw(edit, row, 0, "%*ld", margin - 1, (long)fileptr->lineno);
	wattroff(edit, interface_color_pair[LINE_NUMBER]);
    }
#endif

    /* First simply write the converted line -- afterward we'll add colors
     * and the marking highlight on just the pieces that need it. */
    mvwaddstr(edit, row, margin, converted);

#ifdef USING_OLD_NCURSES
    /* Tell ncurses to really redraw the line without trying to optimize
     * for what it thinks is already there, because it gets it wrong in
     * the case of a wide character in column zero.  See bug #31743. */
    if (seen_wide)
	wredrawln(edit, row, 1);
#endif

#ifndef DISABLE_COLOR
    /* If color syntaxes are available and turned on, apply them. */
    if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX)) {
	const colortype *varnish = openfile->colorstrings;

	/* If there are multiline regexes, make sure there is a cache. */
	if (openfile->syntax->nmultis > 0)
	    alloc_multidata_if_needed(fileptr);

	/* Iterate through all the coloring regexes. */
	for (; varnish != NULL; varnish = varnish->next) {
	    size_t index = 0;
		/* Where in the line we currently begin looking for a match. */
	    int start_col;
		/* The starting column of a piece to paint.  Zero-based. */
	    int paintlen = 0;
		/* The number of characters to paint. */
	    const char *thetext;
		/* The place in converted from where painting starts. */
	    regmatch_t match;
		/* The match positions of a single-line regex. */

	    /* Two notes about regexec().  A return value of zero means
	     * that there is a match.  Also, rm_eo is the first
	     * non-matching character after the match. */

	    wattron(edit, varnish->attributes);

	    /* First case: varnish is a single-line expression. */
	    if (varnish->end == NULL) {
		/* We increment index by rm_eo, to move past the end of the
		 * last match.  Even though two matches may overlap, we
		 * want to ignore them, so that we can highlight e.g. C
		 * strings correctly. */
		while (index < till_x) {
		    /* Note the fifth parameter to regexec().  It says
		     * not to match the beginning-of-line character
		     * unless index is zero.  If regexec() returns
		     * REG_NOMATCH, there are no more matches in the
		     * line. */
		    if (regexec(varnish->start, &fileptr->data[index], 1,
				&match, (index == 0) ? 0 : REG_NOTBOL) != 0)
			break;

		    /* If the match is of length zero, skip it. */
		    if (match.rm_so == match.rm_eo) {
			index = move_mbright(fileptr->data,
						index + match.rm_eo);
			continue;
		    }

		    /* Translate the match to the beginning of the line. */
		    match.rm_so += index;
		    match.rm_eo += index;
		    index = match.rm_eo;

		    /* If the matching part is not visible, skip it. */
		    if (match.rm_eo <= from_x || match.rm_so >= till_x)
			continue;

		    start_col = (match.rm_so <= from_x) ?
					0 : strnlenpt(fileptr->data,
					match.rm_so) - from_col;

		    thetext = converted + actual_x(converted, start_col);

		    paintlen = actual_x(thetext, strnlenpt(fileptr->data,
					match.rm_eo) - from_col - start_col);

		    mvwaddnstr(edit, row, margin + start_col,
						thetext, paintlen);
		}
		goto tail_of_loop;
	    }

	    /* Second case: varnish is a multiline expression. */
	    const filestruct *start_line = fileptr->prev;
		/* The first line before fileptr that matches 'start'. */
	    const filestruct *end_line = fileptr;
		/* The line that matches 'end'. */
	    regmatch_t startmatch, endmatch;
		/* The match positions of the start and end regexes. */

	    /* Assume nothing gets painted until proven otherwise below. */
	    fileptr->multidata[varnish->id] = CNONE;

	    /* First check the multidata of the preceding line -- it tells
	     * us about the situation so far, and thus what to do here. */
	    if (start_line != NULL && start_line->multidata != NULL) {
		if (start_line->multidata[varnish->id] == CWHOLELINE ||
			start_line->multidata[varnish->id] == CENDAFTER ||
			start_line->multidata[varnish->id] == CWOULDBE)
		    goto seek_an_end;
		if (start_line->multidata[varnish->id] == CNONE ||
			start_line->multidata[varnish->id] == CBEGINBEFORE ||
			start_line->multidata[varnish->id] == CSTARTENDHERE)
		    goto step_two;
	    }

	    /* The preceding line has no precalculated multidata.  So, do
	     * some backtracking to find out what to paint. */

	    /* First step: see if there is a line before current that
	     * matches 'start' and is not complemented by an 'end'. */
	    while (start_line != NULL && regexec(varnish->start,
		    start_line->data, 1, &startmatch, 0) == REG_NOMATCH) {
		/* There is no start on this line; but if there is an end,
		 * there is no need to look for starts on earlier lines. */
		if (regexec(varnish->end, start_line->data, 0, NULL, 0) == 0)
		    goto step_two;
		start_line = start_line->prev;
	    }

	    /* If no start was found, skip to the next step. */
	    if (start_line == NULL)
		goto step_two;

	    /* If a found start has been qualified as an end earlier,
	     * believe it and skip to the next step. */
	    if (start_line->multidata != NULL &&
			(start_line->multidata[varnish->id] == CBEGINBEFORE ||
			start_line->multidata[varnish->id] == CSTARTENDHERE))
		goto step_two;

	    /* Is there an uncomplemented start on the found line? */
	    while (TRUE) {
		/* Begin searching for an end after the start match. */
		index += startmatch.rm_eo;
		/* If there is no end after this last start, good. */
		if (regexec(varnish->end, start_line->data + index, 1, &endmatch,
				(index == 0) ? 0 : REG_NOTBOL) == REG_NOMATCH)
		    break;
		/* Begin searching for a new start after the end match. */
		index += endmatch.rm_eo;
		/* If both start and end match are mere anchors, advance. */
		if (startmatch.rm_so == startmatch.rm_eo &&
				endmatch.rm_so == endmatch.rm_eo) {
		    if (start_line->data[index] == '\0')
			break;
		    index = move_mbright(start_line->data, index);
		}
		/* If there is no later start on this line, next step. */
		if (regexec(varnish->start, start_line->data + index,
				1, &startmatch, REG_NOTBOL) == REG_NOMATCH)
		    goto step_two;
	    }
	    /* Indeed, there is a start without an end on that line. */

  seek_an_end:
	    /* We've already checked that there is no end between the start
	     * and the current line.  But is there an end after the start
	     * at all?  We don't paint unterminated starts. */
	    while (end_line != NULL && regexec(varnish->end, end_line->data,
				 1, &endmatch, 0) == REG_NOMATCH)
		end_line = end_line->next;

	    /* If there is no end, there is nothing to paint. */
	    if (end_line == NULL) {
		fileptr->multidata[varnish->id] = CWOULDBE;
		goto tail_of_loop;
	    }

	    /* If the end is on a later line, paint whole line, and be done. */
	    if (end_line != fileptr) {
		mvwaddnstr(edit, row, margin, converted, -1);
		fileptr->multidata[varnish->id] = CWHOLELINE;
		goto tail_of_loop;
	    }

	    /* Only if it is visible, paint the part to be coloured. */
	    if (endmatch.rm_eo > from_x) {
		paintlen = actual_x(converted, strnlenpt(fileptr->data,
						endmatch.rm_eo) - from_col);
		mvwaddnstr(edit, row, margin, converted, paintlen);
	    }
	    fileptr->multidata[varnish->id] = CBEGINBEFORE;

  step_two:
	    /* Second step: look for starts on this line, but begin
	     * looking only after an end match, if there is one. */
	    index = (paintlen == 0) ? 0 : endmatch.rm_eo;

	    while (regexec(varnish->start, fileptr->data + index,
				1, &startmatch, (index == 0) ?
				0 : REG_NOTBOL) == 0) {
		/* Translate the match to be relative to the
		 * beginning of the line. */
		startmatch.rm_so += index;
		startmatch.rm_eo += index;

		start_col = (startmatch.rm_so <= from_x) ?
				0 : strnlenpt(fileptr->data,
				startmatch.rm_so) - from_col;

		thetext = converted + actual_x(converted, start_col);

		if (regexec(varnish->end, fileptr->data + startmatch.rm_eo,
				1, &endmatch, (startmatch.rm_eo == 0) ?
				0 : REG_NOTBOL) == 0) {
		    /* Translate the end match to be relative to
		     * the beginning of the line. */
		    endmatch.rm_so += startmatch.rm_eo;
		    endmatch.rm_eo += startmatch.rm_eo;
		    /* Only paint the match if it is visible on screen and
		     * it is more than zero characters long. */
		    if (endmatch.rm_eo > from_x &&
					endmatch.rm_eo > startmatch.rm_so) {
			paintlen = actual_x(thetext, strnlenpt(fileptr->data,
					endmatch.rm_eo) - from_col - start_col);

			mvwaddnstr(edit, row, margin + start_col,
						thetext, paintlen);

			fileptr->multidata[varnish->id] = CSTARTENDHERE;
		    }
		    index = endmatch.rm_eo;
		    /* If both start and end match are anchors, advance. */
		    if (startmatch.rm_so == startmatch.rm_eo &&
				endmatch.rm_so == endmatch.rm_eo) {
			if (fileptr->data[index] == '\0')
			    break;
			index = move_mbright(fileptr->data, index);
		    }
		    continue;
		}

		/* There is no end on this line.  But maybe on later lines? */
		end_line = fileptr->next;

		while (end_line != NULL && regexec(varnish->end, end_line->data,
					0, NULL, 0) == REG_NOMATCH)
		    end_line = end_line->next;

		/* If there is no end, we're done with this regex. */
		if (end_line == NULL) {
		    fileptr->multidata[varnish->id] = CWOULDBE;
		    break;
		}

		/* Paint the rest of the line, and we're done. */
		mvwaddnstr(edit, row, margin + start_col, thetext, -1);
		fileptr->multidata[varnish->id] = CENDAFTER;
		break;
	    }
  tail_of_loop:
	    wattroff(edit, varnish->attributes);
	}
    }
#endif /* !DISABLE_COLOR */

#ifndef NANO_TINY
    /* If the mark is on, and fileptr is at least partially selected, we
     * need to paint it. */
    if (openfile->mark_set &&
		(fileptr->lineno <= openfile->mark_begin->lineno ||
		fileptr->lineno <= openfile->current->lineno) &&
		(fileptr->lineno >= openfile->mark_begin->lineno ||
		fileptr->lineno >= openfile->current->lineno)) {
	const filestruct *top, *bot;
	    /* The lines where the marked region begins and ends. */
	size_t top_x, bot_x;
	    /* The x positions where the marked region begins and ends. */
	int start_col;
	    /* The column where painting starts.  Zero-based. */
	const char *thetext;
	    /* The place in converted from where painting starts. */
	int paintlen = -1;
	    /* The number of characters to paint.  Negative means "all". */

	mark_order(&top, &top_x, &bot, &bot_x, NULL);

	if (top->lineno < fileptr->lineno || top_x < from_x)
	    top_x = from_x;
	if (bot->lineno > fileptr->lineno || bot_x > till_x)
	    bot_x = till_x;

	/* Only paint if the marked part of the line is on this page. */
	if (top_x < till_x && bot_x > from_x) {
	    /* Compute on which screen column to start painting. */
	    start_col = strnlenpt(fileptr->data, top_x) - from_col;

	    if (start_col < 0)
		start_col = 0;

	    thetext = converted + actual_x(converted, start_col);

	    /* If the end of the mark is onscreen, compute how many
	     * characters to paint.  Otherwise, just paint all. */
	    if (bot_x < till_x) {
		size_t end_col = strnlenpt(fileptr->data, bot_x) - from_col;
		paintlen = actual_x(thetext, end_col - start_col);
	    }

	    wattron(edit, hilite_attribute);
	    mvwaddnstr(edit, row, margin + start_col, thetext, paintlen);
	    wattroff(edit, hilite_attribute);
	}
    }
#endif /* !NANO_TINY */
}

/* Redraw the line at fileptr.  The line will be displayed so that the
 * character with the given index is visible -- if necessary, the line
 * will be horizontally scrolled.  In softwrap mode, however, the entire
 * line will be passed to update_softwrapped_line().  Likely values of
 * index are current_x or zero.  Return the number of additional rows
 * consumed (when softwrapping). */
int update_line(filestruct *fileptr, size_t index)
{
    int row = 0;
	/* The row in the edit window we will be updating. */
    char *converted;
	/* The data of the line with tabs and control characters expanded. */
    size_t from_col = 0;
	/* From which column a horizontally scrolled line is displayed. */

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP))
	return update_softwrapped_line(fileptr);
#endif

    row = fileptr->lineno - openfile->edittop->lineno;

    /* If the line is offscreen, don't even try to display it. */
    if (row < 0 || row >= editwinrows) {
	statusline(ALERT, "Badness: tried to display a line on row %i"
				" -- please report a bug", row);
	return 0;
    }

    /* First, blank out the row. */
    blank_row(edit, row, 0, COLS);

    /* Next, find out from which column to start displaying the line. */
    from_col = get_page_start(strnlenpt(fileptr->data, index));

    /* Expand the line, replacing tabs with spaces, and control
     * characters with their displayed forms. */
    converted = display_string(fileptr->data, from_col, editwincols, TRUE);

    /* Draw the line. */
    edit_draw(fileptr, converted, row, from_col);
    free(converted);

    if (from_col > 0)
	mvwaddch(edit, row, margin, '$');
    if (strlenpt(fileptr->data) > from_col + editwincols)
	mvwaddch(edit, row, COLS - 1, '$');

    return 1;
}

#ifndef NANO_TINY
/* Redraw all the chunks of the given line (as far as they fit onscreen),
 * unless it's edittop, which will be displayed from column firstcolumn.
 * Return the number of additional rows consumed. */
int update_softwrapped_line(filestruct *fileptr)
{
    int row = 0;
	/* The row in the edit window we will write to. */
    filestruct *line = openfile->edittop;
	/* An iterator needed to find the relevant row. */
    int starting_row;
	/* The first row in the edit window that gets updated. */
    size_t from_col = 0;
	/* The starting column of the current chunk. */
    char *converted;
	/* The data of the chunk with tabs and control characters expanded. */
    size_t full_length;
	/* The length of the expanded line. */

    if (fileptr == openfile->edittop)
	from_col = openfile->firstcolumn;
    else
	row -= (openfile->firstcolumn / editwincols);

    /* Find out on which screen row the target line should be shown. */
    while (line != fileptr && line != NULL) {
	row += (strlenpt(line->data) / editwincols) + 1;
	line = line->next;
    }

    /* If the first chunk is offscreen, don't even try to display it. */
    if (row < 0 || row >= editwinrows) {
	statusline(ALERT, "Badness: tried to display a chunk on row %i"
				" -- please report a bug", row);
	return 0;
    }

    full_length = strlenpt(fileptr->data);
    starting_row = row;

    while (from_col <= full_length && row < editwinrows) {
	blank_row(edit, row, 0, COLS);

	/* Convert the chunk to its displayable form and draw it. */
	converted = display_string(fileptr->data, from_col, editwincols, TRUE);
	edit_draw(fileptr, converted, row++, from_col);
	free(converted);

	from_col += editwincols;
    }

    return (row - starting_row);
}
#endif

/* Check whether the mark is on, or whether old_column and new_column are on
 * different "pages" (in softwrap mode, only the former applies), which means
 * that the relevant line needs to be redrawn. */
bool line_needs_update(const size_t old_column, const size_t new_column)
{
#ifndef NANO_TINY
    if (openfile->mark_set)
	return TRUE;
    else
#endif
	return (get_page_start(old_column) != get_page_start(new_column));
}

/* Try to move up nrows softwrapped chunks from the given line and the
 * given column (leftedge).  After moving, leftedge will be set to the
 * starting column of the current chunk.  Return the number of chunks we
 * couldn't move up, which will be zero if we completely succeeded. */
int go_back_chunks(int nrows, filestruct **line, size_t *leftedge)
{
    int i;

    /* Don't move more chunks than the window can hold. */
    if (nrows > editwinrows - 1)
	nrows = (editwinrows < 2) ? 1 : editwinrows - 1;

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	size_t current_chunk = (*leftedge) / editwincols;

	/* Recede through the requested number of chunks. */
	for (i = nrows; i > 0; i--) {
	    if (current_chunk > 0) {
		current_chunk--;
		continue;
	    }

	    if (*line == openfile->fileage)
		break;

	    *line = (*line)->prev;
	    current_chunk = strlenpt((*line)->data) / editwincols;
	}

	/* Only change leftedge when we actually could move. */
	if (i < nrows)
	    *leftedge = current_chunk * editwincols;
    } else
#endif
	for (i = nrows; i > 0 && (*line)->prev != NULL; i--)
	    *line = (*line)->prev;

    return i;
}

/* Try to move down nrows softwrapped chunks from the given line and the
 * given column (leftedge).  After moving, leftedge will be set to the
 * starting column of the current chunk.  Return the number of chunks we
 * couldn't move down, which will be zero if we completely succeeded. */
int go_forward_chunks(int nrows, filestruct **line, size_t *leftedge)
{
    int i;

    /* Don't move more chunks than the window can hold. */
    if (nrows > editwinrows - 1)
	nrows = (editwinrows < 2) ? 1 : editwinrows - 1;

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	size_t current_chunk = (*leftedge) / editwincols;
	size_t last_chunk = strlenpt((*line)->data) / editwincols;

	/* Advance through the requested number of chunks. */
	for (i = nrows; i > 0; i--) {
	    if (current_chunk < last_chunk) {
		current_chunk++;
		continue;
	    }

	    if (*line == openfile->filebot)
		break;

	    *line = (*line)->next;
	    current_chunk = 0;
	    last_chunk = strlenpt((*line)->data) / editwincols;
	}

	/* Only change leftedge when we actually could move. */
	if (i < nrows)
	    *leftedge = current_chunk * editwincols;
    } else
#endif
	for (i = nrows; i > 0 && (*line)->next != NULL; i--)
	    *line = (*line)->next;

    return i;
}

/* Return TRUE if there are fewer than a screen's worth of lines between
 * the line at line number was_lineno (and column was_leftedge, if we're
 * in softwrap mode) and the line at current[current_x]. */
bool less_than_a_screenful(size_t was_lineno, size_t was_leftedge)
{
#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	filestruct *line = openfile->current;
	size_t leftedge = (xplustabs() / editwincols) * editwincols;
	int rows_left = go_back_chunks(editwinrows - 1, &line, &leftedge);

	return (rows_left > 0 || line->lineno < was_lineno ||
		(line->lineno == was_lineno && leftedge <= was_leftedge));
    } else
#endif
	return (openfile->current->lineno - was_lineno < editwinrows);
}

/* Scroll the edit window in the given direction and the given number of rows,
 * and draw new lines on the blank lines left after the scrolling. */
void edit_scroll(scroll_dir direction, int nrows)
{
    filestruct *line;
    size_t leftedge;

    /* Part 1: nrows is the number of rows we're going to scroll the text of
     * the edit window. */

    /* Move the top line of the edit window the requested number of rows up or
     * down, and reduce the number of rows with the amount we couldn't move. */
    if (direction == UPWARD)
	nrows -= go_back_chunks(nrows, &openfile->edittop, &openfile->firstcolumn);
    else
	nrows -= go_forward_chunks(nrows, &openfile->edittop, &openfile->firstcolumn);

    /* Don't bother scrolling zero rows, nor more than the window can hold. */
    if (nrows == 0)
	return;
    if (nrows >= editwinrows) {
	refresh_needed = TRUE;
	return;
    }

    /* Scroll the text of the edit window a number of rows up or down. */
    scrollok(edit, TRUE);
    wscrl(edit, (direction == UPWARD) ? -nrows : nrows);
    scrollok(edit, FALSE);

    /* Part 2: nrows is now the number of rows in the scrolled region of the
     * edit window that we need to draw. */

    /* If we're not on the first "page" (when not softwrapping), or the mark
     * is on, the row next to the scrolled region needs to be redrawn too. */
    if (line_needs_update(openfile->placewewant, 0) && nrows < editwinrows)
	nrows++;

    /* If we scrolled backward, start on the first line of the blank region. */
    line = openfile->edittop;
    leftedge = openfile->firstcolumn;

    /* If we scrolled forward, move down to the start of the blank region. */
    if (direction == DOWNWARD)
	go_forward_chunks(editwinrows - nrows, &line, &leftedge);

#ifndef NANO_TINY
    /* Compensate for the earlier onscreen chunks of a softwrapped line
     * when the first blank row happens to be in the middle of that line. */
    if (ISSET(SOFTWRAP) && line != openfile->edittop)
	nrows += leftedge / editwincols;
#endif

    /* Draw new content on the blank rows inside the scrolled region
     * (and on the bordering row too when it was deemed necessary). */
    while (nrows > 0 && line != NULL) {
	nrows -= update_line(line, (line == openfile->current) ?
					openfile->current_x : 0);
	line = line->next;
    }
}

/* Ensure that firstcolumn is at the starting column of the softwrapped chunk
 * it's on.  We need to do this when the number of columns of the edit window
 * has changed, because then the width of softwrapped chunks has changed. */
void ensure_firstcolumn_is_aligned(void)
{
#ifndef NANO_TINY
    if (openfile->firstcolumn % editwincols != 0)
	openfile->firstcolumn -= (openfile->firstcolumn % editwincols);
#endif
}

/* Return TRUE if current[current_x] is above the top of the screen, and FALSE
 * otherwise. */
bool current_is_above_screen(void)
{
#ifndef NANO_TINY
    if (ISSET(SOFTWRAP))
	/* The cursor is above screen when current[current_x] is before edittop
	 * at column firstcolumn. */
	return (openfile->current->lineno < openfile->edittop->lineno ||
		(openfile->current->lineno == openfile->edittop->lineno &&
		xplustabs() < openfile->firstcolumn));
    else
#endif
	return (openfile->current->lineno < openfile->edittop->lineno);
}

/* Return TRUE if current[current_x] is below the bottom of the screen, and
 * FALSE otherwise. */
bool current_is_below_screen(void)
{
#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	filestruct *line = openfile->edittop;
	size_t leftedge = openfile->firstcolumn;

	/* If current[current_x] is more than a screen's worth of lines after
	 * edittop at column firstcolumn, it's below the screen. */
	return (go_forward_chunks(editwinrows - 1, &line, &leftedge) == 0 &&
			(line->lineno < openfile->current->lineno ||
			(line->lineno == openfile->current->lineno &&
			leftedge < (xplustabs() / editwincols) * editwincols)));
    } else
#endif
	return (openfile->current->lineno >=
			openfile->edittop->lineno + editwinrows);
}

/* Return TRUE if current[current_x] is offscreen relative to edittop, and
 * FALSE otherwise. */
bool current_is_offscreen(void)
{
    return (current_is_above_screen() || current_is_below_screen());
}

/* Update any lines between old_current and current that need to be
 * updated.  Use this if we've moved without changing any text. */
void edit_redraw(filestruct *old_current)
{
    size_t was_pww = openfile->placewewant;

    openfile->placewewant = xplustabs();

    /* If the current line is offscreen, scroll until it's onscreen. */
    if (current_is_offscreen()) {
	adjust_viewport((focusing || !ISSET(SMOOTH_SCROLL)) ? CENTERING : FLOWING);
	refresh_needed = TRUE;
	return;
    }

#ifndef NANO_TINY
    /* If the mark is on, update all lines between old_current and current. */
    if (openfile->mark_set) {
	filestruct *line = old_current;

	while (line != openfile->current) {
	    update_line(line, 0);

	    line = (line->lineno > openfile->current->lineno) ?
			line->prev : line->next;
	}
    } else
#endif
	/* Otherwise, update old_current only if it differs from current
	 * and was horizontally scrolled. */
	if (old_current != openfile->current && get_page_start(was_pww) > 0)
	    update_line(old_current, 0);

    /* Update current if the mark is on or it has changed "page", or if it
     * differs from old_current and needs to be horizontally scrolled. */
    if (line_needs_update(was_pww, openfile->placewewant) ||
			(old_current != openfile->current &&
			get_page_start(openfile->placewewant) > 0))
	update_line(openfile->current, openfile->current_x);
}

/* Refresh the screen without changing the position of lines.  Use this
 * if we've moved and changed text. */
void edit_refresh(void)
{
    filestruct *line;
    int row = 0;

    /* If the current line is out of view, get it back on screen. */
    if (current_is_offscreen()) {
#ifdef DEBUG
	fprintf(stderr, "edit-refresh: line = %ld, edittop = %ld and editwinrows = %d\n",
		(long)openfile->current->lineno, (long)openfile->edittop->lineno, editwinrows);
#endif
	adjust_viewport((focusing || !ISSET(SMOOTH_SCROLL)) ? CENTERING : STATIONARY);
    }

#ifdef DEBUG
    fprintf(stderr, "edit-refresh: now edittop = %ld\n", (long)openfile->edittop->lineno);
#endif

    line = openfile->edittop;

    while (row < editwinrows && line != NULL) {
	if (line == openfile->current)
	    row += update_line(line, openfile->current_x);
	else
	    row += update_line(line, 0);
	line = line->next;
    }

    while (row < editwinrows)
	blank_row(edit, row++, 0, COLS);

    reset_cursor();
    wnoutrefresh(edit);

    refresh_needed = FALSE;
}

/* Move edittop so that current is on the screen.  manner says how it
 * should be moved: CENTERING means that current should end up in the
 * middle of the screen, STATIONARY means that it should stay at the
 * same vertical position, and FLOWING means that it should scroll no
 * more than needed to bring current into view. */
void adjust_viewport(update_type manner)
{
    int goal = 0;

    /* If manner is CENTERING, move edittop half the number of window rows
     * back from current.  If manner is FLOWING, move edittop back 0 rows
     * or (editwinrows - 1) rows, depending on where current has moved.
     * This puts the cursor on the first or the last row.  If manner is
     * STATIONARY, move edittop back current_y rows if current_y is in range
     * of the screen, 0 rows if current_y is below zero, or (editwinrows - 1)
     * rows if current_y is too big.  This puts current at the same place on
     * the screen as before, or... at some undefined place. */
    if (manner == CENTERING)
	goal = editwinrows / 2;
    else if (manner == FLOWING) {
	if (!current_is_above_screen())
	    goal = editwinrows - 1;
    } else {
	goal = openfile->current_y;

	/* Limit goal to (editwinrows - 1) rows maximum. */
	if (goal > editwinrows - 1)
	    goal = editwinrows - 1;
    }

    openfile->edittop = openfile->current;

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP))
	openfile->firstcolumn = (xplustabs() / editwincols) * editwincols;
#endif

    /* Move edittop back goal rows, starting at current[current_x]. */
    go_back_chunks(goal, &openfile->edittop, &openfile->firstcolumn);

#ifdef DEBUG
    fprintf(stderr, "adjust_viewport(): setting edittop to lineno %ld\n", (long)openfile->edittop->lineno);
#endif
}

/* Unconditionally redraw the entire screen. */
void total_redraw(void)
{
#ifdef USE_SLANG
    /* Slang curses emulation brain damage, part 4: Slang doesn't define
     * curscr. */
    SLsmg_touch_screen();
    SLsmg_refresh();
#else
    wrefresh(curscr);
#endif
}

/* Unconditionally redraw the entire screen, and then refresh it using
 * the current file. */
void total_refresh(void)
{
    total_redraw();
    titlebar(NULL);
    edit_refresh();
    bottombars(currmenu);
}

/* Display the main shortcut list on the last two rows of the bottom
 * portion of the window. */
void display_main_list(void)
{
#ifndef DISABLE_COLOR
    if (openfile->syntax &&
		(openfile->syntax->formatter || openfile->syntax->linter))
	set_lint_or_format_shortcuts();
    else
	set_spell_shortcuts();
#endif

    bottombars(MMAIN);
}

/* Show info about the current cursor position on the statusbar.
 * Do this unconditionally when force is TRUE; otherwise, only if
 * suppress_cursorpos is FALSE.  In any case, reset the latter. */
void do_cursorpos(bool force)
{
    char saved_byte;
    size_t sum, cur_xpt = xplustabs() + 1;
    size_t cur_lenpt = strlenpt(openfile->current->data) + 1;
    int linepct, colpct, charpct;

    assert(openfile->fileage != NULL && openfile->current != NULL);

    /* Determine the size of the file up to the cursor. */
    saved_byte = openfile->current->data[openfile->current_x];
    openfile->current->data[openfile->current_x] = '\0';

    sum = get_totsize(openfile->fileage, openfile->current);

    openfile->current->data[openfile->current_x] = saved_byte;

    /* When not at EOF, subtract 1 for an overcounted newline. */
    if (openfile->current != openfile->filebot)
	sum--;

    /* If the showing needs to be suppressed, don't suppress it next time. */
    if (suppress_cursorpos && !force) {
	suppress_cursorpos = FALSE;
	return;
    }

    /* Display the current cursor position on the statusbar. */
    linepct = 100 * openfile->current->lineno / openfile->filebot->lineno;
    colpct = 100 * cur_xpt / cur_lenpt;
    charpct = (openfile->totsize == 0) ? 0 : 100 * sum / openfile->totsize;

    statusline(HUSH,
	_("line %ld/%ld (%d%%), col %lu/%lu (%d%%), char %lu/%lu (%d%%)"),
	(long)openfile->current->lineno,
	(long)openfile->filebot->lineno, linepct,
	(unsigned long)cur_xpt, (unsigned long)cur_lenpt, colpct,
	(unsigned long)sum, (unsigned long)openfile->totsize, charpct);

    /* Displaying the cursor position should not suppress it next time. */
    suppress_cursorpos = FALSE;
}

/* Unconditionally display the current cursor position. */
void do_cursorpos_void(void)
{
    do_cursorpos(TRUE);
}

void enable_nodelay(void)
{
    nodelay_mode = TRUE;
    nodelay(edit, TRUE);
}

void disable_nodelay(void)
{
    nodelay_mode = FALSE;
    nodelay(edit, FALSE);
}

/* Highlight the current word being replaced or spell checked.  We
 * expect word to have tabs and control characters expanded. */
void spotlight(bool active, const char *word)
{
    size_t word_span = strlenpt(word);
    size_t room = word_span;

    /* Compute the number of columns that are available for the word. */
    if (!ISSET(SOFTWRAP)) {
	room = editwincols + get_page_start(xplustabs()) - xplustabs();

	/* If the word is partially offscreen, reserve space for the "$". */
	if (word_span > room)
	    room--;
    }

    reset_cursor();

    if (active)
	wattron(edit, hilite_attribute);

    /* This is so we can show zero-length matches. */
    if (word_span == 0)
	waddch(edit, ' ');
    else
	waddnstr(edit, word, actual_x(word, room));

    if (word_span > room)
	waddch(edit, '$');

    if (active)
	wattroff(edit, hilite_attribute);

    wnoutrefresh(edit);
}

#ifndef DISABLE_EXTRA
#define CREDIT_LEN 54
#define XLCREDIT_LEN 9

/* Easter egg: Display credits.  Assume nodelay(edit) and scrollok(edit)
 * are FALSE. */
void do_credits(void)
{
    bool old_more_space = ISSET(MORE_SPACE);
    bool old_no_help = ISSET(NO_HELP);
    int kbinput = ERR, crpos = 0, xlpos = 0;
    const char *credits[CREDIT_LEN] = {
	NULL,				/* "The nano text editor" */
	NULL,				/* "version" */
	VERSION,
	"",
	NULL,				/* "Brought to you by:" */
	"Chris Allegretta",
	"Jordi Mallach",
	"Adam Rogoyski",
	"Rob Siemborski",
	"Rocco Corsi",
	"David Lawrence Ramsey",
	"David Benbennick",
	"Mark Majeres",
	"Mike Frysinger",
	"Benno Schulenberg",
	"Ken Tyler",
	"Sven Guckes",
	"Bill Soudan",
	"Christian Weisgerber",
	"Erik Andersen",
	"Big Gaute",
	"Joshua Jensen",
	"Ryan Krebs",
	"Albert Chin",
	"",
	NULL,				/* "Special thanks to:" */
	"Monique, Brielle & Joseph",
	"Plattsburgh State University",
	"Benet Laboratories",
	"Amy Allegretta",
	"Linda Young",
	"Jeremy Robichaud",
	"Richard Kolb II",
	NULL,				/* "The Free Software Foundation" */
	"Linus Torvalds",
	NULL,				/* "the many translators and the TP" */
	NULL,				/* "For ncurses:" */
	"Thomas Dickey",
	"Pavel Curtis",
	"Zeyd Ben-Halim",
	"Eric S. Raymond",
	NULL,				/* "and anyone else we forgot..." */
	NULL,				/* "Thank you for using nano!" */
	"",
	"",
	"",
	"",
	"(C) 1999 - 2016",
	"Free Software Foundation, Inc.",
	"",
	"",
	"",
	"",
	"https://nano-editor.org/"
    };

    const char *xlcredits[XLCREDIT_LEN] = {
	N_("The nano text editor"),
	N_("version"),
	N_("Brought to you by:"),
	N_("Special thanks to:"),
	N_("The Free Software Foundation"),
	N_("the many translators and the TP"),
	N_("For ncurses:"),
	N_("and anyone else we forgot..."),
	N_("Thank you for using nano!")
    };

    if (!old_more_space || !old_no_help) {
	SET(MORE_SPACE);
	SET(NO_HELP);
	window_init();
    }

    curs_set(0);
    nodelay(edit, TRUE);

    blank_titlebar();
    blank_edit();
    blank_statusbar();

    wrefresh(topwin);
    wrefresh(edit);
    wrefresh(bottomwin);
    napms(700);

    for (crpos = 0; crpos < CREDIT_LEN + editwinrows / 2; crpos++) {
	if ((kbinput = wgetch(edit)) != ERR)
	    break;

	if (crpos < CREDIT_LEN) {
	    const char *what;
	    size_t start_col;

	    if (credits[crpos] == NULL) {
		assert(0 <= xlpos && xlpos < XLCREDIT_LEN);

		what = _(xlcredits[xlpos]);
		xlpos++;
	    } else
		what = credits[crpos];

	    start_col = COLS / 2 - strlenpt(what) / 2 - 1;
	    mvwaddstr(edit, editwinrows - 1 - (editwinrows % 2),
						start_col, what);
	}

	wrefresh(edit);

	if ((kbinput = wgetch(edit)) != ERR)
	    break;
	napms(700);

	scrollok(edit, TRUE);
	wscrl(edit, 1);
	scrollok(edit, FALSE);
	wrefresh(edit);

	if ((kbinput = wgetch(edit)) != ERR)
	    break;
	napms(700);

	scrollok(edit, TRUE);
	wscrl(edit, 1);
	scrollok(edit, FALSE);
	wrefresh(edit);
    }

    if (kbinput != ERR)
	ungetch(kbinput);

    if (!old_more_space)
	UNSET(MORE_SPACE);
    if (!old_no_help)
	UNSET(NO_HELP);
    window_init();

    nodelay(edit, FALSE);

    total_refresh();
}
#endif /* !DISABLE_EXTRA */
