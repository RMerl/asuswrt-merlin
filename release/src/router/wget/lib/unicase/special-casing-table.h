/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -m 10 lib/unicase/special-casing-table.gperf  */
/* Computed positions: -k'1-3' */


#define TOTAL_KEYWORDS 122
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 3
#define MIN_HASH_VALUE 0
#define MAX_HASH_VALUE 121
/* maximum key range = 122, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
/*ARGSUSED*/
static unsigned int
gl_unicase_special_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
        2,   0,   4,   5,  37,  12, 121,   4, 122, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 122, 120,
      119, 118, 117, 116, 122, 122, 122, 122, 122, 122,
        5,   3, 122, 122, 122, 122, 122, 122, 122, 122,
      115, 122, 122, 122, 122, 122, 114, 122,   6, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
      122, 122, 122,   1, 111, 122, 122, 122, 122, 122,
      109, 122, 108, 122, 107, 122, 106, 122, 122, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
      122, 122, 122, 122, 122,  33, 122, 122, 122, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
      122, 122, 122, 122, 122, 122, 122, 122, 105, 104,
      103, 102, 101, 100,  99,  31,  98,  97,  96,  95,
       94,  93,  92,  91,  28,  90,  89,  88,  87,  86,
       27,  24,  23,  20,  19,  85,  84,  83,  16,  82,
       81,  80,  79,  15,  78,  77,  76,  75,  74,  73,
       72,  71,  70,  69,  68,  67,  64, 122,  65,  64,
       63, 122,  62,  61, 122, 122, 122, 122,  60, 122,
      122, 122, 122, 122,  59,  58,  57, 122,  56,  55,
      122, 122, 122, 122,  14,  55, 122, 122, 122, 122,
       53,  52, 122, 122,  51,  50, 122, 122, 122, 122,
      122, 122, 122,  50, 122, 122,  48,  47,  46, 122,
       45,  44, 122, 122, 122, 122, 122, 122, 122, 122,
       46, 122,  42,  41,  39, 122,  38,  35, 122, 122,
      122,   0,  33, 122, 122, 122, 122
    };
  return asso_values[(unsigned char)str[2]+1] + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct special_casing_rule *
gl_unicase_special_lookup (const char *str, size_t len)
{
  static const unsigned char lengthtable[] =
    {
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
       3,  3,  3,  3,  3,  3,  3,  3,  3,  3
    };
  static const struct special_casing_rule wordlist[] =
    {
#line 126 "lib/unicase/special-casing-table.gperf"
      {"\373\001\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0046, 0x0049,      0 }, { 0xFB01,      0,      0 }, { 0x0046, 0x0069,      0 }, { 0x0066, 0x0069,      0 }},
#line 31 "lib/unicase/special-casing-table.gperf"
      {"\001I\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x02BC, 0x004E,      0 }, { 0x0149,      0,      0 }, { 0x02BC, 0x004E,      0 }, { 0x02BC, 0x006E,      0 }},
#line 125 "lib/unicase/special-casing-table.gperf"
      {"\373\000\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0046, 0x0046,      0 }, { 0xFB00,      0,      0 }, { 0x0046, 0x0066,      0 }, { 0x0066, 0x0066,      0 }},
#line 15 "lib/unicase/special-casing-table.gperf"
      {"\000I\000", 1,  SCC_MORE_ABOVE       , {  'l',  't' }, { 0x0049,      0,      0 }, { 0x0069, 0x0307,      0 }, { 0x0049,      0,      0 }, { 0x0069,      0,      0 }},
#line 127 "lib/unicase/special-casing-table.gperf"
      {"\373\002\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0046, 0x004C,      0 }, { 0xFB02,      0,      0 }, { 0x0046, 0x006C,      0 }, { 0x0066, 0x006C,      0 }},
#line 128 "lib/unicase/special-casing-table.gperf"
      {"\373\003\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0046, 0x0046, 0x0049 }, { 0xFB03,      0,      0 }, { 0x0046, 0x0066, 0x0069 }, { 0x0066, 0x0066, 0x0069 }},
#line 28 "lib/unicase/special-casing-table.gperf"
      {"\0010\000", 1,  SCC_ALWAYS           , {  't',  'r' }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }},
#line 16 "lib/unicase/special-casing-table.gperf"
      {"\000I\001", 1, -SCC_BEFORE_DOT       , {  't',  'r' }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }},
#line 17 "lib/unicase/special-casing-table.gperf"
      {"\000I\002", 1, -SCC_BEFORE_DOT       , {  'a',  'z' }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }},
#line 33 "lib/unicase/special-casing-table.gperf"
      {"\003\007\000", 1,  SCC_AFTER_SOFT_DOTTED, {  'l',  't' }, {      0,      0,      0 }, { 0x0307,      0,      0 }, {      0,      0,      0 }, { 0x0307,      0,      0 }},
#line 29 "lib/unicase/special-casing-table.gperf"
      {"\0010\001", 1,  SCC_ALWAYS           , {  'a',  'z' }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }},
#line 30 "lib/unicase/special-casing-table.gperf"
      {"\0010\002", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0130,      0,      0 }, { 0x0069, 0x0307,      0 }, { 0x0130,      0,      0 }, { 0x0069, 0x0307,      0 }},
#line 130 "lib/unicase/special-casing-table.gperf"
      {"\373\005\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0053, 0x0054,      0 }, { 0xFB05,      0,      0 }, { 0x0053, 0x0074,      0 }, { 0x0073, 0x0074,      0 }},
#line 34 "lib/unicase/special-casing-table.gperf"
      {"\003\007\001", 1,  SCC_AFTER_I          , {  't',  'r' }, { 0x0307,      0,      0 }, {      0,      0,      0 }, { 0x0307,      0,      0 }, { 0x0307,      0,      0 }},
#line 35 "lib/unicase/special-casing-table.gperf"
      {"\003\007\002", 0,  SCC_AFTER_I          , {  'a',  'z' }, { 0x0307,      0,      0 }, {      0,      0,      0 }, { 0x0307,      0,      0 }, { 0x0307,      0,      0 }},
#line 19 "lib/unicase/special-casing-table.gperf"
      {"\000I\004", 0,  SCC_ALWAYS           , {  'a',  'z' }, { 0x0049,      0,      0 }, { 0x0069,      0,      0 }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }},
#line 23 "lib/unicase/special-casing-table.gperf"
      {"\000\314\000", 0,  SCC_ALWAYS           , {  'l',  't' }, { 0x00CC,      0,      0 }, { 0x0069, 0x0307, 0x0300 }, { 0x00CC,      0,      0 }, { 0x00EC,      0,      0 }},
#line 109 "lib/unicase/special-casing-table.gperf"
      {"\037\314\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0397, 0x0399,      0 }, { 0x1FC3,      0,      0 }, { 0x1FCC,      0,      0 }, { 0x03B7, 0x03B9,      0 }},
#line 85 "lib/unicase/special-casing-table.gperf"
      {"\037\243\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6B, 0x0399,      0 }, { 0x1FA3,      0,      0 }, { 0x1FAB,      0,      0 }, { 0x1F63, 0x03B9,      0 }},
#line 80 "lib/unicase/special-casing-table.gperf"
      {"\037\236\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2E, 0x0399,      0 }, { 0x1F96,      0,      0 }, { 0x1F9E,      0,      0 }, { 0x1F26, 0x03B9,      0 }},
#line 37 "lib/unicase/special-casing-table.gperf"
      {"\003\243\000", 0,  SCC_FINAL_SIGMA      , { '\0', '\0' }, { 0x03A3,      0,      0 }, { 0x03C2,      0,      0 }, { 0x03A3,      0,      0 }, { 0x03C3,      0,      0 }},
#line 45 "lib/unicase/special-casing-table.gperf"
      {"\036\236\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1E9E,      0,      0 }, { 0x00DF,      0,      0 }, { 0x1E9E,      0,      0 }, { 0x0073, 0x0073,      0 }},
#line 76 "lib/unicase/special-casing-table.gperf"
      {"\037\232\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2A, 0x0399,      0 }, { 0x1F92,      0,      0 }, { 0x1F9A,      0,      0 }, { 0x1F22, 0x03B9,      0 }},
#line 75 "lib/unicase/special-casing-table.gperf"
      {"\037\231\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F29, 0x0399,      0 }, { 0x1F91,      0,      0 }, { 0x1F99,      0,      0 }, { 0x1F21, 0x03B9,      0 }},
#line 44 "lib/unicase/special-casing-table.gperf"
      {"\036\232\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0041, 0x02BE,      0 }, { 0x1E9A,      0,      0 }, { 0x0041, 0x02BE,      0 }, { 0x0061, 0x02BE,      0 }},
#line 43 "lib/unicase/special-casing-table.gperf"
      {"\036\231\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0059, 0x030A,      0 }, { 0x1E99,      0,      0 }, { 0x0059, 0x030A,      0 }, { 0x0079, 0x030A,      0 }},
#line 74 "lib/unicase/special-casing-table.gperf"
      {"\037\230\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F28, 0x0399,      0 }, { 0x1F90,      0,      0 }, { 0x1F98,      0,      0 }, { 0x1F20, 0x03B9,      0 }},
#line 73 "lib/unicase/special-casing-table.gperf"
      {"\037\227\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2F, 0x0399,      0 }, { 0x1F97,      0,      0 }, { 0x1F9F,      0,      0 }, { 0x1F27, 0x03B9,      0 }},
#line 42 "lib/unicase/special-casing-table.gperf"
      {"\036\230\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0057, 0x030A,      0 }, { 0x1E98,      0,      0 }, { 0x0057, 0x030A,      0 }, { 0x0077, 0x030A,      0 }},
#line 41 "lib/unicase/special-casing-table.gperf"
      {"\036\227\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0054, 0x0308,      0 }, { 0x1E97,      0,      0 }, { 0x0054, 0x0308,      0 }, { 0x0074, 0x0308,      0 }},
#line 72 "lib/unicase/special-casing-table.gperf"
      {"\037\226\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2E, 0x0399,      0 }, { 0x1F96,      0,      0 }, { 0x1F9E,      0,      0 }, { 0x1F26, 0x03B9,      0 }},
#line 66 "lib/unicase/special-casing-table.gperf"
      {"\037\220\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F28, 0x0399,      0 }, { 0x1F90,      0,      0 }, { 0x1F98,      0,      0 }, { 0x1F20, 0x03B9,      0 }},
#line 40 "lib/unicase/special-casing-table.gperf"
      {"\036\226\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0048, 0x0331,      0 }, { 0x1E96,      0,      0 }, { 0x0048, 0x0331,      0 }, { 0x0068, 0x0331,      0 }},
#line 36 "lib/unicase/special-casing-table.gperf"
      {"\003\220\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0399, 0x0308, 0x0301 }, { 0x0390,      0,      0 }, { 0x0399, 0x0308, 0x0301 }, { 0x03B9, 0x0308, 0x0301 }},
#line 57 "lib/unicase/special-casing-table.gperf"
      {"\037\207\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0F, 0x0399,      0 }, { 0x1F87,      0,      0 }, { 0x1F8F,      0,      0 }, { 0x1F07, 0x03B9,      0 }},
#line 21 "lib/unicase/special-casing-table.gperf"
      {"\000i\000", 1,  SCC_ALWAYS           , {  't',  'r' }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }},
#line 124 "lib/unicase/special-casing-table.gperf"
      {"\037\374\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A9, 0x0399,      0 }, { 0x1FF3,      0,      0 }, { 0x1FFC,      0,      0 }, { 0x03C9, 0x03B9,      0 }},
#line 129 "lib/unicase/special-casing-table.gperf"
      {"\373\004\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0046, 0x0046, 0x004C }, { 0xFB04,      0,      0 }, { 0x0046, 0x0066, 0x006C }, { 0x0066, 0x0066, 0x006C }},
#line 123 "lib/unicase/special-casing-table.gperf"
      {"\037\367\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A9, 0x0342, 0x0399 }, { 0x1FF7,      0,      0 }, { 0x03A9, 0x0342, 0x0345 }, { 0x03C9, 0x0342, 0x03B9 }},
#line 22 "lib/unicase/special-casing-table.gperf"
      {"\000i\001", 0,  SCC_ALWAYS           , {  'a',  'z' }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }, { 0x0130,      0,      0 }, { 0x0069,      0,      0 }},
#line 18 "lib/unicase/special-casing-table.gperf"
      {"\000I\003", 1,  SCC_ALWAYS           , {  't',  'r' }, { 0x0049,      0,      0 }, { 0x0069,      0,      0 }, { 0x0049,      0,      0 }, { 0x0131,      0,      0 }},
#line 122 "lib/unicase/special-casing-table.gperf"
      {"\037\366\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A9, 0x0342,      0 }, { 0x1FF6,      0,      0 }, { 0x03A9, 0x0342,      0 }, { 0x03C9, 0x0342,      0 }},
#line 121 "lib/unicase/special-casing-table.gperf"
      {"\037\364\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x038F, 0x0399,      0 }, { 0x1FF4,      0,      0 }, { 0x038F, 0x0345,      0 }, { 0x03CE, 0x03B9,      0 }},
#line 39 "lib/unicase/special-casing-table.gperf"
      {"\005\207\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0535, 0x0552,      0 }, { 0x0587,      0,      0 }, { 0x0535, 0x0582,      0 }, { 0x0565, 0x0582,      0 }},
#line 120 "lib/unicase/special-casing-table.gperf"
      {"\037\363\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A9, 0x0399,      0 }, { 0x1FF3,      0,      0 }, { 0x1FFC,      0,      0 }, { 0x03C9, 0x03B9,      0 }},
#line 119 "lib/unicase/special-casing-table.gperf"
      {"\037\362\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1FFA, 0x0399,      0 }, { 0x1FF2,      0,      0 }, { 0x1FFA, 0x0345,      0 }, { 0x1F7C, 0x03B9,      0 }},
#line 32 "lib/unicase/special-casing-table.gperf"
      {"\001\360\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x004A, 0x030C,      0 }, { 0x01F0,      0,      0 }, { 0x004A, 0x030C,      0 }, { 0x006A, 0x030C,      0 }},
#line 118 "lib/unicase/special-casing-table.gperf"
      {"\037\347\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0308, 0x0342 }, { 0x1FE7,      0,      0 }, { 0x03A5, 0x0308, 0x0342 }, { 0x03C5, 0x0308, 0x0342 }},
#line 117 "lib/unicase/special-casing-table.gperf"
      {"\037\346\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0342,      0 }, { 0x1FE6,      0,      0 }, { 0x03A5, 0x0342,      0 }, { 0x03C5, 0x0342,      0 }},
#line 116 "lib/unicase/special-casing-table.gperf"
      {"\037\344\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A1, 0x0313,      0 }, { 0x1FE4,      0,      0 }, { 0x03A1, 0x0313,      0 }, { 0x03C1, 0x0313,      0 }},
#line 115 "lib/unicase/special-casing-table.gperf"
      {"\037\343\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0308, 0x0301 }, { 0x1FE3,      0,      0 }, { 0x03A5, 0x0308, 0x0301 }, { 0x03C5, 0x0308, 0x0301 }},
#line 114 "lib/unicase/special-casing-table.gperf"
      {"\037\342\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0308, 0x0300 }, { 0x1FE2,      0,      0 }, { 0x03A5, 0x0308, 0x0300 }, { 0x03C5, 0x0308, 0x0300 }},
#line 25 "lib/unicase/special-casing-table.gperf"
      {"\000\337\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0053, 0x0053,      0 }, { 0x00DF,      0,      0 }, { 0x0053, 0x0073,      0 }, { 0x0073, 0x0073,      0 }},
#line 113 "lib/unicase/special-casing-table.gperf"
      {"\037\327\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0399, 0x0308, 0x0342 }, { 0x1FD7,      0,      0 }, { 0x0399, 0x0308, 0x0342 }, { 0x03B9, 0x0308, 0x0342 }},
#line 112 "lib/unicase/special-casing-table.gperf"
      {"\037\326\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0399, 0x0342,      0 }, { 0x1FD6,      0,      0 }, { 0x0399, 0x0342,      0 }, { 0x03B9, 0x0342,      0 }},
#line 111 "lib/unicase/special-casing-table.gperf"
      {"\037\323\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0399, 0x0308, 0x0301 }, { 0x1FD3,      0,      0 }, { 0x0399, 0x0308, 0x0301 }, { 0x03B9, 0x0308, 0x0301 }},
#line 110 "lib/unicase/special-casing-table.gperf"
      {"\037\322\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0399, 0x0308, 0x0300 }, { 0x1FD2,      0,      0 }, { 0x0399, 0x0308, 0x0300 }, { 0x03B9, 0x0308, 0x0300 }},
#line 24 "lib/unicase/special-casing-table.gperf"
      {"\000\315\000", 0,  SCC_ALWAYS           , {  'l',  't' }, { 0x00CD,      0,      0 }, { 0x0069, 0x0307, 0x0301 }, { 0x00CD,      0,      0 }, { 0x00ED,      0,      0 }},
#line 108 "lib/unicase/special-casing-table.gperf"
      {"\037\307\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0397, 0x0342, 0x0399 }, { 0x1FC7,      0,      0 }, { 0x0397, 0x0342, 0x0345 }, { 0x03B7, 0x0342, 0x03B9 }},
#line 107 "lib/unicase/special-casing-table.gperf"
      {"\037\306\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0397, 0x0342,      0 }, { 0x1FC6,      0,      0 }, { 0x0397, 0x0342,      0 }, { 0x03B7, 0x0342,      0 }},
#line 106 "lib/unicase/special-casing-table.gperf"
      {"\037\304\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0389, 0x0399,      0 }, { 0x1FC4,      0,      0 }, { 0x0389, 0x0345,      0 }, { 0x03AE, 0x03B9,      0 }},
#line 105 "lib/unicase/special-casing-table.gperf"
      {"\037\303\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0397, 0x0399,      0 }, { 0x1FC3,      0,      0 }, { 0x1FCC,      0,      0 }, { 0x03B7, 0x03B9,      0 }},
#line 104 "lib/unicase/special-casing-table.gperf"
      {"\037\302\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1FCA, 0x0399,      0 }, { 0x1FC2,      0,      0 }, { 0x1FCA, 0x0345,      0 }, { 0x1F74, 0x03B9,      0 }},
#line 103 "lib/unicase/special-casing-table.gperf"
      {"\037\274\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0391, 0x0399,      0 }, { 0x1FB3,      0,      0 }, { 0x1FBC,      0,      0 }, { 0x03B1, 0x03B9,      0 }},
#line 102 "lib/unicase/special-casing-table.gperf"
      {"\037\267\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0391, 0x0342, 0x0399 }, { 0x1FB7,      0,      0 }, { 0x0391, 0x0342, 0x0345 }, { 0x03B1, 0x0342, 0x03B9 }},
#line 101 "lib/unicase/special-casing-table.gperf"
      {"\037\266\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0391, 0x0342,      0 }, { 0x1FB6,      0,      0 }, { 0x0391, 0x0342,      0 }, { 0x03B1, 0x0342,      0 }},
#line 100 "lib/unicase/special-casing-table.gperf"
      {"\037\264\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0386, 0x0399,      0 }, { 0x1FB4,      0,      0 }, { 0x0386, 0x0345,      0 }, { 0x03AC, 0x03B9,      0 }},
#line 99 "lib/unicase/special-casing-table.gperf"
      {"\037\263\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0391, 0x0399,      0 }, { 0x1FB3,      0,      0 }, { 0x1FBC,      0,      0 }, { 0x03B1, 0x03B9,      0 }},
#line 98 "lib/unicase/special-casing-table.gperf"
      {"\037\262\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1FBA, 0x0399,      0 }, { 0x1FB2,      0,      0 }, { 0x1FBA, 0x0345,      0 }, { 0x1F70, 0x03B9,      0 }},
#line 38 "lib/unicase/special-casing-table.gperf"
      {"\003\260\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0308, 0x0301 }, { 0x03B0,      0,      0 }, { 0x03A5, 0x0308, 0x0301 }, { 0x03C5, 0x0308, 0x0301 }},
#line 97 "lib/unicase/special-casing-table.gperf"
      {"\037\257\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6F, 0x0399,      0 }, { 0x1FA7,      0,      0 }, { 0x1FAF,      0,      0 }, { 0x1F67, 0x03B9,      0 }},
#line 96 "lib/unicase/special-casing-table.gperf"
      {"\037\256\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6E, 0x0399,      0 }, { 0x1FA6,      0,      0 }, { 0x1FAE,      0,      0 }, { 0x1F66, 0x03B9,      0 }},
#line 95 "lib/unicase/special-casing-table.gperf"
      {"\037\255\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6D, 0x0399,      0 }, { 0x1FA5,      0,      0 }, { 0x1FAD,      0,      0 }, { 0x1F65, 0x03B9,      0 }},
#line 94 "lib/unicase/special-casing-table.gperf"
      {"\037\254\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6C, 0x0399,      0 }, { 0x1FA4,      0,      0 }, { 0x1FAC,      0,      0 }, { 0x1F64, 0x03B9,      0 }},
#line 93 "lib/unicase/special-casing-table.gperf"
      {"\037\253\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6B, 0x0399,      0 }, { 0x1FA3,      0,      0 }, { 0x1FAB,      0,      0 }, { 0x1F63, 0x03B9,      0 }},
#line 92 "lib/unicase/special-casing-table.gperf"
      {"\037\252\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6A, 0x0399,      0 }, { 0x1FA2,      0,      0 }, { 0x1FAA,      0,      0 }, { 0x1F62, 0x03B9,      0 }},
#line 91 "lib/unicase/special-casing-table.gperf"
      {"\037\251\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F69, 0x0399,      0 }, { 0x1FA1,      0,      0 }, { 0x1FA9,      0,      0 }, { 0x1F61, 0x03B9,      0 }},
#line 90 "lib/unicase/special-casing-table.gperf"
      {"\037\250\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F68, 0x0399,      0 }, { 0x1FA0,      0,      0 }, { 0x1FA8,      0,      0 }, { 0x1F60, 0x03B9,      0 }},
#line 89 "lib/unicase/special-casing-table.gperf"
      {"\037\247\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6F, 0x0399,      0 }, { 0x1FA7,      0,      0 }, { 0x1FAF,      0,      0 }, { 0x1F67, 0x03B9,      0 }},
#line 88 "lib/unicase/special-casing-table.gperf"
      {"\037\246\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6E, 0x0399,      0 }, { 0x1FA6,      0,      0 }, { 0x1FAE,      0,      0 }, { 0x1F66, 0x03B9,      0 }},
#line 87 "lib/unicase/special-casing-table.gperf"
      {"\037\245\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6D, 0x0399,      0 }, { 0x1FA5,      0,      0 }, { 0x1FAD,      0,      0 }, { 0x1F65, 0x03B9,      0 }},
#line 86 "lib/unicase/special-casing-table.gperf"
      {"\037\244\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6C, 0x0399,      0 }, { 0x1FA4,      0,      0 }, { 0x1FAC,      0,      0 }, { 0x1F64, 0x03B9,      0 }},
#line 84 "lib/unicase/special-casing-table.gperf"
      {"\037\242\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F6A, 0x0399,      0 }, { 0x1FA2,      0,      0 }, { 0x1FAA,      0,      0 }, { 0x1F62, 0x03B9,      0 }},
#line 83 "lib/unicase/special-casing-table.gperf"
      {"\037\241\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F69, 0x0399,      0 }, { 0x1FA1,      0,      0 }, { 0x1FA9,      0,      0 }, { 0x1F61, 0x03B9,      0 }},
#line 82 "lib/unicase/special-casing-table.gperf"
      {"\037\240\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F68, 0x0399,      0 }, { 0x1FA0,      0,      0 }, { 0x1FA8,      0,      0 }, { 0x1F60, 0x03B9,      0 }},
#line 81 "lib/unicase/special-casing-table.gperf"
      {"\037\237\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2F, 0x0399,      0 }, { 0x1F97,      0,      0 }, { 0x1F9F,      0,      0 }, { 0x1F27, 0x03B9,      0 }},
#line 79 "lib/unicase/special-casing-table.gperf"
      {"\037\235\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2D, 0x0399,      0 }, { 0x1F95,      0,      0 }, { 0x1F9D,      0,      0 }, { 0x1F25, 0x03B9,      0 }},
#line 78 "lib/unicase/special-casing-table.gperf"
      {"\037\234\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2C, 0x0399,      0 }, { 0x1F94,      0,      0 }, { 0x1F9C,      0,      0 }, { 0x1F24, 0x03B9,      0 }},
#line 77 "lib/unicase/special-casing-table.gperf"
      {"\037\233\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2B, 0x0399,      0 }, { 0x1F93,      0,      0 }, { 0x1F9B,      0,      0 }, { 0x1F23, 0x03B9,      0 }},
#line 71 "lib/unicase/special-casing-table.gperf"
      {"\037\225\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2D, 0x0399,      0 }, { 0x1F95,      0,      0 }, { 0x1F9D,      0,      0 }, { 0x1F25, 0x03B9,      0 }},
#line 70 "lib/unicase/special-casing-table.gperf"
      {"\037\224\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2C, 0x0399,      0 }, { 0x1F94,      0,      0 }, { 0x1F9C,      0,      0 }, { 0x1F24, 0x03B9,      0 }},
#line 69 "lib/unicase/special-casing-table.gperf"
      {"\037\223\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2B, 0x0399,      0 }, { 0x1F93,      0,      0 }, { 0x1F9B,      0,      0 }, { 0x1F23, 0x03B9,      0 }},
#line 68 "lib/unicase/special-casing-table.gperf"
      {"\037\222\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F2A, 0x0399,      0 }, { 0x1F92,      0,      0 }, { 0x1F9A,      0,      0 }, { 0x1F22, 0x03B9,      0 }},
#line 67 "lib/unicase/special-casing-table.gperf"
      {"\037\221\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F29, 0x0399,      0 }, { 0x1F91,      0,      0 }, { 0x1F99,      0,      0 }, { 0x1F21, 0x03B9,      0 }},
#line 65 "lib/unicase/special-casing-table.gperf"
      {"\037\217\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0F, 0x0399,      0 }, { 0x1F87,      0,      0 }, { 0x1F8F,      0,      0 }, { 0x1F07, 0x03B9,      0 }},
#line 64 "lib/unicase/special-casing-table.gperf"
      {"\037\216\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0E, 0x0399,      0 }, { 0x1F86,      0,      0 }, { 0x1F8E,      0,      0 }, { 0x1F06, 0x03B9,      0 }},
#line 63 "lib/unicase/special-casing-table.gperf"
      {"\037\215\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0D, 0x0399,      0 }, { 0x1F85,      0,      0 }, { 0x1F8D,      0,      0 }, { 0x1F05, 0x03B9,      0 }},
#line 62 "lib/unicase/special-casing-table.gperf"
      {"\037\214\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0C, 0x0399,      0 }, { 0x1F84,      0,      0 }, { 0x1F8C,      0,      0 }, { 0x1F04, 0x03B9,      0 }},
#line 61 "lib/unicase/special-casing-table.gperf"
      {"\037\213\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0B, 0x0399,      0 }, { 0x1F83,      0,      0 }, { 0x1F8B,      0,      0 }, { 0x1F03, 0x03B9,      0 }},
#line 60 "lib/unicase/special-casing-table.gperf"
      {"\037\212\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0A, 0x0399,      0 }, { 0x1F82,      0,      0 }, { 0x1F8A,      0,      0 }, { 0x1F02, 0x03B9,      0 }},
#line 59 "lib/unicase/special-casing-table.gperf"
      {"\037\211\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F09, 0x0399,      0 }, { 0x1F81,      0,      0 }, { 0x1F89,      0,      0 }, { 0x1F01, 0x03B9,      0 }},
#line 58 "lib/unicase/special-casing-table.gperf"
      {"\037\210\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F08, 0x0399,      0 }, { 0x1F80,      0,      0 }, { 0x1F88,      0,      0 }, { 0x1F00, 0x03B9,      0 }},
#line 56 "lib/unicase/special-casing-table.gperf"
      {"\037\206\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0E, 0x0399,      0 }, { 0x1F86,      0,      0 }, { 0x1F8E,      0,      0 }, { 0x1F06, 0x03B9,      0 }},
#line 55 "lib/unicase/special-casing-table.gperf"
      {"\037\205\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0D, 0x0399,      0 }, { 0x1F85,      0,      0 }, { 0x1F8D,      0,      0 }, { 0x1F05, 0x03B9,      0 }},
#line 54 "lib/unicase/special-casing-table.gperf"
      {"\037\204\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0C, 0x0399,      0 }, { 0x1F84,      0,      0 }, { 0x1F8C,      0,      0 }, { 0x1F04, 0x03B9,      0 }},
#line 53 "lib/unicase/special-casing-table.gperf"
      {"\037\203\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0B, 0x0399,      0 }, { 0x1F83,      0,      0 }, { 0x1F8B,      0,      0 }, { 0x1F03, 0x03B9,      0 }},
#line 52 "lib/unicase/special-casing-table.gperf"
      {"\037\202\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F0A, 0x0399,      0 }, { 0x1F82,      0,      0 }, { 0x1F8A,      0,      0 }, { 0x1F02, 0x03B9,      0 }},
#line 51 "lib/unicase/special-casing-table.gperf"
      {"\037\201\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F09, 0x0399,      0 }, { 0x1F81,      0,      0 }, { 0x1F89,      0,      0 }, { 0x1F01, 0x03B9,      0 }},
#line 50 "lib/unicase/special-casing-table.gperf"
      {"\037\200\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x1F08, 0x0399,      0 }, { 0x1F80,      0,      0 }, { 0x1F88,      0,      0 }, { 0x1F00, 0x03B9,      0 }},
#line 49 "lib/unicase/special-casing-table.gperf"
      {"\037V\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0313, 0x0342 }, { 0x1F56,      0,      0 }, { 0x03A5, 0x0313, 0x0342 }, { 0x03C5, 0x0313, 0x0342 }},
#line 48 "lib/unicase/special-casing-table.gperf"
      {"\037T\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0313, 0x0301 }, { 0x1F54,      0,      0 }, { 0x03A5, 0x0313, 0x0301 }, { 0x03C5, 0x0313, 0x0301 }},
#line 47 "lib/unicase/special-casing-table.gperf"
      {"\037R\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0313, 0x0300 }, { 0x1F52,      0,      0 }, { 0x03A5, 0x0313, 0x0300 }, { 0x03C5, 0x0313, 0x0300 }},
#line 46 "lib/unicase/special-casing-table.gperf"
      {"\037P\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x03A5, 0x0313,      0 }, { 0x1F50,      0,      0 }, { 0x03A5, 0x0313,      0 }, { 0x03C5, 0x0313,      0 }},
#line 20 "lib/unicase/special-casing-table.gperf"
      {"\000J\000", 0,  SCC_MORE_ABOVE       , {  'l',  't' }, { 0x004A,      0,      0 }, { 0x006A, 0x0307,      0 }, { 0x004A,      0,      0 }, { 0x006A,      0,      0 }},
#line 27 "lib/unicase/special-casing-table.gperf"
      {"\001.\000", 0,  SCC_MORE_ABOVE       , {  'l',  't' }, { 0x012E,      0,      0 }, { 0x012F, 0x0307,      0 }, { 0x012E,      0,      0 }, { 0x012F,      0,      0 }},
#line 26 "lib/unicase/special-casing-table.gperf"
      {"\001(\000", 0,  SCC_ALWAYS           , {  'l',  't' }, { 0x0128,      0,      0 }, { 0x0069, 0x0307, 0x0303 }, { 0x0128,      0,      0 }, { 0x0129,      0,      0 }},
#line 136 "lib/unicase/special-casing-table.gperf"
      {"\373\027\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0544, 0x053D,      0 }, { 0xFB17,      0,      0 }, { 0x0544, 0x056D,      0 }, { 0x0574, 0x056D,      0 }},
#line 135 "lib/unicase/special-casing-table.gperf"
      {"\373\026\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x054E, 0x0546,      0 }, { 0xFB16,      0,      0 }, { 0x054E, 0x0576,      0 }, { 0x057E, 0x0576,      0 }},
#line 134 "lib/unicase/special-casing-table.gperf"
      {"\373\025\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0544, 0x053B,      0 }, { 0xFB15,      0,      0 }, { 0x0544, 0x056B,      0 }, { 0x0574, 0x056B,      0 }},
#line 133 "lib/unicase/special-casing-table.gperf"
      {"\373\024\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0544, 0x0535,      0 }, { 0xFB14,      0,      0 }, { 0x0544, 0x0565,      0 }, { 0x0574, 0x0565,      0 }},
#line 132 "lib/unicase/special-casing-table.gperf"
      {"\373\023\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0544, 0x0546,      0 }, { 0xFB13,      0,      0 }, { 0x0544, 0x0576,      0 }, { 0x0574, 0x0576,      0 }},
#line 131 "lib/unicase/special-casing-table.gperf"
      {"\373\006\000", 0,  SCC_ALWAYS           , { '\0', '\0' }, { 0x0053, 0x0054,      0 }, { 0xFB06,      0,      0 }, { 0x0053, 0x0074,      0 }, { 0x0073, 0x0074,      0 }}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = gl_unicase_special_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].code;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
