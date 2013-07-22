#ifndef _LINUX_REBOOT_H
#define _LINUX_REBOOT_H

/*
 * Magic values required to use _reboot() system call.
 */

#define	LINUX_REBOOT_MAGIC1	0xfee1dead
#define	LINUX_REBOOT_MAGIC2	672274793
#define	LINUX_REBOOT_MAGIC2A	85072278
#define	LINUX_REBOOT_MAGIC2B	369367448


/*
 * Commands accepted by the _reboot() system call.
 *
 * RESTART     Restart system using default command and mode.
 * HALT        Stop OS and give system control to ROM monitor, if any.
 * CAD_ON      Ctrl-Alt-Del sequence causes RESTART command.
 * CAD_OFF     Ctrl-Alt-Del sequence sends SIGINT to init task.
 * POWER_OFF   Stop OS and remove all power from system, if possible.
 * RESTART2    Restart system using given command string.
 */

#define	LINUX_REBOOT_CMD_RESTART	0x01234567
#define	LINUX_REBOOT_CMD_HALT		0xCDEF0123
#define	LINUX_REBOOT_CMD_CAD_ON		0x89ABCDEF
#define	LINUX_REBOOT_CMD_CAD_OFF	0x00000000
#define	LINUX_REBOOT_CMD_POWER_OFF	0x4321FEDC
#define	LINUX_REBOOT_CMD_RESTART2	0xA1B2C3D4

/* Including <unistd.h> makes sure that on a glibc system
   <features.h> is included, which again defines __GLIBC__ */
#include <unistd.h>
#include "linux_reboot.h"

#define USE_LIBC

#ifdef USE_LIBC

/* libc version */
#if defined __GLIBC__ && __GLIBC__ >= 2
#  include <sys/reboot.h>
#  define REBOOT(cmd) reboot(cmd)
#else
extern int reboot(int, int, int);
#  define REBOOT(cmd) reboot(LINUX_REBOOT_MAGIC1,LINUX_REBOOT_MAGIC2,(cmd))
#endif
static inline int my_reboot(int cmd) {
	return REBOOT(cmd);
}

#else /* no USE_LIBC */

/* direct syscall version */
#include <linux/unistd.h>

#ifdef _syscall3
_syscall3(int,  reboot,  int,  magic, int, magic_too, int, cmd);
#else
/* Let us hope we have a 3-argument reboot here */
extern int reboot(int, int, int);
#endif

static inline int my_reboot(int cmd) {
	return reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, cmd);
}

#endif


#endif /*  _LINUX_REBOOT_H  */
