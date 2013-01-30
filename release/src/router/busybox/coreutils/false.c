/* vi: set sw=4 ts=4: */
/*
 * Mini false implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/000095399/utilities/false.html */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int false_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int false_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return EXIT_FAILURE;
}
