/* $Id: gui.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __GUI_H__
#define __GUI_H__

PJ_BEGIN_DECL

#ifdef USE_GUI

#define printf showMsg
#define puts(str) showMsg("%s\n", str)
#define fgets getInput

void showMsg(const char *format, ...);
char * getInput(char *s, int n, FILE *stream);
pj_bool_t showNotification(pjsua_call_id call_id);

#endif

PJ_END_DECL


#endif	/* __GUI_H__ */
