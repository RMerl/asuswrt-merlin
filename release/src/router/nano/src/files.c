/**************************************************************************
 *   files.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2017 Free Software Foundation, Inc.    *
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <libgen.h>

#define LOCKBUFSIZE 8192

/* Verify that the containing directory of the given filename exists. */
bool has_valid_path(const char *filename)
{
    char *namecopy = mallocstrcpy(NULL, filename);
    char *parentdir = dirname(namecopy);
    struct stat parentinfo;
    bool validity = FALSE;

    if (stat(parentdir, &parentinfo) == -1) {
	if (errno == ENOENT)
	    statusline(ALERT, _("Directory '%s' does not exist"), parentdir);
	else
	    statusline(ALERT, _("Path '%s': %s"), parentdir, strerror(errno));
    } else if (!S_ISDIR(parentinfo.st_mode))
	statusline(ALERT, _("Path '%s' is not a directory"), parentdir);
    else if (access(parentdir, X_OK) == -1)
	statusline(ALERT, _("Path '%s' is not accessible"), parentdir);
    else if (ISSET(LOCKING) && access(parentdir, W_OK) == -1)
	statusline(MILD, _("Directory '%s' is not writable"), parentdir);
    else
	validity = TRUE;

    free(namecopy);

    return validity;
}

/* Add an entry to the circular list of openfile structs. */
void make_new_buffer(void)
{
    if (openfile == NULL) {
	openfile = make_new_opennode();

	/* Make the first open file the only element in the list. */
	openfile->prev = openfile;
	openfile->next = openfile;
    } else {
	openfilestruct *newnode = make_new_opennode();

	/* Add the new open file after the current one in the list. */
	newnode->prev = openfile;
	newnode->next = openfile->next;
	openfile->next->prev = newnode;
	openfile->next = newnode;

	/* Make the new file the current one. */
	openfile = newnode;

	/* There is more than one file open: show Close in help lines. */
	exitfunc->desc = close_tag;
    }

    /* Start initializing the new buffer. */
    openfile->filename = mallocstrcpy(NULL, "");

    initialize_buffer_text();

    openfile->placewewant = 0;
    openfile->current_y = 0;

    openfile->modified = FALSE;
#ifndef NANO_TINY
    openfile->mark_set = FALSE;
    openfile->mark_begin = NULL;
    openfile->mark_begin_x = 0;
    openfile->kind_of_mark = SOFTMARK;

    openfile->fmt = NIX_FILE;

    openfile->undotop = NULL;
    openfile->current_undo = NULL;
    openfile->last_action = OTHER;

    openfile->current_stat = NULL;
    openfile->lock_filename = NULL;
#endif
#ifndef DISABLE_COLOR
    openfile->syntax = NULL;
    openfile->colorstrings = NULL;
#endif
}

/* Initialize the text of the current openfile struct. */
void initialize_buffer_text(void)
{
    assert(openfile != NULL);

    openfile->fileage = make_new_node(NULL);
    openfile->fileage->data = mallocstrcpy(NULL, "");

    openfile->filebot = openfile->fileage;
    openfile->edittop = openfile->fileage;
    openfile->current = openfile->fileage;

    openfile->firstcolumn = 0;
    openfile->current_x = 0;
    openfile->totsize = 0;
}

/* Mark the current file as modified if it isn't already, and then
 * update the titlebar to display the file's new status. */
void set_modified(void)
{
    if (openfile->modified)
	return;

    openfile->modified = TRUE;
    titlebar(NULL);

#ifndef NANO_TINY
    if (!ISSET(LOCKING) || openfile->filename[0] == '\0')
	return;

    if (openfile->lock_filename != NULL) {
	char *fullname = get_full_path(openfile->filename);

	write_lockfile(openfile->lock_filename, fullname, TRUE);
	free(fullname);
    }
#endif
}

#ifndef NANO_TINY
/* Actually write the lockfile.  This function will ALWAYS annihilate
 * any previous version of the file.  We'll borrow INSECURE_BACKUP here
 * to decide about lockfile paranoia here as well...
 *
 * Args:
 *     lockfilename: file name for lock
 *     origfilename: name of the file the lock is for
 *     modified: whether to set the modified bit in the file
 *
 * Returns 1 on success, and 0 on failure (but continue anyway). */
int write_lockfile(const char *lockfilename, const char *origfilename, bool modified)
{
#ifdef HAVE_PWD_H
    int cflags, fd;
    FILE *filestream;
    pid_t mypid;
    uid_t myuid;
    struct passwd *mypwuid;
    struct stat fileinfo;
    char *lockdata = charalloc(1024);
    char myhostname[32];
    size_t lockdatalen = 1024;
    size_t wroteamt;

    mypid = getpid();
    myuid = geteuid();

    /* First run things that might fail before blowing away the old state. */
    if ((mypwuid = getpwuid(myuid)) == NULL) {
	/* TRANSLATORS: Keep the next eight messages at most 76 characters. */
	statusline(MILD, _("Couldn't determine my identity for lock file "
				"(getpwuid() failed)"));
	goto free_the_data;
    }

    if (gethostname(myhostname, 31) < 0) {
	if (errno == ENAMETOOLONG)
	    myhostname[31] = '\0';
	else {
	    statusline(MILD, _("Couldn't determine hostname for lock file: %s"),
			strerror(errno));
	    goto free_the_data;
	}
    }

    /* If the lockfile exists, try to delete it. */
    if (stat(lockfilename, &fileinfo) != -1)
	if (delete_lockfile(lockfilename) < 0)
	    goto free_the_data;

    if (ISSET(INSECURE_BACKUP))
	cflags = O_WRONLY | O_CREAT | O_APPEND;
    else
	cflags = O_WRONLY | O_CREAT | O_EXCL | O_APPEND;

    /* Try to create the lockfile. */
    fd = open(lockfilename, cflags,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0) {
	statusline(MILD, _("Error writing lock file %s: %s"),
			lockfilename, strerror(errno));
	goto free_the_data;
    }

    /* Try to associate a stream with the now open lockfile. */
    filestream = fdopen(fd, "wb");

    if (filestream == NULL) {
	statusline(MILD, _("Error writing lock file %s: %s"), lockfilename,
			strerror(errno));
	goto free_the_data;
    }

    /* This is the lock data we will store:
     *
     * byte 0        - 0x62
     * byte 1        - 0x30
     * bytes 2-12    - program name which created the lock
     * bytes 24-27   - PID (little endian) of creator process
     * bytes 28-44   - username of who created the lock
     * bytes 68-100  - hostname of where the lock was created
     * bytes 108-876 - filename the lock is for
     * byte 1007     - 0x55 if file is modified
     *
     * Looks like VIM also stores undo state in this file, so we're
     * gonna have to figure out how to slap a 'OMG don't use recover on
     * our lockfile' message in here...
     *
     * This is likely very wrong, so this is a WIP. */
    memset(lockdata, 0, lockdatalen);
    lockdata[0] = 0x62;
    lockdata[1] = 0x30;
    lockdata[24] = mypid % 256;
    lockdata[25] = (mypid / 256) % 256;
    lockdata[26] = (mypid / (256 * 256)) % 256;
    lockdata[27] = mypid / (256 * 256 * 256);
    snprintf(&lockdata[2], 11, "nano %s", VERSION);
    strncpy(&lockdata[28], mypwuid->pw_name, 16);
    strncpy(&lockdata[68], myhostname, 31);
    if (origfilename != NULL)
	strncpy(&lockdata[108], origfilename, 768);
    if (modified == TRUE)
	lockdata[1007] = 0x55;

    wroteamt = fwrite(lockdata, sizeof(char), lockdatalen, filestream);
    if (wroteamt < lockdatalen) {
	statusline(MILD, _("Error writing lock file %s: %s"),
			lockfilename, ferror(filestream));
	goto free_the_data;
    }

#ifdef DEBUG
    fprintf(stderr, "In write_lockfile(), write successful (wrote %lu bytes)\n", (unsigned long)wroteamt);
#endif

    if (fclose(filestream) == EOF) {
	statusline(MILD, _("Error writing lock file %s: %s"),
			lockfilename, strerror(errno));
	goto free_the_data;
    }

    openfile->lock_filename = (char *) lockfilename;

    free(lockdata);
    return 1;

  free_the_data:
    free(lockdata);
    return 0;
#else
    return 1;
#endif
}

/* Delete the lockfile.  Return -1 if unsuccessful, and 1 otherwise. */
int delete_lockfile(const char *lockfilename)
{
    if (unlink(lockfilename) < 0 && errno != ENOENT) {
	statusline(MILD, _("Error deleting lock file %s: %s"), lockfilename,
			strerror(errno));
	return -1;
    }
    return 1;
}

/* Deal with lockfiles.  Return -1 on refusing to override the lockfile,
 * and 1 on successfully creating it; 0 means we were not successful in
 * creating the lockfile but we should continue to load the file. */
int do_lockfile(const char *filename)
{
    char *namecopy = (char *) mallocstrcpy(NULL, filename);
    char *secondcopy = (char *) mallocstrcpy(NULL, filename);
    size_t locknamesize = strlen(filename) + strlen(locking_prefix)
		+ strlen(locking_suffix) + 3;
    char *lockfilename = charalloc(locknamesize);
    static char lockprog[11], lockuser[17];
    struct stat fileinfo;
    int lockfd, lockpid, retval = -1;

    snprintf(lockfilename, locknamesize, "%s/%s%s%s", dirname(namecopy),
		locking_prefix, basename(secondcopy), locking_suffix);
    free(secondcopy);
#ifdef DEBUG
    fprintf(stderr, "lock file name is %s\n", lockfilename);
#endif
    if (stat(lockfilename, &fileinfo) != -1) {
	size_t readtot = 0;
	size_t readamt = 0;
	char *lockbuf, *question, *pidstring, *postedname, *promptstr;
	int room, response;

	if ((lockfd = open(lockfilename, O_RDONLY)) < 0) {
	    statusline(MILD, _("Error opening lock file %s: %s"),
			lockfilename, strerror(errno));
	    goto free_the_name;
	}

	lockbuf = charalloc(LOCKBUFSIZE);
	do {
	    readamt = read(lockfd, &lockbuf[readtot], LOCKBUFSIZE - readtot);
	    readtot += readamt;
	} while (readamt > 0 && readtot < LOCKBUFSIZE);

	close(lockfd);

	if (readtot < 48) {
	    statusline(MILD, _("Error reading lock file %s: "
			"Not enough data read"), lockfilename);
	    free(lockbuf);
	    goto free_the_name;
	}

	strncpy(lockprog, &lockbuf[2], 10);
	lockpid = (((unsigned char)lockbuf[27] * 256 + (unsigned char)lockbuf[26]) * 256 +
			(unsigned char)lockbuf[25]) * 256 + (unsigned char)lockbuf[24];
	strncpy(lockuser, &lockbuf[28], 16);
	free(lockbuf);

	pidstring = charalloc(11);
	sprintf (pidstring, "%u", (unsigned int)lockpid);

#ifdef DEBUG
	fprintf(stderr, "lockpid = %d\n", lockpid);
	fprintf(stderr, "program name which created this lock file should be %s\n", lockprog);
	fprintf(stderr, "user which created this lock file should be %s\n", lockuser);
#endif
	/* TRANSLATORS: The second %s is the name of the user, the third that of the editor. */
	question = _("File %s is being edited (by %s with %s, PID %s); continue?");
	room = COLS - strlenpt(question) + 7 - strlenpt(lockuser) -
				strlenpt(lockprog) - strlenpt(pidstring);
	if (room < 4)
	    postedname = mallocstrcpy(NULL, "_");
	else if (room < strlenpt(filename)) {
	    char *fragment = display_string(filename,
				strlenpt(filename) - room + 3, room, FALSE);
	    postedname = charalloc(strlen(fragment) + 4);
	    strcpy(postedname, "...");
	    strcat(postedname, fragment);
	    free(fragment);
	} else
	    postedname = mallocstrcpy(NULL, filename);

	/* Allow extra space for username (14), program name (8), PID (8),
	 * and terminating \0 (1), minus the %s (2) for the file name. */
	promptstr = charalloc(strlen(question) + 29 + strlen(postedname));
	sprintf(promptstr, question, postedname, lockuser, lockprog, pidstring);
	free(postedname);
	free(pidstring);

	response = do_yesno_prompt(FALSE, promptstr);
	free(promptstr);

	if (response < 1) {
	    blank_statusbar();
	    goto free_the_name;
	}
    }

    retval = write_lockfile(lockfilename, filename, FALSE);

  free_the_name:
    free(namecopy);
    if (retval < 1)
	free(lockfilename);

    return retval;
}

/* Perform a stat call on the given filename, allocating a stat struct
 * if necessary.  On success, *pstat points to the stat's result.  On
 * failure, *pstat is freed and made NULL. */
void stat_with_alloc(const char *filename, struct stat **pstat)
{
    if (*pstat == NULL)
	*pstat = (struct stat *)nmalloc(sizeof(struct stat));

    if (stat(filename, *pstat) != 0) {
	free(*pstat);
	*pstat = NULL;
    }
}
#endif /* !NANO_TINY */

/* This does one of three things.  If the filename is "", just create a new
 * empty buffer.  Otherwise, read the given file into the existing buffer,
 * or into a new buffer when MULTIBUFFER is set or there is no buffer yet. */
bool open_buffer(const char *filename, bool undoable)
{
    bool new_buffer = (openfile == NULL || ISSET(MULTIBUFFER));
	/* Whether we load into the current buffer or a new one. */
    char *realname;
	/* The filename after tilde expansion. */
    FILE *f;
    int rc;
	/* rc == -2 means that we have a new file.  -1 means that the
	 * open() failed.  0 means that the open() succeeded. */

    assert(filename != NULL);

    /* Display newlines in filenames as ^J. */
    as_an_at = FALSE;

#ifndef DISABLE_OPERATINGDIR
    if (check_operating_dir(filename, FALSE)) {
	statusline(ALERT, _("Can't insert file from outside of %s"),
				full_operating_dir);
	return FALSE;
    }
#endif

    realname = real_dir_from_tilde(filename);

    /* When the specified filename is not empty, and the corresponding
     * file exists, verify that it is a normal file. */
    if (strcmp(filename, "") != 0) {
	struct stat fileinfo;

	if (stat(realname, &fileinfo) == 0 && !S_ISREG(fileinfo.st_mode)) {
	    if (S_ISDIR(fileinfo.st_mode))
		statusline(ALERT, _("\"%s\" is a directory"), realname);
	    else
		statusline(ALERT, _("\"%s\" is not a normal file"), realname);
	    free(realname);
	    return FALSE;
	}
    }

    /* If we're going to load into a new buffer, first create the new
     * buffer and (if possible) lock the corresponding file. */
    if (new_buffer) {
	make_new_buffer();

	if (has_valid_path(realname)) {
#ifndef NANO_TINY
	    if (ISSET(LOCKING) && filename[0] != '\0') {
		/* When not overriding an existing lock, discard the buffer. */
		if (do_lockfile(realname) < 0) {
#ifndef DISABLE_MULTIBUFFER
		    close_buffer();
#endif
		    free(realname);
		    return FALSE;
		}
	    }
#endif /* !NANO_TINY */
	}
    }

    /* If the filename isn't blank, and we are not in NOREAD_MODE,
     * open the file.  Otherwise, treat it as a new file. */
    rc = (filename[0] != '\0' && !ISSET(NOREAD_MODE)) ?
		open_file(realname, new_buffer, FALSE, &f) : -2;

    /* If we have a file, and we're loading into a new buffer, update
     * the filename. */
    if (rc != -1 && new_buffer)
	openfile->filename = mallocstrcpy(openfile->filename, realname);

    /* If we have a non-new file, read it in.  Then, if the buffer has
     * no stat, update the stat, if applicable. */
    if (rc > 0) {
	read_file(f, rc, realname, undoable, new_buffer);
#ifndef NANO_TINY
	if (openfile->current_stat == NULL)
	    stat_with_alloc(realname, &openfile->current_stat);
#endif
    }

    /* If we have a file, and we're loading into a new buffer, move back
     * to the beginning of the first line of the buffer. */
    if (rc != -1 && new_buffer) {
	openfile->current = openfile->fileage;
	openfile->current_x = 0;
	openfile->placewewant = 0;
    }

#ifndef DISABLE_COLOR
    /* If we're loading into a new buffer, update the colors to account
     * for it, if applicable. */
    if (new_buffer)
	color_update();
#endif
    free(realname);
    return TRUE;
}

#ifndef DISABLE_SPELLER
/* Open the specified file, and if that succeeds, blow away the text of
 * the current buffer and read the file contents into its place. */
void replace_buffer(const char *filename)
{
    FILE *f;
    int descriptor;

    assert(filename != NULL && filename[0] != '\0');

    /* Open the file quietly. */
    descriptor = open_file(filename, FALSE, TRUE, &f);

    /* If opening failed, forget it. */
    if (descriptor < 0)
	return;

    /* Reinitialize the text of the current buffer. */
    free_filestruct(openfile->fileage);
    initialize_buffer_text();

    /* Insert the processed file into its place. */
    read_file(f, descriptor, filename, FALSE, TRUE);

    /* Put current at a place that is certain to exist. */
    openfile->current = openfile->fileage;
}

#ifndef NANO_TINY
/* Open the specified file, and if that succeeds, blow away the text of
 * the current buffer at the given coordinates and read the file
 * contents into its place. */
void replace_marked_buffer(const char *filename, filestruct *top, size_t top_x,
	filestruct *bot, size_t bot_x)
{
    FILE *f;
    int descriptor;
    bool old_no_newlines = ISSET(NO_NEWLINES);
    filestruct *trash_top = NULL;
    filestruct *trash_bot = NULL;

    descriptor = open_file(filename, FALSE, TRUE, &f);

    if (descriptor < 0)
	return;

    /* Don't add a magicline when replacing text in the buffer. */
    SET(NO_NEWLINES);

    /* Throw away the text under the mark, and insert the processed file
     * where the marked text was. */
    extract_buffer(&trash_top, &trash_bot, top, top_x, bot, bot_x);
    free_filestruct(trash_top);
    read_file(f, descriptor, filename, FALSE, TRUE);

    /* Restore the magicline behavior now that we're done fiddling. */
    if (!old_no_newlines)
	UNSET(NO_NEWLINES);
}
#endif /* !ENABLE_TINY */
#endif /* !DISABLE_SPELLER */

/* Update the screen to account for the current buffer. */
void display_buffer(void)
{
    /* Update the titlebar, since the filename may have changed. */
    titlebar(NULL);

#ifndef DISABLE_COLOR
    /* Make sure we're using the buffer's associated colors. */
    color_init();

    /* If there are multiline coloring regexes, and there is no
     * multiline cache data yet, precalculate it now. */
    if (openfile->syntax && openfile->syntax->nmultis > 0 &&
		openfile->fileage->multidata == NULL)
	precalc_multicolorinfo();
#endif

    /* Update the content of the edit window straightaway. */
    edit_refresh();
}

#ifndef DISABLE_MULTIBUFFER
/* Switch to a neighbouring file buffer; to the next if to_next is TRUE;
 * otherwise, to the previous one. */
void switch_to_prevnext_buffer(bool to_next)
{
    assert(openfile != NULL);

    /* If only one file buffer is open, say so and get out. */
    if (openfile == openfile->next) {
	statusbar(_("No more open file buffers"));
	return;
    }

    /* Switch to the next or previous file buffer. */
    openfile = to_next ? openfile->next : openfile->prev;

#ifdef DEBUG
    fprintf(stderr, "filename is %s\n", openfile->filename);
#endif

#ifndef NANO_TINY
    /* When not in softwrap mode, make sure firstcolumn is zero.  It might
     * be nonzero if we had softwrap mode on while in this buffer, and then
     * turned softwrap mode off while in a different buffer. */
    if (!ISSET(SOFTWRAP))
	openfile->firstcolumn = 0;
#endif

    /* Update the screen to account for the current buffer. */
    display_buffer();

    /* Indicate the switch on the statusbar. */
    statusline(HUSH, _("Switched to %s"),
		((openfile->filename[0] == '\0') ?
		_("New Buffer") : openfile->filename));

#ifdef DEBUG
    dump_filestruct(openfile->current);
#endif
}

/* Switch to the previous entry in the openfile filebuffer. */
void switch_to_prev_buffer_void(void)
{
    switch_to_prevnext_buffer(FALSE);
}

/* Switch to the next entry in the openfile filebuffer. */
void switch_to_next_buffer_void(void)
{
    switch_to_prevnext_buffer(TRUE);
}

/* Delete an entry from the circular list of open files, and switch to the
 * one after it.  Return TRUE on success, and FALSE if there are no other
 * open buffers. */
bool close_buffer(void)
{
    assert(openfile != NULL);

    /* If only one file buffer is open, get out. */
    if (openfile == openfile->next)
	return FALSE;

#ifndef DISABLE_HISTORIES
    if (ISSET(POS_HISTORY))
	update_poshistory(openfile->filename,
			openfile->current->lineno, xplustabs() + 1);
#endif

    /* Switch to the next file buffer. */
    switch_to_prevnext_buffer(TRUE);

    /* Close the file buffer we had open before. */
    unlink_opennode(openfile->prev);

    /* If only one buffer is open now, show Exit in the help lines. */
    if (openfile == openfile->next)
	exitfunc->desc = exit_tag;

    return TRUE;
}
#endif /* !DISABLE_MULTIBUFFER */

/* Do a quick permissions check by verifying whether the file is appendable.
 * Err on the side of permissiveness (reporting TRUE when it might be wrong)
 * to not fluster users editing on odd filesystems by printing incorrect
 * warnings. */
int is_file_writable(const char *filename)
{
    struct stat fileinfo, fileinfo2;
    int fd;
    FILE *f;
    char *full_filename;
    bool result = TRUE;

    if (ISSET(VIEW_MODE))
	return TRUE;

    assert(filename != NULL);

    /* Get the specified file's full path. */
    full_filename = get_full_path(filename);

    /* Okay, if we can't stat the absolute path due to some component's
     * permissions, just try the relative one. */
    if (full_filename == NULL ||
		(stat(full_filename, &fileinfo) == -1 && stat(filename, &fileinfo2) != -1))
	full_filename = mallocstrcpy(NULL, filename);

    if ((fd = open(full_filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR |
		S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1 ||
		(f = fdopen(fd, "a")) == NULL)
	result = FALSE;
    else
	fclose(f);

    close(fd);
    free(full_filename);

    return result;
}

/* Encode any NUL bytes in the given line of text, which is of length buf_len,
 * and return a dynamically allocated copy of the resultant string. */
char *encode_data(char *buf, size_t buf_len)
{
    unsunder(buf, buf_len);
    buf[buf_len] = '\0';

    return mallocstrcpy(NULL, buf);
}

/* Read an open file into the current buffer.  f should be set to the
 * open file, and filename should be set to the name of the file.
 * undoable means do we want to create undo records to try and undo
 * this.  Will also attempt to check file writability if fd > 0 and
 * checkwritable == TRUE. */
void read_file(FILE *f, int fd, const char *filename, bool undoable,
		bool checkwritable)
{
    ssize_t was_lineno = openfile->current->lineno;
	/* The line number where we start the insertion. */
    size_t was_leftedge = 0;
	/* The leftedge where we start the insertion. */
    size_t num_lines = 0;
	/* The number of lines in the file. */
    size_t len = 0;
	/* The length of the current line of the file. */
    char input = '\0';
	/* The current input character. */
    char *buf;
	/* The buffer in which we assemble each line of the file. */
    size_t bufx = MAX_BUF_SIZE;
	/* The allocated size of the line buffer; increased as needed. */
    filestruct *topline;
	/* The top of the new buffer where we store the read file. */
    filestruct *bottomline;
	/* The bottom of the new buffer. */
    int input_int;
	/* The current value we read from the file, whether an input
	 * character or EOF. */
    bool writable = TRUE;
	/* Whether the file is writable (in case we care). */
#ifndef NANO_TINY
    int format = 0;
	/* 0 = *nix, 1 = DOS, 2 = Mac, 3 = both DOS and Mac. */
#endif

    buf = charalloc(bufx);

#ifndef NANO_TINY
    if (undoable)
	add_undo(INSERT);

    if (ISSET(SOFTWRAP))
	was_leftedge = (xplustabs() / editwincols) * editwincols;
#endif

    /* Create an empty buffer. */
    topline = make_new_node(NULL);
    bottomline = topline;

    /* Read the entire file into the new buffer. */
    while ((input_int = getc(f)) != EOF) {
	input = (char)input_int;

	/* If it's a *nix file ("\n") or a DOS file ("\r\n"), and file
	 * conversion isn't disabled, handle it! */
	if (input == '\n') {
#ifndef NANO_TINY
	    /* If it's a DOS file or a DOS/Mac file ('\r' before '\n' on
	     * the first line if we think it's a *nix file, or on any
	     * line otherwise), and file conversion isn't disabled,
	     * handle it! */
	    if ((num_lines == 0 || format != 0) && !ISSET(NO_CONVERT) &&
			len > 0 && buf[len - 1] == '\r') {
		if (format == 0 || format == 2)
		    format++;
	    }
	/* If it's a Mac file ('\r' without '\n' on the first line if we
	 * think it's a *nix file, or on any line otherwise), and file
	 * conversion isn't disabled, handle it! */
	} else if ((num_lines == 0 || format != 0) && !ISSET(NO_CONVERT) &&
			len > 0 && buf[len - 1] == '\r') {
	    /* If we currently think the file is a *nix file, set format
	     * to Mac.  If we currently think the file is a DOS file,
	     * set format to both DOS and Mac. */
	    if (format == 0 || format == 1)
		format += 2;
#endif
	} else {
	    /* Store the character. */
	    buf[len] = input;

	    /* Keep track of the total length of the line.  It might have
	     * nulls in it, so we can't just use strlen() later. */
	    len++;

	    /* If needed, increase the buffer size, MAX_BUF_SIZE characters at
	     * a time.  Don't bother decreasing it; it is freed at the end. */
	    if (len == bufx) {
		bufx += MAX_BUF_SIZE;
		buf = charealloc(buf, bufx);
	    }
	    continue;
	}

#ifndef NANO_TINY
	/* If it's a DOS or Mac line, strip the '\r' from it. */
	if (len > 0 && buf[len - 1] == '\r' && !ISSET(NO_CONVERT))
	    buf[--len] = '\0';
#endif

	/* Store the data and make a new line. */
	bottomline->data = encode_data(buf, len);
	bottomline->next = make_new_node(bottomline);
	bottomline = bottomline->next;
	num_lines++;

	/* Reset the length in preparation for the next line. */
	len = 0;

#ifndef NANO_TINY
	/* If it happens to be a Mac line, store the character after the \r
	 * as the first character of the next line. */
	if (input != '\n')
	    buf[len++] = input;
#endif
    }

    /* Perhaps this could use some better handling. */
    if (ferror(f))
	nperror(filename);
    fclose(f);
    if (fd > 0 && checkwritable) {
	close(fd);
	writable = is_file_writable(filename);
    }

    /* If the file ended with newline, or it was entirely empty, make the
     * last line blank.  Otherwise, put the last read data in. */
    if (len == 0)
	bottomline->data = mallocstrcpy(NULL, "");
    else {
	bool mac_line_needs_newline = FALSE;

#ifndef NANO_TINY
	/* If the final character is '\r', and file conversion isn't disabled,
	 * set format to Mac if we currently think the file is a *nix file, or
	 * to DOS-and-Mac if we currently think it is a DOS file. */
	if (buf[len - 1] == '\r' && !ISSET(NO_CONVERT)) {
	    if (format < 2)
		format += 2;

	    /* Strip the carriage return. */
	    buf[--len] = '\0';

	    /* Indicate we need to put a blank line in after this one. */
	    mac_line_needs_newline = TRUE;
	}
#endif
	/* Store the data of the final line. */
	bottomline->data = encode_data(buf, len);
	num_lines++;

	if (mac_line_needs_newline) {
	    bottomline->next = make_new_node(bottomline);
	    bottomline = bottomline->next;
	    bottomline->data = mallocstrcpy(NULL, "");
	}
    }

    free(buf);

    /* Insert the just read buffer into the current one. */
    ingraft_buffer(topline);

    /* Set the desired x position at the end of what was inserted. */
    openfile->placewewant = xplustabs();

    if (!writable)
	statusline(ALERT, _("File '%s' is unwritable"), filename);
#ifndef NANO_TINY
    else if (format == 3) {
	/* TRANSLATORS: Keep the next four messages at most 78 characters. */
	statusline(HUSH, P_("Read %lu line (Converted from DOS and Mac format)",
			"Read %lu lines (Converted from DOS and Mac format)",
			(unsigned long)num_lines), (unsigned long)num_lines);
    } else if (format == 2) {
	openfile->fmt = MAC_FILE;
	statusline(HUSH, P_("Read %lu line (Converted from Mac format)",
			"Read %lu lines (Converted from Mac format)",
			(unsigned long)num_lines), (unsigned long)num_lines);
    } else if (format == 1) {
	openfile->fmt = DOS_FILE;
	statusline(HUSH, P_("Read %lu line (Converted from DOS format)",
			"Read %lu lines (Converted from DOS format)",
			(unsigned long)num_lines), (unsigned long)num_lines);
    }
#endif
    else
	statusline(HUSH, P_("Read %lu line", "Read %lu lines",
			(unsigned long)num_lines), (unsigned long)num_lines);

    /* If we inserted less than a screenful, don't center the cursor. */
    if (less_than_a_screenful(was_lineno, was_leftedge))
	focusing = FALSE;

#ifndef NANO_TINY
    if (undoable)
	update_undo(INSERT);

    if (ISSET(MAKE_IT_UNIX))
	openfile->fmt = NIX_FILE;
#endif
}

/* Open the file with the given name.  If the file does not exist, display
 * "New File" if newfie is TRUE, and say "File not found" otherwise.
 * Return -2 if we say "New File", -1 if the file isn't opened, and the
 * obtained fd otherwise.  *f is set to the opened file. */
int open_file(const char *filename, bool newfie, bool quiet, FILE **f)
{
    struct stat fileinfo, fileinfo2;
    int fd;
    char *full_filename;

    assert(filename != NULL && f != NULL);

    /* Get the specified file's full path. */
    full_filename = get_full_path(filename);

    /* Okay, if we can't stat the path due to a component's
     * permissions, just try the relative one. */
    if (full_filename == NULL || (stat(full_filename, &fileinfo) == -1 &&
					stat(filename, &fileinfo2) != -1))
	full_filename = mallocstrcpy(full_filename, filename);

    if (stat(full_filename, &fileinfo) == -1) {
	if (newfie) {
	    if (!quiet)
		statusbar(_("New File"));
	    free(full_filename);
	    return -2;
	}

	statusline(ALERT, _("File \"%s\" not found"), filename);
	free(full_filename);
	return -1;
    }

    /* Don't open directories, character files, or block files. */
    if (S_ISDIR(fileinfo.st_mode) || S_ISCHR(fileinfo.st_mode) ||
				S_ISBLK(fileinfo.st_mode)) {
	statusline(ALERT, S_ISDIR(fileinfo.st_mode) ?
			_("\"%s\" is a directory") :
			_("\"%s\" is a device file"), filename);
	free(full_filename);
	return -1;
    }

    /* Try opening the file. */
    fd = open(full_filename, O_RDONLY);

    if (fd == -1)
	statusline(ALERT, _("Error reading %s: %s"), filename, strerror(errno));
    else {
	/* The file is A-OK.  Associate a stream with it. */
	*f = fdopen(fd, "rb");

	if (*f == NULL) {
	    statusline(ALERT, _("Error reading %s: %s"), filename, strerror(errno));
	    close(fd);
	} else
	    statusbar(_("Reading File"));
    }

    free(full_filename);

    return fd;
}

/* This function will return the name of the first available extension
 * of a filename (starting with [name][suffix], then [name][suffix].1,
 * etc.).  Memory is allocated for the return value.  If no writable
 * extension exists, we return "". */
char *get_next_filename(const char *name, const char *suffix)
{
    unsigned long i = 0;
    char *buf;
    size_t wholenamelen;

    assert(name != NULL && suffix != NULL);

    wholenamelen = strlen(name) + strlen(suffix);

    /* Reserve space for: the name plus the suffix plus a dot plus
     * possibly five digits plus a null byte. */
    buf = charalloc(wholenamelen + 7);
    sprintf(buf, "%s%s", name, suffix);

    while (TRUE) {
	struct stat fs;

	if (stat(buf, &fs) == -1)
	    return buf;

	/* Limit the number of backup files to a hundred thousand. */
	if (++i == 100000)
	    break;

	sprintf(buf + wholenamelen, ".%lu", i);
    }

    /* There is no possible save file: blank out the filename. */
    *buf = '\0';

    return buf;
}

/* Insert a file into the current buffer, or into a new buffer when
 * the MULTIBUFFER flag is set. */
void do_insertfile(void)
{
    int i;
    const char *msg;
    char *given = mallocstrcpy(NULL, "");
	/* The last answer the user typed at the statusbar prompt. */
#ifndef NANO_TINY
    bool execute = FALSE;
#endif

    /* Display newlines in filenames as ^J. */
    as_an_at = FALSE;

    while (TRUE) {
#ifndef NANO_TINY
	if (execute) {
#ifndef DISABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER))
		/* TRANSLATORS: The next four messages are prompts. */
		msg = _("Command to execute in new buffer");
	    else
#endif
		msg = _("Command to execute");
	} else
#endif /* NANO_TINY */
	{
#ifndef DISABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER))
		msg = _("File to insert into new buffer [from %s]");
	    else
#endif
		msg = _("File to insert [from %s]");
	}

	present_path = mallocstrcpy(present_path, "./");

	i = do_prompt(TRUE, TRUE,
#ifndef NANO_TINY
		execute ? MEXTCMD :
#endif
		MINSERTFILE, given,
#ifndef DISABLE_HISTORIES
		NULL,
#endif
		edit_refresh, msg,
#ifndef DISABLE_OPERATINGDIR
		operating_dir != NULL ? operating_dir :
#endif
		"./");

	/* If we're in multibuffer mode and the filename or command is
	 * blank, open a new buffer instead of canceling. */
	if (i == -1 || (i == -2 && !ISSET(MULTIBUFFER))) {
	    statusbar(_("Cancelled"));
	    break;
	} else {
	    ssize_t was_current_lineno = openfile->current->lineno;
	    size_t was_current_x = openfile->current_x;
#if !defined(NANO_TINY) || !defined(DISABLE_BROWSER)
	    functionptrtype func = func_from_key(&i);
#endif
	    given = mallocstrcpy(given, answer);

#ifndef NANO_TINY
#ifndef DISABLE_MULTIBUFFER
	    if (func == new_buffer_void) {
		/* Don't allow toggling when in view mode. */
		if (!ISSET(VIEW_MODE))
		    TOGGLE(MULTIBUFFER);
		else
		    beep();
		continue;
	    }
#endif
	    if (func == flip_execute_void) {
		execute = !execute;
		continue;
	    }
#endif /* !NANO_TINY */

#ifndef DISABLE_BROWSER
	    if (func == to_files_void) {
		char *chosen = do_browse_from(answer);

		/* If no file was chosen, go back to the prompt. */
		if (chosen == NULL)
		    continue;

		free(answer);
		answer = chosen;
		i = 0;
	    }
#endif
	    /* If we don't have a file yet, go back to the prompt. */
	    if (i != 0 && (!ISSET(MULTIBUFFER) || i != -2))
		continue;

#ifndef NANO_TINY
	    if (execute) {
#ifndef DISABLE_MULTIBUFFER
		/* When in multibuffer mode, first open a blank buffer. */
		if (ISSET(MULTIBUFFER))
		    open_buffer("", FALSE);
#endif
		/* Save the command's output in the current buffer. */
		execute_command(answer);

#ifndef DISABLE_MULTIBUFFER
		/* If this is a new buffer, put the cursor at the top. */
		if (ISSET(MULTIBUFFER)) {
		    openfile->current = openfile->fileage;
		    openfile->current_x = 0;
		    openfile->placewewant = 0;

		    set_modified();
		}
#endif
	    } else
#endif /* !NANO_TINY */
	    {
		/* Make sure the path to the file specified in answer is
		 * tilde-expanded. */
		answer = free_and_assign(answer, real_dir_from_tilde(answer));

		/* Save the file specified in answer in the current buffer. */
		open_buffer(answer, TRUE);
	    }

#ifndef DISABLE_MULTIBUFFER
	    if (ISSET(MULTIBUFFER)) {
#ifndef DISABLE_HISTORIES
		if (ISSET(POS_HISTORY)) {
		    ssize_t priorline, priorcol;
#ifndef NANO_TINY
		    if (!execute)
#endif
		    if (has_old_position(answer, &priorline, &priorcol))
			do_gotolinecolumn(priorline, priorcol, FALSE, FALSE);
		}
#endif /* !DISABLE_HISTORIES */
		/* Update stuff to account for the current buffer. */
		display_buffer();
	    } else
#endif /* !DISABLE_MULTIBUFFER */
	    {
		/* Mark the file as modified if it changed. */
		if (openfile->current->lineno != was_current_lineno ||
			openfile->current_x != was_current_x)
		    set_modified();

		/* Update the cursor position to account for inserted lines. */
		reset_cursor();

		refresh_needed = TRUE;
	    }

	    break;
	}
    }

    free(given);
}

/* If the current mode of operation allows it, go insert a file. */
void do_insertfile_void(void)
{
    if (ISSET(RESTRICTED))
	show_restricted_warning();
#ifndef DISABLE_MULTIBUFFER
    else if (ISSET(VIEW_MODE) && !ISSET(MULTIBUFFER))
	statusbar(_("Key invalid in non-multibuffer mode"));
#endif
    else
	do_insertfile();
}

/* When passed "[relative path]" or "[relative path][filename]" in
 * origpath, return "[full path]" or "[full path][filename]" on success,
 * or NULL on error.  Do this if the file doesn't exist but the relative
 * path does, since the file could exist in memory but not yet on disk).
 * Don't do this if the relative path doesn't exist, since we won't be
 * able to go there. */
char *get_full_path(const char *origpath)
{
    int attempts = 0;
	/* How often we've tried climbing back up the tree. */
    struct stat fileinfo;
    char *currentdir, *d_here, *d_there, *d_there_file = NULL;
    char *last_slash;
    bool path_only;

    if (origpath == NULL)
	return NULL;

    /* Get the current directory.  If it doesn't exist, back up and try
     * again until we get a directory that does, and use that as the
     * current directory. */
    currentdir = charalloc(PATH_MAX + 1);
    d_here = getcwd(currentdir, PATH_MAX + 1);

    while (d_here == NULL && attempts < 20) {
	IGNORE_CALL_RESULT(chdir(".."));
	d_here = getcwd(currentdir, PATH_MAX + 1);
	attempts++;
    }

    /* If we succeeded, canonicalize it in d_here. */
    if (d_here != NULL) {
	/* If the current directory isn't "/", tack a slash onto the end
	 * of it. */
	if (strcmp(d_here, "/") != 0) {
	    d_here = charealloc(d_here, strlen(d_here) + 2);
	    strcat(d_here, "/");
	}
    /* Otherwise, set d_here to "". */
    } else {
	d_here = mallocstrcpy(NULL, "");
	free(currentdir);
    }

    d_there = real_dir_from_tilde(origpath);

    /* If stat()ing d_there fails, assume that d_there refers to a new
     * file that hasn't been saved to disk yet.  Set path_only to TRUE
     * if d_there refers to a directory, and FALSE otherwise. */
    path_only = (stat(d_there, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));

    /* If path_only is TRUE, make sure d_there ends in a slash. */
    if (path_only) {
	size_t d_there_len = strlen(d_there);

	if (d_there[d_there_len - 1] != '/') {
	    d_there = charealloc(d_there, d_there_len + 2);
	    strcat(d_there, "/");
	}
    }

    /* Search for the last slash in d_there. */
    last_slash = strrchr(d_there, '/');

    /* If we didn't find one, then make sure the answer is in the format
     * "d_here/d_there". */
    if (last_slash == NULL) {
	assert(!path_only);

	d_there_file = d_there;
	d_there = d_here;
    } else {
	/* If path_only is FALSE, then save the filename portion of the
	 * answer (everything after the last slash) in d_there_file. */
	if (!path_only)
	    d_there_file = mallocstrcpy(NULL, last_slash + 1);

	/* Remove the filename portion of the answer from d_there. */
	*(last_slash + 1) = '\0';

	/* Go to the path specified in d_there. */
	if (chdir(d_there) == -1) {
	    free(d_there);
	    d_there = NULL;
	} else {
	    free(d_there);

	    /* Get the full path. */
	    currentdir = charalloc(PATH_MAX + 1);
	    d_there = getcwd(currentdir, PATH_MAX + 1);

	    /* If we succeeded, canonicalize it in d_there. */
	    if (d_there != NULL) {
		/* If the current directory isn't "/", tack a slash onto
		 * the end of it. */
		if (strcmp(d_there, "/") != 0) {
		    d_there = charealloc(d_there, strlen(d_there) + 2);
		    strcat(d_there, "/");
		}
	    /* Otherwise, make sure that we return NULL. */
	    } else {
		path_only = TRUE;
		free(currentdir);
	    }

	    /* Finally, go back to the path specified in d_here,
	     * where we were before.  We don't check for a chdir()
	     * error, since we can do nothing if we get one. */
	    IGNORE_CALL_RESULT(chdir(d_here));
	}

	/* Free d_here, since we're done using it. */
	free(d_here);
    }

    /* At this point, if path_only is FALSE and d_there isn't NULL,
     * d_there contains the path portion of the answer and d_there_file
     * contains the filename portion of the answer.  If this is the
     * case, tack the latter onto the end of the former.  d_there will
     * then contain the complete answer. */
    if (!path_only && d_there != NULL) {
	d_there = charealloc(d_there, strlen(d_there) +
		strlen(d_there_file) + 1);
	strcat(d_there, d_there_file);
    }

    /* Free d_there_file, since we're done using it. */
    free(d_there_file);

    return d_there;
}

/* Return the full version of path, as returned by get_full_path().  On
 * error, or if path doesn't reference a directory, or if the directory
 * isn't writable, return NULL. */
char *check_writable_directory(const char *path)
{
    char *full_path = get_full_path(path);

    if (full_path == NULL)
	return NULL;

    /* If we can't write to path or path isn't a directory, return NULL. */
    if (access(full_path, W_OK) != 0 ||
		full_path[strlen(full_path) - 1] != '/') {
	free(full_path);
	return NULL;
    }

    return full_path;
}

/* This function calls mkstemp(($TMPDIR|P_tmpdir|/tmp/)"nano.XXXXXX").
 * On success, it returns the malloc()ed filename and corresponding FILE
 * stream, opened in "r+b" mode.  On error, it returns NULL for the
 * filename and leaves the FILE stream unchanged. */
char *safe_tempfile(FILE **f)
{
    char *full_tempdir = NULL;
    const char *tmpdir_env;
    int fd;
    mode_t original_umask = 0;

    assert(f != NULL);

    /* If $TMPDIR is set, set tempdir to it, run it through
     * get_full_path(), and save the result in full_tempdir.  Otherwise,
     * leave full_tempdir set to NULL. */
    tmpdir_env = getenv("TMPDIR");
    if (tmpdir_env != NULL)
	full_tempdir = check_writable_directory(tmpdir_env);

    /* If $TMPDIR is unset, empty, or not a writable directory, and
     * full_tempdir is NULL, try P_tmpdir instead. */
    if (full_tempdir == NULL)
	full_tempdir = check_writable_directory(P_tmpdir);

    /* if P_tmpdir is NULL, use /tmp. */
    if (full_tempdir == NULL)
	full_tempdir = mallocstrcpy(NULL, "/tmp/");

    full_tempdir = charealloc(full_tempdir, strlen(full_tempdir) + 12);
    strcat(full_tempdir, "nano.XXXXXX");

    original_umask = umask(0);
    umask(S_IRWXG | S_IRWXO);

    fd = mkstemp(full_tempdir);

    if (fd != -1)
	*f = fdopen(fd, "r+b");
    else {
	free(full_tempdir);
	full_tempdir = NULL;
    }

    umask(original_umask);

    return full_tempdir;
}

#ifndef DISABLE_OPERATINGDIR
/* Initialize full_operating_dir based on operating_dir. */
void init_operating_dir(void)
{
    assert(full_operating_dir == NULL);

    if (operating_dir == NULL)
	return;

    full_operating_dir = get_full_path(operating_dir);

    /* If the operating directory is inaccessible, fail. */
    if (full_operating_dir == NULL || chdir(full_operating_dir) == -1)
	die(_("Invalid operating directory\n"));

    snuggly_fit(&full_operating_dir);
}

/* Check to see if we're inside the operating directory.  Return FALSE
 * if we are, or TRUE otherwise.  If allow_tabcomp is TRUE, allow
 * incomplete names that would be matches for the operating directory,
 * so that tab completion will work. */
bool check_operating_dir(const char *currpath, bool allow_tabcomp)
{
    /* full_operating_dir is global for memory cleanup.  It should have
     * already been initialized by init_operating_dir().  Also, a
     * relative operating directory path will only be handled properly
     * if this is done. */

    char *fullpath;
    bool retval = FALSE;
    const char *whereami1, *whereami2 = NULL;

    /* If no operating directory is set, don't bother doing anything. */
    if (operating_dir == NULL)
	return FALSE;

    assert(full_operating_dir != NULL);

    fullpath = get_full_path(currpath);

    /* If fullpath is NULL, it means some directory in the path doesn't
     * exist or is unreadable.  If allow_tabcomp is FALSE, then currpath
     * is what the user typed somewhere.  We don't want to report a
     * non-existent directory as being outside the operating directory,
     * so we return FALSE.  If allow_tabcomp is TRUE, then currpath
     * exists, but is not executable.  So we say it isn't in the
     * operating directory. */
    if (fullpath == NULL)
	return allow_tabcomp;

    whereami1 = strstr(fullpath, full_operating_dir);
    if (allow_tabcomp)
	whereami2 = strstr(full_operating_dir, fullpath);

    /* If both searches failed, we're outside the operating directory.
     * Otherwise, check the search results.  If the full operating
     * directory path is not at the beginning of the full current path
     * (for normal usage) and vice versa (for tab completion, if we're
     * allowing it), we're outside the operating directory. */
    if (whereami1 != fullpath && whereami2 != full_operating_dir)
	retval = TRUE;
    free(fullpath);

    /* Otherwise, we're still inside it. */
    return retval;
}
#endif

#ifndef NANO_TINY
/* Although this sucks, it sucks less than having a single 'my system is
 * messed up and I'm blanket allowing insecure file writing operations'. */
int prompt_failed_backupwrite(const char *filename)
{
    static int response;
    static char *prevfile = NULL; /* What was the last file we were
				   * passed so we don't keep asking
				   * this?  Though maybe we should... */
    if (prevfile == NULL || strcmp(filename, prevfile)) {
	response = do_yesno_prompt(FALSE, _("Failed to write backup file; "
			"continue saving? (Say N if unsure.) "));
	prevfile = mallocstrcpy(prevfile, filename);
    }

    return response;
}

void init_backup_dir(void)
{
    char *full_backup_dir;

    if (backup_dir == NULL)
	return;

    full_backup_dir = get_full_path(backup_dir);

    /* If get_full_path() failed or the backup directory is
     * inaccessible, unset backup_dir. */
    if (full_backup_dir == NULL ||
		full_backup_dir[strlen(full_backup_dir) - 1] != '/') {
	free(full_backup_dir);
	free(backup_dir);
	backup_dir = NULL;
    } else {
	free(backup_dir);
	backup_dir = full_backup_dir;
	snuggly_fit(&backup_dir);
    }
}
#endif /* !NANO_TINY */

/* Read from inn, write to out.  We assume inn is opened for reading,
 * and out for writing.  We return 0 on success, -1 on read error, or -2
 * on write error.  inn is always closed by this function, out is closed
 * only if close_out is true. */
int copy_file(FILE *inn, FILE *out, bool close_out)
{
    int retval = 0;
    char buf[BUFSIZ];
    size_t charsread;
    int (*flush_out_fnc)(FILE *) = (close_out) ? fclose : fflush;

    assert(inn != NULL && out != NULL && inn != out);

    do {
	charsread = fread(buf, sizeof(char), BUFSIZ, inn);
	if (charsread == 0 && ferror(inn)) {
	    retval = -1;
	    break;
	}
	if (fwrite(buf, sizeof(char), charsread, out) < charsread) {
	    retval = -2;
	    break;
	}
    } while (charsread > 0);

    if (fclose(inn) == EOF)
	retval = -1;
    if (flush_out_fnc(out) == EOF)
	retval = -2;

    return retval;
}

/* Write a file out to disk.  If f_open isn't NULL, we assume that it is
 * a stream associated with the file, and we don't try to open it
 * ourselves.  If tmp is TRUE, we set the umask to disallow anyone else
 * from accessing the file, we don't set the filename to its name, and
 * we don't print out how many lines we wrote on the statusbar.
 *
 * tmp means we are writing a temporary file in a secure fashion.  We
 * use it when spell checking or dumping the file on an error.  If
 * method is APPEND, it means we are appending instead of overwriting.
 * If method is PREPEND, it means we are prepending instead of
 * overwriting.  If nonamechange is TRUE, we don't change the current
 * filename.  nonamechange is irrelevant when appending or prepending,
 * or when writing a temporary file.
 *
 * Return TRUE on success or FALSE on error. */
bool write_file(const char *name, FILE *f_open, bool tmp,
	kind_of_writing_type method, bool nonamechange)
{
    bool retval = FALSE;
	/* Instead of returning in this function, you should always
	 * set retval and then goto cleanup_and_exit. */
    size_t lineswritten = 0;
    const filestruct *fileptr = openfile->fileage;
    int fd;
	/* The file descriptor we use. */
    mode_t original_umask = 0;
	/* Our umask, from when nano started. */
#ifndef NANO_TINY
    bool realexists;
	/* The result of stat().  TRUE if the file exists, FALSE
	 * otherwise.  If name is a link that points nowhere, realexists
	 * is FALSE. */
#endif
    struct stat st;
	/* The status fields filled in by stat(). */
    char *realname;
	/* The filename after tilde expansion. */
    FILE *f = f_open;
	/* The actual file, realname, we are writing to. */
    char *tempname = NULL;
	/* The name of the temporary file we write to on prepend. */

    if (*name == '\0')
	return -1;

    if (!tmp)
	titlebar(NULL);

    realname = real_dir_from_tilde(name);

#ifndef DISABLE_OPERATINGDIR
    /* If we're writing a temporary file, we're probably going outside
     * the operating directory, so skip the operating directory test. */
    if (!tmp && check_operating_dir(realname, FALSE)) {
	statusline(ALERT, _("Can't write outside of %s"), full_operating_dir);
	goto cleanup_and_exit;
    }
#endif

    /* If the temp file exists and isn't already open, give up. */
    if (tmp && (lstat(realname, &st) != -1) && f_open == NULL)
	goto cleanup_and_exit;

#ifndef NANO_TINY
    /* Check whether the file (at the end of the symlink) exists. */
    realexists = (stat(realname, &st) != -1);

    /* If we haven't stat()d this file before (say, the user just
     * specified it interactively), stat and save the value now,
     * or else we will chase null pointers when we do modtime checks,
     * preserve file times, and so on, during backup. */
    if (openfile->current_stat == NULL && !tmp && realexists)
	stat_with_alloc(realname, &openfile->current_stat);

    /* We backup only if the backup toggle is set, the file isn't
     * temporary, and the file already exists.  Furthermore, if we
     * aren't appending, prepending, or writing a selection, we backup
     * only if the file has not been modified by someone else since nano
     * opened it. */
    if (ISSET(BACKUP_FILE) && !tmp && realexists && openfile->current_stat &&
		(method != OVERWRITE || openfile->mark_set ||
		openfile->current_stat->st_mtime == st.st_mtime)) {
	int backup_fd;
	FILE *backup_file;
	char *backupname;
	static struct timespec filetime[2];
	int backup_cflags;

	/* Save the original file's access and modification times. */
	filetime[0].tv_sec = openfile->current_stat->st_atime;
	filetime[1].tv_sec = openfile->current_stat->st_mtime;

	if (f_open == NULL) {
	    /* Open the original file to copy to the backup. */
	    f = fopen(realname, "rb");

	    if (f == NULL) {
		statusline(ALERT, _("Error reading %s: %s"), realname,
			strerror(errno));
		/* If we can't read from the original file, go on, since
		 * only saving the original file is better than saving
		 * nothing. */
		goto skip_backup;
	    }
	}

	/* If backup_dir is set, we set backupname to
	 * backup_dir/backupname~[.number], where backupname is the
	 * canonicalized absolute pathname of realname with every '/'
	 * replaced with a '!'.  This means that /home/foo/file is
	 * backed up in backup_dir/!home!foo!file~[.number]. */
	if (backup_dir != NULL) {
	    char *backuptemp = get_full_path(realname);

	    if (backuptemp == NULL)
		/* If get_full_path() failed, we don't have a
		 * canonicalized absolute pathname, so just use the
		 * filename portion of the pathname.  We use tail() so
		 * that e.g. ../backupname will be backed up in
		 * backupdir/backupname~ instead of
		 * backupdir/../backupname~. */
		backuptemp = mallocstrcpy(NULL, tail(realname));
	    else {
		size_t i = 0;

		for (; backuptemp[i] != '\0'; i++) {
		    if (backuptemp[i] == '/')
			backuptemp[i] = '!';
		}
	    }

	    backupname = charalloc(strlen(backup_dir) + strlen(backuptemp) + 1);
	    sprintf(backupname, "%s%s", backup_dir, backuptemp);
	    free(backuptemp);
	    backuptemp = get_next_filename(backupname, "~");
	    if (*backuptemp == '\0') {
		statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, _("Too many backup files?"));
		free(backuptemp);
		free(backupname);
		/* If we can't write to the backup, DON'T go on, since
		 * whatever caused the backup file to fail (e.g. disk
		 * full may well cause the real file write to fail,
		 * which means we could lose both the backup and the
		 * original! */
		goto cleanup_and_exit;
	    } else {
		free(backupname);
		backupname = backuptemp;
	    }
	} else {
	    backupname = charalloc(strlen(realname) + 2);
	    sprintf(backupname, "%s~", realname);
	}

	/* First, unlink any existing backups.  Next, open the backup
	 * file with O_CREAT and O_EXCL.  If it succeeds, we have a file
	 * descriptor to a new backup file. */
	if (unlink(backupname) < 0 && errno != ENOENT && !ISSET(INSECURE_BACKUP)) {
	    if (prompt_failed_backupwrite(backupname))
		goto skip_backup;
	    statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, strerror(errno));
	    free(backupname);
	    goto cleanup_and_exit;
	}

	if (ISSET(INSECURE_BACKUP))
	    backup_cflags = O_WRONLY | O_CREAT | O_APPEND;
	else
	    backup_cflags = O_WRONLY | O_CREAT | O_EXCL | O_APPEND;

	backup_fd = open(backupname, backup_cflags,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	/* Now we've got a safe file stream.  If the previous open()
	 * call failed, this will return NULL. */
	backup_file = fdopen(backup_fd, "wb");

	if (backup_fd < 0 || backup_file == NULL) {
	    statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, strerror(errno));
	    free(backupname);
	    /* If we can't make a backup, DON'T go on, since whatever caused
	     * the backup to fail (e.g. disk full) may well cause the real
	     * file write to fail, in which case we could lose both the
	     * backup and the original! */
	    goto cleanup_and_exit;
	}

	/* Only try chowning the backup when we're root. */
	if (geteuid() == NANO_ROOT_UID &&
			fchown(backup_fd, openfile->current_stat->st_uid,
			openfile->current_stat->st_gid) == -1 &&
			!ISSET(INSECURE_BACKUP)) {
	    fclose(backup_file);
	    if (prompt_failed_backupwrite(backupname))
		goto skip_backup;
	    statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, strerror(errno));
	    free(backupname);
	    goto cleanup_and_exit;
	}

	/* Set the backup's mode bits. */
	if (fchmod(backup_fd, openfile->current_stat->st_mode) == -1 &&
			!ISSET(INSECURE_BACKUP)) {
	    fclose(backup_file);
	    if (prompt_failed_backupwrite(backupname))
		goto skip_backup;
	    statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, strerror(errno));
	    free(backupname);
	    goto cleanup_and_exit;
	}

#ifdef DEBUG
	fprintf(stderr, "Backing up %s to %s\n", realname, backupname);
#endif

	/* Copy the file. */
	if (copy_file(f, backup_file, FALSE) != 0) {
	    fclose(backup_file);
	    statusline(ALERT, _("Error reading %s: %s"), realname,
			strerror(errno));
	    goto cleanup_and_exit;
	}

	/* And set the backup's timestamps. */
	if (futimens(backup_fd, filetime) == -1 && !ISSET(INSECURE_BACKUP)) {
	    fclose(backup_file);
	    if (prompt_failed_backupwrite(backupname))
		goto skip_backup;
	    statusline(HUSH, _("Error writing backup file %s: %s"),
			backupname, strerror(errno));
	    goto cleanup_and_exit;
	}

	fclose(backup_file);
	free(backupname);
    }

    skip_backup:
#endif /* !NANO_TINY */

    if (f_open == NULL) {
	original_umask = umask(0);

	/* If we create a temp file, we don't let anyone else access it.
	 * We create a temp file if tmp is TRUE. */
	if (tmp)
	    umask(S_IRWXG | S_IRWXO);
	else
	    umask(original_umask);
    }

    /* If we're prepending, copy the file to a temp file. */
    if (method == PREPEND) {
	int fd_source;
	FILE *f_source = NULL;

	if (f == NULL) {
	    f = fopen(realname, "rb");

	    if (f == NULL) {
		statusline(ALERT, _("Error reading %s: %s"), realname,
			strerror(errno));
		goto cleanup_and_exit;
	    }
	}

	tempname = safe_tempfile(&f);

	if (tempname == NULL) {
	    statusline(ALERT, _("Error writing temp file: %s"),
			strerror(errno));
	    goto cleanup_and_exit;
	}

	if (f_open == NULL) {
	    fd_source = open(realname, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);

	    if (fd_source != -1) {
		f_source = fdopen(fd_source, "rb");
		if (f_source == NULL) {
		    statusline(ALERT, _("Error reading %s: %s"), realname,
				strerror(errno));
		    close(fd_source);
		    fclose(f);
		    unlink(tempname);
		    goto cleanup_and_exit;
		}
	    }
	}

	if (f_source == NULL || copy_file(f_source, f, TRUE) != 0) {
	    statusline(ALERT, _("Error writing temp file: %s"),
			strerror(errno));
	    unlink(tempname);
	    goto cleanup_and_exit;
	}
    }

    if (f_open == NULL) {
	/* Now open the file in place.  Use O_EXCL if tmp is TRUE.  This
	 * is copied from joe, because wiggy says so *shrug*. */
	fd = open(realname, O_WRONLY | O_CREAT | ((method == APPEND) ?
		O_APPEND : (tmp ? O_EXCL : O_TRUNC)), S_IRUSR |
		S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	/* Set the umask back to the user's original value. */
	umask(original_umask);

	/* If we couldn't open the file, give up. */
	if (fd == -1) {
	    statusline(ALERT, _("Error writing %s: %s"), realname,
			strerror(errno));
	    if (tempname != NULL)
		unlink(tempname);
	    goto cleanup_and_exit;
	}

	f = fdopen(fd, (method == APPEND) ? "ab" : "wb");

	if (f == NULL) {
	    statusline(ALERT, _("Error writing %s: %s"), realname,
			strerror(errno));
	    close(fd);
	    goto cleanup_and_exit;
	}
    }

    while (fileptr != NULL) {
	size_t data_len = strlen(fileptr->data), size;

	/* Convert newlines to nulls, just before we write to disk. */
	sunder(fileptr->data);

	size = fwrite(fileptr->data, sizeof(char), data_len, f);

	/* Convert nulls to newlines.  data_len is the string's real
	 * length. */
	unsunder(fileptr->data, data_len);

	if (size < data_len) {
	    statusline(ALERT, _("Error writing %s: %s"), realname,
			strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}

	/* If we're on the last line of the file, don't write a newline
	 * character after it.  If the last line of the file is blank,
	 * this means that zero bytes are written, in which case we
	 * don't count the last line in the total lines written. */
	if (fileptr == openfile->filebot) {
	    if (fileptr->data[0] == '\0')
		lineswritten--;
	} else {
#ifndef NANO_TINY
	    if (openfile->fmt == DOS_FILE || openfile->fmt == MAC_FILE) {
		if (putc('\r', f) == EOF) {
		    statusline(ALERT, _("Error writing %s: %s"), realname,
				strerror(errno));
		    fclose(f);
		    goto cleanup_and_exit;
		}
	    }

	    if (openfile->fmt != MAC_FILE)
#endif
		if (putc('\n', f) == EOF) {
		    statusline(ALERT, _("Error writing %s: %s"), realname,
				strerror(errno));
		    fclose(f);
		    goto cleanup_and_exit;
		}
	}

	fileptr = fileptr->next;
	lineswritten++;
    }

    /* If we're prepending, open the temp file, and append it to f. */
    if (method == PREPEND) {
	int fd_source;
	FILE *f_source = NULL;

	fd_source = open(tempname, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);

	if (fd_source != -1) {
	    f_source = fdopen(fd_source, "rb");
	    if (f_source == NULL)
		close(fd_source);
	}

	if (f_source == NULL) {
	    statusline(ALERT, _("Error reading %s: %s"), tempname,
			strerror(errno));
	    fclose(f);
	    goto cleanup_and_exit;
	}

	if (copy_file(f_source, f, TRUE) != 0) {
	    statusline(ALERT, _("Error writing %s: %s"), realname,
			strerror(errno));
	    goto cleanup_and_exit;
	}

	unlink(tempname);
    } else if (fclose(f) != 0) {
	statusline(ALERT, _("Error writing %s: %s"), realname,
			strerror(errno));
	goto cleanup_and_exit;
    }

    if (method == OVERWRITE && !tmp) {
	/* If we must set the filename, and it changed, adjust things. */
	if (!nonamechange && strcmp(openfile->filename, realname) != 0) {
#ifndef DISABLE_COLOR
	    char *oldname = openfile->syntax ? openfile->syntax->name : "";
	    filestruct *line = openfile->fileage;
#endif
	    openfile->filename = mallocstrcpy(openfile->filename, realname);

#ifndef DISABLE_COLOR
	    /* See if the applicable syntax has changed. */
	    color_update();
	    color_init();

	    char *newname = openfile->syntax ? openfile->syntax->name : "";

	    /* If the syntax changed, discard and recompute the multidata. */
	    if (strcmp(oldname, newname) != 0) {
		for (; line != NULL; line = line->next) {
		    free(line->multidata);
		    line->multidata = NULL;
		}
		precalc_multicolorinfo();
		refresh_needed = TRUE;
	    }
#endif
	}

#ifndef NANO_TINY
	if (!openfile->mark_set)
	    /* Get or update the stat info to reflect the current state. */
	    stat_with_alloc(realname, &openfile->current_stat);
#endif

	statusline(HUSH, P_("Wrote %lu line", "Wrote %lu lines",
		(unsigned long)lineswritten), (unsigned long)lineswritten);
	openfile->modified = FALSE;
	titlebar(NULL);
    }

    retval = TRUE;

  cleanup_and_exit:
    free(realname);
    free(tempname);

    return retval;
}

#ifndef NANO_TINY
/* Write a marked selection from a file out to disk.  Return TRUE on
 * success or FALSE on error. */
bool write_marked_file(const char *name, FILE *f_open, bool tmp,
	kind_of_writing_type method)
{
    bool retval;
    bool old_modified = openfile->modified;
	/* Save the status, because write_file() unsets the modified flag. */
    bool added_magicline = FALSE;
	/* Whether we added a magicline after filebot. */
    filestruct *top, *bot;
    size_t top_x, bot_x;

    /* Partition the filestruct so that it contains only the marked text. */
    mark_order((const filestruct **)&top, &top_x,
		(const filestruct **)&bot, &bot_x, NULL);
    filepart = partition_filestruct(top, top_x, bot, bot_x);

    /* If we are doing magicline, and the last line of the partition
     * isn't blank, then add a newline at the end of the buffer. */
    if (!ISSET(NO_NEWLINES) && openfile->filebot->data[0] != '\0') {
	new_magicline();
	added_magicline = TRUE;
    }

    retval = write_file(name, f_open, tmp, method, TRUE);

    /* If we added a magicline, remove it now. */
    if (added_magicline)
	remove_magicline();

    /* Unpartition the filestruct so that it contains all the text again. */
    unpartition_filestruct(&filepart);

    if (old_modified)
	set_modified();

    return retval;
}

#endif /* !NANO_TINY */

/* Write the current file to disk.  If the mark is on, write the current
 * marked selection to disk.  If exiting is TRUE, write the entire file
 * to disk regardless of whether the mark is on, and without prompting if
 * the TEMP_FILE flag is set and the current file has a name.  Return 0
 * on error, 1 on success, and 2 when the buffer is to be discarded. */
int do_writeout(bool exiting)
{
    int i;
    bool result = FALSE;
    kind_of_writing_type method = OVERWRITE;
    char *given;
	/* The filename we offer, or what the user typed so far. */
    bool maychange = (openfile->filename[0] == '\0');
	/* Whether it's okay to save the file under a different name. */
#ifndef DISABLE_EXTRA
    static bool did_credits = FALSE;
#endif

    /* Display newlines in filenames as ^J. */
    as_an_at = FALSE;

    if (exiting && ISSET(TEMP_FILE) && openfile->filename[0] != '\0') {
	if (write_file(openfile->filename, NULL, FALSE, OVERWRITE, FALSE))
	    return 1;
	/* If writing the file failed, go on to prompt for a new name. */
    }

    given = mallocstrcpy(NULL,
#ifndef NANO_TINY
	(openfile->mark_set && !exiting) ? "" :
#endif
	openfile->filename);

    while (TRUE) {
	const char *msg;
#ifndef NANO_TINY
	const char *formatstr, *backupstr;

	formatstr = (openfile->fmt == DOS_FILE) ? _(" [DOS Format]") :
			(openfile->fmt == MAC_FILE) ? _(" [Mac Format]") : "";

	backupstr = ISSET(BACKUP_FILE) ? _(" [Backup]") : "";

	/* If we're using restricted mode, don't display the "Write
	 * Selection to File" prompt.  This function is disabled, since
	 * it allows reading from or writing to files not specified on
	 * the command line. */
	if (openfile->mark_set && !exiting && !ISSET(RESTRICTED))
	    /* TRANSLATORS: The next six strings are prompts. */
	    msg = (method == PREPEND) ? _("Prepend Selection to File") :
			(method == APPEND) ? _("Append Selection to File") :
			_("Write Selection to File");
	else
#endif /* !NANO_TINY */
	    msg = (method == PREPEND) ? _("File Name to Prepend to") :
			(method == APPEND) ? _("File Name to Append to") :
			_("File Name to Write");

	present_path = mallocstrcpy(present_path, "./");

	/* If we're using restricted mode, and the filename isn't blank,
	 * disable tab completion. */
	i = do_prompt(!ISSET(RESTRICTED) || openfile->filename[0] == '\0',
		TRUE, MWRITEFILE, given,
#ifndef DISABLE_HISTORIES
		NULL,
#endif
		edit_refresh, "%s%s%s", msg,
#ifndef NANO_TINY
		formatstr, backupstr
#else
		"", ""
#endif
		);

	if (i < 0) {
	    statusbar(_("Cancelled"));
	    break;
	} else {
	    functionptrtype func = func_from_key(&i);

	    /* Upon request, abandon the buffer, if user is sure. */
	    if (func == discard_buffer) {
		if (openfile->modified)
		    i = do_yesno_prompt(FALSE,
				_("Save modified buffer anyway? "));
		else
		    i = 0;

		if (i == 0) {
		    free(given);
		    return 2;	/* Yes, discard the buffer. */
		} else
		    continue;	/* Go back to the filename prompt. */
	    }

	    given = mallocstrcpy(given, answer);

#ifndef DISABLE_BROWSER
	    if (func == to_files_void) {
		char *chosen = do_browse_from(answer);

		if (chosen == NULL)
		    continue;

		/* We have a file now.  Indicate this. */
		free(answer);
		answer = chosen;
	    } else
#endif /* !DISABLE_BROWSER */
#ifndef NANO_TINY
	    if (func == dos_format_void) {
		openfile->fmt = (openfile->fmt == DOS_FILE) ? NIX_FILE :
			DOS_FILE;
		continue;
	    } else if (func == mac_format_void) {
		openfile->fmt = (openfile->fmt == MAC_FILE) ? NIX_FILE :
			MAC_FILE;
		continue;
	    } else if (func == backup_file_void) {
		TOGGLE(BACKUP_FILE);
		continue;
	    } else
#endif /* !NANO_TINY */
	    if (func == prepend_void) {
		method = (method == PREPEND) ? OVERWRITE : PREPEND;
		continue;
	    } else if (func == append_void) {
		method = (method == APPEND) ? OVERWRITE : APPEND;
		continue;
	    } else if (func == do_help_void) {
		continue;
	    }

#ifdef DEBUG
	    fprintf(stderr, "filename is %s\n", answer);
#endif

#ifndef DISABLE_EXTRA
	    /* If the current file has been modified, we've pressed
	     * Ctrl-X at the edit window to exit, we've pressed "y" at
	     * the "Save modified buffer" prompt to save, we've entered
	     * "zzy" as the filename to save under (hence "xyzzy"), and
	     * this is the first time we've done this, show an Easter
	     * egg.  Display the credits. */
	    if (!did_credits && exiting && !ISSET(TEMP_FILE) &&
			strcasecmp(answer, "zzy") == 0) {
		do_credits();
		did_credits = TRUE;
		break;
	    }
#endif

	    if (method == OVERWRITE) {
		bool name_exists, do_warning;
		char *full_answer, *full_filename;
		struct stat st;

		full_answer = get_full_path(answer);
		full_filename = get_full_path(openfile->filename);
		name_exists = (stat((full_answer == NULL) ?
				answer : full_answer, &st) != -1);
		if (openfile->filename[0] == '\0')
		    do_warning = name_exists;
		else
		    do_warning = (strcmp((full_answer == NULL) ?
				answer : full_answer, (full_filename == NULL) ?
				openfile->filename : full_filename) != 0);

		free(full_filename);
		free(full_answer);

		if (do_warning) {
		    /* When in restricted mode, we aren't allowed to overwrite
		     * an existing file with the current buffer, nor to change
		     * the name of the current file if it already has one. */
		    if (ISSET(RESTRICTED)) {
			/* TRANSLATORS: Restricted mode forbids overwriting. */
			warn_and_shortly_pause(_("File exists -- "
					"cannot overwrite"));
			continue;
		    }

		    if (!maychange) {
#ifndef NANO_TINY
			if (exiting || !openfile->mark_set)
#endif
			{
			    if (do_yesno_prompt(FALSE, _("Save file under "
					"DIFFERENT NAME? ")) < 1)
				continue;
			    maychange = TRUE;
			}
		    }

		    if (name_exists) {
			char *question = _("File \"%s\" exists; OVERWRITE? ");
			char *message = charalloc(strlen(question) +
						strlen(answer) + 1);
			sprintf(message, question, answer);

			i = do_yesno_prompt(FALSE, message);
			free(message);

			if (i < 1)
			    continue;
		    }
		}
#ifndef NANO_TINY
		/* Complain if the file exists, the name hasn't changed,
		 * and the stat information we had before does not match
		 * what we have now. */
		else if (name_exists && openfile->current_stat &&
			(openfile->current_stat->st_mtime < st.st_mtime ||
			openfile->current_stat->st_dev != st.st_dev ||
			openfile->current_stat->st_ino != st.st_ino)) {

		    if (do_yesno_prompt(FALSE, _("File was modified since "
				"you opened it; continue saving? ")) < 1)
			continue;
		}
#endif
	    }

	    /* Here's where we allow the selected text to be written to
	     * a separate file.  If we're using restricted mode, this
	     * function is disabled, since it allows reading from or
	     * writing to files not specified on the command line. */
#ifndef NANO_TINY
	    if (openfile->mark_set && !exiting && !ISSET(RESTRICTED))
		result = write_marked_file(answer, NULL, FALSE, method);
	    else
#endif
		result = write_file(answer, NULL, FALSE, method, FALSE);

	    break;
	}
    }

    free(given);

    return result ? 1 : 0;
}

/* Write the current buffer to disk, or discard it. */
void do_writeout_void(void)
{
    /* If the user chose to discard the buffer, close it. */
    if (do_writeout(FALSE) == 2)
	close_and_go();
}

#ifndef NANO_TINY
/* If it has a name, write the current file to disk without prompting. */
void do_savefile(void)
{
    if (openfile->filename[0] != '\0')
	write_file(openfile->filename, NULL, FALSE, OVERWRITE, FALSE);
    else
	do_writeout_void();
}
#endif

/* Return a malloc()ed string containing the actual directory, used to
 * convert ~user/ and ~/ notation. */
char *real_dir_from_tilde(const char *buf)
{
    char *retval;

    if (*buf == '~') {
	size_t i = 1;
	char *tilde_dir;

	/* Figure out how much of the string we need to compare. */
	for (; buf[i] != '/' && buf[i] != '\0'; i++)
	    ;

	/* Get the home directory. */
	if (i == 1) {
	    get_homedir();
	    tilde_dir = mallocstrcpy(NULL, homedir);
	} else {
#ifdef HAVE_PWD_H
	    const struct passwd *userdata;

	    tilde_dir = mallocstrncpy(NULL, buf, i + 1);
	    tilde_dir[i] = '\0';

	    do {
		userdata = getpwent();
	    } while (userdata != NULL &&
			strcmp(userdata->pw_name, tilde_dir + 1) != 0);
	    endpwent();
	    if (userdata != NULL)
		tilde_dir = mallocstrcpy(tilde_dir, userdata->pw_dir);
#else
	    tilde_dir = strdup("");
#endif
	}

	retval = charalloc(strlen(tilde_dir) + strlen(buf + i) + 1);
	sprintf(retval, "%s%s", tilde_dir, buf + i);

	free(tilde_dir);
    } else
	retval = mallocstrcpy(NULL, buf);

    return retval;
}

#if !defined(DISABLE_TABCOMP) || !defined(DISABLE_BROWSER)
/* Our sort routine for file listings.  Sort alphabetically and
 * case-insensitively, and sort directories before filenames. */
int diralphasort(const void *va, const void *vb)
{
    struct stat fileinfo;
    const char *a = *(const char *const *)va;
    const char *b = *(const char *const *)vb;
    bool aisdir = stat(a, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);
    bool bisdir = stat(b, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode);

    if (aisdir && !bisdir)
	return -1;
    if (!aisdir && bisdir)
	return 1;

    /* Standard function brain damage: We should be sorting
     * alphabetically and case-insensitively according to the current
     * locale, but there's no standard strcasecoll() function, so we
     * have to use multibyte strcasecmp() instead. */
    return mbstrcasecmp(a, b);
}

/* Free the memory allocated for array, which should contain len
 * elements. */
void free_chararray(char **array, size_t len)
{
    if (array == NULL)
	return;

    for (; len > 0; len--)
	free(array[len - 1]);

    free(array);
}
#endif

#ifndef DISABLE_TABCOMP
/* Is the given path a directory? */
bool is_dir(const char *buf)
{
    char *dirptr;
    struct stat fileinfo;
    bool retval;

    dirptr = real_dir_from_tilde(buf);

    retval = (stat(dirptr, &fileinfo) != -1 && S_ISDIR(fileinfo.st_mode));

    free(dirptr);

    return retval;
}

/* These functions, username_tab_completion(), cwd_tab_completion()
 * (originally exe_n_cwd_tab_completion()), and input_tab(), were
 * adapted from busybox 0.46 (cmdedit.c).  Here is the notice from that
 * file, with the copyright years updated:
 *
 * Termios command line History and Editting, originally
 * intended for NetBSD sh (ash)
 * Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu>
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *  Majorly adjusted/re-written for busybox:
 *                            Erik Andersen <andersee@debian.org>
 *
 * You may use this code as you wish, so long as the original author(s)
 * are attributed in any redistributions of the source code.
 * This code is 'as is' with no warranty.
 * This code may safely be consumed by a BSD or GPL license. */

/* We consider the first buf_len characters of buf for ~username tab
 * completion. */
char **username_tab_completion(const char *buf, size_t *num_matches,
	size_t buf_len)
{
    char **matches = NULL;

    assert(buf != NULL && num_matches != NULL && buf_len > 0);

    *num_matches = 0;

#ifdef HAVE_PWD_H
    const struct passwd *userdata;

    while ((userdata = getpwent()) != NULL) {
	if (strncmp(userdata->pw_name, buf + 1, buf_len - 1) == 0) {
	    /* Cool, found a match.  Add it to the list.  This makes a
	     * lot more sense to me (Chris) this way... */

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
	     * directory, in which case just go to the next match. */
	    if (check_operating_dir(userdata->pw_dir, TRUE))
		continue;
#endif

	    matches = (char **)nrealloc(matches, (*num_matches + 1) *
					sizeof(char *));
	    matches[*num_matches] = charalloc(strlen(userdata->pw_name) + 2);
	    sprintf(matches[*num_matches], "~%s", userdata->pw_name);
	    ++(*num_matches);
	}
    }
    endpwent();
#endif

    return matches;
}

/* We consider the first buf_len characters of buf for filename tab
 * completion. */
char **cwd_tab_completion(const char *buf, bool allow_files, size_t
	*num_matches, size_t buf_len)
{
    char *dirname = mallocstrcpy(NULL, buf);
    char *slash, *filename;
    size_t filenamelen;
    char **matches = NULL;
    DIR *dir;
    const struct dirent *nextdir;

    *num_matches = 0;
    dirname[buf_len] = '\0';

    /* If there's a / in the name, split out filename and directory parts. */
    slash = strrchr(dirname, '/');
    if (slash != NULL) {
	char *wasdirname = dirname;

	filename = mallocstrcpy(NULL, ++slash);
	/* Cut off the filename part after the slash. */
	*slash = '\0';
	dirname = real_dir_from_tilde(dirname);
	/* A non-absolute path is relative to the current browser directory. */
	if (dirname[0] != '/') {
	    dirname = charealloc(dirname, strlen(present_path) +
						strlen(wasdirname) + 1);
	    sprintf(dirname, "%s%s", present_path, wasdirname);
	}
	free(wasdirname);
    } else {
	filename = dirname;
	dirname = mallocstrcpy(NULL, present_path);
    }

    assert(dirname[strlen(dirname) - 1] == '/');

    dir = opendir(dirname);

    if (dir == NULL) {
	/* Don't print an error, just shut up and return. */
	beep();
	free(filename);
	free(dirname);
	return NULL;
    }

    filenamelen = strlen(filename);

    while ((nextdir = readdir(dir)) != NULL) {
	bool skip_match = FALSE;

#ifdef DEBUG
	fprintf(stderr, "Comparing \'%s\'\n", nextdir->d_name);
#endif
	/* See if this matches. */
	if (strncmp(nextdir->d_name, filename, filenamelen) == 0 &&
		(*filename == '.' || (strcmp(nextdir->d_name, ".") != 0 &&
		strcmp(nextdir->d_name, "..") != 0))) {
	    /* Cool, found a match.  Add it to the list.  This makes a
	     * lot more sense to me (Chris) this way... */

	    char *tmp = charalloc(strlen(dirname) + strlen(nextdir->d_name) + 1);
	    sprintf(tmp, "%s%s", dirname, nextdir->d_name);

#ifndef DISABLE_OPERATINGDIR
	    /* ...unless the match exists outside the operating
	     * directory, in which case just go to the next match. */
	    if (check_operating_dir(tmp, TRUE))
		skip_match = TRUE;
#endif

	    /* ...or unless the match isn't a directory and allow_files
	     * isn't set, in which case just go to the next match. */
	    if (!allow_files && !is_dir(tmp))
		skip_match = TRUE;

	    free(tmp);

	    if (skip_match)
		continue;

	    matches = (char **)nrealloc(matches, (*num_matches + 1) *
					sizeof(char *));
	    matches[*num_matches] = mallocstrcpy(NULL, nextdir->d_name);
	    ++(*num_matches);
	}
    }

    closedir(dir);
    free(dirname);
    free(filename);

    return matches;
}

/* Do tab completion.  place refers to how much the statusbar cursor
 * position should be advanced.  refresh_func is the function we will
 * call to refresh the edit window. */
char *input_tab(char *buf, bool allow_files, size_t *place,
	bool *lastwastab, void (*refresh_func)(void), bool *listed)
{
    size_t num_matches = 0, buf_len;
    char **matches = NULL;

    assert(buf != NULL && place != NULL && *place <= strlen(buf) &&
		lastwastab != NULL && refresh_func != NULL && listed != NULL);

    *listed = FALSE;

    /* If the word starts with `~' and there is no slash in the word,
     * then try completing this word as a username. */
    if (*place > 0 && *buf == '~') {
	const char *slash = strchr(buf, '/');

	if (slash == NULL || slash >= buf + *place)
	    matches = username_tab_completion(buf, &num_matches, *place);
    }

    /* Match against files relative to the current working directory. */
    if (matches == NULL)
	matches = cwd_tab_completion(buf, allow_files, &num_matches, *place);

    buf_len = strlen(buf);

    if (num_matches == 0 || *place != buf_len)
	beep();
    else {
	size_t match, common_len = 0;
	char *mzero, *glued;
	const char *lastslash = revstrstr(buf, "/", buf + *place);
	size_t lastslash_len = (lastslash == NULL) ? 0 : lastslash - buf + 1;
	char char1[MAXCHARLEN], char2[MAXCHARLEN];
	int len1, len2;

	/* Get the number of characters that all matches have in common. */
	while (TRUE) {
	    len1 = parse_mbchar(matches[0] + common_len, char1, NULL);

	    for (match = 1; match < num_matches; match++) {
		len2 = parse_mbchar(matches[match] + common_len, char2, NULL);

		if (len1 != len2 || strncmp(char1, char2, len2) != 0)
		    break;
	    }

	    if (match < num_matches || matches[0][common_len] == '\0')
		break;

	    common_len += len1;
	}

	mzero = charalloc(lastslash_len + common_len + 1);

	strncpy(mzero, buf, lastslash_len);
	strncpy(mzero + lastslash_len, matches[0], common_len);

	common_len += lastslash_len;
	mzero[common_len] = '\0';

	/* Cover also the case of the user specifying a relative path. */
	glued = charalloc(strlen(present_path) + strlen(mzero) + 1);
	sprintf(glued, "%s%s", present_path, mzero);

	assert(common_len >= *place);

	if (num_matches == 1 && (is_dir(mzero) || is_dir(glued))) {
	    mzero[common_len++] = '/';

	    assert(common_len > *place);
	}

	if (num_matches > 1 && (common_len != *place || !*lastwastab))
	    beep();

	/* If the matches have something in common, show that part. */
	if (common_len != *place) {
	    buf = charealloc(buf, common_len + buf_len - *place + 1);
	    charmove(buf + common_len, buf + *place, buf_len - *place + 1);
	    strncpy(buf, mzero, common_len);
	    *place = common_len;
	}

	if (!*lastwastab)
	    *lastwastab = TRUE;
	else if (num_matches > 1) {
	    int longest_name = 0, ncols, editline = 0;

	    /* Sort the list of available choices. */
	    qsort(matches, num_matches, sizeof(char *), diralphasort);

	    /* Find the length of the longest among the choices. */
	    for (match = 0; match < num_matches; match++) {
		size_t namelen = strlenpt(matches[match]);

		if (namelen > longest_name)
		    longest_name = namelen;
	    }

	    if (longest_name > COLS - 1)
		longest_name = COLS - 1;

	    /* Each column will be (longest_name + 2) columns wide, i.e.
	     * two spaces between columns, except that there will be
	     * only one space after the last column. */
	    ncols = (COLS + 1) / (longest_name + 2);

	    /* Blank the edit window and hide the cursor. */
	    blank_edit();
	    curs_set(0);
	    wmove(edit, 0, 0);

	    /* Now print the list of matches out there. */
	    for (match = 0; match < num_matches; match++) {
		char *disp;

		wmove(edit, editline, (longest_name + 2) * (match % ncols));

		if (match % ncols == 0 && editline == editwinrows - 1 &&
			num_matches - match > ncols) {
		    waddstr(edit, _("(more)"));
		    break;
		}

		disp = display_string(matches[match], 0, longest_name, FALSE);
		waddstr(edit, disp);
		free(disp);

		if ((match + 1) % ncols == 0)
		    editline++;
	    }

	    wnoutrefresh(edit);
	    *listed = TRUE;
	}

	free(glued);
	free(mzero);
    }

    free_chararray(matches, num_matches);

    /* When we didn't list any matches now, refresh the edit window, just
     * in case a previous tab showed a list, so we know where we are. */
    if (!*listed)
	refresh_func();

    return buf;
}
#endif /* !DISABLE_TABCOMP */

/* Return the filename part of the given path. */
const char *tail(const char *path)
{
    const char *slash = strrchr(path, '/');

    if (slash == NULL)
	return path;
    else
	return ++slash;
}

#ifndef DISABLE_HISTORIES
/* Return the constructed dirfile path, or NULL if we can't find the home
 * directory.  The string is dynamically allocated, and should be freed. */
char *construct_filename(const char *str)
{
    char *newstr = NULL;

    if (homedir != NULL) {
	size_t homelen = strlen(homedir);

	newstr = charalloc(homelen + strlen(str) + 1);
	strcpy(newstr, homedir);
	strcpy(newstr + homelen, str);
    }

    return newstr;
}

char *histfilename(void)
{
    return construct_filename("/.nano/search_history");
}

/* Construct the legacy history filename. */
/* (To be removed in 2018.) */
char *legacyhistfilename(void)
{
    return construct_filename("/.nano_history");
}

char *poshistfilename(void)
{
    return construct_filename("/.nano/filepos_history");
}

void history_error(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, _(msg), ap);
    va_end(ap);

    fprintf(stderr, _("\nPress Enter to continue\n"));
    while (getchar() != '\n')
	;
}

/* Now that we have more than one history file, let's just rely on a
 * .nano dir for this stuff.  Return 1 if the dir exists or was
 * successfully created, and return 0 otherwise. */
int check_dotnano(void)
{
    int ret = 1;
    struct stat dirstat;
    char *nanodir = construct_filename("/.nano");

    if (stat(nanodir, &dirstat) == -1) {
	if (mkdir(nanodir, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
	    history_error(N_("Unable to create directory %s: %s\n"
				"It is required for saving/loading "
				"search history or cursor positions.\n"),
				nanodir, strerror(errno));
	    ret = 0;
	}
    } else if (!S_ISDIR(dirstat.st_mode)) {
	history_error(N_("Path %s is not a directory and needs to be.\n"
				"Nano will be unable to load or save "
				"search history or cursor positions.\n"),
				nanodir);
	ret = 0;
    }

    free(nanodir);
    return ret;
}

/* Load the search and replace histories from ~/.nano/search_history. */
void load_history(void)
{
    char *nanohist = histfilename();
    char *legacyhist = legacyhistfilename();
    struct stat hstat;

    /* If there is an old history file, migrate it. */
    /* (To be removed in 2018.) */
    if (stat(legacyhist, &hstat) != -1 && stat(nanohist, &hstat) == -1) {
	if (rename(legacyhist, nanohist) == -1)
	    history_error(N_("Detected a legacy nano history file (%s) which I tried to move\n"
			     "to the preferred location (%s) but encountered an error: %s"),
				legacyhist, nanohist, strerror(errno));
	else
	    history_error(N_("Detected a legacy nano history file (%s) which I moved\n"
			     "to the preferred location (%s)\n(see the nano FAQ about this change)"),
				legacyhist, nanohist);
    }

    /* Assume do_rcfile() has reported a missing home directory. */
    if (nanohist != NULL) {
	FILE *hist = fopen(nanohist, "rb");

	if (hist == NULL) {
	    if (errno != ENOENT) {
		/* Don't save history when we quit. */
		UNSET(HISTORYLOG);
		history_error(N_("Error reading %s: %s"), nanohist,
			strerror(errno));
	    }
	} else {
	    /* Load a history list (first the search history, then the
	     * replace history) from the oldest entry to the newest.
	     * Assume the last history entry is a blank line. */
	    filestruct **history = &search_history;
	    char *line = NULL;
	    size_t buf_len = 0;
	    ssize_t read;

	    while ((read = getline(&line, &buf_len, hist)) > 0) {
		line[--read] = '\0';
		if (read > 0) {
		    /* Encode any embedded NUL as 0x0A. */
		    unsunder(line, read);
		    update_history(history, line);
		} else
		    history = &replace_history;
	    }

	    fclose(hist);
	    free(line);
	}
	free(nanohist);
	free(legacyhist);
    }
}

/* Write the lines of a history list, starting with the line at head, to
 * the open file at hist.  Return TRUE if the write succeeded, and FALSE
 * otherwise. */
bool writehist(FILE *hist, const filestruct *head)
{
    const filestruct *item;

    /* Write a history list, from the oldest item to the newest. */
    for (item = head; item != NULL; item = item->next) {
	size_t length = strlen(item->data);

	/* Decode 0x0A bytes as embedded NULs. */
	sunder(item->data);

	if (fwrite(item->data, sizeof(char), length, hist) < length)
	    return FALSE;
	if (putc('\n', hist) == EOF)
	    return FALSE;
    }

    return TRUE;
}

/* Save the search and replace histories to ~/.nano/search_history. */
void save_history(void)
{
    char *nanohist;

    /* Don't save unchanged or empty histories. */
    if (!history_has_changed() || (searchbot->lineno == 1 &&
		replacebot->lineno == 1))
	return;

    nanohist = histfilename();

    if (nanohist != NULL) {
	FILE *hist = fopen(nanohist, "wb");

	if (hist == NULL)
	    fprintf(stderr, _("Error writing %s: %s\n"), nanohist,
			strerror(errno));
	else {
	    /* Make sure no one else can read from or write to the
	     * history file. */
	    chmod(nanohist, S_IRUSR | S_IWUSR);

	    if (!writehist(hist, searchage) || !writehist(hist, replaceage))
		fprintf(stderr, _("Error writing %s: %s\n"), nanohist,
			strerror(errno));

	    fclose(hist);
	}

	free(nanohist);
    }
}

/* Save the recorded last file positions to ~/.nano/filepos_history. */
void save_poshistory(void)
{
    char *poshist = poshistfilename();
    poshiststruct *posptr;
    FILE *hist;

    if (poshist == NULL)
	return;

    hist = fopen(poshist, "wb");

    if (hist == NULL)
	fprintf(stderr, _("Error writing %s: %s\n"), poshist, strerror(errno));
    else {
	/* Don't allow others to read or write the history file. */
	chmod(poshist, S_IRUSR | S_IWUSR);

	for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
	    char *path_and_place;
	    size_t length;

	    /* Assume 20 decimal positions each for line and column number,
	     * plus two spaces, plus the line feed, plus the null byte. */
	    path_and_place = charalloc(strlen(posptr->filename) + 44);
	    sprintf(path_and_place, "%s %ld %ld\n", posptr->filename,
			(long)posptr->lineno, (long)posptr->xno);
	    length = strlen(path_and_place);

	    /* Encode newlines in filenames as nulls. */
	    sunder(path_and_place);
	    /* Restore the terminating newline. */
	    path_and_place[length - 1] = '\n';

	    if (fwrite(path_and_place, sizeof(char), length, hist) < length)
		fprintf(stderr, _("Error writing %s: %s\n"),
					poshist, strerror(errno));
	    free(path_and_place);
	}
	fclose(hist);
    }
    free(poshist);
}

/* Update the recorded last file positions, given a filename, a line
 * and a column.  If no entry is found, add a new one at the end. */
void update_poshistory(char *filename, ssize_t lineno, ssize_t xpos)
{
    poshiststruct *posptr, *theone, *posprev = NULL;
    char *fullpath = get_full_path(filename);

    if (fullpath == NULL || fullpath[strlen(fullpath) - 1] == '/') {
	free(fullpath);
	return;
    }

    /* Look for a matching filename in the list. */
    for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
	if (!strcmp(posptr->filename, fullpath))
	    break;
	posprev = posptr;
    }

    /* Don't record files that have the default cursor position. */
    if (lineno == 1 && xpos == 1) {
	if (posptr != NULL) {
	    if (posprev == NULL)
		position_history = posptr->next;
	    else
		posprev->next = posptr->next;
	    free(posptr->filename);
	    free(posptr);
	}
	free(fullpath);
	return;
    }

    theone = posptr;

    /* If we didn't find it, make a new node; otherwise, if we're
     * not at the end, move the matching one to the end. */
    if (theone == NULL) {
	theone = (poshiststruct *)nmalloc(sizeof(poshiststruct));
	theone->filename = mallocstrcpy(NULL, fullpath);
	if (position_history == NULL)
	    position_history = theone;
	else
	    posprev->next = theone;
    } else if (posptr->next != NULL) {
	if (posprev == NULL)
	    position_history = posptr->next;
	else
	    posprev->next = posptr->next;
	while (posptr->next != NULL)
	    posptr = posptr->next;
	posptr->next = theone;
    }

    /* Store the last cursor position. */
    theone->lineno = lineno;
    theone->xno = xpos;
    theone->next = NULL;

    free(fullpath);
}

/* Check whether the given file matches an existing entry in the recorded
 * last file positions.  If not, return FALSE.  If yes, return TRUE and
 * set line and column to the retrieved values. */
bool has_old_position(const char *file, ssize_t *line, ssize_t *column)
{
    poshiststruct *posptr = position_history;
    char *fullpath = get_full_path(file);

    if (fullpath == NULL)
	return FALSE;

    while (posptr != NULL && strcmp(posptr->filename, fullpath) != 0)
	posptr = posptr->next;

    free(fullpath);

    if (posptr == NULL)
        return FALSE;

    *line = posptr->lineno;
    *column = posptr->xno;
    return TRUE;
}

/* Load the recorded file positions from ~/.nano/filepos_history. */
void load_poshistory(void)
{
    char *poshist = poshistfilename();
    FILE *hist;

    /* If the home directory is missing, do_rcfile() will have reported it. */
    if (poshist == NULL)
	return;

    hist = fopen(poshist, "rb");

    if (hist == NULL) {
	if (errno != ENOENT) {
	    /* When reading failed, don't save history when we quit. */
	    UNSET(POS_HISTORY);
	    history_error(N_("Error reading %s: %s"), poshist, strerror(errno));
	}
    } else {
	char *line = NULL, *lineptr, *xptr;
	size_t buf_len = 0;
	ssize_t read, count = 0;
	poshiststruct *record_ptr = NULL, *newrecord;

	/* Read and parse each line, and store the extracted data. */
	while ((read = getline(&line, &buf_len, hist)) > 5) {
	    /* Decode nulls as embedded newlines. */
	    unsunder(line, read);

	    /* Find where the x index and line number are in the line. */
	    xptr = revstrstr(line, " ", line + read - 3);
	    if (xptr == NULL)
		continue;
	    lineptr = revstrstr(line, " ", xptr - 2);
	    if (lineptr == NULL)
		continue;

	    /* Now separate the three elements of the line. */
	    *(xptr++) = '\0';
	    *(lineptr++) = '\0';

	    /* Create a new position record. */
	    newrecord = (poshiststruct *)nmalloc(sizeof(poshiststruct));
	    newrecord->filename = mallocstrcpy(NULL, line);
	    newrecord->lineno = atoi(lineptr);
	    newrecord->xno = atoi(xptr);
	    newrecord->next = NULL;

	    /* Add the record to the list. */
	    if (position_history == NULL)
		position_history = newrecord;
	    else
		record_ptr->next = newrecord;

	    record_ptr = newrecord;

	    /* Impose a limit, so the file will not grow indefinitely. */
	    if (++count > 200) {
		poshiststruct *drop_record = position_history;

		position_history = position_history->next;

		free(drop_record->filename);
		free(drop_record);
	    }
	}
	fclose(hist);
	free(line);
    }
    free(poshist);
}
#endif /* !DISABLE_HISTORIES */
