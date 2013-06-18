/*

/usr/src/ext2ed/general_com.c

A part of the extended file system 2 disk editor.

---------------------
General user commands
---------------------

First written on: April 9 1995

Copyright (C) 1995 Gadi Oxman

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"
#include "../version.h"

void help (char *command_line)

{
	int i,max_line=0;
	char argument [80],*ptr;

	werase (show_pad);wmove (show_pad,0,0);

	ptr=parse_word (command_line,argument);

	if (*ptr!=0) {
		 ptr=parse_word (ptr,argument);
		 if (*argument!=0) {
			 detailed_help (argument);
			 return;
		}
	}

	if (current_type!=NULL) {

		wprintw (show_pad,"Type %s specific commands:\n",current_type->name);max_line++;

		if (current_type->type_commands.last_command==-1) {
			wprintw (show_pad,"\nnone\n");max_line+=2;
		}
		else
			for (i=0;i<=current_type->type_commands.last_command;i++) {
				if (i%5==0) {
					wprintw (show_pad,"\n");max_line++;
				}
				wprintw (show_pad,"%-13s",current_type->type_commands.names [i]);
				if (i%5!=4)
					wprintw (show_pad,";  ");
			}

		wprintw (show_pad,"\n\n");max_line+=2;
	}

	if (ext2_commands.last_command != -1) {
		wprintw (show_pad,"ext2 filesystem general commands: \n");max_line++;
		for (i=0;i<=ext2_commands.last_command;i++) {
			if (i%5==0) {
				wprintw (show_pad,"\n");max_line++;
			}
			wprintw (show_pad,"%-13s",ext2_commands.names [i]);
			if (i%5!=4)
				wprintw (show_pad,";  ");

		}
		wprintw (show_pad,"\n\n");max_line+=2;
	}

	wprintw (show_pad,"General commands: \n");

	for (i=0;i<=general_commands.last_command;i++) {
		if (i%5==0) {
			wprintw (show_pad,"\n");max_line++;
		}
		wprintw (show_pad,"%-13s",general_commands.names [i]);
		if (i%5!=4)
			wprintw (show_pad,";  ");
	}

	wprintw (show_pad,"\n\n");max_line+=2;

	wprintw (show_pad,"EXT2ED ver %s (%s)\n",E2FSPROGS_VERSION, E2FSPROGS_DATE);
	wprintw (show_pad,"Copyright (C) 1995 Gadi Oxman\n");
	wprintw (show_pad,"Reviewed 2001 Christian Bac\n");
	wprintw (show_pad,"Modified and enchanced by Theodore Ts'o, 2002\n");
	wprintw (show_pad,"EXT2ED is hereby placed under the terms of the GNU General Public License.\n\n");
	wprintw (show_pad,"EXT2ED was programmed as a student project in the software laboratory\n");
	wprintw (show_pad,"of the faculty of electrical engineering in the\n");
	wprintw (show_pad,"Technion - Israel Institute of Technology\n");
	wprintw (show_pad,"with the guide of Avner Lottem and Dr. Ilana David.\n");

	max_line+=10;

	show_pad_info.line=0;show_pad_info.max_line=max_line;

	werase (show_win);wmove (show_win,0,0);
	wprintw (show_win,"EXT2ED help");

	refresh_show_win ();
	refresh_show_pad ();
}

void detailed_help (char *text)

{
	int i;

	if (current_type != NULL)
		for (i=0;i<=current_type->type_commands.last_command;i++) {
			if (strcmp (current_type->type_commands.names [i],text)==0) {
				wprintw (show_pad,"%s - %s\n",text,current_type->type_commands.descriptions [i]);
				refresh_show_pad ();return;
			}
		}

	for (i=0;i<=ext2_commands.last_command;i++) {
		if (strcmp (ext2_commands.names [i],text)==0) {
				wprintw (show_pad,"%s - %s\n",text,ext2_commands.descriptions [i]);
				refresh_show_pad ();return;
		}
	}

	for (i=0;i<=general_commands.last_command;i++) {
		if (strcmp (general_commands.names [i],text)==0) {
				wprintw (show_pad,"%s - %s\n",text,general_commands.descriptions [i]);
				refresh_show_pad ();return;
		}
	}

	if (strcmp ("quit",text)==0) {
		wprintw (show_pad,"quit - Exists EXT2ED");
		refresh_show_pad ();return;
	}

	wprintw (show_pad,"Error - Command %s not aviable now\n",text);
	refresh_show_pad ();return;
}



void set_device (char *command_line)

{
	char *ptr,new_device [80];

	ptr=parse_word (command_line,new_device);
	if (*ptr==0) {
		wprintw (command_win,"Error - Device name not specified\n");
		refresh_command_win ();return;
	}
	parse_word (ptr,new_device);
	check_mounted (new_device);
	if (mounted && !AllowMountedRead) {
		wprintw (command_win,"Error - Filesystem is mounted, aborting\n");
		wprintw (command_win,"You may wish to use the AllowMountedRead on configuration option\n");
		refresh_command_win ();return;
	}

	if (mounted && AllowMountedRead) {
		wprintw (command_win,"Warning - Filesystem is mounted. Displayed data may be unreliable.\n");
		refresh_command_win ();
	}

	if (device_handle!=NULL)
		fclose (device_handle);

	if ( (device_handle=fopen (new_device,"rb"))==NULL) {
		wprintw (command_win,"Error - Can not open device %s\n",new_device);refresh_command_win ();
		return;
	}
	else {
		strcpy (device_name,new_device);
		write_access=0;				/* Write access disabled */
		current_type=NULL;			/* There is no type now */
		remember_lifo.entries_count=0;		/* Empty Object memory */
		free_user_commands (&ext2_commands);	/* Free filesystem specific objects */
		free_struct_descriptors ();
		if (!set_file_system_info ()) {		/* Error while getting info --> abort */
			free_user_commands (&ext2_commands);
			free_struct_descriptors ();
			fclose (device_handle);
			device_handle=NULL;		/* Notice that our device is still not set up */
			device_offset=-1;
			return;
		}
		if (*AlternateDescriptors)		/* Check if user defined objects exist */
			set_struct_descriptors (AlternateDescriptors);
		dispatch ("setoffset 0");
		dispatch ("help");			/* Show help screen */
		wprintw (command_win,"Device changed to %s",device_name);refresh_command_win ();
	}
}

void set_offset (char *command_line)

{
	long mult=1;
	long new_offset;
	char *ptr,new_offset_buffer [80];

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (command_line,new_offset_buffer);

	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (ptr,new_offset_buffer);

	if (strcmp (new_offset_buffer,"block")==0) {
		mult=file_system_info.block_size;
		ptr=parse_word (ptr,new_offset_buffer);
	}

	if (strcmp (new_offset_buffer,"type")==0) {
		if (current_type==NULL) {
			wprintw (command_win,"Error - No type set\n");refresh_command_win ();
			return;
		}

		mult=current_type->length;
		ptr=parse_word (ptr,new_offset_buffer);
	}

	if (*new_offset_buffer==0) {
		wprintw (command_win,"Error - No offset specified\n");refresh_command_win ();
		return;
	}

	if (new_offset_buffer [0]=='+') {
		if (device_offset==-1) {
			wprintw (command_win,"Error - Select a fixed offset first\n");refresh_command_win ();
			return;
		}
		new_offset=device_offset+atol (new_offset_buffer+1)*mult;
	}

	else if (new_offset_buffer [0]=='-') {
		if (device_offset==-1) {
			wprintw (command_win,"Error - Select a fixed offset first\n");refresh_command_win ();
			return;
		}
		new_offset=device_offset-atol (new_offset_buffer+1)*mult;
		if (new_offset<0) new_offset=0;
	}

	else
		new_offset=atol (new_offset_buffer)*mult;

	if ( (fseek (device_handle,new_offset,SEEK_SET))==-1) {
		wprintw (command_win,"Error - Failed to seek to offset %ld in device %s\n",new_offset,device_name);
		refresh_command_win ();
		return;
	};
	device_offset=new_offset;
	wprintw (command_win,"Device offset changed to %ld\n",device_offset);refresh_command_win ();
	load_type_data ();
	type_data.offset_in_block=0;
}

void set_int(short len, void *ptr, char *name, char *value)
{
	char	*char_ptr;
	short	*short_ptr;
	long	*long_ptr;
	long	v;
	char	*tmp;

	v = strtol(value, &tmp, 0);
	if (*tmp) {
		wprintw( command_win, "Bad value - %s\n", value);
		return;
	}
	switch (len) {
	case 1:
		char_ptr = (char *) ptr;
		*char_ptr = v;
		break;
	case 2:
		short_ptr = (short *) ptr;
		*short_ptr = v;
		break;
	case 4:
		long_ptr = (long *) ptr;
		*long_ptr = v;
		break;
	default:
		wprintw (command_win,
			 "set_int: unsupported length: %d\n", len);
		return;
	}
	wprintw (command_win, "Variable %s set to %s\n",
		 name, value);
}

void set_uint(short len, void *ptr, char *name, char *value)
{
	unsigned char	*char_ptr;
	unsigned short	*short_ptr;
	unsigned long	*long_ptr;
	unsigned long	v;
	char		*tmp;

	v = strtoul(value, &tmp, 0);
	if (*tmp) {
		wprintw( command_win, "Bad value - %s\n", value);
		return;
	}
	switch (len) {
	case 1:
		char_ptr = (unsigned char *) ptr;
		*char_ptr = v;
		break;
	case 2:
		short_ptr = (unsigned short *) ptr;
		*short_ptr = v;
		break;
	case 4:
		long_ptr = (unsigned long *) ptr;
		*long_ptr = v;
		break;
	default:
		wprintw (command_win,
			 "set_uint: unsupported length: %d\n", len);
		return;
	}
	wprintw (command_win, "Variable %s set to %s\n",
		 name, value);
}

void set_char(short len, void *ptr, char *name, char *value)
{
	if (strlen(value)+1 > len) {
		wprintw( command_win, "Value %s too big for field\n",
			name, len);
		return;
	}
	memset(ptr, 0, len);
	strcpy((char *) ptr, value);
	wprintw (command_win, "Variable %s set to %s\n",
		 name, value);
}


void set (char *command_line)

{
	unsigned short *int_ptr;
	unsigned char *char_ptr;
	unsigned long *long_ptr,offset=0;
	int i,len, found=0;
	char *ptr,buffer [80],variable [80],value [80];

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	if (current_type==NULL) {
		hex_set (command_line);
		return;
	}

	ptr=parse_word (command_line,buffer);
	if (ptr==NULL || *ptr==0) {
		wprintw (command_win,"Error - Missing arguments\n");refresh_command_win ();
		return;
	}
	parse_word (ptr,buffer);
	ptr=strchr (buffer,'=');
	if (ptr==NULL) {
		wprintw (command_win,"Error - Bad syntax\n");refresh_command_win ();return;
	}
	strncpy (variable,buffer,ptr-buffer);variable [ptr-buffer]=0;
	strcpy (value,++ptr);

	if (current_type==NULL) {
		wprintw (command_win,"Sorry, not yet supported\n");refresh_command_win ();return;
	}

	for (i=0;i<current_type->fields_num && !found;i++) {
		if (strcmp (current_type->field_names [i],variable)==0) {
			found=1;
			ptr=type_data.u.buffer+offset;
			len = current_type->field_lengths [i];
			switch (current_type->field_types [i]) {
			case FIELD_TYPE_INT:
				set_int(len, ptr, variable, value);
				break;
			case FIELD_TYPE_UINT:
				set_uint(len, ptr, variable, value);
				break;
			case FIELD_TYPE_CHAR:
				set_char(len, ptr, variable, value);
				break;
			default:
				wprintw (command_win,
					 "set: unhandled type %d\n",
					 current_type->field_types [i]);
				break;
			}
			refresh_command_win ();
		}
		offset+=current_type->field_lengths [i];
	}
	if (found)
		dispatch ("show");
	else {
		wprintw (command_win,"Error - Variable %s not found\n",variable);
		refresh_command_win ();
	}
}

void hex_set (char *command_line)

{
	unsigned char tmp;
	char *ptr,buffer [80],*ch_ptr;
	int mode=HEX;

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - Argument not specified\n");refresh_command_win ();return;
	}

	ptr=parse_word (ptr,buffer);

	if (strcasecmp (buffer,"text")==0) {
		mode=TEXT;
		strcpy (buffer,ptr);
	}

	else if (strcasecmp (buffer,"hex")==0) {
		mode=HEX;
		ptr=parse_word (ptr,buffer);
	}

	if (*buffer==0) {
		wprintw (command_win,"Error - Data not specified\n");refresh_command_win ();return;
	}

	if (mode==HEX) {
		do {
			tmp=(unsigned char) strtol (buffer,NULL,16);
			type_data.u.buffer [type_data.offset_in_block]=tmp;
			type_data.offset_in_block++;
			ptr=parse_word (ptr,buffer);
			if (type_data.offset_in_block==file_system_info.block_size) {
				if (*ptr) {
					wprintw (command_win,"Error - Ending offset outside block, only partial string changed\n");
					refresh_command_win ();
				}
				type_data.offset_in_block--;
			}
		} while (*buffer) ;
	}

	else {
		ch_ptr=buffer;
		while (*ch_ptr) {
			tmp=(unsigned char) *ch_ptr++;
			type_data.u.buffer [type_data.offset_in_block]=tmp;
			type_data.offset_in_block++;
			if (type_data.offset_in_block==file_system_info.block_size) {
				if (*ch_ptr) {
					wprintw (command_win,"Error - Ending offset outside block, only partial string changed\n");
					refresh_command_win ();
				}
				type_data.offset_in_block--;
			}
		}
	}

	strcpy (buffer,"show");dispatch (buffer);
}



void set_type (char *command_line)

{
	struct struct_descriptor *descriptor_ptr;
	char *ptr,buffer [80],tmp_buffer [80];
	short found=0;

	if (!load_type_data ())
		return;

	ptr=parse_word (command_line,buffer);
	parse_word (ptr,buffer);

	if (strcmp (buffer,"none")==0 || strcmp (buffer,"hex")==0) {
		wprintw (command_win,"Data will be shown as hex dump\n");refresh_command_win ();
		current_type=NULL;
		sprintf (tmp_buffer,"show");dispatch (tmp_buffer);
		return;
	}

	descriptor_ptr=first_type;
	while (descriptor_ptr!=NULL && !found) {
		if (strcmp (descriptor_ptr->name,buffer)==0)
			found=1;
		else
			descriptor_ptr=descriptor_ptr->next;
	}
	if (found) {
		wprintw (command_win,"Structure type set to %s\n",buffer);refresh_command_win ();
		current_type=descriptor_ptr;
		sprintf (tmp_buffer,"show");dispatch (tmp_buffer);
	}
	else {
		wprintw (command_win,"Error - %s is not a valid type\n",buffer);refresh_command_win ();
	}
}

void show_int(short len, void *ptr)
{
	long	temp;
	char	*format;

	switch (len) {
	case 1:
		temp = *((char *) ptr);
		format = "%3d (0x%02x)\n";
		break;
	case 2:
		temp = *((short *) ptr);
		format = "%d (0x%x)\n";
		break;
	case 4:
		temp = *((long *) ptr);
		format = "%d\n";
		break;
	default:
		wprintw (show_pad, "unimplemented\n");
		return;
	}
	wprintw(show_pad, format, temp, temp);
}

void show_uint(short len, void *ptr)
{
	unsigned long	temp;
	char		*format;

	switch (len) {
	case 1:
		temp = *((unsigned char *) ptr);
		temp = temp & 0xFF;
		format = "%3u (0x%02x)\n";
		break;
	case 2:
		temp = *((unsigned short *) ptr);
		temp = temp & 0xFFFF;
		format = "%u (0x%x)\n";
		break;
	case 4:
		temp = (unsigned long) *((unsigned long *) ptr);
		format = "%u\n";
		break;
	default:
		wprintw (show_pad, "unimplemented\n");
		return;
	}
	wprintw(show_pad, format, temp, temp);
}

void show_char(short len, void *ptr)
{
	unsigned char	*cp = (unsigned char *) ptr;
	unsigned char	ch;
	int		i,j;

	wprintw(show_pad, "\"");

	for (i=0; i < len; i++) {
		ch = *cp++;
		if (ch == 0) {
			for (j=i+1; j < len; j++)
				if (cp[j-i])
					break;
			if (j == len)
				break;
		}
		if (ch > 128) {
			wprintw(show_pad, "M-");
			ch -= 128;
		}
		if ((ch < 32) || (ch == 0x7f)) {
			wprintw(show_pad, "^");
			ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
		}
		wprintw(show_pad, "%c", ch);
	}

	wprintw(show_pad, "\"\n");
}



void show (char *command_line)

{
	unsigned int i,l,len,temp_int;
	unsigned long offset=0,temp_long;
	unsigned char temp_char,*ch_ptr;
	void *ptr;

	if (device_handle==NULL)
		return;

	show_pad_info.line=0;

	if (current_type==NULL) {
		wmove (show_pad,0,0);
		ch_ptr=type_data.u.buffer;
		for (l=0;l<file_system_info.block_size/16;l++) {
			wprintw (show_pad,"%08ld :  ",offset);
			for (i=0;i<16;i++) {
				if (type_data.offset_in_block==offset+i)
					wattrset (show_pad,A_REVERSE);

				if (ch_ptr [i]>=' ' && ch_ptr [i]<='z')
					wprintw (show_pad,"%c",ch_ptr [i]);
				else
					wprintw (show_pad,".");
				if (type_data.offset_in_block==offset+i)
					wattrset (show_pad,A_NORMAL);
			}
			wprintw (show_pad,"   ");
			for (i=0;i<16;i++) {
				if (type_data.offset_in_block==offset+i)
					wattrset (show_pad,A_REVERSE);

				wprintw (show_pad,"%02x",ch_ptr [i]);

				if (type_data.offset_in_block==offset+i) {
					wattrset (show_pad,A_NORMAL);
					show_pad_info.line=l-l % show_pad_info.display_lines;
				}

				wprintw (show_pad," ");
			}
			wprintw (show_pad,"\n");
			offset+=16;
			ch_ptr+=16;
		}
		show_pad_info.max_line=l-1;show_pad_info.max_col=COLS-1;
		refresh_show_pad ();show_info ();
	}
	else {
		wmove (show_pad,0,0);l=0;
		for (i=0;i<current_type->fields_num;i++) {
			wprintw (show_pad,"%-20s = ",current_type->field_names [i]);
			ptr=type_data.u.buffer+offset;
			len = current_type->field_lengths[i];
			switch (current_type->field_types[i]) {
			case FIELD_TYPE_INT:
				show_int(len, ptr);
				break;
			case FIELD_TYPE_UINT:
				show_uint(len, ptr);
				break;
			case FIELD_TYPE_CHAR:
				show_char(len, ptr);
				break;
			default:
				wprintw (show_pad, "unimplemented\n");
				break;
			}
			offset+=len;
			l++;
		}
		current_type->length=offset;
		show_pad_info.max_line=l-1;
		refresh_show_pad ();show_info ();
	}
}

void next (char *command_line)

{
	long offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	if (current_type!=NULL) {
		sprintf (buffer,"setoffset type +%ld",offset);
		dispatch (buffer);
		return;
	}

	if (type_data.offset_in_block+offset < file_system_info.block_size) {
		type_data.offset_in_block+=offset;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Offset out of block\n");refresh_command_win ();
	}
}

void prev (char *command_line)

{
	long offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	if (current_type!=NULL) {
		sprintf (buffer,"setoffset type -%ld",offset);
		dispatch (buffer);
		return;
	}

	if (type_data.offset_in_block-offset >= 0) {
		type_data.offset_in_block-=offset;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Offset out of block\n");refresh_command_win ();
	}
}

void pgdn (char *commnad_line)

{
	show_pad_info.line+=show_pad_info.display_lines;
	refresh_show_pad ();refresh_show_win ();
}

void pgup (char *command_line)

{
	show_pad_info.line-=show_pad_info.display_lines;
	refresh_show_pad ();refresh_show_win ();
}

void redraw (char *command_line)

{
	redraw_all ();
	dispatch ("show");
}

void remember (char *command_line)

{
	long entry_num;
	char *ptr,buffer [80];

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (command_line,buffer);

	if (*ptr==0) {
		wprintw (command_win,"Error - Argument not specified\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (ptr,buffer);

	entry_num=remember_lifo.entries_count++;
	if (entry_num>REMEMBER_COUNT-1) {
		entry_num=0;
		remember_lifo.entries_count--;
	}

	remember_lifo.offset [entry_num]=device_offset;
	remember_lifo.type [entry_num]=current_type;
	strcpy (remember_lifo.name [entry_num],buffer);

	if (current_type!=NULL)
		wprintw (command_win,"Object %s in Offset %ld remembered as %s\n",current_type->name,device_offset,buffer);
	else
		wprintw (command_win,"Offset %ld remembered as %s\n",device_offset,buffer);

	refresh_command_win ();
}

void recall (char *command_line)

{
	char *ptr,buffer [80];
	long entry_num;

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (command_line,buffer);

	if (*ptr==0) {
		wprintw (command_win,"Error - Argument not specified\n");refresh_command_win ();
		return;
	}

	ptr=parse_word (ptr,buffer);


	for (entry_num=remember_lifo.entries_count-1;entry_num>=0;entry_num--) {
		if (strcmp (remember_lifo.name [entry_num],buffer)==0)
			break;
	}

	if (entry_num==-1) {
		wprintw (command_win,"Error - Can not recall %s\n",buffer);refresh_command_win ();
		return;
	}

	sprintf (buffer,"setoffset %ld",remember_lifo.offset [entry_num]);dispatch (buffer);
	if (remember_lifo.type [entry_num] != NULL) {
		sprintf (buffer,"settype %s",remember_lifo.type [entry_num]->name);dispatch (buffer);
	}

	else {
		sprintf (buffer,"settype none");dispatch (buffer);
	}

	wprintw (command_win,"Object %s in Offset %ld recalled\n",current_type->name,device_offset);
	refresh_command_win ();
}

void enable_write (char *command_line)

{
	FILE *fp;

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	if (!AllowChanges) {
		wprintw (command_win,"Sorry, write access is not allowed\n");
    		return;
    	}

    	if (mounted) {
    		wprintw (command_win,"Error - Filesystem is mounted\n");
		return;
    	}

	if ( (fp=fopen (device_name,"r+b"))==NULL) {
		wprintw (command_win,"Error - Can not open device %s for reading and writing\n",device_name);refresh_command_win ();
		return;
	}
	fclose (device_handle);
	device_handle=fp;write_access=1;
	wprintw (command_win,"Write access enabled - Be careful\n");refresh_command_win ();
}

void disable_write (char *command_line)

{
	FILE *fp;

	if (device_handle==NULL) {
		wprintw (command_win,"Error - No device opened\n");refresh_command_win ();
		return;
	}

	if ( (fp=fopen (device_name,"rb"))==NULL) {
		wprintw (command_win,"Error - Can not open device %s\n",device_name);refresh_command_win ();
		return;
	}

	fclose (device_handle);
	device_handle=fp;write_access=0;
	wprintw (command_win,"Write access disabled\n");refresh_command_win ();
}

void write_data (char *command_line)

{
	write_type_data ();
}
