#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>


void netsnmp_sensor_arch_init( void ) {
    /* Nothing to do */
    DEBUGMSGTL(("sensors:arch", "Initialise Dummy Sensors module\n"));
}

int
netsnmp_sensor_arch_load(netsnmp_cache *cache, void *vp) {
    time_t now;
    struct tm                  *tm;
    netsnmp_sensor_info        *sp;

    time(&now);
    tm = localtime(&now);

    DEBUGMSGTL(("sensors:arch", "Reload Dummy Sensors module\n"));

    /* First pseudo-sensor - slowly-rising temperature */
    sp = sensor_by_name( "minute", NETSNMP_SENSOR_TYPE_TEMPERATURE );
    sp->value = tm->tm_min;
    snprintf( sp->descr, 256, "Minute-based pseudo-sensor - slowly-rising temperature" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    /* Second pseudo-sensor - quickly-rising temperature */
    sp = sensor_by_name( "second", NETSNMP_SENSOR_TYPE_TEMPERATURE );
    sp->value = tm->tm_sec;
    snprintf( sp->descr, 256, "Second-based pseudo-sensor - quickly-rising temperature" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    /* Third pseudo-sensor - annual fan speed */
    sp = sensor_by_name( "year", NETSNMP_SENSOR_TYPE_RPM );
    sp->value = tm->tm_year + 1900;
    snprintf( sp->descr, 256, "RPM pseudo-sensor - annual fan speed" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    /* Fourth pseudo-sensor - daily voltage */
    sp = sensor_by_name( "day", NETSNMP_SENSOR_TYPE_VOLTAGE_DC );
    sp->value = tm->tm_mday-20;
    snprintf( sp->descr, 256, "Day-based pseudo-sensor - positive or negative voltage" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    /* Fifth pseudo-sensor - monthly voltage */
    sp = sensor_by_name( "month", NETSNMP_SENSOR_TYPE_VOLTAGE_DC );
    sp->value = tm->tm_mon;
    snprintf( sp->descr, 256, "Month-based pseudo-sensor - positive voltage" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    /* Sixth pseudo-sensor - annual daily something */
    sp = sensor_by_name( "yday", NETSNMP_SENSOR_TYPE_OTHER );
    sp->value = tm->tm_yday;
    snprintf( sp->descr, 256, "Day-based pseudo-sensor - annual something" );
    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;

    return 0;
}
