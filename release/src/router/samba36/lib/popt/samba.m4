m4_include(../lib/popt/libpopt.m4)

if test x"$POPT_OBJ" = "x"; then
	SMB_EXT_LIB(LIBPOPT, [${POPT_LIBS}])
else
	SMB_INCLUDE_MK(../lib/popt/config.mk)
fi

