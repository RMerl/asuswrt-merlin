/*
 * WPS TLV
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpstlvbase.h 291563 2011-10-24 11:07:22Z $
 */
#ifndef __TLV__H
#define __TLV__H

#ifdef __cplusplus
extern "C" {
#endif

#include <portability.h>
#include <wpserror.h>

#ifndef UNDER_CE /* to be used for everything other than WinCE */
	#ifndef __unaligned
	#define __unaligned
	#endif
#endif /* ifndef UNDER_CE */


/* Declare TLV header as extern, since it will be defined elsewhere */
typedef struct {
	uint16 attributeType;
	uint16 dataLength;
} WpsTlvHdr;

/* Buffer class */
#define BUF_BLOCK_SIZE 256
#define BUF_ALLOC_MAGIC 0x12345678

typedef struct {
	uint8 *pBase, *pCurrent;
	uint32 m_bufferLength;
	uint32 m_currentLength;
	uint32 m_dataLength;
	bool m_allocated;
	uint32 magic;
} BufferObj;

/* allocate object + data buffer and intialize internals */
BufferObj *buffobj_new(void);

/*
 * initialize as a deserializing buffer. In that case
 * we don't own the data buffer (allocated = false).
 */
void buffobj_dserial(BufferObj * b, uint8 *ptr, uint32 length);

/* create a buffer obj around an existing buffer (allocated = false) */
BufferObj * buffobj_setbuf(uint8 *buf, int len);
void buffobj_del(BufferObj *);
uint8 *buffobj_Advance(BufferObj *b, uint32 offset);
uint8 *buffobj_Pos(BufferObj *b);
uint32 buffobj_Length(BufferObj *b);
uint32 buffobj_Remaining(BufferObj *b);
uint8 *buffobj_Append(BufferObj *b, uint32 length, uint8 *pBuff);
uint8 *buffobj_GetBuf(BufferObj *b);
uint8 *buffobj_Set(BufferObj *b, uint8 *pos);
uint16 buffobj_NextType(BufferObj *b);
uint8 *buffobj_Reset(BufferObj *b);
uint8 *buffobj_RewindLength(BufferObj *b, uint32 length);
uint8 *buffobj_Rewind(BufferObj *b);
int buffobj_lock(BufferObj *b);
int buffobj_unlock(BufferObj *b);

/* WSC 2.0 */
uint8 buffobj_NextSubId(BufferObj *b);


/* under m_xxx in mbuf.h */

/* TLV Base class */
typedef struct {
	void *next;
	uint16 m_type;
	uint16 m_len;
	uint8 *m_pos;
} tlvbase_s;

typedef struct {
	tlvbase_s tlvbase;
	uint8 m_data;
} TlvObj_uint8;

typedef struct {
	tlvbase_s tlvbase;
	uint16 m_data;
} TlvObj_uint16;

typedef struct {
	tlvbase_s tlvbase;
	uint32 m_data;
} TlvObj_uint32;

typedef struct {
	tlvbase_s tlvbase;
	uint8 *m_data;
	bool m_allocated;
} TlvObj_ptru;

typedef struct {
	tlvbase_s tlvbase;
	char *m_data;
	bool m_allocated;
} TlvObj_ptr;


/* Vendor Extension Subelement */
typedef struct {
	uint8 subelementId;
	uint8 subelementLen;
} WpsSubTlvHdr;

typedef struct {
	uint8 m_id;
	uint8 m_len;
	uint8 *m_pos;
} subtlvbase_s;

typedef struct {
	subtlvbase_s subtlvbase;
	uint8 m_data;
} SubTlvObj_uint8;

typedef struct {
	subtlvbase_s subtlvbase;
	uint16 m_data;
} SubTlvObj_uint16;

typedef struct {
	subtlvbase_s subtlvbase;
	uint32 m_data;
} SubTlvObj_uint32;

typedef struct {
	subtlvbase_s subtlvbase;
	uint8 *m_data;
	bool m_allocated;
} SubTlvObj_ptru;

typedef struct {
	subtlvbase_s subtlvbase;
	char *m_data;
	bool m_allocated;
} SubTlvObj_ptr;

#ifdef  __cplusplus
}
#endif

#endif /* WPS_TLV_H */
