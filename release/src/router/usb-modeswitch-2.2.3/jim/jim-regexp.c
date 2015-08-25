/*
 * Implements the regexp and regsub commands for Jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Uses C library regcomp()/regexec() for the matching.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 *
 * Based on code originally from Tcl 6.7:
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <stdlib.h>
#include <string.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jimregexp.h"

static void FreeRegexpInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    regfree(objPtr->internalRep.regexpValue.compre);
    Jim_Free(objPtr->internalRep.regexpValue.compre);
}

static const Jim_ObjType regexpObjType = {
    "regexp",
    FreeRegexpInternalRep,
    NULL,
    NULL,
    JIM_TYPE_NONE
};

static regex_t *SetRegexpFromAny(Jim_Interp *interp, Jim_Obj *objPtr, unsigned flags)
{
    regex_t *compre;
    const char *pattern;
    int ret;

    /* Check if the object is already an uptodate variable */
    if (objPtr->typePtr == &regexpObjType &&
        objPtr->internalRep.regexpValue.compre && objPtr->internalRep.regexpValue.flags == flags) {
        /* nothing to do */
        return objPtr->internalRep.regexpValue.compre;
    }

    /* Not a regexp or the flags do not match */

    /* Get the string representation */
    pattern = Jim_String(objPtr);
    compre = Jim_Alloc(sizeof(regex_t));

    if ((ret = regcomp(compre, pattern, REG_EXTENDED | flags)) != 0) {
        char buf[100];

        regerror(ret, compre, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "couldn't compile regular expression pattern: %s", buf);
        regfree(compre);
        Jim_Free(compre);
        return NULL;
    }

    Jim_FreeIntRep(interp, objPtr);

    objPtr->typePtr = &regexpObjType;
    objPtr->internalRep.regexpValue.flags = flags;
    objPtr->internalRep.regexpValue.compre = compre;

    return compre;
}

int Jim_RegexpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int opt_indices = 0;
    int opt_all = 0;
    int opt_inline = 0;
    regex_t *regex;
    int match, i, j;
    int offset = 0;
    regmatch_t *pmatch = NULL;
    int source_len;
    int result = JIM_OK;
    const char *pattern;
    const char *source_str;
    int num_matches = 0;
    int num_vars;
    Jim_Obj *resultListObj = NULL;
    int regcomp_flags = 0;
    int eflags = 0;
    int option;
    enum {
        OPT_INDICES,  OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_INLINE, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-indices", "-nocase", "-line", "-all", "-inline", "-start", "--", NULL
    };

    if (argc < 3) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?switches? exp string ?matchVar? ?subMatchVar subMatchVar ...?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_INDICES:
                opt_indices = 1;
                break;

            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                break;

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_INLINE:
                opt_inline = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i < 2) {
        goto wrongNumArgs;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }

    pattern = Jim_String(argv[i]);
    source_str = Jim_GetString(argv[i + 1], &source_len);

    num_vars = argc - i - 2;

    if (opt_inline) {
        if (num_vars) {
            Jim_SetResultString(interp, "regexp match variables not allowed when using -inline",
                -1);
            result = JIM_ERR;
            goto done;
        }
        num_vars = regex->re_nsub + 1;
    }

    pmatch = Jim_Alloc((num_vars + 1) * sizeof(*pmatch));

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            source_str += source_len;
        }
        else if (offset > 0) {
            source_str += offset;
        }
        eflags |= REG_NOTBOL;
    }

    if (opt_inline) {
        resultListObj = Jim_NewListObj(interp, NULL, 0);
    }

  next_match:
    match = regexec(regex, source_str, num_vars + 1, pmatch, eflags);
    if (match >= REG_BADPAT) {
        char buf[100];

        regerror(match, regex, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
        result = JIM_ERR;
        goto done;
    }

    if (match == REG_NOMATCH) {
        goto done;
    }

    num_matches++;

    if (opt_all && !opt_inline) {
        /* Just count the number of matches, so skip the substitution h */
        goto try_next_match;
    }

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    j = 0;
    for (i += 2; opt_inline ? j < num_vars : i < argc; i++, j++) {
        Jim_Obj *resultObj;

        if (opt_indices) {
            resultObj = Jim_NewListObj(interp, NULL, 0);
        }
        else {
            resultObj = Jim_NewStringObj(interp, "", 0);
        }

        if (pmatch[j].rm_so == -1) {
            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
            }
        }
        else {
            int len = pmatch[j].rm_eo - pmatch[j].rm_so;

            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so + len - 1));
            }
            else {
                Jim_AppendString(interp, resultObj, source_str + pmatch[j].rm_so, len);
            }
        }

        if (opt_inline) {
            Jim_ListAppendElement(interp, resultListObj, resultObj);
        }
        else {
            /* And now set the result variable */
            result = Jim_SetVariable(interp, argv[i], resultObj);

            if (result != JIM_OK) {
                Jim_FreeObj(interp, resultObj);
                break;
            }
        }
    }

  try_next_match:
    if (opt_all && (pattern[0] != '^' || (regcomp_flags & REG_NEWLINE)) && *source_str) {
        if (pmatch[0].rm_eo) {
            offset += pmatch[0].rm_eo;
            source_str += pmatch[0].rm_eo;
        }
        else {
            source_str++;
            offset++;
        }
        if (*source_str) {
            eflags = REG_NOTBOL;
            goto next_match;
        }
    }

  done:
    if (result == JIM_OK) {
        if (opt_inline) {
            Jim_SetResult(interp, resultListObj);
        }
        else {
            Jim_SetResultInt(interp, num_matches);
        }
    }

    Jim_Free(pmatch);
    return result;
}

#define MAX_SUB_MATCHES 50

int Jim_RegsubCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int regcomp_flags = 0;
    int regexec_flags = 0;
    int opt_all = 0;
    int offset = 0;
    regex_t *regex;
    const char *p;
    int result;
    regmatch_t pmatch[MAX_SUB_MATCHES + 1];
    int num_matches = 0;

    int i, j, n;
    Jim_Obj *varname;
    Jim_Obj *resultObj;
    const char *source_str;
    int source_len;
    const char *replace_str;
    int replace_len;
    const char *pattern;
    int option;
    enum {
        OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-nocase", "-line", "-all", "-start", "--", NULL
    };

    if (argc < 4) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?switches? exp string subSpec ?varName?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                break;

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i != 3 && argc - i != 4) {
        goto wrongNumArgs;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }
    pattern = Jim_String(argv[i]);

    source_str = Jim_GetString(argv[i + 1], &source_len);
    replace_str = Jim_GetString(argv[i + 2], &replace_len);
    varname = argv[i + 3];

    /* Create the result string */
    resultObj = Jim_NewStringObj(interp, "", 0);

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            offset = source_len;
        }
        else if (offset < 0) {
            offset = 0;
        }
    }

    /* Copy the part before -start */
    Jim_AppendString(interp, resultObj, source_str, offset);

    /*
     * The following loop is to handle multiple matches within the
     * same source string;  each iteration handles one match and its
     * corresponding substitution.  If "-all" hasn't been specified
     * then the loop body only gets executed once.
     */

    n = source_len - offset;
    p = source_str + offset;
    do {
        int match = regexec(regex, p, MAX_SUB_MATCHES, pmatch, regexec_flags);

        if (match >= REG_BADPAT) {
            char buf[100];

            regerror(match, regex, buf, sizeof(buf));
            Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
            return JIM_ERR;
        }
        if (match == REG_NOMATCH) {
            break;
        }

        num_matches++;

        /*
         * Copy the portion of the source string before the match to the
         * result variable.
         */
        Jim_AppendString(interp, resultObj, p, pmatch[0].rm_so);

        /*
         * Append the subSpec (replace_str) argument to the variable, making appropriate
         * substitutions.  This code is a bit hairy because of the backslash
         * conventions and because the code saves up ranges of characters in
         * subSpec to reduce the number of calls to Jim_SetVar.
         */

        for (j = 0; j < replace_len; j++) {
            int idx;
            int c = replace_str[j];

            if (c == '&') {
                idx = 0;
            }
            else if (c == '\\' && j < replace_len) {
                c = replace_str[++j];
                if ((c >= '0') && (c <= '9')) {
                    idx = c - '0';
                }
                else if ((c == '\\') || (c == '&')) {
                    Jim_AppendString(interp, resultObj, replace_str + j, 1);
                    continue;
                }
                else {
                    Jim_AppendString(interp, resultObj, replace_str + j - 1, 2);
                    continue;
                }
            }
            else {
                Jim_AppendString(interp, resultObj, replace_str + j, 1);
                continue;
            }
            if ((idx < MAX_SUB_MATCHES) && pmatch[idx].rm_so != -1 && pmatch[idx].rm_eo != -1) {
                Jim_AppendString(interp, resultObj, p + pmatch[idx].rm_so,
                    pmatch[idx].rm_eo - pmatch[idx].rm_so);
            }
        }

        p += pmatch[0].rm_eo;
        n -= pmatch[0].rm_eo;

        /* If -all is not specified, or there is no source left, we are done */
        if (!opt_all || n == 0) {
            break;
        }

        /* An anchored pattern without -line must be done */
        if ((regcomp_flags & REG_NEWLINE) == 0 && pattern[0] == '^') {
            break;
        }

        /* If the pattern is empty, need to step forwards */
        if (pattern[0] == '\0' && n) {
            /* Need to copy the char we are moving over */
            Jim_AppendString(interp, resultObj, p, 1);
            p++;
            n--;
        }

        regexec_flags |= REG_NOTBOL;
    } while (n);

    /*
     * Copy the portion of the string after the last match to the
     * result variable.
     */
    Jim_AppendString(interp, resultObj, p, -1);

    /* And now set or return the result variable */
    if (argc - i == 4) {
        result = Jim_SetVariable(interp, varname, resultObj);

        if (result == JIM_OK) {
            Jim_SetResultInt(interp, num_matches);
        }
        else {
            Jim_FreeObj(interp, resultObj);
        }
    }
    else {
        Jim_SetResult(interp, resultObj);
        result = JIM_OK;
    }

    return result;
}

int Jim_regexpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "regexp", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "regexp", Jim_RegexpCmd, NULL, NULL);
    Jim_CreateCommand(interp, "regsub", Jim_RegsubCmd, NULL, NULL);
    return JIM_OK;
}
