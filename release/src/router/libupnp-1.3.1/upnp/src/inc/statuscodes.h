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

#ifndef GENLIB_NET_HTTP_STATUSCODES_H
#define GENLIB_NET_HTTP_STATUSCODES_H

// HTTP response status codes

#define HTTP_CONTINUE                       100
#define HTTP_SWITCHING_PROCOTOLS            101

#define HTTP_OK                             200
#define HTTP_CREATED                        201
#define HTTP_ACCEPTED                       202
#define HTTP_NON_AUTHORATATIVE              203
#define HTTP_NO_CONTENT                     204
#define HTTP_RESET_CONTENT                  205
#define HTTP_PARTIAL_CONTENT                206

#define HTTP_MULTIPLE_CHOICES               300
#define HTTP_MOVED_PERMANENTLY              301
#define HTTP_FOUND                          302
#define HTTP_SEE_OTHER                      303
#define HTTP_NOT_MODIFIED                   304
#define HTTP_USE_PROXY                      305
#define HTTP_UNUSED_3XX                     306
#define HTTP_TEMPORARY_REDIRECT             307

#define HTTP_BAD_REQUEST                    400
#define HTTP_UNAUTHORIZED                   401
#define HTTP_PAYMENT_REQD                   402
#define HTTP_FORBIDDEN                      403
#define HTTP_NOT_FOUND                      404
#define HTTP_METHOD_NOT_ALLOWED             405
#define HTTP_NOT_ACCEPTABLE                 406
#define HTTP_PROXY_AUTH_REQD                407
#define HTTP_REQUEST_TIMEOUT                408
#define HTTP_CONFLICT                       409
#define HTTP_GONE                           410
#define HTTP_LENGTH_REQUIRED                411
#define HTTP_PRECONDITION_FAILED            412
#define HTTP_REQ_ENTITY_TOO_LARGE           413
#define HTTP_REQ_URI_TOO_LONG               414
#define HTTP_UNSUPPORTED_MEDIA_TYPE         415
#define HTTP_REQUEST_RANGE_NOT_SATISFIABLE  416
#define HTTP_EXPECTATION_FAILED             417

#define HTTP_INTERNAL_SERVER_ERROR          500
#define HTTP_NOT_IMPLEMENTED                501
#define HTTP_BAD_GATEWAY                    502
#define HTTP_SERVICE_UNAVAILABLE            503
#define HTTP_GATEWAY_TIMEOUT                504
#define HTTP_HTTP_VERSION_NOT_SUPPORTED     505

// *********** HTTP lib error codes **********

#define HTTP_E_OUT_OF_MEMORY    -2
#define HTTP_E_BAD_MSG_FORMAT   -3
#define HTTP_E_TIMEDOUT         -4
#define HTTP_E_FILE_READ        -5

// *******************************************

#ifdef __cplusplus
extern "C" {
#endif

const char* http_GetCodeText( int statusCode );

#ifdef __cplusplus
} // extern C
#endif

#endif /* GENLIB_NET_HTTP_STATUSCODES_H */
