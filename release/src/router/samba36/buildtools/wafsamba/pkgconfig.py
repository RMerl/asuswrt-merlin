# handle substitution of variables in pc files

import Build, sys, Logs
from samba_utils import *

def subst_at_vars(task):
    '''substiture @VAR@ style variables in a file'''
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)

    f = open(src, 'r')
    s = f.read()
    f.close()
    # split on the vars
    a = re.split('(@\w+@)', s)
    out = []
    done_var = {}
    back_sub = [ ('PREFIX', '${prefix}'), ('EXEC_PREFIX', '${exec_prefix}')]
    for v in a:
        if re.match('@\w+@', v):
            vname = v[1:-1]
            if not vname in task.env and vname.upper() in task.env:
                vname = vname.upper()
            if not vname in task.env:
                Logs.error("Unknown substitution %s in %s" % (v, task.name))
                sys.exit(1)
            v = SUBST_VARS_RECURSIVE(task.env[vname], task.env)
            # now we back substitute the allowed pc vars
            for (b, m) in back_sub:
                s = task.env[b]
                if s == v[0:len(s)]:
                    if not b in done_var:
                        # we don't want to substitute the first usage
                        done_var[b] = True
                    else:
                        v = m + v[len(s):]
                    break
        out.append(v)
    contents = ''.join(out)
    f = open(tgt, 'w')
    s = f.write(contents)
    f.close()
    return 0


def PKG_CONFIG_FILES(bld, pc_files, vnum=None):
    '''install some pkg_config pc files'''
    dest = '${PKGCONFIGDIR}'
    dest = bld.EXPAND_VARIABLES(dest)
    for f in TO_LIST(pc_files):
        base=os.path.basename(f)
        t = bld.SAMBA_GENERATOR('PKGCONFIG_%s' % base,
                                rule=subst_at_vars,
                                source=f+'.in',
                                target=f)
        t.vars = []
        if t.env.RPATH_ON_INSTALL:
            t.env.LIB_RPATH = t.env.RPATH_ST % t.env.LIBDIR
        else:
            t.env.LIB_RPATH = ''
        if vnum:
            t.env.PACKAGE_VERSION = vnum
        for v in [ 'PREFIX', 'EXEC_PREFIX', 'LIB_RPATH' ]:
            t.vars.append(t.env[v])
        bld.INSTALL_FILES(dest, f, flat=True, destname=base)
Build.BuildContext.PKG_CONFIG_FILES = PKG_CONFIG_FILES


