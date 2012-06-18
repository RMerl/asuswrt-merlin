# $Id: Makefile.am,v 1.3 2006/02/28 21:21:22 r3mi Exp $
#
# "Makefile.am" for "libupnp/ixml"
#
# (C) Copyright 2005 Rémi Turboult <r3mi@users.sourceforge.net>
#

SUBDIRS 		= doc

AM_CPPFLAGS		= -I$(srcdir)/inc -I$(srcdir)/src/inc
AM_CFLAGS		= 

if ENABLE_DEBUG
    AM_CPPFLAGS		+= -DDEBUG
else
    AM_CPPFLAGS		+= -DNDEBUG
endif

lib_LTLIBRARIES		= libixml.la

libixml_la_LDFLAGS	= -version-info $(LT_VERSION_IXML) \
			  -export-symbols-regex '^ixml.*'

libixml_la_SOURCES	= \
			src/ixml.c src/node.c src/ixmlparser.c \
			src/ixmlmembuf.c src/nodeList.c \
			src/element.c src/attr.c src/document.c \
			src/namedNodeMap.c \
			src/inc/ixmlmembuf.h src/inc/ixmlparser.h

upnpincludedir		= $(includedir)/upnp
upnpinclude_HEADERS	= inc/ixml.h 



clean-local:
	@if [ -d bin ] ; then  rm -rf bin ; fi


