/*
 *  Host Resources MIB - printer device group implementation - hr_print.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "host_res.h"
#include "hr_print.h"
#include "struct.h"
#include "util_funcs.h"

#define HRPRINT_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

void            Init_HR_Print(void);
int             Get_Next_HR_Print(void);
void            Save_HR_Print(void);
const char     *describe_printer(int);
int             printer_status(int);
int             printer_detail_status(int);
int             printer_errors(int);
int             header_hrprint(struct variable *, oid *, size_t *, int,
                               size_t *, WriteMethod **);
FILE           *run_lpstat(int *);


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define	HRPRINT_STATUS		1
#define	HRPRINT_ERROR		2

struct variable4 hrprint_variables[] = {
    {HRPRINT_STATUS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrprint, 2, {1, 1}},
    {HRPRINT_ERROR, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrprint, 2, {1, 2}}
};
oid             hrprint_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 5 };


void
init_hr_print(void)
{
    init_device[HRDEV_PRINTER] = Init_HR_Print;
    next_device[HRDEV_PRINTER] = Get_Next_HR_Print;
    /*
     * save_device[ HRDEV_PRINTER ] = Save_HR_Print;        
     */
#ifdef HRPRINT_MONOTONICALLY_INCREASING
    dev_idx_inc[HRDEV_PRINTER] = 1;
#endif

    device_descr[HRDEV_PRINTER] = describe_printer;
    device_status[HRDEV_PRINTER] = printer_status;
    device_errors[HRDEV_PRINTER] = printer_errors;

    REGISTER_MIB("host/hr_print", hrprint_variables, variable4,
                 hrprint_variables_oid);
}

/*
 * header_hrprint(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */
int
header_hrprint(struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
#define HRPRINT_ENTRY_NAME_LENGTH	11
    oid             newname[MAX_OID_LEN];
    int             print_idx, LowIndex = -1;
    int             result;

    DEBUGMSGTL(("host/hr_print", "var_hrprint: "));
    DEBUGMSGOID(("host/hr_print", name, *length));
    DEBUGMSG(("host/hr_print", " %d\n", exact));

    memcpy((char *) newname, (char *) vp->name, vp->namelen * sizeof(oid));
    /*
     * Find "next" print entry 
     */

    Init_HR_Print();
    for (;;) {
        print_idx = Get_Next_HR_Print();
        if (print_idx == -1)
            break;
        newname[HRPRINT_ENTRY_NAME_LENGTH] = print_idx;
        result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
        if (exact && (result == 0)) {
            LowIndex = print_idx;
            /*
             * Save printer status information 
             */
            break;
        }
        if ((!exact && (result < 0)) &&
            (LowIndex == -1 || print_idx < LowIndex)) {
            LowIndex = print_idx;
            /*
             * Save printer status information 
             */
#ifdef HRPRINT_MONOTONICALLY_INCREASING
            break;
#endif
        }
    }

    if (LowIndex == -1) {
        DEBUGMSGTL(("host/hr_print", "... index out of range\n"));
        return (MATCH_FAILED);
    }

    memcpy((char *) name, (char *) newname,
           (vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */

    DEBUGMSGTL(("host/hr_print", "... get print stats "));
    DEBUGMSGOID(("host/hr_print", name, *length));
    DEBUGMSG(("host/hr_print", "\n"));
    return LowIndex;
}


        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/


u_char         *
var_hrprint(struct variable * vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             print_idx;

    print_idx =
        header_hrprint(vp, name, length, exact, var_len, write_method);
    if (print_idx == MATCH_FAILED)
        return NULL;


    switch (vp->magic) {
    case HRPRINT_STATUS:
        long_return = printer_detail_status(print_idx);
        return (u_char *) & long_return;
    case HRPRINT_ERROR:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#else
        long_return = 0;        /* Null string */
        return (u_char *) & long_return;
#endif
    default:
        DEBUGMSGTL(("host/hr_print", "unknown sub-id %d in var_hrprint\n",
                    vp->magic));
    }
    return NULL;
}


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int      HRP_index;
static char   **HRP_name;
static int      HRP_nbrnames, HRP_maxnames;

#define HRP_MAX_INCR 10

void
Init_HR_Print(void)
{
#if HAVE_LPSTAT || HAVE_CGETNEXT || HAVE_PRINTCAP
    int             i;
#if HAVE_PRINTCAP
    FILE           *p;
#elif HAVE_CGETNEXT
    const char     *caps[] = { "/etc/printcap", NULL };
#elif HAVE_LPSTAT
    int	            fd;
    FILE           *p;
#endif

    HRP_index = 0;		/* fail safe at Get_Next_HR_Print */

    if (HRP_name) {
        for (i = 0; i < HRP_nbrnames; i++)
            free(HRP_name[i]);
        HRP_nbrnames = 0;
        HRP_maxnames = 0;
        SNMP_FREE(HRP_name);
    }

#if HAVE_PRINTCAP
    if ((p = fopen("/etc/printcap", "r")) != NULL) {
        char            buf[BUFSIZ], *ptr;
        while (fgets(buf, sizeof buf, p)) {
            buf[strlen(buf) - 1] = 0;
            if (buf[0] == '#' || buf[0] == 0 || buf[0] == ' '
                || buf[0] == '\t')
                continue;
            if ((ptr = strchr(buf, '\\')))
                *ptr = 0;
            if ((ptr = strchr(buf, ':')))
                *ptr = 0;
            if ((ptr = strchr(buf, '|')))
                *ptr = 0;
            ptr = buf;
#elif HAVE_CGETNEXT
    {
        char           *buf = NULL, *ptr;
        while (cgetnext(&buf, caps) > 0) {
            if ((ptr = strchr(buf, ':')))
                *ptr = 0;
            if ((ptr = strchr(buf, '|')))
                *ptr = 0;
            ptr = buf;
#elif HAVE_LPSTAT
    if ((p = run_lpstat(&fd)) != NULL) {
        char            buf[BUFSIZ], ptr[BUFSIZ];
        while (fgets(buf, sizeof buf, p)) {
            sscanf(buf, "%*s %*s %[^:]", ptr);
#endif
            if (HRP_nbrnames == HRP_maxnames) {
                char          **tmp;
                tmp = (char **) calloc(HRP_maxnames + HRP_MAX_INCR, sizeof(char *));
                if (!tmp)
                    goto finish;
		if (HRP_name) {
			memcpy(tmp, HRP_name, HRP_nbrnames * sizeof(char *));
			free(HRP_name);
		}
                HRP_maxnames += HRP_MAX_INCR;
                HRP_name = tmp;
            }
            HRP_name[HRP_nbrnames++] = strdup(ptr);
#if !defined(HAVE_PRINTCAP) && defined(HAVE_CGETNEXT)
            if (buf)
                free(buf);
#endif
        }
finish:
#if HAVE_PRINTCAP
        fclose(p);
#elif HAVE_CGETNEXT
        cgetclose();
#elif HAVE_LPSTAT
        fclose(p);
        close(fd);
#endif
    }
#endif                          /* HAVE_anything */
}

int
Get_Next_HR_Print(void)
{
    /*
     * The initial implementation system
     *   has no printers attached, and I've
     *   no real idea how to detect them,
     *   so don't bother.
     */
    if (HRP_index < HRP_nbrnames)  /* No printer */
        return (HRDEV_PRINTER << HRDEV_TYPE_SHIFT) + HRP_index++;
    else
        return -1;
}

const char     *
describe_printer(int idx)
{
    if (HRP_index == 0)  /* return empty string if not initialized */
	return "";

    DEBUGMSGTL(("host/hr_print", "describe p: %d/%d %s\n", HRP_index, idx,
                HRP_name[HRP_index - 1]));
    return HRP_name[HRP_index - 1];
}

int
printer_status(int idx)
{
    /*
     * hrDeviceStatus OBJECT-TYPE
     * SYNTAX     INTEGER {
     * unknown(1), running(2), warning(3), testing(4), down(5)
     * }
     */

    return 1;                   /* unknown */
}

int
printer_detail_status(int idx)
{
    /*
     * hrPrinterStatus OBJECT-TYPE
     * SYNTAX     INTEGER {
     * other(1), unknown(2), idle(3), printing(4), warmup(5)
     * }
     */

    return 2;                   /* unknown */
}

int
printer_errors(int idx)
{
    return 0;
}

#ifdef        HAVE_LPSTAT
/*
 * Run the lpstat command. If compiled with EXCACHE support, this
 * will actually cache the output for a while which helps a lot
 * with snmpbulkwalk (in fact, it keeps the client from exiting
 * due to timeouts).
 */
FILE           *
run_lpstat(int *fd)
{
    struct extensible ex;

    memset(&ex, 0, sizeof(ex));
    strcpy(ex.command, LPSTAT_PATH " -v");
    if ((*fd = get_exec_output(&ex)) < 0)
        return NULL;

    return fdopen(*fd, "r");
}
#endif
