/*
 *  Host Resources MIB - other device implementation - hr_other.c
 *
 */

#include <net-snmp/net-snmp-config.h>

#include "host_res.h"
#include "hr_other.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif


void            Init_HR_CoProc(void);
int             Get_Next_HR_CoProc(void);
const char     *describe_coproc(int);

void
init_hr_other(void)
{
    init_device[HRDEV_COPROC] = Init_HR_CoProc;
    next_device[HRDEV_COPROC] = Get_Next_HR_CoProc;
    device_descr[HRDEV_COPROC] = describe_coproc;
}



static int      done_coProc;

void
Init_HR_CoProc(void)
{
#ifdef solaris2
    /* 
     * Sun didn't sell many Windows Co-processor boards 
     * somebody who has one will have to figure out how to discover it
     * so assume there are none
     */
    done_coProc = 1;
#else
    done_coProc = 0;
#endif
}

int
Get_Next_HR_CoProc(void)
{
    /*
     * How to identify the presence of a co-processor ? 
     */

    if (done_coProc != 1) {
        done_coProc = 1;
        return (HRDEV_COPROC << HRDEV_TYPE_SHIFT);
    } else
        return -1;
}


const char     *
describe_coproc(int idx)
{
    return ("Guessing that there's a floating point co-processor");
}
