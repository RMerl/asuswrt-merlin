# a waf tool to add autoconf-like macros to the configure section

import Build, os, sys, Options, preproc, Logs
import string
from Configure import conf
from samba_utils import *
import samba_cross

missing_headers = set()

####################################################
# some autoconf like helpers, to make the transition
# to waf a bit easier for those used to autoconf
# m4 files

@runonce
@conf
def DEFINE(conf, d, v, add_to_cflags=False, quote=False):
    '''define a config option'''
    conf.define(d, v, quote=quote)
    if add_to_cflags:
        conf.env.append_value('CCDEFINES', d + '=' + str(v))

def hlist_to_string(conf, headers=None):
    '''convert a headers list to a set of #include lines'''
    hdrs=''
    hlist = conf.env.hlist
    if headers:
        hlist = hlist[:]
        hlist.extend(TO_LIST(headers))
    for h in hlist:
        hdrs += '#include <%s>\n' % h
    return hdrs


@conf
def COMPOUND_START(conf, msg):
    '''start a compound test'''
    def null_check_message_1(self,*k,**kw):
        return
    def null_check_message_2(self,*k,**kw):
        return

    v = getattr(conf.env, 'in_compound', [])
    if v != [] and v != 0:
        conf.env.in_compound = v + 1
        return
    conf.check_message_1(msg)
    conf.saved_check_message_1 = conf.check_message_1
    conf.check_message_1 = null_check_message_1
    conf.saved_check_message_2 = conf.check_message_2
    conf.check_message_2 = null_check_message_2
    conf.env.in_compound = 1


@conf
def COMPOUND_END(conf, result):
    '''start a compound test'''
    conf.env.in_compound -= 1
    if conf.env.in_compound != 0:
        return
    conf.check_message_1 = conf.saved_check_message_1
    conf.check_message_2 = conf.saved_check_message_2
    p = conf.check_message_2
    if result == True:
        p('ok ')
    elif result == False:
        p('not found', 'YELLOW')
    else:
        p(result)


@feature('nolink')
def nolink(self):
    '''using the nolink type in conf.check() allows us to avoid
       the link stage of a test, thus speeding it up for tests
       that where linking is not needed'''
    pass


def CHECK_HEADER(conf, h, add_headers=False, lib=None):
    '''check for a header'''
    if h in missing_headers and lib is None:
        return False
    d = h.upper().replace('/', '_')
    d = d.replace('.', '_')
    d = d.replace('-', '_')
    d = 'HAVE_%s' % d
    if CONFIG_SET(conf, d):
        if add_headers:
            if not h in conf.env.hlist:
                conf.env.hlist.append(h)
        return True

    (ccflags, ldflags) = library_flags(conf, lib)

    hdrs = hlist_to_string(conf, headers=h)
    ret = conf.check(fragment='%s\nint main(void) { return 0; }' % hdrs,
                     type='nolink',
                     execute=0,
                     ccflags=ccflags,
                     msg="Checking for header %s" % h)
    if not ret:
        missing_headers.add(h)
        return False

    conf.DEFINE(d, 1)
    if add_headers and not h in conf.env.hlist:
        conf.env.hlist.append(h)
    return ret


@conf
def CHECK_HEADERS(conf, headers, add_headers=False, together=False, lib=None):
    '''check for a list of headers

    when together==True, then the headers accumulate within this test.
    This is useful for interdependent headers
    '''
    ret = True
    if not add_headers and together:
        saved_hlist = conf.env.hlist[:]
        set_add_headers = True
    else:
        set_add_headers = add_headers
    for hdr in TO_LIST(headers):
        if not CHECK_HEADER(conf, hdr, set_add_headers, lib=lib):
            ret = False
    if not add_headers and together:
        conf.env.hlist = saved_hlist
    return ret


def header_list(conf, headers=None, lib=None):
    '''form a list of headers which exist, as a string'''
    hlist=[]
    if headers is not None:
        for h in TO_LIST(headers):
            if CHECK_HEADER(conf, h, add_headers=False, lib=lib):
                hlist.append(h)
    return hlist_to_string(conf, headers=hlist)


@conf
def CHECK_TYPE(conf, t, alternate=None, headers=None, define=None, lib=None, msg=None):
    '''check for a single type'''
    if define is None:
        define = 'HAVE_' + t.upper().replace(' ', '_')
    if msg is None:
        msg='Checking for %s' % t
    ret = CHECK_CODE(conf, '%s _x' % t,
                     define,
                     execute=False,
                     headers=headers,
                     local_include=False,
                     msg=msg,
                     lib=lib,
                     link=False)
    if not ret and alternate:
        conf.DEFINE(t, alternate)
    return ret


@conf
def CHECK_TYPES(conf, list, headers=None, define=None, alternate=None, lib=None):
    '''check for a list of types'''
    ret = True
    for t in TO_LIST(list):
        if not CHECK_TYPE(conf, t, headers=headers,
                          define=define, alternate=alternate, lib=lib):
            ret = False
    return ret


@conf
def CHECK_TYPE_IN(conf, t, headers=None, alternate=None, define=None):
    '''check for a single type with a header'''
    return CHECK_TYPE(conf, t, headers=headers, alternate=alternate, define=define)


@conf
def CHECK_VARIABLE(conf, v, define=None, always=False,
                   headers=None, msg=None, lib=None):
    '''check for a variable declaration (or define)'''
    if define is None:
        define = 'HAVE_%s' % v.upper()

    if msg is None:
        msg="Checking for variable %s" % v

    return CHECK_CODE(conf,
                      # we need to make sure the compiler doesn't
                      # optimize it out...
                      '''
                      #ifndef %s
                      void *_x; _x=(void *)&%s; return (int)_x;
                      #endif
                      return 0
                      ''' % (v, v),
                      execute=False,
                      link=False,
                      msg=msg,
                      local_include=False,
                      lib=lib,
                      headers=headers,
                      define=define,
                      always=always)


@conf
def CHECK_DECLS(conf, vars, reverse=False, headers=None, always=False):
    '''check a list of variable declarations, using the HAVE_DECL_xxx form
       of define

       When reverse==True then use HAVE_xxx_DECL instead of HAVE_DECL_xxx
       '''
    ret = True
    for v in TO_LIST(vars):
        if not reverse:
            define='HAVE_DECL_%s' % v.upper()
        else:
            define='HAVE_%s_DECL' % v.upper()
        if not CHECK_VARIABLE(conf, v,
                              define=define,
                              headers=headers,
                              msg='Checking for declaration of %s' % v,
                              always=always):
            ret = False
    return ret


def CHECK_FUNC(conf, f, link=True, lib=None, headers=None):
    '''check for a function'''
    define='HAVE_%s' % f.upper()

    ret = False

    conf.COMPOUND_START('Checking for %s' % f)

    if link is None or link == True:
        ret = CHECK_CODE(conf,
                         # this is based on the autoconf strategy
                         '''
                         #define %s __fake__%s
                         #ifdef HAVE_LIMITS_H
                         # include <limits.h>
                         #else
                         # include <assert.h>
                         #endif
                         #undef %s
                         #if defined __stub_%s || defined __stub___%s
                         #error "bad glibc stub"
                         #endif
                         extern char %s();
                         int main() { return %s(); }
                         ''' % (f, f, f, f, f, f, f),
                         execute=False,
                         link=True,
                         addmain=False,
                         add_headers=False,
                         define=define,
                         local_include=False,
                         lib=lib,
                         headers=headers,
                         msg='Checking for %s' % f)

        if not ret:
            ret = CHECK_CODE(conf,
                             # it might be a macro
                             # we need to make sure the compiler doesn't
                             # optimize it out...
                             'void *__x = (void *)%s; return (int)__x' % f,
                             execute=False,
                             link=True,
                             addmain=True,
                             add_headers=True,
                             define=define,
                             local_include=False,
                             lib=lib,
                             headers=headers,
                             msg='Checking for macro %s' % f)

    if not ret and (link is None or link == False):
        ret = CHECK_VARIABLE(conf, f,
                             define=define,
                             headers=headers,
                             msg='Checking for declaration of %s' % f)
    conf.COMPOUND_END(ret)
    return ret


@conf
def CHECK_FUNCS(conf, list, link=True, lib=None, headers=None):
    '''check for a list of functions'''
    ret = True
    for f in TO_LIST(list):
        if not CHECK_FUNC(conf, f, link=link, lib=lib, headers=headers):
            ret = False
    return ret


@conf
def CHECK_SIZEOF(conf, vars, headers=None, define=None):
    '''check the size of a type'''
    ret = True
    for v in TO_LIST(vars):
        v_define = define
        if v_define is None:
            v_define = 'SIZEOF_%s' % v.upper().replace(' ', '_')
        if not CHECK_CODE(conf,
                          'printf("%%u", (unsigned)sizeof(%s))' % v,
                          define=v_define,
                          execute=True,
                          define_ret=True,
                          quote=False,
                          headers=headers,
                          local_include=False,
                          msg="Checking size of %s" % v):
            ret = False
    return ret



@conf
def CHECK_CODE(conf, code, define,
               always=False, execute=False, addmain=True,
               add_headers=True, mandatory=False,
               headers=None, msg=None, cflags='', includes='# .',
               local_include=True, lib=None, link=True,
               define_ret=False, quote=False,
               on_target=True):
    '''check if some code compiles and/or runs'''

    if CONFIG_SET(conf, define):
        return True

    if headers is not None:
        CHECK_HEADERS(conf, headers=headers, lib=lib)

    if add_headers:
        hdrs = header_list(conf, headers=headers, lib=lib)
    else:
        hdrs = ''
    if execute:
        execute = 1
    else:
        execute = 0

    defs = conf.get_config_header()

    if addmain:
        fragment='%s\n%s\n int main(void) { %s; return 0; }\n' % (defs, hdrs, code)
    else:
        fragment='%s\n%s\n%s\n' % (defs, hdrs, code)

    if msg is None:
        msg="Checking for %s" % define

    cflags = TO_LIST(cflags)

    if local_include:
        cflags.append('-I%s' % conf.curdir)

    if not link:
        type='nolink'
    else:
        type='cprogram'

    uselib = TO_LIST(lib)

    (ccflags, ldflags) = library_flags(conf, uselib)

    cflags.extend(ccflags)

    if on_target:
        exec_args = conf.SAMBA_CROSS_ARGS(msg=msg)
    else:
        exec_args = []

    conf.COMPOUND_START(msg)

    ret = conf.check(fragment=fragment,
                     execute=execute,
                     define_name = define,
                     mandatory = mandatory,
                     ccflags=cflags,
                     ldflags=ldflags,
                     includes=includes,
                     uselib=uselib,
                     type=type,
                     msg=msg,
                     quote=quote,
                     exec_args=exec_args,
                     define_ret=define_ret)
    if not ret and CONFIG_SET(conf, define):
        # sometimes conf.check() returns false, but it
        # sets the define. Maybe a waf bug?
        ret = True
    if ret:
        if not define_ret:
            conf.DEFINE(define, 1)
            conf.COMPOUND_END(True)
        else:
            conf.COMPOUND_END(conf.env[define])
        return True
    if always:
        conf.DEFINE(define, 0)
    conf.COMPOUND_END(False)
    return False



@conf
def CHECK_STRUCTURE_MEMBER(conf, structname, member,
                           always=False, define=None, headers=None):
    '''check for a structure member'''
    if define is None:
        define = 'HAVE_%s' % member.upper()
    return CHECK_CODE(conf,
                      '%s s; void *_x; _x=(void *)&s.%s' % (structname, member),
                      define,
                      execute=False,
                      link=False,
                      always=always,
                      headers=headers,
                      local_include=False,
                      msg="Checking for member %s in %s" % (member, structname))


@conf
def CHECK_CFLAGS(conf, cflags):
    '''check if the given cflags are accepted by the compiler
    '''
    return conf.check(fragment='int main(void) { return 0; }\n',
                      execute=0,
                      type='nolink',
                      ccflags=cflags,
                      msg="Checking compiler accepts %s" % cflags)

@conf
def CHECK_LDFLAGS(conf, ldflags):
    '''check if the given ldflags are accepted by the linker
    '''
    return conf.check(fragment='int main(void) { return 0; }\n',
                      execute=0,
                      ldflags=ldflags,
                      msg="Checking linker accepts %s" % ldflags)


@conf
def CONFIG_GET(conf, option):
    '''return True if a configuration option was found'''
    if (option in conf.env):
        return conf.env[option]
    else:
        return None

@conf
def CONFIG_SET(conf, option):
    '''return True if a configuration option was found'''
    return (option in conf.env) and (conf.env[option] != ())
Build.BuildContext.CONFIG_SET = CONFIG_SET
Build.BuildContext.CONFIG_GET = CONFIG_GET


def library_flags(self, libs):
    '''work out flags from pkg_config'''
    ccflags = []
    ldflags = []
    for lib in TO_LIST(libs):
        inc_path = getattr(self.env, 'CPPPATH_%s' % lib.upper(), [])
        lib_path = getattr(self.env, 'LIBPATH_%s' % lib.upper(), [])
        ccflags.extend(['-I%s' % i for i in inc_path])
        ldflags.extend(['-L%s' % l for l in lib_path])
        extra_ccflags = TO_LIST(getattr(self.env, 'CCFLAGS_%s' % lib.upper(), []))
        extra_ldflags = TO_LIST(getattr(self.env, 'LDFLAGS_%s' % lib.upper(), []))
        ccflags.extend(extra_ccflags)
        ldflags.extend(extra_ldflags)
    if 'EXTRA_LDFLAGS' in self.env:
        ldflags.extend(self.env['EXTRA_LDFLAGS'])

    ccflags = unique_list(ccflags)
    ldflags = unique_list(ldflags)
    return (ccflags, ldflags)


@conf
def CHECK_LIB(conf, libs, mandatory=False, empty_decl=True, set_target=True, shlib=False):
    '''check if a set of libraries exist as system libraries

    returns the sublist of libs that do exist as a syslib or []
    '''

    fragment= '''
int foo()
{
    int v = 2;
    return v*2;
}
'''
    ret = []
    liblist  = TO_LIST(libs)
    for lib in liblist[:]:
        if GET_TARGET_TYPE(conf, lib) == 'SYSLIB':
            ret.append(lib)
            continue

        (ccflags, ldflags) = library_flags(conf, lib)
        if shlib:
            res = conf.check(features='cc cshlib', fragment=fragment, lib=lib, uselib_store=lib, ccflags=ccflags, ldflags=ldflags)
        else:
            res = conf.check(lib=lib, uselib_store=lib, ccflags=ccflags, ldflags=ldflags)

        if not res:
            if mandatory:
                Logs.error("Mandatory library '%s' not found for functions '%s'" % (lib, list))
                sys.exit(1)
            if empty_decl:
                # if it isn't a mandatory library, then remove it from dependency lists
                if set_target:
                    SET_TARGET_TYPE(conf, lib, 'EMPTY')
        else:
            conf.define('HAVE_LIB%s' % lib.upper().replace('-','_'), 1)
            conf.env['LIB_' + lib.upper()] = lib
            if set_target:
                conf.SET_TARGET_TYPE(lib, 'SYSLIB')
            ret.append(lib)

    return ret



@conf
def CHECK_FUNCS_IN(conf, list, library, mandatory=False, checklibc=False,
                   headers=None, link=True, empty_decl=True, set_target=True):
    """
    check that the functions in 'list' are available in 'library'
    if they are, then make that library available as a dependency

    if the library is not available and mandatory==True, then
    raise an error.

    If the library is not available and mandatory==False, then
    add the library to the list of dependencies to remove from
    build rules

    optionally check for the functions first in libc
    """
    remaining = TO_LIST(list)
    liblist   = TO_LIST(library)

    # check if some already found
    for f in remaining[:]:
        if CONFIG_SET(conf, 'HAVE_%s' % f.upper()):
            remaining.remove(f)

    # see if the functions are in libc
    if checklibc:
        for f in remaining[:]:
            if CHECK_FUNC(conf, f, link=True, headers=headers):
                remaining.remove(f)

    if remaining == []:
        for lib in liblist:
            if GET_TARGET_TYPE(conf, lib) != 'SYSLIB' and empty_decl:
                SET_TARGET_TYPE(conf, lib, 'EMPTY')
        return True

    checklist = conf.CHECK_LIB(liblist, empty_decl=empty_decl, set_target=set_target)
    for lib in liblist[:]:
        if not lib in checklist and mandatory:
            Logs.error("Mandatory library '%s' not found for functions '%s'" % (lib, list))
            sys.exit(1)

    ret = True
    for f in remaining:
        if not CHECK_FUNC(conf, f, lib=' '.join(checklist), headers=headers, link=link):
            ret = False

    return ret


@conf
def IN_LAUNCH_DIR(conf):
    '''return True if this rule is being run from the launch directory'''
    return os.path.realpath(conf.curdir) == os.path.realpath(Options.launch_dir)
Options.Handler.IN_LAUNCH_DIR = IN_LAUNCH_DIR


@conf
def SAMBA_CONFIG_H(conf, path=None):
    '''write out config.h in the right directory'''
    # we don't want to produce a config.h in places like lib/replace
    # when we are building projects that depend on lib/replace
    if not IN_LAUNCH_DIR(conf):
        return

    if Options.options.developer:
        # we add these here to ensure that -Wstrict-prototypes is not set during configure
        conf.ADD_CFLAGS('-Wall -g -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Werror-implicit-function-declaration -Wformat=2 -Wno-format-y2k -Wmissing-prototypes',
                        testflags=True)
        if os.getenv('TOPLEVEL_BUILD'):
            conf.ADD_CFLAGS('-Wcast-qual', testflags=True)
        conf.env.DEVELOPER_MODE = True

    if Options.options.picky_developer:
        conf.ADD_CFLAGS('-Werror', testflags=True)

    if Options.options.fatal_errors:
        conf.ADD_CFLAGS('-Wfatal-errors', testflags=True)

    if Options.options.pedantic:
        conf.ADD_CFLAGS('-W', testflags=True)

    if path is None:
        conf.write_config_header('config.h', top=True)
    else:
        conf.write_config_header(path)
    conf.SAMBA_CROSS_CHECK_COMPLETE()


@conf
def CONFIG_PATH(conf, name, default):
    '''setup a configurable path'''
    if not name in conf.env:
        if default[0] == '/':
            conf.env[name] = default
        else:
            conf.env[name] = conf.env['PREFIX'] + default

@conf
def ADD_CFLAGS(conf, flags, testflags=False):
    '''add some CFLAGS to the command line
       optionally set testflags to ensure all the flags work
    '''
    if testflags:
        ok_flags=[]
        for f in flags.split():
            if CHECK_CFLAGS(conf, f):
                ok_flags.append(f)
        flags = ok_flags
    if not 'EXTRA_CFLAGS' in conf.env:
        conf.env['EXTRA_CFLAGS'] = []
    conf.env['EXTRA_CFLAGS'].extend(TO_LIST(flags))

@conf
def ADD_LDFLAGS(conf, flags, testflags=False):
    '''add some LDFLAGS to the command line
       optionally set testflags to ensure all the flags work

       this will return the flags that are added, if any
    '''
    if testflags:
        ok_flags=[]
        for f in flags.split():
            if CHECK_LDFLAGS(conf, f):
                ok_flags.append(f)
        flags = ok_flags
    if not 'EXTRA_LDFLAGS' in conf.env:
        conf.env['EXTRA_LDFLAGS'] = []
    conf.env['EXTRA_LDFLAGS'].extend(TO_LIST(flags))
    return flags


@conf
def ADD_EXTRA_INCLUDES(conf, includes):
    '''add some extra include directories to all builds'''
    if not 'EXTRA_INCLUDES' in conf.env:
        conf.env['EXTRA_INCLUDES'] = []
    conf.env['EXTRA_INCLUDES'].extend(TO_LIST(includes))



def CURRENT_CFLAGS(bld, target, cflags, hide_symbols=False):
    '''work out the current flags. local flags are added first'''
    if not 'EXTRA_CFLAGS' in bld.env:
        list = []
    else:
        list = bld.env['EXTRA_CFLAGS'];
    ret = TO_LIST(cflags)
    ret.extend(list)
    if hide_symbols and bld.env.HAVE_VISIBILITY_ATTR:
        ret.append('-fvisibility=hidden')
    return ret


@conf
def CHECK_CC_ENV(conf):
    """trim whitespaces from 'CC'.
    The build farm sometimes puts a space at the start"""
    if os.environ.get('CC'):
        conf.env.CC = TO_LIST(os.environ.get('CC'))
        if len(conf.env.CC) == 1:
            # make for nicer logs if just a single command
            conf.env.CC = conf.env.CC[0]


@conf
def SETUP_CONFIGURE_CACHE(conf, enable):
    '''enable/disable cache of configure results'''
    if enable:
        # when -C is chosen, we will use a private cache and will
        # not look into system includes. This roughtly matches what
        # autoconf does with -C
        cache_path = os.path.join(conf.blddir, '.confcache')
        mkdir_p(cache_path)
        Options.cache_global = os.environ['WAFCACHE'] = cache_path
    else:
        # when -C is not chosen we will not cache configure checks
        # We set the recursion limit low to prevent waf from spending
        # a lot of time on the signatures of the files.
        Options.cache_global = os.environ['WAFCACHE'] = ''
        preproc.recursion_limit = 1
    # in either case we don't need to scan system includes
    preproc.go_absolute = False
