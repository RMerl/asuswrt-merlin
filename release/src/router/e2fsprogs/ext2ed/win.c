/*

/usr/src/ext2ed/win.c

A part of the extended file system 2 disk editor.

--------------------------------------------------------
Window management - Interfacing with the ncurses library
--------------------------------------------------------

First written on: April 17 1995
Modified on : April 05 2001 Christian.Bac@int-evry.fr
it looks like readline does not like that initscr decides to set the tty to
noecho.

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "ext2ed.h"
#include "../version.h"

struct struct_pad_info show_pad_info;
WINDOW *title_win,*show_win,*command_win,*mt_win1,*mt_win2,*show_pad;

/* to remember configuration after initscr
 * and modify it
 */
struct termios termioInit, termioCurrent;

void draw_title_win (void)
{
	char title_string [128];

	werase(title_win);
	box (title_win,0,0);
	sprintf (title_string,"EXT2ED - Extended-2 File System editor ver %s (%s)", E2FSPROGS_VERSION, E2FSPROGS_DATE);
	wmove (title_win,TITLE_WIN_LINES/2,(COLS-strlen (title_string))/2);
	wprintw (title_win,title_string);
	wrefresh(title_win);
}

void setup_show_win(void)
{
	wbkgdset (show_win,A_REVERSE);werase (show_win);
	show_pad_info.line=0;
	show_pad_info.col=0;
	show_pad_info.display_lines=LINES-TITLE_WIN_LINES-SHOW_WIN_LINES-COMMAND_WIN_LINES-2;
	show_pad_info.display_cols=COLS;
	show_pad_info.max_line=show_pad_info.display_lines-1;show_pad_info.max_col=show_pad_info.display_cols-1;
	show_pad_info.disable_output=0;
}

void init_windows (void)
{
	initscr ();
	tcgetattr(0,&termioInit); /* save initial config */
	termioCurrent = termioInit;
	termioCurrent.c_lflag |= ECHO; /* set echo on */
	tcsetattr(0,TCSANOW,&termioCurrent);

	if (LINES<TITLE_WIN_LINES+SHOW_WIN_LINES+COMMAND_WIN_LINES+3) {
		printf ("Sorry, your terminal screen is too small\n");
		printf ("Error - Can not initialize windows\n");
		exit (1);
	}

	title_win=newwin (TITLE_WIN_LINES,COLS,0,0);
	show_win=newwin (SHOW_WIN_LINES,COLS,TITLE_WIN_LINES,0);
	show_pad=newpad (SHOW_PAD_LINES,SHOW_PAD_COLS);
	mt_win1=newwin (1,COLS,TITLE_WIN_LINES+SHOW_WIN_LINES,0);
	mt_win2=newwin (1,COLS,LINES-COMMAND_WIN_LINES-1,0);
	command_win=newwin (COMMAND_WIN_LINES,COLS,LINES-COMMAND_WIN_LINES,0);

	if (title_win==NULL || show_win==NULL || show_pad==NULL || command_win==NULL) {
		printf ("Error - Not enough memory - Can not initialize windows\n");exit (1);
	}

	draw_title_win();

	setup_show_win();

	scrollok (command_win,TRUE);

	refresh_title_win ();
	refresh_show_win ();
	refresh_show_pad();
	refresh_command_win ();
	wrefresh(mt_win1);
	wrefresh(mt_win2);
}

void refresh_title_win (void)
{
	wrefresh (title_win);
}

void refresh_show_win (void)
{
	int current_page,total_pages;

	current_page=show_pad_info.line/show_pad_info.display_lines+1;
	if (show_pad_info.line%show_pad_info.display_lines)
		current_page++;
	total_pages=show_pad_info.max_line/show_pad_info.display_lines+1;

	wmove (show_win,2,COLS-18);
	wprintw (show_win,"Page %d of %d\n",current_page,total_pages);

	wmove (show_win,2,COLS-18);
	wrefresh (show_win);
}


void refresh_show_pad (void)

{
	int left,top,right,bottom,i;

	if (show_pad_info.disable_output)
		return;

	if (show_pad_info.max_line < show_pad_info.display_lines-1) {
		for (i=show_pad_info.max_line+1;i<show_pad_info.display_lines;i++) {
			wmove (show_pad,i,0);wprintw (show_pad,"\n");
		}
	}
	left=0;right=show_pad_info.display_cols-1;
	top=TITLE_WIN_LINES+SHOW_WIN_LINES+1;bottom=top+show_pad_info.display_lines-1;

	if (show_pad_info.line > show_pad_info.max_line-show_pad_info.display_lines+1)
		show_pad_info.line=show_pad_info.max_line-show_pad_info.display_lines+1;

	if (show_pad_info.line < 0)
		show_pad_info.line=0;

#ifdef OLD_NCURSES
	prefresh (show_pad,show_pad_info.line,show_pad_info.col,top,left,show_pad_info.display_lines-1,show_pad_info.display_cols-1);
#else
	prefresh (show_pad,show_pad_info.line,show_pad_info.col,top,left,top+show_pad_info.display_lines-1,left+show_pad_info.display_cols-1);
#endif
}

void refresh_command_win (void)
{
	wrefresh (command_win);
}

void close_windows (void)
{
//	echo ();
	tcsetattr(0,TCSANOW,&termioInit);

	delwin (title_win);
	delwin (command_win);
	delwin (show_win);
	delwin (show_pad);

	endwin ();
}

void show_info (void)
{
	int block_num,block_offset;

	block_num=device_offset/file_system_info.block_size;
	block_offset=device_offset%file_system_info.block_size;

	wmove (show_win,0,0);
	wprintw (show_win,"Offset %-3ld in block %ld. ",block_offset,block_num);
	if (current_type != NULL)
		wprintw (show_win,"Type: %s\n",current_type->name);
	else
		wprintw (show_win,"Type: %s\n","none");

	refresh_show_win ();
}


void redraw_all (void)
{
	int min_lines = TITLE_WIN_LINES+SHOW_WIN_LINES+COMMAND_WIN_LINES+3;
	struct winsize ws;
	int	save_col, save_lines;

	/* get the size of the terminal connected to stdout */
	ioctl(1, TIOCGWINSZ, &ws);
	/*
	 * Do it again because GDB doesn't stop before the first ioctl
	 * call, we want an up-to-date size when we're
	 * single-stepping.
	 */
	if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_row < min_lines)
			ws.ws_row = min_lines;
		if ((ws.ws_row != LINES) || (ws.ws_col != COLS)) {
			wmove (show_win,2,COLS-18);
			wclrtoeol(show_win);
			wrefresh(show_win);
			resizeterm(ws.ws_row, ws.ws_col);
			wresize(title_win, TITLE_WIN_LINES,COLS);
			wresize(show_win, SHOW_WIN_LINES,COLS);
			wresize(command_win, COMMAND_WIN_LINES,COLS);
			wresize(mt_win1, 1,COLS);
			wresize(mt_win2, 1,COLS);
			mvwin(mt_win2, LINES-COMMAND_WIN_LINES-1,0);
			mvwin(command_win, LINES-COMMAND_WIN_LINES,0);
			draw_title_win();
			show_pad_info.display_lines=LINES-TITLE_WIN_LINES-SHOW_WIN_LINES-COMMAND_WIN_LINES-2;
			show_pad_info.display_cols=COLS;
		}
	}
	clearok(title_win, 1);
	clearok(show_win, 1);
	clearok(command_win, 1);
	clearok(mt_win1, 1);
	clearok(mt_win2, 1);
	wrefresh(mt_win1);
	wrefresh(mt_win2);
	refresh_show_pad();
	refresh_show_win();
	refresh_title_win ();
	refresh_command_win ();
}
