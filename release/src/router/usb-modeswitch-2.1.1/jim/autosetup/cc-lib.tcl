# Copyright (c) 2011 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# Provides a library of common tests on top of the 'cc' module.

use cc

module-options {}

# @cc-check-lfs
#
# The equivalent of the AC_SYS_LARGEFILE macro
# 
# defines 'HAVE_LFS' if LFS is available,
# and defines '_FILE_OFFSET_BITS=64' if necessary
#
# Returns 1 if 'LFS' is available or 0 otherwise
#
proc cc-check-lfs {} {
	cc-check-includes sys/types.h
	msg-checking "Checking if -D_FILE_OFFSET_BITS=64 is needed..."
	set lfs 1
	if {[msg-quiet cc-with {-includes sys/types.h} {cc-check-sizeof off_t}] == 8} {
		msg-result no
	} elseif {[msg-quiet cc-with {-includes sys/types.h -cflags -D_FILE_OFFSET_BITS=64} {cc-check-sizeof off_t}] == 8} {
		define _FILE_OFFSET_BITS 64
		msg-result yes
	} else {
		set lfs 0
		msg-result none
	}
	define-feature lfs $lfs
	return $lfs
}

# @cc-check-endian
#
# The equivalent of the AC_C_BIGENDIAN macro
# 
# defines 'HAVE_BIG_ENDIAN' if endian is known to be big,
# or 'HAVE_LITTLE_ENDIAN' if endian is known to be little.
#
# Returns 1 if determined, or 0 if not.
#
proc cc-check-endian {} {
	cc-check-includes sys/types.h sys/param.h
	set rc 0
	msg-checking "Checking endian..."
	cc-with {-includes {sys/types.h sys/param.h}} {
		if {[cctest -code {
			#if !defined(BIG_ENDIAN) || !defined(BYTE_ORDER)
				#error unknown
			#elif BYTE_ORDER != BIG_ENDIAN
				#error little
			#endif
		}]} {
			define-feature big-endian
			msg-result "big"
			set rc 1
		} elseif {[cctest -code {
			#if !defined(LITTLE_ENDIAN) || !defined(BYTE_ORDER)
				#error unknown
			#elif BYTE_ORDER != LITTLE_ENDIAN
				#error big
			#endif
		}]} {
			define-feature little-endian
			msg-result "little"
			set rc 1
		} else {
			msg-result "unknown"
		}
	}
	return $rc
}
