/* $Id: except.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_EXCEPTION_H__
#define __PJ_EXCEPTION_H__

/**
 * @file except.h
 * @brief Exception Handling in C.
 */

#include <pj/types.h>
#include <pj/compat/setjmp.h>
#include <pj/log.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJ_EXCEPT Exception Handling
 * @ingroup PJ_MISC
 * @{
 *
 * \section pj_except_sample_sec Quick Example
 *
 * For the impatient, take a look at some examples:
 *  - @ref page_pjlib_samples_except_c
 *  - @ref page_pjlib_exception_test
 *
 * \section pj_except_except Exception Handling
 *
 * This module provides exception handling syntactically similar to C++ in
 * C language. In Win32 systems, it uses Windows Structured Exception
 * Handling (SEH) if macro PJ_EXCEPTION_USE_WIN32_SEH is non-zero.
 * Otherwise it will use setjmp() and longjmp().
 *
 * On some platforms where setjmp/longjmp is not available, setjmp/longjmp 
 * implementation is provided. See <pj/compat/setjmp.h> for compatibility.
 *
 * The exception handling mechanism is completely thread safe, so the exception
 * thrown by one thread will not interfere with other thread.
 *
 * The exception handling constructs are similar to C++. The blocks will be
 * constructed similar to the following sample:
 *
 * \verbatim
   #define NO_MEMORY     1
   #define SYNTAX_ERROR  2
  
   int sample1()
   {
      PJ_USE_EXCEPTION;  // declare local exception stack.
  
      PJ_TRY {
        ...// do something..
      }
      PJ_CATCH(NO_MEMORY) {
        ... // handle exception 1
      }
      PJ_END;
   }

   int sample2()
   {
      PJ_USE_EXCEPTION;  // declare local exception stack.
  
      PJ_TRY {
        ...// do something..
      }
      PJ_CATCH_ANY {
         if (PJ_GET_EXCEPTION() == NO_MEMORY)
	    ...; // handle no memory situation
	 else if (PJ_GET_EXCEPTION() == SYNTAX_ERROR)
	    ...; // handle syntax error
      }
      PJ_END;
   }
   \endverbatim
 *
 * The above sample uses hard coded exception ID. It is @b strongly
 * recommended that applications request a unique exception ID instead
 * of hard coded value like above.
 *
 * \section pj_except_reg Exception ID Allocation
 *
 * To ensure that exception ID (number) are used consistently and to
 * prevent ID collisions in an application, it is strongly suggested that 
 * applications allocate an exception ID for each possible exception
 * type. As a bonus of this process, the application can identify
 * the name of the exception when the particular exception is thrown.
 *
 * Exception ID management are performed with the following APIs:
 *  - #pj_exception_id_alloc().
 *  - #pj_exception_id_free().
 *  - #pj_exception_id_name().
 *
 *
 * PJLIB itself automatically allocates one exception id, i.e.
 * #PJ_NO_MEMORY_EXCEPTION which is declared in <pj/pool.h>. This exception
 * ID is raised by default pool policy when it fails to allocate memory.
 *
 * CAVEATS:
 *  - unlike C++ exception, the scheme here won't call destructors of local
 *    objects if exception is thrown. Care must be taken when a function
 *    hold some resorce such as pool or mutex etc.
 *  - You CAN NOT make nested exception in one single function without using
 *    a nested PJ_USE_EXCEPTION. Samples:
  \verbatim
	void wrong_sample()
	{
	    PJ_USE_EXCEPTION;

	    PJ_TRY {
		// Do stuffs
		...
	    }
	    PJ_CATCH_ANY {
		// Do other stuffs
		....
		..

		// The following block is WRONG! You MUST declare 
		// PJ_USE_EXCEPTION once again in this block.
		PJ_TRY {
		    ..
		}
		PJ_CATCH_ANY {
		    ..
		}
		PJ_END;
	    }
	    PJ_END;
	}

  \endverbatim

 *  - You MUST NOT exit the function inside the PJ_TRY block. The correct way
 *    is to return from the function after PJ_END block is executed. 
 *    For example, the following code will yield crash not in this code,
 *    but rather in the subsequent execution of PJ_TRY block:
  \verbatim
        void wrong_sample()
	{
	    PJ_USE_EXCEPTION;

	    PJ_TRY {
		// do some stuffs
		...
		return;	        <======= DO NOT DO THIS!
	    }
	    PJ_CATCH_ANY {
	    }
	    PJ_END;
	}
  \endverbatim
  
 *  - You can not provide more than PJ_CATCH or PJ_CATCH_ANY nor use PJ_CATCH
 *    and PJ_CATCH_ANY for a single PJ_TRY.
 *  - Exceptions will always be caught by the first handler (unlike C++ where
 *    exception is only caught if the type matches.

 * \section PJ_EX_KEYWORDS Keywords
 *
 * \subsection PJ_THROW PJ_THROW(expression)
 * Throw an exception. The expression thrown is an integer as the result of
 * the \a expression. This keyword can be specified anywhere within the 
 * program.
 *
 * \subsection PJ_USE_EXCEPTION PJ_USE_EXCEPTION
 * Specify this in the variable definition section of the function block 
 * (or any blocks) to specify that the block has \a PJ_TRY/PJ_CATCH exception 
 * block. 
 * Actually, this is just a macro to declare local variable which is used to
 * push the exception state to the exception stack.
 * Note: you must specify PJ_USE_EXCEPTION as the last statement in the
 * local variable declarations, since it may evaluate to nothing.
 *
 * \subsection PJ_TRY PJ_TRY
 * The \a PJ_TRY keyword is typically followed by a block. If an exception is
 * thrown in this block, then the execution will resume to the \a PJ_CATCH 
 * handler.
 *
 * \subsection PJ_CATCH PJ_CATCH(expression)
 * The \a PJ_CATCH is normally followed by a block. This block will be executed
 * if the exception being thrown is equal to the expression specified in the
 * \a PJ_CATCH.
 *
 * \subsection PJ_CATCH_ANY PJ_CATCH_ANY
 * The \a PJ_CATCH is normally followed by a block. This block will be executed
 * if any exception was raised in the TRY block.
 *
 * \subsection PJ_END PJ_END
 * Specify this keyword to mark the end of \a PJ_TRY / \a PJ_CATCH blocks.
 *
 * \subsection PJ_GET_EXCEPTION PJ_GET_EXCEPTION(void)
 * Get the last exception thrown. This macro is normally called inside the
 * \a PJ_CATCH or \a PJ_CATCH_ANY block, altough it can be used anywhere where
 * the \a PJ_USE_EXCEPTION definition is in scope.
 *
 * 
 * \section pj_except_examples_sec Examples
 *
 * For some examples on how to use the exception construct, please see:
 *  - @ref page_pjlib_samples_except_c
 *  - @ref page_pjlib_exception_test
 */

/**
 * Allocate a unique exception id.
 * Applications don't have to allocate a unique exception ID before using
 * the exception construct. However, by doing so it ensures that there is
 * no collisions of exception ID.
 *
 * As a bonus, when exception number is acquired through this function,
 * the library can assign name to the exception (only if 
 * PJ_HAS_EXCEPTION_NAMES is enabled (default is yes)) and find out the
 * exception name when it catches an exception.
 *
 * @param inst_id   The instance id of pjsua.
 * @param name      Name to be associated with the exception ID.
 * @param id        Pointer to receive the ID.
 *
 * @return          PJ_SUCCESS on success or PJ_ETOOMANY if the library 
 *                  is running out out ids.
 */
PJ_DECL(pj_status_t) pj_exception_id_alloc(int inst_id, const char *name,
                                           pj_exception_id_t *id);

/**
 * Free an exception id.
 *
 * @param inst_id   The instance id of pjsua.
 * @param id        The exception ID.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_exception_id_free(int inst_id, pj_exception_id_t id);

/**
 * Retrieve name associated with the exception id.
 *
 * @param inst_id   The instance id of pjsua.
 * @param id        The exception ID.
 *
 * @return          The name associated with the specified ID.
 */
PJ_DECL(const char*) pj_exception_id_name(int inst_id, pj_exception_id_t id);


/** @} */

#if defined(PJ_EXCEPTION_USE_WIN32_SEH) && PJ_EXCEPTION_USE_WIN32_SEH != 0
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING WINDOWS SEH
 **
 ****************************************************************************/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

PJ_IDECL_NO_RETURN(void)
pj_throw_exception_(pj_exception_id_t id) PJ_ATTR_NORETURN
{
    RaiseException(id,1,0,NULL);
}

#define PJ_USE_EXCEPTION    
#define PJ_TRY		    __try
#define PJ_CATCH(id)	    __except(GetExceptionCode()==id ? \
				      EXCEPTION_EXECUTE_HANDLER : \
				      EXCEPTION_CONTINUE_SEARCH)
#define PJ_CATCH_ANY	    __except(EXCEPTION_EXECUTE_HANDLER)
#define PJ_END		    
#define PJ_THROW(id)	    pj_throw_exception_(id)
#define PJ_GET_EXCEPTION()  GetExceptionCode()


#elif defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING SYMBIAN LEAVE/TRAP FRAMEWORK
 **
 ****************************************************************************/

/* To include this file, the source file must be compiled as
 * C++ code!
 */
#ifdef __cplusplus

class TPjException
{
public:
    int code_;
};

#define PJ_USE_EXCEPTION
#define PJ_TRY			try
//#define PJ_CATCH(id)		
#define PJ_CATCH_ANY		catch (const TPjException & pj_excp_)
#define PJ_END
#define PJ_THROW(x_id)		do { TPjException e; e.code_=x_id; throw e;} \
				while (0)
#define PJ_GET_EXCEPTION()	pj_excp_.code_

#else

#define PJ_USE_EXCEPTION
#define PJ_TRY				
#define PJ_CATCH_ANY		if (0)
#define PJ_END
#define PJ_THROW(x_id)		do { PJ_LOG(1,("PJ_THROW"," error code = %d",x_id)); } while (0)
#define PJ_GET_EXCEPTION()	0

#endif	/* __cplusplus */

#else
/*****************************************************************************
 **
 ** IMPLEMENTATION OF EXCEPTION USING GENERIC SETJMP/LONGJMP
 **
 ****************************************************************************/

/**
 * This structure (which should be invisible to user) manages the TRY handler
 * stack.
 */
struct pj_exception_state_t
{
    struct pj_exception_state_t *prev;  /**< Previous state in the list. */
    pj_jmp_buf state;                   /**< jmp_buf.                    */
};

/**
 * Throw exception.
 * @param id    Exception Id.
 */
PJ_DECL_NO_RETURN(void) 
pj_throw_exception_(int inst_id, pj_exception_id_t id) PJ_ATTR_NORETURN;

/**
 * Push exception handler.
 */
PJ_DECL(void) pj_push_exception_handler_(int inst_id, struct pj_exception_state_t *rec);

/**
 * Pop exception handler.
 */
PJ_DECL(void) pj_pop_exception_handler_(int inst_id, struct pj_exception_state_t *rec);

/**
 * Declare that the function will use exception.
 * @hideinitializer
 */
#define PJ_USE_EXCEPTION    struct pj_exception_state_t pj_x_except__; int pj_x_code__

/**
 * Start exception specification block.
 * @hideinitializer
 */
#define PJ_TRY(inst_id)		    if (1) { \
				pj_push_exception_handler_(inst_id, &pj_x_except__); \
				pj_x_code__ = pj_setjmp(pj_x_except__.state); \
				if (pj_x_code__ == 0)
/**
 * Catch the specified exception Id.
 * @param id    The exception number to catch.
 * @hideinitializer
 */
#define PJ_CATCH(id)	    else if (pj_x_code__ == (id))

/**
 * Catch any exception number.
 * @hideinitializer
 */
#define PJ_CATCH_ANY	    else

/**
 * End of exception specification block.
 * @hideinitializer
 */
#define PJ_END(inst_id)			pj_pop_exception_handler_(inst_id, &pj_x_except__); \
			    } else {}

/**
 * Throw exception.
 * @param exception_id  The exception number.
 * @hideinitializer
 */
#define PJ_THROW(inst_id, exception_id)	pj_throw_exception_(inst_id, exception_id)

/**
 * Get current exception.
 * @return      Current exception code.
 * @hideinitializer
 */
#define PJ_GET_EXCEPTION()	(pj_x_code__)

#endif	/* PJ_EXCEPTION_USE_WIN32_SEH */


PJ_END_DECL



#endif	/* __PJ_EXCEPTION_H__ */


