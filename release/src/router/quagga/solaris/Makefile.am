# Solaris packages automake file

# XXX This file uses GNU make extensions.

.PHONY: packages

# the names of the various subpackages, and some convenient 
# derived variables.
pkg_names = daemons dev doc libs smf
pkg_quagga_daemons = zebra bgpd ospfd ospf6d ripd ripngd
pkg_name_rev = @PACKAGE_VERSION@-@CONFDATE@-@target_os@-@target_cpu@
pkg_depends = $(pkg_names:%=depend.%)
pkg_packages = $(pkg_names:%=@PACKAGE_TARNAME@-%-$(pkg_name_rev).pkg)
pkg_pkginfos = $(pkg_names:%=pkginfo.%.full)
pkg_prototypes = $(pkg_names:%=prototype.%)
pkg_manifests = quagga.xml

# pkgmk variable substitutions wont grok ${variable} in prototype
# file, so we cant let autoconf generate the file sadly
# wish automake would just provide a template for this
edit = $(SED) \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@exec_prefix,$(exec_prefix),g' \
	-e 's,@bindir\@,$(bindir),g' \
	-e 's,@sbindir\@,$(sbindir),g' \
	-e 's,@libexecdir\@,$(libexecdir),g' \
	-e 's,@datadir\@,$(datadir),g' \
	-e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@sharedstatedir\@,$(sharedstatedir),g' \
	-e 's,@localstatedir\@,$(localstatedir),g' \
	-e 's,@libdir\@,$(libdir),g' \
	-e 's,@includedir\@,$(includedir),g' \
	-e 's,@infodir\@,$(infodir),g' \
	-e 's,@mandir\@,$(mandir),g' \
	-e 's,@enable_user\@,$(enable_user),g' \
	-e 's,@enable_group\@,$(enable_group),g' \
	-e 's,@enable_vty_group\@,$(enable_vty_group),g' \
	-e 's,@quagga_statedir\@,$(quagga_statedir),g' \
	-e 's,[@]PACKAGE_NAME[@],@PACKAGE_NAME@,g' \
 	-e 's,[@]PACKAGE_TARNAME[@],@PACKAGE_TARNAME@,g' \
 	-e 's,[@]PACKAGE_VERSION[@],@PACKAGE_VERSION@,g' \
 	-e 's,[@]PACKAGE_BUGREPORT[@],@PACKAGE_BUGREPORT@,g' \
  	-e 's,[@]CONFDATE[@],@CONFDATE@,g' \
 	-e 's,[@]target_cpu[@],$(target_cpu),g' \
 	-e 's,[@]target_host[@],$(target_host),g' \
 	-e 's,[@]target_os[@],$(target_os),g'

# common options for pkgmk
pkg_make_vars = exec_prefix=@exec_prefix@ prefix=@prefix@ \
	builddir=@builddir@ srcdir=@srcdir@ \
	top_builddir=@top_builddir@ top_srcdir=@top_srcdir@ \
	abs_builddir=@abs_builddir@ abs_srcdir=@abs_srcdir@ \
	abs_top_builddir=@abs_top_builddir@ abs_top_srcdir=@abs_top_srcdir@

# pkgmk: write the package to spool in build dir, to avoid root dependencies
pkg_make = pkgmk -o -d @abs_builddir@ \
	-f $< DESTDIR="$(DESTDIR)/" $(pkg_make_vars)

# pkgtrans: write a pkg file stream, shame we cant pipe directly to it from
# pkgmk..
pkg_trans = pkgtrans -s @abs_builddir@ "@abs_builddir@/$@"

# pkgmk can only cope with a single pkginfo, cant 'stack' various 
# pkginfo template files and a package specific pkginfo file in the prototype
# Create the package specific template here, and create the full pkginfo
# by cating this and the common pkginfo.tmpl together.
pkginfo.tmpl: $(srcdir)/pkginfo.tmpl.in Makefile
	rm -f $@
	$(edit) $< > $@

pkginfo.%.tmpl: $(srcdir)/pkginfo.%.tmpl.in Makefile
	rm -f $@
	$(edit) $< > $@

pkginfo.%.full: pkginfo.%.tmpl pkginfo.tmpl Makefile
	cat pkginfo.tmpl pkginfo.$*.tmpl > $@

# use 'edit' above to transform prototype.in to pkgmk acceptable prototype
prototype.%: $(srcdir)/prototype.%.in Makefile
	rm -f $@
	$(edit) $< > $@

# use edit to construct the SMF manifest files
%.xml: $(srcdir)/%.xml.in Makefile
	rm -f $@
	$(edit) $< > $@
# use edit to construct the depend files
depend.%: $(srcdir)/depend.%.in Makefile
	rm -f $@
	$(edit) $< > $@

# method file (bit like init script)
quagga.init: $(srcdir)/quagga.init.in Makefile
	rm -f $@
	$(edit) $< > $@

# construct the pkg
@PACKAGE_TARNAME@-%-$(pkg_name_rev).pkg: prototype.% \
		depend.% quagga.init pkginfo.%.full
	($(pkg_make) && \
	$(pkg_trans) "QUAGGA$*")

%.pkg.gz : %.pkg
	(gzip -c $< > $@)

# pkginfo.package and prototype.package are all built sources
#BUILT_SOURCES = pkginfo.daemons pkginfo.dev pkginfo.doc pkginfo.libs \
#	prototype.daemons prototype.dev prototype.doc prototype.libs
BUILT_SOURCES = $(pkg_pkginfos) pkginfo.tmpl $(pkg_prototypes) \
	$(pkg_manifests) $(pkg_depends) quagga.init

CLEANFILES = $(BUILT_SOURCES) $(pkg_packages)

EXTRA_DIST = $(pkg_manifests:%=%.in) $(pkg_prototypes:%=%.in) \
	$(pkg_names:%=pkginfo.%.tmpl.in) $(srcdir)/pkginfo.tmpl.in \
	$(pkg_depends:%=%.in) quagga.init.in README.txt

pkg-root-install:
	(cd $(top_builddir) && \
	 $(MAKE) DESTDIR=$(abs_builddir)/quagga-root install)

packages: $(pkg_packages)

#nodist_pkgdata_DATA = $(pkg_packages)
