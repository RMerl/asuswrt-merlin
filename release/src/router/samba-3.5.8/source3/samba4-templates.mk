# Templates file for Samba 4
# This relies on GNU make.
#
# © 2008 Jelmer Vernooij <jelmer@samba.org>
#
###############################################################################
# Templates
###############################################################################

# Partially link
# Arguments: target object file, source object files
define partial_link_template 
$(1): $(2) ;
	@echo Partially linking $$@
	@mkdir -p $$(@D)
	@$$(PARTLINK) -o $$@ $$^
endef

# Link a binary
# Arguments: target file, depends, flags
define binary_link_template
$(1)4: $(2) ;
	@echo Linking $$@
	@$$(BNLD) $$(BNLD_FLAGS) $$(INTERN_LDFLAGS) -o $$@ $$(INSTALL_LINK_FLAGS) $(3)  $$(LIBS)
clean::
	@rm -f $(1)

everything:: $(1)4

endef

# Link a host-machine binary
# Arguments: target file, depends, flags
define host_binary_link_template
$(1)4: $(2) ;
	@echo Linking $$@
	@$$(HOSTLD) $$(HOSTLD_FLAGS) -L$${builddir}/bin/static -o $$@ $$(INSTALL_LINK_FLAGS) $(3)

clean::
	rm -f $(1)

binaries:: $(1)4


endef

# Create a prototype header
# Arguments: header file, c files
define proto_header_template
echo:: ;
	echo $(1) ;

proto:: $(1) ;

clean:: ;
	rm -f $(1) ;

$(1): $(2) ;
	@echo "Creating $$@"
	@$$(PERL) $$(srcdir)/../source4/script/mkproto.pl --srcdir=$$(srcdir)/../source4 --builddir=$$(builddir)/../source4 --public=/dev/null --private=$$@ $$^
endef

# Shared module
# Arguments: Target, dependencies, objects
define shared_module_template

$(1): $(2) ;
	@echo Linking $$@
	@mkdir -p $$(@D)
	@$$(MDLD) $$(LDFLAGS) $$(MDLD_FLAGS) $$(INTERN_LDFLAGS) -o $$@ $$(INSTALL_LINK_FLAGS) $(3)

PLUGINS += $(1)

endef

# Shared library
# Arguments: Target, dependencies, link flags, soname
define shared_library_template
$(1): $(2)
	@echo Linking $$@
	@mkdir -p $$(@D)
	@$$(SHLD) $$(LDFLAGS) $$(SHLD_FLAGS) $$(INTERN_LDFLAGS) -o $$@ $$(INSTALL_LINK_FLAGS) \
		$(3) \
		$$(if $$(SONAMEFLAG), $$(SONAMEFLAG)$(notdir $(4))) $$(LIBS)

ifneq ($(notdir $(1)),$(notdir $(4)))
$(4): $(1)
	@echo "Creating symbolic link for $$@"
	@ln -fs $$(<F) $$@
endif

ifneq ($(notdir $(1)),$(notdir $(5)))
$(5): $(1) $(4)
	@echo "Creating symbolic link for $$@"
	@ln -fs $$(<F) $$@
endif
endef

# Shared alias
# Arguments: Target, subsystem name, alias name
define shared_module_alias_template
bin/modules/$(2)/$(3).$$(SHLIBEXT): $(1)
	@ln -fs $$(<F) $$@

PLUGINS += bin/modules/$(2)/$(3).$$(SHLIBEXT)

uninstallplugins::
	@-rm $$(DESTDIR)$$(modulesdir)/$(2)/$(3).$$(SHLIBEXT)
installplugins::
	@ln -fs $(1) $$(DESTDIR)$$(modulesdir)/$(2)/$(3).$$(SHLIBEXT)

endef

define shared_module_install_template
installplugins:: bin/modules/$(1)/$(2)
	@echo Installing $(2) as $$(DESTDIR)$$(modulesdir)/$(1)/$(2)
	@mkdir -p $$(DESTDIR)$$(modulesdir)/$(1)/
	@cp bin/modules/$(1)/$(2) $$(DESTDIR)$$(modulesdir)/$(1)/$(2)
uninstallplugins::
	@echo Uninstalling $$(DESTDIR)$$(modulesdir)/$(1)/$(2)
	@-rm $$(DESTDIR)$$(modulesdir)/$(1)/$(2)

endef

# abspath for older makes
abspath = $(shell cd $(dir $(1)); pwd)/$(notdir $(1))

# Install a binary
# Arguments: path to binary to install
define binary_install_template
installbin:: $$(DESTDIR)$$(bindir)/$(notdir $(1))4

uninstallbin::
	@echo "Removing $(notdir $(1))4"
	@rm -f $$(DESTDIR)$$(bindir)/$(1)4
endef

define sbinary_install_template
installsbin:: $$(DESTDIR)$$(sbindir)/$(notdir $(1))4 installdirs
				
uninstallsbin::
	@echo "Removing $(notdir $(1))4"
	@rm -f $$(DESTDIR)$$(sbindir)/$(1)4
endef
