TEVENT_SOBASE = libtevent.$(SHLIBEXT)
TEVENT_SONAME = $(TEVENT_SOBASE).0
TEVENT_SOLIB = $(TEVENT_SOBASE).$(PACKAGE_VERSION)
TEVENT_STLIB = libtevent.a

$(TEVENT_STLIB): $(TEVENT_OBJ)
	ar -rv $(TEVENT_STLIB) $(TEVENT_OBJ)

$(TEVENT_SOBASE): $(TEVENT_SOLIB)
	ln -fs $< $@

$(TEVENT_SONAME): $(TEVENT_SOLIB)
	ln -fs $< $@

dirs::
	@mkdir -p lib

installdirs::
	mkdir -p $(DESTDIR)$(includedir)
	mkdir -p $(DESTDIR)$(libdir)
	mkdir -p $(DESTDIR)$(libdir)/pkgconfig

installheaders:: installdirs
	cp $(srcdir)/tevent.h $(DESTDIR)$(includedir)

installlibs:: installdirs
	cp tevent.pc $(DESTDIR)$(libdir)/pkgconfig
	cp $(TEVENT_STLIB) $(TEVENT_SOLIB) $(DESTDIR)$(libdir)
	rm -f $(DESTDIR)$(libdir)/$(TEVENT_SONAME)
	ln -s $(TEVENT_SOLIB) $(DESTDIR)$(libdir)/$(TEVENT_SONAME)
	rm -f $(DESTDIR)$(libdir)/$(TEVENT_SOBASE)
	ln -s $(TEVENT_SOLIB) $(DESTDIR)$(libdir)/$(TEVENT_SOBASE)

install:: all installdirs installheaders installlibs $(PYTHON_INSTALL_TARGET)

abi_checks::
	@echo ABI checks:
	@./script/abi_checks.sh tevent tevent.h

test:: abi_checks

clean::
	rm -f $(TEVENT_SOBASE) $(TEVENT_SONAME) $(TEVENT_SOLIB) $(TEVENT_STLIB)
	rm -f tevent.pc
	rm -f tevent.exports.sort tevent.exports.check tevent.exports.check.sort
	rm -f tevent.signatures.sort tevent.signatures.check tevent.signatures.check.sort
