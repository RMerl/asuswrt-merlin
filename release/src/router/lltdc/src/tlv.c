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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "globals.h"

#include "tlv.h"

/************************* E X T E R N A L S   f o r   T L V _ G E T _ F N s ***************************/
#define TLVDEF(_type, _name, _repeat, _number, _access, _inHello) \
extern int get_ ## _name ();
#include "tlvdef.h"
#undef TLVDEF

static int
write_lg_icon_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    lg_icon_t* icon = (lg_icon_t*) data;
    int          num_read;
    off_t        curOffset;
    int          remaining;

    IF_TRACED(TRC_TLVINFO)
        printf("write-lg_icon: tlvnum:%d  isHello:%s  isLarge:%s  offset:" FMT_SIZET "\n",
               number, isHello==TRUE?"true":"false", isLarge==TRUE?"true":"false", offset);
    END_TRACE

    if (icon->fd_icon < 0)
    {
        DEBUG({printf("write-lg_icon: No icon FD, nothing written\n");})
	return 0;	// No jumbo icon was correctly declared.
    }

    if (isHello==TRUE)
    {
        /* Jumbo icon - always force to a large-TLV */
        /* write header in Hello's small-TLV format */
        *buf++ = (uint8_t)number;
        *buf   = (uint8_t)0;
        close(icon->fd_icon);
        icon->fd_icon = -1;
        return 2;
    }
    
    /* try to read as much as will fit in the buffer, allowing 2 bytes for the header */
    curOffset = lseek(icon->fd_icon, (off_t) offset, SEEK_SET);
    if (curOffset != (off_t)offset)
    {
        /* the seek on the icon file failed... whine and cleanup the buffer */
        warn("write_lg_icon_t: lseek to byte " FMT_SIZET " on the iconfile FAILED, errno: %s",
             offset, strerror(errno));
        close(icon->fd_icon);
        icon->fd_icon = -1;
        return 0;
    }
    num_read = read(icon->fd_icon, buf+2, bytes_free-2);
    if (num_read < 0)
    {
        /* the read of the icon failed... whine and cleanup the buffer */
        warn("write_lg_icon_t: read of the jumbo iconfile FAILED, errno: %s", strerror(errno));
        close(icon->fd_icon);
        icon->fd_icon = -1;
        return 0;
    }
    /* adjust the number, length and/or the flag appropriately */
    /* write header in QueryLargeTlvResp large-TLV format */
    remaining = read(icon->fd_icon, buf, 1);	// Check for more...

    IF_TRACED(TRC_TLVINFO)
        printf("writing jumbo-tlv of %d bytes, with %s remaining\n",
                num_read+2, remaining<=0?"none":"some");
    END_TRACE

    if (remaining <= 0)
    {
        /* EOF or error - no more to read, so more -flag is 0 */
        /* replace: *(uint16_t*)buf = htons((uint16_t)num_read); with: */
        g_short_reorder_buffer = htons((uint16_t)num_read);
        memcpy(buf, &g_short_reorder_buffer, 2);
    } else {
        /* found at least one more byte... set the more-flag */
        /* replace: *(uint16_t*)buf = htons((uint16_t)(0x8000) | (uint16_t)num_read); with: */
        g_short_reorder_buffer = htons((uint16_t)(0x8000) | (uint16_t)num_read);
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    IF_TRACED(TRC_TLVINFO)
        printf("txbuf: %02X, %02X, %02X, %02X\n",buf[0],buf[1],buf[2],buf[3]);
    END_TRACE
    close(icon->fd_icon);
    icon->fd_icon = -1;
    return num_read + 2;
}


static int
write_icon_file_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    icon_file_t* icon = (icon_file_t*) data;
    int          num_read;
    off_t        curOffset;

    IF_TRACED(TRC_TLVINFO)
        printf("write-icon-file: tlvnum:%d  isHello:%s  isLarge:%s  offset:" FMT_SIZET "\n",
               number, isHello==TRUE?"true":"false", isLarge==TRUE?"true":"false", offset);
    END_TRACE

    if (icon->fd_icon < 0)
    {
        DEBUG({printf("write-icon-file: No icon FD, nothing written\n");})
	return 0;	// No iconfile was correctly declared.
    }

    if (isHello==TRUE)
    {
        if ((icon->sz_iconfile > (size_t)255) || (bytes_free < 2 + (int)icon->sz_iconfile) || isLarge==TRUE)
        {
            IF_TRACED(TRC_TLVINFO)
                printf("write_icon_file: not enuff room - bytes_free=%d, and icon_sz=" FMT_SIZET "\n",
                       bytes_free, icon->sz_iconfile);
                printf("writing small-tlv of 0 bytes to notify of large-tlv value\n");
            END_TRACE
            /* Not enough room - force to a large-TLV */
            /* write header in Hello's small-TLV format */
            *buf++ = (uint8_t)number;
            *buf   = (uint8_t)0;
            close(icon->fd_icon);
            icon->fd_icon = -1;
            return 2;
        }
    }

    /* try to read as much as will fit in the buffer, allowing 2 bytes for the header, small or large */
    curOffset = lseek(icon->fd_icon, (off_t) offset, SEEK_SET);
    if (curOffset != (off_t)offset)
    {
        /* the seek on the icon file failed... whine and cleanup the buffer */
        warn("write_icon_file_t: lseek to byte " FMT_SIZET " on the iconfile FAILED, errno: %s",
             offset, strerror(errno));
        close(icon->fd_icon);
        icon->fd_icon = -1;
        return 0;
    }
    num_read = read(icon->fd_icon, buf+2, bytes_free-2);
    if (num_read < 0)
    {
        /* the read of the icon failed... whine and cleanup the buffer */
        warn("write_icon_file_t: read of the iconfile FAILED, errno: %s", strerror(errno));
        close(icon->fd_icon);
        icon->fd_icon = -1;
        return 0;
    }
    /* adjust the number, length and/or the flag appropriately */
    if (isHello==TRUE)
    {
        /* write header in Hello's small-TLV format */
        IF_TRACED(TRC_TLVINFO)
            printf("writing small-tlv of %d bytes\n",num_read+2);
        END_TRACE
        *buf++ = (uint8_t)number;
        *buf-- = (uint8_t)num_read;
    } else {
        /* write header in QueryLargeTlvResp large-TLV format */
        int remaining = read(icon->fd_icon, buf, 1);	// Check for more...

        IF_TRACED(TRC_TLVINFO)
            printf("writing large-tlv of %d bytes, with %s remaining\n",
                    num_read+2, remaining<=0?"none":"some");
        END_TRACE

        if (remaining <= 0)
        {
            /* EOF or error - no more to read, so more -flag is 0 */
            /* replace: *(uint16_t*)buf = htons((uint16_t)num_read); with: */
            g_short_reorder_buffer = htons((uint16_t)num_read);
            memcpy(buf, &g_short_reorder_buffer, 2);
        } else {
            /* found at least one more byte... set the more-flag */
            /* replace: *(uint16_t*)buf = htons((uint16_t)(0x8000) | (uint16_t)num_read); with: */
            g_short_reorder_buffer = htons((uint16_t)(0x8000) | (uint16_t)num_read);
            memcpy(buf, &g_short_reorder_buffer, 2);
        }
    }
    IF_TRACED(TRC_TLVINFO)
        printf("txbuf: %02X, %02X, %02X, %02X\n",buf[0],buf[1],buf[2],buf[3]);
    END_TRACE
    close(icon->fd_icon);
    icon->fd_icon = -1;
    return num_read + 2;
}


static int
write_etheraddr_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(etheraddr_t)))
	return 0;

    IF_TRACED(TRC_TLVINFO)
        dbgprintf("write_etheraddr_t(%s): addr=" ETHERADDR_FMT "\n", isLarge?"LTLV":"TLV",
                  ETHERADDR_PRINT(((etheraddr_t*)data)));
    END_TRACE

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(etheraddr_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(etheraddr_t)); with: */
        g_short_reorder_buffer = htons(sizeof(etheraddr_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    /* replace: *((etheraddr_t*)(buf+2)) = *(etheraddr_t*)data; with: */
    memcpy(buf+2, data, sizeof(etheraddr_t));
    return sizeof(etheraddr_t) + 2;
}

static int
write_uint8_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(uint8_t)))
	return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(uint8_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(etheraddr_t)); with: */
        g_short_reorder_buffer = htons(sizeof(uint8_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    *(buf+2) = *(uint8_t*)data;
    return sizeof(uint8_t) + 2;
}

static int
write_uint16_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(uint16_t)))
	return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(uint16_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(uint16_t)); with: */
        g_short_reorder_buffer = htons(sizeof(uint16_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    /* replace: *((uint16_t*)(buf+2)) = *(uint16_t*)data; with: */
    memcpy(buf+2, data, sizeof(uint16_t));
    return sizeof(uint16_t) + 2;
}

static int
write_uint32_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(uint32_t)))
	return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(uint32_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(uint32_t)); with: */
        g_short_reorder_buffer = htons(sizeof(uint32_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    /* replace: *((uint32_t*)(buf+2)) = *(uint32_t*)data; with: */
    memcpy(buf+2, data, sizeof(uint32_t));
    return sizeof(uint32_t) + 2;
}

static int
write_uint64_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(uint64_t)))
	return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(uint64_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(uint64_t)); with: */
        g_short_reorder_buffer = htons(sizeof(uint64_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    /* replace: *((uint64_t*)(buf+2)) = *(uint64_t*)data; with: */
    memcpy(buf+2, data, sizeof(uint64_t));
    return sizeof(uint64_t) + 2;

}

static int
write_ipv4addr_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    return write_uint32_t(number, data, buf, bytes_free, isHello, isLarge, offset);
}

static int
write_ipv6addr_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    if (bytes_free < (int)(2 + sizeof(ipv6addr_t)))
        return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = sizeof(ipv6addr_t);
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(sizeof(ipv6addr_t)); with: */
        g_short_reorder_buffer = htons(sizeof(ipv6addr_t));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    /* replace: *((ipv6addr_t*)(buf+2)) = *(ipv6addr_t*)data; with: */
    memcpy(buf+2, data, sizeof(ipv6addr_t));
    return sizeof(ipv6addr_t) + 2;
}

static int
write_ucs2char_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    int         bytes_needed;
    uint16_t   *s = data;

    /* look for the double-\0 which marks the end of the UCS-2 string */
    bytes_needed = 0;
    while (*s != 0)
    {
	bytes_needed+=2;
	s++;
    }

    /* do we have room? */
    if (bytes_free < 2 + bytes_needed)
	return 0;

    if (isHello)
    {
        /* write hdr in Hello-tlv format */
        *buf = (uint8_t)number;
        if (isLarge)
        {
            *(buf+1) = 0;
            return 2;
        } else {
            *(buf+1) = bytes_needed;
        }
    } else {
        /* write in QueryLargeTlvResp LTLV format */
        /* replace: *(uint16_t*)buf = htons(bytes_needed); with: */
        g_short_reorder_buffer = htons(sizeof(bytes_needed));
        memcpy(buf, &g_short_reorder_buffer, 2);
    }
    memcpy(buf+2, data, bytes_needed);
    return bytes_needed + 2;
}


static int
write_assns_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    assns_t    *assns = (assns_t*)data;
    uint        offset_index = offset / SIZEOF_PKT_ASSN_TABLE_ENTRY;
    int         bytes_used, copy_cnt;
    uint16_t    ltlv_length;

    IF_TRACED(TRC_TLVINFO)
        printf("write-assns-list: tlvnum:%d  isHello:%s  isLarge:%s  offset:" FMT_SIZET "\n",
               number, isHello==TRUE?"true":"false", isLarge==TRUE?"true":"false", offset);
    END_TRACE

    if (isHello==TRUE)
    {
        /* write header in Hello's small-TLV format */
        *buf++ = (uint8_t)number;
        *buf   = (uint8_t)0;
        return 2;
    }

    if (assns->table == NULL)
    {
        warn("write-assns-list: No list was generated, empty list written\n");
        *buf++ = (uint8_t)0;
        *buf   = (uint8_t)0;
	return 2;
    }

    /* seek to the supplied offset, if possible */
    if (offset_index >= assns->assn_cnt)
    {
        warn("write-assns-list: Offset too big, empty list written\n");
        *buf++ = (uint8_t)0;
        *buf   = (uint8_t)0;
        assns->assn_cnt = 0;    // mark for re-collection
	return 2;
    }

    /* write header in QueryLargeTlvResp large-TLV format */
    /* write the ltlv count and more-flag */
    if (((assns->assn_cnt - offset_index) * SIZEOF_PKT_ASSN_TABLE_ENTRY) > (uint)(bytes_free - 2))
    {
        /* another request is required to get it all, set the "more-flag" in the length */
        copy_cnt = (bytes_free - 2) / SIZEOF_PKT_ASSN_TABLE_ENTRY;
        bytes_used = (copy_cnt * SIZEOF_PKT_ASSN_TABLE_ENTRY) + 2;
        ltlv_length = htons((uint16_t)bytes_used | (uint16_t)0x8000);
    } else {
        /* this request will return the whole assn-table - dont set the "more-flag" */
        copy_cnt = assns->assn_cnt - offset_index;
        bytes_used = (copy_cnt * SIZEOF_PKT_ASSN_TABLE_ENTRY) + 2;
        ltlv_length = htons((uint16_t)(bytes_used-2));
        assns->assn_cnt = 0;    // mark for re-collection, now that we have sent the last frame
    }
    memcpy(buf, &ltlv_length, 2);
    buf += 2;

    IF_TRACED(TRC_TLVINFO)
        printf("writing assn-list as large-tlv of %d bytes\n", bytes_used);
    END_TRACE

    {
        /* Copy the assn entries to the packet buffer, packed */
        for (;copy_cnt; copy_cnt--)
        {
            memcpy(buf, &assns->table[offset_index].MACaddr, sizeof(etheraddr_t));
            buf += sizeof(etheraddr_t);
            memcpy(buf, &assns->table[offset_index].MOR, 2);
            *(buf+2) = assns->table[offset_index].PHYtype;
            *(buf+3) = (uint8_t) 0;     // reserved byte
            buf += 4;
            offset_index++;
        }
    }
    return bytes_used;
}


static int
write_comptbl_t(int number, void *data, uint8_t *buf, int bytes_free, bool_t isHello, bool_t isLarge, size_t offset)
{
    comptbl_t    *ctbl = (comptbl_t*)data;
    
    uint8_t    *length = buf;
    int         bytes_used;

    IF_TRACED(TRC_TLVINFO)
        printf("write-component-table: tlvnum:%d  isHello:%s  isLarge:%s  offset:" FMT_SIZET "\n",
               number, isHello==TRUE?"true":"false", isLarge==TRUE?"true":"false", offset);
    END_TRACE

    if (isHello==TRUE)
    {
        /* write header in Hello's small-TLV format */
        *buf++ = (uint8_t)number;
        *buf   = (uint8_t)0;
        return 2;
    }
    
    /* LTLVs start with a length+more-flag (which is off, in this case)
     * This will be filled in at the end when the total length used becomes apparent...
     */

    buf += 2;   // skip length for now
    
    /* Copy in the version and reserved byte */
    *buf++ = ctbl->version;
    *buf++ = (uint8_t)0;
    
    /* then do the sub-tlvs in order, starting with the bridge component */
    if (Bridge_Component != 0xFF)
    {
        *buf++ = (uint8_t) Bridge_Component;        // type
        *buf++ = 1;                                 // length
        *buf++ = ctbl->bridge_behavior;             // value
        IF_TRACED(TRC_TLVINFO)
            printf("w-c-t: version:%d  bridge behavior: %d\n",ctbl->version,ctbl->bridge_behavior);
        END_TRACE
    }

    /* for each radio band, add a component */
    {
        radio_t  *rb = ctbl->radios;
        int       i;

        for (i=0;i<ctbl->radio_cnt;i++,rb++)
        {
            *buf++ = (uint8_t) RadioBand_Component;
            *buf++ = (uint8_t) 10;
            memcpy(buf, &rb->MOR, 2);      buf += 2;
            *buf++ = rb->PHYtype;
            *buf++ = rb->mode;
            memcpy(buf, &rb->BSSID, 6);    buf += 6;

            IF_TRACED(TRC_TLVINFO)
                printf("w-c-t: radio band: %d  MOR: %d  PHY: %d  Mode: %d  BSSID: " ETHERADDR_FMT "\n",
                       i, ntohs(rb->MOR), rb->PHYtype, rb->mode, ETHERADDR_PRINT(&(rb->BSSID)) );
            END_TRACE
        }
    }

    /* Finally, do the switch component */
    if (Switch_Component != 0xFFFFFFFF)
    {
        *buf++ = (uint8_t) Switch_Component;        // type
        *buf++ = 4;                                 // length
        memcpy(buf, &ctbl->link_speed, 4);          // value
        buf += 4;

        IF_TRACED(TRC_TLVINFO)
            printf("w-c-t: switch link speed: %d\n",ntohl(ctbl->link_speed));
        END_TRACE
    }

    /* Now, once the total length is known, put it in the reserved location at the front */
    bytes_used = buf - length;
    g_short_reorder_buffer = htons(bytes_used - 2);
    memcpy(length, &g_short_reorder_buffer, 2);
    return bytes_used;
}


void *(NotUsed[]) = {write_uint16_t,write_ipv6addr_t};

/******************** D E C L A R E   T H E   T L V - D E S C R I P T O R S ***********************/

/* calculate the byte offset of field "_f" in tlv_info_t */
#define offset(_f) \
  (size_t)(&(((tlv_info_t *)0)->_f))

/* NOTE: can't be "const", because the "access" element is written (going from unset to static)   */
tlv_desc_t Tlvs[] =
{
#define TLVDEF(_Ctype, _name, _repeat, _number, _access, _inHello) \
    {_number, offset(_name), _access, _inHello, get_ ## _name, write_ ## _Ctype},
#include "tlvdef.h"
#undef TLVDEF
    {0, 0, 0, FALSE, NULL, NULL}
};

/**************************************************************************************************/

int
tlv_write_info(uint8_t *buf, int bytes_free)
{
    tlv_desc_t *tlv = Tlvs;
    int nb, total;

    IF_TRACED(TRC_TLVINFO)
        printf("tlv_write_info: entering with tracing set to \'%s\', interface = %s\n",
               g_trace_flags?"true":"false", g_osl->responder_if);
    END_TRACE
    total = 0;
    while (tlv->write != NULL && bytes_free > 1)
    {
        nb = tlv_write_tlv(tlv, buf, bytes_free, TRUE, (size_t)0);
        buf += nb;
        total += nb;
        bytes_free -= nb;
	tlv++;
    }

    return total;
}


int
tlv_write_tlv(tlv_desc_t *tlv, uint8_t *buf, int bytes_free, bool_t isHello, size_t LtlvOffset)
{
    uint8_t *infobase = (uint8_t*)(&g_info);
    int    nb = 0;
    int    getfn_returned = -1;

    IF_TRACED(TRC_TLVINFO)
        printf("tlv_write_tlv: Building TLV # %d (%d bytes available in send-buffer\n",
               tlv->number,bytes_free);
    END_TRACE
    switch (tlv->access)
    {
      case  Access_unset:
        if (tlv->get != NULL)
        {
            tlv->access = Access_static;
            IF_TRACED(TRC_TLVINFO)
                printf("tlv_write_tlv: initializing 'unset' TLV %d to 'static'\n", tlv->number);
            END_TRACE
        } else {
            warn("tlv_write_tlv: FAILED initializing 'unset' TLV %d (NULL get_fn)\n", tlv->number);
            break;
        }
        /* deliberate fall-through */

      case  Access_dynamic:
        if (tlv->get != NULL)
        {   /* call the tlv_get_fn for this tlv */
            getfn_returned = tlv->get((void*)(infobase + tlv->offset));
            IF_TRACED(TRC_TLVINFO)
                printf("tlv_write_tlv: getting value for TLV %d returned \'%s\'\n",
                      tlv->number, getfn_returned == TLV_GET_SUCCEEDED ? "SUCCESS" : "FAILURE");
            END_TRACE

            if (getfn_returned != TLV_GET_SUCCEEDED)
            {
                /* On get-failure, reset the access to "unset" if it was "static" */
                if (tlv->access == Access_static) tlv->access = Access_unset;
                break;	// Don't bother to call the write....
            }
        } else {
            warn("tlv_write_tlv: FAILED getting 'static' or 'dynamic' TLV %d (NULL get_fn)\n", tlv->number);
            break;	// Don't bother to call the write....
        }
        /* deliberate fall-through */

      case  Access_static:
        nb = tlv->write(tlv->number, infobase + tlv->offset, buf, bytes_free,
                        isHello,		   /* write in Hello format if isHello; else in LTLV format */
                        (tlv->inHello==FALSE),     /* force those not inHello to be LTLVs */
                        LtlvOffset);		   /* must be 0 in Hello format; <= 32k in LTLV format */
        IF_TRACED(TRC_TLVINFO)
            dbgprintf("tlv_write_tlv: wrote %d bytes of TLV %d (%d bytes were free)\n", nb,
                         tlv->number, bytes_free);
        END_TRACE
        break;

      case  Access_invalid:
        break;
    }
    return nb;
}
