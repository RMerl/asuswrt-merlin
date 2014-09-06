#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>


extern NetsnmpCacheLoad netsnmp_sensor_arch_load;
extern void             netsnmp_sensor_arch_init( void );
static int  _sensor_load( void );
static void _sensor_free( void );

static int _sensorAutoUpdate = 0;   /* 0 means on-demand caching */
static void _sensor_update_stats( unsigned int, void* );

netsnmp_cache     *_sensor_cache     = NULL;
netsnmp_container *_sensor_container = NULL;
static int         _sensor_idx       = 0;

void init_hw_sensors( void ) {

    if ( _sensor_container )
        return;   /* Already initialised */

    DEBUGMSGTL(("sensors", "Initialise Hardware Sensors module\n"));

    /*
     * Define a container to hold the basic list of sensors
     * The four LM-SENSOR-MIB containers will be created in
     *  the relevant initialisation routine(s)
     */
    _sensor_container = netsnmp_container_find("sensorTable:table_container");
    if ( NULL == _sensor_container ) {
        snmp_log( LOG_ERR, "failed to create container for sensorTable");
        return;
    }
    netsnmp_sensor_arch_init( );

    /*
     * If we're sampling the sensor information automatically,
     *   then arrange for this to be triggered regularly.
     *
     * If we're not sampling these values regularly,
     *   create a suitable cache handler instead.
     */
    if ( _sensorAutoUpdate ) {
        DEBUGMSGTL(("sensors", "Reloading Hardware Sensors automatically (%d)\n",
                               _sensorAutoUpdate));
        snmp_alarm_register( _sensorAutoUpdate, SA_REPEAT,
                             _sensor_update_stats, NULL );
    }
    else {
        _sensor_cache = netsnmp_cache_create( 5, netsnmp_sensor_load,
                                                 netsnmp_sensor_free, NULL, 0 );
        DEBUGMSGTL(("sensors", "Reloading Hardware Sensors on-demand (%p)\n",
                               _sensor_cache));
    }
}

void shutdown_hw_sensors( void ) {
    _sensor_free();
}

/*
 *  Return the main sensor container
 */
netsnmp_container *get_sensor_container( void ) { return _sensor_container; }

/*
 *  Return the main sensor cache control structure (if defined)
 */
netsnmp_cache *get_sensor_cache( void ) { return _sensor_cache; }


/*
 * Wrapper routine for automatically updating sensor statistics
 */
void
_sensor_update_stats( unsigned int clientreg, void *data )
{
    _sensor_free();
    _sensor_load();
}

/*
 * Wrapper routine for re-loading sensor statistics on demand
 */
int
netsnmp_sensor_load( netsnmp_cache *cache, void *data )
{
    return _sensor_load();
}

/*
 * Wrapper routine for releasing expired sensor statistics
 */
void
netsnmp_sensor_free( netsnmp_cache *cache, void *data )
{
    _sensor_free();
}


/*
 * Architecture-independent processing of loading sensor statistics
 */
static int
_sensor_load( void )
{
    netsnmp_sensor_arch_load( NULL, NULL );
    return 0;
}

/*
 * Architecture-independent release of sensor statistics
 */
static void
_sensor_free( void )
{
    netsnmp_sensor_info *sp;

    for (sp = CONTAINER_FIRST( _sensor_container );
         sp;
         sp = CONTAINER_NEXT(  _sensor_container, sp )) {

         sp->flags &= ~ NETSNMP_SENSOR_FLAG_ACTIVE;
    }
}


/*
 * Retrieve a sensor entry by name,
 *  or (optionally) insert a new one into the container
 */
netsnmp_sensor_info *
sensor_by_name( const char *name, int create_type )
{
    netsnmp_sensor_info *sp;

    DEBUGMSGTL(("sensors:name", "Get sensor entry (%s)\n", name));

    /*
     *  Look through the list for a matching entry
     */
        /* .. or use a secondary index container ?? */
    for (sp = CONTAINER_FIRST( _sensor_container );
         sp;
         sp = CONTAINER_NEXT(  _sensor_container, sp )) {

        if ( !strcmp( name, sp->name ))
            return sp;
    }

    /*
     * Not found...
     */
    if ( create_type == NETSNMP_SENSOR_FIND_EXIST ) {
        DEBUGMSGTL(("sensors:name", "No such sensor entry\n"));
        return NULL;
    }

    /*
     * ... so let's create a new one, using the type supplied
     */
    sp = SNMP_MALLOC_TYPEDEF( netsnmp_sensor_info );
    if ( sp ) {
        if (strlen(name) >= sizeof(sp->name)) {
            snmp_log(LOG_ERR, "Sensor name is too large: %s\n", name);
            free(sp);
            return NULL;
        }
        strcpy( sp->name, name );
        sp->type = create_type;
        /*
         * Set up the index value.
         *  
         * All this trouble, just for a simple integer.
         * Surely there must be a better way?
         */
        sp->idx.len  = 1;
        sp->idx.oids = SNMP_MALLOC_TYPEDEF( oid );
        sp->idx.oids[0] = ++_sensor_idx;
    }

    DEBUGMSGTL(("sensors:name", "Create sensor entry (type = %d, index = %d\n",
                                 create_type, _sensor_idx));
    CONTAINER_INSERT( _sensor_container, sp );
    return sp;
}

