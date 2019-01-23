/*
   Unix SMB/CIFS implementation.

   SMB2 client library header

   Copyright (C) Andrew Tridgell 2005

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

#ifndef __LIBCLI_SMB2_SMB2_CONSTANTS_H__
#define __LIBCLI_SMB2_SMB2_CONSTANTS_H__

/* offsets into header elements for a sync SMB2 request */
#define SMB2_HDR_PROTOCOL_ID    0x00
#define SMB2_HDR_LENGTH		0x04
#define SMB2_HDR_CREDIT_CHARGE	0x06
#define SMB2_HDR_EPOCH		SMB2_HDR_CREDIT_CHARGE /* TODO: remove this */
#define SMB2_HDR_STATUS		0x08
#define SMB2_HDR_OPCODE		0x0c
#define SMB2_HDR_CREDIT		0x0e
#define SMB2_HDR_FLAGS		0x10
#define SMB2_HDR_NEXT_COMMAND	0x14
#define SMB2_HDR_MESSAGE_ID     0x18
#define SMB2_HDR_PID		0x20
#define SMB2_HDR_TID		0x24
#define SMB2_HDR_SESSION_ID	0x28
#define SMB2_HDR_SIGNATURE	0x30 /* 16 bytes */
#define SMB2_HDR_BODY		0x40

/* offsets into header elements for an async SMB2 request */
#define SMB2_HDR_ASYNC_ID	0x20

/* header flags */
#define SMB2_HDR_FLAG_REDIRECT  0x01
#define SMB2_HDR_FLAG_ASYNC     0x02
#define SMB2_HDR_FLAG_CHAINED   0x04
#define SMB2_HDR_FLAG_SIGNED    0x08
#define SMB2_HDR_FLAG_DFS       0x10000000

/* SMB2 opcodes */
#define SMB2_OP_NEGPROT   0x00
#define SMB2_OP_SESSSETUP 0x01
#define SMB2_OP_LOGOFF    0x02
#define SMB2_OP_TCON      0x03
#define SMB2_OP_TDIS      0x04
#define SMB2_OP_CREATE    0x05
#define SMB2_OP_CLOSE     0x06
#define SMB2_OP_FLUSH     0x07
#define SMB2_OP_READ      0x08
#define SMB2_OP_WRITE     0x09
#define SMB2_OP_LOCK      0x0a
#define SMB2_OP_IOCTL     0x0b
#define SMB2_OP_CANCEL    0x0c
#define SMB2_OP_KEEPALIVE 0x0d
#define SMB2_OP_FIND      0x0e
#define SMB2_OP_NOTIFY    0x0f
#define SMB2_OP_GETINFO   0x10
#define SMB2_OP_SETINFO   0x11
#define SMB2_OP_BREAK     0x12

#define SMB2_MAGIC 0x424D53FE /* 0xFE 'S' 'M' 'B' */

/* SMB2 negotiate dialects */
#define SMB2_DIALECT_REVISION_000       0x0000 /* early beta dialect */
#define SMB2_DIALECT_REVISION_202       0x0202
#define SMB2_DIALECT_REVISION_210       0x0210
#define SMB2_DIALECT_REVISION_2FF       0x02FF

/* SMB2 negotiate security_mode */
#define SMB2_NEGOTIATE_SIGNING_ENABLED   0x01
#define SMB2_NEGOTIATE_SIGNING_REQUIRED  0x02

/* SMB2 capabilities - only 1 so far. I'm sure more will be added */
#define SMB2_CAP_DFS                     0x00000001
#define SMB2_CAP_LEASING                 0x00000002 /* only in dialect 0x210 */
#define SMB2_CAP_LARGE_MTU		 0x00000004 /* only in dialect 0x210 */
/* so we can spot new caps as added */
#define SMB2_CAP_ALL                     SMB2_CAP_DFS

/* SMB2 session flags */
#define SMB2_SESSION_FLAG_IS_GUEST       0x0001
#define SMB2_SESSION_FLAG_IS_NULL        0x0002

/* SMB2 sharetype flags */
#define SMB2_SHARE_TYPE_DISK		0x1
#define SMB2_SHARE_TYPE_PIPE		0x2
#define SMB2_SHARE_TYPE_PRINT		0x3

/* SMB2 share flags */
#define SMB2_SHAREFLAG_MANUAL_CACHING                    0x0000
#define SMB2_SHAREFLAG_AUTO_CACHING                      0x0010
#define SMB2_SHAREFLAG_VDO_CACHING                       0x0020
#define SMB2_SHAREFLAG_NO_CACHING                        0x0030
#define SMB2_SHAREFLAG_DFS                               0x0001
#define SMB2_SHAREFLAG_DFS_ROOT                          0x0002
#define SMB2_SHAREFLAG_RESTRICT_EXCLUSIVE_OPENS          0x0100
#define SMB2_SHAREFLAG_FORCE_SHARED_DELETE               0x0200
#define SMB2_SHAREFLAG_ALLOW_NAMESPACE_CACHING           0x0400
#define SMB2_SHAREFLAG_ACCESS_BASED_DIRECTORY_ENUM       0x0800
#define SMB2_SHAREFLAG_ALL                               0x0F33

/* SMB2 share capafilities */
#define SMB2_SHARE_CAP_DFS		0x8

/* SMB2 create security flags */
#define SMB2_SECURITY_DYNAMIC_TRACKING                   0x01
#define SMB2_SECURITY_EFFECTIVE_ONLY                     0x02

/* SMB2 lock flags */
#define SMB2_LOCK_FLAG_NONE		0x00000000
#define SMB2_LOCK_FLAG_SHARED		0x00000001
#define SMB2_LOCK_FLAG_EXCLUSIVE	0x00000002
#define SMB2_LOCK_FLAG_UNLOCK		0x00000004
#define SMB2_LOCK_FLAG_FAIL_IMMEDIATELY	0x00000010
#define SMB2_LOCK_FLAG_ALL_MASK		0x00000017

/* SMB2 requested oplock levels */
#define SMB2_OPLOCK_LEVEL_NONE                           0x00
#define SMB2_OPLOCK_LEVEL_II                             0x01
#define SMB2_OPLOCK_LEVEL_EXCLUSIVE                      0x08
#define SMB2_OPLOCK_LEVEL_BATCH                          0x09
#define SMB2_OPLOCK_LEVEL_LEASE                          0xFF

/* SMB2 lease bits */
#define SMB2_LEASE_NONE                                  0x00
#define SMB2_LEASE_READ                                  0x01
#define SMB2_LEASE_HANDLE                                0x02
#define SMB2_LEASE_WRITE                                 0x04

/* SMB2 lease break flags */
#define SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED        0x01

/* SMB2 impersonation levels */
#define SMB2_IMPERSONATION_ANONYMOUS                     0x00
#define SMB2_IMPERSONATION_IDENTIFICATION                0x01
#define SMB2_IMPERSONATION_IMPERSONATION                 0x02
#define SMB2_IMPERSONATION_DELEGATE                      0x03

/* SMB2 create tags */
#define SMB2_CREATE_TAG_EXTA "ExtA"
#define SMB2_CREATE_TAG_MXAC "MxAc"
#define SMB2_CREATE_TAG_SECD "SecD"
#define SMB2_CREATE_TAG_DHNQ "DHnQ"
#define SMB2_CREATE_TAG_DHNC "DHnC"
#define SMB2_CREATE_TAG_ALSI "AlSi"
#define SMB2_CREATE_TAG_TWRP "TWrp"
#define SMB2_CREATE_TAG_QFID "QFid"
#define SMB2_CREATE_TAG_RQLS "RqLs"

/* SMB2 Create ignore some more create_options */
#define SMB2_CREATE_OPTIONS_NOT_SUPPORTED_MASK	(NTCREATEX_OPTIONS_TREE_CONNECTION | \
						 NTCREATEX_OPTIONS_OPFILTER)

/*
  SMB2 uses different level numbers for the same old SMB trans2 search levels
*/
#define SMB2_FIND_DIRECTORY_INFO         0x01
#define SMB2_FIND_FULL_DIRECTORY_INFO    0x02
#define SMB2_FIND_BOTH_DIRECTORY_INFO    0x03
#define SMB2_FIND_NAME_INFO              0x0C
#define SMB2_FIND_ID_BOTH_DIRECTORY_INFO 0x25
#define SMB2_FIND_ID_FULL_DIRECTORY_INFO 0x26

/* flags for SMB2 find */
#define SMB2_CONTINUE_FLAG_RESTART    0x01
#define SMB2_CONTINUE_FLAG_SINGLE     0x02
#define SMB2_CONTINUE_FLAG_INDEX      0x04
#define SMB2_CONTINUE_FLAG_REOPEN     0x10

/* getinfo classes */
#define SMB2_GETINFO_FILE               0x01
#define SMB2_GETINFO_FS                 0x02
#define SMB2_GETINFO_SECURITY           0x03
#define SMB2_GETINFO_QUOTA              0x04

#define SMB2_CLOSE_FLAGS_FULL_INFORMATION (0x01)

#endif
