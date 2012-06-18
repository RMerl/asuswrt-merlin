################################################
# Start SUBSYSTEM CHARSET
[SUBSYSTEM::CHARSET]
PUBLIC_DEPENDENCIES = ICONV
PRIVATE_DEPENDENCIES = DYNCONFIG
# End SUBSYSTEM CHARSET
################################################

CHARSET_OBJ_FILES = $(addprefix $(libcharsetsrcdir)/, iconv.o charcnv.o util_unistr.o codepoints.o)

PUBLIC_HEADERS += $(libcharsetsrcdir)/charset.h
