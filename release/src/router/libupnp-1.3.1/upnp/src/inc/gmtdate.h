///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef GENLIB_UTIL_GMTDATE_H
#define GENLIB_UTIL_GMTDATE_H

#include <time.h>
#include <genlib/util/util.h>

#ifdef __cplusplus
extern "C" {
#endif

// input: monthStr:  3-letter or full month
// returns month=0..11 or -1 on failure
// output:
//   charsRead - num chars that match the month
//   fullNameMatch - full name match(1) or 3-letter match(0)
//
int ParseMonth( IN const char* monthStr,
    OUT int* charsRead, OUT int* fullNameMatch );

// input: dayOfWeek:  3-letter or full day of week ("mon" etc)
// returns dayOfWeek=0..6 or -1 on failure
// output:
//   charsRead - num chars that match the month
//   fullNameMatch - full name match(1) or 3-letter match(0)
//
int ParseDayOfWeek( IN const char* dayOfWeek,
    OUT int* charsRead, OUT int* fullNameMatch );

// converts date to string format: RFC 1123 format:
// Sun, 06 Nov 1994 08:49:37 GMT
// String returned must be freed using free() function
// returns NULL if date is NULL
//
// throws OutOfMemoryException
char* DateToString( const struct tm* date );

// parses time in fmt hh:mm:ss, military fmt
// returns 0 on success; -1 on error
int ParseTime( const char* s, int* hour, int* minute, int* second );



// tries to parse date according to RFCs 1123, 850, or asctime()
//  format
// params:
//   str - contains date/time in string format
//   dateTime - date and time obtained from 'str'
// returns: 0 on success, -1 on error
int ParseRFC850DateTime( IN const char* str,
    OUT struct tm* dateTime, OUT int* numCharsParsed );

int ParseRFC1123DateTime( IN const char* str,
    OUT struct tm* dateTime, OUT int* numCharsParsed );

int ParseAsctimeFmt( IN const char* str,
    OUT struct tm* dateTime, OUT int* numCharsParsed );

// parses any of these formats: 1123, 850 or asctime()  
int ParseDateTime( IN const char* str,
    OUT struct tm* dateTime, OUT int* numCharsParsed );

#ifdef __cplusplus
}   /* extern C */
#endif

#endif /* GENLIB_UTIL_GMTDATE_H */
