2007-09-01  Daniel Jacobowitz  <dan@codesourcery.com>

	PR gdb/2138
	From readline 5.2:
	* configure.in (CROSS_COMPILE): Initialize to empty.
	* configure: Regenerated.

2007-03-27  Brooks Moses  <brooks.moses@codesourcery.com>

	* Makefile.in: Add dummy "pdf" target.

2006-11-13  Denis Pilat  <denis.pilat@st.com>

	* terminal.c (_rl_get_screen_size): use wr and wc variable to store
	window size.

2006-10-21  Ulrich Weigand  <uweigand@de.ibm.com>

	* callback.c: Include "xmalloc.h".
	* Makefile.in: Add dependency.

2006-04-24  Daniel Jacobowitz  <dan@codesourcery.com>

	Imported readline 5.1, and upstream patches 001-004.

2006-03-21  Denis Pilat  <denis.pilat@st.com>

	* histfile.c (read_history_range): Remove '\r' character from
	history lines.

2005-02-10  Denis Pilat  <denis.pilat@st.com>

	* readline/terminal.c (_rl_get_screen_size): Get console size from
	the Windows API when compiling with MinGW.

2005-07-25  Mark Mitchell <mark@codesourcery.com>

	* input.c (rl_getc): Use getch to read console input on
	Windows.
	* readline.c (bind_arrow_keys_internal): Translate
	Windows keysequences into ANSI key sequences.
	* rldefs.h (NO_TTY_DRIVER): Define on MinGW.
	* rltty.c: Conditionalize on NO_TTY_DRIVER throughout.
	
2005-07-03  Mark Kettenis <kettenis@gnu.org>

	From Martin Simmons:
	* configure.in: Check for getpwnam instead of getpwname.
	* configure: Regenerate.

2005-05-09  Mark Mitchell <mark@codesourcery.com>

	* aclocal.m4: Use AC_TRY_LINK to check for mbstate_t.
	* complete.c (pwd.h): Guard with HAVE_PWD_H.
	(getpwent): Guard with HAVE_GETPWENT.
	(rl_username_completion_function): Guard use of getpwent.
	(endpwent): Likewise.
	* config.h.in (HAVE_FCNTL): New macro.
	(HAVE_GETPWENT): Likewise.
	(HAVE_GETPWNAM): Likewise.
	(HAVE_GETPWUID): Likewise.
	(HAVE_KILL): Likewise.
	(HAVE_PWD_H): Likewise.
	* configure: Regenerated.
	* configure.in: Handle MinGW when cross compiling.  Check for
	getpwnam, getpwent, getpwuid, kill, and pwd.h.
	* display.c (rl_clear_screen): Treat Windows like DOS.
	(insert_some_chars): Likewise.
	(delete_chars): Likewise.
	* shell.c (pwd.h): Guard with HAVE_PWD_H.
	(getpwuid): Guard with HAVE_GETPWUID.
	(sh_unset_nodelay_mode): Guard use of fnctl with HAVE_FNCTL_H.
	* signals.c (rl_signal_handler): Don't use SIGALRM or
	SIGQUIT if not defined.  Use "raise" if "kill" is not available.
	(rl_set_signals): Don't set handlers for SIGQUIT or SIGALRM if
	they are not defined.
	(rl_clear_signals): Likewise.
	* tilde.c (pwd.h): Guard with HAVE_PWD_H.
	(getpwuid): Guard declaration with HAVE_GETPWUID.
	(getpwnam): Guard declaration with HAVE_GETPWNAM.
	(tilde_expand_word): Guard use of getpwnam with HAVE_GETPWNAM.

2004-02-19  Andrew Cagney  <cagney@redhat.com>

	* config.guess: Update from version 2003-06-12 to 2004-02-16.
	* config.sub: Update from version 2003-06-13 to 2004-02-16.

2004-01-27  Elena Zannoni  <ezannoni@redhat.com>

        Merge in official patches to readline-4.3 from
	ftp://ftp.cwru.edu/pub/bash/readline-4.3-patches:
	NOTE: Patch-ID readline-43-004 was already applied (see below).

	* bind.c (rl_generic_bind): Pressing certain key sequences
	causes an infinite loop in _rl_dispatch_subseq with the `key' argument
	set to 256.  This eventually causes bash to exceed the stack size
	limit and crash with a segmentation violation.
	Patch-ID: readline43-001.

	* readline.c (_rl_dispatch_subseq): Repeating an edit in
	vi-mode with `.' does not work.
	Patch-ID: readline43-002.

	* mbutil.c (_rl_get_char_len, _rl_compare_chars,
	_rl_adjust_point): When in a locale with multibyte characters, the
	readline display updater will occasionally cause a
	segmentation fault when attempting to compute the length of the first
	multibyte character on the line.  
	Patch-ID: readline43-003.

	* vi_mode.c (_rl_vi_change_mbchar_case): Using the vi editing
	mode's case-changing commands in a locale with multibyte characters
	will cause garbage characters to be inserted into the editing buffer.
	Patch-ID: readline43-005.

2003-12-28  Eli Zaretskii  <eliz@elta.co.il>

	* readline.c (rl_save_state, rl_restore_state): Support systems
	that don't define SIGWINCH.

2003-12-25  Eli Zaretskii  <eliz@elta.co.il>

	* terminal.c (_rl_get_screen_size) [__DJGPP__]: Compute the
	screen width and height using console I/O.
	(_rl_init_terminal_io) [__MSDOS__]: Zero out all the _rl_term_*
	variables.  Convert to _rl_* naming scheme.
	(_rl_set_cursor) [__MSDOS__]: Ifdef away this function.

2003-12-23  Eli Zaretskii  <eliz@elta.co.il>

	* display.c (_rl_move_vert) [__MSDOS__]: Don't use undeclared
	variable `l'.  Use `delta' instead recomputing its value anew.
	Assign -delta to i instead of the other way around.

2003-12-11  Michael Chastain  <mec.gnu@mindspring.com>

	* rlmbutil.h: Require HAVE_MBSTATE_T for HANDLE_MULTIBYTE.
	Revert requirement of HAVE_MBRTOWC.  Delete macro definitions
	that attempted to fake mbstate_t if HAVE_MBSRTOCWS is defined
	and HAVE_MBSTATE_T is not defined.

2003-06-14  H.J. Lu <hongjiu.lu@intel.com>
 
	* support/config.guess: Update to 2003-06-12 version.
	* support/config.sub: Update to 2003-06-13 version.

2003-05-25  Mark Kettenis  <kettenis@gnu.org>

	* aclocal.m4: Don't add wcwidth.o if we don't have wchar.h.
	* configure: Regenerate.

2003-05-13  Andreas Jaeger  <aj@suse.de>

        * support/config.guess: Update to 2003-05-09 version.
        * support/config.sub: Update to 2003-05-09 version.

2003-03-03  Joel Brobecker  <brobecker@gnat.com>

	* aclocal.m4: Add check for mbrtowc.
	* config.h.in: Regenerate.
	* configure: Regenerate.
	* rlmbutil.h: Disable multi-byte if mbrtowc is not defined.

2003-03-03  Kris Warkentin  <kewarken@qnx.com>

	* aclocal.m4: Cause wcwidth check to substitute
	HAVE_WCWIDTH for building.
	* Makefile.in: Add wcwidth object to lib if required.
	* shlib/Makefile.in: Likewise.
	* configure: Regenerate.
	
2003-01-09  Michael Chastain  <mec@shout.net>

	From Chet Ramey, <chet@po.cwru.edu>, the readline maintainer:
	ftp://ftp.cwru.edu/pub/bash/readline-4.3-patches/readline43-004

	* display.c: Fix perverse screen refresh with UTF-8.
	When running in a locale with multibyte characters, the
	readline display updater will use carriage returns when
	drawing the line, overwriting any partial output already on
	the screen and not terminated by a newline.
	Patch-ID: readline43-004

2003-01-08  Chris Demetriou  <cgd@broadcom.com>

	* config.guess: Update to 2003-01-03 version.
	* config.sub: Update to 2003-01-03 version.

2002-12-16  Christopher Faylor  <cgf@redhat.com>

	* configure.in: Remove --enable-shared option.  It shouldn't be used
	for gdb.
	* configure: Regenerate.

2002-12-16  Christopher Faylor  <cgf@redhat.com>

	* config/cygwin.cache: Prime mbstate_t.

2002-12-06  Elena Zannoni  <ezannoni@redhat.com>

        Import of readline 4.3. NB: This import includes those gdb
        local changes that aren't in the official readline sources.

        * compat.c, mbutil.c, misc.c, rlmbutil.h, rltypedefs.h,
        text.c, doc/history.0, doc/history.3, support/wcwidth.c,
        examples/readlinebuf.h, examples/rlcat.c: New files.

        * CHANGELOG, CHANGES, INSTALL,  MANIFEST, Makefile.in, README,
        aclocal.m4, ansi_stdlib.h, bind.c, callback.c, chardefs.h,
        complete.c, config.h.in, configure, configure.in, display.c,
        emacs_keymap.c, funmap.c, histexpand.c, histfile.c, histlib.h,
        history.c, history.h, histsearch.c, input.c, isearch.c,
        keymaps.c, keymaps.h, kill.c, macro.c, nls.c, parens.c,
        posixdir.h, readline.c, readline.h, rlconf.h, rldefs.h,
        rlprivate.h, rlshell.h, rlstdc.h, rltty.c, savestring.c,
        search.c, shell.c, signals.c, terminal.c, tilde.c, tilde.h,
        undo.c, util.c, vi_keymap.c, vi_mode.c, xmalloc.c, xmalloc.h,
        doc/Makefile.in, doc/hist.texinfo, doc/hstech.texinfo,
        doc/hsuser.texinfo, doc/manvers.texinfo, doc/readline.3,
        doc/rlman.texinfo, doc/rltech.texinfo, doc/rluser.texinfo
        doc/rluserman.texinfo, doc/texi2dvi, doc/texi2html,
        shlib/Makefile.in, support/install.sh, support/mkdirs,
        support/mkdist, support/shlib-install, support/shobj-conf,
        examples/Inputrc, examples/Makefile.in, examples/fileman.c,
        examples/histexamp.c, examples/manexamp.c, examples/rl.c,
        examples/rlfe.c, examples/rltest.c, examples/rlversion.c:
        Modified files.

2002-08-23  Andrew Cagney  <ac131313@redhat.com>

	* support/config.guess: Import version 2002-08-23.
	* support/config.sub: Import version 2002-08-22.

2002-07-19  Chris Demetriou  <cgd@broadcom.com>

	* support/config.guess: Update from ../config.guess.
	* support/config.sub: Update from ../config.sub.

2002-02-24  Elena Zannoni  <ezannoni@redhat.com>

        * ChangeLog.gdb: Renamed from ChangeLog.Cygnus.

2002-02-24  Daniel Jacobowitz  <drow@mvista.com>

        * support/config.guess: Import from master sources, rev 1.232.
        * support/config.sub: Import from master sources, rev 1.246.

2002-02-01  Ben Elliston  <bje@redhat.com>

	* config.guess: Import from master sources, rev 1.229.
	* config.sub: Import from master sources, rev 1.240.

2002-01-17  H.J. Lu  (hjl@gnu.org)

	* support/config.guess: Import from master sources, rev 1.225.
	* support/config.sub: Import from master sources, rev 1.238.

2001-07-20  Andrew Cagney  <ac131313@redhat.com>

	* support/config.guess: Update using ../config.sub.

2001-07-16  Andrew Cagney  <ac131313@redhat.com>

	* support/config.sub: Update using ../config.sub.

2001-06-15  Elena Zannoni  <ezannoni@redhat.com>

        * configure.in: Add -fsigned-char to LOCAL_CFLAGS for Linux
        running on the IBM S/390.
	* configure: Ditto.

2001-01-07  Michael Sokolov  <msokolov@ivan.Harhan.ORG>

	* rltty.c (save_tty_chars): Fix compilation-stopping typo.

2000-07-10  Eli Zaretskii  <eliz@is.elta.co.il>

	* terminal.c (_rl_get_screen_size) [__DJGPP__]: Determine screen
	size via DJGPP-specific calls.
	(_rl_init_terminal_io) [__MSDOS__]: DJGPP-specific terminal
	initialization.
	(_rl_backspace) [__MSDOS__]: Don't call tputs.
	(ding) [__MSDOS__]: Use DJGPP-specific calls to support visible
	bell.

	* display.c (_rl_move_vert) [__MSDOS__]: Support cursor movement
	upwards with DJGPP-specific calls.
	(_rl_clear_to_eol) [__MSDOS__]: Don't call tputs.
	(_rl_clear_screen) [__MSDOS__]: Support clear-screen with
	DJGPP-specific calls.
	(insert_some_chars) [__MSDOS__]: Don't call tputs.
	(delete_chars) [__MSDOS__]: Don't call tputs.

2000-07-08  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

        * readline/readline.h: Ifdef out the export of savestring().
        It should not have been in the distribution.

2000-07-07  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

        * Import of readline 4.1.

        Locally modified files: Makefile.in, configure.in, configure
        (regenerated), config.h.in (regenerated), rltty.c,
 	shell.c signals.c.

        Locally added files: acconfig.h, config/*, config.h.bot,
        cross-build/*, doc/inc-hit.texinfo.

        New files: USAGE, rlprivate.h, rlshell.h, xmalloc.h.

2000-03-16  Eli Zaretskii  <eliz@is.elta.co.il>

	* support/shobj-conf: Shared libs are unsupported on MSDOS.

	* bind.c (_rl_read_file): Open files in binary mode.  Strip CR
	characters after reading the file.
	(rl_re_read_init_file, rl_read_init_file): Allow for _inputrc on
	DOS.

	* complete.c (username_completion_function): Don't bypass getpw*
	function calls for DJGPP.
	(Filename_completion_function): Handle d:foo/bar file names.

	* display.c (_rl_move_vert) [__GO32__]: fflush the stream, to make
	sure cursor position is up to date.
	(_rl_clear_screen) [__GO32__]: Clear screen and home the cursor.
	(insert_some_characters, delete_characters) [__DJGPP__]: Don't use
	memcpy.

	* histfile.c (read_history_range, history_truncate_file)
	(history_do_write) [__MSDOS__]: Allow for underscore instead of
	the leading dot in file names.

	* input.c: Don't use GO32-specific workarounds if HAVE_SELECT or
	HAVE_TERMIOS_H are defined.

	* readline.c: Don't disable signals if __DJGPP__ is defined.

	* rltty.c: Don't disable signals and don't bypass termios code for
	DJGPP (if HAVE_TERMIOS_H is defined).

	* signals.c: Don't disable signals for DJGPP.

	* terminal.c (_rl_get_screen_size) [__DJGPP__]: Initialize screen
	dimensions.
	(ding) [__DJGPP__]: Support visual bell.

1999-08-13  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	From Philippe De Muyter  <phdm@macqel.be>
	* shell.c (stdio.h): File included, for definition of NULL.
	* readline/rltty.c (get_tty_settings): Conditionalize
	call to set_winsize on TIOGWINSZ.

1999-07-30  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* Imported Readline 4.0. Integrated all the Cygnus
	local changes since last import.

	New files: rlstdc.h, savestring.c, shlib directory,
	doc/manvers.texinfo, examples/rlversion.c, 
	support/install-shlib, support/shobj-conf.

	Removed files: MANIFEST.doc, doc/inc-hist.texi.

1999-07-13  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* acconfig.h: Fix typo: it's GWINSZ_IN_SYS_IOCTL, not
	TIOCGWINSZ_IN_SYS_IOCTL.

	* config.h.in: Regenerate with autoheader.

1999-04-27  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* ChangeLog.Cygnus: new file. It is the old Changelog.
	* ChangeLog: removed. It was conflicting with CHANGELOG
	  on Windows.

1999-04-22  Jason Molenda  (jsm@bugshack.cygnus.com)

	* Makefile.in (install): Make comment about this change more explicit.

1999-04-22  Jason Molenda  (jsm@bugshack.cygnus.com)

        * Makefile.in (install): Don't install the final libreadline.a
        or .h files.

Tue Mar 23 10:56:08 1999  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	Patches from Robert Hoehne <robert.hoehne@gmx.net>:
 	
	* display.c: Change some terminal calls to work on DJGPP.
	* terminal.c: Likewise.
	* Makefile.in: Remove . from the VPATH directive.
	
Tue Mar  9 14:58:13 1999  Geoffrey Noer  <noer@cygnus.com>

	* support/config.sub: Recognize cygwin*, not just cygwin32.

Tue Feb  9 10:38:57 1999  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* configure.in: Do not use the ./support directory.
	* configure: Regenerate.

Wed Jan  6 12:24:19 1999  Christopher Faylor <cgf@cygnus.com>

	* configure.in: Use LOCAL_CFLAGS rather than CFLAGS for
	searching libtermcap directory.
	* configure: Regenerate.

Thu Dec 31 12:07:01 1998  Christopher Faylor <cgf@cygnus.com>

	* configure.in: Search devo libtermcap directory for termcap.h
	when compiling for cygwin.
	* configure: Regenerated.

1998-12-30  Michael Meissner  <meissner@cygnus.com>

	* Makefile.in (install): Only try to copy libreadline.a and
	libhistory.a if they exist.

Tue Dec 29 23:49:20 1998  Christopher Faylor <cgf@cygnus.com>

	* cross-build/cygwin.cache: Add a couple more known settings.
	* configure.in: Fix typo.
	* configure: Regenerated.

Tue Dec 29 18:11:28 1998  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* cross-build: new directory.
	
	* cross-build/cygwin.cache: new file. Used for Cygwin cross builds.

	* configure.in: added tests for cross-build for Cygwin.

1998-12-24  Jason Molenda  (jsm@bugshack.cygnus.com)

	* Makefile.in: Add CYGNUS LOCAL comment.
	* acconfig.h: Add missing defines.
	* config.h.bot: Add missing content.
	* configure, config.h.in: Regenerated.

Wed Dec 23 16:21:41 1998  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* Makefile.in: comment out the rule to rebuild configure by 
	running autoconf. 

Tue Dec 22 10:00:30 1998  Elena Zannoni  <ezannoni@kwikemart.cygnus.com>

	* shell.c (savestring): ifdef'd it out. 

	* Imported new version of Readline 2.2.1. Removed all the Cygnus
	local changes.

	New files: acconfig.h, aclocal.m4, ansi_stdlib.h, callback.c,
 	config.h.in, configure, histexpand.c, histfile.c, histlib.h,
 	histsearch.c, input.c, kill.c, macro.c, nls.c, posixdir.h,
 	posixjmp.h, posixstat.h, rlconf.h, rltty.h, rlwinsize.h, shell.c,
 	tcap.h, terminal.c, undo.c, util.c, support directory.

	Removed files: sysdep*, config directory.
	

Fri Dec  4 15:25:41 1998  David Taylor  <taylor@texas.cygnus.com>

	The following changes were made by Jim Blandy
 	<jimb@zwingli.cygnus.com> and David Taylor
 	<taylor@texas.cygnus.com> as part of a project to merge in changes
 	made by HP; HP did not create ChangeLog entries.

	* config/mp-enable-tui: New file.
	(TUI_CFLAGS): Search devo's include directory, as long as we're
 	totally ruining modularity.
	(INCLUDE_SRCDIR): New var.
	(GDB_TUI_SRCDIR): Fix syntax error.

	* configure.in: Check the --enable-tui flag; if it's set, include
 	a makefile fragment that #defines TUI and adds the needed #include
 	directories.
	(*-*-hpux*): New host; use sysdep-hpux.h.

	* Makefile.in (.c.o): Check the variable set in the makefile
	fragment above.

	* display.c (term_goto): declare it.
	(insert_some_chars): set it.
	(delete_chars): set it.

	* readline.c: add tui include files surrounded by TUI.
	(rl_reset): new function, move some of rl_abort functionality to
 	here.
	(rl_abort): call rl_reset.
	(rl_getc): tui changes.
	(init_terminal_io): tui changes.

	* readline.h (tui_version, fputc_unfiltered, fputs_unfiltered,
	tui_tputs): declare if TUI is defined.

	* rltty.c (prepare_terminal_settings): additional comment.

	* signals.c: add tui include files surrounded by TUI.  move #if
 	and #endif to column 1 so HP's compiler will accept them.  Remove
 	declaration of tuiDoAndReturnToTop since it's declared in tui.h.
	(rl_handle_sigwinch): call tuiDoAndReturnToTop if TUI defined.
  	(rl_handle_sigwinch_on_clear): define if TUI defined.
	(rl_set_signals): if TUI, avoid infinite recursion.
	(rl_clear_signals): install rl_handle_sigwinch_on_clear.

	* sysdep-hpux.h: New file.

Mon Nov  2 15:26:33 1998  Geoffrey Noer  <noer@cygnus.com>

        * configure.in: Check cygwin* instead of cygwin32*.

Tue Jul 28 09:43:27 1998  Jeffrey A Law  (law@cygnus.com)

	* sysdep-hpux11.h: New file.
	* configure.in (*-*-*-hpux11*): Use sysdep-hpux11.h.

Thu Jul 23 17:48:21 1998  Ian Lance Taylor  <ian@cygnus.com>

	* configure.bat: Remove obsolete file.
	* examples/configure.bat: Remove obsolete file.

Wed May 13 13:41:53 1998  Ian Lance Taylor  <ian@cygnus.com>

	* sysdep-6irix.h: New file.
	* configure.in (*-*-irix6*): New host; use sysdep-6irix.h.

	* Makefile.in (isearch.o, search.o): Depend upon sysdep.h.
	(Makefile): Depend upon $(srcdir)/configure.in.

Thu Apr  9 11:59:38 1998  Ian Dall (<Ian.Dall@dsto.defence.gov.au>

	* configure.in (host==netbsd): Include config/mh-bsd44.
	* config/mh-bsd44: New file.

Wed Dec  3 16:48:20 1997  Michael Snyder  (msnyder@cleaver.cygnus.com)

	* rltty.c: fix typos.

Tue Oct  8 08:59:24 1996  Stu Grossman  (grossman@critters.cygnus.com)

	* tilde.c (tilde_word_expand):  __MSDOS___ -> __MSDOS__

Sat Oct 05 11:24:34 1996  Mark Alexander  <marka@cygnus.com>

	* rldefs.h: On Linux, include <termios.h> to fix compile error
	in <termcap.h>.

Wed Sep  4 18:06:51 1996  Stu Grossman  (grossman@critters.cygnus.com)

	* rldefs.h:  Enable HANDLE_SIGNALS for cygwin32.

Thu Aug 29 16:59:45 1996  Michael Meissner  <meissner@tiktok.cygnus.com>

	* configure.in (i[345]86-*-*): Recognize i686 for pentium pro.

Fri Aug 16 17:49:57 1996  Stu Grossman  (grossman@critters.cygnus.com)

	* complete.c:  Include <pwd.h> if not DOS, and if cygwin32 or not
	win32.
	* configure.in:  Add test for *-*-cygwin32* to use config/mh-posix.
	* readline.c:  Move decl of tgetstr to rldefs.h.
	* (_rl_set_screen_size):  Remove redundant ifdef MINIMALs.
	* rldefs.h:  Don't do MINIMAL for cygwin32.  Cygwin32 now uses
	full-blown readline, except for termcap.

Sun Aug 11 21:06:26 1996  Stu Grossman  (grossman@critters.cygnus.com)

	* rldefs.c:  Get rid of define of SIGALRM if _WIN32 or __MSDOS__.
	* Don't define ScreenCols/ScreenRows/... if cygwin32.
	* sysdep-norm.h:  Don't include <malloc.h> if cygwin32.

Sun Aug 11 14:59:09 1996  Fred Fish  <fnf@cygnus.com>

	* rldefs.h:  If __osf__is defined, include <termio.h> instead of
	<sgtty.h>.

Fri Aug  9 08:54:26 1996  Stu Grossman  (grossman@critters.cygnus.com)

	* bind.c complete.c history.c readline.c:  Don't include sys/file.h.
	* complete.c display.c parens.c readline.c rldefs.h rltty.c
	signals.c tilde.c:  Change refs to _MSC_VER and __WIN32__ to _WIN32.
	* signals.c (rl_signal_handler):  Ifdef out kill if _WIN32.
	* sysdep-norm.h:  Ifdef out include of dirent.h if _WIN32.
	Include malloc.h if _WIN32.

Thu Jul 18 15:59:35 1996  Michael Meissner  <meissner@tiktok.cygnus.com>

	* rldefs.h (sys/uio.h) Before sys/stream.h is included under AIX,
	include sys/uio.h, which prevents an undefined structure used in a
	prototype message from being generated.

Tue Jun 25 23:05:55 1996  Jason Molenda  (crash@godzilla.cygnus.co.jp)

        * Makefile.in (datadir): Set to $(prefix)/share.
	(docdir): Removed.

Sun May 26 15:14:42 1996  Fred Fish  <fnf@cygnus.com>

	From: David Mosberger-Tang  <davidm@azstarnet.com>

	* sysdep-linux.h: New file.
	* display.c: Add include of "sysdep.h" to get HAVE_VARARGS_H.
	* configure.in: Change pattern i[345]86-*-linux* into *-*-linux* to
	support non-x86 based Linux platforms.

Sun Apr  7 22:06:11 1996  Fred Fish  <fnf@cygnus.com>

	From: Miles Bader  <miles@gnu.ai.mit.edu>
	* config/mh-gnu: New file.
	* configure.in (*-*-gnu*): New host.

Sun Apr  7 13:21:51 1996  Fred Fish  <fnf@cygnus.com>

	From: Robert Lipe <robertl@dgii.com>
	* configure.in: SCO OpenServer 5 (a.k.a 3.2v5*) is more like
	SCO 3.2v4 than 3.2v2.

Wed Jan  3 18:22:10 1996  steve chamberlain  <sac@slash.cygnus.com>

	* readline.c, display.c, complete.c: Add _MSC_VER to list of
	things which can't do most things.

Thu Nov 16 15:39:05 1995  Geoffrey Noer <noer@cygnus.com>

	* complete.c: Change WIN32 to __WIN32__, added #else return NULL
	to end of that define.

Tue Oct 31 10:38:58 1995  steve chamberlain  <sac@slash.cygnus.com>

	* display.c, parens.c, readline.c, rldefs.h: Change use of
	WIN32 to __WIN32__.

Tue Oct 10 11:07:23 1995  Fred Fish  <fnf@cygnus.com>

	* Makefile.in (BISON): Remove macro.

Tue Oct 10 08:49:00 1995  steve chamberlain  <sac@slash.cygnus.com>

	* complete.c (filename_completion_function): Enable for
	win32 when not MSC.

Sun Oct  8 04:17:19 1995  Peter Schauer  (pes@regent.e-technik.tu-muenchen.de)

	* configure.in:  Handle powerpc-ibm-aix* like rs6000-ibm-aix*.

Sat Oct  7 20:36:16 1995  Michael Meissner  <meissner@cygnus.com>

	* rltty.c (outchar): Return an int, like tputs expects.
	* signals.c (_rl_output_character_function): Ditto.

Fri Sep 29 15:19:23 1995  steve chamberlain  <sac@slash.cygnus.com>

	Fixes for when the host WIN32, but not MSC.
	* complete.c: Sometimes have pwd.h
	* parens.c: WIN32 has similar restrictions to __GO32__.
	* readline.c (__GO32__): Some of this moved into rldefs.h
	* signals.c (__GO32__): Likewise.
	* rldefs.h (MSDOS||WIN32) becomes MSDOS||MSC.
	(WIN32&&!WIN32): New definitions.

Wed Sep 20 12:57:17 1995  Ian Lance Taylor  <ian@cygnus.com>

	* Makefile.in (maintainer-clean): New synonym for realclean.

Wed Mar  1 13:33:43 1995  Michael Meissner  <meissner@tiktok.cygnus.com>

	* rltty.c (outchar): Provide prototype for outchar, to silence
	type warnings in passing outchar to tputs on systems like Linux
	that have full prototypes.

	* signals.c (_rl_output_character_function): Provide prototype to
	silence type warnings.

Sun Jan 15 14:10:37 1995  Steve Chamberlain  <sac@splat>

	* rldefs.h: Define MINIMAL for __GO32__ and WIN32.
	* complete.c, display.c, readline.c, rltty.c: Test MINIMAL
	instead of __GO32__.

Wed Aug 24 13:04:47 1994  Ian Lance Taylor  (ian@sanguine.cygnus.com)

	* configure.in: Change i[34]86 to i[345]86.

Sat Jul 16 13:26:31 1994  Stan Shebs  (shebs@andros.cygnus.com)

	* configure.in (m88*-harris-cxux7*): Recognize.
	* sysdep-cxux7.h: New file.

Fri Jul  8 13:18:33 1994  Steve Chamberlain  (sac@jonny.cygnus.com)

	* rttty.c (control_meta_key_on): Remove superfluous testing of
	__GO32__.

Thu Jun 30 15:21:54 1994  Steve Chamberlain  (sac@jonny.cygnus.com)

	* rltty.c (control_meta_key_on): Don't compile if __GO32__ is
	defined.
	(rltty_set_default_bindings): Likewise.
	* display.c (insert_some_chars, delete_chars): row_start should be
	a short.
	* parens.c (rl_insert_close): No FD_SET if using __GO32__.
	* readline.c (rl_gather_tyi): Strip off spurious high bits.

Sun Jun 12 03:51:52 1994  Peter Schauer  (pes@regent.e-technik.tu-muenchen.de)

	* history.c:  Swap inclusion of rldefs.h and chardefs.h to avoid
	CTRL macro redefinition.

Mon May  9 18:29:42 1994  Ian Lance Taylor  (ian@tweedledumb.cygnus.com)

	* readline.c (readline_default_bindings): Don't compile if
	__GO32__ is defined.
	(_rl_set_screen_size): Likewise.
	* rltty.c (rltty_set_default_bindings): Likewise.
	(control_meta_key): Likewise.
	* display.c: If __GO32__ is defined, include <sys/pc.h>.
	* parens.c: If __GO32__ is defined, undefine FD_SET.
	* signals.c: Include SIGWINCH handling in the set of things which
	is not done if HANDLE_SIGNALS is not set.

Fri May  6 13:38:39 1994  Steve Chamberlain  (sac@cygnus.com)

        * config/mh-go32: New fragment.
	* configure.in (host==go32): Use go32 fragment.

Wed May  4 14:36:53 1994  Stu Grossman  (grossman@cygnus.com)

	* chardefs.h, rldefs.h:  Move decls of string funcs from chardefs.h
	to rldefs.h so that they don't pollute apps that include
	readline.h.
	* history.c:  include rldefs.h to get decls of string funcs.

Wed May  4 12:15:11 1994  Stan Shebs  (shebs@andros.cygnus.com)

	* configure.in (rs6000-bull-bosx*): New configuration, RS/6000
	variant.

Wed Apr 20 10:43:52 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* configure.in: Use mh-posix for sunos4.1*.

Wed Apr 13 21:28:44 1994  Jim Kingdon  (kingdon@deneb.cygnus.com)

	* rltty.c (set_tty_settings): Don't set readline_echoing_p.
	(rl_deprep_terminal) [NEW_TTY_DRIVER]: Set readline_echoing_p.

Sun Mar 13 09:13:12 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* Makefile.in: Add TAGS target.

Wed Mar  9 18:01:31 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* isearch.c, search.c: Include sysdep.h.

Thu Mar  3 17:40:03 1994  Jim Kingdon  (kingdon@deneb.cygnus.com)

	* configure.in: For ISC, use mh-sysv, not mh-isc.

Thu Feb 24 04:13:53 1994  Peter Schauer  (pes@regent.e-technik.tu-muenchen.de)

	* Merge in changes from bash-1.13.5. Merge changes from glob/tilde.c
	into tilde.c and use it. Add system function declarations where
	necessary. Check for __GO32__, not _GO32_ consistently.
	* Makefile.in:  Update dependencies.
	* rltty.c:  Include <sys/file.h> to match include file setup
	in readline.c for rldefs.h. Otherwise we get inconsistent
	TTY_DRIVER definitions in readline.c and rltty.c.
	* bind.c, complete.c:  Do not include <sys/types.h>, it is already
	included via sysdep.h, which causes problems if <sys/types.h> has
	no multiple inclusion protection.
	* readline.c (_rl_set_screen_size):  Reestablish test for
	TIOCGWINSZ_BROKEN.
	* rldefs.h:  Define S_ISREG if necessary.

Fri Feb 18 08:56:35 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* Makefile.in: Add search.o rule for Sun make.

Wed Feb 16 16:35:49 1994  Per Bothner  (bothner@kalessin.cygnus.com)

	* rltty.c:  #if out some code if __GO32__.

Tue Feb 15 14:07:08 1994  Per Bothner  (bothner@kalessin.cygnus.com)

	* readline.c (_rl_output_character_function), display.c:
	Return int, not void, to conform with the expected arg of tputs.
	* readline.c (init_terminal_io):  tgetflag only takes 1 arg.
	* readline.c (_rl_savestring):  New function.
	* chardefs.h:  To avoid conflicts and/or warnings, define
	savestring as a macro wrapper for _rl_savestring.
	* display.c (extern term_xn):  It's an int flag, not a string.
	* charsdefs.h, rldefs.h:  Remove HAVE_STRING_H-related junk.

Sat Feb  5 08:32:30 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* Makefile.in: Remove obsolete rules for history.info and
	readline.info.

Thu Jan 27 17:04:01 1994  Jim Kingdon  (kingdon@deneb.cygnus.com)

	* chardefs.h: Only declare strrchr if it is not #define'd.

Tue Jan 25 11:30:06 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* rldefs.h: Accept __hpux as well as hpux for HP compiler in ANSI mode.

Fri Jan 21 17:31:26 1994  Jim Kingdon  (kingdon@lisa.cygnus.com)

	* chardefs.h, tilde.c: Just declare strrchr rather than trying to
	include a system header.

Fri Jan 21 14:40:43 1994  Fred Fish  (fnf@cygnus.com)

	* Makefile.in (distclean, realclean):  Expand local-distclean
	inline after doing recursion.  You can't recurse after removing
	Makefile.  Make them depend on local-clean.
	* Makefile.in (local-distclean):  Remove now superfluous target.

Mon Jan 17 12:42:07 1994  Ken Raeburn  (raeburn@cujo.cygnus.com)

	* readline.c (doing_an_undo): Delete second declaration, since it
	confuses the alpha-osf1 native compiler.

Sun Jan 16 12:33:11 1994  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* complete.c, bind.c: Include <sys/stat.h>.
	* complete.c: Define X_OK if not defined by a system header.

	* chardefs.h: Don't declare xmalloc.

	* keymaps.h: Include "chardefs.h" not <readline/chardefs.h>.

	* Makefile.in (clean mostlyclean distclean realclean): Recurse
	into subdirectories as well as doing this directory.  Add clean-dvi
	target.

Sat Jan 15 19:36:12 1994  Per Bothner  (bothner@kalessin.cygnus.com)

	* readline.c, display.c:  Patches to allow use of all 80
	columns on most terminals (those with am and xn).

	Merge in changes from bash-1.13.  The most obvious one is
	that the file readline.c has been split into multiple files.
	* bind.c, complete.c, dispay.c, isearch.c, parens.c, rldefs.h,
	rltty.c, search.c signals.c, tilde.c, tilde.h, xmalloc.c:  New files.

Sat Dec 11 16:29:17 1993  Steve Chamberlain  (sac@thepub.cygnus.com)

	* readline.c (rl_getc): If GO32, trim high bit from getkey,
	otherwise fancy PC keys cause grief.

Fri Nov  5 11:49:47 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* configure.in: Add doc to configdirs.
	* Makefile.in (info dvi install-info clean-info): Recurse into doc.

Fri Oct 22 07:55:08 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* configure.in: Add * to end of all OS names.

Tue Oct  5 12:33:51 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* readline.c: Add stuff for HIUX to place where we detect termio
	vs. sgtty (ugh, but I don't see a simple better way).

Wed Sep 29 11:02:58 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* readline.c (parser_if): Free tname when done with it (change
	imported from from bash	1.12 readline).

Tue Sep  7 17:15:37 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* configure.in (m88k-*-sysvr4*): Comment out previous change.

Fri Jul  2 11:05:34 1993  Ian Lance Taylor  (ian@cygnus.com)

	* configure.in (*-*-riscos*): New entry; use mh-sysv.

Wed Jun 23 13:00:12 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* configure.in: Add comment.

Mon Jun 14 14:28:55 1993  Jim Kingdon  (kingdon@eric)

	* configure.in (m88k-*-sysvr4*): Use sysdep-norm.h.

Sun Jun 13 13:04:09 1993  Jim Kingdon  (kingdon@cygnus.com)

	* Makefile.in ({real,dist}clean): Remove sysdep.h.

Thu Jun 10 11:22:41 1993  Jim Kingdon  (kingdon@cygnus.com)

	* Makefile.in: Add mostlyclean, distclean, and realclean targets.

Fri May 21 17:09:28 1993  Jim Kingdon  (kingdon@lioth.cygnus.com)

	* config/mh-isc: New file.
	* configure.in: Use it.

Sat Apr 17 00:40:12 1993  Jim Kingdon  (kingdon at calvin)

	* readline.c, history.c: Don't include sys/types.h; sysdep.h does.

	* config/mh-sysv: Define TIOCGWINSZ_BROKEN.
	readline.c: Check it.

Wed Mar 24 02:06:15 1993  david d `zoo' zuhn  (zoo at poseidon.cygnus.com)

	* Makefile.in: add installcheck & dvi targets

Fri Mar 12 18:36:53 1993  david d `zoo' zuhn  (zoo at cirdan.cygnus.com)

	* configure.in: recognize *-*-solaris2* instead of *-*-solaris* (a
	number of people want to call SunOS 4.1.2 "solaris1.0"
	and get it right)

Tue Mar  2 21:25:36 1993  Fred Fish  (fnf@cygnus.com)

	* sysdep-sysv4.h:  New file for SVR4.
	* configure.in (*-*-sysv4*):  Use sysdep-sysv4.h.

	* configure.in (*-*-ultrix2):  Add triplet from Michael Rendell
	(michael@mercury.cs.mun.ca)

Tue Dec 15 12:38:16 1992  Ian Lance Taylor  (ian@cygnus.com)

	* configure.in (i[34]86-*-sco3.2v4*): use mh-sco4.
	* config/mh-sco4: New file, like mh-sco but without defining
	_POSIX_SOURCE.

Wed Nov 11 21:20:14 1992  John Gilmore  (gnu@cygnus.com)

	* configure.in:  Reformat to one-case-per-line.
	Handle SunOS 3.5, as per Karl Berry, <karl@claude.cs.umb.edu>.

Wed Nov  4 15:32:31 1992  Stu Grossman  (grossman at cygnus.com)

	* sysdep-norm.h:  Remove some crud, install dire warning.

Thu Oct 22 01:08:13 1992  Stu Grossman  (grossman at cygnus.com)

	* configure.in:  Make SCO work again...

Mon Oct 12 15:04:07 1992  Ian Lance Taylor  (ian@cygnus.com)

	* readline.c (init_terminal_io): if tgetent returns 0, the
	terminal type is unknown.

Thu Oct  1 23:44:14 1992  david d `zoo' zuhn  (zoo at cirdan.cygnus.com)

	* configure.in: use cpu-vendor-os triple instead of nested cases

Wed Sep 30 12:58:57 1992  Stu Grossman  (grossman at cygnus.com)

	* readline.c (rl_complete_internal):  Cast alloca to (char *) to
	avoid warning.

Fri Sep 25 12:45:05 1992  Stu Grossman  (grossman at cygnus.com)

	* readline.c (clear_to_eol, rl_generic_bind):  Make static.
	(rl_digit_loop):  Add arg to call to rl_message().
	* vi_mode.c (rl_vi_first_print):  Add arg to call to
	rl_back_to_indent().

Wed Aug 19 14:59:07 1992  Ian Lance Taylor  (ian@cygnus.com)

	* Makefile.in: always create installation directories, use full
	file name for install target.

Wed Aug 12 15:50:57 1992  John Gilmore  (gnu@cygnus.com)

	* readline.c (last_readline_init_file):  Fix typo made by Steve
	Chamberlain/DJ Delorie.  Proper control file name is ~/.inputrc,
	not ~/inputrc.

Thu Jun 25 16:15:27 1992  Stu Grossman  (grossman at cygnus.com)

	* configure.in:  Make bsd based systems use sysdep-obsd.h.

Tue Jun 23 23:22:53 1992  Per Bothner  (bothner@cygnus.com)

	* config/mh-posix:  New file, for Posix-compliant systems.
	* configure.in:  Use mh-posix for Linux (free Unix clone).

Tue Jun 23 21:59:20 1992  Fred Fish  (fnf@cygnus.com)

	* sysdep-norm.h (alloca):  Protect against previous definition as
	a macro with arguments.

Fri Jun 19 15:48:54 1992  Stu Grossman  (grossman at cygnus.com)

	* sysdep-obsd.h:  #include <sys/types.h> to make this more Kosher.

Fri Jun 19 12:53:28 1992  John Gilmore  (gnu at cygnus.com)

	* config/mh-apollo68v, mh-sco, mh-sysv, mh-sysv4}: RANLIB=true.

Mon Jun 15 13:50:34 1992  david d `zoo' zuhn  (zoo at cirdan.cygnus.com)

	* configure.in: use mh-sysv4 on solaris2

Mon Jun 15 12:28:24 1992  Fred Fish  (fnf@cygnus.com)

	* config/mh-ncr3000 (INSTALL):  Don't use /usr/ucb/install,
	it is broken on ncr 3000's.
	* config/mh-ncr3000 (RANLIB):  Use RANLIB=true.

Mon Jun 15 01:35:55 1992  John Gilmore  (gnu at cygnus.com)

	* readline.c: Make new SIGNALS_* macros to parameterize the 
	ugly changes in signal blocking.  Use them throughout,
	reducing #ifdef HAVE_POSIX_SIGNALS and HAVE_BSD_SIGNALS clutter
	significantly.  Make all such places use POSIX if available,
	to avoid losing with poor `sigsetmask' emulation from libiberty.

Sun Jun 14 15:19:51 1992  Stu Grossman  (grossman at cygnus.com)

	* readline.c (insert_some_chars):  Return void.

Thu Jun 11 01:27:45 1992  John Gilmore  (gnu at cygnus.com)

	* readline.c:  #undef PC, which Solaris2 defines in sys/types.h,
	clobbering the termcap global variable PC.

Tue Jun  9 17:30:23 1992  Fred Fish  (fnf@cygnus.com)

	* config/{mh-ncr3000, mh-sysv4}:  Change INSTALL to use
	/usr/ucb/install.

Mon Jun  8 23:10:07 1992  Fred Fish  (fnf@cygnus.com)

	* readline.h (rl_completer_quote_characters):  Add declaration.
	* readline.c (rl_completer_quote_characters):  Add global var.
	* readline.c (strpbrk):  Add prototype and function.
	* readline.c (rl_complete_internal):  Add code to handle
	expansion of quoted strings.

Mon May 11 12:39:30 1992  John Gilmore  (gnu at cygnus.com)

	* readline.c:  Can't initialize FILE *'s with stdin and stdout,
	because they might not be constant.  Patch from Tom Quinn,
	trq@dinoysos.thphys.ox.ac.uk.

Tue Apr 28 21:52:34 1992  John Gilmore  (gnu at cygnus.com)

	* readline.h:  Declare rl_event_hook (which already existed).
	Suggested by Christoph Tietz <tietz@zi.gmd.dbp.de>.

Wed Apr 22 18:08:01 1992  K. Richard Pixley  (rich@rtl.cygnus.com)

	* configure.in: remove subdirs declaration.  The obsolete semantic
	  for subdirs has been usurped by per's new meaning.

Tue Apr 21 11:54:23 1992  K. Richard Pixley  (rich@cygnus.com)

	* Makefile.in: rework CFLAGS so that they can be set on the
	  command line to make.  Remove MINUS_G.  Default CFLAGS to -g.

Fri Apr 10 23:02:27 1992  Fred Fish  (fnf@cygnus.com)

	* configure.in:  Recognize new ncr3000 config.
	* config/mh-ncr3000:  New NCR 3000 config file.

Wed Mar 25 10:46:30 1992  John Gilmore  (gnu at cygnus.com)

	* history.c (stifle_history):  Negative arg treated as zero.

Tue Mar 24 23:46:20 1992  K. Richard Pixley  (rich@cygnus.com)

	* config/mh-sysv: INSTALL_PROG -> INSTALL.

Mon Feb 10 01:41:35 1992  Brian Fox  (bfox at gnuwest.fsf.org)

	* history.c (history_do_write) Build a buffer of all of the lines
	to write and write them in one fell swoop (lower overhead than
	calling write () for each line).  Suggested by Peter Ho.

	* vi_mode.c (rl_vi_subst) Don't forget to end the undo group.

Sat Mar  7 00:15:36 1992  K. Richard Pixley  (rich@rtl.cygnus.com)

	* Makefile.in: remove FIXME's on info and install-info targets.

Fri Mar  6 22:02:04 1992  K. Richard Pixley  (rich@cygnus.com)

	* Makefile.in: added check target.

Wed Feb 26 18:04:40 1992  K. Richard Pixley  (rich@cygnus.com)

	* Makefile.in, configure.in: removed traces of namesubdir,
	  -subdirs, $(subdir), $(unsubdir), some rcs triggers.  Forced
	  copyrights to '92, changed some from Cygnus to FSF.

Fri Feb 21 14:37:32 1992  Steve Chamberlain  (sac at rtl.cygnus.com)

	* readline.c, examples/fileman.c: patches from DJ to support DOS

Thu Feb 20 23:23:16 1992  Stu Grossman  (grossman at cygnus.com)

	* readline.c (rl_read_init_file):  Make sure that null filename is
	not passed to open() or else we end up opening the directory, and
	read a bunch of garbage into keymap[].

Mon Feb 17 17:15:09 1992  Fred Fish  (fnf at cygnus.com)

	* readline.c (readline_default_bindings):  Only make use of VLNEXT
	when both VLNEXT and TERMIOS_TTY_DRIVER is defined.  On SVR4
	<termio.h> includes <termios.h>, so VLNEXT is always defined.

	* sysdep-norm.h (_POSIX_VERSION):  Define this for all SVR4
	systems so that <termios.h> gets used, instead of <termio.h>.

Fri Dec 20 12:04:31 1991  Fred Fish  (fnf at cygnus.com)

	* configure.in:  Change svr4 references to sysv4.

Tue Dec 10 04:07:20 1991  K. Richard Pixley  (rich at rtl.cygnus.com)

	* Makefile.in: infodir belongs in datadir.

Fri Dec  6 23:23:14 1991  K. Richard Pixley  (rich at rtl.cygnus.com)

	* Makefile.in: remove spaces following hyphens, bsd make can't
	  cope. added clean-info.  added standards.text support.  Don't
	  know how to make info anymore.

	* configure.in: commontargets is no longer a recognized hook, so
	  remove it.  new subdir called doc.

Thu Dec  5 22:46:10 1991  K. Richard Pixley  (rich at rtl.cygnus.com)

	* Makefile.in: idestdir and ddestdir go away.  Added copyrights
	  and shift gpl to v2.  Added ChangeLog if it didn't exist. docdir
	  and mandir now keyed off datadir by default.

Fri Nov 22 09:02:32 1991  John Gilmore  (gnu at cygnus.com)

	* sysdep-obsd.h:  Rename from sysdep-newsos.h.
	* configure.in:  Use sysdep-obsd for Mach as well as NEWs.

	* sysdep-norm.h, sysdep-aix.h:  Add <sys/types.h>, which POSIX
	requires to make <dirent.h> work.  Improve Sun alloca decl.

Thu Nov 21 18:48:08 1991  John Gilmore  (gnu at cygnus.com)

	* Makefile.in:  Clean up ../glob/tilde.c -> tilde.o path.
	Clean up makefile a bit in general.

Thu Nov 21 14:40:29 1991  Stu Grossman  (grossman at cygnus.com)

	* configure.in, config/mh-svr4:  Make SVR4 work.

	* readline.c:  Move config stuff to sysdep.h, use typedef dirent
	consistently, remove refs to d_namlen (& D_NAMLEN) to improve
	portability.  Also, update copyright notice.
	readline.h:  remove config stuff that I added erroneously in the
	first place.

	* emacs_keymap.c, funmap.c, history.c, keymaps.c, vi_keymap.c,
	vi_mode.c:  move config stuff to sysdep.h, update copyright notices.

Tue Nov 19 15:02:13 1991  Stu Grossman  (grossman at cygnus.com)

	* history.c:  #include "sysdep.h".

Tue Nov 19 10:49:17 1991  Fred Fish  (fnf at cygnus.com)

	* Makefile.in, config/hm-sysv, config/hm-sco:  Change SYSV to
	USG to match current usage.

	* readline.c:  Add USGr4 to list of defined things to check for
	to use <dirent.h> style directory access.

	* config/hm-svr4:  New file for System V Release 4 (USGr4).

Mon Nov 18 23:59:52 1991  Stu Grossman  (grossman at cygnus.com)

	* readline.c (filename_completion_function):  use struct dirent
	instead	of struct direct.

Fri Nov  1 07:02:13 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c (rl_translate_keyseq) Make C-? translate to RUBOUT
	unconditionally.

Mon Oct 28 11:34:52 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c; Use Posix directory routines and macros.

	* funmap.c; Add entry for call-last-kbd-macro.

	* readline.c (rl_prep_term); Use system EOF character on POSIX
	systems also.

Thu Oct  3 16:19:53 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c; Make a distinction between having a TERMIOS tty
	driver, and having POSIX signal handling.  You might one without
	the other.  New defines used HAVE_POSIX_SIGNALS, and
	TERMIOS_TTY_DRIVER.

Tue Jul 30 22:37:26 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: rl_getc () If a call to read () returns without an
	error, but with zero characters, the file is empty, so return EOF.

Thu Jul 11 20:58:38 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: (rl_get_next_history, rl_get_previous_history)
	Reallocate the buffer space if the line being moved to is longer
	the the current space allocated.  Amazing that no one has found
	this bug until now.

Sun Jul  7 02:37:05 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c:(rl_parse_and_bind) Allow leading whitespace.
	  Make sure TERMIO and TERMIOS systems treat CR and NL
	  disctinctly.
	
Tue Jun 25 04:09:27 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: Rework parsing conditionals to pay attention to the
	prior states of the conditional stack.  This makes $if statements
	work correctly.

Mon Jun 24 20:45:59 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: support for displaying key binding information
	includes the functions rl_list_funmap_names (),
	invoking_keyseqs_in_map (), rl_invoking_keyseqs (),
	rl_dump_functions (), and rl_function_dumper ().

	funmap.c: support for same includes rl_funmap_names ().

	readline.c, funmap.c: no longer define STATIC_MALLOC.  However,
	update both version of xrealloc () to handle a null pointer.

Thu Apr 25 12:03:49 1991  Brian Fox  (bfox at gnuwest.fsf.org)

	* vi_mode.c (rl_vi_fword, fWord, etc.  All functions use
	the macro `isident()'.  Fixed movement bug which prevents
	continious movement through the text.

Fri Jul 27 16:47:01 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c (parser_if) Allow "$if term=foo" construct.

Wed May 23 16:10:33 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c (rl_dispatch) Correctly remember the last command
	executed.  Fixed typo in username_completion_function ().

Mon Apr  9 19:55:48 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: username_completion_function (); For text passed in
	with a leading `~', remember that this could be a filename (after
	it is completed).

Thu Apr  5 13:44:24 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: rl_search_history (): Correctly handle case of an
	unfound search string, but a graceful exit (as with ESC).

	* readline.c: rl_restart_output ();  The Apollo passes the address
	of the file descriptor to TIOCSTART, not the descriptor itself.

Tue Mar 20 05:38:55 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* readline.c: rl_complete (); second call in a row causes possible
	completions to be listed.

	* readline.c: rl_redisplay (), added prompt_this_line variable
	which is the first character character following \n in prompt.

Sun Mar 11 04:32:03 1990  Brian Fox  (bfox at gnuwest.fsf.org)

	* Signals are now supposedly handled inside of SYSV compilation.

Wed Jan 17 19:24:09 1990  Brian Fox  (bfox at sbphy.ucsb.edu)

	* history.c: history_expand (); fixed overwriting memory error,
	added needed argument to call to get_history_event ().

Thu Jan 11 10:54:04 1990  Brian Fox  (bfox at sbphy.ucsb.edu)

	* readline.c: added mark_modified_lines to control the
	display of an asterisk on modified history lines.  Also
	added a user variable called mark-modified-lines to the
	`set' command.

Thu Jan  4 10:38:05 1990  Brian Fox  (bfox at sbphy.ucsb.edu)

	* readline.c: start_insert ().  Only use IC if we don't have an im
	capability.

Fri Sep  8 09:00:45 1989  Brian Fox  (bfox at aurel)

	* readline.c: rl_prep_terminal ().  Only turn on 8th bit
	  as meta-bit iff the terminal is not using parity.

Sun Sep  3 08:57:40 1989  Brian Fox  (bfox at aurel)

	* readline.c: start_insert ().  Uses multiple
	  insertion call in cases where that makes sense.

	  rl_insert ().  Read type-ahead buffer for additional
	  keys that are bound to rl_insert, and insert them
	  all at once.  Make insertion of single keys given
	  with an argument much more efficient.

Tue Aug  8 18:13:57 1989  Brian Fox  (bfox at aurel)

	* readline.c: Changed handling of EOF.  readline () returns
	 (char *)EOF or consed string.  The EOF character is read from the
	 tty, or if the tty doesn't have one, defaults to C-d.

	* readline.c: Added support for event driven programs.
	  rl_event_hook is the address of a function you want called
	  while Readline is waiting for input.

	* readline.c: Cleanup time.  Functions without type declarations
	  do not use return with a value.

	* history.c: history_expand () has new variable which is the
	  characters to ignore immediately following history_expansion_char.

Sun Jul 16 08:14:00 1989  Brian Fox  (bfox at aurel)

	* rl_prep_terminal ()
	  BSD version turns off C-s, C-q, C-y, C-v.

	* readline.c -- rl_prep_terminal ()
	  SYSV version hacks readline_echoing_p.
	  BSD version turns on passing of the 8th bit for the duration
	  of reading the line.

Tue Jul 11 06:25:01 1989  Brian Fox  (bfox at aurel)

	* readline.c: new variable rl_tilde_expander.
	  If non-null, this contains the address of a function to call if
	  the standard meaning for expanding a tilde fails.  The function is
	  called with the text sans tilde (as in "foo"), and returns a
	  malloc()'ed string which is the expansion, or a NULL pointer if
	  there is no expansion. 

	* readline.h - new file chardefs.h
	  Separates things that only readline.c needs from the standard
	  header file publishing interesting things about readline.

	* readline.c:
	  readline_default_bindings () now looks at terminal chararacters
	  and binds those as well.

Wed Jun 28 20:20:51 1989  Brian Fox  (bfox at aurel)

	* Made readline and history into independent libraries.

