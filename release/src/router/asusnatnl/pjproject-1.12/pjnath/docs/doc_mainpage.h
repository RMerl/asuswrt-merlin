/* $Id: doc_mainpage.h 3553 2011-05-05 06:14:19Z nanang $ */
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

@mainpage PJNATH - Open Source ICE, STUN, and TURN Library

PJNATH (PJSIP NAT Helper) is an open source library providing NAT traversal 
functionalities by using standard based protocols such as STUN, TURN, and ICE.


\section background Background


Network Address Translation (NAT) is commonly deployed everywhere primarily to
alleviate the exhaustion of IPv4 address space by allowing multiple hosts to 
share a public/Internet address. While NAT would work well for typical client 
server communications (such as web and email), since it's always the client 
that initiates the conversation and normally client doesn't need to maintain
the connection for a long time, installation of NAT would cause major problem
for peer-to-peer communication, such as (and especially) VoIP. 

<strong>\ref nat_intro "Read more.."</strong>


\section intro Introduction to PJNATH

PJSIP NAT Helper (PJNATH) is a library which contains the implementation of 
standard based NAT traversal solutions. PJNATH can be used as a stand-alone 
library for your software, or you may use PJSUA-LIB library, a very high level
 library integrating PJSIP, PJMEDIA, and PJNATH into simple to use APIs.

PJNATH has the following features:

 - <strong>STUNbis</strong> implementation,\n
   providing both ready to use 
   STUN-aware socket and framework to implement higher level STUN based 
   protocols such as TURN and ICE. The implementation complies to 
   <A HREF="http://www.ietf.org/rfc/rfc5389.txt">RFC 5389</A>
   standard.\n\n

 - <strong>NAT type detection</strong>, \n
   performs detection of the NAT type in front of the endpoint, according
   to <A HREF="http://www.ietf.org/rfc/rfc3489.txt">RFC 3489</A>. 
   While the practice to detect the NAT type to assist NAT 
   traversal has been deprecated in favor of ICE, the information may still
   be useful for troubleshooting purposes, hence the utility is provided.\n\n

 - <strong>Traversal Using Relays around NAT (TURN)</strong> implementation.\n
   TURN is a protocol for relaying communications by means of using relay, 
   and combined with ICE it provides efficient last effort alternative for 
   the communication path. The TURN implementation in PJNATH complies to 
   <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-turn-14.txt">
   draft-ietf-behave-turn-14</A> draft.\n\n

 - <strong>Interactive Connectivity Establishmen (ICE)</strong> implementation.\n
   ICE is a protocol for discovering communication path(s) between two 
   endpoints. The implementation in PJNATH complies to
   <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-mmusic-ice-19.txt">
   draft-ietf-mmusic-ice-19.txt</A> draft

In the future, more protocols will be implemented (such as UPnP IGD, and 
SOCKS5).


\section pjnath_organization_sec Library Organization

The library provides the following main component groups:

 - \ref PJNATH_STUN\n\n
 - \ref PJNATH_TURN\n\n
 - \ref PJNATH_ICE\n\n
 - \ref PJNATH_NAT_DETECT\n\n

Apart from the \ref PJNATH_NAT_DETECT, each component group are further 
divided into two functionalities:

 - <b>Transport objects</b>\n
   The transport objects (such as STUN transport, TURN transport, and ICE
   stream transport) are the implementation of the session object
   <strong>with</strong> particular transport/sockets. They are provided
   as ready to use objects for applications.\n\n

 - <b>Transport independent/session layer</b>\n
   The session objects (such as STUN session, TURN session, and ICE session)
   are the core object for maintaining the protocol session, and it is
   independent of transport (i.e. it does not "own" a socket). This way
   developers can reuse these session objects for any kind of transports,
   such as UDP, TCP, or TLS, with or without using PJLIB socket API.
   The session objects provide function and callback to send and receive
   packets respectively.

For more information about each component groups, please click the component
link above.


\section pjnath_start_sec Getting Started with PJNATH

\subsection dependency Library Dependencies

The PJNATH library depends (and only depends) on PJLIB and PJLIB-UTIL
libraries. All these libraries should have been packaged together with
the main PJSIP distribution. You can download the PJSIP distribution
from <A HREF="http://www.pjsip.org">PJSIP website</A>


\subsection pjnath_using_sec Using the libraries

Please click on the appropriate component under \ref pjnath_organization_sec
section above, which will take you to the documentation on how to use the 
component.


\subsection samples_sec Samples

We attempt to provide simple samples to use each functionality of the PJNATH
library.

Please see <b>\ref samples_page</b> page for the list of samples.


*/



/**
@defgroup samples_page PJNATH Samples and screenshots
@brief Sample applications and screenshots
 */


