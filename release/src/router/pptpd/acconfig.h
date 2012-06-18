/*
 * acconfig.h
 *
 * Additional autoconf defines for this program.
 *
 * $Id: acconfig.h,v 1.6 2005/12/29 01:21:09 quozl Exp $
 */

/* Use BSD User land PPP? */
#undef BSDUSER_PPP

/* Use SLIRP? */
#undef SLIRP

/* Enable Broadcast Relay? */
#undef BCRELAY

/* Work as a PNS rather than a PAC? */
#undef PNS_MODE

/* Communicate between control and manager processes with a pipe */
#undef CTRL_MGR_PIPE

/* Where is my pppd? */
#undef PPP_BINARY

/* Syslog Facility to use?  See openlog(3). */
#undef PPTP_FACILITY

/* Have libwrap? */
#undef HAVE_LIBWRAP

/* Package name */
#undef PACKAGE

/* Version */
#undef VERSION

/* Just #define to int if it's not defined */
#undef socklen_t

/* These would be better as typedefs, but... */
#undef u_int8_t
#undef u_int16_t
#undef u_int32_t

/* And the signed size_t */
/* (normal size_t is done by standard autoconf) */
#undef ssize_t

/* Define if you have an openpty() (non-standard check) */
#undef HAVE_OPENPTY
