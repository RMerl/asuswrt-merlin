/*
 * Dropbear SSH
 * 
 * Copyright (c) 2002-2004 Matt Johnston
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

#include "includes.h"
#include "channel.h"
#include "buffer.h"
#include "circbuffer.h"
#include "dbutil.h"
#include "session.h"
#include "ssh.h"

/* We receive channel data - only used by the client chansession code*/
void recv_msg_channel_extended_data() {

	struct Channel *channel;
	unsigned int datatype;

	TRACE(("enter recv_msg_channel_extended_data"))

	channel = getchannel();

	if (channel->type != &clichansess) {
		TRACE(("leave recv_msg_channel_extended_data: chantype is wrong"))
		return; /* we just ignore it */
	}

	datatype = buf_getint(ses.payload);
	
	if (datatype != SSH_EXTENDED_DATA_STDERR) {
		TRACE(("leave recv_msg_channel_extended_data: wrong datatype: %d",
					datatype))
		return;	
	}

	common_recv_msg_channel_data(channel, channel->errfd, channel->extrabuf);

	TRACE(("leave recv_msg_channel_extended_data"))
}
