/*
 * Copyright (C)2009-2015 D. R. Commander.  All Rights Reserved.
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

/* TurboJPEG/LJT:  this implements the TurboJPEG API using libjpeg or
   libjpeg-turbo */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <jinclude.h>
#define JPEG_INTERNALS
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include "./turbojpeg.h"
#include "./tjutil.h"
#include "transupp.h"
#include "./jpegcomp.h"

extern void jpeg_mem_dest_tj(j_compress_ptr, unsigned char **,
	unsigned long *, boolean);
extern void jpeg_mem_src_tj(j_decompress_ptr, unsigned char *, unsigned long);

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))
#define isPow2(x) (((x)&(x-1))==0)


/* Error handling (based on example in example.c) */

static char errStr[JMSG_LENGTH_MAX]="No error";

struct my_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
	void (*emit_message)(j_common_ptr, int);
	boolean warning;
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

static void my_emit_message(j_common_ptr cinfo, int msg_level)
{
	my_error_ptr myerr=(my_error_ptr)cinfo->err;
	myerr->emit_message(cinfo, msg_level);
	if(msg_level<0) myerr->warning=TRUE;
}


/* Global structures, macros, etc. */

enum {COMPRESS=1, DECOMPRESS=2};

typedef struct _tjinstance
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct my_error_mgr jerr;
	int init, headerRead;
} tjinstance;

static const int pixelsize[TJ_NUMSAMP]={3, 3, 3, 1, 3, 3};

static const JXFORM_CODE xformtypes[TJ_NUMXOP]=
{
	JXFORM_NONE, JXFORM_FLIP_H, JXFORM_FLIP_V, JXFORM_TRANSPOSE,
	JXFORM_TRANSVERSE, JXFORM_ROT_90, JXFORM_ROT_180, JXFORM_ROT_270
};

#define NUMSF 16
static const tjscalingfactor sf[NUMSF]={
	{2, 1},
	{15, 8},
	{7, 4},
	{13, 8},
	{3, 2},
	{11, 8},
	{5, 4},
	{9, 8},
	{1, 1},
	{7, 8},
	{3, 4},
	{5, 8},
	{1, 2},
	{3, 8},
	{1, 4},
	{1, 8}
};

#define _throw(m) {snprintf(errStr, JMSG_LENGTH_MAX, "%s", m);  \
	retval=-1;  goto bailout;}
#define getinstance(handle) tjinstance *this=(tjinstance *)handle;  \
	j_compress_ptr cinfo=NULL;  j_decompress_ptr dinfo=NULL;  \
	if(!this) {snprintf(errStr, JMSG_LENGTH_MAX, "Invalid handle");  \
		return -1;}  \
	cinfo=&this->cinfo;  dinfo=&this->dinfo;  \
	this->jerr.warning=FALSE;
#define getcinstance(handle) tjinstance *this=(tjinstance *)handle;  \
	j_compress_ptr cinfo=NULL;  \
	if(!this) {snprintf(errStr, JMSG_LENGTH_MAX, "Invalid handle");  \
		return -1;}  \
	cinfo=&this->cinfo;  \
	this->jerr.warning=FALSE;
#define getdinstance(handle) tjinstance *this=(tjinstance *)handle;  \
	j_decompress_ptr dinfo=NULL;  \
	if(!this) {snprintf(errStr, JMSG_LENGTH_MAX, "Invalid handle");  \
		return -1;}  \
	dinfo=&this->dinfo;  \
	this->jerr.warning=FALSE;

static int getPixelFormat(int pixelSize, int flags)
{
	if(pixelSize==1) return TJPF_GRAY;
	if(pixelSize==3)
	{
		if(flags&TJ_BGR) return TJPF_BGR;
		else return TJPF_RGB;
	}
	if(pixelSize==4)
	{
		if(flags&TJ_ALPHAFIRST)
		{
			if(flags&TJ_BGR) return TJPF_XBGR;
			else return TJPF_XRGB;
		}
		else
		{
			if(flags&TJ_BGR) return TJPF_BGRX;
			else return TJPF_RGBX;
		}
	}
	return -1;
}

static int setCompDefaults(struct jpeg_compress_struct *cinfo,
	int pixelFormat, int subsamp, int jpegQual, int flags)
{
	int retval=0;
	char *env=NULL;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			cinfo->in_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			cinfo->in_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			cinfo->in_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
		case TJPF_RGBA:
			cinfo->in_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
		case TJPF_BGRA:
			cinfo->in_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
		case TJPF_ARGB:
			cinfo->in_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
		case TJPF_ABGR:
			cinfo->in_color_space=JCS_EXT_XBGR;  break;
		#else
		case TJPF_RGB:
		case TJPF_BGR:
		case TJPF_RGBX:
		case TJPF_BGRX:
		case TJPF_XRGB:
		case TJPF_XBGR:
		case TJPF_RGBA:
		case TJPF_BGRA:
		case TJPF_ARGB:
		case TJPF_ABGR:
			cinfo->in_color_space=JCS_RGB;  pixelFormat=TJPF_RGB;
			break;
		#endif
		case TJPF_CMYK:
			cinfo->in_color_space=JCS_CMYK;  break;
	}

	cinfo->input_components=tjPixelSize[pixelFormat];
	jpeg_set_defaults(cinfo);

#ifndef NO_GETENV
	if((env=getenv("TJ_OPTIMIZE"))!=NULL && strlen(env)>0 && !strcmp(env, "1"))
		cinfo->optimize_coding=TRUE;
	if((env=getenv("TJ_ARITHMETIC"))!=NULL && strlen(env)>0	&& !strcmp(env, "1"))
		cinfo->arith_code=TRUE;
	if((env=getenv("TJ_RESTART"))!=NULL && strlen(env)>0)
	{
		int temp=-1;  char tempc=0;
		if(sscanf(env, "%d%c", &temp, &tempc)>=1 && temp>=0 && temp<=65535)
		{
			if(toupper(tempc)=='B')
			{
				cinfo->restart_interval=temp;
				cinfo->restart_in_rows=0;
			}
			else
				cinfo->restart_in_rows=temp;
		}
	}
#endif

	if(jpegQual>=0)
	{
		jpeg_set_quality(cinfo, jpegQual, TRUE);
		if(jpegQual>=96 || flags&TJFLAG_ACCURATEDCT) cinfo->dct_method=JDCT_ISLOW;
		else cinfo->dct_method=JDCT_FASTEST;
	}
	if(subsamp==TJSAMP_GRAY)
		jpeg_set_colorspace(cinfo, JCS_GRAYSCALE);
	else if(pixelFormat==TJPF_CMYK)
		jpeg_set_colorspace(cinfo, JCS_YCCK);
	else jpeg_set_colorspace(cinfo, JCS_YCbCr);

#ifndef NO_GETENV
	if((env=getenv("TJ_PROGRESSIVE"))!=NULL && strlen(env)>0
		&& !strcmp(env, "1"))
		jpeg_simple_progression(cinfo);
#endif

	cinfo->comp_info[0].h_samp_factor=tjMCUWidth[subsamp]/8;
	cinfo->comp_info[1].h_samp_factor=1;
	cinfo->comp_info[2].h_samp_factor=1;
	if(cinfo->num_components>3)
		cinfo->comp_info[3].h_samp_factor=tjMCUWidth[subsamp]/8;
	cinfo->comp_info[0].v_samp_factor=tjMCUHeight[subsamp]/8;
	cinfo->comp_info[1].v_samp_factor=1;
	cinfo->comp_info[2].v_samp_factor=1;
	if(cinfo->num_components>3)
		cinfo->comp_info[3].v_samp_factor=tjMCUHeight[subsamp]/8;

	return retval;
}

static int setDecompDefaults(struct jpeg_decompress_struct *dinfo,
	int pixelFormat, int flags)
{
	int retval=0;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			dinfo->out_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			dinfo->out_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			dinfo->out_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
			dinfo->out_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
			dinfo->out_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
			dinfo->out_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
			dinfo->out_color_space=JCS_EXT_XBGR;  break;
		#if JCS_ALPHA_EXTENSIONS==1
		case TJPF_RGBA:
			dinfo->out_color_space=JCS_EXT_RGBA;  break;
		case TJPF_BGRA:
			dinfo->out_color_space=JCS_EXT_BGRA;  break;
		case TJPF_ARGB:
			dinfo->out_color_space=JCS_EXT_ARGB;  break;
		case TJPF_ABGR:
			dinfo->out_color_space=JCS_EXT_ABGR;  break;
		#endif
		#else
		case TJPF_RGB:
		case TJPF_BGR:
		case TJPF_RGBX:
		case TJPF_BGRX:
		case TJPF_XRGB:
		case TJPF_XBGR:
		case TJPF_RGBA:
		case TJPF_BGRA:
		case TJPF_ARGB:
		case TJPF_ABGR:
			dinfo->out_color_space=JCS_RGB;  break;
		#endif
		case TJPF_CMYK:
			dinfo->out_color_space=JCS_CMYK;  break;
		default:
			_throw("Unsupported pixel format");
	}

	if(flags&TJFLAG_FASTDCT) dinfo->dct_method=JDCT_FASTEST;

	bailout:
	return retval;
}


static int getSubsamp(j_decompress_ptr dinfo)
{
	int retval=-1, i, k;

	/* The sampling factors actually have no meaning with grayscale JPEG files,
	   and in fact it's possible to generate grayscale JPEGs with sampling
	   factors > 1 (even though those sampling factors are ignored by the
	   decompressor.)  Thus, we need to treat grayscale as a special case. */
	if(dinfo->num_components==1 && dinfo->jpeg_color_space==JCS_GRAYSCALE)
		return TJSAMP_GRAY;

	for(i=0; i<NUMSUBOPT; i++)
	{
		if(dinfo->num_components==pixelsize[i]
			|| ((dinfo->jpeg_color_space==JCS_YCCK
				|| dinfo->jpeg_color_space==JCS_CMYK)
					&& pixelsize[i]==3 && dinfo->num_components==4))
		{
			if(dinfo->comp_info[0].h_samp_factor==tjMCUWidth[i]/8
				&& dinfo->comp_info[0].v_samp_factor==tjMCUHeight[i]/8)
			{
				int match=0;
				for(k=1; k<dinfo->num_components; k++)
				{
					int href=1, vref=1;
					if(dinfo->jpeg_color_space==JCS_YCCK && k==3)
					{
						href=tjMCUWidth[i]/8;  vref=tjMCUHeight[i]/8;
					}
					if(dinfo->comp_info[k].h_samp_factor==href
						&& dinfo->comp_info[k].v_samp_factor==vref)
						match++;
				}
				if(match==dinfo->num_components-1)
				{
					retval=i;  break;
				}
			}
		}
	}
	return retval;
}


#ifndef JCS_EXTENSIONS

/* Conversion functions to emulate the colorspace extensions.  This allows the
   TurboJPEG wrapper to be used with libjpeg */

#define TORGB(PS, ROFFSET, GOFFSET, BOFFSET) {  \
	int rowPad=pitch-width*PS;  \
	while(height--)  \
	{  \
		unsigned char *endOfRow=src+width*PS;  \
		while(src<endOfRow)  \
		{  \
			dst[RGB_RED]=src[ROFFSET];  \
			dst[RGB_GREEN]=src[GOFFSET];  \
			dst[RGB_BLUE]=src[BOFFSET];  \
			dst+=RGB_PIXELSIZE;  src+=PS;  \
		}  \
		src+=rowPad;  \
	}  \
}

static unsigned char *toRGB(unsigned char *src, int width, int pitch,
	int height, int pixelFormat, unsigned char *dst)
{
	unsigned char *retval=src;
	switch(pixelFormat)
	{
		case TJPF_RGB:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=3
			retval=dst;  TORGB(3, 0, 1, 2);
			#endif
			break;
		case TJPF_BGR:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=3
			retval=dst;  TORGB(3, 2, 1, 0);
			#endif
			break;
		case TJPF_RGBX:
		case TJPF_RGBA:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 0, 1, 2);
			#endif
			break;
		case TJPF_BGRX:
		case TJPF_BGRA:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 2, 1, 0);
			#endif
			break;
		case TJPF_XRGB:
		case TJPF_ARGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 1, 2, 3);
			#endif
			break;
		case TJPF_XBGR:
		case TJPF_ABGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 3, 2, 1);
			#endif
			break;
	}
	return retval;
}

#define FROMRGB(PS, ROFFSET, GOFFSET, BOFFSET, SETALPHA) {  \
	int rowPad=pitch-width*PS;  \
	while(height--)  \
	{  \
		unsigned char *endOfRow=dst+width*PS;  \
		while(dst<endOfRow)  \
		{  \
			dst[ROFFSET]=src[RGB_RED];  \
			dst[GOFFSET]=src[RGB_GREEN];  \
			dst[BOFFSET]=src[RGB_BLUE];  \
			SETALPHA  \
			dst+=PS;  src+=RGB_PIXELSIZE;  \
		}  \
		dst+=rowPad;  \
	}  \
}

static void fromRGB(unsigned char *src, unsigned char *dst, int width,
	int pitch, int height, int pixelFormat)
{
	switch(pixelFormat)
	{
		case TJPF_RGB:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=3
			FROMRGB(3, 0, 1, 2,);
			#endif
			break;
		case TJPF_BGR:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=3
			FROMRGB(3, 2, 1, 0,);
			#endif
			break;
		case TJPF_RGBX:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			FROMRGB(4, 0, 1, 2,);
			#endif
			break;
		case TJPF_RGBA:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			FROMRGB(4, 0, 1, 2, dst[3]=0xFF;);
			#endif
			break;
		case TJPF_BGRX:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			FROMRGB(4, 2, 1, 0,);
			#endif
			break;
		case TJPF_BGRA:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			FROMRGB(4, 2, 1, 0, dst[3]=0xFF;);  return;
			#endif
			break;
		case TJPF_XRGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			FROMRGB(4, 1, 2, 3,);  return;
			#endif
			break;
		case TJPF_ARGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			FROMRGB(4, 1, 2, 3, dst[0]=0xFF;);  return;
			#endif
			break;
		case TJPF_XBGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			FROMRGB(4, 3, 2, 1,);  return;
			#endif
			break;
		case TJPF_ABGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			FROMRGB(4, 3, 2, 1, dst[0]=0xFF;);  return;
			#endif
			break;
	}
}

#endif


/* General API functions */

DLLEXPORT char* DLLCALL tjGetErrorStr(void)
{
	return errStr;
}


DLLEXPORT int DLLCALL tjDestroy(tjhandle handle)
{
	getinstance(handle);
	if(setjmp(this->jerr.setjmp_buffer)) return -1;
	if(this->init&COMPRESS) jpeg_destroy_compress(cinfo);
	if(this->init&DECOMPRESS) jpeg_destroy_decompress(dinfo);
	free(this);
	return 0;
}


/* These are exposed mainly because Windows can't malloc() and free() across
   DLL boundaries except when the CRT DLL is used, and we don't use the CRT DLL
   with turbojpeg.dll for compatibility reasons.  However, these functions
   can potentially be used for other purposes by different implementations. */

DLLEXPORT void DLLCALL tjFree(unsigned char *buf)
{
	if(buf) free(buf);
}


DLLEXPORT unsigned char *DLLCALL tjAlloc(int bytes)
{
	return (unsigned char *)malloc(bytes);
}


/* Compressor  */

static tjhandle _tjInitCompress(tjinstance *this)
{
	unsigned char buffer[1], *buf=buffer;  unsigned long size=1;

	/* This is also straight out of example.c */
	this->cinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;
	this->jerr.emit_message=this->jerr.pub.emit_message;
	this->jerr.pub.emit_message=my_emit_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);  return NULL;
	}

	jpeg_create_compress(&this->cinfo);
	/* Make an initial call so it will create the destination manager */
	jpeg_mem_dest_tj(&this->cinfo, &buf, &size, 0);

	this->init|=COMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitCompress(void)
{
	tjinstance *this=NULL;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitCompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitCompress(this);
}


DLLEXPORT unsigned long DLLCALL tjBufSize(int width, int height,
	int jpegSubsamp)
{
	unsigned long retval=0;  int mcuw, mcuh, chromasf;
	if(width<1 || height<1 || jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT)
		_throw("tjBufSize(): Invalid argument");

	/* This allows for rare corner cases in which a JPEG image can actually be
	   larger than the uncompressed input (we wouldn't mention it if it hadn't
	   happened before.) */
	mcuw=tjMCUWidth[jpegSubsamp];
	mcuh=tjMCUHeight[jpegSubsamp];
	chromasf=jpegSubsamp==TJSAMP_GRAY? 0: 4*64/(mcuw*mcuh);
	retval=PAD(width, mcuw) * PAD(height, mcuh) * (2 + chromasf) + 2048;

	bailout:
	return retval;
}

DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height)
{
	unsigned long retval=0;
	if(width<1 || height<1)
		_throw("TJBUFSIZE(): Invalid argument");

	/* This allows for rare corner cases in which a JPEG image can actually be
	   larger than the uncompressed input (we wouldn't mention it if it hadn't
	   happened before.) */
	retval=PAD(width, 16) * PAD(height, 16) * 6 + 2048;

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL tjBufSizeYUV2(int width, int pad, int height,
	int subsamp)
{
	int retval=0, nc, i;

	if(subsamp<0 || subsamp>=NUMSUBOPT)
		_throw("tjBufSizeYUV2(): Invalid argument");

	nc=(subsamp==TJSAMP_GRAY? 1:3);
	for(i=0; i<nc; i++)
	{
		int pw=tjPlaneWidth(i, width, subsamp);
		int stride=PAD(pw, pad);
		int ph=tjPlaneHeight(i, height, subsamp);
		if(pw<0 || ph<0) return -1;
		else retval+=stride*ph;
	}

	bailout:
	return retval;
}

DLLEXPORT unsigned long DLLCALL tjBufSizeYUV(int width, int height,
	int subsamp)
{
	return tjBufSizeYUV2(width, 4, height, subsamp);
}

DLLEXPORT unsigned long DLLCALL TJBUFSIZEYUV(int width, int height,
	int subsamp)
{
	return tjBufSizeYUV(width, height, subsamp);
}


DLLEXPORT int tjPlaneWidth(int componentID, int width, int subsamp)
{
	int pw, nc, retval=0;

	if(width<1 || subsamp<0 || subsamp>=TJ_NUMSAMP)
		_throw("tjPlaneWidth(): Invalid argument");
	nc=(subsamp==TJSAMP_GRAY? 1:3);
	if(componentID<0 || componentID>=nc)
		_throw("tjPlaneWidth(): Invalid argument");

	pw=PAD(width, tjMCUWidth[subsamp]/8);
	if(componentID==0)
		retval=pw;
	else
		retval=pw*8/tjMCUWidth[subsamp];

	bailout:
	return retval;
}


DLLEXPORT int tjPlaneHeight(int componentID, int height, int subsamp)
{
	int ph, nc, retval=0;

	if(height<1 || subsamp<0 || subsamp>=TJ_NUMSAMP)
		_throw("tjPlaneHeight(): Invalid argument");
	nc=(subsamp==TJSAMP_GRAY? 1:3);
	if(componentID<0 || componentID>=nc)
		_throw("tjPlaneHeight(): Invalid argument");

	ph=PAD(height, tjMCUHeight[subsamp]/8);
	if(componentID==0)
		retval=ph;
	else
		retval=ph*8/tjMCUHeight[subsamp];

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL tjPlaneSizeYUV(int componentID, int width,
	int stride, int height, int subsamp)
{
	unsigned long retval=0;
	int pw, ph;

	if(width<1 || height<1 || subsamp<0 || subsamp>=NUMSUBOPT)
		_throw("tjPlaneSizeYUV(): Invalid argument");

	pw=tjPlaneWidth(componentID, width, subsamp);
	ph=tjPlaneHeight(componentID, height, subsamp);
	if(pw<0 || ph<0) return -1;

	if(stride==0) stride=pw;
	else stride=abs(stride);

	retval=stride*(ph-1)+pw;

	bailout:
	return retval;
}


DLLEXPORT int DLLCALL tjCompress2(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char **jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int i, retval=0, alloc=1;  JSAMPROW *row_pointer=NULL;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	#endif

	getcinstance(handle)
	if((this->init&COMPRESS)==0)
		_throw("tjCompress2(): Instance has not been initialized for compression");

	if(srcBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF || jpegBuf==NULL || jpegSize==NULL
		|| jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT || jpegQual<0 || jpegQual>100)
		_throw("tjCompress2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY && pixelFormat!=TJPF_CMYK)
	{
		rgbBuf=(unsigned char *)malloc(width*height*RGB_PIXELSIZE);
		if(!rgbBuf) _throw("tjCompress2(): Memory allocation failure");
		srcBuf=toRGB(srcBuf, width, pitch, height, pixelFormat, rgbBuf);
		pitch=width*RGB_PIXELSIZE;
	}
	#endif

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(flags&TJFLAG_NOREALLOC)
	{
		alloc=0;  *jpegSize=tjBufSize(width, height, jpegSubsamp);
	}
	jpeg_mem_dest_tj(cinfo, jpegBuf, jpegSize, alloc);
	if(setCompDefaults(cinfo, pixelFormat, jpegSubsamp, jpegQual, flags)==-1)
		return -1;

	jpeg_start_compress(cinfo, TRUE);
	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
		_throw("tjCompress2(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&srcBuf[(height-i-1)*pitch];
		else row_pointer[i]=&srcBuf[i*pitch];
	}
	while(cinfo->next_scanline<cinfo->image_height)
	{
		jpeg_write_scanlines(cinfo, &row_pointer[cinfo->next_scanline],
			cinfo->image_height-cinfo->next_scanline);
	}
	jpeg_finish_compress(cinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjCompress(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelSize, unsigned char *jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int retval=0;  unsigned long size;
	if(flags&TJ_YUV)
	{
		size=tjBufSizeYUV(width, height, jpegSubsamp);
		retval=tjEncodeYUV2(handle, srcBuf, width, pitch, height,
			getPixelFormat(pixelSize, flags), jpegBuf, jpegSubsamp, flags);
	}
	else
	{
		retval=tjCompress2(handle, srcBuf, width, pitch, height,
			getPixelFormat(pixelSize, flags), &jpegBuf, &size, jpegSubsamp, jpegQual,
			flags|TJFLAG_NOREALLOC);
	}
	*jpegSize=size;
	return retval;
}


DLLEXPORT int DLLCALL tjEncodeYUVPlanes(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char **dstPlanes,
	int *strides, int subsamp, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	JSAMPLE *_tmpbuf[MAX_COMPONENTS], *_tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *tmpbuf[MAX_COMPONENTS], *tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *outbuf[MAX_COMPONENTS];
	int row, pw0, ph0, pw[MAX_COMPONENTS], ph[MAX_COMPONENTS];
	JSAMPLE *ptr;
	jpeg_component_info *compptr;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	#endif

	getcinstance(handle);

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  _tmpbuf[i]=NULL;
		tmpbuf2[i]=NULL;  _tmpbuf2[i]=NULL;  outbuf[i]=NULL;
	}

	if((this->init&COMPRESS)==0)
		_throw("tjEncodeYUVPlanes(): Instance has not been initialized for compression");

	if(srcBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF || !dstPlanes || !dstPlanes[0] || subsamp<0
		|| subsamp>=NUMSUBOPT)
		_throw("tjEncodeYUVPlanes(): Invalid argument");
	if(subsamp!=TJSAMP_GRAY && (!dstPlanes[1] || !dstPlanes[2]))
		_throw("tjEncodeYUVPlanes(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pixelFormat==TJPF_CMYK)
		_throw("tjEncodeYUVPlanes(): Cannot generate YUV images from CMYK pixels");

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY && pixelFormat!=TJPF_CMYK)
	{
		rgbBuf=(unsigned char *)malloc(width*height*RGB_PIXELSIZE);
		if(!rgbBuf) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		srcBuf=toRGB(srcBuf, width, pitch, height, pixelFormat, rgbBuf);
		pitch=width*RGB_PIXELSIZE;
	}
	#endif

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setCompDefaults(cinfo, pixelFormat, subsamp, -1, flags)==-1) return -1;

	/* Execute only the parts of jpeg_start_compress() that we need.  If we
	   were to call the whole jpeg_start_compress() function, then it would try
	   to write the file headers, which could overflow the output buffer if the
	   YUV image were very small. */
	if(cinfo->global_state!=CSTATE_START)
		_throw("tjEncodeYUVPlanes(): libjpeg API is in the wrong state");
	(*cinfo->err->reset_error_mgr)((j_common_ptr)cinfo);
	jinit_c_master_control(cinfo, FALSE);
	jinit_color_converter(cinfo);
	jinit_downsampler(cinfo);
	(*cinfo->cconvert->start_pass)(cinfo);

	pw0=PAD(width, cinfo->max_h_samp_factor);
	ph0=PAD(height, cinfo->max_v_samp_factor);

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph0))==NULL)
		_throw("tjEncodeYUVPlanes(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&srcBuf[(height-i-1)*pitch];
		else row_pointer[i]=&srcBuf[i*pitch];
	}
	if(height<ph0)
		for(i=height; i<ph0; i++) row_pointer[i]=row_pointer[height-1];

	for(i=0; i<cinfo->num_components; i++)
	{
		compptr=&cinfo->comp_info[i];
		_tmpbuf[i]=(JSAMPLE *)malloc(
			PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
				/compptr->h_samp_factor, 16) * cinfo->max_v_samp_factor + 16);
		if(!_tmpbuf[i]) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*cinfo->max_v_samp_factor);
		if(!tmpbuf[i]) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		for(row=0; row<cinfo->max_v_samp_factor; row++)
		{
			unsigned char *_tmpbuf_aligned=
				(unsigned char *)PAD((size_t)_tmpbuf[i], 16);
			tmpbuf[i][row]=&_tmpbuf_aligned[
				PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
					/compptr->h_samp_factor, 16) * row];
		}
		_tmpbuf2[i]=(JSAMPLE *)malloc(PAD(compptr->width_in_blocks*DCTSIZE, 16)
			* compptr->v_samp_factor + 16);
		if(!_tmpbuf2[i]) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		tmpbuf2[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*compptr->v_samp_factor);
		if(!tmpbuf2[i]) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		for(row=0; row<compptr->v_samp_factor; row++)
		{
			unsigned char *_tmpbuf2_aligned=
				(unsigned char *)PAD((size_t)_tmpbuf2[i], 16);
			tmpbuf2[i][row]=&_tmpbuf2_aligned[
				PAD(compptr->width_in_blocks*DCTSIZE, 16) * row];
		}
		pw[i]=pw0*compptr->h_samp_factor/cinfo->max_h_samp_factor;
		ph[i]=ph0*compptr->v_samp_factor/cinfo->max_v_samp_factor;
		outbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph[i]);
		if(!outbuf[i]) _throw("tjEncodeYUVPlanes(): Memory allocation failure");
		ptr=dstPlanes[i];
		for(row=0; row<ph[i]; row++)
		{
			outbuf[i][row]=ptr;
			ptr+=(strides && strides[i]!=0)? strides[i]:pw[i];
		}
	}

	for(row=0; row<ph0; row+=cinfo->max_v_samp_factor)
	{
		(*cinfo->cconvert->color_convert)(cinfo, &row_pointer[row], tmpbuf, 0,
			cinfo->max_v_samp_factor);
		(cinfo->downsample->downsample)(cinfo, tmpbuf, 0, tmpbuf2, 0);
		for(i=0, compptr=cinfo->comp_info; i<cinfo->num_components; i++, compptr++)
			jcopy_sample_rows(tmpbuf2[i], 0, outbuf[i],
				row*compptr->v_samp_factor/cinfo->max_v_samp_factor,
				compptr->v_samp_factor, pw[i]);
	}
	cinfo->next_scanline+=height;
	jpeg_abort_compress(cinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]!=NULL) free(tmpbuf[i]);
		if(_tmpbuf[i]!=NULL) free(_tmpbuf[i]);
		if(tmpbuf2[i]!=NULL) free(tmpbuf2[i]);
		if(_tmpbuf2[i]!=NULL) free(_tmpbuf2[i]);
		if(outbuf[i]!=NULL) free(outbuf[i]);
	}
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjEncodeYUV3(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char *dstBuf,
	int pad, int subsamp, int flags)
{
	unsigned char *dstPlanes[3];
	int pw0, ph0, strides[3], retval=-1;

	if(width<=0 || height<=0 || dstBuf==NULL || pad<0 || !isPow2(pad)
		|| subsamp<0 || subsamp>=NUMSUBOPT)
		_throw("tjEncodeYUV3(): Invalid argument");

	pw0=tjPlaneWidth(0, width, subsamp);
	ph0=tjPlaneHeight(0, height, subsamp);
	dstPlanes[0]=dstBuf;
	strides[0]=PAD(pw0, pad);
	if(subsamp==TJSAMP_GRAY)
	{
		strides[1]=strides[2]=0;
		dstPlanes[1]=dstPlanes[2]=NULL;
	}
	else
	{
		int pw1=tjPlaneWidth(1, width, subsamp);
		int ph1=tjPlaneHeight(1, height, subsamp);
		strides[1]=strides[2]=PAD(pw1, pad);
		dstPlanes[1]=dstPlanes[0]+strides[0]*ph0;
		dstPlanes[2]=dstPlanes[1]+strides[1]*ph1;
	}

	return tjEncodeYUVPlanes(handle, srcBuf, width, pitch, height, pixelFormat,
		dstPlanes, strides, subsamp, flags);

	bailout:
	return retval;
}

DLLEXPORT int DLLCALL tjEncodeYUV2(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char *dstBuf,
	int subsamp, int flags)
{
	return tjEncodeYUV3(handle, srcBuf, width, pitch, height, pixelFormat,
		dstBuf, 4, subsamp, flags);
}

DLLEXPORT int DLLCALL tjEncodeYUV(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelSize, unsigned char *dstBuf,
	int subsamp, int flags)
{
	return tjEncodeYUV2(handle, srcBuf, width, pitch, height,
		getPixelFormat(pixelSize, flags), dstBuf, subsamp, flags);
}


DLLEXPORT int DLLCALL tjCompressFromYUVPlanes(tjhandle handle,
	unsigned char **srcPlanes, int width, int *strides, int height, int subsamp,
	unsigned char **jpegBuf, unsigned long *jpegSize, int jpegQual, int flags)
{
	int i, row, retval=0, alloc=1;  JSAMPROW *inbuf[MAX_COMPONENTS];
	int pw[MAX_COMPONENTS], ph[MAX_COMPONENTS], iw[MAX_COMPONENTS],
		tmpbufsize=0, usetmpbuf=0, th[MAX_COMPONENTS];
	JSAMPLE *_tmpbuf=NULL, *ptr;  JSAMPROW *tmpbuf[MAX_COMPONENTS];

	getcinstance(handle)

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  inbuf[i]=NULL;
	}

	if((this->init&COMPRESS)==0)
		_throw("tjCompressFromYUVPlanes(): Instance has not been initialized for compression");

	if(!srcPlanes || !srcPlanes[0] || width<=0 || height<=0 || subsamp<0
		|| subsamp>=NUMSUBOPT || jpegBuf==NULL || jpegSize==NULL || jpegQual<0
		|| jpegQual>100)
		_throw("tjCompressFromYUVPlanes(): Invalid argument");
	if(subsamp!=TJSAMP_GRAY && (!srcPlanes[1] || !srcPlanes[2]))
		_throw("tjCompressFromYUVPlanes(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(flags&TJFLAG_NOREALLOC)
	{
		alloc=0;  *jpegSize=tjBufSize(width, height, subsamp);
	}
	jpeg_mem_dest_tj(cinfo, jpegBuf, jpegSize, alloc);
	if(setCompDefaults(cinfo, TJPF_RGB, subsamp, jpegQual, flags)==-1)
		return -1;
	cinfo->raw_data_in=TRUE;

	jpeg_start_compress(cinfo, TRUE);
	for(i=0; i<cinfo->num_components; i++)
	{
		jpeg_component_info *compptr=&cinfo->comp_info[i];
		int ih;
		iw[i]=compptr->width_in_blocks*DCTSIZE;
		ih=compptr->height_in_blocks*DCTSIZE;
		pw[i]=PAD(cinfo->image_width, cinfo->max_h_samp_factor)
			*compptr->h_samp_factor/cinfo->max_h_samp_factor;
		ph[i]=PAD(cinfo->image_height, cinfo->max_v_samp_factor)
			*compptr->v_samp_factor/cinfo->max_v_samp_factor;
		if(iw[i]!=pw[i] || ih!=ph[i]) usetmpbuf=1;
		th[i]=compptr->v_samp_factor*DCTSIZE;
		tmpbufsize+=iw[i]*th[i];
		if((inbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph[i]))==NULL)
			_throw("tjCompressFromYUVPlanes(): Memory allocation failure");
		ptr=srcPlanes[i];
		for(row=0; row<ph[i]; row++)
		{
			inbuf[i][row]=ptr;
			ptr+=(strides && strides[i]!=0)? strides[i]:pw[i];
		}
	}
	if(usetmpbuf)
	{
		if((_tmpbuf=(JSAMPLE *)malloc(sizeof(JSAMPLE)*tmpbufsize))==NULL)
			_throw("tjCompressFromYUVPlanes(): Memory allocation failure");
		ptr=_tmpbuf;
		for(i=0; i<cinfo->num_components; i++)
		{
			if((tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*th[i]))==NULL)
				_throw("tjCompressFromYUVPlanes(): Memory allocation failure");
			for(row=0; row<th[i]; row++)
			{
				tmpbuf[i][row]=ptr;
				ptr+=iw[i];
			}
		}
	}

	for(row=0; row<(int)cinfo->image_height;
		row+=cinfo->max_v_samp_factor*DCTSIZE)
	{
		JSAMPARRAY yuvptr[MAX_COMPONENTS];
		int crow[MAX_COMPONENTS];
		for(i=0; i<cinfo->num_components; i++)
		{
			jpeg_component_info *compptr=&cinfo->comp_info[i];
			crow[i]=row*compptr->v_samp_factor/cinfo->max_v_samp_factor;
			if(usetmpbuf)
			{
				int j, k;
				for(j=0; j<min(th[i], ph[i]-crow[i]); j++)
				{
					memcpy(tmpbuf[i][j], inbuf[i][crow[i]+j], pw[i]);
					/* Duplicate last sample in row to fill out MCU */
					for(k=pw[i]; k<iw[i]; k++) tmpbuf[i][j][k]=tmpbuf[i][j][pw[i]-1];
				}
				/* Duplicate last row to fill out MCU */
				for(j=ph[i]-crow[i]; j<th[i]; j++)
					memcpy(tmpbuf[i][j], tmpbuf[i][ph[i]-crow[i]-1], iw[i]);
				yuvptr[i]=tmpbuf[i];
			}
			else
				yuvptr[i]=&inbuf[i][crow[i]];
		}
		jpeg_write_raw_data(cinfo, yuvptr, cinfo->max_v_samp_factor*DCTSIZE);
	}
	jpeg_finish_compress(cinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]) free(tmpbuf[i]);
		if(inbuf[i]) free(inbuf[i]);
	}
	if(_tmpbuf) free(_tmpbuf);
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjCompressFromYUV(tjhandle handle, unsigned char *srcBuf,
	int width, int pad, int height, int subsamp, unsigned char **jpegBuf,
	unsigned long *jpegSize, int jpegQual, int flags)
{
	unsigned char *srcPlanes[3];
	int pw0, ph0, strides[3], retval=-1;

	if(srcBuf==NULL || width<=0 || pad<1 || height<=0 || subsamp<0
		|| subsamp>=NUMSUBOPT)
		_throw("tjCompressFromYUV(): Invalid argument");

	pw0=tjPlaneWidth(0, width, subsamp);
	ph0=tjPlaneHeight(0, height, subsamp);
	srcPlanes[0]=srcBuf;
	strides[0]=PAD(pw0, pad);
	if(subsamp==TJSAMP_GRAY)
	{
		strides[1]=strides[2]=0;
		srcPlanes[1]=srcPlanes[2]=NULL;
	}
	else
	{
		int pw1=tjPlaneWidth(1, width, subsamp);
		int ph1=tjPlaneHeight(1, height, subsamp);
		strides[1]=strides[2]=PAD(pw1, pad);
		srcPlanes[1]=srcPlanes[0]+strides[0]*ph0;
		srcPlanes[2]=srcPlanes[1]+strides[1]*ph1;
	}

	return tjCompressFromYUVPlanes(handle, srcPlanes, width, strides, height,
		subsamp, jpegBuf, jpegSize, jpegQual, flags);

	bailout:
	return retval;
}


/* Decompressor */

static tjhandle _tjInitDecompress(tjinstance *this)
{
	unsigned char buffer[1];

	/* This is also straight out of example.c */
	this->dinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;
	this->jerr.emit_message=this->jerr.pub.emit_message;
	this->jerr.pub.emit_message=my_emit_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);  return NULL;
	}

	jpeg_create_decompress(&this->dinfo);
	/* Make an initial call so it will create the source manager */
	jpeg_mem_src_tj(&this->dinfo, buffer, 1);

	this->init|=DECOMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitDecompress(void)
{
	tjinstance *this;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitDecompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitDecompress(this);
}


DLLEXPORT int DLLCALL tjDecompressHeader3(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height,
	int *jpegSubsamp, int *jpegColorspace)
{
	int retval=0;

	getdinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompressHeader3(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || width==NULL || height==NULL
		|| jpegSubsamp==NULL || jpegColorspace==NULL)
		_throw("tjDecompressHeader3(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		return -1;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);

	*width=dinfo->image_width;
	*height=dinfo->image_height;
	*jpegSubsamp=getSubsamp(dinfo);
	switch(dinfo->jpeg_color_space)
	{
		case JCS_GRAYSCALE:  *jpegColorspace=TJCS_GRAY;  break;
		case JCS_RGB:        *jpegColorspace=TJCS_RGB;  break;
		case JCS_YCbCr:      *jpegColorspace=TJCS_YCbCr;  break;
		case JCS_CMYK:       *jpegColorspace=TJCS_CMYK;  break;
		case JCS_YCCK:       *jpegColorspace=TJCS_YCCK;  break;
		default:             *jpegColorspace=-1;  break;
	}

	jpeg_abort_decompress(dinfo);

	if(*jpegSubsamp<0)
		_throw("tjDecompressHeader3(): Could not determine subsampling type for JPEG image");
	if(*jpegColorspace<0)
		_throw("tjDecompressHeader3(): Could not determine colorspace of JPEG image");
	if(*width<1 || *height<1)
		_throw("tjDecompressHeader3(): Invalid data returned in header");

	bailout:
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height,
	int *jpegSubsamp)
{
	int jpegColorspace;
	return tjDecompressHeader3(handle, jpegBuf, jpegSize, width, height,
		jpegSubsamp, &jpegColorspace);
}

DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height)
{
	int jpegSubsamp;
	return tjDecompressHeader2(handle, jpegBuf, jpegSize, width, height,
		&jpegSubsamp);
}


DLLEXPORT tjscalingfactor* DLLCALL tjGetScalingFactors(int *numscalingfactors)
{
	if(numscalingfactors==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjGetScalingFactors(): Invalid argument");
		return NULL;
	}

	*numscalingfactors=NUMSF;
	return (tjscalingfactor *)sf;
}


DLLEXPORT int DLLCALL tjDecompress2(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelFormat, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	int jpegwidth, jpegheight, scaledw, scaledh;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	unsigned char *_dstBuf=NULL;  int _pitch=0;
	#endif

	getdinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompress2(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || dstBuf==NULL || width<0 || pitch<0
		|| height<0 || pixelFormat<0 || pixelFormat>=TJ_NUMPF)
		_throw("tjDecompress2(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);
	if(setDecompDefaults(dinfo, pixelFormat, flags)==-1)
	{
		retval=-1;  goto bailout;
	}

	if(flags&TJFLAG_FASTUPSAMPLE) dinfo->do_fancy_upsampling=FALSE;

	jpegwidth=dinfo->image_width;  jpegheight=dinfo->image_height;
	if(width==0) width=jpegwidth;
	if(height==0) height=jpegheight;
	for(i=0; i<NUMSF; i++)
	{
		scaledw=TJSCALED(jpegwidth, sf[i]);
		scaledh=TJSCALED(jpegheight, sf[i]);
		if(scaledw<=width && scaledh<=height)
			break;
	}
	if(i>=NUMSF)
		_throw("tjDecompress2(): Could not scale down to desired image dimensions");
	width=scaledw;  height=scaledh;
	dinfo->scale_num=sf[i].num;
	dinfo->scale_denom=sf[i].denom;

	jpeg_start_decompress(dinfo);
	if(pitch==0) pitch=dinfo->output_width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY && pixelFormat!=TJPF_CMYK &&
		(RGB_RED!=tjRedOffset[pixelFormat] ||
			RGB_GREEN!=tjGreenOffset[pixelFormat] ||
			RGB_BLUE!=tjBlueOffset[pixelFormat] ||
			RGB_PIXELSIZE!=tjPixelSize[pixelFormat]))
	{
		rgbBuf=(unsigned char *)malloc(width*height*3);
		if(!rgbBuf) _throw("tjDecompress2(): Memory allocation failure");
		_pitch=pitch;  pitch=width*3;
		_dstBuf=dstBuf;  dstBuf=rgbBuf;
	}
	#endif

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)
		*dinfo->output_height))==NULL)
		_throw("tjDecompress2(): Memory allocation failure");
	for(i=0; i<(int)dinfo->output_height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP)
			row_pointer[i]=&dstBuf[(dinfo->output_height-i-1)*pitch];
		else row_pointer[i]=&dstBuf[i*pitch];
	}
	while(dinfo->output_scanline<dinfo->output_height)
	{
		jpeg_read_scanlines(dinfo, &row_pointer[dinfo->output_scanline],
			dinfo->output_height-dinfo->output_scanline);
	}
	jpeg_finish_decompress(dinfo);

	#ifndef JCS_EXTENSIONS
	fromRGB(rgbBuf, _dstBuf, width, _pitch, height, pixelFormat);
	#endif

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjDecompress(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelSize, int flags)
{
	if(flags&TJ_YUV)
		return tjDecompressToYUV(handle, jpegBuf, jpegSize, dstBuf, flags);
	else
		return tjDecompress2(handle, jpegBuf, jpegSize, dstBuf, width, pitch,
			height, getPixelFormat(pixelSize, flags), flags);
}


static int setDecodeDefaults(struct jpeg_decompress_struct *dinfo,
	int pixelFormat, int subsamp, int flags)
{
	int i;

	dinfo->scale_num=dinfo->scale_denom=1;

	if(subsamp==TJSAMP_GRAY)
	{
		dinfo->num_components=dinfo->comps_in_scan=1;
		dinfo->jpeg_color_space=JCS_GRAYSCALE;
	}
	else
	{
		dinfo->num_components=dinfo->comps_in_scan=3;
		dinfo->jpeg_color_space=JCS_YCbCr;
	}

	dinfo->comp_info=(jpeg_component_info *)
		(*dinfo->mem->alloc_small)((j_common_ptr)dinfo, JPOOL_IMAGE,
			dinfo->num_components*sizeof(jpeg_component_info));

	for(i=0; i<dinfo->num_components; i++)
	{
		jpeg_component_info *compptr=&dinfo->comp_info[i];
		compptr->h_samp_factor=(i==0)? tjMCUWidth[subsamp]/8:1;
		compptr->v_samp_factor=(i==0)? tjMCUHeight[subsamp]/8:1;
		compptr->component_index=i;
		compptr->component_id=i+1;
		compptr->quant_tbl_no=compptr->dc_tbl_no=compptr->ac_tbl_no=
			(i==0)? 0:1;
		dinfo->cur_comp_info[i]=compptr;
	}
	dinfo->data_precision=8;
	for(i=0; i<2; i++)
	{
		if(dinfo->quant_tbl_ptrs[i]==NULL)
			dinfo->quant_tbl_ptrs[i]=jpeg_alloc_quant_table((j_common_ptr)dinfo);
	}

	return 0;
}


int my_read_markers(j_decompress_ptr dinfo)
{
	return JPEG_REACHED_SOS;
}

void my_reset_marker_reader(j_decompress_ptr dinfo)
{
}

DLLEXPORT int DLLCALL tjDecodeYUVPlanes(tjhandle handle,
	unsigned char **srcPlanes, int *strides, int subsamp, unsigned char *dstBuf,
	int width, int pitch, int height, int pixelFormat, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	JSAMPLE *_tmpbuf[MAX_COMPONENTS];
	JSAMPROW *tmpbuf[MAX_COMPONENTS], *inbuf[MAX_COMPONENTS];
	int row, pw0, ph0, pw[MAX_COMPONENTS], ph[MAX_COMPONENTS];
	JSAMPLE *ptr;
	jpeg_component_info *compptr;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	unsigned char *_dstBuf=NULL;  int _pitch=0;
	#endif
	int (*old_read_markers)(j_decompress_ptr);
	void (*old_reset_marker_reader)(j_decompress_ptr);

	getdinstance(handle);

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  _tmpbuf[i]=NULL;  inbuf[i]=NULL;
	}

	if((this->init&DECOMPRESS)==0)
		_throw("tjDecodeYUVPlanes(): Instance has not been initialized for decompression");

	if(!srcPlanes || !srcPlanes[0] || subsamp<0 || subsamp>=NUMSUBOPT
		|| dstBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF)
		_throw("tjDecodeYUVPlanes(): Invalid argument");
	if(subsamp!=TJSAMP_GRAY && (!srcPlanes[1] || !srcPlanes[2]))
		_throw("tjDecodeYUVPlanes(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pixelFormat==TJPF_CMYK)
		_throw("tjDecodeYUVPlanes(): Cannot decode YUV images into CMYK pixels.");

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];
	dinfo->image_width=width;
	dinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setDecodeDefaults(dinfo, pixelFormat, subsamp, flags)==-1)
	{
		retval=-1;  goto bailout;
	}
	old_read_markers=dinfo->marker->read_markers;
	dinfo->marker->read_markers=my_read_markers;
	old_reset_marker_reader=dinfo->marker->reset_marker_reader;
	dinfo->marker->reset_marker_reader=my_reset_marker_reader;
	jpeg_read_header(dinfo, TRUE);
	dinfo->marker->read_markers=old_read_markers;
	dinfo->marker->reset_marker_reader=old_reset_marker_reader;

	if(setDecompDefaults(dinfo, pixelFormat, flags)==-1)
	{
		retval=-1;  goto bailout;
	}
	dinfo->do_fancy_upsampling=FALSE;
	dinfo->Se=DCTSIZE2-1;
	jinit_master_decompress(dinfo);
	(*dinfo->upsample->start_pass)(dinfo);

	pw0=PAD(width, dinfo->max_h_samp_factor);
	ph0=PAD(height, dinfo->max_v_samp_factor);

	if(pitch==0) pitch=dinfo->output_width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY && pixelFormat!=TJPF_CMYK &&
		(RGB_RED!=tjRedOffset[pixelFormat] ||
			RGB_GREEN!=tjGreenOffset[pixelFormat] ||
			RGB_BLUE!=tjBlueOffset[pixelFormat] ||
			RGB_PIXELSIZE!=tjPixelSize[pixelFormat]))
	{
		rgbBuf=(unsigned char *)malloc(width*height*3);
		if(!rgbBuf) _throw("tjDecodeYUVPlanes(): Memory allocation failure");
		_pitch=pitch;  pitch=width*3;
		_dstBuf=dstBuf;  dstBuf=rgbBuf;
	}
	#endif

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph0))==NULL)
		_throw("tjDecodeYUVPlanes(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&dstBuf[(height-i-1)*pitch];
		else row_pointer[i]=&dstBuf[i*pitch];
	}
	if(height<ph0)
		for(i=height; i<ph0; i++) row_pointer[i]=row_pointer[height-1];

	for(i=0; i<dinfo->num_components; i++)
	{
		compptr=&dinfo->comp_info[i];
		_tmpbuf[i]=(JSAMPLE *)malloc(PAD(compptr->width_in_blocks*DCTSIZE, 16)
			* compptr->v_samp_factor + 16);
		if(!_tmpbuf[i]) _throw("tjDecodeYUVPlanes(): Memory allocation failure");
		tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*compptr->v_samp_factor);
		if(!tmpbuf[i]) _throw("tjDecodeYUVPlanes(): Memory allocation failure");
		for(row=0; row<compptr->v_samp_factor; row++)
		{
			unsigned char *_tmpbuf_aligned=
				(unsigned char *)PAD((size_t)_tmpbuf[i], 16);
			tmpbuf[i][row]=&_tmpbuf_aligned[
				PAD(compptr->width_in_blocks*DCTSIZE, 16) * row];
		}
		pw[i]=pw0*compptr->h_samp_factor/dinfo->max_h_samp_factor;
		ph[i]=ph0*compptr->v_samp_factor/dinfo->max_v_samp_factor;
		inbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph[i]);
		if(!inbuf[i]) _throw("tjDecodeYUVPlanes(): Memory allocation failure");
		ptr=srcPlanes[i];
		for(row=0; row<ph[i]; row++)
		{
			inbuf[i][row]=ptr;
			ptr+=(strides && strides[i]!=0)? strides[i]:pw[i];
		}
	}

	for(row=0; row<ph0; row+=dinfo->max_v_samp_factor)
	{
		JDIMENSION inrow=0, outrow=0;
		for(i=0, compptr=dinfo->comp_info; i<dinfo->num_components; i++, compptr++)
			jcopy_sample_rows(inbuf[i],
				row*compptr->v_samp_factor/dinfo->max_v_samp_factor, tmpbuf[i], 0,
				compptr->v_samp_factor, pw[i]);
		(dinfo->upsample->upsample)(dinfo, tmpbuf, &inrow,
			dinfo->max_v_samp_factor, &row_pointer[row], &outrow,
			dinfo->max_v_samp_factor);
	}
	jpeg_abort_decompress(dinfo);

	#ifndef JCS_EXTENSIONS
	fromRGB(rgbBuf, _dstBuf, width, _pitch, height, pixelFormat);
	#endif

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]!=NULL) free(tmpbuf[i]);
		if(_tmpbuf[i]!=NULL) free(_tmpbuf[i]);
		if(inbuf[i]!=NULL) free(inbuf[i]);
	}
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjDecodeYUV(tjhandle handle, unsigned char *srcBuf,
	int pad, int subsamp, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelFormat, int flags)
{
	unsigned char *srcPlanes[3];
	int pw0, ph0, strides[3], retval=-1;

	if(srcBuf==NULL || pad<0 || !isPow2(pad) || subsamp<0 || subsamp>=NUMSUBOPT
		|| width<=0 || height<=0)
		_throw("tjDecodeYUV(): Invalid argument");

	pw0=tjPlaneWidth(0, width, subsamp);
	ph0=tjPlaneHeight(0, height, subsamp);
	srcPlanes[0]=srcBuf;
	strides[0]=PAD(pw0, pad);
	if(subsamp==TJSAMP_GRAY)
	{
		strides[1]=strides[2]=0;
		srcPlanes[1]=srcPlanes[2]=NULL;
	}
	else
	{
		int pw1=tjPlaneWidth(1, width, subsamp);
		int ph1=tjPlaneHeight(1, height, subsamp);
		strides[1]=strides[2]=PAD(pw1, pad);
		srcPlanes[1]=srcPlanes[0]+strides[0]*ph0;
		srcPlanes[2]=srcPlanes[1]+strides[1]*ph1;
	}

	return tjDecodeYUVPlanes(handle, srcPlanes, strides, subsamp, dstBuf, width,
		pitch, height, pixelFormat, flags);

	bailout:
	return retval;
}

DLLEXPORT int DLLCALL tjDecompressToYUVPlanes(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, unsigned char **dstPlanes,
	int width, int *strides, int height, int flags)
{
	int i, sfi, row, retval=0;  JSAMPROW *outbuf[MAX_COMPONENTS];
	int jpegwidth, jpegheight, jpegSubsamp, scaledw, scaledh;
	int pw[MAX_COMPONENTS], ph[MAX_COMPONENTS], iw[MAX_COMPONENTS],
		tmpbufsize=0, usetmpbuf=0, th[MAX_COMPONENTS];
	JSAMPLE *_tmpbuf=NULL, *ptr;  JSAMPROW *tmpbuf[MAX_COMPONENTS];
	int dctsize;

	getdinstance(handle);

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  outbuf[i]=NULL;
	}

	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompressToYUVPlanes(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || !dstPlanes || !dstPlanes[0] || width<0
		|| height<0)
		_throw("tjDecompressToYUVPlanes(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(!this->headerRead)
	{
		jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
		jpeg_read_header(dinfo, TRUE);
	}
	this->headerRead=0;
	jpegSubsamp=getSubsamp(dinfo);
	if(jpegSubsamp<0)
		_throw("tjDecompressToYUVPlanes(): Could not determine subsampling type for JPEG image");

	if(jpegSubsamp!=TJSAMP_GRAY && (!dstPlanes[1] || !dstPlanes[2]))
		_throw("tjDecompressToYUVPlanes(): Invalid argument");

	jpegwidth=dinfo->image_width;  jpegheight=dinfo->image_height;
	if(width==0) width=jpegwidth;
	if(height==0) height=jpegheight;
	for(i=0; i<NUMSF; i++)
	{
		scaledw=TJSCALED(jpegwidth, sf[i]);
		scaledh=TJSCALED(jpegheight, sf[i]);
		if(scaledw<=width && scaledh<=height)
			break;
	}
	if(i>=NUMSF)
		_throw("tjDecompressToYUVPlanes(): Could not scale down to desired image dimensions");
	if(dinfo->num_components>3)
		_throw("tjDecompressToYUVPlanes(): JPEG image must have 3 or fewer components");

	width=scaledw;  height=scaledh;
	dinfo->scale_num=sf[i].num;
	dinfo->scale_denom=sf[i].denom;
	sfi=i;
	jpeg_calc_output_dimensions(dinfo);

	dctsize=DCTSIZE*sf[sfi].num/sf[sfi].denom;

	for(i=0; i<dinfo->num_components; i++)
	{
		jpeg_component_info *compptr=&dinfo->comp_info[i];
		int ih;
		iw[i]=compptr->width_in_blocks*dctsize;
		ih=compptr->height_in_blocks*dctsize;
		pw[i]=PAD(dinfo->output_width, dinfo->max_h_samp_factor)
			*compptr->h_samp_factor/dinfo->max_h_samp_factor;
		ph[i]=PAD(dinfo->output_height, dinfo->max_v_samp_factor)
			*compptr->v_samp_factor/dinfo->max_v_samp_factor;
		if(iw[i]!=pw[i] || ih!=ph[i]) usetmpbuf=1;
		th[i]=compptr->v_samp_factor*dctsize;
		tmpbufsize+=iw[i]*th[i];
		if((outbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph[i]))==NULL)
			_throw("tjDecompressToYUVPlanes(): Memory allocation failure");
		ptr=dstPlanes[i];
		for(row=0; row<ph[i]; row++)
		{
			outbuf[i][row]=ptr;
			ptr+=(strides && strides[i]!=0)? strides[i]:pw[i];
		}
	}
	if(usetmpbuf)
	{
		if((_tmpbuf=(JSAMPLE *)malloc(sizeof(JSAMPLE)*tmpbufsize))==NULL)
			_throw("tjDecompressToYUVPlanes(): Memory allocation failure");
		ptr=_tmpbuf;
		for(i=0; i<dinfo->num_components; i++)
		{
			if((tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*th[i]))==NULL)
				_throw("tjDecompressToYUVPlanes(): Memory allocation failure");
			for(row=0; row<th[i]; row++)
			{
				tmpbuf[i][row]=ptr;
				ptr+=iw[i];
			}
		}
	}

	if(flags&TJFLAG_FASTUPSAMPLE) dinfo->do_fancy_upsampling=FALSE;
	if(flags&TJFLAG_FASTDCT) dinfo->dct_method=JDCT_FASTEST;
	dinfo->raw_data_out=TRUE;

	jpeg_start_decompress(dinfo);
	for(row=0; row<(int)dinfo->output_height;
		row+=dinfo->max_v_samp_factor*dinfo->_min_DCT_scaled_size)
	{
		JSAMPARRAY yuvptr[MAX_COMPONENTS];
		int crow[MAX_COMPONENTS];
		for(i=0; i<dinfo->num_components; i++)
		{
			jpeg_component_info *compptr=&dinfo->comp_info[i];
			if(jpegSubsamp==TJ_420)
			{
				/* When 4:2:0 subsampling is used with IDCT scaling, libjpeg will try
				   to be clever and use the IDCT to perform upsampling on the U and V
				   planes.  For instance, if the output image is to be scaled by 1/2
				   relative to the JPEG image, then the scaling factor and upsampling
				   effectively cancel each other, so a normal 8x8 IDCT can be used.
				   However, this is not desirable when using the decompress-to-YUV
				   functionality in TurboJPEG, since we want to output the U and V
				   planes in their subsampled form.  Thus, we have to override some
				   internal libjpeg parameters to force it to use the "scaled" IDCT
				   functions on the U and V planes. */
				compptr->_DCT_scaled_size=dctsize;
				compptr->MCU_sample_width=tjMCUWidth[jpegSubsamp]*
					sf[sfi].num/sf[sfi].denom*
					compptr->v_samp_factor/dinfo->max_v_samp_factor;
				dinfo->idct->inverse_DCT[i] = dinfo->idct->inverse_DCT[0];
			}
			crow[i]=row*compptr->v_samp_factor/dinfo->max_v_samp_factor;
			if(usetmpbuf) yuvptr[i]=tmpbuf[i];
			else yuvptr[i]=&outbuf[i][crow[i]];
		}
		jpeg_read_raw_data(dinfo, yuvptr,
			dinfo->max_v_samp_factor*dinfo->_min_DCT_scaled_size);
		if(usetmpbuf)
		{
			int j;
			for(i=0; i<dinfo->num_components; i++)
			{
				for(j=0; j<min(th[i], ph[i]-crow[i]); j++)
				{
					memcpy(outbuf[i][crow[i]+j], tmpbuf[i][j], pw[i]);
				}
			}
		}
	}
	jpeg_finish_decompress(dinfo);

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]) free(tmpbuf[i]);
		if(outbuf[i]) free(outbuf[i]);
	}
	if(_tmpbuf) free(_tmpbuf);
	if(this->jerr.warning) retval=-1;
	return retval;
}

DLLEXPORT int DLLCALL tjDecompressToYUV2(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
	int width, int pad, int height, int flags)
{
	unsigned char *dstPlanes[3];
	int pw0, ph0, strides[3], retval=-1, jpegSubsamp=-1;
	int i, jpegwidth, jpegheight, scaledw, scaledh;

	getdinstance(handle);

	if(jpegBuf==NULL || jpegSize<=0 || dstBuf==NULL || width<0 || pad<1
		|| !isPow2(pad) || height<0)
		_throw("tjDecompressToYUV2(): Invalid argument");

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);
	jpegSubsamp=getSubsamp(dinfo);
	if(jpegSubsamp<0)
		_throw("tjDecompressToYUV2(): Could not determine subsampling type for JPEG image");

	jpegwidth=dinfo->image_width;  jpegheight=dinfo->image_height;
	if(width==0) width=jpegwidth;
	if(height==0) height=jpegheight;

	for(i=0; i<NUMSF; i++)
	{
		scaledw=TJSCALED(jpegwidth, sf[i]);
		scaledh=TJSCALED(jpegheight, sf[i]);
		if(scaledw<=width && scaledh<=height)
			break;
	}
	if(i>=NUMSF)
		_throw("tjDecompressToYUV2(): Could not scale down to desired image dimensions");

	pw0=tjPlaneWidth(0, width, jpegSubsamp);
	ph0=tjPlaneHeight(0, height, jpegSubsamp);
	dstPlanes[0]=dstBuf;
	strides[0]=PAD(pw0, pad);
	if(jpegSubsamp==TJSAMP_GRAY)
	{
		strides[1]=strides[2]=0;
		dstPlanes[1]=dstPlanes[2]=NULL;
	}
	else
	{
		int pw1=tjPlaneWidth(1, width, jpegSubsamp);
		int ph1=tjPlaneHeight(1, height, jpegSubsamp);
		strides[1]=strides[2]=PAD(pw1, pad);
		dstPlanes[1]=dstPlanes[0]+strides[0]*ph0;
		dstPlanes[2]=dstPlanes[1]+strides[1]*ph1;
	}

	this->headerRead=1;
	return tjDecompressToYUVPlanes(handle, jpegBuf, jpegSize, dstPlanes, width,
		strides, height, flags);

	bailout:
	return retval;

}

DLLEXPORT int DLLCALL tjDecompressToYUV(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
	int flags)
{
	return tjDecompressToYUV2(handle, jpegBuf, jpegSize, dstBuf, 0, 4, 0, flags);
}


/* Transformer */

DLLEXPORT tjhandle DLLCALL tjInitTransform(void)
{
	tjinstance *this=NULL;  tjhandle handle=NULL;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitTransform(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	handle=_tjInitCompress(this);
	if(!handle) return NULL;
	handle=_tjInitDecompress(this);
	return handle;
}


DLLEXPORT int DLLCALL tjTransform(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, int n, unsigned char **dstBufs,
	unsigned long *dstSizes, tjtransform *t, int flags)
{
	jpeg_transform_info *xinfo=NULL;
	jvirt_barray_ptr *srccoefs, *dstcoefs;
	int retval=0, i, jpegSubsamp;

	getinstance(handle);
	if((this->init&COMPRESS)==0 || (this->init&DECOMPRESS)==0)
		_throw("tjTransform(): Instance has not been initialized for transformation");

	if(jpegBuf==NULL || jpegSize<=0 || n<1 || dstBufs==NULL || dstSizes==NULL
		|| t==NULL || flags<0)
		_throw("tjTransform(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);

	if((xinfo=(jpeg_transform_info *)malloc(sizeof(jpeg_transform_info)*n))
		==NULL)
		_throw("tjTransform(): Memory allocation failure");
	MEMZERO(xinfo, sizeof(jpeg_transform_info)*n);

	for(i=0; i<n; i++)
	{
		xinfo[i].transform=xformtypes[t[i].op];
		xinfo[i].perfect=(t[i].options&TJXOPT_PERFECT)? 1:0;
		xinfo[i].trim=(t[i].options&TJXOPT_TRIM)? 1:0;
		xinfo[i].force_grayscale=(t[i].options&TJXOPT_GRAY)? 1:0;
		xinfo[i].crop=(t[i].options&TJXOPT_CROP)? 1:0;
		if(n!=1 && t[i].op==TJXOP_HFLIP) xinfo[i].slow_hflip=1;
		else xinfo[i].slow_hflip=0;

		if(xinfo[i].crop)
		{
			xinfo[i].crop_xoffset=t[i].r.x;  xinfo[i].crop_xoffset_set=JCROP_POS;
			xinfo[i].crop_yoffset=t[i].r.y;  xinfo[i].crop_yoffset_set=JCROP_POS;
			if(t[i].r.w!=0)
			{
				xinfo[i].crop_width=t[i].r.w;  xinfo[i].crop_width_set=JCROP_POS;
			}
			else xinfo[i].crop_width=JCROP_UNSET;
			if(t[i].r.h!=0)
			{
				xinfo[i].crop_height=t[i].r.h;  xinfo[i].crop_height_set=JCROP_POS;
			}
			else xinfo[i].crop_height=JCROP_UNSET;
		}
	}

	jcopy_markers_setup(dinfo, JCOPYOPT_ALL);
	jpeg_read_header(dinfo, TRUE);
	jpegSubsamp=getSubsamp(dinfo);
	if(jpegSubsamp<0)
		_throw("tjTransform(): Could not determine subsampling type for JPEG image");

	for(i=0; i<n; i++)
	{
		if(!jtransform_request_workspace(dinfo, &xinfo[i]))
			_throw("tjTransform(): Transform is not perfect");

		if(xinfo[i].crop)
		{
			if((t[i].r.x%xinfo[i].iMCU_sample_width)!=0
				|| (t[i].r.y%xinfo[i].iMCU_sample_height)!=0)
			{
				snprintf(errStr, JMSG_LENGTH_MAX,
					"To crop this JPEG image, x must be a multiple of %d\n"
					"and y must be a multiple of %d.\n",
					xinfo[i].iMCU_sample_width, xinfo[i].iMCU_sample_height);
				retval=-1;  goto bailout;
			}
		}
	}

	srccoefs=jpeg_read_coefficients(dinfo);

	for(i=0; i<n; i++)
	{
		int w, h, alloc=1;
		if(!xinfo[i].crop)
		{
			w=dinfo->image_width;  h=dinfo->image_height;
		}
		else
		{
			w=xinfo[i].crop_width;  h=xinfo[i].crop_height;
		}
		if(flags&TJFLAG_NOREALLOC)
		{
			alloc=0;  dstSizes[i]=tjBufSize(w, h, jpegSubsamp);
		}
		if(!(t[i].options&TJXOPT_NOOUTPUT))
			jpeg_mem_dest_tj(cinfo, &dstBufs[i], &dstSizes[i], alloc);
		jpeg_copy_critical_parameters(dinfo, cinfo);
		dstcoefs=jtransform_adjust_parameters(dinfo, cinfo, srccoefs,
			&xinfo[i]);
		if(!(t[i].options&TJXOPT_NOOUTPUT))
		{
			jpeg_write_coefficients(cinfo, dstcoefs);
			jcopy_markers_execute(dinfo, cinfo, JCOPYOPT_ALL);
		}
		else jinit_c_master_control(cinfo, TRUE);
		jtransform_execute_transformation(dinfo, cinfo, srccoefs,
			&xinfo[i]);
		if(t[i].customFilter)
		{
			int ci, y;  JDIMENSION by;
			for(ci=0; ci<cinfo->num_components; ci++)
			{
				jpeg_component_info *compptr=&cinfo->comp_info[ci];
				tjregion arrayRegion={0, 0, compptr->width_in_blocks*DCTSIZE,
					DCTSIZE};
				tjregion planeRegion={0, 0, compptr->width_in_blocks*DCTSIZE,
					compptr->height_in_blocks*DCTSIZE};
				for(by=0; by<compptr->height_in_blocks; by+=compptr->v_samp_factor)
				{
					JBLOCKARRAY barray=(dinfo->mem->access_virt_barray)
						((j_common_ptr)dinfo, dstcoefs[ci], by, compptr->v_samp_factor,
						TRUE);
					for(y=0; y<compptr->v_samp_factor; y++)
					{
						if(t[i].customFilter(barray[y][0], arrayRegion, planeRegion,
							ci, i, &t[i])==-1)
							_throw("tjTransform(): Error in custom filter");
						arrayRegion.y+=DCTSIZE;
					}
				}
			}
		}
		if(!(t[i].options&TJXOPT_NOOUTPUT)) jpeg_finish_compress(cinfo);
	}

	jpeg_finish_decompress(dinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	if(xinfo) free(xinfo);
	if(this->jerr.warning) retval=-1;
	return retval;
}
