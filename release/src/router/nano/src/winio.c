/**************************************************************************
 *   winio.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014, 2015, 2016 Benno Schulenberg                     *
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

#ifndef NANO_TINY
static sig_atomic_t last_sigwinch_counter = 0;

/* Did we receive a SIGWINCH since we were last called? */
bool the_window_resized(void)
{
    if (sigwinch_counter == last_sigwinch_counter)
	return FALSE;

    last_sigwinch_counter = sigwinch_counter;
    regenerate_screen();
    return TRUE;
}
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
    if (the_window_resized()) {
	ungetch(input);
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
	if (the_window_resized()) {
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
		/* An "ESC ESC [ X" sequence from Option+arrow. */
		switch (keycode) {
		    case 'A':
			retval = KEY_HOME;
			break;
		    case 'B':
			retval = KEY_END;
			break;
		    case 'C':
			retval = controlright;
			break;
		    case 'D':
			retval = controlleft;
			break;
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
			'A' <= *key_buffer && *key_buffer <= 'D') {
		/* This is an iTerm2 sequence: ^[ ^[ [ X. */
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
	return sc_seq_or(do_prev_word_void, shiftcontrolleft);
    } else if (retval == shiftcontrolright) {
	shift_held = TRUE;
	return sc_seq_or(do_next_word_void, shiftcontrolright);
    } else if (retval == shiftcontrolup) {
	shift_held = TRUE;
	return sc_seq_or(do_prev_block, shiftcontrolup);
    } else if (retval == shiftcontroldown) {
	shift_held = TRUE;
	return sc_seq_or(do_next_block, shiftcontroldown);
    } else if (retval == shiftaltleft) {
	shift_held = TRUE;
	return sc_seq_or(do_home, shiftaltleft);
    } else if (retval == shiftaltright) {
	shift_held = TRUE;
	return sc_seq_or(do_end, shiftaltright);
    } else if (retval == shiftaltup) {
	shift_held = TRUE;
	return sc_seq_or(do_page_up, shiftaltup);
    } else if (retval == shiftaltdown) {
	shift_held = TRUE;
	return sc_seq_or(do_page_down, shiftaltdown);
    }
#endif

#ifdef __linux__
    /* When not running under X, check for the bare arrow keys whether
     * Shift/Ctrl/Alt are being held together with them. */
    unsigned char modifiers = 6;

    if (console && ioctl(0, TIOCLINUX, &modifiers) >= 0) {
	/* Is Ctrl being held? */
	if (modifiers & 0x04) {
	    if (retval == KEY_UP)
		return sc_seq_or(do_prev_block, controlup);
	    else if (retval == KEY_DOWN)
		return sc_seq_or(do_next_block, controldown);
	    else if (retval == KEY_LEFT)
		return sc_seq_or(do_prev_word_void, controlleft);
	    else if (retval == KEY_RIGHT)
		return sc_seq_or(do_next_word_void, controlright);
	}

#ifndef NANO_TINY
	/* Is Shift being held? */
	if (modifiers & 0x01)
	    shift_held =TRUE;

	/* Are both Shift and Alt being held? */
	if ((modifiers & 0x09) == 0x09) {
	    if (retval == KEY_UP)
		return sc_seq_or(do_page_up, shiftaltup);
	    else if (retval == KEY_DOWN)
		return sc_seq_or(do_page_down, shiftaltdown);
	    else if (retval == KEY_LEFT)
		return sc_seq_or(do_home, shiftaltleft);
	    else if (retval == KEY_RIGHT)
		return sc_seq_or(do_end, shiftaltright);
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
	case SHIFT_PAGEUP:		/* Fake key, from Shift+Alt+Up. */
	    shift_held = TRUE;
#endif
	case KEY_A3:	/* PageUp (9) on keypad with NumLock off. */
	    return KEY_PPAGE;
#ifndef NANO_TINY
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
		return sc_seq_or(do_delete, KEY_DC);
	    else
		return KEY_BACKSPACE;
#ifdef KEY_SIC
	/* Slang doesn't support KEY_SIC. */
	case KEY_SIC:
	    return sc_seq_or(do_insertfile_void, KEY_IC);
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
	    return first_sc_for(currmenu, do_cancel)->keycode;
#endif
#ifdef KEY_SUSPEND
#ifdef KEY_SSUSPEND
	/* Slang doesn't support KEY_SSUSPEND. */
	case KEY_SSUSPEND:
#endif
	/* Slang doesn't support KEY_SUSPEND. */
	case KEY_SUSPEND:
	    return sc_seq_or(do_suspend_void, KEY_SUSPEND);
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
			return KEY_DC;
		    case '4': /* Esc [ 4 ~ == End on VT220/VT320/Linux
			       * console/xterm. */
			return KEY_END;
		    case '5': /* Esc [ 5 ~ == PageUp on VT220/VT320/
			       * Linux console/xterm/Terminal;
			       * Esc [ 5 ^ == PageUp on Eterm. */
			return KEY_PPAGE;
		    case '6': /* Esc [ 6 ~ == PageDown on VT220/VT320/
			       * Linux console/xterm/Terminal;
			       * Esc [ 6 ^ == PageDown on Eterm. */
			return KEY_NPAGE;
		    case '7': /* Esc [ 7 ~ == Home on rxvt. */
			return KEY_HOME;
		    case '8': /* Esc [ 8 ~ == End on rxvt. */
			return KEY_END;
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

    /* Read in a stream of characters and interpret it if possible. */
    retval = parse_verbatim_kbinput(win, kbinput_len);

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

/* Read in one control character (or an iTerm double Escape), or convert a
 * series of six digits into a Unicode codepoint.  Return in count either 1
 * (for a control character or the first byte of a multibyte sequence), or 2
 * (for an iTerm double Escape). */
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

#ifdef ENABLE_UTF8
    if (using_utf8()) {
	/* Check whether the first code is a valid starter digit: 0 or 1. */
	long uni = get_unicode_kbinput(win, *kbinput);

	/* If the first code isn't the digit 0 nor 1, put it back. */
	if (uni != ERR)
	    unget_input(kbinput, 1);
	/* Otherwise, continue reading in digits until we have a complete
	 * Unicode value, and put back the corresponding byte(s). */
	else {
	    char *uni_mb;
	    int uni_mb_len, *seq, i;

	    while (uni == ERR) {
		free(kbinput);
		while ((kbinput = get_input(win, 1)) == NULL)
		    ;
		uni = get_unicode_kbinput(win, *kbinput);
	    }

	    /* Convert the Unicode value to a multibyte sequence. */
	    uni_mb = make_mbchar(uni, &uni_mb_len);

	    seq = (int *)nmalloc(uni_mb_len * sizeof(int));

	    for (i = 0; i < uni_mb_len; i++)
		seq[i] = (unsigned char)uni_mb[i];

	    /* Insert the multibyte sequence into the input buffer. */
	    unget_input(seq, uni_mb_len);

	    free(seq);
	    free(uni_mb);
	}
    } else
#endif /* ENABLE_UTF8 */
	/* Put back the first code. */
	unget_input(kbinput, 1);

    free(kbinput);

    *count = 1;

    /* If this is an iTerm double escape, take both Escapes. */
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
void blank_line(WINDOW *win, int y, int x, int n)
{
    wmove(win, y, x);

    for (; n > 0; n--)
	waddch(win, ' ');
}

/* Blank the first line of the top portion of the window. */
void blank_titlebar(void)
{
    blank_line(topwin, 0, 0, COLS);
}

/* Blank all the lines of the middle portion of the window, i.e. the
 * edit window. */
void blank_edit(void)
{
    int i;

    for (i = 0; i < editwinrows; i++)
	blank_line(edit, i, 0, COLS);
}

/* Blank the first line of the bottom portion of the window. */
void blank_statusbar(void)
{
    blank_line(bottomwin, 0, 0, COLS);
}

/* If the NO_HELP flag isn't set, blank the last two lines of the bottom
 * portion of the window. */
void blank_bottombars(void)
{
    if (!ISSET(NO_HELP) && LINES > 4) {
	blank_line(bottomwin, 1, 0, COLS);
	blank_line(bottomwin, 2, 0, COLS);
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
 * string is dynamically allocated, and should be freed.  If dollars is
 * TRUE, the caller might put "$" at the beginning or end of the line if
 * it's too long. */
char *display_string(const char *buf, size_t start_col, size_t span,
	bool dollars)
{
    size_t start_index;
	/* Index in buf of the first character shown. */
    size_t column;
	/* Screen column that start_index corresponds to. */
    char *converted;
	/* The string we return. */
    size_t index;
	/* Current position in converted. */

    /* If dollars is TRUE, make room for the "$" at the end of the
     * line. */
    if (dollars && span > 0 && strlenpt(buf) > start_col + span)
	span--;

    if (span == 0)
	return mallocstrcpy(NULL, "");

    start_index = actual_x(buf, start_col);
    column = strnlenpt(buf, start_index);

    assert(column <= start_col);

    /* Allocate enough space to hold the entire converted buffer. */
    converted = charalloc(strlen(buf) * (mb_cur_max() + tabsize) + 1);

    index = 0;
#ifdef USING_OLD_NCURSES
    seen_wide = FALSE;
#endif
    buf += start_index;

    if (*buf != '\0' && *buf != '\t' &&
	(column < start_col || (dollars && column > 0))) {
	/* We don't display the complete first character as it starts to
	 * the left of the screen. */
	if (is_cntrl_mbchar(buf)) {
	    if (column < start_col) {
		converted[index++] = control_mbrep(buf);
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

    while (*buf != '\0') {
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
	    converted[index++] = control_mbrep(buf);
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

    /* Null-terminate converted. */
    converted[index] = '\0';

    /* Make sure converted takes up no more than span columns. */
    index = actual_x(converted, span);
    null_at(&converted, index);

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
    char *fragment;
	/* The tail part of the pathname when dottified. */

    /* If the screen is too small, there is no titlebar. */
    if (topwin == NULL)
	return;

    assert(path != NULL || openfile->filename != NULL);

    wattron(topwin, interface_color_pair[TITLE_BAR]);

    blank_titlebar();

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
    if (pathlen + pluglen + statelen <= COLS)
	waddstr(topwin, path);
    else if (5 + statelen <= COLS) {
	waddstr(topwin, "...");
	fragment = display_string(path, 3 + pathlen - COLS + statelen,
					COLS - statelen, FALSE);
	waddstr(topwin, fragment);
	free(fragment);
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

/* Display a message on the statusbar, and set suppress_cursorpos to
 * TRUE, so that the message won't be immediately overwritten if
 * constant cursor position display is on. */
void statusline(message_type importance, const char *msg, ...)
{
    va_list ap;
    char *compound, *message;
    size_t start_x;
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

    /* Delay another alert message, to allow an earlier one to be noticed. */
    if (lastmessage == ALERT)
	napms(1200);

    if (importance == ALERT)
	beep();

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

    start_x = (COLS - strlenpt(message)) / 2;
    bracketed = (start_x > 1);

    wmove(bottomwin, 0, (bracketed ? start_x - 2 : start_x));
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
 * and put the cursor in the edit window at (current_y, current_x). */
void reset_cursor(void)
{
    size_t xpt = xplustabs();

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	filestruct *line = openfile->edittop;
	openfile->current_y = 0;

	while (line != NULL && line != openfile->current) {
	    openfile->current_y += strlenpt(line->data) / editwincols + 1;
	    line = line->next;
	}
	openfile->current_y += xpt / editwincols;

	if (openfile->current_y < editwinrows)
	    wmove(edit, openfile->current_y, xpt % editwincols + margin);
    } else
#endif
    {
	openfile->current_y = openfile->current->lineno -
				openfile->edittop->lineno;

	if (openfile->current_y < editwinrows)
	    wmove(edit, openfile->current_y, xpt - get_page_start(xpt) + margin);
    }
}

/* edit_draw() takes care of the job of actually painting a line into
 * the edit window.  fileptr is the line to be painted, at row line of
 * the window.  converted is the actual string to be written to the
 * window, with tabs and control characters replaced by strings of
 * regular characters.  start is the column number of the first
 * character of this page.  That is, the first character of converted
 * corresponds to character number actual_x(fileptr->data, start) of the
 * line. */
void edit_draw(filestruct *fileptr, const char *converted, int
	line, size_t start)
{
#if !defined(NANO_TINY) || !defined(DISABLE_COLOR)
    size_t startpos = actual_x(fileptr->data, start);
	/* The position in fileptr->data of the leftmost character
	 * that displays at least partially on the window. */
    size_t endpos = actual_x(fileptr->data, start + editwincols - 1) + 1;
	/* The position in fileptr->data of the first character that is
	 * completely off the window to the right.
	 *
	 * Note that endpos might be beyond the null terminator of the
	 * string. */
#endif

    assert(openfile != NULL && fileptr != NULL && converted != NULL);
    assert(strlenpt(converted) <= editwincols);

#ifdef ENABLE_LINENUMBERS
    /* If line numbering is switched on, show a line number in front of
     * the text -- but only for the parts that are not softwrapped. */
    if (margin > 0) {
	wattron(edit, interface_color_pair[LINE_NUMBER]);
	if (last_drawn_line != fileptr->lineno || last_line_y >= line)
	    mvwprintw(edit, line, 0, "%*i", margin - 1, fileptr->lineno);
	else
	    mvwprintw(edit, line, 0, "%*s", margin - 1, " ");
	wattroff(edit, interface_color_pair[LINE_NUMBER]);
    }
#endif

    /* First simply paint the line -- then we'll add colors or the
     * marking highlight on just the pieces that need it. */
    mvwaddstr(edit, line, margin, converted);

#ifdef USING_OLD_NCURSES
    /* Tell ncurses to really redraw the line without trying to optimize
     * for what it thinks is already there, because it gets it wrong in
     * the case of a wide character in column zero.  See bug #31743. */
    if (seen_wide)
	wredrawln(edit, line, 1);
#endif

#ifndef DISABLE_COLOR
    /* If color syntaxes are available and turned on, we need to display
     * them. */
    if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX)) {
	const colortype *varnish = openfile->colorstrings;

	/* If there are multiline regexes, make sure there is a cache. */
	if (openfile->syntax->nmultis > 0)
	    alloc_multidata_if_needed(fileptr);

	for (; varnish != NULL; varnish = varnish->next) {
	    int x_start;
		/* Starting column for mvwaddnstr.  Zero-based. */
	    int paintlen = 0;
		/* Number of chars to paint on this line.  There are
		 * COLS characters on a whole line. */
	    size_t index;
		/* Index in converted where we paint. */
	    regmatch_t startmatch;
		/* Match position for start_regex. */
	    regmatch_t endmatch;
		/* Match position for end_regex. */

	    wattron(edit, varnish->attributes);
	    /* Two notes about regexec().  A return value of zero means
	     * that there is a match.  Also, rm_eo is the first
	     * non-matching character after the match. */

	    /* First case: varnish is a single-line expression. */
	    if (varnish->end == NULL) {
		size_t k = 0;

		/* We increment k by rm_eo, to move past the end of the
		 * last match.  Even though two matches may overlap, we
		 * want to ignore them, so that we can highlight e.g. C
		 * strings correctly. */
		while (k < endpos) {
		    /* Note the fifth parameter to regexec().  It says
		     * not to match the beginning-of-line character
		     * unless k is zero.  If regexec() returns
		     * REG_NOMATCH, there are no more matches in the
		     * line. */
		    if (regexec(varnish->start, &fileptr->data[k], 1,
			&startmatch, (k == 0) ? 0 : REG_NOTBOL) ==
			REG_NOMATCH)
			break;
		    /* Translate the match to the beginning of the
		     * line. */
		    startmatch.rm_so += k;
		    startmatch.rm_eo += k;

		    /* Skip over a zero-length regex match. */
		    if (startmatch.rm_so == startmatch.rm_eo)
			startmatch.rm_eo++;
		    else if (startmatch.rm_so < endpos &&
			startmatch.rm_eo > startpos) {
			x_start = (startmatch.rm_so <= startpos) ? 0 :
				strnlenpt(fileptr->data,
				startmatch.rm_so) - start;

			index = actual_x(converted, x_start);

			paintlen = actual_x(converted + index,
				strnlenpt(fileptr->data,
				startmatch.rm_eo) - start - x_start);

			assert(0 <= x_start && 0 <= paintlen);

			mvwaddnstr(edit, line, x_start + margin, converted +
				index, paintlen);
		    }
		    k = startmatch.rm_eo;
		}
	    } else {	/* Second case: varnish is a multiline expression. */
		const filestruct *start_line = fileptr->prev;
		    /* The first line before fileptr that matches 'start'. */
		size_t start_col;
		    /* Where the match starts in that line. */
		const filestruct *end_line;
		    /* The line that matches 'end'. */

		/* First see if the multidata was maybe already calculated. */
		if (fileptr->multidata[varnish->id] == CNONE)
		    goto tail_of_loop;
		else if (fileptr->multidata[varnish->id] == CWHOLELINE) {
		    mvwaddnstr(edit, line, margin, converted, -1);
		    goto tail_of_loop;
		} else if (fileptr->multidata[varnish->id] == CBEGINBEFORE) {
		    regexec(varnish->end, fileptr->data, 1, &endmatch, 0);
		    /* If the coloured part is scrolled off, skip it. */
		    if (endmatch.rm_eo <= startpos)
			goto tail_of_loop;
		    paintlen = actual_x(converted, strnlenpt(fileptr->data,
			endmatch.rm_eo) - start);
		    mvwaddnstr(edit, line, margin, converted, paintlen);
		    goto tail_of_loop;
		} if (fileptr->multidata[varnish->id] == -1)
		    /* Assume this until proven otherwise below. */
		    fileptr->multidata[varnish->id] = CNONE;

		/* There is no precalculated multidata, so find it out now.
		 * First check if the beginning of the line is colored by a
		 * start on an earlier line, and an end on this line or later.
		 *
		 * So: find the first line before fileptr matching the start.
		 * If every match on that line is followed by an end, then go
		 * to step two.  Otherwise, find a line after start_line that
		 * matches the end.  If that line is not before fileptr, then
		 * paint the beginning of this line. */

		while (start_line != NULL && regexec(varnish->start,
			start_line->data, 1, &startmatch, 0) == REG_NOMATCH) {
		    /* There is no start; but if there is an end on this line,
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

		/* Skip over a zero-length regex match. */
		if (startmatch.rm_so == startmatch.rm_eo)
		    goto tail_of_loop;

		/* Now start_line is the first line before fileptr containing
		 * a start match.  Is there a start on that line not followed
		 * by an end on that line? */
		start_col = 0;
		while (TRUE) {
		    start_col += startmatch.rm_so;
		    startmatch.rm_eo -= startmatch.rm_so;
		    if (regexec(varnish->end, start_line->data +
				start_col + startmatch.rm_eo, 0, NULL,
				(start_col + startmatch.rm_eo == 0) ?
				0 : REG_NOTBOL) == REG_NOMATCH)
			/* No end found after this start. */
			break;
		    start_col++;
		    if (regexec(varnish->start, start_line->data + start_col,
				1, &startmatch, REG_NOTBOL) == REG_NOMATCH)
			/* No later start on this line. */
			goto step_two;
		}
		/* Indeed, there is a start without an end on that line. */

		/* We've already checked that there is no end before fileptr
		 * and after the start.  But is there an end after the start
		 * at all?  We don't paint unterminated starts. */
		end_line = fileptr;
		while (end_line != NULL && regexec(varnish->end,
			end_line->data, 1, &endmatch, 0) == REG_NOMATCH)
		    end_line = end_line->next;

		/* If no end was found, or it is too early, next step. */
		if (end_line == NULL)
		    goto step_two;
		if (end_line == fileptr && endmatch.rm_eo <= startpos) {
		    fileptr->multidata[varnish->id] = CBEGINBEFORE;
		    goto step_two;
		}

		/* Now paint the start of fileptr.  If the start of fileptr
		 * is on a different line from the end, paintlen is -1, which
		 * means that everything on the line gets painted.  Otherwise,
		 * paintlen is the expanded location of the end of the match
		 * minus the expanded location of the beginning of the page. */
		if (end_line != fileptr) {
		    paintlen = -1;
		    fileptr->multidata[varnish->id] = CWHOLELINE;
#ifdef DEBUG
    fprintf(stderr, "  Marking for id %i  line %i as CWHOLELINE\n", varnish->id, line);
#endif
		} else {
		    paintlen = actual_x(converted, strnlenpt(fileptr->data,
						endmatch.rm_eo) - start);
		    fileptr->multidata[varnish->id] = CBEGINBEFORE;
#ifdef DEBUG
    fprintf(stderr, "  Marking for id %i  line %i as CBEGINBEFORE\n", varnish->id, line);
#endif
		}
		mvwaddnstr(edit, line, margin, converted, paintlen);
		/* If the whole line has been painted, don't bother looking
		 * for any more starts. */
		if (paintlen < 0)
		    goto tail_of_loop;
  step_two:
		/* Second step: look for starts on this line, but start
		 * looking only after an end match, if there is one. */
		start_col = (paintlen == 0) ? 0 : endmatch.rm_eo;

		while (start_col < endpos) {
		    if (regexec(varnish->start, fileptr->data + start_col,
				1, &startmatch, (start_col == 0) ?
				0 : REG_NOTBOL) == REG_NOMATCH ||
				start_col + startmatch.rm_so >= endpos)
			/* No more starts on this line. */
			break;

		    /* Translate the match to be relative to the
		     * beginning of the line. */
		    startmatch.rm_so += start_col;
		    startmatch.rm_eo += start_col;

		    x_start = (startmatch.rm_so <= startpos) ?
				0 : strnlenpt(fileptr->data,
				startmatch.rm_so) - start;

		    index = actual_x(converted, x_start);

		    if (regexec(varnish->end, fileptr->data +
				startmatch.rm_eo, 1, &endmatch,
				(startmatch.rm_eo == 0) ?
				0 : REG_NOTBOL) == 0) {
			/* Translate the end match to be relative to
			 * the beginning of the line. */
			endmatch.rm_so += startmatch.rm_eo;
			endmatch.rm_eo += startmatch.rm_eo;
			/* There is an end on this line.  But does
			 * it appear on this page, and is the match
			 * more than zero characters long? */
			if (endmatch.rm_eo > startpos &&
				endmatch.rm_eo > startmatch.rm_so) {
			    paintlen = actual_x(converted + index,
					strnlenpt(fileptr->data,
					endmatch.rm_eo) - start - x_start);

			    assert(0 <= x_start && x_start < editwincols);

			    mvwaddnstr(edit, line, x_start + margin,
					converted + index, paintlen);
			    if (paintlen > 0) {
				fileptr->multidata[varnish->id] = CSTARTENDHERE;
#ifdef DEBUG
    fprintf(stderr, "  Marking for id %i  line %i as CSTARTENDHERE\n", varnish->id, line);
#endif
			    }
			}
			start_col = endmatch.rm_eo;
			/* Skip over a zero-length match. */
			if (endmatch.rm_so == endmatch.rm_eo)
			    start_col += 1;
		    } else {
			/* There is no end on this line.  But we haven't yet
			 * looked for one on later lines. */
			end_line = fileptr->next;

			while (end_line != NULL &&
				regexec(varnish->end, end_line->data,
				0, NULL, 0) == REG_NOMATCH)
			    end_line = end_line->next;

			/* If there is no end, we're done on this line. */
			if (end_line == NULL)
			    break;

			assert(0 <= x_start && x_start < editwincols);

			/* Paint the rest of the line. */
			mvwaddnstr(edit, line, x_start + margin, converted + index, -1);
			fileptr->multidata[varnish->id] = CENDAFTER;
#ifdef DEBUG
    fprintf(stderr, "  Marking for id %i  line %i as CENDAFTER\n", varnish->id, line);
#endif
			/* We've painted to the end of the line, so don't
			 * bother checking for any more starts. */
			break;
		    }
		}
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
	int x_start;
	    /* The starting column for mvwaddnstr().  Zero-based. */
	int paintlen;
	    /* Number of characters to paint on this line.  There are
	     * COLS characters on a whole line. */
	size_t index;
	    /* Index in converted where we paint. */

	mark_order(&top, &top_x, &bot, &bot_x, NULL);

	if (top->lineno < fileptr->lineno || top_x < startpos)
	    top_x = startpos;
	if (bot->lineno > fileptr->lineno || bot_x > endpos)
	    bot_x = endpos;

	/* Only paint if the marked bit of fileptr is on this page. */
	if (top_x < endpos && bot_x > startpos) {
	    assert(startpos <= top_x);

	    /* x_start is the expanded location of the beginning of the
	     * mark minus the beginning of the page. */
	    x_start = strnlenpt(fileptr->data, top_x) - start;

	    /* If the end of the mark is off the page, paintlen is -1,
	     * meaning that everything on the line gets painted.
	     * Otherwise, paintlen is the expanded location of the end
	     * of the mark minus the expanded location of the beginning
	     * of the mark. */
	    if (bot_x >= endpos)
		paintlen = -1;
	    else
		paintlen = strnlenpt(fileptr->data, bot_x) - (x_start + start);

	    /* If x_start is before the beginning of the page, shift
	     * paintlen x_start characters to compensate, and put
	     * x_start at the beginning of the page. */
	    if (x_start < 0) {
		paintlen += x_start;
		x_start = 0;
	    }

	    assert(x_start >= 0 && x_start <= strlen(converted));

	    index = actual_x(converted, x_start);

	    if (paintlen > 0)
		paintlen = actual_x(converted + index, paintlen);

	    wattron(edit, hilite_attribute);
	    mvwaddnstr(edit, line, x_start + margin, converted + index, paintlen);
	    wattroff(edit, hilite_attribute);
	}
    }
#endif /* !NANO_TINY */
#ifdef ENABLE_LINENUMBERS
    last_drawn_line = fileptr->lineno;
    last_line_y = line;
#endif
}

/* Just update one line in the edit buffer.  This is basically a wrapper
 * for edit_draw().  The line will be displayed starting with
 * fileptr->data[index].  Likely arguments are current_x or zero.
 * Returns: Number of additional lines consumed (needed for SOFTWRAP). */
int update_line(filestruct *fileptr, size_t index)
{
    int line = 0;
	/* The line in the edit window that we want to update. */
    int extralinesused = 0;
    char *converted;
	/* fileptr->data converted to have tabs and control characters
	 * expanded. */
    size_t page_start;

    assert(fileptr != NULL);

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	filestruct *tmp;

	for (tmp = openfile->edittop; tmp && tmp != fileptr; tmp = tmp->next)
	    line += (strlenpt(tmp->data) / editwincols) + 1;
    } else
#endif
	line = fileptr->lineno - openfile->edittop->lineno;

    if (line < 0 || line >= editwinrows)
	return 1;

    /* First, blank out the line. */
    blank_line(edit, line, 0, COLS);

    /* Next, convert variables that index the line to their equivalent
     * positions in the expanded line. */
#ifndef NANO_TINY
    if (ISSET(SOFTWRAP))
	index = 0;
    else
#endif
	index = strnlenpt(fileptr->data, index);
    page_start = get_page_start(index);

    /* Expand the line, replacing tabs with spaces, and control
     * characters with their displayed forms. */
#ifdef NANO_TINY
    converted = display_string(fileptr->data, page_start, editwincols, TRUE);
#else
    converted = display_string(fileptr->data, page_start, editwincols, !ISSET(SOFTWRAP));
#ifdef DEBUG
    if (ISSET(SOFTWRAP) && strlen(converted) >= editwincols - 2)
	fprintf(stderr, "update_line(): converted(1) line = %s\n", converted);
#endif
#endif /* !NANO_TINY */

    /* Paint the line. */
    edit_draw(fileptr, converted, line, page_start);
    free(converted);

#ifndef NANO_TINY
    if (!ISSET(SOFTWRAP)) {
#endif
	if (page_start > 0)
	    mvwaddch(edit, line, margin, '$');
	if (strlenpt(fileptr->data) > page_start + editwincols)
	    mvwaddch(edit, line, COLS - 1, '$');
#ifndef NANO_TINY
    } else {
	size_t full_length = strlenpt(fileptr->data);
	for (index += editwincols; index <= full_length && line < editwinrows - 1; index += editwincols) {
	    line++;
#ifdef DEBUG
	    fprintf(stderr, "update_line(): softwrap code, moving to %d index %lu\n", line, (unsigned long)index);
#endif
	    blank_line(edit, line, 0, COLS);

	    /* Expand the line, replacing tabs with spaces, and control
	     * characters with their displayed forms. */
	    converted = display_string(fileptr->data, index, editwincols, !ISSET(SOFTWRAP));
#ifdef DEBUG
	    if (ISSET(SOFTWRAP) && strlen(converted) >= editwincols - 2)
		fprintf(stderr, "update_line(): converted(2) line = %s\n", converted);
#endif

	    /* Paint the line. */
	    edit_draw(fileptr, converted, line, index);
	    free(converted);
	    extralinesused++;
	}
    }
#endif /* !NANO_TINY */
    return extralinesused;
}

/* Check whether old_column and new_column are on different "pages" (or that
 * the mark is on), which means that the relevant line needs to be redrawn. */
bool need_horizontal_scroll(const size_t old_column, const size_t new_column)
{
#ifndef NANO_TINY
    if (openfile->mark_set)
	return TRUE;
    else
#endif
	return (get_page_start(old_column) != get_page_start(new_column));
}

/* When edittop changes, try and figure out how many lines
 * we really have to work with (i.e. set maxrows). */
void compute_maxrows(void)
{
    int n;
    filestruct *foo = openfile->edittop;

    if (!ISSET(SOFTWRAP)) {
	maxrows = editwinrows;
	return;
    }

    maxrows = 0;
    for (n = 0; n < editwinrows && foo; n++) {
	maxrows++;
	n += strlenpt(foo->data) / editwincols;
	foo = foo->next;
    }

    if (n < editwinrows)
	maxrows += editwinrows - n;

#ifdef DEBUG
    fprintf(stderr, "compute_maxrows(): maxrows = %d\n", maxrows);
#endif
}

/* Scroll the edit window in the given direction and the given number
 * of lines, and draw new lines on the blank lines left after the
 * scrolling.  direction is the direction to scroll, either UPWARD or
 * DOWNWARD, and nlines is the number of lines to scroll.  We change
 * edittop, and assume that current and current_x are up to date.  We
 * also assume that scrollok(edit) is FALSE. */
void edit_scroll(scroll_dir direction, ssize_t nlines)
{
    ssize_t i;
    filestruct *foo;

    assert(nlines > 0);

    /* Part 1: nlines is the number of lines we're going to scroll the
     * text of the edit window. */

    /* Move the top line of the edit window up or down (depending on the
     * value of direction) nlines lines, or as many lines as we can if
     * there are fewer than nlines lines available. */
    for (i = nlines; i > 0; i--) {
	if (direction == UPWARD) {
	    if (openfile->edittop == openfile->fileage)
		break;
	    openfile->edittop = openfile->edittop->prev;
	} else {
	    if (openfile->edittop == openfile->filebot)
		break;
	    openfile->edittop = openfile->edittop->next;
	}

#ifndef NANO_TINY
	/* Don't over-scroll on long lines. */
	if (ISSET(SOFTWRAP) && direction == UPWARD) {
	    ssize_t len = strlenpt(openfile->edittop->data) / editwincols;
	    i -= len;
	    if (len > 0)
		refresh_needed = TRUE;
	}
#endif
    }

    /* Limit nlines to the number of lines we could scroll. */
    nlines -= i;

    /* Don't bother scrolling zero lines, nor more than the window can hold. */
    if (nlines == 0)
	return;
    if (nlines >= editwinrows)
	refresh_needed = TRUE;

    if (refresh_needed == TRUE)
	return;

    /* Scroll the text of the edit window up or down nlines lines,
     * depending on the value of direction. */
    scrollok(edit, TRUE);
    wscrl(edit, (direction == UPWARD) ? -nlines : nlines);
    scrollok(edit, FALSE);

    /* Part 2: nlines is the number of lines in the scrolled region of
     * the edit window that we need to draw. */

    /* If the scrolled region contains only one line, and the line
     * before it is visible in the edit window, we need to draw it too.
     * If the scrolled region contains more than one line, and the lines
     * before and after the scrolled region are visible in the edit
     * window, we need to draw them too. */
    nlines += (nlines == 1) ? 1 : 2;

    if (nlines > editwinrows)
	nlines = editwinrows;

    /* If we scrolled up, we're on the line before the scrolled
     * region. */
    foo = openfile->edittop;

    /* If we scrolled down, move down to the line before the scrolled
     * region. */
    if (direction == DOWNWARD) {
	for (i = editwinrows - nlines; i > 0 && foo != NULL; i--)
	    foo = foo->next;
    }

    /* Draw new lines on any blank lines before or inside the scrolled
     * region.  If we scrolled down and we're on the top line, or if we
     * scrolled up and we're on the bottom line, the line won't be
     * blank, so we don't need to draw it unless the mark is on or we're
     * not on the first page. */
    for (i = nlines; i > 0 && foo != NULL; i--) {
	if ((i == nlines && direction == DOWNWARD) || (i == 1 &&
		direction == UPWARD)) {
	    if (need_horizontal_scroll(openfile->placewewant, 0))
		update_line(foo, (foo == openfile->current) ?
			openfile->current_x : 0);
	} else
	    update_line(foo, (foo == openfile->current) ?
		openfile->current_x : 0);
	foo = foo->next;
    }
    compute_maxrows();
}

/* Update any lines between old_current and current that need to be
 * updated.  Use this if we've moved without changing any text. */
void edit_redraw(filestruct *old_current)
{
    size_t was_pww = openfile->placewewant;

    openfile->placewewant = xplustabs();

    /* If the current line is offscreen, scroll until it's onscreen. */
    if (openfile->current->lineno >= openfile->edittop->lineno + maxrows ||
#ifndef NANO_TINY
		(openfile->current->lineno == openfile->edittop->lineno + maxrows - 1 &&
		ISSET(SOFTWRAP) && strlenpt(openfile->current->data) >= editwincols) ||
#endif
		openfile->current->lineno < openfile->edittop->lineno) {
	adjust_viewport((focusing || !ISSET(SMOOTH_SCROLL)) ? CENTERING : FLOWING);
	refresh_needed = TRUE;
	return;
    }

#ifndef NANO_TINY
    /* If the mark is on, update all lines between old_current and current. */
    if (openfile->mark_set) {
	filestruct *foo = old_current;

	while (foo != openfile->current) {
	    update_line(foo, 0);

	    foo = (foo->lineno > openfile->current->lineno) ?
			foo->prev : foo->next;
	}
    } else
#endif
	/* Otherwise, update old_current only if it differs from current
	 * and was horizontally scrolled. */
	if (old_current != openfile->current && get_page_start(was_pww) > 0)
	    update_line(old_current, 0);

    /* Update current if we've changed page, or if it differs from
     * old_current and needs to be horizontally scrolled. */
    if (need_horizontal_scroll(was_pww, openfile->placewewant) ||
			(old_current != openfile->current &&
			get_page_start(openfile->placewewant) > 0))
	update_line(openfile->current, openfile->current_x);
}

/* Refresh the screen without changing the position of lines.  Use this
 * if we've moved and changed text. */
void edit_refresh(void)
{
    filestruct *foo;
    int nlines;

    /* Figure out what maxrows should really be. */
    compute_maxrows();

    /* If the current line is out of view, get it back on screen. */
    if (openfile->current->lineno < openfile->edittop->lineno ||
		openfile->current->lineno >= openfile->edittop->lineno + maxrows) {
#ifdef DEBUG
	fprintf(stderr, "edit_refresh(): line = %ld, edittop %ld + maxrows %d\n",
		(long)openfile->current->lineno, (long)openfile->edittop->lineno, maxrows);
#endif
	adjust_viewport((focusing || !ISSET(SMOOTH_SCROLL)) ? CENTERING : STATIONARY);
    }

    foo = openfile->edittop;

#ifdef DEBUG
    fprintf(stderr, "edit_refresh(): edittop->lineno = %ld\n", (long)openfile->edittop->lineno);
#endif

    for (nlines = 0; nlines < editwinrows && foo != NULL; nlines++) {
	nlines += update_line(foo, (foo == openfile->current) ?
					openfile->current_x : 0);
	foo = foo->next;
    }

    for (; nlines < editwinrows; nlines++)
	blank_line(edit, nlines, 0, COLS);

    reset_cursor();
    curs_set(1);
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

    /* If manner is CENTERING, move edittop half the number of window
     * lines back from current.  If manner is STATIONARY, move edittop
     * back current_y lines if current_y is in range of the screen,
     * 0 lines if current_y is below zero, or (editwinrows - 1) lines
     * if current_y is too big.  This puts current at the same place
     * on the screen as before, or at the top or bottom if current_y is
     * beyond either.  If manner is FLOWING, move edittop back 0 lines
     * or (editwinrows - 1) lines, depending or where current has moved.
     * This puts the cursor on the first or the last line. */
    if (manner == CENTERING)
	goal = editwinrows / 2;
    else if (manner == FLOWING) {
	if (openfile->current->lineno >= openfile->edittop->lineno) {
	    goal = editwinrows - 1;
#ifndef NANO_TINY
	    if (ISSET(SOFTWRAP))
		goal -= strlenpt(openfile->current->data) / editwincols;
#endif
	}
    } else {
	goal = openfile->current_y;

	/* Limit goal to (editwinrows - 1) lines maximum. */
	if (goal > editwinrows - 1)
	    goal = editwinrows - 1;
    }

    openfile->edittop = openfile->current;

    while (goal > 0 && openfile->edittop->prev != NULL) {
	openfile->edittop = openfile->edittop->prev;
	goal--;
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
	    goal -= strlenpt(openfile->edittop->data) / editwincols;
	    if (goal < 0)
		openfile->edittop = openfile->edittop->next;
	}
#endif
    }
#ifdef DEBUG
    fprintf(stderr, "adjust_viewport(): setting edittop to lineno %ld\n", (long)openfile->edittop->lineno);
#endif
    compute_maxrows();
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

/* If constant is TRUE, we display the current cursor position only if
 * suppress_cursorpos is FALSE.  If constant is FALSE, we display the
 * position always.  In any case we reset suppress_cursorpos to FALSE. */
void do_cursorpos(bool constant)
{
    filestruct *f;
    char c;
    size_t i, cur_xpt = xplustabs() + 1;
    size_t cur_lenpt = strlenpt(openfile->current->data) + 1;
    int linepct, colpct, charpct;

    assert(openfile->fileage != NULL && openfile->current != NULL);

    /* Determine the size of the file up to the cursor. */
    f = openfile->current->next;
    c = openfile->current->data[openfile->current_x];

    openfile->current->next = NULL;
    openfile->current->data[openfile->current_x] = '\0';

    i = get_totsize(openfile->fileage, openfile->current);

    openfile->current->data[openfile->current_x] = c;
    openfile->current->next = f;

    /* If the position needs to be suppressed, don't suppress it next time. */
    if (suppress_cursorpos && constant) {
	suppress_cursorpos = FALSE;
	return;
    }

    /* Display the current cursor position on the statusbar. */
    linepct = 100 * openfile->current->lineno / openfile->filebot->lineno;
    colpct = 100 * cur_xpt / cur_lenpt;
    charpct = (openfile->totsize == 0) ? 0 : 100 * i / openfile->totsize;

    statusline(HUSH,
	_("line %ld/%ld (%d%%), col %lu/%lu (%d%%), char %lu/%lu (%d%%)"),
	(long)openfile->current->lineno,
	(long)openfile->filebot->lineno, linepct,
	(unsigned long)cur_xpt, (unsigned long)cur_lenpt, colpct,
	(unsigned long)i, (unsigned long)openfile->totsize, charpct);

    /* Displaying the cursor position should not suppress it next time. */
    suppress_cursorpos = FALSE;
}

/* Unconditionally display the current cursor position. */
void do_cursorpos_void(void)
{
    do_cursorpos(FALSE);
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
    size_t word_len = strlenpt(word), room;

    /* Compute the number of columns that are available for the word. */
    room = editwincols + get_page_start(xplustabs()) - xplustabs();

    assert(room > 0);

    if (word_len > room)
	room--;

    reset_cursor();
    wnoutrefresh(edit);

    if (active)
	wattron(edit, hilite_attribute);

    /* This is so we can show zero-length matches. */
    if (word_len == 0)
	waddch(edit, ' ');
    else
	waddnstr(edit, word, actual_x(word, room));

    if (word_len > room)
	waddch(edit, '$');

    if (active)
	wattroff(edit, hilite_attribute);
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
	    size_t start_x;

	    if (credits[crpos] == NULL) {
		assert(0 <= xlpos && xlpos < XLCREDIT_LEN);

		what = _(xlcredits[xlpos]);
		xlpos++;
	    } else
		what = credits[crpos];

	    start_x = COLS / 2 - strlenpt(what) / 2 - 1;
	    mvwaddstr(edit, editwinrows - 1 - (editwinrows % 2),
		start_x, what);
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
