/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __UPNPREPLYPARSE_H__
#define __UPNPREPLYPARSE_H__

#include <stdint.h>
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NameValue {
    LIST_ENTRY(NameValue) entries;
    char name[64];
    char value[];
};

struct NameValueParserData {
    LIST_HEAD(listhead, NameValue) head;
    char curelt[64];
};

#define XML_STORE_EMPTY_FL  0x01

/* ParseNameValue() */
void
ParseNameValue(const char * buffer, int bufsize,
               struct NameValueParserData * data, uint32_t flags);

/* ClearNameValueList() */
void
ClearNameValueList(struct NameValueParserData * pdata);

/* GetValueFromNameValueList() */
char *
GetValueFromNameValueList(struct NameValueParserData * pdata,
                          const char * Name);

/* GetValueFromNameValueListIgnoreNS() */
char *
GetValueFromNameValueListIgnoreNS(struct NameValueParserData * pdata,
                                  const char * Name);

/* DisplayNameValueList() */
#ifdef DEBUG
void
DisplayNameValueList(char * buffer, int bufsize);
#endif

#ifdef __cplusplus
}
#endif

#endif

