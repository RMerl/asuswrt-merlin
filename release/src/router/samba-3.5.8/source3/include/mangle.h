#ifndef _MANGLE_H_
#define _MANGLE_H_
/*
  header for 8.3 name mangling interface 
*/

struct mangle_fns {
	void (*reset)(void);
	bool (*is_mangled)(const char *s, const struct share_params *p);
	bool (*must_mangle)(const char *s, const struct share_params *p);
	bool (*is_8_3)(const char *fname, bool check_case, bool allow_wildcards,
		       const struct share_params *p);
	bool (*lookup_name_from_8_3)(TALLOC_CTX *ctx,
				const char *in,
				char **out, /* talloced on the given context. */
				const struct share_params *p);
	bool (*name_to_8_3)(const char *in,
			char out[13],
			bool cache83,
			int default_case,
			const struct share_params *p);
};
#endif /* _MANGLE_H_ */
