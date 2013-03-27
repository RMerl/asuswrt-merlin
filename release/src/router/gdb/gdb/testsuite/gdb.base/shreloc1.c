#if defined(_WIN32) || defined(__CYGWIN__)
# define ATTRIBUTES __attribute((__dllexport__))
#else
# define ATTRIBUTES
#endif

static int static_var_1;

ATTRIBUTES void fn_1 (int referenced) { static_var_1 = referenced; }
ATTRIBUTES int extern_var_1 = 0;
