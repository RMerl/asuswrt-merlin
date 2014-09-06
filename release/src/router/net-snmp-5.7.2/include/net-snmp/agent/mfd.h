/*
 * MIBs For Dummies header
 *
 * $Id$
 */
#ifndef NETSNMP_MFD_H
#define NETSNMP_MFD_H

/***********************************************************************
 *
 * return codes
 *
 **********************************************************************/

/*----------------------------------------------------------------------
 * general success/failure
 */
#define MFD_SUCCESS              SNMP_ERR_NOERROR
#define MFD_ERROR                SNMP_ERR_GENERR

/*
 * object not currently available
 */
#define MFD_SKIP                 SNMP_NOSUCHINSTANCE

/*
 * no more data in table (get-next)
 */
#define MFD_END_OF_DATA          SNMP_ENDOFMIBVIEW

/*----------------------------------------------------------------------
 * set processing errors
 */
/*
 * row creation errors
 */
#define MFD_CANNOT_CREATE_NOW    SNMP_ERR_INCONSISTENTNAME
#define MFD_CANNOT_CREATE_EVER   SNMP_ERR_NOCREATION

/*
 * not writable or resource unavailable
 */
#define MFD_NOT_WRITABLE         SNMP_ERR_NOTWRITABLE
#define MFD_RESOURCE_UNAVAILABLE SNMP_ERR_RESOURCEUNAVAILABLE

/*
 * new value errors
 */
#define MFD_NOT_VALID_NOW        SNMP_ERR_INCONSISTENTVALUE
#define MFD_NOT_VALID_EVER       SNMP_ERR_WRONGVALUE


/***********************************************************************
 *
 * rowreq flags
 *
 **********************************************************************/

/*----------------------------------------------------------------------
 * 8 flags resevered for the user
 */
#define MFD_ROW_FLAG_USER_1            0x00000001 /* user flag 1 */
#define MFD_ROW_FLAG_USER_2            0x00000002 /* user flag 2 */
#define MFD_ROW_FLAG_USER_3            0x00000004 /* user flag 3 */
#define MFD_ROW_FLAG_USER_4            0x00000008 /* user flag 4 */
#define MFD_ROW_FLAG_USER_5            0x00000010 /* user flag 5 */
#define MFD_ROW_FLAG_USER_6            0x00000020 /* user flag 6 */
#define MFD_ROW_FLAG_USER_7            0x00000040 /* user flag 7 */
#define MFD_ROW_FLAG_USER_8            0x00000080 /* user flag 8 */
#define MFD_ROW_FLAG_USER_MASK         0x000000ff /* user flag mask */

/*----------------------------------------------------------------------
 * MFD flags
 *
 * grow left to right, in case we want to add more user flags later
 */
#define MFD_ROW_MASK                   0xffffff00 /* mask to clear user flags */
#define MFD_ROW_CREATED                0x80000000 /* newly created row */
#define MFD_ROW_DATA_FROM_USER         0x40000000 /* we didn't allocate data */
#define MFD_ROW_DELETED                0x20000000 /* deleted row */
#define MFD_ROW_DIRTY                  0x10000000 /* changed row */


#endif                          /* NETSNMP_MFD_H */
