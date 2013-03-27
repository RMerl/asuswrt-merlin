/*  bag.h -- ARMulator support code:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

/********************************************************************/
/* bag.h:                                                           */
/* Header file for bag.c                                            */
/* Offers a data structure for storing and getting pairs of number. */
/* The numbers are stored together, put one can be looked up by     */
/* quoting the other.  If a new pair is entered and one of the      */
/* numbers is a repeat of a previous pair, then the previos pair    */
/* is deleted.                                                      */
/********************************************************************/

typedef enum
{
  NO_ERROR,
  DELETED_OLD_PAIR,
  NO_SUCH_PAIR,
}
Bag_error;

void BAG_putpair (long first, long second);

void BAG_newbag (void);
Bag_error BAG_killpair_byfirst (long first);
Bag_error BAG_killpair_bysecond (long second);

Bag_error BAG_getfirst (long *first, long second);
Bag_error BAG_getsecond (long first, long *second);
