#ifndef VAR_STRUCT_H
#define VAR_STRUCT_H
/*
 * The subtree structure contains a subtree prefix which applies to
 * all variables in the associated variable list.
 *
 * By converting to a tree of subtree structures, entries can
 * now be subtrees of another subtree in the structure. i.e:
 * 1.2
 * 1.2.0
 */

#define UCD_REGISTRY_OID_MAX_LEN	128

/*
 * subtree flags 
 */
#define FULLY_QUALIFIED_INSTANCE    0x01
#define SUBTREE_ATTACHED	    	0x02

typedef struct netsnmp_subtree_s {
    oid		   *name_a;	/* objid prefix of registered subtree */
    u_char          namelen;    /* number of subid's in name above */
    oid            *start_a;	/* objid of start of covered range */
    u_char          start_len;  /* number of subid's in start name */
    oid            *end_a;	/* objid of end of covered range   */
    u_char          end_len;    /* number of subid's in end name */
    struct variable *variables; /* pointer to variables array */
    int             variables_len;      /* number of entries in above array */
    int             variables_width;    /* sizeof each variable entry */
    char           *label_a;	/* calling module's label */
    netsnmp_session *session;
    u_char          flags;
    u_char          priority;
    int             timeout;
    struct netsnmp_subtree_s *next;       /* List of 'sibling' subtrees */
    struct netsnmp_subtree_s *prev;       /* (doubly-linked list) */
    struct netsnmp_subtree_s *children;   /* List of 'child' subtrees */
    int             range_subid;
    oid             range_ubound;
    netsnmp_handler_registration *reginfo;      /* new API */
    int             cacheid;
    int             global_cacheid;
    size_t          oid_off;
} netsnmp_subtree;

/*
 * This is a new variable structure that doesn't have as much memory
 * tied up in the object identifier.  It's elements have also been re-arranged
 * so that the name field can be variable length.  Any number of these
 * structures can be created with lengths tailor made to a particular
 * application.  The first 5 elements of the structure must remain constant.
 */
struct variable1 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[1];    /* object identifier of variable */
};

struct variable2 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[2];    /* object identifier of variable */
};

struct variable3 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[3];    /* object identifier of variable */
};

struct variable4 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[4];    /* object identifier of variable */
};

struct variable7 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[7];    /* object identifier of variable */
};

struct variable8 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[8];    /* object identifier of variable */
};

struct variable13 {
    u_char          magic;      /* passed to function as a hint */
    u_char          type;       /* type of variable */
    u_short         acl;        /* access control list for variable */
    FindVarMethod  *findVar;    /* function that finds variable */
    u_char          namelen;    /* length of name below */
    oid             name[13];   /* object identifier of variable */
};
#endif                          /* VAR_STRUCT_H */
