/*
 * callback.c: A generic callback mechanism 
 */

#ifndef CALLBACK_H
#define CALLBACK_H

#ifdef __cplusplus
extern          "C" {
#endif

#define MAX_CALLBACK_IDS    2
#define MAX_CALLBACK_SUBIDS 16

    /*
     * Callback Major Types 
     */
#define SNMP_CALLBACK_LIBRARY     0
#define SNMP_CALLBACK_APPLICATION 1

    /*
     * SNMP_CALLBACK_LIBRARY minor types 
     */
#define SNMP_CALLBACK_POST_READ_CONFIG	        0
#define SNMP_CALLBACK_STORE_DATA	        1
#define SNMP_CALLBACK_SHUTDOWN		        2
#define SNMP_CALLBACK_POST_PREMIB_READ_CONFIG	3
#define SNMP_CALLBACK_LOGGING			4
#define SNMP_CALLBACK_SESSION_INIT		5
#define SNMP_CALLBACK_PRE_READ_CONFIG	        7
#define SNMP_CALLBACK_PRE_PREMIB_READ_CONFIG	8


    /*
     * Callback priority (lower priority numbers called first(
     */
#define NETSNMP_CALLBACK_HIGHEST_PRIORITY      -1024 
#define NETSNMP_CALLBACK_DEFAULT_PRIORITY       0
#define NETSNMP_CALLBACK_LOWEST_PRIORITY        1024

    typedef int     (SNMPCallback) (int majorID, int minorID,
                                    void *serverarg, void *clientarg);

    struct snmp_gen_callback {
        SNMPCallback   *sc_callback;
        void           *sc_client_arg;
        int             priority;
        struct snmp_gen_callback *next;
    };

    /*
     * function prototypes 
     */
    NETSNMP_IMPORT
    void            init_callbacks(void);

    NETSNMP_IMPORT
    int             netsnmp_register_callback(int major, int minor,
                                              SNMPCallback * new_callback,
                                              void *arg, int priority);
    NETSNMP_IMPORT
    int             snmp_register_callback(int major, int minor,
                                           SNMPCallback * new_callback,
                                           void *arg);
    NETSNMP_IMPORT
    int             snmp_call_callbacks(int major, int minor,
                                        void *caller_arg);
    NETSNMP_IMPORT
    int             snmp_callback_available(int major, int minor);      /* is >1 available */
    NETSNMP_IMPORT
    int             snmp_count_callbacks(int major, int minor); /* ret the number registered */
    NETSNMP_IMPORT
    int             snmp_unregister_callback(int major, int minor,
                                             SNMPCallback * new_callback,
                                             void *arg, int matchargs);
    NETSNMP_IMPORT
    void            clear_callback (void);
    int             netsnmp_callback_clear_client_arg(void *, int i, int j);

    struct snmp_gen_callback *snmp_callback_list(int major, int minor);

#ifdef __cplusplus
}
#endif
#endif                          /* CALLBACK_H */
