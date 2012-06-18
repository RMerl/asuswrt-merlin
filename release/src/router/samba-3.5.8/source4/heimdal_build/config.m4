
external_heimdal=no
AC_MSG_CHECKING([Whether to use external heimdal libraries])
AC_ARG_ENABLE(external-heimdal,
[  --enable-external-heimdal Enable external heimdal libraries (experimental,default=no)],
[ external_heimdal=$enableval ],
[ external_heimdal=no ])
AC_MSG_RESULT($external_heimdal)

if test x"$external_heimdal" = x"yes"; then

# external_heimdal_start
m4_include(heimdal_build/external.m4)
# external_heimdal_end

else

# internal_heimdal_start
m4_include(heimdal_build/internal.m4)
# internal_heimdal_end

fi

