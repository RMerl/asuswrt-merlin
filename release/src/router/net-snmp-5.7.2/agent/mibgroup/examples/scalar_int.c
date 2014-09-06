/**  @example scalar_int.c
 *  This example creates some scalar registrations that allows
 *  some simple variables to be accessed via SNMP.  In a more
 *  realistic example, it is likely that these variables would also be
 *  manipulated in other ways outside of SNMP gets/sets.
 *
 *  If this module is compiled into an agent, you should be able to
 *  issue snmp commands that look something like (authentication
 *  information not shown in these commands):
 *
 *  - snmpget localhost netSnmpExampleInteger.0
 *  - netSnmpExampleScalars = 42
 *
 *  - snmpset localhost netSnmpExampleInteger.0 = 1234
 *  - netSnmpExampleScalars = 1234
 *  
 *  - snmpget localhost netSnmpExampleInteger.0
 *  - netSnmpExampleScalars = 1234
 *
 */

/*
 * start be including the appropriate header files 
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * if --enable-minimalist has been turned on, we need to register
 * the support we need so the needed functions aren't removed at compile time
 */
netsnmp_feature_require(long_instance)

/*
 * Then, we declare the variables we want to be accessed 
 */
static long     example1 = 42;  /* default value */

/*
 * our initialization routine, automatically called by the agent 
 * (to get called, the function name must match init_FILENAME())
 */
void
init_scalar_int(void)
{
    /*
     * the OID we want to register our integer at.  This should be a
     * fully qualified instance.  In our case, it's a scalar at:
     * NET-SNMP-EXAMPLES-MIB::netSnmpExampleInteger.0 (note the
     * trailing 0 which is required for any instantiation of any
     * scalar object) 
     */
    oid             my_registration_oid[] =
        { 1, 3, 6, 1, 4, 1, 8072, 2, 1, 1, 0 };

    /*
     * a debugging statement.  Run the agent with -Dexample_scalar_int to see
     * the output of this debugging statement. 
     */
    DEBUGMSGTL(("example_scalar_int",
                "Initalizing example scalar int.  Default value = %ld\n",
                example1));

    /*
     * the line below registers our "example1" variable above as
     * accessible and makes it writable.  A read only version of the
     * same registration would merely call
     * register_read_only_long_instance() instead.
     * 
     * If we wanted a callback when the value was retrieved or set
     * (even though the details of doing this are handled for you),
     * you could change the NULL pointer below to a valid handler
     * function. 
     */
    netsnmp_register_long_instance("my example int variable",
                                  my_registration_oid,
                                  OID_LENGTH(my_registration_oid),
                                  &example1, NULL);

    DEBUGMSGTL(("example_scalar_int",
                "Done initalizing example scalar int\n"));
}
