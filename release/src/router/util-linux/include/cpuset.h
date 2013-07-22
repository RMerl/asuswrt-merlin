#ifndef UTIL_LINUX_CPUSET_H
#define UTIL_LINUX_CPUSET_H

#include <sched.h>

/*
 * Fallback for old or obscure libcs without dynamically allocated cpusets
 *
 * The following macros are based on code from glibc.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#if !HAVE_DECL_CPU_ALLOC

# define CPU_ZERO_S(setsize, cpusetp) \
  do {									      \
    size_t __i;								      \
    size_t __imax = (setsize) / sizeof (__cpu_mask);			      \
    __cpu_mask *__bits = (cpusetp)->__bits;				      \
    for (__i = 0; __i < __imax; ++__i)					      \
      __bits[__i] = 0;							      \
  } while (0)

# define CPU_SET_S(cpu, setsize, cpusetp) \
   ({ size_t __cpu = (cpu);						      \
      __cpu < 8 * (setsize)						      \
      ? (((__cpu_mask *) ((cpusetp)->__bits))[__CPUELT (__cpu)]		      \
	 |= __CPUMASK (__cpu))						      \
      : 0; })

# define CPU_ISSET_S(cpu, setsize, cpusetp) \
   ({ size_t __cpu = (cpu);						      \
      __cpu < 8 * (setsize)						      \
      ? ((((__cpu_mask *) ((cpusetp)->__bits))[__CPUELT (__cpu)]	      \
	  & __CPUMASK (__cpu))) != 0					      \
      : 0; })

# define CPU_EQUAL_S(setsize, cpusetp1, cpusetp2) \
   ({ __cpu_mask *__arr1 = (cpusetp1)->__bits;				      \
      __cpu_mask *__arr2 = (cpusetp2)->__bits;				      \
      size_t __imax = (setsize) / sizeof (__cpu_mask);			      \
      size_t __i;							      \
      for (__i = 0; __i < __imax; ++__i)				      \
	if (__arr1[__i] != __arr2[__i])					      \
	  break;							      \
      __i == __imax; })

extern int __cpuset_count_s(size_t setsize, const cpu_set_t *set);
# define CPU_COUNT_S(setsize, cpusetp)	__cpuset_count_s(setsize, cpusetp)

# define CPU_ALLOC_SIZE(count) \
	  ((((count) + __NCPUBITS - 1) / __NCPUBITS) * sizeof (__cpu_mask))
# define CPU_ALLOC(count)	(malloc(CPU_ALLOC_SIZE(count)))
# define CPU_FREE(cpuset)	(free(cpuset))

#endif /* !HAVE_DECL_CPU_ALLOC */


#define cpuset_nbits(setsize)	(8 * (setsize))

extern int get_max_number_of_cpus(void);

extern cpu_set_t *cpuset_alloc(int ncpus, size_t *setsize, size_t *nbits);
extern void cpuset_free(cpu_set_t *set);

extern char *cpulist_create(char *str, size_t len, cpu_set_t *set, size_t setsize);
extern int cpulist_parse(const char *str, cpu_set_t *set, size_t setsize);

extern char *cpumask_create(char *str, size_t len, cpu_set_t *set, size_t setsize);
extern int cpumask_parse(const char *str, cpu_set_t *set, size_t setsize);

#endif /* UTIL_LINUX_CPUSET_H */
