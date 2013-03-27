#include <stdio.h>
#include <string.h>

#include "jim-subcmd.h"
#include "jimautoconf.h"

/**
 * Implements the common 'commands' subcommand
 */
static int subcmd_null(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Nothing to do, since the result has already been created */
    return JIM_OK;
}

/**
 * Do-nothing command to support -commands and -usage
 */
static const jim_subcmd_type dummy_subcmd = {
    .cmd = "dummy",
    .function = subcmd_null,
    .flags = JIM_MODFLAG_HIDDEN,
};

static void add_commands(Jim_Interp *interp, const jim_subcmd_type * ct, const char *sep)
{
    const char *s = "";

    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), s, ct->cmd, NULL);
            s = sep;
        }
    }
}

static void bad_subcmd(Jim_Interp *interp, const jim_subcmd_type * command_table, const char *type,
    Jim_Obj *cmd, Jim_Obj *subcmd)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_String(cmd), ", ", type,
        " command \"", Jim_String(subcmd), "\": should be ", NULL);
    add_commands(interp, command_table, ", ");
}

static void show_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * command_table, int argc,
    Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", Jim_String(argv[0]),
        " command ... \", where command is one of: ", NULL);
    add_commands(interp, command_table, ", ");
}

static void add_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * ct, Jim_Obj *cmd)
{
    if (cmd) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_String(cmd), " ", NULL);
    }
    Jim_AppendStrings(interp, Jim_GetResult(interp), ct->cmd, NULL);
    if (ct->args && *ct->args) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", ct->args, NULL);
    }
}

static void show_full_usage(Jim_Interp *interp, const jim_subcmd_type * ct, int argc,
    Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            /* subcmd */
            add_cmd_usage(interp, ct, argv[0]);
            if (ct->description) {
                Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n    ", ct->description, NULL);
            }
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", NULL);
        }
    }
}

static void set_wrong_args(Jim_Interp *interp, const jim_subcmd_type * command_table, Jim_Obj *subcmd)
{
    Jim_SetResultString(interp, "wrong # args: must be \"", -1);
    add_cmd_usage(interp, command_table, subcmd);
    Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);
}

const jim_subcmd_type *Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type * command_table,
    int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct;
    const jim_subcmd_type *partial = 0;
    int cmdlen;
    Jim_Obj *cmd;
    const char *cmdstr;
    const char *cmdname;
    int help = 0;

    cmdname = Jim_String(argv[0]);

    if (argc < 2) {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp), "wrong # args: should be \"", cmdname,
            " command ...\"\n", NULL);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "Use \"", cmdname, " -help\" or \"",
            cmdname, " -help command\" for help", NULL);
        return 0;
    }

    cmd = argv[1];

    if (argc == 2 && Jim_CompareStringImmediate(interp, cmd, "-usage")) {
        /* Show full usage */
        show_full_usage(interp, command_table, argc, argv);
        return &dummy_subcmd;
    }

    /* Check for the help command */
    if (Jim_CompareStringImmediate(interp, cmd, "-help")) {
        if (argc == 2) {
            /* Usage for the command, not the subcommand */
            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        help = 1;

        /* Skip the 'help' command */
        cmd = argv[2];
    }

    /* Check for special builtin '-commands' command first */
    if (Jim_CompareStringImmediate(interp, cmd, "-commands")) {
        /* Build the result here */
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        add_commands(interp, command_table, " ");
        return &dummy_subcmd;
    }

    cmdstr = Jim_GetString(cmd, &cmdlen);

    for (ct = command_table; ct->cmd; ct++) {
        if (Jim_CompareStringImmediate(interp, cmd, ct->cmd)) {
            /* Found an exact match */
            break;
        }
        if (strncmp(cmdstr, ct->cmd, cmdlen) == 0) {
            if (partial) {
                /* Ambiguous */
                if (help) {
                    /* Just show the top level help here */
                    show_cmd_usage(interp, command_table, argc, argv);
                    return &dummy_subcmd;
                }
                bad_subcmd(interp, command_table, "ambiguous", argv[0], argv[1 + help]);
                return 0;
            }
            partial = ct;
        }
        continue;
    }

    /* If we had an unambiguous partial match */
    if (partial && !ct->cmd) {
        ct = partial;
    }

    if (!ct->cmd) {
        /* No matching command */
        if (help) {
            /* Just show the top level help here */
            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        bad_subcmd(interp, command_table, "unknown", argv[0], argv[1 + help]);
        return 0;
    }

    if (help) {
        Jim_SetResultString(interp, "Usage: ", -1);
        /* subcmd */
        add_cmd_usage(interp, ct, argv[0]);
        if (ct->description) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", ct->description, NULL);
        }
        return &dummy_subcmd;
    }

    /* Check the number of args */
    if (argc - 2 < ct->minargs || (ct->maxargs >= 0 && argc - 2 > ct->maxargs)) {
        Jim_SetResultString(interp, "wrong # args: must be \"", -1);
        /* subcmd */
        add_cmd_usage(interp, ct, argv[0]);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);

        return 0;
    }

    /* Good command */
    return ct;
}

int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type * ct, int argc, Jim_Obj *const *argv)
{
    int ret = JIM_ERR;

    if (ct) {
        if (ct->flags & JIM_MODFLAG_FULLARGV) {
            ret = ct->function(interp, argc, argv);
        }
        else {
            ret = ct->function(interp, argc - 2, argv + 2);
        }
        if (ret < 0) {
            set_wrong_args(interp, ct, argv[0]);
            ret = JIM_ERR;
        }
    }
    return ret;
}

int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct =
        Jim_ParseSubCmd(interp, (const jim_subcmd_type *)Jim_CmdPrivData(interp), argc, argv);

    return Jim_CallSubCmd(interp, ct, argc, argv);
}

/* The following two functions are for normal commands */
int
Jim_CheckCmdUsage(Jim_Interp *interp, const jim_subcmd_type * command_table, int argc,
    Jim_Obj *const *argv)
{
    /* -usage or -help */
    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "-usage")
            || Jim_CompareStringImmediate(interp, argv[1], "-help")) {
            Jim_SetResultString(interp, "Usage: ", -1);
            add_cmd_usage(interp, command_table, NULL);
            if (command_table->description) {
                Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", command_table->description,
                    NULL);
            }
            return JIM_OK;
        }
    }
    if (argc >= 2 && command_table->function) {
        /* This is actually a sub command table */

        Jim_Obj *nargv[4];
        int nargc = 0;
        const char *subcmd = NULL;

        if (Jim_CompareStringImmediate(interp, argv[1], "-subcommands")) {
            Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
            add_commands(interp, (jim_subcmd_type *) command_table->function, " ");
            return JIM_OK;
        }

        if (Jim_CompareStringImmediate(interp, argv[1], "-subhelp")
            || Jim_CompareStringImmediate(interp, argv[1], "-help")) {
            subcmd = "-help";
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "-subusage")) {
            subcmd = "-usage";
        }

        if (subcmd) {
            nargv[nargc++] = Jim_NewStringObj(interp, "$handle", -1);
            nargv[nargc++] = Jim_NewStringObj(interp, subcmd, -1);
            if (argc >= 3) {
                nargv[nargc++] = argv[2];
            }
            Jim_ParseSubCmd(interp, (jim_subcmd_type *) command_table->function, nargc, nargv);
            Jim_FreeNewObj(interp, nargv[0]);
            Jim_FreeNewObj(interp, nargv[1]);
            return 0;
        }
    }

    /* Check the number of args */
    if (argc - 1 < command_table->minargs || (command_table->maxargs >= 0
            && argc - 1 > command_table->maxargs)) {
        set_wrong_args(interp, command_table, NULL);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\nUse \"", Jim_String(argv[0]),
            " -help\" for help", NULL);
        return JIM_ERR;
    }

    /* Not usage, but passed arg checking */
    return -1;
}
