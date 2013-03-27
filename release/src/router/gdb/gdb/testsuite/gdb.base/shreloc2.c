#if defined(_WIN32) || defined(__CYGWIN__)
# define ATTRIBUTES __attribute((__dllexport__))
#else
# define ATTRIBUTES
#endif

static int static_var_2;

ATTRIBUTES void fn_2 (int referenced) { static_var_2 = referenced; }
ATTRIBUTES int extern_var_2 = 0;
