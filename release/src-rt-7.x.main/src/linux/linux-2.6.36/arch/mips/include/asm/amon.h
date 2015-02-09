/*
 * Amon support
 */

int amon_cpu_avail(int);
void amon_cpu_start(int, unsigned long, unsigned long,
		    unsigned long, unsigned long);
void amon_cpu_dead(void);
