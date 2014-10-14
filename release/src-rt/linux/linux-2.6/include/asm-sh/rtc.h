#ifndef _ASM_RTC_H
#define _ASM_RTC_H

extern void (*board_time_init)(void);
extern void (*rtc_sh_get_time)(struct timespec *);
extern int (*rtc_sh_set_time)(const time_t);

#endif /* _ASM_RTC_H */
