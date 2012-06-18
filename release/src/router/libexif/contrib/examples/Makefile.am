# Simple example programs
check_PROGRAMS = photographer thumbnail write-exif

# Example programs with dependencies other than plain libexif
COMPLICATED_EXAMPLES = cam_features.c

# Build just the simple examples as a sanity check, but include them all in
# the source archive

EXTRA_DIST = $(COMPLICATED_EXAMPLES)
LDADD = $(top_builddir)/libexif/libexif.la
