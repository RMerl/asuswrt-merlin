#include <errno.h>
#include <string.h>
#include "jim.h"
#include "jimautoconf.h"

#ifdef USE_LINENOISE
#include <unistd.h>
#include "linenoise.h"
#else

#define MAX_LINE_LEN 512

static char *linenoise(const char *prompt)
{
    char *line = malloc(MAX_LINE_LEN);

    fputs(prompt, stdout);
    fflush(stdout);

    if (fgets(line, MAX_LINE_LEN, stdin) == NULL) {
        free(line);
        return NULL;
    }
    return line;
}
#endif

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    char *history_file = NULL;
#ifdef USE_LINENOISE
    const char *home;

    home = getenv("HOME");
    if (home && isatty(STDIN_FILENO)) {
        int history_len = strlen(home) + sizeof("/.jim_history");
        history_file = Jim_Alloc(history_len);
        snprintf(history_file, history_len, "%s/.jim_history", home);
        linenoiseHistoryLoad(history_file);
    }
#endif

    printf("Welcome to Jim version %d.%d" JIM_NL,
        JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");

    while (1) {
        Jim_Obj *scriptObjPtr;
        const char *result;
        int reslen;
        char prompt[20];
        const char *str;

        if (retcode != 0) {
            const char *retcodestr = Jim_ReturnCode(retcode);

            if (*retcodestr == '?') {
                snprintf(prompt, sizeof(prompt) - 3, "[%d] ", retcode);
            }
            else {
                snprintf(prompt, sizeof(prompt) - 3, "[%s] ", retcodestr);
            }
        }
        else {
            prompt[0] = '\0';
        }
        strcat(prompt, ". ");

        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while (1) {
            char state;
            int len;
            char *line;

            line = linenoise(prompt);
            if (line == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                goto out;
            }
            if (Jim_Length(scriptObjPtr) != 0) {
                Jim_AppendString(interp, scriptObjPtr, "\n", 1);
            }
            Jim_AppendString(interp, scriptObjPtr, line, -1);
            free(line);
            str = Jim_GetString(scriptObjPtr, &len);
            if (len == 0) {
                continue;
            }
            if (Jim_ScriptIsComplete(str, len, &state))
                break;

            snprintf(prompt, sizeof(prompt), "%c> ", state);
        }
#ifdef USE_LINENOISE
        if (strcmp(str, "h") == 0) {
            /* built-in history command */
            int i;
            int len;
            char **history = linenoiseHistory(&len);
            for (i = 0; i < len; i++) {
                printf("%4d %s\n", i + 1, history[i]);
            }
            Jim_DecrRefCount(interp, scriptObjPtr);
            continue;
        }

        linenoiseHistoryAdd(Jim_String(scriptObjPtr));
        if (history_file) {
            linenoiseHistorySave(history_file);
        }
#endif
        retcode = Jim_EvalObj(interp, scriptObjPtr);
        Jim_DecrRefCount(interp, scriptObjPtr);



        if (retcode == JIM_EXIT) {
            Jim_Free(history_file);
            return JIM_EXIT;
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
        }
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (reslen) {
            printf("%s\n", result);
        }
    }
  out:
    Jim_Free(history_file);
    return JIM_OK;
}
