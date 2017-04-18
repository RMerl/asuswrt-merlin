/**************************************************************************
 *   color.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2017 Free Software Foundation, Inc.    *
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#ifndef DISABLE_COLOR

/* Initialize the colors for nano's interface, and assign pair numbers
 * for the colors in each syntax. */
void set_colorpairs(void)
{
    const syntaxtype *sint;
    bool using_defaults = FALSE;
    short foreground, background;
    size_t i;

    /* Tell ncurses to enable colors. */
    start_color();

#ifdef HAVE_USE_DEFAULT_COLORS
    /* Allow using the default colors, if available. */
    using_defaults = (use_default_colors() != ERR);
#endif

    /* Initialize the color pairs for nano's interface elements. */
    for (i = 0; i < NUMBER_OF_ELEMENTS; i++) {
	bool bright = FALSE;

	if (parse_color_names(specified_color_combo[i],
			&foreground, &background, &bright)) {
	    if (foreground == -1 && !using_defaults)
		foreground = COLOR_WHITE;
	    if (background == -1 && !using_defaults)
		background = COLOR_BLACK;
	    init_pair(i + 1, foreground, background);
	    interface_color_pair[i] =
			COLOR_PAIR(i + 1) | (bright ? A_BOLD : A_NORMAL);
	}
	else {
	    if (i != FUNCTION_TAG)
		interface_color_pair[i] = hilite_attribute;
	    else
		interface_color_pair[i] = A_NORMAL;
	}

	free(specified_color_combo[i]);
	specified_color_combo[i] = NULL;
    }

    /* For each syntax, go through its list of colors and assign each
     * its pair number, giving identical color pairs the same number. */
    for (sint = syntaxes; sint != NULL; sint = sint->next) {
	colortype *ink;
	int new_number = NUMBER_OF_ELEMENTS + 1;

	for (ink = sint->color; ink != NULL; ink = ink->next) {
	    const colortype *beforenow = sint->color;

	    while (beforenow != ink && (beforenow->fg != ink->fg ||
					beforenow->bg != ink->bg ||
					beforenow->bright != ink->bright))
		beforenow = beforenow->next;

	    if (beforenow != ink)
		ink->pairnum = beforenow->pairnum;
	    else
		ink->pairnum = new_number++;

	    ink->attributes = COLOR_PAIR(ink->pairnum) |
				(ink->bright ? A_BOLD : A_NORMAL);
	}
    }
}

/* Initialize the color information. */
void color_init(void)
{
    const colortype *ink;
    bool using_defaults = FALSE;
    short foreground, background;

    /* If the terminal is not capable of colors, forget it. */
    if (!has_colors())
	return;

#ifdef HAVE_USE_DEFAULT_COLORS
    /* Allow using the default colors, if available. */
    using_defaults = (use_default_colors() != ERR);
#endif

    /* For each coloring expression, initialize the color pair. */
    for (ink = openfile->colorstrings; ink != NULL; ink = ink->next) {
	foreground = ink->fg;
	background = ink->bg;

	if (foreground == -1 && !using_defaults)
	    foreground = COLOR_WHITE;

	if (background == -1 && !using_defaults)
	    background = COLOR_BLACK;

	init_pair(ink->pairnum, foreground, background);
#ifdef DEBUG
	fprintf(stderr, "init_pair(): fg = %hd, bg = %hd\n", foreground, background);
#endif
    }
}

/* Try to match the given shibboleth string with one of the regexes in
 * the list starting at head.  Return TRUE upon success. */
bool found_in_list(regexlisttype *head, const char *shibboleth)
{
    regexlisttype *item;
    regex_t rgx;

    for (item = head; item != NULL; item = item->next) {
	regcomp(&rgx, fixbounds(item->full_regex), NANO_REG_EXTENDED);

	if (regexec(&rgx, shibboleth, 0, NULL, 0) == 0) {
	    regfree(&rgx);
	    return TRUE;
	}

	regfree(&rgx);
    }

    return FALSE;
}

/* Update the color information based on the current filename and content. */
void color_update(void)
{
    syntaxtype *sint = NULL;
    colortype *ink;

    /* If the rcfiles were not read, or contained no syntaxes, get out. */
    if (syntaxes == NULL)
	return;

    /* If we specified a syntax-override string, use it. */
    if (syntaxstr != NULL) {
	/* An override of "none" is like having no syntax at all. */
	if (strcmp(syntaxstr, "none") == 0)
	    return;

	for (sint = syntaxes; sint != NULL; sint = sint->next) {
	    if (strcmp(sint->name, syntaxstr) == 0)
		break;
	}

	if (sint == NULL)
	    statusline(ALERT, _("Unknown syntax name: %s"), syntaxstr);
    }

    /* If no syntax-override string was specified, or it didn't match,
     * try finding a syntax based on the filename (extension). */
    if (sint == NULL) {
	char *reserved = charalloc(PATH_MAX + 1);
	char *currentdir = getcwd(reserved, PATH_MAX + 1);
	char *joinednames = charalloc(PATH_MAX + 1);
	char *fullname = NULL;

	if (currentdir == NULL)
	    free(reserved);
	else {
	    /* Concatenate the current working directory with the
	     * specified filename, and canonicalize the result. */
	    sprintf(joinednames, "%s/%s", currentdir, openfile->filename);
	    fullname = get_full_path(joinednames);
	    free(currentdir);
	}

	if (fullname == NULL)
	    fullname = mallocstrcpy(fullname, openfile->filename);

	for (sint = syntaxes; sint != NULL; sint = sint->next) {
	    if (found_in_list(sint->extensions, fullname))
		break;
	}

	free(joinednames);
	free(fullname);
    }

    /* If the filename didn't match anything, try the first line. */
    if (sint == NULL) {
#ifdef DEBUG
	fprintf(stderr, "No result from file extension, trying headerline...\n");
#endif
	for (sint = syntaxes; sint != NULL; sint = sint->next) {
	    if (found_in_list(sint->headers, openfile->fileage->data))
		break;
	}
    }

#ifdef HAVE_LIBMAGIC
    /* If we still don't have an answer, try using magic. */
    if (sint == NULL) {
	struct stat fileinfo;
	magic_t cookie = NULL;
	const char *magicstring = NULL;
#ifdef DEBUG
	fprintf(stderr, "No result from headerline either, trying libmagic...\n");
#endif
	if (stat(openfile->filename, &fileinfo) == 0) {
	    /* Open the magic database and get a diagnosis of the file. */
	    cookie = magic_open(MAGIC_SYMLINK |
#ifdef DEBUG
				    MAGIC_DEBUG | MAGIC_CHECK |
#endif
				    MAGIC_ERROR);
	    if (cookie == NULL || magic_load(cookie, NULL) < 0)
		statusline(ALERT, _("magic_load() failed: %s"), strerror(errno));
	    else {
		magicstring = magic_file(cookie, openfile->filename);
		if (magicstring == NULL)
		    statusline(ALERT, _("magic_file(%s) failed: %s"),
				openfile->filename, magic_error(cookie));
#ifdef DEBUG
		fprintf(stderr, "Returned magic string is: %s\n", magicstring);
#endif
	    }
	}

	/* Now try and find a syntax that matches the magic string. */
	if (magicstring != NULL) {
	    for (sint = syntaxes; sint != NULL; sint = sint->next) {
		if (found_in_list(sint->magics, magicstring))
		    break;
	    }
	}

	if (stat(openfile->filename, &fileinfo) == 0)
	    magic_close(cookie);
    }
#endif /* HAVE_LIBMAGIC */

    /* If nothing at all matched, see if there is a default syntax. */
    if (sint == NULL) {
	for (sint = syntaxes; sint != NULL; sint = sint->next) {
	    if (strcmp(sint->name, "default") == 0)
		break;
	}
    }

    openfile->syntax = sint;
    openfile->colorstrings = (sint == NULL ? NULL : sint->color);

    /* If a syntax was found, compile its specified regexes (which have
     * already been checked for validity when they were read in). */
    for (ink = openfile->colorstrings; ink != NULL; ink = ink->next) {
	if (ink->start == NULL) {
	    ink->start = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(ink->start, fixbounds(ink->start_regex), ink->rex_flags);
	}

	if (ink->end_regex != NULL && ink->end == NULL) {
	    ink->end = (regex_t *)nmalloc(sizeof(regex_t));
	    regcomp(ink->end, fixbounds(ink->end_regex), ink->rex_flags);
	}
    }
}

/* Determine whether the matches of multiline regexes are still the same,
 * and if not, schedule a screen refresh, so things will be repainted. */
void check_the_multis(filestruct *line)
{
    const colortype *ink;
    bool astart, anend;
    regmatch_t startmatch, endmatch;

    /* If there is no syntax or no multiline regex, there is nothing to do. */
    if (openfile->syntax == NULL || openfile->syntax->nmultis == 0)
	return;

    for (ink = openfile->colorstrings; ink != NULL; ink = ink->next) {
	/* If it's not a multiline regex, skip. */
	if (ink->end == NULL)
	    continue;

	alloc_multidata_if_needed(line);

	astart = (regexec(ink->start, line->data, 1, &startmatch, 0) == 0);
	anend = (regexec(ink->end, line->data, 1, &endmatch, 0) == 0);

	/* Check whether the multidata still matches the current situation. */
	if (line->multidata[ink->id] == CNONE ||
			line->multidata[ink->id] == CWHOLELINE) {
	    if (!astart && !anend)
		continue;
	} else if (line->multidata[ink->id] == CSTARTENDHERE) {
	    if (astart && anend && startmatch.rm_so < endmatch.rm_so)
		continue;
	} else if (line->multidata[ink->id] == CBEGINBEFORE) {
	    if (!astart && anend)
		continue;
	} else if (line->multidata[ink->id] == CENDAFTER) {
	    if (astart && !anend)
		continue;
	}

	/* There is a mismatch, so something changed: repaint. */
	refresh_needed = TRUE;
	return;
    }
}

/* Allocate (for one line) the cache space for multiline color regexes. */
void alloc_multidata_if_needed(filestruct *fileptr)
{
    int i;

    if (fileptr->multidata == NULL) {
	fileptr->multidata = (short *)nmalloc(openfile->syntax->nmultis * sizeof(short));

	for (i = 0; i < openfile->syntax->nmultis; i++)
	    fileptr->multidata[i] = -1;
    }
}

/* Precalculate the multi-line start and end regex info so we can
 * speed up rendering (with any hope at all...). */
void precalc_multicolorinfo(void)
{
    const colortype *ink;
    regmatch_t startmatch, endmatch;
    filestruct *line, *tailline;

    if (openfile->colorstrings == NULL || ISSET(NO_COLOR_SYNTAX))
	return;

#ifdef DEBUG
    fprintf(stderr, "Precalculating the multiline color info...\n");
#endif

    for (ink = openfile->colorstrings; ink != NULL; ink = ink->next) {
	/* If this is not a multi-line regex, skip it. */
	if (ink->end == NULL)
	    continue;

	for (line = openfile->fileage; line != NULL; line = line->next) {
	    int index = 0;

	    alloc_multidata_if_needed(line);
	    /* Assume nothing applies until proven otherwise below. */
	    line->multidata[ink->id] = CNONE;

	    /* For an unpaired start match, mark all remaining lines. */
	    if (line->prev && line->prev->multidata[ink->id] == CWOULDBE) {
		line->multidata[ink->id] = CWOULDBE;
		continue;
	    }

	    /* When the line contains a start match, look for an end, and if
	     * found, mark all the lines that are affected. */
	    while (regexec(ink->start, line->data + index, 1,
			&startmatch, (index == 0) ? 0 : REG_NOTBOL) == 0) {
		/* Begin looking for an end match after the start match. */
		index += startmatch.rm_eo;

		/* If there is an end match on this line, mark the line, but
		 * continue looking for other starts after it. */
		if (regexec(ink->end, line->data + index, 1,
			&endmatch, (index == 0) ? 0 : REG_NOTBOL) == 0) {
		    line->multidata[ink->id] = CSTARTENDHERE;
		    index += endmatch.rm_eo;
		    /* If both start and end are mere anchors, step ahead. */
		    if (startmatch.rm_so == startmatch.rm_eo &&
				endmatch.rm_so == endmatch.rm_eo) {
			/* When at end-of-line, we're done. */
			if (line->data[index] == '\0')
			    break;
			index = move_mbright(line->data, index);
		    }
		    continue;
		}

		/* Look for an end match on later lines. */
		tailline = line->next;

		while (tailline != NULL) {
		    if (regexec(ink->end, tailline->data, 1, &endmatch, 0) == 0)
			break;
		    tailline = tailline->next;
		}

		if (tailline == NULL) {
		    line->multidata[ink->id] = CWOULDBE;
		    break;
		}

		/* We found it, we found it, la la la la la.  Mark all
		 * the lines in between and the end properly. */
		line->multidata[ink->id] = CENDAFTER;

		for (line = line->next; line != tailline; line = line->next) {
		    alloc_multidata_if_needed(line);
		    line->multidata[ink->id] = CWHOLELINE;
		}

		alloc_multidata_if_needed(tailline);
		tailline->multidata[ink->id] = CBEGINBEFORE;

		/* Begin looking for a new start after the end match. */
		index = endmatch.rm_eo;
	    }
	}
    }
}

#endif /* !DISABLE_COLOR */
