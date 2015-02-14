/* 
* Queue.h
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


#ifndef	__QUEUE_H__
#define	__QUEUE_H__



#define ENQUEUEPACKET(_Head, _Tail,_Packet)	\
do						\
{                                               \
    if (!_Head) {                           \
        	_Head = _Packet;                \
        } 					\
	else {                                  \
        	(_Tail)->next = _Packet; 	\
        }                                       \
   	(_Packet)->next = NULL;  		\
    _Tail = _Packet;                        \
}while(0)	
#define DEQUEUEPACKET(Head, Tail )            	\
do						\
{   if(Head)			\
	{                                            \
        if (!Head->next) {                      \
        	Tail = NULL;                    \
        }                                       \
        Head = Head->next;                      \
	}		\
}while(0)
#endif	//__QUEUE_H__
