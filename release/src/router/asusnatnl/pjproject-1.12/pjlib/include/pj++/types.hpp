/* $Id: types.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJPP_TYPES_HPP__
#define __PJPP_TYPES_HPP__

#include <pj/types.h>

class Pj_Pool;
class Pj_Socket ;
class Pj_Lock;


//
// PJLIB initializer.
//
class Pjlib
{
public:
    Pjlib()
    {
        pj_init();
    }
};

//
// Class Pj_Object is declared in pool.hpp
//

//
// Time value wrapper.
//
class Pj_Time_Val : public pj_time_val
{
public:
    Pj_Time_Val()
    {
    }

    Pj_Time_Val(long init_sec, long init_msec)
    {
        sec = init_sec;
        msec = init_msec;
    }

    Pj_Time_Val(const Pj_Time_Val &rhs) 
    { 
        sec=rhs.sec; 
        msec=rhs.msec; 
    }

    explicit Pj_Time_Val(const pj_time_val &tv) 
    { 
        sec = tv.sec; 
        msec = tv.msec; 
    }

    long get_sec()  const    
    { 
        return sec; 
    }

    long get_msec() const    
    { 
        return msec; 
    }

    void set_sec (long s)    
    { 
        sec = s; 
    }

    void set_msec(long ms)   
    { 
        msec = ms; 
        normalize(); 
    }

    long to_msec() const 
    { 
        return PJ_TIME_VAL_MSEC((*this)); 
    }

    bool operator == (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_EQ((*this), rhs);  
    }

    bool operator >  (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_GT((*this), rhs);  
    }

    bool operator >= (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_GTE((*this), rhs); 
    }

    bool operator <  (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_LT((*this), rhs);  
    }

    bool operator <= (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_LTE((*this), rhs); 
    }

    Pj_Time_Val & operator = (const Pj_Time_Val &rhs) 
    {
	sec = rhs.sec;
	msec = rhs.msec;
	return *this;
    }
 
    Pj_Time_Val & operator += (const Pj_Time_Val &rhs) 
    {
	PJ_TIME_VAL_ADD((*this), rhs);
	return *this;
    }

    Pj_Time_Val & operator -= (const Pj_Time_Val &rhs) 
    {
	PJ_TIME_VAL_SUB((*this), rhs);
	return *this;
    }

    /* Must include os.hpp to use these, otherwise unresolved in linking */
    inline pj_status_t	   gettimeofday();
    inline pj_parsed_time  decode();
    inline pj_status_t     encode(const pj_parsed_time *pt);
    inline pj_status_t     to_gmt();
    inline pj_status_t     to_local();


private:
    void normalize() 
    { 
        pj_time_val_normalize(this); 
    }

};

//
// Macro to declare common object comparison operators.
//
#define PJ_DECLARE_OPERATORS(rhs_type)			    \
	    bool operator!=(rhs_type rhs) const {	    \
		return !operator==(rhs); }		    \
	    bool operator<=(rhs_type rhs) const {	    \
		return operator<(rhs) || operator==(rhs); } \
	    bool operator>(rhs_type rhs) const {	    \
		return !operator<=(rhs); }		    \
	    bool operator>=(rhs_type rhs) const {	    \
		return !operator<(rhs); }


#endif	/* __PJPP_TYPES_HPP__ */

