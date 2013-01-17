How to Add a New Applet to BusyBox
==================================

This document details the steps you must take to add a new applet to BusyBox.

Credits:
Matt Kraai - initial writeup
Mark Whitley - the remix
Thomas Lundquist - Trying to keep it updated.

When doing this you should consider using the latest git HEAD.
This is a good thing if you plan to getting it committed into mainline.

Initial Write
-------------

First, write your applet.  Be sure to include copyright information at the top,
such as who you stole the code from and so forth. Also include the mini-GPL
boilerplate. Be sure to name the main function <applet>_main instead of main.
And be sure to put it in <applet>.c. Usage does not have to be taken care of by
your applet.
Make sure to #include "libbb.h" as the first include file in your applet.

For a new applet mu, here is the code that would go in mu.c:

(busybox.h already includes most usual header files. You do not need
#include <stdio.h> etc...)


----begin example code------

/* vi: set sw=4 ts=4: */
/*
 * Mini mu implementation for busybox
 *
 * Copyright (C) [YEAR] by [YOUR NAME] <YOUR EMAIL>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "other.h"

int mu_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mu_main(int argc, char **argv)
{
	int fd;
	ssize_t n;
	char mu;

	fd = xopen("/dev/random", O_RDONLY);

	if ((n = safe_read(fd, &mu, 1)) < 1)
		bb_perror_msg_and_die("/dev/random");

	return mu;
}

----end example code------


Coding Style
------------

Before you submit your applet for inclusion in BusyBox, (or better yet, before
you _write_ your applet) please read through the style guide in the docs
directory and make your program compliant.


Some Words on libbb
-------------------

As you are writing your applet, please be aware of the body of pre-existing
useful functions in libbb. Use these instead of reinventing the wheel.

Additionally, if you have any useful, general-purpose functions in your
applet that could be useful in other applets, consider putting them in libbb.

And it may be possible that some of the other applets uses functions you
could use. If so, you have to rip the function out of the applet and make
a libbb function out of it.

Adding a libbb function:
------------------------

Make a new file named <function_name>.c

----start example code------

#include "libbb.h"
#include "other.h"

int function(char *a)
{
	return *a;
}

----end example code------

Add <function_name>.o in the right alphabetically sorted place
in libbb/Kbuild.src. You should look at the conditional part of
libbb/Kbuild.src as well.

You should also try to find a suitable place in include/libbb.h for
the function declaration. If not, add it somewhere anyway, with or without
ifdefs to include or not.

You can look at libbb/Config.src and try to find out if the function is
tunable and add it there if it is.


Placement / Directory
---------------------

Find the appropriate directory for your new applet.

Make sure you find the appropriate places in the files, the applets are
sorted alphabetically.

Add the applet to Kbuild.src in the chosen directory:

lib-$(CONFIG_MU)               += mu.o

Add the applet to Config.src in the chosen directory:

config MU
	bool "MU"
	default n
	help
	  Returns an indeterminate value.


Usage String(s)
---------------

Next, add usage information for you applet to include/usage.src.h.
This should look like the following:

	#define mu_trivial_usage \
		"-[abcde] FILES"
	#define mu_full_usage \
		"Returns an indeterminate value.\n\n" \
		"Options:\n" \
		"\t-a\t\tfirst function\n" \
		"\t-b\t\tsecond function\n" \
		...

If your program supports flags, the flags should be mentioned on the first
line (-[abcde]) and a detailed description of each flag should go in the
mu_full_usage section, one flag per line. (Numerous examples of this
currently exist in usage.src.h.)


Header Files
------------

Next, add an entry to include/applets.src.h.  Be *sure* to keep the list
in alphabetical order, or else it will break the binary-search lookup
algorithm in busybox.c and the Gods of BusyBox smite you. Yea, verily:

Be sure to read the top of applets.src.h before adding your applet.

	/* all programs above here are alphabetically "less than" 'mu' */
	IF_MU(APPLET(mu, BB_DIR_USR_BIN, BB_SUID_DROP))
	/* all programs below here are alphabetically "greater than" 'mu' */


The Grand Announcement
----------------------

Then create a diff by adding the new files to git (remember your libbb files)
	git add <where you put it>/mu.c
eventually also:
	git add libbb/function.c
then
	git commit
	git format-patch HEAD^
and send it to the mailing list:
	busybox@busybox.net
	http://busybox.net/mailman/listinfo/busybox

Sending patches as attachments is preferred, but not required.
