/* $Id: doc_stun.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */


/**
@defgroup PJNATH_STUN STUN: Session Traversal Utilities for NAT
@ingroup PJNATH
@brief Open source STUN library
 */

/**
@defgroup PJNATH_STUN_SOCK STUN-aware socket transport
@brief STUN aware UDP socket transport
@ingroup PJNATH_STUN
 */


/**
@defgroup PJNATH_STUN_SESSION STUN session
@brief STUN client and server session
@ingroup PJNATH_STUN
 */

/**
@defgroup PJNATH_STUN_BASE Base STUN objects
@ingroup PJNATH_STUN
@brief STUN data structures, objects, and configurations

These section contains STUN base data structures as well as
configurations. Among other things it contains STUN message
representation and parsing, transactions, authentication
framework, as well as compile-time and run-time configurations.
*/


/**
@addtogroup PJNATH_STUN

This module contains implementation of STUN library in PJNATH -
the open source NAT helper containing STUN and ICE.

\section stun_org_sec Library organizations

The STUN part of PJNATH consists of the the following sections (see
<b>Table of Contents</b> below).


\section stun_using_sec Using the STUN transport

The \ref PJNATH_STUN_SOCK is a ready to use object which provides 
send and receive interface for communicating UDP packets as well as
means to communicate with the STUN server and manage the STUN mapped
address.

Some features of the \ref PJNATH_STUN_SOCK:
 - API to send and receive UDP packets,
 - interface to query the STUN mapped address info, 
 - multiplex STUN and non-STUN incoming packets and distinguish between
   STUN responses that belong to internal requests with application data
   (the application data may be STUN packets as well),
 - resolution of the STUN server with DNS SRV query (if wanted), 
 - maintaining STUN keep-alive, and 
 - handle changes in STUN mapped address binding.

Please see \ref PJNATH_STUN_SOCK for more information.


\section stun_advanced_sec Advanced use of the STUN components

The rest of the STUN part of the library provides lower level objects
which can be used to build your own STUN based transport or
protocols (officially called STUN usages). These will be explained 
briefly below.


\subsection stun_sess_sec The STUN session

A STUN session is interactive information exchange between two STUN 
endpoints that lasts for some period of time. It is typically started by
an outgoing or incoming request, and consists of several requests, 
responses, and indications. All requests and responses within the session
typically share a same credential.

The \ref PJNATH_STUN_SESSION is a transport-independent object to
manage a client or server STUN session. It is one of the core object in
PJNATH, and it is used by several higher level objects including the 
\ref PJNATH_STUN_SOCK, \ref PJNATH_TURN_SESSION, and \ref PJNATH_ICE_SESSION.

The \ref PJNATH_STUN_SESSION has the following features:
   - transport independent
   - authentication management
   - static or dynamic credential
   - client transaction management
   - server transaction management

For more information, including how to use it please see 
\ref PJNATH_STUN_SESSION.


\subsection stun_extending_sec Extending STUN to support other usages

At present, the STUN subsystem in PJNATH supports STUN Binding, TURN, and
ICE usages. If other usages are to be supported, typically you would need
to add new STUN methods (and the corresponding request and response message
types), attributes, and error codes to \ref PJNATH_STUN_MSG subsystem of
PJNATH, as well as implementing the logic for the STUN usage.


\section stunsamples_sec STUN samples

The \ref turn_client_sample sample application also contains sample
code to use \ref PJNATH_STUN_SOCK. 

Also see <b>\ref samples_page</b> for other samples.


 */

