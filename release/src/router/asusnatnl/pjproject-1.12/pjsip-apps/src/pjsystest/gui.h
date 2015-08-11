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

#include <pj/types.h>

PJ_BEGIN_DECL

typedef char gui_title[32];

typedef void (*gui_menu_handler) (void);

typedef struct gui_menu 
{
    gui_title		 title;
    gui_menu_handler	 handler;
    unsigned		 submenu_cnt;
    struct gui_menu	*submenus[16];
} gui_menu;

enum gui_flag 
{
    WITH_OK = 0,
    WITH_YESNO = 1,
    WITH_OKCANCEL = 2
};

enum gui_key
{
    KEY_CANCEL = '9',
    KEY_NO = '0',
    KEY_YES = '1',
    KEY_OK = '1',
};

/* Initialize GUI with the menus and stuff */
pj_status_t gui_init(gui_menu *menu);

/* Run GUI main loop */
pj_status_t gui_start(gui_menu *menu);

/* Signal GUI mainloop to stop */
void gui_destroy(void);

/* AUX: display messagebox */
enum gui_key gui_msgbox(const char *title, const char *message, enum gui_flag flag);

/* AUX: sleep */
void gui_sleep(unsigned sec);


PJ_END_DECL


#endif	/* __GUI_H__ */
