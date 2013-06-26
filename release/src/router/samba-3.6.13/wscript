#! /usr/bin/env python

srcdir = '.'
blddir = 'bin'

APPNAME='samba'
VERSION=None

import sys, os
sys.path.insert(0, srcdir+"/buildtools/wafsamba")
import wafsamba, Options, samba_dist, Scripting, Utils, samba_version


samba_dist.DIST_DIRS('.')

#This is a list of files that we don't want in the package, for
#whatever reason.  Directories should be listed with a trailing / to
#avoid over-exclusion.

#This list includes files that would confuse the recipient of a
#samba-4.0.0 branded tarball (until the merge is complete) and the
#core elements of the autotools build system (which is known to
#produce buggy binaries).
samba_dist.DIST_BLACKLIST('README Manifest Read-Manifest-Now Roadmap ' +
                          'packaging/ docs-xml/ examples/ swat/ WHATSNEW.txt MAINTAINERS ')
# install in /usr/local/samba by default
Options.default_prefix = '/usr/local/samba'

os.environ['TOPLEVEL_BUILD'] = '1'

def set_options(opt):
    opt.BUILTIN_DEFAULT('NONE')
    opt.PRIVATE_EXTENSION_DEFAULT('samba4')
    opt.RECURSE('lib/replace')
    opt.RECURSE('source4/dynconfig')
    opt.RECURSE('source4/lib/ldb')
    opt.RECURSE('source4/selftest')
    opt.RECURSE('source4/lib/tls')
    opt.RECURSE('lib/nss_wrapper')
    opt.RECURSE('lib/socket_wrapper')
    opt.RECURSE('lib/uid_wrapper')
    opt.RECURSE('pidl')
    opt.RECURSE('source3')

    gr = opt.option_group('developer options')
    gr.add_option('--enable-build-farm',
                   help='enable special build farm options',
                   action='store_true', dest='BUILD_FARM')

    gr.add_option('--enable-s3build',
                   help='enable build of s3 binaries',
                   action='store_true', dest='S3BUILD')

    opt.tool_options('python') # options for disabling pyc or pyo compilation
    # enable options related to building python extensions


def configure(conf):
    conf.env.toplevel_build = True
    version = samba_version.load_version(env=conf.env)

    conf.DEFINE('CONFIG_H_IS_FROM_SAMBA', 1)
    conf.DEFINE('_SAMBA_BUILD_', version.MAJOR, add_to_cflags=True)
    conf.DEFINE('HAVE_CONFIG_H', 1, add_to_cflags=True)

    if Options.options.developer:
        conf.ADD_CFLAGS('-DDEVELOPER -DDEBUG_PASSWORD')

    if Options.options.S3BUILD:
        conf.env.enable_s3build = True

    # this enables smbtorture.static for s3 in the build farm
    conf.env.BUILD_FARM = Options.options.BUILD_FARM or os.environ.get('RUN_FROM_BUILD_FARM')

    conf.ADD_EXTRA_INCLUDES('#include/public #source4 #lib #source4/lib #source4/include #include')

    conf.RECURSE('lib/replace')

    conf.find_program('python', var='PYTHON', mandatory=True)
    conf.find_program('perl', var='PERL', mandatory=True)
    conf.find_program('xsltproc', var='XSLTPROC')

    # enable tool to build python extensions
    conf.check_tool('python')
    conf.check_python_version((2,4,2))
    conf.SAMBA_CHECK_PYTHON_HEADERS(mandatory=True)

    if sys.platform == 'darwin' and not conf.env['HAVE_ENVIRON_DECL']:
        # Mac OSX needs to have this and it's also needed that the python is compiled with this
        # otherwise you face errors about common symbols
        if not conf.CHECK_SHLIB_W_PYTHON("Checking if -fno-common is needed"):
            conf.ADD_CFLAGS('-fno-common')
        if not conf.CHECK_SHLIB_W_PYTHON("Checking if -undefined dynamic_lookup is not need"):
            conf.env.append_value('shlib_LINKFLAGS', ['-undefined', 'dynamic_lookup'])
    if int(conf.env['PYTHON_VERSION'][0]) >= 3:
        raise Utils.WafError('Python version 3.x is not supported by Samba yet')

    conf.RECURSE('source4/dynconfig')
    conf.RECURSE('source4/lib/ldb')
    conf.RECURSE('source4/heimdal_build')
    conf.RECURSE('source4/lib/tls')
    conf.RECURSE('source4/ntvfs/sysdep')
    conf.RECURSE('lib/util')
    conf.RECURSE('lib/zlib')
    conf.RECURSE('lib/util/charset')
    conf.RECURSE('source4/auth')
    conf.RECURSE('lib/nss_wrapper')
    conf.RECURSE('nsswitch')
    conf.RECURSE('lib/socket_wrapper')
    conf.RECURSE('lib/uid_wrapper')
    conf.RECURSE('lib/popt')
    conf.RECURSE('lib/subunit/c')
    conf.RECURSE('libcli/smbreadline')
    conf.RECURSE('pidl')
    conf.RECURSE('source4/selftest')
    if conf.env.enable_s3build:
        conf.RECURSE('source3')

    # we don't want any libraries or modules to rely on runtime
    # resolution of symbols
    if sys.platform != "openbsd4":
        conf.env.undefined_ldflags = conf.ADD_LDFLAGS('-Wl,-no-undefined', testflags=True)

    # gentoo always adds this. We want our normal build to be as
    # strict as the strictest OS we support, so adding this here
    # allows us to find problems on our development hosts faster.
    # It also results in faster load time.
    if sys.platform != "openbsd4":
        conf.env.asneeded_ldflags = conf.ADD_LDFLAGS('-Wl,--as-needed', testflags=True)

    if not conf.CHECK_NEED_LC("-lc not needed"):
        conf.ADD_LDFLAGS('-lc', testflags=False)

    # we don't want PYTHONDIR in config.h, as otherwise changing
    # --prefix causes a complete rebuild
    del(conf.env.defines['PYTHONDIR'])
    del(conf.env.defines['PYTHONARCHDIR'])

    conf.SAMBA_CONFIG_H('include/config.h')


def etags(ctx):
    '''build TAGS file using etags'''
    import Utils
    source_root = os.path.dirname(Utils.g_module.root_path)
    cmd = 'etags $(find %s -name "*.[ch]" | egrep -v \.inst\.)' % source_root
    print("Running: %s" % cmd)
    os.system(cmd)

def ctags(ctx):
    "build 'tags' file using ctags"
    import Utils
    source_root = os.path.dirname(Utils.g_module.root_path)
    cmd = 'ctags $(find %s -name "*.[ch]" | grep -v "*_proto\.h" | egrep -v \.inst\.)' % source_root
    print("Running: %s" % cmd)
    os.system(cmd)

# putting this here enabled build in the list
# of commands in --help
def build(bld):
    '''build all targets'''
    samba_version.load_version(env=bld.env)
    pass


def pydoctor(ctx):
    '''build python apidocs'''
    cmd='PYTHONPATH=bin/python pydoctor --project-name=Samba --project-url=http://www.samba.org --make-html --docformat=restructuredtext --add-package bin/python/samba'
    print("Running: %s" % cmd)
    os.system(cmd)

def wafdocs(ctx):
    '''build wafsamba apidocs'''
    from samba_utils import recursive_dirlist
    os.system('pwd')
    list = recursive_dirlist('../buildtools/wafsamba', '.', pattern='*.py')

    cmd='PYTHONPATH=bin/python pydoctor --project-name=wafsamba --project-url=http://www.samba.org --make-html --docformat=restructuredtext'
    print(list)
    for f in list:
        cmd += ' --add-module %s' % f
    print("Running: %s" % cmd)
    os.system(cmd)


def dist():
    '''makes a tarball for distribution'''
    samba_version.load_version(env=None)
    samba_dist.dist()

def distcheck():
    '''test that distribution tarball builds and installs'''
    samba_version.load_version(env=None)
    import Scripting
    d = Scripting.distcheck
    d(subdir='source4')

def wildcard_cmd(cmd):
    '''called on a unknown command'''
    from samba_wildcard import run_named_build_task
    run_named_build_task(cmd)

def main():
    from samba_wildcard import wildcard_main
    wildcard_main(wildcard_cmd)
Scripting.main = main

def reconfigure(ctx):
    '''reconfigure if config scripts have changed'''
    import samba_utils
    samba_utils.reconfigure(ctx)
