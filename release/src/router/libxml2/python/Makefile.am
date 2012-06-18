# Makefile for libxml2 python library
AUTOMAKE_OPTIONS = 1.4 foreign

SUBDIRS= . tests

INCLUDES = \
	-I$(PYTHON_INCLUDES) \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/include \
	-I$(top_builddir)/$(subdir)

docsdir = $(datadir)/doc/libxml2-python-$(LIBXML_VERSION)
# libxml2class.txt is generated
dist_docs_DATA = TODO

EXTRA_DIST = 			\
	setup.py		\
	setup.py.in		\
	generator.py		\
	libxml_wrap.h		\
	libxml.py		\
	libxml2-python-api.xml

libxml2mod_la_LDFLAGS = @CYGWIN_EXTRA_LDFLAGS@ @WIN32_EXTRA_LDFLAGS@ -module -avoid-version 

if WITH_PYTHON
mylibs = \
	$(top_builddir)/libxml2.la

python_LTLIBRARIES = libxml2mod.la

libxml2mod_la_SOURCES = $(srcdir)/libxml.c $(srcdir)/types.c
nodist_libxml2mod_la_SOURCES = libxml2-py.c
libxml2mod_la_LIBADD = $(mylibs) @CYGWIN_EXTRA_PYTHON_LIBADD@ @PYTHON_LIBS@

$(srcdir)/libxml.c: libxml2-py.h # to generate before to compile


libxml2.py: $(srcdir)/libxml.py libxml2class.py
	cat $(srcdir)/libxml.py libxml2class.py > $@

python_DATA = \
	libxml2.py

dist_python_DATA = \
	drv_libxml2.py
	
GENERATE = generator.py
API_DESC = $(top_srcdir)/doc/libxml2-api.xml $(srcdir)/libxml2-python-api.xml
GENERATED= libxml2class.py \
           libxml2-export.c \
           libxml2class.txt \
	   libxml2-py.c \
	   libxml2-py.h

CLEANFILES= $(GENERATED) gen_prog libxml2.py

$(GENERATED): gen_prog

gen_prog: $(srcdir)/$(GENERATE) $(API_DESC)
	$(PYTHON) $(srcdir)/$(GENERATE) $(srcdir)
	touch gen_prog
endif

tests test: all
	cd tests && $(MAKE) MAKEFLAGS+=--silent tests

