#ifndef _TC_RED_H_
#define _TC_RED_H_ 1

extern int tc_red_eval_P(unsigned qmin, unsigned qmax, double prob);
extern int tc_red_eval_ewma(unsigned qmin, unsigned burst, unsigned avpkt);
extern int tc_red_eval_idle_damping(int wlog, unsigned avpkt, unsigned bandwidth, __u8 *sbuf);

#endif
