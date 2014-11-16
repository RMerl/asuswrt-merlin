/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* LIC: GPL */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if the setvbuf function takes the buffering type as its second
   argument and the buffer pointer as the third, as on System V
   before release 3.  */
/* #undef SETVBUF_REVERSED */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

#define HAVE_STRUCT_SOCKADDR_LL 1

/* The number of bytes in a unsigned int.  */
#define SIZEOF_UNSIGNED_INT 4

/* The number of bytes in a unsigned long.  */
#define SIZEOF_UNSIGNED_LONG 4

/* The number of bytes in a unsigned short.  */
#define SIZEOF_UNSIGNED_SHORT 2

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the <asm/types.h> header file.  */
#define HAVE_ASM_TYPES_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <getopt.h> header file.  */
#define HAVE_GETOPT_H 1

/* Define if you have the <linux/if_ether.h> header file.  */
#define HAVE_LINUX_IF_ETHER_H 1

/* Define if you have kernel-mode PPPoE in Linux file.  */
#define HAVE_LINUX_KERNEL_PPPOE 1

/* Define if you have the <linux/if_packet.h> header file.  */
#define HAVE_LINUX_IF_PACKET_H 1

/* Define if you have the <linux/if_pppox.h> header file.  */
#define HAVE_LINUX_IF_PPPOX_H 1

/* Define if you have the <net/bpf.h> header file.  */
#define HAVE_NET_BPF_H 1

/* Define if you have the <net/if_arp.h> header file.  */
#define HAVE_NET_IF_ARP_H 1

/* Define if you have the <net/ethernet.h> header file.  */
#define HAVE_NET_ETHERNET_H 1

/* Define if you have the <net/if.h> header file.  */
#define HAVE_NET_IF_H 1

/* Define if you have the <linux/if.h> header file.  */
#define HAVE_LINUX_IF_H 1

/* Define if you have the <net/if_dl.h> header file.  */
/* #undef HAVE_NET_IF_DL_H */

/* Define if you have the <net/if_ether.h> header file.  */
/* #undef HAVE_NET_IF_ETHER_H */

/* Define if you have the <net/if_types.h> header file.  */
/* #undef HAVE_NET_IF_TYPES_H */

/* Define if you have the <netinet/if_ether.h> header file.  */
#define HAVE_NETINET_IF_ETHER_H 1

/* Define if you have the <netpacket/packet.h> header file.  */
#define HAVE_NETPACKET_PACKET_H 1

/* Define if you have the <sys/cdefs.h> header file.  */
#define HAVE_SYS_CDEFS_H 1

/* Define if you have the <sys/dlpi.h> header file.  */
/* #undef HAVE_SYS_DLPI_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/uio.h> header file.  */
#define HAVE_SYS_UIO_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the N_HDLC line discipline in linux/termios.h */
#define HAVE_N_HDLC 1

/* Define if bitfields are packed in reverse order */
#define PACK_BITFIELDS_REVERSED 1

/* Define to include debugging code */
/* #undef DEBUGGING_ENABLED */

/* Solaris moans if we don't do this... */
#ifdef __sun
#define __EXTENSIONS__ 1
#endif
