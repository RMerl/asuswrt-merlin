/**************************************************************************
 *   move.c  --  This file is part of GNU nano.                           *
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

#include <string.h>
#include <ctype.h>

/* Move to the first line of the file. */
void do_first_line(void)
{
    openfile->current = openfile->edittop = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    refresh_needed = TRUE;
}

/* Move to the last line of the file. */
void do_last_line(void)
{
    openfile->current = openfile->filebot;
    openfile->current_x = strlen(openfile->filebot->data);
    openfile->placewewant = xplustabs();
    openfile->current_y = editwinrows - 1;

    refresh_needed = TRUE;
    focusing = FALSE;
}

/* Move up one page. */
void do_page_up(void)
{
    int i, mustmove, skipped = 0;

    /* If the cursor is less than a page away from the top of the file,
     * put it at the beginning of the first line. */
    if (openfile->current->lineno == 1 || (
#ifndef NANO_TINY
		!ISSET(SOFTWRAP) &&
#endif
		openfile->current->lineno <= editwinrows - 2)) {
	do_first_line();
	return;
    }

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
#ifndef NANO_TINY
    if (!ISSET(SMOOTH_SCROLL))
#endif
    {
	openfile->current = openfile->edittop;
	openfile->placewewant = openfile->current_y = 0;
    }

    mustmove = (editwinrows < 3) ? 1 : editwinrows - 2;

    for (i = mustmove; i - skipped > 0 && openfile->current != openfile->fileage; i--) {
	openfile->current = openfile->current->prev;
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP) && openfile->current) {
	    skipped += strlenpt(openfile->current->data) / COLS;
#ifdef DEBUG
	    fprintf(stderr, "do_page_up: i = %d, skipped = %d based on line %ld len %lu\n",
			i, skipped, (long)openfile->current->lineno, (unsigned long)strlenpt(openfile->current->data));
#endif
	}
#endif
    }

    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

#ifdef DEBUG
    fprintf(stderr, "do_page_up: openfile->current->lineno = %lu, skipped = %d\n",
			(unsigned long)openfile->current->lineno, skipped);
#endif

    /* Scroll the edit window up a page. */
    edit_update(STATIONARY);
    refresh_needed = TRUE;
}

/* Move down one page. */
void do_page_down(void)
{
    int i, mustmove;

    /* If the cursor is less than a page away from the bottom of the file,
     * put it at the end of the last line. */
    if (openfile->current->lineno + maxrows - 2 >= openfile->filebot->lineno) {
	do_last_line();
	return;
    }

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
#ifndef NANO_TINY
    if (!ISSET(SMOOTH_SCROLL))
#endif
    {
	openfile->current = openfile->edittop;
	openfile->placewewant = openfile->current_y = 0;
    }

    mustmove = (maxrows < 3) ? 1 : maxrows - 2;

    for (i = mustmove; i > 0 && openfile->current != openfile->filebot; i--) {
	openfile->current = openfile->current->next;
#ifdef DEBUG
	fprintf(stderr, "do_page_down: moving to line %lu\n", (unsigned long)openfile->current->lineno);
#endif

    }

    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* Scroll the edit window down a page. */
    edit_update(STATIONARY);
    refresh_needed = TRUE;
}

#ifndef DISABLE_JUSTIFY
/* Move up to the beginning of the last beginning-of-paragraph line
 * before the current line.  If allow_update is TRUE, update the screen
 * afterwards. */
void do_para_begin(bool allow_update)
{
    filestruct *current_save = openfile->current;

    if (openfile->current != openfile->fileage) {
	do {
	    openfile->current = openfile->current->prev;
	    openfile->current_y--;
	} while (!begpar(openfile->current));
    }

    openfile->current_x = 0;

    if (allow_update)
	edit_redraw(current_save);
}

/* Move up to the beginning of the last beginning-of-paragraph line
 * before the current line, and update the screen afterwards. */
void do_para_begin_void(void)
{
    do_para_begin(TRUE);
}

/* Move down to the beginning of the last line of the current paragraph.
 * Then move down one line farther if there is such a line, or to the
 * end of the current line if not.  If allow_update is TRUE, update the
 * screen afterwards.  A line is the last line of a paragraph if it is
 * in a paragraph, and the next line either is the beginning line of a
 * paragraph or isn't in a paragraph. */
void do_para_end(bool allow_update)
{
    filestruct *const current_save = openfile->current;

    while (openfile->current != openfile->filebot &&
		!inpar(openfile->current))
	openfile->current = openfile->current->next;

    while (openfile->current != openfile->filebot &&
		inpar(openfile->current->next) &&
		!begpar(openfile->current->next)) {
	openfile->current = openfile->current->next;
	openfile->current_y++;
    }

    if (openfile->current != openfile->filebot) {
	openfile->current = openfile->current->next;
	openfile->current_x = 0;
    } else
	openfile->current_x = strlen(openfile->current->data);

    if (allow_update)
	edit_redraw(current_save);
}

/* Move down to the beginning of the last line of the current paragraph.
 * Then move down one line farther if there is such a line, or to the
 * end of the current line if not, and update the screen afterwards. */
void do_para_end_void(void)
{
    do_para_end(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

#ifndef NANO_TINY
/* Move to the preceding block of text in the file. */
void do_prev_block(void)
{
    filestruct *was_current = openfile->current;
    bool is_text = FALSE, seen_text = FALSE;

    /* Skip backward until first blank line after some nonblank line(s). */
    while (openfile->current->prev != NULL && (!seen_text || is_text)) {
	openfile->current = openfile->current->prev;
	is_text = !white_string(openfile->current->data);
	seen_text = seen_text || is_text;
    }

    /* Step forward one line again if this one is blank. */
    if (openfile->current->next != NULL &&
		white_string(openfile->current->data))
	openfile->current = openfile->current->next;

    openfile->current_x = 0;
    edit_redraw(was_current);
}

/* Move to the next block of text in the file. */
void do_next_block(void)
{
    filestruct *was_current = openfile->current;
    bool is_white = white_string(openfile->current->data);
    bool seen_white = is_white;

    /* Skip forward until first nonblank line after some blank line(s). */
    while (openfile->current->next != NULL && (!seen_white || is_white)) {
	openfile->current = openfile->current->next;
	is_white = white_string(openfile->current->data);
	seen_white = seen_white || is_white;
    }

    openfile->current_x = 0;
    edit_redraw(was_current);
}

/* Move to the previous word in the file.  If allow_punct is TRUE, treat
 * punctuation as part of a word.  If allow_update is TRUE, update the
 * screen afterwards. */
void do_prev_word(bool allow_punct, bool allow_update)
{
    filestruct *current_save = openfile->current;
    bool seen_a_word = FALSE, step_forward = FALSE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    /* Move backward until we pass over the start of a word. */
    while (TRUE) {
	/* If at the head of a line, move to the end of the preceding one. */
	if (openfile->current_x == 0) {
	    if (openfile->current->prev == NULL)
		break;
	    openfile->current = openfile->current->prev;
	    openfile->current_x = strlen(openfile->current->data);
	}

	/* Step back one character. */
	openfile->current_x = move_mbleft(openfile->current->data,
						openfile->current_x);

	if (is_word_mbchar(openfile->current->data + openfile->current_x,
				allow_punct)) {
	    seen_a_word = TRUE;
	    /* If at the head of a line now, this surely is a word start. */
	    if (openfile->current_x == 0)
		break;
	} else if (seen_a_word) {
	    /* This is space now: we've overshot the start of the word. */
	    step_forward = TRUE;
	    break;
	}
    }

    if (step_forward)
	/* Move one character forward again to sit on the start of the word. */
	openfile->current_x = move_mbright(openfile->current->data,
						openfile->current_x);

    /* If allow_update is TRUE, update the screen. */
    if (allow_update) {
	focusing = FALSE;
	edit_redraw(current_save);
    }
}

/* Move to the previous word in the file, treating punctuation as part of a
 * word if the WORD_BOUNDS flag is set, and update the screen afterwards. */
void do_prev_word_void(void)
{
    do_prev_word(ISSET(WORD_BOUNDS), TRUE);
}

/* Move to the next word in the file.  If allow_punct is TRUE, treat
 * punctuation as part of a word.  If allow_update is TRUE, update the
 * screen afterwards.  Return TRUE if we started on a word, and FALSE
 * otherwise. */
bool do_next_word(bool allow_punct, bool allow_update)
{
    filestruct *current_save = openfile->current;
    bool started_on_word = is_word_mbchar(openfile->current->data +
				openfile->current_x, allow_punct);
    bool seen_space = !started_on_word;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    /* Move forward until we reach the start of a word. */
    while (TRUE) {
	/* If at the end of a line, move to the beginning of the next one. */
	if (openfile->current->data[openfile->current_x] == '\0') {
	    /* If at the end of the file, stop. */
	    if (openfile->current->next == NULL)
		break;
	    openfile->current = openfile->current->next;
	    openfile->current_x = 0;
	    seen_space = TRUE;
	} else {
	    /* Step forward one character. */
	    openfile->current_x = move_mbright(openfile->current->data,
						openfile->current_x);
	}

	/* If this is not a word character, then it's a separator; else
	 * if we've already seen a separator, then it's a word start. */
	if (!is_word_mbchar(openfile->current->data + openfile->current_x,
				allow_punct))
	    seen_space = TRUE;
	else if (seen_space)
	    break;
    }

    /* If allow_update is TRUE, update the screen. */
    if (allow_update) {
	focusing = FALSE;
	edit_redraw(current_save);
    }

    /* Return whether we started on a word. */
    return started_on_word;
}

/* Move to the next word in the file, treating punctuation as part of a word
 * if the WORD_BOUNDS flag is set, and update the screen afterwards. */
void do_next_word_void(void)
{
    do_next_word(ISSET(WORD_BOUNDS), TRUE);
}
#endif /* !NANO_TINY */

/* Move to the beginning of the current line.  If the SMART_HOME flag is
 * set, move to the first non-whitespace character of the current line
 * if we aren't already there, or to the beginning of the current line
 * if we are. */
void do_home(void)
{
    size_t was_column = xplustabs();

#ifndef NANO_TINY
    if (ISSET(SMART_HOME)) {
	size_t current_x_save = openfile->current_x;

	openfile->current_x = indent_length(openfile->current->data);

	if (openfile->current_x == current_x_save ||
		openfile->current->data[openfile->current_x] == '\0')
	    openfile->current_x = 0;
    } else
#endif
	openfile->current_x = 0;

    openfile->placewewant = xplustabs();

    if (need_horizontal_scroll(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* Move to the end of the current line. */
void do_end(void)
{
    size_t was_column = xplustabs();

    openfile->current_x = strlen(openfile->current->data);
    openfile->placewewant = xplustabs();

    if (need_horizontal_scroll(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* If scroll_only is FALSE, move up one line.  If scroll_only is TRUE,
 * scroll up one line without scrolling the cursor. */
void do_up(bool scroll_only)
{
    size_t was_column = xplustabs();

    /* If we're at the top of the file, or if scroll_only is TRUE and
     * the top of the file is onscreen, get out. */
    if (openfile->current == openfile->fileage
#ifndef NANO_TINY
	|| (scroll_only && openfile->edittop == openfile->fileage)
#endif
	)
	return;

    assert(ISSET(SOFTWRAP) || openfile->current_y == openfile->current->lineno - openfile->edittop->lineno);

    /* Move the current line of the edit window up. */
    openfile->current = openfile->current->prev;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* If scroll_only is FALSE and if we're on the first line of the
     * edit window, scroll the edit window up one line if we're in
     * smooth scrolling mode, or up half a page if we're not.  If
     * scroll_only is TRUE, scroll the edit window up one line
     * unconditionally. */
    if (openfile->current_y == 0
#ifndef NANO_TINY
	|| (ISSET(SOFTWRAP) && openfile->edittop->lineno == openfile->current->next->lineno) || scroll_only
#endif
	)
	edit_scroll(UPWARD,
#ifndef NANO_TINY
		(ISSET(SMOOTH_SCROLL) || scroll_only) ? 1 :
#endif
		editwinrows / 2 + 1);

    /* If the lines weren't already redrawn, see if they need to be. */
    if (openfile->current_y > 0) {
	/* Redraw the prior line if it was horizontally scrolled. */
	if (need_horizontal_scroll(was_column, 0))
	    update_line(openfile->current->next, 0);
	/* Redraw the current line if it needs to be horizontally scrolled. */
	if (need_horizontal_scroll(0, xplustabs()))
	    update_line(openfile->current, openfile->current_x);
    }
}

/* Move up one line. */
void do_up_void(void)
{
    do_up(FALSE);
}

/* If scroll_only is FALSE, move down one line.  If scroll_only is TRUE,
 * scroll down one line without scrolling the cursor. */
void do_down(bool scroll_only)
{
#ifndef NANO_TINY
    int amount = 0, enough;
    filestruct *topline;
#endif
    size_t was_column = xplustabs();

    /* If we're at the bottom of the file, get out. */
    if (openfile->current == openfile->filebot)
	return;

    assert(ISSET(SOFTWRAP) || openfile->current_y ==
		openfile->current->lineno - openfile->edittop->lineno);
    assert(openfile->current->next != NULL);

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	/* Compute the number of lines to scroll. */
	amount = strlenpt(openfile->current->data) / COLS - xplustabs() / COLS +
			strlenpt(openfile->current->next->data) / COLS +
			openfile->current_y - editwinrows + 2;
	topline = openfile->edittop;
	/* Reduce the amount when there are overlong lines at the top. */
	for (enough = 1; enough < amount; enough++) {
	    amount -= strlenpt(topline->data) / COLS;
	    if (amount <= 0) {
		amount = enough;
		break;
	    }
	    topline = topline->next;
	}
    }
#endif

    /* Move to the next line in the file. */
    openfile->current = openfile->current->next;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* If scroll_only is FALSE and if we're on the last line of the
     * edit window, scroll the edit window down one line if we're in
     * smooth scrolling mode, or down half a page if we're not.  If
     * scroll_only is TRUE, scroll the edit window down one line
     * unconditionally. */
    if (openfile->current_y == editwinrows - 1
#ifndef NANO_TINY
	|| amount > 0 || scroll_only
#endif
	) {
#ifndef NANO_TINY
	if (amount < 1 || scroll_only)
	    amount = 1;
#endif
	edit_scroll(DOWNWARD,
#ifndef NANO_TINY
		(ISSET(SMOOTH_SCROLL) || scroll_only) ? amount :
#endif
		editwinrows / 2 + 1);

	if (ISSET(SOFTWRAP)) {
	    refresh_needed = TRUE;
	    return;
	}
    }

    /* If the lines weren't already redrawn, see if they need to be. */
    if (openfile->current_y < editwinrows - 1) {
	/* Redraw the prior line if it was horizontally scrolled. */
	if (need_horizontal_scroll(was_column, 0))
	    update_line(openfile->current->prev, 0);
	/* Redraw the current line if it needs to be horizontally scrolled. */
	if (need_horizontal_scroll(0, xplustabs()))
	    update_line(openfile->current, openfile->current_x);
    }
}

/* Move down one line. */
void do_down_void(void)
{
    do_down(FALSE);
}

#ifndef NANO_TINY
/* Scroll up one line without scrolling the cursor. */
void do_scroll_up(void)
{
    do_up(TRUE);
}

/* Scroll down one line without scrolling the cursor. */
void do_scroll_down(void)
{
    do_down(TRUE);
}
#endif

/* Move left one character. */
void do_left(void)
{
    size_t was_column = xplustabs();

    if (openfile->current_x > 0)
	openfile->current_x = move_mbleft(openfile->current->data,
						openfile->current_x);
    else if (openfile->current != openfile->fileage) {
	do_up_void();
	openfile->current_x = strlen(openfile->current->data);
    }

    openfile->placewewant = xplustabs();

    if (need_horizontal_scroll(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* Move right one character. */
void do_right(void)
{
    size_t was_column = xplustabs();

    assert(openfile->current_x <= strlen(openfile->current->data));

    if (openfile->current->data[openfile->current_x] != '\0')
	openfile->current_x = move_mbright(openfile->current->data,
						openfile->current_x);
    else if (openfile->current != openfile->filebot) {
	openfile->current_x = 0;
	openfile->placewewant = 0;
	if (need_horizontal_scroll(was_column, 0))
	    update_line(openfile->current, 0);
	do_down_void();
	return;
    }

    openfile->placewewant = xplustabs();

    if (need_horizontal_scroll(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}
