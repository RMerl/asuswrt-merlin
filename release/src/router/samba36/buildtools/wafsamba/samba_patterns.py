# a waf tool to add extension based build patterns for Samba

import Task
from TaskGen import extension
from samba_utils import *
from wafsamba import samba_version_file

def write_version_header(task):
    '''print version.h contents'''
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)

    version = samba_version_file(src, task.env.srcdir, env=task.env)
    string = str(version)

    f = open(tgt, 'w')
    s = f.write(string)
    f.close()
    return 0


def SAMBA_MKVERSION(bld, target):
    '''generate the version.h header for Samba'''
    t = bld.SAMBA_GENERATOR('VERSION', 
                            rule=write_version_header,
                            source= 'VERSION',
                            target=target,
                            always=True)
Build.BuildContext.SAMBA_MKVERSION = SAMBA_MKVERSION
