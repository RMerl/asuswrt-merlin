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

#include "list.h"

CSndLossList::CSndLossList(const int& size):
m_piData1(NULL),
m_piData2(NULL),
m_piNext(NULL),
m_iHead(-1),
m_iLength(0),
m_iSize(size),
m_iLastInsertPos(-1),
m_ListLock()
{
   m_piData1 = new int32_t [m_iSize];
   m_piData2 = new int32_t [m_iSize];
   m_piNext = new int [m_iSize];

   // -1 means there is no data in the node
   for (int i = 0; i < size; ++ i)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }

   // sender list needs mutex protection
   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, 0);
   #else
      m_ListLock = CreateMutex(NULL, false, NULL);
   #endif
}

CSndLossList::~CSndLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_piNext;

   #ifndef WIN32
      pthread_mutex_destroy(&m_ListLock);
   #else
      CloseHandle(m_ListLock);
   #endif
}

int CSndLossList::insert(const int32_t& seqno1, const int32_t& seqno2)
{
   CGuard listguard(m_ListLock);

   if (0 == m_iLength)
   {
      // insert data into an empty list

      m_iHead = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      m_piNext[m_iHead] = -1;
      m_iLastInsertPos = m_iHead;

      m_iLength += CSeqNo::seqlen(seqno1, seqno2);

      return m_iLength;
   }

   // otherwise find the position where the data can be inserted
   int origlen = m_iLength;
   int offset = CSeqNo::seqoff(m_piData1[m_iHead], seqno1);
   int loc = (m_iHead + offset + m_iSize) % m_iSize;

   if (offset < 0)
   {
      // Insert data prior to the head pointer

      m_piData1[loc] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[loc] = seqno2;

      // new node becomes head
      m_piNext[loc] = m_iHead;
      m_iHead = loc;
      m_iLastInsertPos = loc;

      m_iLength += CSeqNo::seqlen(seqno1, seqno2);
   }
   else if (offset > 0)
   {
      if (seqno1 == m_piData1[loc])
      {
         m_iLastInsertPos = loc;

         // first seqno is equivlent, compare the second
         if (-1 == m_piData2[loc])
         {
            if (seqno2 != seqno1)
            {
               m_iLength += CSeqNo::seqlen(seqno1, seqno2) - 1;
               m_piData2[loc] = seqno2;
            }
         }
         else if (CSeqNo::seqcmp(seqno2, m_piData2[loc]) > 0)
         {
            // new seq pair is longer than old pair, e.g., insert [3, 7] to [3, 5], becomes [3, 7]
            m_iLength += CSeqNo::seqlen(m_piData2[loc], seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else
            // Do nothing if it is already there
            return 0;
      }
      else
      {
         // searching the prior node
         int i;
         if ((-1 != m_iLastInsertPos) && (CSeqNo::seqcmp(m_piData1[m_iLastInsertPos], seqno1) < 0))
            i = m_iLastInsertPos;
         else
            i = m_iHead;

         while ((-1 != m_piNext[i]) && (CSeqNo::seqcmp(m_piData1[m_piNext[i]], seqno1) < 0))
            i = m_piNext[i];

         if ((-1 == m_piData2[i]) || (CSeqNo::seqcmp(m_piData2[i], seqno1) < 0))
         {
            m_iLastInsertPos = loc;

            // no overlap, create new node
            m_piData1[loc] = seqno1;
            if (seqno2 != seqno1)
               m_piData2[loc] = seqno2;

            m_piNext[loc] = m_piNext[i];
            m_piNext[i] = loc;

            m_iLength += CSeqNo::seqlen(seqno1, seqno2);
         }
         else
         {
            m_iLastInsertPos = i;

            // overlap, coalesce with prior node, insert(3, 7) to [2, 5], ... becomes [2, 7]
            if (CSeqNo::seqcmp(m_piData2[i], seqno2) < 0)
            {
               m_iLength += CSeqNo::seqlen(m_piData2[i], seqno2) - 1;
               m_piData2[i] = seqno2;

               loc = i;
            }
            else
               return 0;
         }
      }
   }
   else
   {
      m_iLastInsertPos = m_iHead;

      // insert to head node
      if (seqno2 != seqno1)
      {
         if (-1 == m_piData2[loc])
         {
            m_iLength += CSeqNo::seqlen(seqno1, seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else if (CSeqNo::seqcmp(seqno2, m_piData2[loc]) > 0)
         {
            m_iLength += CSeqNo::seqlen(m_piData2[loc], seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else 
            return 0;
      }
      else
         return 0;
   }

   // coalesce with next node. E.g., [3, 7], ..., [6, 9] becomes [3, 9] 
   while ((-1 != m_piNext[loc]) && (-1 != m_piData2[loc]))
   {
      int i = m_piNext[loc];

      if (CSeqNo::seqcmp(m_piData1[i], CSeqNo::incseq(m_piData2[loc])) <= 0)
      {
         // coalesce if there is overlap
         if (-1 != m_piData2[i])
         {
            if (CSeqNo::seqcmp(m_piData2[i], m_piData2[loc]) > 0)
            {
               if (CSeqNo::seqcmp(m_piData2[loc], m_piData1[i]) >= 0)
                  m_iLength -= CSeqNo::seqlen(m_piData1[i], m_piData2[loc]);

               m_piData2[loc] = m_piData2[i];
            }
            else
               m_iLength -= CSeqNo::seqlen(m_piData1[i], m_piData2[i]);
         }
         else
         {
            if (m_piData1[i] == CSeqNo::incseq(m_piData2[loc]))
               m_piData2[loc] = m_piData1[i];
            else
               m_iLength --;
         }

         m_piData1[i] = -1;
         m_piData2[i] = -1;
         m_piNext[loc] = m_piNext[i];
      }
      else
         break;
   }

   return m_iLength - origlen;
}

void CSndLossList::remove(const int32_t& seqno)
{
   CGuard listguard(m_ListLock);

   if (0 == m_iLength)
      return;

   // Remove all from the head pointer to a node with a larger seq. no. or the list is empty
   int offset = CSeqNo::seqoff(m_piData1[m_iHead], seqno);
   int loc = (m_iHead + offset + m_iSize) % m_iSize;

   if (0 == offset)
   {
      // It is the head. Remove the head and point to the next node
      loc = (loc + 1) % m_iSize;

      if (-1 == m_piData2[m_iHead])
         loc = m_piNext[m_iHead];
      else
      {
         m_piData1[loc] = CSeqNo::incseq(seqno);
         if (CSeqNo::seqcmp(m_piData2[m_iHead], CSeqNo::incseq(seqno)) > 0)
            m_piData2[loc] = m_piData2[m_iHead];

         m_piData2[m_iHead] = -1;

         m_piNext[loc] = m_piNext[m_iHead];
      }

      m_piData1[m_iHead] = -1;

      if (m_iLastInsertPos == m_iHead)
         m_iLastInsertPos = -1;

      m_iHead = loc;

      m_iLength --;
   }
   else if (offset > 0)
   {
      int h = m_iHead;

      if (seqno == m_piData1[loc])
      {
         // target node is not empty, remove part/all of the seqno in the node.
         int temp = loc;
         loc = (loc + 1) % m_iSize;

         if (-1 == m_piData2[temp])
            m_iHead = m_piNext[temp];
         else
         {
            // remove part, e.g., [3, 7] becomes [], [4, 7] after remove(3)
            m_piData1[loc] = CSeqNo::incseq(seqno);
            if (CSeqNo::seqcmp(m_piData2[temp], m_piData1[loc]) > 0)
               m_piData2[loc] = m_piData2[temp];
            m_iHead = loc;
            m_piNext[loc] = m_piNext[temp];
            m_piNext[temp] = loc;
            m_piData2[temp] = -1;
         }
      }
      else
      {
         // target node is empty, check prior node
         int i = m_iHead;
         while ((-1 != m_piNext[i]) && (CSeqNo::seqcmp(m_piData1[m_piNext[i]], seqno) < 0))
            i = m_piNext[i];

         loc = (loc + 1) % m_iSize;

         if (-1 == m_piData2[i])
            m_iHead = m_piNext[i];
         else if (CSeqNo::seqcmp(m_piData2[i], seqno) > 0)
         {
            // remove part/all seqno in the prior node
            m_piData1[loc] = CSeqNo::incseq(seqno);
            if (CSeqNo::seqcmp(m_piData2[i], m_piData1[loc]) > 0)
               m_piData2[loc] = m_piData2[i];

            m_piData2[i] = seqno;

            m_piNext[loc] = m_piNext[i];
            m_piNext[i] = loc;

            m_iHead = loc;
         }
         else
            m_iHead = m_piNext[i];
      }

      // Remove all nodes prior to the new head
      while (h != m_iHead)
      {
         if (m_piData2[h] != -1)
         {
            m_iLength -= CSeqNo::seqlen(m_piData1[h], m_piData2[h]);
            m_piData2[h] = -1;
         }
         else
            m_iLength --;

         m_piData1[h] = -1;

         if (m_iLastInsertPos == h)
            m_iLastInsertPos = -1;

         h = m_piNext[h];
      }
   }
}

int CSndLossList::getLossLength()
{
   CGuard listguard(m_ListLock);

   return m_iLength;
}

int32_t CSndLossList::getLostSeq()
{
   if (0 == m_iLength)
     return -1;

   CGuard listguard(m_ListLock);

   if (0 == m_iLength)
     return -1;

   if (m_iLastInsertPos == m_iHead)
      m_iLastInsertPos = -1;

   // return the first loss seq. no.
   int32_t seqno = m_piData1[m_iHead];

   // head moves to the next node
   if (-1 == m_piData2[m_iHead])
   {
      //[3, -1] becomes [], and head moves to next node in the list
      m_piData1[m_iHead] = -1;
      m_iHead = m_piNext[m_iHead];
   }
   else
   {
      // shift to next node, e.g., [3, 7] becomes [], [4, 7]
      int loc = (m_iHead + 1) % m_iSize;

      m_piData1[loc] = CSeqNo::incseq(seqno);
      if (CSeqNo::seqcmp(m_piData2[m_iHead], m_piData1[loc]) > 0)
         m_piData2[loc] = m_piData2[m_iHead];

      m_piData1[m_iHead] = -1;
      m_piData2[m_iHead] = -1;

      m_piNext[loc] = m_piNext[m_iHead];
      m_iHead = loc;
   }

   m_iLength --;

   return seqno;
}

////////////////////////////////////////////////////////////////////////////////

CRcvLossList::CRcvLossList(const int& size):
m_piData1(NULL),
m_piData2(NULL),
m_piNext(NULL),
m_piPrior(NULL),
m_iHead(-1),
m_iTail(-1),
m_iLength(0),
m_iSize(size)
{
   m_piData1 = new int32_t [m_iSize];
   m_piData2 = new int32_t [m_iSize];
   m_piNext = new int [m_iSize];
   m_piPrior = new int [m_iSize];

   // -1 means there is no data in the node
   for (int i = 0; i < size; ++ i)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }
}

CRcvLossList::~CRcvLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_piNext;
   delete [] m_piPrior;
}

void CRcvLossList::insert(const int32_t& seqno1, const int32_t& seqno2)
{
   // Data to be inserted must be larger than all those in the list
   // guaranteed by the UDT receiver

   if (0 == m_iLength)
   {
      // insert data into an empty list
      m_iHead = 0;
      m_iTail = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      m_piNext[m_iHead] = -1;
      m_piPrior[m_iHead] = -1;
      m_iLength += CSeqNo::seqlen(seqno1, seqno2);

      return;
   }

   // otherwise searching for the position where the node should be
   int offset = CSeqNo::seqoff(m_piData1[m_iHead], seqno1);
   int loc = (m_iHead + offset) % m_iSize;

   if ((-1 != m_piData2[m_iTail]) && (CSeqNo::incseq(m_piData2[m_iTail]) == seqno1))
   {
      // coalesce with prior node, e.g., [2, 5], [6, 7] becomes [2, 7]
      loc = m_iTail;
      m_piData2[loc] = seqno2;
   }
   else
   {
      // create new node
      m_piData1[loc] = seqno1;

      if (seqno2 != seqno1)
         m_piData2[loc] = seqno2;

      m_piNext[m_iTail] = loc;
      m_piPrior[loc] = m_iTail;
      m_piNext[loc] = -1;
      m_iTail = loc;
   }

   m_iLength += CSeqNo::seqlen(seqno1, seqno2);
}

bool CRcvLossList::remove(const int32_t& seqno)
{
   if (0 == m_iLength)
      return false; 

   // locate the position of "seqno" in the list
   int offset = CSeqNo::seqoff(m_piData1[m_iHead], seqno);
   if (offset < 0)
      return false;

   int loc = (m_iHead + offset) % m_iSize;

   if (seqno == m_piData1[loc])
   {
      // This is a seq. no. that starts the loss sequence

      if (-1 == m_piData2[loc])
      {
         // there is only 1 loss in the sequence, delete it from the node
         if (m_iHead == loc)
         {
            m_iHead = m_piNext[m_iHead];
            if (-1 != m_iHead)
               m_piPrior[m_iHead] = -1;
         }
         else
         {
            m_piNext[m_piPrior[loc]] = m_piNext[loc];
            if (-1 != m_piNext[loc])
               m_piPrior[m_piNext[loc]] = m_piPrior[loc];
            else
               m_iTail = m_piPrior[loc];
         }

         m_piData1[loc] = -1;
      }
      else
      {
         // there are more than 1 loss in the sequence
         // move the node to the next and update the starter as the next loss inSeqNo(seqno)

         // find next node
         int i = (loc + 1) % m_iSize;

         // remove the "seqno" and change the starter as next seq. no.
         m_piData1[i] = CSeqNo::incseq(m_piData1[loc]);

         // process the sequence end
         if (CSeqNo::seqcmp(m_piData2[loc], CSeqNo::incseq(m_piData1[loc])) > 0)
            m_piData2[i] = m_piData2[loc];

         // remove the current node
         m_piData1[loc] = -1;
         m_piData2[loc] = -1;
 
         // update list pointer
         m_piNext[i] = m_piNext[loc];
         m_piPrior[i] = m_piPrior[loc];

         if (m_iHead == loc)
            m_iHead = i;
         else
            m_piNext[m_piPrior[i]] = i;

         if (m_iTail == loc)
            m_iTail = i;
         else
            m_piPrior[m_piNext[i]] = i;
      }

      m_iLength --;

      return true;
   }

   // There is no loss sequence in the current position
   // the "seqno" may be contained in a previous node

   // searching previous node
   int i = (loc - 1 + m_iSize) % m_iSize;
   while (-1 == m_piData1[i])
      i = (i - 1 + m_iSize) % m_iSize;

   // not contained in this node, return
   if ((-1 == m_piData2[i]) || (CSeqNo::seqcmp(seqno, m_piData2[i]) > 0))
       return false;

   if (seqno == m_piData2[i])
   {
      // it is the sequence end

      if (seqno == CSeqNo::incseq(m_piData1[i]))
         m_piData2[i] = -1;
      else
         m_piData2[i] = CSeqNo::decseq(seqno);
   }
   else
   {
      // split the sequence

      // construct the second sequence from CSeqNo::incseq(seqno) to the original sequence end
      // located at "loc + 1"
      loc = (loc + 1) % m_iSize;

      m_piData1[loc] = CSeqNo::incseq(seqno);
      if (CSeqNo::seqcmp(m_piData2[i], m_piData1[loc]) > 0)      
         m_piData2[loc] = m_piData2[i];

      // the first (original) sequence is between the original sequence start to CSeqNo::decseq(seqno)
      if (seqno == CSeqNo::incseq(m_piData1[i]))
         m_piData2[i] = -1;
      else
         m_piData2[i] = CSeqNo::decseq(seqno);

      // update the list pointer
      m_piNext[loc] = m_piNext[i];
      m_piNext[i] = loc;
      m_piPrior[loc] = i;

      if (m_iTail == i)
         m_iTail = loc;
      else
         m_piPrior[m_piNext[loc]] = loc;
   }

   m_iLength --;

   return true;
}

bool CRcvLossList::remove(const int32_t& seqno1, const int32_t& seqno2)
{
   if (seqno1 <= seqno2)
   {
      for (int32_t i = seqno1; i <= seqno2; ++ i)
         remove(i);
   }
   else
   {
      for (int32_t j = seqno1; j < CSeqNo::m_iMaxSeqNo; ++ j)
         remove(j);
      for (int32_t k = 0; k <= seqno2; ++ k)
         remove(k);
   }

   return true;
}

bool CRcvLossList::find(const int32_t& seqno1, const int32_t& seqno2) const
{
   if (0 == m_iLength)
      return false;

   int p = m_iHead;

   while (-1 != p)
   {
      if ((CSeqNo::seqcmp(m_piData1[p], seqno1) == 0) ||
          ((CSeqNo::seqcmp(m_piData1[p], seqno1) > 0) && (CSeqNo::seqcmp(m_piData1[p], seqno2) <= 0)) ||
          ((CSeqNo::seqcmp(m_piData1[p], seqno1) < 0) && (m_piData2[p] != -1) && CSeqNo::seqcmp(m_piData2[p], seqno1) >= 0))
          return true;

      p = m_piNext[p];
   }

   return false;
}

int CRcvLossList::getLossLength() const
{
   return m_iLength;
}

int CRcvLossList::getFirstLostSeq() const
{
   if (0 == m_iLength)
      return -1;

   return m_piData1[m_iHead];
}

void CRcvLossList::getLossArray(int32_t* array, int& len, const int& limit)
{
   len = 0;

   int i = m_iHead;

   while ((len < limit - 1) && (-1 != i))
   {
      array[len] = m_piData1[i];
      if (-1 != m_piData2[i])
      {
         // there are more than 1 loss in the sequence
         array[len] |= 0x80000000;
         ++ len;
         array[len] = m_piData2[i];
      }

      ++ len;

      i = m_piNext[i];
   }
}
