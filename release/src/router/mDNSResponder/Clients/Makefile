# -*- tab-width: 4 -*-
#
# Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# $Log: Makefile,v $
# Revision 1.12  2008/09/05 17:37:08  cheshire
# Need to include ClientCommon.c when building dns-sd
#
# Revision 1.11  2007/05/29 19:57:33  cheshire
# Use "-Wall" for stricter compiler warnings
#
# Revision 1.10  2006/09/21 00:56:37  cheshire
# <rdar://problem/4245577> Need 64-bit version of dns-sd command-line tool
# Add in missing "build/" in targets line for builds other than OS X
#
# Revision 1.9  2006/09/18 18:55:39  cheshire
# <rdar://problem/4245577> Need 64-bit version of dns-sd command-line tool
#
# Revision 1.8  2006/08/14 23:23:55  cheshire
# Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0
#
# Revision 1.7  2006/01/06 01:06:17  cheshire
# <rdar://problem/3978979> Compile library and client programs in one pass
#
# Revision 1.6  2004/09/24 21:15:26  cheshire
# <rdar://problem/3724985> Library "libmdns" misnamed; should be "libdns_sd"
#
# Revision 1.5  2004/09/02 17:32:45  cheshire
# Look for headers in ../mDNSShared before we go to /usr/include
#
# Revision 1.4  2004/05/21 17:25:56  cheshire
# Fixes to make sample client work on Linux
#
# Revision 1.3  2004/03/12 08:05:32  cheshire
# Add a "make clean" target
#
# Revision 1.2  2004/02/11 20:59:26  cheshire
# Fix Makefile so it creates the "build" directory if necessary
#
# Revision 1.1  2004/02/06 03:19:09  cheshire
# Check in code to make command-line "dns-sd" testing tool
#
#
# Notes:
# $@ means "The file name of the target of the rule"
# $< means "The name of the first prerequisite"
# $+ means "The names of all the prerequisites, with spaces between them, exactly as given"
# For more magic automatic variables, see
# <http://www.gnu.org/software/make/manual/html_chapter/make_10.html#SEC111>

#############################################################################

# On OS X the dns_sd library functions are included in libSystem, which is implicitly linked with every executable
# If /usr/lib/libSystem.dylib exists, then we're on OS X, so we don't need also to link the "dns_sd" shared library
ifneq "$(wildcard /usr/lib/libSystem.dylib)" ""
TARGETS = build/dns-sd build/dns-sd64
LIBS =
else
TARGETS = build/dns-sd
LIBS = -L../mDNSPosix/build/prod/ -ldns_sd
endif

all: $(TARGETS)

clean:
	rm -rf build

build:
	mkdir build

build/dns-sd: build dns-sd.c ClientCommon.c
	cc $(filter %.c %.o, $+) $(LIBS) -I../mDNSShared -Wall -o $@

build/dns-sd64: build dns-sd.c ClientCommon.c
	cc $(filter %.c %.o, $+) $(LIBS) -I../mDNSShared -Wall -o $@ -m64

# Note, we can make a 'fat' version of dns-sd using 'lipo', as shown below, but we
# don't, because we don't want or need a 'fat' version of dns-sd, because it will
# never need to access more than 4GB of data. We build the 64-bit version purely so
# we have a test tool for making sure that the APIs work properly from 64-bit clients.
# lipo -create dns-sd dns-sd64 -output dns-sd-fat
