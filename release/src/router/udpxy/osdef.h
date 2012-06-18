/* @(#) define OS-specific types/constants
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com) (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UDPXY_OSDEFH_0101082158
#define UDPXY_OSDEFH_0101082158

#include <sys/socket.h>

/* define OS-specific types/constants
 *
 */

#define HAS_SOCKLEN
#define HAS_SETLINEBUF
#define HAS_VARRUN
#define HAS_VARTMP

#ifdef __hpux
    #undef HAS_SOCKLEN
    #undef HAS_SETLINEBUF
    #define NO_SOCKADDR_SA_LEN
#endif

#if defined(__CYGWIN__)
    #define NO_INET6_SUPPORT
    #define NO_SOCKADDR_SA_LEN
#endif

#if defined(__linux)
    #define NO_SOCKADDR_SA_LEN
#endif

#ifndef HAS_SOCKLEN
    typedef int        a_socklen_t;
#else
    typedef socklen_t  a_socklen_t;
#endif

#ifndef HAS_SETLINEBUF
    #define Setlinebuf(a)  setvbuf(a, NULL, _IOLBF, BUFSIZ)
#else
    #define  Setlinebuf(a) setlinebuf(a)
#endif

#if defined(HAS_VARRUN)
    #define PIDFILE_DIR     "/var/run"
#elif defined(HAS_VARTMP)
    #define PIDFILE_DIR     "/var/tmp"
#endif

#endif /* UDPXY_OSDEFH_0101082158 */

/* __EOF__ */

