/*
 * SNMPv3 View-based Access Control Model
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/agent_callbacks.h>
#include <net-snmp/agent/sysORTable.h>
#include "vacm_vars.h"
#include "util_funcs/header_generic.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

static unsigned int vacmViewSpinLock = 0;

void
init_vacm_vars(void)
{

    oid      reg[] = { SNMP_OID_SNMPMODULES, 16, 2, 2, 1 };

#define PRIVRW	(NETSNMP_SNMPV2ANY | 0x5000)

    struct variable1 vacm_sec2group[] = {
        {SECURITYGROUP, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_vacm_sec2group, 1, {3}},
        {SECURITYSTORAGE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_sec2group, 1, {4}},
        {SECURITYSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_sec2group, 1, {5}},
    };

    struct variable1 vacm_access[] = {
        {ACCESSMATCH, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {4}},
        {ACCESSREAD, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {5}},
        {ACCESSWRITE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {6}},
        {ACCESSNOTIFY, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {7}},
        {ACCESSSTORAGE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {8}},
        {ACCESSSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_access, 1, {9}},
    };

    struct variable3 vacm_view[] = {
        {VACMVIEWSPINLOCK, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_view, 1, {1}},
        {VIEWMASK, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_vacm_view, 3, {2, 1, 3}},
        {VIEWTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_view, 3, {2, 1, 4}},
        {VIEWSTORAGE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_view, 3, {2, 1, 5}},
        {VIEWSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_vacm_view, 3, {2, 1, 6}},
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             vacm_sec2group_oid[] = { OID_VACMGROUPENTRY };
    oid             vacm_access_oid[] = { OID_VACMACCESSENTRY };
    oid             vacm_view_oid[] = { OID_VACMMIBVIEWS };

    /*
     * we need to be called back later 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_vacm, NULL);

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("mibII/vacm:sec2group", vacm_sec2group, variable1,
                 vacm_sec2group_oid);
    REGISTER_MIB("mibII/vacm:access", vacm_access, variable1,
                 vacm_access_oid);
    REGISTER_MIB("mibII/vacm:view", vacm_view, variable3, vacm_view_oid);

    REGISTER_SYSOR_ENTRY(reg, "View-based Access Control Model for SNMP.");
}



u_char         *
var_vacm_sec2group(struct variable * vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
    struct vacm_groupEntry *gp;
    oid            *groupSubtree;
    ssize_t         groupSubtreeLen;
    int             secmodel;
    char            secname[VACMSTRINGLEN], *cp;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch (vp->magic) {
#ifndef NETSNMP_NO_WRITE_SUPPORT 
    case SECURITYGROUP:
        *write_method = write_vacmGroupName;
        break;
    case SECURITYSTORAGE:
        *write_method = write_vacmSecurityToGroupStorageType;
        break;
    case SECURITYSTATUS:
        *write_method = write_vacmSecurityToGroupStatus;
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    default:
        *write_method = NULL;
    }

	*var_len = 0; /* assume 0 length until found */

    if (memcmp(name, vp->name, sizeof(oid) * vp->namelen) != 0) {
        memcpy(name, vp->name, sizeof(oid) * vp->namelen);
        *length = vp->namelen;
    }
    if (exact) {
        if (*length < 13)
            return NULL;

        secmodel = name[11];
        groupSubtree = name + 13;
        groupSubtreeLen = *length - 13;
        if ( name[12] != groupSubtreeLen )
            return NULL;	/* Either extra subids, or an incomplete string */
        cp = secname;
        while (groupSubtreeLen-- > 0) {
            if (*groupSubtree > 255)
                return NULL;    /* illegal value */
            if (cp - secname > VACM_MAX_STRING)
                return NULL;
            *cp++ = (char) *groupSubtree++;
        }
        *cp = 0;

        gp = vacm_getGroupEntry(secmodel, secname);
    } else {
        secmodel = *length > 11 ? name[11] : 0;
        groupSubtree = name + 12;
        groupSubtreeLen = *length - 12;
        cp = secname;
        while (groupSubtreeLen-- > 0) {
            if (*groupSubtree > 255)
                return NULL;    /* illegal value */
            if (cp - secname > VACM_MAX_STRING)
                return NULL;
            *cp++ = (char) *groupSubtree++;
        }
        *cp = 0;
        vacm_scanGroupInit();
        while ((gp = vacm_scanGroupNext()) != NULL) {
            if (gp->securityModel > secmodel ||
                (gp->securityModel == secmodel
                 && strcmp(gp->securityName, secname) > 0))
                break;
        }
        if (gp) {
            name[11] = gp->securityModel;
            *length = 12;
            cp = gp->securityName;
            while (*cp) {
                name[(*length)++] = *cp++;
            }
        }
    }

    if (gp == NULL) {
        return NULL;
    }

    *var_len = sizeof(long_return);
    switch (vp->magic) {
    case SECURITYMODEL:
        long_return = gp->securityModel;
        return (u_char *) & long_return;

    case SECURITYNAME:
        *var_len = gp->securityName[0];
        return (u_char *) & gp->securityName[1];

    case SECURITYGROUP:
        *var_len = strlen(gp->groupName);
        return (u_char *) gp->groupName;

    case SECURITYSTORAGE:
        long_return = gp->storageType;
        return (u_char *) & long_return;

    case SECURITYSTATUS:
        long_return = gp->status;
        return (u_char *) & long_return;

    default:
        break;
    }

    return NULL;
}

u_char         *
var_vacm_access(struct variable * vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    struct vacm_accessEntry *gp;
    int             secmodel, seclevel;
    char            groupName[VACMSTRINGLEN] = { 0 };
    char            contextPrefix[VACMSTRINGLEN] = { 0 };
    oid            *op;
    unsigned long   len, i = 0;
    char           *cp;
    int             cmp;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch (vp->magic) {
#ifndef NETSNMP_NO_WRITE_SUPPORT 
    case ACCESSMATCH:
        *write_method = write_vacmAccessContextMatch;
        break;
    case ACCESSREAD:
        *write_method = write_vacmAccessReadViewName;
        break;
    case ACCESSWRITE:
        *write_method = write_vacmAccessWriteViewName;
        break;
    case ACCESSNOTIFY:
        *write_method = write_vacmAccessNotifyViewName;
        break;
    case ACCESSSTORAGE:
        *write_method = write_vacmAccessStorageType;
        break;
    case ACCESSSTATUS:
        *write_method = write_vacmAccessStatus;
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    default:
        *write_method = NULL;
    }

	*var_len = 0; /* assume 0 length until found */

    if (memcmp(name, vp->name, sizeof(oid) * vp->namelen) != 0) {
        memcpy(name, vp->name, sizeof(oid) * vp->namelen);
        *length = vp->namelen;
    }

    if (exact) {
        if (*length < 15)
            return NULL;

        /*
         * Extract the group name index from the requested OID ....
         */
        op = name + 11;
        len = *op++;
        if (len > VACM_MAX_STRING)
            return NULL;
        cp = groupName;
        while (len-- > 0) {
            if (*op > 255)
                return NULL;    /* illegal value */
            *cp++ = (char) *op++;
        }
        *cp = 0;

        /*
         * ... followed by the context index ...
         */
        len = *op++;
        if (len > VACM_MAX_STRING)
            return NULL;
        cp = contextPrefix;
        while (len-- > 0) {
            if (*op > 255)
                return NULL;    /* illegal value */
            *cp++ = (char) *op++;
        }
        *cp = 0;

        /*
         * ... and the secModel and secLevel index values.
         */
        secmodel = *op++;
        seclevel = *op++;
        if (op != name + *length) {
            return NULL;
        }

        gp = vacm_getAccessEntry(groupName, contextPrefix, secmodel,
                                 seclevel);
        if ( gp && gp->securityLevel != seclevel )
            return NULL;     /* This isn't strictly what was asked for */

    } else {
        secmodel = seclevel = 0;
        groupName[0] = 0;
        contextPrefix[0] = 0;
        op = name + 11;
        if (op >= name + *length) {
        } else {
            len = *op;
            if (len > VACM_MAX_STRING)
                return NULL;
            cp = groupName;
            for (i = 0; i <= len; i++) {
                if (*op > 255) {
                    return NULL;   /* illegal value */
                }
                *cp++ = (char) *op++;
            }
            *cp = 0;
        }
        if (op >= name + *length) {
        } else {
            len = *op;
            if (len > VACM_MAX_STRING)
                return NULL;
            cp = contextPrefix;
            for (i = 0; i <= len; i++) {
                if (*op > 255) {
                    return NULL;   /* illegal value */
                }
                *cp++ = (char) *op++;
            }
            *cp = 0;
        }
        if (op >= name + *length) {
        } else {
            secmodel = *op++;
        }
        if (op >= name + *length) {
        } else {
            seclevel = *op++;
        }
        vacm_scanAccessInit();
        while ((gp = vacm_scanAccessNext()) != NULL) {
            cmp = strcmp(gp->groupName, groupName);
            if (cmp > 0)
                break;
            if (cmp < 0)
                continue;
            cmp = strcmp(gp->contextPrefix, contextPrefix);
            if (cmp > 0)
                break;
            if (cmp < 0)
                continue;
            if (gp->securityModel > secmodel)
                break;
            if (gp->securityModel < secmodel)
                continue;
            if (gp->securityLevel > seclevel)
                break;
        }
        if (gp) {
            *length = 11;
            cp = gp->groupName;
            do {
                name[(*length)++] = *cp++;
            } while (*cp);
            cp = gp->contextPrefix;
            do {
                name[(*length)++] = *cp++;
            } while (*cp);
            name[(*length)++] = gp->securityModel;
            name[(*length)++] = gp->securityLevel;
        }
    }

    if (!gp) {
        return NULL;
    }

    *var_len = sizeof(long_return);
    switch (vp->magic) {
    case ACCESSMATCH:
        long_return = gp->contextMatch;
        return (u_char *) & long_return;

    case ACCESSLEVEL:
        long_return = gp->securityLevel;
        return (u_char *) & long_return;

    case ACCESSMODEL:
        long_return = gp->securityModel;
        return (u_char *) & long_return;

    case ACCESSPREFIX:
        *var_len = *gp->contextPrefix;
        return (u_char *) & gp->contextPrefix[1];

    case ACCESSREAD:
        *var_len = strlen(gp->views[VACM_VIEW_READ]);
        return (u_char *) gp->views[VACM_VIEW_READ];

    case ACCESSWRITE:
        *var_len = strlen(gp->views[VACM_VIEW_WRITE]);
        return (u_char *) gp->views[VACM_VIEW_WRITE];

    case ACCESSNOTIFY:
        *var_len = strlen(gp->views[VACM_VIEW_NOTIFY]);
        return (u_char *) gp->views[VACM_VIEW_NOTIFY];

    case ACCESSSTORAGE:
        long_return = gp->storageType;
        return (u_char *) & long_return;

    case ACCESSSTATUS:
        long_return = gp->status;
        return (u_char *) & long_return;
    }

    return NULL;
}

u_char         *
var_vacm_view(struct variable * vp,
              oid * name,
              size_t * length,
              int exact, size_t * var_len, WriteMethod ** write_method)
{
    struct vacm_viewEntry *gp = NULL;
    char            viewName[VACMSTRINGLEN] = { 0 };
    oid             subtree[MAX_OID_LEN] = { 0 };
    size_t          subtreeLen = 0;
    oid            *op, *op1;
    unsigned long   len = 0, i = 0;
    char           *cp;
    int             cmp, cmp2;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch (vp->magic) {
#ifndef NETSNMP_NO_WRITE_SUPPORT 
    case VIEWMASK:
        *write_method = write_vacmViewMask;
        break;
    case VIEWTYPE:
        *write_method = write_vacmViewType;
        break;
    case VIEWSTORAGE:
        *write_method = write_vacmViewStorageType;
        break;
    case VIEWSTATUS:
        *write_method = write_vacmViewStatus;
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    default:
        *write_method = NULL;
    }

    *var_len = sizeof(long_return);

    if (vp->magic != VACMVIEWSPINLOCK) {
        if (memcmp(name, vp->name, sizeof(oid) * vp->namelen) != 0) {
            memcpy(name, vp->name, sizeof(oid) * vp->namelen);
            *length = vp->namelen;
        }

        if (exact) {
            if (*length < 15)
                return NULL;

            /*
             * Extract the view name index from the requested OID ....
             */
            op = name + 12;
            len = *op++;
            if (len > VACM_MAX_STRING)
                return NULL;
            cp = viewName;
            while (len-- > 0) {
                if (*op > 255)
                    return NULL;
                *cp++ = (char) *op++;
            }
            *cp = 0;

            /*
             * ... followed by the view OID index.
             */
            subtree[0] = len = *op++;
            subtreeLen = 1;
            if (len > MAX_OID_LEN)
                return NULL;
            if ( (op+len) != (name + *length) )
                return NULL;     /* Declared length doesn't match what we actually got */
            op1 = &(subtree[1]);
            while (len-- > 0) {
                *op1++ = *op++;
                subtreeLen++;
            }

            gp = vacm_getViewEntry(viewName, &subtree[1], subtreeLen-1,
                                   VACM_MODE_IGNORE_MASK);
            if (gp != NULL) {
                if (gp->viewSubtreeLen != subtreeLen) {
                    gp = NULL;
                }
            }
        } else {
            viewName[0] = 0;
            op = name + 12;
            if (op >= name + *length) {
            } else {
                len = *op;
                if (len > VACM_MAX_STRING)
                    return NULL;
                cp = viewName;
                for (i = 0; i <= len && op < name + *length; i++) {
                    if (*op > 255) {
                        return NULL;
                    }
                    *cp++ = (char) *op++;
                }
                *cp = 0;
            }
            if (op >= name + *length) {
            } else {
                len = *op++;
                op1 = subtree;
                *op1++ = len;
                subtreeLen++;
                for (i = 0; i <= len && op < name + *length; i++) {
                    *op1++ = *op++;
                    subtreeLen++;
                }
            }
            vacm_scanViewInit();
            while ((gp = vacm_scanViewNext()) != NULL) {
                cmp = strcmp(gp->viewName, viewName);
                cmp2 =
                    snmp_oid_compare(gp->viewSubtree, gp->viewSubtreeLen,
                                     subtree, subtreeLen);
                if (cmp == 0 && cmp2 > 0)
                    break;
                if (cmp > 0)
                    break;
            }
            if (gp) {
                *length = 12;
                cp = gp->viewName;
                do {
                    name[(*length)++] = *cp++;
                } while (*cp);
                op1 = gp->viewSubtree;
                len = gp->viewSubtreeLen;
                while (len-- > 0) {
                    name[(*length)++] = *op1++;
                }
            }
        }

        if (gp == NULL) {
            return NULL;
        }
    } else {
        if (header_generic(vp, name, length, exact, var_len, write_method)) {
            return NULL;
        }
    }                           /*endif -- vp->magic != VACMVIEWSPINLOCK */

    switch (vp->magic) {
    case VACMVIEWSPINLOCK:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        *write_method = write_vacmViewSpinLock;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        long_return = vacmViewSpinLock;
        return (u_char *) & long_return;

    case VIEWNAME:
        *var_len = gp->viewName[0];
        return (u_char *) & gp->viewName[1];

    case VIEWSUBTREE:
        *var_len = gp->viewSubtreeLen * sizeof(oid);
        return (u_char *) gp->viewSubtree;

    case VIEWMASK:
        *var_len = gp->viewMaskLen;
        return (u_char *) gp->viewMask;

    case VIEWTYPE:
        long_return = gp->viewType;
        return (u_char *) & long_return;

    case VIEWSTORAGE:
        long_return = gp->viewStorageType;
        return (u_char *) & long_return;

    case VIEWSTATUS:
        long_return = gp->viewStatus;
        return (u_char *) & long_return;
    }

    return NULL;
}

#ifndef NETSNMP_NO_WRITE_SUPPORT 
int
sec2group_parse_oid(oid * oidIndex, size_t oidLen,
                    int *model, unsigned char **name, size_t * nameLen)
{
    int             nameL;
    int             i;

    /*
     * first check the validity of the oid 
     */
    if ((oidLen <= 0) || (!oidIndex)) {
        return 1;
    }
    nameL = oidIndex[1];        /* the initial name length */
    if ((int) oidLen != nameL + 2) {
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if (name == NULL) {
        return 1;
    }

    *name = (unsigned char *) malloc(nameL + 1);
    if (*name == NULL) {
        return 1;
    }

    *model = oidIndex[0];
    *nameLen = nameL;


    for (i = 0; i < nameL; i++) {
        if (oidIndex[i + 2] > 255) {
            free(*name);
            return 1;
        }
        name[0][i] = (unsigned char) oidIndex[i + 2];
    }
    name[0][nameL] = 0;

    return 0;

}

struct vacm_groupEntry *
sec2group_parse_groupEntry(oid * name, size_t name_len)
{
    struct vacm_groupEntry *geptr;

    char           *newName;
    int             model;
    size_t          nameLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if (sec2group_parse_oid
        (&name[SEC2GROUP_MIB_LENGTH], name_len - SEC2GROUP_MIB_LENGTH,
         &model, (u_char **) & newName, &nameLen))
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    geptr = vacm_getGroupEntry(model, newName);
    free(newName);

    return geptr;

}                               /* end vacm_parse_groupEntry() */

int
write_vacmGroupName(int action,
                    u_char * var_val,
                    u_char var_val_type,
                    size_t var_val_len,
                    u_char * statP, oid * name, size_t name_len)
{
    static unsigned char string[VACMSTRINGLEN];
    struct vacm_groupEntry *geptr;
    static int      resetOnFail;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len < 1 || var_val_len > 32) {
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((geptr = sec2group_parse_groupEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy(string, geptr->groupName, VACMSTRINGLEN);
            memcpy(geptr->groupName, var_val, var_val_len);
            geptr->groupName[var_val_len] = 0;
            if (geptr->status == RS_NOTREADY) {
                geptr->status = RS_NOTINSERVICE;
            }
        }
    } else if (action == FREE) {
        /*
         * Try to undo the SET here (abnormal usage of FREE clause)  
         */
        if ((geptr = sec2group_parse_groupEntry(name, name_len)) != NULL &&
            resetOnFail) {
            memcpy(geptr->groupName, string, VACMSTRINGLEN);
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmSecurityToGroupStorageType(int action,
                                     u_char * var_val,
                                     u_char var_val_type,
                                     size_t var_val_len,
                                     u_char * statP,
                                     oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    struct vacm_groupEntry *geptr;

    if (var_val_type != ASN_INTEGER) {
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        /*
         * don't allow creations here 
         */
        if ((geptr = sec2group_parse_groupEntry(name, name_len)) == NULL) {
            return SNMP_ERR_NOSUCHNAME;
        }
        long_ret = *((long *) var_val);
        if ((long_ret == ST_VOLATILE || long_ret == ST_NONVOLATILE) &&
            (geptr->storageType == ST_VOLATILE ||
             geptr->storageType == ST_NONVOLATILE)) {
            geptr->storageType = long_ret;
        } else if (long_ret == geptr->storageType) {
            return SNMP_ERR_NOERROR;
        } else {
            return SNMP_ERR_INCONSISTENTVALUE;
        }
    }
    return SNMP_ERR_NOERROR;
}


int
write_vacmSecurityToGroupStatus(int action,
                                u_char * var_val,
                                u_char var_val_type,
                                size_t var_val_len,
                                u_char * statP,
                                oid * name, size_t name_len)
{
    static long     long_ret;
    int             model;
    char           *newName;
    size_t          nameLen;
    struct vacm_groupEntry *geptr;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long_ret)) {
            return SNMP_ERR_WRONGLENGTH;
        }
        long_ret = *((long *) var_val);
        if (long_ret == RS_NOTREADY || long_ret < 1 || long_ret > 6) {
            return SNMP_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */

        if (sec2group_parse_oid(&name[SEC2GROUP_MIB_LENGTH],
                                name_len - SEC2GROUP_MIB_LENGTH,
                                &model, (u_char **) & newName, &nameLen)) {
            return SNMP_ERR_INCONSISTENTNAME;
        }

        if (model < 1 || nameLen < 1 || nameLen > 32) {
            free(newName);
            return SNMP_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        geptr = vacm_getGroupEntry(model, newName);

        if (geptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                free(newName);
                long_ret = RS_NOTREADY;
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_DESTROY && geptr->storageType == ST_PERMANENT) {
                free(newName);
                return SNMP_ERR_WRONGVALUE;
            }
        } else {
            if (long_ret == RS_ACTIVE || long_ret == RS_NOTINSERVICE) {
                free(newName);
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {

                /*
                 * Generate a new group entry.  
                 */
                if ((geptr =
                     vacm_createGroupEntry(model, newName)) == NULL) {
                    free(newName);
                    return SNMP_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                geptr->storageType = ST_NONVOLATILE;
                geptr->status = RS_NOTREADY;
            }
        }
        free(newName);
    } else if (action == ACTION) {
        sec2group_parse_oid(&name[SEC2GROUP_MIB_LENGTH],
                            name_len - SEC2GROUP_MIB_LENGTH,
                            &model, (u_char **) & newName, &nameLen);

        geptr = vacm_getGroupEntry(model, newName);

        if (geptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_ACTIVE) {
                /*
                 * Check that all the mandatory objects have been set by now,
                 * otherwise return inconsistentValue.  
                 */
                if (geptr->groupName[0] == 0) {
                    free(newName);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
                geptr->status = RS_ACTIVE;
            } else if (long_ret == RS_CREATEANDWAIT) {
                if (geptr->groupName[0] != 0) {
                    geptr->status = RS_NOTINSERVICE;
                }
            } else if (long_ret == RS_NOTINSERVICE) {
                if (geptr->status == RS_ACTIVE) {
                    geptr->status = RS_NOTINSERVICE;
                } else if (geptr->status == RS_NOTREADY) {
                    free(newName);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free(newName);
    } else if (action == COMMIT) {
        sec2group_parse_oid(&name[SEC2GROUP_MIB_LENGTH],
                            name_len - SEC2GROUP_MIB_LENGTH,
                            &model, (u_char **) & newName, &nameLen);

        geptr = vacm_getGroupEntry(model, newName);

        if (geptr != NULL) {
            if (long_ret == RS_DESTROY) {
                vacm_destroyGroupEntry(model, newName);
            }
        }
        free(newName);
    } else if (action == UNDO) {
        if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
            sec2group_parse_oid(&name[SEC2GROUP_MIB_LENGTH],
                                name_len - SEC2GROUP_MIB_LENGTH,
                                &model, (u_char **) & newName, &nameLen);

            geptr = vacm_getGroupEntry(model, newName);

            if (geptr != NULL) {
                vacm_destroyGroupEntry(model, newName);
            }
            free(newName);
        }
    }

    return SNMP_ERR_NOERROR;
}

int
access_parse_oid(oid * oidIndex, size_t oidLen,
                 unsigned char **groupName, size_t * groupNameLen,
                 unsigned char **contextPrefix, size_t * contextPrefixLen,
                 int *model, int *level)
{
    int             groupNameL, contextPrefixL;
    int             i;

    /*
     * first check the validity of the oid 
     */
    if ((oidLen <= 0) || (!oidIndex)) {
        return 1;
    }
    groupNameL = oidIndex[0];
    contextPrefixL = oidIndex[groupNameL + 1];  /* the initial name length */
    if ((int) oidLen != groupNameL + contextPrefixL + 4) {
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if (contextPrefix == NULL || groupName == NULL) {
        return 1;
    }

    *groupName = (unsigned char *) malloc(groupNameL + 1);
    if (*groupName == NULL) {
        return 1;
    }

    *contextPrefix = (unsigned char *) malloc(contextPrefixL + 1);
    if (*contextPrefix == NULL) {
        free(*groupName);
        return 1;
    }

    *contextPrefixLen = contextPrefixL;
    *groupNameLen = groupNameL;

    for (i = 0; i < groupNameL; i++) {
        if (oidIndex[i + 1] > 255) {
            free(*groupName);
            free(*contextPrefix);
            return 1;
        }
        groupName[0][i] = (unsigned char) oidIndex[i + 1];
    }
    groupName[0][groupNameL] = 0;


    for (i = 0; i < contextPrefixL; i++) {
        if (oidIndex[i + groupNameL + 2] > 255) {
            free(*groupName);
            free(*contextPrefix);
            return 1;
        }
        contextPrefix[0][i] = (unsigned char) oidIndex[i + groupNameL + 2];
    }
    contextPrefix[0][contextPrefixL] = 0;

    *model = oidIndex[groupNameL + contextPrefixL + 2];
    *level = oidIndex[groupNameL + contextPrefixL + 3];

    return 0;

}

struct vacm_accessEntry *
access_parse_accessEntry(oid * name, size_t name_len)
{
    struct vacm_accessEntry *aptr;

    char           *newGroupName = NULL;
    char           *newContextPrefix = NULL;
    int             model, level;
    size_t          groupNameLen, contextPrefixLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if (access_parse_oid
        (&name[ACCESS_MIB_LENGTH], name_len - ACCESS_MIB_LENGTH,
         (u_char **) & newGroupName, &groupNameLen,
         (u_char **) & newContextPrefix, &contextPrefixLen, &model,
         &level))
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    aptr =
        vacm_getAccessEntry(newGroupName, newContextPrefix, model, level);
    SNMP_FREE(newContextPrefix);
    SNMP_FREE(newGroupName);

    return aptr;

}                               /* end vacm_parse_accessEntry() */

int
write_vacmAccessStatus(int action,
                       u_char * var_val,
                       u_char var_val_type,
                       size_t var_val_len,
                       u_char * statP, oid * name, size_t name_len)
{
    static long     long_ret;
    int             model, level;
    char           *newGroupName = NULL, *newContextPrefix = NULL;
    size_t          groupNameLen, contextPrefixLen;
    struct vacm_accessEntry *aptr = NULL;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long_ret)) {
            return SNMP_ERR_WRONGLENGTH;
        }
        long_ret = *((long *) var_val);
        if (long_ret == RS_NOTREADY || long_ret < 1 || long_ret > 6) {
            return SNMP_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */
        if (access_parse_oid(&name[ACCESS_MIB_LENGTH],
                             name_len - ACCESS_MIB_LENGTH,
                             (u_char **) & newGroupName, &groupNameLen,
                             (u_char **) & newContextPrefix,
                             &contextPrefixLen, &model, &level)) {
            return SNMP_ERR_INCONSISTENTNAME;
        }

        if (model < 0 || groupNameLen < 1 || groupNameLen > 32 ||
            contextPrefixLen > 32) {
            free(newGroupName);
            free(newContextPrefix);
            return SNMP_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        aptr =
            vacm_getAccessEntry(newGroupName, newContextPrefix, model,
                                level);

        if (aptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                free(newGroupName);
                free(newContextPrefix);
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_DESTROY && aptr->storageType == ST_PERMANENT) {
                free(newGroupName);
                free(newContextPrefix);
                return SNMP_ERR_WRONGVALUE;
            }
        } else {
            if (long_ret == RS_ACTIVE || long_ret == RS_NOTINSERVICE) {
                free(newGroupName);
                free(newContextPrefix);
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                if ((aptr = vacm_createAccessEntry(newGroupName,
                                                   newContextPrefix,
                                                   model,
                                                   level)) == NULL) {
                    free(newGroupName);
                    free(newContextPrefix);
                    return SNMP_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                aptr->contextMatch = 1; /*  exact(1) is the DEFVAL  */
                aptr->storageType = ST_NONVOLATILE;
                aptr->status = RS_NOTREADY;
            }
        }
        free(newGroupName);
        free(newContextPrefix);
    } else if (action == ACTION) {
        access_parse_oid(&name[ACCESS_MIB_LENGTH],
                         name_len - ACCESS_MIB_LENGTH,
                         (u_char **) & newGroupName, &groupNameLen,
                         (u_char **) & newContextPrefix, &contextPrefixLen,
                         &model, &level);
        aptr =
            vacm_getAccessEntry(newGroupName, newContextPrefix, model,
                                level);

        if (aptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_ACTIVE) {
                aptr->status = RS_ACTIVE;
            } else if (long_ret == RS_CREATEANDWAIT) {
                aptr->status = RS_NOTINSERVICE;
            } else if (long_ret == RS_NOTINSERVICE) {
                if (aptr->status == RS_ACTIVE) {
                    aptr->status = RS_NOTINSERVICE;
                } else if (aptr->status == RS_NOTREADY) {
                    free(newGroupName);
                    free(newContextPrefix);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free(newGroupName);
        free(newContextPrefix);
    } else if (action == COMMIT) {
        access_parse_oid(&name[ACCESS_MIB_LENGTH],
                         name_len - ACCESS_MIB_LENGTH,
                         (u_char **) & newGroupName, &groupNameLen,
                         (u_char **) & newContextPrefix, &contextPrefixLen,
                         &model, &level);
        aptr =
            vacm_getAccessEntry(newGroupName, newContextPrefix, model,
                                level);

        if (aptr) {
            if (long_ret == RS_DESTROY) {
                vacm_destroyAccessEntry(newGroupName, newContextPrefix,
                                        model, level);
            }
        }
        free(newGroupName);
        free(newContextPrefix);
    } else if (action == UNDO) {
        if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
            access_parse_oid(&name[ACCESS_MIB_LENGTH],
                             name_len - ACCESS_MIB_LENGTH,
                             (u_char **) & newGroupName, &groupNameLen,
                             (u_char **) & newContextPrefix,
                             &contextPrefixLen, &model, &level);
            aptr =
                vacm_getAccessEntry(newGroupName, newContextPrefix, model,
                                    level);
            if (aptr != NULL) {
                vacm_destroyAccessEntry(newGroupName, newContextPrefix,
                                        model, level);
            }
        }
        free(newGroupName);
        free(newContextPrefix);
    }

    return SNMP_ERR_NOERROR;
}

int
write_vacmAccessStorageType(int action,
                            u_char * var_val,
                            u_char var_val_type,
                            size_t var_val_len,
                            u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    struct vacm_accessEntry *aptr;

    if (var_val_type != ASN_INTEGER) {
        DEBUGMSGTL(("mibII/vacm_vars",
                    "write to vacmSecurityToGroupStorageType not ASN_INTEGER\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        DEBUGMSGTL(("mibII/vacm_vars",
                    "write to vacmSecurityToGroupStorageType: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        /*
         * don't allow creations here 
         */
        if ((aptr = access_parse_accessEntry(name, name_len)) == NULL) {
            return SNMP_ERR_NOSUCHNAME;
        }
        long_ret = *((long *) var_val);
        /*
         * if ((long_ret == ST_VOLATILE || long_ret == ST_NONVOLATILE) &&
         * (aptr->storageType == ST_VOLATILE ||
         * aptr->storageType == ST_NONVOLATILE)) 
         */
        /*
         * This version only supports volatile storage
         */
        if (long_ret == aptr->storageType) {
            return SNMP_ERR_NOERROR;
        } else {
            return SNMP_ERR_INCONSISTENTVALUE;
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmAccessContextMatch(int action,
                             u_char * var_val,
                             u_char var_val_type,
                             size_t var_val_len,
                             u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    struct vacm_accessEntry *aptr;

    if (var_val_type != ASN_INTEGER) {
        DEBUGMSGTL(("mibII/vacm_vars",
                    "write to vacmAccessContextMatch not ASN_INTEGER\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        DEBUGMSGTL(("mibII/vacm_vars",
                    "write to vacmAccessContextMatch: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        /*
         * don't allow creations here 
         */
        if ((aptr = access_parse_accessEntry(name, name_len)) == NULL) {
            return SNMP_ERR_NOSUCHNAME;
        }
        long_ret = *((long *) var_val);
        if (long_ret == CM_EXACT || long_ret == CM_PREFIX) {
            aptr->contextMatch = long_ret;
        } else {
            return SNMP_ERR_WRONGVALUE;
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmAccessReadViewName(int action,
                             u_char * var_val,
                             u_char var_val_type,
                             size_t var_val_len,
                             u_char * statP, oid * name, size_t name_len)
{
    static unsigned char string[VACMSTRINGLEN];
    struct vacm_accessEntry *aptr = NULL;
    static int      resetOnFail;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessReadViewName not ASN_OCTET_STR\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > 32) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessReadViewName: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((aptr = access_parse_accessEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy(string, aptr->views[VACM_VIEW_READ], VACMSTRINGLEN);
            memcpy(aptr->views[VACM_VIEW_READ], var_val, var_val_len);
            aptr->views[VACM_VIEW_READ][var_val_len] = 0;
        }
    } else if (action == FREE) {
        /*
         * Try to undo the SET here (abnormal usage of FREE clause)  
         */
        if ((aptr = access_parse_accessEntry(name, name_len)) != NULL &&
            resetOnFail) {
            memcpy(aptr->views[VACM_VIEW_READ], string, var_val_len);
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmAccessWriteViewName(int action,
                              u_char * var_val,
                              u_char var_val_type,
                              size_t var_val_len,
                              u_char * statP, oid * name, size_t name_len)
{
    static unsigned char string[VACMSTRINGLEN];
    struct vacm_accessEntry *aptr = NULL;
    static int      resetOnFail;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessWriteViewName not ASN_OCTET_STR\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > 32) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessWriteViewName: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((aptr = access_parse_accessEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy(string, aptr->views[VACM_VIEW_WRITE], VACMSTRINGLEN);
            memcpy(aptr->views[VACM_VIEW_WRITE], var_val, var_val_len);
            aptr->views[VACM_VIEW_WRITE][var_val_len] = 0;
        }
    } else if (action == FREE) {
        /*
         * Try to undo the SET here (abnormal usage of FREE clause)  
         */
        if ((aptr = access_parse_accessEntry(name, name_len)) != NULL &&
            resetOnFail) {
            memcpy(aptr->views[VACM_VIEW_WRITE], string, var_val_len);
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmAccessNotifyViewName(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static unsigned char string[VACMSTRINGLEN];
    struct vacm_accessEntry *aptr = NULL;
    static int      resetOnFail;

    if (action == RESERVE1) {
        resetOnFail = 0;
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessNotifyViewName not ASN_OCTET_STR\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > 32) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmAccessNotifyViewName: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((aptr = access_parse_accessEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy(string, aptr->views[VACM_VIEW_NOTIFY], VACMSTRINGLEN);
            memcpy(aptr->views[VACM_VIEW_NOTIFY], var_val, var_val_len);
            aptr->views[VACM_VIEW_NOTIFY][var_val_len] = 0;
        }
    } else if (action == FREE) {
        /*
         * Try to undo the SET here (abnormal usage of FREE clause)  
         */
        if ((aptr = access_parse_accessEntry(name, name_len)) != NULL &&
            resetOnFail) {
            memcpy(aptr->views[VACM_VIEW_NOTIFY], string, var_val_len);
        }
    }
    return SNMP_ERR_NOERROR;
}

int
view_parse_oid(oid * oidIndex, size_t oidLen,
               unsigned char **viewName, size_t * viewNameLen,
               oid ** subtree, size_t * subtreeLen)
{
    int             viewNameL, subtreeL, i;

    /*
     * first check the validity of the oid 
     */
    if ((oidLen <= 0) || (!oidIndex)) {
        return SNMP_ERR_INCONSISTENTNAME;
    }
    viewNameL = oidIndex[0];
    subtreeL = oidLen - viewNameL - 1;  /* the initial name length */

    /*
     * its valid, malloc the space and store the results 
     */
    if (viewName == NULL || subtree == NULL) {
        return SNMP_ERR_RESOURCEUNAVAILABLE;
    }

    if (subtreeL < 0) {
        return SNMP_ERR_NOCREATION;
    }

    *viewName = (unsigned char *) malloc(viewNameL + 1);

    if (*viewName == NULL) {
        return SNMP_ERR_RESOURCEUNAVAILABLE;
    }

    *subtree = (oid *) malloc(subtreeL * sizeof(oid));
    if (*subtree == NULL) {
        free(*viewName);
        return SNMP_ERR_RESOURCEUNAVAILABLE;
    }

    *subtreeLen = subtreeL;
    *viewNameLen = viewNameL;

    for (i = 0; i < viewNameL; i++) {
        if (oidIndex[i + 1] > 255) {
            free(*viewName);
            free(*subtree);
            return SNMP_ERR_INCONSISTENTNAME;
        }
        viewName[0][i] = (unsigned char) oidIndex[i + 1];
    }
    viewName[0][viewNameL] = 0;

    for (i = 0; i < subtreeL; i++) {
        subtree[0][i] = (oid) oidIndex[i + viewNameL + 1];
    }

    return 0;
}

struct vacm_viewEntry *
view_parse_viewEntry(oid * name, size_t name_len)
{
    struct vacm_viewEntry *vptr;

    char           *newViewName;
    oid            *newViewSubtree;
    size_t          viewNameLen, viewSubtreeLen;

    if (view_parse_oid(&name[VIEW_MIB_LENGTH], name_len - VIEW_MIB_LENGTH,
                       (u_char **) & newViewName, &viewNameLen,
                       (oid **) & newViewSubtree, &viewSubtreeLen))
        return NULL;

    vptr =
        vacm_getViewEntry(newViewName, &newViewSubtree[1], viewSubtreeLen-1,
                          VACM_MODE_IGNORE_MASK);
    free(newViewName);
    free(newViewSubtree);

    return vptr;

}                               /* end vacm_parse_viewEntry() */

int
write_vacmViewStatus(int action,
                     u_char * var_val,
                     u_char var_val_type,
                     size_t var_val_len,
                     u_char * statP, oid * name, size_t name_len)
{
    static long     long_ret;
    char           *newViewName;
    oid            *newViewSubtree;
    size_t          viewNameLen, viewSubtreeLen;
    struct vacm_viewEntry *vptr;
    int             rc = 0;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long_ret)) {
            return SNMP_ERR_WRONGLENGTH;
        }
        long_ret = *((long *) var_val);
        if (long_ret == RS_NOTREADY || long_ret < 1 || long_ret > 6) {
            return SNMP_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */
        if ((rc =
             view_parse_oid(&name[VIEW_MIB_LENGTH],
                            name_len - VIEW_MIB_LENGTH,
                            (u_char **) & newViewName, &viewNameLen,
                            (oid **) & newViewSubtree, &viewSubtreeLen))) {
            return rc;
        }

        if (viewNameLen < 1 || viewNameLen > 32) {
            free(newViewName);
            free(newViewSubtree);
            return SNMP_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        vptr =
            vacm_getViewEntry(newViewName, &newViewSubtree[1], viewSubtreeLen-1,
                              VACM_MODE_IGNORE_MASK);
        if (vptr &&
            netsnmp_oid_equals(vptr->viewSubtree + 1, vptr->viewSubtreeLen - 1,
                               newViewSubtree + 1, viewSubtreeLen - 1) != 0) {
            vptr = NULL;
        }
        if (vptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
                free(newViewName);
                free(newViewSubtree);
                long_ret = RS_NOTREADY;
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_DESTROY && vptr->viewStorageType == ST_PERMANENT) {
                free(newViewName);
                free(newViewSubtree);
                return SNMP_ERR_WRONGVALUE;
            }
        } else {
            if (long_ret == RS_ACTIVE || long_ret == RS_NOTINSERVICE) {
                free(newViewName);
                free(newViewSubtree);
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {

                /*
                 * Generate a new group entry.  
                 */
                if ((vptr =
                     vacm_createViewEntry(newViewName, &newViewSubtree[1],
                                          viewSubtreeLen - 1)) == NULL) {
                    free(newViewName);
                    free(newViewSubtree);
                    return SNMP_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                vptr->viewStorageType = ST_NONVOLATILE;
                vptr->viewStatus = RS_NOTREADY;
                vptr->viewType = SNMP_VIEW_INCLUDED;
            }
        }
        free(newViewName);
        free(newViewSubtree);
    } else if (action == ACTION) {
        view_parse_oid(&name[VIEW_MIB_LENGTH], name_len - VIEW_MIB_LENGTH,
                       (u_char **) & newViewName, &viewNameLen,
                       (oid **) & newViewSubtree, &viewSubtreeLen);

        vptr =
            vacm_getViewEntry(newViewName, &newViewSubtree[1], viewSubtreeLen-1,
                              VACM_MODE_IGNORE_MASK);

        if (vptr != NULL) {
            if (long_ret == RS_CREATEANDGO || long_ret == RS_ACTIVE) {
                vptr->viewStatus = RS_ACTIVE;
            } else if (long_ret == RS_CREATEANDWAIT) {
                vptr->viewStatus = RS_NOTINSERVICE;
            } else if (long_ret == RS_NOTINSERVICE) {
                if (vptr->viewStatus == RS_ACTIVE) {
                    vptr->viewStatus = RS_NOTINSERVICE;
                } else if (vptr->viewStatus == RS_NOTREADY) {
                    free(newViewName);
                    free(newViewSubtree);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free(newViewName);
        free(newViewSubtree);
    } else if (action == COMMIT) {
        view_parse_oid(&name[VIEW_MIB_LENGTH], name_len - VIEW_MIB_LENGTH,
                       (u_char **) & newViewName, &viewNameLen,
                       (oid **) & newViewSubtree, &viewSubtreeLen);

        vptr =
            vacm_getViewEntry(newViewName, &newViewSubtree[1], viewSubtreeLen-1,
                              VACM_MODE_IGNORE_MASK);

        if (vptr != NULL) {
            if (long_ret == RS_DESTROY) {
                vacm_destroyViewEntry(newViewName, newViewSubtree,
                                      viewSubtreeLen);
            }
        }
        free(newViewName);
        free(newViewSubtree);
    } else if (action == UNDO) {
        if (long_ret == RS_CREATEANDGO || long_ret == RS_CREATEANDWAIT) {
            view_parse_oid(&name[VIEW_MIB_LENGTH],
                           name_len - VIEW_MIB_LENGTH,
                           (u_char **) & newViewName, &viewNameLen,
                           (oid **) & newViewSubtree, &viewSubtreeLen);

            vptr = vacm_getViewEntry(newViewName, &newViewSubtree[1],
                                     viewSubtreeLen-1, VACM_MODE_IGNORE_MASK);

            if (vptr != NULL) {
                vacm_destroyViewEntry(newViewName, newViewSubtree,
                                      viewSubtreeLen);
            }
            free(newViewName);
            free(newViewSubtree);
        }
    }

    return SNMP_ERR_NOERROR;
}

int
write_vacmViewStorageType(int action,
                          u_char * var_val,
                          u_char var_val_type,
                          size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len)
{
    long            newValue = *((long *) var_val);
    static long     oldValue;
    struct vacm_viewEntry *vptr = NULL;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long)) {
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((vptr = view_parse_viewEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            if ((newValue == ST_VOLATILE || newValue == ST_NONVOLATILE) &&
                (vptr->viewStorageType == ST_VOLATILE ||
                 vptr->viewStorageType == ST_NONVOLATILE)) {
                oldValue = vptr->viewStorageType;
                vptr->viewStorageType = newValue;
            } else if (newValue == vptr->viewStorageType) {
                return SNMP_ERR_NOERROR;
            } else {
                return SNMP_ERR_INCONSISTENTVALUE;
            }
        }
    } else if (action == UNDO) {
        if ((vptr = view_parse_viewEntry(name, name_len)) != NULL) {
            vptr->viewStorageType = oldValue;
        }
    }

    return SNMP_ERR_NOERROR;
}

int
write_vacmViewMask(int action,
                   u_char * var_val,
                   u_char var_val_type,
                   size_t var_val_len,
                   u_char * statP, oid * name, size_t name_len)
{
    static unsigned char string[VACMSTRINGLEN];
    static long     length;
    struct vacm_viewEntry *vptr = NULL;

    if (action == RESERVE1) {
        if (var_val_type != ASN_OCTET_STR) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > 16) {
            return SNMP_ERR_WRONGLENGTH;
        }
    } else if (action == RESERVE2) {
        if ((vptr = view_parse_viewEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            memcpy(string, vptr->viewMask, vptr->viewMaskLen);
            length = vptr->viewMaskLen;
            memcpy(vptr->viewMask, var_val, var_val_len);
            vptr->viewMaskLen = var_val_len;
        }
    } else if (action == FREE) {
        if ((vptr = view_parse_viewEntry(name, name_len)) != NULL) {
            memcpy(vptr->viewMask, string, length);
            vptr->viewMaskLen = length;
        }
    }
    return SNMP_ERR_NOERROR;
}

int
write_vacmViewType(int action,
                   u_char * var_val,
                   u_char var_val_type,
                   size_t var_val_len,
                   u_char * statP, oid * name, size_t name_len)
{
    long            newValue = *((long *) var_val);
    static long     oldValue;
    struct vacm_viewEntry *vptr = NULL;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long)) {
            return SNMP_ERR_WRONGLENGTH;
        }
        if (newValue < 1 || newValue > 2) {
            return SNMP_ERR_WRONGVALUE;
        }
    } else if (action == RESERVE2) {
        if ((vptr = view_parse_viewEntry(name, name_len)) == NULL) {
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            oldValue = vptr->viewType;
            vptr->viewType = newValue;
        }
    } else if (action == UNDO) {
        if ((vptr = view_parse_viewEntry(name, name_len)) != NULL) {
            vptr->viewType = oldValue;
        }
    }

    return SNMP_ERR_NOERROR;
}

int
write_vacmViewSpinLock(int action,
                       u_char * var_val,
                       u_char var_val_type,
                       size_t var_val_len,
                       u_char * statP, oid * name, size_t name_len)
{
    static long     long_ret;

    if (action == RESERVE1) {
        if (var_val_type != ASN_INTEGER) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmViewSpinLock not ASN_INTEGER\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != sizeof(long_ret)) {
            DEBUGMSGTL(("mibII/vacm_vars",
                        "write to vacmViewSpinLock: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }
        long_ret = *((long *) var_val);
        if (long_ret != (long) vacmViewSpinLock) {
            return SNMP_ERR_INCONSISTENTVALUE;
        }
    } else if (action == COMMIT) {
        if (vacmViewSpinLock == 2147483647) {
            vacmViewSpinLock = 0;
        } else {
            vacmViewSpinLock++;
        }
    }
    return SNMP_ERR_NOERROR;
}
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
