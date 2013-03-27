/* Provides a common approach to implementing Tcl commands
 * which implement subcommands
 */
#ifndef JIM_SUBCMD_H
#define JIM_SUBCMD_H

#include <jim.h>

#ifdef __cplusplus
extern "C" {
#endif


#define JIM_MODFLAG_HIDDEN   0x0001		/* Don't show the subcommand in usage or commands */
#define JIM_MODFLAG_FULLARGV 0x0002		/* Subcmd proc gets called with full argv */

/* Custom flags start at 0x0100 */

/**
 * Returns JIM_OK if OK, JIM_ERR (etc.) on error, break, continue, etc.
 * Returns -1 if invalid args.
 */
typedef int tclmod_cmd_function(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

typedef struct {
	const char *cmd;				/* Name of the (sub)command */
	const char *args;				/* Textual description of allowed args */
	tclmod_cmd_function *function;	/* Function implementing the subcommand */
	short minargs;					/* Minimum required arguments */
	short maxargs;					/* Maximum allowed arguments or -1 if no limit */
	unsigned flags;					/* JIM_MODFLAG_... plus custom flags */
	const char *description;		/* Description of the subcommand */
} jim_subcmd_type;

/**
 * Looks up the appropriate subcommand in the given command table and return
 * the command function which implements the subcommand.
 * NULL will be returned and an appropriate error will be set if the subcommand or
 * arguments are invalid.
 *
 * Typical usage is:
 *  {
 *    const jim_subcmd_type *ct = Jim_ParseSubCmd(interp, command_table, argc, argv);
 *
 *    return Jim_CallSubCmd(interp, ct, argc, argv);
 *  }
 *
 */
const jim_subcmd_type *
Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

/**
 * Parses the args against the given command table and executes the subcommand if found
 * or sets an appropriate error if the subcommand or arguments is invalid.
 *
 * Can be used directly with Jim_CreateCommand() where the ClientData is the command table.
 *
 * e.g. Jim_CreateCommand(interp, "mycmd", Jim_SubCmdProc, command_table, NULL);
 */
int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

/**
 * Invokes the given subcmd with the given args as returned
 * by Jim_ParseSubCmd()
 *
 * If ct is NULL, returns JIM_ERR, leaving any message.
 * Otherwise invokes ct->function
 *
 * If ct->function returns -1, sets an error message and returns JIM_ERR.
 * Otherwise returns the result of ct->function.
 */
int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv);

/**
 * Standard processing for a command.
 *
 * This does the '-help' and '-usage' check and the number of args checks.
 * for a top level command against a single 'jim_subcmd_type' structure.
 *
 * Additionally, if command_table->function is set, it should point to a sub command table
 * and '-subhelp ?subcmd?', '-subusage' and '-subcommands' are then also recognised.
 *
 * Returns 0 if user requested usage, -1 on arg error, 1 if OK to process.
 */
int
Jim_CheckCmdUsage(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

#ifdef __cplusplus
}
#endif

#endif
