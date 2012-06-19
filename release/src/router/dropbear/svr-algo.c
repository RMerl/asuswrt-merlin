/*
 * Dropbear - a SSH2 server
 * SSH client implementation
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * Copyright (c) 2004 by Mihnea Stoenescu
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "algo.h"
#include "dbutil.h"

/* match the first algorithm in the comma-separated list in buf which is
 * also in localalgos[], or return NULL on failure.
 * (*goodguess) is set to 1 if the preferred client/server algos match,
 * 0 otherwise. This is used for checking if the kexalgo/hostkeyalgos are
 * guessed correctly */
algo_type * svr_buf_match_algo(buffer* buf, algo_type localalgos[],
		int *goodguess)
{

	unsigned char * algolist = NULL;
	unsigned char * remotealgos[MAX_PROPOSED_ALGO];
	unsigned int len;
	unsigned int count, i, j;
	algo_type * ret = NULL;

	*goodguess = 0;

	/* get the comma-separated list from the buffer ie "algo1,algo2,algo3" */
	algolist = buf_getstring(buf, &len);
	/* Debug this */
	TRACE(("buf_match_algo: %s", algolist))
	if (len > MAX_PROPOSED_ALGO*(MAX_NAME_LEN+1)) {
		goto out; /* just a sanity check, no other use */
	}

	/* remotealgos will contain a list of the strings parsed out */
	/* We will have at least one string (even if it's just "") */
	remotealgos[0] = algolist;
	count = 1;
	/* Iterate through, replacing ','s with NULs, to split it into
	 * words. */
	for (i = 0; i < len; i++) {
		if (algolist[i] == '\0') {
			/* someone is trying something strange */
			goto out;
		}
		if (algolist[i] == ',') {
			algolist[i] = '\0';
			remotealgos[count] = &algolist[i+1];
			count++;
		}
		if (count >= MAX_PROPOSED_ALGO) {
			break;
		}
	}

	/* iterate and find the first match */
	for (i = 0; i < count; i++) {

		len = strlen(remotealgos[i]);

		for (j = 0; localalgos[j].name != NULL; j++) {
			if (localalgos[j].usable) {
				if (len == strlen(localalgos[j].name) &&
						strncmp(localalgos[j].name, remotealgos[i], len) == 0) {
					/* set if it was a good guess */
					if (i == 0 && j == 0) {
						*goodguess = 1;
					}
					/* set the algo to return */
					ret = &localalgos[j];
					goto out;
				}
			}
		}
	}

out:
	m_free(algolist);
	return ret;
}
