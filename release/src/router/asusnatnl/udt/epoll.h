/*****************************************************************************
Copyright (c) 2001 - 2010, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 08/20/2010
*****************************************************************************/

#ifndef __UDT_EPOLL_H__
#define __UDT_EPOLL_H__


#include <map>
#include <set>
#include "udt.h"


struct CEPollDesc
{
   int m_iID;                                // epoll ID
   std::set<UDTSOCKET> m_sUDTSocksOut;       // set of UDT sockets waiting for write events
   std::set<UDTSOCKET> m_sUDTSocksIn;        // set of UDT sockets waiting for read events

   int m_iLocalID;                           // local system epoll ID
   std::set<SYSSOCKET> m_sLocals;            // set of local (non-UDT) descriptors

   std::set<UDTSOCKET> m_sUDTWrites;         // UDT sockets ready for write
   std::set<UDTSOCKET> m_sUDTReads;          // UDT sockets ready for read
};

class CEPoll
{
friend class CUDT;
friend class CRendezvousQueue;

public:
   CEPoll();
   ~CEPoll();

public: // for CUDTUnited API

      // Functionality:
      //    create a new EPoll.
      // Parameters:
      //    None.
      // Returned value:
      //    new EPoll ID if success, otherwise an error number.

   int create();

      // Functionality:
      //    add a UDT socket to an EPoll.
      // Parameters:
      //    0) [in] eid: EPoll ID.
      //    1) [in] u: UDT Socket ID.
      //    2) [in] events: events to watch.
      // Returned value:
      //    0 if success, otherwise an error number.

   int add_usock(const int eid, const UDTSOCKET& u, const int* events = NULL);

      // Functionality:
      //    add a system socket to an EPoll.
      // Parameters:
      //    0) [in] eid: EPoll ID.
      //    1) [in] s: system Socket ID.
      //    2) [in] events: events to watch.
      // Returned value:
      //    0 if success, otherwise an error number.

   int add_ssock(const int eid, const SYSSOCKET& s, const int* events = NULL);

      // Functionality:
      //    remove a UDT socket event from an EPoll; socket will be removed if no events to watch
      // Parameters:
      //    0) [in] eid: EPoll ID.
      //    1) [in] u: UDT socket ID.
      // Returned value:
      //    0 if success, otherwise an error number.

   int remove_usock(const int eid, const UDTSOCKET& u);

      // Functionality:
      //    remove a system socket event from an EPoll; socket will be removed if no events to watch
      // Parameters:
      //    0) [in] eid: EPoll ID.
      //    1) [in] s: system socket ID.
      // Returned value:
      //    0 if success, otherwise an error number.

   int remove_ssock(const int eid, const SYSSOCKET& s);

      // Functionality:
      //    wait for EPoll events or timeout.
      // Parameters:
      //    0) [in] eid: EPoll ID.
      //    1) [out] readfds: UDT sockets available for reading.
      //    2) [out] writefds: UDT sockets available for writing.
      //    3) [in] msTimeOut: timeout threshold, in milliseconds.
      //    4) [out] lrfds: system file descriptors for reading.
      //    5) [out] lwfds: system file descriptors for writing.
      // Returned value:
      //    number of sockets available for IO.

   int wait(const int eid, std::set<UDTSOCKET>* readfds, std::set<UDTSOCKET>* writefds, int64_t msTimeOut, std::set<SYSSOCKET>* lrfds, std::set<SYSSOCKET>* lwfds);

      // Functionality:
      //    close and release an EPoll.
      // Parameters:
      //    0) [in] eid: EPoll ID.
      // Returned value:
      //    0 if success, otherwise an error number.

   int release(const int eid);

public: // for CUDT to acknowledge IO status

      // Functionality:
      //    set a UDT socket writable.
      // Parameters:
      //    0) [in] uid: UDT socket ID.
      //    1) [in] eids: EPoll IDs to be set
      // Returned value:
      //    0 if success, otherwise an error number.

   int enable_write(const UDTSOCKET& uid, std::set<int>& eids);

      // Functionality:
      //    set a UDT socket readable.
      // Parameters:
      //    0) [in] uid: UDT socket ID.
      //    1) [in] eids: EPoll IDs to be set
      // Returned value:
      //    0 if success, otherwise an error number.

   int enable_read(const UDTSOCKET& uid, std::set<int>& eids);

      // Functionality:
      //    reset a the writable status of a UDT socket.
      // Parameters:
      //    0) [in] uid: UDT socket ID.
      //    1) [in] eids: EPoll IDs to be set
      // Returned value:
      //    0 if success, otherwise an error number.

   int disable_write(const UDTSOCKET& uid, std::set<int>& eids);

      // Functionality:
      //    reset a the readable status of a UDT socket.
      // Parameters:
      //    0) [in] uid: UDT socket ID.
      //    1) [in] eids: EPoll IDs to be set
      // Returned value:
      //    0 if success, otherwise an error number.

   int disable_read(const UDTSOCKET& uid, std::set<int>& eids);

private:
   int m_iIDSeed;                            // seed to generate a new ID
   pthread_mutex_t m_SeedLock;

   std::map<int, CEPollDesc> m_mPolls;       // all epolls
   pthread_mutex_t m_EPollLock;
};


#endif
