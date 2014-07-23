//=========================================================================
// FILENAME	: tagutils-asf.c
// DESCRIPTION	: ASF (wma/wmv) metadata reader
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
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#else
#include <endian.h>
#endif

static inline uint16_t
le16_to_cpu(uint16_t le16)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return le16;
#else
	uint16_t be16 = ((le16 << 8) & 0xff00) | ((le16 >> 8) & 0x00ff);
	return be16;
#endif
}

static inline uint32_t
le32_to_cpu(uint32_t le32)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return le32;
#else
	uint32_t be32 =
		((le32 << 24) & 0xff000000) |
		((le32 << 8) & 0x00ff0000) |
		((le32 >> 8) & 0x0000ff00) |
		((le32 >> 24) & 0x000000ff);
	return be32;
#endif
}

static inline uint64_t
le64_to_cpu(uint64_t le64)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return le64;
#else
	uint64_t be64;
	uint8_t *le64p = (uint8_t*)&le64;
	uint8_t *be64p = (uint8_t*)&be64;
	be64p[0] = le64p[7];
	be64p[1] = le64p[6];
	be64p[2] = le64p[5];
	be64p[3] = le64p[4];
	be64p[4] = le64p[3];
	be64p[5] = le64p[2];
	be64p[6] = le64p[1];
	be64p[7] = le64p[0];
	return be64;
#endif
}

static inline uint32_t
cpu_to_be32(uint32_t cpu32)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint32_t be32 =
		((cpu32 << 24) & 0xff000000) |
		((cpu32 << 8) & 0x00ff0000) |
		((cpu32 >> 8) & 0x0000ff00) |
		((cpu32 >> 24) & 0x000000ff);
	return be32;
#else
	return cpu32;
#endif
}

static inline uint8_t
fget_byte(FILE *fp)
{
	uint8_t d;

	if (fread(&d, sizeof(d), 1, fp) != 1)
		return 0;
	return d;
}

static inline uint16_t
fget_le16(FILE *fp)
{
	uint16_t d;

	if (fread(&d, sizeof(d), 1, fp) != 1)
		return 0;
	d = le16_to_cpu(d);
	return d;
}

static inline uint32_t
fget_le32(FILE *fp)
{
	uint32_t d;

	if (fread(&d, sizeof(d), 1, fp) != 1)
		return 0;
	d = le32_to_cpu(d);
	return d;
}

// NOTE: support U+0000 ~ U+FFFF only.
static int
utf16le_to_utf8(char *dst, int n, uint16_t utf16le)
{
	uint16_t wc = le16_to_cpu(utf16le);
	if (wc < 0x80)
	{
		if (n < 1)
			return 0;
		*dst++ = wc & 0xff;
		return 1;
	}
	else if (wc < 0x800)
	{
		if (n < 2)
			return 0;
		*dst++ = 0xc0 | (wc>>6);
		*dst++ = 0x80 | (wc & 0x3f);
		return 2;
	}
	else
	{
		if (n < 3)
			return 0;
		*dst++ = 0xe0 | (wc>>12);
		*dst++ = 0x80 | ((wc>>6) & 0x3f);
		*dst++ = 0x80 | (wc & 0x3f);
		return 3;
	}
}

static int
_asf_read_file_properties(FILE *fp, asf_file_properties_t *p, uint32_t size)
{
	int len;

	len = sizeof(*p) - offsetof(asf_file_properties_t, FileID);
	if(size < len)
		return -1;

	memset(p, 0, sizeof(*p));
	p->ID = ASF_FileProperties;
	p->Size = size;

	if(len != fread(&p->FileID, 1, len, fp))
		return -1;

	return 0;
}

static void
_pick_dlna_profile(struct song_metadata *psong, uint16_t format)
{
	/* DLNA Profile Name */
	switch( le16_to_cpu(format) )
	{
		case WMA:
			if( psong->max_bitrate < 193000 )
				xasprintf(&(psong->dlna_pn), "WMABASE");
			else if( psong->max_bitrate < 385000 )
				xasprintf(&(psong->dlna_pn), "WMAFULL");
			break;
		case WMAPRO:
			xasprintf(&(psong->dlna_pn), "WMAPRO");
			break;
		case WMALSL:
			xasprintf(&(psong->dlna_pn), "WMALSL%s",
				psong->channels > 2 ? "_MULT5" : "");
		default:
			break;
	}
}

static int
_asf_read_audio_stream(FILE *fp, struct song_metadata *psong, int size)
{
	asf_audio_stream_t s;
	int len;

	len = sizeof(s) - sizeof(s.Hdr);
	if(len > size)
		len = size;

	if(len != fread(&s.wfx, 1, len, fp))
		return -1;

	psong->channels = le16_to_cpu(s.wfx.nChannels);
	psong->bitrate = le32_to_cpu(s.wfx.nAvgBytesPerSec) * 8;
	psong->samplerate = le32_to_cpu(s.wfx.nSamplesPerSec);
	if (!psong->max_bitrate)
		psong->max_bitrate = psong->bitrate;
	_pick_dlna_profile(psong, s.wfx.wFormatTag);

	return 0;
}

static int
_asf_read_media_stream(FILE *fp, struct song_metadata *psong, uint32_t size)
{
	asf_media_stream_t s;
	avi_audio_format_t wfx;
	int len;

	len = sizeof(s) - sizeof(s.Hdr);
	if(len > size)
		len = size;

	memset(&s, 0, sizeof(s));

	if(len != fread(&s.MajorType, 1, len, fp))
		return -1;

	if(IsEqualGUID(&s.MajorType, &ASF_MediaTypeAudio) &&
	   IsEqualGUID(&s.FormatType, &ASF_FormatTypeWave) && s.FormatSize >= sizeof(wfx))
	{

		if(sizeof(wfx) != fread(&wfx, 1, sizeof(wfx), fp))
			return -1;

		psong->channels = le16_to_cpu(wfx.nChannels);
		psong->bitrate = le32_to_cpu(wfx.nAvgBytesPerSec) * 8;
		psong->samplerate = le32_to_cpu(wfx.nSamplesPerSec);
		if (!psong->max_bitrate)
			psong->max_bitrate = psong->bitrate;
		_pick_dlna_profile(psong, wfx.wFormatTag);
	}

	return 0;
}

static int
_asf_read_stream_object(FILE *fp, struct song_metadata *psong, uint32_t size)
{
	asf_stream_object_t s;
	int len;

	len = sizeof(s) - sizeof(asf_object_t);
	if(size < len)
		return -1;

	memset(&s, 0, sizeof(s));

	if(len != fread(&s.StreamType, 1, len, fp))
		return -1;

	if(IsEqualGUID(&s.StreamType, &ASF_AudioStream))
		_asf_read_audio_stream(fp, psong, s.TypeSpecificSize);
	else if(IsEqualGUID(&s.StreamType, &ASF_StreamBufferStream))
		_asf_read_media_stream(fp, psong, s.TypeSpecificSize);
	else if(!IsEqualGUID(&s.StreamType, &ASF_VideoStream))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Unknown asf stream type.\n");
	}

	return 0;
}

static int
_asf_read_extended_stream_object(FILE *fp, struct song_metadata *psong, uint32_t size)
{
	int i, len;
	long off;
	asf_object_t tmp;
	asf_extended_stream_object_t xs;
	asf_stream_name_t nm;
	asf_payload_extension_t pe;

	if(size < sizeof(asf_extended_stream_object_t))
		return -1;

	memset(&xs, 0, sizeof(xs));

	len = sizeof(xs) - offsetof(asf_extended_stream_object_t, StartTime);
	if(len != fread(&xs.StartTime, 1, len, fp))
		return -1;
	off = sizeof(xs);

	for(i = 0; i < xs.StreamNameCount; i++)
	{
		if(off + sizeof(nm) > size)
			return -1;
		if(sizeof(nm) != fread(&nm, 1, sizeof(nm), fp))
			return -1;
		off += sizeof(nm);
		if(off + nm.Length > sizeof(asf_extended_stream_object_t))
			return -1;
		if(nm.Length > 0)
			fseek(fp, nm.Length, SEEK_CUR);
		off += nm.Length;
	}

	for(i = 0; i < xs.PayloadExtensionSystemCount; i++)
	{
		if(off + sizeof(pe) > size)
			return -1;
		if(sizeof(pe) != fread(&pe, 1, sizeof(pe), fp))
			return -1;
		off += sizeof(pe);
		if(pe.InfoLength > 0)
			fseek(fp, pe.InfoLength, SEEK_CUR);
		off += pe.InfoLength;
	}

	if(off < size)
	{
		if(sizeof(tmp) != fread(&tmp, 1, sizeof(tmp), fp))
			return -1;
		if(IsEqualGUID(&tmp.ID, &ASF_StreamHeader))
			_asf_read_stream_object(fp, psong, tmp.Size);
	}

	return 0;
}

static int
_asf_read_header_extension(FILE *fp, struct song_metadata *psong, uint32_t size)
{
	off_t pos;
	long off;
	asf_header_extension_t ext;
	asf_object_t tmp;

	if(size < sizeof(asf_header_extension_t))
		return -1;

	if(sizeof(ext.Reserved1) != fread(&ext.Reserved1, 1, sizeof(ext.Reserved1), fp))
		return -1;
	ext.Reserved2 = fget_le16(fp);
	ext.DataSize = fget_le32(fp);

	pos = ftell(fp);
	off = 0;
	while(off < ext.DataSize)
	{
		if(sizeof(asf_header_extension_t) + off > size)
			break;
		if(sizeof(tmp) != fread(&tmp, 1, sizeof(tmp), fp))
			break;
		if(off + tmp.Size > ext.DataSize)
			break;
		if(IsEqualGUID(&tmp.ID, &ASF_ExtendedStreamPropertiesObject))
			_asf_read_extended_stream_object(fp, psong, tmp.Size);

		off += tmp.Size;
		fseek(fp, pos + off, SEEK_SET);
	}

	return 0;
}

static int
_asf_load_string(FILE *fp, int type, int size, char *buf, int len)
{
	unsigned char data[2048];
	uint16_t wc;
	int i, j;
	int16_t *wd16;
	int32_t *wd32;
	int64_t *wd64;

	i = 0;
	if(size && (size <= sizeof(data)) && (size == fread(data, 1, size, fp)))
	{

		switch(type)
		{
		case ASF_VT_UNICODE:
			for(j = 0; j < size; j += 2)
			{
				wd16 = (int16_t *) &data[j];
				wc = (uint16_t)*wd16;
				i += utf16le_to_utf8(&buf[i], len - i, wc);
			}
			break;
		case ASF_VT_BYTEARRAY:
			for(i = 0; i < size; i++)
			{
				if(i + 1 >= len)
					break;
				buf[i] = data[i];
			}
			break;
		case ASF_VT_BOOL:
		case ASF_VT_DWORD:
			if(size >= 4)
			{
				wd32 = (int32_t *) &data[0];
				i = snprintf(buf, len, "%d", le32_to_cpu(*wd32));
			}
			break;
		case ASF_VT_QWORD:
			if(size >= 8)
			{
				wd64 = (int64_t *) &data[0];
				i = snprintf(buf, len, "%lld", (long long)le64_to_cpu(*wd64));
			}
			break;
		case ASF_VT_WORD:
			if(size >= 2)
			{
				wd16 = (int16_t *) &data[0];
				i = snprintf(buf, len, "%d", le16_to_cpu(*wd16));
			}
			break;
		}

		size = 0;
	}
	else fseek(fp, size, SEEK_CUR);

	buf[i] = 0;
	return i;
}

static void *
_asf_load_picture(FILE *fp, int size, void *bm, int *bm_size)
{
	int i;
	char buf[256];
#if 0
	//
	// Picture type       $xx
	// Data length	  $xx $xx $xx $xx
	// MIME type          <text string> $00
	// Description        <text string> $00
	// Picture data       <binary data>

	char pic_type;
	long pic_size;

	pic_type = fget_byte(fp); size -= 1;
	pic_size = fget_le32(fp); size -= 4;
#else
	fseek(fp, 5, SEEK_CUR);
	size -= 5;
#endif
	for(i = 0; i < sizeof(buf) - 1; i++)
	{
		buf[i] = fget_le16(fp); size -= 2;
		if(!buf[i])
			break;
	}
	buf[i] = '\0';
	if(i == sizeof(buf) - 1)
	{
		while(fget_le16(fp))
			size -= 2;
	}

	if(!strcasecmp(buf, "image/jpeg") ||
	   !strcasecmp(buf, "image/jpg") ||
	   !strcasecmp(buf, "image/peg"))
	{

		while(0 != fget_le16(fp))
			size -= 2;

		if(size > 0)
		{
			if(!(bm = malloc(size)))
			{
				DPRINTF(E_ERROR, L_SCANNER, "Couldn't allocate %d bytes\n", size);
			}
			else
			{
				*bm_size = size;
				if(size > *bm_size || fread(bm, 1, size, fp) != size)
				{
					DPRINTF(E_ERROR, L_SCANNER, "Overrun %d bytes required\n", size);
					free(bm);
					bm = NULL;
				}
			}
		}
		else
		{
			DPRINTF(E_ERROR, L_SCANNER, "No binary data\n");
			size = 0;
			bm = NULL;
		}
	}
	else
	{
		DPRINTF(E_ERROR, L_SCANNER, "Invalid mime type %s\n", buf);
	}

	*bm_size = size;
	return bm;
}

static int
_get_asffileinfo(char *file, struct song_metadata *psong)
{
	FILE *fp;
	asf_object_t hdr;
	asf_object_t tmp;
	unsigned long NumObjects;
	unsigned short TitleLength;
	unsigned short AuthorLength;
	unsigned short CopyrightLength;
	unsigned short DescriptionLength;
	unsigned short RatingLength;
	unsigned short NumEntries;
	unsigned short NameLength;
	unsigned short ValueType;
	unsigned short ValueLength;
	off_t pos;
	char buf[2048];
	asf_file_properties_t FileProperties;

	psong->vbr_scale = -1;

	if(!(fp = fopen(file, "rb")))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Could not open %s for reading\n", file);
		return -1;
	}

	if(sizeof(hdr) != fread(&hdr, 1, sizeof(hdr), fp))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Error reading %s\n", file);
		fclose(fp);
		return -1;
	}
	hdr.Size = le64_to_cpu(hdr.Size);

	if(!IsEqualGUID(&hdr.ID, &ASF_HeaderObject))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Not a valid header\n");
		fclose(fp);
		return -1;
	}
	NumObjects = fget_le32(fp);
	fseek(fp, 2, SEEK_CUR); // Reserved le16

	pos = ftell(fp);
	while(NumObjects > 0)
	{
		if(sizeof(tmp) != fread(&tmp, 1, sizeof(tmp), fp))
			break;
		tmp.Size = le64_to_cpu(tmp.Size);

		if(pos + tmp.Size > hdr.Size)
		{
			DPRINTF(E_ERROR, L_SCANNER, "Size overrun reading header object %llu\n",
				(unsigned long long)tmp.Size);
			break;
		}

		if(IsEqualGUID(&tmp.ID, &ASF_FileProperties))
		{
			_asf_read_file_properties(fp, &FileProperties, tmp.Size);
			psong->song_length = le64_to_cpu(FileProperties.PlayDuration) / 10000;
			psong->bitrate = le64_to_cpu(FileProperties.MaxBitrate);
			psong->max_bitrate = psong->bitrate;
		}
		else if(IsEqualGUID(&tmp.ID, &ASF_ContentDescription))
		{
			TitleLength = fget_le16(fp);
			AuthorLength = fget_le16(fp);
			CopyrightLength = fget_le16(fp);
			DescriptionLength = fget_le16(fp);
			RatingLength = fget_le16(fp);

			if(_asf_load_string(fp, ASF_VT_UNICODE, TitleLength, buf, sizeof(buf)))
			{
				if(buf[0])
					psong->title = strdup(buf);
			}
			if(_asf_load_string(fp, ASF_VT_UNICODE, AuthorLength, buf, sizeof(buf)))
			{
				if(buf[0])
					psong->contributor[ROLE_TRACKARTIST] = strdup(buf);
			}
			if(CopyrightLength)
				fseek(fp, CopyrightLength, SEEK_CUR);
			if(DescriptionLength)
				fseek(fp, DescriptionLength, SEEK_CUR);
			if(RatingLength)
				fseek(fp, RatingLength, SEEK_CUR);
		}
		else if(IsEqualGUID(&tmp.ID, &ASF_ExtendedContentDescription))
		{
			NumEntries = fget_le16(fp);
			while(NumEntries > 0)
			{
				NameLength = fget_le16(fp);
				_asf_load_string(fp, ASF_VT_UNICODE, NameLength, buf, sizeof(buf));
				ValueType = fget_le16(fp);
				ValueLength = fget_le16(fp);

				if(!strcasecmp(buf, "AlbumTitle") || !strcasecmp(buf, "WM/AlbumTitle"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->album = strdup(buf);
				}
				else if(!strcasecmp(buf, "AlbumArtist") || !strcasecmp(buf, "WM/AlbumArtist"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
					{
						if(buf[0])
							psong->contributor[ROLE_ALBUMARTIST] = strdup(buf);
					}
				}
				else if(!strcasecmp(buf, "Description") || !strcasecmp(buf, "WM/Track"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->track = atoi(buf);
				}
				else if(!strcasecmp(buf, "Genre") || !strcasecmp(buf, "WM/Genre"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->genre = strdup(buf);
				}
				else if(!strcasecmp(buf, "Year") || !strcasecmp(buf, "WM/Year"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->year = atoi(buf);
				}
				else if(!strcasecmp(buf, "WM/Director"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->contributor[ROLE_CONDUCTOR] = strdup(buf);
				}
				else if(!strcasecmp(buf, "WM/Composer"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->contributor[ROLE_COMPOSER] = strdup(buf);
				}
				else if(!strcasecmp(buf, "WM/Picture") && (ValueType == ASF_VT_BYTEARRAY))
				{
					psong->image = _asf_load_picture(fp, ValueLength, psong->image, &psong->image_size);
				}
				else if(!strcasecmp(buf, "TrackNumber") || !strcasecmp(buf, "WM/TrackNumber"))
				{
					if(_asf_load_string(fp, ValueType, ValueLength, buf, sizeof(buf)))
						if(buf[0])
							psong->track = atoi(buf);
				}
				else if(!strcasecmp(buf, "isVBR"))
				{
					fseek(fp, ValueLength, SEEK_CUR);
					psong->vbr_scale = 0;
				}
				else if(ValueLength)
				{
					fseek(fp, ValueLength, SEEK_CUR);
				}
				NumEntries--;
			}
		}
		else if(IsEqualGUID(&tmp.ID, &ASF_StreamHeader))
		{
			_asf_read_stream_object(fp, psong, tmp.Size);
		}
		else if(IsEqualGUID(&tmp.ID, &ASF_HeaderExtension))
		{
			_asf_read_header_extension(fp, psong, tmp.Size);
		}
		pos += tmp.Size;
		fseek(fp, pos, SEEK_SET);
		NumObjects--;
	}

#if 0
	if(sizeof(hdr) == fread(&hdr, 1, sizeof(hdr), fp) && IsEqualGUID(&hdr.ID, &ASF_DataObject))
	{
		if(psong->song_length)
		{
			psong->bitrate = (hdr.Size * 8000) / psong->song_length;
		}
	}
#endif

	fclose(fp);
	return 0;
}
