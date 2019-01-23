/*
   Samba Unix/Linux SMB client library
   More client RAP (SMB Remote Procedure Calls) functions
   Copyright (C) 2001 Steve French (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2007 Jeremy Allison. jra@samba.org
   Copyright (C) Andrew Tridgell         1994-1998
   Copyright (C) Gerald (Jerry) Carter   2004
   Copyright (C) James Peach		 2007

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

#ifndef _LIBSMB_CLIRAP_H
#define _LIBSMB_CLIRAP_H

struct cli_state;

/* The following definitions come from libsmb/clirap.c  */

bool cli_api(struct cli_state *cli,
	     char *param, int prcnt, int mprcnt,
	     char *data, int drcnt, int mdrcnt,
	     char **rparam, unsigned int *rprcnt,
	     char **rdata, unsigned int *rdrcnt);
bool cli_NetWkstaUserLogon(struct cli_state *cli,char *user, char *workstation);
int cli_RNetShareEnum(struct cli_state *cli, void (*fn)(const char *, uint32, const char *, void *), void *state);
bool cli_NetServerEnum(struct cli_state *cli, char *workgroup, uint32 stype,
		       void (*fn)(const char *, uint32, const char *, void *),
		       void *state);
bool cli_oem_change_password(struct cli_state *cli, const char *user, const char *new_password,
                             const char *old_password);
struct tevent_req *cli_qpathinfo1_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli,
				       const char *fname);
NTSTATUS cli_qpathinfo1_recv(struct tevent_req *req,
			     time_t *change_time,
			     time_t *access_time,
			     time_t *write_time,
			     SMB_OFF_T *size,
			     uint16 *mode);
NTSTATUS cli_qpathinfo1(struct cli_state *cli,
			const char *fname,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time,
			SMB_OFF_T *size,
			uint16 *mode);
NTSTATUS cli_setpathinfo_basic(struct cli_state *cli, const char *fname,
			       time_t create_time,
			       time_t access_time,
			       time_t write_time,
			       time_t change_time,
			       uint16 mode);
struct tevent_req *cli_qpathinfo2_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli,
				       const char *fname);
NTSTATUS cli_qpathinfo2_recv(struct tevent_req *req,
			     struct timespec *create_time,
			     struct timespec *access_time,
			     struct timespec *write_time,
			     struct timespec *change_time,
			     SMB_OFF_T *size, uint16 *mode,
			     SMB_INO_T *ino);
NTSTATUS cli_qpathinfo2(struct cli_state *cli, const char *fname,
			struct timespec *create_time,
			struct timespec *access_time,
			struct timespec *write_time,
			struct timespec *change_time,
			SMB_OFF_T *size, uint16 *mode,
			SMB_INO_T *ino);
struct tevent_req *cli_qpathinfo_streams_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct cli_state *cli,
					      const char *fname);
NTSTATUS cli_qpathinfo_streams_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    unsigned int *pnum_streams,
				    struct stream_struct **pstreams);
NTSTATUS cli_qpathinfo_streams(struct cli_state *cli, const char *fname,
			       TALLOC_CTX *mem_ctx,
			       unsigned int *pnum_streams,
			       struct stream_struct **pstreams);
NTSTATUS cli_qfilename(struct cli_state *cli, uint16_t fnum, char *name,
		       size_t namelen);
NTSTATUS cli_qfileinfo_basic(struct cli_state *cli, uint16_t fnum,
			     uint16 *mode, SMB_OFF_T *size,
			     struct timespec *create_time,
			     struct timespec *access_time,
			     struct timespec *write_time,
			     struct timespec *change_time,
			     SMB_INO_T *ino);
struct tevent_req *cli_qpathinfo_basic_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct cli_state *cli,
					    const char *fname);
NTSTATUS cli_qpathinfo_basic_recv(struct tevent_req *req,
				  SMB_STRUCT_STAT *sbuf, uint32 *attributes);
NTSTATUS cli_qpathinfo_basic(struct cli_state *cli, const char *name,
			     SMB_STRUCT_STAT *sbuf, uint32 *attributes);
NTSTATUS cli_qpathinfo_alt_name(struct cli_state *cli, const char *fname, fstring alt_name);
struct tevent_req *cli_qpathinfo_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct cli_state *cli, const char *fname,
				      uint16_t level, uint32_t min_rdata,
				      uint32_t max_rdata);
NTSTATUS cli_qpathinfo_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    uint8_t **rdata, uint32_t *num_rdata);
NTSTATUS cli_qpathinfo(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		       const char *fname, uint16_t level, uint32_t min_rdata,
		       uint32_t max_rdata,
		       uint8_t **rdata, uint32_t *num_rdata);

struct tevent_req *cli_qfileinfo_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct cli_state *cli, uint16_t fnum,
				      uint16_t level, uint32_t min_rdata,
				      uint32_t max_rdata);
NTSTATUS cli_qfileinfo_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    uint8_t **rdata, uint32_t *num_rdata);
NTSTATUS cli_qfileinfo(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		       uint16_t fnum, uint16_t level, uint32_t min_rdata,
		       uint32_t max_rdata,
		       uint8_t **rdata, uint32_t *num_rdata);

struct tevent_req *cli_flush_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  uint16_t fnum);
NTSTATUS cli_flush_recv(struct tevent_req *req);
NTSTATUS cli_flush(TALLOC_CTX *mem_ctx, struct cli_state *cli, uint16_t fnum);

struct tevent_req *cli_shadow_copy_data_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli,
					     uint16_t fnum,
					     bool get_names);
NTSTATUS cli_shadow_copy_data_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				   char ***pnames, int *pnum_names);
NTSTATUS cli_shadow_copy_data(TALLOC_CTX *mem_ctx, struct cli_state *cli,
			      uint16_t fnum, bool get_names,
			      char ***pnames, int *pnum_names);

/* The following definitions come from libsmb/clirap2.c  */
struct rap_group_info_1;
struct rap_user_info_1;
struct rap_share_info_2;

int cli_NetGroupDelete(struct cli_state *cli, const char *group_name);
int cli_NetGroupAdd(struct cli_state *cli, struct rap_group_info_1 *grinfo);
int cli_RNetGroupEnum(struct cli_state *cli, void (*fn)(const char *, const char *, void *), void *state);
int cli_RNetGroupEnum0(struct cli_state *cli,
		       void (*fn)(const char *, void *),
		       void *state);
int cli_NetGroupDelUser(struct cli_state * cli, const char *group_name, const char *user_name);
int cli_NetGroupAddUser(struct cli_state * cli, const char *group_name, const char *user_name);
int cli_NetGroupGetUsers(struct cli_state * cli, const char *group_name, void (*fn)(const char *, void *), void *state );
int cli_NetUserGetGroups(struct cli_state * cli, const char *user_name, void (*fn)(const char *, void *), void *state );
int cli_NetUserDelete(struct cli_state *cli, const char * user_name );
int cli_NetUserAdd(struct cli_state *cli, struct rap_user_info_1 * userinfo );
int cli_RNetUserEnum(struct cli_state *cli, void (*fn)(const char *, const char *, const char *, const char *, void *), void *state);
int cli_RNetUserEnum0(struct cli_state *cli,
		      void (*fn)(const char *, void *),
		      void *state);
int cli_NetFileClose(struct cli_state *cli, uint32 file_id );
int cli_NetFileGetInfo(struct cli_state *cli, uint32 file_id, void (*fn)(const char *, const char *, uint16, uint16, uint32));
int cli_NetFileEnum(struct cli_state *cli, const char * user,
		    const char * base_path,
		    void (*fn)(const char *, const char *, uint16, uint16,
			       uint32));
int cli_NetShareAdd(struct cli_state *cli, struct rap_share_info_2 * sinfo );
int cli_NetShareDelete(struct cli_state *cli, const char * share_name );
bool cli_get_pdc_name(struct cli_state *cli, const char *workgroup, char **pdc_name);
bool cli_get_server_domain(struct cli_state *cli);
bool cli_get_server_type(struct cli_state *cli, uint32 *pstype);
bool cli_get_server_name(TALLOC_CTX *mem_ctx, struct cli_state *cli,
			 char **servername);
bool cli_ns_check_server_type(struct cli_state *cli, char *workgroup, uint32 stype);
bool cli_NetWkstaUserLogoff(struct cli_state *cli, const char *user, const char *workstation);
int cli_NetPrintQEnum(struct cli_state *cli,
		void (*qfn)(const char*,uint16,uint16,uint16,const char*,const char*,const char*,const char*,const char*,uint16,uint16),
		void (*jfn)(uint16,const char*,const char*,const char*,const char*,uint16,uint16,const char*,unsigned int,unsigned int,const char*));
int cli_NetPrintQGetInfo(struct cli_state *cli, const char *printer,
	void (*qfn)(const char*,uint16,uint16,uint16,const char*,const char*,const char*,const char*,const char*,uint16,uint16),
	void (*jfn)(uint16,const char*,const char*,const char*,const char*,uint16,uint16,const char*,unsigned int,unsigned int,const char*));
int cli_RNetServiceEnum(struct cli_state *cli, void (*fn)(const char *, const char *, void *), void *state);
int cli_NetSessionEnum(struct cli_state *cli, void (*fn)(char *, char *, uint16, uint16, uint16, unsigned int, unsigned int, unsigned int, char *));
int cli_NetSessionGetInfo(struct cli_state *cli, const char *workstation,
		void (*fn)(const char *, const char *, uint16, uint16, uint16, unsigned int, unsigned int, unsigned int, const char *));
int cli_NetSessionDel(struct cli_state *cli, const char *workstation);
int cli_NetConnectionEnum(struct cli_state *cli, const char *qualifier,
			void (*fn)(uint16_t conid, uint16_t contype,
				uint16_t numopens, uint16_t numusers,
				uint32_t contime, const char *username,
				const char *netname));

#endif /* _LIBSMB_CLIRAP_H */
