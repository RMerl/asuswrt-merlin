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
   Yunhong Gu, last updated 01/22/2011
*****************************************************************************/

#ifndef __UDT_LIST_H__
#define __UDT_LIST_H__


#include "udt.h"
#include "common.h"


class CSndLossList
{
public:
   CSndLossList(const int& size = 1024);
   ~CSndLossList();

      // Functionality:
      //    Insert a seq. no. into the sender loss list.
      // Parameters:
      //    0) [in] seqno1: sequence number starts.
      //    1) [in] seqno2: sequence number ends.
      // Returned value:
      //    number of packets that are not in the list previously.

   int insert(const int32_t& seqno1, const int32_t& seqno2);

      // Functionality:
      //    Remove ALL the seq. no. that are not greater than the parameter.
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    None.

   void remove(const int32_t& seqno);

      // Functionality:
      //    Read the loss length.
      // Parameters:
      //    None.
      // Returned value:
      //    The length of the list.

   int getLossLength();

      // Functionality:
      //    Read the first (smallest) loss seq. no. in the list and remove it.
      // Parameters:
      //    None.
      // Returned value:
      //    The seq. no. or -1 if the list is empty.

   int32_t getLostSeq();

private:
   int32_t* m_piData1;                  // sequence number starts
   int32_t* m_piData2;                  // seqnence number ends
   int* m_piNext;                       // next node in the list

   int m_iHead;                         // first node
   int m_iLength;                       // loss length
   int m_iSize;                         // size of the static array
   int m_iLastInsertPos;                // position of last insert node

   pthread_mutex_t m_ListLock;          // used to synchronize list operation

private:
   CSndLossList(const CSndLossList&);
   CSndLossList& operator=(const CSndLossList&);
};

////////////////////////////////////////////////////////////////////////////////

class CRcvLossList
{
public:
   CRcvLossList(const int& size = 1024);
   ~CRcvLossList();

      // Functionality:
      //    Insert a series of loss seq. no. between "seqno1" and "seqno2" into the receiver's loss list.
      // Parameters:
      //    0) [in] seqno1: sequence number starts.
      //    1) [in] seqno2: seqeunce number ends.
      // Returned value:
      //    None.

   void insert(const int32_t& seqno1, const int32_t& seqno2);

      // Functionality:
      //    Remove a loss seq. no. from the receiver's loss list.
      // Parameters:
      //    0) [in] seqno: sequence number.
      // Returned value:
      //    if the packet is removed (true) or no such lost packet is found (false).

   bool remove(const int32_t& seqno);

      // Functionality:
      //    Remove all packets between seqno1 and seqno2.
      // Parameters:
      //    0) [in] seqno1: start sequence number.
      //    1) [in] seqno2: end sequence number.
      // Returned value:
      //    if the packet is removed (true) or no such lost packet is found (false).

   bool remove(const int32_t& seqno1, const int32_t& seqno2);

      // Functionality:
      //    Find if there is any lost packets whose sequence number falling seqno1 and seqno2.
      // Parameters:
      //    0) [in] seqno1: start sequence number.
      //    1) [in] seqno2: end sequence number.
      // Returned value:
      //    True if found; otherwise false.

   bool find(const int32_t& seqno1, const int32_t& seqno2) const;

      // Functionality:
      //    Read the loss length.
      // Parameters:
      //    None.
      // Returned value:
      //    the length of the list.

   int getLossLength() const;

      // Functionality:
      //    Read the first (smallest) seq. no. in the list.
      // Parameters:
      //    None.
      // Returned value:
      //    the sequence number or -1 if the list is empty.

   int getFirstLostSeq() const;

      // Functionality:
      //    Get a encoded loss array for NAK report.
      // Parameters:
      //    0) [out] array: the result list of seq. no. to be included in NAK.
      //    1) [out] physical length of the result array.
      //    2) [in] limit: maximum length of the array.
      // Returned value:
      //    None.

   void getLossArray(int32_t* array, int& len, const int& limit);

private:
   int32_t* m_piData1;                  // sequence number starts
   int32_t* m_piData2;                  // sequence number ends
   int* m_piNext;                       // next node in the list
   int* m_piPrior;                      // prior node in the list;

   int m_iHead;                         // first node in the list
   int m_iTail;                         // last node in the list;
   int m_iLength;                       // loss length
   int m_iSize;                         // size of the static array

private:
   CRcvLossList(const CRcvLossList&);
   CRcvLossList& operator=(const CRcvLossList&);
};


#endif
