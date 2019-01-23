/*
   CIFSDD - dd for SMB.
   Declarations and administrivia.

   Copyright (C) James Peach 2005-2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

extern const char * const PROGNAME;

enum argtype
{
	ARG_NUMERIC,
	ARG_SIZE,
	ARG_PATHNAME,
	ARG_BOOL,
};

struct argdef
{
	const char *	arg_name;
	enum argtype	arg_type;
	const char *	arg_help;

	union
	{
		bool		bval;
		uint64_t	nval;
		const char *	pval;
	} arg_val;
};

int set_arg_argv(const char * argv);
void set_arg_val(const char * name, ...);

bool check_arg_bool(const char * name);
uint64_t check_arg_numeric(const char * name);
const char * check_arg_pathname(const char * name);

typedef bool (*dd_seek_func)(void * handle, uint64_t offset);
typedef bool (*dd_read_func)(void * handle, uint8_t * buf,
				uint64_t wanted, uint64_t * actual);
typedef bool (*dd_write_func)(void * handle, uint8_t * buf,
				uint64_t wanted, uint64_t * actual);

struct dd_stats_record
{
	struct
	{
		uint64_t	fblocks;	/* Full blocks. */
		uint64_t	pblocks;	/* Partial blocks. */
		uint64_t	bytes;		/* Total bytes read. */
	} in;
	struct
	{
		uint64_t	fblocks;	/* Full blocks. */
		uint64_t	pblocks;	/* Partial blocks. */
		uint64_t	bytes;		/* Total bytes written. */
	} out;
};

extern struct dd_stats_record dd_stats;

struct dd_iohandle
{
	dd_seek_func	io_seek;
	dd_read_func	io_read;
	dd_write_func	io_write;
	int		io_flags;
};

#define DD_END_OF_FILE		0x10000000

#define DD_DIRECT_IO		0x00000001
#define DD_SYNC_IO		0x00000002
#define DD_WRITE		0x00000004
#define DD_OPLOCK		0x00000008

struct smbcli_options;
struct smbcli_session_options;
struct tevent_context;

struct dd_iohandle * dd_open_path(struct resolve_context *resolve_ctx, 
				  struct tevent_context *ev,
				  const char * path,
				  const char **ports,
				uint64_t io_size, int options, 
				const char *socket_options,
				struct smbcli_options *smb_options,
				struct smbcli_session_options *smb_session_options,
				struct gensec_settings *gensec_settings);
bool dd_fill_block(struct dd_iohandle * h, uint8_t * buf,
		uint64_t * buf_size, uint64_t need_size, uint64_t block_size);
bool dd_flush_block(struct dd_iohandle * h, uint8_t * buf,
		uint64_t * buf_size, uint64_t block_size);

/* vim: set sw=8 sts=8 ts=8 tw=79 : */
