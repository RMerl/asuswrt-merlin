
#define TCA_BUF_MAX	(64*1024)

extern struct rtnl_handle rth;
extern int do_qdisc(int argc, char **argv);
extern int do_class(int argc, char **argv);
extern int do_filter(int argc, char **argv);
extern int do_action(int argc, char **argv);
extern int do_tcmonitor(int argc, char **argv);
extern int print_action(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern int print_filter(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern int print_qdisc(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern int print_class(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern void print_size_table(FILE *fp, const char *prefix, struct rtattr *rta);

struct tc_estimator;
extern int parse_estimator(int *p_argc, char ***p_argv, struct tc_estimator *est);

struct tc_sizespec;
extern int parse_size_table(int *p_argc, char ***p_argv, struct tc_sizespec *s);
extern int check_size_table_opts(struct tc_sizespec *s);
