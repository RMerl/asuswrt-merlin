/*
 * $Id$
 */
#ifndef BABY_STEPS_H
#define BABY_STEPS_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/agent/agent_handler.h>

    /*
     * Flags for baby step modes
     */
#define BABY_STEP_NONE                  0
#define BABY_STEP_PRE_REQUEST           (0x1 <<  1)
#define BABY_STEP_OBJECT_LOOKUP         (0x1 <<  2)
#ifndef NETSNMP_NO_WRITE_SUPPORT
#define BABY_STEP_CHECK_VALUE           (0x1 <<  3)
#define BABY_STEP_ROW_CREATE            (0x1 <<  4)
#define BABY_STEP_UNDO_SETUP            (0x1 <<  5)
#define BABY_STEP_SET_VALUE             (0x1 <<  6)
#define BABY_STEP_CHECK_CONSISTENCY     (0x1 <<  7)
#define BABY_STEP_UNDO_SET              (0x1 <<  8)
#define BABY_STEP_COMMIT                (0x1 <<  9)
#define BABY_STEP_UNDO_COMMIT           (0x1 << 10)
#define BABY_STEP_IRREVERSIBLE_COMMIT   (0x1 << 11)
#define BABY_STEP_UNDO_CLEANUP          (0x1 << 12)
#endif /* NETSNMP_NO_WRITE_SUPPORT */
#define BABY_STEP_POST_REQUEST          (0x1 << 13)

#define BABY_STEP_ALL                   (0xffffffff)


#ifndef NETSNMP_NO_WRITE_SUPPORT
#define BABY_STEP_CHECK_OBJECT          BABY_STEP_CHECK_VALUE
#define BABY_STEP_SET_VALUES            BABY_STEP_SET_VALUE
#define BABY_STEP_UNDO_SETS             BABY_STEP_UNDO_SET
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/** @name baby_steps
 *
 * This helper expands the original net-snmp set modes into the newer, finer
 * grained modes.
 *
 *  @{ */

    typedef struct netsnmp_baby_steps_modes_s {
       /** Number of handlers whose myvoid pointer points at this object. */
       int         refcnt;
       u_int       registered;
       u_int       completed;
    } netsnmp_baby_steps_modes;

void                 netsnmp_baby_steps_init(void);

netsnmp_mib_handler *netsnmp_baby_steps_handler_get(u_long modes);

/** @} */


/** @name access_multiplexer
 *
 * This helper calls individual access methods based on the mode. All
 * access methods share the same handler, and the same myvoid pointer.
 * If you need individual myvoid pointers, check out the multiplexer
 * handler (though it currently only works for traditional modes).
 *
 *  @{ */

/** @struct netsnmp_mib_handler_access_methods
 *  Defines the access methods to be called by the access_multiplexer helper
 */
typedef struct netsnmp_baby_steps_access_methods_s {
      
   /*
    * baby step modes
    */
   Netsnmp_Node_Handler *pre_request;
   Netsnmp_Node_Handler *object_lookup;
   Netsnmp_Node_Handler *get_values;
#ifndef NETSNMP_NO_WRITE_SUPPORT
   Netsnmp_Node_Handler *object_syntax_checks;
   Netsnmp_Node_Handler *row_creation;
   Netsnmp_Node_Handler *undo_setup;
   Netsnmp_Node_Handler *set_values;
   Netsnmp_Node_Handler *consistency_checks;
   Netsnmp_Node_Handler *commit;
   Netsnmp_Node_Handler *undo_sets;
   Netsnmp_Node_Handler *undo_cleanup;
   Netsnmp_Node_Handler *undo_commit;
   Netsnmp_Node_Handler *irreversible_commit;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
   Netsnmp_Node_Handler *post_request;

   void                 *my_access_void;

} netsnmp_baby_steps_access_methods;

    netsnmp_mib_handler * netsnmp_baby_steps_access_multiplexer_get(
        netsnmp_baby_steps_access_methods *);

    int netsnmp_baby_step_mode2flag( u_int mode );

/** @} */


/** backwards compatability. don't use in new code */
#define netsnmp_get_baby_steps_handler netsnmp_baby_steps_handler_get
#define netsnmp_init_baby_steps_helper netsnmp_baby_steps_handler_init


#ifdef __cplusplus
}
#endif
#endif /* baby_steps */
