###########################
# this handles the magic we need to do for installing
# with all the configure options that affect rpath and shared
# library use

import Options
from TaskGen import feature, before, after
from samba_utils import *

@feature('install_bin')
@after('apply_core')
@before('apply_link', 'apply_obj_vars')
def install_binary(self):
    '''install a binary, taking account of the different rpath varients'''
    bld = self.bld

    # get the ldflags we will use for install and build
    install_ldflags = install_rpath(self)
    build_ldflags   = build_rpath(bld)

    if not Options.is_install:
        # just need to set rpath if we are not installing
        self.env.RPATH = build_ldflags
        return

    # work out the install path, expanding variables
    install_path = getattr(self, 'samba_inst_path', None) or '${BINDIR}'
    install_path = bld.EXPAND_VARIABLES(install_path)

    orig_target = os.path.basename(self.target)

    if install_ldflags != build_ldflags:
        # we will be creating a new target name, and using that for the
        # install link. That stops us from overwriting the existing build
        # target, which has different ldflags
        self.target += '.inst'

    # setup the right rpath link flags for the install
    self.env.RPATH = install_ldflags

    if not self.samba_install:
        # this binary is marked not to be installed
        return

    # tell waf to install the right binary
    bld.install_as(os.path.join(install_path, orig_target),
                   os.path.join(self.path.abspath(bld.env), self.target),
                   chmod=MODE_755)



@feature('install_lib')
@after('apply_core')
@before('apply_link', 'apply_obj_vars')
def install_library(self):
    '''install a library, taking account of the different rpath varients'''
    if getattr(self, 'done_install_library', False):
        return

    bld = self.bld

    install_ldflags = install_rpath(self)
    build_ldflags   = build_rpath(bld)

    if not Options.is_install or not getattr(self, 'samba_install', True):
        # just need to set the build rpath if we are not installing
        self.env.RPATH = build_ldflags
        return

    # setup the install path, expanding variables
    install_path = getattr(self, 'samba_inst_path', None)
    if install_path is None:
        if getattr(self, 'private_library', False):
            install_path = '${PRIVATELIBDIR}'
        else:
            install_path = '${LIBDIR}'
    install_path = bld.EXPAND_VARIABLES(install_path)

    target_name = self.target

    if install_ldflags != build_ldflags:
        # we will be creating a new target name, and using that for the
        # install link. That stops us from overwriting the existing build
        # target, which has different ldflags
        self.done_install_library = True
        t = self.clone('default')
        t.posted = False
        t.target += '.inst'
        self.env.RPATH = build_ldflags
    else:
        t = self

    t.env.RPATH = install_ldflags

    dev_link     = None

    # in the following the names are:
    # - inst_name is the name with .inst. in it, in the build
    #   directory
    # - install_name is the name in the install directory
    # - install_link is a symlink in the install directory, to install_name

    if getattr(self, 'samba_realname', None):
        install_name = self.samba_realname
        install_link = None
        if getattr(self, 'samba_type', None) == 'PYTHON':
            inst_name    = bld.make_libname(t.target, nolibprefix=True, python=True)
        else:
            inst_name    = bld.make_libname(t.target)
    elif self.vnum:
        vnum_base    = self.vnum.split('.')[0]
        install_name = bld.make_libname(target_name, version=self.vnum)
        install_link = bld.make_libname(target_name, version=vnum_base)
        inst_name    = bld.make_libname(t.target)
        if not self.private_library:
            # only generate the dev link for non-bundled libs
            dev_link     = bld.make_libname(target_name)
    elif getattr(self, 'soname', ''):
        install_name = bld.make_libname(target_name)
        install_link = self.soname
        inst_name    = bld.make_libname(t.target)
    else:
        install_name = bld.make_libname(target_name)
        install_link = None
        inst_name    = bld.make_libname(t.target)

    if t.env.SONAME_ST:
        # ensure we get the right names in the library
        if install_link:
            t.env.append_value('LINKFLAGS', t.env.SONAME_ST % install_link)
        else:
            t.env.append_value('LINKFLAGS', t.env.SONAME_ST % install_name)
        t.env.SONAME_ST = ''

    # tell waf to install the library
    bld.install_as(os.path.join(install_path, install_name),
                   os.path.join(self.path.abspath(bld.env), inst_name))
    if install_link and install_link != install_name:
        # and the symlink if needed
        bld.symlink_as(os.path.join(install_path, install_link), install_name)
    if dev_link:
        bld.symlink_as(os.path.join(install_path, dev_link), install_name)


@feature('cshlib')
@after('apply_implib')
@before('apply_vnum')
def apply_soname(self):
    '''install a library, taking account of the different rpath varients'''

    if self.env.SONAME_ST and getattr(self, 'soname', ''):
        self.env.append_value('LINKFLAGS', self.env.SONAME_ST % self.soname)
        self.env.SONAME_ST = ''

@feature('cshlib')
@after('apply_implib')
@before('apply_vnum')
def apply_vscript(self):
    '''add version-script arguments to library build'''

    if self.env.HAVE_LD_VERSION_SCRIPT and getattr(self, 'version_script', ''):
        self.env.append_value('LINKFLAGS', "-Wl,--version-script=%s" %
            self.version_script)
        self.version_script = None


##############################
# handle the creation of links for libraries and binaries in the build tree

@feature('symlink_lib')
@after('apply_link')
def symlink_lib(self):
    '''symlink a shared lib'''

    if self.target.endswith('.inst'):
        return

    blddir = os.path.dirname(self.bld.srcnode.abspath(self.bld.env))
    libpath = self.link_task.outputs[0].abspath(self.env)

    # calculat the link target and put it in the environment
    soext=""
    vnum = getattr(self, 'vnum', None)
    if vnum is not None:
        soext = '.' + vnum.split('.')[0]

    link_target = getattr(self, 'link_name', '')
    if link_target == '':
        basename = os.path.basename(self.bld.make_libname(self.target, version=soext))
        if getattr(self, "private_library", False):
            link_target = '%s/private/%s' % (LIB_PATH, basename)
        else:
            link_target = '%s/%s' % (LIB_PATH, basename)

    link_target = os.path.join(blddir, link_target)

    if os.path.lexists(link_target):
        if os.path.islink(link_target) and os.readlink(link_target) == libpath:
            return
        os.unlink(link_target)

    link_container = os.path.dirname(link_target)
    if not os.path.isdir(link_container):
        os.makedirs(link_container)

    os.symlink(libpath, link_target)


@feature('symlink_bin')
@after('apply_link')
def symlink_bin(self):
    '''symlink a binary into the build directory'''

    if self.target.endswith('.inst'):
        return

    blddir = os.path.dirname(self.bld.srcnode.abspath(self.bld.env))
    if not self.link_task.outputs or not self.link_task.outputs[0]:
        raise Utils.WafError('no outputs found for %s in symlink_bin' % self.name)
    binpath = self.link_task.outputs[0].abspath(self.env)
    bldpath = os.path.join(self.bld.env.BUILD_DIRECTORY, self.link_task.outputs[0].name)

    if os.path.lexists(bldpath):
        if os.path.islink(bldpath) and os.readlink(bldpath) == binpath:
            return
        os.unlink(bldpath)
    os.symlink(binpath, bldpath)
