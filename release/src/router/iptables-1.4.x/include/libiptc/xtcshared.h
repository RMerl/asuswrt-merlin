#ifndef _LIBXTC_SHARED_H
#define _LIBXTC_SHARED_H 1

typedef char xt_chainlabel[32];
struct xtc_handle;
struct xt_counters;

struct xtc_ops {
	int (*commit)(struct xtc_handle *);
	void (*free)(struct xtc_handle *);
	int (*builtin)(const char *, struct xtc_handle *const);
	int (*is_chain)(const char *, struct xtc_handle *const);
	int (*flush_entries)(const xt_chainlabel, struct xtc_handle *);
	int (*create_chain)(const xt_chainlabel, struct xtc_handle *);
	int (*set_policy)(const xt_chainlabel, const xt_chainlabel,
			  struct xt_counters *, struct xtc_handle *);
	const char *(*strerror)(int);
};

#endif /* _LIBXTC_SHARED_H */
