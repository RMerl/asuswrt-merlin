[SUBSYSTEM::LIBPOPT]
CFLAGS = -I$(poptsrcdir)

LIBPOPT_OBJ_FILES = $(addprefix $(poptsrcdir)/, findme.o popt.o poptconfig.o popthelp.o poptparse.o)

