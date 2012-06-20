/*
 *	wol - wake on lan client
 *
 *	$Id: wol.h,v 1.3 2002/01/10 07:47:49 wol Exp $
 *
 *	Copyright (C) 2000-2002 Thomas Krennwallner <krennwallner@aon.at>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *	USA.
 */


#ifndef _WOL_H
#define _WOL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */


#if defined(HAVE_LOCALE_H)
#include <locale.h>
#endif /* HAVE_LOCALE_H */


/* i18n support */
#if defined(ENABLE_NLS)
#include <libintl.h>
#define _(Text) gettext (Text)
#else
#define _(Text) Text
#endif /* ENABLE_NLS */


#define DEFAULT_PORT 40000
#define DEFAULT_IPADDR "255.255.255.255"


#endif /* _WOL_H */
