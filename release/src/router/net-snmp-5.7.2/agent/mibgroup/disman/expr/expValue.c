/*
 * DisMan Expression MIB:
 *    Core implementation of expression evaluation
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/expr/expExpression.h"
#include "disman/expr/expObject.h"
#include "disman/expr/expValue.h"

#include <ctype.h>

void _expValue_setError( struct expExpression *exp, int reason,
                         oid *suffix, size_t suffix_len,
                         netsnmp_variable_list *var);


#define ASN_PRIV_OPERATOR    (ASN_PRIVATE | 0x0f)
#define ASN_PRIV_FUNCTION    (ASN_PRIVATE | 0x0e)

int ops[128];   /* mapping from operator characters to numeric
                   tokens (ordered by priority). */

void
init_expValue(void)
{
DEBUGMSGTL(("disman:expr:eval", "Init expValue"));
        /* Single-character operators */
    ops['+'] = EXP_OPERATOR_ADD;
    ops['-'] = EXP_OPERATOR_SUBTRACT;
    ops['*'] = EXP_OPERATOR_MULTIPLY;
    ops['/'] = EXP_OPERATOR_DIVIDE;
    ops['%'] = EXP_OPERATOR_REMAINDER;
    ops['^'] = EXP_OPERATOR_BITXOR;
    ops['~'] = EXP_OPERATOR_BITNEGATE;
    ops['|'] = EXP_OPERATOR_BITOR;
    ops['&'] = EXP_OPERATOR_BITAND;
    ops['!'] = EXP_OPERATOR_NOT;
    ops['<'] = EXP_OPERATOR_LESS;
    ops['>'] = EXP_OPERATOR_GREAT;

                                         /*
                                          * Arbitrary offsets, chosen so
                                          * the three blocks don't overlap.
                                          */
        /* "X=" operators */
    ops['='+20] = EXP_OPERATOR_EQUAL;
    ops['!'+20] = EXP_OPERATOR_NOTEQ;
    ops['<'+20] = EXP_OPERATOR_LESSEQ;
    ops['>'+20] = EXP_OPERATOR_GREATEQ;

        /* "XX" operators */
    ops['|'-30] = EXP_OPERATOR_OR;
    ops['&'-30] = EXP_OPERATOR_AND;
    ops['<'-30] = EXP_OPERATOR_LSHIFT;
    ops['>'-30] = EXP_OPERATOR_RSHIFT;
}

    /*
     * Insert the value of the specified object parameter,
     * using the instance 'suffix' for wildcarded objects.
     */
netsnmp_variable_list *
_expValue_evalParam( netsnmp_variable_list *expIdx, int param,
                     oid *suffix, size_t suffix_len )
{
    netsnmp_variable_list *var = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
    struct expObject  *obj;
    netsnmp_variable_list *val_var  = NULL, *oval_var = NULL;  /* values  */
    netsnmp_variable_list *dd_var   = NULL,  *odd_var = NULL;  /* deltaDs */
    netsnmp_variable_list *cond_var = NULL;               /* conditionals */
    int n;

    /*
     * Retrieve the expObject entry for the requested parameter.
     */
    if ( !var || !expIdx || !expIdx->next_variable ||
                 !expIdx->next_variable->next_variable )
        return NULL;

    *expIdx->next_variable->next_variable->val.integer = param;
    obj = (struct expObject *)
               netsnmp_tdata_row_entry(
                   netsnmp_tdata_row_get_byidx( expObject_table_data, expIdx ));
    if (!obj) {
        /*
         * No such parameter configured for this expression
         */
        snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_INDEX );
        var->type = ASN_NULL;
        return var;
    }
    if ( obj->expObjectSampleType != EXPSAMPLETYPE_ABSOLUTE &&
         obj->old_vars == NULL ) {
        /*
         * Can't calculate delta values until the second pass
         */
        snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_RESOURCE );
        var->type = ASN_NULL;
        return var;
    }


    /*
     * For a wildcarded object, search for the matching suffix.
     */
    val_var = obj->vars;
    if ( obj->flags & EXP_OBJ_FLAG_OWILD ) {
        if ( !suffix ) {
            /*
             * If there's no suffix to match against, throw an error.
             * An exact expression with a wildcarded object is invalid.
             *   XXX - Or just use first entry?
             */
            snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_INDEX );
            var->type = ASN_NULL;
            return var;
        }
        /*
         * Otherwise, we'll walk *all* wildcarded values in parallel.
         * This relies on the various varbind lists being set up with
         *   exactly the same entries.  A little extra preparation
         *   during the data gathering simplifies things significantly!
         */
        if ( obj->expObjectSampleType != EXPSAMPLETYPE_ABSOLUTE )
            oval_var = obj->old_vars;
        if ( obj->flags & EXP_OBJ_FLAG_DWILD ) {
            dd_var   = obj->dvars;
            odd_var  = obj->old_dvars;
        }
        if ( obj->flags & EXP_OBJ_FLAG_CWILD )
            cond_var = obj->cvars;
       
        n = obj->expObjectID_len;
        while ( val_var ) {
            if ( snmp_oid_compare( val_var->name+n, val_var->name_length-n,
                                   suffix, suffix_len ))
                break;
            val_var = val_var->next_variable;
            if (oval_var)
                oval_var = oval_var->next_variable;
            if (dd_var) {
                dd_var   =  dd_var->next_variable;
                odd_var  = odd_var->next_variable;
            }
            if (cond_var)
                cond_var = cond_var->next_variable;
        }

    }
    if (!val_var) {
        /*
         * No matching entry
         */
        snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_INDEX );
        var->type = ASN_NULL;
        return var;
    }
        /*
         * Set up any non-wildcarded values - some
         *   of which may be null. That's fine.
         */
    if (!oval_var)
        oval_var = obj->old_vars;
    if (!dd_var) {
        dd_var   = obj->dvars;
        odd_var  = obj->old_dvars;
    }
    if (!cond_var)
        cond_var = obj->cvars;
    

    /*
     * ... and return the appropriate value.
     */
    if (obj->expObjCond_len &&
        (!cond_var || *cond_var->val.integer == 0)) {
        /*
         * expObjectConditional says no
         */
        snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_INDEX );
        var->type = ASN_NULL;
        return var;
    }
    if (dd_var && odd_var &&
        *dd_var->val.integer != *odd_var->val.integer) {
        /*
         * expObjectDeltaD says no
         */
        snmp_set_var_typed_integer( var, ASN_INTEGER, EXPERRCODE_INDEX );
        var->type = ASN_NULL;
        return var;
    }

    /*
     * XXX - May need to check sysUpTime discontinuities
     *            (unless this is handled earlier....)
     */
    switch ( obj->expObjectSampleType ) {
    case EXPSAMPLETYPE_ABSOLUTE:
        snmp_clone_var( val_var, var );
        break;
    case EXPSAMPLETYPE_DELTA:
        snmp_set_var_typed_integer( var, ASN_INTEGER /* or UNSIGNED? */,
                              *val_var->val.integer - *oval_var->val.integer );
        break;
    case EXPSAMPLETYPE_CHANGED:
        if ( val_var->val_len != oval_var->val_len )
            n = 1;
        else if (memcmp( val_var->val.string, oval_var->val.string,
                                               val_var->val_len ) != 0 )
            n = 1;
        else
            n = 0;
        snmp_set_var_typed_integer( var, ASN_UNSIGNED, n );
    }
    return var;
}


    /*
     * Utility routine to parse (and skip over) an integer constant
     */
int
_expParse_integer( char *start, char **end ) {
    int n;
    char *cp;

    n = atoi(start);
    for (cp=start; *cp; cp++)
        if (!isdigit(*cp))
            break;
    *end = cp;
    return n;
}

netsnmp_variable_list *
_expValue_evalOperator(netsnmp_variable_list *left, 
                       netsnmp_variable_list *op, 
                       netsnmp_variable_list *right) {
    int n;

    switch( *op->val.integer ) {
    case EXP_OPERATOR_ADD:
        n = *left->val.integer + *right->val.integer; break;
    case EXP_OPERATOR_SUBTRACT:
        n = *left->val.integer - *right->val.integer; break;
    case EXP_OPERATOR_MULTIPLY:
        n = *left->val.integer * *right->val.integer; break;
    case EXP_OPERATOR_DIVIDE:
        n = *left->val.integer / *right->val.integer; break;
    case EXP_OPERATOR_REMAINDER:
        n = *left->val.integer % *right->val.integer; break;
    case EXP_OPERATOR_BITXOR:
        n = *left->val.integer ^ *right->val.integer; break;
    case EXP_OPERATOR_BITNEGATE:
        n = 99; /* *left->val.integer ~ *right->val.integer; */ break;
    case EXP_OPERATOR_BITOR:
        n = *left->val.integer | *right->val.integer; break;
    case EXP_OPERATOR_BITAND:
        n = *left->val.integer & *right->val.integer; break;
    case EXP_OPERATOR_NOT:
        n = 99; /* *left->val.integer ! *right->val.integer; */ break;
    case EXP_OPERATOR_LESS:
        n = *left->val.integer < *right->val.integer; break;
    case EXP_OPERATOR_GREAT:
        n = *left->val.integer > *right->val.integer; break;
    case EXP_OPERATOR_EQUAL:
        n = *left->val.integer == *right->val.integer; break;
    case EXP_OPERATOR_NOTEQ:
        n = *left->val.integer != *right->val.integer; break;
    case EXP_OPERATOR_LESSEQ:
        n = *left->val.integer <= *right->val.integer; break;
    case EXP_OPERATOR_GREATEQ:
        n = *left->val.integer >= *right->val.integer; break;
    case EXP_OPERATOR_OR:
        n = *left->val.integer || *right->val.integer; break;
    case EXP_OPERATOR_AND:
        n = *left->val.integer && *right->val.integer; break;
    case EXP_OPERATOR_LSHIFT:
        n = *left->val.integer << *right->val.integer; break;
    case EXP_OPERATOR_RSHIFT:
        n = *left->val.integer >> *right->val.integer; break;
        break;
    default:
        left->next_variable = NULL;
        snmp_free_var(left);
        right->next_variable = NULL;
        snmp_free_var(right);

        snmp_set_var_typed_integer( op, ASN_INTEGER, EXPERRCODE_OPERATOR );
        op->type = ASN_NULL;
        return op;
    }

    /* XXX */
    left->next_variable = NULL;
    snmp_free_var(left);
    op->next_variable = NULL;
    snmp_free_var(op);
    snmp_set_var_typed_integer( right, ASN_INTEGER, n );
    return right;
}

netsnmp_variable_list *
_expValue_evalFunction(netsnmp_variable_list *func) {
    netsnmp_variable_list *params = func->next_variable;
    /* XXX */
    params->next_variable = NULL;
    snmp_free_var(params);
    snmp_set_var_typed_integer( func, ASN_INTEGER, 99 );
    return func;
}

netsnmp_variable_list *_expValue_evalExpr2(netsnmp_variable_list *exprAlDente);
netsnmp_variable_list *
_expValue_evalExpr(  netsnmp_variable_list *expIdx,
                     char *exprRaw, char **exprEnd,
                     oid *suffix, size_t suffix_len )
{
    netsnmp_variable_list *exprAlDente = NULL;
    netsnmp_variable_list *vtail = NULL;
    char *cp1, *cp2;
    netsnmp_variable_list *var = NULL;
    int   i, n;
    int   neg = 0;
    oid   oid_buf[MAX_OID_LEN];

    DEBUGMSGTL(("disman:expr:eval1", "Evaluating '%s'\n", exprRaw));
    if (!expIdx || !exprRaw)
        return NULL;

    /*
     * The expression is evaluated in two stages.
     * First, we simplify ("parboil") the raw expression,
     *   tokenizing it into a sequence of varbind values, inserting
     *   object parameters, and (recursively) evaluating any
     *   parenthesised sub-expressions or function arguments.
     */

    for (cp1=exprRaw; cp1 && *cp1; ) {
        switch (*cp1) {
        case '$':
            /*
             * Locate the appropriate instance of the specified
             * parameter, and insert the corresponding value.
             */
            n   = _expParse_integer( cp1+1, &cp1 );
            var = _expValue_evalParam( expIdx, n, suffix, suffix_len );
            if ( var && var->type == ASN_NULL ) {
                DEBUGMSGTL(("disman:expr:eval", "Invalid parameter '%d'\n", n));
                /* Note position of failure in expression */
                var->data = (void *)(cp1 - exprRaw);
                snmp_free_var(exprAlDente);
                return var;
            } else {
                if (vtail)
                   vtail->next_variable = var;
                else
                   exprAlDente = var;
                vtail = var;
                var   = NULL;
            }
            break;
        case '(':
            /*
             * Recursively evaluate the sub-expression
             */
            var = _expValue_evalExpr( expIdx, cp1+1, &cp2,
                                      suffix, suffix_len );
            if ( var && var->type == ASN_NULL ) {
                /* Adjust position of failure */
                var->data = (void *)(cp1 - exprRaw + (int)var->data);
                return var;
            } else if (*cp2 != ')') {
                snmp_free_var(exprAlDente);
                DEBUGMSGTL(("disman:expr:eval", "Unbalanced parenthesis\n"));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_PARENTHESIS );
                var->type = ASN_NULL;
                var->data = (void *)(cp2 - exprRaw);
                return var;
            } else {
                if (vtail)
                   vtail->next_variable = var;
                else
                   exprAlDente = var;
                vtail = var;
                var   = NULL;
                cp1   = cp2+1;   /* Skip to end of sub-expression */
            }
            break;
        case ')':
        case ',':
            /*
             * End of current (sub-)expression
             * Note the end-position, and evaluate
             *  the parboiled list of tokens.
             */
            *exprEnd = cp1;
            var = _expValue_evalExpr2( exprAlDente );
            snmp_free_var(exprAlDente);
            return var;

            /* === Constants === */
        case '.':   /* OID */
            i = 0;
            memset( oid_buf, 0, sizeof(oid_buf));
            while ( cp1 && *cp1 == '.' ) {
                n = _expParse_integer( cp1+1, &cp2 );
OID:
                oid_buf[i++] = n;
                cp1 = cp2;
            }
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_OBJECT_ID,
                                             (u_char*)oid_buf, i*sizeof(oid));
            break;
DIGIT:
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            n = _expParse_integer( cp1, &cp2 );
            if ( cp2 && *cp2 == '.' ) {
                i = 0;
                memset( oid_buf, 0, sizeof(oid_buf));
                goto OID;
            }
            if ( neg )
                n = -n;
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_INTEGER,
                                             (u_char*)&n, sizeof(n));
            vtail = var;
            var   = NULL;
            neg   = 0;
            cp1   = cp2;
            break;

        case '"':   /* String Constant */
            for ( cp2 = cp1+1; *cp2; cp2++ ) {
                if ( *cp2 == '"' )
                    break;
                if ( *cp2 == '\\' && *(cp2+1) == '"' )
                    cp2++;
            }
            if ( *cp2 != '"' ) {
                /*
                 * Unterminated string
                 */
                DEBUGMSGTL(("disman:expr:eval", "Unterminated string\n"));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                var->type = ASN_NULL;
                var->data = (void *)(cp2 - exprRaw);
                return var;
            }
            n = cp2 - cp1;  
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_OCTET_STR,
                                             (u_char*)cp1+1, n);
            vtail = var;
            var   = NULL;
            break;


            /* === Operators === */
        case '-':
            /*
             * Could be a unary minus....
             */
            if (!vtail || vtail->type == ASN_PRIV_OPERATOR) {
                neg = 1;
                goto DIGIT;
            }
            /* 
             * ... or a (single-character) binary operator.
             */
            /* Fallthrough */
        case '+':
        case '*':
        case '/':
        case '%':
        case '^':
        case '~':
            if ( !vtail ) {
                /*
                 * Can't start an expression with a binary operator
                 */
                DEBUGMSGTL(("disman:expr:eval", "Initial binary operator\n"));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                var->type = ASN_NULL;
                var->data = (void *)(cp1 - exprRaw);
                return var;
            }
            n = ops[ *cp1 & 0xFF ];
            DEBUGMSGTL(("disman:expr:eval", "Binary operator %c (%d)\n", *cp1, n));
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_INTEGER,
                                             (u_char*)&n, sizeof(n));
            var->type = ASN_PRIV_OPERATOR;
            vtail = var;
            cp1++;
            break;

            /* 
             * Multi-character binary operators
             */
        case '&':
        case '|':
        case '!':
        case '>':
        case '<':
        case '=':
            if ( !vtail ) {
                /*
                 * Can't start an expression with a binary operator
                 */
                DEBUGMSGTL(("disman:expr:eval", "Initial binary operator\n"));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                var->type = ASN_NULL;
                var->data = (void *)(cp1 - exprRaw);
                return var;
            }
            if ( *(cp1+1) == '=' )
                n = ops[ *cp1++ + 20];
            else if ( *(cp1+1) == *cp1 )
                n = ops[ *cp1++ - 30];
            else
                n = ops[ *cp1 & 0xFF ];
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_INTEGER,
                                             (u_char*)&n, sizeof(n));
            var->type = ASN_PRIV_OPERATOR;
            vtail = var;
            cp1++;
            break;

            /* === Functions === */
        case 'a':    /* average/arraySection */
        case 'c':    /* counter32/counter64  */
        case 'e':    /*    exists            */
        case 'm':    /* maximum/minimum      */
        case 'o':    /* oidBegins/Ends/Contains    */
        case 's':    /* sum / string{B,E,C}  */
            var = snmp_varlist_add_variable( &exprAlDente, NULL, 0,
                                             ASN_OCTET_STR,
                                             (u_char*)cp1, 3 );
                                                   /* XXX */
            var->type = ASN_PRIV_FUNCTION;
            vtail = var;
            while (*cp1 >= 'a' && *cp1 <= 'z')
              cp1++;
            break;

        default:
            if (isalpha( *cp1 )) {
                /*
                 * Unrecognised function call ?
                 */
                DEBUGMSGTL(("disman:expr:eval", "Unrecognised function '%s'\n", cp1));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_FUNCTION );
                var->type = ASN_NULL;
                var->data = (void *)(cp1 - exprRaw);
                return var;
            }
            else if (!isspace( *cp1 )) {
                /*
                 * Unrecognised operator ?
                 */
                DEBUGMSGTL(("disman:expr:eval", "Unrecognised operator '%c'\n", *cp1));
                snmp_set_var_typed_integer( var, ASN_INTEGER,
                                            EXPERRCODE_OPERATOR );
                var->type = ASN_NULL;
                var->data = (void *)(cp1 - exprRaw);
                return var;
            }
            cp1++;
            break;
        }
    }

    /*
     * ... then we evaluate the resulting simplified ("al dente")
     *   expression, in the usual manner.
     */
    *exprEnd = cp1;
    var = _expValue_evalExpr2( exprAlDente );
    DEBUGMSGTL(( "disman:expr:eval1", "Evaluated to "));
    DEBUGMSGVAR(("disman:expr:eval1", var));
    DEBUGMSG((   "disman:expr:eval1", "\n"));
/*  snmp_free_var(exprAlDente); XXX - Crashes */
    return var;
}

netsnmp_variable_list *
_expValue_evalExpr2( netsnmp_variable_list *exprAlD )
{
    netsnmp_variable_list *stack = NULL;
    netsnmp_variable_list *lval, *rval, *op;
    netsnmp_variable_list *vp, *vp1  = NULL;

    DEBUGIF(( "disman:expr:eval2")) {
        for (vp = exprAlD; vp; vp=vp->next_variable) {
            if ( vp->type == ASN_PRIV_OPERATOR )
                DEBUGMSGTL(( "disman:expr:eval2", "Operator %ld\n",
                                                  *vp->val.integer));
            else if ( vp->type == ASN_PRIV_FUNCTION )
                DEBUGMSGTL(( "disman:expr:eval2", "Function %s\n",
                                                   vp->val.string));
            else {
                DEBUGMSGTL(( "disman:expr:eval2", "Operand "));
                DEBUGMSGVAR(("disman:expr:eval2", vp));
                DEBUGMSG((   "disman:expr:eval2", "\n"));
            }
        }
    }
    
    for (vp = exprAlD; vp; vp=vp1) {
        vp1 = vp->next_variable;
        if ( vp->type == ASN_PRIV_OPERATOR ) {
            /* Must *always* follow a value */
            if ( !stack || stack->type == ASN_PRIV_OPERATOR ) {
                snmp_set_var_typed_integer( vp, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                vp->type = ASN_NULL;
                snmp_free_var( stack );
                return vp;
            }
            /*
             * Evaluate any higher priority operators
             *  already on the stack....
             */
            while ( stack->next_variable &&
                    stack->next_variable->type == ASN_PRIV_OPERATOR &&
                  (*stack->next_variable->val.integer > *vp->val.integer)) {
                 rval = stack;
                 op   = stack->next_variable;
                 lval = op->next_variable;
                 stack = lval->next_variable;

                 rval = _expValue_evalOperator( lval, op, rval );
                 rval->next_variable = stack;
                 stack = rval;
            }
            /*
             * ... and then push this operator onto the stack.
             */
            vp->next_variable = stack;
            stack = vp;
        } else if ( vp->type == ASN_PRIV_FUNCTION ) {
            /* Must be first, or follow an operator */
            if ( stack && stack->type != ASN_PRIV_OPERATOR ) {
                snmp_set_var_typed_integer( vp, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                vp->type = ASN_NULL;
                snmp_free_var( stack );
                return vp;
            }
            /*
             * Evaluate this function (consuming the
             *   appropriate parameters from the token 
             *   list), and push the result onto the stack.
             */
            vp = _expValue_evalFunction( vp );
            vp1 = vp->next_variable;
            vp->next_variable = stack;
            stack = vp;
        } else {
            /* Must be first, or follow an operator */
            if ( stack && stack->type != ASN_PRIV_OPERATOR ) {
                snmp_set_var_typed_integer( vp, ASN_INTEGER,
                                            EXPERRCODE_SYNTAX );
                vp->type = ASN_NULL;
                snmp_free_var( stack );
                return vp;
            }
            /*
             * Push this value onto the stack
             */
            vp->next_variable = stack;
            stack = vp;
        }
    }

    /*
     * Now evaluate whatever's left on the stack
     *   and return the final result.
     */
    while ( stack && stack->next_variable ) {
        rval = stack;
        op   = stack->next_variable;
        lval = op->next_variable;
        stack = lval->next_variable;

        rval = _expValue_evalOperator( lval, op, rval );
        rval->next_variable = stack;
        stack = rval;
    }
    return stack;
}

/* =============
 *  Main API 
 * ============= */
netsnmp_variable_list *
expValue_evaluateExpression( struct expExpression *exp,
                             oid *suffix, size_t suffix_len )
{
    char exprRaw[     EXP_STR3_LEN+1 ];
    netsnmp_variable_list *var;
    netsnmp_variable_list owner_var, name_var, param_var;
    long n;
    char *cp;

    if (!exp)
        return NULL;

    /*
     * Set up a varbind list containing the various index values
     *   (including a placeholder for expObjectIndex).
     *
     * This saves having to construct the same index list repeatedly
     */
    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&name_var,  0, sizeof(netsnmp_variable_list));
    memset(&param_var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                  (u_char*)exp->expOwner, strlen(exp->expOwner));
    snmp_set_var_typed_value( &name_var,  ASN_OCTET_STR,
                  (u_char*)exp->expName,  strlen(exp->expName));
    n = 99; /* dummy value */
    snmp_set_var_typed_value( &param_var, ASN_INTEGER,
                             (u_char*)&n, sizeof(long));
    owner_var.next_variable = &name_var;
    name_var.next_variable  = &param_var;

    /*
     * Make a working copy of the expression, and evaluate it.
     */
    memset(exprRaw, 0,                  sizeof(exprRaw));
    memcpy(exprRaw, exp->expExpression, sizeof(exprRaw));

    var = _expValue_evalExpr( &owner_var, exprRaw, &cp, suffix, suffix_len );
    /*
     * Check for any problems, and record the appropriate error
     */
    if (!cp || *cp != '\0') {
        /*
         * When we had finished, there was a lot
         * of bricks^Wcharacters left over....
         */
        _expValue_setError( exp, EXPERRCODE_SYNTAX, suffix, suffix_len, NULL );
        return NULL;
    }
    if (!var) {
        /* Shouldn't happen */
        _expValue_setError( exp, EXPERRCODE_RESOURCE, suffix, suffix_len, NULL );
        return NULL;
    }
    if (var->type == ASN_NULL) {
        /*
         * Error explicitly reported from the evaluation routines.
         */
        _expValue_setError( exp, *(var->val.integer), suffix, suffix_len, var );
        return NULL;
    }
    if (0 /* COMPARE var->type WITH exp->expValueType */ ) {
        /*
         * XXX - Check to see whether the returned type (ASN_XXX)
         *       is compatible with the requested type (an enum)
         */

        /* If not, throw an error */
        _expValue_setError( exp, EXPERRCODE_TYPE, suffix, suffix_len, var );
        return NULL;
    }
    return var;
}

void
_expValue_setError( struct expExpression *exp, int reason,
                    oid *suffix, size_t suffix_len,
                    netsnmp_variable_list *var)
{
    if (!exp)
        return;
    exp->expErrorCount++;
 /* exp->expErrorTime  = NOW; */
    exp->expErrorIndex = ( var && var->data ? (int)var->data : 0 );
    exp->expErrorCode  = reason;
    memset( exp->expErrorInstance, 0, sizeof(exp->expErrorInstance));
    memcpy( exp->expErrorInstance, suffix, suffix_len * sizeof(oid));
    exp->expErrorInst_len = suffix_len;
    snmp_free_var( var );
}
