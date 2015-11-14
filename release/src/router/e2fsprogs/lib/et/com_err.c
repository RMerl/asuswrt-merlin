/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose is hereby granted, provided that
 * the names of M.I.T. and the M.I.T. S.I.P.B. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  M.I.T. and the
 * M.I.T. S.I.P.B. make no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "com_err.h"
#include "error_table.h"
#include "internal.h"

static void
default_com_err_proc (const char *whoami, errcode_t code, const
		      char *fmt, va_list args)
	COM_ERR_ATTR((format(printf, 3, 0)));

static void
default_com_err_proc (const char *whoami, errcode_t code, const
		      char *fmt, va_list args)
{
    int do_cr = 1, fd = fileno(stderr);

    if (whoami) {
	fputs(whoami, stderr);
	fputs(": ", stderr);
    }
    if (code) {
	fputs(error_message(code), stderr);
	fputs(" ", stderr);
    }
    if (fmt) {
        vfprintf (stderr, fmt, args);
    }
    if (!isatty(fd))
	do_cr = 0;
#ifdef HAVE_TERMIOS_H
    else {
	struct termios t;

	if ((tcgetattr(fd, &t)) == 0 &&
	    (t.c_oflag & OPOST) && (t.c_oflag & ONLCR))
	do_cr = 0;
    }
#endif
    if (do_cr)
	fputc('\r', stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

typedef void (*errf) (const char *, errcode_t, const char *, va_list);

errf com_err_hook = default_com_err_proc;

void com_err_va (const char *whoami, errcode_t code, const char *fmt,
		 va_list args)
{
    (*com_err_hook) (whoami, code, fmt, args);
}

void com_err (const char *whoami,
	      errcode_t code,
	      const char *fmt, ...)
{
    va_list pvar;

    if (!com_err_hook)
	com_err_hook = default_com_err_proc;
    va_start(pvar, fmt);
    com_err_va (whoami, code, fmt, pvar);
    va_end(pvar);
}

errf set_com_err_hook(errf new_proc)
{
    errf x = com_err_hook;

    if (new_proc)
	com_err_hook = new_proc;
    else
	com_err_hook = default_com_err_proc;

    return x;
}

errf reset_com_err_hook(void) {
    errf x = com_err_hook;
    com_err_hook = default_com_err_proc;
    return x;
}
