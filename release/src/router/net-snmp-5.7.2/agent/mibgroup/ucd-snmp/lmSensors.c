/* lmSensors.c
 *
 * Sections of this code were derived from the published API's of 
 * some Sun products.  Hence, portions of the code may be copyright
 * Sun Microsystems.
 *
 * Additional code provided by Mike Fisher and Thomas E. Lackley
 *
 * This component allows net-snmp to report sensor information.
 *
 * In order to use it, the ./configure invocation must include...
 *
 * --with-mib-modules="ucd-snmp/lmSensors"
 *
 * It uses one of three different methodologies.  Some platforms make
 * use of an lm_sensors driver to access the information on the
 * health monitoring hardware, such as the LM75 and LM78 chips.
 *
 * For further information see http://secure.netroedge.com/~lm78/
 *
 * The Solaris platform uses the other two methodologies.  Earlier
 * platforms such as the Enterprise 450 use kstat to report sensor
 * information.  Later platforms, such as the V880 use the picld
 * daemon to control system resources and report sensor information.
 * Picld is supported only on Solaris 2.8 and later.
 *
 * Both these methodologies are implemented in a "read only" manner.
 * You cannot use this code to change anything eg. fan speeds.
 *
 * The lmSensors component delivers the information documented in the
 * LM-SENSORS-MIB.  The information is divided up as follows:
 *
 * -temperatures (in thousandsths of a Celsius degree)
 * -fans (rpm's)
 * -voltages (in milliVolts)
 * -other (switches, LEDs and  i2c's (things that use the i2c bus))
 * NOTE: This version does not support gpio's.  Still on the learning curve.
 *
 * Because the MIB only allows output of the datatype Gauge32 this
 * limits the amount of meaningful information that can be delivered
 * from "other" sensors.  Hence, the code does a certain amount of
 * translating.  See the source for individual sensor types.
 *
 * If an "other" sensor delivers a value 99, it means that it
 * is delivering a "status" that the code does not account for.
 * If you discover one of these, please pass it on and I'll
 * put it in.
 *
 * It was recently discovered that the sensors code had not be following
 * the MIB for some sensors.  The MIB required reporting some items
 * in mV and mC.  These changes have been noted in the source.
 *
 * To see debugging messages, run the daemon as follows:
 * 
 * /usr/local/sbin/snmpd -f -L -Ducd-snmp/lmSensors
 * (change path to wherever you installed it)
 *
 * or using gdb:
 *
 * gdb snmpd
 * run -f -L -Ducd-snmp/lmSensors
 *
 * The component can record up to 256 instances of each type.
 *
 * The following should always be included first before anything else 
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * minimal include directives 
 */

#include "util_funcs/header_simple_table.h"
#include <time.h>

netsnmp_feature_require(table_container)


/*
 * Load required drivers and libraries.
 */

#ifdef solaris2
    #include <kstat.h>
    #ifdef HAVE_PICL_H 
        #include <picl.h> /* accesses the picld daemon */
    #else 
        /* the following should be sufficient for any Sun-based sensors */
	#include </usr/platform/sun4u/include/sys/envctrl.h>
    #endif 
#else
    #include <sensors/sensors.h>
    #define CONFIG_FILE_NAME "/etc/sensors.conf"
#endif

#include "lmSensors.h"

#define TEMP_TYPE    (0)
#define FAN_TYPE     (1)
#define VOLT_TYPE    (2)
#define MISC_TYPE    (3)
#define N_TYPES      (4)

#ifdef solaris2
    #define MAX_NAME     (256)
    #define MAX_SENSORS  (256) /* there's a lot of sensors on a v880 */
#else
    #define MAX_NAME     (64)
    #define DEFAULT_SENSORS  (256)
#endif


/*
 * lmSensors_variables_oid:
 *   this is the top level oid that we want to register under.  This
 *   is essentially a prefix, with the suffix appearing in the
 *   variable below.
 */


oid             lmSensors_variables_oid[] =
    { 1, 3, 6, 1, 4, 1, 2021, 13, 16 };

/*
 * variable4 lmSensors_variables:
 *   this variable defines function callbacks and type return information 
 *   for the lmSensors mib section 
 */

struct variable4 lmSensors_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix 
     */
#define   LMTEMPSENSORSINDEX    3
    {LMTEMPSENSORSINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {2, 1, 1}},
#define   LMTEMPSENSORSDEVICE   4
    {LMTEMPSENSORSDEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {2, 1, 2}},
#define   LMTEMPSENSORSVALUE    5
    {LMTEMPSENSORSVALUE, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {2, 1, 3}},
#define   LMFANSENSORSINDEX     8
    {LMFANSENSORSINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {3, 1, 1}},
#define   LMFANSENSORSDEVICE    9
    {LMFANSENSORSDEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {3, 1, 2}},
#define   LMFANSENSORSVALUE     10
    {LMFANSENSORSVALUE, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {3, 1, 3}},
#define   LMVOLTSENSORSINDEX    13
    {LMVOLTSENSORSINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {4, 1, 1}},
#define   LMVOLTSENSORSDEVICE   14
    {LMVOLTSENSORSDEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {4, 1, 2}},
#define   LMVOLTSENSORSVALUE    15
    {LMVOLTSENSORSVALUE, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {4, 1, 3}},
#define   LMMISCSENSORSINDEX    18
    {LMMISCSENSORSINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {5, 1, 1}},
#define   LMMISCSENSORSDEVICE   19
    {LMMISCSENSORSDEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {5, 1, 2}},
#define   LMMISCSENSORSVALUE    20
    {LMMISCSENSORSVALUE, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_lmSensorsTable, 3, {5, 1, 3}},
};

typedef struct {
#ifdef solaris2
    #ifdef HAVE_PICL_H
        char            name[PICL_PROPNAMELEN_MAX]; /*required for picld*/
        int             value;
    #else
       char            name[MAX_NAME];
       int             value;
    #endif
#else
    char            name[MAX_NAME];
    int             value;
#endif
} _sensor;

typedef struct {
    int             n;
#ifdef solaris2
    _sensor         sensor[MAX_SENSORS];
#else
    _sensor*        sensor;
    size_t          current_len;
#endif
} _sensor_array;

static _sensor_array sensor_array[N_TYPES];
static time_t  timestamp;

static int      sensor_init(void);
static int      sensor_load(void);
static int     _sensor_load(time_t t);
#ifndef solaris2
static void     free_sensor_arrays(void);
#endif

/*
 * init_lmSensors():
 *   Initialization routine.  This is called when the agent starts up.
 *   At a minimum, registration of your variables should take place here.
 */
void
init_lmSensors(void)
{
   sensor_init(); 

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("lmSensors", lmSensors_variables, variable4,
                 lmSensors_variables_oid);
}

/*
 * shutdown_lmSensors():
 * A shutdown/cleanup routine.  This is called when the agent shutsdown.
 */
void
shutdown_lmSensors(void)
{
#ifndef solaris2
    DEBUGMSG(("ucd-snmp/lmSensors", "=> shutdown_lmSensors\n"));
    free_sensor_arrays();
    DEBUGMSG(("ucd-snmp/lmSensors", "<= shutdown_lmSensors\n"));
#endif
} /* shutdown_lmSensors */

/*
 * var_lmSensorsTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_lmSensors above.
 */
unsigned char  *
var_lmSensorsTable(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static char     string[SPRINT_MAX_LEN];

    int             s_index;
    int             s_type = -1;
    int             n_sensors;
    unsigned char* ret = NULL;

    _sensor         s;

    if (sensor_load())
    {
        ret = NULL;
        goto leaving;
    }

    switch (vp->magic) {
    case LMTEMPSENSORSINDEX:
    case LMTEMPSENSORSDEVICE:
    case LMTEMPSENSORSVALUE:
        s_type = TEMP_TYPE;
        n_sensors = sensor_array[s_type].n;
        break;

    case LMFANSENSORSINDEX:
    case LMFANSENSORSDEVICE:
    case LMFANSENSORSVALUE:
        s_type = FAN_TYPE;
        n_sensors = sensor_array[s_type].n;
        break;

    case LMVOLTSENSORSINDEX:
    case LMVOLTSENSORSDEVICE:
    case LMVOLTSENSORSVALUE:
        s_type = VOLT_TYPE;
        n_sensors = sensor_array[s_type].n;
        break;

    case LMMISCSENSORSINDEX:
    case LMMISCSENSORSDEVICE:
    case LMMISCSENSORSVALUE:
        s_type = MISC_TYPE;
        n_sensors = sensor_array[s_type].n;
        break;

    default:
        s_type = -1;
        n_sensors = 0;
    }

    if (header_simple_table(vp, name, length, exact,
                            var_len, write_method,
                            n_sensors) == MATCH_FAILED)
    {
        ret = NULL;
        goto leaving;
    }

    if (s_type < 0)
    {
        ret = NULL;
        goto leaving;
    }

    s_index = name[*length - 1] - 1;
    s = sensor_array[s_type].sensor[s_index];

    switch (vp->magic) {
    case LMTEMPSENSORSINDEX:
    case LMFANSENSORSINDEX:
    case LMVOLTSENSORSINDEX:
    case LMMISCSENSORSINDEX:
        long_ret = s_index;
        ret = (unsigned char *) &long_ret;
        goto leaving;

    case LMTEMPSENSORSDEVICE:
    case LMFANSENSORSDEVICE:
    case LMVOLTSENSORSDEVICE:
    case LMMISCSENSORSDEVICE:
        strlcpy(string, s.name, sizeof(string));
        *var_len = strlen(string);
        ret = (unsigned char *) string;
        goto leaving;

    case LMTEMPSENSORSVALUE:
    case LMFANSENSORSVALUE:
    case LMVOLTSENSORSVALUE:
    case LMMISCSENSORSVALUE:
        long_ret = s.value;
        ret = (unsigned char *) &long_ret;
        goto leaving;

    default:
        ERROR_MSG("Unable to handle table request");
    }

leaving:
    return ret;
}

static int
sensor_init(void)
{
    int             res;
#ifndef solaris2
    char            filename[] = CONFIG_FILE_NAME;
    time_t          t = time(NULL);
    FILE            *fp = fopen(filename, "r");
    int             i = 0;
  
    DEBUGMSG(("ucd-snmp/lmSensors", "=> sensor_init\n"));

    for (i = 0; i < N_TYPES; i++)
    {
        sensor_array[i].n = 0;
        sensor_array[i].current_len = 0;
        sensor_array[i].sensor = NULL;
    }

    if (!fp)
    {
        res = 1;
        goto leaving;
    }

    if (sensors_init(fp))
    {
        res = 2;
        goto leaving;
    }

    _sensor_load(t); /* I'll let the linux people decide whether they want to load right away */
leaving:
#endif /* not solaris2 */

    DEBUGMSG(("ucd-snmp/lmSensors", "<= sensor_init\n"));
    return res;
}

static int
sensor_load(void)
{
    int rc = 0;
    time_t	   t = time(NULL);

    if (t > timestamp + 7) /* this may require some tuning - currently 7 seconds*/
    {
#ifndef solaris2
        free_sensor_arrays();
#endif
        rc = _sensor_load(t);
    }

    return rc;
}

/* This next code block includes all kstat and picld code for the Solaris platform.
 * If you're not compiling on a Solaris that supports picld, it won't be included.
 */

#ifdef solaris2
/* *******  picld sensor procedures * */
#ifdef HAVE_PICL_H

/* the following are generic modules for reading sensor information
   the scale variable handles miniVolts */

static int
read_num_sensor(picl_nodehdl_t childh, const char *prop, int scale, int *value)
 {
  picl_nodehdl_t  sensorh;
  picl_propinfo_t sensor_info;
  picl_errno_t    error_code;
  int             valid = 1;

  union valu {
    char buf[PICL_PROPSIZE_MAX];
    uint32_t us4;
    uint16_t us2;
    int32_t is4;
    int16_t is2;
    float f;
  } val;

  error_code = (picl_get_propinfo_by_name(childh, prop,
                                         &sensor_info, &sensorh));

  if (error_code != PICL_SUCCESS) {
     DEBUGMSGTL(("ucd-snmp/lmSensors",
                "sensor info lookup failed in read_num_sensor - error code->%d\n", error_code));
     return(error_code);
  }

  error_code = picl_get_propval(sensorh, &val.buf, sensor_info.size);

  if (error_code != PICL_SUCCESS) {
    DEBUGMSGTL(("ucd-snmp/lmSensors",
               "sensor value lookup failed in read_num_sensor - error code->%d\n", error_code));
    return(error_code);
  }
    
  /* Can't make assumptions about the type or size of the value we get... */

  if  (sensor_info.type == PICL_PTYPE_FLOAT) {
    *value = (int)(val.f*scale);
  } else if (sensor_info.type == PICL_PTYPE_UNSIGNED_INT) {
    if (sensor_info.size == 2) {
      *value = (int)(val.us2 * scale);
    } else if (sensor_info.size == 4) {
      *value = (int)(val.us4 * scale);
    } else
      valid = 0;
  } else if (sensor_info.type == PICL_PTYPE_INT) {
    if (sensor_info.size == 2) {
      *value = (int)(val.is2 * scale);
    } else if (sensor_info.size == 4) {
      *value = (int)(val.is4 * scale);
    } else 
      valid = 0;
  } else
    valid = 0;

  if (valid == 0) {
    DEBUGMSGTL(("ucd-snmp/lmSensors",
               "Don't know how to handle data type %d with length %d\n", 
               sensor_info.type, sensor_info.size));
    error_code = PICL_FAILURE;
  } else 
    DEBUGMSGTL(("ucd-snmp/lmSensors", "read_num_sensor value is %d\n", *value));

  return(error_code);
} /* end of read_num_sensor() */

static int
read_enum_sensor(picl_nodehdl_t childh, const char **options, u_int *value)
{
  picl_nodehdl_t  sensorh;
  picl_propinfo_t sensor_info;
  picl_errno_t    error_code;
  char            state[PICL_PROPSIZE_MAX];
  int             i;

  error_code = (picl_get_propinfo_by_name(childh, "State",
                                         &sensor_info, &sensorh));

  if (error_code != PICL_SUCCESS) {
     DEBUGMSGTL(("ucd-snmp/lmSensors",
                "sensor info lookup failed in read_enum_sensor - error code->%d\n", error_code));
     return(error_code);
  }

  error_code = picl_get_propval(sensorh, state, sensor_info.size);

  if (error_code != PICL_SUCCESS) {
    DEBUGMSGTL(("ucd-snmp/lmSensors",
               "sensor value lookup failed in read_enum_sensor - error code->%d\n", error_code));
    return(error_code);
  }

  /* Start with error value, then try to fill in something better.
     Use case-insensitive match to find the right value since platforms
     may return either case. 
  */

  *value = 99;

  for (i = 0; options[i] != NULL; i++){
    if (strncasecmp(state, options[i], strlen(options[i])) == 0){
      *value = i;
      break;
    } 
  }

  DEBUGMSGTL(("ucd-snmp/lmSensors", "read_enum_sensor value is %d\n", *value));
  return(error_code);
} /* end of read_enum_sensor() */

/* scale variable handles miniVolts*/
 
static void
process_num_sensor(picl_nodehdl_t childh, 
                  const char propname[PICL_PROPNAMELEN_MAX], 
                  const char propval[PICL_PROPNAMELEN_MAX], int typ, int scale)
{
  int value = 0;
  picl_errno_t error_code;

  if (sensor_array[typ].n >= MAX_SENSORS){
    DEBUGMSGTL(("ucd-snmp/lmSensors",
               "There are too many sensors of type %d\n",typ));
  } else{
    error_code = read_num_sensor(childh, propval, scale, &value);
    
    if (error_code == PICL_SUCCESS) {
      sensor_array[typ].sensor[sensor_array[typ].n].value = value;
      snprintf(sensor_array[typ].sensor[sensor_array[typ].n].name,
              (PICL_PROPNAMELEN_MAX - 1),"%s",propname);
      sensor_array[typ].sensor[sensor_array[typ].n].
       name[PICL_PROPNAMELEN_MAX - 1] = '\0';
      sensor_array[typ].n++;
    } else
      DEBUGMSGTL(("ucd-snmp/lmSensors",
                 "read of %s in process_num_sensor returned error code %d\n", propname, error_code));
  }
} /* end process_num_sensor() */

static void
process_enum_sensor(picl_nodehdl_t childh, 
                  const char propname[PICL_PROPNAMELEN_MAX], 
                  int typ, const char **options)
{
  int value = 0;
  picl_errno_t error_code;
 
  if (sensor_array[typ].n >= MAX_SENSORS){
    DEBUGMSGTL(("ucd-snmp/lmSensors",
               "There are too many sensors of type %d\n",typ));
  } else{
    error_code = read_enum_sensor(childh, options, &value);
    
    if (error_code == PICL_SUCCESS) {
      sensor_array[typ].sensor[sensor_array[typ].n].value = value;
      snprintf(sensor_array[typ].sensor[sensor_array[typ].n].name,
              (PICL_PROPNAMELEN_MAX - 1),"%s",propname);
      sensor_array[typ].sensor[sensor_array[typ].n].
       name[PICL_PROPNAMELEN_MAX - 1] = '\0';
      sensor_array[typ].n++;
    } else
      DEBUGMSGTL(("ucd-snmp/lmSensors",
                 "read of %s in process_enum_sensor returned error code %d\n", propname, error_code));
  }
} /* end process_enum_sensor() */

/* The following are modules for dealing with individual sensors types.
   They call the generic modules above.  */

static void
process_individual_fan(picl_nodehdl_t childh, 
                     const char propname[PICL_PROPNAMELEN_MAX])
{
  process_num_sensor(childh, propname, "AtoDSensorValue", FAN_TYPE, 1);
}


static void
process_newtype_fan(picl_nodehdl_t childh,
                     const char propname[PICL_PROPNAMELEN_MAX])
{
  process_num_sensor(childh, propname, "Speed", FAN_TYPE, 1);
}


static void
process_temperature_sensor(picl_nodehdl_t childh,
                               const char propname[PICL_PROPNAMELEN_MAX])
{
  process_num_sensor(childh, propname, "Temperature", TEMP_TYPE, 1000);
} /* MIB asks for mC */

static void
process_voltage_sensor(picl_nodehdl_t childh,
                      const char propname[PICL_PROPNAMELEN_MAX])
{
  process_num_sensor(childh, propname, "Voltage", VOLT_TYPE, 1000);
} /* MIB asks for mV */

static void
process_digital_sensor(picl_nodehdl_t childh,
                      const char propname[PICL_PROPNAMELEN_MAX])
{
  process_num_sensor(childh, propname, "AtoDSensorValue", VOLT_TYPE, 1);
}


static void
process_switch(picl_nodehdl_t childh,
                   const char propname[PICL_PROPNAMELEN_MAX])
{

  const char *settings[]={"OFF","ON","NORMAL","LOCKED","UNKNOWN",
                   "DIAG","SECURE",NULL};

  process_enum_sensor(childh, propname, MISC_TYPE, settings);
}

static void
process_led(picl_nodehdl_t childh,
                   const char propname[PICL_PROPNAMELEN_MAX])
{

  const char *settings[]={"OFF","ON","BLINK",NULL};
  process_enum_sensor(childh, propname, MISC_TYPE, settings);
}

static void
process_i2c(picl_nodehdl_t childh,
                   const char propname[PICL_PROPNAMELEN_MAX])
{
  const char *settings[]={"OK",NULL};
  process_enum_sensor(childh, propname, MISC_TYPE, settings);
}

/* walks its way recusively through the tree of sensors */

static int
process_sensors(int level, picl_nodehdl_t nodeh)
{
    picl_nodehdl_t  childh;
    picl_nodehdl_t  nexth;

    char            propname[PICL_PROPNAMELEN_MAX];
    char            propclass[PICL_CLASSNAMELEN_MAX];
    picl_errno_t    error_code;

    level++;

    DEBUGMSGTL(("ucd-snmp/lmSensors","in process_sensors() level %d\n",level));

    /* look up first child node */
    error_code = picl_get_propval_by_name(nodeh, PICL_PROP_CHILD, &childh,
                                        sizeof (picl_nodehdl_t));
    if (error_code != PICL_SUCCESS) {
                DEBUGMSGTL(("ucd-snmp/lmSensors",
                           "picl_get_propval_by_name(%s) %d\n",
                           PICL_PROP_CHILD, error_code));
                return (error_code);
    }

    /* step through child nodes, get the name first */
    while (error_code == PICL_SUCCESS) {

        error_code = picl_get_propval_by_name(childh, PICL_PROP_NAME,
                                               propname, (PICL_PROPNAMELEN_MAX - 1));
        if (error_code != PICL_SUCCESS) {  /*we found a node with no name.  Impossible.! */
            DEBUGMSGTL(("ucd-snmp/lmSensors",
                       "picl_get_propval_by_name(%s) = %d\n",
                       PICL_PROP_NAME, error_code));
            return (error_code);
        }

        error_code = picl_get_propval_by_name(childh, PICL_PROP_CLASSNAME,
                                                propclass, sizeof (propclass));
        if (error_code != PICL_SUCCESS) {  /*we found a node with no class.  Impossible.! */
            DEBUGMSGTL(("ucd-snmp/lmSensors",
                       "picl_get_propval_by_name(%s) = %d\n",
                       PICL_PROP_CLASSNAME, error_code));
            return (error_code);
        }

        DEBUGMSGTL(("ucd-snmp/lmSensors","found %s of class %s\n",propname,propclass)); 

        if (strstr(propclass,"fan-tachometer"))
           process_individual_fan(childh,propname);
        else if (strstr(propclass,"fan"))
            process_newtype_fan(childh,propname);
        else if (strstr(propclass,"temperature-sensor"))
            process_temperature_sensor(childh,propname);
        else if (strstr(propclass,"voltage-sensor"))
            process_voltage_sensor(childh,propname);
        else if (strstr(propclass,"digital-sensor"))
            process_digital_sensor(childh,propname);
        else if (strstr(propclass,"switch"))
            process_switch(childh,propname);
        else if (strstr(propclass,"led"))
            process_led(childh,propname);
        else if (strstr(propclass,"i2c"))
            process_i2c(childh,propname);
/*
        else if (strstr(propclass,"gpio"))
            process_gpio(childh,propname); 
*/


        /* look for children of children (note, this is recursive) */
       if (!(strstr(propclass,"picl") && 
             (strstr(propname,"frutree") || strstr(propname,"obp")))) {
           error_code = process_sensors(level,childh);
           DEBUGMSGTL(("ucd-snmp/lmSensors",
                      "process_sensors(%s) returned %d\n",
                       propname, error_code));
        }

         /* get next child node at this level*/
        error_code = picl_get_propval_by_name(childh, PICL_PROP_PEER,
                                   &nexth, sizeof (picl_nodehdl_t));
        if (error_code != PICL_SUCCESS) {/* no more children - buh bye*/
           DEBUGMSGTL(("ucd-snmp/lmSensors","Process sensors is out of children!  Returning...\n"));
           return (error_code);
        }

        childh = nexth;

    } /* while */
    return (error_code);
} /* process sensors */

#endif
/* ******** end of picld sensor procedures * */

#endif /* solaris2 */
static int
_sensor_load(time_t t)
{
#ifdef solaris2
    int i,j;
#ifdef HAVE_PICL_H 
    int er_code;
    picl_errno_t     error_code;
    int level=0;
    picl_nodehdl_t  rooth;
#else
    int typ;
    int temp=0; /* do not reset this later, more than one typ has temperatures*/
    int other=0;
    const char *fantypes[]={"CPU","PWR","AFB"};
    kstat_ctl_t *kc;
    kstat_t *kp;
    envctrl_fan_t *fan_info;
    envctrl_ps_t *power_info;
    envctrl_encl_t *enc_info;
#endif

/* DEBUGMSG(("ucd-snmp/lmSensors", "Reading the sensors\n")); */

/* initialize the array */
    for (i = 0; i < N_TYPES; i++){
        sensor_array[i].n = 0;
        for (j=0; j < MAX_SENSORS; j++){
            sensor_array[i].sensor[j].name[0] = '\0';
            sensor_array[i].sensor[j].value = 0;
             }
        } /*end for i*/

/* try picld (if supported), if that doesn't work, try kstat */
#ifdef HAVE_PICL_H 

er_code = picl_initialize();

if (er_code == PICL_SUCCESS) {

    error_code = picl_get_root(&rooth);

    if (error_code != PICL_SUCCESS) {
        DEBUGMSG(("ucd-snmp/lmSensors", "picld couldn't get root error code->%d\n",error_code));
        }
    else{
        DEBUGMSGTL(("ucd-snmp/lmSensors", "found root\n"));
        error_code = process_sensors(level,rooth);
        if (error_code != 255) 
            if (error_code != 7)
                DEBUGMSG(("ucd-snmp/lmSensors", "picld had an internal problem error code->%d\n",error_code));
        } /* end else */

    picl_shutdown();

}  /* end if err_code for picl_initialize */

else {  
    DEBUGMSG(("ucd-snmp/lmSensors", "picld couldn't initialize picld because error code->%d\n",er_code));

} /*end else picl_initialize */

#else  /* end of picld section */
/* initialize kstat */

kc = kstat_open();
if (kc == 0) {
    DEBUGMSG(("ucd-snmp/lmSensors", "couldn't open kstat"));
    } /* endif kc */
else{
    temp = 0;
    kp = kstat_lookup(kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_FANSTAT);
    if (kp == 0) {
        DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't lookup fan kstat\n"));
        } /* endif lookup fans */
    else{
        if (kstat_read(kc, kp, 0) == -1) {
            DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't read fan kstat"));
            } /* endif kstatread fan */
        else{
            typ = 1;
            fan_info = (envctrl_fan_t *) kp->ks_data;
            sensor_array[typ].n = kp->ks_ndata;
            for (i=0; i < kp->ks_ndata; i++){
                DEBUGMSG(("ucd-snmp/lmSensors", "found instance %d fan type %d speed %d OK %d bustedfan %d\n",
                    fan_info->instance, fan_info->type,fan_info->fanspeed,fan_info->fans_ok,fan_info->fanflt_num));
                sensor_array[typ].sensor[i].value = fan_info->fanspeed;
                snprintf(sensor_array[typ].sensor[i].name,(MAX_NAME - 1),
                   "fan type %s number %d",fantypes[fan_info->type],fan_info->instance);
                sensor_array[typ].sensor[i].name[MAX_NAME - 1] = '\0';
                fan_info++;
                } /* end for fan_info */
            } /* end else kstatread fan */
        } /* end else lookup fans*/


    kp = kstat_lookup(kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_PSNAME);
    if (kp == 0) {
        DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't lookup power supply kstat\n"));
        } /* endif lookup power supply */
    else{
        if (kstat_read(kc, kp, 0) == -1) {
            DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't read power supply kstat\n"));
            } /* endif kstatread fan */
        else{
            typ = 0; /* this is a power supply temperature, not a voltage*/
            power_info = (envctrl_ps_t *) kp->ks_data;
            sensor_array[typ].n = kp->ks_ndata;
            for (i=0; i < kp->ks_ndata; i++){
                DEBUGMSG(("ucd-snmp/lmSensors", "found instance %d psupply temp mC %d %dW OK %d share %d limit %d\n",
                    power_info->instance, power_info->ps_tempr*1000,power_info->ps_rating,
                    power_info->ps_ok,power_info->curr_share_ok,power_info->limit_ok));
                sensor_array[typ].sensor[temp].value = power_info->ps_tempr*1000;
                snprintf(sensor_array[typ].sensor[temp].name,(MAX_NAME-1),
                         "power supply %d",power_info->instance);
                sensor_array[typ].sensor[temp].name[MAX_NAME - 1] = '\0';
                power_info++; /* increment the data structure */
                temp++; /* increment the temperature sensor array element */
                } /* end for power_info */
            } /* end else kstatread power supply */
        } /* end else lookup power supplies*/

    kp = kstat_lookup(kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_ENCL);
    if (kp == 0) {
        DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't lookup enclosure kstat\n"));
        } /* endif lookup enclosure */
    else{
        if (kstat_read(kc, kp, 0) == -1) {
            DEBUGMSGTL(("ucd-snmp/lmSensors", "couldn't read enclosure kstat\n"));
            } /* endif kstatread enclosure */
        else{
            enc_info = (envctrl_encl_t *) kp->ks_data; 
            other = 0;
            for (i=0; i < kp->ks_ndata; i++){
               switch (enc_info->type){
               case ENVCTRL_ENCL_FSP:
                   DEBUGMSG(("ucd-snmp/lmSensors", "front panel value %d\n",enc_info->value));
                   typ = 3; /* misc */
                   sensor_array[typ].sensor[other].value = enc_info->value;
                   strlcpy(sensor_array[typ].sensor[other].name, "FSP",
                           MAX_NAME);
                   other++;
                   break;
               case ENVCTRL_ENCL_AMBTEMPR:
                   DEBUGMSG(("ucd-snmp/lmSensors", "ambient temp mC %d\n",enc_info->value*1000));
                   typ = 0; /* temperature sensor */
                   sensor_array[typ].sensor[temp].value = enc_info->value*1000;
                   strlcpy(sensor_array[typ].sensor[temp].name, "Ambient",
                           MAX_NAME);
                   temp++;
                   break;
               case ENVCTRL_ENCL_BACKPLANE4:
                   DEBUGMSG(("ucd-snmp/lmSensors", "There is a backplane4\n"));
                   typ = 3; /* misc */
                   sensor_array[typ].sensor[other].value = enc_info->value;
                   strlcpy(sensor_array[typ].sensor[other].name, "Backplane4",
                           MAX_NAME);
                   other++;
                   break;
               case ENVCTRL_ENCL_BACKPLANE8:
                   DEBUGMSG(("ucd-snmp/lmSensors", "There is a backplane8\n"));
                   typ = 3; /* misc */
                   sensor_array[typ].sensor[other].value = enc_info->value;
                   strlcpy(sensor_array[typ].sensor[other].name, "Backplane8",
                           MAX_NAME);
                   other++;
                   break;
               case ENVCTRL_ENCL_CPUTEMPR:
                   DEBUGMSG(("ucd-snmp/lmSensors", "CPU%d temperature mC %d\n",enc_info->instance,enc_info->value*1000));
                   typ = 0; /* temperature sensor */
                   sensor_array[typ].sensor[temp].value = enc_info->value*1000;
                   snprintf(sensor_array[typ].sensor[temp].name,MAX_NAME,"CPU%d",enc_info->instance);
                   sensor_array[typ].sensor[temp].name[MAX_NAME-1]='\0'; /* null terminate */
                   temp++;
                   break;
               default:
                   DEBUGMSG(("ucd-snmp/lmSensors", "unknown element instance %d type %d value %d\n",
                       enc_info->instance, enc_info->type, enc_info->value));
                   break;
               } /* end switch */
               enc_info++;
               } /* end for enc_info */
               sensor_array[3].n = other;
               sensor_array[0].n = temp;
            } /* end else kstatread enclosure */
        } /* end else lookup enclosure*/

    kstat_close(kc);

} /* end else kstat */
#endif

#else /* end solaris2 only ie. ifdef everything else */

    const sensors_chip_name *chip;
    const sensors_feature_data *data;
    int             chip_nr = 0;
    unsigned int    i = 0;

    DEBUGMSG(("ucd-snmp/lmSensors", "=> sensor_load\n"));

    for (i = 0; i < N_TYPES; i++)
    {
        sensor_array[i].n = 0;
        sensor_array[i].current_len = 0;

        /* Malloc the default number of sensors. */
        sensor_array[i].sensor = (_sensor*)malloc(sizeof(_sensor) * DEFAULT_SENSORS);
        if (sensor_array[i].sensor == NULL)
        {
           /* Continuing would be unsafe */
           snmp_log(LOG_ERR, "Cannot malloc sensor array!"); 
           return 1;
        } /* end if */
        sensor_array[i].current_len = DEFAULT_SENSORS;
    } /* end for */

    while ((chip = sensors_get_detected_chips(&chip_nr))) {
	int             a = 0;
	int             b = 0;

        while ((data = sensors_get_all_features(*chip, &a, &b))) {
            char           *label = NULL;
            double          val;

            if ((data->mode & SENSORS_MODE_R) &&
                (data->mapping == SENSORS_NO_MAPPING) &&
                !sensors_get_label(*chip, data->number, &label) &&
                !sensors_get_feature(*chip, data->number, &val)) {
                int             type = -1;
                float           mul = 0;
                _sensor_array  *array;

                /* The label, as determined for a given chip in sensors.conf,
                 * is used to place each sensor in the appropriate bucket.
                 * Volt, Fan, Temp, and Misc.  If the text being looked for below
                 * is not in the label of a given sensor (e.g., the temp1 sensor
                 * has been labeled 'CPU' and not 'CPU temp') it will end up being
                 * lumped in the MISC bucket. */

                if (strstr(label, "V")) {
                    type = VOLT_TYPE;
                    mul = 1000.0;
                }
                if (strstr(label, "fan") || strstr(label, "Fan")) {
                    type = FAN_TYPE;
                    mul = 1.0;
                }
                if (strstr(label, "temp") || strstr(label, "Temp")) {
                    type = TEMP_TYPE;
                    mul = 1000.0;
                }
                if (type == -1) {
                    type = MISC_TYPE;
                    mul = 1000.0;
                }

                array = &sensor_array[type];
                if ( array->current_len <= array->n) {
                    _sensor* old_buffer = array->sensor;
                    size_t new_size = (sizeof(_sensor) * array->current_len) + (sizeof(_sensor) * DEFAULT_SENSORS);
                    array->sensor = (_sensor*)realloc(array->sensor, new_size);
                    if (array->sensor == NULL)
                    {
                       /* Continuing would be unsafe */
                       snmp_log(LOG_ERR, "too many sensors to fit, and failed to alloc more, failing on %s\n", label);
                       free(old_buffer);
                       old_buffer = NULL;
                       if (label) {
                           free(label);
                           label = NULL;
                       } /* end if label */
                       return 1;
                    } /* end if array->sensor */
                    array->current_len = new_size / sizeof(_sensor);
                    DEBUGMSG(("ucd-snmp/lmSensors", "type #%d increased to %d elements\n", type, (int)array->current_len));
                } /* end if array->current */
                strlcpy(array->sensor[array->n].name, label, MAX_NAME);
                array->sensor[array->n].value = (int) (val * mul);
                DEBUGMSGTL(("sensors","sensor %s, value %d\n",
                            array->sensor[array->n].name,
                            array->sensor[array->n].value));
                array->n++;
            } /* end if data-mode */
	    if (label) {
		free(label);
		label = NULL;
	    } /* end if label */
        } /* end while data */
    } /* end while chip */
    DEBUGMSG(("ucd-snmp/lmSensors", "<= sensor_load\n"));
#endif  /* end else ie. ifdef everything else */
    /* Update the timestamp after a load. */
    timestamp = t;
    return 0;
}

#ifndef solaris2
/* Free all the sensor arrays. */
static void
free_sensor_arrays()
{
   unsigned int i = 0;
   DEBUGMSG(("ucd-snmp/lmSensors", "=> free_sensor_arrays\n"));
   for (i = 0; i < N_TYPES; i++){
       if (sensor_array[i].sensor != NULL)
       {
           free(sensor_array[i].sensor);
           sensor_array[i].sensor = NULL;
       }
       /* For good measure, reset the other values. */
       sensor_array[i].n = 0;
       sensor_array[i].current_len = 0;
   }
   DEBUGMSG(("ucd-snmp/lmSensors", "<= free_sensor_arrays\n"));
}
#endif
