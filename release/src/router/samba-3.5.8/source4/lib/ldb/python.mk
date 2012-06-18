[PYTHON::pyldb]
LIBRARY_REALNAME = ldb.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = LIBLDB PYTALLOC

pyldb_OBJ_FILES = $(ldbsrcdir)/pyldb.o 
$(pyldb_OBJ_FILES): CFLAGS+=-I$(ldbsrcdir)/include
