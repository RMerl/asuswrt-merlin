/* 
 * (c) Copyright 1990-1996 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990-1996 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990-1996 DIGITAL EQUIPMENT CORPORATION
 * (c) Copyright 1991, 1992 Siemens-Nixdorf Information Systems
 * To anyone who acknowledges that this file is provided "AS IS" without
 * any express or implied warranty: permission to use, copy, modify, and
 * distribute this file for any purpose is hereby granted without fee,
 * provided that the above copyright notices and this notice appears in
 * all source code copies, and that none of the names listed above be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  None of these organizations
 * makes any representations about the suitability of this software for
 * any purpose.
 */
/*
 *	Header file for thread synchrounous I/O
 */

#ifndef CMA_THREAD_IO
#define CMA_THREAD_IO

/*
 *  INCLUDE FILES
 */

#include <cma_config.h>
#include <sys/select.h>
#include <cma.h>
#include <sys/types.h>
#include <sys/time.h>
#include <cma_init.h>
#include <cma_errors.h>

/*
 * CONSTANTS
 */

/*
 * Maximum number of files (ie, max_fd+1)
 */
#define cma__c_mx_file	FD_SETSIZE

/*
 * Number of bits per file descriptor bit mask (ie number of bytes * bits/byte)
 */
#define cma__c_nbpm	NFDBITS

/*
 * TYPE DEFINITIONS
 */

typedef enum CMA__T_IO_TYPE {
    cma__c_io_read   = 0,
    cma__c_io_write  = 1,
    cma__c_io_except = 2
    } cma__t_io_type;
#define cma__c_max_io_type	2

/*
 * From our local <sys/types.h>:
 *
 *  typedef long    fd_mask;
 *
 *  typedef struct fd_set {
 *          fd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
 *  } fd_set;
 *
 */
typedef fd_mask cma__t_mask;
typedef fd_set  cma__t_file_mask;



/*
 *  GLOBAL DATA
 */

/*
 * Maximum number of files (ie, max_fd+1) as determined by getdtablesize().
 */
extern int	cma__g_mx_file;

/*
 * Number of submasks (ie "int" sized chunks) per file descriptor mask as
 * determined by getdtablesize().
 */
extern int	cma__g_nspm;

/*
 * MACROS
 */

/*
 * Define a constant for the errno value which indicates that the requested
 * operation was not performed because it would block the process.
 */
# define cma__is_blocking(s) \
    ((s == EAGAIN) || (s == EWOULDBLOCK) || (s == EINPROGRESS) || \
     (s == EALREADY) || (s == EDEADLK))

/*
*	It is necessary to issue an I/O function, before calling cma__io_wait()
*	in the following cases:
*
*		*	This file descriptor has been set non-blocking by CMA
*		*	This file descriptor has been set non-blocking by the user.
*/

#define cma__issue_io_call(fd)					\
	( (cma__g_file[fd]->non_blocking) || \
	  (cma__g_file[fd]->user_fl.user_non_blocking) )


#define cma__set_user_nonblocking(flags) \

/*
 * Determine if the file is open
 */
/*
 * If the file gets closed while waiting for the mutex cma__g_file[rfd]
 * gets set to null. This results in a crash if NDEBUG is set to 0 
 * since cma__int_lock tries to dereference it to set the mutex ownership 
 * after it gets the mutex. The following will still set the ownership
 * in cma__int_lock so we'll set it back to noone if cma__g_file is null
 * when we come back just in case it matters. It shouldn't since its no
 * longer in use but..... 
 * Callers of this should recheck cma__g_file after the reservation to
 * make sure continueing makes sense.
 */
#define cma__fd_reserve(rfd) 	\
		{ \
		cma__t_int_mutex *__mutex__; \
		__mutex__ = cma__g_file[rfd]->mutex; \
		cma__int_lock (__mutex__); \
		if(cma__g_file[rfd] == (cma__t_file_obj *)cma_c_null_ptr) \
			cma__int_unlock(__mutex__); \
		}
		

/*
 * Unreserve a file descriptor
 */
#define cma__fd_unreserve(ufd)	cma__int_unlock (cma__g_file[ufd]->mutex)

/*
 * AND together two select file descriptor masks
 */
#define cma__fdm_and(target,a,b)					\
	{								\
	int __i__ = cma__g_nspm;					\
	while (__i__--)							\
	    (target)->fds_bits[__i__] =					\
		(a)->fds_bits[__i__] & (b)->fds_bits[__i__];		\
	}

/*
 * Clear a bit in a select file descriptor mask
 *
 * FD_CLR(n, p)  :=  ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
 */
#define cma__fdm_clr_bit(n,p)	FD_CLR (n, p)

/*
 * Copy the contents of one file descriptor mask into another.  If the 
 * destination operand is null, do nothing; if the source operand is null, 
 * simply zero the destination.
 */
#define cma__fdm_copy(src,dst,nfds) {					\
	if (dst)							\
	    if (src) {							\
		cma__t_mask *__s__ = (cma__t_mask *)(src);		\
		cma__t_mask *__d__ = (cma__t_mask *)(dst);		\
		int __i__;						\
		for (__i__ = 0; __i__ < (nfds); __i__ += cma__c_nbpm)	\
		    *__d__++ = *__s__++;				\
		}							\
	    else							\
		cma__fdm_zero (dst);					\
	    }

/*
 * To increment count for each bit set in fd - mask
 */
#define cma__fdm_count_bits(map,count)					\
	{								\
	int	__i__ = cma__g_nspm;					\
	while (__i__--) {						\
	    cma__t_mask    __tm__;				        \
	    __tm__ = (map)->fds_bits[__i__];				\
	    while(__tm__) {						\
		(count)++;						\
		__tm__ &= ~(__tm__ & (-__tm__)); /* Assumes 2's comp */	\
		}							\
	    }								\
	}

/*
 * Test if a bit is set in a select file descriptor mask
 *
 * FD_ISSET(n,p)  :=  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
 */
#define cma__fdm_is_set(n,p)	FD_ISSET (n, p)

/*
 * OR together two select file descriptor masks
 */
#define cma__fdm_or(target,a,b)						\
	{								\
	int __i__ = cma__g_nspm;					\
	while (__i__--)							\
	    (target)->fds_bits[__i__] =					\
		(a)->fds_bits[__i__] | (b)->fds_bits[__i__];		\
	}

/*
 * Set a bit in a select file descriptor mask
 * 
 * FD_SET(n,p)  :=  ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
 */
#define cma__fdm_set_bit(n,p)	FD_SET (n, p)

/*
 * Clear a select file descriptor mask.
 */
#define cma__fdm_zero(n)						\
	cma__memset ((char *) n, 0, cma__g_nspm * sizeof(cma__t_mask))





/*
 * CMA "thread-synchronous" I/O read/write operations
 */

    /*
     * Since all CMA "thread-synchronous" I/O (read or write) operations on 
     * U*ix follow the exact same structure, the wrapper routines have been
     * condensed into a macro.
     *
     * The steps performed are as follows:
     *	1. Check that the file descriptor is a legitimate value.
     *	2. Check that the entry in the CMA file "database" which corresponds to 
     *	    the file descriptor indicates that the "file" was "opened" by CMA.
     *  3. Reserve the file, to serialized access to files.  This not only 
     *	    simplifies things, but also defends against non-reentrancy.
     *  4. If the "file" is "set" for non-blocking I/O, check if we
     *      have actually set the file non-blocking yet, and if not do so.
     *	    Then, issue the I/O operantion.
     *	    Success or failure is returned immediately, after unreserving the 
     *	    file.  If the error indicates that the operation would have caused
     *	    the process to block, continue to the next step.
     *	5. The I/O prolog adds this "file" to the global bit mask, which 
     *	    represents all "files" which have threads waiting to perform I/O on 
     *	    them, and causes the thread to block on the condition variable for
     *	    this "file".  Periodically, a select is done on this global bit 
     *	    mask, and the condition variables corresponding to "files" which 
     *	    are ready for I/O are signaled, releasing those waiting threads to
     *	    perform their I/O.
     *  6. When the thread returns from the I/O prolog, it can (hopefully) 
     *	    perform its operation without blocking the process.
     *	7. The I/O epilog clears the bit in the global mask and/or signals the 
     *	    the next thread waiting for this "file", as appropriate.
     *  8. If the I/O failed, continue to loop.
     *	9. Finally, the "file" is unreserved, as we're done with it, and the
     *	    result of the operation is returned.
     *
     *
     * Note:  currently, we believe that timeslicing which is based on the
     *	    virtual-time timer does not cause system calls to return EINTR.  
     *	    Threfore, any EINTR returns are relayed directly to the caller.
     *	    On platforms which do not support a virtual-time timer, the code
     *	    should probably catch EINTR returns and restart the system call.
     */

/*
 * This macro is used for both read-type and write-type functions.
 *
 * Note:  the second call to "func" may require being bracketed in a
 *	  cma__interrupt_disable/cma__interrupt_enable pair, but we'll 
 *	  wait and see if this is necessary.
 */
#define cma__ts_func(func,fd,arglist,type,post_process)	{ \
    cma_t_integer   __res__; \
    cma_t_boolean   __done__ = cma_c_false; \
    if ((fd < 0) || (fd >= cma__g_mx_file)) return (cma__set_errno (EBADF), -1); \
    if (!cma__is_open(fd)) return (cma__set_errno (EBADF), -1); \
    cma__fd_reserve (fd); \
    if (!cma__is_open(fd)) return (cma__set_errno (EBADF), -1); \
    if (cma__issue_io_call(fd)) {\
	if ((!cma__g_file[fd]->set_non_blocking) && \
		(cma__g_file[fd]->non_blocking == cma_c_true)) \
	    cma__set_nonblocking(fd); \
        cma__interrupt_disable (0); \
	TRY { \
	    __res__ = func arglist; \
	    } \
	CATCH_ALL { \
	    cma__interrupt_enable (0); \
	    cma__fd_unreserve (fd); \
	    RERAISE; \
	    } \
	ENDTRY \
        cma__interrupt_enable (0); \
	if ((__res__ != -1) \
		|| (!cma__is_blocking (errno)) \
		|| (cma__g_file[fd]->user_fl.user_non_blocking)) \
	    __done__ = cma_c_true; \
	} \
    if (__done__) { \
	cma__fd_unreserve (fd); \
	} \
    else { \
	TRY { \
	    cma__io_prolog (type, fd); \
	    while (!__done__) { \
		cma__io_wait (type, fd); \
		__res__ = func arglist; \
		if ((__res__ != -1) \
			|| (!cma__is_blocking (errno)) \
			|| (cma__g_file[fd]->user_fl.user_non_blocking)) \
		    __done__ = cma_c_true; \
		} \
	    } \
	FINALLY { \
	    cma__io_epilog (type, fd); \
	    cma__fd_unreserve (fd); \
	    } \
	ENDTRY \
	} \
    if (__res__ != -1)  post_process; \
    return __res__;  \
    }

    /*
     * Since most CMA "thread-synchronous" I/O ("open"-type) operations on 
     * U*ix follow the exact same structure, the wrapper routines have been
     * condensed into a macro.
     *
     * The steps performed are as follows:
     *	1. Issue the open function.
     *	2. If the value returned indicates an error, return it to the caller.
     *  3. If the file descriptor returned is larger than what we think is the
     *	    maximum value (ie if it is too big for our database) then bugcheck.
     *  4. "Open" the "file" in the CMA file database.
     *	5. Return the file descriptor value to the caller.
     *
     * FIX-ME: for the time being, if the I/O operation returns EINTR, we 
     *	    simply return it to the caller; eventually, we should catch this 
     *	    and "do the right thing" (if we can figure out what that is).
     */

/*
 * This macro is used for all "open"-type functions which return a single file
 * desciptor by immediate value.
 */
#define cma__ts_open(func,arglist,post_process)  {		\
    int	__fd__;							\
    TRY {							\
	cma__int_init ();					\
	cma__int_lock (cma__g_io_data_mutex);			\
	__fd__ = func arglist;					\
	cma__int_unlock (cma__g_io_data_mutex);			\
	if (__fd__ >= 0 && __fd__ < cma__g_mx_file)		\
	    post_process;					\
	}							\
    CATCH_ALL							\
	{							\
	cma__set_errno (EBADF);					\
	__fd__ = -1;						\
	}							\
    ENDTRY							\
    if (__fd__ >= cma__g_mx_file)				\
	cma__bugcheck ("cma__ts_open:  fd is too large");	\
    return __fd__;						\
    }
/*
 * This macro is used for all "open"-type functions which return a pair of file
 * desciptors by reference parameter.
 */
#define cma__ts_open2(func,fdpair,arglist,post_process)  {		\
    int	    __res__;							\
    TRY {								\
	cma__int_init ();						\
	cma__int_lock (cma__g_io_data_mutex);				\
	__res__ = func arglist;						\
	cma__int_unlock (cma__g_io_data_mutex);				\
	if (__res__ >= 0 && fdpair[0] < cma__g_mx_file			\
		&& fdpair[1] < cma__g_mx_file)				\
	    post_process;						\
	}								\
    CATCH_ALL								\
	{								\
	cma__set_errno (EBADF);						\
	__res__ = -1;							\
	}								\
    ENDTRY								\
    if ((fdpair[0] >= cma__g_mx_file) || (fdpair[1] >= cma__g_mx_file)) \
	cma__bugcheck ("cma__ts_open2:  one of fd's is too large"); \
    return __res__;							\
    }

/*
 * INTERNAL INTERFACES
 */
extern void cma__close_general (int);

extern void
cma__init_thread_io (void);

extern cma_t_boolean cma__io_available  (cma__t_io_type,int,struct timeval *);

extern void cma__io_epilog  (cma__t_io_type,int);

extern void cma__io_prolog  (cma__t_io_type,int);

extern void cma__io_wait  (cma__t_io_type,int);

extern void cma__open_general  (int);

extern void cma__reinit_thread_io  (int);

extern void cma__set_nonblocking  (int);

extern void cma__set_user_nonblock_flags  (int,int);

extern cma_t_boolean
cma__is_open (int fd);


#endif


