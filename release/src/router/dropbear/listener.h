/*
 * Dropbear SSH
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

#ifndef DROPBEAR_LISTENER_H
#define DROPBEAR_LISTENER_H

#define MAX_LISTENERS 20
#define LISTENER_EXTEND_SIZE 1

struct Listener {

	int socks[DROPBEAR_MAX_SOCKS];
	unsigned int nsocks;

	int index; /* index in the array of listeners */

	void (*acceptor)(struct Listener*, int sock);
	void (*cleanup)(struct Listener*);

	int type; /* CHANNEL_ID_X11, CHANNEL_ID_AGENT, 
				 CHANNEL_ID_TCPDIRECT (for clients),
				 CHANNEL_ID_TCPFORWARDED (for servers) */

	void *typedata;

};

void listeners_initialise(void);
void handle_listeners(fd_set * readfds);
void set_listener_fds(fd_set * readfds);

struct Listener* new_listener(int socks[], unsigned int nsocks, 
		int type, void* typedata, 
		void (*acceptor)(struct Listener* listener, int sock), 
		void (*cleanup)(struct Listener*));

struct Listener * get_listener(int type, void* typedata,
		int (*match)(void*, void*));

void remove_listener(struct Listener* listener);

void remove_all_listeners(void);

#endif /* DROPBEAR_LISTENER_H */
