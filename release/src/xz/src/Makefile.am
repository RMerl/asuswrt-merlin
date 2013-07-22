##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

SUBDIRS = liblzma xzdec

if COND_XZ
SUBDIRS += xz
endif

if COND_LZMAINFO
SUBDIRS += lzmainfo
endif

if COND_SCRIPTS
SUBDIRS += scripts
endif

EXTRA_DIST = common
