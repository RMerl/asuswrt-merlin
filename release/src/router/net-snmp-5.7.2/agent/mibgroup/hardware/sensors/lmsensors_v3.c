#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>

#include <time.h>
#include <sensors/sensors.h>


void netsnmp_sensor_arch_init( void ) {
    FILE *fp = fopen("/etc/sensors.conf", "r");
    DEBUGMSGTL(("sensors:arch", "Initialise v3 LM Sensors module\n"));
    sensors_init( fp );
}

int
netsnmp_sensor_arch_load(netsnmp_cache *cache, void *vp) {
    netsnmp_sensor_info        *sp;
    const sensors_chip_name    *chip;
    const sensors_feature      *data;
    const sensors_subfeature   *data2;
    int             chip_nr = 0;

    DEBUGMSGTL(("sensors:arch", "Reload v3 LM Sensors module\n"));
    while ((chip = sensors_get_detected_chips( NULL, &chip_nr))) {
	int             a = 0;

        while ((data = sensors_get_features( chip, &a))) {
	    int             b = 0;

            DEBUGMSGTL(("sensors:arch:detail", "get_features (%s, %d)\n", data->name, data->number));

            while ((data2 = sensors_get_all_subfeatures( chip, data, &b))) {
                char           *label = NULL;
                double          val = 0;
                int             type = NETSNMP_SENSOR_TYPE_OTHER;

                DEBUGMSGTL(("sensors:arch:detail", "  get_subfeatures (%s, %d)\n", data2->name, data2->number));
                /*
                 * Check the type of this subfeature,
                 *   concentrating on the main "input" measurements.
                 */
                switch ( data2->type ) {
                case SENSORS_SUBFEATURE_IN_INPUT:
                    type = NETSNMP_SENSOR_TYPE_VOLTAGE_DC;
                    break;
                case SENSORS_SUBFEATURE_FAN_INPUT:
                    type = NETSNMP_SENSOR_TYPE_RPM;
                    break;
                case SENSORS_SUBFEATURE_TEMP_INPUT:
                    type = NETSNMP_SENSOR_TYPE_TEMPERATURE;
                    break;
                case SENSORS_SUBFEATURE_VID:
                    type = NETSNMP_SENSOR_TYPE_VOLTAGE_DC;
                    break;
                default:
                    /* Skip everything other than these basic sensor features - ??? */
                    DEBUGMSGTL(("sensors:arch:detail", "  Skip type %x\n", data2->type));
                    continue;
                }
            
                /*
                 * Get the name and value of this subfeature
                 */
/*
                if (!(label = sensors_get_label(chip, data))) {
                    DEBUGMSGTL(("sensors:arch:detail", "  Can't get name (%s)\n", label));
                    continue;
                }
                if (sensors_get_value(chip, data2->number, &val) < 0) {
                    DEBUGMSGTL(("sensors:arch:detail", "  Can't get value (%f)\n", val));
                    continue;
                }
*/
                if (!(label = sensors_get_label(chip, data)) ||
                     (sensors_get_value(chip, data2->number, &val) < 0)) {
                    DEBUGMSGTL(("sensors:arch:detail", "  Can't get name/value (%s, %f)\n", label, val));
                    free(label);
                    label = NULL;
                    continue;
                }
                DEBUGMSGTL(("sensors:arch:detail", "%s = %f\n", label, val));

                /*
                 * Use this type to create a new sensor entry
                 *  (inserting it in the appropriate sub-containers)
                 */
                sp = sensor_by_name( label, type );
                if ( sp ) {
                    sp->value = val;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
	        if (label) {
		    free(label);
		    label = NULL;
	        }
            } /* end while data2 */
        } /* end while data */
    } /* end while chip */

    return 0;
}
