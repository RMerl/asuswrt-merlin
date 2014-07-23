//=========================================================================
// FILENAME	: tagutils-ogg.c
// DESCRIPTION	: Ogg metadata reader
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/*
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file is derived from mt-daap project.
 */

typedef struct _ogg_stream_processor {
	void (*process_page)(struct _ogg_stream_processor *, ogg_page *, struct song_metadata *);
	void (*process_end)(struct _ogg_stream_processor *, struct song_metadata *);
	int isillegal;
	int constraint_violated;
	int shownillegal;
	int isnew;
	long seqno;
	int lostseq;

	int start;
	int end;

	int num;
	char *type;

	ogg_uint32_t serial;
	ogg_stream_state os;
	void *data;
} ogg_stream_processor;

typedef struct {
	ogg_stream_processor *streams;
	int allocated;
	int used;

	int in_headers;
} ogg_stream_set;

typedef struct {
	vorbis_info vi;
	vorbis_comment vc;

	ogg_int64_t bytes;
	ogg_int64_t lastgranulepos;
	ogg_int64_t firstgranulepos;

	int doneheaders;
} ogg_misc_vorbis_info;

#define CONSTRAINT_PAGE_AFTER_EOS   1
#define CONSTRAINT_MUXING_VIOLATED  2

static ogg_stream_set *
_ogg_create_stream_set(void)
{
	ogg_stream_set *set = calloc(1, sizeof(ogg_stream_set));

	set->streams = calloc(5, sizeof(ogg_stream_processor));
	set->allocated = 5;
	set->used = 0;

	return set;
}

static void
_ogg_vorbis_process(ogg_stream_processor *stream, ogg_page *page,
		    struct song_metadata *psong)
{
	ogg_packet packet;
	ogg_misc_vorbis_info *inf = stream->data;
	int i, header = 0;

	ogg_stream_pagein(&stream->os, page);
	if(inf->doneheaders < 3)
		header = 1;

	while(ogg_stream_packetout(&stream->os, &packet) > 0)
	{
		if(inf->doneheaders < 3)
		{
			if(vorbis_synthesis_headerin(&inf->vi, &inf->vc, &packet) < 0)
			{
				DPRINTF(E_WARN, L_SCANNER, "Could not decode vorbis header "
					"packet - invalid vorbis stream (%d)\n", stream->num);
				continue;
			}
			inf->doneheaders++;
			if(inf->doneheaders == 3)
			{
				if(ogg_page_granulepos(page) != 0 || ogg_stream_packetpeek(&stream->os, NULL) == 1)
					DPRINTF(E_WARN, L_SCANNER, "No header in vorbis stream %d\n", stream->num);
				DPRINTF(E_MAXDEBUG, L_SCANNER, "Vorbis headers parsed for stream %d, "
					"information follows...\n", stream->num);
				DPRINTF(E_MAXDEBUG, L_SCANNER, "Channels: %d\n", inf->vi.channels);
				DPRINTF(E_MAXDEBUG, L_SCANNER, "Rate: %ld\n\n", inf->vi.rate);

				psong->samplerate = inf->vi.rate;
				psong->channels = inf->vi.channels;

				if(inf->vi.bitrate_nominal > 0)
				{
					DPRINTF(E_MAXDEBUG, L_SCANNER, "Nominal bitrate: %f kb/s\n",
						(double)inf->vi.bitrate_nominal / 1000.0);
					psong->bitrate = inf->vi.bitrate_nominal / 1000;
				}
				else
				{
					int upper_rate, lower_rate;

					DPRINTF(E_MAXDEBUG, L_SCANNER, "Nominal bitrate not set\n");

					//
					upper_rate = 0;
					lower_rate = 0;

					if(inf->vi.bitrate_upper > 0)
					{
						DPRINTF(E_MAXDEBUG, L_SCANNER, "Upper bitrate: %f kb/s\n",
							(double)inf->vi.bitrate_upper / 1000.0);
						upper_rate = inf->vi.bitrate_upper;
					}
					else
					{
						DPRINTF(E_MAXDEBUG, L_SCANNER, "Upper bitrate not set\n");
					}

					if(inf->vi.bitrate_lower > 0)
					{
						DPRINTF(E_MAXDEBUG, L_SCANNER, "Lower bitrate: %f kb/s\n",
							(double)inf->vi.bitrate_lower / 1000.0);
						lower_rate = inf->vi.bitrate_lower;;
					}
					else
					{
						DPRINTF(E_MAXDEBUG, L_SCANNER, "Lower bitrate not set\n");
					}

					if(upper_rate && lower_rate)
					{
						psong->bitrate = (upper_rate + lower_rate) / 2;
					}
					else
					{
						psong->bitrate = upper_rate + lower_rate;
					}
				}

				if(inf->vc.comments > 0)
					DPRINTF(E_MAXDEBUG, L_SCANNER,
						"User comments section follows...\n");

				for(i = 0; i < inf->vc.comments; i++)
				{
					vc_scan(psong, inf->vc.user_comments[i], inf->vc.comment_lengths[i]);
				}
			}
		}
	}

	if(!header)
	{
		ogg_int64_t gp = ogg_page_granulepos(page);
		if(gp > 0)
		{
			inf->lastgranulepos = gp;
		}
		else
		{
			DPRINTF(E_WARN, L_SCANNER, "Malformed vorbis strem.\n");
		}
		inf->bytes += page->header_len + page->body_len;
	}
}

static void
_ogg_vorbis_end(ogg_stream_processor *stream, struct song_metadata *psong)
{
	ogg_misc_vorbis_info *inf = stream->data;
	double bitrate, time;

	time = (double)inf->lastgranulepos / inf->vi.rate;
	bitrate = inf->bytes * 8 / time / 1000;

	if(psong != NULL)
	{
		if(psong->bitrate <= 0)
		{
			psong->bitrate = bitrate * 1000;
		}
		psong->song_length = time * 1000;
	}

	vorbis_comment_clear(&inf->vc);
	vorbis_info_clear(&inf->vi);

	free(stream->data);
}

static void
_ogg_process_null(ogg_stream_processor *stream, ogg_page *page, struct song_metadata *psong)
{
	// invalid stream
}

static void
_ogg_process_other(ogg_stream_processor *stream, ogg_page *page, struct song_metadata *psong)
{
	ogg_stream_pagein(&stream->os, page);
}

static void
_ogg_free_stream_set(ogg_stream_set *set)
{
	int i;

	for(i = 0; i < set->used; i++)
	{
		if(!set->streams[i].end)
		{
			// no EOS
			if(set->streams[i].process_end)
				set->streams[i].process_end(&set->streams[i], NULL);
		}
		ogg_stream_clear(&set->streams[i].os);
	}

	free(set->streams);
	free(set);
}

static int
_ogg_streams_open(ogg_stream_set *set)
{
	int i;
	int res = 0;

	for(i = 0; i < set->used; i++)
	{
		if(!set->streams[i].end)
			res++;
	}

	return res;
}

static void
_ogg_null_start(ogg_stream_processor *stream)
{
	stream->process_end = NULL;
	stream->type = "invalid";
	stream->process_page = _ogg_process_null;
}

static void
_ogg_other_start(ogg_stream_processor *stream, char *type)
{
	if(type)
		stream->type = type;
	else
		stream->type = "unknown";
	stream->process_page = _ogg_process_other;
	stream->process_end = NULL;
}

static void
_ogg_vorbis_start(ogg_stream_processor *stream)
{
	ogg_misc_vorbis_info *info;

	stream->type = "vorbis";
	stream->process_page = _ogg_vorbis_process;
	stream->process_end = _ogg_vorbis_end;

	stream->data = calloc(1, sizeof(ogg_misc_vorbis_info));

	info = stream->data;

	vorbis_comment_init(&info->vc);
	vorbis_info_init(&info->vi);
}

static ogg_stream_processor *
_ogg_find_stream_processor(ogg_stream_set *set, ogg_page *page)
{
	ogg_uint32_t serial = ogg_page_serialno(page);
	int i;
	int invalid = 0;
	int constraint = 0;
	ogg_stream_processor *stream;

	for(i = 0; i < set->used; i++)
	{
		if(serial == set->streams[i].serial)
		{
			stream = &(set->streams[i]);

			set->in_headers = 0;

			if(stream->end)
			{
				stream->isillegal = 1;
				stream->constraint_violated = CONSTRAINT_PAGE_AFTER_EOS;
				return stream;
			}

			stream->isnew = 0;
			stream->start = ogg_page_bos(page);
			stream->end = ogg_page_eos(page);
			stream->serial = serial;
			return stream;
		}
	}
	if(_ogg_streams_open(set) && !set->in_headers)
	{
		constraint = CONSTRAINT_MUXING_VIOLATED;
		invalid = 1;
	}

	set->in_headers = 1;

	if(set->allocated < set->used)
		stream = &set->streams[set->used];
	else
	{
		set->allocated += 5;
		set->streams = realloc(set->streams, sizeof(ogg_stream_processor) * set->allocated);
		stream = &set->streams[set->used];
	}
	set->used++;
	stream->num = set->used;                // count from 1

	stream->isnew = 1;
	stream->isillegal = invalid;
	stream->constraint_violated = constraint;

	{
		int res;
		ogg_packet packet;

		ogg_stream_init(&stream->os, serial);
		ogg_stream_pagein(&stream->os, page);
		res = ogg_stream_packetout(&stream->os, &packet);
		if(res <= 0)
		{
			DPRINTF(E_WARN, L_SCANNER, "Invalid header page, no packet found\n");
			_ogg_null_start(stream);
		}
		else if(packet.bytes >= 7 && memcmp(packet.packet, "\001vorbis", 7) == 0)
			_ogg_vorbis_start(stream);
		else if(packet.bytes >= 8 && memcmp(packet.packet, "OggMIDI\0", 8) == 0)
			_ogg_other_start(stream, "MIDI");
		else
			_ogg_other_start(stream, NULL);

		res = ogg_stream_packetout(&stream->os, &packet);
		if(res > 0)
		{
			DPRINTF(E_WARN, L_SCANNER, "Invalid header page in stream %d, "
				"contains multiple packets\n", stream->num);
		}

		/* re-init, ready for processing */
		ogg_stream_clear(&stream->os);
		ogg_stream_init(&stream->os, serial);
	}

	stream->start = ogg_page_bos(page);
	stream->end = ogg_page_eos(page);
	stream->serial = serial;

	return stream;
}

static int
_ogg_get_next_page(FILE *f, ogg_sync_state *sync, ogg_page *page,
		   ogg_int64_t *written)
{
	int ret;
	char *buffer;
	int bytes;

	while((ret = ogg_sync_pageout(sync, page)) <= 0)
	{
		if(ret < 0)
			DPRINTF(E_WARN, L_SCANNER, "Hole in data found at approximate offset %lld bytes. Corrupted ogg.\n",
				(long long)*written);

		buffer = ogg_sync_buffer(sync, 4500); // chunk=4500
		bytes = fread(buffer, 1, 4500, f);
		if(bytes <= 0)
		{
			ogg_sync_wrote(sync, 0);
			return 0;
		}
		ogg_sync_wrote(sync, bytes);
		*written += bytes;
	}

	return 1;
}


static int
_get_oggfileinfo(char *filename, struct song_metadata *psong)
{
	FILE *file = fopen(filename, "rb");
	ogg_sync_state sync;
	ogg_page page;
	ogg_stream_set *processors = _ogg_create_stream_set();
	int gotpage = 0;
	ogg_int64_t written = 0;

	if(!file)
	{
		DPRINTF(E_FATAL, L_SCANNER,
			"Error opening input file \"%s\": %s\n", filename,  strerror(errno));
		_ogg_free_stream_set(processors);
		return -1;
	}

	DPRINTF(E_MAXDEBUG, L_SCANNER, "Processing file \"%s\"...\n\n", filename);

	ogg_sync_init(&sync);

	while(_ogg_get_next_page(file, &sync, &page, &written))
	{
		ogg_stream_processor *p = _ogg_find_stream_processor(processors, &page);
		gotpage = 1;

		if(!p)
		{
			DPRINTF(E_FATAL, L_SCANNER, "Could not find a processor for stream, bailing\n");
			_ogg_free_stream_set(processors);
			fclose(file);
			return -1;
		}

		if(p->isillegal && !p->shownillegal)
		{
			char *constraint;
			switch(p->constraint_violated)
			{
			case CONSTRAINT_PAGE_AFTER_EOS:
				constraint = "Page found for stream after EOS flag";
				break;
			case CONSTRAINT_MUXING_VIOLATED:
				constraint = "Ogg muxing constraints violated, new "
					     "stream before EOS of all previous streams";
				break;
			default:
				constraint = "Error unknown.";
			}

			DPRINTF(E_WARN, L_SCANNER,
				"Warning: illegally placed page(s) for logical stream %d\n"
				"This indicates a corrupt ogg file: %s.\n",
				p->num, constraint);
			p->shownillegal = 1;

			if(!p->isnew)
				continue;
		}

		if(p->isnew)
		{
			DPRINTF(E_MAXDEBUG, L_SCANNER, "New logical stream (#%d, serial: %08x): type %s\n",
				p->num, p->serial, p->type);
			if(!p->start)
				DPRINTF(E_WARN, L_SCANNER,
					"stream start flag not set on stream %d\n",
					p->num);
		}
		else if(p->start)
			DPRINTF(E_WARN, L_SCANNER, "stream start flag found in mid-stream "
				"on stream %d\n", p->num);

		if(p->seqno++ != ogg_page_pageno(&page))
		{
			if(!p->lostseq)
				DPRINTF(E_WARN, L_SCANNER,
					"sequence number gap in stream %d. Got page %ld "
					"when expecting page %ld. Indicates missing data.\n",
					p->num, ogg_page_pageno(&page), p->seqno - 1);
			p->seqno = ogg_page_pageno(&page);
			p->lostseq = 1;
		}
		else
			p->lostseq = 0;

		if(!p->isillegal)
		{
			p->process_page(p, &page, psong);

			if(p->end)
			{
				if(p->process_end)
					p->process_end(p, psong);
				DPRINTF(E_MAXDEBUG, L_SCANNER, "Logical stream %d ended\n", p->num);
				p->isillegal = 1;
				p->constraint_violated = CONSTRAINT_PAGE_AFTER_EOS;
			}
		}
	}

	_ogg_free_stream_set(processors);

	ogg_sync_clear(&sync);

	fclose(file);

	if(!gotpage)
	{
		DPRINTF(E_ERROR, L_SCANNER, "No ogg data found in file \"%s\".\n", filename);
		return -1;
	}

	return 0;
}
