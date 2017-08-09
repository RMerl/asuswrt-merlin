# a waf tool to add autoconf-like macros to the configure section
# and for SAMBA_ macros for building libraries, binaries etc

import Options, Build, os
from optparse import SUPPRESS_HELP
from samba_utils import os_path_relpath, TO_LIST

def SAMBA3_ADD_OPTION(opt, option, help=(), dest=None, default=True,
                      with_name="with", without_name="without"):
    if help == ():
        help = ("Build with %s support" % option)
    if dest is None:
        dest = "with_%s" % option.replace('-', '_')

    with_val = "--%s-%s" % (with_name, option)
    without_val = "--%s-%s" % (without_name, option)

    #FIXME: This is broken and will always default to "default" no matter if
    # --with or --without is chosen.
    opt.add_option(with_val, help=help, action="store_true", dest=dest,
                   default=default)
    opt.add_option(without_val, help=SUPPRESS_HELP, action="store_false",
                   dest=dest)
Options.Handler.SAMBA3_ADD_OPTION = SAMBA3_ADD_OPTION

def SAMBA3_IS_STATIC_MODULE(bld, module):
    '''Check whether module is in static list'''
    if module in bld.env['static_modules']:
        return True
    return False
Build.BuildContext.SAMBA3_IS_STATIC_MODULE = SAMBA3_IS_STATIC_MODULE

def SAMBA3_IS_SHARED_MODULE(bld, module):
    '''Check whether module is in shared list'''
    if module in bld.env['shared_modules']:
        return True
    return False
Build.BuildContext.SAMBA3_IS_SHARED_MODULE = SAMBA3_IS_SHARED_MODULE

def SAMBA3_IS_ENABLED_MODULE(bld, module):
    '''Check whether module is in either shared or static list '''
    return SAMBA3_IS_STATIC_MODULE(bld, module) or SAMBA3_IS_SHARED_MODULE(bld, module)
Build.BuildContext.SAMBA3_IS_ENABLED_MODULE = SAMBA3_IS_ENABLED_MODULE



def s3_fix_kwargs(bld, kwargs):
    '''fix the build arguments for s3 build rules to include the
	necessary includes, subdir and cflags options '''
    s3dir = os.path.join(bld.env.srcdir, 'source3')
    s3reldir = os_path_relpath(s3dir, bld.curdir)

    # the extra_includes list is relative to the source3 directory
    extra_includes = [ '.', 'include', 'lib' ]
    if bld.env.use_intree_heimdal:
        extra_includes += [ '../source4/heimdal/lib/com_err',
                            '../source4/heimdal/lib/gssapi',
                            '../source4/heimdal_build' ]

    if not bld.CONFIG_SET('USING_SYSTEM_TDB'):
        extra_includes += [ '../lib/tdb/include' ]

    if not bld.CONFIG_SET('USING_SYSTEM_TEVENT'):
        extra_includes += [ '../lib/tevent' ]

    if not bld.CONFIG_SET('USING_SYSTEM_TALLOC'):
        extra_includes += [ '../lib/talloc' ]

    if not bld.CONFIG_SET('USING_SYSTEM_POPT'):
        extra_includes += [ '../lib/popt' ]

    # s3 builds assume that they will have a bunch of extra include paths
    includes = []
    for d in extra_includes:
        includes += [ os.path.join(s3reldir, d) ]

    # the rule may already have some includes listed
    if 'includes' in kwargs:
        includes += TO_LIST(kwargs['includes'])
    kwargs['includes'] = includes

    # some S3 code assumes that CONFIGFILE is set
    cflags = ['-DCONFIGFILE="%s"' % bld.env['CONFIGFILE']]
    if 'cflags' in kwargs:
        cflags += TO_LIST(kwargs['cflags'])
    kwargs['cflags'] = cflags

# these wrappers allow for mixing of S3 and S4 build rules in the one build

def SAMBA3_LIBRARY(bld, name, *args, **kwargs):
	s3_fix_kwargs(bld, kwargs)
	kwargs['allow_undefined_symbols'] = True
	return bld.SAMBA_LIBRARY(name, *args, **kwargs)
Build.BuildContext.SAMBA3_LIBRARY = SAMBA3_LIBRARY

def SAMBA3_MODULE(bld, name, *args, **kwargs):
	s3_fix_kwargs(bld, kwargs)
	kwargs['allow_undefined_symbols'] = True
	return bld.SAMBA_MODULE(name, *args, **kwargs)
Build.BuildContext.SAMBA3_MODULE = SAMBA3_MODULE

def SAMBA3_SUBSYSTEM(bld, name, *args, **kwargs):
	s3_fix_kwargs(bld, kwargs)
	return bld.SAMBA_SUBSYSTEM(name, *args, **kwargs)
Build.BuildContext.SAMBA3_SUBSYSTEM = SAMBA3_SUBSYSTEM

def SAMBA3_BINARY(bld, name, *args, **kwargs):
	s3_fix_kwargs(bld, kwargs)
	return bld.SAMBA_BINARY(name, *args, **kwargs)
Build.BuildContext.SAMBA3_BINARY = SAMBA3_BINARY
