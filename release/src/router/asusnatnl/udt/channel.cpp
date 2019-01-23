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
****************************************************************************/

/****************************************************************************
written by
   Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <cstring>
   #include <cstdio>
   #include <cerrno>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #ifdef LEGACY_WIN32
      #include <wspiapi.h>
   #endif
#endif
#include "common.h" //for CTimer::getTime()

#include <pjmedia/natnl_stream.h>	
#include <pjmedia/stream.h>
//------------------------------//
#include "channel.h"
#include "packet.h"

#ifdef WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif

#define UMIN(a, b) (((a)>(b)) ? (b) : (a))
#define UMAX(a, b) (((a)<(b)) ? (b) : (a))
#ifdef WIN32
extern DWORD recvThreadId;
extern DWORD sendThreadId;
#else
extern unsigned recvThreadId;
extern unsigned sendThreadId;
#endif
long pkts = 0;
static unsigned char get_check_sum(const unsigned char *data, int len)
{
    int i;
    unsigned char sum = '\0';

    for(i=0;i<len;i++) {
	sum += data[i]; 
    }

   return sum;
}

static int check_packet_integrity(recv_buff *data)
{
    natnl_hdr *hdr = (natnl_hdr *)&data->buff[0];

    if(hdr->magic != 0xFF)
	    return -1;

   // if(hdr->cksum != get_check_sum(&data->buff[sizeof(natnl_hdr)], data->len-sizeof(natnl_hdr)))
//	    return -1;
#if 0
    unsigned short len = ntohs(hdr->length);
    if(len != data->len - sizeof(natnl_hdr))
	    return -1;
#endif
    return 0;
}

CChannel::CChannel():
m_iIPversion(AF_INET),
m_iSockAddrSize(sizeof(sockaddr_in)),
m_iSocket(),
m_call(NULL),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
}

CChannel::CChannel(const int& version):
m_iIPversion(version),
m_iSocket(),
m_call(NULL),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
   m_iSockAddrSize = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

CChannel::~CChannel()
{
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   m_iSocket = socket(m_iIPversion, SOCK_DGRAM, 0);

   #ifdef WIN32
      if (INVALID_SOCKET == m_iSocket)
   #else
      if (m_iSocket < 0)
   #endif
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = m_iSockAddrSize;

      if (0 != bind(m_iSocket, addr, namelen))
         throw CUDTException(1, 3, NET_ERROR);
   }
   else
   {
      //sendto or WSASendTo will also automatically bind the socket
      addrinfo hints;
      addrinfo* res;

      memset(&hints, 0, sizeof(struct addrinfo));

      hints.ai_flags = AI_PASSIVE;
      hints.ai_family = m_iIPversion;
      hints.ai_socktype = SOCK_DGRAM;

      if (0 != getaddrinfo(NULL, "0", &hints, &res))
         throw CUDTException(1, 3, NET_ERROR);

      if (0 != bind(m_iSocket, res->ai_addr, res->ai_addrlen))
         throw CUDTException(1, 3, NET_ERROR);

      freeaddrinfo(res);
   }

   setUDPSockOpt();
}

void CChannel::open(UDPSOCKET udpsock)
{
   m_iSocket = udpsock;
   setUDPSockOpt();
}

/*==== add by Andrew ====*/
void CChannel::open(void *call)
{
   m_iSocket = -1;
   m_call = call;
   //setUDPSockOpt();
}
/*==== end ====*/

void CChannel::setUDPSockOpt()
{
   #if defined(BSD) || defined(OSX)
      // BSD system will fail setsockopt if the requested buffer size exceeds system maximum value
      int maxsize = 64000;
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int)))
         setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&maxsize, sizeof(int));
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int)))
         setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&maxsize, sizeof(int));
   #else
      // for other systems, if requested is greated than maximum, the maximum value will be automactally used
      if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int))) ||
          (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int))))
         throw CUDTException(1, 3, NET_ERROR);
   #endif

   timeval tv;
   tv.tv_sec = 0;
   #if defined (BSD) || defined (OSX)
      // Known BSD bug as the day I wrote this code.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      int opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 3, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)))
         throw CUDTException(1, 3, NET_ERROR);
   #else
      // Set receiving time-out value
      if (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)))
         throw CUDTException(1, 3, NET_ERROR);
   #endif
}

void CChannel::close(void *call)
{
   #ifndef WIN32
      ::close(m_iSocket);
   #else
      if(m_iSocket > 0)
         closesocket(m_iSocket);
      m_iSocket = -1;
      m_call = NULL;
   #endif
}

void CChannel::close() 
{
   #ifndef WIN32
      ::close(m_iSocket);
   #else
      //closesocket(m_iSocket);
      if(m_iSocket > 0)
         closesocket(m_iSocket);
      m_iSocket = -1;
      m_call = NULL;
   #endif
}

int CChannel::getSndBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

int CChannel::getRcvBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const int& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const int& size)
{
   m_iRcvBufSize = size;
}

void CChannel::getSockAddr(sockaddr* addr) const
{
   socklen_t namelen = m_iSockAddrSize;

   getsockname(m_iSocket, addr, &namelen);
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen = m_iSockAddrSize;

   getpeername(m_iSocket, addr, &namelen);
}

#ifdef DEBUGP
void dumpHex(char *buff, int len)
{
	int i;
	for(i=0;i<len;i++) {
		printf("%02X", buff[i]&0xff);
		if(i%4==3) printf("\n");
	}
	printf("\n");
}
#endif

int CChannel::sendto(const sockaddr* addr, CPacket& packet) const
{

   // convert control information into network order
   if (packet.getFlag()) {
      for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = htonl(*((uint32_t *)packet.m_pcData + i));
   } 

   uint32_t* p = packet.m_nHeader;
   for (int j = 0; j < 4; ++ j)
   {
      *p = htonl(*p);
      ++ p;
   }

#ifdef DEBUGP
   //dump ctrl packet
    printf("\nSend Header:\n");
    dumpHex((char *)packet.m_PacketVector[0].iov_base, packet.m_PacketVector[0].iov_len);
   char *bb = (char *)packet.m_PacketVector[0].iov_base;
   if(bb[0]&0x80) {
      printf("Data:\n");
      dumpHex((char *)packet.m_PacketVector[1].iov_base, packet.m_PacketVector[1].iov_len);
      printf("================\n");
   }
#endif

   int res = -1;
   unsigned size;
   unsigned len;
   natnl_hdr hdr = {0xff, 0x00, 0x0000};
   int is_tnl_data = 0;
   pj_thread_desc desc;
   pj_thread_t *thread = 0;

   if(m_iSocket == -1) {
      pjsua_call *call = (pjsua_call *)m_call;
	  if(call == NULL) return -1;

	  // DEAN, prevent assert fail while garbage collector remove UDT socket on multiple instance. 
	  if (!pj_thread_is_registered(call->inst_id)) {
		  int status = pj_thread_register(call->inst_id, "CChannel::sendto", desc, &thread );
		  if (status != PJ_SUCCESS)
			  return -1;
	  }

	  pj_mutex_lock(call->tnl_stream_lock2);

	  natnl_stream *stream = (natnl_stream *)call->tnl_stream;
	  if(stream == NULL) {
	     pj_mutex_unlock(call->tnl_stream_lock2);
	     return -1;
	  }

     size = CPacket::m_iPktHdrSize + packet.getLength() + sizeof(natnl_hdr);
     len = (CPacket::m_iPktHdrSize + packet.getLength());
	  hdr.length = htons(len);

      memcpy((char *)&m_pktBuffer[sizeof(natnl_hdr)], packet.m_PacketVector[0].iov_base, packet.m_PacketVector[0].iov_len);
      memcpy((char *)&m_pktBuffer[packet.m_PacketVector[0].iov_len+sizeof(natnl_hdr)], packet.m_PacketVector[1].iov_base, packet.m_PacketVector[1].iov_len);
      memcpy((char *)&m_pktBuffer[0], &hdr, sizeof(natnl_hdr));

resend:
	  // DEAN, check if this is tunnel data. If true, update last_data time.
	  is_tnl_data = pjmedia_natnl_udt_packet_is_tnl_data(&m_pktBuffer[0], size);

	  pj_assert(size < sizeof(m_pktBuffer));
		
	  ((pj_uint8_t*)m_pktBuffer)[size] = 0;  // tunnel data flag off

	  if (is_tnl_data) {
		  pj_get_timestamp(&stream->last_data);  // DEAN save current time 
		  ((pj_uint8_t*)m_pktBuffer)[size] = 1;  // tunnel data flag on
	  }

	  res = pjmedia_transport_send_rtp(stream->med_tp, m_pktBuffer, size);	// +Roger modified - stream pointer to med_tp
#if 0 // No need to resend it, because UDT will handle this.
	  if(res == 70011) { //EAGAIN
		  m_pTimer->sleepto(50000); //sleep for 50 us
	      goto resend;   
      }
#endif
      pj_mutex_unlock(call->tnl_stream_lock2);
   }
   res = (0 == res) ? size : -1;

   // convert back into local host order
   //for (int k = 0; k < 4; ++ k)
   //   packet.m_nHeader[k] = ntohl(packet.m_nHeader[k]);
   p = packet.m_nHeader;
   for (int k = 0; k < 4; ++ k)
   {
      *p = ntohl(*p);
       ++ p;
   }

   if (packet.getFlag())
   {
      for (int l = 0, n = packet.getLength() / 4; l < n; ++ l)
         *((uint32_t *)packet.m_pcData + l) = ntohl(*((uint32_t *)packet.m_pcData + l));
   }

   return res;
}

extern int natnl_handle_recv_msg(pjsua_call_id call_id, pjmedia_transport *tp, 
								 char *data, int data_len);

int CChannel::recvfrom(sockaddr* addr, CPacket& packet) const
{
   int res = -1;
   recv_buff *rb = NULL;
   pj_thread_desc desc;
   pj_thread_t *thread = 0;

    if (m_iSocket == -1) {
        pjsua_call *call = (pjsua_call *)m_call;
        if(call == NULL)
                return -1;
		if(call->tnl_stream==NULL)
			return -1;

		// DEAN, prevent assert fail while garbage collector remove UDT socket on multiple instance. 
		if (!pj_thread_is_registered(call->inst_id)) {
			int status = pj_thread_register(call->inst_id, "CChannel::recvfrom", desc, &thread );
			if (status != PJ_SUCCESS)
				return -1;
		}

		pj_mutex_lock(call->tnl_stream_lock3);

		natnl_stream *stream = (natnl_stream *)call->tnl_stream;

		//get data from rBuff
		if (stream == NULL) {
			pj_mutex_unlock(call->tnl_stream_lock3);
			return -1;
		}
		// charles CHARLES
		// DEAN commeted, for using pj_sem_try_wait2
        //pj_mutex_unlock(call->tnl_stream_lock3);
		//pj_sem_wait(stream->rbuff_sem);
		pj_sem_trywait2(stream->rbuff_sem);
        //pj_mutex_lock(call->tnl_stream_lock3);

        pj_mutex_lock(stream->rbuff_mutex);

        if (!pj_list_empty(&stream->rbuff)) {
			rb = stream->rbuff.next;
			stream->rbuff_cnt--;
			//PJ_LOG(4, ("channel.cpp", "rbuff_cnt=%d", stream->rbuff_cnt));
			pj_list_erase(rb);
			/*if (rb->len > 0 && 
				((pj_uint32_t *)rb->buff)[0] == NO_CTL_SESS_MGR_HEADER_MAGIC) {  // check the magic
					char *data = (char *)&rb->buff[sizeof(NO_CTL_SESS_MGR_HEADER_MAGIC)];
					int len = rb->len - sizeof(NO_CTL_SESS_MGR_HEADER_MAGIC);
					natnl_handle_recv_msg(call->index, call->tnl_stream->med_tp, data, len);
			} else */if (!check_packet_integrity(rb)) {
				int ds = UMIN(packet.m_PacketVector[1].iov_len, rb->len - sizeof(natnl_hdr) - CPacket::m_iPktHdrSize);
				memcpy(packet.m_PacketVector[0].iov_base, &rb->buff[sizeof(natnl_hdr)], packet.m_PacketVector[0].iov_len);
				memcpy(packet.m_PacketVector[1].iov_base, &rb->buff[packet.m_PacketVector[0].iov_len+sizeof(natnl_hdr)], ds);
				res = rb->len - sizeof(natnl_hdr);
			}
	  }
        pj_mutex_unlock(stream->rbuff_mutex);

		if (rb != NULL) {
#if 1
			//move rb to gcbuff
			pj_mutex_lock(stream->gcbuff_mutex);
			pj_list_push_back(&stream->gcbuff, rb);
			pj_mutex_unlock(stream->gcbuff_mutex);
#else
			free(rb);
			rb = NULL;
#endif
		}
		pj_mutex_unlock(call->tnl_stream_lock3);
    }

   if (res <= 0)
   {
      packet.setLength(-1);
      return -1;
   }

   packet.setLength(res - CPacket::m_iPktHdrSize);

#ifdef DEBUGP
   printf("\nRecv Header:\n");
   dumpHex((char *)packet.m_PacketVector[0].iov_base, packet.m_PacketVector[0].iov_len);
   char *bb = (char *)packet.m_PacketVector[0].iov_base;
   if(bb[0]&0x80) {
      printf("Data:\n");
      dumpHex((char *)packet.m_PacketVector[1].iov_base, packet.m_PacketVector[1].iov_len);
      printf("================\n");
   }
#endif

   // convert back into local host order
   //for (int i = 0; i < 4; ++ i)
   //   packet.m_nHeader[i] = ntohl(packet.m_nHeader[i]);
   uint32_t* p = packet.m_nHeader;
   for (int i = 0; i < 4; ++ i)
   {
      *p = ntohl(*p);
      ++ p;
   }

   if (packet.getFlag())
   {
      for (int j = 0, n = packet.getLength() / 4; j < n; ++ j)
         *((uint32_t *)packet.m_pcData + j) = ntohl(*((uint32_t *)packet.m_pcData + j));
   }
   return packet.getLength();
}

#if 0
int CChannel::get_tnl_stream(void) const
{
	if (m_iSocket == -1) {
		pjsua_call *call = (pjsua_call *)m_call;
		if(call == NULL)
			return NULL;
		if(call->tnl_stream==NULL)
			return NULL;
		return (int)call->tnl_stream;
	}
	return NULL;
}
#endif
