#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/sensors.h>

#include <time.h>

#include <kstat.h>
#include </usr/platform/sun4u/include/sys/envctrl.h>

void netsnmp_sensor_arch_init( void ) {
    DEBUGMSGTL(("sensors:arch", "Initialise KStat Sensors module\n"));
}


int
netsnmp_sensor_arch_load(netsnmp_cache *cache, void *vp) {
    netsnmp_sensor_info        *sp;

    int         i;
    const char *fantypes[]={"CPU","PWR","AFB"};
    char        name[ 256 ];

    kstat_ctl_t    *kc;
    kstat_t        *kp;
    envctrl_fan_t  *fan_info;
    envctrl_ps_t   *power_info;
    envctrl_encl_t *enc_info;


    DEBUGMSGTL(("sensors:arch", "Reload KStat Sensors module\n"));

    kc = kstat_open();
    if ( kc == 0) {
        DEBUGMSGTL(("sensors:arch", "Couldn't open kstat\n"));
        return 1;
    }
    

    /*
     * Retrieve fan information
     */
    kp = kstat_lookup( kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_FANSTAT);
    if (( kp == 0 ) || (kstat_read( kc, kp, 0 ) == -1 )) {
        DEBUGMSGTL(("sensors:arch", "No fan information\n"));
    } else {
        fan_info = (envctrl_fan_t *)kp->ks_data;        
        for (i=0; i<kp->ks_ndata; i++) {
            memset( name, 0, 256 );
            snprintf( name, 255, "%s%d", fantypes[fan_info->type], fan_info->instance );

            sp = sensor_by_name( name, NETSNMP_SENSOR_TYPE_RPM );
            if ( sp ) {
                sp->value = fan_info->fanspeed;
                sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                snprintf( sp->descr, 255, "fan type %s number %d",
                          fantypes[fan_info->type], fan_info->instance );
            }
    
            fan_info++;
        }
    }


    /*
     * Retrieve Power Supply information
     */
    kp = kstat_lookup( kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_PSNAME);
    if (( kp == 0 ) || (kstat_read( kc, kp, 0 ) == -1 )) {
        DEBUGMSGTL(("sensors:arch", "No PSU information\n"));
    } else {
        power_info = (envctrl_ps_t *)kp->ks_data;        
        for (i=0; i<kp->ks_ndata; i++) {
            memset( name, 0, 256 );
            snprintf( name, 255, "PSU%d", power_info->instance );

            sp = sensor_by_name( name, NETSNMP_SENSOR_TYPE_TEMPERATURE);
            if ( sp ) {
                sp->value = power_info->ps_tempr;
                sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                snprintf( sp->descr, 255, "power supply %d", power_info->instance );
            }
    
            power_info++;
        }
    }


    /*
     * Retrieve Enclosure information
     */
    kp = kstat_lookup( kc, ENVCTRL_MODULE_NAME, 0, ENVCTRL_KSTAT_ENCL);
    if (( kp == 0 ) || (kstat_read( kc, kp, 0 ) == -1 )) {
        DEBUGMSGTL(("sensors:arch", "No enclosure information\n"));
    } else {
        enc_info = (envctrl_encl_t *)kp->ks_data;        
        for (i=0; i<kp->ks_ndata; i++) {
            /*
             * The enclosure information covers several different types of sensor
             */
            switch ( enc_info->type ) {
            case ENVCTRL_ENCL_FSP:
                DEBUGMSGTL(("sensors:arch:detail", "Enclosure Front Panel\n"));
                sp = sensor_by_name( "FSP", NETSNMP_SENSOR_TYPE_OTHER);
                if ( sp ) {
                    sp->value = enc_info->value;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
                break;
                
            case ENVCTRL_ENCL_AMBTEMPR:
                DEBUGMSGTL(("sensors:arch:detail", "Enclosure Ambient Temperature\n"));
                sp = sensor_by_name( "Ambient", NETSNMP_SENSOR_TYPE_TEMPERATURE);
                if ( sp ) {
                    sp->value = enc_info->value;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
                break;

            case ENVCTRL_ENCL_CPUTEMPR:
                DEBUGMSGTL(("sensors:arch:detail", "Enclosure CPU Temperature\n"));
                memset( name, 0, 256 );
                snprintf( name, 255, "CPU%d", enc_info->instance );
                sp = sensor_by_name( name, NETSNMP_SENSOR_TYPE_TEMPERATURE);
                if ( sp ) {
                    sp->value = enc_info->value;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                    snprintf( sp->descr, 255, "CPU%d temperature", enc_info->instance );
                }
                break;

            case ENVCTRL_ENCL_BACKPLANE4:
                DEBUGMSGTL(("sensors:arch:detail", "Enclosure Backplane4\n"));
                sp = sensor_by_name( "Backplane4", NETSNMP_SENSOR_TYPE_OTHER);
                if ( sp ) {
                    sp->value = enc_info->value;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
                break;
                
            case ENVCTRL_ENCL_BACKPLANE8:
                DEBUGMSGTL(("sensors:arch:detail", "Enclosure Backplane4\n"));
                sp = sensor_by_name( "Backplane4", NETSNMP_SENSOR_TYPE_OTHER);
                if ( sp ) {
                    sp->value = enc_info->value;
                    sp->flags|= NETSNMP_SENSOR_FLAG_ACTIVE;
                }
                break;

            default:    
                DEBUGMSGTL(("sensors:arch:detail", "Unrecognised Enclosure entry (%d)n",
                                                    enc_info->type));
            }

            enc_info++;
        }
    }

    kstat_close(kc);
    return 0;
}
