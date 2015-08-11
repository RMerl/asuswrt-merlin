/* $Id: doc_turn.h 3553 2011-05-05 06:14:19Z nanang $ */
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
@defgroup PJNATH_TURN TURN: Traversal Using Relays around NAT
@brief TURN protocol implementation
@ingroup PJNATH

\section turn_intro_sec Introduction to TURN

When a direct communication path cannot be found, it is necessary to
use the services of an intermediate host that acts as a relay for the
packets.  This relay typically sits in the public Internet and relays
packets between two hosts that both sit behind NATs.

TURN allows a host behind a NAT (called the TURN client) to request that 
another host (called the TURN server) act as a relay.  The client can 
arrange for the server to relay packets to and from certain other hosts
(called peers) and can control aspects of how the relaying is done.
The client does this by obtaining an IP address and port on the
server, called the relayed-transport-address.  When a peer sends a
packet to the relayed-transport-address, the server relays the packet
to the client.  When the client sends a data packet to the server,
the server relays it to the appropriate peer using the relayed-
transport-address as the source.


\section turn_op_sec Overview of TURN operations

<b>Discovering TURN server</b>.\n
Client learns the IP address of the TURN
server either through some privisioning or by querying DNS SRV records
for TURN service for the specified domain. Client may use UDP or TCP (or
TLS) to connect to the TURN server.

<b>Authentication</b>.\n
All TURN operations requires the use of authentication
(it uses STUN long term autentication method), hence client must be
configured with the correct credential to use the service.

<b>Allocation</b>.\n
Client creates one "relay port" (or called <b>relayed-transport-address</b>
in TURN terminology) in the TURN server by sending TURN \a Allocate request,
hence this process is called creating allocation. Once the allocation is 
successful, client will be given the IP address and port of the "relay 
port" in the Allocate response.

<b>Sending data through the relay</b>.\n
Once allocation has been created, client may send data to any remote 
endpoints (called peers in TURN terminology) via the "relay port". It does 
so by sending Send Indication to the TURN server, giving the peer address 
in the indication message. But note that at this point peers are not allowed
to send data towards the client (via the "relay port") before permission is 
installed for that peer.

<b>Creating permissions</b>.\n
Permission needs to be created in the TURN server so that a peer can send 
data to the client via the relay port (a peer in this case is identified by
its IP address). Without this, when the TURN server receives data from the
peer in the "relay port", it will drop this data.

<b>Receiving data from peers</b>.\n
Once permission has been installed for the peer, any data received by the 
TURN server (from that peer) in the "relay port" will be relayed back to 
client by using Data Indication.

<b>Using ChannelData</b>.\n
TURN provides optimized framing to the data by using ChannelData 
packetization. The client activates this format by sending ChannelBind 
request to the TURN server, which provides (channel) binding which maps a 
particular peer address with a channel number. Data sent or received to/for
this peer will then use ChannelData format instead of Send or Data 
Indications.

<b>Refreshing the allocation, permissions, and channel bindings</b>.\n
Allocations, permissions, and channel bindings need to be refreshed
periodically by client, or otherwise they will expire.

<b>Destroying the allocation</b>.\n
Once the "relay port" is no longer needed, client destroys the allocation
by sending Refresh request with LIFETIME attribute set to zero.


\section turn_org_sec Library organizations

The TURN functionalities in PJNATH primarily consist of 
\ref PJNATH_TURN_SOCK and \ref PJNATH_TURN_SESSION. Please see more
below.


\section turn_using_sec Using TURN transport

The \ref PJNATH_TURN_SOCK is a ready to use object for relaying
application data via a TURN server, by managing all the operations
above.

Among other things it provides the following features:
 - resolution of the TURN server with DNS SRV
 - interface to create allocation, permissions, and channel
   bindings
 - interface to send and receive packets through the relay
 - provides callback to notify the application about incoming data
 - managing the allocation, permissions, and channel bindings

Please see \ref PJNATH_TURN_SOCK for more documentation about and
on how to use this object.


\section turn_owntransport_sec Creating custom TURN transport

The \ref PJNATH_TURN_SESSION is a transport-independent object to
manage a client TURN session. It contains the core logic for managing
the TURN client session as listed in TURN operations above, but
in transport-independent manner (i.e. it doesn't have a socket), so
that developer can integrate TURN client functionality into existing
framework that already has its own means to send and receive data,
or to support new transport types to TURN, such as TLS.

You can create your own (custom) TURN transport by wrapping this
into your own object, and provide it with the means to send and
receive packets.

Please see \ref PJNATH_TURN_SESSION for more information.


\section turn_samples_sec Samples

The \ref turn_client_sample is a sample application to use the
\ref PJNATH_TURN_SOCK. Also there is a sample TURN server in
the distribution as well. 

Also see <b>\ref samples_page</b> for other samples.

 */


/**
 * @defgroup PJNATH_TURN_SOCK TURN client transport
 * @brief Client transport utilizing TURN relay
 * @ingroup PJNATH_TURN
 */

/**
 * @defgroup PJNATH_TURN_SESSION TURN client session
 * @brief Transport independent TURN client session
 * @ingroup PJNATH_TURN
 */
