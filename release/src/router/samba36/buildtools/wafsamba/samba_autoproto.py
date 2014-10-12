# waf build tool for building automatic prototypes from C source

import Build
from samba_utils import *

def SAMBA_AUTOPROTO(bld, header, source):
    '''rule for samba prototype generation'''
    bld.SET_BUILD_GROUP('prototypes')
    relpath = os_path_relpath(bld.curdir, bld.srcnode.abspath())
    name = os.path.join(relpath, header)
    SET_TARGET_TYPE(bld, name, 'PROTOTYPE')
    t = bld(
        name = name,
        source = source,
        target = header,
        on_results=True,
        ext_out='.c',
        before ='cc',
        rule = '${PERL} "${SCRIPT}/mkproto.pl" --srcdir=.. --builddir=. --public=/dev/null --private="${TGT}" ${SRC}'
        )
    t.env.SCRIPT = os.path.join(bld.srcnode.abspath(), 'source4/script')
Build.BuildContext.SAMBA_AUTOPROTO = SAMBA_AUTOPROTO

