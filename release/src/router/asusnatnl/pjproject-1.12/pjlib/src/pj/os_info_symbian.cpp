/* $Id: os_info_symbian.cpp 3437 2011-03-08 06:30:34Z nanang $ */
/* 
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
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
#if !defined(PJ_SYMBIAN) || PJ_SYMBIAN == 0
#   error This file is only for Symbian platform
#endif

#include <pj/ctype.h>
#include <pj/string.h>

#include <f32file.h>	/* link against efsrv.lib	*/
#include <hal.h>	/* link against hal.lib		*/
#include <utf.h>	/* link against charconv.lib	*/


PJ_BEGIN_DECL
unsigned pj_symbianos_get_model_info(char *buf, unsigned buf_size);
unsigned pj_symbianos_get_platform_info(char *buf, unsigned buf_size);
void pj_symbianos_get_sdk_info(pj_str_t *name, pj_uint32_t *ver);
PJ_END_DECL


/* Get Symbian phone model info, returning length of model info */
unsigned pj_symbianos_get_model_info(char *buf, unsigned buf_size)
{
    pj_str_t model_name;

    /* Get machine UID */
    TInt hal_val;
    HAL::Get(HAL::EMachineUid, hal_val);
    pj_ansi_snprintf(buf, buf_size, "0x%08X", hal_val);
    pj_strset2(&model_name, buf);

    /* Get model name */
    const pj_str_t st_copyright = {"(C)", 3};
    const pj_str_t st_nokia = {"Nokia", 5};
    char tmp_buf[64];
    pj_str_t tmp_str;

    _LIT(KModelFilename,"Z:\\resource\\versions\\model.txt");
    RFile file;
    RFs fs;
    TInt err;
    
    fs.Connect(1);
    err = file.Open(fs, KModelFilename, EFileRead);
    if (err == KErrNone) {
	TFileText text;
	text.Set(file);
	TBuf16<64> ModelName16;
	err = text.Read(ModelName16);
	if (err == KErrNone) {
	    TPtr8 ptr8((TUint8*)tmp_buf, sizeof(tmp_buf));
	    ptr8.Copy(ModelName16);
	    pj_strset(&tmp_str, tmp_buf, ptr8.Length());
	    pj_strtrim(&tmp_str);
	}
	file.Close();
    }
    fs.Close();
    if (err != KErrNone)
	goto on_return;
    
    /* The retrieved model name is usually in long format, e.g: 
     * "© Nokia N95 (01.01)", "(C) Nokia E52". As we need only
     * the short version, let's clean it up.
     */
    
    /* Remove preceding non-ASCII chars, e.g: "©" */
    char *p = tmp_str.ptr;
    while (!pj_isascii(*p)) { p++; }
    pj_strset(&tmp_str, p, tmp_str.slen - (p - tmp_str.ptr));
    
    /* Remove "(C)" */
    p = pj_stristr(&tmp_str, &st_copyright);
    if (p) {
	p += st_copyright.slen;
	pj_strset(&tmp_str, p, tmp_str.slen - (p - tmp_str.ptr));
    }

    /* Remove "Nokia" */
    p = pj_stristr(&tmp_str, &st_nokia);
    if (p) {
	p += st_nokia.slen;
	pj_strset(&tmp_str, p, tmp_str.slen - (p - tmp_str.ptr));
    }
    
    /* Remove language version, e.g: "(01.01)" */
    p = pj_strchr(&tmp_str, '(');
    if (p) {
	tmp_str.slen = p - tmp_str.ptr;
    }
    
    pj_strtrim(&tmp_str);
    
    if (tmp_str.slen == 0)
	goto on_return;
    
    if ((unsigned)tmp_str.slen > buf_size - model_name.slen - 3)
	tmp_str.slen = buf_size - model_name.slen - 3;
    
    pj_strcat2(&model_name, "(");
    pj_strcat(&model_name, &tmp_str);
    pj_strcat2(&model_name, ")");
    
    /* Zero terminate */
    buf[model_name.slen] = '\0';
    
on_return:
    return model_name.slen;
}


/* Get platform info, returned format will be "Series60vX.X" */
unsigned pj_symbianos_get_platform_info(char *buf, unsigned buf_size)
{
    /* OS info */
    _LIT(KS60ProductIDFile, "Series60v*.sis");
    _LIT(KROMInstallDir, "z:\\system\\install\\");

    RFs fs;
    TFindFile ff(fs);
    CDir* result;
    pj_str_t plat_info = {NULL, 0};
    TInt err;

    fs.Connect(1);
    err = ff.FindWildByDir(KS60ProductIDFile, KROMInstallDir, result);
    if (err == KErrNone) {
	err = result->Sort(ESortByName|EDescending);
	if (err == KErrNone) {
	    TPtr8 tmp_ptr8((TUint8*)buf, buf_size);
	    const pj_str_t tmp_ext = {".sis", 4};
	    char *p;
	    
	    tmp_ptr8.Copy((*result)[0].iName);
	    pj_strset(&plat_info, buf, (pj_size_t)tmp_ptr8.Length());
	    p = pj_stristr(&plat_info, &tmp_ext);
	    if (p)
		plat_info.slen -= (p - plat_info.ptr);
	}
	delete result;
    }
    fs.Close();
    buf[plat_info.slen] = '\0';
    
    return plat_info.slen;
}


/* Get SDK info */
void pj_symbianos_get_sdk_info(pj_str_t *name, pj_uint32_t *ver)
{
    const pj_str_t S60 = {"S60", 3};
    #if defined(__SERIES60_30__)
	*name = S60;
	*ver  = (3 << 24);
    #elif defined(__SERIES60_31__)
	*name = S60;
	*ver  = (3 << 24) | (1 << 16);
    #elif defined(__S60_32__)
	*name = S60;
	*ver  = (3 << 24) | (2 << 16);
    #elif defined(__S60_50__)
	*name = S60;
	*ver  = (5 << 24);
    #elif defined(__NOKIA_N97__)
	*name = pj_str("N97");
	*ver  = (1 << 24);
    #else
	*name = pj_str("Unknown");
	*ver  = 0;
    #endif
}

