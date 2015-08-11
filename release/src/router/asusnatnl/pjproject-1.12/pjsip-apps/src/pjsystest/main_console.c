/* $Id: main_console.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "systest.h"
#include "gui.h"
#include <stdio.h>
#include <pjlib.h>

static pj_bool_t console_quit;

enum gui_key gui_msgbox(const char *title, const char *message, enum gui_flag flag)
{
    puts(title);
    puts(message);

    for (;;) {
	char input[10], *ret;

	if (flag == WITH_YESNO)
	    printf("%c:Yes  %c:No  ", KEY_YES, KEY_NO);
	else if (flag == WITH_OK)
	    printf("%c:OK  ", KEY_OK);
	else if (flag == WITH_OKCANCEL)
	    printf("%c:OK  %c:Cancel  ", KEY_OK, KEY_CANCEL);
	puts("");

	ret = fgets(input, sizeof(input), stdin);
	if (!ret)
	    return KEY_CANCEL;

	if (input[0]==KEY_NO || input[0]==KEY_YES || input[0]==KEY_CANCEL)
	    return (enum gui_key)input[0];
    }
}

pj_status_t gui_init(gui_menu *menu)
{
    PJ_UNUSED_ARG(menu);
    return PJ_SUCCESS;
}

static void print_menu(const char *indent, char *menu_id, gui_menu *menu)
{
    char child_indent[16];
    unsigned i;

    pj_ansi_snprintf(child_indent, sizeof(child_indent), "%s  ", indent);

    printf("%s%s: %s\n", indent, menu_id, menu->title);

    for (i=0; i<menu->submenu_cnt; ++i) {
	char child_id[10];

	pj_ansi_sprintf(child_id, "%s%u", menu_id, i);

	if (!menu->submenus[i])
	    puts("");
	else
	    print_menu(child_indent, child_id, menu->submenus[i]);
    }
}

pj_status_t gui_start(gui_menu *menu)
{
    while (!console_quit) {
	unsigned i;
	char input[10], *p;
	gui_menu *choice;

	puts("M E N U :");
	puts("---------");
	for (i=0; i<menu->submenu_cnt; ++i) {
	    char menu_id[4];
	    pj_ansi_sprintf(menu_id, "%u", i);
	    print_menu("", menu_id, menu->submenus[i]);
	}
	puts("");
	printf("Enter the menu number: ");

	if (!fgets(input, sizeof(input), stdin))
	    break;

	p = input;
	choice = menu;
	while (*p && *p!='\r' && *p!='\n') {
	    unsigned d = (*p - '0');
	    if (d < 0 || d >= choice->submenu_cnt) {
		puts("Invalid selection");
		choice = NULL;
		break;
	    }

	    choice = choice->submenus[d];
	    ++p;
	}

	if (choice && *p!='\r' && *p!='\n') {
	    puts("Invalid characters entered");
	    continue;
	}

	if (choice && choice->handler)
	    (*choice->handler)();
    }

    return PJ_SUCCESS;
}

void gui_destroy(void)
{
    console_quit = PJ_TRUE;
}

void gui_sleep(unsigned sec)
{
    pj_thread_sleep(sec * 1000);
}

int main()
{
    if (systest_init() != PJ_SUCCESS)
	return 1;

    systest_run();
    systest_deinit();

    return 0;
}

