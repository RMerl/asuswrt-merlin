#ifndef JIM_NVP_H
#define JIM_NVP_H

#include <jim.h>

/** Name Value Pairs, aka: NVP
 *   -  Given a string - return the associated int.
 *   -  Given a number - return the associated string.
 *   .
 *
 * Very useful when the number is not a simple index into an array of
 * known string, or there may be multiple strings (aliases) that mean then same
 * thing.
 *
 * An NVP Table is terminated with ".name = NULL".
 *
 * During the 'name2value' operation, if no matching string is found
 * the pointer to the terminal element (with p->name == NULL) is returned.
 *
 * Example:
 * \code
 *      const Jim_Nvp yn[] = {
 *          { "yes", 1 },
 *          { "no" , 0 },
 *          { "yep", 1 },
 *          { "nope", 0 },
 *          { NULL, -1 },
 *      };
 *
 *  Jim_Nvp *result
 *  e = Jim_Nvp_name2value(interp, yn, "y", &result);
 *         returns &yn[0];
 *  e = Jim_Nvp_name2value(interp, yn, "n", &result);
 *         returns &yn[1];
 *  e = Jim_Nvp_name2value(interp, yn, "Blah", &result);
 *         returns &yn[4];
 * \endcode
 *
 * During the number2name operation, the first matching value is returned.
 */
typedef struct {
	const char *name;
	int         value;
} Jim_Nvp;


int Jim_GetNvp (Jim_Interp *interp,
									Jim_Obj *objPtr,
									const Jim_Nvp *nvp_table,
									const Jim_Nvp **result);

/* Name Value Pairs Operations */
Jim_Nvp *Jim_Nvp_name2value_simple(const Jim_Nvp *nvp_table, const char *name);
Jim_Nvp *Jim_Nvp_name2value_nocase_simple(const Jim_Nvp *nvp_table, const char *name);
Jim_Nvp *Jim_Nvp_value2name_simple(const Jim_Nvp *nvp_table, int v);

int Jim_Nvp_name2value(Jim_Interp *interp, const Jim_Nvp *nvp_table, const char *name, Jim_Nvp **result);
int Jim_Nvp_name2value_nocase(Jim_Interp *interp, const Jim_Nvp *nvp_table, const char *name, Jim_Nvp **result);
int Jim_Nvp_value2name(Jim_Interp *interp, const Jim_Nvp *nvp_table, int value, Jim_Nvp **result);

int Jim_Nvp_name2value_obj(Jim_Interp *interp, const Jim_Nvp *nvp_table, Jim_Obj *name_obj, Jim_Nvp **result);
int Jim_Nvp_name2value_obj_nocase(Jim_Interp *interp, const Jim_Nvp *nvp_table, Jim_Obj *name_obj, Jim_Nvp **result);
int Jim_Nvp_value2name_obj(Jim_Interp *interp, const Jim_Nvp *nvp_table, Jim_Obj *value_obj, Jim_Nvp **result);

/** prints a nice 'unknown' parameter error message to the 'result' */
void Jim_SetResult_NvpUnknown(Jim_Interp *interp,
												   Jim_Obj *param_name,
												   Jim_Obj *param_value,
												   const Jim_Nvp *nvp_table);


/** Debug: convert argc/argv into a printable string for printf() debug
 *
 * \param interp - the interpeter
 * \param argc   - arg count
 * \param argv   - the objects
 *
 * \returns string pointer holding the text.
 *
 * Note, next call to this function will free the old (last) string.
 *
 * For example might want do this:
 * \code
 *     fp = fopen("some.file.log", "a");
 *     fprintf(fp, "PARAMS are: %s\n", Jim_DebugArgvString(interp, argc, argv));
 *     fclose(fp);
 * \endcode
 */
const char *Jim_Debug_ArgvString(Jim_Interp *interp, int argc, Jim_Obj *const *argv);


/** A TCL -ish GetOpt like code.
 *
 * Some TCL objects have various "configuration" values.
 * For example - in Tcl/Tk the "buttons" have many options.
 *
 * Usefull when dealing with command options.
 * that may come in any order...
 *
 * Does not support "-foo = 123" type options.
 * Only supports tcl type options, like "-foo 123"
 */

typedef struct jim_getopt {
	Jim_Interp     *interp;
	int            argc;
	Jim_Obj        * const * argv;
	int            isconfigure; /* non-zero if configure */
} Jim_GetOptInfo;

/** GetOpt - how to.
 *
 * Example (short and incomplete):
 * \code
 *   Jim_GetOptInfo goi;
 *
 *   Jim_GetOpt_Setup(&goi, interp, argc, argv);
 *
 *   while (goi.argc) {
 *         e = Jim_GetOpt_Nvp(&goi, nvp_options, &n);
 *         if (e != JIM_OK) {
 *               Jim_GetOpt_NvpUnknown(&goi, nvp_options, 0);
 *               return e;
 *         }
 *
 *         switch (n->value) {
 *         case ALIVE:
 *             printf("Option ALIVE specified\n");
 *             break;
 *         case FIRST:
 *             if (goi.argc < 1) {
 *                     .. not enough args error ..
 *             }
 *             Jim_GetOpt_String(&goi, &cp, NULL);
 *             printf("FIRSTNAME: %s\n", cp);
 *         case AGE:
 *             Jim_GetOpt_Wide(&goi, &w);
 *             printf("AGE: %d\n", (int)(w));
 *             break;
 *         case POLITICS:
 *             e = Jim_GetOpt_Nvp(&goi, nvp_politics, &n);
 *             if (e != JIM_OK) {
 *                 Jim_GetOpt_NvpUnknown(&goi, nvp_politics, 1);
 *                 return e;
 *             }
 *         }
 *  }
 *
 * \endcode
 *
 */

/** Setup GETOPT
 *
 * \param goi    - get opt info to be initialized
 * \param interp - jim interp
 * \param argc   - argc count.
 * \param argv   - argv (will be copied)
 *
 * \code
 *     Jim_GetOptInfo  goi;
 *
 *     Jim_GetOptSetup(&goi, interp, argc, argv);
 * \endcode
 */

int Jim_GetOpt_Setup(Jim_GetOptInfo *goi,
											Jim_Interp *interp,
											int argc,
											Jim_Obj * const *  argv);


/** Debug - Dump parameters to stderr
 * \param goi - current parameters
 */
void Jim_GetOpt_Debug(Jim_GetOptInfo *goi);



/** Remove argv[0] from the list.
 *
 * \param goi - get opt info
 * \param puthere - where param is put
 *
 */
int Jim_GetOpt_Obj(Jim_GetOptInfo *goi, Jim_Obj **puthere);

/** Remove argv[0] as string.
 *
 * \param goi     - get opt info
 * \param puthere - where param is put
 * \param len     - return its length
 */
int Jim_GetOpt_String(Jim_GetOptInfo *goi, char **puthere, int *len);

/** Remove argv[0] as double.
 *
 * \param goi     - get opt info
 * \param puthere - where param is put.
 *
 */
int Jim_GetOpt_Double(Jim_GetOptInfo *goi, double *puthere);

/** Remove argv[0] as wide.
 *
 * \param goi     - get opt info
 * \param puthere - where param is put.
 */
int Jim_GetOpt_Wide(Jim_GetOptInfo *goi, jim_wide *puthere);

/** Remove argv[0] as NVP.
 *
 * \param goi     - get opt info
 * \param lookup  - nvp lookup table
 * \param puthere - where param is put.
 *
 */
int Jim_GetOpt_Nvp(Jim_GetOptInfo *goi, const Jim_Nvp *lookup, Jim_Nvp **puthere);

/** Create an appropriate error message for an NVP.
 *
 * \param goi - options info
 * \param lookup - the NVP table that was used.
 * \param hadprefix - 0 or 1 if the option had a prefix.
 *
 * This function will set the "interp->result" to a human readable
 * error message listing the available options.
 *
 * This function assumes the previous option argv[-1] is the unknown string.
 *
 * If this option had some prefix, then pass "hadprefix = 1" else pass "hadprefix = 0"
 *
 * Example:
 * \code
 *
 *  while (goi.argc) {
 *     // Get the next option
 *     e = Jim_GetOpt_Nvp(&goi, cmd_options, &n);
 *     if (e != JIM_OK) {
 *          // option was not recognized
 *          // pass 'hadprefix = 0' because there is no prefix
 *          Jim_GetOpt_NvpUnknown(&goi, cmd_options, 0);
 *          return e;
 *     }
 *
 *     switch (n->value) {
 *     case OPT_SEX:
 *          // handle:  --sex male | female | lots | needmore
 *          e = Jim_GetOpt_Nvp(&goi, &nvp_sex, &n);
 *          if (e != JIM_OK) {
 *               Jim_GetOpt_NvpUnknown(&ogi, nvp_sex, 1);
 *               return e;
 *          }
 *          printf("Code: (%d) is %s\n", n->value, n->name);
 *          break;
 *     case ...:
 *          [snip]
 *     }
 * }
 * \endcode
 *
 */
void Jim_GetOpt_NvpUnknown(Jim_GetOptInfo *goi, const Jim_Nvp *lookup, int hadprefix);


/** Remove argv[0] as Enum
 *
 * \param goi     - get opt info
 * \param lookup  - lookup table.
 * \param puthere - where param is put.
 *
 */
int Jim_GetOpt_Enum(Jim_GetOptInfo *goi, const char * const *  lookup, int *puthere);

#endif
