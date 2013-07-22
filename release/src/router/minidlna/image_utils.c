/* MiniDLNA media server
 * Copyright (C) 2009  Justin Maggard
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

/* These functions are mostly based on code from other projects.
 * There are function to effiently resize a JPEG image, and some utility functions.
 * They are here to allow loading and saving JPEG data directly to or from memory with libjpeg.
 * The standard functions only allow you to read from or write to a file.
 *
 * The reading code comes from the JpgAlleg library, at http://wiki.allegro.cc/index.php?title=Libjpeg
 * The writing code was posted on a Google group from openjpeg, at http://groups.google.com/group/openjpeg/browse_thread/thread/331e6cf60f70797f
 * The resize functions come from the resize_image project, at http://www.golac.fr/Image-Resizer
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <jpeglib.h>
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#include "upnpreplyparse.h"
#include "image_utils.h"
#include "log.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define SWAP16(w) ( (((w) >> 8) & 0x00ff) | (((w) << 8) & 0xff00) )
#else
# define SWAP16(w) (w)
#endif

#define JPEG_QUALITY  96

#define COL(red, green, blue) (((red) << 24) | ((green) << 16) | ((blue) << 8) | 0xFF)
#define COL_FULL(red, green, blue, alpha) (((red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))
#define COL_RED(col)   (col >> 24)
#define COL_GREEN(col) ((col >> 16) & 0xFF)
#define COL_BLUE(col)  ((col >> 8) & 0xFF)
#define COL_ALPHA(col) (col & 0xFF)
#define BLACK  0x000000FF


struct my_dst_mgr {
	struct jpeg_destination_mgr jdst;
	JOCTET *buf;
	JOCTET *off;
	size_t sz;
	size_t used;
};

/* Destination manager to store data in a buffer */
static void
my_dst_mgr_init(j_compress_ptr cinfo)
{
	struct my_dst_mgr *dst = (void *)cinfo->dest;

	dst->used = 0;
	dst->sz = cinfo->image_width
		  * cinfo->image_height
		  * cinfo->input_components;
	dst->buf = malloc(dst->sz * sizeof *dst->buf);
	dst->off = dst->buf;
	dst->jdst.next_output_byte = dst->off;
	dst->jdst.free_in_buffer = dst->sz;

	return;

}

static boolean
my_dst_mgr_empty(j_compress_ptr cinfo)
{
	struct my_dst_mgr *dst = (void *)cinfo->dest;

	dst->sz *= 2;
	dst->used = dst->off - dst->buf;
	dst->buf = realloc(dst->buf, dst->sz * sizeof *dst->buf);
	dst->off = dst->buf + dst->used;
	dst->jdst.next_output_byte = dst->off;
	dst->jdst.free_in_buffer = dst->sz - dst->used;

	return TRUE;

}

static void
my_dst_mgr_term(j_compress_ptr cinfo)
{
	struct my_dst_mgr *dst = (void *)cinfo->dest;

	dst->used += dst->sz - dst->jdst.free_in_buffer;
	dst->off = dst->buf + dst->used;

	return;

}

static void
jpeg_memory_dest(j_compress_ptr cinfo, struct my_dst_mgr *dst)
{
	dst->jdst.init_destination = my_dst_mgr_init;
	dst->jdst.empty_output_buffer = my_dst_mgr_empty;
	dst->jdst.term_destination = my_dst_mgr_term;
	cinfo->dest = (void *)dst;

	return;

}

/* Source manager to read data from a buffer */
struct
my_src_mgr
{
	struct jpeg_source_mgr pub;
	JOCTET eoi_buffer[2];
};

static void
init_source(j_decompress_ptr cinfo)
{
	return;
}

static int
fill_input_buffer(j_decompress_ptr cinfo)
{
	struct my_src_mgr *src = (void *)cinfo->src;

	/* Create a fake EOI marker */
	src->eoi_buffer[0] = (JOCTET) 0xFF;
	src->eoi_buffer[1] = (JOCTET) JPEG_EOI;
	src->pub.next_input_byte = src->eoi_buffer;
	src->pub.bytes_in_buffer = 2;

	return TRUE;
}

static void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct my_src_mgr *src = (void *)cinfo->src;
	if (num_bytes > 0)
	{
		while (num_bytes > (long)src->pub.bytes_in_buffer)
		{
			num_bytes -= (long)src->pub.bytes_in_buffer;
			fill_input_buffer(cinfo);
		}
	}
	src->pub.next_input_byte += num_bytes;
	src->pub.bytes_in_buffer -= num_bytes;
}

static void
term_source(j_decompress_ptr cinfo)
{
	return;
}

void
jpeg_memory_src(j_decompress_ptr cinfo, const unsigned char * buffer, size_t bufsize)
{
	struct my_src_mgr *src;

	if (! cinfo->src)
	{
		cinfo->src = (*cinfo->mem->alloc_small)((void *)cinfo, JPOOL_PERMANENT, sizeof(struct my_src_mgr));;
	}
	src = (void *)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.next_input_byte = buffer;
	src->pub.bytes_in_buffer = bufsize;
}

jmp_buf setjmp_buffer;
/* Don't exit on error like libjpeg likes to do */
static void
libjpeg_error_handler(j_common_ptr cinfo)
{
	cinfo->err->output_message(cinfo);
	longjmp(setjmp_buffer, 1);
	return;
}

void
image_free(image_s *pimage)
{
	free(pimage->buf);
	free(pimage);
}

pix
get_pix(image_s *pimage, int32_t x, int32_t y)
{
	if((x >= 0) && (y >= 0) && (x < pimage->width) && (y < pimage->height))
	{
		return(pimage->buf[(y * pimage->width) + x]);
	}
	else
	{
		pix vpix = BLACK;
		return(vpix);
	}
}

void
put_pix_alpha_replace(image_s *pimage, int32_t x, int32_t y, pix col)
{
	if((x >= 0) && (y >= 0) && (x < pimage->width) && (y < pimage->height))
		pimage->buf[(y * pimage->width) + x] = col;
}

int
image_get_jpeg_resolution(const char * path, int * width, int * height)
{
	FILE *img;
	unsigned char buf[8];
	uint16_t offset, h, w;
	int ret = 1;
	size_t nread;
	long size;
	

	img = fopen(path, "r");
	if( !img )
		return -1;

	fseek(img, 0, SEEK_END);
	size = ftell(img);
	rewind(img);

	nread = fread(&buf, 2, 1, img);
	if( (nread < 1) || (buf[0] != 0xFF) || (buf[1] != 0xD8) )
	{
		fclose(img);
		return -1;
	}
	memset(&buf, 0, sizeof(buf));

	while( ftell(img) < size )
	{
		while( nread > 0 && buf[0] != 0xFF && !feof(img) )
			nread = fread(&buf, 1, 1, img);

		while( nread > 0 && buf[0] == 0xFF && !feof(img) )
			nread = fread(&buf, 1, 1, img);

		if( (buf[0] >= 0xc0) && (buf[0] <= 0xc3) )
		{
			nread = fread(&buf, 7, 1, img);
			*width = 0;
			*height = 0;
			if( nread < 1 )
				break;
			memcpy(&h, buf+3, 2);
			*height = SWAP16(h);
			memcpy(&w, buf+5, 2);
			*width = SWAP16(w);
			ret = 0;
			break;
		}
		else
		{
			offset = 0;
			nread = fread(&buf, 2, 1, img);
			if( nread < 1 )
				break;
			memcpy(&offset, buf, 2);
			offset = SWAP16(offset) - 2;
			if( fseek(img, offset, SEEK_CUR) == -1 )
				break;
		}
	}
	fclose(img);
	return ret;
}

int
image_get_jpeg_date_xmp(const char * path, char ** date)
{
	FILE *img;
	unsigned char buf[8];
	char *data = NULL, *newdata;
	uint16_t offset;
	struct NameValueParserData xml;
	char * exif;
	int ret = 1;
	size_t nread;

	img = fopen(path, "r");
	if( !img )
		return(-1);

	nread = fread(&buf, 2, 1, img);
	if( (nread < 1) || (buf[0] != 0xFF) || (buf[1] != 0xD8) )
	{
		fclose(img);
		return(-1);
	}
	memset(&buf, 0, sizeof(buf));

	while( !feof(img) )
	{
		while( nread > 0 && buf[0] != 0xFF && !feof(img) )
			nread = fread(&buf, 1, 1, img);

		while( nread > 0 && buf[0] == 0xFF && !feof(img) )
			nread = fread(&buf, 1, 1, img);

		if( feof(img) )
			break;

		if( buf[0] == 0xE1 ) // APP1 marker
		{
			offset = 0;
			nread = fread(&buf, 2, 1, img);
			if( nread < 1 )
				break;
			memcpy(&offset, buf, 2);
			offset = SWAP16(offset) - 2;

			if( offset < 30 )
			{
				fseek(img, offset, SEEK_CUR);
				continue;
			}

			newdata = realloc(data, 30);
			if( !newdata )
				break;
			data = newdata;

			nread = fread(data, 29, 1, img);
			if( nread < 1 )
				break;
			offset -= 29;
			if( strcmp(data, "http://ns.adobe.com/xap/1.0/") != 0 )
			{
				fseek(img, offset, SEEK_CUR);
				continue;
			}

			newdata = realloc(data, offset+1);
			if( !newdata )
				break;
			data = newdata;
			nread = fread(data, offset, 1, img);
			if( nread < 1 )
				break;

			ParseNameValue(data, offset, &xml, 0);
			exif = GetValueFromNameValueList(&xml, "DateTimeOriginal");
			if( !exif )
			{
				ClearNameValueList(&xml);
				break;
			}
			*date = realloc(*date, strlen(exif)+1);
			strcpy(*date, exif);
			ClearNameValueList(&xml);

			ret = 0;
			break;
		}
		else
		{
			offset = 0;
			nread = fread(&buf, 2, 1, img);
			if( nread < 1 )
				break;
			memcpy(&offset, buf, 2);
			offset = SWAP16(offset) - 2;
			fseek(img, offset, SEEK_CUR);
		}
	}
	fclose(img);
	free(data);
	return ret;
}

image_s *
image_new(int32_t width, int32_t height)
{
	image_s *vimage;

	if((vimage = (image_s *)malloc(sizeof(image_s))) == NULL)
	{
		DPRINTF(E_WARN, L_METADATA, "malloc failed\n");
		return NULL;
	}
	vimage->width = width; vimage->height = height;

	if((vimage->buf = (pix *)malloc(width * height * sizeof(pix))) == NULL)
	{
		DPRINTF(E_WARN, L_METADATA, "malloc failed\n");
		free(vimage);
		return NULL;
	}
	return(vimage);
}

image_s *
image_new_from_jpeg(const char * path, int is_file, const char * buf, int size, int scale, int rotate)
{
	image_s *vimage;
	FILE  *file = NULL;
	struct jpeg_decompress_struct cinfo;
	unsigned char *line[16], *ptr;
	int x, y, i, w, h, ofs;
	int maxbuf;
	struct jpeg_error_mgr pub;

	cinfo.err = jpeg_std_error(&pub);
	pub.error_exit = libjpeg_error_handler;
	jpeg_create_decompress(&cinfo);
	if( is_file )
	{
		if( (file = fopen(path, "r")) == NULL )
		{
			return NULL;
		}
		jpeg_stdio_src(&cinfo, file);
	}
	else
	{
		jpeg_memory_src(&cinfo, (const unsigned char *)buf, size);
	}
	if( setjmp(setjmp_buffer) )
	{
		jpeg_destroy_decompress(&cinfo);
		if( is_file && file )
			fclose(file);
		return NULL;
	}
	jpeg_read_header(&cinfo, TRUE);
	cinfo.scale_denom = scale;
	cinfo.do_fancy_upsampling = FALSE;
	cinfo.do_block_smoothing = FALSE;
	jpeg_start_decompress(&cinfo);
	w = cinfo.output_width;
	h = cinfo.output_height;
	vimage = (rotate & (ROTATE_90|ROTATE_270)) ? image_new(h, w) : image_new(w, h);
	if(!vimage)
	{
		jpeg_destroy_decompress(&cinfo);
		if( is_file )
			fclose(file);
		return NULL;
	}

	if( setjmp(setjmp_buffer) )
	{
		jpeg_destroy_decompress(&cinfo);
		if( is_file && file )
			fclose(file);
		if( vimage )
		{
			free(vimage->buf);
			free(vimage);
		}
		return NULL;
	}

	if(cinfo.rec_outbuf_height > 16)
	{
		DPRINTF(E_WARN, L_METADATA, "ERROR image_from_jpeg : (image_from_jpeg.c) JPEG uses line buffers > 16. Cannot load.\n");
		image_free(vimage);
		if( is_file )
			fclose(file);
		return NULL;
	}
	maxbuf = vimage->width * vimage->height;
	if(cinfo.output_components == 3)
	{
		int rx, ry;
		ofs = 0;
		if((ptr = malloc(w * 3 * cinfo.rec_outbuf_height + 16)) == NULL)
		{
			DPRINTF(E_WARN, L_METADATA, "malloc failed\n");
			image_free(vimage);
			if( is_file )
				fclose(file);
			return NULL;
		}

		for(y = 0; y < h; y += cinfo.rec_outbuf_height)
		{
			ry = (rotate & (ROTATE_90|ROTATE_180)) ? (y - h + 1) * -1 : y;
			for(i = 0; i < cinfo.rec_outbuf_height; i++)
			{
				line[i] = ptr + (w * 3 * i);
			}
			jpeg_read_scanlines(&cinfo, line, cinfo.rec_outbuf_height);
			for(x = 0; x < w * cinfo.rec_outbuf_height; x++)
			{
				rx = (rotate & (ROTATE_180|ROTATE_270)) ? (x - w + 1) * -1 : x;
				ofs = (rotate & (ROTATE_90|ROTATE_270)) ? ry + (rx * h) : rx + (ry * w);
				if( ofs < maxbuf )
					vimage->buf[ofs] = COL(ptr[x + x + x], ptr[x + x + x + 1], ptr[x + x + x + 2]);
			}
		}
		free(ptr);
	}
	else if(cinfo.output_components == 1)
	{
		ofs = 0;
		for(i = 0; i < cinfo.rec_outbuf_height; i++)
		{
			if((line[i] = malloc(w)) == NULL)
			{
				int t = 0;

				for(t = 0; t < i; t++) free(line[t]);
				jpeg_destroy_decompress(&cinfo);
				image_free(vimage);
				if( is_file )
					fclose(file);
				return NULL;
			}
		}
		for(y = 0; y < h; y += cinfo.rec_outbuf_height)
		{
			jpeg_read_scanlines(&cinfo, line, cinfo.rec_outbuf_height);
			for(i = 0; i < cinfo.rec_outbuf_height; i++)
			{
				for(x = 0; x < w; x++)
				{
					vimage->buf[ofs++] = COL(line[i][x], line[i][x], line[i][x]);
				}
			}
		}
		for(i = 0; i < cinfo.rec_outbuf_height; i++)
		{
			 free(line[i]);
		}
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	if( is_file )
		fclose(file);

	return vimage;
}

void
image_upsize(image_s * pdest, image_s * psrc, int32_t width, int32_t height)
{
	int32_t vx, vy;
#if !defined __i386__ && !defined __x86_64__
	int32_t rx, ry;
	pix vcol;

	if((pdest == NULL) || (psrc == NULL))
		return;

	for(vy = 0; vy < height; vy++)
	{
		for(vx = 0; vx < width; vx++)
		{
			rx = ((vx * psrc->width) / width);
			ry = ((vy * psrc->height) / height);
			vcol = get_pix(psrc, rx, ry);
#else
	pix   vcol,vcol1,vcol2,vcol3,vcol4;
	float rx,ry;
	float width_scale, height_scale;
	float x_dist, y_dist;

	width_scale  = (float)psrc->width  / (float)width;
	height_scale = (float)psrc->height / (float)height;

	for(vy = 0;vy < height; vy++)
	{
		for(vx = 0;vx < width; vx++)
		{
			rx = vx * width_scale;
			ry = vy * height_scale;
			vcol1 = get_pix(psrc, (int32_t)rx, (int32_t)ry);
			vcol2 = get_pix(psrc, ((int32_t)rx)+1, (int32_t)ry);
			vcol3 = get_pix(psrc, (int32_t)rx, ((int32_t)ry)+1);
			vcol4 = get_pix(psrc, ((int32_t)rx)+1, ((int32_t)ry)+1);

			x_dist = rx - ((float)((int32_t)rx));
			y_dist = ry - ((float)((int32_t)ry));
			vcol = COL_FULL( (uint8_t)((COL_RED(vcol1)*(1.0-x_dist)
			                  + COL_RED(vcol2)*(x_dist))*(1.0-y_dist)
			                  + (COL_RED(vcol3)*(1.0-x_dist)
			                  + COL_RED(vcol4)*(x_dist))*(y_dist)),
			                 (uint8_t)((COL_GREEN(vcol1)*(1.0-x_dist)
			                  + COL_GREEN(vcol2)*(x_dist))*(1.0-y_dist)
			                  + (COL_GREEN(vcol3)*(1.0-x_dist)
			                  + COL_GREEN(vcol4)*(x_dist))*(y_dist)),
			                 (uint8_t)((COL_BLUE(vcol1)*(1.0-x_dist)
			                  + COL_BLUE(vcol2)*(x_dist))*(1.0-y_dist)
			                  + (COL_BLUE(vcol3)*(1.0-x_dist)
			                  + COL_BLUE(vcol4)*(x_dist))*(y_dist)),
			                 (uint8_t)((COL_ALPHA(vcol1)*(1.0-x_dist)
			                  + COL_ALPHA(vcol2)*(x_dist))*(1.0-y_dist)
			                  + (COL_ALPHA(vcol3)*(1.0-x_dist)
			                  + COL_ALPHA(vcol4)*(x_dist))*(y_dist))
			               );
#endif
			put_pix_alpha_replace(pdest, vx, vy, vcol);
		}
	}
}

void
image_downsize(image_s * pdest, image_s * psrc, int32_t width, int32_t height)
{
	int32_t vx, vy;
	pix vcol;
	int32_t i, j;
#if !defined __i386__ && !defined __x86_64__
	int32_t rx, ry, rx_next, ry_next;
	int red, green, blue, alpha;
	int factor;

	if((pdest == NULL) || (psrc == NULL))
		return;

	for(vy = 0; vy < height; vy++)
	{
		for(vx = 0; vx < width; vx++)
		{

			rx = ((vx * psrc->width) / width);
			ry = ((vy * psrc->height) / height);

			red = green = blue = alpha = 0;

			rx_next = rx + (psrc->width / width);
			ry_next = ry + (psrc->width / width);
			factor = 0;

			for( j = rx; j < rx_next; j++)
			{
				for( i = ry; i < ry_next; i++)
				{
					factor += 1;
					vcol = get_pix(psrc, j, i);

					red   += COL_RED(vcol);
					green += COL_GREEN(vcol);
					blue  += COL_BLUE(vcol);
					alpha += COL_ALPHA(vcol);
				}
			}

			red   /= factor;
			green /= factor;
			blue  /= factor;
			alpha /= factor;

			/* on sature les valeurs */
			red   = (red   > 255) ? 255 : ((red   < 0) ? 0 : red  );
			green = (green > 255) ? 255 : ((green < 0) ? 0 : green);
			blue  = (blue  > 255) ? 255 : ((blue  < 0) ? 0 : blue );
			alpha = (alpha > 255) ? 255 : ((alpha < 0) ? 0 : alpha);
#else
	float rx,ry;
	float width_scale, height_scale;
	float red, green, blue, alpha;
	int32_t half_square_width, half_square_height;
	float round_width, round_height;

	if( (pdest == NULL) || (psrc == NULL) )
		return;

	width_scale  = (float)psrc->width  / (float)width;
	height_scale = (float)psrc->height / (float)height;

	half_square_width  = (int32_t)(width_scale  / 2.0);
	half_square_height = (int32_t)(height_scale / 2.0);
	round_width  = (width_scale  / 2.0) - (float)half_square_width;
	round_height = (height_scale / 2.0) - (float)half_square_height;
	if(round_width  > 0.0)
		half_square_width++;
	else
		round_width = 1.0;
	if(round_height > 0.0)
		half_square_height++;
	else
		round_height = 1.0;

	for(vy = 0;vy < height; vy++)  
	{
		for(vx = 0;vx < width; vx++)
		{
			rx = vx * width_scale;
			ry = vy * height_scale;
			vcol = get_pix(psrc, (int32_t)rx, (int32_t)ry);

			red = green = blue = alpha = 0.0;

			for(j=0;j<half_square_height<<1;j++)
			{
				for(i=0;i<half_square_width<<1;i++)
				{
					vcol = get_pix(psrc, ((int32_t)rx)-half_square_width+i,
					                     ((int32_t)ry)-half_square_height+j);
          
					if(((j == 0) || (j == (half_square_height<<1)-1)) && 
					   ((i == 0) || (i == (half_square_width<<1)-1)))
					{
						red   += round_width*round_height*(float)COL_RED  (vcol);
						green += round_width*round_height*(float)COL_GREEN(vcol);
						blue  += round_width*round_height*(float)COL_BLUE (vcol);
						alpha += round_width*round_height*(float)COL_ALPHA(vcol);
					}
					else if((j == 0) || (j == (half_square_height<<1)-1))
					{
						red   += round_height*(float)COL_RED  (vcol);
						green += round_height*(float)COL_GREEN(vcol);
						blue  += round_height*(float)COL_BLUE (vcol);
						alpha += round_height*(float)COL_ALPHA(vcol);
					}
					else if((i == 0) || (i == (half_square_width<<1)-1))
					{
						red   += round_width*(float)COL_RED  (vcol);
						green += round_width*(float)COL_GREEN(vcol);
						blue  += round_width*(float)COL_BLUE (vcol);
						alpha += round_width*(float)COL_ALPHA(vcol);
					}
					else
					{
						red   += (float)COL_RED  (vcol);
						green += (float)COL_GREEN(vcol);
						blue  += (float)COL_BLUE (vcol);
						alpha += (float)COL_ALPHA(vcol);
					}
				}
			}
      
			red   /= width_scale*height_scale;
			green /= width_scale*height_scale;
			blue  /= width_scale*height_scale;
			alpha /= width_scale*height_scale;
      
			/* on sature les valeurs */
			red   = (red   > 255.0)? 255.0 : ((red   < 0.0)? 0.0:red  );
			green = (green > 255.0)? 255.0 : ((green < 0.0)? 0.0:green);
			blue  = (blue  > 255.0)? 255.0 : ((blue  < 0.0)? 0.0:blue );
			alpha = (alpha > 255.0)? 255.0 : ((alpha < 0.0)? 0.0:alpha);
#endif
			put_pix_alpha_replace(pdest, vx, vy,
					      COL_FULL((uint8_t)red, (uint8_t)green, (uint8_t)blue, (uint8_t)alpha));
		}
	}
}

image_s *
image_resize(image_s * src_image, int32_t width, int32_t height)
{
	image_s * dst_image;

	dst_image = image_new(width, height);
	if( !dst_image )
		return NULL;
	if( (src_image->width < width) || (src_image->height < height) )
		image_upsize(dst_image, src_image, width, height);
	else
		image_downsize(dst_image, src_image, width, height);

	return dst_image;
}


unsigned char *
image_save_to_jpeg_buf(image_s * pimage, int * size)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	int row_stride;
	char *data;
	int i, x;
	struct my_dst_mgr dst;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_memory_dest(&cinfo, &dst);
	cinfo.image_width = pimage->width;
	cinfo.image_height = pimage->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	row_stride = cinfo.image_width * 3;
	if((data = malloc(row_stride)) == NULL)
	{
		DPRINTF(E_WARN, L_METADATA, "malloc failed\n");
		return NULL;
	}
	i = 0;
	while(cinfo.next_scanline < cinfo.image_height)
	{
		for(x = 0; x < pimage->width; x++)
		{
			data[x * 3]     = COL_RED(pimage->buf[i]);
			data[x * 3 + 1] = COL_GREEN(pimage->buf[i]);
			data[x * 3 + 2] = COL_BLUE(pimage->buf[i]);
			i++;
		}
		row_pointer[0] = (unsigned char *)data;
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	*size = dst.used;
	free(data);
	jpeg_destroy_compress(&cinfo);

	return dst.buf;
}

int
image_save_to_jpeg_file(image_s * pimage, const char * path)
{
	int nwritten, size = 0;
	unsigned char * buf;
	FILE * dst_file;

	buf = image_save_to_jpeg_buf(pimage, &size);
	if( !buf )
		return -1;
 	dst_file = fopen(path, "w");
	if( !dst_file )
	{
		free(buf);
		return -1;
	}
	nwritten = fwrite(buf, 1, size, dst_file);
	fclose(dst_file);
	free(buf);

	return (nwritten==size ? 0 : 1);
}
