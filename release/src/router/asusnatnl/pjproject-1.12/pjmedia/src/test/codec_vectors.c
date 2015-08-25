/* $Id: codec_vectors.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include "test.h"
#include <pjmedia-codec.h>

#define THIS_FILE   "codec_vectors.c"
#define TMP_OUT	    "output.tmp"

/*
 * Encode test. Read input from WAV file, encode to temporary output file, 
 * and compare the temporary output file to the reference file.
 */
static int codec_test_encode(pjmedia_codec_mgr *mgr, 
			     char *codec_name, 
			     unsigned bitrate,
			     const char *wav_file,
			     const char *ref_encoded_file)
{
    pj_str_t codec_id = pj_str(codec_name);
    pj_pool_t *pool = NULL;
    unsigned count, samples_per_frame, encoded_frame_len = 0, pos;
    pjmedia_codec *codec = NULL;
    const pjmedia_codec_info *ci[1];
    pjmedia_codec_param codec_param;
    pjmedia_port *wav_port = NULL;
    pjmedia_frame in_frame, out_frame;
    FILE *output = NULL, *fref = NULL;
    int rc = 0;
    pj_status_t status;

    pool = pj_pool_create(mem, "codec-vectors", 512, 512, NULL);
    if (!pool)  {
	rc = -20;
	goto on_return;
    }

    /* Find and open the codec */
    count = 1;
    status = pjmedia_codec_mgr_find_codecs_by_id(mgr, &codec_id, &count, ci, NULL);
    if (status != PJ_SUCCESS) {
	rc = -30;
	goto on_return;
    }

    status = pjmedia_codec_mgr_alloc_codec(mgr, ci[0], &codec);
    if (status != PJ_SUCCESS) {
	rc = -40;
	goto on_return;
    }

    status = pjmedia_codec_mgr_get_default_param(mgr, ci[0], &codec_param);
    if (status != PJ_SUCCESS) {
	rc = -50;
	goto on_return;
    }

    codec_param.info.avg_bps = bitrate;
    codec_param.setting.vad = 0;

    status = codec->op->init(codec, pool);
    if (status != PJ_SUCCESS) {
	rc = -60;
	goto on_return;
    }

    status = codec->op->open(codec, &codec_param);
    if (status != PJ_SUCCESS) {
	rc = -70;
	goto on_return;
    }

    /* Open WAV file */
    status = pjmedia_wav_player_port_create(pool, wav_file, 
					    codec_param.info.frm_ptime, 
					    PJMEDIA_FILE_NO_LOOP, 0, 
					    &wav_port);
    if (status != PJ_SUCCESS) {
	rc = -80;
	goto on_return;
    }

    /* Open output file */
    output = fopen(TMP_OUT, "wb");
    if (!output) {
	rc = -90;
	goto on_return;
    }

    /* Allocate buffer for PCM and encoded frames */
    samples_per_frame = codec_param.info.clock_rate * codec_param.info.frm_ptime / 1000;
    in_frame.buf = pj_pool_alloc(pool, samples_per_frame * 2);
    out_frame.buf = (pj_uint8_t*) pj_pool_alloc(pool, samples_per_frame);

    /* Loop read WAV file and encode and write to output file */
    for (;;) {
	in_frame.size = samples_per_frame * 2;
	in_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

	status = pjmedia_port_get_frame(wav_port, &in_frame);
	if (status != PJ_SUCCESS || in_frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    break;

	out_frame.size = samples_per_frame;
	status = codec->op->encode(codec, &in_frame, samples_per_frame,
				   &out_frame);
	if (status != PJ_SUCCESS) {
	    rc = -95;
	    goto on_return;
	}

	if (out_frame.size) {
	    int cnt;

	    cnt = fwrite(out_frame.buf, out_frame.size, 1, output);

	    if (encoded_frame_len == 0)
		encoded_frame_len = out_frame.size;
	}    
    }

    fclose(output);
    output = NULL;
    
    /* Compare encoded files */
    fref = fopen(ref_encoded_file, "rb");
    if (!fref) {
	rc = -100;
	goto on_return;
    }

    output = fopen(TMP_OUT, "rb");
    if (!output) {
	rc = -110;
	goto on_return;
    }

    pos = 0;
    for (;;) {
	int count;
	
	count = fread(in_frame.buf, encoded_frame_len, 1, fref);
	if (count != 1)
	    break;

	count = fread(out_frame.buf, encoded_frame_len, 1, output);
	if (count != 1)
	    break;

	if (memcmp(in_frame.buf, out_frame.buf, encoded_frame_len)) {
	    unsigned i;
	    pj_uint8_t *in = (pj_uint8_t*)in_frame.buf;
	    pj_uint8_t *out = (pj_uint8_t*)out_frame.buf;

	    for (i=0; i<encoded_frame_len; ++i) {
		if (in[i] != out[i])
		    break;
	    }

	    PJ_LOG(1,(THIS_FILE,"     failed: mismatch at pos %d", pos+i));
	    rc = -200;
	    break;
	}

	pos += encoded_frame_len;
    }

on_return:
    if (output)
	fclose(output);

    if (fref)
	fclose(fref);

    if (codec) {
	codec->op->close(codec);
	pjmedia_codec_mgr_dealloc_codec(mgr, codec);
    }

    if (wav_port)
	pjmedia_port_destroy(wav_port);

    if (pool)
	pj_pool_release(pool);

    return rc;
}


/*
 * Read file in ITU format (".itu" extension).
 *
 * Set swap_endian to TRUE if the ITU file is stored in little 
 * endian format (normally true).
 */
static int read_ITU_format(FILE  *fp_bitstream,
			   short *out_words,
			   short *p_frame_error_flag,
			   int number_of_16bit_words_per_frame,
			   pj_bool_t swap_endian)
{
    enum { MAX_BITS_PER_FRAME = 160*8 };
    short i,j;
    short nsamp;
    short packed_word;
    short bit_count;
    short bit;
    short in_array[MAX_BITS_PER_FRAME+2];
    short one = 0x0081;
    short zero = 0x007f;
    short frame_start = 0x6b21;

    nsamp = (short)fread(in_array, 2, 2 + 16*number_of_16bit_words_per_frame,
			 fp_bitstream);

    j = 0;
    bit = in_array[j++];
    if (bit != frame_start) {
        *p_frame_error_flag = 1;
    } else {
        *p_frame_error_flag = 0;
        
        /* increment j to skip over the number of bits in frame */
        j++;

        for (i=0; i<number_of_16bit_words_per_frame; i++) {
            packed_word = 0;
            bit_count = 15;
            while (bit_count >= 0) {
    	        bit = in_array[j++];
    	        if (bit == zero) 
    	            bit = 0;
    	        else if (bit == one) 
    	            bit = 1;
    	        else 
    	            *p_frame_error_flag = 1;

	        packed_word <<= 1;
	        packed_word = (short )(packed_word + bit);
	        bit_count--;
            }

	    if (swap_endian)
		out_words[i] = pj_ntohs(packed_word);
	    else
		out_words[i] = packed_word;
        }
    }
    return (nsamp-1)/16;
}


/*
 * Decode test
 *
 * Decode the specified encoded file in "in_encoded_file" into temporary
 * PCM output file, and compare the temporary PCM output file with
 * the PCM reference file.
 *
 * Some reference file requires manipulation to the PCM output
 * before comparison, such manipulation can be done by supplying
 * this function with the "manip" function.
 */
static int codec_test_decode(pjmedia_codec_mgr *mgr, 
			     char *codec_name, 
			     unsigned bitrate,
			     unsigned encoded_len,
			     const char *in_encoded_file,
			     const char *ref_pcm_file,
			     void (*manip)(short *pcm, unsigned count))
{
    pj_str_t codec_id = pj_str(codec_name);
    pj_pool_t *pool = NULL;
    unsigned count, samples_per_frame, pos;
    pjmedia_codec *codec = NULL;
    const pjmedia_codec_info *ci[1];
    pjmedia_codec_param codec_param;
    pjmedia_frame out_frame;
    void *pkt;
    FILE *input = NULL, *output = NULL, *fref = NULL;
    pj_bool_t is_itu_format = PJ_FALSE;
    int rc = 0;
    pj_status_t status;

    pool = pj_pool_create(mem, "codec-vectors", 512, 512, NULL);
    if (!pool)  {
	rc = -20;
	goto on_return;
    }

    /* Find and open the codec */
    count = 1;
    status = pjmedia_codec_mgr_find_codecs_by_id(mgr, &codec_id, &count, ci, NULL);
    if (status != PJ_SUCCESS) {
	rc = -30;
	goto on_return;
    }

    status = pjmedia_codec_mgr_alloc_codec(mgr, ci[0], &codec);
    if (status != PJ_SUCCESS) {
	rc = -40;
	goto on_return;
    }

    status = pjmedia_codec_mgr_get_default_param(mgr, ci[0], &codec_param);
    if (status != PJ_SUCCESS) {
	rc = -50;
	goto on_return;
    }

    codec_param.info.avg_bps = bitrate;
    codec_param.setting.vad = 0;

    status = codec->op->init(codec, pool);
    if (status != PJ_SUCCESS) {
	rc = -60;
	goto on_return;
    }

    status = codec->op->open(codec, &codec_param);
    if (status != PJ_SUCCESS) {
	rc = -70;
	goto on_return;
    }

    /* Open input file */
    input = fopen(in_encoded_file, "rb");
    if (!input) {
	rc = -80;
	goto on_return;
    }

    /* Is the file in ITU format? */
    is_itu_format = pj_ansi_stricmp(in_encoded_file+strlen(in_encoded_file)-4,
				    ".itu")==0;

    /* Open output file */
    output = fopen(TMP_OUT, "wb");
    if (!output) {
	rc = -90;
	goto on_return;
    }

    /* Allocate buffer for PCM and encoded frames */
    samples_per_frame = codec_param.info.clock_rate * codec_param.info.frm_ptime / 1000;
    pkt = pj_pool_alloc(pool, samples_per_frame * 2);
    out_frame.buf = (pj_uint8_t*) pj_pool_alloc(pool, samples_per_frame * 2);

    /* Loop read WAV file and encode and write to output file */
    for (;;) {
	pjmedia_frame in_frame[2];
	pj_timestamp ts;
	unsigned count;
	pj_bool_t has_frame;

	if (is_itu_format) {
	    int nsamp;
	    short frame_err = 0;

	    nsamp = read_ITU_format(input, (short*)pkt, &frame_err,
				    encoded_len / 2, PJ_TRUE);
	    if (nsamp != (int)encoded_len / 2)
		break;

	    has_frame = !frame_err;
	} else {
	    if (fread(pkt, encoded_len, 1, input) != 1)
		break;

	    has_frame = PJ_TRUE;
	}

	if (has_frame) {
	    count = 2;
	    if (codec->op->parse(codec, pkt, encoded_len, &ts, 
				 &count, in_frame) != PJ_SUCCESS) 
	    {
		rc = -100;
		goto on_return;
	    }

	    if (count != 1) {
		rc = -110;
		goto on_return;
	    }

	    if (codec->op->decode(codec, &in_frame[0], samples_per_frame*2, 
				  &out_frame) != PJ_SUCCESS) 
	    {
		rc = -120;
		goto on_return;
	    }
	} else {
	    if (codec->op->recover(codec, samples_per_frame*2, 
				   &out_frame) != PJ_SUCCESS)
	    {
		rc = -125;
		goto on_return;
	    }
	}

	if (manip)
	    manip((short*)out_frame.buf, samples_per_frame);

	if (fwrite(out_frame.buf, out_frame.size, 1, output) != 1) {
	    rc = -130;
	    goto on_return;
	}
    }

    fclose(input);
    input = NULL;

    fclose(output);
    output = NULL;
    
    /* Compare encoded files */
    fref = fopen(ref_pcm_file, "rb");
    if (!fref) {
	rc = -140;
	goto on_return;
    }

    output = fopen(TMP_OUT, "rb");
    if (!output) {
	rc = -110;
	goto on_return;
    }

    pos = 0;
    for (;;) {
	int count;
	
	count = fread(pkt, samples_per_frame*2, 1, fref);
	if (count != 1)
	    break;

	count = fread(out_frame.buf, samples_per_frame*2, 1, output);
	if (count != 1)
	    break;

	if (memcmp(pkt, out_frame.buf, samples_per_frame*2)) {
	    unsigned i;
	    pj_int16_t *in = (pj_int16_t*)pkt;
	    pj_int16_t *out = (pj_int16_t*)out_frame.buf;

	    for (i=0; i<samples_per_frame; ++i) {
		if (in[i] != out[i])
		    break;
	    }

	    PJ_LOG(1,(THIS_FILE,"     failed: mismatch at samples %d", pos+i));
	    rc = -200;
	    break;
	}

	pos += samples_per_frame;
    }

on_return:
    if (output)
	fclose(output);

    if (fref)
	fclose(fref);

    if (input)
	fclose(input);

    if (codec) {
	codec->op->close(codec);
	pjmedia_codec_mgr_dealloc_codec(mgr, codec);
    }

    if (pool)
	pj_pool_release(pool);

    return rc;
}

#if PJMEDIA_HAS_G7221_CODEC
/* For ITU testing, off the 2 lsbs. */
static void g7221_pcm_manip(short *pcm, unsigned count)
{
    unsigned i;
    for (i=0; i<count; i++)
        pcm[i] &= 0xfffc;

}
#endif	/* PJMEDIA_HAS_G7221_CODEC */

int codec_test_vectors(void)
{
    pjmedia_endpt *endpt;
    pjmedia_codec_mgr *mgr;
    int rc, rc_final = 0;
    struct enc_vectors {
	char	    *codec_name;
	unsigned     bit_rate;
	const char  *wav_file;
	const char  *ref_encoded_file;
    } enc_vectors[] = 
    {
#if PJMEDIA_HAS_G7221_CODEC
	{ "G7221/16000/1", 24000, 
	  "../src/test/vectors/g722_1_enc_in.wav", 
	  "../src/test/vectors/g722_1_enc_out_24000_be.pak"
	},
	{ "G7221/16000/1", 32000, 
	  "../src/test/vectors/g722_1_enc_in.wav", 
	  "../src/test/vectors/g722_1_enc_out_32000_be.pak"
	},
#endif
	{ NULL }
    };
    struct dec_vectors {
	char	    *codec_name;
	unsigned     bit_rate;
	unsigned     encoded_frame_len;
	void	    (*manip)(short *pcm, unsigned count);
	const char  *enc_file;
	const char  *ref_pcm_file;
    } dec_vectors[] = 
    {
#if PJMEDIA_HAS_G7221_CODEC
	{ "G7221/16000/1", 24000, 60,
	  &g7221_pcm_manip,
	  "../src/test/vectors/g722_1_enc_out_24000_be.pak", 
	  "../src/test/vectors/g722_1_dec_out_24000.pcm"
	},
	{ "G7221/16000/1", 32000, 80,
	  &g7221_pcm_manip,
	  "../src/test/vectors/g722_1_enc_out_32000_be.pak", 
	  "../src/test/vectors/g722_1_dec_out_32000.pcm"
	},
	{ "G7221/16000/1", 24000, 60,
	  &g7221_pcm_manip,
	  "../src/test/vectors/g722_1_dec_in_24000_fe.itu",
	  "../src/test/vectors/g722_1_dec_out_24000_fe.pcm"
	},
	{ "G7221/16000/1", 32000, 80,
	  &g7221_pcm_manip,
	  "../src/test/vectors/g722_1_dec_in_32000_fe.itu",
	  "../src/test/vectors/g722_1_dec_out_32000_fe.pcm"
	},
#endif
	{ NULL }
    };
    unsigned i;
    pj_status_t status;

    status = pjmedia_endpt_create(mem, NULL, 0, 0, 0, &endpt);
    if (status != PJ_SUCCESS)
	return -5;

    mgr = pjmedia_endpt_get_codec_mgr(endpt);

#if PJMEDIA_HAS_G7221_CODEC
    status = pjmedia_codec_g7221_init(endpt);
    if (status != PJ_SUCCESS) {
	pjmedia_endpt_destroy(endpt);
	return -7;
    }

    /* Set shift value to zero for the test vectors */
    pjmedia_codec_g7221_set_pcm_shift(0);
#endif

    PJ_LOG(3,(THIS_FILE,"  encode tests:"));
    for (i=0; i<PJ_ARRAY_SIZE(enc_vectors); ++i) {
	if (!enc_vectors[i].codec_name)
	    continue;
	PJ_LOG(3,(THIS_FILE,"    %s @%d bps %s ==> %s", 
		  enc_vectors[i].codec_name, 
		  enc_vectors[i].bit_rate,
		  enc_vectors[i].wav_file,
		  enc_vectors[i].ref_encoded_file));
	rc = codec_test_encode(mgr, enc_vectors[i].codec_name,
			       enc_vectors[i].bit_rate,
			       enc_vectors[i].wav_file,
			       enc_vectors[i].ref_encoded_file);
	if (rc != 0)
	    rc_final = rc;
    }

    PJ_LOG(3,(THIS_FILE,"  decode tests:"));
    for (i=0; i<PJ_ARRAY_SIZE(dec_vectors); ++i) {
	if (!dec_vectors[i].codec_name)
	    continue;
	PJ_LOG(3,(THIS_FILE,"    %s @%d bps %s ==> %s", 
		  dec_vectors[i].codec_name, 
		  dec_vectors[i].bit_rate,
		  dec_vectors[i].enc_file,
		  dec_vectors[i].ref_pcm_file));
	rc = codec_test_decode(mgr, dec_vectors[i].codec_name,
			       dec_vectors[i].bit_rate,
			       dec_vectors[i].encoded_frame_len,
			       dec_vectors[i].enc_file,
			       dec_vectors[i].ref_pcm_file,
			       dec_vectors[i].manip);
	if (rc != 0)
	    rc_final = rc;
    }

    if (pj_file_exists(TMP_OUT))
	pj_file_delete(TMP_OUT);

    pjmedia_endpt_destroy(endpt);
    return rc_final;
}

