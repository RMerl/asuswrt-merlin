#include <unistd.h>
#include <string.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"

/* -----------------------------------------------------------------------------
 * Packages handling
 * ---------------------------------------------------------------------------*/

int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver, int flags)
{
    /* If the package was already provided returns an error. */
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->packages, name);

    /* An empty result means the automatic entry. This can be replaced */
    if (he && *(const char *)he->u.val) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "package \"%s\" was already provided", name);
        }
        return JIM_ERR;
    }
    if (he) {
        Jim_DeleteHashEntry(&interp->packages, name);
    }
    Jim_AddHashEntry(&interp->packages, name, (char *)ver);
    return JIM_OK;
}

static char *JimFindPackage(Jim_Interp *interp, char **prefixes, int prefixc, const char *pkgName)
{
    int i;
    char *buf = Jim_Alloc(JIM_PATH_LEN);

    for (i = 0; i < prefixc; i++) {
        if (prefixes[i] == NULL)
            continue;

        /* Loadable modules are tried first */
#ifdef jim_ext_load
        snprintf(buf, JIM_PATH_LEN, "%s/%s.so", prefixes[i], pkgName);
        if (access(buf, R_OK) == 0) {
            return buf;
        }
#endif
        if (strcmp(prefixes[i], ".") == 0) {
            snprintf(buf, JIM_PATH_LEN, "%s.tcl", pkgName);
        }
        else {
            snprintf(buf, JIM_PATH_LEN, "%s/%s.tcl", prefixes[i], pkgName);
        }

        if (access(buf, R_OK) == 0) {
            return buf;
        }
    }
    Jim_Free(buf);
    return NULL;
}

/* Search for a suitable package under every dir specified by JIM_LIBPATH,
 * and load it if possible. If a suitable package was loaded with success
 * JIM_OK is returned, otherwise JIM_ERR is returned. */
static int JimLoadPackage(Jim_Interp *interp, const char *name, int flags)
{
    Jim_Obj *libPathObjPtr;
    char **prefixes, *path;
    int prefixc, i, retCode = JIM_ERR;

    libPathObjPtr = Jim_GetGlobalVariableStr(interp, JIM_LIBPATH, JIM_NONE);
    if (libPathObjPtr == NULL) {
        prefixc = 0;
        libPathObjPtr = NULL;
    }
    else {
        Jim_IncrRefCount(libPathObjPtr);
        prefixc = Jim_ListLength(interp, libPathObjPtr);
    }

    prefixes = Jim_Alloc(sizeof(char *) * prefixc);
    for (i = 0; i < prefixc; i++) {
        Jim_Obj *prefixObjPtr;

        if (Jim_ListIndex(interp, libPathObjPtr, i, &prefixObjPtr, JIM_NONE) != JIM_OK) {
            prefixes[i] = NULL;
            continue;
        }
        prefixes[i] = Jim_StrDup(Jim_String(prefixObjPtr));
    }

    /* Scan every directory for the the first match */
    path = JimFindPackage(interp, prefixes, prefixc, name);
    if (path != NULL) {
        char *p = strrchr(path, '.');

        /* Note: Even if the file fails to load, we consider the package loaded.
         *       This prevents issues with recursion.
         *       Use a dummy version of "" to signify this case.
         */
        Jim_PackageProvide(interp, name, "", 0);

        /* Try to load/source it */
        if (p && strcmp(p, ".tcl") == 0) {
            retCode = Jim_EvalFileGlobal(interp, path);
        }
#ifdef jim_ext_load
        else {
            retCode = Jim_LoadLibrary(interp, path);
        }
#endif
        if (retCode != JIM_OK) {
            /* Upon failure, remove the dummy entry */
            Jim_DeleteHashEntry(&interp->packages, name);
        }
        Jim_Free(path);
    }
    for (i = 0; i < prefixc; i++)
        Jim_Free(prefixes[i]);
    Jim_Free(prefixes);
    if (libPathObjPtr)
        Jim_DecrRefCount(interp, libPathObjPtr);
    return retCode;
}

int Jim_PackageRequire(Jim_Interp *interp, const char *name, int flags)
{
    Jim_HashEntry *he;

    /* Start with an empty error string */
    Jim_SetResultString(interp, "", 0);

    he = Jim_FindHashEntry(&interp->packages, name);
    if (he == NULL) {
        /* Try to load the package. */
        int retcode = JimLoadPackage(interp, name, flags);
        if (retcode != JIM_OK) {
            if (flags & JIM_ERRMSG) {
                int len;

                Jim_GetString(Jim_GetResult(interp), &len);
                Jim_SetResultFormatted(interp, "%#s%sCan't load package %s",
                    Jim_GetResult(interp), len ? "\n" : "", name);
            }
            return retcode;
        }

        /* In case the package did no 'package provide' */
        Jim_PackageProvide(interp, name, "1.0", 0);

        /* Now it must exist */
        he = Jim_FindHashEntry(&interp->packages, name);
    }

    Jim_SetResultString(interp, he->u.val, -1);
    return JIM_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * package provide name ?version?
 *
 *      This procedure is invoked to declare that a particular version
 *      of a particular package is now present in an interpreter.  There
 *      must not be any other version of this package already
 *      provided in the interpreter.
 *
 * Results:
 *      Returns JIM_OK and sets the package version (or 1.0 if not specified).
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_provide(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *version = "1.0";

    if (argc == 2) {
        version = Jim_String(argv[1]);
    }
    return Jim_PackageProvide(interp, Jim_String(argv[0]), version, JIM_ERRMSG);
}

/*
 *----------------------------------------------------------------------
 *
 * package require name ?version?
 *
 *      This procedure is load a given package.
 *      Note that the version is ignored.
 *
 * Results:
 *      Returns JIM_OK and sets the package version.
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_require(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* package require failing is important enough to add to the stack */
    interp->addStackTrace++;

    return Jim_PackageRequire(interp, Jim_String(argv[0]), JIM_ERRMSG);
}

/*
 *----------------------------------------------------------------------
 *
 * package list
 *
 *      Returns a list of known packages
 *
 * Results:
 *      Returns JIM_OK and sets a list of known packages.
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_list(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    htiter = Jim_GetHashTableIterator(&interp->packages);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);

    Jim_SetResult(interp, listObjPtr);

    return JIM_OK;
}

static const jim_subcmd_type package_command_table[] = {
    {.cmd = "provide",
            .args = "name ?version?",
            .function = package_cmd_provide,
            .minargs = 1,
            .maxargs = 2,
        .description = "Indicates that the current script provides the given package"},
    {.cmd = "require",
            .args = "name ?version?",
            .function = package_cmd_require,
            .minargs = 1,
            .maxargs = 2,
        .description = "Loads the given package by looking in standard places"},
    {.cmd = "list",
            .function = package_cmd_list,
            .minargs = 0,
            .maxargs = 0,
        .description = "Lists all known packages"},
    {0}
};

int Jim_packageInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "package", Jim_SubCmdProc, (void *)package_command_table, NULL);
    return JIM_OK;
}
