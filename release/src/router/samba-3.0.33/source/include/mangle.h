#ifndef _MANGLE_H_
#define _MANGLE_H_
/*
  header for 8.3 name mangling interface 
*/

struct mangle_fns {
	void (*reset)(void);
	BOOL (*is_mangled)(const char *s, const struct share_params *p);
	BOOL (*is_8_3)(const char *fname, BOOL check_case, BOOL allow_wildcards,
		       const struct share_params *p);
	BOOL (*check_cache)(char *s, size_t maxlen,
			    const struct share_params *p);
	void (*name_map)(char *OutName, BOOL need83, BOOL cache83,
			 int default_case,
			 const struct share_params *p);
};
#endif /* _MANGLE_H_ */
