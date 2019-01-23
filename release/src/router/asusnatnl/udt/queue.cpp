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
   Yunhong Gu, last updated 05/05/2011
*****************************************************************************/

#ifdef WIN32
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #ifdef LEGACY_WIN32
      #include <wspiapi.h>
   #endif
#endif
#include <cstring>

#include "common.h"
#include "core.h"
#include "queue.h"
#include <pjmedia/natnl_stream.h>

unsigned recvThreadId = 0;
unsigned sendThreadId = 0;

using namespace std;

CUnitQueue::CUnitQueue():
m_pQEntry(NULL),
m_pCurrQueue(NULL),
m_pLastQueue(NULL),
m_iSize(0),
m_iCount(0),
m_iMSS(),
m_iIPversion()
{
}

CUnitQueue::~CUnitQueue()
{
   CQEntry* p = m_pQEntry;

   while (p != NULL)
   {
      delete [] p->m_pUnit;
      delete [] p->m_pBuffer;

      CQEntry* q = p;
      if (p == m_pLastQueue)
         p = NULL;
      else
         p = p->m_pNext;
      delete q;
   }
}

int CUnitQueue::init(const int& size, const int& mss, const int& version)
{
   CQEntry* tempq = NULL;
   CUnit* tempu = NULL;
   char* tempb = NULL;

   try
   {
      tempq = new CQEntry;
      tempu = new CUnit [size];
      tempb = new char [size * mss];
   }
   catch (...)
   {
      delete tempq;
      delete [] tempu;
      delete [] tempb;

      return -1;
   }

   for (int i = 0; i < size; ++ i)
   {
      tempu[i].m_iFlag = 0;
      tempu[i].m_Packet.m_pcData = tempb + i * mss;
   }
   tempq->m_pUnit = tempu;
   tempq->m_pBuffer = tempb;
   tempq->m_iSize = size;

   m_pQEntry = m_pCurrQueue = m_pLastQueue = tempq;
   m_pQEntry->m_pNext = m_pQEntry;

   m_pAvailUnit = m_pCurrQueue->m_pUnit;

   m_iSize = size;
   m_iMSS = mss;
   m_iIPversion = version;

   return 0;
}

int CUnitQueue::increase()
{
   // adjust/correct m_iCount
   int real_count = 0;
   CQEntry* p = m_pQEntry;
   while (p != NULL)
   {
      CUnit* u = p->m_pUnit;
      for (CUnit* end = u + p->m_iSize; u != end; ++ u)
         if (u->m_iFlag != 0)
            ++ real_count;

      if (p == m_pLastQueue)
         p = NULL;
      else
         p = p->m_pNext;
   }
   m_iCount = real_count;
   if (double(m_iCount) / m_iSize < 0.9)
      return -1;

   CQEntry* tempq = NULL;
   CUnit* tempu = NULL;
   char* tempb = NULL;

   // all queues have the same size
   int size = m_pQEntry->m_iSize;

   try
   {
      tempq = new CQEntry;
      tempu = new CUnit [size];
      tempb = new char [size * m_iMSS];
   }
   catch (...)
   {
      delete tempq;
      delete [] tempu;
      delete [] tempb;

      return -1;
   }

   for (int i = 0; i < size; ++ i)
   {
      tempu[i].m_iFlag = 0;
      tempu[i].m_Packet.m_pcData = tempb + i * m_iMSS;
   }
   tempq->m_pUnit = tempu;
   tempq->m_pBuffer = tempb;
   tempq->m_iSize = size;

   m_pLastQueue->m_pNext = tempq;
   m_pLastQueue = tempq;
   m_pLastQueue->m_pNext = m_pQEntry;

   m_iSize += size;

   return 0;
}

int CUnitQueue::shrink()
{
   // currently queue cannot be shrunk.
   return -1;
}

CUnit* CUnitQueue::getNextAvailUnit()
{
   if (m_iCount * 10 > m_iSize * 9)
      increase();

   if (m_iCount >= m_iSize)
      return NULL;

   CQEntry* entrance = m_pCurrQueue;

   do
   {
      for (CUnit* sentinel = m_pCurrQueue->m_pUnit + m_pCurrQueue->m_iSize - 1; m_pAvailUnit != sentinel; ++ m_pAvailUnit)
         if (m_pAvailUnit->m_iFlag == 0)
            return m_pAvailUnit;

      if (m_pCurrQueue->m_pUnit->m_iFlag == 0)
      {
         m_pAvailUnit = m_pCurrQueue->m_pUnit;
         return m_pAvailUnit;
      }

      m_pCurrQueue = m_pCurrQueue->m_pNext;
      m_pAvailUnit = m_pCurrQueue->m_pUnit;
   } while (m_pCurrQueue != entrance);

   increase();

   return NULL;
}


CSndUList::CSndUList():
m_pHeap(NULL),
m_iArrayLength(4096),
m_iLastEntry(-1),
m_ListLock(),
m_pWindowLock(NULL),
m_pWindowCond(NULL),
m_pTimer(NULL)
{
   m_pHeap = new CSNode*[m_iArrayLength];

   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, NULL);
   #else
      m_ListLock = CreateMutex(NULL, false, NULL);
   #endif
}

CSndUList::~CSndUList()
{
   delete [] m_pHeap;

   #ifndef WIN32
      pthread_mutex_destroy(&m_ListLock);
   #else
      CloseHandle(m_ListLock);
   #endif
}

void CSndUList::insert(const int64_t& ts, const CUDT* u)
{
   CGuard listguard(m_ListLock);

   // increase the heap array size if necessary
   if (m_iLastEntry == m_iArrayLength - 1)
   {
      CSNode** temp = NULL;

      try
      {
         temp = new CSNode*[m_iArrayLength * 2];
      }
      catch(...)
      {
         return;
      }

      memcpy(temp, m_pHeap, sizeof(CSNode*) * m_iArrayLength);
      m_iArrayLength *= 2;
      delete [] m_pHeap;
      m_pHeap = temp;
   }

   insert_(ts, u);
}

void CSndUList::update(const CUDT* u, const bool& reschedule)
{
   CGuard listguard(m_ListLock);

   CSNode* n = u->m_pSNode;

   if (n->m_iHeapLoc >= 0)
   {
      if (!reschedule)
         return;

      if (n->m_iHeapLoc == 0)
      {
         n->m_llTimeStamp = 1;
         m_pTimer->interrupt();
         return;
      }

      remove_(u);
   }

   insert_(1, u);
}

int CSndUList::pop(sockaddr*& addr, CPacket& pkt)
{
   CGuard listguard(m_ListLock);

   if (-1 == m_iLastEntry)
      return -1;

   // no pop until the next schedulled time
   uint64_t ts;
   CTimer::rdtsc(ts);
   if (ts < m_pHeap[0]->m_llTimeStamp)
      return -1;

   CUDT* u = m_pHeap[0]->m_pUDT;
   remove_(u);

   if (!u->m_bConnected || u->m_bBroken)
      return -1;

   // pack a packet from the socket
   if (u->packData(pkt, ts) <= 0)
      return -1;

   addr = u->m_pPeerAddr;

   // insert a new entry, ts is the next processing time
   if (ts > 0)
      insert_(ts, u);

   return 1;
}

void CSndUList::remove(const CUDT* u)
{
   CGuard listguard(m_ListLock);

   remove_(u);
}

uint64_t CSndUList::getNextProcTime()
{
   CGuard listguard(m_ListLock);

   if (-1 == m_iLastEntry)
      return 0;

   return m_pHeap[0]->m_llTimeStamp;
}

void CSndUList::insert_(const int64_t& ts, const CUDT* u)
{
   CSNode* n = u->m_pSNode;

   // do not insert repeated node
   if (n->m_iHeapLoc >= 0)
      return;

   m_iLastEntry ++;
   m_pHeap[m_iLastEntry] = n;
   n->m_llTimeStamp = ts;

   int q = m_iLastEntry;
   int p = q;
   while (p != 0)
   {
      p = (q - 1) >> 1;
      if (m_pHeap[p]->m_llTimeStamp > m_pHeap[q]->m_llTimeStamp)
      {
         CSNode* t = m_pHeap[p];
         m_pHeap[p] = m_pHeap[q];
         m_pHeap[q] = t;
         t->m_iHeapLoc = q;
         q = p;
      }
      else
         break;
   }

   n->m_iHeapLoc = q;

   // an earlier event has been inserted, wake up sending worker
   if (n->m_iHeapLoc == 0)
      m_pTimer->interrupt();

   // first entry, activate the sending queue
   if (0 == m_iLastEntry)
   {
      #ifndef WIN32
         pthread_mutex_lock(m_pWindowLock);
         pthread_cond_signal(m_pWindowCond);
         pthread_mutex_unlock(m_pWindowLock);
      #else
         SetEvent(*m_pWindowCond);
      #endif
   }
}

void CSndUList::remove_(const CUDT* u)
{
   CSNode* n = u->m_pSNode;

   if (n->m_iHeapLoc >= 0)
   {
      // remove the node from heap
      m_pHeap[n->m_iHeapLoc] = m_pHeap[m_iLastEntry];
      m_iLastEntry --;
      m_pHeap[n->m_iHeapLoc]->m_iHeapLoc = n->m_iHeapLoc;

      int q = n->m_iHeapLoc;
      int p = q * 2 + 1;
      while (p <= m_iLastEntry)
      {
         if ((p + 1 <= m_iLastEntry) && (m_pHeap[p]->m_llTimeStamp > m_pHeap[p + 1]->m_llTimeStamp))
            p ++;

         if (m_pHeap[q]->m_llTimeStamp > m_pHeap[p]->m_llTimeStamp)
         {
            CSNode* t = m_pHeap[p];
            m_pHeap[p] = m_pHeap[q];
            m_pHeap[p]->m_iHeapLoc = p;
            m_pHeap[q] = t;
            m_pHeap[q]->m_iHeapLoc = q;

            q = p;
            p = q * 2 + 1;
         }
         else
            break;
      }

      n->m_iHeapLoc = -1;
   }

   // the only event has been deleted, wake up immediately
   if (0 == m_iLastEntry)
      m_pTimer->interrupt();
}

//
CSndQueue::CSndQueue():
m_WorkerThread(NULL),
m_pSndUList(NULL),
m_pChannel(NULL),
m_pTimer(NULL),
m_WindowLock(),
m_WindowCond(),
m_bClosing(false),
m_ExitCond()
{
   #ifndef WIN32
      pthread_cond_init(&m_WindowCond, NULL);
      pthread_mutex_init(&m_WindowLock, NULL);
   #else
      m_WindowLock = CreateMutex(NULL, false, NULL);
      m_WindowCond = CreateEvent(NULL, false, false, NULL);
      m_ExitCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CSndQueue::~CSndQueue()
{
   m_bClosing = true;

   #ifndef WIN32
      pthread_mutex_lock(&m_WindowLock);
      pthread_cond_signal(&m_WindowCond);
      pthread_mutex_unlock(&m_WindowLock);
      if (0 != m_WorkerThread) {
         pj_thread_join(m_WorkerThread);
      }
      pthread_cond_destroy(&m_WindowCond);
      pthread_mutex_destroy(&m_WindowLock);
   #else
      SetEvent(m_WindowCond);
      if (NULL != m_WorkerThread)
         WaitForSingleObject(m_ExitCond, INFINITE);
      //CloseHandle(m_WorkerThread);
      CloseHandle(m_WindowLock);
      CloseHandle(m_WindowCond);
      CloseHandle(m_ExitCond);
   #endif

   delete m_pSndUList;
}

void CSndQueue::init(const CChannel* c, const CTimer* t)
{
   m_pChannel = (CChannel*)c;
   m_pTimer = (CTimer*)t;
   m_pSndUList = new CSndUList;
   m_pSndUList->m_pWindowLock = &m_WindowLock;
   m_pSndUList->m_pWindowCond = &m_WindowCond;
   m_pSndUList->m_pTimer = m_pTimer;

   /* Init PJLIB: */
   //if(pj_init())
   //      throw CUDTException(3, 1);

   pjsua_call *call = (pjsua_call *)c->m_call;
   this->m_iInstId = call->inst_id;
	//natnl_stream *strm = (natnl_stream *)c->tnl_stream;
	pj_thread_t *thread;
	int status = pj_thread_create(call->tnl_stream->own_pool, "SndQ", &CSndQueue::worker, 
			  this, 0, 0, &thread);
	if(status)
	 throw CUDTException(3, 1);

	m_WorkerThread = thread;
}

int PJ_THREAD_FUNC CSndQueue::worker(void *param)
{
   CSndQueue* self = (CSndQueue*)param;

#if 1
   pj_thread_desc desc;
   pj_thread_t *thread = 0;
   if (!pj_thread_is_registered(self->m_iInstId)) {
      int status = pj_thread_register(self->m_iInstId, "SndQ", desc, &thread );
      if (status != PJ_SUCCESS)
         throw CUDTException(3, 1);
   }
#endif

#ifdef WIN32
   printf("CSndQueue::worker ThreadId=[%08X]\n", GetCurrentThreadId());
#endif
   while (!self->m_bClosing)
   {
      uint64_t ts = self->m_pSndUList->getNextProcTime();

      if (ts > 0)
      {
         // wait until next processing time of the first socket on the list
         uint64_t currtime;
         CTimer::rdtsc(currtime);
	 if (currtime < ts) 
            self->m_pTimer->sleepto(ts);

         // it is time to send the next pkt
         sockaddr* addr;
         CPacket pkt;
		 //try {
         if (self->m_pSndUList->pop(addr, pkt) < 0)
            continue;
		 //}catch(...) {
		//	 continue;
		 //}
		 //try {
			self->m_pChannel->sendto(addr, pkt);
		 //}catch(...) {
		//	continue;
		 //}
      }
      else
      {
         // wait here if there is no sockets with data to be sent
         #ifndef WIN32
            pthread_mutex_lock(&self->m_WindowLock);
            if (!self->m_bClosing && (self->m_pSndUList->m_iLastEntry < 0))
               pthread_cond_wait(&self->m_WindowCond, &self->m_WindowLock);
            pthread_mutex_unlock(&self->m_WindowLock);
         #else
            WaitForSingleObject(self->m_WindowCond, INFINITE);
         #endif
      }
   }

   #ifndef WIN32
      return 0;
   #else
      SetEvent(self->m_ExitCond);
      return 0;
   #endif
}

int CSndQueue::sendto(const sockaddr* addr, CPacket& packet)
{
   // send out the packet immediately (high priority), this is a control packet
   m_pChannel->sendto(addr, packet);
   return packet.getLength();
}


//
CRcvUList::CRcvUList():
m_pUList(NULL),
m_pLast(NULL)
{
}

CRcvUList::~CRcvUList()
{
}

void CRcvUList::insert(const CUDT* u)
{
   CRNode* n = u->m_pRNode;
   CTimer::rdtsc(n->m_llTimeStamp);

   if (NULL == m_pUList)
   {
      // empty list, insert as the single node
      n->m_pPrev = n->m_pNext = NULL;
      m_pLast = m_pUList = n;

      return;
   }

   // always insert at the end for RcvUList
   n->m_pPrev = m_pLast;
   n->m_pNext = NULL;
   m_pLast->m_pNext = n;
   m_pLast = n;
}

void CRcvUList::remove(const CUDT* u)
{
   CRNode* n = u->m_pRNode;

   if (!n->m_bOnList)
      return;

   if (NULL == n->m_pPrev)
   {
      // n is the first node
      m_pUList = n->m_pNext;
      if (NULL == m_pUList)
         m_pLast = NULL;
      else
         m_pUList->m_pPrev = NULL;
   }
   else
   {
      n->m_pPrev->m_pNext = n->m_pNext;
      if (NULL == n->m_pNext)
      {
         // n is the last node
         m_pLast = n->m_pPrev;
      }
      else
         n->m_pNext->m_pPrev = n->m_pPrev;
   }

   n->m_pNext = n->m_pPrev = NULL;
}

void CRcvUList::update(const CUDT* u)
{
   CRNode* n = u->m_pRNode;

   if (!n->m_bOnList)
      return;

   CTimer::rdtsc(n->m_llTimeStamp);

   // if n is the last node, do not need to change
   if (NULL == n->m_pNext)
      return;

   if (NULL == n->m_pPrev)
   {
      m_pUList = n->m_pNext;
      m_pUList->m_pPrev = NULL;
   }
   else
   {
      n->m_pPrev->m_pNext = n->m_pNext;
      n->m_pNext->m_pPrev = n->m_pPrev;
   }

   n->m_pPrev = m_pLast;
   n->m_pNext = NULL;
   m_pLast->m_pNext = n;
   m_pLast = n;
}

//
CHash::CHash():
m_pBucket(NULL),
m_iHashSize(0)
{
}

CHash::~CHash()
{
   for (int i = 0; i < m_iHashSize; ++ i)
   {
      CBucket* b = m_pBucket[i];
      while (NULL != b)
      {
         CBucket* n = b->m_pNext;
         delete b;
         b = n;
      }
   }

   delete [] m_pBucket;
}

void CHash::init(const int& size)
{
   m_pBucket = new CBucket* [size];

   for (int i = 0; i < size; ++ i)
      m_pBucket[i] = NULL;

   m_iHashSize = size;
}

CUDT* CHash::lookup(const int32_t& id)
{
   // simple hash function (% hash table size); suitable for socket descriptors
   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if (id == b->m_iID)
         return b->m_pUDT;
      b = b->m_pNext;
   }

   return NULL;
}

void CHash::insert(const int32_t& id, const CUDT* u)
{
   CBucket* b = m_pBucket[id % m_iHashSize];

   CBucket* n = new CBucket;
   n->m_iID = id;
   n->m_pUDT = (CUDT*)u;
   n->m_pNext = b;

   m_pBucket[id % m_iHashSize] = n;
}

void CHash::remove(const int32_t& id)
{
   CBucket* b = m_pBucket[id % m_iHashSize];
   CBucket* p = NULL;

   while (NULL != b)
   {
      if (id == b->m_iID)
      {
         if (NULL == p)
            m_pBucket[id % m_iHashSize] = b->m_pNext;
         else
            p->m_pNext = b->m_pNext;

         delete b;

         return;
      }

      p = b;
      b = b->m_pNext;
   }
}


//
CRendezvousQueue::CRendezvousQueue():
m_lRendezvousID(),
m_RIDVectorLock()
{
   #ifndef WIN32
      pthread_mutex_init(&m_RIDVectorLock, NULL);
   #else
      m_RIDVectorLock = CreateMutex(NULL, false, NULL);
   #endif
}

CRendezvousQueue::~CRendezvousQueue()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_RIDVectorLock);
   #else
      CloseHandle(m_RIDVectorLock);
   #endif

   for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++ i)
   {
      if (AF_INET == i->m_iIPversion)
         delete (sockaddr_in*)i->m_pPeerAddr;
      else
         delete (sockaddr_in6*)i->m_pPeerAddr;
   }

   m_lRendezvousID.clear();
}

void CRendezvousQueue::insert(const UDTSOCKET& id, CUDT* u, const int& ipv, const sockaddr* addr, const uint64_t& ttl)
{
   CGuard vg(m_RIDVectorLock);

   CRL r;
   r.m_iID = id;
   r.m_pUDT = u;
   r.m_iIPversion = ipv;
   r.m_pPeerAddr = (AF_INET == ipv) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
   memcpy(r.m_pPeerAddr, addr, (AF_INET == ipv) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
   r.m_ullTTL = ttl;

   m_lRendezvousID.push_back(r);
}

void CRendezvousQueue::remove(const UDTSOCKET& id)
{
   CGuard vg(m_RIDVectorLock);

   for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++ i)
   {
      if (i->m_iID == id)
      {
         if (AF_INET == i->m_iIPversion)
            delete (sockaddr_in*)i->m_pPeerAddr;
         else
            delete (sockaddr_in6*)i->m_pPeerAddr;

         m_lRendezvousID.erase(i);

         return;
      }
   }
}

CUDT* CRendezvousQueue::retrieve(const sockaddr* addr, UDTSOCKET& id)
{
   CGuard vg(m_RIDVectorLock);

   // TODO: optimize search
   for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++ i)
   {
      //if (CIPAddress::ipcmp(addr, i->m_pPeerAddr, i->m_iIPversion) && ((0 == id) || (id == i->m_iID)))
      if ((0 == id) || (id == i->m_iID))
      {
         id = i->m_iID;
         return i->m_pUDT;
      }
   }

   return NULL;
}

void CRendezvousQueue::updateConnStatus()
{
   if (m_lRendezvousID.empty())
      return;

   CGuard vg(m_RIDVectorLock);

   for (list<CRL>::iterator i = m_lRendezvousID.begin(); i != m_lRendezvousID.end(); ++ i)
   {
      // avoid sending too many requests, at most 1 request per 250ms
      if (CTimer::getTime() - i->m_pUDT->m_llLastReqTime > 250000)
      {
         if (CTimer::getTime() >= i->m_ullTTL)
         {
            // connection timer expired, acknowledge app via epoll (UDT send will return error so that apps know this connection has failed)
            i->m_pUDT->m_bConnecting = false;
            CUDT::s_UDTUnited.m_EPoll.enable_write(i->m_iID, i->m_pUDT->m_sPollID);
            continue;
         }

         CPacket request;
         char* reqdata = new char [i->m_pUDT->m_iPayloadSize];
         request.pack(0, NULL, reqdata, i->m_pUDT->m_iPayloadSize);
         // ID = 0, connection request
         request.m_iID = !i->m_pUDT->m_bRendezvous ? 0 : i->m_pUDT->m_ConnRes.m_iID;
         int hs_size = i->m_pUDT->m_iPayloadSize;
         i->m_pUDT->m_ConnReq.serialize(reqdata, hs_size);
         request.setLength(hs_size);
         i->m_pUDT->m_pSndQueue->sendto(i->m_pPeerAddr, request);
         i->m_pUDT->m_llLastReqTime = CTimer::getTime();
         delete [] reqdata;
      }
   }
}

//
CRcvQueue::CRcvQueue():
m_WorkerThread(NULL),
m_TimerThread(NULL),
m_UnitQueue(),
m_pRcvUList(NULL),
m_pHash(NULL),
m_pChannel(NULL),
m_pTimer(NULL),
m_iPayloadSize(),
m_bClosing(false),
m_ExitCond(),
m_TimerExitCond(),
m_LSLock(),
m_pListener(NULL),
m_pRendezvousQueue(NULL),
m_vNewEntry(),
m_IDLock(),
m_mBuffer(),
m_PassLock(),
m_PassCond()
{
   #ifndef WIN32
      pthread_mutex_init(&m_PassLock, NULL);
      pthread_cond_init(&m_PassCond, NULL);
      pthread_mutex_init(&m_LSLock, NULL);
      pthread_mutex_init(&m_IDLock, NULL);
   #else
      m_PassLock = CreateMutex(NULL, false, NULL);
      m_PassCond = CreateEvent(NULL, false, false, NULL);
      m_LSLock = CreateMutex(NULL, false, NULL);
      m_IDLock = CreateMutex(NULL, false, NULL);
      m_ExitCond = CreateEvent(NULL, false, false, NULL);
	  m_TimerExitCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CRcvQueue::~CRcvQueue()
{
   m_bClosing = true;

   #ifndef WIN32
      if (0 != m_WorkerThread) {
         pj_thread_join(m_WorkerThread);
         //pthread_join(m_WorkerThread, NULL);
      }
      if (0 != m_TimerThread) {
         pj_thread_join(m_TimerThread);
         //pthread_join(m_TimerThread, NULL);
      }
      pthread_mutex_destroy(&m_PassLock);
      pthread_cond_destroy(&m_PassCond);
      pthread_mutex_destroy(&m_LSLock);
      pthread_mutex_destroy(&m_IDLock);
   #else
      if (NULL != m_WorkerThread)
         WaitForSingleObject(m_ExitCond, INFINITE);
      if (NULL != m_TimerThread)
         WaitForSingleObject(m_TimerExitCond, INFINITE);
      //CloseHandle(m_WorkerThread);
      CloseHandle(m_PassLock);
      CloseHandle(m_PassCond);
      CloseHandle(m_LSLock);
      CloseHandle(m_IDLock);
      CloseHandle(m_ExitCond);
	  CloseHandle(m_TimerExitCond);
   #endif

   delete m_pRcvUList;
   delete m_pHash;
   delete m_pRendezvousQueue;

   // remove all queued messages
   for (map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.begin(); i != m_mBuffer.end(); ++ i)
   {
      while (!i->second.empty())
      {
         CPacket* pkt = i->second.front();
         delete [] pkt->m_pcData;
         delete pkt;
         i->second.pop();
      }
   }
}

void CRcvQueue::init(const int& qsize, const int& payload, const int& version, const int& hsize, const CChannel* cc, const CTimer* t)
{
   m_iPayloadSize = payload;

   m_UnitQueue.init(qsize, payload, version);

   m_pHash = new CHash;
   m_pHash->init(hsize);

   m_pChannel = (CChannel*)cc;
   m_pChannel->m_pTimer = (CTimer *)t;
   m_pTimer = (CTimer*)t;

   m_pRcvUList = new CRcvUList;
   m_pRendezvousQueue = new CRendezvousQueue;

   /* Init PJLIB: */
   //if(pj_init())
   //      throw CUDTException(3, 1);

   pjsua_call *call = (pjsua_call *)m_pChannel->m_call;
   this->m_iInstId = call->inst_id;
  //natnl_stream *strm = (natnl_stream *)m_pChannel->m_stream;
  pj_thread_t *thread;
  int status = pj_thread_create(call->tnl_stream->own_pool, "RcvQ", &CRcvQueue::worker, 
		      this, 0, 0, &thread);
  if(status)
	  throw CUDTException(3, 1);
  // charles CHARLES mark
  // DEAN commented, for using pj_sem_trywait2
#if 0
  pj_thread_t *thread_tm_chk;
  status = pj_thread_create(call->tnl_stream->own_pool, "TimerChecker", &CRcvQueue::timer_check_worker, 
	  this, 0, 0, &thread_tm_chk);
  if(status)
	  throw CUDTException(3, 1);
#endif
  m_WorkerThread = thread;
  // DEAN commented, for using pj_sem_trywait2
  //m_TimerThread  = thread_tm_chk;
}

int PJ_THREAD_FUNC CRcvQueue::worker(void *param)
{
   CRcvQueue* self = (CRcvQueue*)param;

#if 1
   pj_thread_desc desc;
   pj_thread_t *thread = 0;
   if (!pj_thread_is_registered(self->m_iInstId)) {
      int status = pj_thread_register(self->m_iInstId, "RcvQ", desc, &thread );
      if (status != PJ_SUCCESS)
         throw CUDTException(3, 1);

   }
#endif

   sockaddr* addr = (AF_INET == self->m_UnitQueue.m_iIPversion) ? (sockaddr*) new sockaddr_in : (sockaddr*) new sockaddr_in6;
   CUDT* u = NULL;
   int32_t id;

#ifdef WIN32
   printf("CRcvQueue::worker ThreadId=[%08X]\n", GetCurrentThreadId());
#endif
   while (!self->m_bClosing)
   {
	    // charles add
		//pj_thread_sleep(1);
      #ifdef NO_BUSY_WAITING
         self->m_pTimer->tick();
      #endif

      // check waiting list, if new socket, insert it to the list
      while (self->ifNewEntry())
      {
         CUDT* ne = self->getNewEntry();
         if (NULL != ne)
         {
            self->m_pRcvUList->insert(ne);
            self->m_pHash->insert(ne->m_SocketID, ne);
		 }
      }

      // find next available slot for incoming packet
      CUnit* unit = self->m_UnitQueue.getNextAvailUnit();
      if (NULL == unit)
      {
         // no space, skip this packet
         CPacket temp;
         temp.m_pcData = new char[self->m_iPayloadSize];
         temp.setLength(self->m_iPayloadSize);
         self->m_pChannel->recvfrom(addr, temp);
         delete [] temp.m_pcData;
		 goto TIMER_CHECK;
		 // DEAN commented, for using pj_sem_trywait2
		 //continue;
      }

      unit->m_Packet.setLength(self->m_iPayloadSize);

      // reading next incoming packet, recvfrom returns -1 is nothing has been received
	  if (self->m_pChannel->recvfrom(addr, unit->m_Packet) < 0)
		  goto TIMER_CHECK;
		  // DEAN commented, for using pj_sem_trywait2
		  //continue;

      id = unit->m_Packet.m_iID;

      // ID 0 is for connection request, which should be passed to the listening socket or rendezvous sockets
      if (0 == id)
      {
         if (NULL != self->m_pListener)
            ((CUDT*)self->m_pListener)->listen(addr, unit->m_Packet);
         else if (NULL != (u = self->m_pRendezvousQueue->retrieve(addr, id)))
         {
            // asynchronous connect: call connect here
            // otherwise wait for the UDT socket to retrieve this packet
            if (!u->m_bSynRecving)
               u->connect(unit->m_Packet);
            else
               self->storePkt(id, unit->m_Packet.clone());
         }
      }
      else if (id > 0)
      {
         if (NULL != (u = self->m_pHash->lookup(id)))
         {
            //if (CIPAddress::ipcmp(addr, u->m_pPeerAddr, u->m_iIPversion))
            {
               if (u->m_bConnected && !u->m_bBroken && !u->m_bClosing)
               {
                  if (0 == unit->m_Packet.getFlag())
                     u->processData(unit);
                  else
                     u->processCtrl(unit->m_Packet);

                  u->checkTimers();
                  self->m_pRcvUList->update(u);
               }
            }
         }
         else if (NULL != (u = self->m_pRendezvousQueue->retrieve(addr, id)))
         {
            if (!u->m_bSynRecving)
               u->connect(unit->m_Packet);
            else
               self->storePkt(id, unit->m_Packet.clone());
         }
	  }
// DEAN rollback original mechanism, for using pj_sem_trywait2
#if 1
TIMER_CHECK:
      // take care of the timing event for all UDT sockets

      uint64_t currtime;
      CTimer::rdtsc(currtime);

      CRNode* ul = self->m_pRcvUList->m_pUList;
      uint64_t ctime = currtime - 100000 * CTimer::getCPUFrequency();
      while ((NULL != ul) && (ul->m_llTimeStamp < ctime))
      {
         CUDT* u = ul->m_pUDT;

         if (u->m_bConnected && !u->m_bBroken && !u->m_bClosing)
         {
            u->checkTimers();
            self->m_pRcvUList->update(u);
         }
         else
         {
            // the socket must be removed from Hash table first, then RcvUList
            self->m_pHash->remove(u->m_SocketID);
            self->m_pRcvUList->remove(u);
            u->m_pRNode->m_bOnList = false;
         }

		 ul = self->m_pRcvUList->m_pUList;
      }
#endif
      // Check connection requests status for all sockets in the RendezvousQueue.
      self->m_pRendezvousQueue->updateConnStatus();

   }

   if (AF_INET == self->m_UnitQueue.m_iIPversion)
      delete (sockaddr_in*)addr;
   else
      delete (sockaddr_in6*)addr;

  
   #ifndef WIN32
      return 0;
   #else
      SetEvent(self->m_ExitCond);
      return 0;
   #endif
}

int PJ_THREAD_FUNC CRcvQueue::timer_check_worker(void *param)
{
	CRcvQueue* self = (CRcvQueue*)param;
	//struct natnl_stream *tnl_stream = (struct natnl_stream *)self->m_pChannel->get_tnl_stream();

#if 1
   pj_thread_desc desc;
   pj_thread_t *thread = 0;
   if (!pj_thread_is_registered(self->m_iInstId)) {
      int status = pj_thread_register(self->m_iInstId, "RcvQ_timer_check", desc, &thread );
      if (status != PJ_SUCCESS)
         throw CUDTException(3, 1);

   }
#endif
	while (!self->m_bClosing) {
		// take care of the timing event for all UDT sockets
		uint64_t currtime;
		CTimer::rdtsc(currtime);

		CRNode* ul = self->m_pRcvUList->m_pUList;
		uint64_t ctime = currtime - 100000 * CTimer::getCPUFrequency();
		while( (NULL != ul) && (ul->m_llTimeStamp < ctime))
		{
			CUDT* u = ul->m_pUDT;
			
			#ifdef NO_BUSY_WAITING
				self->m_pTimer->tick();
			#endif

			if (u->m_bConnected && !u->m_bBroken && !u->m_bClosing)
			{
				u->checkTimers();
				self->m_pRcvUList->update(u);
			}
			else
			{
				// the socket must be removed from Hash table first, then RcvUList
				self->m_pHash->remove(u->m_SocketID);
				self->m_pRcvUList->remove(u);
				u->m_pRNode->m_bOnList = false;
			}

			ul = self->m_pRcvUList->m_pUList;

		}
		pj_thread_sleep(10);
	}

	// Check connection requests status for all sockets in the RendezvousQueue.
	//self->m_pRendezvousQueue->updateConnStatus();
	// To end prevent Channel::recvfrom thread for unlocking infinite semaphore wait
	//pj_sem_post(tnl_stream->rbuff_sem);

   #ifndef WIN32
      return 0;
   #else
      SetEvent(self->m_TimerExitCond);
      return 0;
   #endif
}


int CRcvQueue::recvfrom(const int32_t& id, CPacket& packet)
{
   CGuard bufferlock(m_PassLock);

   map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);

   if (i == m_mBuffer.end())
   {
      #ifndef WIN32
         uint64_t now = CTimer::getTime();
         timespec timeout;

         timeout.tv_sec = now / 1000000 + 1;
         timeout.tv_nsec = (now % 1000000) * 1000;

         pthread_cond_timedwait(&m_PassCond, &m_PassLock, &timeout);
      #else
         ReleaseMutex(m_PassLock);
         WaitForSingleObject(m_PassCond, 1000);
         WaitForSingleObject(m_PassLock, INFINITE);
      #endif

      i = m_mBuffer.find(id);
      if (i == m_mBuffer.end())
      {
         packet.setLength(-1);
         return -1;
      }
   }

   // retrieve the earliest packet
   CPacket* newpkt = i->second.front();

   if (packet.getLength() < newpkt->getLength())
   {
      packet.setLength(-1);
      return -1;
   }

   // copy packet content
   memcpy(packet.m_nHeader, newpkt->m_nHeader, CPacket::m_iPktHdrSize);
   memcpy(packet.m_pcData, newpkt->m_pcData, newpkt->getLength());
   packet.setLength(newpkt->getLength());

   delete [] newpkt->m_pcData;
   delete newpkt;

   // remove this message from queue, 
   // if no more messages left for this socket, release its data structure
   i->second.pop();
   if (i->second.empty())
      m_mBuffer.erase(i);

   return packet.getLength();
}

int CRcvQueue::setListener(const CUDT* u)
{
   CGuard lslock(m_LSLock);

   if (NULL != m_pListener)
      return -1;

   m_pListener = (CUDT*)u;
   return 0;
}

void CRcvQueue::removeListener(const CUDT* u)
{
   CGuard lslock(m_LSLock);

   if (u == m_pListener)
      m_pListener = NULL;
}

void CRcvQueue::registerConnector(const UDTSOCKET& id, CUDT* u, const int& ipv, const sockaddr* addr, const uint64_t& ttl)
{
   m_pRendezvousQueue->insert(id, u, ipv, addr, ttl);
}

void CRcvQueue::removeConnector(const UDTSOCKET& id)
{
   m_pRendezvousQueue->remove(id);

   CGuard bufferlock(m_PassLock);

   map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);
   if (i != m_mBuffer.end())
   {
      while (!i->second.empty())
      {
         delete [] i->second.front()->m_pcData;
         delete i->second.front();
         i->second.pop();
      }
      m_mBuffer.erase(i);
   }
}

void CRcvQueue::setNewEntry(CUDT* u)
{
   CGuard listguard(m_IDLock);
   m_vNewEntry.push_back(u);
}

bool CRcvQueue::ifNewEntry()
{
   return !(m_vNewEntry.empty());
}

CUDT* CRcvQueue::getNewEntry()
{
   CGuard listguard(m_IDLock);

   if (m_vNewEntry.empty())
      return NULL;

   CUDT* u = (CUDT*)*(m_vNewEntry.begin());
   m_vNewEntry.erase(m_vNewEntry.begin());

   return u;
}

void CRcvQueue::storePkt(const int32_t& id, CPacket* pkt)
{
   CGuard bufferlock(m_PassLock);   

   map<int32_t, std::queue<CPacket*> >::iterator i = m_mBuffer.find(id);

   if (i == m_mBuffer.end())
   {
      m_mBuffer[id].push(pkt);

      #ifndef WIN32
         pthread_cond_signal(&m_PassCond);
      #else
         SetEvent(m_PassCond);
      #endif
   }
   else
   {
      //avoid storing too many packets, in case of malfunction or attack
      if (i->second.size() > 16)
         return;

      i->second.push(pkt);
   }
}
