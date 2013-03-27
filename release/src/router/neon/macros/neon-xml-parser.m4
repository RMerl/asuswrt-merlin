# Copyright (C) 1998-2005, 2007 Joe Orton <joe@manyfish.co.uk>    -*- autoconf -*-
#
# This file is free software; you may copy and/or distribute it with
# or without modifications, as long as this notice is preserved.
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.

# The above license applies to THIS FILE ONLY, the neon library code
# itself may be copied and distributed under the terms of the GNU
# LGPL, see COPYING.LIB for more details

# This file is part of the neon HTTP/WebDAV client library.
# See http://www.webdav.org/neon/ for the latest version. 
# Please send any feedback to <neon@lists.manyfish.co.uk>

# Check for XML parser, supporting libxml 2.x and expat 1.95.x,
# or a bundled copy of expat.
#  *  Bundled expat if a directory name argument is passed
#     -> expat dir must contain minimal expat sources, i.e.
#        xmltok, xmlparse sub-directories.  See sitecopy/cadaver for
#	 examples of how to do this.
#
# Usage: 
#  NEON_XML_PARSER()
# or
#  NEON_XML_PARSER([expat-srcdir], [expat-builddir])

dnl Find expat: run $1 if found, else $2
AC_DEFUN([NE_XML_EXPAT], [
AC_CHECK_HEADER(expat.h,
  [AC_CHECK_LIB(expat, XML_SetXmlDeclHandler, [
    AC_DEFINE(HAVE_EXPAT, 1, [Define if you have expat])
    neon_xml_parser_message="expat"
    NEON_LIBS="$NEON_LIBS -lexpat"
    neon_xml_parser=expat
    AC_CHECK_TYPE(XML_Size, 
      [NEON_FORMAT(XML_Size, [#include <expat.h>])],
      [AC_DEFINE_UNQUOTED([NE_FMT_XML_SIZE], ["d"])],
      [#include <expat.h>])
  ], [$1])], [$1])
])

dnl Find libxml2: run $1 if found, else $2
AC_DEFUN([NE_XML_LIBXML2], [
AC_CHECK_PROG(XML2_CONFIG, xml2-config, xml2-config)
if test -n "$XML2_CONFIG"; then
    neon_xml_parser_message="libxml `$XML2_CONFIG --version`"
    AC_DEFINE(HAVE_LIBXML, 1, [Define if you have libxml])
    # xml2-config in some versions erroneously includes -I/include
    # in the --cflags output.
    CPPFLAGS="$CPPFLAGS `$XML2_CONFIG --cflags | sed 's| -I/include||g'`"
    NEON_LIBS="$NEON_LIBS `$XML2_CONFIG --libs | sed 's|-L/usr/lib ||g'`"
    AC_CHECK_HEADERS(libxml/xmlversion.h libxml/parser.h,,[
      AC_MSG_ERROR([could not find parser.h, libxml installation problem?])])
    neon_xml_parser=libxml2
else
    $1
fi
])

dnl Configure for a bundled expat build.
AC_DEFUN([NE_XML_BUNDLED_EXPAT], [

AC_REQUIRE([AC_C_BIGENDIAN])
# Define XML_BYTE_ORDER for expat sources.
if test $ac_cv_c_bigendian = "yes"; then
  ne_xml_border=21
else
  ne_xml_border=12
fi

# mini-expat doesn't pick up config.h
CPPFLAGS="$CPPFLAGS -DXML_BYTE_ORDER=$ne_xml_border -DXML_DTD -I$1/xmlparse -I$1/xmltok"

AC_DEFINE_UNQUOTED([NE_FMT_XML_SIZE], ["d"])

# Use the bundled expat sources
AC_LIBOBJ($2/xmltok/xmltok)
AC_LIBOBJ($2/xmltok/xmlrole)
AC_LIBOBJ($2/xmlparse/xmlparse)
AC_LIBOBJ($2/xmlparse/hashtable)

AC_DEFINE(HAVE_EXPAT)

AC_DEFINE(HAVE_XMLPARSE_H, 1, [Define if using expat which includes xmlparse.h])

])

AC_DEFUN([NEON_XML_PARSER], [

dnl Switches to force choice of library
AC_ARG_WITH([libxml2],
AS_HELP_STRING([--with-libxml2], [force use of libxml 2.x]))
AC_ARG_WITH([expat], 
AS_HELP_STRING([--with-expat], [force use of expat]))

dnl Flag to force choice of included expat, if available.
ifelse($#, 2, [
AC_ARG_WITH([included-expat],
AS_HELP_STRING([--with-included-expat], [use bundled expat sources]),,
with_included_expat=no)],
with_included_expat=no)

if test "$NEON_NEED_XML_PARSER" = "yes"; then
  # Find an XML parser
  neon_xml_parser=none

  # Forced choice of expat:
  case $with_expat in
  yes) NE_XML_EXPAT([AC_MSG_ERROR([expat library not found, cannot proceed])]) ;;
  no) ;;
  */libexpat.la) 
       # Special case for Subversion
       ne_expdir=`echo $with_expat | sed 's:/libexpat.la$::'`
       AC_DEFINE(HAVE_EXPAT)
       AC_DEFINE_UNQUOTED([NE_FMT_XML_SIZE], ["d"])
       CPPFLAGS="$CPPFLAGS -I$ne_expdir"
       if test "x${NEON_TARGET}" = "xlibneon.la"; then
         NEON_LTLIBS=$with_expat
       else
         # no dependency on libexpat => crippled libneon, so do partial install
         ALLOW_INSTALL=lib
       fi
       neon_xml_parser=expat
       neon_xml_parser_message="expat in $ne_expdir"
       ;;
  /*) AC_MSG_ERROR([--with-expat does not take a directory argument]) ;;
  esac

  # If expat wasn't specifically enabled and libxml was:
  if test "${neon_xml_parser}-${with_libxml2}-${with_included_expat}" = "none-yes-no"; then
     NE_XML_LIBXML2(
      [AC_MSG_ERROR([libxml2.x library not found, cannot proceed])])
  fi

  # Otherwise, by default search for expat then libxml2:
  if test "${neon_xml_parser}-${with_included_expat}" = "none-no"; then
     NE_XML_EXPAT([NE_XML_LIBXML2([:])])
  fi

  # If an XML parser still has not been found, fail or use the bundled expat
  if test "$neon_xml_parser" = "none"; then
    m4_if($1, [], 
       [AC_MSG_ERROR([no XML parser was found: expat or libxml 2.x required])],
       [# Configure the bundled copy of expat
        NE_XML_BUNDLED_EXPAT($@)
	neon_xml_parser_message="bundled expat in $1"])
  fi

  AC_MSG_NOTICE([XML parser used: $neon_xml_parser_message])
fi

])
