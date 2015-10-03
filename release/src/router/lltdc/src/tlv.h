/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef TLV_H
#define TLV_H

typedef struct {
    etheraddr_t MACaddr;
    uint16_t    MOR;
    uint8_t     PHYtype;
} assn_table_entry_t;

#define SIZEOF_PKT_ASSN_TABLE_ENTRY 10          // bytes, when packed on the wire...

typedef struct {
    assn_table_entry_t    *table;
    uint                   assn_cnt;            // number of entries in the allocated table
    struct timeval         collection_time;     // timestamp when table was generated
} assns_t;

typedef struct {
    size_t       sz_iconfile;
    int          fd_icon;
} icon_file_t;

typedef struct {
    size_t       sz_iconfile;
    int          fd_icon;
} lg_icon_t;

typedef enum {
    Bridge_Component = 0,
    RadioBand_Component,
    Switch_Component
} component_t;

typedef struct {
    uint16_t    MOR;
    uint8_t     PHYtype;
    uint8_t     mode;
    etheraddr_t BSSID;
} radio_t;

typedef struct {
    uint8_t     version;
    uint8_t     bridge_behavior;        // 0xFF => no bridge
    uint32_t    link_speed;             // 0xFFFFFFFF => no switch
    int         radio_cnt;
    radio_t    *radios;
} comptbl_t;


typedef enum {
    Access_invalid,	// placeholders - will never be init'd or written
    Access_unset,	// for TLVs that will become "Access_static" once they are init'd
    Access_static,	// TLVs that only require one initialization, and are const thereafter
    Access_dynamic	// TLVs that require freshly-gathered data each time they are written
} tlv_access_t;

/* Struct contains storage for the actual tlv data (or a pointer to that storage, if data has a
 * variable length) for each tlv.
 * Each TLVDEF in this struct generates a uniquely named data element.
 */
typedef struct _tlv_info {
#define TLVDEF(_Ctype, _name, _repeat, _number, _access, _inHello) \
    _Ctype _name _repeat;
#include "tlvdef.h"
#undef TLVDEF
} tlv_info_t;

/* A "tlv_get_fn" creates an instance of the TLV's data, and puts that data (or a pointer to
 * the data itself, as required by the corresponding companion "tlv_write_fn" described next)
 * into the tlv_info_t structure's data field for this TLV-type. There will always be one unique
 * tlv_get_fn for each TLV, since all such data will be derived from a unique source.
 *
 * This "tlv_get_fn" will be called only by the function "tlv_write_info()". For each TLV marked
 * "Access_unset", tlv_write_info() will call the tlv_get_fn only once (the first time the data
 * value is to be written), and then mark that TLV as "Access_static". Each TLV marked "Access_dynamic"
 * will result in tlv_write_info() calling the tlv_get_fn each time the data is to be written by the
 * corresponding tlv_write_fn. TLVs marked "Access_static" will be written from data pre-loaded when it
 * was marked "Access_unset".
 * Those TLVs marked "Access_invalid" will never have their corresponding tlv_get_fn called (though
 * those tlv_get_fn's must exist, if only as a skeleton, in order to compile successfully);
 * likewise, TLVs marked "Access_invalid" will never have their tlv_write_fn called at all.
 */

#define TLV_GET_FAILED    0
#define TLV_GET_SUCCEEDED 1

typedef int (*tlv_get_fn)(void *data);

/* A "tlv_write_fn" writes a TLV ("Type", "Length", and "Value") for "data" to "buf" and
 * returns how many bytes of buf it consumed.  The TLV "Type" is "number", and the "Length" is
 * known automatically by the (often shared) writer routine for this TLV. The "Value" is found
 * using the "data" pointer in a way unique to each writer. "data" is an offset into the storage
 * defined by the tlv_info_t, which may be used to hold the actual TLV "Value", a pointer to
 * such a "Value", or in any other way that is meaningful to the convention established between
 * an individual tlv_get_fn and its associated tlv_write_fn (writer routine).
 * The flag "isHello" tells the writer what format to write in - Hello-style TLV (if TRUE) or
 * QueryLargeTLV style (if anything but TRUE). The flag "isLarge" is ignored unless isHello
 * is TRUE, when it determines whether to force the small TLV to report as LargeTLV.
 * Finally, the "offset" parameter is used to seek in largeTLVs that need it, in order to
 * return the correct segment of data, when the data extends over a greater span than fits
 * into one packet (the classic use for LargeTLVs). It is otherwise ignored, and may be NULL.
 *
 * The return value carries the number of bytes actually written to the buffer.   */

typedef int (*tlv_write_fn)(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset);

/* Struct to hold an offset into the tlv_info_t of the tlv's data, as well as access-modes and
 * pointers to get_fn's and write_fn's for each tlv. Because this struct is fixed-length, an array
 * of such structures can be indexed by TLV protocol number to read or write the tlv data. */
typedef struct {
    int	number;		 /* TLV protocol number (aka "Type") */
    int offset; 	 /* offset of (data or ptr) from start of tlv_info_t struct, in bytes */
    tlv_access_t access; /* access-mode indicator (invalid, unset, static, or dynamic */
    bool_t       inHello;/* TRUE if Hello is allowed to return it - QLTLV can ALWAYS return it. */
    tlv_get_fn   get;    /* suitable function to populate the tlv_info struct for the writer */
    tlv_write_fn write;  /* suitable function to write it to network buffer */
} tlv_desc_t;

extern tlv_desc_t  Tlvs[];

/* Serialise "info" into a Hello frame, at position "buf" where "bytes_free"
 * are available.  Returns the number of bytes consumed. */
extern int tlv_write_info(uint8_t *buf, int bytes_free);

/* Serialize a single tlv into either Hello format (isHello=TRUE) or QueryLargeTlvResp format.
 * Returns the number of bytes consumed. */
extern int tlv_write_tlv(tlv_desc_t *tlv, uint8_t *buf, int bytes_free, bool_t isHello, size_t LtlvOffset);

#endif /* TLV_H */
