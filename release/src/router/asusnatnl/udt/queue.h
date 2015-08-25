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
   Yunhong Gu, last updated 01/12/2011
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include "channel.h"
#include "common.h"
#include "packet.h"
#include <list>
#include <map>
#include <queue>
#include <vector>
#include <pj/os.h>

class CUDT;

struct CUnit
{
   CPacket m_Packet;		// packet
   int m_iFlag;			// 0: free, 1: occupied, 2: msg read but not freed (out-of-order), 3: msg dropped
};

class CUnitQueue
{
friend class CRcvQueue;
friend class CRcvBuffer;

public:
   CUnitQueue();
   ~CUnitQueue();

public:

      // Functionality:
      //    Initialize the unit queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] mss: maximum segament size
      //    3) [in] version: IP version
      // Returned value:
      //    0: success, -1: failure.

   int init(const int& size, const int& mss, const int& version);

      // Functionality:
      //    Increase (double) the unit queue size.
      // Parameters:
      //    None.
      // Returned value:
      //    0: success, -1: failure.

   int increase();

      // Functionality:
      //    Decrease (halve) the unit queue size.
      // Parameters:
      //    None.
      // Returned value:
      //    0: success, -1: failure.

   int shrink();

      // Functionality:
      //    find an available unit for incoming packet.
      // Parameters:
      //    None.
      // Returned value:
      //    Pointer to the available unit, NULL if not found.

   CUnit* getNextAvailUnit();

private:
   struct CQEntry
   {
      CUnit* m_pUnit;		// unit queue
      char* m_pBuffer;		// data buffer
      int m_iSize;		// size of each queue

      CQEntry* m_pNext;
   }
   *m_pQEntry,			// pointer to the first unit queue
   *m_pCurrQueue,		// pointer to the current available queue
   *m_pLastQueue;		// pointer to the last unit queue

   CUnit* m_pAvailUnit;         // recent available unit

   int m_iSize;			// total size of the unit queue, in number of packets
   int m_iCount;		// total number of valid packets in the queue

   int m_iMSS;			// unit buffer size
   int m_iIPversion;		// IP version

private:
   CUnitQueue(const CUnitQueue&);
   CUnitQueue& operator=(const CUnitQueue&);
};

struct CSNode
{
   CUDT* m_pUDT;		// Pointer to the instance of CUDT socket
   uint64_t m_llTimeStamp;      // Time Stamp

   int m_iHeapLoc;		// location on the heap, -1 means not on the heap
};

class CSndUList
{
friend class CSndQueue;

public:
   CSndUList();
   ~CSndUList();

public:

      // Functionality:
      //    Insert a new UDT instance into the list.
      // Parameters:
      //    1) [in] ts: time stamp: next processing time
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int64_t& ts, const CUDT* u);

      // Functionality:
      //    Update the timestamp of the UDT instance on the list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      //    2) [in] resechedule: if the timestampe shoudl be rescheduled
      // Returned value:
      //    None.

   void update(const CUDT* u, const bool& reschedule = true);

      // Functionality:
      //    Retrieve the next packet and peer address from the first entry, and reschedule it in the queue.
      // Parameters:
      //    0) [out] addr: destination address of the next packet
      //    1) [out] pkt: the next packet to be sent
      // Returned value:
      //    1 if successfully retrieved, -1 if no packet found.

   int pop(sockaddr*& addr, CPacket& pkt);

      // Functionality:
      //    Remove UDT instance from the list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void remove(const CUDT* u);

      // Functionality:
      //    Retrieve the next scheduled processing time.
      // Parameters:
      //    None.
      // Returned value:
      //    Scheduled processing time of the first UDT socket in the list.

   uint64_t getNextProcTime();

private:
   void insert_(const int64_t& ts, const CUDT* u);
   void remove_(const CUDT* u);

private:
   CSNode** m_pHeap;			// The heap array
   int m_iArrayLength;			// physical length of the array
   int m_iLastEntry;			// position of last entry on the heap array

   pthread_mutex_t m_ListLock;

   pthread_mutex_t* m_pWindowLock;
   pthread_cond_t* m_pWindowCond;

   CTimer* m_pTimer;

private:
   CSndUList(const CSndUList&);
   CSndUList& operator=(const CSndUList&);
};

struct CRNode
{
   CUDT* m_pUDT;                // Pointer to the instance of CUDT socket
   uint64_t m_llTimeStamp;      // Time Stamp

   CRNode* m_pPrev;             // previous link
   CRNode* m_pNext;             // next link

   bool m_bOnList;              // if the node is already on the list
};

class CRcvUList
{
public:
   CRcvUList();
   ~CRcvUList();

public:

      // Functionality:
      //    Insert a new UDT instance to the list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const CUDT* u);

      // Functionality:
      //    Remove the UDT instance from the list.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void remove(const CUDT* u);

      // Functionality:
      //    Move the UDT instance to the end of the list, if it already exists; otherwise, do nothing.
      // Parameters:
      //    1) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void update(const CUDT* u);

public:
   CRNode* m_pUList;		// the head node

private:
   CRNode* m_pLast;		// the last node

private:
   CRcvUList(const CRcvUList&);
   CRcvUList& operator=(const CRcvUList&);
};

class CHash
{
public:
   CHash();
   ~CHash();

public:

      // Functionality:
      //    Initialize the hash table.
      // Parameters:
      //    1) [in] size: hash table size
      // Returned value:
      //    None.

   void init(const int& size);

      // Functionality:
      //    Look for a UDT instance from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    Pointer to a UDT instance, or NULL if not found.

   CUDT* lookup(const int32_t& id);

      // Functionality:
      //    Insert an entry to the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove an entry from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

private:
   struct CBucket
   {
      int32_t m_iID;		// Socket ID
      CUDT* m_pUDT;		// Socket instance

      CBucket* m_pNext;		// next bucket
   } **m_pBucket;		// list of buckets (the hash table)

   int m_iHashSize;		// size of hash table

private:
   CHash(const CHash&);
   CHash& operator=(const CHash&);
};

class CRendezvousQueue
{
public:
   CRendezvousQueue();
   ~CRendezvousQueue();

public:
   void insert(const UDTSOCKET& id, CUDT* u, const int& ipv, const sockaddr* addr, const uint64_t& ttl);
   void remove(const UDTSOCKET& id);
   CUDT* retrieve(const sockaddr* addr, UDTSOCKET& id);

   void updateConnStatus();

private:
   struct CRL
   {
      UDTSOCKET m_iID;			// UDT socket ID (self)
      CUDT* m_pUDT;			// UDT instance
      int m_iIPversion;                 // IP version
      sockaddr* m_pPeerAddr;		// UDT sonnection peer address
      uint64_t m_ullTTL;			// the time that this request expires
   };
   std::list<CRL> m_lRendezvousID;      // The sockets currently in rendezvous mode

   pthread_mutex_t m_RIDVectorLock;
};

class CSndQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CSndQueue();
   ~CSndQueue();

public:

      // Functionality:
      //    Initialize the sending queue.
      // Parameters:
      //    1) [in] c: UDP channel to be associated to the queue
      //    2) [in] t: Timer
      // Returned value:
      //    None.

   void init(const CChannel* c, const CTimer* t);

      // Functionality:
      //    Send out a packet to a given address.
      // Parameters:
      //    1) [in] addr: destination address
      //    2) [in] packet: packet to be sent out
      // Returned value:
      //    Size of data sent out.

   int sendto(const sockaddr* addr, CPacket& packet);

private:
   static int PJ_THREAD_FUNC worker(void *param);

   pj_thread_t *m_WorkerThread;

private:
   CSndUList* m_pSndUList;		// List of UDT instances for data sending
   CChannel* m_pChannel;                // The UDP channel for data sending
   CTimer* m_pTimer;			// Timing facility

   pthread_mutex_t m_WindowLock;
   pthread_cond_t m_WindowCond;

   volatile bool m_bClosing;		// closing the worker
   pthread_cond_t m_ExitCond;

   int m_iInstId;
private:
   CSndQueue(const CSndQueue&);
   CSndQueue& operator=(const CSndQueue&);
};

class CRcvQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CRcvQueue();
   ~CRcvQueue();

public:

      // Functionality:
      //    Initialize the receiving queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] mss: maximum packet size
      //    3) [in] version: IP version
      //    4) [in] hsize: hash table size
      //    5) [in] c: UDP channel to be associated to the queue
      //    6) [in] t: timer
      // Returned value:
      //    None.

   void init(const int& size, const int& payload, const int& version, const int& hsize, const CChannel* c, const CTimer* t);

      // Functionality:
      //    Read a packet for a specific UDT socket id.
      // Parameters:
      //    1) [in] id: Socket ID
      //    2) [out] packet: received packet
      // Returned value:
      //    Data size of the packet

   int recvfrom(const int32_t& id, CPacket& packet);

private:
	static int PJ_THREAD_FUNC worker(void *param);
	static int PJ_THREAD_FUNC timer_check_worker(void *param);

	pj_thread_t *m_WorkerThread;
	// charles CHARLES mark
	pj_thread_t *m_TimerThread;
private:
   CUnitQueue m_UnitQueue;		// The received packet queue

   CRcvUList* m_pRcvUList;		// List of UDT instances that will read packets from the queue
   CHash* m_pHash;			// Hash table for UDT socket looking up
   CChannel* m_pChannel;		// UDP channel for receving packets
   CTimer* m_pTimer;			// shared timer with the snd queue

   int m_iPayloadSize;                  // packet payload size

   volatile bool m_bClosing;            // closing the workder
   pthread_cond_t m_ExitCond;
   pthread_cond_t m_TimerExitCond;

private:
   int setListener(const CUDT* u);
   void removeListener(const CUDT* u);

   void registerConnector(const UDTSOCKET& id, CUDT* u, const int& ipv, const sockaddr* addr, const uint64_t& ttl);
   void removeConnector(const UDTSOCKET& id);

   void setNewEntry(CUDT* u);
   bool ifNewEntry();
   CUDT* getNewEntry();

   void storePkt(const int32_t& id, CPacket* pkt);

private:
   pthread_mutex_t m_LSLock;
   volatile CUDT* m_pListener;                          // pointer to the (unique, if any) listening UDT entity
   CRendezvousQueue* m_pRendezvousQueue;                // The list of sockets in rendezvous mode

   std::vector<CUDT*> m_vNewEntry;                      // newly added entries, to be inserted
   pthread_mutex_t m_IDLock;

   std::map<int32_t, std::queue<CPacket*> > m_mBuffer;	// temporary buffer for rendezvous connection request
   pthread_mutex_t m_PassLock;
   pthread_cond_t m_PassCond;

   int m_iInstId;

private:
   CRcvQueue(const CRcvQueue&);
   CRcvQueue& operator=(const CRcvQueue&);
};

struct CMultiplexer
{
   CSndQueue* m_pSndQueue;	// The sending queue
   CRcvQueue* m_pRcvQueue;	// The receiving queue
   CChannel* m_pChannel;	// The UDP channel for sending and receiving
   CTimer* m_pTimer;		// The timer

   int m_iPort;			// The UDP port number of this multiplexer
   int m_iIPversion;		// IP version
   int m_iMSS;			// Maximum Segment Size
   int m_iRefCount;		// number of UDT instances that are associated with this multiplexer
   bool m_bReusable;		// if this one can be shared with others

   int m_iID;			// multiplexer ID
};

#endif
