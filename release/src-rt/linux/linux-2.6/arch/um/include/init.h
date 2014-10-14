#ifndef _LINUX_UML_INIT_H
#define _LINUX_UML_INIT_H

/* These macros are used to mark some functions or
 * initialized data (doesn't apply to uninitialized data)
 * as `initialization' functions. The kernel can take this
 * as hint that the function is used only during the initialization
 * phase and free up used memory resources after
 *
 * Usage:
 * For functions:
 *
 * You should add __init immediately before the function name, like:
 *
 * static void __init initme(int x, int y)
 * {
 *    extern int z; z = x * y;
 * }
 *
 * If the function has a prototype somewhere, you can also add
 * __init between closing brace of the prototype and semicolon:
 *
 * extern int initialize_foobar_device(int, int, int) __init;
 *
 * For initialized data:
 * You should insert __initdata between the variable name and equal
 * sign followed by value, e.g.:
 *
 * static int init_variable __initdata = 0;
 * static char linux_logo[] __initdata = { 0x32, 0x36, ... };
 *
 * Don't forget to initialize data not at file scope, i.e. within a function,
 * as gcc otherwise puts the data into the bss section and not into the init
 * section.
 *
 * Also note, that this data cannot be "const".
 */

#ifndef _LINUX_INIT_H
typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

/* These are for everybody (although not all archs will actually
   discard it in modules) */
#define __init		__attribute__ ((__section__ (".init.text")))
#define __initdata	__attribute__ ((__section__ (".init.data")))
#define __exitdata	__attribute__ ((__section__(".exit.data")))
#define __exit_call	__attribute_used__ __attribute__ ((__section__ (".exitcall.exit")))

#ifdef MODULE
#define __exit		__attribute__ ((__section__(".exit.text")))
#else
#define __exit		__attribute_used__ __attribute__ ((__section__(".exit.text")))
#endif

#endif

#ifndef MODULE
struct uml_param {
        const char *str;
        int (*setup_func)(char *, int *);
};

extern initcall_t __uml_initcall_start, __uml_initcall_end;
extern initcall_t __uml_postsetup_start, __uml_postsetup_end;
extern const char *__uml_help_start, *__uml_help_end;
#endif

#define __uml_initcall(fn)					  	\
	static initcall_t __uml_initcall_##fn __uml_init_call = fn

#define __uml_exitcall(fn)						\
	static exitcall_t __uml_exitcall_##fn __uml_exit_call = fn

extern struct uml_param __uml_setup_start, __uml_setup_end;

#define __uml_postsetup(fn)						\
	static initcall_t __uml_postsetup_##fn __uml_postsetup_call = fn

#define __non_empty_string(dummyname,string)				\
	struct __uml_non_empty_string_struct_##dummyname		\
	{								\
		char _string[sizeof(string)-2];				\
	}

#ifndef MODULE
#define __uml_setup(str, fn, help...)					\
	__non_empty_string(fn ##_setup, str);				\
	__uml_help(fn, help);						\
	static char __uml_setup_str_##fn[] __initdata = str;		\
	static struct uml_param __uml_setup_##fn __uml_init_setup = { __uml_setup_str_##fn, fn }
#else
#define __uml_setup(str, fn, help...)					\

#endif

#define __uml_help(fn, help...)						\
	__non_empty_string(fn ##__help, help);				\
	static char __uml_help_str_##fn[] __initdata = help;		\
	static const char *__uml_help_##fn __uml_setup_help = __uml_help_str_##fn

/*
 * Mark functions and data as being only used at initialization
 * or exit time.
 */
#define __uml_init_setup	__attribute_used__ __attribute__ ((__section__ (".uml.setup.init")))
#define __uml_setup_help	__attribute_used__ __attribute__ ((__section__ (".uml.help.init")))
#define __uml_init_call		__attribute_used__ __attribute__ ((__section__ (".uml.initcall.init")))
#define __uml_postsetup_call	__attribute_used__ __attribute__ ((__section__ (".uml.postsetup.init")))
#define __uml_exit_call		__attribute_used__ __attribute__ ((__section__ (".uml.exitcall.exit")))

#ifndef __KERNEL__

#define __define_initcall(level,fn) \
	static initcall_t __initcall_##fn __attribute_used__ \
	__attribute__((__section__(".initcall" level ".init"))) = fn

/* Userspace initcalls shouldn't depend on anything in the kernel, so we'll
 * make them run first.
 */
#define __initcall(fn) __define_initcall("1", fn)

#define __exitcall(fn) static exitcall_t __exitcall_##fn __exit_call = fn

#define __init_call	__attribute_used__ __attribute__ ((__section__ (".initcall.init")))

#endif

#endif /* _LINUX_UML_INIT_H */

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
