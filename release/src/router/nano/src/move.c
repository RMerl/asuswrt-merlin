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
    openfile->current = openfile->fileage;
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

    /* Set the last line of the screen as the target for the cursor. */
    openfile->current_y = editwinrows - 1;

    refresh_needed = TRUE;
    focusing = FALSE;
}

/* Determine the actual current chunk and the target column. */
void get_edge_and_target(size_t *leftedge, size_t *target_column)
{
#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	size_t realspan = strlenpt(openfile->current->data);

	if (openfile->placewewant < realspan)
	    realspan = openfile->placewewant;

	*leftedge = (realspan / editwincols) * editwincols;
	*target_column = openfile->placewewant % editwincols;
    } else
#endif
    {
	*leftedge = 0;
	*target_column = openfile->placewewant;
    }
}

/* Move up nearly one screenful. */
void do_page_up(void)
{
    int mustmove = (editwinrows < 3) ? 1 : editwinrows - 2;
    size_t leftedge, target_column;

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
    if (!ISSET(SMOOTH_SCROLL)) {
	openfile->current = openfile->edittop;
	openfile->placewewant = openfile->firstcolumn;
	openfile->current_y = 0;
    }

    get_edge_and_target(&leftedge, &target_column);

    /* Move up the required number of lines or chunks.  If we can't, we're
     * at the top of the file, so put the cursor there and get out. */
    if (go_back_chunks(mustmove, &openfile->current, &leftedge) > 0) {
	do_first_line();
	return;
    }

    openfile->placewewant = leftedge + target_column;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* Scroll the edit window up a page. */
    adjust_viewport(STATIONARY);
    refresh_needed = TRUE;
}

/* Move down nearly one screenful. */
void do_page_down(void)
{
    int mustmove = (editwinrows < 3) ? 1 : editwinrows - 2;
    size_t leftedge, target_column;

    /* If we're not in smooth scrolling mode, put the cursor at the
     * beginning of the top line of the edit window, as Pico does. */
    if (!ISSET(SMOOTH_SCROLL)) {
	openfile->current = openfile->edittop;
	openfile->placewewant = openfile->firstcolumn;
	openfile->current_y = 0;
    }

    get_edge_and_target(&leftedge, &target_column);

    /* Move down the required number of lines or chunks.  If we can't, we're
     * at the bottom of the file, so put the cursor there and get out. */
    if (go_forward_chunks(mustmove, &openfile->current, &leftedge) > 0) {
	do_last_line();
	return;
    }

    openfile->placewewant = leftedge + target_column;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* Scroll the edit window down a page. */
    adjust_viewport(STATIONARY);
    refresh_needed = TRUE;
}

#ifndef DISABLE_JUSTIFY
/* Move up to the beginning of the last beginning-of-paragraph line
 * before the current line.  If allow_update is TRUE, update the screen
 * afterwards. */
void do_para_begin(bool allow_update)
{
    filestruct *was_current = openfile->current;

    if (openfile->current != openfile->fileage) {
	do
	    openfile->current = openfile->current->prev;
	while (!begpar(openfile->current));
    }

    openfile->current_x = 0;

    if (allow_update)
	edit_redraw(was_current);
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
    filestruct *was_current = openfile->current;

    while (openfile->current != openfile->filebot &&
		!inpar(openfile->current))
	openfile->current = openfile->current->next;

    while (openfile->current != openfile->filebot &&
		inpar(openfile->current->next) &&
		!begpar(openfile->current->next)) {
	openfile->current = openfile->current->next;
    }

    if (openfile->current != openfile->filebot) {
	openfile->current = openfile->current->next;
	openfile->current_x = 0;
    } else
	openfile->current_x = strlen(openfile->current->data);

    if (allow_update)
	edit_redraw(was_current);
}

/* Move down to the beginning of the last line of the current paragraph.
 * Then move down one line farther if there is such a line, or to the
 * end of the current line if not, and update the screen afterwards. */
void do_para_end_void(void)
{
    do_para_end(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

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
    filestruct *was_current = openfile->current;
    bool seen_a_word = FALSE, step_forward = FALSE;

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
	edit_redraw(was_current);
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
    filestruct *was_current = openfile->current;
    bool started_on_word = is_word_mbchar(openfile->current->data +
				openfile->current_x, allow_punct);
    bool seen_space = !started_on_word;

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
	edit_redraw(was_current);
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

/* Move to the beginning of the current line (or softwrapped chunk).
 * If be_clever is TRUE, do a smart home when wanted and possible,
 * and do a dynamic home when in softwrap mode and it'spossible.
 * If be_clever is FALSE, just do a simple home. */
void do_home(bool be_clever)
{
    filestruct *was_current = openfile->current;
    size_t was_column = xplustabs();
    bool moved_off_chunk = TRUE;
#ifndef NANO_TINY
    bool moved = FALSE;
    size_t leftedge_x = 0;

    if (ISSET(SOFTWRAP))
	leftedge_x = actual_x(openfile->current->data,
			(was_column / editwincols) * editwincols);

    if (ISSET(SMART_HOME) && be_clever) {
	size_t indent_x = indent_length(openfile->current->data);

	if (openfile->current->data[indent_x] != '\0') {
	    /* If we're exactly on the indent, move fully home.  Otherwise,
	     * when not softwrapping or not after the first nonblank chunk,
	     * move to the first nonblank character. */
	    if (openfile->current_x == indent_x) {
		openfile->current_x = 0;
		moved = TRUE;
	    } else if (!ISSET(SOFTWRAP) || leftedge_x <= indent_x) {
		openfile->current_x = indent_x;
		moved = TRUE;
	    }
	}
    }

    if (!moved && ISSET(SOFTWRAP)) {
	/* If already at the left edge of the screen, move fully home.
	 * Otherwise, move to the left edge. */
	if (openfile->current_x == leftedge_x && be_clever)
	    openfile->current_x = 0;
	else {
	    openfile->current_x = leftedge_x;
	    moved_off_chunk = FALSE;
	}
    } else if (!moved)
#endif
	openfile->current_x = 0;

    openfile->placewewant = xplustabs();

    /* If we changed chunk, we might be offscreen.  Otherwise,
     * update current if the mark is on or we changed "page". */
    if (ISSET(SOFTWRAP) && moved_off_chunk) {
	focusing = FALSE;
	edit_redraw(was_current);
    } else if (line_needs_update(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* Do a (smart or dynamic) home. */
void do_home_void(void)
{
    do_home(TRUE);
}

/* Move to the end of the current line (or softwrapped chunk).
 * If be_clever is TRUE, do a dynamic end when in softwrap mode and
 * it's possible.  If be_clever is FALSE, just do a simple end. */
void do_end(bool be_clever)
{
    filestruct *was_current = openfile->current;
    size_t was_column = xplustabs();
    size_t line_len = strlen(openfile->current->data);
    bool moved_off_chunk = TRUE;

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP)) {
	size_t rightedge_x = actual_x(openfile->current->data,
			((was_column / editwincols) * editwincols) +
			(editwincols - 1));

	/* If already at the right edge of the screen, move fully to
	 * the end of the line.  Otherwise, move to the right edge. */
	if (openfile->current_x == rightedge_x && be_clever)
	    openfile->current_x = line_len;
	else {
	    openfile->current_x = rightedge_x;
	    moved_off_chunk = FALSE;
	}
    } else
#endif
	openfile->current_x = line_len;

    openfile->placewewant = xplustabs();

    /* If we changed chunk, we might be offscreen.  Otherwise,
     * update current if the mark is on or we changed "page". */
    if (ISSET(SOFTWRAP) && moved_off_chunk) {
	focusing = FALSE;
	edit_redraw(was_current);
    } else if (line_needs_update(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* Do a (dynamic) end. */
void do_end_void(void)
{
    do_end(TRUE);
}

/* Move the cursor to the preceding line or chunk.  If scroll_only is TRUE,
 * also scroll the screen one row, so the cursor stays in the same spot. */
void do_up(bool scroll_only)
{
    filestruct *was_current = openfile->current;
    size_t was_column = xplustabs();
    size_t leftedge, target_column;

    /* When just scrolling and the top of the file is onscreen, get out. */
    if (scroll_only && openfile->edittop == openfile->fileage &&
			openfile->firstcolumn == 0)
	return;

    get_edge_and_target(&leftedge, &target_column);

    /* If we can't move up one line or chunk, we're at top of file. */
    if (go_back_chunks(1, &openfile->current, &leftedge) > 0)
	return;

    openfile->placewewant = leftedge + target_column;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* When the cursor was on the first line of the edit window (or when just
     * scrolling without moving the cursor), scroll the edit window up -- one
     * line if we're in smooth scrolling mode, and half a page otherwise. */
    if (openfile->current_y == 0 || scroll_only)
	edit_scroll(UPWARD, (ISSET(SMOOTH_SCROLL) || scroll_only) ?
				1 : editwinrows / 2 + 1);

    if (openfile->current_y > 0) {
	/* Redraw the prior line if it's not offscreen, and it's not the same
	 * line as the current one, and the line was horizontally scrolled. */
	if ((!scroll_only || openfile->current_y < editwinrows - 1) &&
			openfile->current != was_current &&
			line_needs_update(was_column, 0))
	    update_line(openfile->current->next, 0);
	/* Redraw the current line if it needs to be horizontally scrolled. */
	if (line_needs_update(0, xplustabs()))
	    update_line(openfile->current, openfile->current_x);
    }
}

/* Move up one line or chunk. */
void do_up_void(void)
{
    do_up(FALSE);
}

/* Move the cursor to next line or chunk.  If scroll_only is TRUE, also
 * scroll the screen one row, so the cursor stays in the same spot. */
void do_down(bool scroll_only)
{
    filestruct *was_current = openfile->current;
    size_t was_column = xplustabs();
    size_t leftedge, target_column;

    get_edge_and_target(&leftedge, &target_column);

    /* If we can't move down one line or chunk, we're at bottom of file. */
    if (go_forward_chunks(1, &openfile->current, &leftedge) > 0)
	return;

    openfile->placewewant = leftedge + target_column;
    openfile->current_x = actual_x(openfile->current->data,
					openfile->placewewant);

    /* When the cursor was on the last line of the edit window (or when just
     * scrolling without moving the cursor), scroll the edit window down -- one
     * line if we're in smooth scrolling mode, and half a page otherwise. */
    if (openfile->current_y == editwinrows - 1 || scroll_only)
	edit_scroll(DOWNWARD, (ISSET(SMOOTH_SCROLL) || scroll_only) ?
				1 : editwinrows / 2 + 1);

    if (openfile->current_y < editwinrows - 1) {
	/* Redraw the prior line if it's not offscreen, and it's not the same
	 * line as the current one, and the line was horizontally scrolled. */
	if ((!scroll_only || openfile->current_y > 0) &&
			openfile->current != was_current &&
			line_needs_update(was_column, 0))
	    update_line(openfile->current->prev, 0);
	/* Redraw the current line if it needs to be horizontally scrolled. */
	if (line_needs_update(0, xplustabs()))
	    update_line(openfile->current, openfile->current_x);
    }
}

/* Move down one line or chunk. */
void do_down_void(void)
{
    do_down(FALSE);
}

#ifndef NANO_TINY
/* Scroll up one line or chunk without scrolling the cursor. */
void do_scroll_up(void)
{
    do_up(TRUE);
}

/* Scroll down one line or chunk without scrolling the cursor. */
void do_scroll_down(void)
{
    do_down(TRUE);
}
#endif

/* Move left one character. */
void do_left(void)
{
    size_t was_column = xplustabs();
#ifndef NANO_TINY
    size_t was_chunk = (was_column / editwincols);
    filestruct *was_current = openfile->current;
#endif

    if (openfile->current_x > 0)
	openfile->current_x = move_mbleft(openfile->current->data,
						openfile->current_x);
    else if (openfile->current != openfile->fileage) {
	do_up_void();
	do_end(FALSE);
	return;
    }

    openfile->placewewant = xplustabs();

#ifndef NANO_TINY
    /* If we were on the first line of the edit window, and we changed chunk,
     * we're now above the first line of the edit window, so scroll up. */
    if (ISSET(SOFTWRAP) && openfile->current_y == 0 &&
		openfile->current == was_current &&
		openfile->placewewant / editwincols != was_chunk) {
	edit_scroll(UPWARD, ISSET(SMOOTH_SCROLL) ? 1 : editwinrows / 2 + 1);
	return;
    }
#endif

    /* Update current if the mark is on or it has changed "page". */
    if (line_needs_update(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}

/* Move right one character. */
void do_right(void)
{
    size_t was_column = xplustabs();
#ifndef NANO_TINY
    size_t was_chunk = (was_column / editwincols);
    filestruct *was_current = openfile->current;
#endif

    if (openfile->current->data[openfile->current_x] != '\0')
	openfile->current_x = move_mbright(openfile->current->data,
						openfile->current_x);
    else if (openfile->current != openfile->filebot) {
	do_home(FALSE);
	do_down_void();
	return;
    }

    openfile->placewewant = xplustabs();

#ifndef NANO_TINY
    /* If we were on the last line of the edit window, and we changed chunk,
     * we're now below the first line of the edit window, so scroll down. */
    if (ISSET(SOFTWRAP) && openfile->current_y == editwinrows - 1 &&
		openfile->current == was_current &&
		openfile->placewewant / editwincols != was_chunk) {
	edit_scroll(DOWNWARD, ISSET(SMOOTH_SCROLL) ? 1 : editwinrows / 2 + 1);
	return;
    }
#endif

    /* Update current if the mark is on or it has changed "page". */
    if (line_needs_update(was_column, openfile->placewewant))
	update_line(openfile->current, openfile->current_x);
}
