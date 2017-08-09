# Samba automatic dependency handling and project rules

import Build, os, sys, re, Environment, Logs, time
from samba_utils import *
from samba_autoconf import *
from samba_bundled import BUILTIN_LIBRARY

@conf
def ADD_GLOBAL_DEPENDENCY(ctx, dep):
    '''add a dependency for all binaries and libraries'''
    if not 'GLOBAL_DEPENDENCIES' in ctx.env:
        ctx.env.GLOBAL_DEPENDENCIES = []
    ctx.env.GLOBAL_DEPENDENCIES.append(dep)


@conf
def BREAK_CIRCULAR_LIBRARY_DEPENDENCIES(ctx):
    '''indicate that circular dependencies between libraries should be broken.'''
    ctx.env.ALLOW_CIRCULAR_LIB_DEPENDENCIES = True


@conf
def SET_SYSLIB_DEPS(conf, target, deps):
    '''setup some implied dependencies for a SYSLIB'''
    cache = LOCAL_CACHE(conf, 'SYSLIB_DEPS')
    cache[target] = deps


def expand_subsystem_deps(bld):
    '''expand the reverse dependencies resulting from subsystem
       attributes of modules. This is walking over the complete list
       of declared subsystems, and expands the samba_deps_extended list for any
       module<->subsystem dependencies'''

    subsystem_list = LOCAL_CACHE(bld, 'INIT_FUNCTIONS')
    targets    = LOCAL_CACHE(bld, 'TARGET_TYPE')

    for subsystem_name in subsystem_list:
        bld.ASSERT(subsystem_name in targets, "Subsystem target %s not declared" % subsystem_name)
        type = targets[subsystem_name]
        if type == 'DISABLED' or type == 'EMPTY':
            continue

        # for example,
        #    subsystem_name = dcerpc_server (a subsystem)
        #    subsystem      = dcerpc_server (a subsystem object)
        #    module_name    = rpc_epmapper (a module within the dcerpc_server subsystem)
        #    module         = rpc_epmapper (a module object within the dcerpc_server subsystem)

        subsystem = bld.name_to_obj(subsystem_name, bld.env)
        bld.ASSERT(subsystem is not None, "Unable to find subsystem %s" % subsystem_name)
        for d in subsystem_list[subsystem_name]:
            module_name = d['TARGET']
            module_type = targets[module_name]
            if module_type in ['DISABLED', 'EMPTY']:
                continue
            bld.ASSERT(subsystem is not None,
                       "Subsystem target %s for %s (%s) not found" % (subsystem_name, module_name, module_type))
            if module_type in ['SUBSYSTEM']:
                # if a module is a plain object type (not a library) then the
                # subsystem it is part of needs to have it as a dependency, so targets
                # that depend on this subsystem get the modules of that subsystem
                subsystem.samba_deps_extended.append(module_name)
        subsystem.samba_deps_extended = unique_list(subsystem.samba_deps_extended)



def build_dependencies(self):
    '''This builds the dependency list for a target. It runs after all the targets are declared

    The reason this is not just done in the SAMBA_*() rules is that we have no way of knowing
    the full dependency list for a target until we have all of the targets declared.
    '''

    if self.samba_type in ['LIBRARY', 'BINARY', 'PYTHON']:
        self.uselib        = list(self.final_syslibs)
        self.uselib_local  = list(self.final_libs)
        self.add_objects   = list(self.final_objects)

        # extra link flags from pkg_config
        libs = self.final_syslibs.copy()

        (ccflags, ldflags) = library_flags(self, list(libs))
        new_ldflags        = getattr(self, 'samba_ldflags', [])[:]
        new_ldflags.extend(ldflags)
        self.ldflags       = new_ldflags

        if getattr(self, 'allow_undefined_symbols', False) and self.env.undefined_ldflags:
            for f in self.env.undefined_ldflags:
                self.ldflags.remove(f)

        debug('deps: computed dependencies for target %s: uselib=%s uselib_local=%s add_objects=%s',
              self.sname, self.uselib, self.uselib_local, self.add_objects)

    if self.samba_type in ['SUBSYSTEM']:
        # this is needed for the ccflags of libs that come from pkg_config
        self.uselib = list(self.final_syslibs)
        self.uselib.extend(list(self.direct_syslibs))
        for lib in self.final_libs:
            t = self.bld.name_to_obj(lib, self.bld.env)
            self.uselib.extend(list(t.final_syslibs))
        self.uselib = unique_list(self.uselib)

    if getattr(self, 'uselib', None):
        up_list = []
        for l in self.uselib:
           up_list.append(l.upper())
        self.uselib = up_list


def build_includes(self):
    '''This builds the right set of includes for a target.

    One tricky part of this is that the includes= attribute for a
    target needs to use paths which are relative to that targets
    declaration directory (which we can get at via t.path).

    The way this works is the includes list gets added as
    samba_includes in the main build task declaration. Then this
    function runs after all of the tasks are declared, and it
    processes the samba_includes attribute to produce a includes=
    attribute
    '''

    if getattr(self, 'samba_includes', None) is None:
        return

    bld = self.bld

    inc_deps = includes_objects(bld, self, set(), {})

    includes = []

    # maybe add local includes
    if getattr(self, 'local_include', True) == True and getattr(self, 'local_include_first', True):
        includes.append('.')

    includes.extend(self.samba_includes_extended)

    if 'EXTRA_INCLUDES' in bld.env and getattr(self, 'global_include', True):
        includes.extend(bld.env['EXTRA_INCLUDES'])

    includes.append('#')

    inc_set = set()
    inc_abs = []

    for d in inc_deps:
        t = bld.name_to_obj(d, bld.env)
        bld.ASSERT(t is not None, "Unable to find dependency %s for %s" % (d, self.sname))
        inclist = getattr(t, 'samba_includes_extended', [])[:]
        if getattr(t, 'local_include', True) == True:
            inclist.append('.')
        if inclist == []:
            continue
        tpath = t.samba_abspath
        for inc in inclist:
            npath = tpath + '/' + inc
            if not npath in inc_set:
                inc_abs.append(npath)
                inc_set.add(npath)

    mypath = self.path.abspath(bld.env)
    for inc in inc_abs:
        relpath = os_path_relpath(inc, mypath)
        includes.append(relpath)

    if getattr(self, 'local_include', True) == True and not getattr(self, 'local_include_first', True):
        includes.append('.')

    # now transform the includes list to be relative to the top directory
    # which is represented by '#' in waf. This allows waf to cache the
    # includes lists more efficiently
    includes_top = []
    for i in includes:
        if i[0] == '#':
            # some are already top based
            includes_top.append(i)
            continue
        absinc = os.path.join(self.path.abspath(), i)
        relinc = os_path_relpath(absinc, self.bld.srcnode.abspath())
        includes_top.append('#' + relinc)

    self.includes = unique_list(includes_top)
    debug('deps: includes for target %s: includes=%s',
          self.sname, self.includes)




def add_init_functions(self):
    '''This builds the right set of init functions'''

    bld = self.bld

    subsystems = LOCAL_CACHE(bld, 'INIT_FUNCTIONS')

    # cope with the separated object lists from BINARY and LIBRARY targets
    sname = self.sname
    if sname.endswith('.objlist'):
        sname = sname[0:-8]

    modules = []
    if sname in subsystems:
        modules.append(sname)

    m = getattr(self, 'samba_modules', None)
    if m is not None:
        modules.extend(TO_LIST(m))

    m = getattr(self, 'samba_subsystem', None)
    if m is not None:
        modules.append(m)

    sentinal = getattr(self, 'init_function_sentinal', 'NULL')

    targets    = LOCAL_CACHE(bld, 'TARGET_TYPE')
    cflags = getattr(self, 'samba_cflags', [])[:]

    if modules == []:
        sname = sname.replace('-','_')
        sname = sname.replace('/','_')
        cflags.append('-DSTATIC_%s_MODULES=%s' % (sname, sentinal))
        if sentinal == 'NULL':
            cflags.append('-DSTATIC_%s_MODULES_PROTO' % sname)
        self.ccflags = cflags
        return

    for m in modules:
        bld.ASSERT(m in subsystems,
                   "No init_function defined for module '%s' in target '%s'" % (m, self.sname))
        init_fn_list = []
        for d in subsystems[m]:
            if targets[d['TARGET']] != 'DISABLED':
                init_fn_list.append(d['INIT_FUNCTION'])
        if init_fn_list == []:
            cflags.append('-DSTATIC_%s_MODULES=%s' % (m, sentinal))
            if sentinal == 'NULL':
                cflags.append('-DSTATIC_%s_MODULES_PROTO' % m)
        else:
            cflags.append('-DSTATIC_%s_MODULES=%s' % (m, ','.join(init_fn_list) + ',' + sentinal))
            proto=''
            for f in init_fn_list:
                proto = proto + '_MODULE_PROTO(%s)' % f
            cflags.append('-DSTATIC_%s_MODULES_PROTO=%s' % (m, proto))
    self.ccflags = cflags



def check_duplicate_sources(bld, tgt_list):
    '''see if we are compiling the same source file more than once
       without an allow_duplicates attribute'''

    debug('deps: checking for duplicate sources')

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')
    ret = True

    global tstart

    for t in tgt_list:
        source_list = TO_LIST(getattr(t, 'source', ''))
        tpath = os.path.normpath(os_path_relpath(t.path.abspath(bld.env), t.env.BUILD_DIRECTORY + '/default'))
        obj_sources = set()
        for s in source_list:
            p = os.path.normpath(os.path.join(tpath, s))
            if p in obj_sources:
                Logs.error("ERROR: source %s appears twice in target '%s'" % (p, t.sname))
                sys.exit(1)
            obj_sources.add(p)
        t.samba_source_set = obj_sources

    subsystems = {}

    # build a list of targets that each source file is part of
    for t in tgt_list:
        sources = []
        if not targets[t.sname] in [ 'LIBRARY', 'BINARY', 'PYTHON' ]:
            continue
        for obj in t.add_objects:
            t2 = t.bld.name_to_obj(obj, bld.env)
            source_set = getattr(t2, 'samba_source_set', set())
            for s in source_set:
                if not s in subsystems:
                    subsystems[s] = {}
                if not t.sname in subsystems[s]:
                    subsystems[s][t.sname] = []
                subsystems[s][t.sname].append(t2.sname)

    for s in subsystems:
        if len(subsystems[s]) > 1 and Options.options.SHOW_DUPLICATES:
            Logs.warn("WARNING: source %s is in more than one target: %s" % (s, subsystems[s].keys()))
        for tname in subsystems[s]:
            if len(subsystems[s][tname]) > 1:
                raise Utils.WafError("ERROR: source %s is in more than one subsystem of target '%s': %s" % (s, tname, subsystems[s][tname]))
                
    return ret


def check_orpaned_targets(bld, tgt_list):
    '''check if any build targets are orphaned'''

    target_dict = LOCAL_CACHE(bld, 'TARGET_TYPE')

    debug('deps: checking for orphaned targets')

    for t in tgt_list:
        if getattr(t, 'samba_used', False) == True:
            continue
        type = target_dict[t.sname]
        if not type in ['BINARY', 'LIBRARY', 'MODULE', 'ET', 'PYTHON']:
            if re.search('^PIDL_', t.sname) is None:
                Logs.warn("Target %s of type %s is unused by any other target" % (t.sname, type))


def check_group_ordering(bld, tgt_list):
    '''see if we have any dependencies that violate the group ordering

    It is an error for a target to depend on a target from a later
    build group
    '''

    def group_name(g):
        tm = bld.task_manager
        return [x for x in tm.groups_names if id(tm.groups_names[x]) == id(g)][0]

    for g in bld.task_manager.groups:
        gname = group_name(g)
        for t in g.tasks_gen:
            t.samba_group = gname

    grp_map = {}
    idx = 0
    for g in bld.task_manager.groups:
        name = group_name(g)
        grp_map[name] = idx
        idx += 1

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    ret = True
    for t in tgt_list:
        tdeps = getattr(t, 'add_objects', []) + getattr(t, 'uselib_local', [])
        for d in tdeps:
            t2 = bld.name_to_obj(d, bld.env)
            if t2 is None:
                continue
            map1 = grp_map[t.samba_group]
            map2 = grp_map[t2.samba_group]

            if map2 > map1:
                Logs.error("Target %r in build group %r depends on target %r from later build group %r" % (
                           t.sname, t.samba_group, t2.sname, t2.samba_group))
                ret = False

    return ret


def show_final_deps(bld, tgt_list):
    '''show the final dependencies for all targets'''

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    for t in tgt_list:
        if not targets[t.sname] in ['LIBRARY', 'BINARY', 'PYTHON', 'SUBSYSTEM']:
            continue
        debug('deps: final dependencies for target %s: uselib=%s uselib_local=%s add_objects=%s',
              t.sname, t.uselib, getattr(t, 'uselib_local', []), getattr(t, 'add_objects', []))


def add_samba_attributes(bld, tgt_list):
    '''ensure a target has a the required samba attributes'''

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    for t in tgt_list:
        if t.name != '':
            t.sname = t.name
        else:
            t.sname = t.target
        t.samba_type = targets[t.sname]
        t.samba_abspath = t.path.abspath(bld.env)
        t.samba_deps_extended = t.samba_deps[:]
        t.samba_includes_extended = TO_LIST(t.samba_includes)[:]
        t.ccflags = getattr(t, 'samba_cflags', '')

def replace_grouping_libraries(bld, tgt_list):
    '''replace dependencies based on grouping libraries

    If a library is marked as a grouping library, then any target that
    depends on a subsystem that is part of that grouping library gets
    that dependency replaced with a dependency on the grouping library
    '''

    targets  = LOCAL_CACHE(bld, 'TARGET_TYPE')

    grouping = {}

    # find our list of grouping libraries, mapped from the subsystems they depend on
    for t in tgt_list:
        if not getattr(t, 'grouping_library', False):
            continue
        for dep in t.samba_deps_extended:
            bld.ASSERT(dep in targets, "grouping library target %s not declared in %s" % (dep, t.sname))
            if targets[dep] == 'SUBSYSTEM':
                grouping[dep] = t.sname

    # now replace any dependencies on elements of grouping libraries
    for t in tgt_list:
        for i in range(len(t.samba_deps_extended)):
            dep = t.samba_deps_extended[i]
            if dep in grouping:
                if t.sname != grouping[dep]:
                    debug("deps: target %s: replacing dependency %s with grouping library %s" % (t.sname, dep, grouping[dep]))
                    t.samba_deps_extended[i] = grouping[dep]



def build_direct_deps(bld, tgt_list):
    '''build the direct_objects and direct_libs sets for each target'''

    targets  = LOCAL_CACHE(bld, 'TARGET_TYPE')
    syslib_deps  = LOCAL_CACHE(bld, 'SYSLIB_DEPS')

    global_deps = bld.env.GLOBAL_DEPENDENCIES
    global_deps_exclude = set()
    for dep in global_deps:
        t = bld.name_to_obj(dep, bld.env)
        for d in t.samba_deps:
            # prevent loops from the global dependencies list
            global_deps_exclude.add(d)
            global_deps_exclude.add(d + '.objlist')

    for t in tgt_list:
        t.direct_objects = set()
        t.direct_libs = set()
        t.direct_syslibs = set()
        deps = t.samba_deps_extended[:]
        if getattr(t, 'samba_use_global_deps', False) and not t.sname in global_deps_exclude:
            deps.extend(global_deps)
        for d in deps:
            if d == t.sname: continue
            if not d in targets:
                Logs.error("Unknown dependency '%s' in '%s'" % (d, t.sname))
                sys.exit(1)
            if targets[d] in [ 'EMPTY', 'DISABLED' ]:
                continue
            if targets[d] == 'PYTHON' and targets[t.sname] != 'PYTHON' and t.sname.find('.objlist') == -1:
                # this check should be more restrictive, but for now we have pidl-generated python
                # code that directly depends on other python modules
                Logs.error('ERROR: Target %s has dependency on python module %s' % (t.sname, d))
                sys.exit(1)
            if targets[d] == 'SYSLIB':
                t.direct_syslibs.add(d)
                if d in syslib_deps:
                    for implied in TO_LIST(syslib_deps[d]):
                        if BUILTIN_LIBRARY(bld, implied):
                            t.direct_objects.add(implied)
                        elif targets[implied] == 'SYSLIB':
                            t.direct_syslibs.add(implied)
                        elif targets[implied] in ['LIBRARY', 'MODULE']:
                            t.direct_libs.add(implied)
                        else:
                            Logs.error('Implied dependency %s in %s is of type %s' % (
                                implied, t.sname, targets[implied]))
                            sys.exit(1)
                continue
            t2 = bld.name_to_obj(d, bld.env)
            if t2 is None:
                Logs.error("no task %s of type %s in %s" % (d, targets[d], t.sname))
                sys.exit(1)
            if t2.samba_type in [ 'LIBRARY', 'MODULE' ]:
                t.direct_libs.add(d)
            elif t2.samba_type in [ 'SUBSYSTEM', 'ASN1', 'PYTHON' ]:
                t.direct_objects.add(d)
    debug('deps: built direct dependencies')


def dependency_loop(loops, t, target):
    '''add a dependency loop to the loops dictionary'''
    if t.sname == target:
        return
    if not target in loops:
        loops[target] = set()
    if not t.sname in loops[target]:
        loops[target].add(t.sname)


def indirect_libs(bld, t, chain, loops):
    '''recursively calculate the indirect library dependencies for a target

    An indirect library is a library that results from a dependency on
    a subsystem
    '''

    ret = getattr(t, 'indirect_libs', None)
    if ret is not None:
        return ret

    ret = set()
    for obj in t.direct_objects:
        if obj in chain:
            dependency_loop(loops, t, obj)
            continue
        chain.add(obj)
        t2 = bld.name_to_obj(obj, bld.env)
        r2 = indirect_libs(bld, t2, chain, loops)
        chain.remove(obj)
        ret = ret.union(t2.direct_libs)
        ret = ret.union(r2)

    for obj in indirect_objects(bld, t, set(), loops):
        if obj in chain:
            dependency_loop(loops, t, obj)
            continue
        chain.add(obj)
        t2 = bld.name_to_obj(obj, bld.env)
        r2 = indirect_libs(bld, t2, chain, loops)
        chain.remove(obj)
        ret = ret.union(t2.direct_libs)
        ret = ret.union(r2)

    t.indirect_libs = ret

    return ret


def indirect_objects(bld, t, chain, loops):
    '''recursively calculate the indirect object dependencies for a target

    indirect objects are the set of objects from expanding the
    subsystem dependencies
    '''

    ret = getattr(t, 'indirect_objects', None)
    if ret is not None: return ret

    ret = set()
    for lib in t.direct_objects:
        if lib in chain:
            dependency_loop(loops, t, lib)
            continue
        chain.add(lib)
        t2 = bld.name_to_obj(lib, bld.env)
        r2 = indirect_objects(bld, t2, chain, loops)
        chain.remove(lib)
        ret = ret.union(t2.direct_objects)
        ret = ret.union(r2)

    t.indirect_objects = ret
    return ret


def extended_objects(bld, t, chain):
    '''recursively calculate the extended object dependencies for a target

    extended objects are the union of:
       - direct objects
       - indirect objects
       - direct and indirect objects of all direct and indirect libraries
    '''

    ret = getattr(t, 'extended_objects', None)
    if ret is not None: return ret

    ret = set()
    ret = ret.union(t.final_objects)

    for lib in t.final_libs:
        if lib in chain:
            continue
        t2 = bld.name_to_obj(lib, bld.env)
        chain.add(lib)
        r2 = extended_objects(bld, t2, chain)
        chain.remove(lib)
        ret = ret.union(t2.final_objects)
        ret = ret.union(r2)

    t.extended_objects = ret
    return ret


def includes_objects(bld, t, chain, inc_loops):
    '''recursively calculate the includes object dependencies for a target

    includes dependencies come from either library or object dependencies
    '''
    ret = getattr(t, 'includes_objects', None)
    if ret is not None:
        return ret

    ret = t.direct_objects.copy()
    ret = ret.union(t.direct_libs)

    for obj in t.direct_objects:
        if obj in chain:
            dependency_loop(inc_loops, t, obj)
            continue
        chain.add(obj)
        t2 = bld.name_to_obj(obj, bld.env)
        r2 = includes_objects(bld, t2, chain, inc_loops)
        chain.remove(obj)
        ret = ret.union(t2.direct_objects)
        ret = ret.union(r2)

    for lib in t.direct_libs:
        if lib in chain:
            dependency_loop(inc_loops, t, lib)
            continue
        chain.add(lib)
        t2 = bld.name_to_obj(lib, bld.env)
        if t2 is None:
            targets = LOCAL_CACHE(bld, 'TARGET_TYPE')
            Logs.error('Target %s of type %s not found in direct_libs for %s' % (
                lib, targets[lib], t.sname))
            sys.exit(1)
        r2 = includes_objects(bld, t2, chain, inc_loops)
        chain.remove(lib)
        ret = ret.union(t2.direct_objects)
        ret = ret.union(r2)

    t.includes_objects = ret
    return ret


def break_dependency_loops(bld, tgt_list):
    '''find and break dependency loops'''
    loops = {}
    inc_loops = {}

    # build up the list of loops
    for t in tgt_list:
        indirect_objects(bld, t, set(), loops)
        indirect_libs(bld, t, set(), loops)
        includes_objects(bld, t, set(), inc_loops)

    # break the loops
    for t in tgt_list:
        if t.sname in loops:
            for attr in ['direct_objects', 'indirect_objects', 'direct_libs', 'indirect_libs']:
                objs = getattr(t, attr, set())
                setattr(t, attr, objs.difference(loops[t.sname]))

    for loop in loops:
        debug('deps: Found dependency loops for target %s : %s', loop, loops[loop])

    for loop in inc_loops:
        debug('deps: Found include loops for target %s : %s', loop, inc_loops[loop])

    # expand the loops mapping by one level
    for loop in loops.copy():
        for tgt in loops[loop]:
            if tgt in loops:
                loops[loop] = loops[loop].union(loops[tgt])

    for loop in inc_loops.copy():
        for tgt in inc_loops[loop]:
            if tgt in inc_loops:
                inc_loops[loop] = inc_loops[loop].union(inc_loops[tgt])


    # expand indirect subsystem and library loops
    for loop in loops.copy():
        t = bld.name_to_obj(loop, bld.env)
        if t.samba_type in ['SUBSYSTEM']:
            loops[loop] = loops[loop].union(t.indirect_objects)
            loops[loop] = loops[loop].union(t.direct_objects)
        if t.samba_type in ['LIBRARY','PYTHON']:
            loops[loop] = loops[loop].union(t.indirect_libs)
            loops[loop] = loops[loop].union(t.direct_libs)
        if loop in loops[loop]:
            loops[loop].remove(loop)

    # expand indirect includes loops
    for loop in inc_loops.copy():
        t = bld.name_to_obj(loop, bld.env)
        inc_loops[loop] = inc_loops[loop].union(t.includes_objects)
        if loop in inc_loops[loop]:
            inc_loops[loop].remove(loop)

    # add in the replacement dependencies
    for t in tgt_list:
        for loop in loops:
            for attr in ['indirect_objects', 'indirect_libs']:
                objs = getattr(t, attr, set())
                if loop in objs:
                    diff = loops[loop].difference(objs)
                    if t.sname in diff:
                        diff.remove(t.sname)
                    if diff:
                        debug('deps: Expanded target %s of type %s from loop %s by %s', t.sname, t.samba_type, loop, diff)
                        objs = objs.union(diff)
                setattr(t, attr, objs)

        for loop in inc_loops:
            objs = getattr(t, 'includes_objects', set())
            if loop in objs:
                diff = inc_loops[loop].difference(objs)
                if t.sname in diff:
                    diff.remove(t.sname)
                if diff:
                    debug('deps: Expanded target %s includes of type %s from loop %s by %s', t.sname, t.samba_type, loop, diff)
                    objs = objs.union(diff)
            setattr(t, 'includes_objects', objs)


def reduce_objects(bld, tgt_list):
    '''reduce objects by looking for indirect object dependencies'''
    rely_on = {}

    for t in tgt_list:
        t.extended_objects = None

    changed = False

    for type in ['BINARY', 'PYTHON', 'LIBRARY']:
        for t in tgt_list:
            if t.samba_type != type: continue
            # if we will indirectly link to a target then we don't need it
            new = t.final_objects.copy()
            for l in t.final_libs:
                t2 = bld.name_to_obj(l, bld.env)
                t2_obj = extended_objects(bld, t2, set())
                dup = new.intersection(t2_obj)
                if t.sname in rely_on:
                    dup = dup.difference(rely_on[t.sname])
                if dup:
                    debug('deps: removing dups from %s of type %s: %s also in %s %s',
                          t.sname, t.samba_type, dup, t2.samba_type, l)
                    new = new.difference(dup)
                    changed = True
                    if not l in rely_on:
                        rely_on[l] = set()
                    rely_on[l] = rely_on[l].union(dup)
            t.final_objects = new

    if not changed:
        return False

    # add back in any objects that were relied upon by the reduction rules
    for r in rely_on:
        t = bld.name_to_obj(r, bld.env)
        t.final_objects = t.final_objects.union(rely_on[r])

    return True


def show_library_loop(bld, lib1, lib2, path, seen):
    '''show the detailed path of a library loop between lib1 and lib2'''

    t = bld.name_to_obj(lib1, bld.env)
    if not lib2 in getattr(t, 'final_libs', set()):
        return

    for d in t.samba_deps_extended:
        if d in seen:
            continue
        seen.add(d)
        path2 = path + '=>' + d
        if d == lib2:
            Logs.warn('library loop path: ' + path2)
            return
        show_library_loop(bld, d, lib2, path2, seen)
        seen.remove(d)


def calculate_final_deps(bld, tgt_list, loops):
    '''calculate the final library and object dependencies'''
    for t in tgt_list:
        # start with the maximum possible list
        t.final_libs    = t.direct_libs.union(indirect_libs(bld, t, set(), loops))
        t.final_objects = t.direct_objects.union(indirect_objects(bld, t, set(), loops))

    for t in tgt_list:
        # don't depend on ourselves
        if t.sname in t.final_libs:
            t.final_libs.remove(t.sname)
        if t.sname in t.final_objects:
            t.final_objects.remove(t.sname)

    # handle any non-shared binaries
    for t in tgt_list:
        if t.samba_type == 'BINARY' and bld.NONSHARED_BINARY(t.sname):
            subsystem_list = LOCAL_CACHE(bld, 'INIT_FUNCTIONS')
            targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

            # replace lib deps with objlist deps
            for l in t.final_libs:
                objname = l + '.objlist'
                t2 = bld.name_to_obj(objname, bld.env)
                if t2 is None:
                    Logs.error('ERROR: subsystem %s not found' % objname)
                    sys.exit(1)
                t.final_objects.add(objname)
                t.final_objects = t.final_objects.union(extended_objects(bld, t2, set()))
                if l in subsystem_list:
                    # its a subsystem - we also need the contents of any modules
                    for d in subsystem_list[l]:
                        module_name = d['TARGET']
                        if targets[module_name] == 'LIBRARY':
                            objname = module_name + '.objlist'
                        elif targets[module_name] == 'SUBSYSTEM':
                            objname = module_name
                        else:
                            continue
                        t2 = bld.name_to_obj(objname, bld.env)
                        if t2 is None:
                            Logs.error('ERROR: subsystem %s not found' % objname)
                            sys.exit(1)
                        t.final_objects.add(objname)
                        t.final_objects = t.final_objects.union(extended_objects(bld, t2, set()))
            t.final_libs = set()

    # find any library loops
    for t in tgt_list:
        if t.samba_type in ['LIBRARY', 'PYTHON']:
            for l in t.final_libs.copy():
                t2 = bld.name_to_obj(l, bld.env)
                if t.sname in t2.final_libs:
                    if getattr(bld.env, "ALLOW_CIRCULAR_LIB_DEPENDENCIES", False):
                        # we could break this in either direction. If one of the libraries
                        # has a version number, and will this be distributed publicly, then
                        # we should make it the lower level library in the DAG
                        Logs.warn('deps: removing library loop %s from %s' % (t.sname, t2.sname))
                        dependency_loop(loops, t, t2.sname)
                        t2.final_libs.remove(t.sname)
                    else:
                        Logs.error('ERROR: circular library dependency between %s and %s'
                            % (t.sname, t2.sname))
                        show_library_loop(bld, t.sname, t2.sname, t.sname, set())
                        show_library_loop(bld, t2.sname, t.sname, t2.sname, set())
                        sys.exit(1)

    for loop in loops:
        debug('deps: Found dependency loops for target %s : %s', loop, loops[loop])

    # we now need to make corrections for any library loops we broke up
    # any target that depended on the target of the loop and doesn't
    # depend on the source of the loop needs to get the loop source added
    for type in ['BINARY','PYTHON','LIBRARY','BINARY']:
        for t in tgt_list:
            if t.samba_type != type: continue
            for loop in loops:
                if loop in t.final_libs:
                    diff = loops[loop].difference(t.final_libs)
                    if t.sname in diff:
                        diff.remove(t.sname)
                    if t.sname in diff:
                        diff.remove(t.sname)
                    # make sure we don't recreate the loop again!
                    for d in diff.copy():
                        t2 = bld.name_to_obj(d, bld.env)
                        if t2.samba_type == 'LIBRARY':
                            if t.sname in t2.final_libs:
                                debug('deps: removing expansion %s from %s', d, t.sname)
                                diff.remove(d)
                    if diff:
                        debug('deps: Expanded target %s by loop %s libraries (loop %s) %s', t.sname, loop,
                              loops[loop], diff)
                        t.final_libs = t.final_libs.union(diff)

    # remove objects that are also available in linked libs
    count = 0
    while reduce_objects(bld, tgt_list):
        count += 1
        if count > 100:
            Logs.warn("WARNING: Unable to remove all inter-target object duplicates")
            break
    debug('deps: Object reduction took %u iterations', count)

    # add in any syslib dependencies
    for t in tgt_list:
        if not t.samba_type in ['BINARY','PYTHON','LIBRARY','SUBSYSTEM']:
            continue
        syslibs = set()
        for d in t.final_objects:
            t2 = bld.name_to_obj(d, bld.env)
            syslibs = syslibs.union(t2.direct_syslibs)
        # this adds the indirect syslibs as well, which may not be needed
        # depending on the linker flags
        for d in t.final_libs:
            t2 = bld.name_to_obj(d, bld.env)
            syslibs = syslibs.union(t2.direct_syslibs)
        t.final_syslibs = syslibs


    # find any unresolved library loops
    lib_loop_error = False
    for t in tgt_list:
        if t.samba_type in ['LIBRARY', 'PYTHON']:
            for l in t.final_libs.copy():
                t2 = bld.name_to_obj(l, bld.env)
                if t.sname in t2.final_libs:
                    Logs.error('ERROR: Unresolved library loop %s from %s' % (t.sname, t2.sname))
                    lib_loop_error = True
    if lib_loop_error:
        sys.exit(1)

    debug('deps: removed duplicate dependencies')


def show_dependencies(bld, target, seen):
    '''recursively show the dependencies of target'''

    if target in seen:
        return

    t = bld.name_to_obj(target, bld.env)
    if t is None:
        Logs.error("ERROR: Unable to find target '%s'" % target)
        sys.exit(1)

    Logs.info('%s(OBJECTS): %s' % (target, t.direct_objects))
    Logs.info('%s(LIBS): %s' % (target, t.direct_libs))
    Logs.info('%s(SYSLIBS): %s' % (target, t.direct_syslibs))

    seen.add(target)

    for t2 in t.direct_objects:
        show_dependencies(bld, t2, seen)


def show_object_duplicates(bld, tgt_list):
    '''show a list of object files that are included in more than
    one library or binary'''

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    used_by = {}

    Logs.info("showing duplicate objects")

    for t in tgt_list:
        if not targets[t.sname] in [ 'LIBRARY', 'PYTHON' ]:
            continue
        for n in getattr(t, 'final_objects', set()):
            t2 = bld.name_to_obj(n, bld.env)
            if not n in used_by:
                used_by[n] = set()
            used_by[n].add(t.sname)

    for n in used_by:
        if len(used_by[n]) > 1:
            Logs.info("target '%s' is used by %s" % (n, used_by[n]))

    Logs.info("showing indirect dependency counts (sorted by count)")

    def indirect_count(t1, t2):
        return len(t2.indirect_objects) - len(t1.indirect_objects)

    sorted_list = sorted(tgt_list, cmp=indirect_count)
    for t in sorted_list:
        if len(t.indirect_objects) > 1:
            Logs.info("%s depends on %u indirect objects" % (t.sname, len(t.indirect_objects)))


######################################################################
# this provides a way to save our dependency calculations between runs
savedeps_version = 3
savedeps_inputs  = ['samba_deps', 'samba_includes', 'local_include', 'local_include_first', 'samba_cflags',
                    'source', 'grouping_library', 'samba_ldflags', 'allow_undefined_symbols',
                    'use_global_deps', 'global_include' ]
savedeps_outputs = ['uselib', 'uselib_local', 'add_objects', 'includes', 'ccflags', 'ldflags', 'samba_deps_extended']
savedeps_outenv  = ['INC_PATHS']
savedeps_envvars = ['NONSHARED_BINARIES', 'GLOBAL_DEPENDENCIES', 'EXTRA_CFLAGS', 'EXTRA_LDFLAGS', 'EXTRA_INCLUDES' ]
savedeps_caches  = ['GLOBAL_DEPENDENCIES', 'TARGET_TYPE', 'INIT_FUNCTIONS', 'SYSLIB_DEPS']
savedeps_files   = ['buildtools/wafsamba/samba_deps.py']

def save_samba_deps(bld, tgt_list):
    '''save the dependency calculations between builds, to make
       further builds faster'''
    denv = Environment.Environment()

    denv.version = savedeps_version
    denv.savedeps_inputs = savedeps_inputs
    denv.savedeps_outputs = savedeps_outputs
    denv.input = {}
    denv.output = {}
    denv.outenv = {}
    denv.caches = {}
    denv.envvar = {}
    denv.files  = {}

    for f in savedeps_files:
        denv.files[f] = os.stat(os.path.join(bld.srcnode.abspath(), f)).st_mtime

    for c in savedeps_caches:
        denv.caches[c] = LOCAL_CACHE(bld, c)

    for e in savedeps_envvars:
        denv.envvar[e] = bld.env[e]

    for t in tgt_list:
        # save all the input attributes for each target
        tdeps = {}
        for attr in savedeps_inputs:
            v = getattr(t, attr, None)
            if v is not None:
                tdeps[attr] = v
        if tdeps != {}:
            denv.input[t.sname] = tdeps

        # save all the output attributes for each target
        tdeps = {}
        for attr in savedeps_outputs:
            v = getattr(t, attr, None)
            if v is not None:
                tdeps[attr] = v
        if tdeps != {}:
            denv.output[t.sname] = tdeps

        tdeps = {}
        for attr in savedeps_outenv:
            if attr in t.env:
                tdeps[attr] = t.env[attr]
        if tdeps != {}:
            denv.outenv[t.sname] = tdeps

    depsfile = os.path.join(bld.bdir, "sambadeps")
    denv.store(depsfile)



def load_samba_deps(bld, tgt_list):
    '''load a previous set of build dependencies if possible'''
    depsfile = os.path.join(bld.bdir, "sambadeps")
    denv = Environment.Environment()
    try:
        debug('deps: checking saved dependencies')
        denv.load(depsfile)
        if (denv.version != savedeps_version or
            denv.savedeps_inputs != savedeps_inputs or
            denv.savedeps_outputs != savedeps_outputs):
            return False
    except:
        return False

    # check if critical files have changed
    for f in savedeps_files:
        if f not in denv.files:
            return False
        if denv.files[f] != os.stat(os.path.join(bld.srcnode.abspath(), f)).st_mtime:
            return False

    # check if caches are the same
    for c in savedeps_caches:
        if c not in denv.caches or denv.caches[c] != LOCAL_CACHE(bld, c):
            return False

    # check if caches are the same
    for e in savedeps_envvars:
        if e not in denv.envvar or denv.envvar[e] != bld.env[e]:
            return False

    # check inputs are the same
    for t in tgt_list:
        tdeps = {}
        for attr in savedeps_inputs:
            v = getattr(t, attr, None)
            if v is not None:
                tdeps[attr] = v
        if t.sname in denv.input:
            olddeps = denv.input[t.sname]
        else:
            olddeps = {}
        if tdeps != olddeps:
            #print '%s: \ntdeps=%s \nodeps=%s' % (t.sname, tdeps, olddeps)
            return False

    # put outputs in place
    for t in tgt_list:
        if not t.sname in denv.output: continue
        tdeps = denv.output[t.sname]
        for a in tdeps:
            setattr(t, a, tdeps[a])

    # put output env vars in place
    for t in tgt_list:
        if not t.sname in denv.outenv: continue
        tdeps = denv.outenv[t.sname]
        for a in tdeps:
            t.env[a] = tdeps[a]

    debug('deps: loaded saved dependencies')
    return True



def check_project_rules(bld):
    '''check the project rules - ensuring the targets are sane'''

    loops = {}
    inc_loops = {}

    tgt_list = get_tgt_list(bld)

    add_samba_attributes(bld, tgt_list)

    force_project_rules = (Options.options.SHOWDEPS or
                           Options.options.SHOW_DUPLICATES)

    if not force_project_rules and load_samba_deps(bld, tgt_list):
        return

    global tstart
    tstart = time.clock()

    bld.new_rules = True
    Logs.info("Checking project rules ...")

    debug('deps: project rules checking started')

    expand_subsystem_deps(bld)

    debug("deps: expand_subsystem_deps: %f" % (time.clock() - tstart))

    replace_grouping_libraries(bld, tgt_list)

    debug("deps: replace_grouping_libraries: %f" % (time.clock() - tstart))

    build_direct_deps(bld, tgt_list)

    debug("deps: build_direct_deps: %f" % (time.clock() - tstart))

    break_dependency_loops(bld, tgt_list)

    debug("deps: break_dependency_loops: %f" % (time.clock() - tstart))

    if Options.options.SHOWDEPS:
            show_dependencies(bld, Options.options.SHOWDEPS, set())

    calculate_final_deps(bld, tgt_list, loops)

    debug("deps: calculate_final_deps: %f" % (time.clock() - tstart))

    if Options.options.SHOW_DUPLICATES:
            show_object_duplicates(bld, tgt_list)

    # run the various attribute generators
    for f in [ build_dependencies, build_includes, add_init_functions ]:
        debug('deps: project rules checking %s', f)
        for t in tgt_list: f(t)
        debug("deps: %s: %f" % (f, time.clock() - tstart))

    debug('deps: project rules stage1 completed')

    #check_orpaned_targets(bld, tgt_list)

    if not check_duplicate_sources(bld, tgt_list):
        Logs.error("Duplicate sources present - aborting")
        sys.exit(1)

    debug("deps: check_duplicate_sources: %f" % (time.clock() - tstart))

    if not check_group_ordering(bld, tgt_list):
        Logs.error("Bad group ordering - aborting")
        sys.exit(1)

    debug("deps: check_group_ordering: %f" % (time.clock() - tstart))

    show_final_deps(bld, tgt_list)

    debug("deps: show_final_deps: %f" % (time.clock() - tstart))

    debug('deps: project rules checking completed - %u targets checked',
          len(tgt_list))

    if not bld.is_install:
        save_samba_deps(bld, tgt_list)

    debug("deps: save_samba_deps: %f" % (time.clock() - tstart))

    Logs.info("Project rules pass")


def CHECK_PROJECT_RULES(bld):
    '''enable checking of project targets for sanity'''
    if bld.env.added_project_rules:
        return
    bld.env.added_project_rules = True
    bld.add_pre_fun(check_project_rules)
Build.BuildContext.CHECK_PROJECT_RULES = CHECK_PROJECT_RULES


