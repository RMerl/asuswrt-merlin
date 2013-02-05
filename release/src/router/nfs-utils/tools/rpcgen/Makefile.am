## Process this file with automake to produce Makefile.in

CC=$(CC_FOR_BUILD)
LIBTOOL = @LIBTOOL@ --tag=CC

noinst_PROGRAMS = rpcgen
rpcgen_SOURCES = rpc_clntout.c rpc_cout.c rpc_hout.c rpc_main.c \
	       	 rpc_parse.c rpc_scan.c rpc_svcout.c rpc_tblout.c \
		 rpc_util.c rpc_sample.c rpc_output.h rpc_parse.h \
		 rpc_scan.h rpc_util.h

rpcgen_CFLAGS=$(CFLAGS_FOR_BUILD)
rpcgen_CPPLAGS=$(CPPFLAGS_FOR_BUILD)
rpcgen_LDFLAGS=$(LDFLAGS_FOR_BUILD)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = rpcgen.new.1
