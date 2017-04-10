/**************************************************************************
 *   prompt.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016 Benno Schulenberg                                 *
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static char *prompt = NULL;
	/* The prompt string used for statusbar questions. */
static size_t statusbar_x = HIGHEST_POSITIVE;
	/* The cursor position in answer. */

/* Read in a keystroke, interpret it if it is a shortcut or toggle, and
 * return it.  Set ran_func to TRUE if we ran a function associated with
 * a shortcut key, and set finished to TRUE if we're done after running
 * or trying to run a function associated with a shortcut key. */
int do_statusbar_input(bool *ran_func, bool *finished)
{
    int input;
	/* The character we read in. */
    static int *kbinput = NULL;
	/* The input buffer. */
    static size_t kbinput_len = 0;
	/* The length of the input buffer. */
    const sc *s;
    bool have_shortcut = FALSE;
    const subnfunc *f;

    *ran_func = FALSE;
    *finished = FALSE;

    /* Read in a character. */
    input = get_kbinput(bottomwin);

#ifndef NANO_TINY
    if (input == KEY_WINCH)
	return KEY_WINCH;
#endif

#ifndef DISABLE_MOUSE
    /* If we got a mouse click and it was on a shortcut, read in the
     * shortcut character. */
    if (input == KEY_MOUSE) {
	if (do_statusbar_mouse() == 1)
	    input = get_kbinput(bottomwin);
	else
	    return ERR;
    }
#endif

    /* Check for a shortcut in the current list. */
    s = get_shortcut(&input);

    /* If we got a shortcut from the current list, or a "universal"
     * statusbar prompt shortcut, set have_shortcut to TRUE. */
    have_shortcut = (s != NULL);

    /* If we got a non-high-bit control key, a meta key sequence, or a
     * function key, and it's not a shortcut or toggle, throw it out. */
    if (!have_shortcut) {
	if (is_ascii_cntrl_char(input) || meta_key || !is_byte(input)) {
	    beep();
	    input = ERR;
	}
    }

    /* If the keystroke isn't a shortcut nor a toggle, it's a normal text
     * character: add the it to the input buffer, when allowed. */
    if (input != ERR && !have_shortcut) {
	/* Only accept input when not in restricted mode, or when not at
	 * the "Write File" prompt, or when there is no filename yet. */
	if (!ISSET(RESTRICTED) || currmenu != MWRITEFILE ||
			openfile->filename[0] == '\0') {
	    kbinput_len++;
	    kbinput = (int *)nrealloc(kbinput, kbinput_len * sizeof(int));
	    kbinput[kbinput_len - 1] = input;
	}
     }

    /* If we got a shortcut, or if there aren't any other keystrokes waiting
     * after the one we read in, we need to insert all the characters in the
     * input buffer (if not empty) into the answer. */
    if ((have_shortcut || get_key_buffer_len() == 0) && kbinput != NULL) {
	/* Inject all characters in the input buffer at once, filtering out
	 * control characters. */
	do_statusbar_output(kbinput, kbinput_len, TRUE);

	/* Empty the input buffer. */
	kbinput_len = 0;
	free(kbinput);
	kbinput = NULL;
    }

    if (have_shortcut) {
	if (s->scfunc == do_tab || s->scfunc == do_enter)
	    ;
	else if (s->scfunc == do_left)
	    do_statusbar_left();
	else if (s->scfunc == do_right)
	    do_statusbar_right();
#ifndef NANO_TINY
	else if (s->scfunc == do_prev_word_void)
	    do_statusbar_prev_word();
	else if (s->scfunc == do_next_word_void)
	    do_statusbar_next_word();
#endif
	else if (s->scfunc == do_home_void)
	    do_statusbar_home();
	else if (s->scfunc == do_end_void)
	    do_statusbar_end();
	/* When in restricted mode at the "Write File" prompt and the
	 * filename isn't blank, disallow any input and deletion. */
	else if (ISSET(RESTRICTED) && currmenu == MWRITEFILE &&
				openfile->filename[0] != '\0' &&
				(s->scfunc == do_verbatim_input ||
				s->scfunc == do_cut_text_void ||
				s->scfunc == do_delete ||
				s->scfunc == do_backspace))
	    ;
	else if (s->scfunc == do_verbatim_input) {
	    do_statusbar_verbatim_input();
	} else if (s->scfunc == do_cut_text_void)
	    do_statusbar_cut_text();
	else if (s->scfunc == do_delete)
	    do_statusbar_delete();
	else if (s->scfunc == do_backspace)
	    do_statusbar_backspace();
	else {
	    /* Handle any other shortcut in the current menu, setting
	     * ran_func to TRUE if we try to run their associated functions,
	     * and setting finished to TRUE to indicatethat we're done after
	     * running or trying to run their associated functions. */
	    f = sctofunc(s);
	    if (s->scfunc != NULL) {
		*ran_func = TRUE;
		if (f && (!ISSET(VIEW_MODE) || f->viewok) &&
				f->scfunc != do_gotolinecolumn_void)
		    f->scfunc();
	    }
	    *finished = TRUE;
	}
    }

    return input;
}

#ifndef DISABLE_MOUSE
/* Handle a mouse click on the statusbar prompt or the shortcut list. */
int do_statusbar_mouse(void)
{
    int mouse_x, mouse_y;
    int retval = get_mouseinput(&mouse_x, &mouse_y, TRUE);

    /* We can click on the statusbar window text to move the cursor. */
    if (retval == 0 && wmouse_trafo(bottomwin, &mouse_y, &mouse_x, FALSE)) {
	size_t start_col;

	start_col = strlenpt(prompt) + 2;

	/* Move to where the click occurred. */
	if (mouse_x >= start_col && mouse_y == 0) {
	    statusbar_x = actual_x(answer,
			get_statusbar_page_start(start_col, start_col +
			statusbar_xplustabs()) + mouse_x - start_col);
	    update_the_statusbar();
	}
    }

    return retval;
}
#endif

/* The user typed input_len multibyte characters.  Add them to the answer,
 * filtering out ASCII control characters if filtering is TRUE. */
void do_statusbar_output(int *the_input, size_t input_len,
	bool filtering)
{
    char *output = charalloc(input_len + 1);
    char *char_buf = charalloc(mb_cur_max());
    int i, char_len;

    /* Copy the typed stuff so it can be treated. */
    for (i = 0; i < input_len; i++)
	output[i] = (char)the_input[i];
    output[i] = '\0';

    i = 0;

    while (i < input_len) {
	/* Encode any NUL byte as 0x0A. */
	if (output[i] == '\0')
	    output[i] = '\n';

	/* Interpret the next multibyte character. */
	char_len = parse_mbchar(output + i, char_buf, NULL);

	i += char_len;

	/* When filtering, skip any ASCII control character. */
	if (filtering && is_ascii_cntrl_char(*(output + i - char_len)))
	    continue;

	/* Insert the typed character into the existing answer string. */
	answer = charealloc(answer, strlen(answer) + char_len + 1);
	charmove(answer + statusbar_x + char_len, answer + statusbar_x,
				strlen(answer) - statusbar_x + 1);
	strncpy(answer + statusbar_x, char_buf, char_len);

	statusbar_x += char_len;
    }

    free(char_buf);
    free(output);

    update_the_statusbar();
}

/* Move to the beginning of the answer. */
void do_statusbar_home(void)
{
    statusbar_x = 0;
    update_the_statusbar();
}

/* Move to the end of the answer. */
void do_statusbar_end(void)
{
    statusbar_x = strlen(answer);
    update_the_statusbar();
}

/* Move left one character. */
void do_statusbar_left(void)
{
    if (statusbar_x > 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);
	update_the_statusbar();
    }
}

/* Move right one character. */
void do_statusbar_right(void)
{
    if (answer[statusbar_x] != '\0') {
	statusbar_x = move_mbright(answer, statusbar_x);
	update_the_statusbar();
    }
}

/* Backspace over one character. */
void do_statusbar_backspace(void)
{
    if (statusbar_x > 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);
	do_statusbar_delete();
    }
}

/* Delete one character. */
void do_statusbar_delete(void)
{
    if (answer[statusbar_x] != '\0') {
	int char_len = parse_mbchar(answer + statusbar_x, NULL, NULL);

	charmove(answer + statusbar_x, answer + statusbar_x + char_len,
			strlen(answer) - statusbar_x - char_len + 1);

	update_the_statusbar();
    }
}

/* Zap some or all text from the answer. */
void do_statusbar_cut_text(void)
{
    if (!ISSET(CUT_TO_END))
	statusbar_x = 0;

    answer[statusbar_x] = '\0';

    update_the_statusbar();
}

#ifndef NANO_TINY
/* Move to the next word in the answer. */
void do_statusbar_next_word(void)
{
    bool seen_space = !is_word_mbchar(answer + statusbar_x, FALSE);

    /* Move forward until we reach the start of a word. */
    while (answer[statusbar_x] != '\0') {
	statusbar_x = move_mbright(answer, statusbar_x);

	/* If this is not a word character, then it's a separator; else
	 * if we've already seen a separator, then it's a word start. */
	if (!is_word_mbchar(answer + statusbar_x, FALSE))
	    seen_space = TRUE;
	else if (seen_space)
	    break;
    }

    update_the_statusbar();
}

/* Move to the previous word in the answer. */
void do_statusbar_prev_word(void)
{
    bool seen_a_word = FALSE, step_forward = FALSE;

    /* Move backward until we pass over the start of a word. */
    while (statusbar_x != 0) {
	statusbar_x = move_mbleft(answer, statusbar_x);

	if (is_word_mbchar(answer + statusbar_x, FALSE))
	    seen_a_word = TRUE;
	else if (seen_a_word) {
	    /* This is space now: we've overshot the start of the word. */
	    step_forward = TRUE;
	    break;
	}
    }

    if (step_forward)
	/* Move one character forward again to sit on the start of the word. */
	statusbar_x = move_mbright(answer, statusbar_x);

    update_the_statusbar();
}
#endif /* !NANO_TINY */

/* Get verbatim input and inject it into the answer, without filtering. */
void do_statusbar_verbatim_input(void)
{
    int *kbinput;
    size_t kbinput_len;

    kbinput = get_verbatim_kbinput(bottomwin, &kbinput_len);

    do_statusbar_output(kbinput, kbinput_len, FALSE);
}

/* Return the zero-based column position of the cursor in the answer. */
size_t statusbar_xplustabs(void)
{
    return strnlenpt(answer, statusbar_x);
}

/* Return the column number of the first character of the answer that is
 * displayed in the statusbar when the cursor is at the given column,
 * with the available room for the answer starting at base.  Note that
 * (0 <= column - get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t base, size_t column)
{
    if (column == base || column < COLS - 1)
	return 0;
    else if (COLS > base + 2)
	return column - base - 1 - (column - base - 1) % (COLS - base - 2);
    else
	return column - 2;
}

/* Reinitialize the cursor position in the answer. */
void reinit_statusbar_x(void)
{
    statusbar_x = HIGHEST_POSITIVE;
}

/* Put the cursor in the answer at statusbar_x. */
void reset_statusbar_cursor(void)
{
    size_t start_col = strlenpt(prompt) + 2;
    size_t xpt = statusbar_xplustabs();

    /* Work around a cursor-misplacement bug in VTEs. */
    wmove(bottomwin, 0, 0);
    wnoutrefresh(bottomwin);
    doupdate();

    wmove(bottomwin, 0, start_col + xpt -
			get_statusbar_page_start(start_col, start_col + xpt));

    wnoutrefresh(bottomwin);
}

/* Repaint the statusbar. */
void update_the_statusbar(void)
{
    size_t base, the_page, end_page;
    char *expanded;

    base = strlenpt(prompt) + 2;
    the_page = get_statusbar_page_start(base, base + strnlenpt(answer, statusbar_x));
    end_page = get_statusbar_page_start(base, base + strlenpt(answer) - 1);

    wattron(bottomwin, interface_color_pair[TITLE_BAR]);

    blank_statusbar();

    mvwaddstr(bottomwin, 0, 0, prompt);
    waddch(bottomwin, ':');
    waddch(bottomwin, (the_page == 0) ? ' ' : '<');

    expanded = display_string(answer, the_page, COLS - base - 1, FALSE);
    waddstr(bottomwin, expanded);
    free(expanded);

    waddch(bottomwin, (the_page >= end_page) ? ' ' : '>');

    wattroff(bottomwin, interface_color_pair[TITLE_BAR]);

    reset_statusbar_cursor();
}

/* Get a string of input at the statusbar prompt. */
functionptrtype acquire_an_answer(int *actual, bool allow_tabs,
	bool allow_files, bool *listed,
#ifndef DISABLE_HISTORIES
	filestruct **history_list,
#endif
	void (*refresh_func)(void))
{
    int kbinput = ERR;
    bool ran_func, finished;
    functionptrtype func;
#ifndef DISABLE_TABCOMP
    bool tabbed = FALSE;
	/* Whether we've pressed Tab. */
#endif
#ifndef DISABLE_HISTORIES
    char *history = NULL;
	/* The current history string. */
    char *magichistory = NULL;
	/* The temporary string typed at the bottom of the history, if
	 * any. */
#ifndef DISABLE_TABCOMP
    int last_kbinput = ERR;
	/* The key we pressed before the current key. */
    size_t complete_len = 0;
	/* The length of the original string that we're trying to
	 * tab complete, if any. */
#endif
#endif /* !DISABLE_HISTORIES */

    if (statusbar_x > strlen(answer))
	statusbar_x = strlen(answer);

#ifdef DEBUG
    fprintf(stderr, "acquiring: answer = \"%s\", statusbar_x = %lu\n", answer, (unsigned long) statusbar_x);
#endif

    update_the_statusbar();

    /* Refresh edit window and statusbar before getting input. */
    wnoutrefresh(edit);
    wnoutrefresh(bottomwin);

    while (TRUE) {
	/* Ensure the cursor is shown when waiting for input. */
	curs_set(1);

	kbinput = do_statusbar_input(&ran_func, &finished);

#ifndef NANO_TINY
	/* If the window size changed, go reformat the prompt string. */
	if (kbinput == KEY_WINCH) {
	    refresh_func();
	    *actual = KEY_WINCH;
#ifndef DISABLE_HISTORIES
	    free(magichistory);
#endif
	    return NULL;
	}
#endif /* !NANO_TINY */

	func = func_from_key(&kbinput);

	if (func == do_cancel || func == do_enter)
	    break;

#ifndef DISABLE_TABCOMP
	if (func != do_tab)
	    tabbed = FALSE;

	if (func == do_tab) {
#ifndef DISABLE_HISTORIES
	    if (history_list != NULL) {
		if (last_kbinput != the_code_for(do_tab, TAB_CODE))
		    complete_len = strlen(answer);

		if (complete_len > 0) {
		    answer = get_history_completion(history_list,
					answer, complete_len);
		    statusbar_x = strlen(answer);
		}
	    } else
#endif
	    if (allow_tabs)
		answer = input_tab(answer, allow_files, &statusbar_x,
					&tabbed, refresh_func, listed);
	} else
#endif /* !DISABLE_TABCOMP */
#ifndef DISABLE_HISTORIES
	if (func == get_history_older_void) {
	    if (history_list != NULL) {
		/* If we're scrolling up at the bottom of the history list
		 * and answer isn't blank, save answer in magichistory. */
		if ((*history_list)->next == NULL && *answer != '\0')
		    magichistory = mallocstrcpy(magichistory, answer);

		/* Get the older search from the history list and save it in
		 * answer.  If there is no older search, don't do anything. */
		if ((history = get_history_older(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    statusbar_x = strlen(answer);
		}

		/* This key has a shortcut-list entry when it's used to
		 * move to an older search, which means that finished has
		 * been set to TRUE.  Set it back to FALSE here, so that
		 * we aren't kicked out of the statusbar prompt. */
		finished = FALSE;
	    }
	} else if (func == get_history_newer_void) {
	    if (history_list != NULL) {
		/* Get the newer search from the history list and save it in
		 * answer.  If there is no newer search, don't do anything. */
		if ((history = get_history_newer(history_list)) != NULL) {
		    answer = mallocstrcpy(answer, history);
		    statusbar_x = strlen(answer);
		}

		/* If, after scrolling down, we're at the bottom of the
		 * history list, answer is blank, and magichistory is set,
		 * save magichistory in answer. */
		if ((*history_list)->next == NULL &&
			*answer == '\0' && magichistory != NULL) {
		    answer = mallocstrcpy(answer, magichistory);
		    statusbar_x = strlen(answer);
		}

		/* This key has a shortcut-list entry when it's used to
		 * move to a newer search, which means that finished has
		 * been set to TRUE.  Set it back to FALSE here, so that
		 * we aren't kicked out of the statusbar prompt. */
		finished = FALSE;
	    }
	} else
#endif /* !DISABLE_HISTORIES */
	if (func == do_help_void) {
	    /* This key has a shortcut-list entry when it's used to go to
	     * the help browser or display a message indicating that help
	     * is disabled, which means that finished has been set to TRUE.
	     * Set it back to FALSE here, so that we aren't kicked out of
	     * the statusbar prompt. */
	    finished = FALSE;
	}

	/* If we have a shortcut with an associated function, break out if
	 * we're finished after running or trying to run the function. */
	if (finished)
	    break;

	update_the_statusbar();

#if !defined(DISABLE_HISTORIES) && !defined(DISABLE_TABCOMP)
	last_kbinput = kbinput;
#endif
    }

#ifndef DISABLE_HISTORIES
    /* Set the current position in the history list to the bottom. */
    if (history_list != NULL) {
	history_reset(*history_list);
	free(magichistory);
    }
#endif

    *actual = kbinput;

    return func;
}

/* Ask a question on the statusbar.  The prompt will be stored in the
 * static prompt, which should be NULL initially, and the answer will be
 * stored in the answer global.  Returns -1 on aborted enter, -2 on a
 * blank string, and 0 otherwise, the valid shortcut key caught.
 * curranswer is any editable text that we want to put up by default,
 * and refresh_func is the function we want to call to refresh the edit
 * window.
 *
 * The allow_tabs parameter indicates whether we should allow tabs to be
 * interpreted.  The allow_files parameter indicates whether we should
 * allow all files (as opposed to just directories) to be tab completed. */
int do_prompt(bool allow_tabs, bool allow_files,
	int menu, const char *curranswer,
#ifndef DISABLE_HISTORIES
	filestruct **history_list,
#endif
	void (*refresh_func)(void), const char *msg, ...)
{
    va_list ap;
    int retval;
    functionptrtype func = NULL;
    bool listed = FALSE;
    /* Save a possible current statusbar x position. */
    size_t was_statusbar_x = statusbar_x;

    bottombars(menu);

    answer = mallocstrcpy(answer, curranswer);

#ifndef NANO_TINY
  redo_theprompt:
#endif
    prompt = charalloc((COLS * mb_cur_max()) + 1);
    va_start(ap, msg);
    vsnprintf(prompt, COLS * mb_cur_max(), msg, ap);
    va_end(ap);
    /* Reserve five columns for colon plus angles plus answer, ":<aa>". */
    prompt[actual_x(prompt, (COLS < 5) ? 0 : COLS - 5)] = '\0';

    func = acquire_an_answer(&retval, allow_tabs, allow_files, &listed,
#ifndef DISABLE_HISTORIES
			history_list,
#endif
			refresh_func);

    free(prompt);
    prompt = NULL;

#ifndef NANO_TINY
    if (retval == KEY_WINCH)
	goto redo_theprompt;
#endif

    /* If we're done with this prompt, restore the x position to what
     * it was at a possible previous prompt. */
    if (func == do_cancel || func == do_enter)
	statusbar_x = was_statusbar_x;

    /* If we left the prompt via Cancel or Enter, set the return value
     * properly. */
    if (func == do_cancel)
	retval = -1;
    else if (func == do_enter)
	retval = (*answer == '\0') ? -2 : 0;

    blank_statusbar();
    wnoutrefresh(bottomwin);

#ifdef DEBUG
    fprintf(stderr, "answer = \"%s\"\n", answer);
#endif

#ifndef DISABLE_TABCOMP
    /* If we've done tab completion, there might still be a list of
     * filename matches on the edit window.  Clear them off. */
    if (listed)
	refresh_func();
#endif

    return retval;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the statusbar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * TRUE when passed in), and -1 for Cancel. */
int do_yesno_prompt(bool all, const char *msg)
{
    int response = -2, width = 16;
    char *message = display_string(msg, 0, COLS, FALSE);

    /* TRANSLATORS: For the next three strings, if possible, specify
     * the single-byte shortcuts for both your language and English.
     * For example, in French: "OoYy", for both "Oui" and "Yes". */
    const char *yesstr = _("Yy");
    const char *nostr = _("Nn");
    const char *allstr = _("Aa");

    /* The above three variables consist of all the single-byte characters
     * that are accepted for the corresponding answer.  Of each variable,
     * the first character is displayed in the help lines. */

    while (response == -2) {
	int kbinput;
	functionptrtype func;

	if (!ISSET(NO_HELP)) {
	    char shortstr[3];
		/* Temporary string for (translated) " Y", " N" and " A". */

	    if (COLS < 32)
		width = COLS / 2;

	    /* Clear the shortcut list from the bottom of the screen. */
	    blank_bottombars();

	    /* Now show the ones for "Yes", "No", "Cancel" and maybe "All". */
	    sprintf(shortstr, " %c", yesstr[0]);
	    wmove(bottomwin, 1, 0);
	    onekey(shortstr, _("Yes"), width);

	    if (all) {
		shortstr[1] = allstr[0];
		wmove(bottomwin, 1, width);
		onekey(shortstr, _("All"), width);
	    }

	    shortstr[1] = nostr[0];
	    wmove(bottomwin, 2, 0);
	    onekey(shortstr, _("No"), width);

	    wmove(bottomwin, 2, width);
	    onekey("^C", _("Cancel"), width);
	}

	/* Color the statusbar over its full width and display the question. */
	wattron(bottomwin, interface_color_pair[TITLE_BAR]);
	blank_statusbar();
	mvwaddnstr(bottomwin, 0, 0, message, actual_x(message, COLS - 1));
	wattroff(bottomwin, interface_color_pair[TITLE_BAR]);

	wnoutrefresh(bottomwin);

	/* When not replacing, show the cursor. */
	if (!all)
	    curs_set(1);

	currmenu = MYESNO;
	kbinput = get_kbinput(bottomwin);

	func = func_from_key(&kbinput);

	if (func == do_cancel)
	    response = -1;
#ifndef DISABLE_MOUSE
	else if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;
	    /* We can click on the Yes/No/All shortcuts to select an answer. */
	    if (get_mouseinput(&mouse_x, &mouse_y, FALSE) == 0 &&
			wmouse_trafo(bottomwin, &mouse_y, &mouse_x, FALSE) &&
			mouse_x < (width * 2) && mouse_y > 0) {
		int x = mouse_x / width;
			/* The x-coordinate among the Yes/No/All shortcuts. */
		int y = mouse_y - 1;
			/* The y-coordinate among the Yes/No/All shortcuts. */

		assert(0 <= x && x <= 1 && 0 <= y && y <= 1);

		/* x == 0 means they clicked Yes or No.
		 * y == 0 means Yes or All. */
		response = -2 * x * y + x - y + 1;

		if (response == 2 && !all)
		    response = -2;
	    }
	}
#endif /* !DISABLE_MOUSE */
	else {
	    /* Look for the kbinput in the Yes, No (and All) strings. */
	    if (strchr(yesstr, kbinput) != NULL)
		response = 1;
	    else if (strchr(nostr, kbinput) != NULL)
		response = 0;
	    else if (all && strchr(allstr, kbinput) != NULL)
		response = 2;
	}
    }

    free(message);

    return response;
}
