
/* Jim - Readline bindings for Jim
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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

#include "jim.h"
#include "jimautoconf.h"

#include <readline/readline.h>
#include <readline/history.h>

static int JimRlReadlineCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *line;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "prompt");
        return JIM_ERR;
    }
    line = readline(Jim_String(argv[1]));
    if (!line) {
        return JIM_EXIT;
    }
    Jim_SetResult(interp, Jim_NewStringObj(interp, line, -1));
    return JIM_OK;
}

static int JimRlAddHistoryCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "string");
        return JIM_ERR;
    }
    add_history(Jim_String(argv[1]));
    return JIM_OK;
}

int Jim_readlineInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "readline", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "readline.readline", JimRlReadlineCommand, NULL, NULL);
    Jim_CreateCommand(interp, "readline.addhistory", JimRlAddHistoryCommand, NULL, NULL);
    return JIM_OK;
}
