# specialist handling of header files for Samba

import Build, re, Task, TaskGen, shutil, sys, Logs
from samba_utils import *


def header_install_path(header, header_path):
    '''find the installation path for a header, given a header_path option'''
    if not header_path:
        return ''
    if not isinstance(header_path, list):
        return header_path
    for (p1, dir) in header_path:
        for p2 in TO_LIST(p1):
            if fnmatch.fnmatch(header, p2):
                return dir
    # default to current path
    return ''


re_header = re.compile('^\s*#\s*include[ \t]*"([^"]+)"', re.I | re.M)

# a dictionary mapping source header paths to public header paths
header_map = {}

def find_suggested_header(hpath):
    '''find a suggested header path to use'''
    base = os.path.basename(hpath)
    ret = []
    for h in header_map:
        if os.path.basename(h) == base:
            ret.append('<%s>' % header_map[h])
            ret.append('"%s"' % h)
    return ret

def create_public_header(task):
    '''create a public header from a private one, output within the build tree'''
    src = task.inputs[0].abspath(task.env)
    tgt = task.outputs[0].bldpath(task.env)

    if os.path.exists(tgt):
        os.unlink(tgt)

    relsrc = os_path_relpath(src, task.env.TOPDIR)

    infile  = open(src, mode='r')
    outfile = open(tgt, mode='w')
    linenumber = 0

    search_paths = [ '', task.env.RELPATH ]
    for i in task.env.EXTRA_INCLUDES:
        if i.startswith('#'):
            search_paths.append(i[1:])

    for line in infile:
        linenumber += 1

        # allow some straight substitutions
        if task.env.public_headers_replace and line.strip() in task.env.public_headers_replace:
            outfile.write(task.env.public_headers_replace[line.strip()] + '\n')
            continue

        # see if its an include line
        m = re_header.match(line)
        if m is None:
            outfile.write(line)
            continue

        # its an include, get the header path
        hpath = m.group(1)
        if hpath.startswith("bin/default/"):
            hpath = hpath[12:]

        # some are always allowed
        if task.env.public_headers_skip and hpath in task.env.public_headers_skip:
            outfile.write(line)
            continue

        # work out the header this refers to
        found = False
        for s in search_paths:
            p = os.path.normpath(os.path.join(s, hpath))
            if p in header_map:
                outfile.write("#include <%s>\n" % header_map[p])
                found = True
                break
        if found:
            continue

        if task.env.public_headers_allow_broken:
            Logs.warn("Broken public header include '%s' in '%s'" % (hpath, relsrc))
            outfile.write(line)
            continue

        # try to be nice to the developer by suggesting an alternative
        suggested = find_suggested_header(hpath)
        outfile.close()
        os.unlink(tgt)
        sys.stderr.write("%s:%u:Error: unable to resolve public header %s (maybe try one of %s)\n" % (
            os.path.relpath(src, os.getcwd()), linenumber, hpath, suggested))
        raise Utils.WafError("Unable to resolve header path '%s' in public header '%s' in directory %s" % (
            hpath, relsrc, task.env.RELPATH))
    infile.close()
    outfile.close()


def public_headers_simple(bld, public_headers, header_path=None, public_headers_install=True):
    '''install some headers - simple version, no munging needed
    '''
    if not public_headers_install:
        return
    for h in TO_LIST(public_headers):
        inst_path = header_install_path(h, header_path)
        if h.find(':') != -1:
            s = h.split(":")
            h_name =  s[0]
            inst_name = s[1]
        else:
            h_name =  h
            inst_name = os.path.basename(h)
        bld.INSTALL_FILES('${INCLUDEDIR}', h_name, destname=inst_name)
        


def PUBLIC_HEADERS(bld, public_headers, header_path=None, public_headers_install=True):
    '''install some headers

    header_path may either be a string that is added to the INCLUDEDIR,
    or it can be a dictionary of wildcard patterns which map to destination
    directories relative to INCLUDEDIR
    '''
    bld.SET_BUILD_GROUP('final')

    if not bld.env.build_public_headers:
        # in this case no header munging neeeded. Used for tdb, talloc etc
        public_headers_simple(bld, public_headers, header_path=header_path,
                              public_headers_install=public_headers_install)
        return

    # create the public header in the given path
    # in the build tree
    for h in TO_LIST(public_headers):
        inst_path = header_install_path(h, header_path)
        if h.find(':') != -1:
            s = h.split(":")
            h_name =  s[0]
            inst_name = s[1]
        else:
            h_name =  h
            inst_name = os.path.basename(h)
        relpath1 = os_path_relpath(bld.srcnode.abspath(), bld.curdir)
        relpath2 = os_path_relpath(bld.curdir, bld.srcnode.abspath())
        targetdir = os.path.normpath(os.path.join(relpath1, bld.env.build_public_headers, inst_path))
        if not os.path.exists(os.path.join(bld.curdir, targetdir)):
            raise Utils.WafError("missing source directory %s for public header %s" % (targetdir, inst_name))
        target = os.path.join(targetdir, inst_name)

        # the source path of the header, relative to the top of the source tree
        src_path = os.path.normpath(os.path.join(relpath2, h_name))

        # the install path of the header, relative to the public include directory
        target_path = os.path.normpath(os.path.join(inst_path, inst_name))

        header_map[src_path] = target_path

        t = bld.SAMBA_GENERATOR('HEADER_%s/%s/%s' % (relpath2, inst_path, inst_name),
                                group='headers',
                                rule=create_public_header,
                                source=h_name,
                                target=target)
        t.env.RELPATH = relpath2
        t.env.TOPDIR  = bld.srcnode.abspath()
        if not bld.env.public_headers_list:
            bld.env.public_headers_list = []
        bld.env.public_headers_list.append(os.path.join(inst_path, inst_name))
        if public_headers_install:
            bld.INSTALL_FILES('${INCLUDEDIR}',
                              target,
                              destname=os.path.join(inst_path, inst_name), flat=True)
Build.BuildContext.PUBLIC_HEADERS = PUBLIC_HEADERS
