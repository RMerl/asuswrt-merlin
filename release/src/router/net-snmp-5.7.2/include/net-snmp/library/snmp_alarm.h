#ifndef SNMP_ALARM_H
#define SNMP_ALARM_H

#ifdef __cplusplus
extern          "C" {
#endif

    typedef void    (SNMPAlarmCallback) (unsigned int clientreg,
                                         void *clientarg);

    /*
     * alarm flags 
     */
#define SA_REPEAT 0x01          /* keep repeating every X seconds */
#define SA_FIRED 0x10          /* Being processed in run_alarms */

    struct snmp_alarm {
        /** Alarm interval. Zero if single-shot. */
        struct timeval  t;
        unsigned int    flags;
        unsigned int    clientreg;
        /** Last time the alarm fired [monotonic clock]. */
        struct timeval  t_lastM;
        /** Next time the alarm will fire [monotonic clock]. */
        struct timeval  t_nextM;
        void           *clientarg;
        SNMPAlarmCallback *thecallback;
        struct snmp_alarm *next;
    };

    /*
     * the ones you should need 
     */
    NETSNMP_IMPORT
    void            snmp_alarm_unregister(unsigned int clientreg);
    void	    snmp_alarm_unregister_all(void);
    NETSNMP_IMPORT
    unsigned int    snmp_alarm_register(unsigned int when,
                                        unsigned int flags,
                                        SNMPAlarmCallback * thecallback,
                                        void *clientarg);

    unsigned int    snmp_alarm_register_hr(struct timeval t,
                                           unsigned int flags,
                                           SNMPAlarmCallback * cb,
                                           void *cd);
    NETSNMP_IMPORT
    int             snmp_alarm_reset(unsigned int clientreg);


    /*
     * the ones you shouldn't 
     */
    NETSNMP_IMPORT void init_snmp_alarm(void);
    int             init_alarm_post_config(int majorid, int minorid,
                                           void *serverarg,
                                           void *clientarg);
    void            sa_update_entry(struct snmp_alarm *alrm);
    struct snmp_alarm *sa_find_next(void);
    NETSNMP_IMPORT void run_alarms(void);
    RETSIGTYPE      alarm_handler(int a);
    void            set_an_alarm(void);
    int             netsnmp_get_next_alarm_time(struct timeval *alarm_tm,
                                                const struct timeval *now);
    int             get_next_alarm_delay_time(struct timeval *delta);


#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_ALARM_H */
