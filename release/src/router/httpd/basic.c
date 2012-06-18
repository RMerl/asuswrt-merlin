/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Basic skin (shtml)
 *
 * Copyright 2003, ASUSTeK Inc.
 * All Rights Reserved.		
 *				     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of ASUSTeK Inc..			    
 *
 */

#include <stdio.h>
#include <httpd.h>

struct mime_handler mime_handlers[] = {
	{ "**.htm", "text/html", NULL, NULL, do_file, NULL },
	{ "**.html", "text/html", NULL, NULL, do_file, NULL },
	{ "**.gif", "image/gif", NULL, NULL, do_file, NULL },
	{ "**.jpg", "image/jpeg", NULL, NULL, do_file, NULL },
	{ "**.jpeg", "image/gif", NULL, NULL, do_file, NULL },
	{ "**.png", "image/png", NULL, NULL, do_file, NULL },
	{ "**.css", "text/css", NULL, NULL, do_file, NULL },
	{ "**.au", "audio/basic", NULL, NULL, do_file, NULL },
	{ "**.wav", "audio/wav", NULL, NULL, do_file, NULL },
	{ "**.avi", "video/x-msvideo", NULL, NULL, do_file, NULL },
	{ "**.mov", "video/quicktime", NULL, NULL, do_file, NULL },
	{ "**.mpeg", "video/mpeg", NULL, NULL, do_file, NULL },
	{ "**.vrml", "model/vrml", NULL, NULL, do_file, NULL },
	{ "**.midi", "audio/midi", NULL, NULL, do_file, NULL },
	{ "**.mp3", "audio/mpeg", NULL, NULL, do_file, NULL },
	{ "**.pac", "application/x-ns-proxy-autoconfig", NULL, NULL, do_file, NULL },
	{ NULL, NULL, NULL, NULL, NULL, NULL }
};

struct ej_handler ej_handlers[] = {
	{ NULL, NULL }
};
