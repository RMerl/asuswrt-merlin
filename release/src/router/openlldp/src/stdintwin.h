/**
 * Borrowed from Open1x http://open1x.sourceforge.net
 *
 * See LICENSE file for more info.
 *
 * \file stdintwin.h
 *
 * \author chris@open1x.org
 *
 * $Id: stdintwin.h,v 1.2 2010/06/08 05:06:29 condurre Exp $
 * $Date: 2010/06/08 05:06:29 $
 */
#ifndef __STDINTWIN_H__
#define __STDINTWIN_H__

#ifdef WIN32
#define uint8_t  unsigned __int8
#define uint16_t unsigned __int16
#define uint32_t unsigned __int32
#define uint64_t unsigned __int64

#define int8_t  __int8
#define int16_t __int16
#define int32_t __int32
#endif

#endif // __STDINTWIN_H__
