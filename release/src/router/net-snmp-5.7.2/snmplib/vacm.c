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

/*
 * vacm.c
 *
 * SNMPv3 View-based Access Control Model
 */

#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <stdio.h>
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

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/vacm.h>

static struct vacm_viewEntry *viewList = NULL, *viewScanPtr = NULL;
static struct vacm_accessEntry *accessList = NULL, *accessScanPtr = NULL;
static struct vacm_groupEntry *groupList = NULL, *groupScanPtr = NULL;

/*
 * Macro to extend view masks with 1 bits when shorter than subtree lengths
 * REF: vacmViewTreeFamilyMask [RFC3415], snmpNotifyFilterMask [RFC3413]
 */

#define VIEW_MASK(viewPtr, idx, mask) \
    ((idx >= viewPtr->viewMaskLen) ? mask : (viewPtr->viewMask[idx] & mask))

/**
 * Initilizes the VACM code.
 * Specifically:
 *  - adds a set of enums mapping view numbers to human readable names
 */
void
init_vacm(void)
{
    /* views for access via get/set/send-notifications */
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("read"),
                         VACM_VIEW_READ);
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("write"),
                         VACM_VIEW_WRITE);
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("notify"),
                         VACM_VIEW_NOTIFY);

    /* views for permissions when receiving notifications */
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("log"),
                         VACM_VIEW_LOG);
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("execute"),
                         VACM_VIEW_EXECUTE);
    se_add_pair_to_slist(VACM_VIEW_ENUM_NAME, strdup("net"),
                         VACM_VIEW_NET);
}

void
vacm_save(const char *token, const char *type)
{
    struct vacm_viewEntry *vptr;
    struct vacm_accessEntry *aptr;
    struct vacm_groupEntry *gptr;
    int i;

    for (vptr = viewList; vptr != NULL; vptr = vptr->next) {
        if (vptr->viewStorageType == ST_NONVOLATILE)
            vacm_save_view(vptr, token, type);
    }

    for (aptr = accessList; aptr != NULL; aptr = aptr->next) {
        if (aptr->storageType == ST_NONVOLATILE) {
            /* Store the standard views (if set) */
            if ( aptr->views[VACM_VIEW_READ  ][0] ||
                 aptr->views[VACM_VIEW_WRITE ][0] ||
                 aptr->views[VACM_VIEW_NOTIFY][0] )
                vacm_save_access(aptr, token, type);
            /* Store any other (valid) access views */
            for ( i=VACM_VIEW_NOTIFY+1; i<VACM_MAX_VIEWS; i++ ) {
                if ( aptr->views[i][0] )
                    vacm_save_auth_access(aptr, token, type, i);
            }
        }
    }

    for (gptr = groupList; gptr != NULL; gptr = gptr->next) {
        if (gptr->storageType == ST_NONVOLATILE)
            vacm_save_group(gptr, token, type);
    }
}

/*
 * vacm_save_view(): saves a view entry to the persistent cache 
 */
void
vacm_save_view(struct vacm_viewEntry *view, const char *token,
               const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d ", token, "View",
            view->viewStatus, view->viewStorageType, view->viewType);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */

    cptr =
        read_config_save_octet_string(cptr, (u_char *) view->viewName + 1,
                                      view->viewName[0]);
    *cptr++ = ' ';
    cptr =
        read_config_save_objid(cptr, view->viewSubtree+1,
                                     view->viewSubtreeLen-1);
    *cptr++ = ' ';
    cptr = read_config_save_octet_string(cptr, (u_char *) view->viewMask,
                                         view->viewMaskLen);

    read_config_store(type, line);
}

void
vacm_parse_config_view(const char *token, const char *line)
{
    struct vacm_viewEntry view;
    struct vacm_viewEntry *vptr;
    char           *viewName = (char *) &view.viewName;
    oid            *viewSubtree = (oid *) & view.viewSubtree;
    u_char         *viewMask;
    size_t          len;

    view.viewStatus = atoi(line);
    line = skip_token_const(line);
    view.viewStorageType = atoi(line);
    line = skip_token_const(line);
    view.viewType = atoi(line);
    line = skip_token_const(line);
    len = sizeof(view.viewName);
    line =
        read_config_read_octet_string(line, (u_char **) & viewName, &len);
    view.viewSubtreeLen = MAX_OID_LEN;
    line =
        read_config_read_objid_const(line, (oid **) & viewSubtree,
                               &view.viewSubtreeLen);

    vptr =
        vacm_createViewEntry(view.viewName, view.viewSubtree,
                             view.viewSubtreeLen);
    if (!vptr) {
        return;
    }

    vptr->viewStatus = view.viewStatus;
    vptr->viewStorageType = view.viewStorageType;
    vptr->viewType = view.viewType;
    viewMask = vptr->viewMask;
    vptr->viewMaskLen = sizeof(vptr->viewMask);
    line =
        read_config_read_octet_string(line, &viewMask, &vptr->viewMaskLen);
}

/*
 * vacm_save_access(): saves an access entry to the persistent cache 
 */
void
vacm_save_access(struct vacm_accessEntry *access_entry, const char *token,
                 const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d %d %d ",
            token, "Access", access_entry->status,
            access_entry->storageType, access_entry->securityModel,
            access_entry->securityLevel, access_entry->contextMatch);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */
    cptr =
        read_config_save_octet_string(cptr,
                                      (u_char *) access_entry->groupName + 1,
                                      access_entry->groupName[0] + 1);
    *cptr++ = ' ';
    cptr =
        read_config_save_octet_string(cptr,
                                      (u_char *) access_entry->contextPrefix + 1,
                                      access_entry->contextPrefix[0] + 1);

    *cptr++ = ' ';
    cptr = read_config_save_octet_string(cptr, (u_char *) access_entry->views[VACM_VIEW_READ],
                                         strlen(access_entry->views[VACM_VIEW_READ]) + 1);
    *cptr++ = ' ';
    cptr =
        read_config_save_octet_string(cptr, (u_char *) access_entry->views[VACM_VIEW_WRITE],
                                      strlen(access_entry->views[VACM_VIEW_WRITE]) + 1);
    *cptr++ = ' ';
    cptr =
        read_config_save_octet_string(cptr, (u_char *) access_entry->views[VACM_VIEW_NOTIFY],
                                      strlen(access_entry->views[VACM_VIEW_NOTIFY]) + 1);

    read_config_store(type, line);
}

void
vacm_save_auth_access(struct vacm_accessEntry *access_entry,
                      const char *token, const char *type, int authtype)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d %d %d ",
            token, "AuthAccess", access_entry->status,
            access_entry->storageType, access_entry->securityModel,
            access_entry->securityLevel, access_entry->contextMatch);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */
    cptr =
        read_config_save_octet_string(cptr,
                                      (u_char *) access_entry->groupName + 1,
                                      access_entry->groupName[0] + 1);
    *cptr++ = ' ';
    cptr =
        read_config_save_octet_string(cptr,
                                      (u_char *) access_entry->contextPrefix + 1,
                                      access_entry->contextPrefix[0] + 1);

    snprintf(cptr, sizeof(line)-(cptr-line), " %d ", authtype);
    while ( *cptr )
        cptr++;

    *cptr++ = ' ';
    cptr = read_config_save_octet_string(cptr,
                               (u_char *)access_entry->views[authtype],
                                  strlen(access_entry->views[authtype]) + 1);

    read_config_store(type, line);
}

char *
_vacm_parse_config_access_common(struct vacm_accessEntry **aptr,
                                 const char *line)
{
    struct vacm_accessEntry access;
    char           *cPrefix = (char *) &access.contextPrefix;
    char           *gName   = (char *) &access.groupName;
    size_t          len;

    access.status = atoi(line);
    line = skip_token_const(line);
    access.storageType = atoi(line);
    line = skip_token_const(line);
    access.securityModel = atoi(line);
    line = skip_token_const(line);
    access.securityLevel = atoi(line);
    line = skip_token_const(line);
    access.contextMatch = atoi(line);
    line = skip_token_const(line);
    len  = sizeof(access.groupName);
    line = read_config_read_octet_string(line, (u_char **) &gName,   &len);
    len  = sizeof(access.contextPrefix);
    line = read_config_read_octet_string(line, (u_char **) &cPrefix, &len);

    *aptr = vacm_getAccessEntry(access.groupName,
                                  access.contextPrefix,
                                  access.securityModel,
                                  access.securityLevel);
    if (!*aptr)
        *aptr = vacm_createAccessEntry(access.groupName,
                                  access.contextPrefix,
                                  access.securityModel,
                                  access.securityLevel);
    if (!*aptr)
        return NULL;

    (*aptr)->status = access.status;
    (*aptr)->storageType   = access.storageType;
    (*aptr)->securityModel = access.securityModel;
    (*aptr)->securityLevel = access.securityLevel;
    (*aptr)->contextMatch  = access.contextMatch;
    return NETSNMP_REMOVE_CONST(char *, line);
}

void
vacm_parse_config_access(const char *token, const char *line)
{
    struct vacm_accessEntry *aptr;
    char           *readView, *writeView, *notifyView;
    size_t          len;

    line = _vacm_parse_config_access_common(&aptr, line);
    if (!line)
        return;

    readView = (char *) aptr->views[VACM_VIEW_READ];
    len = sizeof(aptr->views[VACM_VIEW_READ]);
    line =
        read_config_read_octet_string(line, (u_char **) & readView, &len);
    writeView = (char *) aptr->views[VACM_VIEW_WRITE];
    len = sizeof(aptr->views[VACM_VIEW_WRITE]);
    line =
        read_config_read_octet_string(line, (u_char **) & writeView, &len);
    notifyView = (char *) aptr->views[VACM_VIEW_NOTIFY];
    len = sizeof(aptr->views[VACM_VIEW_NOTIFY]);
    line =
        read_config_read_octet_string(line, (u_char **) & notifyView,
                                      &len);
}

void
vacm_parse_config_auth_access(const char *token, const char *line)
{
    struct vacm_accessEntry *aptr;
    int             authtype;
    char           *view;
    size_t          len;

    line = _vacm_parse_config_access_common(&aptr, line);
    if (!line)
        return;

    authtype = atoi(line);
    line = skip_token_const(line);

    view = (char *) aptr->views[authtype];
    len  = sizeof(aptr->views[authtype]);
    line = read_config_read_octet_string(line, (u_char **) & view, &len);
}

/*
 * vacm_save_group(): saves a group entry to the persistent cache 
 */
void
vacm_save_group(struct vacm_groupEntry *group_entry, const char *token,
                const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d ",
            token, "Group", group_entry->status,
            group_entry->storageType, group_entry->securityModel);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */

    cptr =
        read_config_save_octet_string(cptr,
                                      (u_char *) group_entry->securityName + 1,
                                      group_entry->securityName[0] + 1);
    *cptr++ = ' ';
    cptr = read_config_save_octet_string(cptr, (u_char *) group_entry->groupName,
                                         strlen(group_entry->groupName) + 1);

    read_config_store(type, line);
}

void
vacm_parse_config_group(const char *token, const char *line)
{
    struct vacm_groupEntry group;
    struct vacm_groupEntry *gptr;
    char           *securityName = (char *) &group.securityName;
    char           *groupName;
    size_t          len;

    group.status = atoi(line);
    line = skip_token_const(line);
    group.storageType = atoi(line);
    line = skip_token_const(line);
    group.securityModel = atoi(line);
    line = skip_token_const(line);
    len = sizeof(group.securityName);
    line =
        read_config_read_octet_string(line, (u_char **) & securityName,
                                      &len);

    gptr = vacm_createGroupEntry(group.securityModel, group.securityName);
    if (!gptr)
        return;

    gptr->status = group.status;
    gptr->storageType = group.storageType;
    groupName = (char *) gptr->groupName;
    len = sizeof(group.groupName);
    line =
        read_config_read_octet_string(line, (u_char **) & groupName, &len);
}

struct vacm_viewEntry *
netsnmp_view_get(struct vacm_viewEntry *head, const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen, int mode)
{
    struct vacm_viewEntry *vp, *vpret = NULL;
    char            view[VACMSTRINGLEN];
    int             found, glen;
    int count=0;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    view[0] = glen;
    strcpy(view + 1, viewName);
    for (vp = head; vp; vp = vp->next) {
        if (!memcmp(view, vp->viewName, glen + 1)
            && viewSubtreeLen >= (vp->viewSubtreeLen - 1)) {
            int             mask = 0x80;
            unsigned int    oidpos, maskpos = 0;
            found = 1;

            for (oidpos = 0;
                 found && oidpos < vp->viewSubtreeLen - 1;
                 oidpos++) {
                if (mode==VACM_MODE_IGNORE_MASK || (VIEW_MASK(vp, maskpos, mask) != 0)) {
                    if (viewSubtree[oidpos] !=
                        vp->viewSubtree[oidpos + 1])
                        found = 0;
                }
                if (mask == 1) {
                    mask = 0x80;
                    maskpos++;
                } else
                    mask >>= 1;
            }

            if (found) {
                /*
                 * match successful, keep this node if its longer than
                 * the previous or (equal and lexicographically greater
                 * than the previous). 
                 */
                count++;
                if (mode == VACM_MODE_CHECK_SUBTREE) {
                    vpret = vp;
                } else if (vpret == NULL
                           || vp->viewSubtreeLen > vpret->viewSubtreeLen
                           || (vp->viewSubtreeLen == vpret->viewSubtreeLen
                               && snmp_oid_compare(vp->viewSubtree + 1,
                                                   vp->viewSubtreeLen - 1,
                                                   vpret->viewSubtree + 1,
                                                   vpret->viewSubtreeLen - 1) >
                               0)) {
                    vpret = vp;
                }
            }
        }
    }
    DEBUGMSGTL(("vacm:getView", ", %s\n", (vpret) ? "found" : "none"));
    if (mode == VACM_MODE_CHECK_SUBTREE && count > 1) {
        return NULL;
    }
    return vpret;
}

/*******************************************************************o-o******
 * vacm_checkSubtree
 *
 * Check to see if everything within a subtree is in view, not in view,
 * or possibly both.
 *
 * Parameters:
 *   *viewName           - Name of view to check
 *   *viewSubtree        - OID of subtree
 *    viewSubtreeLen     - length of subtree OID
 *      
 * Returns:
 *   VACM_SUCCESS          The OID is included in the view.
 *   VACM_NOTINVIEW        If no entry in the view list includes the
 *                         provided OID, or the OID is explicitly excluded
 *                         from the view. 
 *   VACM_SUBTREE_UNKNOWN  The entire subtree has both allowed and disallowed
 *                         portions.
 */
int
netsnmp_view_subtree_check(struct vacm_viewEntry *head, const char *viewName,
                           oid * viewSubtree, size_t viewSubtreeLen)
{
    struct vacm_viewEntry *vp, *vpShorter = NULL, *vpLonger = NULL;
    char            view[VACMSTRINGLEN];
    int             found, glen;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return VACM_NOTINVIEW;
    view[0] = glen;
    strcpy(view + 1, viewName);
    DEBUGMSGTL(("9:vacm:checkSubtree", "view %s\n", viewName));
    for (vp = head; vp; vp = vp->next) {
        if (!memcmp(view, vp->viewName, glen + 1)) {
            /*
             * If the subtree defined in the view is shorter than or equal
             * to the subtree we are comparing, then it might envelop the
             * subtree we are comparing against.
             */
            if (viewSubtreeLen >= (vp->viewSubtreeLen - 1)) {
                int             mask = 0x80;
                unsigned int    oidpos, maskpos = 0;
                found = 1;

                /*
                 * check the mask
                 */
                for (oidpos = 0;
                     found && oidpos < vp->viewSubtreeLen - 1;
                     oidpos++) {
                    if (VIEW_MASK(vp, maskpos, mask) != 0) {
                        if (viewSubtree[oidpos] !=
                            vp->viewSubtree[oidpos + 1])
                            found = 0;
                    }
                    if (mask == 1) {
                        mask = 0x80;
                        maskpos++;
                    } else
                        mask >>= 1;
                }

                if (found) {
                    /*
                     * match successful, keep this node if it's longer than
                     * the previous or (equal and lexicographically greater
                     * than the previous). 
                     */
                    DEBUGMSGTL(("9:vacm:checkSubtree", " %s matched?\n", vp->viewName));
    
                    if (vpShorter == NULL
                        || vp->viewSubtreeLen > vpShorter->viewSubtreeLen
                        || (vp->viewSubtreeLen == vpShorter->viewSubtreeLen
                           && snmp_oid_compare(vp->viewSubtree + 1,
                                               vp->viewSubtreeLen - 1,
                                               vpShorter->viewSubtree + 1,
                                               vpShorter->viewSubtreeLen - 1) >
                                   0)) {
                        vpShorter = vp;
                    }
                }
            }
            /*
             * If the subtree defined in the view is longer than the
             * subtree we are comparing, then it might ambiguate our
             * response.
             */
            else {
                int             mask = 0x80;
                unsigned int    oidpos, maskpos = 0;
                found = 1;

                /*
                 * check the mask up to the length of the provided subtree
                 */
                for (oidpos = 0;
                     found && oidpos < viewSubtreeLen;
                     oidpos++) {
                    if (VIEW_MASK(vp, maskpos, mask) != 0) {
                        if (viewSubtree[oidpos] !=
                            vp->viewSubtree[oidpos + 1])
                            found = 0;
                    }
                    if (mask == 1) {
                        mask = 0x80;
                        maskpos++;
                    } else
                        mask >>= 1;
                }

                if (found) {
                    /*
                     * match successful.  If we already found a match
                     * with a different view type, then parts of the subtree 
                     * are included and others are excluded, so return UNKNOWN.
                     */
                    DEBUGMSGTL(("9:vacm:checkSubtree", " %s matched?\n", vp->viewName));
                    if (vpLonger != NULL
                        && (vpLonger->viewType != vp->viewType)) {
                        DEBUGMSGTL(("vacm:checkSubtree", ", %s\n", "unknown"));
                        return VACM_SUBTREE_UNKNOWN;
                    }
                    else if (vpLonger == NULL) {
                        vpLonger = vp;
                    }
                }
            }
        }
    }
    DEBUGMSGTL(("9:vacm:checkSubtree", " %s matched\n", viewName));

    /*
     * If we found a matching view subtree with a longer OID than the provided
     * OID, check to see if its type is consistent with any matching view
     * subtree we may have found with a shorter OID than the provided OID.
     *
     * The view type of the longer OID is inconsistent with the shorter OID in
     * either of these two cases:
     *  1) No matching shorter OID was found and the view type of the longer
     *     OID is INCLUDE.
     *  2) A matching shorter ID was found and its view type doesn't match
     *     the view type of the longer OID.
     */
    if (vpLonger != NULL) {
        if ((!vpShorter && vpLonger->viewType != SNMP_VIEW_EXCLUDED)
            || (vpShorter && vpLonger->viewType != vpShorter->viewType)) {
            DEBUGMSGTL(("vacm:checkSubtree", ", %s\n", "unknown"));
            return VACM_SUBTREE_UNKNOWN;
        }
    }

    if (vpShorter && vpShorter->viewType != SNMP_VIEW_EXCLUDED) {
        DEBUGMSGTL(("vacm:checkSubtree", ", %s\n", "included"));
        return VACM_SUCCESS;
    }

    DEBUGMSGTL(("vacm:checkSubtree", ", %s\n", "excluded"));
    return VACM_NOTINVIEW;
}

void
vacm_scanViewInit(void)
{
    viewScanPtr = viewList;
}

struct vacm_viewEntry *
vacm_scanViewNext(void)
{
    struct vacm_viewEntry *returnval = viewScanPtr;
    if (viewScanPtr)
        viewScanPtr = viewScanPtr->next;
    return returnval;
}

struct vacm_viewEntry *
netsnmp_view_create(struct vacm_viewEntry **head, const char *viewName,
                     oid * viewSubtree, size_t viewSubtreeLen)
{
    struct vacm_viewEntry *vp, *lp, *op = NULL;
    int             cmp, cmp2, glen;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    vp = (struct vacm_viewEntry *) calloc(1,
                                          sizeof(struct vacm_viewEntry));
    if (vp == NULL)
        return NULL;
    vp->reserved =
        (struct vacm_viewEntry *) calloc(1, sizeof(struct vacm_viewEntry));
    if (vp->reserved == NULL) {
        free(vp);
        return NULL;
    }

    vp->viewName[0] = glen;
    strcpy(vp->viewName + 1, viewName);
    vp->viewSubtree[0] = viewSubtreeLen;
    memcpy(vp->viewSubtree + 1, viewSubtree, viewSubtreeLen * sizeof(oid));
    vp->viewSubtreeLen = viewSubtreeLen + 1;

    lp = *head;
    while (lp) {
        cmp = memcmp(lp->viewName, vp->viewName, glen + 1);
        cmp2 = snmp_oid_compare(lp->viewSubtree, lp->viewSubtreeLen,
                                vp->viewSubtree, vp->viewSubtreeLen);
        if (cmp == 0 && cmp2 > 0)
            break;
        if (cmp > 0)
            break;
        op = lp;
        lp = lp->next;
    }
    vp->next = lp;
    if (op)
        op->next = vp;
    else
        *head = vp;
    return vp;
}

void
netsnmp_view_destroy(struct vacm_viewEntry **head, const char *viewName,
                      oid * viewSubtree, size_t viewSubtreeLen)
{
    struct vacm_viewEntry *vp, *lastvp = NULL;

    if ((*head) && !strcmp((*head)->viewName + 1, viewName)
        && (*head)->viewSubtreeLen == viewSubtreeLen
        && !memcmp((char *) (*head)->viewSubtree, (char *) viewSubtree,
                   viewSubtreeLen * sizeof(oid))) {
        vp = (*head);
        (*head) = (*head)->next;
    } else {
        for (vp = (*head); vp; vp = vp->next) {
            if (!strcmp(vp->viewName + 1, viewName)
                && vp->viewSubtreeLen == viewSubtreeLen
                && !memcmp((char *) vp->viewSubtree, (char *) viewSubtree,
                           viewSubtreeLen * sizeof(oid)))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}

void
netsnmp_view_clear(struct vacm_viewEntry **head)
{
    struct vacm_viewEntry *vp;
    while ((vp = (*head))) {
        (*head) = vp->next;
        if (vp->reserved)
            free(vp->reserved);
        free(vp);
    }
}

struct vacm_groupEntry *
vacm_getGroupEntry(int securityModel, const char *securityName)
{
    struct vacm_groupEntry *vp;
    char            secname[VACMSTRINGLEN];
    int             glen;

    glen = (int) strlen(securityName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    secname[0] = glen;
    strcpy(secname + 1, securityName);

    for (vp = groupList; vp; vp = vp->next) {
        if ((securityModel == vp->securityModel
             || vp->securityModel == SNMP_SEC_MODEL_ANY)
            && !memcmp(vp->securityName, secname, glen + 1))
            return vp;
    }
    return NULL;
}

void
vacm_scanGroupInit(void)
{
    groupScanPtr = groupList;
}

struct vacm_groupEntry *
vacm_scanGroupNext(void)
{
    struct vacm_groupEntry *returnval = groupScanPtr;
    if (groupScanPtr)
        groupScanPtr = groupScanPtr->next;
    return returnval;
}

struct vacm_groupEntry *
vacm_createGroupEntry(int securityModel, const char *securityName)
{
    struct vacm_groupEntry *gp, *lg, *og;
    int             cmp, glen;

    glen = (int) strlen(securityName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    gp = (struct vacm_groupEntry *) calloc(1,
                                           sizeof(struct vacm_groupEntry));
    if (gp == NULL)
        return NULL;
    gp->reserved =
        (struct vacm_groupEntry *) calloc(1,
                                          sizeof(struct vacm_groupEntry));
    if (gp->reserved == NULL) {
        free(gp);
        return NULL;
    }

    gp->securityModel = securityModel;
    gp->securityName[0] = glen;
    strcpy(gp->securityName + 1, securityName);

    lg = groupList;
    og = NULL;
    while (lg) {
        if (lg->securityModel > securityModel)
            break;
        if (lg->securityModel == securityModel &&
            (cmp =
             memcmp(lg->securityName, gp->securityName, glen + 1)) > 0)
            break;
        /*
         * if (lg->securityModel == securityModel && cmp == 0) abort(); 
         */
        og = lg;
        lg = lg->next;
    }
    gp->next = lg;
    if (og == NULL)
        groupList = gp;
    else
        og->next = gp;
    return gp;
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
void
vacm_destroyGroupEntry(int securityModel, const char *securityName)
{
    struct vacm_groupEntry *vp, *lastvp = NULL;

    if (groupList && groupList->securityModel == securityModel
        && !strcmp(groupList->securityName + 1, securityName)) {
        vp = groupList;
        groupList = groupList->next;
    } else {
        for (vp = groupList; vp; vp = vp->next) {
            if (vp->securityModel == securityModel
                && !strcmp(vp->securityName + 1, securityName))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}
#endif /* NETSNMP_NO_WRITE_SUPPORT */

void
vacm_destroyAllGroupEntries(void)
{
    struct vacm_groupEntry *gp;
    while ((gp = groupList)) {
        groupList = gp->next;
        if (gp->reserved)
            free(gp->reserved);
        free(gp);
    }
}

struct vacm_accessEntry *
_vacm_choose_best( struct vacm_accessEntry *current,
                   struct vacm_accessEntry *candidate)
{
    /*
     * RFC 3415: vacmAccessTable:
     *    2) if this set has [more than] one member, ...
     *       it comes down to deciding how to weight the
     *       preferences between ContextPrefixes,
     *       SecurityModels, and SecurityLevels
     */
    if (( !current ) ||
        /* a) if the subset of entries with securityModel
         *    matching the securityModel in the message is
         *    not empty, then discard the rest
         */
        (  current->securityModel == SNMP_SEC_MODEL_ANY &&
         candidate->securityModel != SNMP_SEC_MODEL_ANY ) ||
        /* b) if the subset of entries with vacmAccessContextPrefix
         *    matching the contextName in the message is
         *    not empty, then discard the rest
         */
        (  current->contextMatch  == CONTEXT_MATCH_PREFIX &&
         candidate->contextMatch  == CONTEXT_MATCH_EXACT ) ||
        /* c) discard all entries with ContextPrefixes shorter
         *    than the longest one remaining in the set
         */
        (  current->contextMatch  == CONTEXT_MATCH_PREFIX &&
           current->contextPrefix[0] < candidate->contextPrefix[0] ) ||
        /* d) select the entry with the highest securityLevel
         */
        (  current->securityLevel < candidate->securityLevel )) {

        return candidate;
    }

    return current;
}

struct vacm_accessEntry *
vacm_getAccessEntry(const char *groupName,
                    const char *contextPrefix,
                    int securityModel, int securityLevel)
{
    struct vacm_accessEntry *vp, *best=NULL;
    char            group[VACMSTRINGLEN];
    char            context[VACMSTRINGLEN];
    int             glen, clen;

    glen = (int) strlen(groupName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    clen = (int) strlen(contextPrefix);
    if (clen < 0 || clen > VACM_MAX_STRING)
        return NULL;

    group[0] = glen;
    strcpy(group + 1, groupName);
    context[0] = clen;
    strcpy(context + 1, contextPrefix);
    for (vp = accessList; vp; vp = vp->next) {
        if ((securityModel == vp->securityModel
             || vp->securityModel == SNMP_SEC_MODEL_ANY)
            && securityLevel >= vp->securityLevel
            && !memcmp(vp->groupName, group, glen + 1)
            &&
            ((vp->contextMatch == CONTEXT_MATCH_EXACT
              && clen == vp->contextPrefix[0]
              && (memcmp(vp->contextPrefix, context, clen + 1) == 0))
             || (vp->contextMatch == CONTEXT_MATCH_PREFIX
                 && clen >= vp->contextPrefix[0]
                 && (memcmp(vp->contextPrefix + 1, context + 1,
                            vp->contextPrefix[0]) == 0))))
            best = _vacm_choose_best( best, vp );
    }
    return best;
}

void
vacm_scanAccessInit(void)
{
    accessScanPtr = accessList;
}

struct vacm_accessEntry *
vacm_scanAccessNext(void)
{
    struct vacm_accessEntry *returnval = accessScanPtr;
    if (accessScanPtr)
        accessScanPtr = accessScanPtr->next;
    return returnval;
}

struct vacm_accessEntry *
vacm_createAccessEntry(const char *groupName,
                       const char *contextPrefix,
                       int securityModel, int securityLevel)
{
    struct vacm_accessEntry *vp, *lp, *op = NULL;
    int             cmp, glen, clen;

    glen = (int) strlen(groupName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    clen = (int) strlen(contextPrefix);
    if (clen < 0 || clen > VACM_MAX_STRING)
        return NULL;
    vp = (struct vacm_accessEntry *) calloc(1,
                                            sizeof(struct
                                                   vacm_accessEntry));
    if (vp == NULL)
        return NULL;
    vp->reserved =
        (struct vacm_accessEntry *) calloc(1,
                                           sizeof(struct
                                                  vacm_accessEntry));
    if (vp->reserved == NULL) {
        free(vp);
        return NULL;
    }

    vp->securityModel = securityModel;
    vp->securityLevel = securityLevel;
    vp->groupName[0] = glen;
    strcpy(vp->groupName + 1, groupName);
    vp->contextPrefix[0] = clen;
    strcpy(vp->contextPrefix + 1, contextPrefix);

    lp = accessList;
    while (lp) {
        cmp = memcmp(lp->groupName, vp->groupName, glen + 1);
        if (cmp > 0)
            break;
        if (cmp < 0)
            goto next;
        cmp = memcmp(lp->contextPrefix, vp->contextPrefix, clen + 1);
        if (cmp > 0)
            break;
        if (cmp < 0)
            goto next;
        if (lp->securityModel > securityModel)
            break;
        if (lp->securityModel < securityModel)
            goto next;
        if (lp->securityLevel > securityLevel)
            break;
      next:
        op = lp;
        lp = lp->next;
    }
    vp->next = lp;
    if (op == NULL)
        accessList = vp;
    else
        op->next = vp;
    return vp;
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
void
vacm_destroyAccessEntry(const char *groupName,
                        const char *contextPrefix,
                        int securityModel, int securityLevel)
{
    struct vacm_accessEntry *vp, *lastvp = NULL;

    if (accessList && accessList->securityModel == securityModel
        && accessList->securityLevel == securityLevel
        && !strcmp(accessList->groupName + 1, groupName)
        && !strcmp(accessList->contextPrefix + 1, contextPrefix)) {
        vp = accessList;
        accessList = accessList->next;
    } else {
        for (vp = accessList; vp; vp = vp->next) {
            if (vp->securityModel == securityModel
                && vp->securityLevel == securityLevel
                && !strcmp(vp->groupName + 1, groupName)
                && !strcmp(vp->contextPrefix + 1, contextPrefix))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}
#endif /* NETSNMP_NO_WRITE_SUPPORT */

void
vacm_destroyAllAccessEntries(void)
{
    struct vacm_accessEntry *ap;
    while ((ap = accessList)) {
        accessList = ap->next;
        if (ap->reserved)
            free(ap->reserved);
        free(ap);
    }
}

int
store_vacm(int majorID, int minorID, void *serverarg, void *clientarg)
{
    /*
     * figure out our application name 
     */
    char           *appname = (char *) clientarg;
    if (appname == NULL) {
        appname = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					NETSNMP_DS_LIB_APPTYPE);
    }

    /*
     * save the VACM MIB 
     */
    vacm_save("vacm", appname);
    return SNMPERR_SUCCESS;
}

/*
 * returns 1 if vacm has *any* (non-built-in) configuration entries,
 * regardless of whether or not there is enough to make a decision,
 * else return 0 
 */
int
vacm_is_configured(void)
{
    if (accessList == NULL && groupList == NULL) {
        return 0;
    }
    return 1;
}

/*
 * backwards compatability
 */
struct vacm_viewEntry *
vacm_getViewEntry(const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen, int mode)
{
    return netsnmp_view_get( viewList, viewName, viewSubtree, viewSubtreeLen,
                             mode);
}

int
vacm_checkSubtree(const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen)
{
    return netsnmp_view_subtree_check( viewList, viewName, viewSubtree,
                                       viewSubtreeLen);
}

struct vacm_viewEntry *
vacm_createViewEntry(const char *viewName,
                     oid * viewSubtree, size_t viewSubtreeLen)
{
    return netsnmp_view_create( &viewList, viewName, viewSubtree,
                                viewSubtreeLen);
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
void
vacm_destroyViewEntry(const char *viewName,
                      oid * viewSubtree, size_t viewSubtreeLen)
{
    netsnmp_view_destroy( &viewList, viewName, viewSubtree, viewSubtreeLen);
}
#endif /* NETSNMP_NO_WRITE_SUPPORT */

void
vacm_destroyAllViewEntries(void)
{
    netsnmp_view_clear( &viewList );
}

