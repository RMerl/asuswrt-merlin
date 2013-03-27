#if defined(_WIN32) || defined(__CYGWIN__)
# define ATTRIBUTES __attribute((__dllimport__))
#else
# define ATTRIBUTES
#endif

extern ATTRIBUTES void fn_1 (int);
extern ATTRIBUTES void fn_2 (int);
extern ATTRIBUTES int extern_var_1;
extern ATTRIBUTES int extern_var_2;

int main ()
{
  fn_1 (extern_var_1);
  fn_2 (extern_var_2);

  return 0;
}
