#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>
#include "hardware/sensors/hw_sensors.h"
#include "ucd-snmp/lmsensorsMib.h"

netsnmp_container *sensorContainer = NULL;

void initialize_lmSensorsTable(const char *tableName, const oid *tableOID,
                               netsnmp_container_op *filter, int mult );

int _sensor_filter_temp( netsnmp_container *c, const void *v );
int _sensor_filter_fan(  netsnmp_container *c, const void *v );
int _sensor_filter_volt( netsnmp_container *c, const void *v );
int _sensor_filter_misc( netsnmp_container *c, const void *v );

static const oid lmTempSensorsTable_oid[]   = {1,3,6,1,4,1,2021,13,16,2};
static const oid lmFanSensorsTable_oid[]    = {1,3,6,1,4,1,2021,13,16,3};
static const oid lmVoltSensorsTable_oid[]   = {1,3,6,1,4,1,2021,13,16,4};
static const oid lmMiscSensorsTable_oid[]   = {1,3,6,1,4,1,2021,13,16,5};
            /* All the tables have the same length root OID */
const size_t lmSensorsTables_oid_len = OID_LENGTH(lmMiscSensorsTable_oid);


/* Initialise the LM Sensors MIB module */
void
init_lmsensorsMib(void)
{
    DEBUGMSGTL(("ucd-snmp/lmsensorsMib","Initializing LM-SENSORS-MIB tables\n"));

    /* 
     * Initialise the four LM-SENSORS-MIB tables
     *
     * They are almost identical, so we can use the same registration code.
     */
    initialize_lmSensorsTable( "lmTempSensorsTable", lmTempSensorsTable_oid,
                                _sensor_filter_temp, 1000 );  /* MIB asks for mC */
    initialize_lmSensorsTable( "lmFanSensorsTable",  lmFanSensorsTable_oid,
                                _sensor_filter_fan,  1);
    initialize_lmSensorsTable( "lmVoltSensorsTable", lmVoltSensorsTable_oid,
                                _sensor_filter_volt, 1000 );  /* MIB asks for mV */
    initialize_lmSensorsTable( "lmMiscSensorsTable", lmMiscSensorsTable_oid,
                                _sensor_filter_misc, 1 );
}

/*
 * Common initialisation code, used for setting up all four tables
 */
void
initialize_lmSensorsTable(const char *tableName, const oid *tableOID,
                          netsnmp_container_op *filter, int mult )
{
    netsnmp_handler_registration    *reg;
    netsnmp_table_registration_info *table_info;
    netsnmp_cache     *cache;
    netsnmp_container *container;

    /*
     * Ensure the HAL sensors module has been initialised,
     *   and retrieve the main sensors container.
     * This table will then be registered using a filter on this container.
     */
    sensorContainer = get_sensor_container();
    if ( !sensorContainer ) {
        init_hw_sensors( );
        sensorContainer = get_sensor_container();
    }
    container = netsnmp_container_find("sensorTable:table_container");
    container->insert_filter = filter;
    netsnmp_container_add_index( sensorContainer, container );


    /*
     * Create a basic registration structure for the table
     */
    reg = netsnmp_create_handler_registration(
               tableName, lmSensorsTables_handler,
               tableOID,  lmSensorsTables_oid_len, HANDLER_CAN_RONLY
              );

    /*
     * Register the table using the filtered container
     * Include an indicator of any scaling to be applied to the sensor value
     */
    reg->my_reg_void = (void *)mult;
    table_info = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes(table_info, ASN_INTEGER, 0);
    table_info->min_column = COLUMN_LMSENSORS_INDEX;
    table_info->max_column = COLUMN_LMSENSORS_VALUE;
    netsnmp_container_table_register( reg, table_info, container, 0 );

    /*
     * If the HAL sensors module was configured as an on-demand caching
     *  module (rather than being automatically loaded regularly),
     *  then ensure this table makes use of that cache.
     */
    cache = get_sensor_cache();
    if ( cache ) {
        netsnmp_inject_handler_before( reg, netsnmp_cache_handler_get( cache ),
                                            "table_container");
    }

}


/*
 *  Container filters for the four tables
 *
 *  Used to ensure that sensor entries appear in the appropriate table.
 */
int _sensor_filter_temp( netsnmp_container *c, const void *v ) {
    const netsnmp_sensor_info *sp = (const netsnmp_sensor_info *)v;
    /* Only matches temperature sensors */
    return (( sp->type == NETSNMP_SENSOR_TYPE_TEMPERATURE ) ? 0 : 1 );
}

int _sensor_filter_fan( netsnmp_container *c, const void *v ) {
    const netsnmp_sensor_info *sp = (const netsnmp_sensor_info *)v;
    /* Only matches fan sensors */
    return (( sp->type == NETSNMP_SENSOR_TYPE_RPM ) ? 0 : 1 );
}

int _sensor_filter_volt( netsnmp_container *c, const void *v ) {
    const netsnmp_sensor_info *sp = (const netsnmp_sensor_info *)v;
    /* Only matches voltage sensors (AC or DC) */
    return ((( sp->type == NETSNMP_SENSOR_TYPE_VOLTAGE_DC ) ||
             ( sp->type == NETSNMP_SENSOR_TYPE_VOLTAGE_AC )) ? 0 : 1 );
}

int _sensor_filter_misc( netsnmp_container *c, const void *v ) {
    const netsnmp_sensor_info *sp = (const netsnmp_sensor_info *)v;
    /* Matches everything except temperature, fan or voltage sensors */
    return ((( sp->type == NETSNMP_SENSOR_TYPE_TEMPERATURE ) ||
             ( sp->type == NETSNMP_SENSOR_TYPE_RPM         ) ||
             ( sp->type == NETSNMP_SENSOR_TYPE_VOLTAGE_DC  ) ||
             ( sp->type == NETSNMP_SENSOR_TYPE_VOLTAGE_AC  )) ? 1 : 0 );
}


/*
 * Handle requests for any of the four lmXxxxSensorsTables 
 *
 * This is possible because all the table share the
 *  same structure and behaviour.
 */
int
lmSensorsTables_handler(
    netsnmp_mib_handler               *handler,
    netsnmp_handler_registration      *reginfo,
    netsnmp_agent_request_info        *reqinfo,
    netsnmp_request_info              *requests) {

    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    netsnmp_sensor_info        *sensor_info;
    int mult  = (int)reginfo->my_reg_void;

    DEBUGMSGTL(( "ucd-snmp/lmsensorsMib","lmSensorsTables_handler - root: "));
    DEBUGMSGOID(("ucd-snmp/lmsensorsMib", reginfo->rootoid, reginfo->rootoid_len));
    DEBUGMSG((   "ucd-snmp/lmsensorsMib",", mode %d\n", reqinfo->mode ));
    /*
     * This is a read-only table, so we only need to handle GET requests.
     *    (The container helper converts GETNEXT->GET requests automatically).
     */
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            sensor_info = (netsnmp_sensor_info *)
                            netsnmp_container_table_extract_context(request);
            if ( !sensor_info ) {
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                continue;
            }
    
            table_info   =  netsnmp_extract_table_info(request);
            switch (table_info->colnum) {
            case COLUMN_LMSENSORS_INDEX:
                snmp_set_var_typed_integer( request->requestvb, ASN_INTEGER,
                                            sensor_info->idx.oids[0]);
                break;
            case COLUMN_LMSENSORS_DEVICE:
                if ( sensor_info->descr[0] != '\0' ) {
                    snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                              sensor_info->descr, strlen(sensor_info->descr));
                } else {
                    snmp_set_var_typed_value( request->requestvb, ASN_OCTET_STR,
                                              sensor_info->name,  strlen(sensor_info->name));
                }
                break;
            case COLUMN_LMSENSORS_VALUE:
                /* Multiply the value by the appropriate scaling factor for this table */
                snmp_set_var_typed_integer( request->requestvb, ASN_GAUGE,
                                            (int)(mult*sensor_info->value));
                break;
            default:
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHOBJECT);
                break;
            }
        }
        break;

    }
    return SNMP_ERR_NOERROR;
}
