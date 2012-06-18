pythonbuilddir = bin/python

installpython::
	mkdir -p $(DESTDIR)$(pythondir)

# Install Python
# Arguments: Module path
define python_module_template

installpython:: $$(pythonbuilddir)/$(1) ;
	mkdir -p $$(DESTDIR)$$(pythondir)/$$(dir $(1))
	cp $$< $$(DESTDIR)$$(pythondir)/$(1)

uninstallpython:: 
	rm -f $$(DESTDIR)$$(pythondir)/$(1) ;

pythonmods:: $$(pythonbuilddir)/$(1) ;

endef

define python_py_module_template

$$(pythonbuilddir)/$(1): $(2) ;
	mkdir -p $$(@D)
	cp $$< $$@

$(call python_module_template,$(1))

endef

# Python C module
# Arguments: File name, dependencies, link list
define python_c_module_template

$$(pythonbuilddir)/$(1): $(2) ; 
	@echo Linking $$@
	@mkdir -p $$(@D)
	@$$(MDLD) $$(LDFLAGS) $$(MDLD_FLAGS) $$(INTERN_LDFLAGS) -o $$@ $$(INSTALL_LINK_FLAGS) $(3)

$(call python_module_template,$(1))
endef

pythonmods::

clean::
	@echo "Removing python modules"
	@rm -rf $(pythonbuilddir)

pydoctor:: pythonmods
	LD_LIBRARY_PATH=bin/shared PYTHONPATH=$(pythonbuilddir) pydoctor --project-name=Samba --project-url=http://www.samba.org --make-html --docformat=restructuredtext --add-package $(pythonbuilddir)/samba

bin/python/%.py: 
	mkdir -p $(@D)
	cp $< $@
