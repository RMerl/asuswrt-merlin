/*
 */

#define CNID_META_CNID_LEN      4
#define CNID_META_MDATE_LEN     4  /* space for 8 */
#define CNID_META_CDATE_LEN     4  /* space for 8 */
#define CNID_META_BDATE_LEN     4  /* ditto */
#define CNID_META_ADATE_LEN     4  /* ditto */
#define CNID_META_AFPI_LEN      4  /* plus permission bits */
#define CNID_META_FINDERI_LEN   32 
#define CNID_META_PRODOSI_LEN   8
#define CNID_META_RFORKLEN_LEN  4  /* space for 8 */
#define CNID_META_MACNAME_LEN   32 /* maximum size */
#define CNID_META_SHORTNAME_LEN 12 /* max size (8.3) */
#define CNID_META_FILLER_LEN    4

#define CNID_META_CNID_OFF     0
#define CNID_META_MDATE_OFF    (CNID_META_CNID_OFF + CNID_META_CNID_LEN + \
				CNID_META_FILLER_LEN)
#define CNID_META_CDATE_OFF    (CNID_META_MDATE_OFF + CNID_META_MDATE_LEN + \
				CNID_META_FILLER_LEN)
#define CNID_META_BDATE_OFF    (CNID_META_CDATE_OFF + CNID_META_CDATE_LEN + \
				CNID_META_FILLER_LEN)
#define CNID_META_ADATE_OFF    (CNID_META_BDATE_OFF + CNID_META_BDATE_LEN + \
				CNID_META_FILLER_LEN)
#define CNID_META_AFPI_OFF     (CNID_META_ADATE_OFF + CNID_META_ADATE_LEN)
#define CNID_META_FINDERI_OFF  (CNID_META_AFPI_OFF + CNID_META_AFPI_LEN)
#define CNID_META_PRODOSI_OFF  (CNID_META_FINDERI_OFF + CNID_META_FINDERI_LEN)
#define CNID_META_RFORKLEN_OFF (CNID_META_PRODOSI_OFF + CNID_META_PRODOSI_LEN)
#define CNID_META_MACNAME_OFF  (CNID_META_RFORKLEN_OFF + \
				CNID_META_RFORKLEN_LEN)
#define CNID_META_SHORTNAME_OFF (CNID_META_MACNAME_OFF + 


#define cnid_meta_clear(a)  
#define cnid_meta_get(id)

#define cnid_meta_cnid(a)  
#define cnid_meta_modifydate(a)
#define cnid_meta_createdate(a)
#define cnid_meta_backupdate(a)
#define cnid_meta_accessdate(a)
#define cnid_meta_afpi(a)
#define cnid_meta_finderi(a)
#define cnid_meta_prodosi(a)
#define cnid_meta_rforklen(a)
#define cnid_meta_macname(a)
#define cnid_meta_shortname(a)
#define cnid_meta_longname(a)

