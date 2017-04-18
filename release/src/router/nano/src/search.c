/**************************************************************************
 *   search.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2017 Free Software Foundation, Inc.    *
 *   Copyright (C) 2015, 2016, 2017 Benno Schulenberg                     *
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
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

static bool came_full_circle = FALSE;
	/* Have we reached the starting line again while searching? */
#ifndef DISABLE_HISTORIES
static bool history_changed = FALSE;
	/* Have any of the history lists changed? */
#endif
static bool regexp_compiled = FALSE;
	/* Have we compiled any regular expressions? */

/* Compile the given regular expression and store it in search_regexp.
 * Return TRUE if the expression is valid, and FALSE otherwise. */
bool regexp_init(const char *regexp)
{
    int value = regcomp(&search_regexp, fixbounds(regexp),
		NANO_REG_EXTENDED | (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE));

    /* If regex compilation failed, show the error message. */
    if (value != 0) {
	size_t len = regerror(value, &search_regexp, NULL, 0);
	char *str = charalloc(len);

	regerror(value, &search_regexp, str, len);
	statusline(ALERT, _("Bad regex \"%s\": %s"), regexp, str);
	free(str);

	return FALSE;
    }

    regexp_compiled = TRUE;

    return TRUE;
}

/* Decompile the compiled regular expression we used in the last
 * search, if any. */
void regexp_cleanup(void)
{
    if (regexp_compiled) {
	regexp_compiled = FALSE;
	regfree(&search_regexp);
    }
}

/* Indicate on the statusbar that the string at str was not found by the
 * last search. */
void not_found_msg(const char *str)
{
    char *disp;
    int numchars;

    assert(str != NULL);

    disp = display_string(str, 0, (COLS / 2) + 1, FALSE);
    numchars = actual_x(disp, mbstrnlen(disp, COLS / 2));

    statusline(HUSH, _("\"%.*s%s\" not found"), numchars, disp,
		(disp[numchars] == '\0') ? "" : "...");

    free(disp);
}

/* Abort the current search or replace.  Clean up by displaying the main
 * shortcut list, updating the screen if the mark was on before, and
 * decompiling the compiled regular expression we used in the last
 * search, if any. */
void search_replace_abort(void)
{
#ifndef NANO_TINY
    if (openfile->mark_set)
	refresh_needed = TRUE;
#endif
    regexp_cleanup();
}

/* Set up the system variables for a search or replace.  If use_answer
 * is TRUE, only set backupstring to answer.  Return -2 to run the
 * opposite program (search -> replace, replace -> search), return -1 if
 * the search should be canceled (due to Cancel, a blank search string,
 * Go to Line, or a failed regcomp()), return 0 on success, and return 1
 * on rerun calling program.
 *
 * replacing is TRUE if we call from do_replace(), and FALSE if called
 * from do_search(). */
int search_init(bool replacing, bool use_answer)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;
	/* The search string we'll be using. */
    functionptrtype func;

    /* If use_answer is TRUE, set backupstring to answer and get out. */
    if (use_answer) {
	backupstring = mallocstrcpy(backupstring, answer);
	return 0;
    }

    /* We display the search prompt below.  If the user types a partial
     * search string and then Replace or a toggle, we will return to
     * do_search() or do_replace() and be called again.  In that case,
     * we should put the same search string back up. */

    if (*last_search != '\0') {
	char *disp = display_string(last_search, 0, COLS / 3, FALSE);

	buf = charalloc(strlen(disp) + 7);
	/* We use (COLS / 3) here because we need to see more on the line. */
	sprintf(buf, " [%s%s]", disp,
		(strlenpt(last_search) > COLS / 3) ? "..." : "");
	free(disp);
    } else
	buf = mallocstrcpy(NULL, "");

    /* This is now one simple call.  It just does a lot. */
    i = do_prompt(FALSE, FALSE,
		replacing ? MREPLACE : MWHEREIS, backupstring,
#ifndef DISABLE_HISTORIES
		&search_history,
#endif
		/* TRANSLATORS: This is the main search prompt. */
		edit_refresh, "%s%s%s%s%s%s", _("Search"),
		/* TRANSLATORS: The next three modify the search prompt. */
		ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") : "",
		ISSET(USE_REGEXP) ? _(" [Regexp]") : "",
		ISSET(BACKWARDS_SEARCH) ? _(" [Backwards]") : "", replacing ?
#ifndef NANO_TINY
		/* TRANSLATORS: The next two modify the search prompt. */
		openfile->mark_set ? _(" (to replace) in selection") :
#endif
		_(" (to replace)") : "", buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    free(backupstring);
    backupstring = NULL;

    /* If the search was cancelled, or we have a blank answer and
     * nothing was searched for yet during this session, get out. */
    if (i == -1 || (i == -2 && *last_search == '\0')) {
	statusbar(_("Cancelled"));
	return -1;
    }

    /* If Enter was pressed, see what we got. */
    if (i == 0 || i == -2) {
	/* If an answer was given, remember it. */
	if (*answer != '\0') {
	    last_search = mallocstrcpy(last_search, answer);
#ifndef DISABLE_HISTORIES
	    update_history(&search_history, answer);
#endif
	}
	if (ISSET(USE_REGEXP) && !regexp_init(last_search))
	    return -1;
	else
	    return 0;	/* We have a valid string or regex. */
    }

    func = func_from_key(&i);

    if (func == case_sens_void) {
	TOGGLE(CASE_SENSITIVE);
	backupstring = mallocstrcpy(backupstring, answer);
	return 1;
    } else if (func == backwards_void) {
	TOGGLE(BACKWARDS_SEARCH);
	backupstring = mallocstrcpy(backupstring, answer);
	return 1;
    } else if (func == regexp_void) {
	TOGGLE(USE_REGEXP);
	backupstring = mallocstrcpy(backupstring, answer);
	return 1;
    } else if (func == do_replace || func == flip_replace_void) {
	backupstring = mallocstrcpy(backupstring, answer);
	return -2;	/* Call the opposite search function. */
    } else if (func == do_gotolinecolumn_void) {
	do_gotolinecolumn(openfile->current->lineno,
			openfile->placewewant + 1, TRUE, TRUE);
	return 3;
    }

    return -1;
}

/* Look for needle, starting at (current, current_x).  begin is the line
 * where we first started searching, at column begin_x.  Return 1 when we
 * found something, 0 when nothing, and -2 on cancel.  When match_len is
 * not NULL, set it to the length of the found string, if any. */
int findnextstr(const char *needle, bool whole_word_only, bool have_region,
	size_t *match_len, bool skipone, const filestruct *begin, size_t begin_x)
{
    size_t found_len = strlen(needle);
	/* The length of a match -- will be recomputed for a regex. */
    int feedback = 0;
	/* When bigger than zero, show and wipe the "Searching..." message. */
    filestruct *line = openfile->current;
	/* The line that we will search through now. */
    const char *from = line->data + openfile->current_x;
	/* The point in the line from where we start searching. */
    const char *found = NULL;
	/* A pointer to the location of the match, if any. */
    size_t found_x;
	/* The x coordinate of a found occurrence. */
    time_t lastkbcheck = time(NULL);

    enable_nodelay();

    if (begin == NULL)
	came_full_circle = FALSE;

    /* Start searching through the lines, looking for the needle. */
    while (TRUE) {
	/* Glance at the keyboard once every second. */
	if (time(NULL) - lastkbcheck > 0) {
	    int input = parse_kbinput(edit);

	    lastkbcheck = time(NULL);

	    /* Consume all waiting keystrokes until a Cancel. */
	    while (input) {
		if (func_from_key(&input) == do_cancel) {
		    statusbar(_("Cancelled"));
		    disable_nodelay();
		    return -2;
		}
		input = parse_kbinput(NULL);
	    }

	    if (++feedback > 0)
		/* TRANSLATORS: This is shown when searching takes
		 * more than half a second. */
		statusbar(_("Searching..."));
	}

	/* Search for the needle in the current line. */
	if (!skipone)
	    found = strstrwrapper(line->data, needle, from);

	/* Ignore the initial match at the starting position: continue
	 * searching from the next character, or invalidate the match. */
	if (skipone || (!whole_word_only && !came_full_circle &&
				found == begin->data + begin_x)) {
	    skipone = FALSE;
	    if (ISSET(BACKWARDS_SEARCH) && from != line->data) {
		from = line->data + move_mbleft(line->data, from - line->data);
		continue;
	    } else if (!ISSET(BACKWARDS_SEARCH) && *from != '\0') {
		from += move_mbright(from, 0);
		continue;
	    }
	    found = NULL;
	}

	if (found != NULL) {
	    /* When doing a regex search, compute the length of the match. */
	    if (ISSET(USE_REGEXP))
		found_len = regmatches[0].rm_eo - regmatches[0].rm_so;
#ifndef DISABLE_SPELLER
	    /* When we're spell checking, a match should be a separate word;
	     * if it's not, continue looking in the rest of the line. */
	    if (whole_word_only && !is_separate_word(found - line->data,
						found_len, line->data)) {
		from = found + move_mbright(found, 0);
		continue;
	    }
#endif
	    /* The match is valid. */
	    break;
	}

	/* If we're back at the beginning, then there is no needle. */
	if (came_full_circle) {
	    not_found_msg(needle);
	    disable_nodelay();
	    return 0;
	}

	/* Move to the previous or next line in the file. */
	if (ISSET(BACKWARDS_SEARCH))
	    line = line->prev;
	else
	    line = line->next;

	/* If we've reached the start or end of the buffer, wrap around;
	 * but stop when spell-checking or replacing in a region. */
	if (line == NULL) {
	    if (whole_word_only || have_region) {
		disable_nodelay();
		return 0;
	    }

	    if (ISSET(BACKWARDS_SEARCH))
		line = openfile->filebot;
	    else
		line = openfile->fileage;

	    statusbar(_("Search Wrapped"));
	    /* Delay the "Searching..." message for at least two seconds. */
	    feedback = -2;
	}

	/* If we've reached the original starting line, take note. */
	if (line == begin)
	    came_full_circle = TRUE;

	/* Set the starting x to the start or end of the line. */
	from = line->data;
	if (ISSET(BACKWARDS_SEARCH))
	    from += strlen(line->data);
    }

    found_x = found - line->data;

    /* Ensure that the found occurrence is not beyond the starting x. */
    if (came_full_circle && ((!ISSET(BACKWARDS_SEARCH) && found_x > begin_x) ||
			(ISSET(BACKWARDS_SEARCH) && found_x < begin_x))) {
	not_found_msg(needle);
	disable_nodelay();
	return 0;
    }

    disable_nodelay();

    /* Set the current position to point at what we found. */
    openfile->current = line;
    openfile->current_x = found_x;

    /* When requested, pass back the length of the match. */
    if (match_len != NULL)
	*match_len = found_len;

    if (feedback > 0)
	blank_statusbar();

    return 1;
}

/* Ask what to search for and then go looking for it. */
void do_search(void)
{
    int i = search_init(FALSE, FALSE);

    if (i == -1)	/* Cancelled, or some other exit reason. */
	search_replace_abort();
    else if (i == -2)	/* Do a replace instead. */
	do_replace();
    else if (i == 1)	/* Toggled something. */
	do_search();

    if (i == 0)
	go_looking();
}

#ifndef NANO_TINY
/* Search in the backward direction for the next occurrence. */
void do_findprevious(void)
{
    if ISSET(BACKWARDS_SEARCH)
	do_research();
    else {
	SET(BACKWARDS_SEARCH);
	do_research();
	UNSET(BACKWARDS_SEARCH);
    }
}

/* Search in the forward direction for the next occurrence. */
void do_findnext(void)
{
    if ISSET(BACKWARDS_SEARCH) {
	UNSET(BACKWARDS_SEARCH);
	do_research();
	SET(BACKWARDS_SEARCH);
    } else
	do_research();
}
#endif /* !NANO_TINY */

/* Search for the last string without prompting. */
void do_research(void)
{
#ifndef DISABLE_HISTORIES
    /* If nothing was searched for yet during this run of nano, but
     * there is a search history, take the most recent item. */
    if (*last_search == '\0' && searchbot->prev != NULL)
	last_search = mallocstrcpy(last_search, searchbot->prev->data);
#endif

    if (*last_search == '\0') {
	statusbar(_("No current search pattern"));
	return;
    }

    if (ISSET(USE_REGEXP) && !regexp_init(last_search))
	return;

    /* Use the search-menu key bindings, to allow cancelling. */
    currmenu = MWHEREIS;

    go_looking();
}

/* Search for the global string 'last_search'.  Inform the user when
 * the string occurs only once. */
void go_looking(void)
{
    filestruct *was_current = openfile->current;
    size_t was_current_x = openfile->current_x;
    int didfind;
#ifdef DEBUG
    clock_t start = clock();
#endif

    came_full_circle = FALSE;

    didfind = findnextstr(last_search, FALSE, FALSE, NULL, FALSE,
				openfile->current, openfile->current_x);

    /* If we found something, and we're back at the exact same spot
     * where we started searching, then this is the only occurrence. */
    if (didfind == 1 && openfile->current == was_current &&
		openfile->current_x == was_current_x)
	statusbar(_("This is the only occurrence"));

#ifdef DEBUG
    statusline(HUSH, "Took: %.2f", (double)(clock() - start) / CLOCKS_PER_SEC);
#endif

    edit_redraw(was_current);
    search_replace_abort();
}

/* Calculate the size of the replacement text, taking possible
 * subexpressions \1 to \9 into account.  Return the replacement
 * text in the passed string only when create is TRUE. */
int replace_regexp(char *string, bool create)
{
    const char *c = answer;
    size_t replacement_size = 0;

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */
    while (*c != '\0') {
	int num = (*(c + 1) - '0');

	if (*c != '\\' || num < 1 || num > 9 || num > search_regexp.re_nsub) {
	    if (create)
		*string++ = *c;
	    c++;
	    replacement_size++;
	} else {
	    size_t i = regmatches[num].rm_eo - regmatches[num].rm_so;

	    /* Skip over the replacement expression. */
	    c += 2;

	    /* But add the length of the subexpression to new_size. */
	    replacement_size += i;

	    /* And if create is TRUE, append the result of the
	     * subexpression match to the new line. */
	    if (create) {
		strncpy(string, openfile->current->data +
					regmatches[num].rm_so, i);
		string += i;
	    }
	}
    }

    if (create)
	*string = '\0';

    return replacement_size;
}

/* Return a copy of the current line with one needle replaced. */
char *replace_line(const char *needle)
{
    char *copy;
    size_t match_len;
    size_t new_line_size = strlen(openfile->current->data) + 1;

    /* First adjust the size of the new line for the change. */
    if (ISSET(USE_REGEXP)) {
	match_len = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size += replace_regexp(NULL, FALSE) - match_len;
    } else {
	match_len = strlen(needle);
	new_line_size += strlen(answer) - match_len;
    }

    /* Create the buffer. */
    copy = charalloc(new_line_size);

    /* Copy the head of the original line. */
    strncpy(copy, openfile->current->data, openfile->current_x);

    /* Add the replacement text. */
    if (ISSET(USE_REGEXP))
	replace_regexp(copy + openfile->current_x, TRUE);
    else
	strcpy(copy + openfile->current_x, answer);

    assert(openfile->current_x + match_len <= strlen(openfile->current->data));

    /* Copy the tail of the original line. */
    strcat(copy, openfile->current->data + openfile->current_x + match_len);

    return copy;
}

/* Step through each occurrence of the search string and prompt the user
 * before replacing it.  We seek for needle, and replace it with answer.
 * The parameters real_current and real_current_x are needed in order to
 * allow the cursor position to be updated when a word before the cursor
 * is replaced by a shorter word.  Return -1 if needle isn't found, -2 if
 * the seeking is aborted, else the number of replacements performed. */
ssize_t do_replace_loop(const char *needle, bool whole_word_only,
	const filestruct *real_current, size_t *real_current_x)
{
    ssize_t numreplaced = -1;
    size_t match_len;
    bool replaceall = FALSE;
    bool skipone = FALSE;
    bool mark_was_set = FALSE;
#ifndef NANO_TINY
    filestruct *top, *bot;
    size_t top_x, bot_x;
    bool right_side_up = FALSE;
	/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
	 * FALSE if (current, current_x) is. */

    mark_was_set = openfile->mark_set;

    /* If the mark is on, frame the region, and turn the mark off. */
    if (mark_was_set) {
	mark_order((const filestruct **)&top, &top_x,
			(const filestruct **)&bot, &bot_x, &right_side_up);
	openfile->mark_set = FALSE;

	/* Start either at the top or the bottom of the marked region. */
	if (!ISSET(BACKWARDS_SEARCH)) {
	    openfile->current = top;
	    openfile->current_x = top_x;
	} else {
	    openfile->current = bot;
	    openfile->current_x = bot_x;
	}
    }
#endif /* !NANO_TINY */

    came_full_circle = FALSE;

    while (TRUE) {
	int i = 0;
	int result = findnextstr(needle, whole_word_only, mark_was_set,
			&match_len, skipone, real_current, *real_current_x);

	/* If nothing more was found, or the user aborted, stop looping. */
	if (result < 1) {
	    if (result < 0)
		numreplaced = -2;  /* It's a Cancel instead of Not found. */
	    break;
	}

#ifndef NANO_TINY
	/* An occurrence outside of the marked region means we're done. */
	if (mark_was_set && (openfile->current->lineno > bot->lineno ||
				openfile->current->lineno < top->lineno ||
				(openfile->current == bot &&
				openfile->current_x + match_len > bot_x) ||
				(openfile->current == top &&
				openfile->current_x < top_x)))
	    break;
#endif

	/* Indicate that we found the search string. */
	if (numreplaced == -1)
	    numreplaced = 0;

	if (!replaceall) {
	    size_t xpt = xplustabs();
	    char *exp_word = display_string(openfile->current->data,
			xpt, strnlenpt(openfile->current->data,
			openfile->current_x + match_len) - xpt, FALSE);

	    /* Refresh the edit window, scrolling it if necessary. */
	    edit_refresh();

	    /* Don't show cursor, to not distract from highlighted match. */
	    curs_set(0);

	    spotlight(TRUE, exp_word);

	    /* TRANSLATORS: This is a prompt. */
	    i = do_yesno_prompt(TRUE, _("Replace this instance?"));

	    spotlight(FALSE, exp_word);

	    free(exp_word);

	    if (i == -1)  /* The replacing was cancelled. */
		break;
	    else if (i == 2)
		replaceall = TRUE;

	    /* When "No" or moving backwards, the search routine should
	     * first move one character further before continuing. */
	    skipone = (i == 0 || ISSET(BACKWARDS_SEARCH));
	}

	if (i == 1 || replaceall) {  /* Yes, replace it. */
	    char *copy;
	    size_t length_change;

#ifndef NANO_TINY
	    add_undo(REPLACE);
#endif
	    copy = replace_line(needle);

	    length_change = strlen(copy) - strlen(openfile->current->data);

#ifndef NANO_TINY
	    /* If the mark was on and it was located after the cursor,
	     * then adjust its x position for any text length changes. */
	    if (mark_was_set && !right_side_up) {
		if (openfile->current == openfile->mark_begin &&
			openfile->mark_begin_x > openfile->current_x) {
		    if (openfile->mark_begin_x < openfile->current_x + match_len)
			openfile->mark_begin_x = openfile->current_x;
		    else
			openfile->mark_begin_x += length_change;
		    bot_x = openfile->mark_begin_x;
		}
	    }

	    /* If the mark was not on or it was before the cursor, then
	     * adjust the cursor's x position for any text length changes. */
	    if (!mark_was_set || right_side_up) {
#endif
		if (openfile->current == real_current &&
			openfile->current_x < *real_current_x) {
		    if (*real_current_x < openfile->current_x + match_len)
			*real_current_x = openfile->current_x + match_len;
		    *real_current_x += length_change;
#ifndef NANO_TINY
		    bot_x = *real_current_x;
		}
#endif
	    }

	    /* Don't find the same zero-length or BOL match again. */
	    if (match_len == 0 || (*needle == '^' && ISSET(USE_REGEXP)))
		skipone = TRUE;

	    /* When moving forward, put the cursor just after the replacement
	     * text, so that searching will continue there. */
	    if (!ISSET(BACKWARDS_SEARCH))
		openfile->current_x += match_len + length_change;

	    /* Update the file size, and put the changed line into place. */
	    openfile->totsize += mbstrlen(copy) - mbstrlen(openfile->current->data);
	    free(openfile->current->data);
	    openfile->current->data = copy;

	    if (!replaceall) {
#ifndef DISABLE_COLOR
		/* When doing syntax coloring, the replacement might require
		 * a change of colors, so refresh the whole edit window. */
		if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX))
		    edit_refresh();
		else
#endif
		    update_line(openfile->current, openfile->current_x);
	    }

	    set_modified();
	    as_an_at = TRUE;
	    numreplaced++;
	}
    }

    if (numreplaced == -1)
	not_found_msg(needle);
#ifndef DISABLE_COLOR
    else if (numreplaced > 0)
	refresh_needed = TRUE;
#endif

#ifndef NANO_TINY
    if (mark_was_set)
	openfile->mark_set = TRUE;
#endif

    /* If the NO_NEWLINES flag isn't set, and text has been added to the
     * magicline, make a new magicline. */
    if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0')
	new_magicline();

    return numreplaced;
}

/* Replace a string. */
void do_replace(void)
{
    filestruct *edittop_save, *begin;
    size_t firstcolumn_save, begin_x;
    ssize_t numreplaced;
    int i;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	return;
    }

    i = search_init(TRUE, FALSE);

    if (i == -1)	/* Cancelled, or some other exit reason. */
	search_replace_abort();
    else if (i == -2)	/* Do a search instead. */
	do_search();
    else if (i == 1)	/* Toggled something. */
	do_replace();

    if (i != 0)
	return;

    i = do_prompt(FALSE, FALSE, MREPLACEWITH, NULL,
#ifndef DISABLE_HISTORIES
		&replace_history,
#endif
		/* TRANSLATORS: This is a prompt. */
		edit_refresh, _("Replace with"));

#ifndef DISABLE_HISTORIES
    /* If the replace string is not "", add it to the replace history list. */
    if (i == 0)
	update_history(&replace_history, answer);
#endif

    /* When cancelled, or when a function was run, get out. */
    if (i == -1 || i > 0) {
	if (i == -1)
	    statusbar(_("Cancelled"));
	search_replace_abort();
	return;
    }

    /* Save where we are. */
    edittop_save = openfile->edittop;
    firstcolumn_save = openfile->firstcolumn;
    begin = openfile->current;
    begin_x = openfile->current_x;

    numreplaced = do_replace_loop(last_search, FALSE, begin, &begin_x);

    /* Restore where we were. */
    openfile->edittop = edittop_save;
    openfile->firstcolumn = firstcolumn_save;
    openfile->current = begin;
    openfile->current_x = begin_x;
    refresh_needed = TRUE;

    if (numreplaced >= 0)
	statusline(HUSH, P_("Replaced %lu occurrence",
		"Replaced %lu occurrences", (unsigned long)numreplaced),
		(unsigned long)numreplaced);

    search_replace_abort();
}

/* Go to the specified line and x position. */
void goto_line_posx(ssize_t line, size_t pos_x)
{
    for (openfile->current = openfile->fileage; line > 1 &&
		openfile->current != openfile->filebot; line--)
	openfile->current = openfile->current->next;

    openfile->current_x = pos_x;
    openfile->placewewant = xplustabs();

    refresh_needed = TRUE;
}

/* Go to the specified line and column, or ask for them if interactive
 * is TRUE.  In the latter case also update the screen afterwards.
 * Note that both the line and column number should be one-based. */
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer,
	bool interactive)
{
    if (interactive) {
	functionptrtype func;

	/* Ask for the line and column. */
	int i = do_prompt(FALSE, FALSE, MGOTOLINE,
		use_answer ? answer : NULL,
#ifndef DISABLE_HISTORIES
		NULL,
#endif
		/* TRANSLATORS: This is a prompt. */
		edit_refresh, _("Enter line number, column number"));

	/* If the user cancelled or gave a blank answer, get out. */
	if (i < 0) {
	    statusbar(_("Cancelled"));
	    return;
	}

	func = func_from_key(&i);

	if (func == gototext_void) {
	    /* Retain what the user typed so far and switch to searching. */
	    search_init(TRUE, TRUE);
	    do_search();
	}

	/* If a function was executed, we're done here. */
	if (i > 0)
	    return;

	/* Try to extract one or two numbers from the user's response. */
	if (!parse_line_column(answer, &line, &column)) {
	    statusline(ALERT, _("Invalid line or column number"));
	    return;
	}
    } else {
	if (line < 1)
	    line = openfile->current->lineno;

	if (column < 1)
	    column = openfile->placewewant + 1;
    }

    /* Take a negative line number to mean: from the end of the file. */
    if (line < 0)
	line = openfile->filebot->lineno + line + 1;
    if (line < 1)
	line = 1;

    /* Iterate to the requested line. */
    for (openfile->current = openfile->fileage; line > 1 &&
		openfile->current != openfile->filebot; line--)
	openfile->current = openfile->current->next;

    /* Take a negative column number to mean: from the end of the line. */
    if (column < 0)
	column = strlenpt(openfile->current->data) + column + 2;
    if (column < 1)
	column = 1;

    /* Set the x position that corresponds to the requested column. */
    openfile->current_x = actual_x(openfile->current->data, column - 1);
    openfile->placewewant = column - 1;

    /* When the position was manually given, center the target line. */
    if (interactive) {
	adjust_viewport(CENTERING);
	refresh_needed = TRUE;
    } else {
	int rows_from_tail;

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
	    filestruct *line = openfile->current;
	    size_t leftedge = (xplustabs() / editwincols) * editwincols;

	    rows_from_tail = (editwinrows / 2) -
			go_forward_chunks(editwinrows / 2, &line, &leftedge);
	} else
#endif
	    rows_from_tail = openfile->filebot->lineno -
				openfile->current->lineno;

	/* If the target line is close to the tail of the file, put the last
	 * line or chunk on the bottom line of the screen; otherwise, just
	 * center the target line. */
	if (rows_from_tail < editwinrows / 2) {
	    openfile->current_y = editwinrows - 1 - rows_from_tail;
	    adjust_viewport(STATIONARY);
	} else
	    adjust_viewport(CENTERING);
    }
}

/* Go to the specified line and column, asking for them beforehand. */
void do_gotolinecolumn_void(void)
{
    do_gotolinecolumn(openfile->current->lineno,
	openfile->placewewant + 1, FALSE, TRUE);
}

#ifndef NANO_TINY
/* Search for a match to one of the two characters in bracket_set.  If
 * reverse is TRUE, search backwards for the leftmost bracket.
 * Otherwise, search forwards for the rightmost bracket.  Return TRUE if
 * we found a match, and FALSE otherwise. */
bool find_bracket_match(bool reverse, const char *bracket_set)
{
    filestruct *fileptr = openfile->current;
    const char *rev_start = NULL, *found = NULL;

    assert(mbstrlen(bracket_set) == 2);

    /* rev_start might end up 1 character before the start or after the
     * end of the line.  This won't be a problem because we'll skip over
     * it below in that case, and rev_start will be properly set when
     * the search continues on the previous or next line. */
    if (reverse)
	rev_start = fileptr->data + (openfile->current_x - 1);
    else
	rev_start = fileptr->data + (openfile->current_x + 1);

    /* Look for either of the two characters in bracket_set.  rev_start
     * can be 1 character before the start or after the end of the line.
     * In either case, just act as though no match is found. */
    while (TRUE) {
	if ((rev_start > fileptr->data && *(rev_start - 1) == '\0') ||
			rev_start < fileptr->data)
	    found = NULL;
	else if (reverse)
	    found = mbrevstrpbrk(fileptr->data, bracket_set, rev_start);
	else
	    found = mbstrpbrk(rev_start, bracket_set);

	if (found)
	    break;

	if (reverse)
	    fileptr = fileptr->prev;
	else
	    fileptr = fileptr->next;

	/* If we've reached the start or end of the buffer, get out. */
	if (fileptr == NULL)
	    return FALSE;

	rev_start = fileptr->data;
	if (reverse)
	    rev_start += strlen(fileptr->data);
    }

    /* Set the current position to the found matching bracket. */
    openfile->current = fileptr;
    openfile->current_x = found - fileptr->data;

    return TRUE;
}

/* Search for a match to the bracket at the current cursor position, if
 * there is one. */
void do_find_bracket(void)
{
    filestruct *current_save;
    size_t current_x_save;
    const char *ch;
	/* The location in matchbrackets of the bracket at the current
	 * cursor position. */
    int ch_len;
	/* The length of ch in bytes. */
    const char *wanted_ch;
	/* The location in matchbrackets of the bracket complementing
	 * the bracket at the current cursor position. */
    int wanted_ch_len;
	/* The length of wanted_ch in bytes. */
    char bracket_set[MAXCHARLEN * 2 + 1];
	/* The pair of characters in ch and wanted_ch. */
    size_t i;
	/* Generic loop variable. */
    size_t matchhalf;
	/* The number of single-byte characters in one half of
	 * matchbrackets. */
    size_t mbmatchhalf;
	/* The number of multibyte characters in one half of
	 * matchbrackets. */
    size_t count = 1;
	/* The initial bracket count. */
    bool reverse;
	/* The direction we search. */

    assert(mbstrlen(matchbrackets) % 2 == 0);

    ch = openfile->current->data + openfile->current_x;

    if ((ch = mbstrchr(matchbrackets, ch)) == NULL) {
	statusbar(_("Not a bracket"));
	return;
    }

    /* Save where we are. */
    current_save = openfile->current;
    current_x_save = openfile->current_x;

    /* If we're on an opening bracket, which must be in the first half
     * of matchbrackets, we want to search forwards for a closing
     * bracket.  If we're on a closing bracket, which must be in the
     * second half of matchbrackets, we want to search backwards for an
     * opening bracket. */
    matchhalf = 0;
    mbmatchhalf = mbstrlen(matchbrackets) / 2;

    for (i = 0; i < mbmatchhalf; i++)
	matchhalf += parse_mbchar(matchbrackets + matchhalf, NULL, NULL);

    reverse = ((ch - matchbrackets) >= matchhalf);

    /* If we're on an opening bracket, set wanted_ch to the character
     * that's matchhalf characters after ch.  If we're on a closing
     * bracket, set wanted_ch to the character that's matchhalf
     * characters before ch. */
    wanted_ch = ch;

    while (mbmatchhalf > 0) {
	if (reverse)
	    wanted_ch = matchbrackets + move_mbleft(matchbrackets,
				wanted_ch - matchbrackets);
	else
	    wanted_ch += move_mbright(wanted_ch, 0);

	mbmatchhalf--;
    }

    ch_len = parse_mbchar(ch, NULL, NULL);
    wanted_ch_len = parse_mbchar(wanted_ch, NULL, NULL);

    /* Fill bracket_set in with the values of ch and wanted_ch. */
    strncpy(bracket_set, ch, ch_len);
    strncpy(bracket_set + ch_len, wanted_ch, wanted_ch_len);
    bracket_set[ch_len + wanted_ch_len] = '\0';

    while (TRUE) {
	if (find_bracket_match(reverse, bracket_set)) {
	    /* If we found an identical bracket, increment count.  If we
	     * found a complementary bracket, decrement it. */
	    count += (strncmp(openfile->current->data + openfile->current_x,
				 ch, ch_len) == 0) ? 1 : -1;

	    /* If count is zero, we've found a matching bracket.  Update
	     * the screen and get out. */
	    if (count == 0) {
		focusing = FALSE;
		edit_redraw(current_save);
		break;
	    }
	} else {
	    /* We didn't find either an opening or closing bracket.
	     * Indicate this, restore where we were, and get out. */
	    statusbar(_("No matching bracket"));
	    openfile->current = current_save;
	    openfile->current_x = current_x_save;
	    break;
	}
    }
}
#endif /* !NANO_TINY */

#ifndef DISABLE_HISTORIES
/* Indicate whether any of the history lists have changed. */
bool history_has_changed(void)
{
    return history_changed;
}

/* Initialize the search and replace history lists. */
void history_init(void)
{
    search_history = make_new_node(NULL);
    search_history->data = mallocstrcpy(NULL, "");
    searchage = search_history;
    searchbot = search_history;

    replace_history = make_new_node(NULL);
    replace_history->data = mallocstrcpy(NULL, "");
    replaceage = replace_history;
    replacebot = replace_history;
}

/* Set the current position in the history list h to the bottom. */
void history_reset(const filestruct *h)
{
    if (h == search_history)
	search_history = searchbot;
    else if (h == replace_history)
	replace_history = replacebot;
}

/* Return the first node containing the first len characters of the
 * string s in the history list, starting at h_start and ending at
 * h_end, or NULL if there isn't one. */
filestruct *find_history(const filestruct *h_start, const filestruct
	*h_end, const char *s, size_t len)
{
    const filestruct *p;

    for (p = h_start; p != h_end->prev && p != NULL; p = p->prev) {
	if (strncmp(s, p->data, len) == 0)
	    return (filestruct *)p;
    }

    return NULL;
}

/* Update a history list (the one in which h is the current position)
 * with a fresh string s.  That is: add s, or move it to the end. */
void update_history(filestruct **h, const char *s)
{
    filestruct **hage = NULL, **hbot = NULL, *thesame;

    assert(h != NULL && s != NULL);

    if (*h == search_history) {
	hage = &searchage;
	hbot = &searchbot;
    } else if (*h == replace_history) {
	hage = &replaceage;
	hbot = &replacebot;
    }

    assert(hage != NULL && hbot != NULL);

    /* See if the string is already in the history. */
    thesame = find_history(*hbot, *hage, s, HIGHEST_POSITIVE);

    /* If an identical string was found, delete that item. */
    if (thesame != NULL) {
	filestruct *after = thesame->next;

	/* If the string is at the head of the list, move the head. */
	if (thesame == *hage)
	    *hage = after;

	unlink_node(thesame);
	renumber(after);
    }

    /* If the history is full, delete the oldest item (the one at the
     * head of the list), to make room for a new item at the end. */
    if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
	filestruct *oldest = *hage;

	*hage = (*hage)->next;
	unlink_node(oldest);
	renumber(*hage);
    }

    /* Store the fresh string in the last item, then create a new item. */
    (*hbot)->data = mallocstrcpy((*hbot)->data, s);
    splice_node(*hbot, make_new_node(*hbot));
    *hbot = (*hbot)->next;
    (*hbot)->data = mallocstrcpy(NULL, "");

    /* Indicate that the history needs to be saved on exit. */
    history_changed = TRUE;

    /* Set the current position in the list to the bottom. */
    *h = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->prev == NULL)
	return NULL;

    *h = (*h)->prev;

    return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->next == NULL)
	return NULL;

    *h = (*h)->next;

    return (*h)->data;
}

/* More placeholders. */
void get_history_newer_void(void)
{
    ;
}
void get_history_older_void(void)
{
    ;
}

#ifndef DISABLE_TABCOMP
/* Move h to the next string that's a tab completion of the string s,
 * looking at only the first len characters of s, and return that
 * string.  If there isn't one, or if len is 0, don't move h and return
 * s. */
char *get_history_completion(filestruct **h, char *s, size_t len)
{
    assert(s != NULL);

    if (len > 0) {
	filestruct *hage = NULL, *hbot = NULL, *p;

	assert(h != NULL);

	if (*h == search_history) {
	    hage = searchage;
	    hbot = searchbot;
	} else if (*h == replace_history) {
	    hage = replaceage;
	    hbot = replacebot;
	}

	assert(hage != NULL && hbot != NULL);

	/* Search the history list from the current position to the top
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history((*h)->prev, hage, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->prev, hage, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}

	/* Search the history list from the bottom to the current position
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history(hbot, *h, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->prev, *h, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}
    }

    /* If we're here, we didn't find a match, we didn't find an inexact
     * match, or len is 0.  Return s. */
    return (char *)s;
}
#endif /* !DISABLE_TABCOMP */
#endif /* !DISABLE_HISTORIES */
