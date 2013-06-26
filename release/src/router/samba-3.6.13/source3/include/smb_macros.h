/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) John H Terpstra 1996-1999
   Copyright (C) Luke Kenneth Casson Leighton 1996-1999
   Copyright (C) Paul Ashton 1998 - 1999
   
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

#ifndef _SMB_MACROS_H
#define _SMB_MACROS_H

/* Misc bit macros */
#define BOOLSTR(b) ((b) ? "Yes" : "No")
#define BITSETW(ptr,bit) ((SVAL(ptr,0) & (1<<(bit)))!=0)

/* for readability... */
#define IS_DOS_READONLY(test_mode) (((test_mode) & FILE_ATTRIBUTE_READONLY) != 0)
#define IS_DOS_DIR(test_mode)      (((test_mode) & FILE_ATTRIBUTE_DIRECTORY) != 0)
#define IS_DOS_ARCHIVE(test_mode)  (((test_mode) & FILE_ATTRIBUTE_ARCHIVE) != 0)
#define IS_DOS_SYSTEM(test_mode)   (((test_mode) & FILE_ATTRIBUTE_SYSTEM) != 0)
#define IS_DOS_HIDDEN(test_mode)   (((test_mode) & FILE_ATTRIBUTE_HIDDEN) != 0)

#define SMB_WARN(condition, message) \
    ((condition) ? (void)0 : \
     DEBUG(0, ("WARNING: %s: %s\n", #condition, message)))

#define SMB_ASSERT_ARRAY(a,n) SMB_ASSERT((sizeof(a)/sizeof((a)[0])) >= (n))

/* these are useful macros for checking validity of handles */
#define IS_IPC(conn)       ((conn) && (conn)->ipc)
#define IS_PRINT(conn)       ((conn) && (conn)->printer)
/* you must add the following extern declaration to files using this macro
 * (do not add it to the macro as that causes nested extern declaration warnings)
 * extern struct current_user current_user;
 */
#define FSP_BELONGS_CONN(fsp,conn) do {\
			if (!((fsp) && (conn) && ((conn)==(fsp)->conn) && (current_user.vuid==(fsp)->vuid))) \
				return ERROR_NT(NT_STATUS_INVALID_HANDLE); \
			} while(0)

#define CHECK_READ(fsp,req) (((fsp)->fh->fd != -1) && ((fsp)->can_read || \
			((req->flags2 & FLAGS2_READ_PERMIT_EXECUTE) && \
			 (fsp->access_mask & FILE_EXECUTE))))

#define CHECK_WRITE(fsp) ((fsp)->can_write && ((fsp)->fh->fd != -1))

#define ERROR_WAS_LOCK_DENIED(status) (NT_STATUS_EQUAL((status), NT_STATUS_LOCK_NOT_GRANTED) || \
				NT_STATUS_EQUAL((status), NT_STATUS_FILE_LOCK_CONFLICT) )

/* the service number for the [globals] defaults */ 
#define GLOBAL_SECTION_SNUM	(-1)
/* translates a connection number into a service number */
#define SNUM(conn)         	((conn)?(conn)->params->service:GLOBAL_SECTION_SNUM)


/* access various service details */
#define CAN_WRITE(conn)    (!conn->read_only)
#define VALID_SNUM(snum)   (lp_snum_ok(snum))
#define GUEST_OK(snum)     (VALID_SNUM(snum) && lp_guest_ok(snum))
#define GUEST_ONLY(snum)   (VALID_SNUM(snum) && lp_guest_only(snum))
#define CAN_SETDIR(snum)   (!lp_no_set_dir(snum))
#define CAN_PRINT(conn)    ((conn) && lp_print_ok(SNUM(conn)))
#define MAP_HIDDEN(conn)   ((conn) && lp_map_hidden(SNUM(conn)))
#define MAP_SYSTEM(conn)   ((conn) && lp_map_system(SNUM(conn)))
#define MAP_ARCHIVE(conn)   ((conn) && lp_map_archive(SNUM(conn)))
#define IS_HIDDEN_PATH(conn,path)  ((conn) && is_in_path((path),(conn)->hide_list,(conn)->case_sensitive))
#define IS_VETO_PATH(conn,path)  ((conn) && is_in_path((path),(conn)->veto_list,(conn)->case_sensitive))
#define IS_VETO_OPLOCK_PATH(conn,path)  ((conn) && is_in_path((path),(conn)->veto_oplock_list,(conn)->case_sensitive))

/* 
 * Used by the stat cache code to check if a returned
 * stat structure is valid.
 */

#define VALID_STAT(st) ((st).st_ex_nlink != 0)
#define VALID_STAT_OF_DIR(st) (VALID_STAT(st) && S_ISDIR((st).st_ex_mode))
#define SET_STAT_INVALID(st) ((st).st_ex_nlink = 0)

/* Macros to get at offsets within smb_lkrng and smb_unlkrng
   structures. We cannot define these as actual structures
   due to possible differences in structure packing
   on different machines/compilers. */

#define SMB_LPID_OFFSET(indx) (10 * (indx))
#define SMB_LKOFF_OFFSET(indx) ( 2 + (10 * (indx)))
#define SMB_LKLEN_OFFSET(indx) ( 6 + (10 * (indx)))
#define SMB_LARGE_LPID_OFFSET(indx) (20 * (indx))
#define SMB_LARGE_LKOFF_OFFSET_HIGH(indx) (4 + (20 * (indx)))
#define SMB_LARGE_LKOFF_OFFSET_LOW(indx) (8 + (20 * (indx)))
#define SMB_LARGE_LKLEN_OFFSET_HIGH(indx) (12 + (20 * (indx)))
#define SMB_LARGE_LKLEN_OFFSET_LOW(indx) (16 + (20 * (indx)))

#define ERROR_DOS(class,code) error_packet(outbuf,class,code,NT_STATUS_OK,__LINE__,__FILE__)
#define ERROR_NT(status) error_packet(outbuf,0,0,status,__LINE__,__FILE__)
#define ERROR_FORCE_NT(status) error_packet(outbuf,-1,-1,status,__LINE__,__FILE__)
#define ERROR_BOTH(status,class,code) error_packet(outbuf,class,code,status,__LINE__,__FILE__)

#define reply_nterror(req,status) reply_nt_error(req,status,__LINE__,__FILE__)
#define reply_force_doserror(req,eclass,ecode) reply_force_dos_error(req,eclass,ecode,__LINE__,__FILE__)
#define reply_botherror(req,status,eclass,ecode) reply_both_error(req,eclass,ecode,status,__LINE__,__FILE__)

#if 0
/* defined in IDL */
/* these are the datagram types */
#define DGRAM_DIRECT_UNIQUE 0x10
#endif

#define SMB_ROUNDUP(x,r) ( ((x)%(r)) ? ( (((x)+(r))/(r))*(r) ) : (x))

/* Extra macros added by Ying Chen at IBM - speed increase by inlining. */
#define smb_buf(buf) (((char *)(buf)) + smb_size + CVAL(buf,smb_wct)*2)
#define smb_buflen(buf) (SVAL(buf,smb_vwv0 + (int)CVAL(buf, smb_wct)*2))

/* the remaining number of bytes in smb buffer 'buf' from pointer 'p'. */
#define smb_bufrem(buf, p) (smb_buflen(buf)-PTR_DIFF(p, smb_buf(buf)))
#define smbreq_bufrem(req, p) (req->buflen - PTR_DIFF(p, req->buf))


/* Note that chain_size must be available as an extern int to this macro. */
#define smb_offset(p,buf) (PTR_DIFF(p,buf+4))

#define smb_len(buf) (PVAL(buf,3)|(PVAL(buf,2)<<8)|((PVAL(buf,1)&1)<<16))
#define _smb_setlen(buf,len) do { buf[0] = 0; buf[1] = ((len)&0x10000)>>16; \
        buf[2] = ((len)&0xFF00)>>8; buf[3] = (len)&0xFF; } while (0)

#define smb_len_large(buf) (PVAL(buf,3)|(PVAL(buf,2)<<8)|(PVAL(buf,1)<<16))
#define _smb_setlen_large(buf,len) do { buf[0] = 0; buf[1] = ((len)&0xFF0000)>>16; \
        buf[2] = ((len)&0xFF00)>>8; buf[3] = (len)&0xFF; } while (0)

#define ENCRYPTION_REQUIRED(conn) ((conn) ? ((conn)->encrypt_level == Required) : false)
#define IS_CONN_ENCRYPTED(conn) ((conn) ? (conn)->encrypted_tid : false)

/****************************************************************************
true if two IPv4 addresses are equal
****************************************************************************/

#define ip_equal_v4(ip1,ip2) ((ip1).s_addr == (ip2).s_addr)

/*****************************************************************
 splits out the last subkey of a key
 *****************************************************************/  

#define reg_get_subkey(full_keyname, key_name, subkey_name) \
	split_at_last_component(full_keyname, key_name, '\\', subkey_name)

/****************************************************************************
 Return True if the offset is at zero.
****************************************************************************/

#define dptr_zero(buf) (IVAL(buf,1) == 0)

/*******************************************************************
copy an IP address from one buffer to another
********************************************************************/

#define putip(dest,src) memcpy(dest,src,4)

/*******************************************************************
 Return True if a server has CIFS UNIX capabilities.
********************************************************************/

#define SERVER_HAS_UNIX_CIFS(c) ((c)->capabilities & CAP_UNIX)

/****************************************************************************
 Make a filename into unix format.
****************************************************************************/

#define IS_DIRECTORY_SEP(c) ((c) == '\\' || (c) == '/')
#define unix_format(fname) string_replace(fname,'\\','/')
#define unix_format_w(fname) string_replace_w(fname, UCS2_CHAR('\\'), UCS2_CHAR('/'))

/****************************************************************************
 Make a file into DOS format.
****************************************************************************/

#define dos_format(fname) string_replace(fname,'/','\\')

/*****************************************************************************
 Check to see if we are a DC for this domain
*****************************************************************************/

#define IS_DC  (lp_server_role()==ROLE_DOMAIN_PDC || lp_server_role()==ROLE_DOMAIN_BDC) 

/*
 * If you add any entries to KERBEROS_VERIFY defines, please modify the below expressions
 * so they remain accurate.
 */
#define USE_KERBEROS_KEYTAB (KERBEROS_VERIFY_SECRETS != lp_kerberos_method())
#define USE_SYSTEM_KEYTAB \
    ((KERBEROS_VERIFY_SECRETS_AND_KEYTAB == lp_kerberos_method()) || \
     (KERBEROS_VERIFY_SYSTEM_KEYTAB == lp_kerberos_method()))

/*****************************************************************************
 Safe allocation macros.
*****************************************************************************/

#define SMB_MALLOC_ARRAY(type,count) (type *)malloc_array(sizeof(type),(count))
#define SMB_MEMALIGN_ARRAY(type,align,count) (type *)memalign_array(sizeof(type),align,(count))
#define SMB_REALLOC(p,s) Realloc((p),(s),True)	/* Always frees p on error or s == 0 */
#define SMB_REALLOC_KEEP_OLD_ON_ERROR(p,s) Realloc((p),(s),False) /* Never frees p on error or s == 0 */
#define SMB_REALLOC_ARRAY(p,type,count) (type *)realloc_array((p),sizeof(type),(count),True) /* Always frees p on error or s == 0 */
#define SMB_REALLOC_ARRAY_KEEP_OLD_ON_ERROR(p,type,count) (type *)realloc_array((p),sizeof(type),(count),False) /* Never frees p on error or s == 0 */
#define SMB_CALLOC_ARRAY(type,count) (type *)calloc_array(sizeof(type),(count))
#define SMB_XMALLOC_P(type) (type *)smb_xmalloc_array(sizeof(type),1)
#define SMB_XMALLOC_ARRAY(type,count) (type *)smb_xmalloc_array(sizeof(type),(count))

/* The new talloc is paranoid malloc checker safe. */

#if 0

Disable these now we have checked all code paths and ensured
NULL returns on zero request. JRA.

#define TALLOC(ctx, size) talloc_zeronull(ctx, size, __location__)
#define TALLOC_P(ctx, type) (type *)talloc_zeronull(ctx, sizeof(type), #type)
#define TALLOC_ARRAY(ctx, type, count) (type *)_talloc_array_zeronull(ctx, sizeof(type), count, #type)
#define TALLOC_MEMDUP(ctx, ptr, size) _talloc_memdup_zeronull(ctx, ptr, size, __location__)
#define TALLOC_ZERO(ctx, size) _talloc_zero_zeronull(ctx, size, __location__)
#define TALLOC_ZERO_P(ctx, type) (type *)_talloc_zero_zeronull(ctx, sizeof(type), #type)
#define TALLOC_ZERO_ARRAY(ctx, type, count) (type *)_talloc_zero_array_zeronull(ctx, sizeof(type), count, #type)
#define TALLOC_SIZE(ctx, size) talloc_zeronull(ctx, size, __location__)
#define TALLOC_ZERO_SIZE(ctx, size) _talloc_zero_zeronull(ctx, size, __location__)

#else

#define TALLOC(ctx, size) talloc_named_const(ctx, size, __location__)
#define TALLOC_P(ctx, type) (type *)talloc_named_const(ctx, sizeof(type), #type)
#define TALLOC_ARRAY(ctx, type, count) (type *)_talloc_array(ctx, sizeof(type), count, #type)
#define TALLOC_MEMDUP(ctx, ptr, size) _talloc_memdup(ctx, ptr, size, __location__)
#define TALLOC_ZERO(ctx, size) _talloc_zero(ctx, size, __location__)
#define TALLOC_ZERO_P(ctx, type) (type *)_talloc_zero(ctx, sizeof(type), #type)
#define TALLOC_ZERO_ARRAY(ctx, type, count) (type *)_talloc_zero_array(ctx, sizeof(type), count, #type)
#define TALLOC_SIZE(ctx, size) talloc_named_const(ctx, size, __location__)
#define TALLOC_ZERO_SIZE(ctx, size) _talloc_zero(ctx, size, __location__)

#endif

#define TALLOC_REALLOC(ctx, ptr, count) _talloc_realloc(ctx, ptr, count, __location__)
#define TALLOC_REALLOC_ARRAY(ctx, ptr, type, count) (type *)_talloc_realloc_array(ctx, ptr, sizeof(type), count, #type)
#define talloc_destroy(ctx) talloc_free(ctx)
#ifndef TALLOC_FREE
#define TALLOC_FREE(ctx) do { talloc_free(ctx); ctx=NULL; } while(0)
#endif

/* only define PARANOID_MALLOC_CHECKER with --enable-developer */

#if defined(DEVELOPER)
#  define PARANOID_MALLOC_CHECKER 1
#endif

#if defined(PARANOID_MALLOC_CHECKER)

/* Get medieval on our ass about malloc.... */

/* Restrictions on malloc/realloc/calloc. */
#ifdef malloc
#undef malloc
#endif
#define malloc(s) __ERROR_DONT_USE_MALLOC_DIRECTLY

#ifdef realloc
#undef realloc
#endif
#define realloc(p,s) __ERROR_DONT_USE_REALLOC_DIRECTLY

#ifdef calloc
#undef calloc
#endif
#define calloc(n,s) __ERROR_DONT_USE_CALLOC_DIRECTLY

#ifdef strndup
#undef strndup
#endif
#define strndup(s,n) __ERROR_DONT_USE_STRNDUP_DIRECTLY

#ifdef strdup
#undef strdup
#endif
#define strdup(s) __ERROR_DONT_USE_STRDUP_DIRECTLY

#define SMB_MALLOC(s) malloc_(s)
#define SMB_MALLOC_P(type) (type *)malloc_(sizeof(type))

#define SMB_STRDUP(s) smb_xstrdup(s)
#define SMB_STRNDUP(s,n) smb_xstrndup(s,n)

#else

/* Regular malloc code. */

#define SMB_MALLOC(s) malloc(s)
#define SMB_MALLOC_P(type) (type *)malloc(sizeof(type))

#define SMB_STRDUP(s) strdup(s)
#define SMB_STRNDUP(s,n) strndup(s,n)

#endif

#define ADD_TO_ARRAY(mem_ctx, type, elem, array, num) \
do { \
	*(array) = ((mem_ctx) != NULL) ? \
		TALLOC_REALLOC_ARRAY(mem_ctx, (*(array)), type, (*(num))+1) : \
		SMB_REALLOC_ARRAY((*(array)), type, (*(num))+1); \
	SMB_ASSERT((*(array)) != NULL); \
	(*(array))[*(num)] = (elem); \
	(*(num)) += 1; \
} while (0)

#define ADD_TO_LARGE_ARRAY(mem_ctx, type, elem, array, num, size) \
	add_to_large_array((mem_ctx), sizeof(type), &(elem), (void *)(array), (num), (size));

#ifndef toupper_ascii_fast
/* Warning - this must only be called with 0 <= c < 128. IT WILL
 * GIVE GARBAGE if c > 128 or c < 0. JRA.
 */
extern const char toupper_ascii_fast_table[];
#define toupper_ascii_fast(c) toupper_ascii_fast_table[(unsigned int)(c)];
#endif

#endif /* _SMB_MACROS_H */
