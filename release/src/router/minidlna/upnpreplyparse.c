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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "upnpreplyparse.h"
#include "minixml.h"

static void
NameValueParserStartElt(void * d, const char * name, int l)
{
    struct NameValueParserData * data = (struct NameValueParserData *)d;
    if(l>63)
        l = 63;
    memcpy(data->curelt, name, l);
    data->curelt[l] = '\0';

    /* store root element */
    if(!data->head.lh_first)
    {
        struct NameValue * nv;
        nv = malloc(sizeof(struct NameValue)+l+1);
        strcpy(nv->name, "rootElement");
        memcpy(nv->value, name, l);
        nv->value[l] = '\0';
        LIST_INSERT_HEAD(&(data->head), nv, entries);
    }
}

static void
NameValueParserGetData(void * d, const char * datas, int l)
{
    struct NameValueParserData * data = (struct NameValueParserData *)d;
    struct NameValue * nv;
    if(l>1975)
        l = 1975;
    nv = malloc(sizeof(struct NameValue)+l+1);
    strncpy(nv->name, data->curelt, 64);
    nv->name[63] = '\0';
    memcpy(nv->value, datas, l);
    nv->value[l] = '\0';
    LIST_INSERT_HEAD(&(data->head), nv, entries);
}

void
ParseNameValue(const char * buffer, int bufsize,
                    struct NameValueParserData * data)
{
    struct xmlparser parser;
    LIST_INIT(&(data->head));
    /* init xmlparser object */
    parser.xmlstart = buffer;
    parser.xmlsize = bufsize;
    parser.data = data;
    parser.starteltfunc = NameValueParserStartElt;
    parser.endeltfunc = 0;
    parser.datafunc = NameValueParserGetData;
    parser.attfunc = 0;
    parsexml(&parser);
}

void
ClearNameValueList(struct NameValueParserData * pdata)
{
    struct NameValue * nv;
    while((nv = pdata->head.lh_first) != NULL)
    {
        LIST_REMOVE(nv, entries);
        free(nv);
    }
}

char * 
GetValueFromNameValueList(struct NameValueParserData * pdata,
                          const char * Name)
{
    struct NameValue * nv;
    char * p = NULL;
    for(nv = pdata->head.lh_first;
        (nv != NULL) && (p == NULL);
        nv = nv->entries.le_next)
    {
        if(strcmp(nv->name, Name) == 0)
            p = nv->value;
    }
    return p;
}

/* debug all-in-one function 
 * do parsing then display to stdout */
#ifdef DEBUG
void
DisplayNameValueList(char * buffer, int bufsize)
{
    struct NameValueParserData pdata;
    struct NameValue * nv;
    ParseNameValue(buffer, bufsize, &pdata);
    for(nv = pdata.head.lh_first;
        nv != NULL;
        nv = nv->entries.le_next)
    {
        printf("%s = %s\n", nv->name, nv->value);
    }
    ClearNameValueList(&pdata);
}
#endif

