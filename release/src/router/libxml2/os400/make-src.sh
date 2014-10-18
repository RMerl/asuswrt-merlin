#!/bin/sh
#
#       libxml compilation script for the OS/400.
#
#       See Copyright for the status of this software.
#
#       Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.
#

SCRIPTDIR=`dirname "${0}"`
. "${SCRIPTDIR}/initscript.sh"
cd "${TOPDIR}"


#      Create and compile the identification source file.

echo '#pragma comment(user, "libxml2 version '"${LIBXML_VERSION}"'")' > os400.c
echo '#pragma comment(user, __DATE__)' >> os400.c
echo '#pragma comment(user, __TIME__)' >> os400.c
echo '#pragma comment(copyright, "Copyright (C) 1998-2014 Daniel Veillard. OS/400 version by P. Monnerat.")' >> os400.c
make_module     OS400           os400.c
LINK=                           # No need to rebuild service program yet.
MODULES=


#       Get source list.

foldlines()

{
        sed -e ':begin'                                                 \
            -e '/\\$/{'                                                 \
            -e 's/\\$/ /'                                               \
            -e 'N'                                                      \
            -e 'bbegin'                                                 \
            -e '}'                                                      \
            -e 's/\n//g'                                                \
            -e 's/[[:space:]]*$//'
}


get_make_var()

{
        foldlines < Makefile.am                                         |
        sed -e "/^${1}[[:space:]]*=[[:space:]]*/{"                      \
            -e 's///'                                                   \
            -e 'q'                                                      \
            -e '}'                                                      \
            -e 'd'
}


docb_sources=`get_make_var docb_sources`
trio_sources=`get_make_var trio_sources`
CSOURCES=`eval echo "\`get_make_var libxml2_la_SOURCES | tr '()' '{}'\`"`


#       Compile the sources into modules.

INCLUDES="'`pwd`'"

#       OS/400 specific modules first.

make_module     DLFCN           "${SCRIPTDIR}/dlfcn/dlfcn.c"    ''      ebcdic
make_module     ICONV           "${SCRIPTDIR}/iconv/iconv.c"    ''      ebcdic
make_module     WRAPPERS        "${SCRIPTDIR}/wrappers.c"       ''      ebcdic
make_module     TRANSCODE       "${SCRIPTDIR}/transcode.c"
make_module     RPGSUPPORT      "${SCRIPTDIR}/rpgsupport.c"

#       Regular libxml2 modules.

for SRC in ${CSOURCES}
do      MODULE=`db2_name "${SRC}"`
        make_module "${MODULE}" "${SRC}"
done


#       If needed, (re)create the static binding directory.

if action_needed "${LIBIFSNAME}/${STATBNDDIR}.BNDDIR"
then    LINK=YES
fi

if [ "${LINK}" ]
then    rm -rf "${LIBIFSNAME}/${STATBNDDIR}.BNDDIR"
        CMD="CRTBNDDIR BNDDIR(${TARGETLIB}/${STATBNDDIR})"
        CMD="${CMD} TEXT('libxml2 static binding directory')"
        system "${CMD}"

        for MODULE in ${MODULES}
        do      CMD="ADDBNDDIRE BNDDIR(${TARGETLIB}/${STATBNDDIR})"
                CMD="${CMD} OBJ((${TARGETLIB}/${MODULE} *MODULE))"
                system "${CMD}"
        done
fi


#       The exportation file for service program creation must be in a DB2
#               source file, so make sure it exists.

if action_needed "${LIBIFSNAME}/TOOLS.FILE"
then    CMD="CRTSRCPF FILE(${TARGETLIB}/TOOLS) RCDLEN(112)"
        CMD="${CMD} CCSID(${TGTCCSID}) TEXT('libxml2: build tools')"
        system "${CMD}"
fi


#       Generate all exported symbol table versions in a binding source file.

BSF="${LIBIFSNAME}/TOOLS.FILE/BNDSRC.MBR"
PGMEXPS=

OS400SYMS=`cat os400/transcode.h os400/rpgsupport.h                     |
           sed -e 'H'                                                   \
               -e 'g'                                                   \
               -e 's/\n/ /'                                             \
               -e 's/\\$/ /'                                            \
               -e 's/.*/& /'                                            \
               -e 's/\/\*.*\*\// /g'                                    \
               -e 'h'                                                   \
               -e ':loop'                                               \
               -e 'g'                                                   \
               -e '/\/\*/d'                                             \
               -e '/;/!d'                                               \
               -e 's/[^;]*;//'                                          \
               -e 'x'                                                   \
               -e 's/[[:space:]]*;.*$//'                                \
               -e '/XMLPUBFUN/{'                                        \
               -e 's/[[:space:]]*(.*$//'                                \
               -e 's/.*[[:space:]*]//'                                  \
               -e 'p'                                                   \
               -e 'bloop'                                               \
               -e '}'                                                   \
               -e '/XMLPUBVAR/!bloop'                                   \
               -e ':loop2'                                              \
               -e '/\[[^][]*\]/{'                                       \
               -e 's///'                                                \
               -e 'bloop2'                                              \
               -e '}'                                                   \
               -e 's/[[:space:]]*,[[:space:]]*/,/g'                     \
               -e 's/[^,]*[[:space:]*]//'                               \
               -e 's/[^[:alnum:]_,]//g'                                 \
               -e 's/,/\n/g'                                            \
               -e 'p'                                                   \
               -e 'bloop'`

sed -e 's/#.*//'                                                        \
    -e 's/[[:space:]]*$//'                                              \
    -e 's/^[[:space:]]*//'                                              \
    -e '/^*global:$/d'                                                  \
    -e '/^$/d'                                                          \
    -e '/[[:space:]]*{$/{'                                              \
    -e   's///'                                                         \
    -e   'h'                                                            \
    -e   's/[^A-Za-z0-9]/_/g'                                           \
    -e   's/^[0-9]/_&/'                                                 \
    -e   'x'                                                            \
    -e   'G'                                                            \
    -e   's/\(.*\)\n\(.*\)/\2_SIGNATURE="\1"/'                          \
    -e   'p'                                                            \
    -e   's/.*//'                                                       \
    -e   'x'                                                            \
    -e   "s/.*/SONAME='&'/"                                             \
    -e   'b'                                                            \
    -e '}'                                                              \
    -e '/[[:space:]]*;$/!d'                                             \
    -e 's///'                                                           \
    -e '/^xmlDllMain$/d'                                                \
    -e '/^}[[:space:]]*/!{'                                             \
    -e   'H'                                                            \
    -e   'd'                                                            \
    -e '}'                                                              \
    -e 's///'                                                           \
    -e '/^$/!{'                                                         \
    -e   's/[^A-Za-z0-9]/_/g'                                           \
    -e   's/^[0-9]/_&/'                                                 \
    -e   's/.*/${&}/'                                                   \
    -e   'x'                                                            \
    -e   'H'                                                            \
    -e   's/.*//'                                                       \
    -e '}'                                                              \
    -e 'x'                                                              \
    -e 's/\n/ /g'                                                       \
    -e 's/^[[:space:]]*//'                                              \
    -e 's/.*/declare ${SONAME}="&"/'                                    \
    -e 's/.*/&; PGMEXPS="${SONAME} ${PGMEXPS}"/'                        \
        < "${TOPDIR}/libxml2.syms"    > bndvars
. ./bndvars

PGMLVL=CURRENT
for PGMEXP in ${PGMEXPS}
do      SIGNATURE=`echo "${PGMEXP}" | sed 's/^LIBXML2_//'`
        eval ENTRIES=\"\${${PGMEXP}}\"
        echo " STRPGMEXP PGMLVL(*${PGMLVL}) SIGNATURE('${SIGNATURE}')"
        for ENTRY in ${ENTRIES} ${OS400SYMS}
        do      echo " EXPORT    SYMBOL('${ENTRY}')"
        done
        echo ' ENDPGMEXP'
        PGMLVL=PRV
done            > "${BSF}"


#       Build the service program if needed.

if action_needed "${LIBIFSNAME}/${SRVPGM}.SRVPGM"
then    LINK=YES
fi

if [ "${LINK}" ]
then    CMD="CRTSRVPGM SRVPGM(${TARGETLIB}/${SRVPGM})"
        CMD="${CMD} SRCFILE(${TARGETLIB}/TOOLS) SRCMBR(BNDSRC)"
        CMD="${CMD} MODULE(${TARGETLIB}/OS400)"
        CMD="${CMD} BNDDIR((${TARGETLIB}/${STATBNDDIR})"
        if [ "${WITH_ZLIB}" -ne 0 ]
        then    CMD="${CMD} (${ZLIB_LIB}/${ZLIB_BNDDIR})"
        fi
        CMD="${CMD})"
        CMD="${CMD} BNDSRVPGM(QADRTTS)"
        CMD="${CMD} TEXT('libxml2 dynamic library')"
        CMD="${CMD} TGTRLS(${TGTRLS})"
        system "${CMD}"
        LINK=YES
fi


#       If needed, (re)create the dynamic binding directory.

if action_needed "${LIBIFSNAME}/${DYNBNDDIR}.BNDDIR"
then    LINK=YES
fi

if [ "${LINK}" ]
then    rm -rf "${LIBIFSNAME}/${DYNBNDDIR}.BNDDIR"
        CMD="CRTBNDDIR BNDDIR(${TARGETLIB}/${DYNBNDDIR})"
        CMD="${CMD} TEXT('libxml2 dynamic binding directory')"
        system "${CMD}"
        CMD="ADDBNDDIRE BNDDIR(${TARGETLIB}/${DYNBNDDIR})"
        CMD="${CMD} OBJ((*LIBL/${SRVPGM} *SRVPGM))"
        system "${CMD}"
fi
