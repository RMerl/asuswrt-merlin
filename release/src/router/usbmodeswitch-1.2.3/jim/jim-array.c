
/*
 * Implements the array command for jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"

static int array_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Just a regular [info exists] */
    Jim_SetResultInt(interp, Jim_GetVariable(interp, argv[0], 0) != 0);
    return JIM_OK;
}

static int array_cmd_get(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    int all = 0;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    Jim_Obj *dictObj;
    Jim_Obj **dictValuesObj;

    if (!objPtr) {
        return JIM_OK;
    }

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {
        all = 1;
    }

    /* If it is a dictionary or list with an even number of elements, nothing else to do */
    if (all) {
        if (Jim_IsDict(objPtr) || (Jim_IsList(objPtr) && Jim_ListLength(interp, objPtr) % 2 == 0)) {
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }

    if (Jim_DictKeysVector(interp, objPtr, NULL, 0, &dictObj, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_DictPairs(interp, dictObj, &dictValuesObj, &len) != JIM_OK) {
        return JIM_ERR;
    }

    if (all) {
        /* Return the whole array */
        Jim_SetResult(interp, dictObj);
    }
    else {
        /* Only return the matching values */
        resultObj = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; i < len; i += 2) {
            if (Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
                Jim_ListAppendElement(interp, resultObj, dictValuesObj[i]);
                Jim_ListAppendElement(interp, resultObj, dictValuesObj[i + 1]);
            }
        }

        Jim_SetResult(interp, resultObj);
    }
    Jim_Free(dictValuesObj);
    return JIM_OK;

}

static int array_cmd_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (!objPtr) {
        return JIM_OK;
    }

    return Jim_DictKeys(interp, objPtr, argc == 1 ? NULL : argv[1]);
}

static int array_cmd_unset(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr;
    Jim_Obj *dictObj;
    Jim_Obj **dictValuesObj;

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {
        /* Unset the whole array */
        Jim_UnsetVariable(interp, argv[0], JIM_NONE);
        return JIM_OK;
    }

    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (Jim_DictKeysVector(interp, objPtr, NULL, 0, &dictObj, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_DictPairs(interp, dictObj, &dictValuesObj, &len) != JIM_OK) {
        return JIM_ERR;
    }

    /* Create a new object with the values which don't match */
    resultObj = Jim_NewDictObj(interp, NULL, 0);

    for (i = 0; i < len; i += 2) {
        if (!Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
            Jim_DictAddElement(interp, resultObj, dictValuesObj[i], dictValuesObj[i + 1]);
        }
    }
    Jim_Free(dictValuesObj);

    Jim_SetVariable(interp, argv[0], resultObj);
    return JIM_OK;
}

static int array_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int len = 0;

    /* Not found means zero length */
    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        len = Jim_DictSize(interp, objPtr);
        if (len < 0) {
            return JIM_ERR;
        }
    }

    Jim_SetResultInt(interp, len);

    return JIM_OK;
}

static int array_cmd_set(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    int rc = JIM_OK;
    Jim_Obj *listObj = argv[1];

    if (Jim_GetVariable(interp, argv[0], JIM_NONE) == NULL) {
        /* Doesn't exist, so just set the list directly */
        return Jim_SetVariable(interp, argv[0], listObj);
    }

    len = Jim_ListLength(interp, listObj);
    if (len % 2) {
        Jim_SetResultString(interp, "list must have an even number of elements", -1);
        return JIM_ERR;
    }
    for (i = 0; i < len && rc == JIM_OK; i += 2) {
        Jim_Obj *nameObj;
        Jim_Obj *valueObj;

        Jim_ListIndex(interp, listObj, i, &nameObj, JIM_NONE);
        Jim_ListIndex(interp, listObj, i + 1, &valueObj, JIM_NONE);

        rc = Jim_SetDictKeysVector(interp, argv[0], &nameObj, 1, valueObj, JIM_ERRMSG);
    }

    return rc;
}

static const jim_subcmd_type array_command_table[] = {
        {       .cmd = "exists",
                .args = "arrayName",
                .function = array_cmd_exists,
                .minargs = 1,
                .maxargs = 1,
                .description = "Does array exist?"
        },
        {       .cmd = "get",
                .args = "arrayName ?pattern?",
                .function = array_cmd_get,
                .minargs = 1,
                .maxargs = 2,
                .description = "Array contents as name value list"
        },
        {       .cmd = "names",
                .args = "arrayName ?pattern?",
                .function = array_cmd_names,
                .minargs = 1,
                .maxargs = 2,
                .description = "Array keys as a list"
        },
        {       .cmd = "set",
                .args = "arrayName list",
                .function = array_cmd_set,
                .minargs = 2,
                .maxargs = 2,
                .description = "Set array from list"
        },
        {       .cmd = "size",
                .args = "arrayName",
                .function = array_cmd_size,
                .minargs = 1,
                .maxargs = 1,
                .description = "Number of elements in array"
        },
        {       .cmd = "unset",
                .args = "arrayName ?pattern?",
                .function = array_cmd_unset,
                .minargs = 1,
                .maxargs = 2,
                .description = "Unset elements of an array"
        },
        {       .cmd = 0,
        }
};

int Jim_arrayInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "array", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "array", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
