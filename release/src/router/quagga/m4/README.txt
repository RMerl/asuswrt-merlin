This directory contains local additions to/overrides for the Quagga
autoconf build system.

At this time additions are:

- m4 files taken from libtool CVS circa august 2004. These have been
imported into Quagga as they are more robust with respect to configuring
libtool support for languages which Quagga does not use. As and when libtool
releases become commonly available with that capability, these can be
removed. The files are:

	argz.m4
	libtool.m4
	ltdl.m4
	ltoptions.m4
	ltsugar.m4
	ltversion.m4

