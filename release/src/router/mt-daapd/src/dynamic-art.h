/*
 * $Id: dynamic-art.h,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Dynamically merge .jpeg data into an mp3 tag
 *
 * Copyright (C) 2004 Hiren Joshi (hirenj@mooh.org)
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

#ifndef _DYNAMIC_ART_H_
#define _DYNAMIC_ART_H_

int da_get_image_fd(char *filename);
int da_attach_image(int img_fd, int out_fd, int mp3_fd, int offset);
off_t da_aac_attach_image(int img_fd, int out_fd, int aac_fd, int offset);
int copyblock(int fromfd, int tofd, size_t size);
int fcopyblock(FILE *fromfp, int tofd, size_t size);

#endif /* _DYNAMICART_H_ */
