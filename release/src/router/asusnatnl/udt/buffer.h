/*****************************************************************************
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 05/05/2009
*****************************************************************************/

#ifndef __UDT_BUFFER_H__
#define __UDT_BUFFER_H__


#include "udt.h"
#include "list.h"
#include "queue.h"
#include <fstream>

class CSndBuffer
{
public:
   CSndBuffer(const int& size = 32, const int& mss = 1500);
   ~CSndBuffer();

      // Functionality:
      //    Insert a user buffer into the sending list.
      // Parameters:
      //    0) [in] data: pointer to the user data block.
      //    1) [in] len: size of the block.
      //    2) [in] ttl: time to live in milliseconds
      //    3) [in] order: if the block should be delivered in order, for DGRAM only
      // Returned value:
      //    None.

   void addBuffer(const char* data, const int& len, const int& ttl = -1, const bool& order = false);

      // Functionality:
      //    Read a block of data from file and insert it into the sending list.
      // Parameters:
      //    0) [in] ifs: input file stream.
      //    1) [in] len: size of the block.
      // Returned value:
      //    actual size of data added from the file.

   int addBufferFromFile(std::fstream& ifs, const int& len);

      // Functionality:
      //    Find data position to pack a DATA packet from the furthest reading point.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [out] msgno: message number of the packet.
      // Returned value:
      //    Actual length of data read.

   int readData(char** data, int32_t& msgno);

      // Functionality:
      //    Find data position to pack a DATA packet for a retransmission.
      // Parameters:
      //    0) [out] data: the pointer to the data position.
      //    1) [in] offset: offset from the last ACK point.
      //    2) [out] msgno: message number of the packet.
      //    3) [out] msglen: length of the message
      // Returned value:
      //    Actual length of data read.

   int readData(char** data, const int offset, int32_t& msgno, int& msglen);

      // Functionality:
      //    Update the ACK point and may release/unmap/return the user data according to the flag.
      // Parameters:
      //    0) [in] offset: number of packets acknowledged.
      // Returned value:
      //    None.

   void ackData(const int& offset);

      // Functionality:
      //    Read size of data still in the sending list.
      // Parameters:
      //    None.
      // Returned value:
      //    Current size of the data in the sending list.

   int getCurrBufSize() const;

private:
   void increase();

private:
   pthread_mutex_t m_BufLock;           // used to synchronize buffer operation

   struct Block
   {
      char* m_pcData;                   // pointer to the data block
      int m_iLength;                    // length of the block

      int32_t m_iMsgNo;                 // message number
      uint64_t m_OriginTime;            // original request time
      int m_iTTL;                       // time to live (milliseconds)

      Block* m_pNext;                   // next block
   } *m_pBlock, *m_pFirstBlock, *m_pCurrBlock, *m_pLastBlock;

   // m_pBlock:         The head pointer
   // m_pFirstBlock:    The first block
   // m_pCurrBlock:	The current block
   // m_pLastBlock:     The last block (if first == last, buffer is empty)

   struct Buffer
   {
      char* m_pcData;			// buffer
      int m_iSize;			// size
      Buffer* m_pNext;			// next buffer
   } *m_pBuffer;			// physical buffer

   int32_t m_iNextMsgNo;                // next message number

   int m_iSize;				// buffer size (number of packets)
   int m_iMSS;                          // maximum seqment/packet size

   int m_iCount;			// number of used blocks

private:
   CSndBuffer(const CSndBuffer&);
   CSndBuffer& operator=(const CSndBuffer&);
};

////////////////////////////////////////////////////////////////////////////////

class CRcvBuffer
{
public:
   CRcvBuffer(CUnitQueue* queue, const int& bufsize = 65536);
   ~CRcvBuffer();

      // Functionality:
      //    Write data into the buffer.
      // Parameters:
      //    0) [in] unit: pointer to a data unit containing new packet
      //    1) [in] offset: offset from last ACK point.
      // Returned value:
      //    0 is success, -1 if data is repeated.

   int addData(CUnit* unit, int offset);

      // Functionality:
      //    Read data into a user buffer.
      // Parameters:
      //    0) [in] data: pointer to user buffer.
      //    1) [in] len: length of user buffer.
      // Returned value:
      //    size of data read.

   int readBuffer(char* data, const int& len);

      // Functionality:
      //    Read data directly into file.
      // Parameters:
      //    0) [in] file: C++ file stream.
      //    1) [in] len: expected length of data to write into the file.
      // Returned value:
      //    size of data read.

   int readBufferToFile(std::fstream& ofs, const int& len);

      // Functionality:
      //    Update the ACK point of the buffer.
      // Parameters:
      //    0) [in] len: size of data to be acknowledged.
      // Returned value:
      //    1 if a user buffer is fulfilled, otherwise 0.

   void ackData(const int& len);

      // Functionality:
      //    Query how many buffer space left for data receiving.
      // Parameters:
      //    None.
      // Returned value:
      //    size of available buffer space (including user buffer) for data receiving.

   int getAvailBufSize() const;

      // Functionality:
      //    Query how many data has been continuously received (for reading).
      // Parameters:
      //    None.
      // Returned value:
      //    size of valid (continous) data for reading.

   int getRcvDataSize() const;

      // Functionality:
      //    mark the message to be dropped from the message list.
      // Parameters:
      //    0) [in] msgno: message nuumer.
      // Returned value:
      //    None.

   void dropMsg(const int32_t& msgno);

      // Functionality:
      //    read a message.
      // Parameters:
      //    0) [out] data: buffer to write the message into.
      //    1) [in] len: size of the buffer.
      // Returned value:
      //    actuall size of data read.

   int readMsg(char* data, const int& len);

      // Functionality:
      //    Query how many messages are available now.
      // Parameters:
      //    None.
      // Returned value:
      //    number of messages available for recvmsg.

   int getRcvMsgNum();

private:
   bool scanMsg(int& start, int& end, bool& passack);

private:
   CUnit** m_pUnit;                     // pointer to the protocol buffer
   int m_iSize;                         // size of the protocol buffer
   CUnitQueue* m_pUnitQueue;		// the shared unit queue

   int m_iStartPos;                     // the head position for I/O (inclusive)
   int m_iLastAckPos;                   // the last ACKed position (exclusive)
					// EMPTY: m_iStartPos = m_iLastAckPos   FULL: m_iStartPos = m_iLastAckPos + 1
   int m_iMaxPos;			// the furthest data position

   int m_iNotch;			// the starting read point of the first unit

private:
   CRcvBuffer();
   CRcvBuffer(const CRcvBuffer&);
   CRcvBuffer& operator=(const CRcvBuffer&);
};


#endif
