dnl Autoconf macros to check for the Berkeley DB library

dnl -- check header for minimum version and return version in
dnl -- $atalk_cv_bdb_MAJOR and $atalk_cv_bdb_MINOR
AC_DEFUN([NETATALK_BDB_HEADER],[
    dnl check for header version
    AC_MSG_CHECKING([$1/db.h version >= ${DB_MAJOR_REQ}.${DB_MINOR_REQ}.${DB_PATCH_REQ}])

    atalk_cv_bdb_MAJOR=`grep DB_VERSION_MAJOR "$1/db.h" | cut -f 3`
    atalk_cv_bdb_MINOR=`grep DB_VERSION_MINOR "$1/db.h" | cut -f 3`

    if test $atalk_cv_bdb_MAJOR -gt $DB_MAJOR_REQ ; then
        AC_MSG_RESULT([yes])
        atalk_cv_bdbheader=yes
    elif test $DB_MAJOR_REQ -gt $atalk_cv_bdb_MAJOR ; then
        AC_MSG_RESULT([no])
        atalk_cv_bdbheader=no
    elif test $DB_MINOR_REQ -gt $atalk_cv_bdb_MINOR ; then
        AC_MSG_RESULT([no])
        atalk_cv_bdbheader=no
    else
        AC_MSG_RESULT([yes])
        atalk_cv_bdbheader=yes
    fi
])

dnl -- Try to link and run with lib with version taken from
dnl -- $atalk_cv_bdb_MAJOR and $atalk_cv_bdb_MINOR
AC_DEFUN([NETATALK_BDB_TRY_LINK],[
    atalk_cv_bdb_version=no
    maj=$atalk_cv_bdb_MAJOR
    min=$atalk_cv_bdb_MINOR
    atalk_cv_bdb_try_libs="db$maj$min db$maj.$min db-$maj$min db-$maj.$min db$maj-$maj.$min db"

    for lib in $atalk_cv_bdb_try_libs ; do
        LIBS="-l$lib $savedlibs"
        AC_MSG_CHECKING([Berkeley DB library (-l$lib)])
        AC_TRY_RUN([
            #include <stdio.h>
            #include <db.h>
            int main(void) {
                int major, minor, patch;
                char *version_str;
                version_str = db_version(&major, &minor, &patch);
                if ((major*100 + minor*10 + patch) < (DB_MAJOR_REQ*100 + DB_MINOR_REQ*10 + DB_PATCH_REQ)) {
                    printf("linking wrong library version (%d.%d.%d), ",major, minor, patch);
                    return (2);
                }
                if ( major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR || patch != DB_VERSION_PATCH) {
                    printf("header/library version mismatch (%d.%d.%d/%d.%d.%d), ",
                        DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH, major, minor, patch);
                    return (3);
                }
                printf("%d.%d.%d ... ",major, minor, patch);
                return (0);
            }
        ],[
            AC_MSG_RESULT(yes)
            atalk_cv_bdb_version="yes"
            atalk_cv_lib_db="-l$lib"
            break
        ],[
            AC_MSG_RESULT(no)
        ],[
            bdblibs=`ls $bdblibdir/lib$lib.* 2>/dev/null`
            for bdblib in $bdblibs ; do
                echo "Testing for lib file $bdblib" >&AS_MESSAGE_LOG_FD
                if test -f "$bdblib" ; then
                    AC_MSG_RESULT([yes (cross-compiling)])
                    atalk_cv_bdb_version="yes"
                    atalk_cv_lib_db="-l$lib"
                    break
                fi
            done
            if test "x$atalk_cv_bdb_version" = "xyes" ; then
                break
            fi
            AC_MSG_RESULT([no (cross-compiling)])
        ])
    done
    LIBS="$savedlibs"
])

dnl -- This is called from configure
AC_DEFUN([AC_NETATALK_PATH_BDB],[
if test "x$bdb_required" = "xyes"; then
    trybdbdir=""
    dobdbsearch=yes
    bdb_search_dirs="/usr/local /usr"
    search_subdirs="/ /db5 /db5.1 /db51 /db5.0 /db50 /db4.8 /db48 /db4.7 /db47 /db4.6 /db46 /db4"

    bdbfound=no
    savedcflags="$CFLAGS"
    savedldflags="$LDFLAGS"
    savedcppflags="$CPPFLAGS"
    savedlibs="$LIBS"
    saved_shlibpath_var=$shlibpath_var

    dnl required BDB version: 4.6, because of cursor API change
    DB_MAJOR_REQ=4
    DB_MINOR_REQ=6
    DB_PATCH_REQ=0

    dnl make sure atalk_libname is defined beforehand
    [[ -n "$atalk_libname" ]] || AC_MSG_ERROR([internal error, atalk_libname undefined])
    saved_atalk_libname=$atalk_libname

    dnl define the required BDB version
    AC_DEFINE_UNQUOTED(DB_MAJOR_REQ, ${DB_MAJOR_REQ}, [Required BDB version, major])
    AC_DEFINE_UNQUOTED(DB_MINOR_REQ, ${DB_MINOR_REQ}, [Required BDB version, minor])
    AC_DEFINE_UNQUOTED(DB_PATCH_REQ, ${DB_PATCH_REQ}, [Required BDB version, patch])

    AC_ARG_WITH(bdb,
        [  --with-bdb=PATH         specify path to Berkeley DB installation[[auto]]],
        if test "x$withval" = "xno"; then
            dobdbsearch=no
        elif test "x$withval" = "xyes"; then
            dobdbsearch=yes
        else
            bdb_search_dirs="$withval"
        fi
    )

    if test "x$dobdbsearch" = "xyes"; then
        for bdbdir in $bdb_search_dirs; do
            if test $bdbfound = "yes"; then
                break;
            fi
            for subdir in ${search_subdirs}; do
                AC_MSG_CHECKING([for Berkeley DB headers in ${bdbdir}/include${subdir}])
                dnl -- First check the mere existence of the header
                if test -f "${bdbdir}/include${subdir}/db.h" ; then
                    AC_MSG_RESULT([yes])

                    dnl -- Check if it meets minimun requirement, also return the version
                    NETATALK_BDB_HEADER([${bdbdir}/include${subdir}])

                    if test ${atalk_cv_bdbheader} != "no"; then
                        bdblibdir="${bdbdir}/${atalk_libname}"
                        bdbbindir="${bdbdir}/bin"

                        CPPFLAGS="-I${bdbdir}/include${subdir} $CPPFLAGS"
                        LDFLAGS="-L$bdblibdir $LDFLAGS"

                        dnl -- Uses version set by NETATALK_BDB_HEADER to try to run
                        dnl -- a conftest that checks that header/lib version match
                        dnl -- $shlibpath_var is set by LIBTOOL, its value is
                        dnl -- LD_LIBRARY_PATH on many platforms. This will be fairly
                        dnl -- portable hopefully. Reference:
                        dnl -- http://lists.gnu.org/archive/html/autoconf/2009-03/msg00040.html
                        eval export $shlibpath_var=$bdblibdir
                        NETATALK_BDB_TRY_LINK
                        eval export $shlibpath_var=$saved_shlibpath_var

                        if test x"${atalk_cv_bdb_version}" = x"yes"; then
                            BDB_CFLAGS="-I${bdbdir}/include${subdir}"
                            BDB_LIBS="-L${bdblibdir} ${atalk_cv_lib_db}"
                            if test x"$need_dash_r" = x"yes"; then
                                BDB_LIBS="$BDB_LIBS -R${bdblibdir}"
                            fi
                            BDB_BIN="$bdbbindir"
                            BDB_PATH="$bdbdir"
                            bdbfound=yes
                            break;
                        fi

                        dnl -- Search lib in "lib" too, as $atalk_libname might be set
                        dnl -- to "lib64" or "lib/64" which would not be found above
                        dnl -- if 64bit lib were installed in a dir named "lib"
                        if test x"$atalk_libname" != x"lib" ; then
                           bdblibdir="${bdbdir}/lib"
                           bdbbindir="${bdbdir}/bin"

                           CPPFLAGS="-I${bdbdir}/include${subdir} $CPPFLAGS"
                           LDFLAGS="-L$bdblibdir $LDFLAGS"

                           eval export $shlibpath_var=$bdblibdir
                           NETATALK_BDB_TRY_LINK
                           eval export $shlibpath_var=$saved_shlibpath_var

                           if test x"${atalk_cv_bdb_version}" = x"yes"; then
                              BDB_CFLAGS="-I${bdbdir}/include${subdir}"
                              BDB_LIBS="-L${bdblibdir} ${atalk_cv_lib_db}"
                              if test x"$need_dash_r" = x"yes"; then
                                 BDB_LIBS="$BDB_LIBS -R${bdblibdir}"
                              fi
                              BDB_BIN="$bdbbindir"
                              BDB_PATH="$bdbdir"
                              bdbfound=yes
                              break;
                           fi
                        fi
                    fi
                    CFLAGS="$savedcflags"
                    LDFLAGS="$savedldflags"
                    CPPFLAGS="$savedcppflags"
                    LIBS="$savedlibs"
                else
                    AC_MSG_RESULT([no])
                fi
            done
        done
    fi

    CFLAGS="$savedcflags"
    LDFLAGS="$savedldflags"
    CPPFLAGS="$savedcppflags"
    LIBS="$savedlibs"
    atalk_libname=$saved_atalk_libname

    if test "x$bdbfound" = "xyes"; then
        ifelse([$1], , :, [$1])
    else
        ifelse([$2], , :, [$2])     
		AC_MSG_ERROR([Berkeley DB library required but not found!])
    fi

    CFLAGS_REMOVE_USR_INCLUDE(BDB_CFLAGS)
    LIB_REMOVE_USR_LIB(BDB_LIBS)
    AC_SUBST(BDB_CFLAGS)
    AC_SUBST(BDB_LIBS)
    AC_SUBST(BDB_BIN)
    AC_SUBST(BDB_PATH)
fi
])


