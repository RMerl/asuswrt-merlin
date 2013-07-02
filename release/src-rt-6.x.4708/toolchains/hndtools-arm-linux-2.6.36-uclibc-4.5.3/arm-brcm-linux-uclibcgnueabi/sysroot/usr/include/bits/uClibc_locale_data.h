#define __lc_time_data_LEN		303
#define __lc_time_rows_LEN		100
#define __lc_time_item_offsets_LEN		50
#define __lc_time_item_idx_LEN		53
#define __lc_numeric_data_LEN		7
#define __lc_numeric_rows_LEN		6
#define __lc_numeric_item_offsets_LEN		3
#define __lc_numeric_item_idx_LEN		5
#define __lc_monetary_data_LEN		25
#define __lc_monetary_rows_LEN		44
#define __lc_monetary_item_offsets_LEN		22
#define __lc_monetary_item_idx_LEN		43
#define __lc_messages_data_LEN		29
#define __lc_messages_rows_LEN		8
#define __lc_messages_item_offsets_LEN		4
#define __lc_messages_item_idx_LEN		6
#define __lc_ctype_data_LEN		21
#define __lc_ctype_rows_LEN		10
#define __lc_ctype_item_offsets_LEN		10
#define __lc_ctype_item_idx_LEN		10
#define __CTYPE_HAS_UTF_8_LOCALES			1
#define __LOCALE_DATA_CATEGORIES			6
#define __LOCALE_DATA_WIDTH_LOCALES			9
#define __LOCALE_DATA_NUM_LOCALES			3
#define __LOCALE_DATA_NUM_LOCALE_NAMES		2
#define __LOCALE_DATA_AT_MODIFIERS_LENGTH		7
#define __lc_names_LEN		69
#define __lc_collate_data_LEN  7370
#define __CTYPE_HAS_8_BIT_LOCALES		1

#define __LOCALE_DATA_Cctype_IDX_SHIFT	3
#define __LOCALE_DATA_Cctype_IDX_LEN		16
#define __LOCALE_DATA_Cctype_ROW_LEN		4
#define __LOCALE_DATA_Cctype_PACKED		1

#define __LOCALE_DATA_Cuplow_IDX_SHIFT	3
#define __LOCALE_DATA_Cuplow_IDX_LEN		16
#define __LOCALE_DATA_Cuplow_ROW_LEN		8

#define __LOCALE_DATA_Cc2wc_IDX_LEN		16
#define __LOCALE_DATA_Cc2wc_IDX_SHIFT		3
#define __LOCALE_DATA_Cc2wc_ROW_LEN		8

typedef struct {
	unsigned char idx8ctype[16];
	unsigned char idx8uplow[16];
	unsigned char idx8c2wc[16];
	unsigned char idx8wc2c[38];
} __codeset_8_bit_t;


#define __LOCALE_DATA_Cwc2c_DOMAIN_MAX	0x25ff
#define __LOCALE_DATA_Cwc2c_TI_SHIFT		4
#define __LOCALE_DATA_Cwc2c_TT_SHIFT		4
#define __LOCALE_DATA_Cwc2c_II_LEN		38
#define __LOCALE_DATA_Cwc2c_TI_LEN		32
#define __LOCALE_DATA_Cwc2c_TT_LEN		144


#define __LOCALE_DATA_Cwc2c_TBL_LEN		176

#define __LOCALE_DATA_Cuplow_TBL_LEN		24


#define __LOCALE_DATA_Cctype_TBL_LEN		40


#define __LOCALE_DATA_Cc2wc_TBL_LEN		136



#define __LOCALE_DATA_NUM_CODESETS		2
#define __LOCALE_DATA_CODESET_LIST \
	"\x03\x09" \
	"\0" \
	"ASCII\0" \
	"ISO-8859-1\0"

#define __CTYPE_HAS_CODESET_ASCII
#define __CTYPE_HAS_CODESET_ISO_8859_1
#define __CTYPE_HAS_CODESET_UTF_8
#define __LOCALE_DATA_WC_TABLE_DOMAIN_MAX   0x2ffff

#define __LOCALE_DATA_WCctype_II_LEN        768
#define __LOCALE_DATA_WCctype_TI_LEN       1888
#define __LOCALE_DATA_WCctype_UT_LEN        948
#define __LOCALE_DATA_WCctype_II_SHIFT        5
#define __LOCALE_DATA_WCctype_TI_SHIFT        3


#define __LOCALE_DATA_WCuplow_II_LEN        384
#define __LOCALE_DATA_WCuplow_TI_LEN        576
#define __LOCALE_DATA_WCuplow_UT_LEN        720
#define __LOCALE_DATA_WCuplow_II_SHIFT        6
#define __LOCALE_DATA_WCuplow_TI_SHIFT        3


#define __LOCALE_DATA_WCuplow_diffs       98


/* #define __LOCALE_DATA_MAGIC_SIZE 64 */
#ifndef __WCHAR_ENABLED
#if 0
#warning WHOA!!! __WCHAR_ENABLED is not defined! defining it now...
#endif
#define __WCHAR_ENABLED
#endif

/* TODO - fix */
#ifdef __WCHAR_ENABLED
#define __LOCALE_DATA_WCctype_TBL_LEN      (__LOCALE_DATA_WCctype_II_LEN + __LOCALE_DATA_WCctype_TI_LEN + __LOCALE_DATA_WCctype_UT_LEN)
#define __LOCALE_DATA_WCuplow_TBL_LEN      (__LOCALE_DATA_WCuplow_II_LEN + __LOCALE_DATA_WCuplow_TI_LEN + __LOCALE_DATA_WCuplow_UT_LEN)
#define __LOCALE_DATA_WCuplow_diff_TBL_LEN (2 * __LOCALE_DATA_WCuplow_diffs)
/* #define WCcomb_TBL_LEN		(WCcomb_II_LEN + WCcomb_TI_LEN + WCcomb_UT_LEN) */
#endif

#undef __PASTE2
#define __PASTE2(A,B)   A ## B
#undef __PASTE3
#define __PASTE3(A,B,C) A ## B ## C

#define __LOCALE_DATA_COMMON_MMAP(X) \
	unsigned char   __PASTE3(lc_,X,_data)[__PASTE3(__lc_,X,_data_LEN)];

#define __LOCALE_DATA_COMMON_MMIDX(X) \
	unsigned char   __PASTE3(lc_,X,_rows)[__PASTE3(__lc_,X,_rows_LEN)]; \
	uint16_t        __PASTE3(lc_,X,_item_offsets)[__PASTE3(__lc_,X,_item_offsets_LEN)]; \
	uint16_t        __PASTE3(lc_,X,_item_idx)[__PASTE3(__lc_,X,_item_idx_LEN)]; \


typedef struct {
#ifdef __LOCALE_DATA_MAGIC_SIZE
	unsigned char magic[__LOCALE_DATA_MAGIC_SIZE];
#endif

#ifdef __CTYPE_HAS_8_BIT_LOCALES
	const unsigned char tbl8ctype[__LOCALE_DATA_Cctype_TBL_LEN];
	const unsigned char tbl8uplow[__LOCALE_DATA_Cuplow_TBL_LEN];
#ifdef __WCHAR_ENABLED
	const uint16_t tbl8c2wc[__LOCALE_DATA_Cc2wc_TBL_LEN]; /* char > 0x7f to wide char */
	const unsigned char tbl8wc2c[__LOCALE_DATA_Cwc2c_TBL_LEN];
	/* translit  */
#endif /* __WCHAR_ENABLED */
#endif /* __CTYPE_HAS_8_BIT_LOCALES */
#ifdef __WCHAR_ENABLED
	const unsigned char tblwctype[__LOCALE_DATA_WCctype_TBL_LEN];
	const unsigned char tblwuplow[__LOCALE_DATA_WCuplow_TBL_LEN];
	const int16_t tblwuplow_diff[__LOCALE_DATA_WCuplow_diff_TBL_LEN];
/* 	const unsigned char tblwcomb[WCcomb_TBL_LEN]; */
	/* width?? */
#endif

	__LOCALE_DATA_COMMON_MMAP(ctype)
	__LOCALE_DATA_COMMON_MMAP(numeric)
	__LOCALE_DATA_COMMON_MMAP(monetary)
	__LOCALE_DATA_COMMON_MMAP(time)
	/* collate is different */
	__LOCALE_DATA_COMMON_MMAP(messages)


#ifdef __CTYPE_HAS_8_BIT_LOCALES
	const __codeset_8_bit_t codeset_8_bit[__LOCALE_DATA_NUM_CODESETS];
#endif

	__LOCALE_DATA_COMMON_MMIDX(ctype)
	__LOCALE_DATA_COMMON_MMIDX(numeric)
	__LOCALE_DATA_COMMON_MMIDX(monetary)
	__LOCALE_DATA_COMMON_MMIDX(time)
	/* collate is different */
	__LOCALE_DATA_COMMON_MMIDX(messages)

	const uint16_t collate_data[__lc_collate_data_LEN];

	unsigned char lc_common_item_offsets_LEN[__LOCALE_DATA_CATEGORIES];
	size_t lc_common_tbl_offsets[__LOCALE_DATA_CATEGORIES * 4];
	/* offsets from start of locale_mmap_t */
	/* rows, item_offsets, item_idx, data */

#ifdef __LOCALE_DATA_NUM_LOCALES
	unsigned char locales[__LOCALE_DATA_NUM_LOCALES * __LOCALE_DATA_WIDTH_LOCALES];
	unsigned char locale_names5[5*__LOCALE_DATA_NUM_LOCALE_NAMES];
	unsigned char locale_at_modifiers[__LOCALE_DATA_AT_MODIFIERS_LENGTH];
#endif

	unsigned char lc_names[__lc_names_LEN];
#ifdef __CTYPE_HAS_8_BIT_LOCALES
	unsigned char codeset_list[sizeof(__LOCALE_DATA_CODESET_LIST)]; /* TODO - fix */
#endif
} __locale_mmap_t;

extern const __locale_mmap_t *__locale_mmap;
