/**************************************************************************
 *   text.c                                                               *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 3, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *   General Public License for more details.                             *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA            *
 *   02110-1301, USA.                                                     *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#ifndef NANO_TINY
static pid_t pid = -1;
	/* The PID of the forked process in execute_command(), for use
	 * with the cancel_command() signal handler. */
#endif
#ifndef DISABLE_WRAPPING
static bool prepend_wrap = FALSE;
	/* Should we prepend wrapped text to the next line? */
#endif
#ifndef DISABLE_JUSTIFY
static filestruct *jusbottom = NULL;
	/* Pointer to the end of the justify buffer. */
#endif

#ifndef NANO_TINY
/* Toggle the mark. */
void do_mark(void)
{
    openfile->mark_set = !openfile->mark_set;
    if (openfile->mark_set) {
	statusbar(_("Mark Set"));
	openfile->mark_begin = openfile->current;
	openfile->mark_begin_x = openfile->current_x;
    } else {
	statusbar(_("Mark Unset"));
	openfile->mark_begin = NULL;
	openfile->mark_begin_x = 0;
	edit_refresh();
    }
}
#endif /* !NANO_TINY */

#if !defined(DISABLE_COLOR) || !defined(DISABLE_SPELLER)
/* Return an error message containing the given name. */
char *invocation_error(const char *name)
{
    char *message, *invoke_error = _("Error invoking \"%s\"");

    message = charalloc(strlen(invoke_error) + strlen(name) + 1);
    sprintf(message, invoke_error, name);
    return message;
}
#endif

/* Delete the character under the cursor. */
void do_deletion(undo_type action)
{
#ifndef NANO_TINY
    size_t orig_lenpt = 0;
#endif

    assert(openfile->current != NULL && openfile->current->data != NULL &&
		openfile->current_x <= strlen(openfile->current->data));

    openfile->placewewant = xplustabs();

    if (openfile->current->data[openfile->current_x] != '\0') {
	/* We're in the middle of a line: delete the current character. */
	int char_buf_len = parse_mbchar(openfile->current->data +
					openfile->current_x, NULL, NULL);
	size_t line_len = strlen(openfile->current->data +
					openfile->current_x);

	assert(openfile->current_x < strlen(openfile->current->data));

#ifndef NANO_TINY
	update_undo(action);

	if (ISSET(SOFTWRAP))
	    orig_lenpt = strlenpt(openfile->current->data);
#endif

	/* Move the remainder of the line "in", over the current character. */
	charmove(&openfile->current->data[openfile->current_x],
		&openfile->current->data[openfile->current_x + char_buf_len],
		line_len - char_buf_len + 1);
	null_at(&openfile->current->data, openfile->current_x +
		line_len - char_buf_len);

#ifndef NANO_TINY
	/* Adjust the mark if it is after the cursor on the current line. */
	if (openfile->mark_set && openfile->mark_begin == openfile->current &&
				openfile->mark_begin_x > openfile->current_x)
	    openfile->mark_begin_x -= char_buf_len;
#endif
	/* Adjust the file size. */
	openfile->totsize--;
    } else if (openfile->current != openfile->filebot) {
	/* We're at the end of a line and not at the end of the file: join
	 * this line with the next. */
	filestruct *joining = openfile->current->next;

	assert(openfile->current_x == strlen(openfile->current->data));

	/* If there is a magic line, and we're before it: don't eat it. */
	if (joining == openfile->filebot && openfile->current_x != 0 &&
		!ISSET(NO_NEWLINES)) {
#ifndef NANO_TINY
	    if (action == BACK)
		add_undo(BACK);
#endif
	    return;
	}

#ifndef NANO_TINY
	add_undo(action);
#endif
	/* Add the contents of the next line to those of the current one. */
	openfile->current->data = charealloc(openfile->current->data,
		strlen(openfile->current->data) + strlen(joining->data) + 1);
	strcat(openfile->current->data, joining->data);

	/* Adjust the file size. */
	openfile->totsize--;

#ifndef NANO_TINY
	/* Remember the new file size for a possible redo. */
	openfile->current_undo->newsize = openfile->totsize;

	/* Adjust the mark if it was on the line that was "eaten". */
	if (openfile->mark_set && openfile->mark_begin == joining) {
	    openfile->mark_begin = openfile->current;
	    openfile->mark_begin_x += openfile->current_x;
	}
#endif
	unlink_node(joining);
	renumber(openfile->current);

	/* Two lines were joined, so we need to refresh the screen. */
	refresh_needed = TRUE;
    } else
	/* We're at the end-of-file: nothing to do. */
	return;

#ifndef NANO_TINY
    if (ISSET(SOFTWRAP) && refresh_needed == FALSE)
	if (strlenpt(openfile->current->data) / COLS != orig_lenpt / COLS)
	    refresh_needed = TRUE;
#endif

    set_modified();
}

/* Delete the character under the cursor. */
void do_delete(void)
{
    do_deletion(DEL);
}

/* Backspace over one character.  That is, move the cursor left one
 * character, and then delete the character under the cursor. */
void do_backspace(void)
{
    if (openfile->current != openfile->fileage || openfile->current_x > 0) {
	do_left();
	do_deletion(BACK);
    }
}

#ifndef NANO_TINY
/* Delete text from the cursor until the first start of a word to
 * the right, or to the left when backward is true. */
void do_cutword(bool backward)
{
    /* Remember the current cursor position. */
    filestruct *is_current = openfile->current;
    size_t is_current_x = openfile->current_x;

    /* Remember where the cutbuffer is and then make it seem blank. */
    filestruct *is_cutbuffer = cutbuffer;
    filestruct *is_cutbottom = cutbottom;
    cutbuffer = NULL;
    cutbottom = NULL;

    /* Move the cursor to a word start, to the left or to the right. */
    if (backward)
	do_prev_word(ISSET(WORD_BOUNDS), FALSE);
    else
	do_next_word(ISSET(WORD_BOUNDS), FALSE);

    /* Set the mark at the start of that word. */
    openfile->mark_begin = openfile->current;
    openfile->mark_begin_x = openfile->current_x;
    openfile->mark_set = TRUE;

    /* Put the cursor back where it was, so an undo will put it there too. */
    openfile->current = is_current;
    openfile->current_x = is_current_x;

    /* Now kill the marked region and a word is gone. */
    do_cut_text_void();

    /* Discard the cut word and restore the cutbuffer. */
    free_filestruct(cutbuffer);
    cutbuffer = is_cutbuffer;
    cutbottom = is_cutbottom;
}

/* Delete a word leftward. */
void do_cut_prev_word(void)
{
    do_cutword(TRUE);
}

/* Delete a word rightward. */
void do_cut_next_word(void)
{
    do_cutword(FALSE);
}
#endif /* !NANO_TINY */

/* Insert a tab.  If the TABS_TO_SPACES flag is set, insert the number
 * of spaces that a tab would normally take up. */
void do_tab(void)
{
#ifndef NANO_TINY
    if (ISSET(TABS_TO_SPACES)) {
	char *spaces = charalloc(tabsize + 1);
	size_t length = tabsize - (xplustabs() % tabsize);

	charset(spaces, ' ', length);
	spaces[length] = '\0';

	do_output(spaces, length, TRUE);

	free(spaces);
    } else
#endif
	do_output((char *)"\t", 1, TRUE);
}

#ifndef NANO_TINY
/* Indent or unindent the current line (or, if the mark is on, all lines
 * covered by the mark) len columns, depending on whether len is
 * positive or negative.  If the TABS_TO_SPACES flag is set, indent or
 * unindent by len spaces.  Otherwise, indent or unindent by (len /
 * tabsize) tabs and (len % tabsize) spaces. */
void do_indent(ssize_t cols)
{
    bool indent_changed = FALSE;
	/* Whether any indenting or unindenting was done. */
    bool unindent = FALSE;
	/* Whether we're unindenting text. */
    char *line_indent = NULL;
	/* The text added to each line in order to indent it. */
    size_t line_indent_len = 0;
	/* The length of the text added to each line in order to indent
	 * it. */
    filestruct *top, *bot, *f;
    size_t top_x, bot_x;

    assert(openfile->current != NULL && openfile->current->data != NULL);

    /* If cols is zero, get out. */
    if (cols == 0)
	return;

    /* If cols is negative, make it positive and set unindent to TRUE. */
    if (cols < 0) {
	cols = -cols;
	unindent = TRUE;
    /* Otherwise, we're indenting, in which case the file will always be
     * modified, so set indent_changed to TRUE. */
    } else
	indent_changed = TRUE;

    /* If the mark is on, use all lines covered by the mark. */
    if (openfile->mark_set)
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, NULL);
    /* Otherwise, use the current line. */
    else {
	top = openfile->current;
	bot = top;
    }

    if (!unindent) {
	/* Set up the text we'll be using as indentation. */
	line_indent = charalloc(cols + 1);

	if (ISSET(TABS_TO_SPACES)) {
	    /* Set the indentation to cols spaces. */
	    charset(line_indent, ' ', cols);
	    line_indent_len = cols;
	} else {
	    /* Set the indentation to (cols / tabsize) tabs and (cols %
	     * tabsize) spaces. */
	    size_t num_tabs = cols / tabsize;
	    size_t num_spaces = cols % tabsize;

	    charset(line_indent, '\t', num_tabs);
	    charset(line_indent + num_tabs, ' ', num_spaces);

	    line_indent_len = num_tabs + num_spaces;
	}

	line_indent[line_indent_len] = '\0';
    }

    /* Go through each line of the text. */
    for (f = top; f != bot->next; f = f->next) {
	size_t line_len = strlen(f->data);
	size_t indent_len = indent_length(f->data);

	if (!unindent) {
	    /* If we're indenting, add the characters in line_indent to
	     * the beginning of the non-whitespace text of this line. */
	    f->data = charealloc(f->data, line_len + line_indent_len + 1);
	    charmove(&f->data[indent_len + line_indent_len],
		&f->data[indent_len], line_len - indent_len + 1);
	    strncpy(f->data + indent_len, line_indent, line_indent_len);
	    openfile->totsize += line_indent_len;

	    /* Keep track of the change in the current line. */
	    if (openfile->mark_set && f == openfile->mark_begin &&
			openfile->mark_begin_x >= indent_len)
		openfile->mark_begin_x += line_indent_len;

	    if (f == openfile->current && openfile->current_x >= indent_len)
		openfile->current_x += line_indent_len;

	    /* If the NO_NEWLINES flag isn't set, and this is the
	     * magicline, add a new magicline. */
	    if (!ISSET(NO_NEWLINES) && f == openfile->filebot)
		new_magicline();
	} else {
	    size_t indent_col = strnlenpt(f->data, indent_len);
		/* The length in columns of the indentation on this line. */

	    if (cols <= indent_col) {
		size_t indent_new = actual_x(f->data, indent_col - cols);
			/* The length of the indentation remaining on
			 * this line after we unindent. */
		size_t indent_shift = indent_len - indent_new;
			/* The change in the indentation on this line
			 * after we unindent. */

		/* If we're unindenting, and there's at least cols
		 * columns' worth of indentation at the beginning of the
		 * non-whitespace text of this line, remove it. */
		charmove(&f->data[indent_new], &f->data[indent_len],
			line_len - indent_shift - indent_new + 1);
		null_at(&f->data, line_len - indent_shift + 1);
		openfile->totsize -= indent_shift;

		/* Keep track of the change in the current line. */
		if (openfile->mark_set && f == openfile->mark_begin &&
			openfile->mark_begin_x > indent_new) {
		    if (openfile->mark_begin_x <= indent_len)
			openfile->mark_begin_x = indent_new;
		    else
			openfile->mark_begin_x -= indent_shift;
		}

		if (f == openfile->current &&
			openfile->current_x > indent_new) {
		    if (openfile->current_x <= indent_len)
			openfile->current_x = indent_new;
		    else
			openfile->current_x -= indent_shift;
		}

		/* We've unindented, so the indentation changed. */
		indent_changed = TRUE;
	    }
	}
    }

    if (!unindent)
	/* Clean up. */
	free(line_indent);

    if (indent_changed) {
	/* Throw away the undo stack, to prevent making mistakes when
	 * the user tries to undo something in the reindented text. */
	discard_until(NULL, openfile);
	openfile->current_undo = NULL;

	/* Mark the file as modified. */
	set_modified();

	/* Update the screen. */
	refresh_needed = TRUE;
    }
}

/* Indent the current line, or all lines covered by the mark if the mark
 * is on, tabsize columns. */
void do_indent_void(void)
{
    do_indent(tabsize);
}

/* Unindent the current line, or all lines covered by the mark if the
 * mark is on, tabsize columns. */
void do_unindent(void)
{
    do_indent(-tabsize);
}
#endif /* !NANO_TINY */

#ifdef ENABLE_COMMENT
/* Test whether the string is empty or consists of only blanks. */
bool white_string(const char *s)
{
    while (*s != '\0' && (is_blank_mbchar(s) || *s == '\r'))
	s += move_mbright(s, 0);

    return !*s;
}

/* Comment or uncomment the current line or the marked lines. */
void do_comment()
{
    const char *comment_seq = "#";
    undo_type action = UNCOMMENT;
    filestruct *top, *bot, *f;
    size_t top_x, bot_x;
    bool empty, all_empty = TRUE;

    assert(openfile->current != NULL && openfile->current->data != NULL);

#ifndef DISABLE_COLOR
    if (openfile->syntax && openfile->syntax->comment)
	comment_seq = openfile->syntax->comment;

    /* Does the syntax not allow comments? */
    if (strlen(comment_seq) == 0) {
	statusbar(_("Commenting is not supported for this file type"));
	return;
    }
#endif

    /* Determine which lines to work on. */
    if (openfile->mark_set)
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, NULL);
    else {
	top = openfile->current;
	bot = top;
    }

    /* If only the magic line is selected, don't do anything. */
    if (top == bot && bot == openfile->filebot && !ISSET(NO_NEWLINES)) {
	statusbar(_("Cannot comment past end of file"));
	return;
    }

    /* Figure out whether to comment or uncomment the selected line or lines. */
    for (f = top; f != bot->next; f = f->next) {
	empty = white_string(f->data);

	/* If this line is not blank and not commented, we comment all. */
	if (!empty && !comment_line(PREFLIGHT, f, comment_seq)) {
	    action = COMMENT;
	    break;
	}
	all_empty = all_empty && empty;
    }

    /* If all selected lines are blank, we comment them. */
    action = all_empty ? COMMENT : action;

    add_undo(action);

    /* Store the comment sequence used for the operation, because it could
     * change when the file name changes; we need to know what it was. */
    openfile->current_undo->strdata = mallocstrcpy(NULL, comment_seq);

    /* Process the selected line or lines. */
    for (f = top; f != bot->next; f = f->next) {
	/* Comment/uncomment a line, and add undo data when line changed. */
	if (comment_line(action, f, comment_seq))
	    update_comment_undo(f->lineno);
    }

    set_modified();
    refresh_needed = TRUE;
}

/* Test whether the given line can be uncommented, or add or remove a comment,
 * depending on action.  Return TRUE if the line is uncommentable, or when
 * anything was added or removed; FALSE otherwise. */
bool comment_line(undo_type action, filestruct *f, const char *comment_seq)
{
    size_t comment_seq_len = strlen(comment_seq);
    const char *post_seq = strchr(comment_seq, '|');
	/* The postfix, if this is a bracketing type comment sequence. */
    size_t pre_len = post_seq ? post_seq++ - comment_seq : comment_seq_len;
	/* Length of prefix. */
    size_t post_len = post_seq ? comment_seq_len - pre_len - 1 : 0;
	/* Length of postfix. */
    size_t line_len = strlen(f->data);

    if (!ISSET(NO_NEWLINES) && f == openfile->filebot)
	return FALSE;

    if (action == COMMENT) {
	/* Make room for the comment sequence(s), move the text right and
	 * copy them in. */
	f->data = charealloc(f->data, line_len + pre_len + post_len + 1);
	charmove(&f->data[pre_len], f->data, line_len);
	charmove(f->data, comment_seq, pre_len);
	if (post_len)
	    charmove(&f->data[pre_len + line_len], post_seq, post_len);
	f->data[pre_len + line_len + post_len] = '\0';

	openfile->totsize += pre_len + post_len;

	/* If needed, adjust the position of the mark and of the cursor. */
	if (openfile->mark_set && f == openfile->mark_begin)
	    openfile->mark_begin_x += pre_len;
	if (f == openfile->current) {
	    openfile->current_x += pre_len;
	    openfile->placewewant = xplustabs();
	}

	return TRUE;
    }

    /* If the line is commented, report it as uncommentable, or uncomment it. */
    if (strncmp(f->data, comment_seq, pre_len) == 0 && (post_len == 0 ||
		strcmp(&f->data[line_len - post_len], post_seq) == 0)) {

	if (action == PREFLIGHT)
	    return TRUE;

	/* Erase the comment prefix by moving the non-comment part. */
	charmove(f->data, &f->data[pre_len], line_len - pre_len);
	/* Truncate the postfix if there was one. */
	f->data[line_len - pre_len - post_len] = '\0';

	openfile->totsize -= pre_len + post_len;

	/* If needed, adjust the position of the mark and then the cursor. */
	if (openfile->mark_set && f == openfile->mark_begin) {
	    if (openfile->mark_begin_x < pre_len)
		openfile->mark_begin_x = 0;
	    else
		openfile->mark_begin_x -= pre_len;
	}
	if (f == openfile->current) {
	    if (openfile->current_x < pre_len)
		openfile->current_x = 0;
	    else
		openfile->current_x -= pre_len;
	    openfile->placewewant = xplustabs();
	}

	return TRUE;
    }

    return FALSE;
}

/* Perform an undo or redo for a comment or uncomment action. */
void handle_comment_action(undo *u, bool undoing, bool add_comment)
{
    undo_group *group = u->grouping;

    /* When redoing, reposition the cursor and let the commenter adjust it. */
    if (!undoing)
	goto_line_posx(u->lineno, u->begin);

    while (group) {
	filestruct *f = fsfromline(group->top_line);

	while (f && f->lineno <= group->bottom_line) {
	    comment_line(undoing ^ add_comment ?
				COMMENT : UNCOMMENT, f, u->strdata);
	    f = f->next;
	}
	group = group->next;
    }

    /* When undoing, reposition the cursor to the recorded location. */
    if (undoing)
	goto_line_posx(u->lineno, u->begin);

    refresh_needed = TRUE;
}
#endif /* ENABLE_COMMENT */

#ifndef NANO_TINY
#define redo_paste undo_cut
#define undo_paste redo_cut

/* Undo a cut, or redo an uncut. */
void undo_cut(undo *u)
{
    /* If we cut the magicline, we may as well not crash. :/ */
    if (!u->cutbuffer)
	return;

    /* Get to where we need to uncut from. */
    if (u->xflags == WAS_WHOLE_LINE)
	goto_line_posx(u->mark_begin_lineno, 0);
    else
	goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);

    copy_from_filestruct(u->cutbuffer);

    if (u->xflags != WAS_MARKED_FORWARD && u->type != PASTE)
	goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
}

/* Redo a cut, or undo an uncut. */
void redo_cut(undo *u)
{
    /* If we cut the magicline, we may as well not crash. :/ */
    if (!u->cutbuffer)
	return;

    filestruct *oldcutbuffer = cutbuffer, *oldcutbottom = cutbottom;
    cutbuffer = cutbottom = NULL;

    goto_line_posx(u->lineno, u->begin);

    openfile->mark_set = TRUE;
    openfile->mark_begin = fsfromline(u->mark_begin_lineno);
    openfile->mark_begin_x = (u->xflags == WAS_WHOLE_LINE) ? 0 : u->mark_begin_x;

    do_cut_text(FALSE, FALSE);

    free_filestruct(cutbuffer);
    cutbuffer = oldcutbuffer;
    cutbottom = oldcutbottom;
}

/* Undo the last thing(s) we did. */
void do_undo(void)
{
    undo *u = openfile->current_undo;
    filestruct *f, *t = NULL;
    char *data, *undidmsg = NULL;

    if (!u) {
	statusbar(_("Nothing in undo buffer!"));
	return;
    }

    f = fsfromline(u->mark_begin_lineno);
    if (!f)
	return;

#ifdef DEBUG
    fprintf(stderr, "  >> Undoing a type %d...\n", u->type);
    fprintf(stderr, "  >> Data we're about to undo = \"%s\"\n", f->data);
#endif

    openfile->current_x = u->begin;
    switch (u->type) {
    case ADD:
	/* TRANSLATORS: Eight of the next nine strings describe actions
	 * that are undone or redone.  It are all nouns, not verbs. */
	undidmsg = _("text add");
	data = charalloc(strlen(f->data) - strlen(u->strdata) + 1);
	strncpy(data, f->data, u->begin);
	strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
	free(f->data);
	f->data = data;
	goto_line_posx(u->lineno, u->begin);
	break;
    case BACK:
    case DEL:
	undidmsg = _("text delete");
	data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
	strncpy(data, f->data, u->begin);
	strcpy(&data[u->begin], u->strdata);
	strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
	free(f->data);
	f->data = data;
	goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	break;
#ifndef DISABLE_WRAPPING
    case SPLIT_END:
	goto_line_posx(u->lineno, u->begin);
	openfile->current_undo = openfile->current_undo->next;
	openfile->last_action = OTHER;
	while (openfile->current_undo->type != SPLIT_BEGIN)
	    do_undo();
	u = openfile->current_undo;
	f = openfile->current;
    case SPLIT_BEGIN:
	undidmsg = _("text add");
	break;
#endif
    case JOIN:
	undidmsg = _("line join");
	/* When the join was done by a Backspace at the tail of the file,
	 * and the nonewlines flag isn't set, do not re-add a newline that
	 * wasn't actually deleted; just position the cursor. */
	if (u->xflags == WAS_FINAL_BACKSPACE && !ISSET(NO_NEWLINES)) {
	    goto_line_posx(openfile->filebot->lineno, 0);
	    break;
	}
	t = make_new_node(f);
	t->data = mallocstrcpy(NULL, u->strdata);
	data = mallocstrncpy(NULL, f->data, u->mark_begin_x + 1);
	data[u->mark_begin_x] = '\0';
	free(f->data);
	f->data = data;
	splice_node(f, t);
	goto_line_posx(u->lineno, u->begin);
	break;
    case CUT_EOF:
    case CUT:
	undidmsg = _("text cut");
	undo_cut(u);
	f = fsfromline(u->lineno);
	break;
    case PASTE:
	undidmsg = _("text uncut");
	undo_paste(u);
	f = fsfromline(u->mark_begin_lineno);
	break;
    case ENTER:
	if (f->next == NULL) {
	    statusline(ALERT, _("Internal error: line is missing.  "
				"Please save your work."));
	    break;
	}
	undidmsg = _("line break");
	f->data = charealloc(f->data, strlen(f->data) +
				strlen(&f->next->data[u->mark_begin_x]) + 1);
	strcat(f->data, &f->next->data[u->mark_begin_x]);
	unlink_node(f->next);
	goto_line_posx(u->lineno, u->begin);
	break;
#ifdef ENABLE_COMMENT
    case COMMENT:
	handle_comment_action(u, TRUE, TRUE);
	undidmsg = _("comment");
	break;
    case UNCOMMENT:
	handle_comment_action(u, TRUE, FALSE);
	undidmsg = _("uncomment");
	break;
#endif
    case INSERT:
	undidmsg = _("text insert");
	filestruct *oldcutbuffer = cutbuffer, *oldcutbottom = cutbottom;
	cutbuffer = NULL;
	cutbottom = NULL;
	/* Instead of a line number, u->mark_begin_lineno contains the number
	 * of lines of the inserted segment, because the file was partitioned
	 * when update_undo() was called; so, calculate the end-line number. */
	openfile->mark_begin = fsfromline(u->lineno + u->mark_begin_lineno - 1);
	openfile->mark_begin_x = u->mark_begin_x;
	openfile->mark_set = TRUE;
	goto_line_posx(u->lineno, u->begin);
	cut_marked();
	free_filestruct(u->cutbuffer);
	u->cutbuffer = cutbuffer;
	u->cutbottom = cutbottom;
	cutbuffer = oldcutbuffer;
	cutbottom = oldcutbottom;
	openfile->mark_set = FALSE;
	break;
    case REPLACE:
	undidmsg = _("text replace");
	goto_line_posx(u->lineno, u->begin);
	data = u->strdata;
	u->strdata = f->data;
	f->data = data;
	break;
    default:
	statusline(ALERT, _("Internal error: unknown type.  "
				"Please save your work."));
	break;
    }

    if (undidmsg)
	statusline(HUSH, _("Undid action (%s)"), undidmsg);

    renumber(f);
    openfile->current_undo = openfile->current_undo->next;
    openfile->last_action = OTHER;
    openfile->mark_set = FALSE;
    openfile->placewewant = xplustabs();
    openfile->totsize = u->wassize;
    set_modified();
}

/* Redo the last thing(s) we undid. */
void do_redo(void)
{
    filestruct *f;
    char *data, *redidmsg = NULL;
    undo *u = openfile->undotop;

    if (u == NULL || u == openfile->current_undo) {
	statusbar(_("Nothing to re-do!"));
	return;
    }

    /* Get the previous undo item. */
    while (u != NULL && u->next != openfile->current_undo)
	u = u->next;

    if (u->next != openfile->current_undo) {
	statusline(ALERT, _("Internal error: cannot set up redo.  "
				"Please save your work."));
	return;
    }

    f = fsfromline(u->type == INSERT ? 1 : u->mark_begin_lineno);
    if (!f)
	return;

#ifdef DEBUG
    fprintf(stderr, "  >> Redo running for type %d\n", u->type);
    fprintf(stderr, "  >> Data we're about to redo = \"%s\"\n", f->data);
#endif

    switch (u->type) {
    case ADD:
	redidmsg = _("text add");
	data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
	strncpy(data, f->data, u->begin);
	strcpy(&data[u->begin], u->strdata);
	strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
	free(f->data);
	f->data = data;
	goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	break;
    case BACK:
    case DEL:
	redidmsg = _("text delete");
	data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
	strncpy(data, f->data, u->begin);
	strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
	free(f->data);
	f->data = data;
	goto_line_posx(u->lineno, u->begin);
	break;
    case ENTER:
	redidmsg = _("line break");
	filestruct *shoveline = make_new_node(f);
	shoveline->data = mallocstrcpy(NULL, u->strdata);
	data = mallocstrncpy(NULL, f->data, u->begin + 1);
	data[u->begin] = '\0';
	free(f->data);
	f->data = data;
	splice_node(f, shoveline);
	renumber(shoveline);
	goto_line_posx(u->lineno + 1, u->mark_begin_x);
	break;
#ifndef DISABLE_WRAPPING
    case SPLIT_BEGIN:
	goto_line_posx(u->lineno, u->begin);
	openfile->current_undo = u;
	openfile->last_action = OTHER;
	while (openfile->current_undo->type != SPLIT_END)
	    do_redo();
	u = openfile->current_undo;
	goto_line_posx(u->lineno, u->begin);
    case SPLIT_END:
	redidmsg = _("text add");
	break;
#endif
    case JOIN:
	if (f->next == NULL) {
	    statusline(ALERT, _("Internal error: line is missing.  "
				"Please save your work."));
	    break;
	}
	redidmsg = _("line join");
	/* When the join was done by a Backspace at the tail of the file,
	 * and the nonewlines flag isn't set, do not join anything, as
	 * nothing was actually deleted; just position the cursor. */
	if (u->xflags == WAS_FINAL_BACKSPACE && !ISSET(NO_NEWLINES)) {
	    goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	    break;
	}
	f->data = charealloc(f->data, strlen(f->data) + strlen(u->strdata) + 1);
	strcat(f->data, u->strdata);
	unlink_node(f->next);
	renumber(f);
	goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
	break;
    case CUT_EOF:
    case CUT:
	redidmsg = _("text cut");
	redo_cut(u);
	break;
    case PASTE:
	redidmsg = _("text uncut");
	redo_paste(u);
	break;
    case REPLACE:
	redidmsg = _("text replace");
	data = u->strdata;
	u->strdata = f->data;
	f->data = data;
	goto_line_posx(u->lineno, u->begin);
	break;
    case INSERT:
	redidmsg = _("text insert");
	goto_line_posx(u->lineno, u->begin);
	copy_from_filestruct(u->cutbuffer);
	free_filestruct(u->cutbuffer);
	u->cutbuffer = NULL;
	break;
#ifdef ENABLE_COMMENT
    case COMMENT:
	handle_comment_action(u, FALSE, TRUE);
	redidmsg = _("comment");
	break;
    case UNCOMMENT:
	handle_comment_action(u, FALSE, FALSE);
	redidmsg = _("uncomment");
	break;
#endif
    default:
	statusline(ALERT, _("Internal error: unknown type.  "
				"Please save your work."));
	break;
    }

    if (redidmsg)
	statusline(HUSH, _("Redid action (%s)"), redidmsg);

    openfile->current_undo = u;
    openfile->last_action = OTHER;
    openfile->mark_set = FALSE;
    openfile->placewewant = xplustabs();
    openfile->totsize = u->newsize;
    set_modified();
}
#endif /* !NANO_TINY */

/* Someone hits Enter *gasp!* */
void do_enter()
{
    filestruct *newnode = make_new_node(openfile->current);
    size_t extra = 0;

    assert(openfile->current != NULL && openfile->current->data != NULL);

#ifndef NANO_TINY
    add_undo(ENTER);

    /* Do auto-indenting, like the neolithic Turbo Pascal editor. */
    if (ISSET(AUTOINDENT)) {
	/* If we are breaking the line in the indentation, the new
	 * indentation should have only current_x characters, and
	 * current_x should not change. */
	extra = indent_length(openfile->current->data);
	if (extra > openfile->current_x)
	    extra = openfile->current_x;
    }
#endif
    newnode->data = charalloc(strlen(openfile->current->data +
	openfile->current_x) + extra + 1);
    strcpy(&newnode->data[extra], openfile->current->data +
	openfile->current_x);
#ifndef NANO_TINY
    if (ISSET(AUTOINDENT)) {
	strncpy(newnode->data, openfile->current->data, extra);
	openfile->totsize += extra;
    }
#endif
    null_at(&openfile->current->data, openfile->current_x);
#ifndef NANO_TINY
    if (openfile->mark_set && openfile->current == openfile->mark_begin &&
		openfile->current_x < openfile->mark_begin_x) {
	openfile->mark_begin = newnode;
	openfile->mark_begin_x += extra - openfile->current_x;
    }
#endif
    openfile->current_x = extra;

    splice_node(openfile->current, newnode);
    openfile->current = newnode;
    renumber(newnode);

    openfile->totsize++;
    set_modified();

    openfile->placewewant = xplustabs();

#ifndef NANO_TINY
    update_undo(ENTER);
#endif

    refresh_needed = TRUE;
}

#ifndef NANO_TINY
/* Send a SIGKILL (unconditional kill) to the forked process in
 * execute_command(). */
RETSIGTYPE cancel_command(int signal)
{
    if (kill(pid, SIGKILL) == -1)
	nperror("kill");
}

/* Execute command in a shell.  Return TRUE on success. */
bool execute_command(const char *command)
{
    int fd[2];
    FILE *f;
    char *shellenv;
    struct sigaction oldaction, newaction;
	/* Original and temporary handlers for SIGINT. */
    bool sig_failed = FALSE;
	/* Did sigaction() fail without changing the signal handlers? */

    /* Make our pipes. */
    if (pipe(fd) == -1) {
	statusbar(_("Could not create pipe"));
	return FALSE;
    }

    /* Check $SHELL for the shell to use.  If it isn't set, use /bin/sh.
     * Note that $SHELL should contain only a path, with no arguments. */
    shellenv = getenv("SHELL");
    if (shellenv == NULL)
	shellenv = (char *) "/bin/sh";

    /* Fork a child. */
    if ((pid = fork()) == 0) {
	close(fd[0]);
	dup2(fd[1], fileno(stdout));
	dup2(fd[1], fileno(stderr));

	/* If execl() returns at all, there was an error. */
	execl(shellenv, tail(shellenv), "-c", command, NULL);
	exit(0);
    }

    /* Continue as parent. */
    close(fd[1]);

    if (pid == -1) {
	close(fd[0]);
	statusbar(_("Could not fork"));
	return FALSE;
    }

    /* Before we start reading the forked command's output, we set
     * things up so that Ctrl-C will cancel the new process. */

    /* Enable interpretation of the special control keys so that we get
     * SIGINT when Ctrl-C is pressed. */
    enable_signals();

    if (sigaction(SIGINT, NULL, &newaction) == -1) {
	sig_failed = TRUE;
	nperror("sigaction");
    } else {
	newaction.sa_handler = cancel_command;
	if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
	    sig_failed = TRUE;
	    nperror("sigaction");
	}
    }

    /* Note that now oldaction is the previous SIGINT signal handler,
     * to be restored later. */

    f = fdopen(fd[0], "rb");
    if (f == NULL)
	nperror("fdopen");

    read_file(f, 0, "stdin", TRUE, FALSE);

    if (wait(NULL) == -1)
	nperror("wait");

    if (!sig_failed && sigaction(SIGINT, &oldaction, NULL) == -1)
	nperror("sigaction");

    /* Restore the terminal to its previous state.  In the process,
     * disable interpretation of the special control keys so that we can
     * use Ctrl-C for other things. */
    terminal_init();

    return TRUE;
}

/* Discard undo items that are newer than the given one, or all if NULL. */
void discard_until(const undo *thisitem, openfilestruct *thefile)
{
    undo *dropit = thefile->undotop;
    undo_group *group;

    while (dropit != NULL && dropit != thisitem) {
	thefile->undotop = dropit->next;
	free(dropit->strdata);
	free_filestruct(dropit->cutbuffer);
#ifdef ENABLE_COMMENT
	group = dropit->grouping;
	while (group != NULL) {
	    undo_group *next = group->next;
	    free(group);
	    group = next;
	}
#endif
	free(dropit);
	dropit = thefile->undotop;
    }

    /* Prevent a chain of editing actions from continuing. */
    thefile->last_action = OTHER;
}

/* Add a new undo struct to the top of the current pile. */
void add_undo(undo_type action)
{
    undo *u = openfile->current_undo;
	/* The thing we did previously. */

    /* When doing contiguous adds or contiguous cuts -- which means: with
     * no cursor movement in between -- don't add a new undo item. */
    if (u && u->mark_begin_lineno == openfile->current->lineno && action == openfile->last_action &&
	((action == ADD && u->type == ADD && u->mark_begin_x == openfile->current_x) ||
	(action == CUT && u->type == CUT && u->xflags < MARK_WAS_SET && keeping_cutbuffer())))
	return;

    /* Blow away newer undo items if we add somewhere in the middle. */
    discard_until(u, openfile);

#ifdef DEBUG
    fprintf(stderr, "  >> Adding an undo...\n");
#endif

    /* Allocate and initialize a new undo type. */
    u = (undo *) nmalloc(sizeof(undo));
    u->type = action;
#ifndef DISABLE_WRAPPING
    if (u->type == SPLIT_BEGIN) {
	/* Some action, most likely an ADD, was performed that invoked
	 * do_wrap().  Rearrange the undo order so that this previous
	 * action is after the SPLIT_BEGIN undo. */
	u->next = openfile->undotop->next;
	openfile->undotop->next = u;
    } else
#endif
    {
	u->next = openfile->undotop;
	openfile->undotop = u;
	openfile->current_undo = u;
    }
    u->strdata = NULL;
    u->cutbuffer = NULL;
    u->cutbottom = NULL;
    u->lineno = openfile->current->lineno;
    u->begin = openfile->current_x;
    u->mark_begin_lineno = openfile->current->lineno;
    u->mark_begin_x = openfile->current_x;
    u->wassize = openfile->totsize;
    u->xflags = 0;
    u->grouping = NULL;

    switch (u->type) {
    /* We need to start copying data into the undo buffer
     * or we won't be able to restore it later. */
    case ADD:
	u->wassize--;
	break;
    case BACK:
	/* If the next line is the magic line, don't ever undo this
	 * backspace, as it won't actually have deleted anything. */
	if (openfile->current->next == openfile->filebot && openfile->current->data[0] != '\0')
	    u->xflags = WAS_FINAL_BACKSPACE;
    case DEL:
	if (u->begin != strlen(openfile->current->data)) {
	    char *char_buf = charalloc(mb_cur_max() + 1);
	    int char_buf_len = parse_mbchar(&openfile->current->data[u->begin], char_buf, NULL);
	    null_at(&char_buf, char_buf_len);
	    u->strdata = char_buf;
	    if (u->type == BACK)
		u->mark_begin_x += char_buf_len;
	    break;
	}
	/* Else purposely fall into the line-joining code. */
    case JOIN:
	if (openfile->current->next) {
	    if (u->type == BACK) {
		u->lineno = openfile->current->next->lineno;
		u->begin = 0;
	    }
	    u->strdata = mallocstrcpy(NULL, openfile->current->next->data);
	}
	action = u->type = JOIN;
	break;
#ifndef DISABLE_WRAPPING
    case SPLIT_BEGIN:
	action = openfile->undotop->type;
	break;
    case SPLIT_END:
	break;
#endif
    case INSERT:
	break;
    case REPLACE:
	u->strdata = mallocstrcpy(NULL, openfile->current->data);
	break;
    case CUT_EOF:
	cutbuffer_reset();
	break;
    case CUT:
	cutbuffer_reset();
	if (openfile->mark_set) {
	    u->mark_begin_lineno = openfile->mark_begin->lineno;
	    u->mark_begin_x = openfile->mark_begin_x;
	    u->xflags = MARK_WAS_SET;
	} else if (!ISSET(CUT_TO_END)) {
	    /* The entire line is being cut regardless of the cursor position. */
	    u->begin = 0;
	    u->xflags = WAS_WHOLE_LINE;
	}
	break;
    case PASTE:
	u->cutbuffer = copy_filestruct(cutbuffer);
	u->lineno += cutbottom->lineno - cutbuffer->lineno;
	break;
    case ENTER:
	break;
#ifdef ENABLE_COMMENT
    case COMMENT:
    case UNCOMMENT:
	break;
#endif
    default:
	statusline(ALERT, _("Internal error: unknown type.  "
				"Please save your work."));
	break;
    }

#ifdef DEBUG
    fprintf(stderr, "  >> openfile->current->data = \"%s\", current_x = %lu, u->begin = %lu, type = %d\n",
		openfile->current->data, (unsigned long)openfile->current_x, (unsigned long)u->begin, action);
#endif
    openfile->last_action = action;
}

#ifdef ENABLE_COMMENT
/* Update a comment undo item.  This should be called once for each line
 * affected by the comment/uncomment feature. */
void update_comment_undo(ssize_t lineno)
{
    undo *u = openfile->current_undo;

    /* If there already is a group and the current line is contiguous with it,
     * extend the group; otherwise, create a new group. */
    if (u->grouping && u->grouping->bottom_line + 1 == lineno)
	u->grouping->bottom_line++;
    else {
	undo_group *born = (undo_group *)nmalloc(sizeof(undo_group));

	born->next = u->grouping;
	u->grouping = born;
	born->top_line = lineno;
	born->bottom_line = lineno;
    }

    /* Store the file size after the change, to be used when redoing. */
    u->newsize = openfile->totsize;
}
#endif /* ENABLE_COMMENT */

/* Update an undo item, or determine whether a new one is really needed
 * and bounce the data to add_undo instead.  The latter functionality
 * just feels gimmicky and may just be more hassle than it's worth,
 * so it should be axed if needed. */
void update_undo(undo_type action)
{
    undo *u;

#ifdef DEBUG
fprintf(stderr, "  >> Updating... action = %d, openfile->last_action = %d, openfile->current->lineno = %ld",
		action, openfile->last_action, (long)openfile->current->lineno);
	if (openfile->current_undo)
	    fprintf(stderr, ", openfile->current_undo->lineno = %ld\n", (long)openfile->current_undo->lineno);
	else
	    fprintf(stderr, "\n");
#endif

    /* Change to an add if we're not using the same undo struct
     * that we should be using. */
    if (action != openfile->last_action ||
		(action != ENTER && action != CUT && action != INSERT &&
		openfile->current->lineno != openfile->current_undo->lineno)) {
	add_undo(action);
	return;
    }

    assert(openfile->undotop != NULL);
    u = openfile->undotop;

    u->newsize = openfile->totsize;

    switch (u->type) {
    case ADD: {
#ifdef DEBUG
	fprintf(stderr, "  >> openfile->current->data = \"%s\", current_x = %lu, u->begin = %lu\n",
			openfile->current->data, (unsigned long)openfile->current_x, (unsigned long)u->begin);
#endif
	char *char_buf = charalloc(mb_cur_max());
	size_t char_buf_len = parse_mbchar(&openfile->current->data[u->mark_begin_x], char_buf, NULL);
	u->strdata = addstrings(u->strdata, u->strdata ? strlen(u->strdata) : 0, char_buf, char_buf_len);
#ifdef DEBUG
	fprintf(stderr, "  >> current undo data is \"%s\"\n", u->strdata);
#endif
	u->mark_begin_lineno = openfile->current->lineno;
	u->mark_begin_x = openfile->current_x;
	break;
    }
    case BACK:
    case DEL: {
	char *char_buf = charalloc(mb_cur_max());
	size_t char_buf_len = parse_mbchar(&openfile->current->data[openfile->current_x], char_buf, NULL);
	if (openfile->current_x == u->begin) {
	    /* They're deleting. */
	    u->strdata = addstrings(u->strdata, strlen(u->strdata), char_buf, char_buf_len);
	    u->mark_begin_x = openfile->current_x;
	} else if (openfile->current_x == u->begin - char_buf_len) {
	    /* They're backspacing. */
	    u->strdata = addstrings(char_buf, char_buf_len, u->strdata, strlen(u->strdata));
	    u->begin = openfile->current_x;
	} else {
	    /* They deleted something else on the line. */
	    free(char_buf);
	    add_undo(u->type);
	    return;
	}
#ifdef DEBUG
	fprintf(stderr, "  >> current undo data is \"%s\"\nu->begin = %lu\n", u->strdata, (unsigned long)u->begin);
#endif
	break;
    }
    case CUT_EOF:
    case CUT:
	if (!cutbuffer)
	    break;
	free_filestruct(u->cutbuffer);
	u->cutbuffer = copy_filestruct(cutbuffer);
	if (u->xflags == MARK_WAS_SET) {
	    /* If the "marking" operation was from right-->left or
	     * bottom-->top, then swap the mark points. */
	    if ((u->lineno == u->mark_begin_lineno && u->begin < u->mark_begin_x)
			|| u->lineno < u->mark_begin_lineno) {
		size_t x_loc = u->begin;
		u->begin = u->mark_begin_x;
		u->mark_begin_x = x_loc;

		ssize_t line = u->lineno;
		u->lineno = u->mark_begin_lineno;
		u->mark_begin_lineno = line;
	    } else
		u->xflags = WAS_MARKED_FORWARD;
	} else {
	    /* Compute the end of the cut for the undo, using our copy. */
	    u->cutbottom = u->cutbuffer;
	    while (u->cutbottom->next != NULL)
		u->cutbottom = u->cutbottom->next;
	    u->lineno = u->mark_begin_lineno + u->cutbottom->lineno -
					u->cutbuffer->lineno;
	    if (ISSET(CUT_TO_END) || u->type == CUT_EOF) {
		u->begin = strlen(u->cutbottom->data);
		if (u->lineno == u->mark_begin_lineno)
		    u->begin += u->mark_begin_x;
	    } else if (openfile->current == openfile->filebot &&
			ISSET(NO_NEWLINES))
		u->begin = strlen(u->cutbottom->data);
	}
	break;
    case REPLACE:
    case PASTE:
	u->begin = openfile->current_x;
	u->lineno = openfile->current->lineno;
	break;
    case INSERT:
	/* Store the number of lines (plus one) of the insertion. */
	u->mark_begin_lineno = openfile->current->lineno;
	/* When the insertion contains no newline, store the adjusted
	 * x position; otherwise, store the length of the last line. */
	if (openfile->fileage == openfile->filebot)
	    u->mark_begin_x = openfile->current_x;
	else
	    u->mark_begin_x = strlen(openfile->filebot->data);
	break;
    case ENTER:
	u->strdata = mallocstrcpy(NULL, openfile->current->data);
	u->mark_begin_x = openfile->current_x;
	break;
#ifndef DISABLE_WRAPPING
    case SPLIT_BEGIN:
    case SPLIT_END:
#endif
    case JOIN:
	/* These cases are handled by the earlier check for a new line and action. */
	break;
    default:
	statusline(ALERT, _("Internal error: unknown type.  "
				"Please save your work."));
	break;
    }

#ifdef DEBUG
    fprintf(stderr, "  >> Done in update_undo (type was %d)\n", action);
#endif
}
#endif /* !NANO_TINY */

#ifndef DISABLE_WRAPPING
/* Unset the prepend_wrap flag.  We need to do this as soon as we do
 * something other than type text. */
void wrap_reset(void)
{
    prepend_wrap = FALSE;
}

/* Try wrapping the given line.  Return TRUE if wrapped, FALSE otherwise. */
bool do_wrap(filestruct *line)
{
    size_t line_len;
	/* The length of the line we wrap. */
    ssize_t wrap_loc;
	/* The index of line->data where we wrap. */
    const char *after_break;
	/* The text after the wrap point. */
    size_t after_break_len;
	/* The length of after_break. */
    const char *next_line = NULL;
	/* The next line, minus indentation. */
    size_t next_line_len = 0;
	/* The length of next_line. */

    /* There are three steps.  First, we decide where to wrap.  Then, we
     * create the new wrap line.  Finally, we clean up. */

    /* Step 1, finding where to wrap.  We are going to add a new line
     * after a blank character.  In this step, we call break_line() to
     * get the location of the last blank we can break the line at, and
     * set wrap_loc to the location of the character after it, so that
     * the blank is preserved at the end of the line.
     *
     * If there is no legal wrap point, or we reach the last character
     * of the line while trying to find one, we should return without
     * wrapping.  Note that if autoindent is turned on, we don't break
     * at the end of it! */
    assert(line != NULL && line->data != NULL);

    /* Save the length of the line. */
    line_len = strlen(line->data);

    /* Find the last blank where we can break the line. */
    wrap_loc = break_line(line->data, fill
#ifndef DISABLE_HELP
	, FALSE
#endif
	);

    /* If we couldn't break the line, or we've reached the end of it, we
     * don't wrap. */
    if (wrap_loc == -1 || line->data[wrap_loc] == '\0')
	return FALSE;

    /* Otherwise, move forward to the character just after the blank. */
    wrap_loc += move_mbright(line->data + wrap_loc, 0);

    /* If we've reached the end of the line, we don't wrap. */
    if (line->data[wrap_loc] == '\0')
	return FALSE;

#ifndef NANO_TINY
    /* If autoindent is turned on, and we're on the character just after
     * the indentation, we don't wrap. */
    if (ISSET(AUTOINDENT) && wrap_loc == indent_length(line->data))
	return FALSE;

    add_undo(SPLIT_BEGIN);
#endif

    size_t old_x = openfile->current_x;
    filestruct * oldLine = openfile->current;
    openfile->current = line;

    /* Step 2, making the new wrap line.  It will consist of indentation
     * followed by the text after the wrap point, optionally followed by
     * a space (if the text after the wrap point doesn't end in a blank)
     * and the text of the next line, if they can fit without wrapping,
     * the next line exists, and the prepend_wrap flag is set. */

    /* after_break is the text that will be wrapped to the next line. */
    after_break = line->data + wrap_loc;
    after_break_len = line_len - wrap_loc;

    assert(strlen(after_break) == after_break_len);

    /* We prepend the wrapped text to the next line, if the prepend_wrap
     * flag is set, there is a next line, and prepending would not make
     * the line too long. */
    if (prepend_wrap && line != openfile->filebot) {
	const char *end = after_break + move_mbleft(after_break,
		after_break_len);

	/* Go to the end of the line. */
	openfile->current_x = line_len;

	/* If after_break doesn't end in a blank, make sure it ends in a
	 * space. */
	if (!is_blank_mbchar(end) && !ISSET(JUSTIFY_TRIM)) {
#ifndef NANO_TINY
	    add_undo(ADD);
#endif
	    line_len++;
	    line->data = charealloc(line->data, line_len + 1);
	    line->data[line_len - 1] = ' ';
	    line->data[line_len] = '\0';
	    after_break = line->data + wrap_loc;
	    after_break_len++;
	    openfile->totsize++;
	    openfile->current_x++;
#ifndef NANO_TINY
	    update_undo(ADD);
#endif
	}

	next_line = line->next->data;
	next_line_len = strlen(next_line);

	if (after_break_len + next_line_len <= fill) {
	    /* Delete the LF to join the two lines. */
	    do_delete();
	    /* Delete any leading blanks from the joined-on line. */
	    while (is_blank_mbchar(&line->data[openfile->current_x]))
		do_delete();
	    renumber(line);
	}
    }

    /* Go to the wrap location and split the line there. */
    openfile->current_x = wrap_loc;
    do_enter();

    if (old_x < wrap_loc) {
	openfile->current_x = old_x;
	openfile->current = oldLine;
	prepend_wrap = TRUE;
    } else {
	openfile->current_x += (old_x - wrap_loc);
	prepend_wrap = FALSE;
    }

    openfile->placewewant = xplustabs();

#ifndef NANO_TINY
    add_undo(SPLIT_END);
#endif

    return TRUE;
}
#endif /* !DISABLE_WRAPPING */

#if !defined(DISABLE_HELP) || !defined(DISABLE_WRAPJUSTIFY)
/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most (goal + 1).  If there is
 * no such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if newln is TRUE. */
ssize_t break_line(const char *line, ssize_t goal
#ifndef DISABLE_HELP
	, bool newln
#endif
	)
{
    ssize_t blank_loc = -1;
	/* Current tentative return value.  Index of the last blank we
	 * found with short enough display width. */
    ssize_t cur_loc = 0;
	/* Current index in line. */
    size_t cur_pos = 0;
	/* Current column position in line. */
    int char_len = 0;
	/* Length of current character, in bytes. */

    assert(line != NULL);

    while (*line != '\0' && goal >= cur_pos) {
	char_len = parse_mbchar(line, NULL, &cur_pos);

	if (is_blank_mbchar(line)
#ifndef DISABLE_HELP
		|| (newln && *line == '\n')
#endif
		) {
	    blank_loc = cur_loc;

#ifndef DISABLE_HELP
	    if (newln && *line == '\n')
		break;
#endif
	}

	line += char_len;
	cur_loc += char_len;
    }

    if (goal >= cur_pos)
	/* In fact, the whole line displays shorter than goal. */
	return cur_loc;

#ifndef DISABLE_HELP
    if (newln && blank_loc <= 0) {
	/* If no blank was found, or was found only as the first
	 * character, force a line break. */
	cur_loc -= char_len;
	return cur_loc;
    }
#endif

    if (blank_loc == -1) {
	/* No blank was found within the goal width,
	 * so now try and find a blank beyond it. */
	bool found_blank = FALSE;
	ssize_t found_blank_loc = 0;

	while (*line != '\0') {
	    char_len = parse_mbchar(line, NULL, NULL);

	    if (is_blank_mbchar(line)
#ifndef DISABLE_HELP
		|| (newln && *line == '\n')
#endif
		) {
		found_blank = TRUE;
		found_blank_loc = cur_loc;
	    } else if (found_blank)
		return found_blank_loc;

	    line += char_len;
	    cur_loc += char_len;
	}

	return -1;
    }

    /* Move to the last blank after blank_loc, if there is one. */
    line -= cur_loc;
    line += blank_loc;
    char_len = parse_mbchar(line, NULL, NULL);
    line += char_len;

    while (*line != '\0' && is_blank_mbchar(line)) {
	char_len = parse_mbchar(line, NULL, NULL);

	line += char_len;
	blank_loc += char_len;
    }

    return blank_loc;
}
#endif /* !DISABLE_HELP || !DISABLE_WRAPJUSTIFY */

#if !defined(NANO_TINY) || !defined(DISABLE_JUSTIFY)
/* The "indentation" of a line is the whitespace between the quote part
 * and the non-whitespace of the line. */
size_t indent_length(const char *line)
{
    size_t len = 0;
    char *blank_mb;
    int blank_mb_len;

    assert(line != NULL);

    blank_mb = charalloc(mb_cur_max());

    while (*line != '\0') {
	blank_mb_len = parse_mbchar(line, blank_mb, NULL);

	if (!is_blank_mbchar(blank_mb))
	    break;

	line += blank_mb_len;
	len += blank_mb_len;
    }

    free(blank_mb);

    return len;
}
#endif /* !NANO_TINY || !DISABLE_JUSTIFY */

#ifndef DISABLE_JUSTIFY
/* justify_format() replaces blanks with spaces and multiple spaces by 1
 * (except it maintains up to 2 after a character in punct optionally
 * followed by a character in brackets, and removes all from the end).
 *
 * justify_format() might make paragraph->data shorter, and change the
 * actual pointer with null_at().
 *
 * justify_format() will not look at the first skip characters of
 * paragraph.  skip should be at most strlen(paragraph->data).  The
 * character at paragraph[skip + 1] must not be blank. */
void justify_format(filestruct *paragraph, size_t skip)
{
    char *end, *new_end, *new_paragraph_data;
    size_t shift = 0;
#ifndef NANO_TINY
    size_t mark_shift = 0;
#endif

    /* These four asserts are assumptions about the input data. */
    assert(paragraph != NULL);
    assert(paragraph->data != NULL);
    assert(skip < strlen(paragraph->data));
    assert(!is_blank_mbchar(paragraph->data + skip));

    end = paragraph->data + skip;
    new_paragraph_data = charalloc(strlen(paragraph->data) + 1);
    strncpy(new_paragraph_data, paragraph->data, skip);
    new_end = new_paragraph_data + skip;

    while (*end != '\0') {
	int end_len;

	/* If this character is blank, change it to a space if
	 * necessary, and skip over all blanks after it. */
	if (is_blank_mbchar(end)) {
	    end_len = parse_mbchar(end, NULL, NULL);

	    *new_end = ' ';
	    new_end++;
	    end += end_len;

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_TINY
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is punctuation optionally followed by a
	 * bracket and then followed by blanks, change no more than two
	 * of the blanks to spaces if necessary, and skip over all
	 * blanks after them. */
	} else if (mbstrchr(punct, end) != NULL) {
	    end_len = parse_mbchar(end, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }

	    if (*end != '\0' && mbstrchr(brackets, end) != NULL) {
		end_len = parse_mbchar(end, NULL, NULL);

		while (end_len > 0) {
		    *new_end = *end;
		    new_end++;
		    end++;
		    end_len--;
		}
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    if (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		*new_end = ' ';
		new_end++;
		end += end_len;
	    }

	    while (*end != '\0' && is_blank_mbchar(end)) {
		end_len = parse_mbchar(end, NULL, NULL);

		end += end_len;
		shift += end_len;

#ifndef NANO_TINY
		/* Keep track of the change in the current line. */
		if (openfile->mark_set && openfile->mark_begin ==
			paragraph && openfile->mark_begin_x >= end -
			paragraph->data)
		    mark_shift += end_len;
#endif
	    }
	/* If this character is neither blank nor punctuation, leave it
	 * unchanged. */
	} else {
	    end_len = parse_mbchar(end, NULL, NULL);

	    while (end_len > 0) {
		*new_end = *end;
		new_end++;
		end++;
		end_len--;
	    }
	}
    }

    assert(*end == '\0');

    *new_end = *end;

    /* If there are spaces at the end of the line, remove them. */
    while (new_end > new_paragraph_data + skip &&
	*(new_end - 1) == ' ') {
	new_end--;
	shift++;
    }

    if (shift > 0) {
	openfile->totsize -= shift;
	null_at(&new_paragraph_data, new_end - new_paragraph_data);
	free(paragraph->data);
	paragraph->data = new_paragraph_data;

#ifndef NANO_TINY
	/* Adjust the mark coordinates to compensate for the change in
	 * the current line. */
	if (openfile->mark_set && openfile->mark_begin == paragraph) {
	    openfile->mark_begin_x -= mark_shift;
	    if (openfile->mark_begin_x > new_end - new_paragraph_data)
		openfile->mark_begin_x = new_end - new_paragraph_data;
	}
#endif
    } else
	free(new_paragraph_data);
}

/* The "quote part" of a line is the largest initial substring matching
 * the quote string.  This function returns the length of the quote part
 * of the given line.
 *
 * Note that if !HAVE_REGEX_H then we match concatenated copies of
 * quotestr. */
size_t quote_length(const char *line)
{
#ifdef HAVE_REGEX_H
    regmatch_t matches;
    int rc = regexec(&quotereg, line, 1, &matches, 0);

    if (rc == REG_NOMATCH || matches.rm_so == (regoff_t)-1)
	return 0;
    /* matches.rm_so should be 0, since the quote string should start
     * with the caret ^. */
    return matches.rm_eo;
#else	/* !HAVE_REGEX_H */
    size_t qdepth = 0;

    /* Compute quote depth level. */
    while (strncmp(line + qdepth, quotestr, quotelen) == 0)
	qdepth += quotelen;
    return qdepth;
#endif	/* !HAVE_REGEX_H */
}

/* a_line and b_line are lines of text.  The quotation part of a_line is
 * the first a_quote characters.  Check that the quotation part of
 * b_line is the same. */
bool quotes_match(const char *a_line, size_t a_quote, const char
	*b_line)
{
    /* Here is the assumption about a_quote. */
    assert(a_quote == quote_length(a_line));

    return (a_quote == quote_length(b_line) &&
	strncmp(a_line, b_line, a_quote) == 0);
}

/* We assume a_line and b_line have no quote part.  Then, we return
 * whether b_line could follow a_line in a paragraph. */
bool indents_match(const char *a_line, size_t a_indent, const char
	*b_line, size_t b_indent)
{
    assert(a_indent == indent_length(a_line));
    assert(b_indent == indent_length(b_line));

    return (b_indent <= a_indent &&
	strncmp(a_line, b_line, b_indent) == 0);
}

/* Is foo the beginning of a paragraph?
 *
 *   A line of text consists of a "quote part", followed by an
 *   "indentation part", followed by text.  The functions quote_length()
 *   and indent_length() calculate these parts.
 *
 *   A line is "part of a paragraph" if it has a part not in the quote
 *   part or the indentation.
 *
 *   A line is "the beginning of a paragraph" if it is part of a
 *   paragraph and
 *	1) it is the top line of the file, or
 *	2) the line above it is not part of a paragraph, or
 *	3) the line above it does not have precisely the same quote
 *	   part, or
 *	4) the indentation of this line is not an initial substring of
 *	   the indentation of the previous line, or
 *	5) this line has no quote part and some indentation, and
 *	   autoindent isn't turned on.
 *   The reason for number 5) is that if autoindent isn't turned on,
 *   then an indented line is expected to start a paragraph, as in
 *   books.  Thus, nano can justify an indented paragraph only if
 *   autoindent is turned on. */
bool begpar(const filestruct *const foo)
{
    size_t quote_len, indent_len, temp_id_len;

    if (foo == NULL)
	return FALSE;

    /* Case 1). */
    if (foo == openfile->fileage)
	return TRUE;

    quote_len = quote_length(foo->data);
    indent_len = indent_length(foo->data + quote_len);

    /* Not part of a paragraph. */
    if (foo->data[quote_len + indent_len] == '\0')
	return FALSE;

    /* Case 3). */
    if (!quotes_match(foo->data, quote_len, foo->prev->data))
	return TRUE;

    temp_id_len = indent_length(foo->prev->data + quote_len);

    /* Case 2) or 5) or 4). */
    if (foo->prev->data[quote_len + temp_id_len] == '\0' ||
	(quote_len == 0 && indent_len > 0
#ifndef NANO_TINY
	&& !ISSET(AUTOINDENT)
#endif
	) || !indents_match(foo->prev->data + quote_len, temp_id_len,
	foo->data + quote_len, indent_len))
	return TRUE;

    return FALSE;
}

/* Is foo inside a paragraph? */
bool inpar(const filestruct *const foo)
{
    size_t quote_len;

    if (foo == NULL)
	return FALSE;

    quote_len = quote_length(foo->data);

    return (foo->data[quote_len + indent_length(foo->data +
	quote_len)] != '\0');
}

/* Move the next par_len lines, starting with first_line, into the
 * justify buffer, leaving copies of those lines in place.  Assume that
 * par_len is greater than zero, and that there are enough lines after
 * first_line. */
void backup_lines(filestruct *first_line, size_t par_len)
{
    filestruct *top = first_line;
	/* The top of the paragraph we're backing up. */
    filestruct *bot = first_line;
	/* The bottom of the paragraph we're backing up. */
    size_t i;
	/* Generic loop variable. */
    size_t current_x_save = openfile->current_x;
    ssize_t fl_lineno_save = first_line->lineno;
    ssize_t edittop_lineno_save = openfile->edittop->lineno;
    ssize_t current_lineno_save = openfile->current->lineno;
#ifndef NANO_TINY
    bool old_mark_set = openfile->mark_set;
    ssize_t mb_lineno_save = 0;
    size_t mark_begin_x_save = 0;

    if (old_mark_set) {
	mb_lineno_save = openfile->mark_begin->lineno;
	mark_begin_x_save = openfile->mark_begin_x;
    }
#endif

    /* par_len will be one greater than the number of lines between
     * current and filebot if filebot is the last line in the
     * paragraph. */
    assert(par_len > 0 && openfile->current->lineno + par_len <=
				openfile->filebot->lineno + 1);

    /* Move bot down par_len lines to the line after the last line of
     * the paragraph, if there is one. */
    for (i = par_len; i > 0 && bot != openfile->filebot; i--)
	bot = bot->next;

    /* Move the paragraph from the current buffer's filestruct to the
     * justify buffer. */
    move_to_filestruct(&jusbuffer, &jusbottom, top, 0, bot,
		(i == 1 && bot == openfile->filebot) ? strlen(bot->data) : 0);

    /* Copy the paragraph back to the current buffer's filestruct from
     * the justify buffer. */
    copy_from_filestruct(jusbuffer);

    /* Move upward from the last line of the paragraph to the first
     * line, putting first_line, edittop, current, and mark_begin at the
     * same lines in the copied paragraph that they had in the original
     * paragraph. */
    if (openfile->current != openfile->fileage) {
	top = openfile->current->prev;
#ifndef NANO_TINY
	if (old_mark_set &&
		openfile->current->lineno == mb_lineno_save) {
	    openfile->mark_begin = openfile->current;
	    openfile->mark_begin_x = mark_begin_x_save;
	}
#endif
    } else
	top = openfile->current;
    for (i = par_len; i > 0 && top != NULL; i--) {
	if (top->lineno == fl_lineno_save)
	    first_line = top;
	if (top->lineno == edittop_lineno_save)
	    openfile->edittop = top;
	if (top->lineno == current_lineno_save)
	    openfile->current = top;
#ifndef NANO_TINY
	if (old_mark_set && top->lineno == mb_lineno_save) {
	    openfile->mark_begin = top;
	    openfile->mark_begin_x = mark_begin_x_save;
	}
#endif
	top = top->prev;
    }

    /* Put current_x at the same place in the copied paragraph that it
     * had in the original paragraph. */
    openfile->current_x = current_x_save;

    set_modified();
}

/* Find the beginning of the current paragraph if we're in one, or the
 * beginning of the next paragraph if we're not.  Afterwards, save the
 * quote length and paragraph length in *quote and *par.  Return TRUE if
 * we found a paragraph, and FALSE if there was an error or we didn't
 * find a paragraph.
 *
 * See the comment at begpar() for more about when a line is the
 * beginning of a paragraph. */
bool find_paragraph(size_t *const quote, size_t *const par)
{
    size_t quote_len;
	/* Length of the initial quotation of the paragraph we search
	 * for. */
    size_t par_len;
	/* Number of lines in the paragraph we search for. */
    filestruct *current_save;
	/* The line at the beginning of the paragraph we search for. */
    ssize_t current_y_save;
	/* The y-coordinate at the beginning of the paragraph we search
	 * for. */

#ifdef HAVE_REGEX_H
    if (quoterc != 0) {
	statusline(ALERT, _("Bad quote string %s: %s"), quotestr, quoteerr);
	return FALSE;
    }
#endif

    assert(openfile->current != NULL);

    /* If we're at the end of the last line of the file, it means that
     * there aren't any paragraphs left, so get out. */
    if (openfile->current == openfile->filebot && openfile->current_x ==
	strlen(openfile->filebot->data))
	return FALSE;

    /* If the current line isn't in a paragraph, move forward to the
     * last line of the next paragraph, if any. */
    if (!inpar(openfile->current)) {
	do_para_end(FALSE);

	/* If we end up past the beginning of the line, it means that
	 * we're at the end of the last line of the file, and the line
	 * isn't blank, in which case the last line of the file is the
	 * last line of the next paragraph.
	 *
	 * Otherwise, if we end up on a line that's in a paragraph, it
	 * means that we're on the line after the last line of the next
	 * paragraph, in which case we should move back to the last line
	 * of the next paragraph. */
	if (openfile->current_x == 0) {
	    if (!inpar(openfile->current->prev))
		return FALSE;
	    if (openfile->current != openfile->fileage)
		openfile->current = openfile->current->prev;
	}
    }

    /* If the current line isn't the first line of the paragraph, move
     * back to the first line of the paragraph. */
    if (!begpar(openfile->current))
	do_para_begin(FALSE);

    /* Now current is the first line of the paragraph.  Set quote_len to
     * the quotation length of that line, and set par_len to the number
     * of lines in this paragraph. */
    quote_len = quote_length(openfile->current->data);
    current_save = openfile->current;
    current_y_save = openfile->current_y;
    do_para_end(FALSE);
    par_len = openfile->current->lineno - current_save->lineno;

    /* If we end up past the beginning of the line, it means that we're
     * at the end of the last line of the file, and the line isn't
     * blank, in which case the last line of the file is part of the
     * paragraph. */
    if (openfile->current_x > 0)
	par_len++;
    openfile->current = current_save;
    openfile->current_y = current_y_save;

    /* Save the values of quote_len and par_len. */
    assert(quote != NULL && par != NULL);

    *quote = quote_len;
    *par = par_len;

    return TRUE;
}

/* If full_justify is TRUE, justify the entire file.  Otherwise, justify
 * the current paragraph. */
void do_justify(bool full_justify)
{
    filestruct *first_par_line = NULL;
	/* Will be the first line of the justified paragraph(s), if any.
	 * For restoring after unjustify. */
    filestruct *last_par_line = NULL;
	/* Will be the line after the last line of the justified
	 * paragraph(s), if any.  Also for restoring after unjustify. */
    bool filebot_inpar = FALSE;
	/* Whether the text at filebot is part of the current
	 * paragraph. */
    int kbinput;
	/* The first keystroke after a justification. */
    functionptrtype func;
	/* The function associated with that keystroke. */

    /* We save these variables to be restored if the user
     * unjustifies. */
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
    size_t current_x_save = openfile->current_x;
    size_t totsize_save = openfile->totsize;
#ifndef NANO_TINY
    filestruct *mark_begin_save = openfile->mark_begin;
    size_t mark_begin_x_save = openfile->mark_begin_x;
#endif
    bool modified_save = openfile->modified;

    /* Move to the beginning of the current line, so that justifying at
     * the end of the last line of the file, if that line isn't blank,
     * will work the first time through. */
    openfile->current_x = 0;

    /* If we're justifying the entire file, start at the beginning. */
    if (full_justify)
	openfile->current = openfile->fileage;

    while (TRUE) {
	size_t i;
	    /* Generic loop variable. */
	filestruct *curr_first_par_line;
	    /* The first line of the current paragraph. */
	size_t quote_len;
	    /* Length of the initial quotation of the current
	     * paragraph. */
	size_t indent_len;
	    /* Length of the initial indentation of the current
	     * paragraph. */
	size_t par_len;
	    /* Number of lines in the current paragraph. */
	ssize_t break_pos;
	    /* Where we will break lines. */
	char *indent_string;
	    /* The first indentation that doesn't match the initial
	     * indentation of the current paragraph.  This is put at the
	     * beginning of every line broken off the first justified
	     * line of the paragraph.  Note that this works because a
	     * paragraph can only contain two indentations at most: the
	     * initial one, and a different one starting on a line after
	     * the first.  See the comment at begpar() for more about
	     * when a line is part of a paragraph. */

	/* Find the first line of the paragraph to be justified.  That
	 * is the start of this paragraph if we're in one, or the start
	 * of the next otherwise.  Save the quote length and paragraph
	 * length (number of lines).  Don't refresh the screen yet,
	 * since we'll do that after we justify.
	 *
	 * If the search failed, we do one of two things.  If we're
	 * justifying the whole file, and we've found at least one
	 * paragraph, it means that we should justify all the way to the
	 * last line of the file, so set the last line of the text to be
	 * justified to the last line of the file and break out of the
	 * loop.  Otherwise, it means that there are no paragraph(s) to
	 * justify, so refresh the screen and get out. */
	if (!find_paragraph(&quote_len, &par_len)) {
	    if (full_justify && first_par_line != NULL) {
		last_par_line = openfile->filebot;
		break;
	    } else {
		refresh_needed = TRUE;
		return;
	    }
	}

	/* par_len will be one greater than the number of lines between
	 * current and filebot if filebot is the last line in the
	 * paragraph.  Set filebot_inpar to TRUE if this is the case. */
	filebot_inpar = (openfile->current->lineno + par_len ==
		openfile->filebot->lineno + 1);

	/* If we haven't already done it, move the original paragraph(s)
	 * to the justify buffer, splice a copy of the original
	 * paragraph(s) into the file in the same place, and set
	 * first_par_line to the first line of the copy. */
	if (first_par_line == NULL) {
	    backup_lines(openfile->current, full_justify ?
		openfile->filebot->lineno - openfile->current->lineno +
		((openfile->filebot->data[0] != '\0') ? 1 : 0) :
		par_len);
	    first_par_line = openfile->current;
	}

	/* Set curr_first_par_line to the first line of the current
	 * paragraph. */
	curr_first_par_line = openfile->current;

	/* Initialize indent_string to a blank string. */
	indent_string = mallocstrcpy(NULL, "");

	/* Find the first indentation in the paragraph that doesn't
	 * match the indentation of the first line, and save it in
	 * indent_string.  If all the indentations are the same, save
	 * the indentation of the first line in indent_string. */
	{
	    const filestruct *indent_line = openfile->current;
	    bool past_first_line = FALSE;

	    for (i = 0; i < par_len; i++) {
		indent_len = quote_len +
			indent_length(indent_line->data + quote_len);

		if (indent_len != strlen(indent_string)) {
		    indent_string = mallocstrncpy(indent_string,
			indent_line->data, indent_len + 1);
		    indent_string[indent_len] = '\0';

		    if (past_first_line)
			break;
		}

		if (indent_line == openfile->current)
		    past_first_line = TRUE;

		indent_line = indent_line->next;
	    }
	}

	/* Now tack all the lines of the paragraph together, skipping
	 * the quoting and indentation on all lines after the first. */
	for (i = 0; i < par_len - 1; i++) {
	    filestruct *next_line = openfile->current->next;
	    size_t line_len = strlen(openfile->current->data);
	    size_t next_line_len = strlen(openfile->current->next->data);

	    indent_len = quote_len +
		indent_length(openfile->current->next->data + quote_len);

	    next_line_len -= indent_len;
	    openfile->totsize -= indent_len;

	    /* We're just about to tack the next line onto this one.  If
	     * this line isn't empty, make sure it ends in a space. */
	    if (line_len > 0 &&
			openfile->current->data[line_len - 1] != ' ') {
		line_len++;
		openfile->current->data =
			charealloc(openfile->current->data, line_len + 1);
		openfile->current->data[line_len - 1] = ' ';
		openfile->current->data[line_len] = '\0';
		openfile->totsize++;
	    }

	    openfile->current->data = charealloc(openfile->current->data,
			line_len + next_line_len + 1);
	    strcat(openfile->current->data, next_line->data + indent_len);

#ifndef NANO_TINY
	    /* If needed, adjust the coordinates of the mark. */
	    if (openfile->mark_set &&
			openfile->mark_begin == next_line) {
		openfile->mark_begin = openfile->current;
		openfile->mark_begin_x += line_len - indent_len;
	    }
#endif
	    /* Don't destroy edittop! */
	    if (next_line == openfile->edittop)
		openfile->edittop = openfile->current;

	    unlink_node(next_line);

	    /* If we've removed the next line, we need to go through
	     * this line again. */
	    i--;

	    par_len--;
	    openfile->totsize--;
	}

	/* Call justify_format() on the paragraph, which will remove
	 * excess spaces from it and change all blank characters to
	 * spaces. */
	justify_format(openfile->current, quote_len +
		indent_length(openfile->current->data + quote_len));

	while (par_len > 0 && strlenpt(openfile->current->data) > fill) {
	    size_t line_len = strlen(openfile->current->data);

	    indent_len = strlen(indent_string);

	    /* If this line is too long, try to wrap it to the next line
	     * to make it short enough. */
	    break_pos = break_line(openfile->current->data + indent_len,
		fill - strnlenpt(openfile->current->data, indent_len)
#ifndef DISABLE_HELP
		, FALSE
#endif
		);

	    /* We can't break the line, or don't need to, so get out. */
	    if (break_pos == -1 || break_pos + indent_len == line_len)
		break;

	    /* Move forward to the character after the indentation and
	     * just after the space. */
	    break_pos += indent_len + 1;

	    assert(break_pos <= line_len);

	    /* If this paragraph is non-quoted, and autoindent isn't
	     * turned on, set the indentation length to zero so that the
	     * indentation is treated as part of the line. */
	    if (quote_len == 0
#ifndef NANO_TINY
		&& !ISSET(AUTOINDENT)
#endif
		)
		indent_len = 0;

	    /* Insert a new line after the current one. */
	    splice_node(openfile->current, make_new_node(openfile->current));

	    /* Copy the text after where we're going to break the
	     * current line to the next line. */
	    openfile->current->next->data = charalloc(indent_len + 1 +
		line_len - break_pos);
	    strncpy(openfile->current->next->data, indent_string,
		indent_len);
	    strcpy(openfile->current->next->data + indent_len,
		openfile->current->data + break_pos);

	    par_len++;
	    openfile->totsize += indent_len + 1;

#ifndef NANO_TINY
	    /* Adjust the mark coordinates to compensate for the change
	     * in the current line. */
	    if (openfile->mark_set &&
			openfile->mark_begin == openfile->current &&
			openfile->mark_begin_x > break_pos) {
		openfile->mark_begin = openfile->current->next;
		openfile->mark_begin_x -= break_pos - indent_len;
	    }
#endif

	    /* Break the current line. */
	if (ISSET(JUSTIFY_TRIM)) {
		while (break_pos > 0 &&
			is_blank_mbchar(&openfile->current->data[break_pos - 1])) {
		    break_pos--;
		}
	    }
	    null_at(&openfile->current->data, break_pos);

	    /* Go to the next line. */
	    par_len--;
	    openfile->current_y++;
	    openfile->current = openfile->current->next;
	}

	/* We're done breaking lines, so we don't need indent_string
	 * anymore. */
	free(indent_string);

	/* Go to the next line, if possible.  If there is no next line,
	 * move to the end of the current line. */
	if (openfile->current != openfile->filebot) {
	    openfile->current_y++;
	    openfile->current = openfile->current->next;
	} else
	    openfile->current_x = strlen(openfile->current->data);

	/* Renumber the lines of the now-justified current paragraph,
	 * since both find_paragraph() and edit_refresh() need the line
	 * numbers to be right. */
	renumber(curr_first_par_line);

	/* We've just finished justifying the paragraph.  If we're not
	 * justifying the entire file, break out of the loop.
	 * Otherwise, continue the loop so that we justify all the
	 * paragraphs in the file. */
	if (!full_justify)
	    break;
    }

    /* We are now done justifying the paragraph or the file, so clean
     * up.  current_y and totsize have been maintained above.  If we
     * actually justified something, set last_par_line to the new end of
     * the paragraph. */
    if (first_par_line != NULL)
	last_par_line = openfile->current;

    edit_refresh();

    /* Display the shortcut list with UnJustify. */
    uncutfunc->desc = unjust_tag;
    display_main_list();

    /* Now get a keystroke and see if it's unjustify.  If not, put back
     * the keystroke and return. */
#ifndef NANO_TINY
    do {
#endif
	statusbar(_("Can now UnJustify!"));
	reset_cursor();
	curs_set(1);
	kbinput = do_input(FALSE);
#ifndef NANO_TINY
    } while (kbinput == KEY_WINCH);
#endif

    /* If needed, unset the cursor-position suppression flag, so the cursor
     * position /will/ be displayed upon a return to the main loop. */
    if (ISSET(CONST_UPDATE))
	do_cursorpos(TRUE);

    func = func_from_key(&kbinput);

    if (func == do_uncut_text
#ifndef NANO_TINY
			|| func == do_undo
#endif
		) {
	/* Splice the justify buffer back into the file, but only if we
	 * actually justified something. */
	if (first_par_line != NULL) {
	    filestruct *top_save;

	    /* Partition the filestruct so that it contains only the
	     * text of the justified paragraph. */
	    filepart = partition_filestruct(first_par_line, 0,
				last_par_line, filebot_inpar ?
				strlen(last_par_line->data) : 0);

	    /* Remove the text of the justified paragraph, and
	     * replace it with the text in the justify buffer. */
	    free_filestruct(openfile->fileage);
	    openfile->fileage = jusbuffer;
	    openfile->filebot = jusbottom;

	    top_save = openfile->fileage;

	    /* Unpartition the filestruct so that it contains all the
	     * text again.  Note that the justified paragraph has been
	     * replaced with the unjustified paragraph. */
	    unpartition_filestruct(&filepart);

	    /* Renumber, starting with the beginning line of the old
	     * partition. */
	    renumber(top_save);

	    /* Restore the justify we just did (ungrateful user!). */
	    openfile->edittop = edittop_save;
	    openfile->current = current_save;
	    openfile->current_x = current_x_save;
	    openfile->totsize = totsize_save;
#ifndef NANO_TINY
	    if (openfile->mark_set) {
		openfile->mark_begin = mark_begin_save;
		openfile->mark_begin_x = mark_begin_x_save;
	    }
#endif
	    openfile->modified = modified_save;

	    /* Clear the justify buffer. */
	    jusbuffer = NULL;

	    if (!openfile->modified)
		titlebar(NULL);
	    refresh_needed = TRUE;
	}
    } else {
	/* Put the keystroke back into the queue. */
	unget_kbinput(kbinput, meta_key);

	/* Set the desired screen column (always zero, except at EOF). */
	openfile->placewewant = xplustabs();

#ifndef NANO_TINY
	/* Throw away the entire undo stack, to prevent a crash when
	 * the user tries to undo something in the justified text. */
	discard_until(NULL, openfile);
	openfile->current_undo = NULL;
#endif
	/* Blow away the text in the justify buffer. */
	free_filestruct(jusbuffer);
	jusbuffer = NULL;
    }

    blank_statusbar();

    /* Display the shortcut list with UnCut. */
    uncutfunc->desc = uncut_tag;
    display_main_list();
}

/* Justify the current paragraph. */
void do_justify_void(void)
{
    do_justify(FALSE);
}

/* Justify the entire file. */
void do_full_justify(void)
{
    do_justify(TRUE);
}
#endif /* !DISABLE_JUSTIFY */

#ifndef DISABLE_SPELLER
/* A word is misspelled in the file.  Let the user replace it.  We
 * return FALSE if the user cancels. */
bool do_int_spell_fix(const char *word)
{
    char *save_search, *exp_word;
    size_t current_x_save = openfile->current_x;
    filestruct *edittop_save = openfile->edittop;
    filestruct *current_save = openfile->current;
	/* Save where we are. */
    bool proceed = FALSE;
	/* The return value of this function. */
    bool result;
	/* The return value of searching for a misspelled word. */
    unsigned stash[sizeof(flags) / sizeof(flags[0])];
	/* A storage place for the current flag settings. */
#ifndef NANO_TINY
    bool old_mark_set = openfile->mark_set;
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
#endif

    /* Save the settings of the global flags. */
    memcpy(stash, flags, sizeof(flags));

    /* Make sure spell-check is case sensitive. */
    SET(CASE_SENSITIVE);

#ifndef NANO_TINY
    /* Make sure spell-check goes forward only. */
    UNSET(BACKWARDS_SEARCH);
#endif
#ifdef HAVE_REGEX_H
    /* Make sure spell-check doesn't use regular expressions. */
    UNSET(USE_REGEXP);
#endif

    /* Save the current search string, then set it to the misspelled word. */
    save_search = last_search;
    last_search = mallocstrcpy(NULL, word);

#ifndef NANO_TINY
    /* If the mark is on, start at the beginning of the marked region. */
    if (old_mark_set) {
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, &right_side_up);
	/* If the region is marked normally, swap the end points, so that
	 * (current, current_x) (where searching starts) is at the top. */
	if (right_side_up) {
	    openfile->current = top;
	    openfile->current_x = (size_t)(top_x - 1);
	    openfile->mark_begin = bot;
	    openfile->mark_begin_x = bot_x;
	} else
	    openfile->current_x = (size_t)(openfile->current_x - 1);
	openfile->mark_set = FALSE;
    } else
#endif
    /* Otherwise, start from the top of the file. */
    {
	openfile->edittop = openfile->fileage;
	openfile->current = openfile->fileage;
	openfile->current_x = (size_t)-1;
    }

    /* Find the first whole occurrence of word. */
    result = findnextstr(TRUE, NULL, 0, word, NULL);

    /* If the word isn't found, alert the user; if it is, allow correction. */
    if (result == 0) {
	statusline(ALERT, _("Unfindable word: %s"), word);
	lastmessage = HUSH;
	proceed = TRUE;
	napms(2800);
    } else if (result == 1) {
	exp_word = display_string(openfile->current->data, xplustabs(),
					strlenpt(word), FALSE);
	edit_refresh();

	spotlight(TRUE, exp_word);

	/* Let the user supply a correctly spelled alternative. */
	proceed = (do_prompt(FALSE,
#ifndef DISABLE_TABCOMP
				TRUE,
#endif
				MSPELL, word,
#ifndef DISABLE_HISTORIES
				NULL,
#endif
				edit_refresh, _("Edit a replacement")) != -1);

	spotlight(FALSE, exp_word);

	free(exp_word);

	/* If a replacement was given, go through all occurrences. */
	if (proceed && strcmp(word, answer) != 0) {
	    openfile->current_x--;
#ifndef NANO_TINY
	    /* Replacements should happen only in the marked region. */
	    openfile->mark_set = old_mark_set;
#endif
	    do_replace_loop(TRUE, current_save, &current_x_save, word);

	    /* TRANSLATORS: Shown after fixing misspellings in one word. */
	    statusbar(_("Next word..."));
	    napms(400);
	}
    }

#ifndef NANO_TINY
    if (old_mark_set) {
	/* Restore the (compensated) end points of the marked region. */
	if (right_side_up) {
	    openfile->current = openfile->mark_begin;
	    openfile->current_x = openfile->mark_begin_x;
	    openfile->mark_begin = top;
	    openfile->mark_begin_x = top_x;
	} else {
	    openfile->current = top;
	    openfile->current_x = top_x;
	}
	openfile->mark_set = TRUE;
    } else
#endif
    {
	/* Restore the (compensated) cursor position. */
	openfile->current = current_save;
	openfile->current_x = current_x_save;
    }

    /* Restore the string that was last searched for. */
    free(last_search);
    last_search = save_search;

    /* Restore the viewport to where it was. */
    openfile->edittop = edittop_save;

    /* Restore the settings of the global flags. */
    memcpy(flags, stash, sizeof(flags));

    return proceed;
}

/* Internal (integrated) spell checking using the spell program,
 * filtered through the sort and uniq programs.  Return NULL for normal
 * termination, and the error string otherwise. */
const char *do_int_speller(const char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
    pid_t pid_spell, pid_sort, pid_uniq;
    int spell_status, sort_status, uniq_status;

    /* Create all three pipes up front. */
    if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 || pipe(uniq_fd) == -1)
	return _("Could not create pipe");

    statusbar(_("Creating misspelled word list, please wait..."));

    /* A new process to run spell in. */
    if ((pid_spell = fork()) == 0) {
	/* Child continues (i.e. future spell process). */
	close(spell_fd[0]);

	/* Replace the standard input with the temp file. */
	if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1)
	    goto close_pipes_and_exit;

	if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(tempfile_fd);

	/* Send spell's standard output to the pipe. */
	if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[1]);

	/* Start the spell program; we are using $PATH. */
	execlp("spell", "spell", NULL);

	/* This should not be reached if spell is found. */
	exit(1);
    }

    /* Parent continues here. */
    close(spell_fd[1]);

    /* A new process to run sort in. */
    if ((pid_sort = fork()) == 0) {
	/* Child continues (i.e. future sort process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[0]);

	/* Send sort's standard output to the new pipe. */
	if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[1]);

	/* Start the sort program.  Use -f to ignore case. */
	execlp("sort", "sort", "-f", NULL);

	/* This should not be reached if sort is found. */
	exit(1);
    }

    close(spell_fd[0]);
    close(sort_fd[1]);

    /* A new process to run uniq in. */
    if ((pid_uniq = fork()) == 0) {
	/* Child continues (i.e. future uniq process).  Replace the
	 * standard input with the standard output of the old pipe. */
	if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[0]);

	/* Send uniq's standard output to the new pipe. */
	if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(uniq_fd[1]);

	/* Start the uniq program; we are using PATH. */
	execlp("uniq", "uniq", NULL);

	/* This should not be reached if uniq is found. */
	exit(1);
    }

    close(sort_fd[0]);
    close(uniq_fd[1]);

    /* The child process was not forked successfully. */
    if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
	close(uniq_fd[0]);
	return _("Could not fork");
    }

    /* Get the system pipe buffer size. */
    if ((pipe_buff_size = fpathconf(uniq_fd[0], _PC_PIPE_BUF)) < 1) {
	close(uniq_fd[0]);
	return _("Could not get size of pipe buffer");
    }

    /* Read in the returned spelling errors. */
    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(uniq_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {
	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = charealloc(read_buff, read_buff_size);
	read_buff_ptr += read_buff_read;
    }

    *read_buff_ptr = '\0';
    close(uniq_fd[0]);

    /* Process the spelling errors. */
    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr != '\0') {
	if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
	    *read_buff_ptr = '\0';
	    if (read_buff_word != read_buff_ptr) {
		if (!do_int_spell_fix(read_buff_word)) {
		    read_buff_word = read_buff_ptr;
		    break;
		}
	    }
	    read_buff_word = read_buff_ptr + 1;
	}
	read_buff_ptr++;
    }

    /* Special case: the last word doesn't end with '\r' or '\n'. */
    if (read_buff_word != read_buff_ptr)
	do_int_spell_fix(read_buff_word);

    free(read_buff);
    search_replace_abort();
    refresh_needed = TRUE;

    /* Process the end of the three processes. */
    waitpid(pid_spell, &spell_status, 0);
    waitpid(pid_sort, &sort_status, 0);
    waitpid(pid_uniq, &uniq_status, 0);

    if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status))
	return _("Error invoking \"spell\"");

    if (WIFEXITED(sort_status) == 0 || WEXITSTATUS(sort_status))
	return _("Error invoking \"sort -f\"");

    if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status))
	return _("Error invoking \"uniq\"");

    /* When all went okay. */
    return NULL;

  close_pipes_and_exit:
    /* Don't leak any handles. */
    close(tempfile_fd);
    close(spell_fd[0]);
    close(spell_fd[1]);
    close(sort_fd[0]);
    close(sort_fd[1]);
    close(uniq_fd[0]);
    close(uniq_fd[1]);
    exit(1);
}

/* External (alternate) spell checking.  Return NULL for normal
 * termination, and the error string otherwise. */
const char *do_alt_speller(char *tempfile_name)
{
    int alt_spell_status;
    size_t current_x_save = openfile->current_x;
    ssize_t current_y_save = openfile->current_y;
    ssize_t lineno_save = openfile->current->lineno;
    struct stat spellfileinfo;
    time_t timestamp;
    pid_t pid_spell;
    char *ptr;
    static int arglen = 3;
    static char **spellargs = NULL;
#ifndef NANO_TINY
    bool old_mark_set = openfile->mark_set;
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    filestruct *top, *bot;
    size_t top_x, bot_x;
    bool right_side_up = FALSE;
    ssize_t mb_lineno_save = 0;
	/* We're going to close the current file, and open the output of
	 * the alternate spell command.  The line that mark_begin points
	 * to will be freed, so we save the line number and restore it
	 * afterwards. */
    size_t size_of_surrounding = 0;
	/* The size of the text outside of a marked region. */
#endif

    /* Get the timestamp and the size of the temporary file. */
    stat(tempfile_name, &spellfileinfo);
    timestamp = spellfileinfo.st_mtime;

    /* If the number of bytes to check is zero, get out. */
    if (spellfileinfo.st_size == 0)
	return NULL;

#ifndef NANO_TINY
    if (old_mark_set) {
	/* If the mark is on, save the number of the line it starts on,
	 * and then turn the mark off. */
	mb_lineno_save = openfile->mark_begin->lineno;
	openfile->mark_set = FALSE;
    }
#endif

    endwin();

    /* Set up an argument list to pass to execvp(). */
    if (spellargs == NULL) {
	spellargs = (char **)nmalloc(arglen * sizeof(char *));

	spellargs[0] = strtok(alt_speller, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    spellargs = (char **)nrealloc(spellargs, arglen *
		sizeof(char *));
	    spellargs[arglen - 3] = ptr;
	}
	spellargs[arglen - 1] = NULL;
    }
    spellargs[arglen - 2] = tempfile_name;

    /* Start a new process for the alternate speller. */
    if ((pid_spell = fork()) == 0) {
	/* Start alternate spell program; we are using $PATH. */
	execvp(spellargs[0], spellargs);

	/* Should not be reached, if alternate speller is found!!! */
	exit(1);
    }

    /* If we couldn't fork, get out. */
    if (pid_spell < 0)
	return _("Could not fork");

#ifndef NANO_TINY
    /* Block SIGWINCHes so the spell checker doesn't get any. */
    allow_sigwinch(FALSE);
#endif

    /* Wait for the alternate spell checker to finish. */
    wait(&alt_spell_status);

    /* Reenter curses mode. */
    doupdate();

    /* Restore the terminal to its previous state. */
    terminal_init();

    if (!WIFEXITED(alt_spell_status) || WEXITSTATUS(alt_spell_status) != 0) {
#ifndef NANO_TINY
	/* Turn the mark back on if it was on before. */
	openfile->mark_set = old_mark_set;
#endif
	return invocation_error(alt_speller);
    }

#ifndef NANO_TINY
    if (old_mark_set) {
	/* Trim the filestruct so that it contains only the marked text. */
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, &right_side_up);
	filepart = partition_filestruct(top, top_x, bot, bot_x);

	/* Foresay whether a magicline will be added when the
	 * spell-checked text is read back in. */
	if (!ISSET(NO_NEWLINES))
	    added_magicline = (openfile->filebot->data[0] != '\0');

	/* Compute the size of the text outside of the marked region. */
	size_of_surrounding = openfile->totsize - get_totsize(top, bot);
    }
#endif

    /* Replace the text of the current buffer with the spell-checked text. */
    replace_buffer(tempfile_name);

#ifndef NANO_TINY
    if (old_mark_set) {
	filestruct *top_save = openfile->fileage;
	/* Adjust the end point of the marked region for any change in
	 * length of the region's last line. */
	if (right_side_up)
	    current_x_save = strlen(openfile->filebot->data);
	else
	    openfile->mark_begin_x = strlen(openfile->filebot->data);

	/* If a magicline was added, remove it again. */
	if (added_magicline)
	    remove_magicline();

	/* Unpartition the filestruct so that it contains all the text
	 * again.  Note that we've replaced the marked text originally
	 * in the partition with the spell-checked marked text in the
	 * temp file. */
	unpartition_filestruct(&filepart);

	/* Renumber, starting with the beginning line of the old partition. */
	renumber(top_save);

	/* Add back the size of the text surrounding the marked region. */
	openfile->totsize += size_of_surrounding;

	/* Restore the position of the mark, and turn it back on. */
	openfile->mark_begin = fsfromline(mb_lineno_save);
	openfile->mark_set = TRUE;
    }
#endif /* !NANO_TINY */

    /* Go back to the old position. */
    goto_line_posx(lineno_save, current_x_save);
    openfile->current_y = current_y_save;
    edit_update(STATIONARY);

    /* Stat the temporary file again, and mark the buffer as modified only
     * if this file was changed since it was written. */
    stat(tempfile_name, &spellfileinfo);
    if (spellfileinfo.st_mtime != timestamp)
	set_modified();

#ifndef NANO_TINY
    /* Unblock SIGWINCHes again. */
    allow_sigwinch(TRUE);
#endif

    return NULL;
}

/* Spell check the current file.  If an alternate spell checker is
 * specified, use it.  Otherwise, use the internal spell checker. */
void do_spell(void)
{
    bool status;
    FILE *temp_file;
    char *temp;
    const char *spell_msg;

    if (ISSET(RESTRICTED)) {
	show_restricted_warning();
	return;
    }

    temp = safe_tempfile(&temp_file);

    if (temp == NULL) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
	return;
    }

#ifndef NANO_TINY
    if (openfile->mark_set)
	status = write_marked_file(temp, temp_file, TRUE, OVERWRITE);
    else
#endif
	status = write_file(temp, temp_file, TRUE, OVERWRITE, FALSE);

    if (!status) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
	free(temp);
	return;
    }

    blank_bottombars();
    statusbar(_("Invoking spell checker, please wait"));

    spell_msg = (alt_speller != NULL) ? do_alt_speller(temp) :
					do_int_speller(temp);
    unlink(temp);
    free(temp);

    /* If the spell-checker printed any error messages onscreen, make
     * sure that they're cleared off. */
    total_refresh();

    if (spell_msg != NULL) {
	if (errno == 0)
	    /* Don't display an error message of "Success". */
	    statusline(ALERT, _("Spell checking failed: %s"), spell_msg);
	else
	    statusline(ALERT, _("Spell checking failed: %s: %s"), spell_msg,
						strerror(errno));
    } else
	statusbar(_("Finished checking spelling"));
}
#endif /* !DISABLE_SPELLER */

#ifndef DISABLE_COLOR
/* Run a linting program on the current buffer.  Return NULL for normal
 * termination, and the error string otherwise. */
void do_linter(void)
{
    char *read_buff, *read_buff_ptr, *read_buff_word, *ptr;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    size_t parsesuccess = 0;
    int lint_status, lint_fd[2];
    pid_t pid_lint;
    static int arglen = 3;
    static char **lintargs = NULL;
    char *lintcopy, *convendptr = NULL;
    lintstruct *lints = NULL, *tmplint = NULL, *curlint = NULL;

    if (ISSET(RESTRICTED)) {
	show_restricted_warning();
	return;
    }

    if (!openfile->syntax || !openfile->syntax->linter) {
	statusbar(_("No linter defined for this type of file!"));
	return;
    }

    if (openfile->modified) {
	int i = do_yesno_prompt(FALSE, _("Save modified buffer before linting?"));

	if (i == -1) {
	    statusbar(_("Cancelled"));
	    return;
	} else if (i == 1 && (do_writeout(FALSE) != TRUE))
	    return;
    }

    lintcopy = mallocstrcpy(NULL, openfile->syntax->linter);
    /* Create a pipe up front. */
    if (pipe(lint_fd) == -1) {
	statusbar(_("Could not create pipe"));
	return;
    }

    blank_bottombars();
    statusbar(_("Invoking linter, please wait"));

    /* Set up an argument list to pass to execvp(). */
    if (lintargs == NULL) {
	lintargs = (char **)nmalloc(arglen * sizeof(char *));

	lintargs[0] = strtok(lintcopy, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    lintargs = (char **)nrealloc(lintargs, arglen * sizeof(char *));
	    lintargs[arglen - 3] = ptr;
	}
	lintargs[arglen - 1] = NULL;
    }
    lintargs[arglen - 2] = openfile->filename;

    /* Start a new process to run the linter in. */
    if ((pid_lint = fork()) == 0) {

	/* Child continues here (i.e. the future linting process). */
	close(lint_fd[0]);

	/* Send the linter's standard output + err to the pipe. */
	if (dup2(lint_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    exit(9);
	if (dup2(lint_fd[1], STDERR_FILENO) != STDERR_FILENO)
	    exit(9);

	close(lint_fd[1]);

	/* Start the linter program; we are using $PATH. */
	execvp(lintargs[0], lintargs);

	/* This is only reached when the linter is not found. */
	exit(9);
    }

    /* Parent continues here. */
    close(lint_fd[1]);

    /* If the child process was not forked successfully... */
    if (pid_lint < 0) {
	close(lint_fd[0]);
	statusbar(_("Could not fork"));
	return;
    }

    /* Get the system pipe buffer size. */
    if ((pipe_buff_size = fpathconf(lint_fd[0], _PC_PIPE_BUF)) < 1) {
	close(lint_fd[0]);
	statusbar(_("Could not get size of pipe buffer"));
	return;
    }

    /* Read in the returned syntax errors. */
    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(lint_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {
#ifdef DEBUG
	fprintf(stderr, "text.c:do_linter:%ld bytes (%s)\n", (long)bytesread, read_buff_ptr);
#endif
	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = charealloc(read_buff, read_buff_size);
	read_buff_ptr += read_buff_read;
    }

    *read_buff_ptr = '\0';
    close(lint_fd[0]);

#ifdef DEBUG
		fprintf(stderr, "text.c:do_lint:Raw output: %s\n", read_buff);
#endif

    /* Process the linter output. */
    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr != '\0') {
	if ((*read_buff_ptr == '\r') || (*read_buff_ptr == '\n')) {
	    *read_buff_ptr = '\0';
	    if (read_buff_word != read_buff_ptr) {
		char *filename = NULL, *linestr = NULL, *maybecol = NULL;
		char *message = mallocstrcpy(NULL, read_buff_word);

		/* At the moment we're assuming the following formats:
		 *
		 * filenameorcategory:line:column:message (e.g. splint)
		 * filenameorcategory:line:message        (e.g. pyflakes)
		 * filenameorcategory:line,col:message    (e.g. pylint)
		 *
		 * This could be turned into some scanf() based parser,
		 * but ugh. */
		if ((filename = strtok(read_buff_word, ":")) != NULL) {
		    if ((linestr = strtok(NULL, ":")) != NULL) {
			if ((maybecol = strtok(NULL, ":")) != NULL) {
			    ssize_t tmplineno = 0, tmpcolno = 0;
			    char *tmplinecol;

			    tmplineno = strtol(linestr, NULL, 10);
			    if (tmplineno <= 0) {
				read_buff_ptr++;
				free(message);
				continue;
			    }

			    tmpcolno = strtol(maybecol, &convendptr, 10);
			    if (*convendptr != '\0') {
				/* Previous field might still be
				 * line,col format. */
				strtok(linestr, ",");
				if ((tmplinecol = strtok(NULL, ",")) != NULL)
				    tmpcolno = strtol(tmplinecol, NULL, 10);
			    }

#ifdef DEBUG
			    fprintf(stderr, "text.c:do_lint:Successful parse! %ld:%ld:%s\n", (long)tmplineno, (long)tmpcolno, message);
#endif
			    /* Nice.  We have a lint message we can use. */
			    parsesuccess++;
			    tmplint = curlint;
			    curlint = nmalloc(sizeof(lintstruct));
			    curlint->next = NULL;
			    curlint->prev = tmplint;
			    if (curlint->prev != NULL)
				curlint->prev->next = curlint;
			    curlint->msg = mallocstrcpy(NULL, message);
			    curlint->lineno = tmplineno;
			    curlint->colno = tmpcolno;
			    curlint->filename = mallocstrcpy(NULL, filename);

			    if (lints == NULL)
				lints = curlint;
			}
		    }
		} else
		    free(message);
	    }
	    read_buff_word = read_buff_ptr + 1;
	}
	read_buff_ptr++;
    }

    /* Process the end of the linting process. */
    waitpid(pid_lint, &lint_status, 0);

    if (!WIFEXITED(lint_status) || WEXITSTATUS(lint_status) > 2) {
	statusbar(invocation_error(openfile->syntax->linter));
	return;
    }

    free(read_buff);

    if (parsesuccess == 0) {
	statusline(HUSH, _("Got 0 parsable lines from command: %s"),
			openfile->syntax->linter);
	return;
    }

    bottombars(MLINTER);
    tmplint = NULL;
    curlint = lints;

    while (TRUE) {
	int kbinput;
	functionptrtype func;

	if (tmplint != curlint) {
#ifndef NANO_TINY
	    struct stat lintfileinfo;

	  new_lint_loop:
	    if (stat(curlint->filename, &lintfileinfo) != -1) {
		if (openfile->current_stat->st_ino != lintfileinfo.st_ino) {
		    openfilestruct *tmpof = openfile;
		    while (tmpof != openfile->next) {
			if (tmpof->current_stat->st_ino == lintfileinfo.st_ino)
			    break;
			tmpof = tmpof->next;
		    }
		    if (tmpof->current_stat->st_ino != lintfileinfo.st_ino) {
			char *msg = charalloc(1024 + strlen(curlint->filename));
			int i;

			sprintf(msg, _("This message is for unopened file %s,"
					" open it in a new buffer?"),
				curlint->filename);
			/* Show a cursor after the question. */
			curs_set(1);
			i = do_yesno_prompt(FALSE, msg);
			free(msg);
			if (i == -1) {
			    statusbar(_("Cancelled"));
			    goto free_lints_and_return;
			} else if (i == 1) {
			    SET(MULTIBUFFER);
			    open_buffer(curlint->filename, FALSE);
			} else {
			    char *dontwantfile = curlint->filename;

			    while (curlint != NULL && !strcmp(curlint->filename, dontwantfile))
				curlint = curlint->next;
			    if (curlint == NULL) {
				statusbar(_("No more errors in unopened files, cancelling"));
				break;
			    } else
				goto new_lint_loop;
			}
		    } else
			openfile = tmpof;
		}
	    }
#endif /* !NANO_TINY */
	    do_gotolinecolumn(curlint->lineno, curlint->colno, FALSE, FALSE);
	    titlebar(NULL);
	    edit_refresh();
	    statusbar(curlint->msg);
	    bottombars(MLINTER);
	}

	/* Place and show the cursor to indicate the affected line. */
	reset_cursor();
	wnoutrefresh(edit);
	curs_set(1);

	kbinput = get_kbinput(bottomwin);

#ifndef NANO_TINY
	if (kbinput == KEY_WINCH)
	    continue;
#endif
	func = func_from_key(&kbinput);
	tmplint = curlint;

	if (func == do_cancel)
	    break;
	else if (func == do_help_void) {
	    tmplint = NULL;
	    do_help_void();
	} else if (func == do_page_down) {
	    if (curlint->next != NULL)
		curlint = curlint->next;
	    else
		statusbar(_("At last message"));
	} else if (func == do_page_up) {
	    if (curlint->prev != NULL)
		curlint = curlint->prev;
	    else
		statusbar(_("At first message"));
	}
    }

    blank_statusbar();

#ifndef NANO_TINY
  free_lints_and_return:
#endif
    for (curlint = lints; curlint != NULL;) {
	tmplint = curlint;
	curlint = curlint->next;
	free(tmplint->msg);
	free(tmplint->filename);
	free(tmplint);
    }
}

#ifndef DISABLE_SPELLER
/* Run a formatter for the current syntax.  This expects the formatter
 * to be non-interactive and operate on a file in-place, which we'll
 * pass it on the command line. */
void do_formatter(void)
{
    bool status;
    FILE *temp_file;
    int format_status;
    ssize_t lineno_save = openfile->current->lineno;
    ssize_t current_y_save = openfile->current_y;
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    pid_t pid_format;
    static int arglen = 3;
    static char **formatargs = NULL;
    char *temp, *ptr, *finalstatus = NULL;

    if (openfile->totsize == 0) {
	statusbar(_("Finished"));
	return;
    }

    temp = safe_tempfile(&temp_file);

    if (temp == NULL) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
	return;
    }

    /* We're not supporting partial formatting, oi vey. */
    openfile->mark_set = FALSE;
    status = write_file(temp, temp_file, TRUE, OVERWRITE, FALSE);

    if (!status) {
	statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
	free(temp);
	return;
    }

    blank_bottombars();
    statusbar(_("Invoking formatter, please wait"));

    /* Set up an argument list to pass to execvp(). */
    if (formatargs == NULL) {
	formatargs = (char **)nmalloc(arglen * sizeof(char *));

	formatargs[0] = strtok(openfile->syntax->formatter, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    formatargs = (char **)nrealloc(formatargs, arglen *	sizeof(char *));
	    formatargs[arglen - 3] = ptr;
	}
	formatargs[arglen - 1] = NULL;
    }
    formatargs[arglen - 2] = temp;

    /* Start a new process for the formatter. */
    if ((pid_format = fork()) == 0) {
	/* Start the formatting program; we are using $PATH. */
	execvp(formatargs[0], formatargs);

	/* Should not be reached, if the formatter is found! */
	exit(1);
    }

    /* If we couldn't fork, get out. */
    if (pid_format < 0) {
	statusbar(_("Could not fork"));
	unlink(temp);
	free(temp);
	return;
    }

#ifndef NANO_TINY
    /* Block SIGWINCHes so the formatter doesn't get any. */
    allow_sigwinch(FALSE);
#endif

    /* Wait for the formatter to finish. */
    wait(&format_status);

    if (!WIFEXITED(format_status) || WEXITSTATUS(format_status) != 0)
	finalstatus = invocation_error(openfile->syntax->formatter);
    else {
	/* Replace the text of the current buffer with the formatted text. */
	replace_buffer(temp);

	/* Restore the cursor position, and mark the file as modified. */
	goto_line_posx(lineno_save, current_x_save);
	openfile->current_y = current_y_save;
	openfile->placewewant = pww_save;
	edit_update(STATIONARY);

	set_modified();

	finalstatus = _("Finished formatting");
    }

    unlink(temp);
    free(temp);

#ifndef NANO_TINY
    /* Unblock SIGWINCHes again. */
    allow_sigwinch(TRUE);
#endif

    statusbar(finalstatus);

    /* If there were error messages, allow the user some time to read them. */
    if (WIFEXITED(format_status) && WEXITSTATUS(format_status) == 2)
	sleep(4);

    /* If there were any messages, clear them off. */
    total_refresh();
}
#endif /* !DISABLE_SPELLER */
#endif /* !DISABLE_COLOR */

#ifndef NANO_TINY
/* Our own version of "wc".  Note that its character counts are in
 * multibyte characters instead of single-byte characters. */
void do_wordlinechar_count(void)
{
    size_t words = 0, chars = 0;
    ssize_t nlines = 0;
    size_t current_x_save = openfile->current_x;
    size_t pww_save = openfile->placewewant;
    filestruct *current_save = openfile->current;
    bool old_mark_set = openfile->mark_set;
    filestruct *top, *bot;
    size_t top_x, bot_x;

    /* If the mark is on, partition the filestruct so that it
     * contains only the marked text, and turn the mark off. */
    if (old_mark_set) {
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, NULL);
	filepart = partition_filestruct(top, top_x, bot, bot_x);
	openfile->mark_set = FALSE;
    }

    /* Start at the top of the file. */
    openfile->current = openfile->fileage;
    openfile->current_x = 0;
    openfile->placewewant = 0;

    /* Keep moving to the next word (counting punctuation characters as
     * part of a word, as "wc -w" does), without updating the screen,
     * until we reach the end of the file, incrementing the total word
     * count whenever we're on a word just before moving. */
    while (openfile->current != openfile->filebot ||
	openfile->current->data[openfile->current_x] != '\0') {
	if (do_next_word(TRUE, FALSE))
	    words++;
    }

    /* Get the total line and character counts, as "wc -l"  and "wc -c"
     * do, but get the latter in multibyte characters. */
    if (old_mark_set) {
	nlines = openfile->filebot->lineno - openfile->fileage->lineno + 1;
	chars = get_totsize(openfile->fileage, openfile->filebot);

	/* Unpartition the filestruct so that it contains all the text
	 * again, and turn the mark back on. */
	unpartition_filestruct(&filepart);
	openfile->mark_set = TRUE;
    } else {
	nlines = openfile->filebot->lineno;
	chars = openfile->totsize;
    }

    /* Restore where we were. */
    openfile->current = current_save;
    openfile->current_x = current_x_save;
    openfile->placewewant = pww_save;

    /* Display the total word, line, and character counts on the statusbar. */
    statusline(HUSH, _("%sWords: %lu  Lines: %ld  Chars: %lu"), old_mark_set ?
		_("In Selection:  ") : "", (unsigned long)words, (long)nlines,
		(unsigned long)chars);
}
#endif /* !NANO_TINY */

/* Get verbatim input. */
void do_verbatim_input(void)
{
    int *kbinput;
    size_t kbinput_len, i;
    char *output;

    /* TRANSLATORS: This is displayed when the next keystroke will be
     * inserted verbatim. */
    statusbar(_("Verbatim Input"));
    reset_cursor();
    curs_set(1);

    /* Read in all the verbatim characters. */
    kbinput = get_verbatim_kbinput(edit, &kbinput_len);

    /* If constant cursor position display is on, make sure the current
     * cursor position will be properly displayed on the statusbar.
     * Otherwise, blank the statusbar. */
    if (ISSET(CONST_UPDATE))
	do_cursorpos(TRUE);
    else {
	blank_statusbar();
	wnoutrefresh(bottomwin);
    }

    /* Display all the verbatim characters at once, not filtering out
     * control characters. */
    output = charalloc(kbinput_len + 1);

    for (i = 0; i < kbinput_len; i++)
	output[i] = (char)kbinput[i];
    output[i] = '\0';

    free(kbinput);

    do_output(output, kbinput_len, TRUE);

    free(output);
}
