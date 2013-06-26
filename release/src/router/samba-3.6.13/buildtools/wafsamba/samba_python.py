# waf build tool for building IDL files with pidl

import Build
from samba_utils import *
from samba_autoconf import *

from Configure import conf
@conf
def SAMBA_CHECK_PYTHON_HEADERS(conf, mandatory=True):
    if conf.env["python_headers_checked"] == []:
        conf.check_python_headers(mandatory)
        conf.env["python_headers_checked"] = "yes"
    else:
        conf.msg("python headers", "using cache")


def SAMBA_PYTHON(bld, name,
                 source='',
                 deps='',
                 public_deps='',
                 realname=None,
                 cflags='',
                 includes='',
                 init_function_sentinal=None,
                 local_include=True,
                 vars=None,
                 enabled=True):
    '''build a python extension for Samba'''

    # when we support static python modules we'll need to gather
    # the list from all the SAMBA_PYTHON() targets
    if init_function_sentinal is not None:
        cflags += '-DSTATIC_LIBPYTHON_MODULES=%s' % init_function_sentinal

    source = bld.EXPAND_VARIABLES(source, vars=vars)

    if realname is None:
        # a SAMBA_PYTHON target without a realname is just a
        # library with pyembed=True
        bld.SAMBA_LIBRARY(name,
                          source=source,
                          deps=deps,
                          public_deps=public_deps,
                          includes=includes,
                          cflags=cflags,
                          local_include=local_include,
                          vars=vars,
                          pyembed=True,
                          enabled=enabled)
        return

    link_name = 'python/%s' % realname

    bld.SAMBA_LIBRARY(name,
                      source=source,
                      deps=deps,
                      public_deps=public_deps,
                      includes=includes,
                      cflags=cflags,
                      realname=realname,
                      local_include=local_include,
                      vars=vars,
                      link_name=link_name,
                      pyembed=True,
                      target_type='PYTHON',
                      install_path='${PYTHONARCHDIR}',
                      enabled=enabled)

Build.BuildContext.SAMBA_PYTHON = SAMBA_PYTHON
