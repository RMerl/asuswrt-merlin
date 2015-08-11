# Copyright (c) 2010 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# The 'cc-shared' module provides support for shared libraries and shared objects.
# It defines the following variables:
#
## SH_CFLAGS         Flags to use compiling sources destined for a shared library
## SH_LDFLAGS        Flags to use linking a shared library
## SHOBJ_CFLAGS      Flags to use compiling sources destined for a shared object
## SHOBJ_LDFLAGS     Flags to use linking a shared object
## SH_LINKFLAGS      Flags to use linking an executable which will load shared objects
## LD_LIBRARY_PATH   Environment variable which specifies path to shared libraries

module-options {}

foreach i {SH_LINKFLAGS SH_CFLAGS SH_LDFLAGS SHOBJ_CFLAGS SHOBJ_LDFLAGS} {
	define $i ""
}

define LD_LIBRARY_PATH LD_LIBRARY_PATH

switch -glob -- [get-define host] {
	*-*-darwin* {
		define SH_CFLAGS -dynamic
		define SH_LDFLAGS "-dynamiclib"
		define SHOBJ_CFLAGS "-dynamic -fno-common"
		define SHOBJ_LDFLAGS "-bundle -undefined dynamic_lookup"
		define LD_LIBRARY_PATH DYLD_LIBRARY_PATH
	}
	*-*-ming* {
		define SH_LDFLAGS -shared
		define SHOBJ_LDFLAGS -shared
	}
	*-*-cygwin {
		define SH_LDFLAGS -shared
		define SHOBJ_LDFLAGS -shared
	}
	*-*-solaris* {
		# XXX: These haven't been fully tested. 
		#define SH_LINKFLAGS -Wl,-export-dynamic
		define SH_CFLAGS -Kpic
		define SHOBJ_CFLAGS -Kpic
		define SHOBJ_LDFLAGS "-G"
	}
	*-*-hpux {
		# XXX: These haven't been tested
		define SH_LINKFLAGS -Wl,+s
		define SH_CFLAGS +z
		define SHOBJ_CFLAGS "+O3 +z"
		define SHOBJ_LDFLAGS -b
		define LD_LIBRARY_PATH SHLIB_PATH
	}
	* {
		# Generic Unix settings
		define SH_LINKFLAGS -rdynamic
		define SH_CFLAGS -fpic
		define SH_LDFLAGS -shared
		define SHOBJ_CFLAGS -fpic
		define SHOBJ_LDFLAGS "-shared"
	}
}
