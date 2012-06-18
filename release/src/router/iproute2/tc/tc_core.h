#ifndef _TC_CORE_H_
#define _TC_CORE_H_ 1

#include <asm/types.h>
#include <linux/pkt_sched.h>

long tc_core_usec2tick(long usec);
long tc_core_tick2usec(long tick);
unsigned tc_calc_xmittime(unsigned rate, unsigned size);
int tc_calc_rtable(unsigned bps, __u32 *rtab, int cell_log, unsigned mtu, unsigned mpu);

int tc_setup_estimator(unsigned A, unsigned time_const, struct tc_estimator *est);

int tc_core_init(void);

extern struct rtnl_handle g_rth;
extern int is_batch_mode;

#endif
