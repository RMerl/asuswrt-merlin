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
#if HAVE_FFMPEG_LIBAVUTIL_AVUTIL_H
#include <ffmpeg/libavutil/avutil.h>
#elif HAVE_LIBAV_LIBAVUTIL_AVUTIL_H
#include <libav/libavutil/avutil.h>
#elif HAVE_LIBAVUTIL_AVUTIL_H
#include <libavutil/avutil.h>
#elif HAVE_FFMPEG_AVUTIL_H
#include <ffmpeg/avutil.h>
#elif HAVE_LIBAV_AVUTIL_H
#include <libav/avutil.h>
#elif HAVE_AVUTIL_H
#include <avutil.h>
#endif

#if HAVE_FFMPEG_LIBAVCODEC_AVCODEC_H
#include <ffmpeg/libavcodec/avcodec.h>
#elif HAVE_LIBAV_LIBAVCODEC_AVCODEC_H
#include <libav/libavcodec/avcodec.h>
#elif HAVE_LIBAVCODEC_AVCODEC_H
#include <libavcodec/avcodec.h>
#elif HAVE_FFMPEG_AVCODEC_H
#include <ffmpeg/avcodec.h>
#elif HAVE_LIBAV_AVCODEC_H
#include <libav/avcodec.h>
#elif HAVE_AVCODEC_H
#include <avcodec.h>
#endif

#if HAVE_FFMPEG_LIBAVFORMAT_AVFORMAT_H
#include <ffmpeg/libavformat/avformat.h>
#elif HAVE_LIBAV_LIBAVFORMAT_AVFORMAT_H
#include <libav/libavformat/avformat.h>
#elif HAVE_LIBAVFORMAT_AVFORMAT_H
#include <libavformat/avformat.h>
#elif HAVE_FFMPEG_AVFORMAT_H
#include <ffmpeg/avformat.h>
#elif HAVE_LIBAV_LIBAVFORMAT_H
#include <libav/avformat.h>
#elif HAVE_AVFORMAT_H
#include <avformat.h>
#endif

#ifndef FF_PROFILE_H264_BASELINE
#define FF_PROFILE_H264_BASELINE 66
#endif
#ifndef FF_PROFILE_H264_CONSTRAINED_BASELINE
#define FF_PROFILE_H264_CONSTRAINED_BASELINE 578
#endif
#ifndef FF_PROFILE_H264_MAIN
#define FF_PROFILE_H264_MAIN 77
#endif
#ifndef FF_PROFILE_H264_HIGH
#define FF_PROFILE_H264_HIGH 100
#endif
#ifndef FF_PROFILE_SKIP
#define FF_PROFILE_SKIP -100
#endif

#if LIBAVCODEC_VERSION_MAJOR < 53
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#endif

#if LIBAVCODEC_VERSION_INT <= ((51<<16)+(50<<8)+1)
#define CODEC_ID_WMAPRO CODEC_ID_NONE
#endif

#if LIBAVCODEC_VERSION_MAJOR < 55
#define AV_CODEC_ID_AAC CODEC_ID_AAC
#define AV_CODEC_ID_AC3 CODEC_ID_AC3
#define AV_CODEC_ID_ADPCM_IMA_QT CODEC_ID_ADPCM_IMA_QT
#define AV_CODEC_ID_AMR_NB CODEC_ID_AMR_NB
#define AV_CODEC_ID_DTS CODEC_ID_DTS
#define AV_CODEC_ID_H264 CODEC_ID_H264
#define AV_CODEC_ID_MP2 CODEC_ID_MP2
#define AV_CODEC_ID_MP3 CODEC_ID_MP3
#define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
#define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
#define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
#define AV_CODEC_ID_MSMPEG4V3 CODEC_ID_MSMPEG4V3
#define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
#define AV_CODEC_ID_VC1 CODEC_ID_VC1
#define AV_CODEC_ID_WMAPRO CODEC_ID_WMAPRO
#define AV_CODEC_ID_WMAV1 CODEC_ID_WMAV1
#define AV_CODEC_ID_WMAV2 CODEC_ID_WMAV2
#define AV_CODEC_ID_WMV3 CODEC_ID_WMV3
#endif

#if LIBAVUTIL_VERSION_INT < ((50<<16)+(13<<8)+0)
#define av_strerror(x, y, z) snprintf(y, z, "%d", x)
#endif

#if LIBAVFORMAT_VERSION_INT >= ((52<<16)+(31<<8)+0)
# if LIBAVUTIL_VERSION_INT < ((51<<16)+(5<<8)+0) && !defined(FF_API_OLD_METADATA2)
#define AV_DICT_IGNORE_SUFFIX AV_METADATA_IGNORE_SUFFIX
#define av_dict_get av_metadata_get
typedef AVMetadataTag AVDictionaryEntry;
# endif
#endif

static inline int
lav_open(AVFormatContext **ctx, const char *filename)
{
	int ret;
#if LIBAVFORMAT_VERSION_INT >= ((53<<16)+(17<<8)+0)
	ret = avformat_open_input(ctx, filename, NULL, NULL);
	if (ret == 0)
		avformat_find_stream_info(*ctx, NULL);
#else
	ret = av_open_input_file(ctx, filename, NULL, 0, NULL);
	if (ret == 0)
		av_find_stream_info(*ctx);
#endif
	return ret;
}

static inline void
lav_close(AVFormatContext *ctx)
{
#if LIBAVFORMAT_VERSION_INT >= ((53<<16)+(17<<8)+0)
	avformat_close_input(&ctx);
#else
	av_close_input_file(ctx);
#endif
}

static inline int
lav_get_fps(AVStream *s)
{
#if LIBAVCODEC_VERSION_MAJOR < 54
	if (s->r_frame_rate.den)
		return s->r_frame_rate.num / s->r_frame_rate.den;
#else
	if (s->avg_frame_rate.den)
		return s->avg_frame_rate.num / s->avg_frame_rate.den;
#endif
	return 0;
}

static inline int
lav_get_interlaced(AVCodecContext *vc, AVStream *s)
{
#if LIBAVCODEC_VERSION_MAJOR < 54
	return (vc->time_base.den ? (s->r_frame_rate.num / vc->time_base.den) : 0);
#else
	return (vc->time_base.den ? (s->avg_frame_rate.num / vc->time_base.den) : 0);
#endif
}

static inline int
lav_is_thumbnail_stream(AVStream *s, uint8_t **data, int *size)
{
#if LIBAVFORMAT_VERSION_INT >= ((54<<16)+(6<<8))
	if (s->disposition & AV_DISPOSITION_ATTACHED_PIC &&
	    s->codec->codec_id == AV_CODEC_ID_MJPEG)
	{
		if (data)
			*data = s->attached_pic.data;
		if (size)
			*size = s->attached_pic.size;
		return 1;
	}
#endif
	return 0;
}
