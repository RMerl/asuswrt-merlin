/*
 * Hardware Abstraction Layer - Sensors module
 *
 * Public interface
 */

#define NETSNMP_SENSOR_TYPE_OTHER       1
#define NETSNMP_SENSOR_TYPE_VOLTAGE_AC  3
#define NETSNMP_SENSOR_TYPE_VOLTAGE_DC  4
#define NETSNMP_SENSOR_TYPE_CURRENT     5
#define NETSNMP_SENSOR_TYPE_POWER       6
#define NETSNMP_SENSOR_TYPE_FREQUENCY   7
#define NETSNMP_SENSOR_TYPE_TEMPERATURE 8
#define NETSNMP_SENSOR_TYPE_HUMIDITY    9
#define NETSNMP_SENSOR_TYPE_RPM        10
#define NETSNMP_SENSOR_TYPE_VOLUME     11
#define NETSNMP_SENSOR_TYPE_BOOLEAN    12


#define NETSNMP_SENSOR_FLAG_ACTIVE     0x01
#define NETSNMP_SENSOR_FLAG_NAVAIL     0x02
#define NETSNMP_SENSOR_FLAG_BROKEN     0x04
#define NETSNMP_SENSOR_FLAG_DISABLE    0x08

#define NETSNMP_SENSOR_MASK_STATUS     0x06  /* NAVAIL|BROKEN */


#define NETSNMP_SENSOR_FIND_CREATE     1   /* or use one of the sensor type values */
#define NETSNMP_SENSOR_FIND_EXIST      0

typedef struct netsnmp_sensor_info_s netsnmp_sensor_info;
struct netsnmp_sensor_info_s {

    netsnmp_index  idx;
    /* int  idx; */
    char  name[256];
    
    int   type;
    float value;
    char  descr[256];
    long  flags;
};

netsnmp_container   *get_sensor_container( void );
netsnmp_cache       *get_sensor_cache( void );
netsnmp_sensor_info *sensor_by_name( const char *, int );
NetsnmpCacheLoad     netsnmp_sensor_load;
NetsnmpCacheFree     netsnmp_sensor_free;
