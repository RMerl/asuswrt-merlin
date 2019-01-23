/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Derrell Lipman 2009
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "includes.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "libsmbclient.h"
#include "libsmb_internal.h"


/* Get rid of the malloc checker */
#ifdef malloc
#undef malloc
#endif

/*
 * Define the functions which implement the pthread interface
 */
SMB_THREADS_DEF_PTHREAD_IMPLEMENTATION(tf);


/**
 * Initialize for threads using the Posix Threads (pthread)
 * implementation. This is a built-in implementation, avoiding the need to
 * implement the component functions of the thread interface. If this function
 * is used, it is not necessary to call smbc_thread_impl().
 *
 * @return {void}
 */
void
smbc_thread_posix(void)
{
        smb_thread_set_functions(&tf);
}

