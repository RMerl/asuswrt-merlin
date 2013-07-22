include $(top_srcdir)/config/include-Makefile.am

dist_man_MANS = uuid.3 uuid_clear.3 uuid_compare.3 uuid_copy.3 uuid_generate.3 \
		uuid_is_null.3 uuid_parse.3 uuid_time.3 uuid_unparse.3

UUID_GENERATE_LINKS = uuid_generate_random.3 uuid_generate_time.3 uuid_generate_time_safe.3

man_MANS = $(UUID_GENERATE_LINKS)
CLEANFILES = $(man_MANS)

$(UUID_GENERATE_LINKS):
	$(AM_V_GEN)echo ".so man3/uuid_generate.3" > $@
