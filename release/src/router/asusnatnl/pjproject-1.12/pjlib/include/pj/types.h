/* $Id: types.h 3987 2012-03-23 08:24:58Z bennylp $ */
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
#ifndef __PJ_TYPES_H__
#define __PJ_TYPES_H__


/**
 * @file types.h
 * @brief Declaration of basic types and utility.
 */
/**
 * @defgroup PJ_BASIC Basic Data Types and Library Functionality.
 * @ingroup PJ_DS
 * @{
 */
#include <pj/config.h>

PJ_BEGIN_DECL

/* ************************************************************************* */

/** Signed 32bit integer. */
typedef int		pj_int32_t;

/** Unsigned 32bit integer. */
typedef unsigned int	pj_uint32_t;

/** Signed 16bit integer. */
typedef short		pj_int16_t;

/** Unsigned 16bit integer. */
typedef unsigned short	pj_uint16_t;

/** Signed 8bit integer. */
typedef signed char	pj_int8_t;

/** Unsigned 8bit integer. */
typedef unsigned char	pj_uint8_t;

/** Large unsigned integer. */
typedef size_t		pj_size_t;

/** Large signed integer. */
typedef long		pj_ssize_t;

/** Status code. */
typedef int		pj_status_t;

/** Boolean. */
typedef int		pj_bool_t;

/** Native char type, which will be equal to wchar_t for Unicode
 * and char for ANSI. */
#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0
    typedef wchar_t pj_char_t;
#else
    typedef char pj_char_t;
#endif

/** This macro creates Unicode or ANSI literal string depending whether
 *  native platform string is Unicode or ANSI. */
#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0
#   define PJ_T(literal_str)	L##literal_str
#else
#   define PJ_T(literal_str)	literal_str
#endif


/** Status is OK. */
#define PJ_SUCCESS  0

/** True value. */
#define PJ_TRUE	    1

/** False value. */
#define PJ_FALSE    0

/**
 * File offset type.
 */
#if defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
typedef pj_int64_t pj_off_t;
#else
typedef pj_ssize_t pj_off_t;
#endif

/* ************************************************************************* */
/*
 * Data structure types.
 */
/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library. By convention, the string is NOT null terminated.
 */
struct pj_str_t
{
    /** Buffer pointer, which is by convention NOT null terminated. */
    char       *ptr;

    /** The length of the string. */
    pj_ssize_t  slen;
};

/**
 * This structure represents high resolution (64bit) time value. The time
 * values represent time in cycles, which is retrieved by calling
 * #pj_get_timestamp().
 */
//charles debug
#pragma pack(1)
//charles debug
typedef union pj_timestamp
{
    struct
    {
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
	pj_uint32_t lo;     /**< Low 32-bit value of the 64-bit value. */
	pj_uint32_t hi;     /**< high 32-bit value of the 64-bit value. */
#else
	pj_uint32_t hi;     /**< high 32-bit value of the 64-bit value. */
	pj_uint32_t lo;     /**< Low 32-bit value of the 64-bit value. */
#endif
    } u32;                  /**< The 64-bit value as two 32-bit values. */

#if PJ_HAS_INT64
    pj_uint64_t u64;        /**< The whole 64-bit value, where available. */
#endif
} pj_timestamp;



/**
 * The opaque data type for linked list, which is used as arguments throughout
 * the linked list operations.
 */
typedef void pj_list_type;

/** 
 * List.
 */
typedef struct pj_list pj_list;

/**
 * Opaque data type for hash tables.
 */
typedef struct pj_hash_table_t pj_hash_table_t;

/**
 * Opaque data type for hash entry (only used internally by hash table).
 */
typedef struct pj_hash_entry pj_hash_entry;

/**
 * Data type for hash search iterator.
 * This structure should be opaque, however applications need to declare
 * concrete variable of this type, that's why the declaration is visible here.
 */
typedef struct pj_hash_iterator_t
{
    pj_uint32_t	     index;     /**< Internal index.     */
    pj_hash_entry   *entry;     /**< Internal entry.     */
} pj_hash_iterator_t;
//charles debug
#pragma pack()
//charles debug

/**
 * Forward declaration for memory pool factory.
 */
typedef struct pj_pool_factory pj_pool_factory;

/**
 * Opaque data type for memory pool.
 */
typedef struct pj_pool_t pj_pool_t;

/**
 * Forward declaration for caching pool, a pool factory implementation.
 */
typedef struct pj_caching_pool pj_caching_pool;

/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library.
 */
typedef struct pj_str_t pj_str_t;

/**
 * Opaque data type for I/O Queue structure.
 */
typedef struct pj_ioqueue_t pj_ioqueue_t;

/**
 * Opaque data type for key that identifies a handle registered to the
 * I/O queue framework.
 */
typedef struct pj_ioqueue_key_t pj_ioqueue_key_t;

/**
 * Opaque data to identify timer heap.
 */
typedef struct pj_timer_heap_t pj_timer_heap_t;

/** 
 * Opaque data type for atomic operations.
 */
typedef struct pj_atomic_t pj_atomic_t;

/**
 * Value type of an atomic variable.
 */
typedef PJ_ATOMIC_VALUE_TYPE pj_atomic_value_t;
 
/* ************************************************************************* */

/** Thread handle. */
typedef struct pj_thread_t pj_thread_t;

/** Lock object. */
typedef struct pj_lock_t pj_lock_t;

/** Group lock */
typedef struct pj_grp_lock_t pj_grp_lock_t;

/** Mutex handle. */
typedef struct pj_mutex_t pj_mutex_t;

/** Semaphore handle. */
typedef struct pj_sem_t pj_sem_t;

/** Event object. */
typedef struct pj_event_t pj_event_t;

/** Unidirectional stream pipe object. */
typedef struct pj_pipe_t pj_pipe_t;

/** Operating system handle. */
typedef void *pj_oshandle_t;

/** Socket handle. */
#if defined(PJ_WIN64) && PJ_WIN64!=0
typedef pj_int64_t pj_sock_t;
#else
typedef long pj_sock_t;
#endif

/** Generic socket address. */
typedef void pj_sockaddr_t;

/** Forward declaration. */
typedef struct pj_sockaddr_in pj_sockaddr_in;

/** Color type. */
typedef unsigned int pj_color_t;

/** Exception id. */
typedef int pj_exception_id_t;

/* ************************************************************************* */

/** Utility macro to compute the number of elements in static array. */
#define PJ_ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))

/** Maximum value for signed 32-bit integer. */
#define PJ_MAXINT32  0x7FFFFFFFL

/**
 * Length of object names.
 */
#define PJ_MAX_OBJ_NAME	32

/* ************************************************************************* */
/*
 * General.
 */
/**
 * Initialize the PJ Library.
 * This function must be called before using the library. The purpose of this
 * function is to initialize static library data, such as character table used
 * in random string generation, and to initialize operating system dependent
 * functionality (such as WSAStartup() in Windows).
 *
 * Apart from calling pj_init(), application typically should also initialize
 * the random seed by calling pj_srand().
 *
 * @param inst_id  The instance id of pjsua.
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_init(int inst_id);


/**
 * Shutdown PJLIB.
 */
PJ_DECL(void) pj_shutdown(int inst_id);

/**
 * Type of callback to register to pj_atexit().
 */
typedef void (*pj_exit_callback)(int inst_id);

/**
 * Register cleanup function to be called by PJLIB when pj_shutdown() is 
 * called.
 *
 * @param inst_id	The instance id of pjsua.
 * @param func	    The function to be registered.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_atexit(int inst_id, pj_exit_callback func);



/**
 * Swap the byte order of an 16bit data.
 *
 * @param val16	    The 16bit data.
 *
 * @return	    An 16bit data with swapped byte order.
 */
PJ_INLINE(pj_int16_t) pj_swap16(pj_int16_t val16)
{
    pj_uint8_t *p = (pj_uint8_t*)&val16;
    pj_uint8_t tmp = *p;
    *p = *(p+1);
    *(p+1) = tmp;
    return val16;
}

/**
 * Swap the byte order of an 32bit data.
 *
 * @param val32	    The 32bit data.
 *
 * @return	    An 32bit data with swapped byte order.
 */
PJ_INLINE(pj_int32_t) pj_swap32(pj_int32_t val32)
{
    pj_uint8_t *p = (pj_uint8_t*)&val32;
    pj_uint8_t tmp = *p;
    *p = *(p+3);
    *(p+3) = tmp;
    tmp = *(p+1);
    *(p+1) = *(p+2);
    *(p+2) = tmp;
    return val32;
}


/**
 * @}
 */
/**
 * @addtogroup PJ_TIME Time Data Type and Manipulation.
 * @ingroup PJ_MISC
 * @{
 */

/**
 * Representation of time value in this library.
 * This type can be used to represent either an interval or a specific time
 * or date. 
 */
typedef struct pj_time_val
{
    /** The seconds part of the time. */
    long    sec;

    /** The miliseconds fraction of the time. */
    long    msec;

} pj_time_val;

typedef struct pj_time_val2
{
    /** The seconds part of the time. */
    long    sec;

    /** The microseconds fraction of the time. */
    long    usec;
} pj_time_val2;

/**
 * Normalize the value in time value.
 * @param t     Time value to be normalized.
 */
PJ_DECL(void) pj_time_val_normalize(pj_time_val *t);

/**
 * Get the total time value in miliseconds. This is the same as
 * multiplying the second part with 1000 and then add the miliseconds
 * part to the result.
 *
 * @param t     The time value.
 * @return      Total time in miliseconds.
 * @hideinitializer
 */
#define PJ_TIME_VAL_MSEC(t)	((t).sec * 1000 + (t).msec)

/**
 * This macro will check if \a t1 is equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if both time values are equal.
 * @hideinitializer
 */
#define PJ_TIME_VAL_EQ(t1, t2)	((t1).sec==(t2).sec && (t1).msec==(t2).msec)

/**
 * This macro will check if \a t1 is greater than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_GT(t1, t2)	((t1).sec>(t2).sec || \
                                ((t1).sec==(t2).sec && (t1).msec>(t2).msec))

/**
 * This macro will check if \a t1 is greater than or equal to \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than or equal to t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_GTE(t1, t2)	(PJ_TIME_VAL_GT(t1,t2) || \
                                 PJ_TIME_VAL_EQ(t1,t2))

/**
 * This macro will check if \a t1 is less than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_LT(t1, t2)	(!(PJ_TIME_VAL_GTE(t1,t2)))

/**
 * This macro will check if \a t1 is less than or equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than or equal to t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_LTE(t1, t2)	(!PJ_TIME_VAL_GT(t1, t2))

/**
 * Add \a t2 to \a t1 and store the result in \a t1. Effectively
 *
 * this macro will expand as: (\a t1 += \a t2).
 * @param t1    The time value to add.
 * @param t2    The time value to be added to \a t1.
 * @hideinitializer
 */
#define PJ_TIME_VAL_ADD(t1, t2)	    do {			    \
					(t1).sec += (t2).sec;	    \
					(t1).msec += (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * Substract \a t2 from \a t1 and store the result in \a t1. Effectively
 * this macro will expand as (\a t1 -= \a t2).
 *
 * @param t1    The time value to subsctract.
 * @param t2    The time value to be substracted from \a t1.
 * @hideinitializer
 */
#define PJ_TIME_VAL_SUB(t1, t2)	    do {			    \
					(t1).sec -= (t2).sec;	    \
					(t1).msec -= (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * This structure represent the parsed representation of time.
 * It is acquired by calling #pj_time_decode().
 */
typedef struct pj_parsed_time
{
    /** This represents day of week where value zero means Sunday */
    int wday;

    /* This represents day of the year, 0-365, where zero means
     *  1st of January.
     */
    /*int yday; */

    /** This represents day of month: 1-31 */
    int day;

    /** This represents month, with the value is 0 - 11 (zero is January) */
    int mon;

    /** This represent the actual year (unlike in ANSI libc where
     *  the value must be added by 1900).
     */
    int year;

    /** This represents the second part, with the value is 0-59 */
    int sec;

    /** This represents the minute part, with the value is: 0-59 */
    int min;

    /** This represents the hour part, with the value is 0-23 */
    int hour;

    /** This represents the milisecond part, with the value is 0-999 */
    int msec;

} pj_parsed_time;


/**
 * @}	// Time Management
 */

/* ************************************************************************* */
/*
 * Terminal.
 */
/**
 * Color code combination.
 */
enum {
    PJ_TERM_COLOR_R	= 2,    /**< Red            */
    PJ_TERM_COLOR_G	= 4,    /**< Green          */
    PJ_TERM_COLOR_B	= 1,    /**< Blue.          */
    PJ_TERM_COLOR_BRIGHT = 8    /**< Bright mask.   */
};




PJ_END_DECL


#endif /* __PJ_TYPES_H__ */

