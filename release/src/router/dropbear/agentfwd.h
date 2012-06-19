/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
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
#ifndef _AGENTFWD_H_
#define _AGENTFWD_H_

#include "includes.h"
#include "chansession.h"
#include "channel.h"
#include "auth.h"
#include "list.h"

/* An agent reply can be reasonably large, as it can
 * contain a list of all public keys held by the agent.
 * 10000 is arbitrary */
#define MAX_AGENT_REPLY  10000

int svr_agentreq(struct ChanSess * chansess);
void svr_agentcleanup(struct ChanSess * chansess);
void svr_agentset(struct ChanSess *chansess);

/* client functions */
void cli_load_agent_keys(m_list * ret_list);
void agent_buf_sign(buffer *sigblob, sign_key *key, 
    const unsigned char *data, unsigned int len);
void cli_setup_agent(struct Channel *channel);


#ifdef __hpux
#define seteuid(a)       setresuid(-1, (a), -1)
#define setegid(a)       setresgid(-1, (a), -1)
#endif

extern const struct ChanType cli_chan_agent;

#endif /* _AGENTFWD_H_ */
