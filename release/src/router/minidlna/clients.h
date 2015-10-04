/* MiniDLNA media server
 * Copyright (C) 2013  NETGEAR
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __CLIENTS_H__
#define __CLIENTS_H__
#include <stdint.h>
#include <sys/time.h>
#include <netinet/in.h>

#define CLIENT_CACHE_SLOTS 25

/* Client capability/quirk flags */
#define FLAG_DLNA               0x00000001
#define FLAG_MIME_AVI_DIVX      0x00000002
#define FLAG_MIME_AVI_AVI       0x00000004
#define FLAG_MIME_FLAC_FLAC     0x00000008
#define FLAG_MIME_WAV_WAV       0x00000010
#define FLAG_RESIZE_THUMBS      0x00000020
#define FLAG_NO_RESIZE          0x00000040
#define FLAG_MS_PFS             0x00000080 /* Microsoft PlaysForSure client */
#define FLAG_SAMSUNG            0x00000100
#define FLAG_SAMSUNG_DCM10      0x00000200
#define FLAG_AUDIO_ONLY         0x00000400
#define FLAG_FORCE_SORT         0x00000800
#define FLAG_CAPTION_RES        0x00001000
/* Response-related flags */
#define FLAG_HAS_CAPTIONS       0x80000000
#define RESPONSE_FLAGS          0xF0000000

enum match_types {
	EMatchNone,
	EUserAgent,
	EXAVClientInfo,
	EFriendlyName,
	EModelName,
	EFriendlyNameSSDP
};

enum client_types {
	EXbox = 1,
	EPS3,
	EDenonReceiver,
	EDirecTV,
	EFreeBox,
	ELGDevice,
	ELifeTab,
	EMarantzDMP,
	EMediaRoom,
	ENetgearEVA2000,
	EPanasonic,
	EPopcornHour,
	ERokuSoundBridge,
	ESamsungSeriesA,
	ESamsungSeriesB,
	ESamsungSeriesCDEBDP,
	ESamsungSeriesCDE,
	ESonyBDP,
	ESonyBravia,
	ESonyInternetTV,
	EToshibaTV,
	EAsusOPlay,
	EBubbleUPnP,
	ENetFrontLivingConnect,
	EStandardDLNA150,
	EStandardUPnP
};

struct client_type_s {
	enum client_types type;
	uint32_t flags;
	const char *name;
	const char *match;
	enum match_types match_type;
};

struct client_cache_s {
	struct in_addr addr;
	unsigned char mac[6];
	struct client_type_s *type;
	time_t age;
	int connections;
};

extern struct client_type_s client_types[];
extern struct client_cache_s clients[CLIENT_CACHE_SLOTS];

struct client_cache_s *SearchClientCache(struct in_addr addr, int quiet);
struct client_cache_s *AddClientCache(struct in_addr addr, int type);

#endif
