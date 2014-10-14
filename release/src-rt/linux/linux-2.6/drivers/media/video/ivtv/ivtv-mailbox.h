/*
    mailbox functions
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>
    Copyright (C) 2005-2007  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef IVTV_MAILBOX_H
#define IVTV_MAILBOX_H

#define IVTV_MBOX_DMA_END         8
#define IVTV_MBOX_DMA             9

void ivtv_api_get_data(struct ivtv_mailbox_data *mbdata, int mb,
		       int argc, u32 data[]);
int ivtv_api(struct ivtv *itv, int cmd, int args, u32 data[]);
int ivtv_vapi_result(struct ivtv *itv, u32 data[CX2341X_MBOX_MAX_DATA], int cmd, int args, ...);
int ivtv_vapi(struct ivtv *itv, int cmd, int args, ...);
int ivtv_api_func(void *priv, u32 cmd, int in, int out, u32 data[CX2341X_MBOX_MAX_DATA]);
void ivtv_mailbox_cache_invalidate(struct ivtv *itv);

#endif
