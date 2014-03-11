#ifndef __CLIENTS_H__
#define __CLIENTS_H__
#include <stdint.h>
#include <netinet/in.h>

#define CLIENT_CACHE_SLOTS 20

#define FLAG_DLNA               0x00000001
#define FLAG_MIME_AVI_DIVX      0x00000002
#define FLAG_MIME_AVI_AVI       0x00000004
#define FLAG_MIME_FLAC_FLAC     0x00000008
#define FLAG_MIME_WAV_WAV       0x00000010
#define FLAG_RESIZE_THUMBS      0x00000020
#define FLAG_NO_RESIZE          0x00000040
#define FLAG_MS_PFS             0x00000080 // Microsoft PlaysForSure client
#define FLAG_SAMSUNG            0x00000100
#define FLAG_SAMSUNG_TV         0x00000200
#define FLAG_AUDIO_ONLY         0x00000400
#define FLAG_FORCE_SORT         0x00000800

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
	ESamsungSeriesC,
	ESamsungSeriesCTV,
	ESonyBDP,
	ESonyBravia,
	ESonyInternetTV,
	EToshibaTV,
	EStandardDLNA150
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
	enum client_types type;
	time_t age;
};

extern struct client_type_s client_types[];
extern struct client_cache_s clients[CLIENT_CACHE_SLOTS];

int SearchClientCache(struct in_addr addr, int quiet);
int AddClientCache(struct in_addr addr, int type);

#endif
