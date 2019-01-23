
# compiler definition for a generic C compiler
# based on suncc.py from waf

import os, optparse
import Utils, Options, Configure
import ccroot, ar
from Configure import conftest

from compiler_cc import c_compiler

c_compiler['default'] = ['gcc', 'generic_cc']
c_compiler['hpux'] = ['gcc', 'generic_cc']

@conftest
def find_generic_cc(conf):
	v = conf.env
	cc = None
	if v['CC']: cc = v['CC']
	elif 'CC' in conf.environ: cc = conf.environ['CC']
	if not cc: cc = conf.find_program('cc', var='CC')
	if not cc: conf.fatal('generic_cc was not found')
	cc = conf.cmd_to_list(cc)
	v['CC']  = cc
	v['CC_NAME'] = 'generic'

@conftest
def generic_cc_common_flags(conf):
	v = conf.env

	v['CC_SRC_F']            = ''
	v['CC_TGT_F']            = ['-c', '-o', '']
	v['CPPPATH_ST']          = '-I%s' # template for adding include paths

	# linker
	if not v['LINK_CC']: v['LINK_CC'] = v['CC']
	v['CCLNK_SRC_F']         = ''
	v['CCLNK_TGT_F']         = ['-o', '']

	v['LIB_ST']              = '-l%s' # template for adding libs
	v['LIBPATH_ST']          = '-L%s' # template for adding libpaths
	v['STATICLIB_ST']        = '-l%s'
	v['STATICLIBPATH_ST']    = '-L%s'
	v['CCDEFINES_ST']        = '-D%s'

#	v['SONAME_ST']           = '-Wl,-h -Wl,%s'
#	v['SHLIB_MARKER']        = '-Bdynamic'
#	v['STATICLIB_MARKER']    = '-Bstatic'

	# program
	v['program_PATTERN']     = '%s'

	# shared library
#	v['shlib_CCFLAGS']       = ['-Kpic', '-DPIC']
#	v['shlib_LINKFLAGS']     = ['-G']
	v['shlib_PATTERN']       = 'lib%s.so'

	# static lib
#	v['staticlib_LINKFLAGS'] = ['-Bstatic']
#	v['staticlib_PATTERN']   = 'lib%s.a'

detect = '''
find_generic_cc
find_cpp
find_ar
generic_cc_common_flags
cc_load_tools
cc_add_flags
link_add_flags
'''

