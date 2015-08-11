/* $Id: audio_tool.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjmedia.h>
#include <pjlib.h>
#include <stdio.h>

#define THIS_FILE	"audio_tool.c"

static pj_caching_pool caching_pool;
static pj_pool_factory *pf;
static FILE *fhnd;
static pj_med_mgr_t *mm;
static pjmedia_codec *codec;
static pjmedia_codec_param cattr;


#define WRITE_ORIGINAL_PCM 0
#if WRITE_ORIGINAL_PCM
static FILE *fhnd_pcm;
#endif

static char talker_sdp[] = 
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 4002 RTP/AVP 0\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=sendonly\r\n";
static char listener_sdp[] = 
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 4000 RTP/AVP 0\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=recvonly\r\n";

static pj_status_t play_callback(/* in */   void *user_data,
				 /* in */   pj_uint32_t timestamp,
				 /* out */  void *frame,
				 /* out */  unsigned size)
{
    char pkt[160];
    struct pjmedia_frame in, out;
    int frmsz = cattr.avg_bps / 8 * cattr.ptime / 1000;

    if (fread(pkt, frmsz, 1, fhnd) != 1) {
	puts("EOF");
	return -1;
    } else {
	in.type = PJMEDIA_FRAME_TYPE_AUDIO;
	in.buf = pkt;
	in.size = frmsz;
	out.buf = frame;
	if (codec->op->decode (codec, &in, size, &out) != 0)
	    return -1;

	size = out.size;
	return 0;
    }
}

static pj_status_t rec_callback( /* in */   void *user_data,
			         /* in */   pj_uint32_t timestamp,
			         /* in */   const void *frame,
			         /* in*/    unsigned size)
{
    char pkt[160];
    struct pjmedia_frame in, out;
    //int frmsz = cattr.avg_bps / 8 * cattr.ptime / 1000;

#if WRITE_ORIGINAL_PCM
    fwrite(frame, size, 1, fhnd_pcm);
#endif

    in.type = PJMEDIA_FRAME_TYPE_AUDIO;
    in.buf = (void*)frame;
    in.size = size;
    out.buf = pkt;

    if (codec->op->encode(codec, &in, sizeof(pkt), &out) != 0)
	return -1;

    if (fwrite(pkt, out.size, 1, fhnd) != 1)
	return -1;
    return 0;
}

static pj_status_t init()
{
    pjmedia_codec_mgr *cm;
    pjmedia_codec_info id;
    int i;

    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pf = &caching_pool.factory;

    if (pj_snd_init(&caching_pool.factory))
	return -1;

    PJ_LOG(3,(THIS_FILE, "Dumping audio devices:"));
    for (i=0; i<pj_snd_get_dev_count(); ++i) {
	const pj_snd_dev_info *info;
	info = pj_snd_get_dev_info(i);
	PJ_LOG(3,(THIS_FILE, "  %d: %s\t(%d in, %d out", 
			     i, info->name, 
			     info->input_count, info->output_count));
    }

    mm = pj_med_mgr_create (&caching_pool.factory);
    cm = pj_med_mgr_get_codec_mgr (mm);

    id.type = PJMEDIA_TYPE_AUDIO;
    id.pt = 0;
    id.encoding_name = pj_str("PCMU");
    id.sample_rate = 8000;

    codec = pjmedia_codec_mgr_alloc_codec (cm, &id);
    codec->op->default_attr(codec, &cattr);
    codec->op->open(codec, &cattr);
    return 0;
}

static pj_status_t deinit()
{
    pjmedia_codec_mgr *cm;
    cm = pj_med_mgr_get_codec_mgr (mm);
    codec->op->close(codec);
    pjmedia_codec_mgr_dealloc_codec (cm, codec);
    pj_med_mgr_destroy (mm);
    pj_caching_pool_destroy(&caching_pool);
    return 0;
}

static pj_status_t record_file (const char *filename)
{
    pj_snd_stream *stream;
    pj_snd_stream_info info;
    int status;
    char s[10];

    printf("Recording to file %s...\n", filename);

    fhnd = fopen(filename, "wb");
    if (!fhnd)
	return -1;

#if WRITE_ORIGINAL_PCM
    fhnd_pcm = fopen("ORIGINAL.PCM", "wb");
    if (!fhnd_pcm)
	return -1;
#endif

    pj_bzero(&info, sizeof(info));
    info.bits_per_sample = 16;
    info.bytes_per_frame = 2;
    info.frames_per_packet = 160;
    info.samples_per_frame = 1;
    info.samples_per_sec = 8000;

    stream = pj_snd_open_recorder(-1, &info, &rec_callback, NULL);
    if (!stream)
	return -1;

    status = pj_snd_stream_start(stream);
    if (status != 0)
	goto on_error;

    puts("Press <ENTER> to exit recording");
    fgets(s, sizeof(s), stdin);

    pj_snd_stream_stop(stream);
    pj_snd_stream_close(stream);

#if WRITE_ORIGINAL_PCM
    fclose(fhnd_pcm);
#endif
    fclose(fhnd);
    return 0;

on_error:
    pj_snd_stream_stop(stream);
    pj_snd_stream_close(stream);
    return -1;
}


static pj_status_t play_file (const char *filename)
{
    pj_snd_stream *stream;
    pj_snd_stream_info info;
    int status;
    char s[10];

    printf("Playing file %s...\n", filename);

    fhnd = fopen(filename, "rb");
    if (!fhnd)
	return -1;

    pj_bzero(&info, sizeof(info));
    info.bits_per_sample = 16;
    info.bytes_per_frame = 2;
    info.frames_per_packet = 160;
    info.samples_per_frame = 1;
    info.samples_per_sec = 8000;

    stream = pj_snd_open_player(-1, &info, &play_callback, NULL);
    if (!stream)
	return -1;

    status = pj_snd_stream_start(stream);
    if (status != 0)
	goto on_error;

    puts("Press <ENTER> to exit playing");
    fgets(s, sizeof(s), stdin);

    pj_snd_stream_stop(stream);
    pj_snd_stream_close(stream);

    fclose(fhnd);
    return 0;

on_error:
    pj_snd_stream_stop(stream);
    pj_snd_stream_close(stream);
    return -1;
}

static int create_ses_by_remote_sdp(int local_port, char *sdp)
{
    pj_media_session_t *ses = NULL;
    pjsdp_session_desc *sdp_ses;
    pj_media_sock_info skinfo;
    pj_pool_t *pool;
    char s[4];
    const pj_media_stream_info *info[2];
    int i, count;

    pool = pj_pool_create(pf, "sdp", 1024, 0, NULL);
    if (!pool) {
	PJ_LOG(1,(THIS_FILE, "Unable to create pool"));
	return -1;
    }

    pj_bzero(&skinfo, sizeof(skinfo));
    skinfo.rtp_sock = skinfo.rtcp_sock = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, 0);
    if (skinfo.rtp_sock == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE, "Unable to create socket"));
	goto on_error;
    }

    pj_sockaddr_init2(&skinfo.rtp_addr_name, "0.0.0.0", local_port);
    if (pj_sock_bind(skinfo.rtp_sock, (struct pj_sockaddr*)&skinfo.rtp_addr_name, sizeof(pj_sockaddr_in)) != 0) {
	PJ_LOG(1,(THIS_FILE, "Unable to bind socket"));
	goto on_error;
    }

    sdp_ses = pjsdp_parse(sdp, strlen(sdp), pool);
    if (!sdp_ses) {
	PJ_LOG(1,(THIS_FILE, "Error parsing SDP"));
	goto on_error;
    }

    ses = pj_media_session_create_from_sdp(mm, sdp_ses, &skinfo);
    if (!ses) {
	PJ_LOG(1,(THIS_FILE, "Unable to create session from SDP"));
	goto on_error;
    }

    if (pj_media_session_activate(ses) != 0) {
	PJ_LOG(1,(THIS_FILE, "Error activating session"));
	goto on_error;
    }

    count = pj_media_session_enum_streams(ses, 2, info);
    printf("\nDumping streams: \n");
    for (i=0; i<count; ++i) {
	const char *dir;
	char *local_ip;

	switch (info[i]->dir) {
	case PJMEDIA_DIR_NONE:
	    dir = "- NONE -"; break;
	case PJMEDIA_DIR_ENCODING:
	    dir = "SENDONLY"; break;
	case PJMEDIA_DIR_DECODING:
	    dir = "RECVONLY"; break;
	case PJMEDIA_DIR_ENCODING_DECODING:
	    dir = "SENDRECV"; break;
	default:
	    dir = "?UNKNOWN"; break;
	}

	local_ip = pj_sockaddr_get_str_addr(&info[i]->sock_info.rtp_addr_name);

	printf("  Stream %d: %.*s %s local=%s:%d remote=%.*s:%d\n",
	       i, info[i]->type.slen, info[i]->type.ptr,
	       dir, 
	       local_ip, pj_sockaddr_get_port(&info[i]->sock_info.rtp_addr_name),
	       info[i]->rem_addr.slen, info[i]->rem_addr.ptr, info[i]->rem_port);
    }

    puts("Press <ENTER> to quit");
    fgets(s, sizeof(s), stdin);

    pj_media_session_destroy(ses);
    pj_sock_close(skinfo.rtp_sock);
    pj_pool_release(pool);

    return 0;

on_error:
    if (ses)
	pj_media_session_destroy(ses);
    if (skinfo.rtp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(skinfo.rtp_sock);
    if (pool)
	pj_pool_release(pool);
    return -1;
}

#if WRITE_ORIGINAL_PCM
static pj_status_t convert(const char *src, const char *dst)
{
    char pcm[320];
    char frame[160];
    struct pjmedia_frame in, out;

    fhnd_pcm = fopen(src, "rb");
    if (!fhnd_pcm)
	return -1;
    fhnd = fopen(dst, "wb");
    if (!fhnd)
	return -1;

    while (fread(pcm, 320, 1, fhnd_pcm) == 1) {

	in.type = PJMEDIA_FRAME_TYPE_AUDIO;
	in.buf = pcm;
	in.size = 320;
	out.buf = frame;

	if (codec->op->encode(codec, &in, 160, &out) != 0)
	    break;

	if (fwrite(frame, out.size, 1, fhnd) != 1)
	    break;

    }

    fclose(fhnd);
    fclose(fhnd_pcm);
    return 0;
}
#endif

static void usage(const char *exe)
{
    printf("Usage: %s <command> <file>\n", exe);
    puts("where:");
    puts("  <command>     play|record|send|recv");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
	usage(argv[0]);
	return 1;
    }

    pj_init();

    init();

    if (stricmp(argv[1], "record")==0) {
	record_file("FILE.PCM");
    } else if (stricmp(argv[1], "play")==0) {
	play_file("FILE.PCM");
    } else if (stricmp(argv[1], "send")==0) {
	create_ses_by_remote_sdp(4002, listener_sdp);
    } else if (stricmp(argv[1], "recv")==0) {
	create_ses_by_remote_sdp(4000, talker_sdp);
    } else {
	usage(argv[0]);
    }
    deinit();
    return 0;
}
