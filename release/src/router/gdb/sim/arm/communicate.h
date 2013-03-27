/*  communicate.h -- ARMulator comms support defns:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

int MYread_char (int sock, unsigned char *c);
void MYwrite_char (int sock, unsigned char c);
int MYread_word (int sock, ARMword * here);
void MYwrite_word (int sock, ARMword i);
void MYwrite_string (int sock, char *s);
int MYread_FPword (int sock, char *putinhere);
void MYwrite_FPword (int sock, char *fromhere);
int passon (int source, int dest, int n);

int wait_for_osreply (ARMword * reply);	/* from kid.c */

#define OS_SendNothing 0x0
#define OS_SendChar 0x1
#define OS_SendWord 0x2
#define OS_SendString 0x3

/* The pipes between the two processes */
extern int mumkid[2];
extern int kidmum[2];
