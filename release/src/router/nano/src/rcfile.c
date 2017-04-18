/**************************************************************************
 *   rcfile.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2017 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014 Mike Frysinger                                    *
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

#include <glob.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#ifndef DISABLE_NANORC

#ifndef RCFILE_NAME
#define RCFILE_NAME ".nanorc"
#endif

static const rcoption rcopts[] = {
    {"boldtext", BOLD_TEXT},
#ifdef ENABLE_LINENUMBERS
    {"linenumbers", LINE_NUMBERS},
#endif
#ifndef DISABLE_JUSTIFY
    {"brackets", 0},
#endif
    {"const", CONST_UPDATE},  /* deprecated form, remove in 2018 */
    {"constantshow", CONST_UPDATE},
#ifndef DISABLE_WRAPJUSTIFY
    {"fill", 0},
#endif
#ifndef DISABLE_HISTORIES
    {"historylog", HISTORYLOG},
#endif
    {"morespace", MORE_SPACE},
#ifndef DISABLE_MOUSE
    {"mouse", USE_MOUSE},
#endif
#ifndef DISABLE_MULTIBUFFER
    {"multibuffer", MULTIBUFFER},
#endif
    {"nohelp", NO_HELP},
    {"nonewlines", NO_NEWLINES},
    {"nopauses", NO_PAUSES},
#ifndef DISABLE_WRAPPING
    {"nowrap", NO_WRAP},
#endif
#ifndef DISABLE_OPERATINGDIR
    {"operatingdir", 0},
#endif
#ifndef DISABLE_HISTORIES
    {"poslog", POS_HISTORY},  /* deprecated form, remove in 2018 */
    {"positionlog", POS_HISTORY},
#endif
    {"preserve", PRESERVE},
#ifndef DISABLE_JUSTIFY
    {"punct", 0},
    {"quotestr", 0},
#endif
    {"rebinddelete", REBIND_DELETE},
    {"rebindkeypad", REBIND_KEYPAD},
    {"regexp", USE_REGEXP},
#ifndef DISABLE_SPELLER
    {"speller", 0},
#endif
    {"suspend", SUSPEND},
    {"tabsize", 0},
    {"tempfile", TEMP_FILE},
    {"view", VIEW_MODE},
#ifndef NANO_TINY
    {"allow_insecure_backup", INSECURE_BACKUP},
    {"autoindent", AUTOINDENT},
    {"backup", BACKUP_FILE},
    {"backupdir", 0},
    {"backwards", BACKWARDS_SEARCH},
    {"casesensitive", CASE_SENSITIVE},
    {"cut", CUT_TO_END},
    {"justifytrim", JUSTIFY_TRIM},
    {"locking", LOCKING},
    {"matchbrackets", 0},
    {"noconvert", NO_CONVERT},
    {"quickblank", QUICK_BLANK},
    {"quiet", QUIET},
    {"showcursor", SHOW_CURSOR},
    {"smarthome", SMART_HOME},
    {"smooth", SMOOTH_SCROLL},
    {"softwrap", SOFTWRAP},
    {"tabstospaces", TABS_TO_SPACES},
    {"unix", MAKE_IT_UNIX},
    {"whitespace", 0},
    {"wordbounds", WORD_BOUNDS},
    {"wordchars", 0},
#endif
#ifndef DISABLE_COLOR
    {"titlecolor", 0},
    {"numbercolor", 0},
    {"statuscolor", 0},
    {"keycolor", 0},
    {"functioncolor", 0},
#endif
    {NULL, 0}
};

static bool errors = FALSE;
	/* Whether we got any errors while parsing an rcfile. */
static size_t lineno = 0;
	/* If we did, the line number where the last error occurred. */
static char *nanorc = NULL;
	/* The path to the rcfile we're parsing. */
#ifndef DISABLE_COLOR
static bool opensyntax = FALSE;
	/* Whether we're allowed to add to the last syntax.  When a file ends,
	 * or when a new syntax command is seen, this bool becomes FALSE. */
static syntaxtype *live_syntax;
	/* The syntax that is currently being parsed. */
static colortype *lastcolor = NULL;
	/* The end of the color list for the current syntax. */
#endif

/* We have an error in some part of the rcfile.  Print the error message
 * on stderr, and then make the user hit Enter to continue starting
 * nano. */
void rcfile_error(const char *msg, ...)
{
    va_list ap;

    if (ISSET(QUIET))
	return;

    fprintf(stderr, "\n");
    if (lineno > 0) {
	errors = TRUE;
	fprintf(stderr, _("Error in %s on line %lu: "), nanorc, (unsigned long)lineno);
    }

    va_start(ap, msg);
    vfprintf(stderr, _(msg), ap);
    va_end(ap);

    fprintf(stderr, "\n");
}
#endif /* !DISABLE_NANORC */

#if !defined(DISABLE_NANORC) || !defined(DISABLE_HISTORIES)
/* Parse the next word from the string, null-terminate it, and return
 * a pointer to the first character after the null terminator.  The
 * returned pointer will point to '\0' if we hit the end of the line. */
char *parse_next_word(char *ptr)
{
    while (!isblank((unsigned char)*ptr) && *ptr != '\0')
	ptr++;

    if (*ptr == '\0')
	return ptr;

    /* Null-terminate and advance ptr. */
    *ptr++ = '\0';

    while (isblank((unsigned char)*ptr))
	ptr++;

    return ptr;
}
#endif /* !DISABLE_NANORC || !DISABLE_HISTORIES */

#ifndef DISABLE_NANORC
/* Parse an argument, with optional quotes, after a keyword that takes
 * one.  If the next word starts with a ", we say that it ends with the
 * last " of the line.  Otherwise, we interpret it as usual, so that the
 * arguments can contain "'s too. */
char *parse_argument(char *ptr)
{
    const char *ptr_save = ptr;
    char *last_quote = NULL;

    assert(ptr != NULL);

    if (*ptr != '"')
	return parse_next_word(ptr);

    do {
	ptr++;
	if (*ptr == '"')
	    last_quote = ptr;
    } while (*ptr != '\0');

    if (last_quote == NULL) {
	if (*ptr == '\0')
	    ptr = NULL;
	else
	    *ptr++ = '\0';
	rcfile_error(N_("Argument '%s' has an unterminated \""), ptr_save);
    } else {
	*last_quote = '\0';
	ptr = last_quote + 1;
    }
    if (ptr != NULL)
	while (isblank((unsigned char)*ptr))
	    ptr++;
    return ptr;
}

#ifndef DISABLE_COLOR
/* Pass over the current regex string in the line starting at ptr,
 * null-terminate it, and return a pointer to the /next/ word. */
char *parse_next_regex(char *ptr)
{
    assert(ptr != NULL);

    /* Continue until the end of line, or until a " followed by a
     * blank character or the end of line. */
    while (*ptr != '\0' && (*ptr != '"' ||
		(*(ptr + 1) != '\0' && !isblank((unsigned char)ptr[1]))))
	ptr++;

    assert(*ptr == '"' || *ptr == '\0');

    if (*ptr == '\0') {
	rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	return NULL;
    }

    /* Null-terminate and advance ptr. */
    *ptr++ = '\0';

    while (isblank((unsigned char)*ptr))
	ptr++;

    return ptr;
}

/* Compile the regular expression regex to see if it's valid.  Return
 * TRUE if it is, and FALSE otherwise. */
bool nregcomp(const char *regex, int compile_flags)
{
    regex_t preg;
    const char *r = fixbounds(regex);
    int rc = regcomp(&preg, r, compile_flags);

    if (rc != 0) {
	size_t len = regerror(rc, &preg, NULL, 0);
	char *str = charalloc(len);

	regerror(rc, &preg, str, len);
	rcfile_error(N_("Bad regex \"%s\": %s"), r, str);
	free(str);
    }

    regfree(&preg);
    return (rc == 0);
}

/* Parse the next syntax name and its possible extension regexes from the
 * line at ptr, and add it to the global linked list of color syntaxes. */
void parse_syntax(char *ptr)
{
    char *nameptr;
	/* A pointer to what should be the name of the syntax. */

    opensyntax = FALSE;

    assert(ptr != NULL);

    /* Check that the syntax name is not empty. */
    if (*ptr == '\0' || (*ptr == '"' &&
			(*(ptr + 1) == '\0' || *(ptr + 1) == '"'))) {
	rcfile_error(N_("Missing syntax name"));
	return;
    }

    nameptr = ++ptr;
    ptr = parse_next_word(ptr);

    /* Check that the name starts and ends with a double quote. */
    if (*(nameptr - 1) != '\x22' || nameptr[strlen(nameptr) - 1] != '\x22') {
	rcfile_error(N_("A syntax name must be quoted"));
	return;
    }

    /* Strip the end quote. */
    nameptr[strlen(nameptr) - 1] = '\0';

    /* Redefining the "none" syntax is not allowed. */
    if (strcmp(nameptr, "none") == 0) {
	rcfile_error(N_("The \"none\" syntax is reserved"));
	return;
    }

    /* Initialize a new syntax struct. */
    live_syntax = (syntaxtype *)nmalloc(sizeof(syntaxtype));
    live_syntax->name = mallocstrcpy(NULL, nameptr);
    live_syntax->extensions = NULL;
    live_syntax->headers = NULL;
    live_syntax->magics = NULL;
    live_syntax->linter = NULL;
    live_syntax->formatter = NULL;
    live_syntax->comment = NULL;
    live_syntax->color = NULL;
    lastcolor = NULL;
    live_syntax->nmultis = 0;

    /* Hook the new syntax in at the top of the list. */
    live_syntax->next = syntaxes;
    syntaxes = live_syntax;

    opensyntax = TRUE;

#ifdef DEBUG
    fprintf(stderr, "Starting a new syntax type: \"%s\"\n", nameptr);
#endif

    /* The default syntax should have no associated extensions. */
    if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
	rcfile_error(
		N_("The \"default\" syntax does not accept extensions"));
	return;
    }

    /* If there seem to be extension regexes, pick them up. */
    if (*ptr != '\0')
	grab_and_store("extension", ptr, &live_syntax->extensions);
}
#endif /* !DISABLE_COLOR */

/* Check whether the given executable function is "universal" (meaning
 * any horizontal movement or deletion) and thus is present in almost
 * all menus. */
bool is_universal(void (*func))
{
    if (func == do_left || func == do_right ||
	func == do_home_void || func == do_end_void ||
	func == do_prev_word_void || func == do_next_word_void ||
	func == do_verbatim_input || func == do_cut_text_void ||
	func == do_delete || func == do_backspace ||
	func == do_tab || func == do_enter)
	return TRUE;
    else
	return FALSE;
}

/* Bind or unbind a key combo, to or from a function. */
void parse_binding(char *ptr, bool dobind)
{
    char *keyptr = NULL, *keycopy = NULL, *funcptr = NULL, *menuptr = NULL;
    sc *s, *newsc = NULL;
    int menu;

    assert(ptr != NULL);

#ifdef DEBUG
    fprintf(stderr, "Starting the rebinding code...\n");
#endif

    if (*ptr == '\0') {
	rcfile_error(N_("Missing key name"));
	return;
    }

    keyptr = ptr;
    ptr = parse_next_word(ptr);
    keycopy = mallocstrcpy(NULL, keyptr);

    if (strlen(keycopy) < 2) {
	rcfile_error(N_("Key name is too short"));
	goto free_copy;
    }

    /* Uppercase only the first two or three characters of the key name. */
    keycopy[0] = toupper((unsigned char)keycopy[0]);
    keycopy[1] = toupper((unsigned char)keycopy[1]);
    if (keycopy[0] == 'M' && keycopy[1] == '-') {
	if (strlen(keycopy) > 2)
	    keycopy[2] = toupper((unsigned char)keycopy[2]);
	else {
	    rcfile_error(N_("Key name is too short"));
	    goto free_copy;
	}
    }

    /* Allow the codes for Insert and Delete to be rebound, but apart
     * from those two only Control, Meta and Function sequences. */
    if (!strcasecmp(keycopy, "Ins") || !strcasecmp(keycopy, "Del"))
	keycopy[1] = tolower((unsigned char)keycopy[1]);
    else if (keycopy[0] != '^' && keycopy[0] != 'M' && keycopy[0] != 'F') {
	rcfile_error(N_("Key name must begin with \"^\", \"M\", or \"F\""));
	goto free_copy;
    } else if ((keycopy[0] == 'M' && keycopy[1] != '-') ||
		(keycopy[0] == '^' && ((keycopy[1] < 'A' || keycopy[1] > 'z') ||
		keycopy[1] == '[' || keycopy[1] == '`' ||
		(strlen(keycopy) > 2 && strcmp(keycopy, "^Space") != 0))) ||
		(strlen(keycopy) > 3 && strcmp(keycopy, "^Space") != 0 &&
		strcmp(keycopy, "M-Space") != 0)) {
	rcfile_error(N_("Key name %s is invalid"), keycopy);
	goto free_copy;
    }

    if (dobind) {
	funcptr = ptr;
	ptr = parse_next_word(ptr);

	if (funcptr[0] == '\0') {
	    rcfile_error(N_("Must specify a function to bind the key to"));
	    goto free_copy;
	}
    }

    menuptr = ptr;
    ptr = parse_next_word(ptr);

    if (menuptr[0] == '\0') {
	/* TRANSLATORS: Do not translate the word "all". */
	rcfile_error(N_("Must specify a menu (or \"all\") in which to bind/unbind the key"));
	goto free_copy;
    }

    if (dobind) {
	newsc = strtosc(funcptr);
	if (newsc == NULL) {
	    rcfile_error(N_("Cannot map name \"%s\" to a function"), funcptr);
	    goto free_copy;
	}
    }

    menu = strtomenu(menuptr);
    if (menu < 1) {
	rcfile_error(N_("Cannot map name \"%s\" to a menu"), menuptr);
	goto free_copy;
    }

#ifdef DEBUG
    if (dobind)
	fprintf(stderr, "newsc address is now %ld, assigned func = %ld, menu = %x\n",
	    (long)&newsc, (long)newsc->scfunc, menu);
    else
	fprintf(stderr, "unbinding \"%s\" from menu %x\n", keycopy, menu);
#endif

    if (dobind) {
	subnfunc *f;
	int mask = 0;

	/* Tally up the menus where the function exists. */
	for (f = allfuncs; f != NULL; f = f->next)
	    if (f->scfunc == newsc->scfunc)
		mask = mask | f->menus;

#ifndef NANO_TINY
	/* Handle the special case of the toggles. */
	if (newsc->scfunc == do_toggle_void)
	    mask = MMAIN;
#endif

	/* Now limit the given menu to those where the function exists. */
	if (is_universal(newsc->scfunc))
	    menu = menu & MMOST;
	else
	    menu = menu & mask;

	if (!menu) {
	    rcfile_error(N_("Function '%s' does not exist in menu '%s'"), funcptr, menuptr);
	    free(newsc);
	    goto free_copy;
	}

	newsc->menus = menu;
	assign_keyinfo(newsc, keycopy, 0);

	/* Do not allow rebinding a frequent escape-sequence starter: Esc [. */
	if (newsc->meta && newsc->keycode == 91) {
	    rcfile_error(N_("Sorry, keystroke \"%s\" may not be rebound"), newsc->keystr);
	    free(newsc);
	    goto free_copy;
	}
#ifdef DEBUG
	fprintf(stderr, "s->keystr = \"%s\"\n", newsc->keystr);
	fprintf(stderr, "s->keycode = \"%d\"\n", newsc->keycode);
#endif
    }

    /* Now find and delete any existing same shortcut in the menu(s). */
    for (s = sclist; s != NULL; s = s->next) {
	if ((s->menus & menu) && !strcmp(s->keystr, keycopy)) {
#ifdef DEBUG
	    fprintf(stderr, "deleting entry from among menus %x\n", s->menus);
#endif
	    s->menus &= ~menu;
	}
    }

    if (dobind) {
#ifndef NANO_TINY
	/* If this is a toggle, copy its sequence number. */
	if (newsc->scfunc == do_toggle_void) {
	    for (s = sclist; s != NULL; s = s->next)
		if (s->scfunc == do_toggle_void && s->toggle == newsc->toggle)
		    newsc->ordinal = s->ordinal;
	} else
	    newsc->ordinal = 0;
#endif
	/* Add the new shortcut at the start of the list. */
	newsc->next = sclist;
	sclist = newsc;
	return;
    }

  free_copy:
    free(keycopy);
}

/* Verify that the given file is not a folder nor a device. */
bool is_good_file(char *file)
{
    struct stat rcinfo;

    /* If the thing exists, it may not be a directory nor a device. */
    if (stat(file, &rcinfo) != -1 && (S_ISDIR(rcinfo.st_mode) ||
		S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode))) {
	rcfile_error(S_ISDIR(rcinfo.st_mode) ? _("\"%s\" is a directory") :
					_("\"%s\" is a device file"), file);
	return FALSE;
    } else
	return TRUE;
}

#ifndef DISABLE_COLOR
/* Read and parse one included syntax file. */
static void parse_one_include(char *file)
{
    FILE *rcstream;

    /* Don't open directories, character files, or block files. */
    if (!is_good_file(file))
	return;

    /* Open the included syntax file. */
    rcstream = fopen(file, "rb");

    if (rcstream == NULL) {
	rcfile_error(_("Error reading %s: %s"), file, strerror(errno));
	return;
    }

    /* Use the name and line number position of the included syntax file
     * while parsing it, so we can know where any errors in it are. */
    nanorc = file;
    lineno = 0;

#ifdef DEBUG
    fprintf(stderr, "Parsing file \"%s\"\n", file);
#endif

    parse_rcfile(rcstream, TRUE);
}

/* Expand globs in the passed name, and parse the resultant files. */
void parse_includes(char *ptr)
{
    char *option, *nanorc_save = nanorc, *expanded;
    size_t lineno_save = lineno, i;
    glob_t files;

    option = ptr;
    if (*option == '"')
	option++;
    ptr = parse_argument(ptr);

    /* Expand tildes first, then the globs. */
    expanded = real_dir_from_tilde(option);

    if (glob(expanded, GLOB_ERR|GLOB_NOSORT, NULL, &files) == 0) {
	for (i = 0; i < files.gl_pathc; ++i)
	    parse_one_include(files.gl_pathv[i]);
    } else
	rcfile_error(_("Error expanding %s: %s"), option, strerror(errno));

    globfree(&files);
    free(expanded);

    /* We're done with the included file(s).  Restore the original
     * filename and line number position. */
    nanorc = nanorc_save;
    lineno = lineno_save;
}

/* Return the short value corresponding to the color named in colorname,
 * and set bright to TRUE if that color is bright. */
short color_to_short(const char *colorname, bool *bright)
{
    assert(colorname != NULL && bright != NULL);

    if (strncasecmp(colorname, "bright", 6) == 0) {
	*bright = TRUE;
	colorname += 6;
    }

    if (strcasecmp(colorname, "green") == 0)
	return COLOR_GREEN;
    else if (strcasecmp(colorname, "red") == 0)
	return COLOR_RED;
    else if (strcasecmp(colorname, "blue") == 0)
	return COLOR_BLUE;
    else if (strcasecmp(colorname, "white") == 0)
	return COLOR_WHITE;
    else if (strcasecmp(colorname, "yellow") == 0)
	return COLOR_YELLOW;
    else if (strcasecmp(colorname, "cyan") == 0)
	return COLOR_CYAN;
    else if (strcasecmp(colorname, "magenta") == 0)
	return COLOR_MAGENTA;
    else if (strcasecmp(colorname, "black") == 0)
	return COLOR_BLACK;

    rcfile_error(N_("Color \"%s\" not understood.\n"
		"Valid colors are \"green\", \"red\", \"blue\",\n"
		"\"white\", \"yellow\", \"cyan\", \"magenta\" and\n"
		"\"black\", with the optional prefix \"bright\"\n"
		"for foreground colors."), colorname);
    return -1;
}

/* Parse the color string in the line at ptr, and add it to the current
 * file's associated colors.  rex_flags are the regex compilation flags
 * to use, excluding or including REG_ICASE for case (in)sensitivity. */
void parse_colors(char *ptr, int rex_flags)
{
    short fg, bg;
    bool bright = FALSE;
    char *item;

    assert(ptr != NULL);

    if (!opensyntax) {
	rcfile_error(
		N_("A '%s' command requires a preceding 'syntax' command"),
		"color");
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing color name"));
	return;
    }

    item = ptr;
    ptr = parse_next_word(ptr);
    if (!parse_color_names(item, &fg, &bg, &bright))
	return;

    if (*ptr == '\0') {
	rcfile_error(N_("Missing regex string after '%s' command"), "color");
	return;
    }

    /* Now for the fun part.  Start adding regexes to individual strings
     * in the colorstrings array, woo! */
    while (ptr != NULL && *ptr != '\0') {
	colortype *newcolor = NULL;
	    /* The container for a color plus its regexes. */
	bool goodstart;
	    /* Whether the start expression was valid. */
	bool expectend = FALSE;
	    /* Whether to expect an end= line. */

	if (strncasecmp(ptr, "start=", 6) == 0) {
	    ptr += 6;
	    expectend = TRUE;
	}

	if (*ptr != '"') {
	    rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	    ptr = parse_next_regex(ptr);
	    continue;
	}

	item = ++ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	if (*item == '\0') {
	    rcfile_error(N_("Empty regex string"));
	    goodstart = FALSE;
	} else
	    goodstart = nregcomp(item, rex_flags);

	/* If the starting regex is valid, initialize a new color struct,
	 * and hook it in at the tail of the linked list. */
	if (goodstart) {
	    newcolor = (colortype *)nmalloc(sizeof(colortype));

	    newcolor->fg = fg;
	    newcolor->bg = bg;
	    newcolor->bright = bright;
	    newcolor->rex_flags = rex_flags;

	    newcolor->start_regex = mallocstrcpy(NULL, item);
	    newcolor->start = NULL;

	    newcolor->end_regex = NULL;
	    newcolor->end = NULL;

	    newcolor->next = NULL;

#ifdef DEBUG
	    fprintf(stderr, "Adding an entry for fg %hd, bg %hd\n", fg, bg);
#endif
	    if (lastcolor == NULL)
		live_syntax->color = newcolor;
	    else
		lastcolor->next = newcolor;

	    lastcolor = newcolor;
	}

	if (!expectend)
	    continue;

	if (ptr == NULL || strncasecmp(ptr, "end=", 4) != 0) {
	    rcfile_error(N_("\"start=\" requires a corresponding \"end=\""));
	    return;
	}

	ptr += 4;
	if (*ptr != '"') {
	    rcfile_error(N_("Regex strings must begin and end with a \" character"));
	    continue;
	}

	item = ++ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    break;

	if (*item == '\0') {
	    rcfile_error(N_("Empty regex string"));
	    continue;
	}

	/* If the start regex was invalid, skip past the end regex
	 * to stay in sync. */
	if (!goodstart)
	    continue;

	/* If it's valid, save the ending regex string. */
	if (nregcomp(item, rex_flags))
	    newcolor->end_regex = mallocstrcpy(NULL, item);

	/* Lame way to skip another static counter. */
	newcolor->id = live_syntax->nmultis;
	live_syntax->nmultis++;
    }
}

/* Parse the color name, or pair of color names, in combostr. */
bool parse_color_names(char *combostr, short *fg, short *bg, bool *bright)
{
    bool no_fgcolor = FALSE;

    if (combostr == NULL)
	return FALSE;

    if (strchr(combostr, ',') != NULL) {
	char *bgcolorname;
	strtok(combostr, ",");
	bgcolorname = strtok(NULL, ",");
	if (bgcolorname == NULL) {
	    /* If we have a background color without a foreground color,
	     * parse it properly. */
	    bgcolorname = combostr + 1;
	    no_fgcolor = TRUE;
	}
	if (strncasecmp(bgcolorname, "bright", 6) == 0) {
	    rcfile_error(N_("Background color \"%s\" cannot be bright"), bgcolorname);
	    return FALSE;
	}
	*bg = color_to_short(bgcolorname, bright);
    } else
	*bg = -1;

    if (!no_fgcolor) {
	*fg = color_to_short(combostr, bright);

	/* Don't try to parse screwed-up foreground colors. */
	if (*fg == -1)
	    return FALSE;
    } else
	*fg = -1;

    return TRUE;
}

/* Read regex strings enclosed in double quotes from the line pointed at
 * by ptr, and store them quoteless in the passed storage place. */
void grab_and_store(const char *kind, char *ptr, regexlisttype **storage)
{
    regexlisttype *lastthing;

    if (!opensyntax) {
	rcfile_error(
		N_("A '%s' command requires a preceding 'syntax' command"), kind);
	return;
    }

    /* The default syntax doesn't take any file matching stuff. */
    if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
	rcfile_error(
		N_("The \"default\" syntax does not accept '%s' regexes"), kind);
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing regex string after '%s' command"), kind);
	return;
    }

    lastthing = *storage;

    /* If there was an earlier command, go to the last of those regexes. */
    while (lastthing != NULL && lastthing->next != NULL)
	lastthing = lastthing->next;

    /* Now gather any valid regexes and add them to the linked list. */
    while (*ptr != '\0') {
	const char *regexstring;
	regexlisttype *newthing;

	if (*ptr != '"') {
	    rcfile_error(
		N_("Regex strings must begin and end with a \" character"));
	    return;
	}

	regexstring = ++ptr;
	ptr = parse_next_regex(ptr);
	if (ptr == NULL)
	    return;

	/* If the regex string is malformed, skip it. */
	if (!nregcomp(regexstring, NANO_REG_EXTENDED | REG_NOSUB))
	    continue;

	/* Copy the regex into a struct, and hook this in at the end. */
	newthing = (regexlisttype *)nmalloc(sizeof(regexlisttype));
	newthing->full_regex = mallocstrcpy(NULL, regexstring);
	newthing->next = NULL;

	if (lastthing == NULL)
	    *storage = newthing;
	else
	    lastthing->next = newthing;

	lastthing = newthing;
    }
}

/* Parse and store the name given after a linter/formatter command. */
void pick_up_name(const char *kind, char *ptr, char **storage)
{
    assert(ptr != NULL);

    if (!opensyntax) {
	rcfile_error(
		N_("A '%s' command requires a preceding 'syntax' command"), kind);
	return;
    }

    if (*ptr == '\0') {
	rcfile_error(N_("Missing command after '%s'"), kind);
	return;
    }

    free(*storage);

    /* Allow unsetting the command by using an empty string. */
    if (!strcmp(ptr, "\"\""))
	*storage = NULL;
    else if (*ptr == '"') {
	*storage = mallocstrcpy(NULL, ++ptr);
	char* q = *storage;
	char* p = *storage;
	/* Snip out the backslashes of escaped characters. */
	while (*p != '"') {
	    if (*p == '\0') {
		rcfile_error(N_("Argument of '%s' lacks closing \""), kind);
		free(*storage);
		*storage = NULL;
		return;
	    } else if (*p == '\\' && *(p + 1) != '\0') {
		p++;
	    }
	    *q++ = *p++;
	}
	*q = '\0';
    }
    else
	*storage = mallocstrcpy(NULL, ptr);
}
#endif /* !DISABLE_COLOR */

/* Verify that the user has not unmapped every shortcut for a
 * function that we consider 'vital' (such as "Exit"). */
static void check_vitals_mapped(void)
{
    subnfunc *f;
    int v;
#define VITALS 5
    void (*vitals[VITALS])(void) = { do_exit, do_exit, do_cancel, do_cancel, do_cancel };
    int inmenus[VITALS] = { MMAIN, MHELP, MWHEREIS, MREPLACE, MGOTOLINE };

    for  (v = 0; v < VITALS; v++) {
	for (f = allfuncs; f != NULL; f = f->next) {
	    if (f->scfunc == vitals[v] && f->menus & inmenus[v]) {
		const sc *s = first_sc_for(inmenus[v], f->scfunc);
		if (!s) {
		    fprintf(stderr, _("Fatal error: no keys mapped for function "
					"\"%s\".  Exiting.\n"), f->desc);
		    fprintf(stderr, _("If needed, use nano with the -I option "
					"to adjust your nanorc settings.\n"));
		    exit(1);
		}
		break;
	    }
	}
    }
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If syntax_only is TRUE, allow the file to
 * to contain only color syntax commands. */
void parse_rcfile(FILE *rcstream, bool syntax_only)
{
    char *buf = NULL;
    ssize_t len;
    size_t n = 0;

    while ((len = getline(&buf, &n, rcstream)) > 0) {
	char *ptr, *keyword, *option;
	int set = 0;
	size_t i;

	/* Ignore the newline. */
	if (buf[len - 1] == '\n')
	    buf[len - 1] = '\0';

	lineno++;
	ptr = buf;
	while (isblank((unsigned char)*ptr))
	    ptr++;

	/* If we have a blank line or a comment, skip to the next
	 * line. */
	if (*ptr == '\0' || *ptr == '#')
	    continue;

	/* Otherwise, skip to the next space. */
	keyword = ptr;
	ptr = parse_next_word(ptr);

#ifndef DISABLE_COLOR
	/* Handle extending first... */
	if (strcasecmp(keyword, "extendsyntax") == 0) {
	    syntaxtype *sint;
	    char *syntaxname = ptr;

	    ptr = parse_next_word(ptr);

	    for (sint = syntaxes; sint != NULL; sint = sint->next)
		if (!strcmp(sint->name, syntaxname))
		    break;

	    if (sint == NULL) {
		rcfile_error(N_("Could not find syntax \"%s\" to extend"),
				syntaxname);
		opensyntax = FALSE;
		continue;
	    }

	    live_syntax = sint;
	    opensyntax = TRUE;

	    /* Refind the tail of the color list for this syntax. */
	    lastcolor = sint->color;
	    if (lastcolor != NULL)
		while (lastcolor->next != NULL)
		    lastcolor = lastcolor->next;

	    keyword = ptr;
	    ptr = parse_next_word(ptr);
	}

	/* Try to parse the keyword. */
	if (strcasecmp(keyword, "syntax") == 0) {
	    if (opensyntax && lastcolor == NULL)
		rcfile_error(N_("Syntax \"%s\" has no color commands"),
				live_syntax->name);
	    parse_syntax(ptr);
	}
	else if (strcasecmp(keyword, "header") == 0)
	    grab_and_store("header", ptr, &live_syntax->headers);
	else if (strcasecmp(keyword, "magic") == 0)
#ifdef HAVE_LIBMAGIC
	    grab_and_store("magic", ptr, &live_syntax->magics);
#else
	    ;
#endif
	else if (strcasecmp(keyword, "comment") == 0)
#ifdef ENABLE_COMMENT
	    pick_up_name("comment", ptr, &live_syntax->comment);
#else
	    ;
#endif
	else if (strcasecmp(keyword, "color") == 0)
	    parse_colors(ptr, NANO_REG_EXTENDED);
	else if (strcasecmp(keyword, "icolor") == 0)
	    parse_colors(ptr, NANO_REG_EXTENDED | REG_ICASE);
	else if (strcasecmp(keyword, "linter") == 0)
	    pick_up_name("linter", ptr, &live_syntax->linter);
	else if (strcasecmp(keyword, "formatter") == 0)
#ifndef DISABLE_SPELLER
	    pick_up_name("formatter", ptr, &live_syntax->formatter);
#else
	    ;
#endif
	else if (syntax_only)
	    rcfile_error(N_("Command \"%s\" not allowed in included file"),
					keyword);
	else if (strcasecmp(keyword, "include") == 0)
	    parse_includes(ptr);
	else
#endif /* !DISABLE_COLOR */
	if (strcasecmp(keyword, "set") == 0)
	    set = 1;
	else if (strcasecmp(keyword, "unset") == 0)
	    set = -1;
	else if (strcasecmp(keyword, "bind") == 0)
	    parse_binding(ptr, TRUE);
	else if (strcasecmp(keyword, "unbind") == 0)
	    parse_binding(ptr, FALSE);
	else
	    rcfile_error(N_("Command \"%s\" not understood"), keyword);

#ifndef DISABLE_COLOR
	/* If a syntax was extended, it stops at the end of the command. */
	if (live_syntax != syntaxes)
	    opensyntax = FALSE;
#endif

	if (set == 0)
	    continue;

	if (*ptr == '\0') {
	    rcfile_error(N_("Missing option"));
	    continue;
	}

	option = ptr;
	ptr = parse_next_word(ptr);

	/* Find the just read name among the existing options. */
	for (i = 0; rcopts[i].name != NULL; i++) {
	    if (strcasecmp(option, rcopts[i].name) == 0)
		break;
	}

	if (rcopts[i].name == NULL) {
	    rcfile_error(N_("Unknown option \"%s\""), option);
	    continue;
	}

#ifdef DEBUG
	fprintf(stderr, "    Option name = \"%s\"\n", rcopts[i].name);
	fprintf(stderr, "    Flag = %ld\n", rcopts[i].flag);
#endif
	/* First handle unsetting. */
	if (set == -1) {
	    if (rcopts[i].flag != 0)
		UNSET(rcopts[i].flag);
	    else
		rcfile_error(N_("Cannot unset option \"%s\""), rcopts[i].name);
	    continue;
	}

	/* If the option has a flag, it doesn't take an argument. */
	if (rcopts[i].flag != 0) {
	    SET(rcopts[i].flag);
	    continue;
	}

	/* The option doesn't have a flag, so it takes an argument. */
	if (*ptr == '\0') {
	    rcfile_error(N_("Option \"%s\" requires an argument"),
				rcopts[i].name);
	    continue;
	}

	option = ptr;
	if (*option == '"')
	    option++;
	ptr = parse_argument(ptr);

	option = mallocstrcpy(NULL, option);
#ifdef DEBUG
	fprintf(stderr, "    Option argument = \"%s\"\n", option);
#endif
	/* Make sure the option argument is a valid multibyte string. */
	if (!is_valid_mbstring(option)) {
	    rcfile_error(N_("Argument is not a valid multibyte string"));
	    continue;
	}

#ifndef DISABLE_COLOR
	if (strcasecmp(rcopts[i].name, "titlecolor") == 0)
	    specified_color_combo[TITLE_BAR] = option;
	else if (strcasecmp(rcopts[i].name, "numbercolor") == 0)
	    specified_color_combo[LINE_NUMBER] = option;
	else if (strcasecmp(rcopts[i].name, "statuscolor") == 0)
	    specified_color_combo[STATUS_BAR] = option;
	else if (strcasecmp(rcopts[i].name, "keycolor") == 0)
	    specified_color_combo[KEY_COMBO] = option;
	else if (strcasecmp(rcopts[i].name, "functioncolor") == 0)
	    specified_color_combo[FUNCTION_TAG] = option;
	else
#endif
#ifndef DISABLE_OPERATINGDIR
	if (strcasecmp(rcopts[i].name, "operatingdir") == 0)
	    operating_dir = option;
	else
#endif
#ifndef DISABLE_WRAPJUSTIFY
	if (strcasecmp(rcopts[i].name, "fill") == 0) {
	    if (!parse_num(option, &wrap_at)) {
		rcfile_error(N_("Requested fill size \"%s\" is invalid"),
				option);
		wrap_at = -CHARS_FROM_EOL;
	    } else
		UNSET(NO_WRAP);
	    free(option);
	} else
#endif
#ifndef NANO_TINY
	if (strcasecmp(rcopts[i].name, "matchbrackets") == 0) {
	    matchbrackets = option;
	    if (has_blank_mbchars(matchbrackets)) {
		rcfile_error(N_("Non-blank characters required"));
		free(matchbrackets);
		matchbrackets = NULL;
	    }
	} else if (strcasecmp(rcopts[i].name, "whitespace") == 0) {
	    whitespace = option;
	    if (mbstrlen(whitespace) != 2 || strlenpt(whitespace) != 2) {
		rcfile_error(N_("Two single-column characters required"));
		free(whitespace);
		whitespace = NULL;
	    } else {
		whitespace_len[0] = parse_mbchar(whitespace, NULL, NULL);
		whitespace_len[1] = parse_mbchar(whitespace +
					whitespace_len[0], NULL, NULL);
	    }
	} else
#endif
#ifndef DISABLE_JUSTIFY
	if (strcasecmp(rcopts[i].name, "punct") == 0) {
	    punct = option;
	    if (has_blank_mbchars(punct)) {
		rcfile_error(N_("Non-blank characters required"));
		free(punct);
		punct = NULL;
	    }
	} else if (strcasecmp(rcopts[i].name, "brackets") == 0) {
	    brackets = option;
	    if (has_blank_mbchars(brackets)) {
		rcfile_error(N_("Non-blank characters required"));
		free(brackets);
		brackets = NULL;
	    }
	} else if (strcasecmp(rcopts[i].name, "quotestr") == 0)
	    quotestr = option;
	else
#endif
#ifndef NANO_TINY
	if (strcasecmp(rcopts[i].name, "backupdir") == 0)
	    backup_dir = option;
	else
	if (strcasecmp(rcopts[i].name, "wordchars") == 0)
	    word_chars = option;
	else
#endif
#ifndef DISABLE_SPELLER
	if (strcasecmp(rcopts[i].name, "speller") == 0)
	    alt_speller = option;
	else
#endif
	if (strcasecmp(rcopts[i].name, "tabsize") == 0) {
	    if (!parse_num(option, &tabsize) || tabsize <= 0) {
		rcfile_error(N_("Requested tab size \"%s\" is invalid"),
				option);
		tabsize = -1;
	    }
	    free(option);
	} else
	    assert(FALSE);
    }

#ifndef DISABLE_COLOR
    if (opensyntax && lastcolor == NULL)
	rcfile_error(N_("Syntax \"%s\" has no color commands"),
			live_syntax->name);

    opensyntax = FALSE;
#endif

    free(buf);
    fclose(rcstream);
    lineno = 0;

    return;
}

/* Read and interpret one of the two nanorc files. */
void parse_one_nanorc(void)
{
    FILE *rcstream;

    /* Don't try to open directories nor devices. */
    if (!is_good_file(nanorc))
	return;

#ifdef DEBUG
    fprintf(stderr, "Going to parse file \"%s\"\n", nanorc);
#endif

    rcstream = fopen(nanorc, "rb");

    /* If opening the file succeeded, parse it.  Otherwise, only
     * complain if the file actually exists. */
    if (rcstream != NULL)
	parse_rcfile(rcstream, FALSE);
    else if (errno != ENOENT)
	rcfile_error(N_("Error reading %s: %s"), nanorc, strerror(errno));
}

/* First read the system-wide rcfile, then the user's rcfile. */
void do_rcfiles(void)
{
    nanorc = mallocstrcpy(nanorc, SYSCONFDIR "/nanorc");

    /* Process the system-wide nanorc. */
    parse_one_nanorc();

    /* When configured with --disable-wrapping-as-root, turn wrapping off
     * for root, so that only root's .nanorc or --fill can turn it on. */
#ifdef DISABLE_ROOTWRAPPING
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

    get_homedir();

    if (homedir == NULL)
	rcfile_error(N_("I can't find my home directory!  Wah!"));
    else {
	nanorc = charealloc(nanorc, strlen(homedir) + strlen(RCFILE_NAME) + 2);
	sprintf(nanorc, "%s/%s", homedir, RCFILE_NAME);

	/* Process the current user's nanorc. */
	parse_one_nanorc();
    }

    check_vitals_mapped();

    free(nanorc);

    if (errors && !ISSET(QUIET) && !ISSET(NO_PAUSES)) {
	errors = FALSE;
	fprintf(stderr, _("\nPress Enter to continue starting nano.\n"));
	while (getchar() != '\n')
	    ;
    }
}

#endif /* !DISABLE_NANORC */
