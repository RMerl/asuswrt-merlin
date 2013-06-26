# a waf tool to extract symbols from object files or libraries
# using nm, producing a set of exposed defined/undefined symbols

import Utils, Build, subprocess, Logs
from samba_wildcard import fake_build_environment
from samba_utils import *

# these are the data structures used in symbols.py:
#
# bld.env.symbol_map : dictionary mapping public symbol names to list of
#                      subsystem names where that symbol exists
#
# t.in_library       : list of libraries that t is in
#
# bld.env.public_symbols: set of public symbols for each subsystem
# bld.env.used_symbols  : set of used symbols for each subsystem
#
# bld.env.syslib_symbols: dictionary mapping system library name to set of symbols
#                         for that library
#
# LOCAL_CACHE(bld, 'TARGET_TYPE') : dictionary mapping subsystem name to target type

def symbols_extract(objfiles, dynamic=False):
    '''extract symbols from objfile, returning a dictionary containing
       the set of undefined and public symbols for each file'''

    ret = {}

    cmd = ["nm"]
    if dynamic:
        # needed for some .so files
        cmd.append("-D")
    cmd.extend(objfiles)

    nmpipe = subprocess.Popen(cmd, stdout=subprocess.PIPE).stdout
    if len(objfiles) == 1:
        filename = objfiles[0]
        ret[filename] = { "PUBLIC": set(), "UNDEFINED" : set()}

    for line in nmpipe:
        line = line.strip()
        if line.endswith(':'):
            filename = line[:-1]
            ret[filename] = { "PUBLIC": set(), "UNDEFINED" : set() }
            continue
        cols = line.split(" ")
        if cols == ['']:
            continue
        # see if the line starts with an address
        if len(cols) == 3:
            symbol_type = cols[1]
            symbol = cols[2]
        else:
            symbol_type = cols[0]
            symbol = cols[1]
        if symbol_type in "BDGTRVWSi":
            # its a public symbol
            ret[filename]["PUBLIC"].add(symbol)
        elif symbol_type in "U":
            ret[filename]["UNDEFINED"].add(symbol)

    return ret


def real_name(name):
    if name.find(".objlist") != -1:
        name = name[:-8]
    return name


def find_syslib_path(bld, libname, deps):
    '''find the path to the syslib we will link against'''
    # the strategy is to use the targets that depend on the library, and run ldd
    # on it to find the real location of the library that is used

    linkpath = deps[0].link_task.outputs[0].abspath(bld.env)

    if libname == "python":
        libname += bld.env.PYTHON_VERSION

    ret = None

    lddpipe = subprocess.Popen(['ldd', linkpath], stdout=subprocess.PIPE).stdout
    for line in lddpipe:
        line = line.strip()
        cols = line.split(" ")
        if len(cols) < 3 or cols[1] != "=>":
            continue
        if cols[0].startswith("lib%s." % libname.lower()):
            ret = cols[2]
        if cols[0].startswith("libc."):
            # save this one too
            bld.env.libc_path = cols[2]
    return ret


def build_symbol_sets(bld, tgt_list):
    '''build the public_symbols and undefined_symbols attributes for each target'''

    if bld.env.public_symbols:
        return

    objlist = []  # list of object file
    objmap = {}   # map from object filename to target (subsystem) name

    for t in tgt_list:
        t.public_symbols = set()
        t.undefined_symbols = set()
        t.used_symbols = set()
        for tsk in getattr(t, 'compiled_tasks', []):
            for output in tsk.outputs:
                objpath = output.abspath(bld.env)
                objlist.append(objpath)
                objmap[objpath] = t

    symbols = symbols_extract(objlist)
    for obj in objlist:
        t = objmap[obj]
        t.public_symbols = t.public_symbols.union(symbols[obj]["PUBLIC"])
        t.undefined_symbols = t.undefined_symbols.union(symbols[obj]["UNDEFINED"])
        t.used_symbols = t.used_symbols.union(symbols[obj]["UNDEFINED"])

    t.undefined_symbols = t.undefined_symbols.difference(t.public_symbols)

    # and the reverse map of public symbols to subsystem name
    bld.env.symbol_map = {}

    for t in tgt_list:
        for s in t.public_symbols:
            if not s in bld.env.symbol_map:
                bld.env.symbol_map[s] = []
            bld.env.symbol_map[s].append(real_name(t.sname))

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    bld.env.public_symbols = {}
    for t in tgt_list:
        name = real_name(t.sname)
        if name in bld.env.public_symbols:
            bld.env.public_symbols[name] = bld.env.public_symbols[name].union(t.public_symbols)
        else:
            bld.env.public_symbols[name] = t.public_symbols
        if t.samba_type == 'LIBRARY':
            for dep in t.add_objects:
                t2 = bld.name_to_obj(dep, bld.env)
                bld.ASSERT(t2 is not None, "Library '%s' has unknown dependency '%s'" % (name, dep))
                bld.env.public_symbols[name] = bld.env.public_symbols[name].union(t2.public_symbols)

    bld.env.used_symbols = {}
    for t in tgt_list:
        name = real_name(t.sname)
        if name in bld.env.used_symbols:
            bld.env.used_symbols[name] = bld.env.used_symbols[name].union(t.used_symbols)
        else:
            bld.env.used_symbols[name] = t.used_symbols
        if t.samba_type == 'LIBRARY':
            for dep in t.add_objects:
                t2 = bld.name_to_obj(dep, bld.env)
                bld.ASSERT(t2 is not None, "Library '%s' has unknown dependency '%s'" % (name, dep))
                bld.env.used_symbols[name] = bld.env.used_symbols[name].union(t2.used_symbols)


def build_syslib_sets(bld, tgt_list):
    '''build the public_symbols for all syslibs'''

    if bld.env.syslib_symbols:
        return

    # work out what syslibs we depend on, and what targets those are used in
    syslibs = {}
    objmap = {}
    for t in tgt_list:
        if getattr(t, 'uselib', []) and t.samba_type in [ 'LIBRARY', 'BINARY', 'PYTHON' ]:
            for lib in t.uselib:
                if lib in ['PYEMBED', 'PYEXT']:
                    lib = "python"
                if not lib in syslibs:
                    syslibs[lib] = []
                syslibs[lib].append(t)

    # work out the paths to each syslib
    syslib_paths = []
    for lib in syslibs:
        path = find_syslib_path(bld, lib, syslibs[lib])
        if path is None:
            Logs.warn("Unable to find syslib path for %s" % lib)
        if path is not None:
            syslib_paths.append(path)
            objmap[path] = lib.lower()

    # add in libc
    syslib_paths.append(bld.env.libc_path)
    objmap[bld.env.libc_path] = 'c'

    symbols = symbols_extract(syslib_paths, dynamic=True)

    # keep a map of syslib names to public symbols
    bld.env.syslib_symbols = {}
    for lib in symbols:
        bld.env.syslib_symbols[lib] = symbols[lib]["PUBLIC"]

    # add to the map of symbols to dependencies
    for lib in symbols:
        for sym in symbols[lib]["PUBLIC"]:
            if not sym in bld.env.symbol_map:
                bld.env.symbol_map[sym] = []
            bld.env.symbol_map[sym].append(objmap[lib])

    # keep the libc symbols as well, as these are useful for some of the
    # sanity checks
    bld.env.libc_symbols = symbols[bld.env.libc_path]["PUBLIC"]

    # add to the combined map of dependency name to public_symbols
    for lib in bld.env.syslib_symbols:
        bld.env.public_symbols[objmap[lib]] = bld.env.syslib_symbols[lib]


def build_autodeps(bld, t):
    '''build the set of dependencies for a target'''
    deps = set()
    name = real_name(t.sname)

    targets    = LOCAL_CACHE(bld, 'TARGET_TYPE')

    for sym in t.undefined_symbols:
        if sym in t.public_symbols:
            continue
        if sym in bld.env.symbol_map:
            depname = bld.env.symbol_map[sym]
            if depname == [ name ]:
                # self dependencies aren't interesting
                continue
            if t.in_library == depname:
                # no need to depend on the library we are part of
                continue
            if depname[0] in ['c', 'python']:
                # these don't go into autodeps
                continue
            if targets[depname[0]] in [ 'SYSLIB' ]:
                deps.add(depname[0])
                continue
            t2 = bld.name_to_obj(depname[0], bld.env)
            if len(t2.in_library) != 1:
                deps.add(depname[0])
                continue
            if t2.in_library == t.in_library:
                # if we're part of the same library, we don't need to autodep
                continue
            deps.add(t2.in_library[0])
    t.autodeps = deps


def build_library_names(bld, tgt_list):
    '''add a in_library attribute to all targets that are part of a library'''

    if bld.env.done_build_library_names:
        return

    for t in tgt_list:
        t.in_library = []

    for t in tgt_list:
        if t.samba_type in [ 'LIBRARY' ]:
            for obj in t.samba_deps_extended:
                t2 = bld.name_to_obj(obj, bld.env)
                if t2 and t2.samba_type in [ 'SUBSYSTEM', 'ASN1' ]:
                    if not t.sname in t2.in_library:
                        t2.in_library.append(t.sname)
    bld.env.done_build_library_names = True


def check_library_deps(bld, t):
    '''check that all the autodeps that have mutual dependency of this
    target are in the same library as the target'''

    name = real_name(t.sname)

    if len(t.in_library) > 1:
        Logs.warn("WARNING: Target '%s' in multiple libraries: %s" % (t.sname, t.in_library))

    for dep in t.autodeps:
        t2 = bld.name_to_obj(dep, bld.env)
        if t2 is None:
            continue
        for dep2 in t2.autodeps:
            if dep2 == name and t.in_library != t2.in_library:
                Logs.warn("WARNING: mutual dependency %s <=> %s" % (name, real_name(t2.sname)))
                Logs.warn("Libraries should match. %s != %s" % (t.in_library, t2.in_library))
                # raise Utils.WafError("illegal mutual dependency")


def check_syslib_collisions(bld, tgt_list):
    '''check if a target has any symbol collisions with a syslib

    We do not want any code in Samba to use a symbol name from a
    system library. The chance of that causing problems is just too
    high. Note that libreplace uses a rep_XX approach of renaming
    symbols via macros
    '''

    has_error = False
    for t in tgt_list:
        for lib in bld.env.syslib_symbols:
            common = t.public_symbols.intersection(bld.env.syslib_symbols[lib])
            if common:
                Logs.error("ERROR: Target '%s' has symbols '%s' which is also in syslib '%s'" % (t.sname, common, lib))
                has_error = True
    if has_error:
        raise Utils.WafError("symbols in common with system libraries")


def check_dependencies(bld, t):
    '''check for depenencies that should be changed'''

    if bld.name_to_obj(t.sname + ".objlist", bld.env):
        return

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    remaining = t.undefined_symbols.copy()
    remaining = remaining.difference(t.public_symbols)

    sname = real_name(t.sname)

    deps = set(t.samba_deps)
    for d in t.samba_deps:
        if targets[d] in [ 'EMPTY', 'DISABLED', 'SYSLIB' ]:
            continue
        bld.ASSERT(d in bld.env.public_symbols, "Failed to find symbol list for dependency '%s'" % d)
        diff = remaining.intersection(bld.env.public_symbols[d])
        if not diff and targets[sname] != 'LIBRARY':
            Logs.info("Target '%s' has no dependency on %s" % (sname, d))
        else:
            remaining = remaining.difference(diff)

    t.unsatisfied_symbols = set()
    needed = {}
    for sym in remaining:
        if sym in bld.env.symbol_map:
            dep = bld.env.symbol_map[sym]
            if not dep[0] in needed:
                needed[dep[0]] = set()
            needed[dep[0]].add(sym)
        else:
            t.unsatisfied_symbols.add(sym)

    for dep in needed:
        Logs.info("Target '%s' should add dep '%s' for symbols %s" % (sname, dep, " ".join(needed[dep])))



def check_syslib_dependencies(bld, t):
    '''check for syslib depenencies'''

    if bld.name_to_obj(t.sname + ".objlist", bld.env):
        return

    sname = real_name(t.sname)

    remaining = set()

    features = TO_LIST(t.features)
    if 'pyembed' in features or 'pyext' in features:
        t.unsatisfied_symbols = t.unsatisfied_symbols.difference(bld.env.public_symbols['python'])

    needed = {}
    for sym in t.unsatisfied_symbols:
        if sym in bld.env.symbol_map:
            dep = bld.env.symbol_map[sym][0]
            if dep == 'c':
                continue
            if not dep in needed:
                needed[dep] = set()
            needed[dep].add(sym)
        else:
            remaining.add(sym)

    for dep in needed:
        Logs.info("Target '%s' should add syslib dep '%s' for symbols %s" % (sname, dep, " ".join(needed[dep])))

    if remaining:
        debug("deps: Target '%s' has unsatisfied symbols: %s" % (sname, " ".join(remaining)))



def symbols_symbolcheck(task):
    '''check the internal dependency lists'''
    bld = task.env.bld
    tgt_list = get_tgt_list(bld)

    build_symbol_sets(bld, tgt_list)
    build_library_names(bld, tgt_list)

    for t in tgt_list:
        t.autodeps = set()
        if getattr(t, 'source', ''):
            build_autodeps(bld, t)

    for t in tgt_list:
        check_dependencies(bld, t)

    for t in tgt_list:
        check_library_deps(bld, t)

def symbols_syslibcheck(task):
    '''check the syslib dependencies'''
    bld = task.env.bld
    tgt_list = get_tgt_list(bld)

    build_syslib_sets(bld, tgt_list)
    check_syslib_collisions(bld, tgt_list)

    for t in tgt_list:
        check_syslib_dependencies(bld, t)


def symbols_whyneeded(task):
    """check why 'target' needs to link to 'subsystem'"""
    bld = task.env.bld
    tgt_list = get_tgt_list(bld)

    why = Options.options.WHYNEEDED.split(":")
    if len(why) != 2:
        raise Utils.WafError("usage: WHYNEEDED=TARGET:DEPENDENCY")
    target = why[0]
    subsystem = why[1]

    build_symbol_sets(bld, tgt_list)
    build_library_names(bld, tgt_list)
    build_syslib_sets(bld, tgt_list)

    Logs.info("Checking why %s needs to link to %s" % (target, subsystem))
    if not target in bld.env.used_symbols:
        Logs.warn("unable to find target '%s' in used_symbols dict" % target)
        return
    if not subsystem in bld.env.public_symbols:
        Logs.warn("unable to find subsystem '%s' in public_symbols dict" % subsystem)
        return
    overlap = bld.env.used_symbols[target].intersection(bld.env.public_symbols[subsystem])
    if not overlap:
        Logs.info("target '%s' doesn't use any public symbols from '%s'" % (target, subsystem))
    else:
        Logs.info("target '%s' uses symbols %s from '%s'" % (target, overlap, subsystem))



def symbols_dupcheck(task):
    '''check for symbols defined in two different subsystems'''
    bld = task.env.bld
    tgt_list = get_tgt_list(bld)

    targets = LOCAL_CACHE(bld, 'TARGET_TYPE')

    Logs.info("Checking for duplicate symbols")
    for sym in bld.env.symbol_map:
        subsystems = set(bld.env.symbol_map[sym])
        if len(subsystems) == 1:
            continue

        if sym in ['main', '_init', '_fini', 'init_samba_module', 'samba_init_module', 'ldb_init_module' ]:
            # these are expected to be in many subsystems
            continue

        # if all of them are in system libraries, we can ignore them. This copes
        # with the duplication between libc, libpthread and libattr
        all_syslib = True
        for s in subsystems:
            if s != 'c' and (not s in targets or targets[s] != 'SYSLIB'):
                all_syslib = False
        if all_syslib:
            continue
        Logs.info("symbol %s appears in %s" % (sym, subsystems))


def SYMBOL_CHECK(bld):
    '''check our dependency lists'''
    if Options.options.SYMBOLCHECK:
        bld.SET_BUILD_GROUP('symbolcheck')
        task = bld(rule=symbols_symbolcheck, always=True, name='symbol checking')
        task.env.bld = bld

        bld.SET_BUILD_GROUP('syslibcheck')
        task = bld(rule=symbols_syslibcheck, always=True, name='syslib checking')
        task.env.bld = bld

        bld.SET_BUILD_GROUP('syslibcheck')
        task = bld(rule=symbols_dupcheck, always=True, name='symbol duplicate checking')
        task.env.bld = bld

    if Options.options.WHYNEEDED:
        bld.SET_BUILD_GROUP('syslibcheck')
        task = bld(rule=symbols_whyneeded, always=True, name='check why a dependency is needed')
        task.env.bld = bld


Build.BuildContext.SYMBOL_CHECK = SYMBOL_CHECK
