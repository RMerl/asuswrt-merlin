//=========================================================================
// FILENAME	: tagutils-asf.h
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


#define __PACKED__  __attribute__((packed))

#include <endian.h>

typedef struct _GUID {
	__u32 l;
	__u16 w[2];
	__u8 b[8];
} __PACKED__ GUID;

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	GUID name = { l, { w1, w2 }, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#define IsEqualGUID(rguid1, rguid2) (!memcmp(rguid1, rguid2, sizeof(GUID)))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP32(l) (l)
#define SWAP16(w) (w)
#else
#define SWAP32(l) ( (((l) >> 24) & 0x000000ff) | (((l) >> 8) & 0x0000ff00) | (((l) << 8) & 0x00ff0000) | (((l) << 24) & 0xff000000) )
#define SWAP16(w) ( (((w) >> 8) & 0x00ff) | (((w) << 8) & 0xff00) )
#endif

DEFINE_GUID(ASF_StreamHeader, SWAP32(0xb7dc0791), SWAP16(0xa9b7), SWAP16(0x11cf),
	    0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_VideoStream, SWAP32(0xbc19efc0), SWAP16(0x5b4d), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_AudioStream, SWAP32(0xf8699e40), SWAP16(0x5b4d), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_HeaderObject, SWAP32(0x75b22630), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_FileProperties, SWAP32(0x8cabdca1), SWAP16(0xa947), SWAP16(0x11cf),
	    0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_ContentDescription, SWAP32(0x75b22633), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_ExtendedContentDescription, SWAP32(0xd2d0a440), SWAP16(0xe307), SWAP16(0x11d2),
	    0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50);

DEFINE_GUID(ASF_ClientGuid, SWAP32(0x8d262e32), SWAP16(0xfc28), SWAP16(0x11d7),
	    0xa9, 0xea, 0x00, 0x04, 0x5a, 0x6b, 0x76, 0xc2);

DEFINE_GUID(ASF_HeaderExtension, SWAP32(0x5fbf03b5), SWAP16(0xa92e), SWAP16(0x11cf),
	    0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_CodecList, SWAP32(0x86d15240), SWAP16(0x311d), SWAP16(0x11d0),
	    0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_DataObject, SWAP32(0x75b22636), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_PaddingObject, SWAP32(0x1806d474), SWAP16(0xcadf), SWAP16(0x4509),
	    0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8);

DEFINE_GUID(ASF_SimpleIndexObject, SWAP32(0x33000890), SWAP16(0xe5b1), SWAP16(0x11cf),
	    0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb);

DEFINE_GUID(ASF_NoErrorCorrection, SWAP32(0x20fb5700), SWAP16(0x5b55), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_AudioSpread, SWAP32(0xbfc3cd50), SWAP16(0x618f), SWAP16(0x11cf),
	    0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20);

DEFINE_GUID(ASF_Reserved1, SWAP32(0xabd3d211), SWAP16(0xa9ba), SWAP16(0x11cf),
	    0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Reserved2, SWAP32(0x86d15241), SWAP16(0x311d), SWAP16(0x11d0),
	    0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_ContentEncryptionObject, SWAP32(0x2211B3FB), SWAP16(0xBD23), SWAP16(0x11D2),
	    0xB4, 0xB7, 0x00, 0xA0, 0xC9, 0x55, 0xFC, 0x6E);

DEFINE_GUID(ASF_ExtendedContentEncryptionObject, SWAP32(0x298AE614), SWAP16(0x2622), SWAP16(0x4C17),
	    0xB9, 0x35, 0xDA, 0xE0, 0x7E, 0xE9, 0x28, 0x9C);

DEFINE_GUID(ASF_ExtendedStreamPropertiesObject, SWAP32(0x14E6A5CB), SWAP16(0xC672), SWAP16(0x4332),
	    0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A);

DEFINE_GUID(ASF_MediaTypeAudio, SWAP32(0x31178C9D), SWAP16(0x03E1), SWAP16(0x4528),
	    0xB5, 0x82, 0x3D, 0xF9, 0xDB, 0x22, 0xF5, 0x03);

DEFINE_GUID(ASF_FormatTypeWave, SWAP32(0xC4C4C4D1), SWAP16(0x0049), SWAP16(0x4E2B),
	    0x98, 0xFB, 0x95, 0x37, 0xF6, 0xCE, 0x51, 0x6D);

DEFINE_GUID(ASF_StreamBufferStream, SWAP32(0x3AFB65E2), SWAP16(0x47EF), SWAP16(0x40F2),
	    0xAC, 0x2C, 0x70, 0xA9, 0x0D, 0x71, 0xD3, 0x43);

typedef struct _BITMAPINFOHEADER {
	__u32 biSize;
	__s32 biWidth;
	__s32 biHeight;
	__u16 biPlanes;
	__u16 biBitCount;
	__u32 biCompression;
	__u32 biSizeImage;
	__s32 biXPelsPerMeter;
	__s32 biYPelsPerMeter;
	__u32 biClrUsed;
	__u32 biClrImportant;
} __PACKED__ BITMAPINFOHEADER;

typedef struct _WAVEFORMATEX {
	__u16 wFormatTag;
	__u16 nChannels;
	__u32 nSamplesPerSec;
	__u32 nAvgBytesPerSec;
	__u16 nBlockAlign;
	__u16 wBitsPerSample;
	__u16 cbSize;
} __PACKED__ WAVEFORMATEX;

typedef struct _asf_stream_object_t {
	GUID ID;
	__u64 Size;
	GUID StreamType;
	GUID ErrorCorrectionType;
	__u64 TimeOffset;
	__u32 TypeSpecificSize;
	__u32 ErrorCorrectionSize;
	__u16 StreamNumber;
	__u32 Reserved;
} __PACKED__ asf_stream_object_t;

typedef struct _asf_media_stream_t {
	asf_stream_object_t Hdr;
	GUID MajorType;
	GUID SubType;
	__u32 FixedSizeSamples;
	__u32 TemporalCompression;
	__u32 SampleSize;
	GUID FormatType;
	__u32 FormatSize;
} __PACKED__ asf_media_stream_t;

typedef struct _avi_audio_format_t {
	__u16 wFormatTag;
	__u16 nChannels;
	__u32 nSamplesPerSec;
	__u32 nAvgBytesPerSec;
	__u16 nBlockAlign;
	__u16 wBitsPerSample;
	__u16 cbSize;
} __PACKED__ avi_audio_format_t;

typedef struct _asf_extended_stream_object_t {
	GUID ID;
	__u64 Size;
	__u64 StartTime;
	__u64 EndTime;
	__u32 DataBitrate;
	__u32 BufferSize;
	__u32 InitialBufferFullness;
	__u32 AltDataBitrate;
	__u32 AltBufferSize;
	__u32 AltInitialBufferFullness;
	__u32 MaximumObjectSize;
	__u32 Flags;
	__u16 StreamNumber;
	__u16 LanguageIDIndex;
	__u64 AvgTimePerFrame;
	__u16 StreamNameCount;
	__u16 PayloadExtensionSystemCount;
} __PACKED__ asf_extended_stream_object_t;

typedef struct _asf_stream_name_t {
	__u16 ID;
	__u16 Length;
} __PACKED__ asf_stream_name_t;

typedef struct _asf_payload_extension_t {
	GUID ID;
	__u16 Size;
	__u32 InfoLength;
} __PACKED__ asf_payload_extension_t;



typedef struct _asf_object_t {
	GUID ID;
	__u64 Size;
} __PACKED__ asf_object_t;

typedef struct _asf_codec_entry_t {
	__u16 Type;
	__u16 NameLen;
	__u32 Name;
	__u16 DescLen;
	__u32 Desc;
	__u16 InfoLen;
	__u32 Info;
} __PACKED__ asf_codec_entry_t;

typedef struct _asf_codec_list_t {
	GUID ID;
	__u64 Size;
	GUID Reserved;
	__u64 NumEntries;
	asf_codec_entry_t Entries[2];
	asf_codec_entry_t VideoCodec;
} __PACKED__ asf_codec_list_t;

typedef struct _asf_content_description_t {
	GUID ID;
	__u64 Size;
	__u16 TitleLength;
	__u16 AuthorLength;
	__u16 CopyrightLength;
	__u16 DescriptionLength;
	__u16 RatingLength;
	__u32 Title;
	__u32 Author;
	__u32 Copyright;
	__u32 Description;
	__u32 Rating;
} __PACKED__ asf_content_description_t;

typedef struct _asf_file_properties_t {
	GUID ID;
	__u64 Size;
	GUID FileID;
	__u64 FileSize;
	__u64 CreationTime;
	__u64 TotalPackets;
	__u64 PlayDuration;
	__u64 SendDuration;
	__u64 Preroll;
	__u32 Flags;
	__u32 MinPacketSize;
	__u32 MaxPacketSize;
	__u32 MaxBitrate;
} __PACKED__ asf_file_properties_t;

typedef struct _asf_header_extension_t {
	GUID ID;
	__u64 Size;
	GUID Reserved1;
	__u16 Reserved2;
	__u32 DataSize;
} __PACKED__ asf_header_extension_t;

typedef struct _asf_video_stream_t {
	asf_stream_object_t Hdr;
	__u32 Width;
	__u32 Height;
	__u8 ReservedFlags;
	__u16 FormatSize;
	BITMAPINFOHEADER bmi;
	__u8 ebih[1];
} __PACKED__ asf_video_stream_t;

typedef struct _asf_audio_stream_t {
	asf_stream_object_t Hdr;
	WAVEFORMATEX wfx;
} __PACKED__ asf_audio_stream_t;

typedef struct _asf_payload_t {
	__u8 StreamNumber;
	__u8 MediaObjectNumber;
	__u32 MediaObjectOffset;
	__u8 ReplicatedDataLength;
	__u32 ReplicatedData[2];
	__u32 PayloadLength;
} __PACKED__ asf_payload_t;

typedef struct _asf_packet_t {
	__u8 TypeFlags;
	__u8 ECFlags;
	__u8 ECType;
	__u8 ECCycle;
	__u8 PropertyFlags;
	__u32 PacketLength;
	__u32 Sequence;
	__u32 PaddingLength;
	__u32 SendTime;
	__u16 Duration;
	__u8 PayloadFlags;
	asf_payload_t Payload;
} __PACKED__ asf_packet_t;

typedef struct _asf_data_object_t {
	GUID ID;
	__u64 Size;
	GUID FileID;
	__u64 TotalPackets;
	unsigned short Reserved;
} __PACKED__ asf_data_object_t;

typedef struct _asf_padding_object_t {
	GUID ID;
	__u64 Size;
} __PACKED__ asf_padding_object_t;

typedef struct _asf_simple_index_object_t {
	GUID ID;
	__u64 Size;
	GUID FileID;
	__u32 IndexEntryTimeInterval;
	__u32 MaximumPacketCount;
	__u32 IndexEntriesCount;
} __PACKED__ asf_simple_index_object_t;

typedef struct _asf_header_object_t {
	GUID ID;
	__u64 Size;
	__u32 NumObjects;
	__u16 Reserved;
	asf_header_extension_t HeaderExtension;
	asf_content_description_t ContentDescription;
	asf_file_properties_t FileProperties;
	asf_video_stream_t *        VideoStream;
	asf_audio_stream_t *        AudioStream;
	asf_codec_list_t CodecList;
	asf_padding_object_t PaddingObject;
} __PACKED__ asf_header_object_t;


#define ASF_VT_UNICODE          (0)
#define ASF_VT_BYTEARRAY        (1)
#define ASF_VT_BOOL             (2)
#define ASF_VT_DWORD            (3)
#define ASF_VT_QWORD            (4)
#define ASF_VT_WORD             (5)

static int _get_asffileinfo(char *file, struct song_metadata *psong);
