all:: $(LIBRARY)_p.a

real-subdirs:: Makefile
	$(E) "	MKDIR profiled"
	$(Q) mkdir -p profiled

clean::
	$(RM) -rf profiled
	$(RM) -f $(LIBRARY)_p.a ../$(LIBRARY)_p.a

$(LIBRARY)_p.a: $(OBJS)
	$(E) "	GEN_PROFILED_LIB $(ELF_LIB)"
	$(Q) (if test -r $@; then $(RM) -f $@.bak && $(MV) $@ $@.bak; fi)
	$(Q) (cd profiled; $(ARUPD) ../$@ $(OBJS))
	-$(Q) $(RANLIB) $@
	$(Q) $(RM) -f ../$@
	$(Q) (cd ..; $(LN) $(LINK_BUILD_FLAGS) \
		`echo $(my_dir) | sed -e 's;lib/;;'`/$@ $@)

install:: $(LIBRARY)_p.a installdirs
	$(E) "	INSTALL_DATA $(libdir)/$(LIBRARY)_p.a"
	$(Q) $(INSTALL_DATA) $(LIBRARY)_p.a $(DESTDIR)$(libdir)/$(LIBRARY)_p.a
	-$(Q) $(RANLIB) $(DESTDIR)$(libdir)/$(LIBRARY)_p.a
	$(Q) $(CHMOD) $(LIBMODE) $(DESTDIR)$(libdir)/$(LIBRARY)_p.a

uninstall::
	$(RM) -f $(DESTDIR)$(libdir)/$(LIBRARY)_p.a
