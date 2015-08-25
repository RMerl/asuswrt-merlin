/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef __UDT_CHANNEL_H__
#define __UDT_CHANNEL_H__


#include "udt.h"
#include "common.h"
#include "packet.h"

class CChannel
{
public:
   CChannel();
   CChannel(const int& version);
   ~CChannel();

      // Functionality:
      //    Opne a UDP channel.
      // Parameters:
      //    0) [in] addr: The local address that UDP will use.
      // Returned value:
      //    None.

   void open(const sockaddr* addr = NULL);

      // Functionality:
      //    Opne a UDP channel based on an existing UDP socket.
      // Parameters:
      //    0) [in] udpsock: UDP socket descriptor.
      // Returned value:
      //    None.

   void open(UDPSOCKET udpsock);

   /*==== add by Andrew ====*/
   void open(void *stream);
   void close(void *stream);
   /*==== end ====*/

      // Functionality:
      //    Disconnect and close the UDP entity.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void close();

      // Functionality:
      //    Get the UDP sending buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP sending buffer size.

   int getSndBufSize();

      // Functionality:
      //    Get the UDP receiving buffer size.
      // Parameters:
      //    None.
      // Returned value:
      //    Current UDP receiving buffer size.

   int getRcvBufSize();

      // Functionality:
      //    Set the UDP sending buffer size.
      // Parameters:
      //    0) [in] size: expected UDP sending buffer size.
      // Returned value:
      //    None.

   void setSndBufSize(const int& size);

      // Functionality:
      //    Set the UDP receiving buffer size.
      // Parameters:
      //    0) [in] size: expected UDP receiving buffer size.
      // Returned value:
      //    None.

   void setRcvBufSize(const int& size);

      // Functionality:
      //    Query the socket address that the channel is using.
      // Parameters:
      //    0) [out] addr: pointer to store the returned socket address.
      // Returned value:
      //    None.

   void getSockAddr(sockaddr* addr) const;

      // Functionality:
      //    Query the peer side socket address that the channel is connect to.
      // Parameters:
      //    0) [out] addr: pointer to store the returned socket address.
      // Returned value:
      //    None.

   void getPeerAddr(sockaddr* addr) const;

      // Functionality:
      //    Send a packet to the given address.
      // Parameters:
      //    0) [in] addr: pointer to the destination address.
      //    1) [in] packet: reference to a CPacket entity.
      // Returned value:
      //    Actual size of data sent.

   int sendto(const sockaddr* addr, CPacket& packet) const;

      // Functionality:
      //    Receive a packet from the channel and record the source address.
      // Parameters:
      //    0) [in] addr: pointer to the source address.
      //    1) [in] packet: reference to a CPacket entity.
      // Returned value:
      //    Actual size of data received.

   int recvfrom(sockaddr* addr, CPacket& packet) const;

   //int get_tnl_stream(void) const;

private:
   void setUDPSockOpt();

private:
   int m_iIPversion;                    // IP version
   int m_iSockAddrSize;                 // socket address structure size (pre-defined to avoid run-time test)

   UDPSOCKET m_iSocket;                 // socket descriptor

   int m_iSndBufSize;                   // UDP sending buffer size
   int m_iRcvBufSize;                   // UDP receiving buffer size

   void enterCS(const pthread_mutex_t& lock);
   void leaveCS(const pthread_mutex_t& lock);

public:
   void *m_call;
   CTimer *m_pTimer;

private:
   unsigned char m_pktBuffer[2500];
};


#endif
