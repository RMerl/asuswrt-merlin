LDB_LIB = -Llib -lldb

LIB_FLAGS=$(LDFLAGS) $(LIBS) $(LDB_LIB) $(POPT_LIBS) $(TALLOC_LIBS) \
		  $(TDB_LIBS) $(TEVENT_LIBS) $(LDAP_LIBS) $(LIBDL)

LDB_TDB_DIR=ldb_tdb
LDB_TDB_OBJ=$(LDB_TDB_DIR)/ldb_tdb.o \
	$(LDB_TDB_DIR)/ldb_pack.o $(LDB_TDB_DIR)/ldb_search.o $(LDB_TDB_DIR)/ldb_index.o \
	$(LDB_TDB_DIR)/ldb_cache.o $(LDB_TDB_DIR)/ldb_tdb_wrap.o

LDB_MAP_DIR=ldb_map
LDB_MAP_OBJ=$(LDB_MAP_DIR)/ldb_map.o $(LDB_MAP_DIR)/ldb_map_inbound.o \
	    $(LDB_MAP_DIR)/ldb_map_outbound.o

COMDIR=common
COMMON_OBJ=$(COMDIR)/ldb.o $(COMDIR)/ldb_ldif.o \
	   $(COMDIR)/ldb_parse.o $(COMDIR)/ldb_msg.o $(COMDIR)/ldb_utf8.o \
	   $(COMDIR)/ldb_debug.o $(COMDIR)/ldb_modules.o \
	   $(COMDIR)/ldb_dn.o $(COMDIR)/ldb_match.o $(COMDIR)/ldb_attributes.o \
	   $(COMDIR)/attrib_handlers.o $(COMDIR)/ldb_controls.o $(COMDIR)/qsort.o

MODDIR=modules
MODULES_OBJ=$(MODDIR)/rdn_name.o ${MODDIR}/asq.o \
	   $(MODDIR)/paged_results.o $(MODDIR)/sort.o

NSSDIR=nssldb
NSS_OBJ= $(NSSDIR)/ldb-nss.o $(NSSDIR)/ldb-pwd.o $(NSSDIR)/ldb-grp.o
NSS_LIB = lib/libnss_ldb.$(SHLIBEXT).2

lib/libldb.a: $(OBJS)
	ar -rv $@ $(OBJS)
	@-ranlib $@

sample.$(SHLIBEXT): tests/sample_module.o
	$(MDLD) $(MDLD_FLAGS) -o $@ tests/sample_module.o

bin/ldbadd: tools/ldbadd.o tools/cmdline.o
	$(CC) -o bin/ldbadd tools/ldbadd.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbsearch: tools/ldbsearch.o tools/cmdline.o
	$(CC) -o bin/ldbsearch tools/ldbsearch.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbdel: tools/ldbdel.o tools/cmdline.o
	$(CC) -o bin/ldbdel tools/ldbdel.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbmodify: tools/ldbmodify.o tools/cmdline.o
	$(CC) -o bin/ldbmodify tools/ldbmodify.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbedit: tools/ldbedit.o tools/cmdline.o
	$(CC) -o bin/ldbedit tools/ldbedit.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbrename: tools/ldbrename.o tools/cmdline.o
	$(CC) -o bin/ldbrename tools/ldbrename.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

bin/ldbtest: tools/ldbtest.o tools/cmdline.o
	$(CC) -o bin/ldbtest tools/ldbtest.o tools/cmdline.o $(LIB_FLAGS) $(LD_EXPORT_DYNAMIC)

examples/ldbreader: examples/ldbreader.o
	$(CC) -o examples/ldbreader examples/ldbreader.o $(LIB_FLAGS)

examples/ldifreader: examples/ldifreader.o
	$(CC) -o examples/ldifreader examples/ldifreader.o $(LIB_FLAGS)

# Python bindings
build-python:: ldb.$(SHLIBEXT)

pyldb.o: $(ldbdir)/pyldb.c
	$(CC) $(PICFLAG) -c $(ldbdir)/pyldb.c $(CFLAGS) `$(PYTHON_CONFIG) --cflags`

ldb.$(SHLIBEXT): pyldb.o 
	$(SHLD) $(SHLD_FLAGS) -o ldb.$(SHLIBEXT) pyldb.o $(LIB_FLAGS) `$(PYTHON_CONFIG) --ldflags`

install-python:: build-python
	mkdir -p $(DESTDIR)`$(PYTHON) -c "import distutils.sysconfig; print distutils.sysconfig.get_python_lib(1, prefix='$(prefix)')"`
	cp ldb.$(SHLIBEXT) $(DESTDIR)`$(PYTHON) -c "import distutils.sysconfig; print distutils.sysconfig.get_python_lib(1, prefix='$(prefix)')"`

check-python:: build-python lib/$(SONAME)
	$(LIB_PATH_VAR)=lib PYTHONPATH=.:$(ldbdir) $(PYTHON) $(ldbdir)/tests/python/api.py

clean::
	rm -f ldb.$(SHLIBEXT)
