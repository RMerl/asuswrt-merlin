/*
 * defaults.h
 *
 * This file contains some tuneable parameters, most of which can be overriden
 * at run-time.
 *
 * $Id: defaults.h,v 1.9 2006/09/04 23:17:25 quozl Exp $
 */

#ifndef _PPTPD_DEFAULTS_H
#define _PPTPD_DEFAULTS_H

/* Definitions for true and false */

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

/* String sizes for the config file */

#define MAX_CONFIG_FILENAME_SIZE	256
#define MAX_CONFIG_STRING_SIZE		512

/* For IP parser */

#define LOCAL 0
#define REMOTE 1

/* Default configuration values, mostly configurable */

#define CONNECTIONS_DEFAULT		100
#define DEFAULT_LOCAL_IP_LIST		"192.168.0.1-100"
#define DEFAULT_REMOTE_IP_LIST		"192.168.1.1-100"

#define MAX_CALLS_PER_TCP_LINK		128

#ifdef PNS_MODE
#define MAX_CALLS			60
#endif

#define PPP_SPEED_DEFAULT		"115200"
#if EMBED
#define PPTPD_CONFIG_FILE_DEFAULT	"/etc/config/pptpd.conf"
#else
#define PPTPD_CONFIG_FILE_DEFAULT	"/etc/pptpd.conf"
#endif
#define PIDFILE_DEFAULT			"/var/run/pptpd.pid"

#define STIMEOUT_DEFAULT		10 /* seconds */
#define PTIMEOUT_DEFAULT		1000 /* msec */

/* Location of binaries */

#if 1
#define PPTP_CTRL_BIN                   "/usr/sbin/pptpctrl"
#define PPTPD_BIN                       "/usr/sbin/pptpd"
#define BCRELAY_BIN                     "/usr/sbin/bcrelay"
#else
#define PPTP_CTRL_BIN			SBINDIR "/pptpctrl"
#define PPTPD_BIN			SBINDIR "/pptpd"
#define BCRELAY_BIN			SBINDIR "/bcrelay"
#endif

/* Parameters permitted in the config file */

#define CONNECTIONS_KEYWORD		"connections"
#define SPEED_KEYWORD			"speed"
#define PPPD_OPTION_KEYWORD		"option"
#define DEBUG_KEYWORD			"debug"
#ifdef BCRELAY
#define BCRELAY_KEYWORD			"bcrelay"
#endif
#define LOCALIP_KEYWORD			"localip"
#define REMOTEIP_KEYWORD		"remoteip"
#define LISTEN_KEYWORD			"listen"
#define PIDFILE_KEYWORD			"pidfile"
#define STIMEOUT_KEYWORD		"stimeout"
#define PTIMEOUT_KEYWORD		"ptimeout"
#define NOIPPARAM_KEYWORD		"noipparam"
#define PPP_BINARY_KEYWORD		"ppp"
#define LOGWTMP_KEYWORD			"logwtmp"
#define DELEGATE_KEYWORD		"delegate"
#define KEEP_KEYWORD		 	"keep"

#endif	/* !_PPTPD_DEFAULTS_H */
