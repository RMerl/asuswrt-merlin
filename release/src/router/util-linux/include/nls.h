#ifndef UTIL_LINUX_NLS_H
#define UTIL_LINUX_NLS_H

int main(int argc, char *argv[]);

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#else
# undef setlocale
# define setlocale(Category, Locale) /* empty */
#endif

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
# define P_(Singular, Plural, n) ngettext (Singular, Plural, n)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) (Text)
# define N_(Text) (Text)
# define P_(Singular, Plural, n) ((n) == 1 ? (Singular) : (Plural))
#endif

#ifdef HAVE_LANGINFO_H
# include <langinfo.h>
#else

typedef int nl_item;
extern char *langinfo_fallback(nl_item item);

# define nl_langinfo	langinfo_fallback

enum {
	CODESET = 1,
	RADIXCHAR,
	THOUSEP,
	D_T_FMT,
	D_FMT,
	T_FMT,
	T_FMT_AMPM,
	AM_STR,
	PM_STR,

	DAY_1,
	DAY_2,
	DAY_3,
	DAY_4,
	DAY_5,
	DAY_6,
	DAY_7,

	ABDAY_1,
	ABDAY_2,
	ABDAY_3,
	ABDAY_4,
	ABDAY_5,
	ABDAY_6,
	ABDAY_7,

	MON_1,
	MON_2,
	MON_3,
	MON_4,
	MON_5,
	MON_6,
	MON_7,
	MON_8,
	MON_9,
	MON_10,
	MON_11,
	MON_12,

	ABMON_1,
	ABMON_2,
	ABMON_3,
	ABMON_4,
	ABMON_5,
	ABMON_6,
	ABMON_7,
	ABMON_8,
	ABMON_9,
	ABMON_10,
	ABMON_11,
	ABMON_12,

	ERA_D_FMT,
	ERA_D_T_FMT,
	ERA_T_FMT,
	ALT_DIGITS,
	CRNCYSTR,
	YESEXPR,
	NOEXPR
};

#endif /* !HAVE_LANGINFO_H */

#endif /* UTIL_LINUX_NLS_H */
