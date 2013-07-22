#
# WARNING: this is not gtk-doc.make file from gtk-doc project. This
#          file has been modified to match with util-linux requirements:
#
#          * install files to $datadir
#          * don't maintain generated files in git repository
#          * don't distribute the final html files
#          * don't require --enable-gtk-doc for "make dist"
#          * support out-of-tree build ($srcdir != $builddir)
#
# -- kzak, Nov 2009
#

####################################
# Everything below here is generic #
####################################

if GTK_DOC_USE_LIBTOOL
GTKDOC_CC = $(LIBTOOL) --tag=CC --mode=compile $(CC) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(LIBTOOL) --tag=CC --mode=link $(CC) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS)
GTKDOC_RUN = $(LIBTOOL) --mode=execute
else
GTKDOC_CC = $(CC) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(CC) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS)
GTKDOC_RUN =
endif

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(docdir)/$(DOC_MODULE)

DISTCLEANFILES =

EXTRA_DIST = 				\
	$(content_files)		\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE)-sections.txt
#	$(DOC_MODULE)-overrides.txt

DOC_STAMPS=scan-build.stamp sgml-build.stamp html-build.stamp \
	   $(srcdir)/setup.stamp $(srcdir)/sgml.stamp \
	   $(srcdir)/html.stamp

SCANOBJ_FILES = 		 \
	$(DOC_MODULE).args 	 \
	$(DOC_MODULE).hierarchy  \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals    \
        $(DOC_MODULE).types            # util-linux: we don't use types

REPORT_FILES = \
	$(DOC_MODULE)-undocumented.txt \
	$(DOC_MODULE)-undeclared.txt \
	$(DOC_MODULE)-unused.txt

CLEANFILES = $(SCANOBJ_FILES) $(REPORT_FILES) $(DOC_STAMPS)

if ENABLE_GTK_DOC
all-local: html-build.stamp
else
all-local:
endif

docs: html-build.stamp

$(REPORT_FILES): sgml-build.stamp


#### setup ####

setup-build.stamp:
	-@if test "$(abs_srcdir)" != "$(abs_builddir)" ; then \
	   echo 'gtk-doc: Preparing build'; \
	   files=`echo $(EXTRA_DIST) $(expand_content_files) $(srcdir)/$(DOC_MODULE).types`; \
	   if test "x$$files" != "x" ; then \
	       for file in $$files ; do \
	           test -f $(abs_srcdir)/$$file && \
	               cp -p $(abs_srcdir)/$$file $(abs_builddir)/; \
	       done \
	   fi \
	fi
	@touch setup-build.stamp


setup.stamp: setup-build.stamp
	@true


#### scan ####

scan-build.stamp: $(HFILE_GLOB) $(CFILE_GLOB) $(srcdir)/$(DOC_MODULE)-*.txt $(content_files)

	@test -f $(DOC_MODULE)-sections.txt || \
		cp $(srcdir)/$(DOC_MODULE)-sections.txt $(builddir)

	$(AM_V_GEN)gtkdoc-scan --module=$(DOC_MODULE) \
	            --source-dir=$(srcdir)/$(DOC_SOURCE_DIR) \
	            --source-dir=$(builddir)/$(DOC_SOURCE_DIR) \
	            --ignore-headers="$(IGNORE_HFILES)" \
	            --output-dir=$(builddir) \
	            $(SCAN_OPTIONS) $(EXTRA_HFILES)

	@ if grep -l '^..*$$' $(srcdir)/$(DOC_MODULE).types > /dev/null 2>&1 ; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" RUN="$(GTKDOC_RUN)" \
	    CFLAGS="$(GTKDOC_CFLAGS) $(CFLAGS)" LDFLAGS="$(GTKDOC_LIBS) \
            $(LDFLAGS)" gtkdoc-scangobj $(SCANGOBJ_OPTIONS) \
			--module=$(DOC_MODULE) --output-dir=$(builddir) ; \
	else \
	    for i in $(SCANOBJ_FILES) ; do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	@ touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt: scan-build.stamp
	@true

#### templates ####
#
#tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(srcdir)/$(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
#	@echo 'gtk-doc: Rebuilding template files'
#	test -z $(builddir)/tmpl || $(MKDIR_P) $(builddir)/tmpl
#	gtkdoc-mktmpl --module=$(DOC_MODULE) \
#	              $(MKTMPL_OPTIONS)
#	touch tmpl-build.stamp
#
#tmpl.stamp: tmpl-build.stamp
#	@true
#
#tmpl/*.sgml:
#	@true
#

#### xml ####

sgml-build.stamp: setup.stamp $(HFILE_GLOB) $(CFILE_GLOB) $(DOC_MODULE)-decl.txt  $(DOC_MODULE)-sections.txt $(expand_content_files)
	$(AM_V_GEN)gtkdoc-mkdb --module=$(DOC_MODULE) \
	            --source-dir=$(srcdir)/$(DOC_SOURCE_DIR) \
	            --source-dir=$(builddir)/$(DOC_SOURCE_DIR) \
	            --output-format=xml \
	            --ignore-files="$(IGNORE_HFILES)" \
	            --expand-content-files="$(expand_content_files)" \
	            --main-sgml-file=$(srcdir)/$(DOC_MAIN_SGML_FILE) \
	            $(MKDB_OPTIONS)
	@touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(srcdir)/$(DOC_MAIN_SGML_FILE) $(content_files)
	@rm -rf $(builddir)/html
	@$(MKDIR_P) $(builddir)/html
	$(AM_V_GEN)cd $(builddir)/html && \
	  gtkdoc-mkhtml --path="$(abs_builddir):$(abs_builddir)/xml:$(abs_srcdir)" \
	                $(MKHTML_OPTIONS) \
	                $(DOC_MODULE) \
	                $(abs_srcdir)/$(DOC_MAIN_SGML_FILE)

	@test "x$(HTML_IMAGES)" = "x" || \
		( cd $(srcdir) && cp $(HTML_IMAGES) $(abs_builddir)/html )

	$(AM_V_GEN)gtkdoc-fixxref --module-dir=html \
	               --html-dir=$(HTML_DIR) \
	               $(FIXXREF_OPTIONS)
	@touch html-build.stamp

##############

clean-local:
	rm -f *~ *.bak
	rm -rf .libs

distclean-local:
	rm -rf xml html $(REPORT_FILES) *.stamp \
	       $(DOC_MODULE)-overrides.txt \
	       $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt
	test $(abs_builddir) ==  $(abs_srcdir) || \
	       rm -f $(DOC_MODULE)-*.txt $(DOC_MODULE)-*.xml *.xml.in

install-data-local:
	installfiles=`echo $(builddir)/html/*`; \
	if test "$$installfiles" = '$(builddir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  if test -n "$(DOC_MODULE_VERSION)"; then \
	    installdir="$(DESTDIR)$(TARGET_DIR)-$(DOC_MODULE_VERSION)"; \
	  else \
	    installdir="$(DESTDIR)$(TARGET_DIR)"; \
	  fi; \
	  $(mkinstalldirs) $${installdir} ; \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $${installdir}; \
	  done; \
	  if test -n "$(DOC_MODULE_VERSION)"; then \
	    mv -f $${installdir}/$(DOC_MODULE).devhelp2 \
	      $${installdir}/$(DOC_MODULE)-$(DOC_MODULE_VERSION).devhelp2; \
	    mv -f $${installdir}/$(DOC_MODULE).devhelp \
	      $${installdir}/$(DOC_MODULE)-$(DOC_MODULE_VERSION).devhelp; \
	  fi; \
	  ! which gtkdoc-rebase >/dev/null 2>&1 || \
	    gtkdoc-rebase --relative --dest-dir=$(DESTDIR) --html-dir=$${installdir} ; \
	fi

uninstall-local:
	if test -n "$(DOC_MODULE_VERSION)"; then \
	  installdir="$(DESTDIR)$(TARGET_DIR)-$(DOC_MODULE_VERSION)"; \
	else \
	  installdir="$(DESTDIR)$(TARGET_DIR)"; \
	fi; \
	rm -rf $${installdir}

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

#dist-hook: dist-check-gtkdoc dist-hook-local sgml.stamp html-build.stamp
#	mkdir $(distdir)/tmpl
#	mkdir $(distdir)/xml
#	mkdir $(distdir)/html
#	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
#	-cp $(srcdir)/xml/*.xml $(distdir)/xml
#	cp $(srcdir)/html/* $(distdir)/html
#	-cp $(srcdir)/$(DOC_MODULE).types $(distdir)/
#	-cp $(srcdir)/$(DOC_MODULE)-sections.txt $(distdir)/
#	cd $(distdir) && rm -f $(DISTCLEANFILES)
#	! which gtkdoc-rebase >/dev/null 2>&1 || \
#	  gtkdoc-rebase --online --relative --html-dir=$(distdir)/html
#
#.PHONY : dist-hook-local docs
