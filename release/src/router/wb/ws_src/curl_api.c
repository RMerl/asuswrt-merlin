/***************************************************************************
 *									_	_ ____	_
 *	Project						___| | | |	_ \| |
 *							   / __| | | | |_) | |
 *							  |	(__| |_| |	_ <| |___
 *							   \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 -	2011, Daniel Stenberg, <daniel@haxx.se>, et	al.
 *
 * This	software is	licensed as	described in the file COPYING, which
 * you should have received	as part	of this	distribution. The terms
 * are also	available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge,	publish, distribute	and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to	do so, under the terms of the COPYING file.
 *
 * This	software is	distributed	on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either	express	or implied.
 *
 ***************************************************************************/
/* An example source code that issues a	HTTP POST and we provide the actual
 * data	through	a read callback.
 */
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <curl_api.h>
#include <log.h>

#define	USE_CHUNKED			//	define this	for	unknown	following size
#define	DISABLE_EXPECT	//	if USE_CHUNKED define above, please	add	this definition
#define SKIP_PEER_VERIFICATION
#define SKIP_HOSTNAME_VERIFICATION
#define CURL_DBG 1

size_t curl_get_size(CURL* curl)
{
	Cdbg(CURL_DBG, "get size");
	size_t length = 0;
	CURLcode code;
	code = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD , &length); 
	Cdbg(CURL_DBG, "length=%d", length);
	return length;
}

int	curl_io(const char*	web_path, const char* custom_header[],PRWCB pRWCB)
/*
int	curl_io(const char*	web_path,
		const char* 	custom_header[],
		READ_CB		pReadCb,
		void*		pReadData,
		WRITE_CB	pWriteCb,
		void*		pWriteData
	)
*/
{
  	CURL *curl;
	CURLcode res;
#ifdef USE_CHUNKED
  	// charles add :
	struct curl_slist *chunk = NULL;
#endif
  	curl = curl_easy_init();
  	if(curl) {
		
		/* First set the URL that is about to receive our POST.	*/
		curl_easy_setopt(curl, CURLOPT_URL,	web_path);

    		/* Now specify we want to POST data	*/
    		curl_easy_setopt(curl, CURLOPT_POST, 1L);
    
	    	/* we want to use our own read function	*/
//	    	curl_easy_setopt(curl, CURLOPT_READFUNCTION, pReadCb);
    		curl_easy_setopt(curl, CURLOPT_READFUNCTION, pRWCB->read_cb);
    	
	    	/* pointer to pass to our read function	*/
    		//curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);
	    	curl_easy_setopt(curl, CURLOPT_READDATA, pRWCB->pInput);
//	    	curl_easy_setopt(curl, CURLOPT_READDATA, pReadData);
    
		/* get verbose debug output	please */
		curl_easy_setopt(curl, CURLOPT_VERBOSE,	1L);
    	
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pRWCB->write_cb);
	//	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pWriteCb);
		//		pRWCB->pWriteInfo->curlptr = curl;
		Cdbg(CURL_DBG, "pWriteData = %p", pRWCB->write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, pRWCB);
		//curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)pWriteData);

		/*
		   If you use POST to a HTTP	1.1	server,	you	can	send data without knowing
		   the size before starting the POST	if you use chunked encoding. You
		   enable this by adding	a header like "Transfer-Encoding: chunked" with
		   CURLOPT_HTTPHEADER. With HTTP	1.0	or without chunked transfer, you must
		   specify the size in the request.
		 */
#ifdef USE_CHUNKED
		{
			//struct curl_slist	*chunk = NULL;

			chunk	= curl_slist_append(chunk, "Transfer-Encoding: chunked");
			res =	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
			/* use curl_slist_free_all() after the *perform()	call to	free this
			   list again	*/
		}
#else
		/* Set the expected	POST size. If you want to POST large amounts of	data,
		   consider	CURLOPT_POSTFIELDSIZE_LARGE	*/
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (curl_off_t)pooh.sizeleft);
#endif
    
#ifdef DISABLE_EXPECT
		/*
		   Using	POST with HTTP 1.1 implies the use of a	"Expect: 100-continue"
		   header.  You can disable this	header with	CURLOPT_HTTPHEADER as usual.
NOTE:	if you want	chunked	transfer too, you need to combine these	two
since	you	can	only set one list of headers with CURLOPT_HTTPHEADER. */

		/* A less good option would	be to enforce HTTP 1.0,	but	that might also
		   have	other implications.	*/
		{
			//struct curl_slist	*chunk = NULL;

			chunk	= curl_slist_append(chunk, "Expect:");
			res =	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
			/* use curl_slist_free_all() after the *perform()	call to	free this
			   list again	*/
		}
#endif
    		
#if	defined(USE_CHUNKED) &&	defined(DISABLE_EXPECT)
    		{
    		// charles add :
		//    		struct curl_slist *chunk = NULL;
			int			i =0;
			while(custom_header[i]){
				printf("custom header =%s",	custom_header[i]);
				chunk =	curl_slist_append(chunk, custom_header[i]);
				i++;
			}
			res	= curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		}

#endif

#ifdef SKIP_PEER_VERIFICATION
		/*
		 * If you want to connect to a site who isn't using a certificate that is
		 * signed by one of the certs in the CA bundle you have, you can skip the
		 * verification of the server's certificate. This makes the connection
		 * A LOT LESS SECURE.
		 *
		 * If you have a CA cert for the server stored someplace else than in the
		 * default bundle, then the CURLOPT_CAPATH option might come handy for
		 * you.
		 */ 
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
 
#ifdef SKIP_HOSTNAME_VERIFICATION
		/*
		 * If the site you're connecting to uses a different host name that what
		 * they have mentioned in their server certificate's commonName (or
		 * subjectAltName) fields, libcurl will refuse to connect. You can skip
		 * this check, but this will make the connection less secure.
		 */ 
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
    		
		/* Perform the request,	res	will get the return	code */
		Cdbg(CURL_DBG, "perform start");
		res	= curl_easy_perform(curl);
		Cdbg(CURL_DBG, "perform end");

		if(chunk) curl_slist_free_all(chunk); /* free the list again */

		/* always cleanup */
		curl_easy_cleanup(curl);
    	}
	return 0;
}
