/*
 * Line6 Linux USB driver - 0.8.0
 *
 * Copyright (C) 2004-2009 Markus Grabner (grabner@icg.tugraz.at)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

#ifndef MIDIBUF_H
#define MIDIBUF_H


struct MidiBuffer {
	unsigned char *buf;
	int size;
	int split;
	int pos_read, pos_write;
	int full;
	int command_prev;
};


extern int midibuf_bytes_used(struct MidiBuffer *mb);
extern int midibuf_bytes_free(struct MidiBuffer *mb);
extern void midibuf_destroy(struct MidiBuffer *mb);
extern int midibuf_ignore(struct MidiBuffer *mb, int length);
extern int midibuf_init(struct MidiBuffer *mb, int size, int split);
extern int midibuf_read(struct MidiBuffer *mb, unsigned char *data, int length);
extern void midibuf_reset(struct MidiBuffer *mb);
extern int midibuf_skip_message(struct MidiBuffer *mb, unsigned short mask);
extern void midibuf_status(struct MidiBuffer *mb);
extern int midibuf_write(struct MidiBuffer *mb, unsigned char *data,
			 int length);


#endif
