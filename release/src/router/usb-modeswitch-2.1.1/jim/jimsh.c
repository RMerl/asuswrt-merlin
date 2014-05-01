
/* Jimsh - An interactive shell for Jim
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2009 Steve Bennett <steveb@workware.net.au>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jim.h"
#include "jimautoconf.h"

/* From initjimsh.tcl */
extern int Jim_initjimshInit(Jim_Interp *interp);

static void JimSetArgv(Jim_Interp *interp, int argc, char *const argv[])
{
    int n;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    /* Populate argv global var */
    for (n = 0; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);

        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv", listObj);
    Jim_SetVariableStr(interp, "argc", Jim_NewIntObj(interp, argc));
}

int main(int argc, char *const argv[])
{
    int retcode;
    Jim_Interp *interp;

    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("%d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
        return 0;
    }

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    /* Register static extensions */
    if (Jim_InitStaticExtensions(interp) != JIM_OK) {
        Jim_MakeErrorMessage(interp);
        fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
    }

    Jim_SetVariableStrWithStr(interp, "jim_argv0", argv[0]);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, argc == 1 ? "1" : "0");
    retcode = Jim_initjimshInit(interp);

    if (argc == 1) {
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
            fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
        }
        if (retcode != JIM_EXIT) {
            JimSetArgv(interp, 0, NULL);
            retcode = Jim_InteractivePrompt(interp);
        }
    }
    else {
        if (argc > 2 && strcmp(argv[1], "-e") == 0) {
            JimSetArgv(interp, argc - 3, argv + 3);
            retcode = Jim_Eval(interp, argv[2]);
            if (retcode != JIM_ERR) {
                printf("%s\n", Jim_String(Jim_GetResult(interp)));
            }
        }
        else {
            Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[1], -1));
            JimSetArgv(interp, argc - 2, argv + 2);
            retcode = Jim_EvalFile(interp, argv[1]);
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
            fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
        }
    }
    if (retcode == JIM_EXIT) {
        retcode = Jim_GetExitCode(interp);
    }
    else if (retcode == JIM_ERR) {
        retcode = 1;
    }
    else {
        retcode = 0;
    }
    Jim_FreeInterp(interp);
    return retcode;
}
