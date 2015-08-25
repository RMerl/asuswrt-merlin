/*
 * -*- c -*-
 *
 * $Id: wavstreamer.c 1484 2007-01-17 01:06:16Z rpedde $
 *
 * Read wav file from stdin and patch the info in header and write
 * it to stdout.
 *
 * Copyright (C) 2005 Timo J. Rinne (tri@iki.fi)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_GETOPT_H
# include "getopt.h"
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

char *av0;

#if 0
/* need better longopt detection in configure.in */
struct option longopts[] =
    { { "help",    0, NULL, 'h' },
      { "samples", 1, NULL, 's' },
      { "length",  1, NULL, 'l' },
      { "offset",  1, NULL, '0' },
      { NULL,      0, NULL,  0  } };
#endif

#define GET_WAV_INT32(p) ((((unsigned long)((p)[3])) << 24) |   \
                          (((unsigned long)((p)[2])) << 16) |   \
                          (((unsigned long)((p)[1])) <<  8) |   \
                          (((unsigned long)((p)[0]))))

#define GET_WAV_INT16(p) ((((unsigned long)((p)[1])) <<  8) |   \
                          (((unsigned long)((p)[0]))))

unsigned char *read_hdr(FILE *f, size_t *hdr_len)
{
    unsigned char *hdr;
    unsigned long format_data_length;

    hdr = malloc(256);
    if (hdr == NULL)
        return NULL;

    if (fread(hdr, 44, 1, f) != 1)
        goto fail;

    if (strncmp((char*)hdr + 12, "fmt ", 4))
        goto fail;

    format_data_length = GET_WAV_INT32(hdr + 16);

    if ((format_data_length < 16) || (format_data_length > 100))
        goto fail;

    *hdr_len = 44;

    if (format_data_length > 16) {
        if (fread(hdr + 44, format_data_length - 16, 1, f) != 1)
            goto fail;
        *hdr_len += format_data_length - 16;
    }

    return hdr;

 fail:
    if (hdr != NULL)
        free(hdr);
    return NULL;
}

size_t parse_hdr(unsigned char *hdr, size_t hdr_len,
                 unsigned long *chunk_data_length_ret,
                 unsigned long *format_data_length_ret,
                 unsigned long *compression_code_ret,
                 unsigned long *channel_count_ret,
                 unsigned long *sample_rate_ret,
                 unsigned long *sample_bit_length_ret,
                 unsigned long *data_length_ret)
{
    unsigned long chunk_data_length;
    unsigned long format_data_length;
    unsigned long compression_code;
    unsigned long channel_count;
    unsigned long sample_rate;
    unsigned long sample_bit_length;
    unsigned long data_length;

    if (strncmp((char*)hdr + 0, "RIFF", 4) ||
        strncmp((char*)hdr + 8, "WAVE", 4) ||
        strncmp((char*)hdr + 12, "fmt ", 4))
        return 0;

    format_data_length = GET_WAV_INT32(hdr + 16);

    if (strncmp((char*)hdr + 20 + format_data_length, "data", 4))
        return 0;

    chunk_data_length = GET_WAV_INT32(hdr + 4);
    compression_code = GET_WAV_INT16(hdr + 20);
    channel_count = GET_WAV_INT16(hdr + 22);
    sample_rate = GET_WAV_INT32(hdr + 24);
    sample_bit_length = GET_WAV_INT16(hdr + 34);
    data_length = GET_WAV_INT32(hdr + 20 + format_data_length + 4);

    if ((format_data_length != 16) ||
        (compression_code != 1) ||
        (channel_count < 1) ||
        (sample_rate == 0) ||
        (sample_rate > 512000) ||
        (sample_bit_length < 2))
        return 0;

    *chunk_data_length_ret = chunk_data_length;
    *format_data_length_ret = format_data_length;
    *compression_code_ret = compression_code;
    *channel_count_ret = channel_count;
    *sample_rate_ret = sample_rate;
    *sample_bit_length_ret = sample_bit_length;
    *data_length_ret = data_length;

    return 20 + format_data_length + 8;
}

size_t patch_hdr(unsigned char *hdr, size_t hdr_len,
                 unsigned long sec, unsigned long us,
                 unsigned long samples,
                 size_t *data_length_ret)
{
    unsigned long chunk_data_length;
    unsigned long format_data_length;
    unsigned long compression_code;
    unsigned long channel_count;
    unsigned long sample_rate;
    unsigned long sample_bit_length;
    unsigned long data_length;
    unsigned long bytes_per_sample;

    if (parse_hdr(hdr, hdr_len,
                  &chunk_data_length,
                  &format_data_length,
                  &compression_code,
                  &channel_count,
                  &sample_rate,
                  &sample_bit_length,
                  &data_length) != hdr_len)
        return 0;

    if (hdr_len != (20 + format_data_length + 8))
        return 0;

    if (format_data_length > 16) {
        memmove(hdr + 20 + 16, hdr + 20 + format_data_length, 8);
        hdr[16] = 16;
        hdr[17] = 0;
        hdr[18] = 0;
        hdr[19] = 0;
        format_data_length = 16;
        hdr_len = 44;
    }

    bytes_per_sample = channel_count * ((sample_bit_length + 7) / 8);

    if (samples == 0) {
        samples = sample_rate * sec;
        samples += ((sample_rate / 100) * (us / 10)) / 1000;
    }

    if (samples > 0) {
        data_length = samples * bytes_per_sample;
        chunk_data_length = data_length + 36;
    } else {
        chunk_data_length = 0xffffffff;
        data_length = chunk_data_length - 36;
    }

    if (data_length_ret != NULL)
        *data_length_ret = data_length;

    hdr[4] = chunk_data_length % 0x100;
    hdr[5] = (chunk_data_length >> 8) % 0x100;
    hdr[6] = (chunk_data_length >> 16) % 0x100;
    hdr[7] = (chunk_data_length >> 24) % 0x100;

    hdr[40] = data_length % 0x100;
    hdr[41] = (data_length >> 8) % 0x100;
    hdr[42] = (data_length >> 16) % 0x100;
    hdr[43] = (data_length >> 24) % 0x100;

    return hdr_len;
}

static void usage(int exitval);

static void usage(int exitval)
{
    fprintf(stderr,
            "Usage: %s [ options ] [input-file]\n", av0);
    fprintf(stderr,
            "\n");
    fprintf(stderr,
            "Options:\n");
    fprintf(stderr,
            "-l len    |  --length=len     Length of the sound file in seconds.\n");
    fprintf(stderr,
            "-s len    |  --samples=len    Length of the sound file in samples.\n");
    fprintf(stderr,
            "-o offset |  --offset=offset  Number of bytes to discard from the stream.\n");
    fprintf(stderr,
            "\n");
    fprintf(stderr,
            "--samples and --length are mutually exclusive.\n");

#if 1
    fprintf(stderr,
            "\n\nLong options are not available on this system.\n");
#endif

    exit(exitval);
}

int main(int argc, char **argv)
{
    int c;
    unsigned long sec = 0, us = 0, samples = 0;
    unsigned long offset = 0;
    char *end;
    FILE *f=NULL;
    unsigned char *hdr;
    size_t hdr_len;
    size_t data_len;
    unsigned char buf[0x1000];
    size_t buf_len;

    if (strchr(argv[0], '/'))
        av0 = strrchr(argv[0], '/') + 1;
    else
        av0 = argv[0];

#if 0
    while ((c = getopt_long(argc, argv, "+hl:o:s:", longopts, NULL)) != EOF) {
#else
    while ((c = getopt(argc, argv, "hl:o:s:")) != -1) {
#endif
        switch(c) {
        case 'h':
            usage(0);
            /*NOTREACHED*/
            break;

        case 'l':
            sec = strtoul(optarg, &end, 10);
            if ((optarg[0] == '-') || (end == optarg) || ((end[0] != '\0') && (end[0] != '.'))) {
                fprintf(stderr, "%s: Invalid -l argument.\n", av0);
                exit(-1);
            } else if (*end == '.') {
                char tmp[7];
                int i;

                memset(tmp, '0', sizeof (tmp) - 1);
                tmp[sizeof (tmp) - 1] = '\0';
                for (i = 0; (i < (sizeof (tmp) - 1)) && (isdigit(end[i+1])); i++)
                    tmp[i] = end[i+1];
                us = strtoul(tmp, NULL, 10);
            } else {
                us = 0;
            }

            /*
            if ((sec == 0) && (us == 0)) {
                fprintf(stderr, "%s: Invalid -l argument (zero is not acceptable).\n", av0);
                exit(-1);
            }
            */

            if (samples != 0) {
                fprintf(stderr, "%s: Parameters -s and -l are mutually exclusive.\n", av0);
                exit(-1);
            }
            break;

        case 's':
            samples = strtoul(optarg, &end, 10);
            if ((optarg[0] == '-') || (end == optarg) || (end[0] != '\0')) {
                fprintf(stderr, "%s: Invalid -s argument.\n", av0);
                exit(-1);
            }
            if (samples == 0) {
                fprintf(stderr, "%s: Invalid -s argument (zero is not acceptable).\n", av0);
                exit(-1);
            }
            if ((sec != 0) || (us != 0)) {
                fprintf(stderr, "%s: Parameters -l and -s are mutually exclusive.\n", av0);
                exit(-1);
            }
            break;

        case 'o':
            offset = strtoul(optarg, &end, 10);
            if ((*optarg == '-') || (end == optarg) || (*end != '\0')) {
                fprintf(stderr, "%s: Invalid -o argument.\n", av0);
                exit(-1);
            }
            break;

        default:
            fprintf(stderr, "%s: Bad command line option -%c.\n", av0, optopt);
            usage(-1);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        f = stdin;
    } else if (argc == 1) {
        f = fopen(argv[0], "rb");
        if (f == NULL) {
            fprintf(stderr, "%s: Can't open file %s for reading.\n", av0, argv[0]);
            exit(1);
        }
    } else {
        fprintf(stderr, "%s: Too many command line arguments.\n", av0);
        usage(-1);
    }

    hdr = read_hdr(f, &hdr_len);
    if (hdr == NULL) {
        fprintf(stderr, "%s: Can't read wav header.\n", av0);
        exit(2);
    }
    if ((hdr_len = patch_hdr(hdr, hdr_len, sec, us, samples, &data_len)) == 0) {
        free(hdr);
        fprintf(stderr, "%s: Can't parse (or patch) wav header.\n", av0);
        exit(2);
    }

    if (offset > hdr_len + data_len) {
        fprintf(stderr, "%s: Offset is beyond EOF.\n", av0);
        exit(3);
    }

    if ((offset > 0) && (offset < hdr_len)) {
        memmove(hdr, hdr + offset, hdr_len - offset);
        hdr_len -= offset;
        offset = 0;
    }

    if (offset > 0) {
        offset -= hdr_len;
    } else {
        if (fwrite(hdr, hdr_len, 1, stdout) != 1) {
            fprintf(stderr, "%s: Write failed.\n", av0);
            exit(4);
        }
    }

    free(hdr);
    hdr = NULL;
    hdr_len = 0;

    if (offset > 0) {
        data_len -= offset;
        while (offset > 0) {
            buf_len = (offset > sizeof (buf)) ? sizeof (buf) : offset;
            if (fread(buf, buf_len, 1, f) != 1) {
                fprintf(stderr, "%s: Read failed.\n", av0);
                exit(5);
            }
            offset -= buf_len;
        }
    }

    while (data_len > 0) {
        buf_len = (data_len > sizeof (buf)) ? sizeof (buf) : data_len;
        if (fread(buf, buf_len, 1, f) != 1) {
            fprintf(stderr, "%s: Read failed.\n", av0);
            exit(5);
        }
        if (fwrite(buf, buf_len, 1, stdout) != 1) {
            fprintf(stderr, "%s: Write failed.\n", av0);
            exit(4);
        }
        data_len -= buf_len;
    }

    if (f != stdout) {
        fclose(f);
    }
    exit(0);
}

