The Samba Build System
======================

The build system basically has two main parts: the autoconf-generated 
shell scripts which check for availability of functions and libraries 
which is stored in the .m4 files and the information about the various 
subsystems which is stored in the .mk files.

Object Types
------------
the build system knows about the following object types

SUBSYSTEM:
	a SUBSYSTEM is basicly a collection of functions, which provide an
	an generic API for a specific problem (e.g. libldb provides an api
	for gneric ldb databases. libldb_plugin provides a generic api
	for calling ldb plugins, so 'libldb' and 'libldb_plugin' are subsystems)

MODULE:
	a MODULE is a specify implementation of a API provided by a SUBSYSTEM.
	(e.g. 'libldb_tdb' and 'libldb_ldap' are implementations of the subsystem 'libldb' API,
	 and 'libldb_plugin_timestamp' is a module of the 'libldb_plugin' subsystem)	

EXT_LIB:
	an EXT_LIB is an external library which is needed by a SUBSYSTEM, MODULE, BINARY or LIBRARY.
	(e.g. 'gtk' or 'KRB5')

BINARY:
	a BINARY means a executable binary file.
	(e.g. 'smbtorture' or 'ldbedit')
	a BINARY typically has only commandline handling and basic 
	functionality code in it and depends on the functions of
	SUBSYSTEM's (REQUIRED_SUBSYSTEMS).

LIBRARY:
	a LIBRARY means a static and/or shared library,
	which depends on the used OS.
	(e.g. for libldb 'libldb.so', 'libldb.so.0' 'libldb.so.0.0.1'
	      and libldb.a are created on linux)
	a LIBRARY typicly has only glue code in it and depends on
	SUBSYSTEM's (REQUIRED_SUBSYSTEMS).

File summary:
-------------
public.m4 - public M4 macros of the build system
config_mk.pm - Support for reading .mk files 
dot.pm - Support for generating .dot files for analysis of dependencies
input.pm - Input validation
main.pm - Main
makefile.pm - Makefile generation
output.pm - Dependency calculation

Layout
-------

Toplevel file: configure.in
- included by autogen.sh: aclocal.m4
  which includes the SMB_YXZ*() macros

- default tests of the build system
  are in build/smb_build/check_*.m4
  (mostly compiler and basic C type and function
   checks)

- subsystem specific stuff should be included by 'SMB_INCLUDE_M4()'


Generating the configure file
-------------------------
you need to rerun ./autogen.sh when 'configure.in' or any
'.m4' file was modified, then you need to rerun configure.


Generating config.status
-----------------------------
you need to run ./config.status (or 'configure') after a '.mk'
file was changed.


Examples
--------
for now please take a look at the .m4 and .mk files
you find in the source tree, they should be a good reference to start.
