/*
 * usmUser.c
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <stdlib.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "util_funcs/header_generic.h"
#include "usmUser.h"

#ifndef NETSNMP_NO_WRITE_SUPPORT 
int usmStatusCheck(struct usmUser *uptr);
#endif  /* !NETSNMP_NO_WRITE_SUPPORT */

netsnmp_feature_child_of(usmuser_all, libnetsnmpmibs)
netsnmp_feature_child_of(init_register_usmuser_context, usmuser_all)

struct variable4 usmUser_variables[] = {
    {USMUSERSPINLOCK, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 1, {1}},
    {USMUSERSECURITYNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_usmUser, 3, {2, 1, 3}},
    {USMUSERCLONEFROM, ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 4}},
    {USMUSERAUTHPROTOCOL, ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 5}},
    {USMUSERAUTHKEYCHANGE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 6}},
    {USMUSEROWNAUTHKEYCHANGE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 7}},
    {USMUSERPRIVPROTOCOL, ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 8}},
    {USMUSERPRIVKEYCHANGE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 9}},
    {USMUSEROWNPRIVKEYCHANGE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 10}},
    {USMUSERPUBLIC, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 11}},
    {USMUSERSTORAGETYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 12}},
    {USMUSERSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_usmUser, 3, {2, 1, 13}},

};

oid             usmUser_variables_oid[] = { 1, 3, 6, 1, 6, 3, 15, 1, 2 };


/*
 * needed for the write_ functions to find the start of the index 
 */
#define USM_MIB_LENGTH 12

static unsigned int usmUserSpinLock = 0;

void
init_usmUser(void)
{
    REGISTER_MIB("snmpv3/usmUser", usmUser_variables, variable4,
                 usmUser_variables_oid);
}

#ifndef NETSNMP_FEATURE_REMOVE_INIT_REGISTER_USMUSER_CONTEXT
void
init_register_usmUser_context(const char *contextName) {
    register_mib_context("snmpv3/usmUser",
                         (struct variable *) usmUser_variables,
                         sizeof(struct variable4),
                         sizeof(usmUser_variables)/sizeof(struct variable4),
                         usmUser_variables_oid,
                         sizeof(usmUser_variables_oid)/sizeof(oid),
                         DEFAULT_MIB_PRIORITY, 0, 0, NULL,
                         contextName, -1, 0);
}
#endif /* NETSNMP_FEATURE_REMOVE_INIT_REGISTER_USMUSER_CONTEXT */

/*******************************************************************-o-******
 * usm_generate_OID
 *
 * Parameters:
 *	*prefix		(I) OID prefix to the usmUser table entry.
 *	 prefixLen	(I)
 *	*uptr		(I) Pointer to a user in the user list.
 *	*length		(O) Length of generated index OID.
 *      
 * Returns:
 *	Pointer to the OID index for the user (uptr)  -OR-
 *	NULL on failure.
 *
 *
 * Generate the index OID for a given usmUser name.  'length' is set to
 * the length of the index OID.
 *
 * Index OID format is:
 *
 *    <...prefix>.<engineID_length>.<engineID>.<user_name_length>.<user_name>
 */
oid            *
usm_generate_OID(oid * prefix, size_t prefixLen, struct usmUser *uptr,
                 size_t * length)
{
    oid            *indexOid;
    int             i;

    *length = 2 + uptr->engineIDLen + strlen(uptr->name) + prefixLen;
    indexOid = (oid *) malloc(*length * sizeof(oid));
    if (indexOid) {
        memmove(indexOid, prefix, prefixLen * sizeof(oid));

        indexOid[prefixLen] = uptr->engineIDLen;
        for (i = 0; i < (int) uptr->engineIDLen; i++)
            indexOid[prefixLen + 1 + i] = (oid) uptr->engineID[i];

        indexOid[prefixLen + uptr->engineIDLen + 1] = strlen(uptr->name);
        for (i = 0; i < (int) strlen(uptr->name); i++)
            indexOid[prefixLen + uptr->engineIDLen + 2 + i] =
                (oid) uptr->name[i];
    }
    return indexOid;

}                               /* end usm_generate_OID() */

/*
 * usm_parse_oid(): parses an index to the usmTable to break it down into
 * a engineID component and a name component.  The results are stored in:
 * 
 * **engineID:   a newly malloced string.
 * *engineIDLen: The length of the malloced engineID string above.
 * **name:       a newly malloced string.
 * *nameLen:     The length of the malloced name string above.
 * 
 * returns 1 if an error is encountered, or 0 if successful.
 */
int
usm_parse_oid(oid * oidIndex, size_t oidLen,
              unsigned char **engineID, size_t * engineIDLen,
              unsigned char **name, size_t * nameLen)
{
    int             nameL;
    int             engineIDL;
    int             i;

    /*
     * first check the validity of the oid 
     */
    if ((oidLen <= 0) || (!oidIndex)) {
        DEBUGMSGTL(("usmUser",
                    "parse_oid: null oid or zero length oid passed in\n"));
        return 1;
    }
    engineIDL = *oidIndex;      /* initial engineID length */
    if ((int) oidLen < engineIDL + 2) {
        DEBUGMSGTL(("usmUser",
                    "parse_oid: invalid oid length: less than the engineIDLen\n"));
        return 1;
    }
    nameL = oidIndex[engineIDL + 1];    /* the initial name length */
    if ((int) oidLen != engineIDL + nameL + 2) {
        DEBUGMSGTL(("usmUser",
                    "parse_oid: invalid oid length: length is not exact\n"));
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if (engineID == NULL || name == NULL) {
        DEBUGMSGTL(("usmUser",
                    "parse_oid: null storage pointer passed in.\n"));
        return 1;
    }

    *engineID = (unsigned char *) malloc(engineIDL);
    if (*engineID == NULL) {
        DEBUGMSGTL(("usmUser",
                    "parse_oid: malloc of the engineID failed\n"));
        return 1;
    }
    *engineIDLen = engineIDL;

    *name = (unsigned char *) malloc(nameL + 1);
    if (*name == NULL) {
        DEBUGMSGTL(("usmUser", "parse_oid: malloc of the name failed\n"));
        free(*engineID);
        return 1;
    }
    *nameLen = nameL;

    for (i = 0; i < engineIDL; i++) {
        if (oidIndex[i + 1] > 255) {
            goto UPO_parse_error;
        }
        engineID[0][i] = (unsigned char) oidIndex[i + 1];
    }

    for (i = 0; i < nameL; i++) {
        if (oidIndex[i + 2 + engineIDL] > 255) {
          UPO_parse_error:
            free(*engineID);
            free(*name);
            return 1;
        }
        name[0][i] = (unsigned char) oidIndex[i + 2 + engineIDL];
    }
    name[0][nameL] = 0;

    return 0;

}                               /* end usm_parse_oid() */

/*******************************************************************-o-******
 * usm_parse_user
 *
 * Parameters:
 *	*name		Complete OID indexing a given usmUser entry.
 *	 name_length
 *      
 * Returns:
 *	Pointer to a usmUser  -OR-
 *	NULL if name does not convert to a usmUser.
 * 
 * Convert an (full) OID and return a pointer to a matching user in the
 * user list if one exists.
 */
struct usmUser *
usm_parse_user(oid * name, size_t name_len)
{
    struct usmUser *uptr;

    char           *newName;
    u_char         *engineID;
    size_t          nameLen, engineIDLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if (usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                      &engineID, &engineIDLen, (u_char **) & newName,
                      &nameLen))
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    uptr = usm_get_user(engineID, engineIDLen, newName);
    free(engineID);
    free(newName);

    return uptr;

}                               /* end usm_parse_user() */

/*******************************************************************-o-******
 * var_usmUser
 *
 * Parameters:
 *	  *vp	   (I)     Variable-binding associated with this action.
 *	  *name	   (I/O)   Input name requested, output name found.
 *	  *length  (I/O)   Length of input and output oid's.
 *	   exact   (I)     TRUE if an exact match was requested.
 *	  *var_len (O)     Length of variable or 0 if function returned.
 *	(**write_method)   Hook to name a write method (UNUSED).
 *      
 * Returns:
 *	Pointer to (char *) containing related data of length 'length'
 *	  (May be NULL.)
 *
 *
 * Call-back function passed to the agent in order to return information
 * for the USM MIB tree.
 *
 *
 * If this invocation is not for USMUSERSPINLOCK, lookup user name
 * in the usmUser list.
 *
 * If the name does not match any user and the request
 * is for an exact match, -or- if the usmUser list is empty, create a 
 * new list entry.
 *
 * Finally, service the given USMUSER* var-bind.  A NULL user generally
 * results in a NULL return value.
 */
u_char         *
var_usmUser(struct variable * vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    struct usmUser *uptr = NULL, *nptr;
    int             i, rtest, result;
    oid            *indexOid;
    size_t          len;

    /*
     * variables we may use later 
     */
    static long     long_ret;
    static u_char   string[1];
    static oid      objid[2];   /* for .0.0 */

    if (!vp || !name || !length || !var_len)
        return NULL;

    /* assume it isnt writable for the time being */
    *write_method = (WriteMethod*)0;    

    /* assume an integer and change later if not */
    *var_len = sizeof(long_ret);

    if (vp->magic != USMUSERSPINLOCK) {
        oid             newname[MAX_OID_LEN];
        len = (*length < vp->namelen) ? *length : vp->namelen;
        rtest = snmp_oid_compare(name, len, vp->name, len);
        if (rtest > 0 ||
            /*
             * (rtest == 0 && !exact && (int) vp->namelen+1 < (int) *length) || 
             */
            (exact == 1 && rtest != 0)) {
            if (var_len)
                *var_len = 0;
            return NULL;
        }
        memset(newname, 0, sizeof(newname));
        if (((int) *length) <= (int) vp->namelen || rtest == -1) {
            /*
             * oid is not within our range yet 
             */
            /*
             * need to fail if not exact 
             */
            uptr = usm_get_userList();

        } else {
            for (nptr = usm_get_userList(), uptr = NULL;
                 nptr != NULL; nptr = nptr->next) {
                indexOid =
                    usm_generate_OID(vp->name, vp->namelen, nptr, &len);
                result = snmp_oid_compare(name, *length, indexOid, len);
                DEBUGMSGTL(("usmUser", "Checking user: %s - ",
                            nptr->name));
                for (i = 0; i < (int) nptr->engineIDLen; i++) {
                    DEBUGMSG(("usmUser", " %x", nptr->engineID[i]));
                }
                DEBUGMSG(("usmUser", " - %d \n  -> OID: ", result));
                DEBUGMSGOID(("usmUser", indexOid, len));
                DEBUGMSG(("usmUser", "\n"));

                free(indexOid);

                if (exact) {
                    if (result == 0) {
                        uptr = nptr;
                    }
                } else {
                    if (result == 0) {
                        /*
                         * found an exact match.  Need the next one for !exact 
                         */
                        uptr = nptr->next;
                    } else if (result == -1) {
                        uptr = nptr;
                        break;
                    }
                }
            }
        }                       /* endif -- name <= vp->name */

        /*
         * if uptr is NULL and exact we need to continue for creates 
         */
        if (uptr == NULL && !exact)
            return (NULL);

        if (uptr) {
            indexOid = usm_generate_OID(vp->name, vp->namelen, uptr, &len);
            *length = len;
            memmove(name, indexOid, len * sizeof(oid));

            DEBUGMSGTL(("usmUser", "Found user: %s - ", uptr->name));
            for (i = 0; i < (int) uptr->engineIDLen; i++) {
                DEBUGMSG(("usmUser", " %x", uptr->engineID[i]));
            }
            DEBUGMSG(("usmUser", "\n  -> OID: "));
            DEBUGMSGOID(("usmUser", indexOid, len));
            DEBUGMSG(("usmUser", "\n"));

            free(indexOid);
        }
    } else {
        if (header_generic(vp, name, length, exact, var_len, write_method))
            return NULL;
    }                           /* endif -- vp->magic != USMUSERSPINLOCK */

    switch (vp->magic) {
#ifndef NETSNMP_NO_WRITE_SUPPORT 
    case USMUSERSPINLOCK:
        *write_method = write_usmUserSpinLock;
        long_ret = usmUserSpinLock;
        return (unsigned char *) &long_ret;

    case USMUSERSECURITYNAME:
        if (uptr) {
            *var_len = strlen(uptr->secName);
            return (unsigned char *) uptr->secName;
        }
        return NULL;

    case USMUSERCLONEFROM:
        *write_method = write_usmUserCloneFrom;
        if (uptr) {
            objid[0] = 0;       /* "When this object is read, the ZeroDotZero OID */
            objid[1] = 0;       /*  is returned." */
            *var_len = sizeof(oid) * 2;
            return (unsigned char *) objid;
        }
        return NULL;

    case USMUSERAUTHPROTOCOL:
        *write_method = write_usmUserAuthProtocol;
        if (uptr) {
            *var_len = uptr->authProtocolLen * sizeof(oid);
            return (u_char *) uptr->authProtocol;
        }
        return NULL;

    case USMUSERAUTHKEYCHANGE:
    case USMUSEROWNAUTHKEYCHANGE:
        /*
         * we treat these the same, and let the calling module
         * distinguish between them 
         */
        *write_method = write_usmUserAuthKeyChange;
        if (uptr) {
            *string = 0;        /* always return a NULL string */
            *var_len = 0;
            return string;
        }
        return NULL;

    case USMUSERPRIVPROTOCOL:
        *write_method = write_usmUserPrivProtocol;
        if (uptr) {
            *var_len = uptr->privProtocolLen * sizeof(oid);
            return (u_char *) uptr->privProtocol;
        }
        return NULL;

    case USMUSERPRIVKEYCHANGE:
    case USMUSEROWNPRIVKEYCHANGE:
        /*
         * we treat these the same, and let the calling module
         * distinguish between them 
         */
        *write_method = write_usmUserPrivKeyChange;
        if (uptr) {
            *string = 0;        /* always return a NULL string */
            *var_len = 0;
            return string;
        }
        return NULL;

    case USMUSERPUBLIC:
        *write_method = write_usmUserPublic;
        if (uptr) {
            if (uptr->userPublicString) {
                *var_len = uptr->userPublicStringLen;
                return uptr->userPublicString;
            }
            *string = 0;
            *var_len = 0;       /* return an empty string if the public
                                 * string hasn't been defined yet */
            return string;
        }
        return NULL;

    case USMUSERSTORAGETYPE:
        *write_method = write_usmUserStorageType;
        if (uptr) {
            long_ret = uptr->userStorageType;
            return (unsigned char *) &long_ret;
        }
        return NULL;

    case USMUSERSTATUS:
        *write_method = write_usmUserStatus;
        if (uptr) {
            long_ret = uptr->userStatus;
            return (unsigned char *) &long_ret;
        }
        return NULL;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_usmUser\n",
                    vp->magic));
#else /* !NETSNMP_NO_WRITE_SUPPORT */ 
    default:
        DEBUGMSGTL(("snmpd", "no write support for var_usmUser\n"));
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    }
    return NULL;

}                               /* end var_usmUser() */


#ifndef NETSNMP_NO_WRITE_SUPPORT 

/*
 * write_usmUserSpinLock(): called when a set is performed on the
 * usmUserSpinLock object 
 */
int
write_usmUserSpinLock(int action,
                      u_char * var_val,
                      u_char var_val_type,
                      size_t var_val_len,
                      u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;

    if (var_val_type != ASN_INTEGER) {
        DEBUGMSGTL(("usmUser",
                    "write to usmUserSpinLock not ASN_INTEGER\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        DEBUGMSGTL(("usmUser", "write to usmUserSpinLock: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    long_ret = *((long *) var_val);
    if (long_ret != (long) usmUserSpinLock)
        return SNMP_ERR_INCONSISTENTVALUE;
    if (action == COMMIT) {
        if (usmUserSpinLock == 2147483647)
            usmUserSpinLock = 0;
        else
            usmUserSpinLock++;
    }
    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserSpinLock() */

/*******************************************************************-o-******
 * write_usmUserCloneFrom
 *
 * Parameters:
 *	 action
 *	*var_val
 *	 var_val_type
 *	 var_val_len
 *	*statP		(UNUSED)
 *	*name		OID of user to clone from.
 *	 name_len
 *      
 * Returns:
 *	SNMP_ERR_NOERROR		On success  -OR-  If user exists
 *					  and has already been cloned.
 *	SNMP_ERR_GENERR			Local function call failures.
 *	SNMP_ERR_INCONSISTENTNAME	'name' does not exist in user list
 *					  -OR-  user to clone from != RS_ACTIVE.
 *	SNMP_ERR_WRONGLENGTH		OID length > than local buffer size.
 *	SNMP_ERR_WRONGTYPE		ASN_OBJECT_ID is wrong.
 *
 *
 * XXX:  should handle action=UNDO's.
 */
int
write_usmUserCloneFrom(int action,
                       u_char * var_val,
                       u_char var_val_type,
                       size_t var_val_len,
                       u_char * statP, oid * name, size_t name_len)
{
    struct usmUser *uptr, *cloneFrom;

    if (action == RESERVE1) {
        if (var_val_type != ASN_OBJECT_ID) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserCloneFrom not ASN_OBJECT_ID\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > USM_LENGTH_OID_MAX * sizeof(oid) ||
            var_val_len % sizeof(oid) != 0) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserCloneFrom: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            /*
             * We don't allow creations here.  
             */
            return SNMP_ERR_INCONSISTENTNAME;
        }

        /*
         * Has the user already been cloned?  If so, writes to this variable
         * are defined to have no effect and to produce no error.  
         */
        if (uptr->cloneFrom != NULL) {
            return SNMP_ERR_NOERROR;
        }

        cloneFrom =
            usm_parse_user((oid *) var_val, var_val_len / sizeof(oid));
        if (cloneFrom == NULL || cloneFrom->userStatus != SNMP_ROW_ACTIVE) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        uptr->cloneFrom = snmp_duplicate_objid((oid *) var_val,
                                               var_val_len / sizeof(oid));
        usm_cloneFrom_user(cloneFrom, uptr);

        if (usmStatusCheck(uptr) && uptr->userStatus == SNMP_ROW_NOTREADY) {
            uptr->userStatus = SNMP_ROW_NOTINSERVICE;
        }
    }

    return SNMP_ERR_NOERROR;
}

/*******************************************************************-o-******
 * write_usmUserAuthProtocol
 *
 * Parameters:
 *	 action
 *	*var_val	OID of auth transform to set.
 *	 var_val_type
 *	 var_val_len
 *	*statP
 *	*name		OID of user upon which to perform set operation.
 *	 name_len
 *      
 * Returns:
 *	SNMP_ERR_NOERROR		On success.
 *	SNMP_ERR_GENERR
 *	SNMP_ERR_INCONSISTENTVALUE
 *	SNMP_ERR_NOSUCHNAME
 *	SNMP_ERR_WRONGLENGTH
 *	SNMP_ERR_WRONGTYPE
 */
int
write_usmUserAuthProtocol(int action,
                          u_char * var_val,
                          u_char var_val_type,
                          size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len)
{
    static oid     *optr;
    static size_t   olen;
    static int      resetOnFail;
    struct usmUser *uptr;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OBJECT_ID) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserAuthProtocol not ASN_OBJECT_ID\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > USM_LENGTH_OID_MAX * sizeof(oid) ||
            var_val_len % sizeof(oid) != 0) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserAuthProtocol: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }

        if (uptr->userStatus == RS_ACTIVE
            || uptr->userStatus == RS_NOTREADY
            || uptr->userStatus == RS_NOTINSERVICE) {
            /*
             * The authProtocol is already set.  It is only legal to CHANGE it
             * to usmNoAuthProtocol...  
             */
            if (snmp_oid_compare
                ((oid *) var_val, var_val_len / sizeof(oid),
                 usmNoAuthProtocol,
                 sizeof(usmNoAuthProtocol) / sizeof(oid)) == 0) {
                /*
                 * ... and then only if the privProtocol is equal to
                 * usmNoPrivProtocol.  
                 */
                if (snmp_oid_compare
                    (uptr->privProtocol, uptr->privProtocolLen,
                     usmNoPrivProtocol,
                     sizeof(usmNoPrivProtocol) / sizeof(oid)) != 0) {
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
                optr = uptr->authProtocol;
                olen = uptr->authProtocolLen;
                resetOnFail = 1;
                uptr->authProtocol = snmp_duplicate_objid((oid *) var_val,
                                                          var_val_len /
                                                          sizeof(oid));
                if (uptr->authProtocol == NULL) {
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->authProtocolLen = var_val_len / sizeof(oid);
            } else
                if (snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     uptr->authProtocol, uptr->authProtocolLen) == 0) {
                /*
                 * But it's also okay to set it to the same thing as it
                 * currently is.  
                 */
                return SNMP_ERR_NOERROR;
            } else {
                return SNMP_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * This row is under creation.  It's okay to set
             * usmUserAuthProtocol to any valid authProtocol but it will be
             * overwritten when usmUserCloneFrom is set (so don't write it if
             * that has already been set).  
             */

            if (snmp_oid_compare
                ((oid *) var_val, var_val_len / sizeof(oid),
                 usmNoAuthProtocol,
                 sizeof(usmNoAuthProtocol) / sizeof(oid)) == 0
#ifndef NETSNMP_DISABLE_MD5
                || snmp_oid_compare((oid *) var_val,
                                    var_val_len / sizeof(oid),
                                    usmHMACMD5AuthProtocol,
                                    sizeof(usmHMACMD5AuthProtocol) /
                                    sizeof(oid)) == 0
#endif
                || snmp_oid_compare((oid *) var_val,
                                    var_val_len / sizeof(oid),
                                    usmHMACSHA1AuthProtocol,
                                    sizeof(usmHMACSHA1AuthProtocol) /
                                    sizeof(oid)) == 0) {
                if (uptr->cloneFrom != NULL) {
                    optr = uptr->authProtocol;
                    olen = uptr->authProtocolLen;
                    resetOnFail = 1;
                    uptr->authProtocol =
                        snmp_duplicate_objid((oid *) var_val,
                                             var_val_len / sizeof(oid));
                    if (uptr->authProtocol == NULL) {
                        return SNMP_ERR_RESOURCEUNAVAILABLE;
                    }
                    uptr->authProtocolLen = var_val_len / sizeof(oid);
                }
            } else {
                /*
                 * Unknown authentication protocol.  
                 */
                return SNMP_ERR_WRONGVALUE;
            }
        }
    } else if (action == COMMIT) {
        SNMP_FREE(optr);
    } else if (action == FREE || action == UNDO) {
        if ((uptr = usm_parse_user(name, name_len)) != NULL) {
            if (resetOnFail) {
                SNMP_FREE(uptr->authProtocol);
                uptr->authProtocol = optr;
                uptr->authProtocolLen = olen;
            }
        }
    }
    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserAuthProtocol() */

/*******************************************************************-o-******
 * write_usmUserAuthKeyChange
 *
 * Parameters:
 *	 action		
 *	*var_val	Octet string representing new KeyChange value.
 *	 var_val_type
 *	 var_val_len
 *	*statP		(UNUSED)
 *	*name		OID of user upon which to perform set operation.
 *	 name_len
 *      
 * Returns:
 *	SNMP_ERR_NOERR		Success.
 *	SNMP_ERR_WRONGTYPE	
 *	SNMP_ERR_WRONGLENGTH	
 *	SNMP_ERR_NOSUCHNAME	
 *	SNMP_ERR_GENERR
 *
 * Note: This function handles both the usmUserAuthKeyChange and
 *       usmUserOwnAuthKeyChange objects.  We are not passed the name
 *       of the user requseting the keychange, so we leave this to the
 *       calling module to verify when and if we should be called.  To
 *       change this would require a change in the mib module API to
 *       pass in the securityName requesting the change.
 *
 * XXX:  should handle action=UNDO's.
 */
int
write_usmUserAuthKeyChange(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    struct usmUser *uptr;
    unsigned char   buf[SNMP_MAXBUF_SMALL];
    size_t          buflen = SNMP_MAXBUF_SMALL;
    const char      fnAuthKey[] = "write_usmUserAuthKeyChange";
    const char      fnOwnAuthKey[] = "write_usmUserOwnAuthKeyChange";
    const char     *fname;
    static unsigned char *oldkey;
    static size_t   oldkeylen;
    static int      resetOnFail;

    if (name[USM_MIB_LENGTH - 1] == 6) {
        fname = fnAuthKey;
    } else {
        fname = fnOwnAuthKey;
    }

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("usmUser", "write to %s not ASN_OCTET_STR\n",
                        fname));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len == 0) {
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
#ifndef NETSNMP_DISABLE_MD5
            if (snmp_oid_compare(uptr->authProtocol, uptr->authProtocolLen,
                                 usmHMACMD5AuthProtocol,
                                 sizeof(usmHMACMD5AuthProtocol) /
                                 sizeof(oid)) == 0) {
                if (var_val_len != 0 && var_val_len != 32) {
                    return SNMP_ERR_WRONGLENGTH;
                }
            } else
#endif
                if (snmp_oid_compare
                    (uptr->authProtocol, uptr->authProtocolLen,
                     usmHMACSHA1AuthProtocol,
                     sizeof(usmHMACSHA1AuthProtocol) / sizeof(oid)) == 0) {
                if (var_val_len != 0 && var_val_len != 40) {
                    return SNMP_ERR_WRONGLENGTH;
                }
            }
        }
    } else if (action == ACTION) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        if (uptr->cloneFrom == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        if (snmp_oid_compare(uptr->authProtocol, uptr->authProtocolLen,
                             usmNoAuthProtocol,
                             sizeof(usmNoAuthProtocol) / sizeof(oid)) ==
            0) {
            /*
             * "When the value of the corresponding usmUserAuthProtocol is
             * usmNoAuthProtocol, then a set is successful, but effectively
             * is a no-op."  
             */
            DEBUGMSGTL(("usmUser",
                        "%s: noAuthProtocol keyChange... success!\n",
                        fname));
            return SNMP_ERR_NOERROR;
        }

        /*
         * Change the key.  
         */
        DEBUGMSGTL(("usmUser", "%s: changing auth key for user %s\n",
                    fname, uptr->secName));

        if (decode_keychange(uptr->authProtocol, uptr->authProtocolLen,
                             uptr->authKey, uptr->authKeyLen,
                             var_val, var_val_len,
                             buf, &buflen) != SNMPERR_SUCCESS) {
            DEBUGMSGTL(("usmUser", "%s: ... failed\n", fname));
            return SNMP_ERR_GENERR;
        }
        DEBUGMSGTL(("usmUser", "%s: ... succeeded\n", fname));
        resetOnFail = 1;
        oldkey = uptr->authKey;
        oldkeylen = uptr->authKeyLen;
        memdup(&uptr->authKey, buf, buflen);
        if (uptr->authKey == NULL) {
            return SNMP_ERR_RESOURCEUNAVAILABLE;
        }
        uptr->authKeyLen = buflen;
    } else if (action == COMMIT) {
        SNMP_FREE(oldkey);
    } else if (action == UNDO) {
        if ((uptr = usm_parse_user(name, name_len)) != NULL && resetOnFail) {
            SNMP_FREE(uptr->authKey);
            uptr->authKey = oldkey;
            uptr->authKeyLen = oldkeylen;
        }
    }

    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserAuthKeyChange() */

int
write_usmUserPrivProtocol(int action,
                          u_char * var_val,
                          u_char var_val_type,
                          size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len)
{
    static oid     *optr;
    static size_t   olen;
    static int      resetOnFail;
    struct usmUser *uptr;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OBJECT_ID) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserPrivProtocol not ASN_OBJECT_ID\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > USM_LENGTH_OID_MAX * sizeof(oid) ||
            var_val_len % sizeof(oid) != 0) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserPrivProtocol: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }

        if (uptr->userStatus == RS_ACTIVE
            || uptr->userStatus == RS_NOTREADY
            || uptr->userStatus == RS_NOTINSERVICE) {
            /*
             * The privProtocol is already set.  It is only legal to CHANGE it
             * to usmNoPrivProtocol.  
             */
            if (snmp_oid_compare
                ((oid *) var_val, var_val_len / sizeof(oid),
                 usmNoPrivProtocol,
                 sizeof(usmNoPrivProtocol) / sizeof(oid)) == 0) {
                resetOnFail = 1;
                optr = uptr->privProtocol;
                olen = uptr->privProtocolLen;
                uptr->privProtocol = snmp_duplicate_objid((oid *) var_val,
                                                          var_val_len /
                                                          sizeof(oid));
                if (uptr->privProtocol == NULL) {
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->privProtocolLen = var_val_len / sizeof(oid);
            } else
                if (snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     uptr->privProtocol, uptr->privProtocolLen) == 0) {
                /*
                 * But it's also okay to set it to the same thing as it
                 * currently is.  
                 */
                return SNMP_ERR_NOERROR;
            } else {
                return SNMP_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * This row is under creation.  It's okay to set
             * usmUserPrivProtocol to any valid privProtocol with the proviso
             * that if usmUserAuthProtocol is set to usmNoAuthProtocol, it may
             * only be set to usmNoPrivProtocol.  The value will be overwritten
             * when usmUserCloneFrom is set (so don't write it if that has
             * already been set).  
             */
            if (snmp_oid_compare(uptr->authProtocol, uptr->authProtocolLen,
                                 usmNoAuthProtocol,
                                 sizeof(usmNoAuthProtocol) /
                                 sizeof(oid)) == 0) {
                if (snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     usmNoPrivProtocol,
                     sizeof(usmNoPrivProtocol) / sizeof(oid)) != 0) {
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            } else {
                if (snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     usmNoPrivProtocol,
                     sizeof(usmNoPrivProtocol) / sizeof(oid)) != 0
#ifndef NETSNMP_DISABLE_DES
                 && snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     usmDESPrivProtocol,
                     sizeof(usmDESPrivProtocol) / sizeof(oid)) != 0
#endif
                 && snmp_oid_compare
                    ((oid *) var_val, var_val_len / sizeof(oid),
                     usmAESPrivProtocol,
                     sizeof(usmAESPrivProtocol) / sizeof(oid)) != 0) {
                    return SNMP_ERR_WRONGVALUE;
                }
            }
            resetOnFail = 1;
            optr = uptr->privProtocol;
            olen = uptr->privProtocolLen;
            uptr->privProtocol = snmp_duplicate_objid((oid *) var_val,
                                                      var_val_len /
                                                      sizeof(oid));
            if (uptr->privProtocol == NULL) {
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            }
            uptr->privProtocolLen = var_val_len / sizeof(oid);
        }
    } else if (action == COMMIT) {
        SNMP_FREE(optr);
    } else if (action == FREE || action == UNDO) {
        if ((uptr = usm_parse_user(name, name_len)) != NULL) {
            if (resetOnFail) {
                SNMP_FREE(uptr->privProtocol);
                uptr->privProtocol = optr;
                uptr->privProtocolLen = olen;
            }
        }
    }

    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserPrivProtocol() */

/*
 * Note: This function handles both the usmUserPrivKeyChange and
 *       usmUserOwnPrivKeyChange objects.  We are not passed the name
 *       of the user requseting the keychange, so we leave this to the
 *       calling module to verify when and if we should be called.  To
 *       change this would require a change in the mib module API to
 *       pass in the securityName requesting the change.
 *
 */
int
write_usmUserPrivKeyChange(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    struct usmUser *uptr;
    unsigned char   buf[SNMP_MAXBUF_SMALL];
    size_t          buflen = SNMP_MAXBUF_SMALL;
    const char      fnPrivKey[] = "write_usmUserPrivKeyChange";
    const char      fnOwnPrivKey[] = "write_usmUserOwnPrivKeyChange";
    const char     *fname;
    static unsigned char *oldkey;
    static size_t   oldkeylen;
    static int      resetOnFail;

    if (name[USM_MIB_LENGTH - 1] == 9) {
        fname = fnPrivKey;
    } else {
        fname = fnOwnPrivKey;
    }

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("usmUser", "write to %s not ASN_OCTET_STR\n",
                        fname));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len == 0) {
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
#ifndef NETSNMP_DISABLE_DES
            if (snmp_oid_compare(uptr->privProtocol, uptr->privProtocolLen,
                                 usmDESPrivProtocol,
                                 sizeof(usmDESPrivProtocol) /
                                 sizeof(oid)) == 0) {
                if (var_val_len != 0 && var_val_len != 32) {
                    return SNMP_ERR_WRONGLENGTH;
                }
            }
#endif
            if (snmp_oid_compare(uptr->privProtocol, uptr->privProtocolLen,
                                 usmAESPrivProtocol,
                                 sizeof(usmAESPrivProtocol) /
                                 sizeof(oid)) == 0) {
                if (var_val_len != 0 && var_val_len != 32) {
                    return SNMP_ERR_WRONGLENGTH;
                }
            }
        }
    } else if (action == ACTION) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        if (uptr->cloneFrom == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        if (snmp_oid_compare(uptr->privProtocol, uptr->privProtocolLen,
                             usmNoPrivProtocol,
                             sizeof(usmNoPrivProtocol) / sizeof(oid)) ==
            0) {
            /*
             * "When the value of the corresponding usmUserPrivProtocol is
             * usmNoPrivProtocol, then a set is successful, but effectively
             * is a no-op."  
             */
            DEBUGMSGTL(("usmUser",
                        "%s: noPrivProtocol keyChange... success!\n",
                        fname));
            return SNMP_ERR_NOERROR;
        }

        /*
         * Change the key. 
         */
        DEBUGMSGTL(("usmUser", "%s: changing priv key for user %s\n",
                    fname, uptr->secName));

        if (decode_keychange(uptr->authProtocol, uptr->authProtocolLen,
                             uptr->privKey, uptr->privKeyLen,
                             var_val, var_val_len,
                             buf, &buflen) != SNMPERR_SUCCESS) {
            DEBUGMSGTL(("usmUser", "%s: ... failed\n", fname));
            return SNMP_ERR_GENERR;
        }
        DEBUGMSGTL(("usmUser", "%s: ... succeeded\n", fname));
        resetOnFail = 1;
        oldkey = uptr->privKey;
        oldkeylen = uptr->privKeyLen;
        memdup(&uptr->privKey, buf, buflen);
        if (uptr->privKey == NULL) {
            return SNMP_ERR_RESOURCEUNAVAILABLE;
        }
        uptr->privKeyLen = buflen;
    } else if (action == COMMIT) {
        SNMP_FREE(oldkey);
    } else if (action == UNDO) {
        if ((uptr = usm_parse_user(name, name_len)) != NULL && resetOnFail) {
            SNMP_FREE(uptr->privKey);
            uptr->privKey = oldkey;
            uptr->privKeyLen = oldkeylen;
        }
    }

    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserPrivKeyChange() */

int
write_usmUserPublic(int action,
                    u_char * var_val,
                    u_char var_val_type,
                    size_t var_val_len,
                    u_char * statP, oid * name, size_t name_len)
{
    struct usmUser *uptr = NULL;

    if (var_val_type != ASN_OCTET_STR) {
        DEBUGMSGTL(("usmUser",
                    "write to usmUserPublic not ASN_OCTET_STR\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > 32) {
        DEBUGMSGTL(("usmUser", "write to usmUserPublic: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        /*
         * don't allow creations here 
         */
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_NOSUCHNAME;
        }
        if (uptr->userPublicString)
            free(uptr->userPublicString);
        uptr->userPublicString = (u_char *) malloc(var_val_len);
        if (uptr->userPublicString == NULL) {
            return SNMP_ERR_GENERR;
        }
        memcpy(uptr->userPublicString, var_val, var_val_len);
        uptr->userPublicStringLen = var_val_len;
        DEBUGMSG(("usmUser", "setting public string: %d - ", (int)var_val_len));
        DEBUGMSGHEX(("usmUser", uptr->userPublicString, var_val_len));
        DEBUGMSG(("usmUser", "\n"));
    }
    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserPublic() */

int
write_usmUserStorageType(int action,
                         u_char * var_val,
                         u_char var_val_type,
                         size_t var_val_len,
                         u_char * statP, oid * name, size_t name_len)
{
    long            long_ret = *((long *) var_val);
    static long     oldValue;
    struct usmUser *uptr;
    static int      resetOnFail;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserStorageType not ASN_INTEGER\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long)) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserStorageType: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
        if (long_ret < 1 || long_ret > 5) {
            return SNMP_ERR_WRONGVALUE;
        }
    } else if (action == RESERVE2) {
        if ((uptr = usm_parse_user(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        }
        if ((long_ret == ST_VOLATILE || long_ret == ST_NONVOLATILE) &&
            (uptr->userStorageType == ST_VOLATILE ||
             uptr->userStorageType == ST_NONVOLATILE)) {
            oldValue = uptr->userStorageType;
            uptr->userStorageType = long_ret;
            resetOnFail = 1;
        } else {
            /*
             * From RFC2574:
             * 
             * "Note that any user who employs authentication or privacy must
             * allow its secret(s) to be updated and thus cannot be 'readOnly'.
             * 
             * If an initial set operation tries to set the value to 'readOnly'
             * for a user who employs authentication or privacy, then an
             * 'inconsistentValue' error must be returned.  Note that if the
             * value has been previously set (implicit or explicit) to any
             * value, then the rules as defined in the StorageType Textual
             * Convention apply.  
             */
            DEBUGMSGTL(("usmUser",
                        "long_ret %ld uptr->st %d uptr->status %d\n",
                        long_ret, uptr->userStorageType,
                        uptr->userStatus));

            if (long_ret == ST_READONLY &&
                uptr->userStorageType != ST_READONLY &&
                (uptr->userStatus == RS_ACTIVE ||
                 uptr->userStatus == RS_NOTINSERVICE)) {
                return SNMP_ERR_WRONGVALUE;
            } else if (long_ret == ST_READONLY &&
                       (snmp_oid_compare
                        (uptr->privProtocol, uptr->privProtocolLen,
                         usmNoPrivProtocol,
                         sizeof(usmNoPrivProtocol) / sizeof(oid)) != 0
                        || snmp_oid_compare(uptr->authProtocol,
                                            uptr->authProtocolLen,
                                            usmNoAuthProtocol,
                                            sizeof(usmNoAuthProtocol) /
                                            sizeof(oid)) != 0)) {
                return SNMP_ERR_INCONSISTENTVALUE;
            } else {
                return SNMP_ERR_WRONGVALUE;
            }
        }
    } else if (action == UNDO || action == FREE) {
        if ((uptr = usm_parse_user(name, name_len)) != NULL && resetOnFail) {
            uptr->userStorageType = oldValue;
        }
    }
    return SNMP_ERR_NOERROR;
}                               /* end write_usmUserStorageType() */


/*
 * Return 1 if enough objects have been set up to transition rowStatus to
 * notInService(2) or active(1).  
 */

int
usmStatusCheck(struct usmUser *uptr)
{
    if (uptr == NULL) {
        return 0;
    } else {
        if (uptr->cloneFrom == NULL) {
            return 0;
        }
    }
    return 1;
}

/*******************************************************************-o-******
 * write_usmUserStatus
 *
 * Parameters:
 *	 action
 *	*var_val
 *	 var_val_type
 *	 var_val_len
 *	*statP
 *	*name
 *	 name_len
 *      
 * Returns:
 *	SNMP_ERR_NOERROR		On success.
 *	SNMP_ERR_GENERR	
 *	SNMP_ERR_INCONSISTENTNAME
 *	SNMP_ERR_INCONSISTENTVALUE
 *	SNMP_ERR_WRONGLENGTH
 *	SNMP_ERR_WRONGTYPE
 */
int
write_usmUserStatus(int action,
                    u_char * var_val,
                    u_char var_val_type,
                    size_t var_val_len,
                    u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    unsigned char  *engineID;
    size_t          engineIDLen;
    char           *newName;
    size_t          nameLen;
    struct usmUser *uptr = NULL;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserStatus not ASN_INTEGER\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long_ret)) {
            DEBUGMSGTL(("usmUser",
                        "write to usmUserStatus: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
        long_ret = *((long *) var_val);
        if (long_ret == RS_NOTREADY || long_ret < 1 || long_ret > 6) {
            return SNMP_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for engineID/name first.  
         */
        if (usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                          &engineID, &engineIDLen, (u_char **) & newName,
                          &nameLen)) {
            DEBUGMSGTL(("usmUser",
                        "can't parse the OID for engineID or name\n"));
            return SNMP_ERR_INCONSISTENTNAME;
        }

        if (engineIDLen < 5 || engineIDLen > 32 || nameLen < 1
            || nameLen > 32) {
            SNMP_FREE(engineID);
            SNMP_FREE(newName);
            return SNMP_ERR_NOCREATION;
        }

        /*
         * Now see if a user already exists with these index values. 
         */
        uptr = usm_get_user(engineID, engineIDLen, newName);

        if (uptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                SNMP_FREE(engineID);
                SNMP_FREE(newName);
                long_ret = RS_NOTREADY;
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            SNMP_FREE(engineID);
            SNMP_FREE(newName);
        } else {
            if (long_ret == RS_ACTIVE || long_ret == RS_NOTINSERVICE) {
                SNMP_FREE(engineID);
                SNMP_FREE(newName);
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                if ((uptr = usm_create_user()) == NULL) {
                    SNMP_FREE(engineID);
                    SNMP_FREE(newName);
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->engineID = engineID;
                uptr->name = newName;
                uptr->secName = strdup(uptr->name);
                if (uptr->secName == NULL) {
                    usm_free_user(uptr);
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->engineIDLen = engineIDLen;

                /*
                 * Set status to createAndGo or createAndWait so we can tell
                 * that this row is under creation.  
                 */

                uptr->userStatus = long_ret;

                /*
                 * Add to the list of users (we will take it off again
                 * later if something goes wrong).  
                 */

                usm_add_user(uptr);
            } else {
                SNMP_FREE(engineID);
                SNMP_FREE(newName);
            }
        }
    } else if (action == ACTION) {
        usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                      &engineID, &engineIDLen, (u_char **) & newName,
                      &nameLen);
        uptr = usm_get_user(engineID, engineIDLen, newName);
        SNMP_FREE(engineID);
        SNMP_FREE(newName);

        if (uptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_ACTIVE) {
                if (usmStatusCheck(uptr)) {
                    uptr->userStatus = RS_ACTIVE;
                } else {
                    SNMP_FREE(engineID);
                    SNMP_FREE(newName);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            } else if (long_ret == RS_CREATEANDWAIT) {
                if (usmStatusCheck(uptr)) {
                    uptr->userStatus = RS_NOTINSERVICE;
                } else {
                    uptr->userStatus = RS_NOTREADY;
                }
            } else if (long_ret == RS_NOTINSERVICE) {
                if (uptr->userStatus == RS_ACTIVE ||
                    uptr->userStatus == RS_NOTINSERVICE) {
                    uptr->userStatus = RS_NOTINSERVICE;
                } else {
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            }
        }
    } else if (action == COMMIT) {
        usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                      &engineID, &engineIDLen, (u_char **) & newName,
                      &nameLen);
        uptr = usm_get_user(engineID, engineIDLen, newName);
        SNMP_FREE(engineID);
        SNMP_FREE(newName);

        if (uptr != NULL) {
            if (long_ret == RS_DESTROY) {
                usm_remove_user(uptr);
                usm_free_user(uptr);
            }
        }
    } else if (action == UNDO || action == FREE) {
        if (usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                      &engineID, &engineIDLen, (u_char **) & newName,
                      &nameLen)) {
            /* Can't extract engine info from the OID - nothing to undo */
            return SNMP_ERR_NOERROR;
        }
        uptr = usm_get_user(engineID, engineIDLen, newName);
        SNMP_FREE(engineID);
        SNMP_FREE(newName);

        if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
            usm_remove_user(uptr);
            usm_free_user(uptr);
        }
    }

    return SNMP_ERR_NOERROR;
}  /* write_usmUserStatus */

#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 

#if 0

    /*
     * see if we can parse the oid for engineID/name first 
     */
if (usm_parse_oid(&name[USM_MIB_LENGTH], name_len - USM_MIB_LENGTH,
                  &engineID, &engineIDLen, (u_char **) & newName,
                  &nameLen))
    return SNMP_ERR_INCONSISTENTNAME;

    /*
     * Now see if a user already exists with these index values 
     */
uptr = usm_get_user(engineID, engineIDLen, newName);


if (uptr) {                     /* If so, we set the appropriate value... */
    free(engineID);
    free(newName);
    if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
        return SNMP_ERR_INCONSISTENTVALUE;
    }
    if (long_ret == RS_DESTROY) {
        usm_remove_user(uptr);
        usm_free_user(uptr);
    } else {
        uptr->userStatus = long_ret;
    }

} else {                        /* ...else we create a new user */
    /*
     * check for a valid status column set 
     */
    if (long_ret == RS_ACTIVE || long_ret == RS_NOTINSERVICE) {
        free(engineID);
        free(newName);
        return SNMP_ERR_INCONSISTENTVALUE;
    }
    if (long_ret == RS_DESTROY) {
        /*
         * destroying a non-existent row is actually legal 
         */
        free(engineID);
        free(newName);
        return SNMP_ERR_NOERROR;
    }

    /*
     * generate a new user 
     */
    if ((uptr = usm_create_user()) == NULL) {
        free(engineID);
        free(newName);
        return SNMP_ERR_GENERR;
    }

    /*
     * copy in the engineID 
     */
    uptr->engineID = (unsigned char *) malloc(engineIDLen);
    if (uptr->engineID == NULL) {
        free(engineID);
        free(newName);
        usm_free_user(uptr);
        return SNMP_ERR_GENERR;
    }
    uptr->engineIDLen = engineIDLen;
    memcpy(uptr->engineID, engineID, engineIDLen);
    free(engineID);

    /*
     * copy in the name and secname 
     */
    if ((uptr->name = strdup(newName)) == NULL) {
        free(newName);
        usm_free_user(uptr);
        return SNMP_ERR_GENERR;
    }
    free(newName);
    if ((uptr->secName = strdup(uptr->name)) == NULL) {
        usm_free_user(uptr);
        return SNMP_ERR_GENERR;
    }

    /*
     * set the status of the row based on the request 
     */
    if (long_ret == RS_CREATEANDGO)
        uptr->userStatus = RS_ACTIVE;
    else if (long_ret == RS_CREATEANDWAIT)
        uptr->userStatus = RS_NOTINSERVICE;

    /*
     * finally, add it to our list of users 
     */
    usm_add_user(uptr);

}                               /* endif -- uptr */
}                               /* endif -- action==COMMIT */

return SNMP_ERR_NOERROR;

}                               /* end write_usmUserStatus() */
#endif
