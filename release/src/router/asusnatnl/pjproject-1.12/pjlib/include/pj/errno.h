/* $Id: errno.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_ERRNO_H__
#define __PJ_ERRNO_H__

/**
 * @file errno.h
 * @brief PJLIB Error Subsystem
 */
#include <pj/types.h>
#include <pj/compat/errno.h>
#include <stdarg.h>

PJ_BEGIN_DECL

/**
 * @defgroup pj_errno Error Subsystem
 * @{
 *
 * The PJLIB Error Subsystem is a framework to unify all error codes
 * produced by all components into a single error space, and provide
 * uniform set of APIs to access them. With this framework, any error
 * codes are encoded as pj_status_t value. The framework is extensible,
 * application may register new error spaces to be recognized by
 * the framework.
 *
 * @section pj_errno_retval Return Values
 *
 * All functions that returns @a pj_status_t returns @a PJ_SUCCESS if the
 * operation was completed successfully, or non-zero value to indicate 
 * error. If the error came from operating system, then the native error
 * code is translated/folded into PJLIB's error namespace by using
 * #PJ_STATUS_FROM_OS() macro. The function will do this automatically
 * before returning the error to caller.
 *
 * @section err_services Retrieving and Displaying Error Messages
 *
 * The framework provides the following APIs to retrieve and/or display
 * error messages:
 *
 *   - #pj_strerror(): this is the base API to retrieve error string
 *      description for the specified pj_status_t error code.
 *
 *   - #PJ_PERROR() macro: use this macro similar to PJ_LOG to format
 *      an error message and display them to the log
 *
 *   - #pj_perror(): this function is similar to PJ_PERROR() but unlike
 *      #PJ_PERROR(), this function will always be included in the
 *      link process. Due to this reason, prefer to use #PJ_PERROR()
 *      if the application is concerned about the executable size.
 *
 * Application MUST NOT pass native error codes (such as error code from
 * functions like GetLastError() or errno) to PJLIB functions expecting
 * @a pj_status_t.
 *
 * @section err_extending Extending the Error Space
 *
 * Application may register new error space to be recognized by the
 * framework by using #pj_register_strerror(). Use the range started
 * from PJ_ERRNO_START_USER to avoid conflict with existing error
 * spaces.
 *
 */

/**
 * Guidelines on error message length.
 */
#define PJ_ERR_MSG_SIZE  80

/**
 * Buffer for title string of #PJ_PERROR().
 */
#ifndef PJ_PERROR_TITLE_BUF_SIZE
#   define PJ_PERROR_TITLE_BUF_SIZE	120
#endif


/**
 * Get the last platform error/status, folded into pj_status_t.
 * @return	OS dependent error code, folded into pj_status_t.
 * @remark	This function gets errno, or calls GetLastError() function and
 *		convert the code into pj_status_t with PJ_STATUS_FROM_OS. Do
 *		not call this for socket functions!
 * @see	pj_get_netos_error()
 */
PJ_DECL(pj_status_t) pj_get_os_error(void);

/**
 * Set last error.
 * @param code	pj_status_t
 */
PJ_DECL(void) pj_set_os_error(pj_status_t code);

/**
 * Get the last error from socket operations.
 * @return	Last socket error, folded into pj_status_t.
 */
PJ_DECL(pj_status_t) pj_get_netos_error(void);

/**
 * Set error code.
 * @param code	pj_status_t.
 */
PJ_DECL(void) pj_set_netos_error(pj_status_t code);


/**
 * Get the error message for the specified error code. The message
 * string will be NULL terminated.
 *
 * @param statcode  The error code.
 * @param buf	    Buffer to hold the error message string.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pj_strerror( pj_status_t statcode, 
			       char *buf, pj_size_t bufsize);

/**
 * A utility macro to print error message pertaining to the specified error 
 * code to the log. This macro will construct the error message title 
 * according to the 'title_fmt' argument, and add the error string pertaining
 * to the error code after the title string. A colon (':') will be added 
 * automatically between the title and the error string.
 *
 * This function is similar to pj_perror() function, but has the advantage
 * that the function call can be omitted from the link process if the
 * log level argument is below PJ_LOG_MAX_LEVEL threshold.
 *
 * Note that the title string constructed from the title_fmt will be built on
 * a string buffer which size is PJ_PERROR_TITLE_BUF_SIZE, which normally is
 * allocated from the stack. By default this buffer size is small (around
 * 120 characters). Application MUST ensure that the constructed title string
 * will not exceed this limit, since not all platforms support truncating
 * the string.
 *
 * @see pj_perror()
 *
 * @param level	    The logging verbosity level, valid values are 0-6. Lower
 *		    number indicates higher importance, with level zero 
 *		    indicates fatal error. Only numeral argument is 
 *		    permitted (e.g. not variable).
 * @param arg	    Enclosed 'printf' like arguments, with the following
 *		    arguments:
 *		     - the sender (NULL terminated string),
 *		     - the error code (pj_status_t)
 *		     - the format string (title_fmt), and 
 *		     - optional variable number of arguments suitable for the 
 *		       format string.
 *
 * Sample:
 * \verbatim
   PJ_PERROR(2, (__FILE__, PJ_EBUSY, "Error making %s", "coffee"));
   \endverbatim
 * @hideinitializer
 */
#define PJ_PERROR(level,arg)	do { \
				    pj_perror_wrapper_##level(arg); \
				} while (0)

#define PJ_PERROR_GOTO(level, lebel, arg)	do { \
				    pj_perror_wrapper_##level(arg); \
					goto lebel; \
				} while (0)

/**
 * A utility function to print error message pertaining to the specified error 
 * code to the log. This function will construct the error message title 
 * according to the 'title_fmt' argument, and add the error string pertaining
 * to the error code after the title string. A colon (':') will be added 
 * automatically between the title and the error string.
 *
 * Unlike the PJ_PERROR() macro, this function takes the \a log_level argument
 * as a normal argument, unlike in PJ_PERROR() where a numeral value must be
 * given. However this function will always be linked to the executable,
 * unlike PJ_PERROR() which can be omitted when the level is below the 
 * PJ_LOG_MAX_LEVEL.
 *
 * Note that the title string constructed from the title_fmt will be built on
 * a string buffer which size is PJ_PERROR_TITLE_BUF_SIZE, which normally is
 * allocated from the stack. By default this buffer size is small (around
 * 120 characters). Application MUST ensure that the constructed title string
 * will not exceed this limit, since not all platforms support truncating
 * the string.
 *
 * @see PJ_PERROR()
 */
PJ_DECL(void) pj_perror(int log_level, const char *sender, pj_status_t status,
		        const char *title_fmt, ...);


/**
 * Type of callback to be specified in #pj_register_strerror()
 *
 * @param e	    The error code to lookup.
 * @param msg	    Buffer to store the error message.
 * @param max	    Length of the buffer.
 *
 * @return	    The error string.
 */
typedef pj_str_t (*pj_error_callback)(pj_status_t e, char *msg, pj_size_t max);


/**
 * Register strerror message handler for the specified error space.
 * Application can register its own handler to supply the error message
 * for the specified error code range. This handler will be called
 * by #pj_strerror().
 *
 * @param start_code	The starting error code where the handler should
 *			be called to retrieve the error message.
 * @param err_space	The size of error space. The error code range then
 *			will fall in start_code to start_code+err_space-1
 *			range.
 * @param f		The handler to be called when #pj_strerror() is
 *			supplied with error code that falls into this range.
 *
 * @return		PJ_SUCCESS or the specified error code. The 
 *			registration may fail when the error space has been
 *			occupied by other handler, or when there are too many
 *			handlers registered to PJLIB.
 */
PJ_DECL(pj_status_t) pj_register_strerror(pj_status_t start_code,
					  pj_status_t err_space,
					  pj_error_callback f);

/**
 * @hideinitializer
 * Return platform os error code folded into pj_status_t code. This is
 * the macro that is used throughout the library for all PJLIB's functions
 * that returns error from operating system. Application may override
 * this macro to reduce size (e.g. by defining it to always return 
 * #PJ_EUNKNOWN).
 *
 * Note:
 *  This macro MUST return non-zero value regardless whether zero is
 *  passed as the argument. The reason is to protect logic error when
 *  the operating system doesn't report error codes properly.
 *
 * @param os_code   Platform OS error code. This value may be evaluated
 *		    more than once.
 * @return	    The platform os error code folded into pj_status_t.
 */
#ifndef PJ_RETURN_OS_ERROR
#   define PJ_RETURN_OS_ERROR(os_code)   (os_code ? \
					    PJ_STATUS_FROM_OS(os_code) : -1)
#endif


/**
 * @hideinitializer
 * Fold a platform specific error into an pj_status_t code.
 *
 * @param e	The platform os error code.
 * @return	pj_status_t
 * @warning	Macro implementation; the syserr argument may be evaluated
 *		multiple times.
 */
#if PJ_NATIVE_ERR_POSITIVE
#   define PJ_STATUS_FROM_OS(e) (e == 0 ? PJ_SUCCESS : e + PJ_ERRNO_START_SYS)
#else
#   define PJ_STATUS_FROM_OS(e) (e == 0 ? PJ_SUCCESS : PJ_ERRNO_START_SYS - e)
#endif

/**
 * @hideinitializer
 * Fold an pj_status_t code back to the native platform defined error.
 *
 * @param e	The pj_status_t folded platform os error code.
 * @return	pj_os_err_type
 * @warning	macro implementation; the statcode argument may be evaluated
 *		multiple times.  If the statcode was not created by 
 *		pj_get_os_error or PJ_STATUS_FROM_OS, the results are undefined.
 */
#if PJ_NATIVE_ERR_POSITIVE
#   define PJ_STATUS_TO_OS(e) (e == 0 ? PJ_SUCCESS : e - PJ_ERRNO_START_SYS)
#else
#   define PJ_STATUS_TO_OS(e) (e == 0 ? PJ_SUCCESS : PJ_ERRNO_START_SYS - e)
#endif


/**
 * @defgroup pj_errnum PJLIB's Own Error Codes
 * @ingroup pj_errno
 * @{
 */

/**
 * Use this macro to generate error message text for your error code,
 * so that they look uniformly as the rest of the libraries.
 *
 * @param code	The error code
 * @param msg	The error test.
 */
#ifndef PJ_BUILD_ERR
#   define PJ_BUILD_ERR(code,msg) { code, msg " (" #code ")" }
#endif


/**
 * @hideinitializer
 * Unknown error has been reported.
 */
#define PJ_EUNKNOWN	    (PJ_ERRNO_START_STATUS + 1)	/* 70001 */
/**
 * @hideinitializer
 * The operation is pending and will be completed later.
 */
#define PJ_EPENDING	    (PJ_ERRNO_START_STATUS + 2)	/* 70002 */
/**
 * @hideinitializer
 * Too many connecting sockets.
 */
#define PJ_ETOOMANYCONN	    (PJ_ERRNO_START_STATUS + 3)	/* 70003 */
/**
 * @hideinitializer
 * Invalid argument.
 */
#define PJ_EINVAL	    (PJ_ERRNO_START_STATUS + 4)	/* 70004 */
/**
 * @hideinitializer
 * Name too long (eg. hostname too long).
 */
#define PJ_ENAMETOOLONG	    (PJ_ERRNO_START_STATUS + 5)	/* 70005 */
/**
 * @hideinitializer
 * Not found.
 */
#define PJ_ENOTFOUND	    (PJ_ERRNO_START_STATUS + 6)	/* 70006 */
/**
 * @hideinitializer
 * Not enough memory.
 */
#define PJ_ENOMEM	    (PJ_ERRNO_START_STATUS + 7)	/* 70007 */
/**
 * @hideinitializer
 * Bug detected!
 */
#define PJ_EBUG             (PJ_ERRNO_START_STATUS + 8)	/* 70008 */
/**
 * @hideinitializer
 * Operation timed out.
 */
#define PJ_ETIMEDOUT        (PJ_ERRNO_START_STATUS + 9)	/* 70009 */
/**
 * @hideinitializer
 * Too many objects.
 */
#define PJ_ETOOMANY         (PJ_ERRNO_START_STATUS + 10)/* 70010 */
/**
 * @hideinitializer
 * Object is busy.
 */
#define PJ_EBUSY            (PJ_ERRNO_START_STATUS + 11)/* 70011 */
/**
 * @hideinitializer
 * The specified option is not supported.
 */
#define PJ_ENOTSUP	    (PJ_ERRNO_START_STATUS + 12)/* 70012 */
/**
 * @hideinitializer
 * Invalid operation.
 */
#define PJ_EINVALIDOP	    (PJ_ERRNO_START_STATUS + 13)/* 70013 */
/**
 * @hideinitializer
 * Operation is cancelled.
 */
#define PJ_ECANCELLED	    (PJ_ERRNO_START_STATUS + 14)/* 70014 */
/**
 * @hideinitializer
 * Object already exists.
 */
#define PJ_EEXISTS          (PJ_ERRNO_START_STATUS + 15)/* 70015 */
/**
 * @hideinitializer
 * End of file.
 */
#define PJ_EEOF		    (PJ_ERRNO_START_STATUS + 16)/* 70016 */
/**
 * @hideinitializer
 * Size is too big.
 */
#define PJ_ETOOBIG	    (PJ_ERRNO_START_STATUS + 17)/* 70017 */
/**
 * @hideinitializer
 * Error in gethostbyname(). This is a generic error returned when
 * gethostbyname() has returned an error.
 */
#define PJ_ERESOLVE	    (PJ_ERRNO_START_STATUS + 18)/* 70018 */
/**
 * @hideinitializer
 * Size is too small.
 */
#define PJ_ETOOSMALL	    (PJ_ERRNO_START_STATUS + 19)/* 70019 */
/**
 * @hideinitializer
 * Ignored
 */
#define PJ_EIGNORED	    (PJ_ERRNO_START_STATUS + 20)/* 70020 */
/**
 * @hideinitializer
 * IPv6 is not supported
 */
#define PJ_EIPV6NOTSUP	    (PJ_ERRNO_START_STATUS + 21)/* 70021 */
/**
 * @hideinitializer
 * Unsupported address family
 */
#define PJ_EAFNOTSUP	    (PJ_ERRNO_START_STATUS + 22)/* 70022 */
/**
 * @hideinitializer
 * Object no longer exists
 */
#define PJ_EGONE	    (PJ_ERRNO_START_STATUS + 23)/* 70023 */

/** @} */   /* pj_errnum */

/** @} */   /* pj_errno */


/**
 * PJ_ERRNO_START is where PJLIB specific error values start.
 */
#define PJ_ERRNO_START		20000

/**
 * PJ_ERRNO_SPACE_SIZE is the maximum number of errors in one of 
 * the error/status range below.
 */
#define PJ_ERRNO_SPACE_SIZE	50000

/**
 * PJ_ERRNO_START_STATUS is where PJLIB specific status codes start.
 * Effectively the error in this class would be 70000 - 119000.
 */
#define PJ_ERRNO_START_STATUS	(PJ_ERRNO_START + PJ_ERRNO_SPACE_SIZE)

/**
 * PJ_ERRNO_START_SYS converts platform specific error codes into
 * pj_status_t values.
 * Effectively the error in this class would be 120000 - 169000.
 */
#define PJ_ERRNO_START_SYS	(PJ_ERRNO_START_STATUS + PJ_ERRNO_SPACE_SIZE)

/**
 * PJ_ERRNO_START_USER are reserved for applications that use error
 * codes along with PJLIB codes.
 * Effectively the error in this class would be 170000 - 219000.
 */
#define PJ_ERRNO_START_USER	(PJ_ERRNO_START_SYS + PJ_ERRNO_SPACE_SIZE)


/*
 * Below are list of error spaces that have been taken so far:
 *  - PJSIP_ERRNO_START		(PJ_ERRNO_START_USER)
 *  - PJMEDIA_ERRNO_START	(PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE)
 *  - PJSIP_SIMPLE_ERRNO_START	(PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*2)
 *  - PJLIB_UTIL_ERRNO_START	(PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*3)
 *  - PJNATH_ERRNO_START	(PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*4)
 *  - PJMEDIA_AUDIODEV_ERRNO_START (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*5)
 */

/* Internal */
void pj_errno_clear_handlers(void);


/****** Internal for PJ_PERROR *******/

/**
 * @def pj_perror_wrapper_1(arg)
 * Internal function to write log with verbosity 1. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 1.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 1
    #define pj_perror_wrapper_1(arg)	pj_perror_1 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_1(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_1(arg)
#endif

/**
 * @def pj_perror_wrapper_2(arg)
 * Internal function to write log with verbosity 2. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 2.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 2
    #define pj_perror_wrapper_2(arg)	pj_perror_2 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_2(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_2(arg)
#endif

/**
 * @def pj_perror_wrapper_3(arg)
 * Internal function to write log with verbosity 3. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 3.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 3
    #define pj_perror_wrapper_3(arg)	pj_perror_3 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_3(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_3(arg)
#endif

/**
 * @def pj_perror_wrapper_4(arg)
 * Internal function to write log with verbosity 4. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 4.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 4
    #define pj_perror_wrapper_4(arg)	pj_perror_4 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_4(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_4(arg)
#endif

/**
 * @def pj_perror_wrapper_5(arg)
 * Internal function to write log with verbosity 5. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 5.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 5
    #define pj_perror_wrapper_5(arg)	pj_perror_5 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_5(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_5(arg)
#endif

/**
 * @def pj_perror_wrapper_6(arg)
 * Internal function to write log with verbosity 6. Will evaluate to
 * empty expression if PJ_LOG_MAX_LEVEL is below 6.
 * @param arg       Log expression.
 */
#if PJ_LOG_MAX_LEVEL >= 6
    #define pj_perror_wrapper_6(arg)	pj_perror_6 arg
    /** Internal function. */
    PJ_DECL(void) pj_perror_6(const char *sender, pj_status_t status, 
			      const char *title_fmt, ...);
#else
    #define pj_perror_wrapper_6(arg)
#endif




PJ_END_DECL

#endif	/* __PJ_ERRNO_H__ */

