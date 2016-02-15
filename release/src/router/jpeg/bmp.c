/*
 * Copyright (C)2011, 2015 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include "cdjpeg.h"
#include <jpeglib.h>
#include <jpegint.h>
#include "tjutil.h"
#include "bmp.h"


/* This duplicates the functionality of the VirtualGL bitmap library using
   the components from cjpeg and djpeg */


/* Error handling (based on example in example.c) */

static char errStr[JMSG_LENGTH_MAX]="No error";

struct my_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit(j_common_ptr cinfo)
{
	my_error_ptr myerr=(my_error_ptr)cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

/* Based on output_message() in jerror.c */

static void my_output_message(j_common_ptr cinfo)
{
	(*cinfo->err->format_message)(cinfo, errStr);
}

#define _throw(m) {snprintf(errStr, JMSG_LENGTH_MAX, "%s", m);  \
	retval=-1;  goto bailout;}
#define _throwunix(m) {snprintf(errStr, JMSG_LENGTH_MAX, "%s\n%s", m,  \
	strerror(errno));  retval=-1;  goto bailout;}


static void pixelconvert(unsigned char *srcbuf, int srcpf, int srcbottomup,
	unsigned char *dstbuf, int dstpf, int dstbottomup, int w, int h)
{
	unsigned char *srcrowptr=srcbuf, *srccolptr;
	int srcps=tjPixelSize[srcpf];
	int srcstride=srcbottomup? -w*srcps:w*srcps;
	unsigned char *dstrowptr=dstbuf, *dstcolptr;
	int dstps=tjPixelSize[dstpf];
	int dststride=dstbottomup? -w*dstps:w*dstps;
	int row, col;

	if(srcbottomup) srcrowptr=&srcbuf[w*srcps*(h-1)];
	if(dstbottomup) dstrowptr=&dstbuf[w*dstps*(h-1)];

	/* NOTE: These quick & dirty CMYK<->RGB conversion routines are for testing
	   purposes only.  Properly converting between CMYK and RGB requires a color
	   management system. */

	if(dstpf==TJPF_CMYK)
	{
		for(row=0; row<h; row++, srcrowptr+=srcstride, dstrowptr+=dststride)
		{
			for(col=0, srccolptr=srcrowptr, dstcolptr=dstrowptr;
				col<w; col++, srccolptr+=srcps)
			{
				double c=1.0-((double)(srccolptr[tjRedOffset[srcpf]])/255.);
				double m=1.0-((double)(srccolptr[tjGreenOffset[srcpf]])/255.);
				double y=1.0-((double)(srccolptr[tjBlueOffset[srcpf]])/255.);
				double k=min(min(c,m),min(y,1.0));
				if(k==1.0) c=m=y=0.0;
				else
				{
					c=(c-k)/(1.0-k);
					m=(m-k)/(1.0-k);
					y=(y-k)/(1.0-k);
				}
				if(c>1.0) c=1.0;  if(c<0.) c=0.;
				if(m>1.0) m=1.0;  if(m<0.) m=0.;
				if(y>1.0) y=1.0;  if(y<0.) y=0.;
				if(k>1.0) k=1.0;  if(k<0.) k=0.;
				*dstcolptr++=(unsigned char)(255.0-c*255.0+0.5);
				*dstcolptr++=(unsigned char)(255.0-m*255.0+0.5);
				*dstcolptr++=(unsigned char)(255.0-y*255.0+0.5);
				*dstcolptr++=(unsigned char)(255.0-k*255.0+0.5);
			}
		}
	}
	else if(srcpf==TJPF_CMYK)
	{
		for(row=0; row<h; row++, srcrowptr+=srcstride, dstrowptr+=dststride)
		{
			for(col=0, srccolptr=srcrowptr, dstcolptr=dstrowptr;
				col<w; col++, dstcolptr+=dstps)
			{
				double c=(double)(*srccolptr++);
				double m=(double)(*srccolptr++);
				double y=(double)(*srccolptr++);
				double k=(double)(*srccolptr++);
				double r=c*k/255.;
				double g=m*k/255.;
				double b=y*k/255.;
				if(r>255.0) r=255.0;  if(r<0.) r=0.;
				if(g>255.0) g=255.0;  if(g<0.) g=0.;
				if(b>255.0) b=255.0;  if(b<0.) b=0.;
				dstcolptr[tjRedOffset[dstpf]]=(unsigned char)(r+0.5);
				dstcolptr[tjGreenOffset[dstpf]]=(unsigned char)(g+0.5);
				dstcolptr[tjBlueOffset[dstpf]]=(unsigned char)(b+0.5);
			}
		}
	}
	else
	{
		for(row=0; row<h; row++, srcrowptr+=srcstride, dstrowptr+=dststride)
		{
			for(col=0, srccolptr=srcrowptr, dstcolptr=dstrowptr;
				col<w; col++, srccolptr+=srcps, dstcolptr+=dstps)
			{
				dstcolptr[tjRedOffset[dstpf]]=srccolptr[tjRedOffset[srcpf]];
				dstcolptr[tjGreenOffset[dstpf]]=srccolptr[tjGreenOffset[srcpf]];
				dstcolptr[tjBlueOffset[dstpf]]=srccolptr[tjBlueOffset[srcpf]];
			}
		}
	}
}


int loadbmp(char *filename, unsigned char **buf, int *w, int *h,
	int dstpf, int bottomup)
{
	int retval=0, dstps, srcpf, tempc;
	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jerr;
	cjpeg_source_ptr src;
	FILE *file=NULL;

	memset(&cinfo, 0, sizeof(struct jpeg_compress_struct));

	if(!filename || !buf || !w || !h || dstpf<0 || dstpf>=TJ_NUMPF)
		_throw("loadbmp(): Invalid argument");

	if((file=fopen(filename, "rb"))==NULL)
		_throwunix("loadbmp(): Cannot open input file");

	cinfo.err=jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit=my_error_exit;
	jerr.pub.output_message=my_output_message;

	if(setjmp(jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;  goto bailout;
	}

	jpeg_create_compress(&cinfo);
	if((tempc=getc(file))<0 || ungetc(tempc, file)==EOF)
		_throwunix("loadbmp(): Could not read input file")
	else if(tempc==EOF) _throw("loadbmp(): Input file contains no data");

	if(tempc=='B')
	{
		if((src=jinit_read_bmp(&cinfo))==NULL)
			_throw("loadbmp(): Could not initialize bitmap loader");
	}
	else if(tempc=='P')
	{
		if((src=jinit_read_ppm(&cinfo))==NULL)
			_throw("loadbmp(): Could not initialize bitmap loader");
	}
	else _throw("loadbmp(): Unsupported file type");

	src->input_file=file;
	(*src->start_input)(&cinfo, src);
	(*cinfo.mem->realize_virt_arrays)((j_common_ptr)&cinfo);

	*w=cinfo.image_width;  *h=cinfo.image_height;

	if(cinfo.input_components==1 && cinfo.in_color_space==JCS_RGB)
		srcpf=TJPF_GRAY;
	else srcpf=TJPF_RGB;

	dstps=tjPixelSize[dstpf];
	if((*buf=(unsigned char *)malloc((*w)*(*h)*dstps))==NULL)
		_throw("loadbmp(): Memory allocation failure");

	while(cinfo.next_scanline<cinfo.image_height)
	{
		int i, nlines=(*src->get_pixel_rows)(&cinfo, src);
		for(i=0; i<nlines; i++)
		{
			unsigned char *outbuf;  int row;
			row=cinfo.next_scanline+i;
			if(bottomup) outbuf=&(*buf)[((*h)-row-1)*(*w)*dstps];
			else outbuf=&(*buf)[row*(*w)*dstps];
			pixelconvert(src->buffer[i], srcpf, 0, outbuf, dstpf, bottomup, *w,
				nlines);
		}
		cinfo.next_scanline+=nlines;
	}

	(*src->finish_input)(&cinfo, src);

	bailout:
	jpeg_destroy_compress(&cinfo);
	if(file) fclose(file);
	if(retval<0 && buf && *buf) {free(*buf);  *buf=NULL;}
	return retval;
}


int savebmp(char *filename, unsigned char *buf, int w, int h, int srcpf,
	int bottomup)
{
	int retval=0, srcps, dstpf;
	struct jpeg_decompress_struct dinfo;
	struct my_error_mgr jerr;
	djpeg_dest_ptr dst;
	FILE *file=NULL;
	char *ptr=NULL;

	memset(&dinfo, 0, sizeof(struct jpeg_decompress_struct));

	if(!filename || !buf || w<1 || h<1 || srcpf<0 || srcpf>=TJ_NUMPF)
		_throw("savebmp(): Invalid argument");

	if((file=fopen(filename, "wb"))==NULL)
		_throwunix("savebmp(): Cannot open output file");

	dinfo.err=jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit=my_error_exit;
	jerr.pub.output_message=my_output_message;

	if(setjmp(jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;  goto bailout;
	}

	jpeg_create_decompress(&dinfo);
	if(srcpf==TJPF_GRAY)
	{
		dinfo.out_color_components=dinfo.output_components=1;
		dinfo.out_color_space=JCS_GRAYSCALE;
	}
	else
	{
		dinfo.out_color_components=dinfo.output_components=3;
		dinfo.out_color_space=JCS_RGB;
	}
	dinfo.image_width=w;  dinfo.image_height=h;
	dinfo.global_state=DSTATE_READY;
	dinfo.scale_num=dinfo.scale_denom=1;

	ptr=strrchr(filename, '.');
	if(ptr && !strcasecmp(ptr, ".bmp"))
	{
		if((dst=jinit_write_bmp(&dinfo, 0))==NULL)
			_throw("savebmp(): Could not initialize bitmap writer");
	}
	else
	{
		if((dst=jinit_write_ppm(&dinfo))==NULL)
			_throw("savebmp(): Could not initialize PPM writer");
	}

	dst->output_file=file;
	(*dst->start_output)(&dinfo, dst);
	(*dinfo.mem->realize_virt_arrays)((j_common_ptr)&dinfo);

	if(srcpf==TJPF_GRAY) dstpf=srcpf;
	else dstpf=TJPF_RGB;
	srcps=tjPixelSize[srcpf];

	while(dinfo.output_scanline<dinfo.output_height)
	{
		int i, nlines=dst->buffer_height;
		for(i=0; i<nlines; i++)
		{
			unsigned char *inbuf;  int row;
			row=dinfo.output_scanline+i;
			if(bottomup) inbuf=&buf[(h-row-1)*w*srcps];
			else inbuf=&buf[row*w*srcps];
			pixelconvert(inbuf, srcpf, bottomup, dst->buffer[i], dstpf, 0, w,
				nlines);
		}
		(*dst->put_pixel_rows)(&dinfo, dst, nlines);
		dinfo.output_scanline+=nlines;
	}

	(*dst->finish_output)(&dinfo, dst);

	bailout:
	jpeg_destroy_decompress(&dinfo);
	if(file) fclose(file);
	return retval;
}

const char *bmpgeterr(void)
{
	return errStr;
}
