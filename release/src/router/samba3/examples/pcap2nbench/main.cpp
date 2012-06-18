/*\
 *  pcap2nbench - Converts libpcap network traces to nbench input
 *  Copyright (C) 2004  Jim McDonough <jmcd@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Written by Anthony Liguori <aliguori@us.ibm.com>
\*/

#include <iostream>
#include <pcap.h>
#include <getopt.h>
#include <stdint.h>
#include <netinet/in.h>

#include "ethernet.hpp"
#include "ip.hpp"
#include "tcp.hpp"
#include "smb.hpp"
#include "ntcreateandxrequest.hpp"
#include "ntcreateandxresponse.hpp"
#include "readandxrequest.hpp"
#include "writeandxrequest.hpp"
#include "closerequest.hpp"

#include <vector>
#include <set>


/* derived from source/include/nterr.h */
const char *nt_status_to_string[] = {
  "NT_STATUS_OK", /* 0x0000 */
  "NT_STATUS_UNSUCCESSFUL", /* 0x0001 */
  "NT_STATUS_NOT_IMPLEMENTED", /* 0x0002 */
  "NT_STATUS_INVALID_INFO_CLASS", /* 0x0003 */
  "NT_STATUS_INFO_LENGTH_MISMATCH", /* 0x0004 */
  "NT_STATUS_ACCESS_VIOLATION", /* 0x0005 */
  "NT_STATUS_IN_PAGE_ERROR", /* 0x0006 */
  "NT_STATUS_PAGEFILE_QUOTA", /* 0x0007 */
  "NT_STATUS_INVALID_HANDLE", /* 0x0008 */
  "NT_STATUS_BAD_INITIAL_STACK", /* 0x0009 */
  "NT_STATUS_BAD_INITIAL_PC", /* 0x000a */
  "NT_STATUS_INVALID_CID", /* 0x000b */
  "NT_STATUS_TIMER_NOT_CANCELED", /* 0x000c */
  "NT_STATUS_INVALID_PARAMETER", /* 0x000d */
  "NT_STATUS_NO_SUCH_DEVICE", /* 0x000e */
  "NT_STATUS_NO_SUCH_FILE", /* 0x000f */
  "NT_STATUS_INVALID_DEVICE_REQUEST", /* 0x0010 */
  "NT_STATUS_END_OF_FILE", /* 0x0011 */
  "NT_STATUS_WRONG_VOLUME", /* 0x0012 */
  "NT_STATUS_NO_MEDIA_IN_DEVICE", /* 0x0013 */
  "NT_STATUS_UNRECOGNIZED_MEDIA", /* 0x0014 */
  "NT_STATUS_NONEXISTENT_SECTOR", /* 0x0015 */
  "NT_STATUS_MORE_PROCESSING_REQUIRED", /* 0x0016 */
  "NT_STATUS_NO_MEMORY", /* 0x0017 */
  "NT_STATUS_CONFLICTING_ADDRESSES", /* 0x0018 */
  "NT_STATUS_NOT_MAPPED_VIEW", /* 0x0019 */
  "NT_STATUS_UNABLE_TO_FREE_VM", /* 0x001a */
  "NT_STATUS_UNABLE_TO_DELETE_SECTION", /* 0x001b */
  "NT_STATUS_INVALID_SYSTEM_SERVICE", /* 0x001c */
  "NT_STATUS_ILLEGAL_INSTRUCTION", /* 0x001d */
  "NT_STATUS_INVALID_LOCK_SEQUENCE", /* 0x001e */
  "NT_STATUS_INVALID_VIEW_SIZE", /* 0x001f */
  "NT_STATUS_INVALID_FILE_FOR_SECTION", /* 0x0020 */
  "NT_STATUS_ALREADY_COMMITTED", /* 0x0021 */
  "NT_STATUS_ACCESS_DENIED", /* 0x0022 */
  "NT_STATUS_BUFFER_TOO_SMALL", /* 0x0023 */
  "NT_STATUS_OBJECT_TYPE_MISMATCH", /* 0x0024 */
  "NT_STATUS_NONCONTINUABLE_EXCEPTION", /* 0x0025 */
  "NT_STATUS_INVALID_DISPOSITION", /* 0x0026 */
  "NT_STATUS_UNWIND", /* 0x0027 */
  "NT_STATUS_BAD_STACK", /* 0x0028 */
  "NT_STATUS_INVALID_UNWIND_TARGET", /* 0x0029 */
  "NT_STATUS_NOT_LOCKED", /* 0x002a */
  "NT_STATUS_PARITY_ERROR", /* 0x002b */
  "NT_STATUS_UNABLE_TO_DECOMMIT_VM", /* 0x002c */
  "NT_STATUS_NOT_COMMITTED", /* 0x002d */
  "NT_STATUS_INVALID_PORT_ATTRIBUTES", /* 0x002e */
  "NT_STATUS_PORT_MESSAGE_TOO_LONG", /* 0x002f */
  "NT_STATUS_INVALID_PARAMETER_MIX", /* 0x0030 */
  "NT_STATUS_INVALID_QUOTA_LOWER", /* 0x0031 */
  "NT_STATUS_DISK_CORRUPT_ERROR", /* 0x0032 */
  "NT_STATUS_OBJECT_NAME_INVALID", /* 0x0033 */
  "NT_STATUS_OBJECT_NAME_NOT_FOUND", /* 0x0034 */
  "NT_STATUS_OBJECT_NAME_COLLISION", /* 0x0035 */
  "NT_STATUS_HANDLE_NOT_WAITABLE", /* 0x0036 */
  "NT_STATUS_PORT_DISCONNECTED", /* 0x0037 */
  "NT_STATUS_DEVICE_ALREADY_ATTACHED", /* 0x0038 */
  "NT_STATUS_OBJECT_PATH_INVALID", /* 0x0039 */
  "NT_STATUS_OBJECT_PATH_NOT_FOUND", /* 0x003a */
  "NT_STATUS_OBJECT_PATH_SYNTAX_BAD", /* 0x003b */
  "NT_STATUS_DATA_OVERRUN", /* 0x003c */
  "NT_STATUS_DATA_LATE_ERROR", /* 0x003d */
  "NT_STATUS_DATA_ERROR", /* 0x003e */
  "NT_STATUS_CRC_ERROR", /* 0x003f */
  "NT_STATUS_SECTION_TOO_BIG", /* 0x0040 */
  "NT_STATUS_PORT_CONNECTION_REFUSED", /* 0x0041 */
  "NT_STATUS_INVALID_PORT_HANDLE", /* 0x0042 */
  "NT_STATUS_SHARING_VIOLATION", /* 0x0043 */
  "NT_STATUS_QUOTA_EXCEEDED", /* 0x0044 */
  "NT_STATUS_INVALID_PAGE_PROTECTION", /* 0x0045 */
  "NT_STATUS_MUTANT_NOT_OWNED", /* 0x0046 */
  "NT_STATUS_SEMAPHORE_LIMIT_EXCEEDED", /* 0x0047 */
  "NT_STATUS_PORT_ALREADY_SET", /* 0x0048 */
  "NT_STATUS_SECTION_NOT_IMAGE", /* 0x0049 */
  "NT_STATUS_SUSPEND_COUNT_EXCEEDED", /* 0x004a */
  "NT_STATUS_THREAD_IS_TERMINATING", /* 0x004b */
  "NT_STATUS_BAD_WORKING_SET_LIMIT", /* 0x004c */
  "NT_STATUS_INCOMPATIBLE_FILE_MAP", /* 0x004d */
  "NT_STATUS_SECTION_PROTECTION", /* 0x004e */
  "NT_STATUS_EAS_NOT_SUPPORTED", /* 0x004f */
  "NT_STATUS_EA_TOO_LARGE", /* 0x0050 */
  "NT_STATUS_NONEXISTENT_EA_ENTRY", /* 0x0051 */
  "NT_STATUS_NO_EAS_ON_FILE", /* 0x0052 */
  "NT_STATUS_EA_CORRUPT_ERROR", /* 0x0053 */
  "NT_STATUS_FILE_LOCK_CONFLICT", /* 0x0054 */
  "NT_STATUS_LOCK_NOT_GRANTED", /* 0x0055 */
  "NT_STATUS_DELETE_PENDING", /* 0x0056 */
  "NT_STATUS_CTL_FILE_NOT_SUPPORTED", /* 0x0057 */
  "NT_STATUS_UNKNOWN_REVISION", /* 0x0058 */
  "NT_STATUS_REVISION_MISMATCH", /* 0x0059 */
  "NT_STATUS_INVALID_OWNER", /* 0x005a */
  "NT_STATUS_INVALID_PRIMARY_GROUP", /* 0x005b */
  "NT_STATUS_NO_IMPERSONATION_TOKEN", /* 0x005c */
  "NT_STATUS_CANT_DISABLE_MANDATORY", /* 0x005d */
  "NT_STATUS_NO_LOGON_SERVERS", /* 0x005e */
  "NT_STATUS_NO_SUCH_LOGON_SESSION", /* 0x005f */
  "NT_STATUS_NO_SUCH_PRIVILEGE", /* 0x0060 */
  "NT_STATUS_PRIVILEGE_NOT_HELD", /* 0x0061 */
  "NT_STATUS_INVALID_ACCOUNT_NAME", /* 0x0062 */
  "NT_STATUS_USER_EXISTS", /* 0x0063 */
  "NT_STATUS_NO_SUCH_USER", /* 0x0064 */
  "NT_STATUS_GROUP_EXISTS", /* 0x0065 */
  "NT_STATUS_NO_SUCH_GROUP", /* 0x0066 */
  "NT_STATUS_MEMBER_IN_GROUP", /* 0x0067 */
  "NT_STATUS_MEMBER_NOT_IN_GROUP", /* 0x0068 */
  "NT_STATUS_LAST_ADMIN", /* 0x0069 */
  "NT_STATUS_WRONG_PASSWORD", /* 0x006a */
  "NT_STATUS_ILL_FORMED_PASSWORD", /* 0x006b */
  "NT_STATUS_PASSWORD_RESTRICTION", /* 0x006c */
  "NT_STATUS_LOGON_FAILURE", /* 0x006d */
  "NT_STATUS_ACCOUNT_RESTRICTION", /* 0x006e */
  "NT_STATUS_INVALID_LOGON_HOURS", /* 0x006f */
  "NT_STATUS_INVALID_WORKSTATION", /* 0x0070 */
  "NT_STATUS_PASSWORD_EXPIRED", /* 0x0071 */
  "NT_STATUS_ACCOUNT_DISABLED", /* 0x0072 */
  "NT_STATUS_NONE_MAPPED", /* 0x0073 */
  "NT_STATUS_TOO_MANY_LUIDS_REQUESTED", /* 0x0074 */
  "NT_STATUS_LUIDS_EXHAUSTED", /* 0x0075 */
  "NT_STATUS_INVALID_SUB_AUTHORITY", /* 0x0076 */
  "NT_STATUS_INVALID_ACL", /* 0x0077 */
  "NT_STATUS_INVALID_SID", /* 0x0078 */
  "NT_STATUS_INVALID_SECURITY_DESCR", /* 0x0079 */
  "NT_STATUS_PROCEDURE_NOT_FOUND", /* 0x007a */
  "NT_STATUS_INVALID_IMAGE_FORMAT", /* 0x007b */
  "NT_STATUS_NO_TOKEN", /* 0x007c */
  "NT_STATUS_BAD_INHERITANCE_ACL", /* 0x007d */
  "NT_STATUS_RANGE_NOT_LOCKED", /* 0x007e */
  "NT_STATUS_DISK_FULL", /* 0x007f */
  "NT_STATUS_SERVER_DISABLED", /* 0x0080 */
  "NT_STATUS_SERVER_NOT_DISABLED", /* 0x0081 */
  "NT_STATUS_TOO_MANY_GUIDS_REQUESTED", /* 0x0082 */
  "NT_STATUS_GUIDS_EXHAUSTED", /* 0x0083 */
  "NT_STATUS_INVALID_ID_AUTHORITY", /* 0x0084 */
  "NT_STATUS_AGENTS_EXHAUSTED", /* 0x0085 */
  "NT_STATUS_INVALID_VOLUME_LABEL", /* 0x0086 */
  "NT_STATUS_SECTION_NOT_EXTENDED", /* 0x0087 */
  "NT_STATUS_NOT_MAPPED_DATA", /* 0x0088 */
  "NT_STATUS_RESOURCE_DATA_NOT_FOUND", /* 0x0089 */
  "NT_STATUS_RESOURCE_TYPE_NOT_FOUND", /* 0x008a */
  "NT_STATUS_RESOURCE_NAME_NOT_FOUND", /* 0x008b */
  "NT_STATUS_ARRAY_BOUNDS_EXCEEDED", /* 0x008c */
  "NT_STATUS_FLOAT_DENORMAL_OPERAND", /* 0x008d */
  "NT_STATUS_FLOAT_DIVIDE_BY_ZERO", /* 0x008e */
  "NT_STATUS_FLOAT_INEXACT_RESULT", /* 0x008f */
  "NT_STATUS_FLOAT_INVALID_OPERATION", /* 0x0090 */
  "NT_STATUS_FLOAT_OVERFLOW", /* 0x0091 */
  "NT_STATUS_FLOAT_STACK_CHECK", /* 0x0092 */
  "NT_STATUS_FLOAT_UNDERFLOW", /* 0x0093 */
  "NT_STATUS_INTEGER_DIVIDE_BY_ZERO", /* 0x0094 */
  "NT_STATUS_INTEGER_OVERFLOW", /* 0x0095 */
  "NT_STATUS_PRIVILEGED_INSTRUCTION", /* 0x0096 */
  "NT_STATUS_TOO_MANY_PAGING_FILES", /* 0x0097 */
  "NT_STATUS_FILE_INVALID", /* 0x0098 */
  "NT_STATUS_ALLOTTED_SPACE_EXCEEDED", /* 0x0099 */
  "NT_STATUS_INSUFFICIENT_RESOURCES", /* 0x009a */
  "NT_STATUS_DFS_EXIT_PATH_FOUND", /* 0x009b */
  "NT_STATUS_DEVICE_DATA_ERROR", /* 0x009c */
  "NT_STATUS_DEVICE_NOT_CONNECTED", /* 0x009d */
  "NT_STATUS_DEVICE_POWER_FAILURE", /* 0x009e */
  "NT_STATUS_FREE_VM_NOT_AT_BASE", /* 0x009f */
  "NT_STATUS_MEMORY_NOT_ALLOCATED", /* 0x00a0 */
  "NT_STATUS_WORKING_SET_QUOTA", /* 0x00a1 */
  "NT_STATUS_MEDIA_WRITE_PROTECTED", /* 0x00a2 */
  "NT_STATUS_DEVICE_NOT_READY", /* 0x00a3 */
  "NT_STATUS_INVALID_GROUP_ATTRIBUTES", /* 0x00a4 */
  "NT_STATUS_BAD_IMPERSONATION_LEVEL", /* 0x00a5 */
  "NT_STATUS_CANT_OPEN_ANONYMOUS", /* 0x00a6 */
  "NT_STATUS_BAD_VALIDATION_CLASS", /* 0x00a7 */
  "NT_STATUS_BAD_TOKEN_TYPE", /* 0x00a8 */
  "NT_STATUS_BAD_MASTER_BOOT_RECORD", /* 0x00a9 */
  "NT_STATUS_INSTRUCTION_MISALIGNMENT", /* 0x00aa */
  "NT_STATUS_INSTANCE_NOT_AVAILABLE", /* 0x00ab */
  "NT_STATUS_PIPE_NOT_AVAILABLE", /* 0x00ac */
  "NT_STATUS_INVALID_PIPE_STATE", /* 0x00ad */
  "NT_STATUS_PIPE_BUSY", /* 0x00ae */
  "NT_STATUS_ILLEGAL_FUNCTION", /* 0x00af */
  "NT_STATUS_PIPE_DISCONNECTED", /* 0x00b0 */
  "NT_STATUS_PIPE_CLOSING", /* 0x00b1 */
  "NT_STATUS_PIPE_CONNECTED", /* 0x00b2 */
  "NT_STATUS_PIPE_LISTENING", /* 0x00b3 */
  "NT_STATUS_INVALID_READ_MODE", /* 0x00b4 */
  "NT_STATUS_IO_TIMEOUT", /* 0x00b5 */
  "NT_STATUS_FILE_FORCED_CLOSED", /* 0x00b6 */
  "NT_STATUS_PROFILING_NOT_STARTED", /* 0x00b7 */
  "NT_STATUS_PROFILING_NOT_STOPPED", /* 0x00b8 */
  "NT_STATUS_COULD_NOT_INTERPRET", /* 0x00b9 */
  "NT_STATUS_FILE_IS_A_DIRECTORY", /* 0x00ba */
  "NT_STATUS_NOT_SUPPORTED", /* 0x00bb */
  "NT_STATUS_REMOTE_NOT_LISTENING", /* 0x00bc */
  "NT_STATUS_DUPLICATE_NAME", /* 0x00bd */
  "NT_STATUS_BAD_NETWORK_PATH", /* 0x00be */
  "NT_STATUS_NETWORK_BUSY", /* 0x00bf */
  "NT_STATUS_DEVICE_DOES_NOT_EXIST", /* 0x00c0 */
  "NT_STATUS_TOO_MANY_COMMANDS", /* 0x00c1 */
  "NT_STATUS_ADAPTER_HARDWARE_ERROR", /* 0x00c2 */
  "NT_STATUS_INVALID_NETWORK_RESPONSE", /* 0x00c3 */
  "NT_STATUS_UNEXPECTED_NETWORK_ERROR", /* 0x00c4 */
  "NT_STATUS_BAD_REMOTE_ADAPTER", /* 0x00c5 */
  "NT_STATUS_PRINT_QUEUE_FULL", /* 0x00c6 */
  "NT_STATUS_NO_SPOOL_SPACE", /* 0x00c7 */
  "NT_STATUS_PRINT_CANCELLED", /* 0x00c8 */
  "NT_STATUS_NETWORK_NAME_DELETED", /* 0x00c9 */
  "NT_STATUS_NETWORK_ACCESS_DENIED", /* 0x00ca */
  "NT_STATUS_BAD_DEVICE_TYPE", /* 0x00cb */
  "NT_STATUS_BAD_NETWORK_NAME", /* 0x00cc */
  "NT_STATUS_TOO_MANY_NAMES", /* 0x00cd */
  "NT_STATUS_TOO_MANY_SESSIONS", /* 0x00ce */
  "NT_STATUS_SHARING_PAUSED", /* 0x00cf */
  "NT_STATUS_REQUEST_NOT_ACCEPTED", /* 0x00d0 */
  "NT_STATUS_REDIRECTOR_PAUSED", /* 0x00d1 */
  "NT_STATUS_NET_WRITE_FAULT", /* 0x00d2 */
  "NT_STATUS_PROFILING_AT_LIMIT", /* 0x00d3 */
  "NT_STATUS_NOT_SAME_DEVICE", /* 0x00d4 */
  "NT_STATUS_FILE_RENAMED", /* 0x00d5 */
  "NT_STATUS_VIRTUAL_CIRCUIT_CLOSED", /* 0x00d6 */
  "NT_STATUS_NO_SECURITY_ON_OBJECT", /* 0x00d7 */
  "NT_STATUS_CANT_WAIT", /* 0x00d8 */
  "NT_STATUS_PIPE_EMPTY", /* 0x00d9 */
  "NT_STATUS_CANT_ACCESS_DOMAIN_INFO", /* 0x00da */
  "NT_STATUS_CANT_TERMINATE_SELF", /* 0x00db */
  "NT_STATUS_INVALID_SERVER_STATE", /* 0x00dc */
  "NT_STATUS_INVALID_DOMAIN_STATE", /* 0x00dd */
  "NT_STATUS_INVALID_DOMAIN_ROLE", /* 0x00de */
  "NT_STATUS_NO_SUCH_DOMAIN", /* 0x00df */
  "NT_STATUS_DOMAIN_EXISTS", /* 0x00e0 */
  "NT_STATUS_DOMAIN_LIMIT_EXCEEDED", /* 0x00e1 */
  "NT_STATUS_OPLOCK_NOT_GRANTED", /* 0x00e2 */
  "NT_STATUS_INVALID_OPLOCK_PROTOCOL", /* 0x00e3 */
  "NT_STATUS_INTERNAL_DB_CORRUPTION", /* 0x00e4 */
  "NT_STATUS_INTERNAL_ERROR", /* 0x00e5 */
  "NT_STATUS_GENERIC_NOT_MAPPED", /* 0x00e6 */
  "NT_STATUS_BAD_DESCRIPTOR_FORMAT", /* 0x00e7 */
  "NT_STATUS_INVALID_USER_BUFFER", /* 0x00e8 */
  "NT_STATUS_UNEXPECTED_IO_ERROR", /* 0x00e9 */
  "NT_STATUS_UNEXPECTED_MM_CREATE_ERR", /* 0x00ea */
  "NT_STATUS_UNEXPECTED_MM_MAP_ERROR", /* 0x00eb */
  "NT_STATUS_UNEXPECTED_MM_EXTEND_ERR", /* 0x00ec */
  "NT_STATUS_NOT_LOGON_PROCESS", /* 0x00ed */
  "NT_STATUS_LOGON_SESSION_EXISTS", /* 0x00ee */
  "NT_STATUS_INVALID_PARAMETER_1", /* 0x00ef */
  "NT_STATUS_INVALID_PARAMETER_2", /* 0x00f0 */
  "NT_STATUS_INVALID_PARAMETER_3", /* 0x00f1 */
  "NT_STATUS_INVALID_PARAMETER_4", /* 0x00f2 */
  "NT_STATUS_INVALID_PARAMETER_5", /* 0x00f3 */
  "NT_STATUS_INVALID_PARAMETER_6", /* 0x00f4 */
  "NT_STATUS_INVALID_PARAMETER_7", /* 0x00f5 */
  "NT_STATUS_INVALID_PARAMETER_8", /* 0x00f6 */
  "NT_STATUS_INVALID_PARAMETER_9", /* 0x00f7 */
  "NT_STATUS_INVALID_PARAMETER_10", /* 0x00f8 */
  "NT_STATUS_INVALID_PARAMETER_11", /* 0x00f9 */
  "NT_STATUS_INVALID_PARAMETER_12", /* 0x00fa */
  "NT_STATUS_REDIRECTOR_NOT_STARTED", /* 0x00fb */
  "NT_STATUS_REDIRECTOR_STARTED", /* 0x00fc */
  "NT_STATUS_STACK_OVERFLOW", /* 0x00fd */
  "NT_STATUS_NO_SUCH_PACKAGE", /* 0x00fe */
  "NT_STATUS_BAD_FUNCTION_TABLE", /* 0x00ff */
  "NT_STATUS_DIRECTORY_NOT_EMPTY", /* 0x0101 */
  "NT_STATUS_FILE_CORRUPT_ERROR", /* 0x0102 */
  "NT_STATUS_NOT_A_DIRECTORY", /* 0x0103 */
  "NT_STATUS_BAD_LOGON_SESSION_STATE", /* 0x0104 */
  "NT_STATUS_LOGON_SESSION_COLLISION", /* 0x0105 */
  "NT_STATUS_NAME_TOO_LONG", /* 0x0106 */
  "NT_STATUS_FILES_OPEN", /* 0x0107 */
  "NT_STATUS_CONNECTION_IN_USE", /* 0x0108 */
  "NT_STATUS_MESSAGE_NOT_FOUND", /* 0x0109 */
  "NT_STATUS_PROCESS_IS_TERMINATING", /* 0x010a */
  "NT_STATUS_INVALID_LOGON_TYPE", /* 0x010b */
  "NT_STATUS_NO_GUID_TRANSLATION", /* 0x010c */
  "NT_STATUS_CANNOT_IMPERSONATE", /* 0x010d */
  "NT_STATUS_IMAGE_ALREADY_LOADED", /* 0x010e */
  "NT_STATUS_ABIOS_NOT_PRESENT", /* 0x010f */
  "NT_STATUS_ABIOS_LID_NOT_EXIST", /* 0x0110 */
  "NT_STATUS_ABIOS_LID_ALREADY_OWNED", /* 0x0111 */
  "NT_STATUS_ABIOS_NOT_LID_OWNER", /* 0x0112 */
  "NT_STATUS_ABIOS_INVALID_COMMAND", /* 0x0113 */
  "NT_STATUS_ABIOS_INVALID_LID", /* 0x0114 */
  "NT_STATUS_ABIOS_SELECTOR_NOT_AVAILABLE", /* 0x0115 */
  "NT_STATUS_ABIOS_INVALID_SELECTOR", /* 0x0116 */
  "NT_STATUS_NO_LDT", /* 0x0117 */
  "NT_STATUS_INVALID_LDT_SIZE", /* 0x0118 */
  "NT_STATUS_INVALID_LDT_OFFSET", /* 0x0119 */
  "NT_STATUS_INVALID_LDT_DESCRIPTOR", /* 0x011a */
  "NT_STATUS_INVALID_IMAGE_NE_FORMAT", /* 0x011b */
  "NT_STATUS_RXACT_INVALID_STATE", /* 0x011c */
  "NT_STATUS_RXACT_COMMIT_FAILURE", /* 0x011d */
  "NT_STATUS_MAPPED_FILE_SIZE_ZERO", /* 0x011e */
  "NT_STATUS_TOO_MANY_OPENED_FILES", /* 0x011f */
  "NT_STATUS_CANCELLED", /* 0x0120 */
  "NT_STATUS_CANNOT_DELETE", /* 0x0121 */
  "NT_STATUS_INVALID_COMPUTER_NAME", /* 0x0122 */
  "NT_STATUS_FILE_DELETED", /* 0x0123 */
  "NT_STATUS_SPECIAL_ACCOUNT", /* 0x0124 */
  "NT_STATUS_SPECIAL_GROUP", /* 0x0125 */
  "NT_STATUS_SPECIAL_USER", /* 0x0126 */
  "NT_STATUS_MEMBERS_PRIMARY_GROUP", /* 0x0127 */
  "NT_STATUS_FILE_CLOSED", /* 0x0128 */
  "NT_STATUS_TOO_MANY_THREADS", /* 0x0129 */
  "NT_STATUS_THREAD_NOT_IN_PROCESS", /* 0x012a */
  "NT_STATUS_TOKEN_ALREADY_IN_USE", /* 0x012b */
  "NT_STATUS_PAGEFILE_QUOTA_EXCEEDED", /* 0x012c */
  "NT_STATUS_COMMITMENT_LIMIT", /* 0x012d */
  "NT_STATUS_INVALID_IMAGE_LE_FORMAT", /* 0x012e */
  "NT_STATUS_INVALID_IMAGE_NOT_MZ", /* 0x012f */
  "NT_STATUS_INVALID_IMAGE_PROTECT", /* 0x0130 */
  "NT_STATUS_INVALID_IMAGE_WIN_16", /* 0x0131 */
  "NT_STATUS_LOGON_SERVER_CONFLICT", /* 0x0132 */
  "NT_STATUS_TIME_DIFFERENCE_AT_DC", /* 0x0133 */
  "NT_STATUS_SYNCHRONIZATION_REQUIRED", /* 0x0134 */
  "NT_STATUS_DLL_NOT_FOUND", /* 0x0135 */
  "NT_STATUS_OPEN_FAILED", /* 0x0136 */
  "NT_STATUS_IO_PRIVILEGE_FAILED", /* 0x0137 */
  "NT_STATUS_ORDINAL_NOT_FOUND", /* 0x0138 */
  "NT_STATUS_ENTRYPOINT_NOT_FOUND", /* 0x0139 */
  "NT_STATUS_CONTROL_C_EXIT", /* 0x013a */
  "NT_STATUS_LOCAL_DISCONNECT", /* 0x013b */
  "NT_STATUS_REMOTE_DISCONNECT", /* 0x013c */
  "NT_STATUS_REMOTE_RESOURCES", /* 0x013d */
  "NT_STATUS_LINK_FAILED", /* 0x013e */
  "NT_STATUS_LINK_TIMEOUT", /* 0x013f */
  "NT_STATUS_INVALID_CONNECTION", /* 0x0140 */
  "NT_STATUS_INVALID_ADDRESS", /* 0x0141 */
  "NT_STATUS_DLL_INIT_FAILED", /* 0x0142 */
  "NT_STATUS_MISSING_SYSTEMFILE", /* 0x0143 */
  "NT_STATUS_UNHANDLED_EXCEPTION", /* 0x0144 */
  "NT_STATUS_APP_INIT_FAILURE", /* 0x0145 */
  "NT_STATUS_PAGEFILE_CREATE_FAILED", /* 0x0146 */
  "NT_STATUS_NO_PAGEFILE", /* 0x0147 */
  "NT_STATUS_INVALID_LEVEL", /* 0x0148 */
  "NT_STATUS_WRONG_PASSWORD_CORE", /* 0x0149 */
  "NT_STATUS_ILLEGAL_FLOAT_CONTEXT", /* 0x014a */
  "NT_STATUS_PIPE_BROKEN", /* 0x014b */
  "NT_STATUS_REGISTRY_CORRUPT", /* 0x014c */
  "NT_STATUS_REGISTRY_IO_FAILED", /* 0x014d */
  "NT_STATUS_NO_EVENT_PAIR", /* 0x014e */
  "NT_STATUS_UNRECOGNIZED_VOLUME", /* 0x014f */
  "NT_STATUS_SERIAL_NO_DEVICE_INITED", /* 0x0150 */
  "NT_STATUS_NO_SUCH_ALIAS", /* 0x0151 */
  "NT_STATUS_MEMBER_NOT_IN_ALIAS", /* 0x0152 */
  "NT_STATUS_MEMBER_IN_ALIAS", /* 0x0153 */
  "NT_STATUS_ALIAS_EXISTS", /* 0x0154 */
  "NT_STATUS_LOGON_NOT_GRANTED", /* 0x0155 */
  "NT_STATUS_TOO_MANY_SECRETS", /* 0x0156 */
  "NT_STATUS_SECRET_TOO_LONG", /* 0x0157 */
  "NT_STATUS_INTERNAL_DB_ERROR", /* 0x0158 */
  "NT_STATUS_FULLSCREEN_MODE", /* 0x0159 */
  "NT_STATUS_TOO_MANY_CONTEXT_IDS", /* 0x015a */
  "NT_STATUS_LOGON_TYPE_NOT_GRANTED", /* 0x015b */
  "NT_STATUS_NOT_REGISTRY_FILE", /* 0x015c */
  "NT_STATUS_NT_CROSS_ENCRYPTION_REQUIRED", /* 0x015d */
  "NT_STATUS_DOMAIN_CTRLR_CONFIG_ERROR", /* 0x015e */
  "NT_STATUS_FT_MISSING_MEMBER", /* 0x015f */
  "NT_STATUS_ILL_FORMED_SERVICE_ENTRY", /* 0x0160 */
  "NT_STATUS_ILLEGAL_CHARACTER", /* 0x0161 */
  "NT_STATUS_UNMAPPABLE_CHARACTER", /* 0x0162 */
  "NT_STATUS_UNDEFINED_CHARACTER", /* 0x0163 */
  "NT_STATUS_FLOPPY_VOLUME", /* 0x0164 */
  "NT_STATUS_FLOPPY_ID_MARK_NOT_FOUND", /* 0x0165 */
  "NT_STATUS_FLOPPY_WRONG_CYLINDER", /* 0x0166 */
  "NT_STATUS_FLOPPY_UNKNOWN_ERROR", /* 0x0167 */
  "NT_STATUS_FLOPPY_BAD_REGISTERS", /* 0x0168 */
  "NT_STATUS_DISK_RECALIBRATE_FAILED", /* 0x0169 */
  "NT_STATUS_DISK_OPERATION_FAILED", /* 0x016a */
  "NT_STATUS_DISK_RESET_FAILED", /* 0x016b */
  "NT_STATUS_SHARED_IRQ_BUSY", /* 0x016c */
  "NT_STATUS_FT_ORPHANING", /* 0x016d */
  "NT_STATUS_PARTITION_FAILURE", /* 0x0172 */
  "NT_STATUS_INVALID_BLOCK_LENGTH", /* 0x0173 */
  "NT_STATUS_DEVICE_NOT_PARTITIONED", /* 0x0174 */
  "NT_STATUS_UNABLE_TO_LOCK_MEDIA", /* 0x0175 */
  "NT_STATUS_UNABLE_TO_UNLOAD_MEDIA", /* 0x0176 */
  "NT_STATUS_EOM_OVERFLOW", /* 0x0177 */
  "NT_STATUS_NO_MEDIA", /* 0x0178 */
  "NT_STATUS_NO_SUCH_MEMBER", /* 0x017a */
  "NT_STATUS_INVALID_MEMBER", /* 0x017b */
  "NT_STATUS_KEY_DELETED", /* 0x017c */
  "NT_STATUS_NO_LOG_SPACE", /* 0x017d */
  "NT_STATUS_TOO_MANY_SIDS", /* 0x017e */
  "NT_STATUS_LM_CROSS_ENCRYPTION_REQUIRED", /* 0x017f */
  "NT_STATUS_KEY_HAS_CHILDREN", /* 0x0180 */
  "NT_STATUS_CHILD_MUST_BE_VOLATILE", /* 0x0181 */
  "NT_STATUS_DEVICE_CONFIGURATION_ERROR", /* 0x0182 */
  "NT_STATUS_DRIVER_INTERNAL_ERROR", /* 0x0183 */
  "NT_STATUS_INVALID_DEVICE_STATE", /* 0x0184 */
  "NT_STATUS_IO_DEVICE_ERROR", /* 0x0185 */
  "NT_STATUS_DEVICE_PROTOCOL_ERROR", /* 0x0186 */
  "NT_STATUS_BACKUP_CONTROLLER", /* 0x0187 */
  "NT_STATUS_LOG_FILE_FULL", /* 0x0188 */
  "NT_STATUS_TOO_LATE", /* 0x0189 */
  "NT_STATUS_NO_TRUST_LSA_SECRET", /* 0x018a */
  "NT_STATUS_NO_TRUST_SAM_ACCOUNT", /* 0x018b */
  "NT_STATUS_TRUSTED_DOMAIN_FAILURE", /* 0x018c */
  "NT_STATUS_TRUSTED_RELATIONSHIP_FAILURE", /* 0x018d */
  "NT_STATUS_EVENTLOG_FILE_CORRUPT", /* 0x018e */
  "NT_STATUS_EVENTLOG_CANT_START", /* 0x018f */
  "NT_STATUS_TRUST_FAILURE", /* 0x0190 */
  "NT_STATUS_MUTANT_LIMIT_EXCEEDED", /* 0x0191 */
  "NT_STATUS_NETLOGON_NOT_STARTED", /* 0x0192 */
  "NT_STATUS_ACCOUNT_EXPIRED", /* 0x0193 */
  "NT_STATUS_POSSIBLE_DEADLOCK", /* 0x0194 */
  "NT_STATUS_NETWORK_CREDENTIAL_CONFLICT", /* 0x0195 */
  "NT_STATUS_REMOTE_SESSION_LIMIT", /* 0x0196 */
  "NT_STATUS_EVENTLOG_FILE_CHANGED", /* 0x0197 */
  "NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT", /* 0x0198 */
  "NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT", /* 0x0199 */
  "NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT", /* 0x019a */
  "NT_STATUS_DOMAIN_TRUST_INCONSISTENT", /* 0x019b */
  "NT_STATUS_FS_DRIVER_REQUIRED", /* 0x019c */
  "NT_STATUS_NO_USER_SESSION_KEY", /* 0x0202 */
  "NT_STATUS_USER_SESSION_DELETED", /* 0x0203 */
  "NT_STATUS_RESOURCE_LANG_NOT_FOUND", /* 0x0204 */
  "NT_STATUS_INSUFF_SERVER_RESOURCES", /* 0x0205 */
  "NT_STATUS_INVALID_BUFFER_SIZE", /* 0x0206 */
  "NT_STATUS_INVALID_ADDRESS_COMPONENT", /* 0x0207 */
  "NT_STATUS_INVALID_ADDRESS_WILDCARD", /* 0x0208 */
  "NT_STATUS_TOO_MANY_ADDRESSES", /* 0x0209 */
  "NT_STATUS_ADDRESS_ALREADY_EXISTS", /* 0x020a */
  "NT_STATUS_ADDRESS_CLOSED", /* 0x020b */
  "NT_STATUS_CONNECTION_DISCONNECTED", /* 0x020c */
  "NT_STATUS_CONNECTION_RESET", /* 0x020d */
  "NT_STATUS_TOO_MANY_NODES", /* 0x020e */
  "NT_STATUS_TRANSACTION_ABORTED", /* 0x020f */
  "NT_STATUS_TRANSACTION_TIMED_OUT", /* 0x0210 */
  "NT_STATUS_TRANSACTION_NO_RELEASE", /* 0x0211 */
  "NT_STATUS_TRANSACTION_NO_MATCH", /* 0x0212 */
  "NT_STATUS_TRANSACTION_RESPONDED", /* 0x0213 */
  "NT_STATUS_TRANSACTION_INVALID_ID", /* 0x0214 */
  "NT_STATUS_TRANSACTION_INVALID_TYPE", /* 0x0215 */
  "NT_STATUS_NOT_SERVER_SESSION", /* 0x0216 */
  "NT_STATUS_NOT_CLIENT_SESSION", /* 0x0217 */
  "NT_STATUS_CANNOT_LOAD_REGISTRY_FILE", /* 0x0218 */
  "NT_STATUS_DEBUG_ATTACH_FAILED", /* 0x0219 */
  "NT_STATUS_SYSTEM_PROCESS_TERMINATED", /* 0x021a */
  "NT_STATUS_DATA_NOT_ACCEPTED", /* 0x021b */
  "NT_STATUS_NO_BROWSER_SERVERS_FOUND", /* 0x021c */
  "NT_STATUS_VDM_HARD_ERROR", /* 0x021d */
  "NT_STATUS_DRIVER_CANCEL_TIMEOUT", /* 0x021e */
  "NT_STATUS_REPLY_MESSAGE_MISMATCH", /* 0x021f */
  "NT_STATUS_MAPPED_ALIGNMENT", /* 0x0220 */
  "NT_STATUS_IMAGE_CHECKSUM_MISMATCH", /* 0x0221 */
  "NT_STATUS_LOST_WRITEBEHIND_DATA", /* 0x0222 */
  "NT_STATUS_CLIENT_SERVER_PARAMETERS_INVALID", /* 0x0223 */
  "NT_STATUS_PASSWORD_MUST_CHANGE", /* 0x0224 */
  "NT_STATUS_NOT_FOUND", /* 0x0225 */
  "NT_STATUS_NOT_TINY_STREAM", /* 0x0226 */
  "NT_STATUS_RECOVERY_FAILURE", /* 0x0227 */
  "NT_STATUS_STACK_OVERFLOW_READ", /* 0x0228 */
  "NT_STATUS_FAIL_CHECK", /* 0x0229 */
  "NT_STATUS_DUPLICATE_OBJECTID", /* 0x022a */
  "NT_STATUS_OBJECTID_EXISTS", /* 0x022b */
  "NT_STATUS_CONVERT_TO_LARGE", /* 0x022c */
  "NT_STATUS_RETRY", /* 0x022d */
  "NT_STATUS_FOUND_OUT_OF_SCOPE", /* 0x022e */
  "NT_STATUS_ALLOCATE_BUCKET", /* 0x022f */
  "NT_STATUS_PROPSET_NOT_FOUND", /* 0x0230 */
  "NT_STATUS_MARSHALL_OVERFLOW", /* 0x0231 */
  "NT_STATUS_INVALID_VARIANT", /* 0x0232 */
  "NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND", /* 0x0233 */
  "NT_STATUS_ACCOUNT_LOCKED_OUT", /* 0x0234 */
  "NT_STATUS_HANDLE_NOT_CLOSABLE", /* 0x0235 */
  "NT_STATUS_CONNECTION_REFUSED", /* 0x0236 */
  "NT_STATUS_GRACEFUL_DISCONNECT", /* 0x0237 */
  "NT_STATUS_ADDRESS_ALREADY_ASSOCIATED", /* 0x0238 */
  "NT_STATUS_ADDRESS_NOT_ASSOCIATED", /* 0x0239 */
  "NT_STATUS_CONNECTION_INVALID", /* 0x023a */
  "NT_STATUS_CONNECTION_ACTIVE", /* 0x023b */
  "NT_STATUS_NETWORK_UNREACHABLE", /* 0x023c */
  "NT_STATUS_HOST_UNREACHABLE", /* 0x023d */
  "NT_STATUS_PROTOCOL_UNREACHABLE", /* 0x023e */
  "NT_STATUS_PORT_UNREACHABLE", /* 0x023f */
  "NT_STATUS_REQUEST_ABORTED", /* 0x0240 */
  "NT_STATUS_CONNECTION_ABORTED", /* 0x0241 */
  "NT_STATUS_BAD_COMPRESSION_BUFFER", /* 0x0242 */
  "NT_STATUS_USER_MAPPED_FILE", /* 0x0243 */
  "NT_STATUS_AUDIT_FAILED", /* 0x0244 */
  "NT_STATUS_TIMER_RESOLUTION_NOT_SET", /* 0x0245 */
  "NT_STATUS_CONNECTION_COUNT_LIMIT", /* 0x0246 */
  "NT_STATUS_LOGIN_TIME_RESTRICTION", /* 0x0247 */
  "NT_STATUS_LOGIN_WKSTA_RESTRICTION", /* 0x0248 */
  "NT_STATUS_IMAGE_MP_UP_MISMATCH", /* 0x0249 */
  "NT_STATUS_INSUFFICIENT_LOGON_INFO", /* 0x0250 */
  "NT_STATUS_BAD_DLL_ENTRYPOINT", /* 0x0251 */
  "NT_STATUS_BAD_SERVICE_ENTRYPOINT", /* 0x0252 */
  "NT_STATUS_LPC_REPLY_LOST", /* 0x0253 */
  "NT_STATUS_IP_ADDRESS_CONFLICT1", /* 0x0254 */
  "NT_STATUS_IP_ADDRESS_CONFLICT2", /* 0x0255 */
  "NT_STATUS_REGISTRY_QUOTA_LIMIT", /* 0x0256 */
  "NT_STATUS_PATH_NOT_COVERED", /* 0x0257 */
  "NT_STATUS_NO_CALLBACK_ACTIVE", /* 0x0258 */
  "NT_STATUS_LICENSE_QUOTA_EXCEEDED", /* 0x0259 */
  "NT_STATUS_PWD_TOO_SHORT", /* 0x025a */
  "NT_STATUS_PWD_TOO_RECENT", /* 0x025b */
  "NT_STATUS_PWD_HISTORY_CONFLICT", /* 0x025c */
  "NT_STATUS_PLUGPLAY_NO_DEVICE", /* 0x025e */
  "NT_STATUS_UNSUPPORTED_COMPRESSION", /* 0x025f */
  "NT_STATUS_INVALID_HW_PROFILE", /* 0x0260 */
  "NT_STATUS_INVALID_PLUGPLAY_DEVICE_PATH", /* 0x0261 */
  "NT_STATUS_DRIVER_ORDINAL_NOT_FOUND", /* 0x0262 */
  "NT_STATUS_DRIVER_ENTRYPOINT_NOT_FOUND", /* 0x0263 */
  "NT_STATUS_RESOURCE_NOT_OWNED", /* 0x0264 */
  "NT_STATUS_TOO_MANY_LINKS", /* 0x0265 */
  "NT_STATUS_QUOTA_LIST_INCONSISTENT", /* 0x0266 */
  "NT_STATUS_FILE_IS_OFFLINE" /* 0x0267 */
};

#define NT_STATUS(a) nt_status_to_string[a & 0x3FFFFFFF]

struct Packet
{
  size_t frame;
  
  uint8_t magic[4];

  bool valid_smb() {
    return !memcmp(smb_hdr.magic, magic, 4);
  }

  Packet(const uint8_t *data, size_t size) : 
    ip_hdr(data + 14, size - 14),
    tcp_hdr(data + 14 + ip_hdr.header_length,
	    size - 14 - ip_hdr.header_length),
    smb_hdr(data + 14 + ip_hdr.header_length + tcp_hdr.length,
	    size - 14 - ip_hdr.header_length - tcp_hdr.length)
  {
    const uint8_t da[] = { 0xFF, 'S', 'M', 'B' };

    memcpy(magic, da, sizeof(da));

    if (valid_smb()) {
      size_t len = 14 + ip_hdr.header_length + tcp_hdr.length + 36;

      switch (smb_hdr.command) {
      case NtCreateAndXRequest::COMMAND:
	if (smb_hdr.flags & 0x80) {
	  ntcreate_resp = NtCreateAndXResponse(data+len, size - len);
	} else {
	  ntcreate_req = NtCreateAndXRequest(data + len, size - len);
	}
	break;
      case ReadAndXRequest::COMMAND:
	if (!(smb_hdr.flags & 0x80)) {
	  read_req = ReadAndXRequest(data + len, size - len);
	}
	break;
      case WriteAndXRequest::COMMAND:
	if (!(smb_hdr.flags & 0x80)) {
	  write_req = WriteAndXRequest(data + len, size - len);
	}
	break;
      case CloseRequest::COMMAND:
	if (!(smb_hdr.flags & 0x80)) {
	  close_req = CloseRequest(data + len, size - len);
	}
	break;
      }
    }
  }

  ip ip_hdr;
  tcp tcp_hdr;
  smb smb_hdr;

  NtCreateAndXRequest ntcreate_req;
  NtCreateAndXResponse ntcreate_resp;
  ReadAndXRequest read_req;
  WriteAndXRequest write_req;
  CloseRequest close_req;
};

int main(int argc, char **argv)
{
  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *cap;
  struct pcap_pkthdr *pkt_hdr;
  const uint8_t *data;

  static struct option long_opts[] = {
    { "show-files", 0, 0, 's' },
    { "drop-incomplete-sessions", 0, 0, 'i' },
    { 0,            0, 0, 0   }
  };
  const char *short_opts = "si";
  int opt_ind;
  char ch;
  int show_files = 0;
  int drop_incomplete_sessions = 0;

  while ((ch = getopt_long(argc,argv,short_opts,long_opts,&opt_ind)) != -1) {
    switch (ch) {
    case 's':
      show_files = 1;
      break;
    case 'i':
      drop_incomplete_sessions = 1;
      break;
    default:
      break;
    }
  }

  if ((argc - optind) != 1) {
    std::cout << "Usage: " << argv[0] << " [OPTIONS] FILE" << std::endl;
    exit(1);
  }

  cap = pcap_open_offline(argv[optind], errbuf);

  if (!cap) {
    std::cout << "pcap_open_offline(" << argv[optind] << "): " << errbuf
	      << std::endl;
    exit(1);
  }

  std::vector<Packet> packets;
  size_t frame = 0;
  std::set<uint16_t> current_fids;

  while (1 == pcap_next_ex(cap, &pkt_hdr, &data)) {
    Packet packet(data, pkt_hdr->len);

    ++frame;

    if (packet.valid_smb()) {
      packet.frame = frame;
      packets.push_back(packet);
    }
  }

  pcap_close(cap);

  for (std::vector<Packet>::iterator i = packets.begin();
       i != packets.end(); ++i) {
    if (!(i->smb_hdr.flags & 0x80)) {
      /* we have a request */
      std::vector<Packet>::iterator j;

      /* look ahead for the response */
      for (j = i; j != packets.end(); ++j) {
	if (j->smb_hdr.flags & 0x80 && // response
	    j->smb_hdr.command == i->smb_hdr.command &&
	    j->smb_hdr.tid == i->smb_hdr.tid &&
	    j->smb_hdr.pid == i->smb_hdr.pid &&
	    j->smb_hdr.uid == i->smb_hdr.uid &&
	    j->smb_hdr.mid == i->smb_hdr.mid) {
	  break;
	}
      }

      /* no response?  guess we can't display this command */
      if (j == packets.end()) continue;

      size_t len;

      switch (i->smb_hdr.command) {
      case NtCreateAndXRequest::COMMAND:
	std::cout << "NTCreateX \"" << i->ntcreate_req.file_name << "\" "
		  << i->ntcreate_req.create_options << " "
		  << i->ntcreate_req.disposition << " "
		  << j->ntcreate_resp.fid << " "
		  << NT_STATUS(j->smb_hdr.nt_status) << std::endl;
	current_fids.insert(j->ntcreate_resp.fid);
	break;
      case ReadAndXRequest::COMMAND:
	len = i->read_req.max_count_high * 64 * 1024 +
	  i->read_req.max_count_low;

	if (!drop_incomplete_sessions || current_fids.count(i->read_req.fid)) {
	  std::cout << "ReadX " << i->read_req.fid << " "
		    << i->read_req.offset << " "
		    << len << " " << len << " "
		    << NT_STATUS(j->smb_hdr.nt_status) << std::endl;
	}
	break;
      case WriteAndXRequest::COMMAND:
	len = i->write_req.data_length_hi * 64 * 1024 +
	  i->write_req.data_length_lo;

	if (!drop_incomplete_sessions||current_fids.count(i->write_req.fid)) {
	  std::cout << "WriteX " << i->write_req.fid << " "
		    << i->write_req.offset << " "
		    << len << " " << len << " "
		    << NT_STATUS(j->smb_hdr.nt_status) << std::endl;
	}
	break;
      case CloseRequest::COMMAND:
	if (!drop_incomplete_sessions||current_fids.count(i->close_req.fid)) {
	  std::cout << "Close " << i->close_req.fid << " "
		    << NT_STATUS(j->smb_hdr.nt_status) << std::endl;
	}
	current_fids.erase(i->close_req.fid);
	break;
      }
    }
  }

  return 0;
}
