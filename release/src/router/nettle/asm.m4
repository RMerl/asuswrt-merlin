divert(-1)
changequote(<,>)dnl
dnl (progn (modify-syntax-entry ?< "(>") (modify-syntax-entry ?> ")<") )

dnl FORTRAN style comment character
define(<C>, <
dnl>)dnl
dnl Disable m4 comment processing, since the default, #, is used for
dnl constants on some architectures, in particular ARM.
changecom()dnl

dnl Including files from the srcdir
define(<include_src>, <include(srcdir/$1)>)dnl

dnl default definition, changed in fat builds
define(<fat_transform>, <$1>)
define(<C_NAME>, <SYMBOL_PREFIX<>fat_transform($1)>)

dnl Pseudo ops
define(<DECLARE_FUNC>,
<ifelse(ELF_STYLE,yes,
<.type $1,TYPE_FUNCTION>,
COFF_STYLE, yes,
<.def $1
.scl 2
.type 32
.endef>,
<>)>)

define(<GMP_NUMB_BITS>,<>)dnl

define(<PROLOGUE>,
<.globl C_NAME($1)
DECLARE_FUNC(C_NAME($1))
C_NAME($1):>)

define(<EPILOGUE>,
<ifelse(ELF_STYLE,yes,
<.size C_NAME($1), . - C_NAME($1)>,<>)>)

define(<m4_log2>, <m4_log2_internal($1,1,0)>)
define(<m4_log2_internal>,
<ifelse($3, 10, <not-a-power-of-two>,
$1, $2, $3,
<m4_log2_internal($1, eval(2*$2), eval(1 + $3))>)>)

dnl Argument to ALIGN is always in bytes, and converted to a
dnl logarithmic .align if necessary.

define(<ALIGN>,
<.align ifelse(ALIGN_LOG,yes,<m4_log2($1)>,$1)
>)

dnl Struct defining macros

dnl STRUCTURE(prefix) 
define(<STRUCTURE>, <define(<SOFFSET>, 0)define(<SPREFIX>, <$1>)>)dnl

dnl STRUCT(name, size)
define(<STRUCT>,
<define(SPREFIX<_>$1, SOFFSET)dnl
 define(<SOFFSET>, eval(SOFFSET + ($2)))>)dnl

dnl UCHAR(name)
define(<UCHAR>, <STRUCT(<$1>, 1)>)dnl

dnl UNSIGNED(name)
define(<UNSIGNED>, <STRUCT(<$1>, 4)>)dnl

dnl Offsets in arcfour_ctx
STRUCTURE(ARCFOUR)
  STRUCT(S, 256)
  UCHAR(I)
  UCHAR(J)

dnl Offsets in aes_table
define(AES_SBOX_SIZE,	256)dnl
define(AES_TABLE_SIZE,	1024)dnl

STRUCTURE(AES)
  STRUCT(SBOX, AES_SBOX_SIZE)
  STRUCT(TABLE0, AES_TABLE_SIZE)
  STRUCT(TABLE1, AES_TABLE_SIZE)
  STRUCT(TABLE2, AES_TABLE_SIZE)
  STRUCT(TABLE3, AES_TABLE_SIZE)

C For 64-bit implementation
STRUCTURE(P1305)
  STRUCT(R0, 8)
  STRUCT(R1, 8)
  STRUCT(S1, 8)
  STRUCT(PAD, 12)
  STRUCT(H2, 4)
  STRUCT(H0, 8)
  STRUCT(H1, 8)

divert
