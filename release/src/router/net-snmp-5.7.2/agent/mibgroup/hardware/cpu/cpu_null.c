/*
 *   dummy HAL CPU module
 *      for systems not using any of the supported interfaces
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

    /*
     * Initialise the HAL CPU system
     *   (including a basic description)
     */
void init_cpu_null( void ) {

    netsnmp_cpu_info  *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name,  "Overall CPU statistics");
    strcpy(cpu->descr, "An electronic chip that makes the computer work");
    strcat(cpu->descr, " (but that's not important right now)");

    cpu = netsnmp_cpu_get_byIdx( 0, 1 );
    strcpy(cpu->name,  "cpu0");
    strcpy(cpu->descr, "An electronic chip that makes the computer work");
    cpu->status = 2;  /* running */

    cpu_num = 1;
}



    /*
     * We can't load the CPU usage statistics
     *   because we don't know how to do this!
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {

    return 0;  /* or -1 ? */
}
