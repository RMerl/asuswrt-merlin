/* vi: set sw=8 ts=8: */
/*
 * This file suffers from chronically incorrect tabification
 * of messages. Before editing this file:
 * 1. Switch you editor to 8-space tab mode.
 * 2. Do not use \t in messages, use real tab character.
 * 3. Start each source line with message as follows:
 *    |<7 spaces>"text with tabs"....
 * or
 *    |<5 spaces>"\ntext with tabs"....
 */
#ifndef BB_USAGE_H
#define BB_USAGE_H 1

#define NOUSAGE_STR "\b"

INSERT

#define busybox_notes_usage \
       "Hello world!\n"

#endif
/* Asus added these on top of their 1.17.4.  The code came from
 * Qualcomm, so we can't backport it properly from 1.2x.
 * Putting these here for now, however this shouldn't matter
   as these two applets are only for QCA devices
   (which we don't support).  If it fails to build we can
   always simply remove these Qualcomm-specific additions.
*/

#define md_trivial_usage \
       "md address [count]"
#define md_full_usage "\n\n" \
       "md address [count]\n"

#define mm_trivial_usage \
       "mm address value"
#define mm_full_usage "\n\n" \
       "mm address value\n"

#define ethreg_trivial_usage \
       "ethreg ......"
#define ethreg_full_usage "\n\n" \
       "ethreg ......\n"


