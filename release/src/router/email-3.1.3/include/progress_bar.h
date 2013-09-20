/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#ifndef PROGRESS_H
#define PROGRESS_H  1

struct prbar {
	short error;
	short bar_size;
	short percent;
	int progress;
	int truncated_file_size;
	int actual_file_size;
	int curr_size;
	char *subject;
	char *size_type;
	char *buf;
};

struct prbar *prbarInit(size_t bytes);
void prbarPrint(size_t bytes, struct prbar *bar);
void prbarDestroy(struct prbar *bar);

#endif /* PROGRESS_H */

