/**************************************************************************
 *   browser.c  --  This file is part of GNU nano.                        *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2017 Free Software Foundation, Inc.    *
 *   Copyright (C) 2015, 2016 Benno Schulenberg                           *
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef DISABLE_BROWSER

static char **filelist = NULL;
	/* The list of files to display in the file browser. */
static size_t filelist_len = 0;
	/* The number of files in the list. */
static int width = 0;
	/* The number of files that we can display per screen row. */
static int longest = 0;
	/* The number of columns in the longest filename in the list. */
static size_t selected = 0;
	/* The currently selected filename in the list; zero-based. */

/* Our main file browser function.  path is the tilde-expanded path we
 * start browsing from. */
char *do_browser(char *path)
{
    char *retval = NULL;
    int kbinput;
    char *present_name = NULL;
	/* The name of the currently selected file, or of the directory we
	 * were in before backing up to "..". */
    size_t old_selected;
	/* The number of the selected file before the current selected file. */
    functionptrtype func;
	/* The function of the key the user typed in. */
    DIR *dir;
	/* The directory whose contents we are showing. */

    /* Don't show a cursor in the file list. */
    curs_set(0);

  read_directory_contents:
	/* We come here when we refresh or select a new directory. */

    path = free_and_assign(path, get_full_path(path));

    if (path != NULL)
	dir = opendir(path);

    if (path == NULL || dir == NULL) {
	statusline(ALERT, _("Cannot open directory: %s"), strerror(errno));
	/* If we don't have a file list yet, there is nothing to show. */
	if (filelist == NULL) {
	    napms(1200);
	    lastmessage = HUSH;
	    free(path);
	    free(present_name);
	    return NULL;
	}
	path = mallocstrcpy(path, present_path);
	present_name = mallocstrcpy(present_name, filelist[selected]);
    }

    assert(path != NULL && path[strlen(path) - 1] == '/');

    if (dir != NULL) {
	/* Get the file list, and set longest and width in the process. */
	read_the_list(path, dir);
	closedir(dir);
	dir = NULL;
    }

    /* If given, reselect the present_name and then discard it. */
    if (present_name != NULL) {
	browser_select_dirname(present_name);

	free(present_name);
	present_name = NULL;
    /* Otherwise, select the first file or directory in the list. */
    } else
	selected = 0;

    old_selected = (size_t)-1;

    present_path = mallocstrcpy(present_path, path);

    titlebar(path);

    while (TRUE) {
	/* Make sure that the cursor is off. */
	curs_set(0);
	lastmessage = HUSH;

	bottombars(MBROWSER);

	/* Display (or redisplay) the file list if the list itself or
	 * the selected file has changed. */
	if (old_selected != selected)
	    browser_refresh();

	old_selected = selected;

	kbinput = get_kbinput(edit);

#ifndef DISABLE_MOUSE
	if (kbinput == KEY_MOUSE) {
	    int mouse_x, mouse_y;

	    /* We can click on the edit window to select a filename. */
	    if (get_mouseinput(&mouse_x, &mouse_y, TRUE) == 0 &&
			wmouse_trafo(edit, &mouse_y, &mouse_x, FALSE)) {
		/* longest is the width of each column.  There
		 * are two spaces between each column. */
		selected = selected - selected % (editwinrows * width) +
				(mouse_y * width) + (mouse_x / (longest + 2));

		/* If they clicked beyond the end of a row,
		 * select the last filename in that row. */
		if (mouse_x > width * (longest + 2))
		    selected--;

		/* If we're beyond the list, select the last filename. */
		if (selected > filelist_len - 1)
		    selected = filelist_len - 1;

		/* If we selected the same filename as last time, fake a
		 * press of the Enter key so that the file is read in. */
		if (old_selected == selected)
		    unget_kbinput(KEY_ENTER, FALSE);
	    }

	    continue;
	}
#endif /* !DISABLE_MOUSE */

	func = parse_browser_input(&kbinput);

	if (func == total_refresh) {
	    total_redraw();
#ifndef NANO_TINY
	    /* Simulate a window resize to force a directory reread. */
	    kbinput = KEY_WINCH;
#endif
	} else if (func == do_help_void) {
#ifndef DISABLE_HELP
	    do_help_void();
#ifndef NANO_TINY
	    /* The window dimensions might have changed, so act as if. */
	    kbinput = KEY_WINCH;
#endif
#else
	    say_there_is_no_help();
#endif
	} else if (func == do_search) {
	    /* Search for a filename. */
	    do_filesearch();
	} else if (func == do_research) {
	    /* Search for another filename. */
	    do_fileresearch();
	} else if (func == do_left) {
	    if (selected > 0)
		selected--;
	} else if (func == do_right) {
	    if (selected < filelist_len - 1)
		selected++;
#ifndef NANO_TINY
	} else if (func == do_prev_word_void) {
	    selected -= (selected % width);
	} else if (func == do_next_word_void) {
	    selected += width - 1 - (selected % width);
	    if (selected >= filelist_len)
		selected = filelist_len - 1;
#endif
	} else if (func == do_up_void) {
	    if (selected >= width)
		selected -= width;
	} else if (func == do_down_void) {
	    if (selected + width <= filelist_len - 1)
		selected += width;
	} else if (func == do_prev_block) {
	    selected = ((selected / (editwinrows * width)) *
				editwinrows * width) + selected % width;
	} else if (func == do_next_block) {
	    selected = ((selected / (editwinrows * width)) *
				editwinrows * width) + selected % width +
				editwinrows * width - width;
	    if (selected >= filelist_len)
		selected = (filelist_len / width) * width + selected % width;
	    if (selected >= filelist_len)
		selected -= width;
	} else if (func == do_page_up) {
	    if (selected < width)
		selected = 0;
	    else if (selected < editwinrows * width)
		selected = selected % width;
	    else
		selected -= editwinrows * width;
	} else if (func == do_page_down) {
	    if (selected + width >= filelist_len - 1)
		selected = filelist_len - 1;
	    else if (selected + editwinrows * width >= filelist_len)
		selected = (selected + editwinrows * width - filelist_len) %
				width +	filelist_len - width;
	    else
		selected += editwinrows * width;
	} else if (func == do_first_file) {
	    selected = 0;
	} else if (func == do_last_file) {
	    selected = filelist_len - 1;
	} else if (func == goto_dir_void) {
	    /* Ask for the directory to go to. */
	    int i = do_prompt(TRUE, FALSE, MGOTODIR, NULL,
#ifndef DISABLE_HISTORIES
			NULL,
#endif
			/* TRANSLATORS: This is a prompt. */
			browser_refresh, _("Go To Directory"));

	    if (i < 0) {
		statusbar(_("Cancelled"));
		continue;
	    }

	    path = free_and_assign(path, real_dir_from_tilde(answer));

	    /* If the given path is relative, join it with the current path. */
	    if (*path != '/') {
		path = charealloc(path, strlen(present_path) +
						strlen(answer) + 1);
		sprintf(path, "%s%s", present_path, answer);
	    }

#ifndef DISABLE_OPERATINGDIR
	    if (check_operating_dir(path, FALSE)) {
		/* TRANSLATORS: This refers to the confining effect of the
		 * option --operatingdir, not of --restricted. */
		statusline(ALERT, _("Can't go outside of %s"),
				full_operating_dir);
		path = mallocstrcpy(path, present_path);
		continue;
	    }
#endif
	    /* Snip any trailing slashes, so the name can be compared. */
	    while (strlen(path) > 1 && path[strlen(path) - 1] == '/')
		path[strlen(path) - 1] = '\0';

	    /* In case the specified directory cannot be entered, select it
	     * (if it is in the current list) so it will be highlighted. */
	    for (i = 0; i < filelist_len; i++)
		if (strcmp(filelist[i], path) == 0)
		    selected = i;

	    /* Try opening and reading the specified directory. */
	    goto read_directory_contents;
	} else if (func == do_enter) {
	    struct stat st;

	    /* It isn't possible to move up from the root directory. */
	    if (strcmp(filelist[selected], "/..") == 0) {
		statusline(ALERT, _("Can't move up a directory"));
		continue;
	    }

#ifndef DISABLE_OPERATINGDIR
	    /* Note: The selected file can be outside the operating
	     * directory if it's ".." or if it's a symlink to a
	     * directory outside the operating directory. */
	    if (check_operating_dir(filelist[selected], FALSE)) {
		statusline(ALERT, _("Can't go outside of %s"),
				full_operating_dir);
		continue;
	    }
#endif
	    /* If for some reason the file is inaccessible, complain. */
	    if (stat(filelist[selected], &st) == -1) {
		statusline(ALERT, _("Error reading %s: %s"),
				filelist[selected], strerror(errno));
		continue;
	    }

	    /* If it isn't a directory, a file was selected -- we're done. */
	    if (!S_ISDIR(st.st_mode)) {
		retval = mallocstrcpy(NULL, filelist[selected]);
		break;
	    }

	    /* If we are moving up one level, remember where we came from, so
	     * this directory can be highlighted and easily reentered. */
	    if (strcmp(tail(filelist[selected]), "..") == 0)
		present_name = strip_last_component(filelist[selected]);

	    /* Try opening and reading the selected directory. */
	    path = mallocstrcpy(path, filelist[selected]);
	    goto read_directory_contents;
	} else if (func == do_exit) {
	    /* Exit from the file browser. */
	    break;
#ifndef NANO_TINY
	} else if (kbinput == KEY_WINCH) {
	    ;
#endif
	} else
	    unbound_key(kbinput);

#ifndef NANO_TINY
	/* If the window resized, refresh the file list. */
	if (kbinput == KEY_WINCH) {
	    /* Remember the selected file, to be able to reselect it. */
	    present_name = mallocstrcpy(NULL, filelist[selected]);
	    /* Reread the contents of the current directory. */
	    goto read_directory_contents;
	}
#endif
    }

    titlebar(NULL);
    edit_refresh();

    free(path);

    free_chararray(filelist, filelist_len);
    filelist = NULL;
    filelist_len = 0;

    return retval;
}

/* The file browser front end.  We check to see if inpath has a
 * directory in it.  If it does, we start do_browser() from there.
 * Otherwise, we start do_browser() from the current directory. */
char *do_browse_from(const char *inpath)
{
    struct stat st;
    char *path;
	/* This holds the tilde-expanded version of inpath. */

    path = real_dir_from_tilde(inpath);

    /* Perhaps path is a directory.  If so, we'll pass it to
     * do_browser().  Or perhaps path is a directory / a file.  If so,
     * we'll try stripping off the last path element and passing it to
     * do_browser().  Or perhaps path doesn't have a directory portion
     * at all.  If so, we'll just pass the current directory to
     * do_browser(). */
    if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	path = free_and_assign(path, strip_last_component(path));

	if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
	    char * currentdir = charalloc(PATH_MAX + 1);

	    free(path);
	    path = getcwd(currentdir, PATH_MAX + 1);

	    if (path == NULL) {
		free(currentdir);
		statusline(MILD, _("The working directory has disappeared"));
		beep();
		napms(1200);
		return NULL;
	    }
	}
    }

#ifndef DISABLE_OPERATINGDIR
    /* If the resulting path isn't in the operating directory, use
     * the operating directory instead. */
    if (check_operating_dir(path, FALSE))
	path = mallocstrcpy(path, operating_dir);
#endif

    return do_browser(path);
}

/* Set filelist to the list of files contained in the directory path,
 * set filelist_len to the number of files in that list, set longest to
 * the width in columns of the longest filename in that list (between 15
 * and COLS), and set width to the number of files that we can display
 * per screen row.  And sort the list too. */
void read_the_list(const char *path, DIR *dir)
{
    const struct dirent *nextdir;
    size_t i = 0, path_len = strlen(path);

    assert(path != NULL && path[strlen(path) - 1] == '/' && dir != NULL);

    longest = 0;

    /* Find the length of the longest filename in the current folder. */
    while ((nextdir = readdir(dir)) != NULL) {
	size_t name_len = strlenpt(nextdir->d_name);

	if (name_len > longest)
	    longest = name_len;

	i++;
    }

    /* Put 10 characters' worth of blank space between columns of filenames
     * in the list whenever possible, as Pico does. */
    longest += 10;

    /* If needed, make room for ".. (parent dir)". */
    if (longest < 15)
	longest = 15;
    /* Make sure we're not wider than the window. */
    if (longest > COLS)
	longest = COLS;

    rewinddir(dir);

    free_chararray(filelist, filelist_len);

    filelist_len = i;

    filelist = (char **)nmalloc(filelist_len * sizeof(char *));

    i = 0;

    while ((nextdir = readdir(dir)) != NULL && i < filelist_len) {
	/* Don't show the "." entry. */
	if (strcmp(nextdir->d_name, ".") == 0)
	    continue;

	filelist[i] = charalloc(path_len + strlen(nextdir->d_name) + 1);
	sprintf(filelist[i], "%s%s", path, nextdir->d_name);

	i++;
    }

    /* Maybe the number of files in the directory changed between the
     * first time we scanned and the second.  i is the actual length of
     * filelist, so record it. */
    filelist_len = i;

    assert(filelist != NULL);

    /* Sort the list of names. */
    qsort(filelist, filelist_len, sizeof(char *), diralphasort);

    /* Calculate how many files fit on a line -- feigning room for two
     * spaces beyond the right edge, and adding two spaces of padding
     * between columns. */
    width = (COLS + 2) / (longest + 2);
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for compatibility with Pico. */
functionptrtype parse_browser_input(int *kbinput)
{
    if (!meta_key) {
	switch (*kbinput) {
	    case ' ':
		return do_page_down;
	    case '-':
		return do_page_up;
	    case '?':
		return do_help_void;
	    case 'E':
	    case 'e':
	    case 'Q':
	    case 'q':
	    case 'X':
	    case 'x':
		return do_exit;
	    case 'G':
	    case 'g':
		return goto_dir_void;
	    case 'S':
	    case 's':
		return do_enter;
	    case 'W':
	    case 'w':
		return do_search;
	}
    }
    return func_from_key(kbinput);
}

/* Set width to the number of files that we can display per screen row,
 * if necessary, and display the list of files. */
void browser_refresh(void)
{
    size_t i;
    int row = 0, col = 0;
	/* The current row and column while the list is getting displayed. */
    int the_row = 0, the_column = 0;
	/* The row and column of the selected item. */
    char *info;
	/* The additional information that we'll display about a file. */

    titlebar(present_path);
    blank_edit();

    wmove(edit, 0, 0);

    i = selected - selected % (editwinrows * width);

    for (; i < filelist_len && row < editwinrows; i++) {
	struct stat st;
	const char *thename = tail(filelist[i]);
		/* The filename we display, minus the path. */
	size_t namelen = strlenpt(thename);
		/* The length of the filename in columns. */
	size_t infolen;
		/* The length of the file information in columns. */
	int infomaxlen = 7;
		/* The maximum length of the file information in columns:
		 * normally seven, but will be twelve for "(parent dir)". */
	bool dots = (COLS >= 15 && namelen >= longest - infomaxlen);
		/* Whether to put an ellipsis before the filename?  We don't
		 * waste space on dots when there are fewer than 15 columns. */
	char *disp = display_string(thename, dots ?
		namelen + infomaxlen + 4 - longest : 0, longest, FALSE);
		/* The filename (or a fragment of it) in displayable format.
		 * When a fragment, account for dots plus one space padding. */

	/* If this is the selected item, start its highlighting, and
	 * remember its location to be able to place the cursor on it. */
	if (i == selected) {
	    wattron(edit, hilite_attribute);
	    the_row = row;
	    the_column = col;
	}

	blank_row(edit, row, col, longest);

	/* If the name is too long, we display something like "...ename". */
	if (dots)
	    mvwaddstr(edit, row, col, "...");
	mvwaddstr(edit, row, dots ? col + 3 : col, disp);

	free(disp);

	col += longest;

	/* Show information about the file: "--" for symlinks (except when
	 * they point to a directory) and for files that have disappeared,
	 * "(dir)" for directories, and the file size for normal files. */
	if (lstat(filelist[i], &st) == -1 || S_ISLNK(st.st_mode)) {
	    if (stat(filelist[i], &st) == -1 || !S_ISDIR(st.st_mode))
		info = mallocstrcpy(NULL, "--");
	    else
		/* TRANSLATORS: Try to keep this at most 7 characters. */
		info = mallocstrcpy(NULL, _("(dir)"));
	} else if (S_ISDIR(st.st_mode)) {
	    if (strcmp(thename, "..") == 0) {
		/* TRANSLATORS: Try to keep this at most 12 characters. */
		info = mallocstrcpy(NULL, _("(parent dir)"));
		infomaxlen = 12;
	    } else
		info = mallocstrcpy(NULL, _("(dir)"));
	} else {
	    off_t result = st.st_size;
	    char modifier;

	    info = charalloc(infomaxlen + 1);

	    /* Massage the file size into a human-readable form. */
	    if (st.st_size < (1 << 10))
		modifier = ' ';  /* bytes */
	    else if (st.st_size < (1 << 20)) {
		result >>= 10;
		modifier = 'K';  /* kilobytes */
	    } else if (st.st_size < (1 << 30)) {
		result >>= 20;
		modifier = 'M';  /* megabytes */
	    } else {
		result >>= 30;
		modifier = 'G';  /* gigabytes */
	    }

	    /* Show the size if less than a terabyte, else show "(huge)". */
	    if (result < (1 << 10))
		sprintf(info, "%4ju %cB", (intmax_t)result, modifier);
	    else
		/* TRANSLATORS: Try to keep this at most 7 characters.
		 * If necessary, you can leave out the parentheses. */
		info = mallocstrcpy(info, _("(huge)"));
	}

	/* Make sure info takes up no more than infomaxlen columns. */
	infolen = strlenpt(info);
	if (infolen > infomaxlen) {
	    info[actual_x(info, infomaxlen)] = '\0';
	    infolen = infomaxlen;
	}

	mvwaddstr(edit, row, col - infolen, info);

	/* If this is the selected item, finish its highlighting. */
	if (i == selected)
	    wattroff(edit, hilite_attribute);

	free(info);

	/* Add some space between the columns. */
	col += 2;

	/* If the next entry isn't going to fit on the current row,
	 * move to the next row. */
	if (col > COLS - longest) {
	    row++;
	    col = 0;
	}
    }

    /* If requested, put the cursor on the selected item and switch it on. */
    if (ISSET(SHOW_CURSOR)) {
	wmove(edit, the_row, the_column);
	curs_set(1);
    }

    wnoutrefresh(edit);
}

/* Look for needle.  If we find it, set selected to its location.
 * Note that needle must be an exact match for a file in the list. */
void browser_select_dirname(const char *needle)
{
    size_t looking_at = 0;

    for (; looking_at < filelist_len; looking_at++) {
	if (strcmp(filelist[looking_at], needle) == 0) {
	    selected = looking_at;
	    break;
	}
    }

    /* If the sought name isn't found, move the highlight so that the
     * changed selection will be noticed. */
    if (looking_at == filelist_len) {
	--selected;

	/* Make sure we stay within the available range. */
	if (selected >= filelist_len)
	    selected = filelist_len - 1;
    }
}

/* Set up the system variables for a filename search.  Return -1 or -2 if
 * the search should be canceled (due to Cancel or a blank search string),
 * return 0 when we have a string, and return a positive value when some
 * function was run. */
int filesearch_init(void)
{
    int input;
    char *buf;

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
    input = do_prompt(FALSE, FALSE, MWHEREISFILE, NULL,
#ifndef DISABLE_HISTORIES
		&search_history,
#endif
		browser_refresh, "%s%s", _("Search"), buf);

    /* Release buf now that we don't need it anymore. */
    free(buf);

    /* If only Enter was pressed but we have a previous string, it's okay. */
    if (input == -2 && *last_search != '\0')
	return 0;

    /* Otherwise negative inputs are a bailout. */
    if (input < 0)
	statusbar(_("Cancelled"));

    return input;
}

/* Look for the given needle in the list of files. */
void findnextfile(const char *needle)
{
    size_t looking_at = selected;
	/* The location in the file list of the filename we're looking at. */
    const char *thename;
	/* The plain filename, without the path. */
    unsigned stash[sizeof(flags) / sizeof(flags[0])];
	/* A storage place for the current flag settings. */

    /* Save the settings of all flags. */
    memcpy(stash, flags, sizeof(flags));

    /* Search forward, case insensitive, and without regexes. */
    UNSET(BACKWARDS_SEARCH);
    UNSET(CASE_SENSITIVE);
    UNSET(USE_REGEXP);

    /* Step through each filename in the list until a match is found or
     * we've come back to the point where we started. */
    while (TRUE) {
	/* Move to the next filename in the list, or back to the first. */
	if (looking_at < filelist_len - 1)
	    looking_at++;
	else {
	    looking_at = 0;
	    statusbar(_("Search Wrapped"));
	}

	/* Get the bare filename, without the path. */
	thename = tail(filelist[looking_at]);

	/* If the needle matches, we're done.  And if we're back at the file
	 * where we started, it is the only occurrence. */
	if (strstrwrapper(thename, needle, thename)) {
	    if (looking_at == selected)
		statusbar(_("This is the only occurrence"));
	    break;
	}

	/* If we're back at the beginning and didn't find any match... */
	if (looking_at == selected) {
	    not_found_msg(needle);
	    break;
	}
    }

    /* Restore the settings of all flags. */
    memcpy(flags, stash, sizeof(flags));

    /* Select the one we've found. */
    selected = looking_at;
}

/* Search for a filename. */
void do_filesearch(void)
{
    /* If the user cancelled or jumped to first or last file, don't search. */
    if (filesearch_init() != 0)
	return;

    /* If answer is now "", copy last_search into answer. */
    if (*answer == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef DISABLE_HISTORIES
    /* If answer is not empty, add the string to the search history list. */
    if (*answer != '\0')
	update_history(&search_history, answer);
#endif

    findnextfile(answer);
}

/* Search again for the last given filename, without prompting. */
void do_fileresearch(void)
{
    if (*last_search == '\0')
	statusbar(_("No current search pattern"));
    else
	findnextfile(last_search);
}

/* Select the first file in the list. */
void do_first_file(void)
{
    selected = 0;
}

/* Select the last file in the list. */
void do_last_file(void)
{
    selected = filelist_len - 1;
}

/* Strip one element from the end of path, and return the stripped path.
 * The returned string is dynamically allocated, and should be freed. */
char *strip_last_component(const char *path)
{
    char *copy = mallocstrcpy(NULL, path);
    char *last_slash = strrchr(copy, '/');

    if (last_slash != NULL)
	*last_slash = '\0';

    return copy;
}

#endif /* !DISABLE_BROWSER */
