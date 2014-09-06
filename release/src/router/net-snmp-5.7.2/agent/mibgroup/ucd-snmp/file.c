/* Portions of this file are subject to the following copyrights.  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>

#include <sys/types.h>
#include <sys/stat.h>
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
#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "struct.h"
#include "file.h"
#include "util_funcs/header_simple_table.h"

#define MAXFILE   20

struct filestat fileTable[MAXFILE];
int             fileCount;

void
init_file(void)
{
    struct variable2 file_table[] = {
        {FILE_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {1}},
        {FILE_NAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {2}},
        {FILE_SIZE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {3}},
        {FILE_MAX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {4}},
        {FILE_ERROR, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {100}},
        {FILE_MSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_file_table, 1, {101}}
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             file_variables_oid[] = { NETSNMP_UCDAVIS_MIB, 15, 1 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/file", file_table, variable2,
                 file_variables_oid);

    snmpd_register_config_handler("file", file_parse_config,
                                  file_free_config, "file [maxsize]");

}

void
file_free_config(void)
{
    fileCount = 0;
}

void
file_parse_config(const char *token, char *cptr)
{
    char *cp;
	
    if (fileCount < MAXFILE) {
        fileTable[fileCount].max = -1;

        cp = copy_nword(cptr, fileTable[fileCount].name, FILE_NAME_MAX);

	if (strlen(fileTable[fileCount].name) >= FILE_NAME_MAX - 1) {
            config_perror("file name too long");
            return;
	}

        if (cp)
            fileTable[fileCount].max = strtoul(cp, NULL, 10);
        else
            fileTable[fileCount].max = -1;

        fileCount++;
    }
}

void
updateFile(int iindex)
{
    struct stat     sb;

    if (stat(fileTable[iindex].name, &sb) == 0)
        fileTable[iindex].size = sb.st_size >> 10;
}

/*
 * OID functions 
 */

u_char         *
var_file_table(struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static char     error[256];
    int             iindex;
    struct filestat *file;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, fileCount))
        return (NULL);

    iindex = name[*length - 1] - 1;

    updateFile(iindex);

    file = &fileTable[iindex];

    switch (vp->magic) {
    case FILE_INDEX:
        long_ret = iindex + 1;
        return (u_char *) & long_ret;

    case FILE_NAME:
        *var_len = strlen(file->name);
        return (u_char *) file->name;

    case FILE_SIZE:
        long_ret = file->size;
        return (u_char *) & long_ret;

    case FILE_MAX:
        long_ret = file->max;
        return (u_char *) & long_ret;

    case FILE_ERROR:
        if (file->max >= 0 && file->size > file->max)
            long_ret = 1;
        else
            long_ret = 0;

        return (u_char *) & long_ret;

    case FILE_MSG:
        if (file->max >= 0 && file->size > file->max)
            snprintf(error, sizeof(error), FILE_ERROR_MSG, file->name,
		file->max, file->size);
        else
            strcpy(error, "");

        *var_len = strlen(error);
        return (u_char *) error;

    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_file_table\n",
                    vp->magic));
    }

    return NULL;
}
