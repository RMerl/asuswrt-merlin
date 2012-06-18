#ifndef _IPTABLES_COMMON_H
#define _IPTABLES_COMMON_H
/* Shared definitions between ipv4 and ipv6. */

enum exittype {
	OTHER_PROBLEM = 1,
	PARAMETER_PROBLEM,
	VERSION_PROBLEM
};
extern void exit_printhelp(void) __attribute__((noreturn));
extern void exit_tryhelp(int) __attribute__((noreturn));
int check_inverse(const char option[], int *invert, int *optind, int argc);
extern int string_to_number(const char *, 
			    unsigned int, 
			    unsigned int,
			    unsigned int *);
extern int string_to_number_l(const char *, 
			    unsigned long int, 
			    unsigned long int,
			    unsigned long *);
extern int string_to_number_ll(const char *, 
			    unsigned long long int, 
			    unsigned long long int,
			    unsigned long long *);
extern int iptables_insmod(const char *modname, const char *modprobe);
void exit_error(enum exittype, char *, ...)__attribute__((noreturn,
							  format(printf,2,3)));
extern const char *program_name, *program_version;
extern char *lib_dir;

#ifdef NO_SHARED_LIBS
# ifdef _INIT
#  define _init _INIT
# endif
  extern void init_extensions(void);
#endif

#endif /*_IPTABLES_COMMON_H*/
