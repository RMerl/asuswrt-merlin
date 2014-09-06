/*
 * snmp_client.h
 */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#ifndef SNMP_CLIENT_H
#define SNMP_CLIENT_H

#include <net-snmp/types.h>
#include <net-snmp/varbind_api.h>
#include <net-snmp/pdu_api.h>
#include <net-snmp/output_api.h>
#include <net-snmp/session_api.h>

#ifdef __cplusplus
extern          "C" {
#endif


    struct snmp_pdu;
    struct snmp_session;
    struct variable_list;

    struct synch_state {
        int             waiting;
        int             status;
        /*
         * status codes 
         */
#define STAT_SUCCESS	0
#define STAT_ERROR	1
#define STAT_TIMEOUT 2
        int             reqid;
        netsnmp_pdu    *pdu;
    };

    NETSNMP_IMPORT
    void            snmp_replace_var_types(netsnmp_variable_list * vbl,
                                           u_char old_type,
                                           u_char new_type);
    NETSNMP_IMPORT
    void            snmp_reset_var_buffers(netsnmp_variable_list * var);
    void            snmp_reset_var_types(netsnmp_variable_list * vbl,
                                         u_char new_type);
    NETSNMP_IMPORT
    int             count_varbinds(netsnmp_variable_list * var_ptr);
    NETSNMP_IMPORT
    int             count_varbinds_of_type(netsnmp_variable_list * var_ptr,
                                           u_char type);
    netsnmp_variable_list *find_varbind_of_type(netsnmp_variable_list *
                                                var_ptr, u_char type);
    NETSNMP_IMPORT
    netsnmp_variable_list *find_varbind_in_list(netsnmp_variable_list *vblist,
                                                const oid *name, size_t len);

    netsnmp_pdu    *snmp_split_pdu(netsnmp_pdu *, int skipCount,
                                   int copyCount);

    unsigned long   snmp_varbind_len(netsnmp_pdu *pdu);
    NETSNMP_IMPORT
    int             snmp_clone_var(netsnmp_variable_list *,
                                   netsnmp_variable_list *);
    NETSNMP_IMPORT
    int             snmp_synch_response_cb(netsnmp_session *,
                                           netsnmp_pdu *, netsnmp_pdu **,
                                           snmp_callback);
    NETSNMP_IMPORT
    int             snmp_clone_mem(void **, const void *, unsigned);


NETSNMP_IMPORT
void              netsnmp_query_set_default_session(netsnmp_session *);
NETSNMP_IMPORT
netsnmp_session * netsnmp_query_get_default_session_unchecked( void );
NETSNMP_IMPORT
netsnmp_session * netsnmp_query_get_default_session( void );
NETSNMP_IMPORT
int netsnmp_query_get(     netsnmp_variable_list *, netsnmp_session *);
NETSNMP_IMPORT
int netsnmp_query_getnext( netsnmp_variable_list *, netsnmp_session *);
NETSNMP_IMPORT
int netsnmp_query_walk(    netsnmp_variable_list *, netsnmp_session *);
#ifndef NETSNMP_NO_WRITE_SUPPORT
NETSNMP_IMPORT
int netsnmp_query_set(     netsnmp_variable_list *, netsnmp_session *);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

/** **************************************************************************
 *
 * state machine
 *
 */
    /** forward declare */
    struct netsnmp_state_machine_step_s;
    struct netsnmp_state_machine_input_s;

    /** state machine process */
    typedef int (netsnmp_state_machine_func)(struct netsnmp_state_machine_input_s *input,
                                             struct netsnmp_state_machine_step_s *step);

    typedef struct netsnmp_state_machine_step_s {

        const char   *name; /* primarily for logging/debugging */
        u_int         sm_flags;
        
        netsnmp_state_machine_func *run;
        int                         result; /* return code for this step */
        
        
        struct netsnmp_state_machine_step_s *on_success;
        struct netsnmp_state_machine_step_s *on_error;
        
        /*
         * user fields (not touched by state machine functions)
         */
        u_int         flags;
        void         *step_context;
        
    } netsnmp_state_machine_step;

    typedef struct netsnmp_state_machine_input_s {
        const char                  *name;
        int                          steps_so_far;
        netsnmp_state_machine_step  *steps;
        netsnmp_state_machine_step  *cleanup;
        netsnmp_state_machine_step  *last_run;

        /*
         * user fields (not touched by state machine functions)
         */
        void         *input_context;

    } netsnmp_state_machine_input;


    NETSNMP_IMPORT int
    netsnmp_state_machine_run( netsnmp_state_machine_input *input );

    NETSNMP_IMPORT int
    netsnmp_row_create(netsnmp_session *sess, netsnmp_variable_list *vars,
                       int row_status_index);


#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_CLIENT_H */
