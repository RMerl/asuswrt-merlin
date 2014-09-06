#ifndef EXPOBJECT_H
#define EXPOBJECT_H

#include "disman/expr/expExpression.h"

    /*
     * Flags relating to the expression object table
     */
#define EXP_OBJ_FLAG_OWILD   0x01    /* for expObjectIDWildcard           */
#define EXP_OBJ_FLAG_DDISC   0x02    /* non-trivial expObjDiscontinuityID */
#define EXP_OBJ_FLAG_DWILD   0x04    /* for expObjDiscontinuityIDWildcard */
#define EXP_OBJ_FLAG_CWILD   0x08    /* for expObjConditionalWildcard     */
#define EXP_OBJ_FLAG_PREFIX  0x10    /* expExpressionPrefix object        */
#define EXP_OBJ_FLAG_ACTIVE  0x20    /* for expObjectEntryStatus          */
#define EXP_OBJ_FLAG_FIXED   0x40    /* for snmpd.conf persistence        */
#define EXP_OBJ_FLAG_VALID   0x80    /* for row creation/undo             */

    /*
     * Standard lengths for various Expression-MIB OCTET STRING objects:
     *   short tags                   ( 32 characters)
     *   SnmpAdminString-style values (255 characters)
     *   "long" DisplayString values (1024 characters)
     */
#define EXP_STR1_LEN	32
#define EXP_STR2_LEN	255
#define EXP_STR3_LEN	1024

/*
 * Data structure for an expObject row.
 */
struct expObject {
    /*
     * Index values 
     */
    char            expOwner[ EXP_STR1_LEN+1 ];
    char            expName[  EXP_STR1_LEN+1 ];
    u_long          expObjectIndex;

    /*
     * Column values 
     */
    oid             expObjectID[  MAX_OID_LEN ];
    oid             expObjDeltaD[ MAX_OID_LEN ];
    oid             expObjCond[   MAX_OID_LEN ];
    size_t          expObjectID_len;
    size_t          expObjDeltaD_len;
    size_t          expObjCond_len;
    long            expObjectSampleType;
    long            expObjDiscontinuityType;

    netsnmp_variable_list  *vars, *old_vars;
    netsnmp_variable_list *dvars, *old_dvars;
    netsnmp_variable_list *cvars, *old_cvars;

    long            flags;
};

  /*
   * Container structure for the expObjectTable,
   * and initialisation routine to create this.
   */
extern netsnmp_tdata *expObject_table_data;
void             init_expObject_table_data(void);

/*
 * function declarations 
 */
void             init_expObject(void);
struct expObject  * expObject_createEntry( const char *, const char *, long, int );
netsnmp_tdata_row * expObject_createRow(   const char *, const char *, long, int );
void                expObject_removeEntry( netsnmp_tdata_row * );

netsnmp_tdata_row * expObject_getFirst( const char *, const char * );
netsnmp_tdata_row * expObject_getNext(  netsnmp_tdata_row * );
void                expObject_getData( struct expExpression *,
                                       struct expObject * );
#endif                          /* EXPOBJECT_H */
