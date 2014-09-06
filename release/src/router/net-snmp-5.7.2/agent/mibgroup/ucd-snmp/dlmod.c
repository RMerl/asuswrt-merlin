/*
 *  Dynamic Loadable Agent Modules MIB (UCD-DLMOD-MIB) - dlmod.c
 *
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <dlfcn.h>
#include "dlmod.h"

#ifndef SNMPDLMODPATH
#define SNMPDLMODPATH "/usr/local/lib/snmp/dlmod"
#endif

struct dlmod {
  struct dlmod   *next;
  int             index;
  char            name[64 + 1];
  char            path[255 + 1];
  char            error[255 + 1];
  void           *handle;
  int             status;
};

#define DLMOD_LOADED		1
#define DLMOD_UNLOADED		2
#define DLMOD_ERROR		3
#define DLMOD_LOAD		4
#define DLMOD_UNLOAD		5
#define DLMOD_CREATE		6
#define DLMOD_DELETE		7

static struct dlmod *dlmods = NULL;
static unsigned int dlmod_next_index = 1;
static char     dlmod_path[1024];

static struct dlmod   *
dlmod_create_module(void)
{
    struct dlmod  **pdlmod, *dlm;

    DEBUGMSGTL(("dlmod", "dlmod_create_module\n"));
    dlm = (struct dlmod *) calloc(1, sizeof(struct dlmod));
    if (dlm == NULL)
        return NULL;

    dlm->index = (int)dlmod_next_index++;
    dlm->status = DLMOD_UNLOADED;

    for (pdlmod = &dlmods; *pdlmod != NULL; pdlmod = &((*pdlmod)->next));
    (*pdlmod) = dlm;

    return dlm;
}

static void
dlmod_delete_module(struct dlmod *dlm)
{
    struct dlmod  **pdlmod;

    DEBUGMSGTL(("dlmod", "dlmod_delete_module\n"));
    if (!dlm || dlm->status != DLMOD_UNLOADED)
        return;

    for (pdlmod = &dlmods; *pdlmod; pdlmod = &((*pdlmod)->next))
        if (*pdlmod == dlm) {
            *pdlmod = dlm->next;
            free(dlm);
            return;
        }
}

static void
dlmod_load_module(struct dlmod *dlm)
{
    DEBUGMSGTL(("dlmod", "dlmod_load_module %s: %s\n", dlm->name,
                dlm->path));

    if (!dlm || !dlm->path || !dlm->name ||
        (dlm->status != DLMOD_UNLOADED && dlm->status != DLMOD_ERROR))
        return;

    if (dlm->path[0] == '/') {
#ifdef RTLD_NOW
        dlm->handle = dlopen(dlm->path, RTLD_NOW);
#else
        dlm->handle = dlopen(dlm->path, RTLD_LAZY);
#endif
        if (dlm->handle == NULL) {
            snprintf(dlm->error, sizeof(dlm->error),
                     "dlopen failed: %s", dlerror());
            dlm->status = DLMOD_ERROR;
            return;
        }
    } else {
        char *st, *p, tmp_path[255];

        for (p = strtok_r(dlmod_path, ":", &st); p; p = strtok_r(NULL, ":", &st)) {
            snprintf(tmp_path, sizeof(tmp_path), "%s/%s.so", p, dlm->path);
            DEBUGMSGTL(("dlmod", "p: %s tmp_path: %s\n", p, tmp_path));
#ifdef RTLD_NOW
            dlm->handle = dlopen(tmp_path, RTLD_NOW);
#else
            dlm->handle = dlopen(tmp_path, RTLD_LAZY);
#endif
            if (dlm->handle == NULL) {
                snprintf(dlm->error, sizeof(dlm->error),
                         "dlopen failed: %s", dlerror());
                dlm->status = DLMOD_ERROR;
            }
        }
        strlcpy(dlm->path, tmp_path, sizeof(dlm->path));
        if (dlm->status == DLMOD_ERROR)
            return;
    }
    {
        char sym_init[64 + sizeof("init_")];
        int  (*dl_init) (void);

        snprintf(sym_init, sizeof(sym_init), "init_%s", dlm->name);
        dl_init = dlsym(dlm->handle, sym_init);
        if (dl_init == NULL) {
            dlclose(dlm->handle);
            snprintf(dlm->error, sizeof(dlm->error),
                     "dlsym failed: can't find \'%s\'", sym_init);
            dlm->status = DLMOD_ERROR;
            return;
        }
        dl_init();
    }

    dlm->error[0] = '\0';
    dlm->status = DLMOD_LOADED;
}

static void
dlmod_unload_module(struct dlmod *dlm)
{
    char            sym_deinit[64 + sizeof("shutdown_")];
    int             (*dl_deinit) (void);

    if (!dlm || dlm->status != DLMOD_LOADED)
        return;

    snprintf(sym_deinit, sizeof(sym_deinit), "deinit_%s", dlm->name);
    dl_deinit = dlsym(dlm->handle, sym_deinit);
    if (!dl_deinit) {
        snprintf(sym_deinit, sizeof(sym_deinit), "shutdown_%s", dlm->name);
        dl_deinit = dlsym(dlm->handle, sym_deinit);
    }
    if (dl_deinit) {
        DEBUGMSGTL(("dlmod", "Calling %s()\n", sym_deinit));
        dl_deinit();
    } else {
        DEBUGMSGTL(("dlmod", "No destructor for %s\n", dlm->name));
    }
    dlclose(dlm->handle);
    dlm->status = DLMOD_UNLOADED;
    DEBUGMSGTL(("dlmod", "Module %s unloaded\n", dlm->name));
}

static struct dlmod   *
dlmod_get_by_index(int iindex)
{
    struct dlmod   *dlmod;

    for (dlmod = dlmods; dlmod; dlmod = dlmod->next)
        if (dlmod->index == iindex)
            return dlmod;

    return NULL;
}

/*
 * Functions to parse config lines
 */

static void
dlmod_parse_config(const char *token, char *cptr)
{
    char           *dlm_name, *dlm_path;
    struct dlmod   *dlm;
    char           *st;

    if (cptr == NULL) {
        config_perror("Bad dlmod line");
        return;
    }
    /*
     * remove comments
     */
    *(cptr + strcspn(cptr, "#;\r\n")) = '\0';

    dlm = dlmod_create_module();
    if (!dlm)
        return;

    /*
     * dynamic module name
     */
    dlm_name = strtok_r(cptr, "\t ", &st);
    if (dlm_name == NULL) {
        config_perror("Bad dlmod line");
        dlmod_delete_module(dlm);
        return;
    }
    strlcpy(dlm->name, dlm_name, sizeof(dlm->name));

    /*
     * dynamic module path
     */
    dlm_path = strtok_r(NULL, "\t ", &st);
    if (dlm_path)
        strlcpy(dlm->path, dlm_path, sizeof(dlm->path));
    else
        strlcpy(dlm->path, dlm_name, sizeof(dlm->path));

    dlmod_load_module(dlm);

    if (dlm->status == DLMOD_ERROR)
        snmp_log(LOG_ERR, "%s\n", dlm->error);
}

static void
dlmod_free_config(void)
{
    struct dlmod   *dtmp, *dtmp2;

    for (dtmp = dlmods; dtmp != NULL;) {
        dtmp2 = dtmp;
        dtmp = dtmp->next;
        dlmod_unload_module(dtmp2);
        free(dtmp2);
    }
    dlmods = NULL;
}

/*
 * Functions to handle SNMP management
 */

#define DLMODNEXTINDEX 		1
#define DLMODINDEX     		2
#define DLMODNAME      		3
#define DLMODPATH      		4
#define DLMODERROR     		5
#define DLMODSTATUS    		6

/*
 * header_dlmod(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 */

static int
header_dlmod(struct variable *vp,
             oid * name,
             size_t * length,
             int exact, size_t * var_len, WriteMethod ** write_method)
{
#define DLMOD_NAME_LENGTH 10
    oid             newname[MAX_OID_LEN];
    int             result;

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    newname[DLMOD_NAME_LENGTH] = 0;

    result =
        snmp_oid_compare(name, *length, newname, (int) vp->namelen + 1);
    if ((exact && (result != 0)) || (!exact && (result >= 0))) {
        return MATCH_FAILED;
    }

    memcpy((char *) name, (char *) newname,
           ((int) vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = 0;
    *var_len = sizeof(long);    /* default to 'long' results */
    return MATCH_SUCCEEDED;
}


static u_char         *
var_dlmod(struct variable * vp,
          oid * name,
          size_t * length,
          int exact, size_t * var_len, WriteMethod ** write_method)
{

    /*
     * variables we may use later
     */

    *write_method = 0;          /* assume it isnt writable for the time being */
    *var_len = sizeof(int);     /* assume an integer and change later
                                 * if not */

    if (header_dlmod(vp, name, length, exact,
                     var_len, write_method) == MATCH_FAILED)
        return NULL;

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch (vp->magic) {
    case DLMODNEXTINDEX:
        long_return = dlmod_next_index;
        return (unsigned char *) &long_return;
    default:
        DEBUGMSGTL(("dlmod", "unknown sub-id %d in var_dlmod\n",
                    vp->magic));
    }
    return NULL;
}


static int
write_dlmodName(int action,
                u_char * var_val,
                u_char var_val_type,
                size_t var_val_len,
                u_char * statP, oid * name, size_t name_len)
{
    static struct dlmod *dlm;

    if (var_val_type != ASN_OCTET_STR) {
        snmp_log(LOG_ERR, "write to dlmodName not ASN_OCTET_STR\n");
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(dlm->name)-1) {
        snmp_log(LOG_ERR, "write to dlmodName: bad length: too long\n");
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        dlm = dlmod_get_by_index(name[12]);
        if (!dlm || dlm->status == DLMOD_LOADED)
            return SNMP_ERR_RESOURCEUNAVAILABLE;
        strncpy(dlm->name, (const char *) var_val, var_val_len);
        dlm->name[var_val_len] = 0;
    }
    return SNMP_ERR_NOERROR;
}

static int
write_dlmodPath(int action,
                u_char * var_val,
                u_char var_val_type,
                size_t var_val_len,
                u_char * statP, oid * name, size_t name_len)
{
    static struct dlmod *dlm;

    if (var_val_type != ASN_OCTET_STR) {
        snmp_log(LOG_ERR, "write to dlmodPath not ASN_OCTET_STR\n");
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(dlm->path)-1) {
        snmp_log(LOG_ERR, "write to dlmodPath: bad length: too long\n");
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        dlm = dlmod_get_by_index(name[12]);
        if (!dlm || dlm->status == DLMOD_LOADED)
            return SNMP_ERR_RESOURCEUNAVAILABLE;
        strncpy(dlm->path, (const char *) var_val, var_val_len);
        dlm->path[var_val_len] = 0;
    }
    return SNMP_ERR_NOERROR;
}

static int
write_dlmodStatus(int action,
                  u_char * var_val,
                  u_char var_val_type,
                  size_t var_val_len,
                  u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later
     */
    struct dlmod   *dlm;

    if (var_val_type != ASN_INTEGER) {
        snmp_log(LOG_ERR, "write to dlmodStatus not ASN_INTEGER\n");
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long)) {
        snmp_log(LOG_ERR, "write to dlmodStatus: bad length\n");
        return SNMP_ERR_WRONGLENGTH;
    }
    if (action == COMMIT) {
        /*
         * object identifier in form .1.3.6.1.4.1.2021.13.14.2.1.4.x
         * where X is index with offset 12
         */

        dlm = dlmod_get_by_index(name[12]);
        switch (*((long *) var_val)) {
        case DLMOD_CREATE:
            if (dlm || (name[12] != dlmod_next_index))
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            dlm = dlmod_create_module();
            if (!dlm)
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            break;
        case DLMOD_LOAD:
            if (!dlm || dlm->status == DLMOD_LOADED)
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            dlmod_load_module(dlm);
            break;
        case DLMOD_UNLOAD:
            if (!dlm || dlm->status != DLMOD_LOADED)
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            dlmod_unload_module(dlm);
            break;
        case DLMOD_DELETE:
            if (!dlm || dlm->status == DLMOD_LOADED)
                return SNMP_ERR_RESOURCEUNAVAILABLE;
            dlmod_delete_module(dlm);
            break;
        default:
            return SNMP_ERR_WRONGVALUE;
        }
    }
    return SNMP_ERR_NOERROR;
}

/*
 * header_dlmodEntry(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 *
 */


static struct dlmod *
header_dlmodEntry(struct variable *vp,
                  oid * name,
                  size_t * length,
                  int exact, size_t * var_len, WriteMethod ** write_method)
{
#define DLMODENTRY_NAME_LENGTH 12
    oid             newname[MAX_OID_LEN];
    int             result;
    struct dlmod   *dlm = NULL;
    unsigned int    dlmod_index;

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    *write_method = 0;

    for (dlmod_index = 1; dlmod_index < dlmod_next_index; dlmod_index++) {
        dlm = dlmod_get_by_index(dlmod_index);

        DEBUGMSGTL(("dlmod", "dlmodEntry dlm: %p dlmod_index: %d\n",
                    dlm, dlmod_index));

        if (dlm) {
            newname[12] = dlmod_index;
            result = snmp_oid_compare(name, *length, newname,
                                      (int) vp->namelen + 1);

            if ((exact && (result == 0)) || (!exact && (result < 0)))
                break;
        }
    }

    if (dlmod_index >= dlmod_next_index) {
        if (dlmod_index == dlmod_next_index &&
            exact && vp->magic == DLMODSTATUS)

            *write_method = write_dlmodStatus;
        return NULL;
    }

    memcpy((char *) name, (char *) newname,
           ((int) vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *var_len = sizeof(long);
    return dlm;
}

static u_char         *
var_dlmodEntry(struct variable * vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * variables we may use later
     */
    struct dlmod   *dlm;

    *var_len = sizeof(int);     /* assume an integer and change later
                                 * if not */

    dlm =
        header_dlmodEntry(vp, name, length, exact, var_len, write_method);
    if (dlm == NULL)
        return NULL;

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch (vp->magic) {
    case DLMODNAME:
        *write_method = write_dlmodName;
        *var_len = strlen(dlm->name);
        return (unsigned char *) dlm->name;
    case DLMODPATH:
        *write_method = write_dlmodPath;
        *var_len = strlen(dlm->path);
        return (unsigned char *) dlm->path;
    case DLMODERROR:
        *var_len = strlen(dlm->error);
        return (unsigned char *) dlm->error;
    case DLMODSTATUS:
        *write_method = write_dlmodStatus;
        long_return = dlm->status;
        return (unsigned char *) &long_return;
    default:
        DEBUGMSGTL(("dlmod", "unknown sub-id %d in var_dlmodEntry\n",
                    vp->magic));
    }
    return NULL;
}

/*
 * this variable defines function callbacks and type return
 * information for the dlmod mib
 */
static struct variable4 dlmod_variables[] = {
    {DLMODNEXTINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_dlmod, 1, {1}},
    {DLMODNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_dlmodEntry, 3, {2, 1, 2}},
    {DLMODPATH, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_dlmodEntry, 3, {2, 1, 3}},
    {DLMODERROR, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_dlmodEntry, 3, {2, 1, 4}},
    {DLMODSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_dlmodEntry, 3, {2, 1, 5}},
};

static oid dlmod_variables_oid[] = { 1, 3, 6, 1, 4, 1, 2021, 13, 14 };

void
init_dlmod(void)
{
    REGISTER_MIB("dlmod", dlmod_variables, variable4, dlmod_variables_oid);

    /*
     * TODO: REGISTER_SYSOR_ENTRY
     */

    DEBUGMSGTL(("dlmod", "register mib\n"));

    snmpd_register_config_handler("dlmod", dlmod_parse_config,
                                  dlmod_free_config,
                                  "module-name module-path");

    {
        const char * const p = getenv("SNMPDLMODPATH");
        strncpy(dlmod_path, SNMPDLMODPATH, sizeof(dlmod_path));
        dlmod_path[ sizeof(dlmod_path) - 1 ] = 0;
        if (p) {
            if (p[0] == ':') {
                int len = strlen(dlmod_path);
                if (dlmod_path[len - 1] != ':') {
                    strncat(dlmod_path, ":", sizeof(dlmod_path) - len - 1);
                    len++;
                }
                strncat(dlmod_path, p + 1,   sizeof(dlmod_path) - len);
            } else
                strncpy(dlmod_path, p, sizeof(dlmod_path));
        }
    }

    dlmod_path[ sizeof(dlmod_path)-1 ] = 0;
    DEBUGMSGTL(("dlmod", "dlmod_path: %s\n", dlmod_path));
}

netsnmp_feature_require(snmpd_unregister_config_handler)

void
shutdown_dlmod(void)
{
    snmpd_unregister_config_handler("dlmod");
    unregister_mib(dlmod_variables_oid, OID_LENGTH(dlmod_variables_oid));
}
