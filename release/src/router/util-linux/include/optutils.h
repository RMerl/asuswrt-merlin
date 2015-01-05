#ifndef UTIL_LINUX_OPTUTILS_H
#define UTIL_LINUX_OPTUTILS_H

static inline const char *option_to_longopt(int c, const struct option *opts)
{
	const struct option *o;

	for (o = opts; o->name; o++)
		if (o->val == c)
			return o->name;
	return NULL;
}

#endif

