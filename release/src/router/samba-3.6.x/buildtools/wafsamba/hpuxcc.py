# compiler definition for HPUX
# based on suncc.py from waf

import os, optparse, sys
import Utils, Options, Configure
import ccroot, ar
from Configure import conftest
import gcc


@conftest
def gcc_modifier_hpux(conf):
    v=conf.env
    v['CCFLAGS_DEBUG']=['-g']
    v['CCFLAGS_RELEASE']=['-O2']
    v['CC_SRC_F']=''
    v['CC_TGT_F']=['-c','-o','']
    v['CPPPATH_ST']='-I%s'
    if not v['LINK_CC']:v['LINK_CC']=v['CC']
    v['CCLNK_SRC_F']=''
    v['CCLNK_TGT_F']=['-o','']
    v['LIB_ST']='-l%s'
    v['LIBPATH_ST']='-L%s'
    v['STATICLIB_ST']='-l%s'
    v['STATICLIBPATH_ST']='-L%s'
    v['RPATH_ST']='-Wl,-rpath,%s'
    v['CCDEFINES_ST']='-D%s'
    v['SONAME_ST']='-Wl,-h,%s'
    v['SHLIB_MARKER']=[]
#    v['STATICLIB_MARKER']='-Wl,-Bstatic'
    v['FULLSTATIC_MARKER']='-static'
    v['program_PATTERN']='%s'
    v['shlib_CCFLAGS']=['-fPIC','-DPIC']
    v['shlib_LINKFLAGS']=['-shared']
    v['shlib_PATTERN']='lib%s.sl'
#   v['staticlib_LINKFLAGS']=['-Wl,-Bstatic']
    v['staticlib_PATTERN']='lib%s.a'

gcc.gcc_modifier_hpux = gcc_modifier_hpux

from TaskGen import feature, after
@feature('cprogram', 'cshlib')
@after('apply_link', 'apply_lib_vars', 'apply_obj_vars')
def hpux_addfullpath(self):
  if sys.platform == 'hp-ux11':
    link = getattr(self, 'link_task', None)
    if link:
        lst = link.env.LINKFLAGS
        buf = []
        for x in lst:
           if x.startswith('-L'):
               p2 = x[2:]
               if not os.path.isabs(p2):
                   x = x[:2] + self.bld.srcnode.abspath(link.env) + "/../" + x[2:].lstrip('.')
           buf.append(x)
        link.env.LINKFLAGS = buf
