#ifndef _M68K_MACHDEP_H
#define _M68K_MACHDEP_H

#include <linux/seq_file.h>
#include <linux/interrupt.h>

struct pt_regs;
struct mktime;
struct rtc_time;
struct rtc_pll_info;
struct buffer_head;

extern void (*mach_sched_init) (irq_handler_t handler);
/* machine dependent irq functions */
extern void (*mach_init_IRQ) (void);
extern void (*mach_get_model) (char *model);
extern void (*mach_get_hardware_list) (struct seq_file *m);
/* machine dependent timer functions */
extern unsigned long (*mach_gettimeoffset)(void);
extern int (*mach_hwclk)(int, struct rtc_time*);
extern unsigned int (*mach_get_ss)(void);
extern int (*mach_get_rtc_pll)(struct rtc_pll_info *);
extern int (*mach_set_rtc_pll)(struct rtc_pll_info *);
extern int (*mach_set_clock_mmss)(unsigned long);
extern void (*mach_gettod)(int *year, int *mon, int *day, int *hour,
			    int *min, int *sec);
extern void (*mach_reset)( void );
extern void (*mach_halt)( void );
extern void (*mach_power_off)( void );
extern unsigned long (*mach_hd_init) (unsigned long, unsigned long);
extern void (*mach_hd_setup)(char *, int *);
extern long mach_max_dma_address;
extern void (*mach_heartbeat) (int);
extern void (*mach_l2_flush) (int);
extern void (*mach_beep) (unsigned int, unsigned int);

/* Hardware clock functions */
extern void hw_timer_init(void);
extern unsigned long hw_timer_offset(void);
extern irqreturn_t arch_timer_interrupt(int irq, void *dummy);

extern void config_BSP(char *command, int len);
extern void do_IRQ(int irq, struct pt_regs *fp);

#endif /* _M68K_MACHDEP_H */
