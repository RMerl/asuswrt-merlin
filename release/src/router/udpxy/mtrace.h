/* @(#) debug tracing facilities 
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
#ifndef _MTRACE_H_09182007_1541_
#define _MTRACE_H_09182007_1541_

/*
 * Evaluate expression if TRACE_MODULE
 * is defined in the enclosing module
 */
#ifdef TRACE_MODULE
    #define TRACE( expr ) (expr)
#else
    #define TRACE( expr ) ((void)0)
#endif

#endif /* _MTRACE_H_09182007_1541_ */

/* __EOF__ */

