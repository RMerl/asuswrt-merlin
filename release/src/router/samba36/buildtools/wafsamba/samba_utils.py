# a waf tool to add autoconf-like macros to the configure section
# and for SAMBA_ macros for building libraries, binaries etc

import Build, os, sys, Options, Utils, Task, re, fnmatch, Logs
from TaskGen import feature, before
from Configure import conf
from Logs import debug
import shlex

# TODO: make this a --option
LIB_PATH="shared"


# sigh, python octal constants are a mess
MODE_644 = int('644', 8)
MODE_755 = int('755', 8)

@conf
def SET_TARGET_TYPE(ctx, target, value):
    '''set the target type of a target'''
    cache = LOCAL_CACHE(ctx, 'TARGET_TYPE')
    if target in cache and cache[target] != 'EMPTY':
        Logs.error("ERROR: Target '%s' in directory %s re-defined as %s - was %s" % (target, ctx.curdir, value, cache[target]))
        sys.exit(1)
    LOCAL_CACHE_SET(ctx, 'TARGET_TYPE', target, value)
    debug("task_gen: Target '%s' created of type '%s' in %s" % (target, value, ctx.curdir))
    return True


def GET_TARGET_TYPE(ctx, target):
    '''get target type from cache'''
    cache = LOCAL_CACHE(ctx, 'TARGET_TYPE')
    if not target in cache:
        return None
    return cache[target]


######################################################
# this is used as a decorator to make functions only
# run once. Based on the idea from
# http://stackoverflow.com/questions/815110/is-there-a-decorator-to-simply-cache-function-return-values
runonce_ret = {}
def runonce(function):
    def runonce_wrapper(*args):
        if args in runonce_ret:
            return runonce_ret[args]
        else:
            ret = function(*args)
            runonce_ret[args] = ret
            return ret
    return runonce_wrapper


def ADD_LD_LIBRARY_PATH(path):
    '''add something to LD_LIBRARY_PATH'''
    if 'LD_LIBRARY_PATH' in os.environ:
        oldpath = os.environ['LD_LIBRARY_PATH']
    else:
        oldpath = ''
    newpath = oldpath.split(':')
    if not path in newpath:
        newpath.append(path)
        os.environ['LD_LIBRARY_PATH'] = ':'.join(newpath)


def needs_private_lib(bld, target):
    '''return True if a target links to a private library'''
    for lib in getattr(target, "uselib_local", []):
        t = bld.name_to_obj(lib, bld.env)
        if t and getattr(t, 'private_library', False):
            return True
    return False


def install_rpath(target):
    '''the rpath value for installation'''
    bld = target.bld
    bld.env['RPATH'] = []
    ret = set()
    if bld.env.RPATH_ON_INSTALL:
        ret.add(bld.EXPAND_VARIABLES(bld.env.LIBDIR))
    if bld.env.RPATH_ON_INSTALL_PRIVATE and needs_private_lib(bld, target):
        ret.add(bld.EXPAND_VARIABLES(bld.env.PRIVATELIBDIR))
    return list(ret)


def build_rpath(bld):
    '''the rpath value for build'''
    rpaths = [os.path.normpath('%s/%s' % (bld.env.BUILD_DIRECTORY, d)) for d in ("shared", "shared/private")]
    bld.env['RPATH'] = []
    if bld.env.RPATH_ON_BUILD:
        return rpaths
    for rpath in rpaths:
        ADD_LD_LIBRARY_PATH(rpath)
    return []


@conf
def LOCAL_CACHE(ctx, name):
    '''return a named build cache dictionary, used to store
       state inside other functions'''
    if name in ctx.env:
        return ctx.env[name]
    ctx.env[name] = {}
    return ctx.env[name]


@conf
def LOCAL_CACHE_SET(ctx, cachename, key, value):
    '''set a value in a local cache'''
    cache = LOCAL_CACHE(ctx, cachename)
    cache[key] = value


@conf
def ASSERT(ctx, expression, msg):
    '''a build assert call'''
    if not expression:
        raise Utils.WafError("ERROR: %s\n" % msg)
Build.BuildContext.ASSERT = ASSERT


def SUBDIR(bld, subdir, list):
    '''create a list of files by pre-pending each with a subdir name'''
    ret = ''
    for l in TO_LIST(list):
        ret = ret + os.path.normpath(os.path.join(subdir, l)) + ' '
    return ret
Build.BuildContext.SUBDIR = SUBDIR


def dict_concat(d1, d2):
    '''concatenate two dictionaries d1 += d2'''
    for t in d2:
        if t not in d1:
            d1[t] = d2[t]


def exec_command(self, cmd, **kw):
    '''this overrides the 'waf -v' debug output to be in a nice
    unix like format instead of a python list.
    Thanks to ita on #waf for this'''
    import Utils, Logs
    _cmd = cmd
    if isinstance(cmd, list):
        _cmd = ' '.join(cmd)
    debug('runner: %s' % _cmd)
    if self.log:
        self.log.write('%s\n' % cmd)
        kw['log'] = self.log
    try:
        if not kw.get('cwd', None):
            kw['cwd'] = self.cwd
    except AttributeError:
        self.cwd = kw['cwd'] = self.bldnode.abspath()
    return Utils.exec_command(cmd, **kw)
Build.BuildContext.exec_command = exec_command


def ADD_COMMAND(opt, name, function):
    '''add a new top level command to waf'''
    Utils.g_module.__dict__[name] = function
    opt.name = function
Options.Handler.ADD_COMMAND = ADD_COMMAND


@feature('cc', 'cshlib', 'cprogram')
@before('apply_core','exec_rule')
def process_depends_on(self):
    '''The new depends_on attribute for build rules
       allow us to specify a dependency on output from
       a source generation rule'''
    if getattr(self , 'depends_on', None):
        lst = self.to_list(self.depends_on)
        for x in lst:
            y = self.bld.name_to_obj(x, self.env)
            self.bld.ASSERT(y is not None, "Failed to find dependency %s of %s" % (x, self.name))
            y.post()
            if getattr(y, 'more_includes', None):
                  self.includes += " " + y.more_includes


os_path_relpath = getattr(os.path, 'relpath', None)
if os_path_relpath is None:
    # Python < 2.6 does not have os.path.relpath, provide a replacement
    # (imported from Python2.6.5~rc2)
    def os_path_relpath(path, start):
        """Return a relative version of a path"""
        start_list = os.path.abspath(start).split("/")
        path_list = os.path.abspath(path).split("/")

        # Work out how much of the filepath is shared by start and path.
        i = len(os.path.commonprefix([start_list, path_list]))

        rel_list = ['..'] * (len(start_list)-i) + path_list[i:]
        if not rel_list:
            return start
        return os.path.join(*rel_list)


def unique_list(seq):
    '''return a uniquified list in the same order as the existing list'''
    seen = {}
    result = []
    for item in seq:
        if item in seen: continue
        seen[item] = True
        result.append(item)
    return result


def TO_LIST(str, delimiter=None):
    '''Split a list, preserving quoted strings and existing lists'''
    if str is None:
        return []
    if isinstance(str, list):
        return str
    lst = str.split(delimiter)
    # the string may have had quotes in it, now we
    # check if we did have quotes, and use the slower shlex
    # if we need to
    for e in lst:
        if e[0] == '"':
            return shlex.split(str)
    return lst


def subst_vars_error(string, env):
    '''substitute vars, throw an error if a variable is not defined'''
    lst = re.split('(\$\{\w+\})', string)
    out = []
    for v in lst:
        if re.match('\$\{\w+\}', v):
            vname = v[2:-1]
            if not vname in env:
                Logs.error("Failed to find variable %s in %s" % (vname, string))
                sys.exit(1)
            v = env[vname]
        out.append(v)
    return ''.join(out)


@conf
def SUBST_ENV_VAR(ctx, varname):
    '''Substitute an environment variable for any embedded variables'''
    return subst_vars_error(ctx.env[varname], ctx.env)
Build.BuildContext.SUBST_ENV_VAR = SUBST_ENV_VAR


def ENFORCE_GROUP_ORDERING(bld):
    '''enforce group ordering for the project. This
       makes the group ordering apply only when you specify
       a target with --target'''
    if Options.options.compile_targets:
        @feature('*')
        @before('exec_rule', 'apply_core', 'collect')
        def force_previous_groups(self):
            if getattr(self.bld, 'enforced_group_ordering', False) == True:
                return
            self.bld.enforced_group_ordering = True

            def group_name(g):
                tm = self.bld.task_manager
                return [x for x in tm.groups_names if id(tm.groups_names[x]) == id(g)][0]

            my_id = id(self)
            bld = self.bld
            stop = None
            for g in bld.task_manager.groups:
                for t in g.tasks_gen:
                    if id(t) == my_id:
                        stop = id(g)
                        debug('group: Forcing up to group %s for target %s',
                              group_name(g), self.name or self.target)
                        break
                if stop != None:
                    break
            if stop is None:
                return

            for i in xrange(len(bld.task_manager.groups)):
                g = bld.task_manager.groups[i]
                bld.task_manager.current_group = i
                if id(g) == stop:
                    break
                debug('group: Forcing group %s', group_name(g))
                for t in g.tasks_gen:
                    if not getattr(t, 'forced_groups', False):
                        debug('group: Posting %s', t.name or t.target)
                        t.forced_groups = True
                        t.post()
Build.BuildContext.ENFORCE_GROUP_ORDERING = ENFORCE_GROUP_ORDERING


def recursive_dirlist(dir, relbase, pattern=None):
    '''recursive directory list'''
    ret = []
    for f in os.listdir(dir):
        f2 = dir + '/' + f
        if os.path.isdir(f2):
            ret.extend(recursive_dirlist(f2, relbase))
        else:
            if pattern and not fnmatch.fnmatch(f, pattern):
                continue
            ret.append(os_path_relpath(f2, relbase))
    return ret


def mkdir_p(dir):
    '''like mkdir -p'''
    if not dir:
        return
    if dir.endswith("/"):
        mkdir_p(dir[:-1])
        return
    if os.path.isdir(dir):
        return
    mkdir_p(os.path.dirname(dir))
    os.mkdir(dir)


def SUBST_VARS_RECURSIVE(string, env):
    '''recursively expand variables'''
    if string is None:
        return string
    limit=100
    while (string.find('${') != -1 and limit > 0):
        string = subst_vars_error(string, env)
        limit -= 1
    return string


@conf
def EXPAND_VARIABLES(ctx, varstr, vars=None):
    '''expand variables from a user supplied dictionary

    This is most useful when you pass vars=locals() to expand
    all your local variables in strings
    '''

    if isinstance(varstr, list):
        ret = []
        for s in varstr:
            ret.append(EXPAND_VARIABLES(ctx, s, vars=vars))
        return ret

    if not isinstance(varstr, str):
        return varstr

    import Environment
    env = Environment.Environment()
    ret = varstr
    # substitute on user supplied dict if avaiilable
    if vars is not None:
        for v in vars.keys():
            env[v] = vars[v]
        ret = SUBST_VARS_RECURSIVE(ret, env)

    # if anything left, subst on the environment as well
    if ret.find('${') != -1:
        ret = SUBST_VARS_RECURSIVE(ret, ctx.env)
    # make sure there is nothing left. Also check for the common
    # typo of $( instead of ${
    if ret.find('${') != -1 or ret.find('$(') != -1:
        Logs.error('Failed to substitute all variables in varstr=%s' % ret)
        sys.exit(1)
    return ret
Build.BuildContext.EXPAND_VARIABLES = EXPAND_VARIABLES


def RUN_COMMAND(cmd,
                env=None,
                shell=False):
    '''run a external command, return exit code or signal'''
    if env:
        cmd = SUBST_VARS_RECURSIVE(cmd, env)

    status = os.system(cmd)
    if os.WIFEXITED(status):
        return os.WEXITSTATUS(status)
    if os.WIFSIGNALED(status):
        return - os.WTERMSIG(status)
    Logs.error("Unknown exit reason %d for command: %s" (status, cmd))
    return -1


# make sure we have md5. some systems don't have it
try:
    from hashlib import md5
except:
    try:
        import md5
    except:
        import Constants
        Constants.SIG_NIL = hash('abcd')
        class replace_md5(object):
            def __init__(self):
                self.val = None
            def update(self, val):
                self.val = hash((self.val, val))
            def digest(self):
                return str(self.val)
            def hexdigest(self):
                return self.digest().encode('hex')
        def replace_h_file(filename):
            f = open(filename, 'rb')
            m = replace_md5()
            while (filename):
                filename = f.read(100000)
                m.update(filename)
            f.close()
            return m.digest()
        Utils.md5 = replace_md5
        Task.md5 = replace_md5
        Utils.h_file = replace_h_file


def LOAD_ENVIRONMENT():
    '''load the configuration environment, allowing access to env vars
       from new commands'''
    import Environment
    env = Environment.Environment()
    try:
        env.load('.lock-wscript')
        env.load(env.blddir + '/c4che/default.cache.py')
    except:
        pass
    return env


def IS_NEWER(bld, file1, file2):
    '''return True if file1 is newer than file2'''
    t1 = os.stat(os.path.join(bld.curdir, file1)).st_mtime
    t2 = os.stat(os.path.join(bld.curdir, file2)).st_mtime
    return t1 > t2
Build.BuildContext.IS_NEWER = IS_NEWER


@conf
def RECURSE(ctx, directory):
    '''recurse into a directory, relative to the curdir or top level'''
    try:
        visited_dirs = ctx.visited_dirs
    except:
        visited_dirs = ctx.visited_dirs = set()
    d = os.path.join(ctx.curdir, directory)
    if os.path.exists(d):
        abspath = os.path.abspath(d)
    else:
        abspath = os.path.abspath(os.path.join(Utils.g_module.srcdir, directory))
    ctxclass = ctx.__class__.__name__
    key = ctxclass + ':' + abspath
    if key in visited_dirs:
        # already done it
        return
    visited_dirs.add(key)
    relpath = os_path_relpath(abspath, ctx.curdir)
    if ctxclass == 'Handler':
        return ctx.sub_options(relpath)
    if ctxclass == 'ConfigurationContext':
        return ctx.sub_config(relpath)
    if ctxclass == 'BuildContext':
        return ctx.add_subdirs(relpath)
    Logs.error('Unknown RECURSE context class', ctxclass)
    raise
Options.Handler.RECURSE = RECURSE
Build.BuildContext.RECURSE = RECURSE


def CHECK_MAKEFLAGS(bld):
    '''check for MAKEFLAGS environment variable in case we are being
    called from a Makefile try to honor a few make command line flags'''
    if not 'WAF_MAKE' in os.environ:
        return
    makeflags = os.environ.get('MAKEFLAGS')
    if makeflags is None:
        return
    jobs_set = False
    # we need to use shlex.split to cope with the escaping of spaces
    # in makeflags
    for opt in shlex.split(makeflags):
        # options can come either as -x or as x
        if opt[0:2] == 'V=':
            Options.options.verbose = Logs.verbose = int(opt[2:])
            if Logs.verbose > 0:
                Logs.zones = ['runner']
            if Logs.verbose > 2:
                Logs.zones = ['*']
        elif opt[0].isupper() and opt.find('=') != -1:
            loc = opt.find('=')
            setattr(Options.options, opt[0:loc], opt[loc+1:])
        elif opt[0] != '-':
            for v in opt:
                if v == 'j':
                    jobs_set = True
                elif v == 'k':
                    Options.options.keep = True                
        elif opt == '-j':
            jobs_set = True
        elif opt == '-k':
            Options.options.keep = True                
    if not jobs_set:
        # default to one job
        Options.options.jobs = 1
            
Build.BuildContext.CHECK_MAKEFLAGS = CHECK_MAKEFLAGS

option_groups = {}

def option_group(opt, name):
    '''find or create an option group'''
    global option_groups
    if name in option_groups:
        return option_groups[name]
    gr = opt.add_option_group(name)
    option_groups[name] = gr
    return gr
Options.Handler.option_group = option_group


def save_file(filename, contents, create_dir=False):
    '''save data to a file'''
    if create_dir:
        mkdir_p(os.path.dirname(filename))
    try:
        f = open(filename, 'w')
        f.write(contents)
        f.close()
    except:
        return False
    return True


def load_file(filename):
    '''return contents of a file'''
    try:
        f = open(filename, 'r')
        r = f.read()
        f.close()
    except:
        return None
    return r


def reconfigure(ctx):
    '''rerun configure if necessary'''
    import Configure, samba_wildcard, Scripting
    if not os.path.exists(".lock-wscript"):
        raise Utils.WafError('configure has not been run')
    bld = samba_wildcard.fake_build_environment()
    Configure.autoconfig = True
    Scripting.check_configured(bld)


def map_shlib_extension(ctx, name, python=False):
    '''map a filename with a shared library extension of .so to the real shlib name'''
    if name is None:
        return None
    if name[-1:].isdigit():
        # some libraries have specified versions in the wscript rule
        return name
    (root1, ext1) = os.path.splitext(name)
    if python:
        (root2, ext2) = os.path.splitext(ctx.env.pyext_PATTERN)
    else:
        (root2, ext2) = os.path.splitext(ctx.env.shlib_PATTERN)
    return root1+ext2
Build.BuildContext.map_shlib_extension = map_shlib_extension

def apply_pattern(filename, pattern):
    '''apply a filename pattern to a filename that may have a directory component'''
    dirname = os.path.dirname(filename)
    if not dirname:
        return pattern % filename
    basename = os.path.basename(filename)
    return os.path.join(dirname, pattern % basename)

def make_libname(ctx, name, nolibprefix=False, version=None, python=False):
    """make a library filename
         Options:
              nolibprefix: don't include the lib prefix
              version    : add a version number
              python     : if we should use python module name conventions"""

    if python:
        libname = apply_pattern(name, ctx.env.pyext_PATTERN)
    else:
        libname = apply_pattern(name, ctx.env.shlib_PATTERN)
    if nolibprefix and libname[0:3] == 'lib':
        libname = libname[3:]
    if version:
        if version[0] == '.':
            version = version[1:]
        (root, ext) = os.path.splitext(libname)
        if ext == ".dylib":
            # special case - version goes before the prefix
            libname = "%s.%s%s" % (root, version, ext)
        else:
            libname = "%s%s.%s" % (root, ext, version)
    return libname
Build.BuildContext.make_libname = make_libname


def get_tgt_list(bld):
    '''return a list of build objects for samba'''

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    # build a list of task generators we are interested in
    tgt_list = []
    for tgt in targets:
        type = targets[tgt]
        if not type in ['SUBSYSTEM', 'MODULE', 'BINARY', 'LIBRARY', 'ASN1', 'PYTHON']:
            continue
        t = bld.name_to_obj(tgt, bld.env)
        if t is None:
            Logs.error("Target %s of type %s has no task generator" % (tgt, type))
            sys.exit(1)
        tgt_list.append(t)
    return tgt_list
