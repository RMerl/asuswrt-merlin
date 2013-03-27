#ifndef _TC_UTIL_H_
#define _TC_UTIL_H_ 1

#define MAX_MSG 16384
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/gen_stats.h>
#include "tc_core.h"

/* This is the deprecated multiqueue interface */
#ifndef TCA_PRIO_MAX
enum
{
	TCA_PRIO_UNSPEC,
	TCA_PRIO_MQ,
	__TCA_PRIO_MAX
};

#define TCA_PRIO_MAX    (__TCA_PRIO_MAX - 1)
#endif

struct qdisc_util
{
	struct  qdisc_util *next;
	const char *id;
	int	(*parse_qopt)(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n);
	int	(*print_qopt)(struct qdisc_util *qu, FILE *f, struct rtattr *opt);
	int 	(*print_xstats)(struct qdisc_util *qu, FILE *f, struct rtattr *xstats);

	int	(*parse_copt)(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n);
	int	(*print_copt)(struct qdisc_util *qu, FILE *f, struct rtattr *opt);
};

extern __u16 f_proto;
struct filter_util
{
	struct filter_util *next;
	char	id[16];
	int	(*parse_fopt)(struct filter_util *qu, char *fhandle, int argc,
			      char **argv, struct nlmsghdr *n);
	int	(*print_fopt)(struct filter_util *qu, FILE *f, struct rtattr *opt, __u32 fhandle);
};

struct action_util
{
	struct  action_util *next;
	char    id[16];
	int     (*parse_aopt)(struct action_util *a, int *argc, char ***argv,
			      int code, struct nlmsghdr *n);
	int     (*print_aopt)(struct action_util *au, FILE *f, struct rtattr *opt);
	int     (*print_xstats)(struct action_util *au, FILE *f, struct rtattr *xstats);
};

extern const char *get_tc_lib(void);

extern struct qdisc_util *get_qdisc_kind(const char *str);
extern struct filter_util *get_filter_kind(const char *str);

extern int get_qdisc_handle(__u32 *h, const char *str);
extern int get_rate(unsigned *rate, const char *str);
extern int get_percent(unsigned *percent, const char *str);
extern int get_size(unsigned *size, const char *str);
extern int get_size_and_cell(unsigned *size, int *cell_log, char *str);
extern int get_time(unsigned *time, const char *str);
extern int get_linklayer(unsigned *val, const char *arg);

extern void print_rate(char *buf, int len, __u32 rate);
extern void print_size(char *buf, int len, __u32 size);
extern void print_percent(char *buf, int len, __u32 percent);
extern void print_qdisc_handle(char *buf, int len, __u32 h);
extern void print_time(char *buf, int len, __u32 time);
extern void print_linklayer(char *buf, int len, unsigned linklayer);
extern char * sprint_rate(__u32 rate, char *buf);
extern char * sprint_size(__u32 size, char *buf);
extern char * sprint_qdisc_handle(__u32 h, char *buf);
extern char * sprint_tc_classid(__u32 h, char *buf);
extern char * sprint_time(__u32 time, char *buf);
extern char * sprint_ticks(__u32 ticks, char *buf);
extern char * sprint_percent(__u32 percent, char *buf);
extern char * sprint_linklayer(unsigned linklayer, char *buf);

extern void print_tcstats_attr(FILE *fp, struct rtattr *tb[], char *prefix, struct rtattr **xstats);
extern void print_tcstats2_attr(FILE *fp, struct rtattr *rta, char *prefix, struct rtattr **xstats);

extern int get_tc_classid(__u32 *h, const char *str);
extern int print_tc_classid(char *buf, int len, __u32 h);
extern char * sprint_tc_classid(__u32 h, char *buf);

extern int tc_print_police(FILE *f, struct rtattr *tb);
extern int parse_police(int *, char ***, int, struct nlmsghdr *);

extern char *action_n2a(int action, char *buf, int len);
extern int  action_a2n(char *arg, int *result);
extern int  act_parse_police(struct action_util *a,int *, char ***, int, struct nlmsghdr *);
extern int  print_police(struct action_util *a, FILE *f,
			 struct rtattr *tb);
extern int  police_print_xstats(struct action_util *a,FILE *f,
				struct rtattr *tb);
extern int  tc_print_action(FILE *f, const struct rtattr *tb);
extern int  tc_print_ipt(FILE *f, const struct rtattr *tb);
extern int  parse_action(int *, char ***, int, struct nlmsghdr *);
extern void print_tm(FILE *f, const struct tcf_t *tm);

#endif
