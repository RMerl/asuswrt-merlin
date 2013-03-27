#ifndef _XTABLES_INTERNAL_H
#define _XTABLES_INTERNAL_H 1

#ifndef XT_LIB_DIR
#	define XT_LIB_DIR "/lib/xtables"
#endif

/* protocol family dependent informations */
struct afinfo {
	/* protocol family */
	int family;

	/* prefix of library name (ex "libipt_" */
	char *libprefix;

	/* used by setsockopt (ex IPPROTO_IP */
	int ipproto;

	/* kernel module (ex "ip_tables" */
	char *kmod;

	/* optname to check revision support of match */
	int so_rev_match;

	/* optname to check revision support of match */
	int so_rev_target;
};

enum xt_tryload {
	DONT_LOAD,
	DURING_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

struct xtables_rule_match {
	struct xtables_rule_match *next;
	struct xtables_match *match;
	/* Multiple matches of the same type: the ones before
	   the current one are completed from parsing point of view */
	unsigned int completed;
};

extern char *lib_dir;

extern void *fw_calloc(size_t count, size_t size);
extern void *fw_malloc(size_t size);

extern const char *modprobe_program;
extern int xtables_insmod(const char *modname, const char *modprobe, int quiet);
extern int load_xtables_ko(const char *modprobe, int quiet);

/* This is decleared in ip[6]tables.c */
extern struct afinfo afinfo;

/* Keeping track of external matches and targets: linked lists.  */
extern struct xtables_match *xtables_matches;
extern struct xtables_target *xtables_targets;

extern struct xtables_match *find_match(const char *name, enum xt_tryload,
					struct xtables_rule_match **match);
extern struct xtables_target *find_target(const char *name, enum xt_tryload);

extern void _init(void);

#endif /* _XTABLES_INTERNAL_H */
