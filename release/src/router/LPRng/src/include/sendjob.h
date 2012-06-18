/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: sendjob.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _SENDJOB_1_
#define _SENDJOB_1_ 1

/* PROTOTYPES */
int Send_job( struct job *job, struct job *logjob,
	int connect_timeout_len, int connect_interval, int max_connect_interval,
	int transfer_timeout, char *final_filter );
int Send_normal( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter );
int Send_control( int *sock, struct job *job, struct job *logjob, int transfer_timeout,
	int block_fd );
int Send_data_files( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter );
int Send_block( int *sock, struct job *job, struct job *logjob, int transfer_timeout );

#endif
