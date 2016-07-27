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

#ifndef DROPBEAR_PACKET_H_

#define DROPBEAR_PACKET_H_

#include "includes.h"
#include "queue.h"
#include "buffer.h"

void write_packet(void);
void read_packet(void);
void decrypt_packet(void);
void encrypt_packet(void);

void writebuf_enqueue(buffer * writebuf, unsigned char packet_type);

void process_packet(void);

void maybe_flush_reply_queue(void);
typedef struct PacketType {
	unsigned char type; /* SSH_MSG_FOO */
	void (*handler)(void);
} packettype;

#define PACKET_PADDING_OFF 4
#define PACKET_PAYLOAD_OFF 5

#define INIT_READBUF 128

#endif /* DROPBEAR_PACKET_H_ */
