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
#include "vacm_conf.h"

#include "snmpd.h"

/**
 * Registers the VACM token handlers for inserting rows into the vacm tables.
 * These tokens will be recognised by both 'snmpd' and 'snmptrapd'.
 */
void
init_vacm_config_tokens(void) {
    snmpd_register_config_handler("group", vacm_parse_group,
                                  vacm_free_group,
                                  "name v1|v2c|usm|... security");
    snmpd_register_config_handler("access", vacm_parse_access,
                                  vacm_free_access,
                                  "name context model level prefix read write notify");
    snmpd_register_config_handler("setaccess", vacm_parse_setaccess,
                                  vacm_free_access,
                                  "name context model level prefix viewname viewval");
    snmpd_register_config_handler("view", vacm_parse_view, vacm_free_view,
                                  "name type subtree [mask]");
    snmpd_register_const_config_handler("vacmView",
                                        vacm_parse_config_view, NULL, NULL);
    snmpd_register_const_config_handler("vacmGroup",
                                        vacm_parse_config_group,
                                        NULL, NULL);
    snmpd_register_const_config_handler("vacmAccess",
                                        vacm_parse_config_access,
                                        NULL, NULL);
    snmpd_register_const_config_handler("vacmAuthAccess",
                                        vacm_parse_config_auth_access,
                                        NULL, NULL);

    /* easy community auth handler */
    snmpd_register_config_handler("authcommunity",
                                  vacm_parse_authcommunity,
                                  NULL, "authtype1,authtype2 community [default|hostname|network/bits [oid|-V view [context]]]");

    /* easy user auth handler */
    snmpd_register_config_handler("authuser",
                                  vacm_parse_authuser,
                                  NULL, "authtype1,authtype2 [-s secmodel] user [noauth|auth|priv [oid|-V view [context]]]");
    /* easy group auth handler */
    snmpd_register_config_handler("authgroup",
                                  vacm_parse_authuser,
                                  NULL, "authtype1,authtype2 [-s secmodel] group [noauth|auth|priv [oid|-V view [context]]]");

    snmpd_register_config_handler("authaccess", vacm_parse_authaccess,
                                  vacm_free_access,
                                  "name authtype1,authtype2 [-s secmodel] group view [noauth|auth|priv [context|context*]]");

    /*
     * Define standard views "_all_" and "_none_"
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_PRE_READ_CONFIG,
                           vacm_standard_views, NULL);
    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_POST_READ_CONFIG,
                           vacm_warn_if_not_configured, NULL);
}

/**
 * Registers the easier-to-use VACM token handlers for quick access rules.
 * These tokens will only be recognised by 'snmpd'.
 */
void
init_vacm_snmpd_easy_tokens(void) {
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    snmpd_register_config_handler("rwcommunity", vacm_parse_rwcommunity, NULL,
                                  "community [default|hostname|network/bits [oid|-V view [context]]]");
    snmpd_register_config_handler("rocommunity", vacm_parse_rocommunity, NULL,
                                  "community [default|hostname|network/bits [oid|-V view [context]]]");
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
    snmpd_register_config_handler("rwcommunity6", vacm_parse_rwcommunity6, NULL,
                                  "community [default|hostname|network/bits [oid|-V view [context]]]");
    snmpd_register_config_handler("rocommunity6", vacm_parse_rocommunity6, NULL,
                                  "community [default|hostname|network/bits [oid|-V view [context]]]");
#endif
#endif /* support for community based SNMP */
    snmpd_register_config_handler("rwuser", vacm_parse_rwuser, NULL,
                                  "user [noauth|auth|priv [oid|-V view [context]]]");
    snmpd_register_config_handler("rouser", vacm_parse_rouser, NULL,
                                  "user [noauth|auth|priv [oid|-V view [context]]]");
}

void
init_vacm_conf(void)
{
    init_vacm_config_tokens();
    init_vacm_snmpd_easy_tokens();
    /*
     * register ourselves to handle access control  ('snmpd' only)
     */
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_ACM_CHECK, vacm_in_view_callback,
                           NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_ACM_CHECK_INITIAL,
                           vacm_in_view_callback, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_ACM_CHECK_SUBTREE,
                           vacm_in_view_callback, NULL);
}



void
vacm_parse_group(const char *token, char *param)
{
    char            group[VACMSTRINGLEN], model[VACMSTRINGLEN], security[VACMSTRINGLEN];
    int             imodel;
    struct vacm_groupEntry *gp = NULL;
    char           *st;

    st = copy_nword(param, group, sizeof(group)-1);
    st = copy_nword(st, model, sizeof(model)-1);
    st = copy_nword(st, security, sizeof(security)-1);

    if (group[0] == 0) {
        config_perror("missing GROUP parameter");
        return;
    }
    if (model[0] == 0) {
        config_perror("missing MODEL parameter");
        return;
    }
    if (security[0] == 0) {
        config_perror("missing SECURITY parameter");
        return;
    }
    if (strcasecmp(model, "v1") == 0)
        imodel = SNMP_SEC_MODEL_SNMPv1;
    else if (strcasecmp(model, "v2c") == 0)
        imodel = SNMP_SEC_MODEL_SNMPv2c;
    else if (strcasecmp(model, "any") == 0) {
        config_perror
            ("bad security model \"any\" should be: v1, v2c, usm or a registered security plugin name - installing anyway");
        imodel = SNMP_SEC_MODEL_ANY;
    } else {
        if ((imodel = se_find_value_in_slist("snmp_secmods", model)) ==
            SE_DNE) {
            config_perror
                ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
            return;
        }
    }
    if (strlen(security) + 1 > sizeof(gp->groupName)) {
        config_perror("security name too long");
        return;
    }
    gp = vacm_createGroupEntry(imodel, security);
    if (!gp) {
        config_perror("failed to create group entry");
        return;
    }
    strlcpy(gp->groupName, group, sizeof(gp->groupName));
    gp->storageType = SNMP_STORAGE_PERMANENT;
    gp->status = SNMP_ROW_ACTIVE;
    free(gp->reserved);
    gp->reserved = NULL;
}

void
vacm_free_group(void)
{
    vacm_destroyAllGroupEntries();
}

#define PARSE_CONT 0
#define PARSE_FAIL 1

int
_vacm_parse_access_common(const char *token, char *param, char **st,
                          char **name, char **context, int *imodel,
                          int *ilevel, int *iprefix)
{
    char *model, *level, *prefix;

    *name = strtok_r(param, " \t\n", st);
    if (!*name) {
        config_perror("missing NAME parameter");
        return PARSE_FAIL;
    }
    *context = strtok_r(NULL, " \t\n", st);
    if (!*context) {
        config_perror("missing CONTEXT parameter");
        return PARSE_FAIL;
    }

    model = strtok_r(NULL, " \t\n", st);
    if (!model) {
        config_perror("missing MODEL parameter");
        return PARSE_FAIL;
    }
    level = strtok_r(NULL, " \t\n", st);
    if (!level) {
        config_perror("missing LEVEL parameter");
        return PARSE_FAIL;
    }
    prefix = strtok_r(NULL, " \t\n", st);
    if (!prefix) {
        config_perror("missing PREFIX parameter");
        return PARSE_FAIL;
    }

    if (strcmp(*context, "\"\"") == 0 || strcmp(*context, "\'\'") == 0)
        **context = 0;
    if (strcasecmp(model, "any") == 0)
        *imodel = SNMP_SEC_MODEL_ANY;
    else if (strcasecmp(model, "v1") == 0)
        *imodel = SNMP_SEC_MODEL_SNMPv1;
    else if (strcasecmp(model, "v2c") == 0)
        *imodel = SNMP_SEC_MODEL_SNMPv2c;
    else {
        if ((*imodel = se_find_value_in_slist("snmp_secmods", model))
            == SE_DNE) {
            config_perror
                ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
            return PARSE_FAIL;
        }
    }
    
    if (strcasecmp(level, "noauth") == 0)
        *ilevel = SNMP_SEC_LEVEL_NOAUTH;
    else if (strcasecmp(level, "noauthnopriv") == 0)
        *ilevel = SNMP_SEC_LEVEL_NOAUTH;
    else if (strcasecmp(level, "auth") == 0)
        *ilevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
    else if (strcasecmp(level, "authnopriv") == 0)
        *ilevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
    else if (strcasecmp(level, "priv") == 0)
        *ilevel = SNMP_SEC_LEVEL_AUTHPRIV;
    else if (strcasecmp(level, "authpriv") == 0)
        *ilevel = SNMP_SEC_LEVEL_AUTHPRIV;
    else {
        config_perror
            ("bad security level (noauthnopriv, authnopriv, authpriv)");
        return PARSE_FAIL;
    }

    if (strcmp(prefix, "exact") == 0)
        *iprefix = 1;
    else if (strcmp(prefix, "prefix") == 0)
        *iprefix = 2;
    else if (strcmp(prefix, "0") == 0) {
        config_perror
            ("bad prefix match parameter \"0\", should be: exact or prefix - installing anyway");
        *iprefix = 1;
    } else {
        config_perror
            ("bad prefix match parameter, should be: exact or prefix");
        return PARSE_FAIL;
    }

    return PARSE_CONT;
}

/* **************************************/
/* authorization parsing token handlers */
/* **************************************/

int
vacm_parse_authtokens(const char *token, char **confline)
{
    char authspec[SNMP_MAXBUF_MEDIUM];
    char *strtok_state;
    char *type;
    int viewtype, viewtypes = 0;

    *confline = copy_nword(*confline, authspec, sizeof(authspec));
    
    DEBUGMSGTL(("vacm_parse_authtokens","parsing %s",authspec));
    if (!*confline) {
        config_perror("Illegal configuration line: missing fields");
        return -1;
    }

    type = strtok_r(authspec, ",|:", &strtok_state);
    while(type && *type != '\0') {
        viewtype = se_find_value_in_slist(VACM_VIEW_ENUM_NAME, type);
        if (viewtype < 0 || viewtype >= VACM_MAX_VIEWS) {
            config_perror("Illegal view name");
        } else {
            viewtypes |= (1 << viewtype);
        }
        type = strtok_r(NULL, ",|:", &strtok_state);
    }
    DEBUGMSG(("vacm_parse_authtokens","  .. result = 0x%x\n",viewtypes));
    return viewtypes;
}

void
vacm_parse_authuser(const char *token, char *confline)
{
    int viewtypes = vacm_parse_authtokens(token, &confline);
    if (viewtypes != -1)
        vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_V3, viewtypes);
}

void
vacm_parse_authcommunity(const char *token, char *confline)
{
    int viewtypes = vacm_parse_authtokens(token, &confline);
    if (viewtypes != -1)
        vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_COM, viewtypes);
}

void
vacm_parse_authaccess(const char *token, char *confline)
{
    char *group, *view, *tmp;
    const char *context;
    int  model = SNMP_SEC_MODEL_ANY;
    int  level, prefix;
    int  i;
    char   *st;
    struct vacm_accessEntry *ap;
    int  viewtypes = vacm_parse_authtokens(token, &confline);

    if (viewtypes == -1)
        return;

    group = strtok_r(confline, " \t\n", &st);
    if (!group) {
        config_perror("missing GROUP parameter");
        return;
    }
    view = strtok_r(NULL, " \t\n", &st);
    if (!view) {
        config_perror("missing VIEW parameter");
        return;
    }

    /*
     * Check for security model option
     */
    if ( strcasecmp(view, "-s") == 0 ) {
        tmp = strtok_r(NULL, " \t\n", &st);
        if (tmp) {
            if (strcasecmp(tmp, "any") == 0)
                model = SNMP_SEC_MODEL_ANY;
            else if (strcasecmp(tmp, "v1") == 0)
                model = SNMP_SEC_MODEL_SNMPv1;
            else if (strcasecmp(tmp, "v2c") == 0)
                model = SNMP_SEC_MODEL_SNMPv2c;
            else {
                model = se_find_value_in_slist("snmp_secmods", tmp);
                if (model == SE_DNE) {
                    config_perror
                        ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
                    return;
                }
            }
        } else {
            config_perror("missing SECMODEL parameter");
            return;
        }
        view = strtok_r(NULL, " \t\n", &st);
        if (!view) {
            config_perror("missing VIEW parameter");
            return;
        }
    }
    if (strlen(view) >= VACMSTRINGLEN ) {
        config_perror("View value too long");
        return;
    }

    /*
     * Now parse optional fields, or provide default values
     */
    
    tmp = strtok_r(NULL, " \t\n", &st);
    if (tmp) {
        if (strcasecmp(tmp, "noauth") == 0)
            level = SNMP_SEC_LEVEL_NOAUTH;
        else if (strcasecmp(tmp, "noauthnopriv") == 0)
            level = SNMP_SEC_LEVEL_NOAUTH;
        else if (strcasecmp(tmp, "auth") == 0)
            level = SNMP_SEC_LEVEL_AUTHNOPRIV;
        else if (strcasecmp(tmp, "authnopriv") == 0)
            level = SNMP_SEC_LEVEL_AUTHNOPRIV;
        else if (strcasecmp(tmp, "priv") == 0)
            level = SNMP_SEC_LEVEL_AUTHPRIV;
        else if (strcasecmp(tmp, "authpriv") == 0)
            level = SNMP_SEC_LEVEL_AUTHPRIV;
        else {
            config_perror
                ("bad security level (noauthnopriv, authnopriv, authpriv)");
                return;
        }
    } else {
        /*  What about  SNMP_SEC_MODEL_ANY ?? */
        if ( model == SNMP_SEC_MODEL_SNMPv1 ||
             model == SNMP_SEC_MODEL_SNMPv2c )
            level = SNMP_SEC_LEVEL_NOAUTH;
        else
            level = SNMP_SEC_LEVEL_AUTHNOPRIV;
    }
    

    context = tmp = strtok_r(NULL, " \t\n", &st);
    if (tmp) {
        tmp = (tmp + strlen(tmp)-1);
        if (tmp && *tmp == '*') {
            *tmp = '\0';
            prefix = 2;
        } else {
            prefix = 1;
        }
    } else {
        context = "";
        prefix  = 1;   /* Or prefix(2) ?? */
    }

    /*
     * Now we can create the access entry
     */
    ap = vacm_getAccessEntry(group, context, model, level);
    if (!ap) {
        ap = vacm_createAccessEntry(group, context, model, level);
        DEBUGMSGTL(("vacm:conf:authaccess",
                    "no existing access found; creating a new one\n"));
    } else {
        DEBUGMSGTL(("vacm:conf:authaccess",
                    "existing access found, using it\n"));
    }
    if (!ap) {
        config_perror("failed to create access entry");
        return;
    }

    for (i = 0; i <= VACM_MAX_VIEWS; i++) {
        if (viewtypes & (1 << i)) {
            strcpy(ap->views[i], view);
        }
    }
    ap->contextMatch = prefix;
    ap->storageType  = SNMP_STORAGE_PERMANENT;
    ap->status       = SNMP_ROW_ACTIVE;
    if (ap->reserved)
        free(ap->reserved);
    ap->reserved = NULL;
}
 
void
vacm_parse_setaccess(const char *token, char *param)
{
    char *name, *context, *viewname, *viewval;
    int  imodel, ilevel, iprefix;
    int  viewnum;
    char   *st;
    struct vacm_accessEntry *ap;
 
    if (_vacm_parse_access_common(token, param, &st, &name,
                                  &context, &imodel, &ilevel, &iprefix)
        == PARSE_FAIL) {
        return;
    }

    viewname = strtok_r(NULL, " \t\n", &st);
    if (!viewname) {
        config_perror("missing viewname parameter");
        return;
    }
    viewval = strtok_r(NULL, " \t\n", &st);
    if (!viewval) {
        config_perror("missing viewval parameter");
        return;
    }

    if (strlen(viewval) + 1 > sizeof(ap->views[VACM_VIEW_NOTIFY])) {
        config_perror("View value too long");
        return;
    }

    viewnum = se_find_value_in_slist(VACM_VIEW_ENUM_NAME, viewname);
    if (viewnum < 0 || viewnum >= VACM_MAX_VIEWS) {
        config_perror("Illegal view name");
        return;
    }
        
    ap = vacm_getAccessEntry(name, context, imodel, ilevel);
    if (!ap) {
        ap = vacm_createAccessEntry(name, context, imodel, ilevel);
        DEBUGMSGTL(("vacm:conf:setaccess",
                    "no existing access found; creating a new one\n"));
    } else {
        DEBUGMSGTL(("vacm:conf:setaccess",
                    "existing access found, using it\n"));
    }
    if (!ap) {
        config_perror("failed to create access entry");
        return;
    }

    strcpy(ap->views[viewnum], viewval);
    ap->contextMatch = iprefix;
    ap->storageType = SNMP_STORAGE_PERMANENT;
    ap->status = SNMP_ROW_ACTIVE;
    free(ap->reserved);
    ap->reserved = NULL;
}

void
vacm_parse_access(const char *token, char *param)
{
    char           *name, *context, *readView, *writeView, *notify;
    int             imodel, ilevel, iprefix;
    struct vacm_accessEntry *ap;
    char   *st;

 
    if (_vacm_parse_access_common(token, param, &st, &name,
                                  &context, &imodel, &ilevel, &iprefix)
        == PARSE_FAIL) {
        return;
    }

    readView = strtok_r(NULL, " \t\n", &st);
    if (!readView) {
        config_perror("missing readView parameter");
        return;
    }
    writeView = strtok_r(NULL, " \t\n", &st);
    if (!writeView) {
        config_perror("missing writeView parameter");
        return;
    }
    notify = strtok_r(NULL, " \t\n", &st);
    if (!notify) {
        config_perror("missing notifyView parameter");
        return;
    }

    if (strlen(readView) + 1 > sizeof(ap->views[VACM_VIEW_READ])) {
        config_perror("readView too long");
        return;
    }
    if (strlen(writeView) + 1 > sizeof(ap->views[VACM_VIEW_WRITE])) {
        config_perror("writeView too long");
        return;
    }
    if (strlen(notify) + 1 > sizeof(ap->views[VACM_VIEW_NOTIFY])) {
        config_perror("notifyView too long");
        return;
    }
    ap = vacm_createAccessEntry(name, context, imodel, ilevel);
    if (!ap) {
        config_perror("failed to create access entry");
        return;
    }
    strcpy(ap->views[VACM_VIEW_READ], readView);
    strcpy(ap->views[VACM_VIEW_WRITE], writeView);
    strcpy(ap->views[VACM_VIEW_NOTIFY], notify);
    ap->contextMatch = iprefix;
    ap->storageType = SNMP_STORAGE_PERMANENT;
    ap->status = SNMP_ROW_ACTIVE;
    free(ap->reserved);
    ap->reserved = NULL;
}

void
vacm_free_access(void)
{
    vacm_destroyAllAccessEntries();
}

void
vacm_parse_view(const char *token, char *param)
{
    char           *name, *type, *subtree, *mask;
    int             inclexcl;
    struct vacm_viewEntry *vp;
    oid             suboid[MAX_OID_LEN];
    size_t          suboid_len = 0;
    size_t          mask_len = 0;
    u_char          viewMask[VACMSTRINGLEN];
    size_t          i;
    char            *st;

    name = strtok_r(param, " \t\n", &st);
    if (!name) {
        config_perror("missing NAME parameter");
        return;
    }
    type = strtok_r(NULL, " \n\t", &st);
    if (!type) {
        config_perror("missing TYPE parameter");
        return;
    }
    subtree = strtok_r(NULL, " \t\n", &st);
    if (!subtree) {
        config_perror("missing SUBTREE parameter");
        return;
    }
    mask = strtok_r(NULL, "\0", &st);

    if (strcmp(type, "included") == 0)
        inclexcl = SNMP_VIEW_INCLUDED;
    else if (strcmp(type, "excluded") == 0)
        inclexcl = SNMP_VIEW_EXCLUDED;
    else {
        config_perror("TYPE must be included/excluded?");
        return;
    }
    suboid_len = strlen(subtree)-1;
    if (subtree[suboid_len] == '.')
        subtree[suboid_len] = '\0';   /* stamp on a trailing . */
    suboid_len = MAX_OID_LEN;
    if (!snmp_parse_oid(subtree, suboid, &suboid_len)) {
        config_perror("bad SUBTREE object id");
        return;
    }
    if (mask) {
        unsigned int val;
        i = 0;
        for (mask = strtok_r(mask, " .:", &st); mask; mask = strtok_r(NULL, " .:", &st)) {
            if (i >= sizeof(viewMask)) {
                config_perror("MASK too long");
                return;
            }
            if (sscanf(mask, "%x", &val) == 0) {
                config_perror("invalid MASK");
                return;
            }
            viewMask[i] = val;
            i++;
        }
        mask_len = i;
    } else {
        for (i = 0; i < sizeof(viewMask); i++)
            viewMask[i] = 0xff;
    }
    vp = vacm_createViewEntry(name, suboid, suboid_len);
    if (!vp) {
        config_perror("failed to create view entry");
        return;
    }
    memcpy(vp->viewMask, viewMask, sizeof(viewMask));
    vp->viewMaskLen = mask_len;
    vp->viewType = inclexcl;
    vp->viewStorageType = SNMP_STORAGE_PERMANENT;
    vp->viewStatus = SNMP_ROW_ACTIVE;
    free(vp->reserved);
    vp->reserved = NULL;
}

void
vacm_free_view(void)
{
    vacm_destroyAllViewEntries();
}

void
vacm_gen_com2sec(int commcount, const char *community, const char *addressname,
                 const char *publishtoken,
                 void (*parser)(const char *, char *),
                 char *secname, size_t secname_len,
                 char *viewname, size_t viewname_len, int version,
                 const char *context)
{
    char            line[SPRINT_MAX_LEN];

    /*
     * com2sec6|comsec [-Cn CONTEXT] anonymousSecNameNUM    ADDRESS  COMMUNITY 
     */
    snprintf(secname, secname_len-1, "comm%d", commcount);
    secname[secname_len-1] = '\0';
    if (viewname) {
        snprintf(viewname, viewname_len-1, "viewComm%d", commcount);
        viewname[viewname_len-1] = '\0';
    }
    if ( context && *context )
       snprintf(line, sizeof(line), "-Cn %s %s %s '%s'",
             context, secname, addressname, community);
    else
       snprintf(line, sizeof(line), "%s %s '%s'",
             secname, addressname, community);
    line[ sizeof(line)-1 ] = 0;
    DEBUGMSGTL((publishtoken, "passing: %s %s\n", publishtoken, line));
    (*parser)(publishtoken, line);

    /*
     * sec->group mapping 
     */
    /*
     * group   anonymousGroupNameNUM  any      anonymousSecNameNUM 
     */
    if ( version & SNMP_SEC_MODEL_SNMPv1 ) {
        snprintf(line, sizeof(line),
             "grp%.28s v1 %s", secname, secname);
        line[ sizeof(line)-1 ] = 0;
        DEBUGMSGTL((publishtoken, "passing: %s %s\n", "group", line));
        vacm_parse_group("group", line);
    }

    if ( version & SNMP_SEC_MODEL_SNMPv2c ) {
        snprintf(line, sizeof(line),
             "grp%.28s v2c %s", secname, secname);
        line[ sizeof(line)-1 ] = 0;
        DEBUGMSGTL((publishtoken, "passing: %s %s\n", "group", line));
        vacm_parse_group("group", line);
    }
}

void
vacm_parse_rwuser(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_V3,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}

void
vacm_parse_rouser(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_V3,
                       VACM_VIEW_READ_BIT);
}

void
vacm_parse_rocommunity(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_COMIPV4,
                       VACM_VIEW_READ_BIT);
}

void
vacm_parse_rwcommunity(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_COMIPV4,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}

void
vacm_parse_rocommunity6(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_COMIPV6,
                       VACM_VIEW_READ_BIT);
}

void
vacm_parse_rwcommunity6(const char *token, char *confline)
{
    vacm_create_simple(token, confline, VACM_CREATE_SIMPLE_COMIPV6,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}


void
vacm_create_simple(const char *token, char *confline,
                   int parsetype, int viewtypes)
{
    char            line[SPRINT_MAX_LEN];
    char            community[COMMUNITY_MAX_LEN];
    char            theoid[SPRINT_MAX_LEN];
    char            viewname[SPRINT_MAX_LEN];
    char           *view_ptr = viewname;
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    char            addressname[SPRINT_MAX_LEN];
#endif
    const char     *rw = "none";
    char            model[SPRINT_MAX_LEN];
    char           *cp, *tmp;
    char            secname[SPRINT_MAX_LEN];
    char            grpname[SPRINT_MAX_LEN];
    char            authlevel[SPRINT_MAX_LEN];
    char            context[SPRINT_MAX_LEN];
    int             ctxprefix = 1;  /* Default to matching all contexts */
    static int      commcount = 0;
    /* Conveniently, the community-based security
       model values can also be used as bit flags */
    int             commversion = SNMP_SEC_MODEL_SNMPv1 |
                                  SNMP_SEC_MODEL_SNMPv2c;

    /*
     * init 
     */
    strcpy(model, "any");
    memset(context, 0, sizeof(context));
    memset(secname, 0, sizeof(secname));
    memset(grpname, 0, sizeof(grpname));

    /*
     * community name or user name 
     */
    cp = copy_nword(confline, community, sizeof(community));

    if (parsetype == VACM_CREATE_SIMPLE_V3) {
        /*
         * maybe security model type 
         */
        if (strcmp(community, "-s") == 0) {
            /*
             * -s model ... 
             */
            if (cp)
                cp = copy_nword(cp, model, sizeof(model));
            if (!cp) {
                config_perror("illegal line");
                return;
            }
            if (cp)
                cp = copy_nword(cp, community, sizeof(community));
        } else {
            strcpy(model, "usm");
        }
        /*
         * authentication level 
         */
        if (cp && *cp)
            cp = copy_nword(cp, authlevel, sizeof(authlevel));
        else
            strcpy(authlevel, "auth");
        DEBUGMSGTL((token, "setting auth level: \"%s\"\n", authlevel));
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    } else {
        if (strcmp(community, "-v") == 0) {
            /*
             * -v version ... 
             */
            if (cp)
                cp = copy_nword(cp, model, sizeof(model));
            if (!cp) {
                config_perror("illegal line");
                return;
            }
            if ( strcasecmp( model,  "1" ) == 0 )
                strcpy(model, "v1");
            if ( strcasecmp( model, "v1" ) == 0 )
                commversion = SNMP_SEC_MODEL_SNMPv1;
            if ( strcasecmp( model,  "2c" ) == 0 )
                strcpy(model, "v2c");
            if ( strcasecmp( model, "v2c" ) == 0 )
                commversion = SNMP_SEC_MODEL_SNMPv2c;
            if (cp)
                cp = copy_nword(cp, community, sizeof(community));
        }
        /*
         * source address 
         */
        if (cp && *cp) {
            cp = copy_nword(cp, addressname, sizeof(addressname));
        } else {
            strcpy(addressname, "default");
        }
        /*
         * authlevel has to be noauth 
         */
        strcpy(authlevel, "noauth");
#endif /* support for community based SNMP */
    }

    /*
     * oid they can touch 
     */
    if (cp && *cp) {
        if (strncmp(cp, "-V ", 3) == 0) {
             cp = skip_token(cp);
             cp = copy_nword(cp, viewname, sizeof(viewname));
             view_ptr = NULL;
        } else {
             cp = copy_nword(cp, theoid, sizeof(theoid));
        }
    } else {
        strcpy(theoid, ".1");
        strcpy(viewname, "_all_");
        view_ptr = NULL;
    }
    /*
     * optional, non-default context
     */
    if (cp && *cp) {
        cp = copy_nword(cp, context, sizeof(context));
        tmp = (context + strlen(context)-1);
        if (tmp && *tmp == '*') {
            *tmp = '\0';
            ctxprefix = 1;
        } else {
            /*
             * If no context field is given, then we default to matching
             *   all contexts (for compatability with previous releases).
             * But if a field context is specified (not ending with '*')
             *   then this should be taken as an exact match.
             * Specifying a context field of "" will match the default
             *   context (and *only* the default context).
             */
            ctxprefix = 0;
        }
    }

    if (viewtypes & VACM_VIEW_WRITE_BIT)
        rw = viewname;

    commcount++;

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#ifdef NETSNMP_TRANSPORT_UDP_DOMAIN
    if (parsetype == VACM_CREATE_SIMPLE_COMIPV4 ||
        parsetype == VACM_CREATE_SIMPLE_COM) {
        vacm_gen_com2sec(commcount, community, addressname,
                         "com2sec", &netsnmp_udp_parse_security,
                         secname, sizeof(secname),
                         view_ptr, sizeof(viewname), commversion, context);
    }
#endif

#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
    if (parsetype == VACM_CREATE_SIMPLE_COMUNIX ||
        parsetype == VACM_CREATE_SIMPLE_COM) {
        if ( *context )
           snprintf(line, sizeof(line), "-Cn %s %s %s '%s'",
             context, secname, addressname, community);
        else
            snprintf(line, sizeof(line), "%s %s '%s'",
                 secname, addressname, community);
        line[ sizeof(line)-1 ] = 0;
        DEBUGMSGTL((token, "passing: %s %s\n", "com2secunix", line));
        netsnmp_unix_parse_security("com2secunix", line);
    }
#endif

#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
    if (parsetype == VACM_CREATE_SIMPLE_COMIPV6 ||
        parsetype == VACM_CREATE_SIMPLE_COM) {
        vacm_gen_com2sec(commcount, community, addressname,
                         "com2sec6", &netsnmp_udp6_parse_security,
                         secname, sizeof(secname),
                         view_ptr, sizeof(viewname), commversion, context);
    }
#endif
#endif /* support for community based SNMP */

    if (parsetype == VACM_CREATE_SIMPLE_V3) {
        /* support for SNMPv3 user names */
        if (view_ptr) {
            sprintf(viewname,"viewUSM%d",commcount);
        }
        if ( strcmp( token, "authgroup" ) == 0 ) {
            strlcpy(grpname, community, sizeof(grpname));
        } else {
            strlcpy(secname, community, sizeof(secname));

            /*
             * sec->group mapping 
             */
            /*
             * group   anonymousGroupNameNUM  any      anonymousSecNameNUM 
             */
            snprintf(grpname, sizeof(grpname), "grp%.28s", secname);
            for (tmp=grpname; *tmp; tmp++)
                if (!isalnum((unsigned char)(*tmp)))
                    *tmp = '_';
            snprintf(line, sizeof(line),
                     "%s %s \"%s\"", grpname, model, secname);
            line[ sizeof(line)-1 ] = 0;
            DEBUGMSGTL((token, "passing: %s %s\n", "group", line));
            vacm_parse_group("group", line);
        }
    } else {
        snprintf(grpname, sizeof(grpname), "grp%.28s", secname);
        for (tmp=grpname; *tmp; tmp++)
            if (!isalnum((unsigned char)(*tmp)))
                *tmp = '_';
    }

    /*
     * view definition 
     */
    /*
     * view    anonymousViewNUM       included OID 
     */
    if (view_ptr) {
        snprintf(line, sizeof(line), "%s included %s", viewname, theoid);
        line[ sizeof(line)-1 ] = 0;
        DEBUGMSGTL((token, "passing: %s %s\n", "view", line));
        vacm_parse_view("view", line);
    }

    /*
     * map everything together 
     */
    if ((viewtypes == VACM_VIEW_READ_BIT) ||
        (viewtypes == (VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT))) {
        /* Use the simple line access command */
        /*
         * access  anonymousGroupNameNUM  "" MODEL AUTHTYPE prefix anonymousViewNUM [none/anonymousViewNUM] [none/anonymousViewNUM] 
         */
        snprintf(line, sizeof(line),
                 "%s %s %s %s %s %s %s %s",
                 grpname, context[0] ? context : "\"\"",
                 model, authlevel,
                (ctxprefix ? "prefix" : "exact"),
                 viewname, rw, rw);
        line[ sizeof(line)-1 ] = 0;
        DEBUGMSGTL((token, "passing: %s %s\n", "access", line));
        vacm_parse_access("access", line);
    } else {
        /* Use one setaccess line per access type */
        /*
         * setaccess  anonymousGroupNameNUM  "" MODEL AUTHTYPE prefix viewname viewval
         */
        int i;
        DEBUGMSGTL((token, " checking view levels for %x\n", viewtypes));
        for(i = 0; i <= VACM_MAX_VIEWS; i++) {
            if (viewtypes & (1 << i)) {
                snprintf(line, sizeof(line),
                         "%s %s %s %s %s %s %s",
                         grpname, context[0] ? context : "\"\"",
                         model, authlevel,
                        (ctxprefix ? "prefix" : "exact"),
                         se_find_label_in_slist(VACM_VIEW_ENUM_NAME, i),
                         viewname);
                line[ sizeof(line)-1 ] = 0;
                DEBUGMSGTL((token, "passing: %s %s\n", "setaccess", line));
                vacm_parse_setaccess("setaccess", line);
            }
        }
    }
}

int
vacm_standard_views(int majorID, int minorID, void *serverarg,
                            void *clientarg)
{
    char            line[SPRINT_MAX_LEN];

    memset(line, 0, sizeof(line));

    snprintf(line, sizeof(line), "_all_ included .0");
    vacm_parse_view("view", line);
    snprintf(line, sizeof(line), "_all_ included .1");
    vacm_parse_view("view", line);
    snprintf(line, sizeof(line), "_all_ included .2");
    vacm_parse_view("view", line);

    snprintf(line, sizeof(line), "_none_ excluded .0");
    vacm_parse_view("view", line);
    snprintf(line, sizeof(line), "_none_ excluded .1");
    vacm_parse_view("view", line);
    snprintf(line, sizeof(line), "_none_ excluded .2");
    vacm_parse_view("view", line);

    return SNMP_ERR_NOERROR;
}

int
vacm_warn_if_not_configured(int majorID, int minorID, void *serverarg,
                            void *clientarg)
{
    const char * name = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                        NETSNMP_DS_LIB_APPTYPE);
    const int agent_mode =  netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                                   NETSNMP_DS_AGENT_ROLE);
    if (NULL==name)
        name = "snmpd";
    
    if (!vacm_is_configured()) {
        /*
         *  An AgentX subagent relies on the master agent to apply suitable
         *    access control checks, so doesn't need local VACM configuration.
         *  The trap daemon has a separate check (see below).
         *
         *  Otherwise, an AgentX master or SNMP standalone agent requires some
         *    form of VACM configuration.  No config means that no incoming
         *    requests will be accepted, so warn the user accordingly.
         */
        if ((MASTER_AGENT == agent_mode) && (strcmp(name, "snmptrapd") != 0)) {
            snmp_log(LOG_WARNING,
                 "Warning: no access control information configured.\n"
                 "  (Config search path: %s)\n"
                 "  It's unlikely this agent can serve any useful purpose in this state.\n"
                 "  Run \"snmpconf -g basic_setup\" to help you "
                 "configure the %s.conf file for this agent.\n",
                 get_configuration_directory(), name);
        }

        /*
         *  The trap daemon implements VACM-style access control for incoming
         *    notifications, but offers a way of turning this off (for backwards
         *    compatability).  Check for this explicitly, and warn if necessary.
         *
         *  NB:  The NETSNMP_DS_APP_NO_AUTHORIZATION definition is a duplicate
         *       of an identical setting in "apps/snmptrapd_ds.h".
         *       These two need to be kept in synch.
         */
#ifndef NETSNMP_DS_APP_NO_AUTHORIZATION
#define NETSNMP_DS_APP_NO_AUTHORIZATION 17
#endif
        if (!strcmp(name, "snmptrapd") &&
            !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                    NETSNMP_DS_APP_NO_AUTHORIZATION)) {
            snmp_log(LOG_WARNING,
                 "Warning: no access control information configured.\n"
                 "  (Config search path: %s)\n"
                 "This receiver will *NOT* accept any incoming notifications.\n",
                 get_configuration_directory());
        }
    }
    return SNMP_ERR_NOERROR;
}

int
vacm_in_view_callback(int majorID, int minorID, void *serverarg,
                      void *clientarg)
{
    struct view_parameters *view_parms =
        (struct view_parameters *) serverarg;
    int             retval;

    if (view_parms == NULL)
        return 1;
    retval = vacm_in_view(view_parms->pdu, view_parms->name,
                          view_parms->namelen, view_parms->check_subtree);
    if (retval != 0)
        view_parms->errorcode = retval;
    return retval;
}


/**
 * vacm_in_view: decides if a given PDU can be acted upon
 *
 * Parameters:
 *	*pdu
 *	*name
 *	 namelen
 *       check_subtree
 *      
 * Returns:
 * VACM_SUCCESS(0)	   On success.
 * VACM_NOSECNAME(1)	   Missing security name.
 * VACM_NOGROUP(2)	   Missing group
 * VACM_NOACCESS(3)	   Missing access
 * VACM_NOVIEW(4)	   Missing view
 * VACM_NOTINVIEW(5)	   Not in view
 * VACM_NOSUCHCONTEXT(6)   No Such Context
 * VACM_SUBTREE_UNKNOWN(7) When testing an entire subtree, UNKNOWN (ie, the entire
 *                         subtree has both allowed and disallowed portions)
 *
 * Debug output listed as follows:
 *	\<securityName\> \<groupName\> \<viewName\> \<viewType\>
 */
int
vacm_in_view(netsnmp_pdu *pdu, oid * name, size_t namelen,
             int check_subtree)
{
    int viewtype;

    switch (pdu->command) {
    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
    case SNMP_MSG_GETBULK:
        viewtype = VACM_VIEW_READ;
        break;
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
        viewtype = VACM_VIEW_WRITE;
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    case SNMP_MSG_TRAP:
    case SNMP_MSG_TRAP2:
    case SNMP_MSG_INFORM:
        viewtype = VACM_VIEW_NOTIFY;
        break;
    default:
        snmp_log(LOG_ERR, "bad msg type in vacm_in_view: %d\n",
                 pdu->command);
        viewtype = VACM_VIEW_READ;
    }
    return vacm_check_view(pdu, name, namelen, check_subtree, viewtype);
}

/**
 * vacm_check_view: decides if a given PDU can be taken based on a view type
 *
 * Parameters:
 *	*pdu
 *	*name
 *	 namelen
 *       check_subtree
 *       viewtype
 *      
 * Returns:
 * VACM_SUCCESS(0)	   On success.
 * VACM_NOSECNAME(1)	   Missing security name.
 * VACM_NOGROUP(2)	   Missing group
 * VACM_NOACCESS(3)	   Missing access
 * VACM_NOVIEW(4)	   Missing view
 * VACM_NOTINVIEW(5)	   Not in view
 * VACM_NOSUCHCONTEXT(6)   No Such Context
 * VACM_SUBTREE_UNKNOWN(7) When testing an entire subtree, UNKNOWN (ie, the entire
 *                         subtree has both allowed and disallowed portions)
 *
 * Debug output listed as follows:
 *	\<securityName\> \<groupName\> \<viewName\> \<viewType\>
 */
int
vacm_check_view(netsnmp_pdu *pdu, oid * name, size_t namelen,
                int check_subtree, int viewtype)
{
    return vacm_check_view_contents(pdu, name, namelen, check_subtree, viewtype,
                                    VACM_CHECK_VIEW_CONTENTS_NO_FLAGS);
}

int
vacm_check_view_contents(netsnmp_pdu *pdu, oid * name, size_t namelen,
                         int check_subtree, int viewtype, int flags)
{
    struct vacm_accessEntry *ap;
    struct vacm_groupEntry *gp;
    struct vacm_viewEntry *vp;
    char            vacm_default_context[1] = "";
    const char     *contextName = vacm_default_context;
    const char     *sn = NULL;
    char           *vn;
    const char     *pdu_community;

    /*
     * len defined by the vacmContextName object 
     */
#define CONTEXTNAMEINDEXLEN 32
    char            contextNameIndex[CONTEXTNAMEINDEXLEN + 1];

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#if defined(NETSNMP_DISABLE_SNMPV1)
    if (pdu->version == SNMP_VERSION_2c)
#else
#if defined(NETSNMP_DISABLE_SNMPV2C)
    if (pdu->version == SNMP_VERSION_1)
#else
    if (pdu->version == SNMP_VERSION_1 || pdu->version == SNMP_VERSION_2c)
#endif
#endif
    {
        pdu_community = (const char *) pdu->community;
        if (!pdu_community)
            pdu_community = "";
        if (snmp_get_do_debugging()) {
            char           *buf;
            if (pdu->community) {
                buf = (char *) malloc(1 + pdu->community_len);
                memcpy(buf, pdu->community, pdu->community_len);
                buf[pdu->community_len] = '\0';
            } else {
                DEBUGMSGTL(("mibII/vacm_vars", "NULL community"));
                buf = strdup("NULL");
            }

            DEBUGMSGTL(("mibII/vacm_vars",
                        "vacm_in_view: ver=%ld, community=%s\n",
                        pdu->version, buf));
            free(buf);
        }

        /*
         * Okay, if this PDU was received from a UDP or a TCP transport then
         * ask the transport abstraction layer to map its source address and
         * community string to a security name for us.  
         */

        if (0) {
#ifdef NETSNMP_TRANSPORT_UDP_DOMAIN
        } else if (pdu->tDomain == netsnmpUDPDomain
#ifdef NETSNMP_TRANSPORT_TCP_DOMAIN
            || pdu->tDomain == netsnmp_snmpTCPDomain
#endif
            ) {
            if (!netsnmp_udp_getSecName(pdu->transport_data,
                                        pdu->transport_data_length,
                                        pdu_community,
                                        pdu->community_len, &sn,
                                        &contextName)) {
                /*
                 * There are no com2sec entries.  
                 */
                sn = NULL;
            }
            /* force the community -> context name mapping here */
            SNMP_FREE(pdu->contextName);
            pdu->contextName = strdup(contextName);
            pdu->contextNameLen = strlen(contextName);
#endif
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
        } else if (pdu->tDomain == netsnmp_UDPIPv6Domain
#ifdef NETSNMP_TRANSPORT_TCPIPV6_DOMAIN
                   || pdu->tDomain == netsnmp_TCPIPv6Domain
#endif
            ) {
            if (!netsnmp_udp6_getSecName(pdu->transport_data,
                                         pdu->transport_data_length,
                                         pdu_community,
                                         pdu->community_len, &sn,
                                         &contextName)) {
                /*
                 * There are no com2sec entries.  
                 */
                sn = NULL;
            }
            /* force the community -> context name mapping here */
            SNMP_FREE(pdu->contextName);
            pdu->contextName = strdup(contextName);
            pdu->contextNameLen = strlen(contextName);
#endif
#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
        } else if (pdu->tDomain == netsnmp_UnixDomain){
            if (!netsnmp_unix_getSecName(pdu->transport_data,
                                         pdu->transport_data_length,
                                         pdu_community,
                                         pdu->community_len, &sn,
                                         &contextName)) {
					sn = NULL;
            }
            /* force the community -> context name mapping here */
            SNMP_FREE(pdu->contextName);
            pdu->contextName = strdup(contextName);
            pdu->contextNameLen = strlen(contextName);
#endif	
        } else {
            /*
             * Map other <community, transport-address> pairs to security names
             * here.  For now just let non-IPv4 transport always succeed.
             * 
             * WHAAAATTTT.  No, we don't let non-IPv4 transports
             * succeed!  You must fix this to make it usable, sorry.
             * From a security standpoint this is insane. -- Wes
             */
            /** @todo alternate com2sec mappings for non v4 transports.
                Should be implemented via registration */
            sn = NULL;
        }

    } else
#endif /* support for community based SNMP */
      if (find_sec_mod(pdu->securityModel)) {
        /*
         * any legal defined v3 security model 
         */
        DEBUGMSG(("mibII/vacm_vars",
                  "vacm_in_view: ver=%ld, model=%d, secName=%s\n",
                  pdu->version, pdu->securityModel, pdu->securityName));
        sn = pdu->securityName;
        contextName = pdu->contextName;
    } else {
        sn = NULL;
    }

    if (sn == NULL) {
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
        snmp_increment_statistic(STAT_SNMPINBADCOMMUNITYNAMES);
#endif
        DEBUGMSGTL(("mibII/vacm_vars",
                    "vacm_in_view: No security name found\n"));
        return VACM_NOSECNAME;
    }

    if (pdu->contextNameLen > CONTEXTNAMEINDEXLEN) {
        DEBUGMSGTL(("mibII/vacm_vars",
                    "vacm_in_view: bad ctxt length %d\n",
                    (int)pdu->contextNameLen));
        return VACM_NOSUCHCONTEXT;
    }
    /*
     * NULL termination of the pdu field is ugly here.  Do in PDU parsing? 
     */
    if (pdu->contextName)
        memcpy(contextNameIndex, pdu->contextName, pdu->contextNameLen);
    else
        contextNameIndex[0] = '\0';

    contextNameIndex[pdu->contextNameLen] = '\0';
    if (!(flags & VACM_CHECK_VIEW_CONTENTS_DNE_CONTEXT_OK) &&
        !netsnmp_subtree_find_first(contextNameIndex)) {
        /*
         * rfc 3415 section 3.2, step 1
         * no such context here; return no such context error 
         */
        DEBUGMSGTL(("mibII/vacm_vars", "vacm_in_view: no such ctxt \"%s\"\n",
                    contextNameIndex));
        return VACM_NOSUCHCONTEXT;
    }

    DEBUGMSGTL(("mibII/vacm_vars", "vacm_in_view: sn=%s", sn));

    gp = vacm_getGroupEntry(pdu->securityModel, sn);
    if (gp == NULL) {
        DEBUGMSG(("mibII/vacm_vars", "\n"));
        return VACM_NOGROUP;
    }
    DEBUGMSG(("mibII/vacm_vars", ", gn=%s", gp->groupName));

    ap = vacm_getAccessEntry(gp->groupName, contextNameIndex,
                             pdu->securityModel, pdu->securityLevel);
    if (ap == NULL) {
        DEBUGMSG(("mibII/vacm_vars", "\n"));
        return VACM_NOACCESS;
    }

    if (name == NULL) { /* only check the setup of the vacm for the request */
        DEBUGMSG(("mibII/vacm_vars", ", Done checking setup\n"));
        return VACM_SUCCESS;
    }

    if (viewtype < 0 || viewtype >= VACM_MAX_VIEWS) {
        DEBUGMSG(("mibII/vacm_vars", " illegal view type\n"));
        return VACM_NOACCESS;
    }
    vn = ap->views[viewtype];
    DEBUGMSG(("mibII/vacm_vars", ", vn=%s", vn));

    if (check_subtree) {
        DEBUGMSG(("mibII/vacm_vars", "\n"));
        return vacm_checkSubtree(vn, name, namelen);
    }

    vp = vacm_getViewEntry(vn, name, namelen, VACM_MODE_FIND);

    if (vp == NULL) {
        DEBUGMSG(("mibII/vacm_vars", "\n"));
        return VACM_NOVIEW;
    }
    DEBUGMSG(("mibII/vacm_vars", ", vt=%d\n", vp->viewType));

    if (vp->viewType == SNMP_VIEW_EXCLUDED) {
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#if defined(NETSNMP_DISABLE_SNMPV1)
        if (pdu->version == SNMP_VERSION_2c)
#else
#if defined(NETSNMP_DISABLE_SNMPV2C)
        if (pdu->version == SNMP_VERSION_1)
#else
        if (pdu->version == SNMP_VERSION_1 || pdu->version == SNMP_VERSION_2c)
#endif
#endif
        {
            snmp_increment_statistic(STAT_SNMPINBADCOMMUNITYUSES);
        }
#endif
        return VACM_NOTINVIEW;
    }

    return VACM_SUCCESS;

}                               /* end vacm_in_view() */


