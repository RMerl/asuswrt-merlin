#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>

#include <time.h>
#include <sensors/sensors.h>

void netsnmp_sensor_arch_init( void ) {
    FILE *fp = fopen("/etc/sensors.conf", "r");
    DEBUGMSGTL(("sensors:arch", "Initialise LM Sensors module\n"));
    sensors_init( fp );
}

int
netsnmp_sensor_arch_load(netsnmp_cache *cache, void *vp) {
    netsnmp_sensor_info        *sp;
    const sensors_chip_name    *chip;
    const sensors_feature_data *data;
    int             chip_nr = 0;

    DEBUGMSGTL(("sensors:arch", "Reload LM Sensors module\n"));
    while ((chip = sensors_get_detected_chips(&chip_nr))) {
	int             a = 0;
	int             b = 0;

        while ((data = sensors_get_all_features(*chip, &a, &b))) {
            DEBUGMSGTL(("sensors:arch:detail", "get_all_features (%d, %d)\n", a, b));
            char           *label = NULL;
            double          val;
            int             type = NETSNMP_SENSOR_TYPE_OTHER;

            if ((data->mode & SENSORS_MODE_R) &&
                (data->mapping == SENSORS_NO_MAPPING) &&
                !sensors_get_label(*chip, data->number, &label) &&
                !sensors_get_feature(*chip, data->number, &val)) {

                DEBUGMSGTL(("sensors:arch:detail", "%s = %f\n", label, val));
                /*
                 * Determine the type of sensor from the description.
                 *
                 * If the text being looked for below is not in the label of a
                 * given sensor (e.g., the temp1 sensor has been labeled 'CPU'
                 * rather than 'CPU temp') it will be categorised as OTHER.
                 */
                if (strstr(label, "V")) {
                    type = NETSNMP_SENSOR_TYPE_VOLTAGE_DC;
                }
                if (strstr(label, "fan") || strstr(label, "Fan")) {
                    type = NETSNMP_SENSOR_TYPE_RPM;
                }
                if (strstr(label, "temp") || strstr(label, "Temp")) {
                    type = NETSNMP_SENSOR_TYPE_TEMPERATURE;
                }

                /*
                 * Use this type to create a new sensor entry
                 *  (inserting it in the appropriate sub-containers)
                 */
                sp = sensor_by_name( label, type );
                if ( sp ) {
                    sp->value = val;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
            }
	    if (label) {
		free(label);
		label = NULL;
	    }
        } /* end while data */
    } /* end while chip */

    return 0;
}
