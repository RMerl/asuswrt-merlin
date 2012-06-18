/* Support macros for making weak and strong aliases for symbols,
   and for using symbol sets and linker warnings with GNU ld.
   Copyright (C) 1995-1998,2000-2003,2004,2005,2006
	Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _LIBC_SYMBOLS_H
#define _LIBC_SYMBOLS_H	1

/* This is defined for the compilation of all C library code.  features.h
   tests this to avoid inclusion of stubs.h while compiling the library,
   before stubs.h has been generated.  Some library code that is shared
   with other packages also tests this symbol to see if it is being
   compiled as part of the C library.  We must define this before including
   config.h, because it makes some definitions conditional on whether libc
   itself is being compiled, or just some generator program.  */
#define _LIBC	1


/* This file's macros are included implicitly in the compilation of every
   file in the C library by -imacros.

   We include uClibc_arch_features.h which is defined by arch devs.
   It should define for us the following symbols:

   * HAVE_ASM_SET_DIRECTIVE if we have `.set B, A' instead of `A = B'.
   * ASM_GLOBAL_DIRECTIVE with `.globl' or `.global'.
   * ASM_TYPE_DIRECTIVE_PREFIX with `@' or `#' or whatever for .type,
     or leave it undefined if there is no .type directive.
   * HAVE_ELF if using ELF, which supports weak symbols using `.weak'.
   * HAVE_ASM_WEAK_DIRECTIVE if we have weak symbols using `.weak'.
   * HAVE_ASM_WEAKEXT_DIRECTIVE if we have weak symbols using `.weakext'.

   */

#include <bits/uClibc_arch_features.h>

/* Enable declarations of GNU extensions, since we are compiling them.  */
#define _GNU_SOURCE	1

/* Prepare for the case that `__builtin_expect' is not available.  */
#if defined __GNUC__ && __GNUC__ == 2 && __GNUC_MINOR__ < 96
# define __builtin_expect(x, expected_value) (x)
#endif
#ifndef likely
# define likely(x)	__builtin_expect((!!(x)),1)
#endif
#ifndef unlikely
# define unlikely(x)	__builtin_expect((!!(x)),0)
#endif
#ifndef __LINUX_COMPILER_H
# define __LINUX_COMPILER_H
#endif
#ifndef __cast__
# define __cast__(_to)
#endif

#define attribute_unused __attribute__ ((unused))

#if defined __GNUC__ || defined __ICC
# define attribute_noreturn __attribute__ ((__noreturn__))
#else
# define attribute_noreturn
#endif

#ifndef NOT_IN_libc
# define IS_IN_libc 1
#endif

/* Indirect stringification.  Doing two levels allows
 * the parameter to be a macro itself.
 */
#define __stringify_1(x)    #x
#define __stringify(x)      __stringify_1(x)

#ifdef __UCLIBC_HAVE_ASM_SET_DIRECTIVE__
# define HAVE_ASM_SET_DIRECTIVE
#else
# undef HAVE_ASM_SET_DIRECTIVE
#endif

#ifdef __UCLIBC_ASM_GLOBAL_DIRECTIVE__
# define ASM_GLOBAL_DIRECTIVE __UCLIBC_ASM_GLOBAL_DIRECTIVE__
#else
# define ASM_GLOBAL_DIRECTIVE .global
#endif

#ifdef __UCLIBC_HAVE_ASM_WEAK_DIRECTIVE__
# define HAVE_ASM_WEAK_DIRECTIVE
#else
# undef HAVE_ASM_WEAK_DIRECTIVE
#endif

#ifdef __UCLIBC_HAVE_ASM_WEAKEXT_DIRECTIVE__
# define HAVE_ASM_WEAKEXT_DIRECTIVE
#else
# undef HAVE_ASM_WEAKEXT_DIRECTIVE
#endif

#ifdef __UCLIBC_HAVE_ASM_GLOBAL_DOT_NAME__
# define HAVE_ASM_GLOBAL_DOT_NAME
#else
# undef HAVE_ASM_GLOBAL_DOT_NAME
#endif

#if defined HAVE_ASM_WEAK_DIRECTIVE || defined HAVE_ASM_WEAKEXT_DIRECTIVE
# define HAVE_WEAK_SYMBOLS
#endif

#undef C_SYMBOL_NAME
#ifndef C_SYMBOL_NAME
# ifndef __UCLIBC_UNDERSCORES__
#  define C_SYMBOL_NAME(name) name
# else
#  define C_SYMBOL_NAME(name) _##name
# endif
#endif

#ifdef __UCLIBC_ASM_LINE_SEP__
# define ASM_LINE_SEP __UCLIBC_ASM_LINE_SEP__
#else
# define ASM_LINE_SEP ;
#endif

#ifdef HAVE_ASM_GLOBAL_DOT_NAME
# ifndef C_SYMBOL_DOT_NAME
#  if defined __GNUC__ && defined __GNUC_MINOR__ \
      && (__GNUC__ << 16) + __GNUC_MINOR__ >= (3 << 16) + 1
#   define C_SYMBOL_DOT_NAME(name) .name
#  else
#   define C_SYMBOL_DOT_NAME(name) .##name
#  endif
# endif
#endif

#ifndef __ASSEMBLER__
/* GCC understands weak symbols and aliases; use its interface where
   possible, instead of embedded assembly language.  */

/* Define ALIASNAME as a strong alias for NAME.  */
# define strong_alias(name, aliasname) _strong_alias(name, aliasname)
# define _strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

/* This comes between the return type and function name in
   a function definition to make that definition weak.  */
# define weak_function __attribute__ ((weak))
# define weak_const_function __attribute__ ((weak, __const__))

# ifdef HAVE_WEAK_SYMBOLS

/* Define ALIASNAME as a weak alias for NAME.
   If weak aliases are not available, this defines a strong alias.  */
#  define weak_alias(name, aliasname) _weak_alias (name, aliasname)
#  define _weak_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));

/* Declare SYMBOL as weak undefined symbol (resolved to 0 if not defined).  */
#  define weak_extern(symbol) _weak_extern (weak symbol)
#  define _weak_extern(expr) _Pragma (#expr)

# else

#  define weak_alias(name, aliasname) strong_alias(name, aliasname)
#  define weak_extern(symbol) /* Nothing. */

# endif

#else /* __ASSEMBLER__ */

# ifdef HAVE_ASM_SET_DIRECTIVE
#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .set C_SYMBOL_NAME (alias),C_SYMBOL_NAME (original) ASM_LINE_SEP	\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP		\
  .set C_SYMBOL_DOT_NAME (alias),C_SYMBOL_DOT_NAME (original)
#   define strong_data_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .set C_SYMBOL_NAME (alias),C_SYMBOL_NAME (original)
#  else
#   define strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .set C_SYMBOL_NAME (alias),C_SYMBOL_NAME (original)
#   define strong_data_alias(original, alias) strong_alias(original, alias)
#  endif
# else
#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original) ASM_LINE_SEP		\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP		\
  C_SYMBOL_DOT_NAME (alias) = C_SYMBOL_DOT_NAME (original)
#   define strong_data_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original)
#  else
#   define strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original)
#   define strong_data_alias(original, alias) strong_alias(original, alias)
#  endif
# endif

# ifdef HAVE_WEAK_SYMBOLS
#  ifdef HAVE_ASM_WEAKEXT_DIRECTIVE
#   ifdef HAVE_ASM_GLOBAL_DOT_NAME
#    define weak_alias(original, alias)					\
  .weakext C_SYMBOL_NAME (alias), C_SYMBOL_NAME (original) ASM_LINE_SEP \
  .weakext C_SYMBOL_DOT_NAME (alias), C_SYMBOL_DOT_NAME (original)
#   else
#    define weak_alias(original, alias)					\
  .weakext C_SYMBOL_NAME (alias), C_SYMBOL_NAME (original)
#   endif
#   define weak_extern(symbol)						\
  .weakext C_SYMBOL_NAME (symbol)

#  else /* ! HAVE_ASM_WEAKEXT_DIRECTIVE */

#   ifdef HAVE_ASM_SET_DIRECTIVE
#    ifdef HAVE_ASM_GLOBAL_DOT_NAME
#     define weak_alias(original, alias)				\
  .weak C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  .set C_SYMBOL_NAME (alias), C_SYMBOL_NAME (original) ASM_LINE_SEP	\
  .weak C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP				\
  .set C_SYMBOL_DOT_NAME (alias), C_SYMBOL_DOT_NAME (original)
#    else
#     define weak_alias(original, alias)				\
  .weak C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  .set C_SYMBOL_NAME (alias), C_SYMBOL_NAME (original)
#    endif
#   else /* ! HAVE_ASM_SET_DIRECTIVE */
#    ifdef HAVE_ASM_GLOBAL_DOT_NAME
#     define weak_alias(original, alias)				\
  .weak C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original) ASM_LINE_SEP		\
  .weak C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP				\
  C_SYMBOL_DOT_NAME (alias) = C_SYMBOL_DOT_NAME (original)
#    else
#     define weak_alias(original, alias)				\
  .weak C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original)
#    endif
#   endif
#   define weak_extern(symbol)						\
  .weak C_SYMBOL_NAME (symbol)

#  endif /* ! HAVE_ASM_WEAKEXT_DIRECTIVE */

# else /* ! HAVE_WEAK_SYMBOLS */

#  define weak_alias(original, alias) strong_alias(original, alias)
#  define weak_extern(symbol) /* Nothing */
# endif /* ! HAVE_WEAK_SYMBOLS */

#endif /* __ASSEMBLER__ */

/* On some platforms we can make internal function calls (i.e., calls of
   functions not exported) a bit faster by using a different calling
   convention.  */
#ifndef internal_function
# define internal_function	/* empty */
#endif

/* We want the .gnu.warning.SYMBOL section to be unallocated.  */
#define __make_section_unallocated(section_string)	\
  __asm__ (".section " section_string "\n\t.previous");

/* Tacking on "\n#APP\n\t#" to the section name makes gcc put it's bogus
   section attributes on what looks like a comment to the assembler.  */
#ifdef __sparc__ /* HAVE_SECTION_QUOTES */
# define __sec_comment "\"\n#APP\n\t#\""
#else
# define __sec_comment "\n#APP\n\t#"
#endif

/* When a reference to SYMBOL is encountered, the linker will emit a
   warning message MSG.  */
#define link_warning(symbol, msg) \
  __make_section_unallocated (".gnu.warning." #symbol) \
  static const char __evoke_link_warning_##symbol[]	\
    __attribute__ ((used, section (".gnu.warning." #symbol __sec_comment))) \
    = msg;

/* Handling on non-exported internal names.  We have to do this only
   for shared code.  */
#ifdef SHARED
# define INTUSE(name) name##_internal
# define INTDEF(name) strong_alias (name, name##_internal)
# define INTVARDEF(name) \
  _INTVARDEF (name, name##_internal)
# if defined HAVE_VISIBILITY_ATTRIBUTE
#  define _INTVARDEF(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name), \
						   visibility ("hidden")));
# else
#  define _INTVARDEF(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));
# endif
# define INTDEF2(name, newname) strong_alias (name, newname##_internal)
# define INTVARDEF2(name, newname) _INTVARDEF (name, newname##_internal)
#else
# define INTUSE(name) name
# define INTDEF(name)
# define INTVARDEF(name)
# define INTDEF2(name, newname)
# define INTVARDEF2(name, newname)
#endif

/* The following macros are used for PLT bypassing within libc.so
   (and if needed other libraries similarly).
   First of all, you need to have the function prototyped somewhere,
   say in foo/foo.h:

   int foo (int __bar);

   If calls to foo within libc.so should always go to foo defined in libc.so,
   then in include/foo.h you add:

   libc_hidden_proto (foo)

   line and after the foo function definition:

   int foo (int __bar)
   {
     return __bar;
   }
   libc_hidden_def (foo)

   or

   int foo (int __bar)
   {
     return __bar;
   }
   libc_hidden_weak (foo)

   Similarly for global data.  If references to foo within libc.so should
   always go to foo defined in libc.so, then in include/foo.h you add:

   libc_hidden_proto (foo)

   line and after foo's definition:

   int foo = INITIAL_FOO_VALUE;
   libc_hidden_data_def (foo)

   or

   int foo = INITIAL_FOO_VALUE;
   libc_hidden_data_weak (foo)

   If foo is normally just an alias (strong or weak) to some other function,
   you should use the normal strong_alias first, then add libc_hidden_def
   or libc_hidden_weak:

   int baz (int __bar)
   {
     return __bar;
   }
   strong_alias (baz, foo)
   libc_hidden_weak (foo)

   If the function should be internal to multiple objects, say ld.so and
   libc.so, the best way is to use:

   #if !defined NOT_IN_libc || defined IS_IN_rtld
   hidden_proto (foo)
   #endif

   in include/foo.h and the normal macros at all function definitions
   depending on what DSO they belong to.

   If versioned_symbol macro is used to define foo,
   libc_hidden_ver macro should be used, as in:

   int __real_foo (int __bar)
   {
     return __bar;
   }
   versioned_symbol (libc, __real_foo, foo, GLIBC_2_1);
   libc_hidden_ver (__real_foo, foo)  */

/* uClibc specific (the above comment was copied from glibc):
 * a. when ppc64 will be supported, we need changes to support:
 * strong_data_alias (used by asm hidden_data_def)
 * b. libc_hidden_proto(foo) should be added after the header having foo's prototype
 * or after extern foo... to all source files that should use the internal version
 * of foo within libc, even to the file defining foo itself, libc_hidden_def does
 * not hide __GI_foo itself, although the name suggests it (hiding is done exclusively
 * by libc_hidden_proto). The reasoning to have it after the header w/ foo's prototype is
 * to get first the __REDIRECT from original header and then create the __GI_foo alias
 * c. no versioning support, hidden[_data]_ver are noop
 * d. hidden_def() in asm is _hidden_strong_alias (not strong_alias) */

/* Arrange to hide uClibc internals */
#if (defined __GNUC__ && \
  (defined __GNUC_MINOR__ && ( __GNUC__ >= 3 && __GNUC_MINOR__ >= 3 ) \
   || __GNUC__ >= 4)) || defined __ICC
# define attribute_hidden __attribute__ ((visibility ("hidden")))
# define __hidden_proto_hiddenattr(attrs...) __attribute__ ((visibility ("hidden"), ##attrs))
#else
# define attribute_hidden
# define __hidden_proto_hiddenattr(attrs...)
#endif

#if /*!defined STATIC &&*/ !defined __BCC__
# ifndef __ASSEMBLER__
#  define hidden_proto(name, attrs...) __hidden_proto (name, __GI_##name, ##attrs)
#  define __hidden_proto(name, internal, attrs...) \
   extern __typeof (name) name __asm__ (__hidden_asmname (#internal)) \
   __hidden_proto_hiddenattr (attrs);
#  define __hidden_asmname(name) __hidden_asmname1 (__USER_LABEL_PREFIX__, name)
#  define __hidden_asmname1(prefix, name) __hidden_asmname2(prefix, name)
#  define __hidden_asmname2(prefix, name) #prefix name
#  define __hidden_ver1(local, internal, name) \
   extern __typeof (name) __EI_##name __asm__(__hidden_asmname (#internal)); \
   extern __typeof (name) __EI_##name __attribute__((alias (__hidden_asmname1 (,#local))))
#  define hidden_ver(local, name)	__hidden_ver1(local, __GI_##name, name);
#  define hidden_data_ver(local, name)	hidden_ver(local, name)
#  define hidden_def(name)		__hidden_ver1(__GI_##name, name, name);
#  define hidden_data_def(name)		hidden_def(name)
#  define hidden_weak(name)	\
	__hidden_ver1(__GI_##name, name, name) __attribute__((weak));
#  define hidden_data_weak(name)	hidden_weak(name)

# else /* __ASSEMBLER__ */
# ifdef HAVE_ASM_SET_DIRECTIVE
#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define _hidden_strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  .set C_SYMBOL_NAME (alias),C_SYMBOL_NAME (original) ASM_LINE_SEP	\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP			\
  .set C_SYMBOL_DOT_NAME (alias),C_SYMBOL_DOT_NAME (original)
#  else
#   define _hidden_strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  .set C_SYMBOL_NAME (alias),C_SYMBOL_NAME (original)
#  endif
# else
#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define _hidden_strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original) ASM_LINE_SEP		\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP			\
  C_SYMBOL_DOT_NAME (alias) = C_SYMBOL_DOT_NAME (original)
#  else
#   define _hidden_strong_alias(original, alias)				\
  ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME (alias) ASM_LINE_SEP		\
  .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
  C_SYMBOL_NAME (alias) = C_SYMBOL_NAME (original)
#  endif
# endif

#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define _hidden_weak_alias(original, alias)				\
     .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
     .hidden C_SYMBOL_DOT_NAME (alias) ASM_LINE_SEP			\
     weak_alias(original, alias)
#  else
#   define _hidden_weak_alias(original, alias)				\
     .hidden C_SYMBOL_NAME (alias) ASM_LINE_SEP				\
     weak_alias(original, alias)
#  endif

/* For assembly, we need to do the opposite of what we do in C:
   in assembly gcc __REDIRECT stuff is not in place, so functions
   are defined by its normal name and we need to create the
   __GI_* alias to it, in C __REDIRECT causes the function definition
   to use __GI_* name and we need to add alias to the real name.
   There is no reason to use hidden_weak over hidden_def in assembly,
   but we provide it for consistency with the C usage.
   hidden_proto doesn't make sense for assembly but the equivalent
   is to call via the HIDDEN_JUMPTARGET macro instead of JUMPTARGET.  */
#  define hidden_def(name)	_hidden_strong_alias (name, __GI_##name)
#  define hidden_weak(name)	_hidden_weak_alias (name, __GI_##name)
#  define hidden_ver(local, name) strong_alias (local, __GI_##name)
#  define hidden_data_def(name)	_hidden_strong_alias (name, __GI_##name)
#  define hidden_data_weak(name)	_hidden_weak_alias (name, __GI_##name)
#  define hidden_data_ver(local, name) strong_data_alias (local, __GI_##name)
#  ifdef HAVE_ASM_GLOBAL_DOT_NAME
#   define HIDDEN_JUMPTARGET(name) .__GI_##name
#  else
#   define HIDDEN_JUMPTARGET(name) __GI_##name
#  endif
# endif /* __ASSEMBLER__ */
#else /* SHARED */
# ifndef __ASSEMBLER__
#  define hidden_proto(name, attrs...)
# else
#  define HIDDEN_JUMPTARGET(name) name
# endif /* Not  __ASSEMBLER__ */
# define hidden_weak(name)
# define hidden_def(name)
# define hidden_ver(local, name)
# define hidden_data_weak(name)
# define hidden_data_def(name)
# define hidden_data_ver(local, name)
#endif /* SHARED */

/* uClibc does not support versioning yet. */
#define versioned_symbol(lib, local, symbol, version) /* weak_alias(local, symbol) */
#undef hidden_ver
#define hidden_ver(local, name) /* strong_alias(local, __GI_##name) */
#undef hidden_data_ver
#define hidden_data_ver(local, name) /* strong_alias(local,__GI_##name) */

#if !defined NOT_IN_libc
# define libc_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libc_hidden_def(name) hidden_def (name)
# define libc_hidden_weak(name) hidden_weak (name)
# define libc_hidden_ver(local, name) hidden_ver (local, name)
# define libc_hidden_data_def(name) hidden_data_def (name)
# define libc_hidden_data_weak(name) hidden_data_weak (name)
# define libc_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libc_hidden_proto(name, attrs...)
# define libc_hidden_def(name)
# define libc_hidden_weak(name)
# define libc_hidden_ver(local, name)
# define libc_hidden_data_def(name)
# define libc_hidden_data_weak(name)
# define libc_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_rtld
# define rtld_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define rtld_hidden_def(name) hidden_def (name)
# define rtld_hidden_weak(name) hidden_weak (name)
# define rtld_hidden_ver(local, name) hidden_ver (local, name)
# define rtld_hidden_data_def(name) hidden_data_def (name)
# define rtld_hidden_data_weak(name) hidden_data_weak (name)
# define rtld_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define rtld_hidden_proto(name, attrs...)
# define rtld_hidden_def(name)
# define rtld_hidden_weak(name)
# define rtld_hidden_ver(local, name)
# define rtld_hidden_data_def(name)
# define rtld_hidden_data_weak(name)
# define rtld_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libm
# define libm_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libm_hidden_def(name) hidden_def (name)
# define libm_hidden_weak(name) hidden_weak (name)
# define libm_hidden_ver(local, name) hidden_ver (local, name)
# define libm_hidden_data_def(name) hidden_data_def (name)
# define libm_hidden_data_weak(name) hidden_data_weak (name)
# define libm_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libm_hidden_proto(name, attrs...)
# define libm_hidden_def(name)
# define libm_hidden_weak(name)
# define libm_hidden_ver(local, name)
# define libm_hidden_data_def(name)
# define libm_hidden_data_weak(name)
# define libm_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libresolv
# define libresolv_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libresolv_hidden_def(name) hidden_def (name)
# define libresolv_hidden_weak(name) hidden_weak (name)
# define libresolv_hidden_ver(local, name) hidden_ver (local, name)
# define libresolv_hidden_data_def(name) hidden_data_def (name)
# define libresolv_hidden_data_weak(name) hidden_data_weak (name)
# define libresolv_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libresolv_hidden_proto(name, attrs...)
# define libresolv_hidden_def(name)
# define libresolv_hidden_weak(name)
# define libresolv_hidden_ver(local, name)
# define libresolv_hidden_data_def(name)
# define libresolv_hidden_data_weak(name)
# define libresolv_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_librt
# define librt_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define librt_hidden_def(name) hidden_def (name)
# define librt_hidden_weak(name) hidden_weak (name)
# define librt_hidden_ver(local, name) hidden_ver (local, name)
# define librt_hidden_data_def(name) hidden_data_def (name)
# define librt_hidden_data_weak(name) hidden_data_weak (name)
# define librt_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define librt_hidden_proto(name, attrs...)
# define librt_hidden_def(name)
# define librt_hidden_weak(name)
# define librt_hidden_ver(local, name)
# define librt_hidden_data_def(name)
# define librt_hidden_data_weak(name)
# define librt_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libdl
# define libdl_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libdl_hidden_def(name) hidden_def (name)
# define libdl_hidden_weak(name) hidden_weak (name)
# define libdl_hidden_ver(local, name) hidden_ver (local, name)
# define libdl_hidden_data_def(name) hidden_data_def (name)
# define libdl_hidden_data_weak(name) hidden_data_weak (name)
# define libdl_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libdl_hidden_proto(name, attrs...)
# define libdl_hidden_def(name)
# define libdl_hidden_weak(name)
# define libdl_hidden_ver(local, name)
# define libdl_hidden_data_def(name)
# define libdl_hidden_data_weak(name)
# define libdl_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libintl
# define libintl_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libintl_hidden_def(name) hidden_def (name)
# define libintl_hidden_weak(name) hidden_weak (name)
# define libintl_hidden_ver(local, name) hidden_ver (local, name)
# define libintl_hidden_data_def(name) hidden_data_def (name)
# define libintl_hidden_data_weak(name) hidden_data_weak (name)
# define libintl_hidden_data_ver(local, name) hidden_data_ver(local, name)
#else
# define libintl_hidden_proto(name, attrs...)
# define libintl_hidden_def(name)
# define libintl_hidden_weak(name)
# define libintl_hidden_ver(local, name)
# define libintl_hidden_data_def(name)
# define libintl_hidden_data_weak(name)
# define libintl_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libnsl
# define libnsl_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libnsl_hidden_def(name) hidden_def (name)
# define libnsl_hidden_weak(name) hidden_weak (name)
# define libnsl_hidden_ver(local, name) hidden_ver (local, name)
# define libnsl_hidden_data_def(name) hidden_data_def (name)
# define libnsl_hidden_data_weak(name) hidden_data_weak (name)
# define libnsl_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libnsl_hidden_proto(name, attrs...)
# define libnsl_hidden_def(name)
# define libnsl_hidden_weak(name)
# define libnsl_hidden_ver(local, name)
# define libnsl_hidden_data_def(name)
# define libnsl_hidden_data_weak(name)
# define libnsl_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libutil
# define libutil_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libutil_hidden_def(name) hidden_def (name)
# define libutil_hidden_weak(name) hidden_weak (name)
# define libutil_hidden_ver(local, name) hidden_ver (local, name)
# define libutil_hidden_data_def(name) hidden_data_def (name)
# define libutil_hidden_data_weak(name) hidden_data_weak (name)
# define libutil_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libutil_hidden_proto(name, attrs...)
# define libutil_hidden_def(name)
# define libutil_hidden_weak(name)
# define libutil_hidden_ver(local, name)
# define libutil_hidden_data_def(name)
# define libutil_hidden_data_weak(name)
# define libutil_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libcrypt
# define libcrypt_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libcrypt_hidden_def(name) hidden_def (name)
# define libcrypt_hidden_weak(name) hidden_weak (name)
# define libcrypt_hidden_ver(local, name) hidden_ver (local, name)
# define libcrypt_hidden_data_def(name) hidden_data_def (name)
# define libcrypt_hidden_data_weak(name) hidden_data_weak (name)
# define libcrypt_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libcrypt_hidden_proto(name, attrs...)
# define libcrypt_hidden_def(name)
# define libcrypt_hidden_weak(name)
# define libcrypt_hidden_ver(local, name)
# define libcrypt_hidden_data_def(name)
# define libcrypt_hidden_data_weak(name)
# define libcrypt_hidden_data_ver(local, name)
#endif

#if defined NOT_IN_libc && defined IS_IN_libpthread
# define libpthread_hidden_proto(name, attrs...) hidden_proto (name, ##attrs)
# define libpthread_hidden_def(name) hidden_def (name)
# define libpthread_hidden_weak(name) hidden_weak (name)
# define libpthread_hidden_ver(local, name) hidden_ver (local, name)
# define libpthread_hidden_data_def(name) hidden_data_def (name)
# define libpthread_hidden_data_weak(name) hidden_data_weak (name)
# define libpthread_hidden_data_ver(local, name) hidden_data_ver (local, name)
#else
# define libpthread_hidden_proto(name, attrs...)
# define libpthread_hidden_def(name)
# define libpthread_hidden_weak(name)
# define libpthread_hidden_ver(local, name)
# define libpthread_hidden_data_def(name)
# define libpthread_hidden_data_weak(name)
# define libpthread_hidden_data_ver(local, name)
#endif

#endif /* libc-symbols.h */
