/*	$KAME: control.h,v 1.7 2005/01/12 06:06:11 suz Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __sun__
#ifndef	U_INT16_T_DEFINED
#define	U_INT16_T_DEFINED
typedef uint16_t u_int16_t;
#endif
#ifndef	U_INT32_T_DEFINED
#define	U_INT32_T_DEFINED
typedef uint32_t u_int32_t;
#endif
#endif

#define DEFAULT_SERVER_CONTROL_ADDR "::1" /* default IPv6 address for server
					   * control socket */
#define DEFAULT_SERVER_CONTROL_PORT "5547" /* default TCP port for server
					    * control socket */
#define DEFAULT_CLIENT_CONTROL_ADDR "::1" /* default IPv6 address for client
					   * control socket */
#define DEFAULT_CLIENT_CONTROL_PORT "5546" /* default TCP port for client
					    * control socket */

#define DHCP6CTL_VERSION 0

/* control commands */
#define DHCP6CTL_COMMAND_RELOAD 1
#define DHCP6CTL_COMMAND_REMOVE 2
#define DHCP6CTL_COMMAND_START 3
#define DHCP6CTL_COMMAND_STOP 4

/* control objects */
#define DHCP6CTL_BINDING 1
#define DHCP6CTL_BINDING_IA 2
#define DHCP6CTL_IA_PD 3
#define DHCP6CTL_INTERFACE 4
#define DHCP6CTL_IA_NA 5

/*
 * Hash protocol/algorithm types.  Use same values for DHCPv6 protocol
 * authentication for code sharing.
 */
enum { DHCP6CTL_AUTHPROTO_UNDEF = -1 };
enum { DHCP6CTL_AUTHALG_UNDEF = -1, DHCP6CTL_AUTHALG_HMACMD5 = 1 };

/*
 * Packet formats of command protocol
 */
struct dhcp6ctl {
	u_int16_t command;
	u_int16_t len;
	u_int16_t version;
	u_int16_t reserved;
	u_int32_t timestamp;
} __attribute__ ((__packed__));

struct dhcp6ctl_iaspec {
	u_int32_t flags;
	u_int32_t type;
	u_int32_t id;
	u_int32_t duidlen;
	/* variable length of DUID follows */
} __attribute__ ((__packed__));
