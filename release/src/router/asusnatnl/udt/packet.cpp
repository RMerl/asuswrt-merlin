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
   Yunhong Gu, last updated 02/12/2011
*****************************************************************************/


//////////////////////////////////////////////////////////////////////////////
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                        Packet Header                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~              Data / Control Information Field                 ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                        Sequence Number                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |ff |o|                     Message Number                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     Destination Socket ID                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 0:
//      0: Data Packet
//      1: Control Packet
//   bit ff:
//      11: solo message packet
//      10: first packet of a message
//      01: last packet of a message
//   bit o:
//      0: in order delivery not required
//      1: in order delivery required
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|            Type             |             Reserved          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                       Additional Info                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     Destination Socket ID                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 1-15:
//      0: Protocol Connection Handshake
//              Add. Info:    Undefined
//              Control Info: Handshake information (see CHandShake)
//      1: Keep-alive
//              Add. Info:    Undefined
//              Control Info: None
//      2: Acknowledgement (ACK)
//              Add. Info:    The ACK sequence number
//              Control Info: The sequence number to which (but not include) all the previous packets have beed received
//              Optional:     RTT
//                            RTT Variance
//                            available receiver buffer size (in bytes)
//                            advertised flow window size (number of packets)
//                            estimated bandwidth (number of packets per second)
//      3: Negative Acknowledgement (NAK)
//              Add. Info:    Undefined
//              Control Info: Loss list (see loss list coding below)
//      4: Congestion/Delay Warning
//              Add. Info:    Undefined
//              Control Info: None
//      5: Shutdown
//              Add. Info:    Undefined
//              Control Info: None
//      6: Acknowledgement of Acknowledement (ACK-square)
//              Add. Info:    The ACK sequence number
//              Control Info: None
//      7: Message Drop Request
//              Add. Info:    Message ID
//              Control Info: first sequence number of the message
//                            last seqeunce number of the message
//      8: Error Signal from the Peer Side
//              Add. Info:    Error code
//              Control Info: None
//      0x7FFF: Explained by bits 16 - 31
//              
//   bit 16 - 31:
//      This space is used for future expansion or user defined control packets. 
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|                 Sequence Number a (first)                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number b (last)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number (single)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Loss List Field Coding:
//      For any consectutive lost seqeunce numbers that the differnece between
//      the last and first is more than 1, only record the first (a) and the
//      the last (b) sequence numbers in the loss list field, and modify the
//      the first bit of a to 1.
//      For any single loss or consectutive loss less than 2 packets, use
//      the original sequence numbers in the field.


#include <cstring>
#include "packet.h"


const int CPacket::m_iPktHdrSize = 16;
const int CHandShake::m_iContentSize = 48;


// Set up the aliases in the constructure
CPacket::CPacket():
m_iSeqNo((int32_t&)(m_nHeader[0])),
m_iMsgNo((int32_t&)(m_nHeader[1])),
m_iTimeStamp((int32_t&)(m_nHeader[2])),
m_iID((int32_t&)(m_nHeader[3])),
m_pcData((char*&)(m_PacketVector[1].iov_base)),
__pad()
{
   for (int i = 0; i < 4; ++ i)
      m_nHeader[i] = 0;
   m_PacketVector[0].iov_base = (char *)m_nHeader;
   m_PacketVector[0].iov_len = CPacket::m_iPktHdrSize;
   m_PacketVector[1].iov_base = NULL;
   m_PacketVector[1].iov_len = 0;
}

CPacket::~CPacket()
{
}

int CPacket::getLength() const
{
   return m_PacketVector[1].iov_len;
}

void CPacket::setLength(const int& len)
{
   m_PacketVector[1].iov_len = len;
}

void CPacket::pack(const int& pkttype, void* lparam, void* rparam, const int& size)
{
   // Set (bit-0 = 1) and (bit-1~15 = type)
   m_nHeader[0] = 0x80000000 | (pkttype << 16);

   // Set additional information and control information field
   switch (pkttype)
   {
   case 2: //0010 - Acknowledgement (ACK)
      // ACK packet seq. no.
      if (NULL != lparam)
         m_nHeader[1] = *(int32_t *)lparam;

      // data ACK seq. no. 
      // optional: RTT (microsends), RTT variance (microseconds) advertised flow window size (packets), and estimated link capacity (packets per second)
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size;

      break;

   case 6: //0110 - Acknowledgement of Acknowledgement (ACK-2)
      // ACK packet seq. no.
      m_nHeader[1] = *(int32_t *)lparam;

      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = 4; //0;

      break;

   case 3: //0011 - Loss Report (NAK)
      // loss list
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size;

      break;

   case 4: //0100 - Congestion Warning
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = 4; //0;
  
      break;

   case 1: //0001 - Keep-alive
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = 4; //0;

      break;

   case 0: //0000 - Handshake
      // control info filed is handshake info
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size; //sizeof(CHandShake);

      break;

   case 5: //0101 - Shutdown
      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = 4; //0;

      break;

   case 7: //0111 - Message Drop Request
      // msg id 
      m_nHeader[1] = *(int32_t *)lparam;

      //first seq no, last seq no
      m_PacketVector[1].iov_base = (char *)rparam;
      m_PacketVector[1].iov_len = size;

      break;

   case 8: //1000 - Error Signal from the Peer Side
      // Error type
      m_nHeader[1] = *(int32_t *)lparam;

      // control info field should be none
      // but "writev" does not allow this
      m_PacketVector[1].iov_base = (char *)&__pad; //NULL;
      m_PacketVector[1].iov_len = 4; //0;

      break;

   case 32767: //0x7FFF - Reserved for user defined control packets
      // for extended control packet
      // "lparam" contains the extended type information for bit 16 - 31
      // "rparam" is the control information
      m_nHeader[0] |= *(int32_t *)lparam;

      if (NULL != rparam)
      {
         m_PacketVector[1].iov_base = (char *)rparam;
         m_PacketVector[1].iov_len = size;
      }
      else
      {
         m_PacketVector[1].iov_base = (char *)&__pad;
         m_PacketVector[1].iov_len = 4;
      }

      break;

   default:
      break;
   }
}

iovec* CPacket::getPacketVector()
{
   return m_PacketVector;
}

int CPacket::getFlag() const
{
   // read bit 0
   return m_nHeader[0] >> 31;
}

int CPacket::getType() const
{
   // read bit 1~15
   return (m_nHeader[0] >> 16) & 0x00007FFF;
}

int CPacket::getExtendedType() const
{
   // read bit 16~31
   return m_nHeader[0] & 0x0000FFFF;
}

int32_t CPacket::getAckSeqNo() const
{
   // read additional information field
   return m_nHeader[1];
}

int CPacket::getMsgBoundary() const
{
   // read [1] bit 0~1
   return m_nHeader[1] >> 30;
}

bool CPacket::getMsgOrderFlag() const
{
   // read [1] bit 2
   return (1 == ((m_nHeader[1] >> 29) & 1));
}

int32_t CPacket::getMsgSeq() const
{
   // read [1] bit 3~31
   return m_nHeader[1] & 0x1FFFFFFF;
}

CPacket* CPacket::clone() const
{
   CPacket* pkt = new CPacket;
   memcpy(pkt->m_nHeader, m_nHeader, m_iPktHdrSize);
   pkt->m_pcData = new char[m_PacketVector[1].iov_len];
   memcpy(pkt->m_pcData, m_pcData, m_PacketVector[1].iov_len);
   pkt->m_PacketVector[1].iov_len = m_PacketVector[1].iov_len;

   return pkt;
}

CHandShake::CHandShake():
m_iVersion(0),
m_iType(0),
m_iISN(0),
m_iMSS(0),
m_iFlightFlagSize(0),
m_iReqType(0),
m_iID(0),
m_iCookie(0)
{
   for (int i = 0; i < 4; ++ i)
      m_piPeerIP[i] = 0;
}

int CHandShake::serialize(char* buf, int& size)
{
   if (size < m_iContentSize)
      return -1;

   int32_t* p = (int32_t*)buf;
   *p++ = m_iVersion;
   *p++ = m_iType;
   *p++ = m_iISN;
   *p++ = m_iMSS;
   *p++ = m_iFlightFlagSize;
   *p++ = m_iReqType;
   *p++ = m_iID;
   *p++ = m_iCookie;
   for (int i = 0; i < 4; ++ i)
      *p++ = m_piPeerIP[i];

   size = m_iContentSize;

   return 0;
}

int CHandShake::deserialize(const char* buf, const int& size)
{
   if (size < m_iContentSize)
      return -1;

   int32_t* p = (int32_t*)buf;
   m_iVersion = *p++;
   m_iType = *p++;
   m_iISN = *p++;
   m_iMSS = *p++;
   m_iFlightFlagSize = *p++;
   m_iReqType = *p++;
   m_iID = *p++;
   m_iCookie = *p++;
   for (int i = 0; i < 4; ++ i)
      m_piPeerIP[i] = *p++;

   return 0;
}
