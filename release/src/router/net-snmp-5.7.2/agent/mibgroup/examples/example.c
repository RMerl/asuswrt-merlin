/*
 *  Template MIB group implementation - example.c
 *
 */

/*
 * include important headers 
 */
#include <net-snmp/net-snmp-config.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * header_generic() comes from here 
 */
#include "util_funcs/header_generic.h"

/*
 * include our .h file 
 */
#include "example.h"


   /*
    *  Certain objects can be set via configuration file directives.
    *  These variables hold the values for such objects, as they need to
    *   be accessible to both the config handlers, and the callback routine.
    */
#define EXAMPLE_STR_LEN	300
#define EXAMPLE_STR_DEFAULT	"life the universe and everything"
int             example_int = 42;
char            example_str[EXAMPLE_STR_LEN];

        /*
         * Forward declarations for the config handlers 
         */
void            example_parse_config_exampleint(const char *token,
                                                char *cptr);
void            example_parse_config_examplestr(const char *token,
                                                char *cptr);
void            example_free_config_exampleint(void);
void            example_free_config_examplestr(void);


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

    /*
     * This array structure defines a representation of the
     *  MIB being implemented.
     *
     * The type of the array is 'struct variableN', where N is
     *  large enough to contain the longest OID sub-component
     *  being loaded.  This will normally be the maximum value
     *  of the fifth field in each line.  In this case, the second
     *  and third entries are both of size 2, so we're using
     *  'struct variable2'
     *
     * The supported values for N are listed in <agent/var_struct.h>
     *  If the value you need is not listed there, simply use the
     *  next largest that is.
     *
     * The format of each line is as follows
     *  (using the first entry as an example):
     *      1: EXAMPLESTRING:
     *          The magic number defined in the example header file.
     *          This is passed to the callback routine and is used
     *            to determine which object is being queried.
     *      2: ASN_OCTET_STR:
     *          The type of the object.
     *          Valid types are listed in <snmp_impl.h>
     *      3: NETSNMP_OLDAPI_RONLY (or NETSNMP_OLDAPI_RWRITE):
     *          Whether this object can be SET or not.
     *      4: var_example:
     *          The callback routine, used when the object is queried.
     *          This will usually be the same for all objects in a module
     *            and is typically defined later in this file.
     *      5: 1:
     *          The length of the OID sub-component (the next field)
     *      6: {1}:
     *          The OID sub-components of this entry.
     *          In other words, the bits of the full OID that differ
     *            between the various entries of this array.
     *          This value is appended to the common prefix (defined later)
     *            to obtain the full OID of each entry.
     */
struct variable2 example_variables[] = {
    {EXAMPLESTRING, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_example, 1, {1}},
    {EXAMPLEINTEGER, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_example, 2, {2, 1}},
    {EXAMPLEOBJECTID, ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_example, 2, {2, 2}},
    {EXAMPLETIMETICKS, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_example, 1, {3}},
    {EXAMPLEIPADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_example, 1, {4}},
    {EXAMPLECOUNTER, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_example, 1, {5}},
    {EXAMPLEGAUGE, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_example, 1, {6}},
    {EXAMPLETRIGGERTRAP, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_example, 1, {7}},
    {EXAMPLETRIGGERTRAP2, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_example, 1, {8}}
};

    /*
     * This array defines the OID of the top of the mib tree that we're
     *  registering underneath.
     * Note that this needs to be the correct size for the OID being 
     *  registered, so that the length of the OID can be calculated.
     *  The format given here is the simplest way to achieve this.
     */
oid             example_variables_oid[] = { 1, 3, 6, 1, 4, 1, 2021, 254 };



    /*
     * This function is called at the time the agent starts up
     *  to do any initializations that might be required.
     *
     * In theory it is optional and can be omitted if no
     *  initialization is needed.  In practise, every module
     *  will need to register itself (or the objects being
     *  implemented will not appear in the MIB tree), and this
     *  registration is typically done here.
     *
     * If this function is added or removed, you must re-run
     *  the configure script, to detect this change.
     */
void
init_example(void)
{
    /*
     * Register ourselves with the agent to handle our mib tree.
     * The arguments are:
     *    descr:   A short description of the mib group being loaded.
     *    var:     The variable structure to load.
     *                  (the name of the variable structure defined above)
     *    vartype: The type of this variable structure
     *    theoid:  The OID pointer this MIB is being registered underneath.
     */
    REGISTER_MIB("example", example_variables, variable2,
                 example_variables_oid);


    /*
     *  Register config handlers for the two objects that can be set
     *   via configuration file directive.
     *  Also set a default value for the string object.  Note that the
     *   example integer variable was initialised above.
     */
    strlcpy(example_str, EXAMPLE_STR_DEFAULT, sizeof(example_str));

    snmpd_register_config_handler("exampleint",
                                  example_parse_config_exampleint,
                                  example_free_config_exampleint,
                                  "exampleint value");
    snmpd_register_config_handler("examplestr",
                                  example_parse_config_examplestr,
                                  example_free_config_examplestr,
                                  "examplestr value");
    snmpd_register_config_handler("examplestring",
                                  example_parse_config_examplestr,
                                  example_free_config_examplestr,
                                  "examplestring value");

    /*
     * One common requirement is to read values from the kernel.
     * This is usually initialised here, to speed up access when the
     *  information is read in, as a response to an incoming request.
     *
     * This module doesn't actually use this mechanism,
     * so this call is commented out here.
     */
    /*
     * auto_nlist( "example_symbol", 0, 0 ); 
     */
}

        /*********************
	 *
	 *  Configuration file handling functions
	 *
	 *********************/

void
example_parse_config_exampleint(const char *token, char *cptr)
{
    example_int = atoi(cptr);
}

void
example_parse_config_examplestr(const char *token, char *cptr)
{
    /*
     * Make sure the string fits in the space allocated for it.
     */
    if (strlen(cptr) < sizeof(example_str))
        strcpy(example_str, cptr);
    else {
        /*
         * Truncate the string if necessary.
         * An alternative approach would be to log an error,
         *  and discard this value altogether.
         */
        sprintf(example_str, "%.*s...", (int) (sizeof(example_str) - 4), cptr);
        netsnmp_assert(strlen(example_str) < sizeof(example_str));
    }
}

        /*
         * We don't need to do anything special when closing down 
         */
void
example_free_config_exampleint(void)
{
}

void
example_free_config_examplestr(void)
{
}

        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

    /*
     * Define the callback function used in the example_variables structure.
     * This is called whenever an incoming request refers to an object
     *  within this sub-tree.
     *
     * Four of the parameters are used to pass information in.
     * These are:
     *    vp      The entry from the 'example_variables' array for the
     *             object being queried.
     *    name    The OID from the request.
     *    length  The length of this OID.
     *    exact   A flag to indicate whether this is an 'exact' request
     *             (GET/SET) or an 'inexact' one (GETNEXT/GETBULK).
     *
     * Four of the parameters are used to pass information back out.
     * These are:
     *    name     The OID being returned.
     *    length   The length of this OID.
     *    var_len  The length of the answer being returned.
     *    write_method   A pointer to the SET function for this object.
     *
     * Note that name & length serve a dual purpose in both roles.
     */

u_char         *
var_example(struct variable *vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     *  The result returned from this function needs to be a pointer to
     *    static data (so that it can be accessed from outside).
     *  Define suitable variables for any type of data we may return.
     */
    static char     string[EXAMPLE_STR_LEN];    /* for EXAMPLESTRING   */
    static oid      oid_ret[8]; /* for EXAMPLEOBJECTID */
    static long     long_ret;   /* for everything else */

    /*
     * Before returning an answer, we need to check that the request
     *  refers to a valid instance of this object.  The utility routine
     *  'header_generic' can be used to do this for scalar objects.
     *
     * This routine 'header_simple_table' does the same thing for "simple"
     *  tables. (See the AGENT.txt file for the definition of a simple table).
     *
     * Both these utility routines also set up default values for the
     *  return arguments (assuming the check succeeded).
     * The name and length are set suitably for the current object,
     *  var_len assumes that the result is an integer of some form,
     *  and write_method assumes that the object cannot be set.
     *
     * If these assumptions are correct, this callback routine simply
     * needs to return a pointer to the appropriate value (using 'long_ret').
     * Otherwise, 'var_len' and/or 'write_method' should be set suitably.
     */
    DEBUGMSGTL(("example", "var_example entered\n"));
    if (header_generic(vp, name, length, exact, var_len, write_method) ==
        MATCH_FAILED)
        return NULL;


    /*
     * Many object will need to obtain data from the operating system in
     *  order to return the appropriate value.  Typically, this is done
     *  here - immediately following the 'header' call, and before the
     *  switch statement. This is particularly appropriate if a single 
     *  interface call can return data for all the objects supported.
     *
     * This example module does not rely on external data, so no such
     *  calls are needed in this case.  
     */

    /*
     * Now use the magic number from the variable pointer 'vp' to
     *  select the particular object being queried.
     * In each case, one of the static objects is set up with the
     *  appropriate information, and returned mapped to a 'u_char *'
     */
    switch (vp->magic) {
    case EXAMPLESTRING:
        strcpy(string, example_str);
        /*
         * Note that the assumption that the answer will be an
         *  integer does not hold true in this case, so the length
         *  of the answer needs to be set explicitly.           
         */
        *var_len = strlen(string);
        return (u_char *) string;

    case EXAMPLEINTEGER:
        /*
         * Here the length assumption is correct, but the
         *  object is writeable, so we need to set the
         *  write_method pointer as well as the current value.
         */
        long_ret = example_int;
        *write_method = write_exampleint;
        return (u_char *) & long_ret;

    case EXAMPLEOBJECTID:
        oid_ret[0] = 1;
        oid_ret[1] = 3;
        oid_ret[2] = 6;
        oid_ret[3] = 1;
        oid_ret[4] = 4;
        oid_ret[5] = oid_ret[6] = oid_ret[7] = 42;
        /*
         * Again, the assumption regarding the answer length is wrong.
         */
        *var_len = 8 * sizeof(oid);
        return (u_char *) oid_ret;

    case EXAMPLETIMETICKS:
        /*
         * Here both assumptions are correct,
         *  so we just need to set up the answer.
         */
        long_ret = 363136200;   /* 42 days, 42 minutes and 42.0 seconds */
        return (u_char *) & long_ret;

    case EXAMPLEIPADDRESS:
        /*
         * ipaddresses get returned as a long.  ick 
         */
        /*
         * we're returning 127.0.0.1 
         */
        long_ret = ntohl(INADDR_LOOPBACK);
        return (u_char *) & long_ret;

    case EXAMPLECOUNTER:
        long_ret = 42;
        return (u_char *) & long_ret;

    case EXAMPLEGAUGE:
        long_ret = 42;          /* Do we detect a theme running through these answers? */
        return (u_char *) & long_ret;

    case EXAMPLETRIGGERTRAP:
        /*
         * This object is essentially "write-only".
         * It only exists to trigger the sending of a trap.
         * Reading it will always return 0.
         */
        long_ret = 0;
        *write_method = write_exampletrap;
        return (u_char *) & long_ret;

    case EXAMPLETRIGGERTRAP2:
        /*
         * This object is essentially "write-only".
         * It only exists to trigger the sending of a v2 trap.
         * Reading it will always return 0.
         */
        long_ret = 0;
        *write_method = write_exampletrap2;
        return (u_char *) & long_ret;

    default:
        /*
         *  This will only be triggered if there's a problem with
         *   the coding of the module.  SNMP requests that reference
         *   a non-existant OID will be directed elsewhere.
         *  If this branch is reached, log an error, so that
         *   the problem can be investigated.
         */
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in examples/var_example\n",
                    vp->magic));
    }
    /*
     * If we fall through to here, fail by returning NULL.
     * This is essentially a continuation of the 'default' case above.
     */
    return NULL;
}

        /*********************
	 *
	 *  Writeable object SET handling routines
	 *
	 *********************/
int
write_exampleint(int action,
                 u_char * var_val,
                 u_char var_val_type,
                 size_t var_val_len,
                 u_char * statP, oid * name, size_t name_len)
{
    /*
     * Define an arbitrary maximum permissible value 
     */
#define MAX_EXAMPLE_INT	100
    static long     intval;
    static long     old_intval;

    switch (action) {
    case RESERVE1:
        /*
         *  Check that the value being set is acceptable
         */
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("example", "%x not integer type", var_val_type));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > sizeof(long)) {
            DEBUGMSGTL(("example", "wrong length %" NETSNMP_PRIz "u",
                        var_val_len));
            return SNMP_ERR_WRONGLENGTH;
        }

        intval = *((long *) var_val);
        if (intval > MAX_EXAMPLE_INT) {
            DEBUGMSGTL(("example", "wrong value %lx", intval));
            return SNMP_ERR_WRONGVALUE;
        }
        break;

    case RESERVE2:
        /*
         *  This is conventially where any necesary
         *   resources are allocated (e.g. calls to malloc)
         *  Here, we are using static variables
         *   so don't need to worry about this.
         */
        break;

    case FREE:
        /*
         *  This is where any of the above resources
         *   are freed again (because one of the other
         *   values being SET failed for some reason).
         *  Again, since we are using static variables
         *   we don't need to worry about this either.
         */
        break;

    case ACTION:
        /*
         *  Set the variable as requested.
         *   Note that this may need to be reversed,
         *   so save any information needed to do this.
         */
        old_intval = example_int;
        example_int = intval;
        break;

    case UNDO:
        /*
         *  Something failed, so re-set the
         *   variable to its previous value
         *  (and free any allocated resources).
         */
        example_int = old_intval;
        break;

    case COMMIT:
        /*
         *  Everything worked, so we can discard any
         *   saved information, and make the change
         *   permanent (e.g. write to the config file).
         *  We also free any allocated resources.
         *
         *  In this case, there's nothing to do.
         */
        break;

    }
    return SNMP_ERR_NOERROR;
}

int
write_exampletrap(int action,
                  u_char * var_val,
                  u_char var_val_type,
                  size_t var_val_len,
                  u_char * statP, oid * name, size_t name_len)
{
    long            intval;

    DEBUGMSGTL(("example", "write_exampletrap entered: action=%d\n",
                action));
    switch (action) {
    case RESERVE1:
        /*
         *  The only acceptable value is the integer 1
         */
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("example", "%x not integer type", var_val_type));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > sizeof(long)) {
            DEBUGMSGTL(("example", "wrong length %" NETSNMP_PRIz "u",
                        var_val_len));
            return SNMP_ERR_WRONGLENGTH;
        }

        intval = *((long *) var_val);
        if (intval != 1) {
            DEBUGMSGTL(("example", "wrong value %lx", intval));
            return SNMP_ERR_WRONGVALUE;
        }
        break;

    case RESERVE2:
        /*
         * No resources are required.... 
         */
        break;

    case FREE:
        /*
         * ... so no resources need be freed 
         */
        break;

    case ACTION:
        /*
         *  Having triggered the sending of a trap,
         *   it would be impossible to revoke this,
         *   so we can't actually invoke the action here.
         */
        break;

    case UNDO:
        /*
         * We haven't done anything yet,
         * so there's nothing to undo 
         */
        break;

    case COMMIT:
        /*
         *  Everything else worked, so it's now safe
         *   to trigger the trap.
         *  Note that this is *only* acceptable since
         *   the trap sending routines are "failsafe".
         *  (In fact, they can fail, but they return no
         *   indication of this, which is the next best thing!)
         */
        DEBUGMSGTL(("example", "write_exampletrap sending the trap\n"));
        send_easy_trap(SNMP_TRAP_ENTERPRISESPECIFIC, 99);
        DEBUGMSGTL(("example", "write_exampletrap trap sent\n"));
        break;

    }
    return SNMP_ERR_NOERROR;
}

/*
 * this documents how to send a SNMPv2 (and higher) trap via the
 * send_v2trap() API.
 * 
 * Coding SNMP-v2 Trap:
 * 
 * The SNMPv2-Trap PDU contains at least a pair of object names and
 * values: - sysUpTime.0 whose value is the time in hundredths of a
 * second since the netwok management portion of system was last
 * reinitialized.  - snmpTrapOID.0 which is part of the trap group SNMPv2
 * MIB whose value is the object-id of the specific trap you have defined
 * in your own MIB.  Other variables can be added to caracterize the
 * trap.
 * 
 * The function send_v2trap adds automaticallys the two objects but the
 * value of snmpTrapOID.0 is 0.0 by default. If you want to add your trap
 * name, you have to reconstruct this object and to add your own
 * variable.
 * 
 */



int
write_exampletrap2(int action,
                   u_char * var_val,
                   u_char var_val_type,
                   size_t var_val_len,
                   u_char * statP, oid * name, size_t name_len)
{
    long            intval;

    /*
     * these variales will be used when we send the trap 
     */
    oid             objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };     /* snmpTrapOID.0 */
    oid             demo_trap[] = { 1, 3, 6, 1, 4, 1, 2021, 13, 990 };  /*demo-trap */
    oid             example_string_oid[] =
        { 1, 3, 6, 1, 4, 1, 2021, 254, 1, 0 };
    static netsnmp_variable_list var_trap;
    static netsnmp_variable_list var_obj;

    DEBUGMSGTL(("example", "write_exampletrap2 entered: action=%d\n",
                action));
    switch (action) {
    case RESERVE1:
        /*
         *  The only acceptable value is the integer 1
         */
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("example", "%x not integer type", var_val_type));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > sizeof(long)) {
            DEBUGMSGTL(("example", "wrong length %" NETSNMP_PRIz "u",
                        var_val_len));
            return SNMP_ERR_WRONGLENGTH;
        }

        intval = *((long *) var_val);
        if (intval != 1) {
            DEBUGMSGTL(("example", "wrong value %lx", intval));
            return SNMP_ERR_WRONGVALUE;
        }
        break;

    case RESERVE2:
        /*
         * No resources are required.... 
         */
        break;

    case FREE:
        /*
         * ... so no resources need be freed 
         */
        break;

    case ACTION:
        /*
         *  Having triggered the sending of a trap,
         *   it would be impossible to revoke this,
         *   so we can't actually invoke the action here.
         */
        break;

    case UNDO:
        /*
         * We haven't done anything yet,
         * so there's nothing to undo 
         */
        break;

    case COMMIT:
        /*
         *  Everything else worked, so it's now safe
         *   to trigger the trap.
         *  Note that this is *only* acceptable since
         *   the trap sending routines are "failsafe".
         *  (In fact, they can fail, but they return no
         *   indication of this, which is the next best thing!)
         */

        /*
         * trap definition objects 
         */

        var_trap.next_variable = &var_obj;      /* next variable */
        var_trap.name = objid_snmptrap; /* snmpTrapOID.0 */
        var_trap.name_length = sizeof(objid_snmptrap) / sizeof(oid);    /* number of sub-ids */
        var_trap.type = ASN_OBJECT_ID;
        var_trap.val.objid = demo_trap; /* demo-trap objid */
        var_trap.val_len = sizeof(demo_trap);   /* length in bytes (not number of subids!) */


        /*
         * additional objects 
         */


        var_obj.next_variable = NULL;   /* No more variables after this one */
        var_obj.name = example_string_oid;
        var_obj.name_length = sizeof(example_string_oid) / sizeof(oid); /* number of sub-ids */
        var_obj.type = ASN_OCTET_STR;   /* type of variable */
        var_obj.val.string = (unsigned char *) example_str;       /* value */
        var_obj.val_len = strlen(example_str);
        DEBUGMSGTL(("example", "write_exampletrap2 sending the v2 trap\n"));
        send_v2trap(&var_trap);
        DEBUGMSGTL(("example", "write_exampletrap2 v2 trap sent\n"));

        break;

    }
    return SNMP_ERR_NOERROR;
}
