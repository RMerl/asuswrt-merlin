/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006-2007, Thomas Bernard
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
#ifndef __MINIDLNATYPES_H__
#define __MINIDLNATYPES_H__

#include "config.h"
#include <netinet/in.h>

/* structure for storing lan addresses
 * with ascii representation and mask */
struct lan_addr_s {
	char str[16];	/* example: 192.168.0.1 */
	struct in_addr addr, mask;	/* ip/mask */
};

struct runtime_vars_s {
	int port;	/* HTTP Port */
	int notify_interval;	/* seconds between SSDP announces */
	char *root_container;	/* root ObjectID (instead of "0") */
};

struct string_s {
	char *data; // ptr to start of memory area
	int off;
	int size;
};

enum media_types {
	ALL_MEDIA,
	AUDIO_ONLY,
	VIDEO_ONLY,
	IMAGES_ONLY,
	NO_MEDIA
};

enum file_types {
	TYPE_UNKNOWN,
	TYPE_DIR,
	TYPE_FILE
};

enum client_types {
	EXbox = 1,
	EPS3,
	ESamsungSeriesC,
	EDenonReceiver,
	EFreeBox,
	EPopcornHour,
	EMediaRoom,
	ESonyBDP,
	ESonyBravia,
	ERokuSoundBridge,
	EToshibaTV,
	ELGDevice,
	ENetgearEVA2000,
	ESamsungSeriesA,
	ESamsungSeriesB,
	EMarantzDMP,
	ELifeTab,
	EStandardDLNA150 = 100
};

struct media_dir_s {
	char * path;            /* Base path */
	enum media_types type;  /* type of files to scan */
	struct media_dir_s * next;
};

struct album_art_name_s {
	char * name;            /* Base path */
	uint8_t wildcard;
	struct album_art_name_s * next;
};

struct client_cache_s {
	struct in_addr addr;
	unsigned char mac[6];
	enum client_types type;
	uint32_t flags;
	time_t age;
};

#endif
