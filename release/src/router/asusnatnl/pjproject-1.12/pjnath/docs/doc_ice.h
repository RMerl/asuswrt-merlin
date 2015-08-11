/* $Id: doc_ice.h 3553 2011-05-05 06:14:19Z nanang $ */
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
@defgroup PJNATH_ICE ICE: Interactive Connectivity Establishment
@brief Interactive Connectivity Establishment (ICE)
@ingroup PJNATH
*/

/**
@defgroup PJNATH_ICE_STREAM_TRANSPORT ICE stream transport
@brief Transport for media streams using ICE
@ingroup PJNATH_ICE
 */

/**
@defgroup PJNATH_ICE_SESSION ICE Session
@brief Transport Independent ICE Session
@ingroup PJNATH_ICE
 */

/**
@addtogroup PJNATH_ICE
\section org Library organizations

See <b>Table of Contents</b> below.

\section ice_intro_sec Introduction to ICE

Interactive Connectivity Establishment (ICE) is the ultimate 
weapon a client can have in its NAT traversal solution arsenals, 
as it promises that if there is indeed one path for two clients 
to communicate, then ICE will find this path. And if there are 
more than one paths which the clients can communicate, ICE will 
use the best/most efficient one.

ICE works by combining several protocols (such as STUN and TURN) 
altogether and offering several candidate paths for the communication,
thereby maximising the chance of success, but at the same time also 
has the capability to prioritize the candidates, so that the more 
expensive alternative (namely relay) will only be used as the last
resort when else fails. ICE negotiation process involves several 
stages:

 - candidate gathering, where the client finds out all the possible 
   addresses that it can use for the communication. It may find 
   three types of candidates: host candidate to represent its 
   physical NICs, server reflexive candidate for the address that 
   has been resolved from STUN, and relay candidate for the address 
   that the client has allocated from a TURN relay.
 - prioritizing these candidates. Typically the relay candidate will
   have the lowest priority to use since it's the most expensive.
 - encoding these candidates, sending it to remote peer, and 
   negotiating it with offer-answer.
 - pairing the candidates, where it pairs every local candidates 
   with every remote candidates that it receives from the remote peer.
 - checking the connectivity for each candidate pairs.
 - concluding the result. Since every possible path combinations are 
   checked, if there is a path to communicate ICE will find it.


\section icestrans_sec Using ICE transport

The \ref PJNATH_ICE_STREAM_TRANSPORT is a ready to use object which
performs the above ICE operations as well as provides application with
interface to send and receive data using the negotiated path.

Please see \ref PJNATH_ICE_STREAM_TRANSPORT on how to use this object.


\section ice_owntransport_sec Creating custom ICE transport

If the \ref PJNATH_ICE_STREAM_TRANSPORT is not suitable for use 
for some reason, you will need to implement your own ICE transport,
by combining the \ref PJNATH_ICE_SESSION with your own means to
send and receive packets. The \ref PJNATH_ICE_STREAM_TRANSPORT 
provides the best example on how to do this.


\section ice_samples_sec Samples

The \ref ice_demo_sample sample demonstrates how to use 
\ref PJNATH_ICE_STREAM_TRANSPORT <b>without</b> using signaling 
protocol such as <b>SIP</b>. It provides  interactive user interface
to create and manage the ICE sessions as well as to exchange SDP 
with another ice_demo instance.

Also see <b>\ref samples_page</b> for other samples.
 */


