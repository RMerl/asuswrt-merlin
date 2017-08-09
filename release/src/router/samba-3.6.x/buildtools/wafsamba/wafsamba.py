# a waf tool to add autoconf-like macros to the configure section
# and for SAMBA_ macros for building libraries, binaries etc

import Build, os, sys, Options, Task, Utils, cc, TaskGen, fnmatch, re, shutil, Logs, Constants
from Configure import conf
from Logs import debug
from samba_utils import SUBST_VARS_RECURSIVE
TaskGen.task_gen.apply_verif = Utils.nada

# bring in the other samba modules
from samba_optimisation import *
from samba_utils import *
from samba_version import *
from samba_autoconf import *
from samba_patterns import *
from samba_pidl import *
from samba_autoproto import *
from samba_python import *
from samba_deps import *
from samba_bundled import *
import samba_install
import samba_conftests
import samba_abi
import samba_headers
import tru64cc
import irixcc
import hpuxcc
import generic_cc
import samba_dist
import samba_wildcard
import stale_files
import symbols
import pkgconfig

# some systems have broken threading in python
if os.environ.get('WAF_NOTHREADS') == '1':
    import nothreads

LIB_PATH="shared"

os.environ['PYTHONUNBUFFERED'] = '1'


if Constants.HEXVERSION < 0x105019:
    Logs.error('''
Please use the version of waf that comes with Samba, not
a system installed version. See http://wiki.samba.org/index.php/Waf
for details.

Alternatively, please run ./configure and make as usual. That will
call the right version of waf.''')
    sys.exit(1)


@conf
def SAMBA_BUILD_ENV(conf):
    '''create the samba build environment'''
    conf.env.BUILD_DIRECTORY = conf.blddir
    mkdir_p(os.path.join(conf.blddir, LIB_PATH))
    mkdir_p(os.path.join(conf.blddir, LIB_PATH, "private"))
    mkdir_p(os.path.join(conf.blddir, "modules"))
    mkdir_p(os.path.join(conf.blddir, 'python/samba/dcerpc'))
    # this allows all of the bin/shared and bin/python targets
    # to be expressed in terms of build directory paths
    mkdir_p(os.path.join(conf.blddir, 'default'))
    for p in ['python','shared', 'modules']:
        link_target = os.path.join(conf.blddir, 'default/' + p)
        if not os.path.lexists(link_target):
            os.symlink('../' + p, link_target)

    # get perl to put the blib files in the build directory
    blib_bld = os.path.join(conf.blddir, 'default/pidl/blib')
    blib_src = os.path.join(conf.srcdir, 'pidl/blib')
    mkdir_p(blib_bld + '/man1')
    mkdir_p(blib_bld + '/man3')
    if os.path.islink(blib_src):
        os.unlink(blib_src)
    elif os.path.exists(blib_src):
        shutil.rmtree(blib_src)


def ADD_INIT_FUNCTION(bld, subsystem, target, init_function):
    '''add an init_function to the list for a subsystem'''
    if init_function is None:
        return
    bld.ASSERT(subsystem is not None, "You must specify a subsystem for init_function '%s'" % init_function)
    cache = LOCAL_CACHE(bld, 'INIT_FUNCTIONS')
    if not subsystem in cache:
        cache[subsystem] = []
    cache[subsystem].append( { 'TARGET':target, 'INIT_FUNCTION':init_function } )
Build.BuildContext.ADD_INIT_FUNCTION = ADD_INIT_FUNCTION



#################################################################
def SAMBA_LIBRARY(bld, libname, source,
                  deps='',
                  public_deps='',
                  includes='',
                  public_headers=None,
                  public_headers_install=True,
                  header_path=None,
                  pc_files=None,
                  vnum=None,
                  soname=None,
                  cflags='',
                  ldflags='',
                  external_library=False,
                  realname=None,
                  autoproto=None,
                  group='libraries',
                  depends_on='',
                  local_include=True,
                  global_include=True,
                  vars=None,
                  subdir=None,
                  install_path=None,
                  install=True,
                  pyembed=False,
                  pyext=False,
                  target_type='LIBRARY',
                  bundled_extension=True,
                  link_name=None,
                  abi_directory=None,
                  abi_match=None,
                  hide_symbols=False,
                  manpages=None,
                  private_library=False,
                  grouping_library=False,
                  allow_undefined_symbols=False,
                  enabled=True):
    '''define a Samba library'''

    if not enabled:
        SET_TARGET_TYPE(bld, libname, 'DISABLED')
        return

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)

    # remember empty libraries, so we can strip the dependencies
    if ((source == '') or (source == [])) and deps == '' and public_deps == '':
        SET_TARGET_TYPE(bld, libname, 'EMPTY')
        return

    if BUILTIN_LIBRARY(bld, libname):
        obj_target = libname
    else:
        obj_target = libname + '.objlist'

    if group == 'libraries':
        subsystem_group = 'main'
    else:
        subsystem_group = group

    # first create a target for building the object files for this library
    # by separating in this way, we avoid recompiling the C files
    # separately for the install library and the build library
    bld.SAMBA_SUBSYSTEM(obj_target,
                        source         = source,
                        deps           = deps,
                        public_deps    = public_deps,
                        includes       = includes,
                        public_headers = public_headers,
                        public_headers_install = public_headers_install,
                        header_path    = header_path,
                        cflags         = cflags,
                        group          = subsystem_group,
                        autoproto      = autoproto,
                        depends_on     = depends_on,
                        hide_symbols   = hide_symbols,
                        pyext          = pyext or (target_type == "PYTHON"),
                        local_include  = local_include,
                        global_include = global_include)

    if BUILTIN_LIBRARY(bld, libname):
        return

    if not SET_TARGET_TYPE(bld, libname, target_type):
        return

    # the library itself will depend on that object target
    deps += ' ' + public_deps
    deps = TO_LIST(deps)
    deps.append(obj_target)

    realname = bld.map_shlib_extension(realname, python=(target_type=='PYTHON'))
    link_name = bld.map_shlib_extension(link_name, python=(target_type=='PYTHON'))

    # we don't want any public libraries without version numbers
    if not private_library and vnum is None and soname is None and target_type != 'PYTHON' and not realname:
        raise Utils.WafError("public library '%s' must have a vnum" % libname)

    if target_type == 'PYTHON' or realname or not private_library:
        bundled_name = libname.replace('_', '-')
    else:
        bundled_name = PRIVATE_NAME(bld, libname, bundled_extension, private_library)

    ldflags = TO_LIST(ldflags)

    features = 'cc cshlib symlink_lib install_lib'
    if target_type == 'PYTHON':
        features += ' pyext'
    if pyext or pyembed:
        # this is quite strange. we should add pyext feature for pyext
        # but that breaks the build. This may be a bug in the waf python tool
        features += ' pyembed'

    if abi_directory:
        features += ' abi_check'

    vscript = None
    if bld.env.HAVE_LD_VERSION_SCRIPT:
        if private_library:
            version = "%s_%s" % (Utils.g_module.APPNAME, Utils.g_module.VERSION)
        elif vnum:
            version = "%s_%s" % (libname, vnum)
        else:
            version = None
        if version:
            vscript = "%s.vscript" % libname
            bld.ABI_VSCRIPT(libname, abi_directory, version, vscript,
                            abi_match)
            fullname = apply_pattern(bundled_name, bld.env.shlib_PATTERN)
            fullpath = bld.path.find_or_declare(fullname)
            vscriptpath = bld.path.find_or_declare(vscript)
            if not fullpath:
                raise Utils.WafError("unable to find fullpath for %s" % fullname)
            if not vscriptpath:
                raise Utils.WafError("unable to find vscript path for %s" % vscript)
            bld.add_manual_dependency(fullpath, vscriptpath)
            if Options.is_install:
                # also make the .inst file depend on the vscript
                instname = apply_pattern(bundled_name + '.inst', bld.env.shlib_PATTERN)
                bld.add_manual_dependency(bld.path.find_or_declare(instname), bld.path.find_or_declare(vscript))
            vscript = os.path.join(bld.path.abspath(bld.env), vscript)

    bld.SET_BUILD_GROUP(group)
    t = bld(
        features        = features,
        source          = [],
        target          = bundled_name,
        depends_on      = depends_on,
        samba_ldflags   = ldflags,
        samba_deps      = deps,
        samba_includes  = includes,
        version_script  = vscript,
        local_include   = local_include,
        global_include  = global_include,
        vnum            = vnum,
        soname          = soname,
        install_path    = None,
        samba_inst_path = install_path,
        name            = libname,
        samba_realname  = realname,
        samba_install   = install,
        abi_directory   = "%s/%s" % (bld.path.abspath(), abi_directory),
        abi_match       = abi_match,
        private_library = private_library,
        grouping_library=grouping_library,
        allow_undefined_symbols=allow_undefined_symbols
        )

    if realname and not link_name:
        link_name = 'shared/%s' % realname

    if link_name:
        t.link_name = link_name

    if pc_files is not None:
        bld.PKG_CONFIG_FILES(pc_files, vnum=vnum)

    if manpages is not None and 'XSLTPROC_MANPAGES' in bld.env and bld.env['XSLTPROC_MANPAGES']:
        bld.MANPAGES(manpages)


Build.BuildContext.SAMBA_LIBRARY = SAMBA_LIBRARY


#################################################################
def SAMBA_BINARY(bld, binname, source,
                 deps='',
                 includes='',
                 public_headers=None,
                 header_path=None,
                 modules=None,
                 ldflags=None,
                 cflags='',
                 autoproto=None,
                 use_hostcc=False,
                 use_global_deps=True,
                 compiler=None,
                 group='binaries',
                 manpages=None,
                 local_include=True,
                 global_include=True,
                 subsystem_name=None,
                 pyembed=False,
                 vars=None,
                 subdir=None,
                 install=True,
                 install_path=None,
                 enabled=True):
    '''define a Samba binary'''

    if not enabled:
        SET_TARGET_TYPE(bld, binname, 'DISABLED')
        return

    if not SET_TARGET_TYPE(bld, binname, 'BINARY'):
        return

    features = 'cc cprogram symlink_bin install_bin'
    if pyembed:
        features += ' pyembed'

    obj_target = binname + '.objlist'

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)
    source = unique_list(TO_LIST(source))

    if group == 'binaries':
        subsystem_group = 'main'
    else:
        subsystem_group = group

    # first create a target for building the object files for this binary
    # by separating in this way, we avoid recompiling the C files
    # separately for the install binary and the build binary
    bld.SAMBA_SUBSYSTEM(obj_target,
                        source         = source,
                        deps           = deps,
                        includes       = includes,
                        cflags         = cflags,
                        group          = subsystem_group,
                        autoproto      = autoproto,
                        subsystem_name = subsystem_name,
                        local_include  = local_include,
                        global_include = global_include,
                        use_hostcc     = use_hostcc,
                        pyext          = pyembed,
                        use_global_deps= use_global_deps)

    bld.SET_BUILD_GROUP(group)

    # the binary itself will depend on that object target
    deps = TO_LIST(deps)
    deps.append(obj_target)

    t = bld(
        features       = features,
        source         = [],
        target         = binname,
        samba_deps     = deps,
        samba_includes = includes,
        local_include  = local_include,
        global_include = global_include,
        samba_modules  = modules,
        top            = True,
        samba_subsystem= subsystem_name,
        install_path   = None,
        samba_inst_path= install_path,
        samba_install  = install,
        samba_ldflags  = TO_LIST(ldflags)
        )

    if manpages is not None and 'XSLTPROC_MANPAGES' in bld.env and bld.env['XSLTPROC_MANPAGES']:
        bld.MANPAGES(manpages)

Build.BuildContext.SAMBA_BINARY = SAMBA_BINARY


#################################################################
def SAMBA_MODULE(bld, modname, source,
                 deps='',
                 includes='',
                 subsystem=None,
                 init_function=None,
                 module_init_name='samba_init_module',
                 autoproto=None,
                 autoproto_extra_source='',
                 cflags='',
                 internal_module=True,
                 local_include=True,
                 global_include=True,
                 vars=None,
                 subdir=None,
                 enabled=True,
                 pyembed=False,
                 allow_undefined_symbols=False
                 ):
    '''define a Samba module.'''

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)

    if internal_module or BUILTIN_LIBRARY(bld, modname):
        bld.SAMBA_SUBSYSTEM(modname, source,
                    deps=deps,
                    includes=includes,
                    autoproto=autoproto,
                    autoproto_extra_source=autoproto_extra_source,
                    cflags=cflags,
                    local_include=local_include,
                    global_include=global_include,
                    enabled=enabled)

        bld.ADD_INIT_FUNCTION(subsystem, modname, init_function)
        return

    if not enabled:
        SET_TARGET_TYPE(bld, modname, 'DISABLED')
        return

    obj_target = modname + '.objlist'

    realname = modname
    if subsystem is not None:
        deps += ' ' + subsystem
        while realname.startswith("lib"+subsystem+"_"):
            realname = realname[len("lib"+subsystem+"_"):]
        while realname.startswith(subsystem+"_"):
            realname = realname[len(subsystem+"_"):]

    realname = bld.make_libname(realname)
    while realname.startswith("lib"):
        realname = realname[len("lib"):]

    build_link_name = "modules/%s/%s" % (subsystem, realname)

    if init_function:
        cflags += " -D%s=%s" % (init_function, module_init_name)

    bld.SAMBA_LIBRARY(modname,
                      source,
                      deps=deps,
                      includes=includes,
                      cflags=cflags,
                      realname = realname,
                      autoproto = autoproto,
                      local_include=local_include,
                      global_include=global_include,
                      vars=vars,
                      link_name=build_link_name,
                      install_path="${MODULESDIR}/%s" % subsystem,
                      pyembed=pyembed,
                      allow_undefined_symbols=allow_undefined_symbols
                      )


Build.BuildContext.SAMBA_MODULE = SAMBA_MODULE


#################################################################
def SAMBA_SUBSYSTEM(bld, modname, source,
                    deps='',
                    public_deps='',
                    includes='',
                    public_headers=None,
                    public_headers_install=True,
                    header_path=None,
                    cflags='',
                    cflags_end=None,
                    group='main',
                    init_function_sentinal=None,
                    autoproto=None,
                    autoproto_extra_source='',
                    depends_on='',
                    local_include=True,
                    local_include_first=True,
                    global_include=True,
                    subsystem_name=None,
                    enabled=True,
                    use_hostcc=False,
                    use_global_deps=True,
                    vars=None,
                    subdir=None,
                    hide_symbols=False,
                    pyext=False):
    '''define a Samba subsystem'''

    if not enabled:
        SET_TARGET_TYPE(bld, modname, 'DISABLED')
        return

    # remember empty subsystems, so we can strip the dependencies
    if ((source == '') or (source == [])) and deps == '' and public_deps == '':
        SET_TARGET_TYPE(bld, modname, 'EMPTY')
        return

    if not SET_TARGET_TYPE(bld, modname, 'SUBSYSTEM'):
        return

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)
    source = unique_list(TO_LIST(source))

    deps += ' ' + public_deps

    bld.SET_BUILD_GROUP(group)

    features = 'cc'
    if pyext:
        features += ' pyext'

    t = bld(
        features       = features,
        source         = source,
        target         = modname,
        samba_cflags   = CURRENT_CFLAGS(bld, modname, cflags, hide_symbols=hide_symbols),
        depends_on     = depends_on,
        samba_deps     = TO_LIST(deps),
        samba_includes = includes,
        local_include  = local_include,
        local_include_first  = local_include_first,
        global_include = global_include,
        samba_subsystem= subsystem_name,
        samba_use_hostcc = use_hostcc,
        samba_use_global_deps = use_global_deps
        )

    if cflags_end is not None:
        t.samba_cflags.extend(TO_LIST(cflags_end))

    if autoproto is not None:
        bld.SAMBA_AUTOPROTO(autoproto, source + TO_LIST(autoproto_extra_source))
    if public_headers is not None:
        bld.PUBLIC_HEADERS(public_headers, header_path=header_path,
                           public_headers_install=public_headers_install)
    return t


Build.BuildContext.SAMBA_SUBSYSTEM = SAMBA_SUBSYSTEM


def SAMBA_GENERATOR(bld, name, rule, source='', target='',
                    group='generators', enabled=True,
                    public_headers=None,
                    public_headers_install=True,
                    header_path=None,
                    vars=None,
                    always=False):
    '''A generic source generator target'''

    if not SET_TARGET_TYPE(bld, name, 'GENERATOR'):
        return

    if not enabled:
        return

    bld.SET_BUILD_GROUP(group)
    t = bld(
        rule=rule,
        source=bld.EXPAND_VARIABLES(source, vars=vars),
        target=target,
        shell=isinstance(rule, str),
        on_results=True,
        before='cc',
        ext_out='.c',
        samba_type='GENERATOR',
        dep_vars = [rule] + (vars or []),
        name=name)

    if always:
        t.always = True

    if public_headers is not None:
        bld.PUBLIC_HEADERS(public_headers, header_path=header_path,
                           public_headers_install=public_headers_install)
    return t
Build.BuildContext.SAMBA_GENERATOR = SAMBA_GENERATOR



@runonce
def SETUP_BUILD_GROUPS(bld):
    '''setup build groups used to ensure that the different build
    phases happen consecutively'''
    bld.p_ln = bld.srcnode # we do want to see all targets!
    bld.env['USING_BUILD_GROUPS'] = True
    bld.add_group('setup')
    bld.add_group('build_compiler_source')
    bld.add_group('vscripts')
    bld.add_group('base_libraries')
    bld.add_group('generators')
    bld.add_group('compiler_prototypes')
    bld.add_group('compiler_libraries')
    bld.add_group('build_compilers')
    bld.add_group('build_source')
    bld.add_group('prototypes')
    bld.add_group('headers')
    bld.add_group('main')
    bld.add_group('symbolcheck')
    bld.add_group('libraries')
    bld.add_group('binaries')
    bld.add_group('syslibcheck')
    bld.add_group('final')
Build.BuildContext.SETUP_BUILD_GROUPS = SETUP_BUILD_GROUPS


def SET_BUILD_GROUP(bld, group):
    '''set the current build group'''
    if not 'USING_BUILD_GROUPS' in bld.env:
        return
    bld.set_group(group)
Build.BuildContext.SET_BUILD_GROUP = SET_BUILD_GROUP



@conf
def ENABLE_TIMESTAMP_DEPENDENCIES(conf):
    """use timestamps instead of file contents for deps
    this currently doesn't work"""
    def h_file(filename):
        import stat
        st = os.stat(filename)
        if stat.S_ISDIR(st[stat.ST_MODE]): raise IOError('not a file')
        m = Utils.md5()
        m.update(str(st.st_mtime))
        m.update(str(st.st_size))
        m.update(filename)
        return m.digest()
    Utils.h_file = h_file



t = Task.simple_task_type('copy_script', 'rm -f "${LINK_TARGET}" && ln -s "${SRC[0].abspath(env)}" ${LINK_TARGET}',
                          shell=True, color='PINK', ext_in='.bin')
t.quiet = True

@feature('copy_script')
@before('apply_link')
def copy_script(self):
    tsk = self.create_task('copy_script', self.allnodes[0])
    tsk.env.TARGET = self.target

def SAMBA_SCRIPT(bld, name, pattern, installdir, installname=None):
    '''used to copy scripts from the source tree into the build directory
       for use by selftest'''

    source = bld.path.ant_glob(pattern)

    bld.SET_BUILD_GROUP('build_source')
    for s in TO_LIST(source):
        iname = s
        if installname != None:
            iname = installname
        target = os.path.join(installdir, iname)
        tgtdir = os.path.dirname(os.path.join(bld.srcnode.abspath(bld.env), '..', target))
        mkdir_p(tgtdir)
        t = bld(features='copy_script',
                source       = s,
                target       = target,
                always       = True,
                install_path = None)
        t.env.LINK_TARGET = target

Build.BuildContext.SAMBA_SCRIPT = SAMBA_SCRIPT

def copy_and_fix_python_path(task):
    pattern='sys.path.insert(0, "bin/python")'
    if task.env["PYTHONARCHDIR"] in sys.path and task.env["PYTHONDIR"] in sys.path:
        replacement = ""
    elif task.env["PYTHONARCHDIR"] == task.env["PYTHONDIR"]:
        replacement="""sys.path.insert(0, "%s")""" % task.env["PYTHONDIR"]
    else:
        replacement="""sys.path.insert(0, "%s")
sys.path.insert(1, "%s")""" % (task.env["PYTHONARCHDIR"], task.env["PYTHONDIR"])

    installed_location=task.outputs[0].bldpath(task.env)
    source_file = open(task.inputs[0].srcpath(task.env))
    installed_file = open(installed_location, 'w')
    for line in source_file:
        newline = line
        if pattern in line:
            newline = line.replace(pattern, replacement)
        installed_file.write(newline)
    installed_file.close()
    os.chmod(installed_location, 0755)
    return 0


def install_file(bld, destdir, file, chmod=MODE_644, flat=False,
                 python_fixup=False, destname=None, base_name=None):
    '''install a file'''
    destdir = bld.EXPAND_VARIABLES(destdir)
    if not destname:
        destname = file
        if flat:
            destname = os.path.basename(destname)
    dest = os.path.join(destdir, destname)
    if python_fixup:
        # fixup the python path it will use to find Samba modules
        inst_file = file + '.inst'
        bld.SAMBA_GENERATOR('python_%s' % destname,
                            rule=copy_and_fix_python_path,
                            source=file,
                            target=inst_file)
        file = inst_file
    if base_name:
        file = os.path.join(base_name, file)
    bld.install_as(dest, file, chmod=chmod)


def INSTALL_FILES(bld, destdir, files, chmod=MODE_644, flat=False,
                  python_fixup=False, destname=None, base_name=None):
    '''install a set of files'''
    for f in TO_LIST(files):
        install_file(bld, destdir, f, chmod=chmod, flat=flat,
                     python_fixup=python_fixup, destname=destname,
                     base_name=base_name)
Build.BuildContext.INSTALL_FILES = INSTALL_FILES


def INSTALL_WILDCARD(bld, destdir, pattern, chmod=MODE_644, flat=False,
                     python_fixup=False, exclude=None, trim_path=None):
    '''install a set of files matching a wildcard pattern'''
    files=TO_LIST(bld.path.ant_glob(pattern))
    if trim_path:
        files2 = []
        for f in files:
            files2.append(os_path_relpath(f, trim_path))
        files = files2

    if exclude:
        for f in files[:]:
            if fnmatch.fnmatch(f, exclude):
                files.remove(f)
    INSTALL_FILES(bld, destdir, files, chmod=chmod, flat=flat,
                  python_fixup=python_fixup, base_name=trim_path)
Build.BuildContext.INSTALL_WILDCARD = INSTALL_WILDCARD


def INSTALL_DIRS(bld, destdir, dirs):
    '''install a set of directories'''
    destdir = bld.EXPAND_VARIABLES(destdir)
    dirs = bld.EXPAND_VARIABLES(dirs)
    for d in TO_LIST(dirs):
        bld.install_dir(os.path.join(destdir, d))
Build.BuildContext.INSTALL_DIRS = INSTALL_DIRS


def MANPAGES(bld, manpages):
    '''build and install manual pages'''
    bld.env.MAN_XSL = 'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl'
    for m in manpages.split():
        source = m + '.xml'
        bld.SAMBA_GENERATOR(m,
                            source=source,
                            target=m,
                            group='final',
                            rule='${XSLTPROC} -o ${TGT} --nonet ${MAN_XSL} ${SRC}'
                            )
        bld.INSTALL_FILES('${MANDIR}/man%s' % m[-1], m, flat=True)
Build.BuildContext.MANPAGES = MANPAGES


#############################################################
# give a nicer display when building different types of files
def progress_display(self, msg, fname):
    col1 = Logs.colors(self.color)
    col2 = Logs.colors.NORMAL
    total = self.position[1]
    n = len(str(total))
    fs = '[%%%dd/%%%dd] %s %%s%%s%%s\n' % (n, n, msg)
    return fs % (self.position[0], self.position[1], col1, fname, col2)

def link_display(self):
    if Options.options.progress_bar != 0:
        return Task.Task.old_display(self)
    fname = self.outputs[0].bldpath(self.env)
    return progress_display(self, 'Linking', fname)
Task.TaskBase.classes['cc_link'].display = link_display

def samba_display(self):
    if Options.options.progress_bar != 0:
        return Task.Task.old_display(self)

    targets    = LOCAL_CACHE(self, 'TARGET_TYPE')
    if self.name in targets:
        target_type = targets[self.name]
        type_map = { 'GENERATOR' : 'Generating',
                     'PROTOTYPE' : 'Generating'
                     }
        if target_type in type_map:
            return progress_display(self, type_map[target_type], self.name)

    if len(self.inputs) == 0:
        return Task.Task.old_display(self)

    fname = self.inputs[0].bldpath(self.env)
    if fname[0:3] == '../':
        fname = fname[3:]
    ext_loc = fname.rfind('.')
    if ext_loc == -1:
        return Task.Task.old_display(self)
    ext = fname[ext_loc:]

    ext_map = { '.idl' : 'Compiling IDL',
                '.et'  : 'Compiling ERRTABLE',
                '.asn1': 'Compiling ASN1',
                '.c'   : 'Compiling' }
    if ext in ext_map:
        return progress_display(self, ext_map[ext], fname)
    return Task.Task.old_display(self)

Task.TaskBase.classes['Task'].old_display = Task.TaskBase.classes['Task'].display
Task.TaskBase.classes['Task'].display = samba_display


@after('apply_link')
@feature('cshlib')
def apply_bundle_remove_dynamiclib_patch(self):
    if self.env['MACBUNDLE'] or getattr(self,'mac_bundle',False):
        if not getattr(self,'vnum',None):
            try:
                self.env['LINKFLAGS'].remove('-dynamiclib')
                self.env['LINKFLAGS'].remove('-single_module')
            except ValueError:
                pass
