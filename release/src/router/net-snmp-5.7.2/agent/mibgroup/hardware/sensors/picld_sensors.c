#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>

#include <time.h>

#include <picl.h>

void netsnmp_sensor_arch_init( void ) {
    DEBUGMSGTL(("sensors:arch", "Initialise PICLd Sensors module\n"));
    picl_initialize();
}


/*
 * Handle a numeric-valued sensor
 */
static int
read_num_sensor( picl_nodehdl_t childh, const char *propval, float *value )
{
    picl_nodehdl_t  sensorh;
    picl_propinfo_t sensor_info;
    picl_errno_t    error_code;

    union valu {
        char buf[PICL_PROPSIZE_MAX];
        uint32_t us4;
        uint16_t us2;
        int32_t is4;
        int16_t is2;
        float f;
    } val;

    /*
     *  Retrieve the specified sensor information and value
     */
    error_code = picl_get_propinfo_by_name(childh, propval, &sensor_info, &sensorh);
    if ( error_code != PICL_SUCCESS ) {
        DEBUGMSGTL(("sensors:arch:detail", "sensor info lookup failed (%d)\n",
                                            error_code));
        return( error_code );
    }

    error_code = picl_get_propval(sensorh, &val.buf, sensor_info.size);
    if ( error_code != PICL_SUCCESS ) {
        DEBUGMSGTL(("sensors:arch:detail", "sensor value lookup failed (%d)\n",
                                            error_code));
        return( error_code );
    }

    /*
     *  Check the validity (type and size) of this value
     */
    if ( sensor_info.type == PICL_PTYPE_FLOAT ) {
        *value = val.f;
    } else if ( sensor_info.type == PICL_PTYPE_UNSIGNED_INT ) {
        /* 16-bit or 32-bit unsigned integers */
        if ( sensor_info.size == 2 ) {
            *value = val.us2;
        } else if ( sensor_info.size == 4 ) { 
            *value = val.us4;
        } else {
            DEBUGMSGTL(("sensors:arch:detail", "unsigned integer (%d bit)\n",
                                                sensor_info.size * 8));
            return PICL_FAILURE;
        }
    } else if ( sensor_info.type == PICL_PTYPE_INT ) {
        /* 16-bit or 32-bit signed integers */
        if ( sensor_info.size == 2 ) {
            *value = val.is2;
        } else if ( sensor_info.size == 4 ) { 
            *value = val.is4;
        } else {
            DEBUGMSGTL(("sensors:arch:detail", "signed integer (%d bit)\n",
                                                sensor_info.size * 8));
            return PICL_FAILURE;
        }
    } else {
        DEBUGMSGTL(("sensors:arch:detail", "unrecognised type (%d)\n",
                                            sensor_info.type));
        return PICL_FAILURE;
    }

    return error_code;
}

static int
process_num_sensor( picl_nodehdl_t childh, const char *propname, const char *propval, int typ )
{
    netsnmp_sensor_info        *sp;
    float                       value;
    picl_errno_t    error_code;

    sp = sensor_by_name( propname, typ );
    if ( !sp ) {
         return -1;
    }

    error_code = read_num_sensor( childh, propval, &value );
    if ( error_code == PICL_SUCCESS ) {
        sp->value = value;
        sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
    } else {
        DEBUGMSGTL(("sensors:arch:detail", "Failed to read %s sensor value (%d)\n",
                                            propname, error_code));
        return -1;
    }
    return 0;
}



/*
 *    Handle an enumeration-valued sensor
 */
const char *switch_settings[] = { "OFF","ON","NORMAL","LOCKED",
				  "UNKNOWN","DIAG","SECURE",
                                  NULL };
const char *led_settings[]    = { "OFF","ON","BLINK",
                                  NULL };
const char *i2c_settings[]    = { "OK",
                                  NULL };

static int
read_enum_sensor( picl_nodehdl_t childh, float *value, const char **options )
{
    picl_nodehdl_t  sensorh;
    picl_propinfo_t sensor_info;
    picl_errno_t    error_code;
    char            state[PICL_PROPSIZE_MAX];
    int             i;

    /*
     *  Retrieve the specified sensor information and value
     */
    error_code = picl_get_propinfo_by_name(childh, "State", &sensor_info, &sensorh);
    if ( error_code != PICL_SUCCESS ) {
        DEBUGMSGTL(("sensors:arch:detail", "sensor info lookup failed (%d)\n",
                                            error_code));
        return( error_code );
    }

    error_code = picl_get_propval(sensorh, state, sensor_info.size);
    if ( error_code != PICL_SUCCESS ) {
        DEBUGMSGTL(("sensors:arch:detail", "sensor value lookup failed (%d)\n",
                                            error_code));
        return( error_code );
    }

    /*
     * Try to find a matching entry in the list of options.
     * Note that some platforms may use upper or lower case
     *   versions of these enumeration values
     *  (so the checks are case insensitive)
     */
    *value = 99;    /* Dummy value */
    for ( i=0;  options[i] != NULL; i++ ) {
        if (strncasecmp(state, options[i], strlen(options[i])) == 0) {
            *value = i;
            return 0;
        }
    }
    
    DEBUGMSGTL(("sensors:arch:detail", "Enumeration state %s not matched\n",
                                        state));
    return 0;  /* Or an error ? */
}

static int
process_enum_sensor( picl_nodehdl_t childh, const char *propname, int typ, const char **options )
{
    netsnmp_sensor_info        *sp;
    float                       value;
    picl_errno_t    error_code;

    sp = sensor_by_name( propname, typ );
    if ( !sp ) {
         return -1;
    }

    error_code = read_enum_sensor( childh, &value, options );
    if ( error_code == PICL_SUCCESS ) {
        sp->value = value;
        sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
    } else {
        DEBUGMSGTL(("sensors:arch:detail", "Failed to read %s sensor value (%d)\n",
                                            propname, error_code));
        return -1;
    }
    return 0;
}



/*
 *  Recursively walk through the tree of sensors
 */
static int
process_sensors( int level, picl_nodehdl_t nodeh ) {
    picl_nodehdl_t childh, nexth;
    char           propname[  PICL_PROPNAMELEN_MAX  ];
    char           propclass[ PICL_CLASSNAMELEN_MAX ];
    picl_errno_t   error_code;

    level++;
    DEBUGMSGTL(("sensors:arch:detail", "process_sensors - level %d\n", level));

    /* Look up the first child node at this level */
    error_code = picl_get_propval_by_name( nodeh, PICL_PROP_CHILD,
                                           &childh, sizeof(childh));
    if ( error_code != PICL_SUCCESS ) {
        DEBUGMSGTL(("sensors:arch:detail", "Failed to get first child node (%d)\n",
                                            error_code));
        return( error_code );
    }

    /* Step through the child nodes, retrieving the name and class of each one */
    while ( error_code == PICL_SUCCESS ) {
        error_code = picl_get_propval_by_name( childh, PICL_PROP_NAME,
                                               propname, sizeof(propname)-1);
        if ( error_code != PICL_SUCCESS ) {
            /* The Node With No Name */
            DEBUGMSGTL(("sensors:arch:detail", "get property name failed (%d)\n",
                                                error_code));
            return( error_code );
        }

        error_code = picl_get_propval_by_name( childh, PICL_PROP_CLASSNAME,
                                               propclass, sizeof(propclass)-1);
        if ( error_code != PICL_SUCCESS ) {
            /* The Classless Society */
            DEBUGMSGTL(("sensors:arch:detail", "get property class failed (%d)\n",
                                                error_code));
            return( error_code );
        }

        DEBUGMSGTL(("sensors:arch:detail", "Name: %s, Class %s\n",
                                            propname, propclass ));


        /*
         *  Three classes represent further groups of sensors, etc.
         *  Call 'process_sensors' recursively to handle this next level
         */
        if (( strstr( propclass, "picl"    )) ||
            ( strstr( propclass, "frutree" )) ||
            ( strstr( propclass, "obp"     ))) {
            process_sensors( level, childh );
        }
        /*
         *  Otherwise retrieve the value appropriately based on the
         *     class of the sensor.
         *
         *  We need to specify the name of the PICL property to retrieve
         *     for this class of sensor, and the Net-SNMP sensor type.
         */
        else if ( strstr( propclass, "fan-tachometer" )) {
            process_num_sensor( childh, propname, "AtoDSensorValue",
                                                   NETSNMP_SENSOR_TYPE_RPM );
        } else if ( strstr( propclass, "fan" )) {
            process_num_sensor( childh, propname, "Speed",
                                                   NETSNMP_SENSOR_TYPE_RPM );
        } else if ( strstr( propclass, "temperature-sensor" )) {
            process_num_sensor( childh, propname, "Temperature",
                                                   NETSNMP_SENSOR_TYPE_TEMPERATURE );
        } else if ( strstr( propclass, "voltage-sensor" )) {
            process_num_sensor( childh, propname, "Voltage",
                                          /* ?? */ NETSNMP_SENSOR_TYPE_VOLTAGE_DC );
        } else if ( strstr( propclass, "digital-sensor" )) {
            process_num_sensor( childh, propname, "AtoDSensorValue",
                                          /* ?? */ NETSNMP_SENSOR_TYPE_VOLTAGE_DC );
            /*
             * Enumeration-valued sensors use a fixed PICL property ("State"),
             *   but take a list of the values appropriate for that sensor,
             *   as well as the Net-SNMP sensor type.
             */
        } else if ( strstr( propclass, "switch" )) {
            process_enum_sensor( childh, propname, NETSNMP_SENSOR_TYPE_OTHER,
                                                   switch_settings );
        } else if ( strstr( propclass, "led" )) {
            process_enum_sensor( childh, propname, NETSNMP_SENSOR_TYPE_OTHER,
                                                   led_settings );
        } else if ( strstr( propclass, "i2c" )) {
            process_enum_sensor( childh, propname, NETSNMP_SENSOR_TYPE_BOOLEAN, /* ?? */
                                                   i2c_settings );
        } else {
            /* Skip other classes of sensor */
            DEBUGMSGTL(("sensors:arch:detail", "Skipping class %s\n", propclass ));
        }

        /*
         *  Move on to the next child node at the current level (if any)
         */
        error_code = picl_get_propval_by_name( childh, PICL_PROP_PEER,
                                               &nexth, sizeof(nexth));
        if ( error_code != PICL_SUCCESS ) {
            /* That's All Folks! */
            return (( error_code == PICL_PROPNOTFOUND )
                          ? PICL_SUCCESS : error_code );
        }
        childh = nexth;
    }
    
    return error_code;
}


int
netsnmp_sensor_arch_load(netsnmp_cache *cache, void *vp) {
    int               error_code;
    picl_nodehdl_t    rooth;

    DEBUGMSGTL(("sensors:arch", "Reload PICLd Sensors module\n"));

    error_code = picl_get_root(&rooth);
    if ( error_code != PICL_SUCCESS) {
        DEBUGMSGTL(("sensors:arch", "Couldn't get root node (error %d)\n", error_code));
        return 1;
    }

    error_code = process_sensors(0, rooth);
    if ( error_code != 255 )
        if ( error_code != 7 )  /* ignore PICL_PROPNOTFOUND error */
            DEBUGMSGTL(("sensors:arch", "Internal PICLd problem (error %d)\n", error_code));

    return 0;
}

void netsnmp_sensor_arch_shutdown( void ) {
    DEBUGMSGTL(("sensors:arch", "Shutdown PicLD Sensors module\n"));
    picl_shutdown();
}

