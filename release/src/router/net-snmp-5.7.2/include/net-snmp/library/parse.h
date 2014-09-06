#ifndef PARSE_H
#define PARSE_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/mib_api.h>

    /*
     * parse.h
     */
/***********************************************************
        Copyright 1989 by Carnegie Mellon University

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

#define NETSNMP_MAXLABEL 64      /* maximum characters in a label */
#define MAXTOKEN        128     /* maximum characters in a token */
#define MAXQUOTESTR     4096    /* maximum characters in a quoted string */

/*
 * MAXLABEL appears to be unused in code, and conflicts with
 * <arpa/nameser.h>. Only define it if requested. This will
 * cause problems if local DNSSEC validation is also enabled.
 */
#ifdef UCD_COMPATIBLE
#define MAXLABEL        NETSNMP_MAXLABEL
#endif

    struct variable_list;

    /*
     * A linked list of tag-value pairs for enumerated integers.
     */
    struct enum_list {
        struct enum_list *next;
        int             value;
        char           *label;
    };

    /*
     * A linked list of ranges
     */
    struct range_list {
        struct range_list *next;
        int             low, high;
    };

    /*
     * A linked list of indexes
     */
    struct index_list {
        struct index_list *next;
        char           *ilabel;
        char            isimplied;
    };

    /*
     * A linked list of varbinds
     */
    struct varbind_list {
        struct varbind_list *next;
        char           *vblabel;
    };

    /*
     * A tree in the format of the tree structure of the MIB.
     */
    struct tree {
        struct tree    *child_list;     /* list of children of this node */
        struct tree    *next_peer;      /* Next node in list of peers */
        struct tree    *next;   /* Next node in hashed list of names */
        struct tree    *parent;
        char           *label;  /* This node's textual name */
        u_long          subid;  /* This node's integer subidentifier */
        int             modid;  /* The module containing this node */
        int             number_modules;
        int            *module_list;    /* To handle multiple modules */
        int             tc_index;       /* index into tclist (-1 if NA) */
        int             type;   /* This node's object type */
        int             access; /* This nodes access */
        int             status; /* This nodes status */
        struct enum_list *enums;        /* (optional) list of enumerated integers */
        struct range_list *ranges;
        struct index_list *indexes;
        char           *augments;
        struct varbind_list *varbinds;
        char           *hint;
        char           *units;
        int             (*printomat) (u_char **, size_t *, size_t *, int,
                                      const netsnmp_variable_list *,
                                      const struct enum_list *, const char *,
                                      const char *);
        void            (*printer) (char *, const netsnmp_variable_list *, const struct enum_list *, const char *, const char *);   /* Value printing function */
        char           *description;    /* description (a quoted string) */
        char           *reference;    /* references (a quoted string) */
        int             reported;       /* 1=report started in print_subtree... */
        char           *defaultValue;
       char	       *parseErrorString; /* Contains the error string if there are errors in parsing MIBs */
    };

    /*
     * Information held about each MIB module
     */
    struct module_import {
        char           *label;  /* The descriptor being imported */
        int             modid;  /* The module imported from */
    };

    struct module {
        char           *name;   /* This module's name */
        char           *file;   /* The file containing the module */
        struct module_import *imports;  /* List of descriptors being imported */
        int             no_imports;     /* The number of such import descriptors */
        /*
         * -1 implies the module hasn't been read in yet 
         */
        int             modid;  /* The index number of this module */
        struct module  *next;   /* Linked list pointer */
    };

    struct module_compatability {
        const char     *old_module;
        const char     *new_module;
        const char     *tag;    /* NULL implies unconditional replacement,
                                 * otherwise node identifier or prefix */
        size_t          tag_len;        /* 0 implies exact match (or unconditional) */
        struct module_compatability *next;      /* linked list */
    };


    /*
     * non-aggregate types for tree end nodes 
     */
#define TYPE_OTHER          0
#define TYPE_OBJID          1
#define TYPE_OCTETSTR       2
#define TYPE_INTEGER        3
#define TYPE_NETADDR        4
#define TYPE_IPADDR         5
#define TYPE_COUNTER        6
#define TYPE_GAUGE          7
#define TYPE_TIMETICKS      8
#define TYPE_OPAQUE         9
#define TYPE_NULL           10
#define TYPE_COUNTER64      11
#define TYPE_BITSTRING      12
#define TYPE_NSAPADDRESS    13
#define TYPE_UINTEGER       14
#define TYPE_UNSIGNED32     15
#define TYPE_INTEGER32      16

#define TYPE_SIMPLE_LAST    16

#define TYPE_TRAPTYPE	    20
#define TYPE_NOTIFTYPE      21
#define TYPE_OBJGROUP	    22
#define TYPE_NOTIFGROUP	    23
#define TYPE_MODID	    24
#define TYPE_AGENTCAP       25
#define TYPE_MODCOMP        26
#define TYPE_OBJIDENTITY    27

#define MIB_ACCESS_READONLY    18
#define MIB_ACCESS_READWRITE   19
#define	MIB_ACCESS_WRITEONLY   20
#define MIB_ACCESS_NOACCESS    21
#define MIB_ACCESS_NOTIFY      67
#define MIB_ACCESS_CREATE      48

#define MIB_STATUS_MANDATORY   23
#define MIB_STATUS_OPTIONAL    24
#define MIB_STATUS_OBSOLETE    25
#define MIB_STATUS_DEPRECATED  39
#define MIB_STATUS_CURRENT     57

#define	ANON	"anonymous#"
#define	ANON_LEN  strlen(ANON)

    int             netsnmp_unload_module(const char *name);
#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
    int             unload_module(const char *name);
#endif
    void            netsnmp_init_mib_internals(void);
    void            unload_all_mibs(void);
    int             add_mibfile(const char*, const char*, FILE *);
    int             which_module(const char *);
    NETSNMP_IMPORT
    char           *module_name(int, char *);
    NETSNMP_IMPORT
    void            print_subtree(FILE *, struct tree *, int);
    NETSNMP_IMPORT
    void            print_ascii_dump_tree(FILE *, struct tree *, int);
    NETSNMP_IMPORT
    struct tree    *find_tree_node(const char *, int);
    NETSNMP_IMPORT
    const char     *get_tc_descriptor(int);
    NETSNMP_IMPORT
    const char     *get_tc_description(int);
    NETSNMP_IMPORT
    struct tree    *find_best_tree_node(const char *, struct tree *,
                                        u_int *);
    /*
     * backwards compatability 
     */
    NETSNMP_IMPORT
    struct tree    *find_node(const char *, struct tree *);
    struct tree    *find_node2(const char *, const char *); 
    NETSNMP_IMPORT
    struct module  *find_module(int);
    void            adopt_orphans(void);
    NETSNMP_IMPORT
    char           *snmp_mib_toggle_options(char *options);
    NETSNMP_IMPORT
    void            snmp_mib_toggle_options_usage(const char *lead,
                                                  FILE * outf);
    NETSNMP_IMPORT
    void            print_mib(FILE *);
    NETSNMP_IMPORT
    void            print_mib_tree(FILE *, struct tree *, int);
    int             get_mib_parse_error_count(void);
    NETSNMP_IMPORT
    int             snmp_get_token(FILE * fp, char *token, int maxtlen);
    NETSNMP_IMPORT
    struct tree    *find_best_tree_node(const char *name,
                                        struct tree *tree_top,
                                        u_int * match);

#ifdef __cplusplus
}
#endif
#endif                          /* PARSE_H */
