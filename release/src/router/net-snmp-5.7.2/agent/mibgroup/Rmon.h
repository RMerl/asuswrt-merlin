/**************************************************************
 * Copyright (C) 2001 Alex Rozin, Optical Access
 * 
 *                     All Rights Reserved
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 * 
 * ALEX ROZIN DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * ALEX ROZIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 ******************************************************************/

/*
 * $Log$
 * Revision 5.0  2002/04/20 07:30:00  hardaker
 * cvs file version number change
 *
 * Revision 1.2  2002/04/20 07:07:34  hardaker
 * White space, oh glorious white space.
 * How great our though?
 * The code is fine.
 * We agree on functionality easily.
 * What really troubles us?
 * Something we can't see.
 * Something between the code.
 * We bow down to your magnificence,
 * For you are everywhere,
 * Between everything.
 * Pretty nothingness you are.
 *
 * Revision 1.1  2001/05/09 19:36:13  slif
 * Include Alex Rozin's Rmon.
 *
 * Revision 1.1.2.1  2001/04/16 14:45:05  alex
 * Rmon1 : first edition
 *
 */

config_require(Rmon/rows)
config_require(Rmon/agutil)
config_require(Rmon/statistics)
/* older implementation: */
/* config_require(Rmon/alarm) */
config_require(Rmon/alarmTable)
config_require(Rmon/history)
config_require(Rmon/event)
config_add_mib(RMON-MIB)

