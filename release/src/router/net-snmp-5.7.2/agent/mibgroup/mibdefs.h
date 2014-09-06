/*
 * 2 global mib defs:
 * ERRORFLAG:  A binary flag to signal an error condition.
 * Also used as exit code.
 * ERRORMSG:  A text message describing what caused the above condition,
 * Also used as the single line return message from programs 
 */

#define MIBINDEX 1
#define ERRORNAME 2
#define ERRORFLAG 100
#define ERRORMSG 101
#define ERRORFIX 102
#define ERRORFIXCMD 103
