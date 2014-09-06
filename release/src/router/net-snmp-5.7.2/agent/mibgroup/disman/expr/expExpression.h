#ifndef EXPEXPRESSION_H
#define EXPEXPRESSION_H

#include "disman/expr/exp_enum.h"

    /*
     * Flags relating to the expression table ....
     */
#define EXP_FLAG_ACTIVE  0x01    /* for expExpressionEntryStatus */
#define EXP_FLAG_FIXED   0x02    /* for snmpd.conf persistence   */
#define EXP_FLAG_VALID   0x04    /* for row creation/undo        */
#define EXP_FLAG_SYSUT   0x08    /* sysUpTime.0 discontinuity    */

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
 * Data structure for an expression row.
 * Covers both expExpressionTable and expErrorTable
 */
struct expExpression {
    /*
     * Index values 
     */
    char            expOwner[ EXP_STR1_LEN+1 ];
    char            expName[  EXP_STR1_LEN+1 ];

    /*
     * Column values for the main expExpressionTable
     */
    char            expExpression[ EXP_STR3_LEN+1 ];
    char            expComment[    EXP_STR2_LEN+1 ];
    oid             expPrefix[     MAX_OID_LEN ];
    size_t          expPrefix_len;
    long            expValueType;
    long            expDeltaInterval;
    u_long          expErrorCount;

    /*
     * Column values for the expExpressionErrorTable
     */
    u_long          expErrorTime;
    long            expErrorIndex;
    long            expErrorCode;
    oid             expErrorInstance[ MAX_OID_LEN ];
    size_t          expErrorInst_len;

    unsigned int    alarm;
    netsnmp_session *session;
    netsnmp_variable_list *pvars;  /* expPrefix values */
    long            sysUpTime;
    long            count;
    long            flags;
};


  /*
   * Container structure for the expExpressionTable,
   * and initialisation routine to create this.
   */
extern netsnmp_tdata *expr_table_data;
extern void      init_expr_table_data(void);

/*
 * function declarations
 */
void             init_expExpression(void);

struct expExpression *expExpression_createEntry(const char *, const char *, int);
netsnmp_tdata_row    *expExpression_createRow(const char *, const char *, int);
void                  expExpression_removeEntry(   netsnmp_tdata_row *);

struct expExpression *expExpression_getEntry(const char *, const char *);
struct expExpression *expExpression_getFirstEntry( void );
struct expExpression *expExpression_getNextEntry(const char *, const char *);

void                  expExpression_enable(  struct expExpression *);
void                  expExpression_disable( struct expExpression *);

void                  expExpression_getData(   unsigned int, void *);
void                  expExpression_evaluate(struct expExpression *);
long                  expExpression_getNumEntries(int);

#endif                          /* EXPEXPRESSIONTABLE_H */
