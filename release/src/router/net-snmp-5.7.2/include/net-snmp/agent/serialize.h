#ifndef SERIALIZE_H
#define SERIALIZE_H

/*
 * The serialized helper merely calls its clients multiple times for a
 * * given request set, so they don't have to loop through the requests
 * * themselves.
 */

#ifdef __cplusplus
extern          "C" {
#endif

    netsnmp_mib_handler *netsnmp_get_serialize_handler(void);
    int             netsnmp_register_serialize(netsnmp_handler_registration
                                               *reginfo);
    void            netsnmp_init_serialize(void);

    Netsnmp_Node_Handler netsnmp_serialize_helper_handler;

#ifdef __cplusplus
}
#endif
#endif
