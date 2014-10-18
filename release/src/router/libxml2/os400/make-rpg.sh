#!/bin/sh
#
#       Installation of the ILE/RPG header files in the OS/400 library.
#
#       See Copyright for the status of this software.
#
#       Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.
#

SCRIPTDIR=`dirname "${0}"`
. "${SCRIPTDIR}/initscript.sh"
cd "${TOPDIR}/os400/libxmlrpg"


#       Create the OS/400 source program file for the ILE/RPG header files.

SRCPF="${LIBIFSNAME}/LIBXMLRPG.FILE"

if action_needed "${SRCPF}"
then    CMD="CRTSRCPF FILE(${TARGETLIB}/LIBXMLRPG) RCDLEN(112)"
        CMD="${CMD} CCSID(${TGTCCSID}) TEXT('libxml2: ILE/RPG header files')"
        system "${CMD}"
fi


#       Map file names to DB2 name syntax.

> tmpsubstfile

for HFILE in *.rpgle *.rpgle.in
do      NAME="`basename \"${HFILE}\" .in`"
        VAR="`basename \"${NAME}\" .rpgle`"
        VAL="`db2_name \"${NAME}\" nomangle`"

        if [ "${VAR}" = 'xmlschemastypes' ]
        then    VAL=SCHMTYPES
        fi

        echo "s/${VAR}/${VAL}/g" >> tmpsubstfile
        eval "VAR_${VAR}=\"${VAL}\""
done


change_include()

{
        sed -e '\#^....../include  *"libxmlrpg/#{'                      \
            -e 's///'                                                   \
            -e 's/".*//'                                                \
            -f tmpsubstfile                                             \
            -e 's#.*#      /include libxmlrpg,&#'                       \
            -e '}'
}


#       Create the IFS directory for the ILE/RPG header files.

RPGIFSDIR="${IFSDIR}/include/libxmlrpg"

if action_needed "${RPGIFSDIR}"
then    mkdir -p "${RPGIFSDIR}"
fi

#       Copy the header files to IFS ILE/RPG include directory.
#       Copy them with include path editing to the DB2 library.

for HFILE in *.rpgle *.rpgle.in
do      IFSCMD="cat \"${HFILE}\""
        DB2CMD="change_include < \"${HFILE}\""
        IFSFILE="`basename \"${HFILE}\" .in`"

        case "${HFILE}" in

        *.in)   IFSCMD="${IFSCMD} | versioned_copy"
                DB2CMD="${DB2CMD} | versioned_copy"
                ;;
        esac

        IFSDEST="${RPGIFSDIR}/${IFSFILE}"

        if action_needed "${IFSDEST}" "${HFILE}"
        then    eval "${IFSCMD}" > "${IFSDEST}"
        fi

        eval DB2MBR="\"\${VAR_`basename \"${IFSDEST}\" .rpgle`}\""
        DB2DEST="${SRCPF}/${DB2MBR}.MBR"

        if action_needed "${DB2DEST}" "${HFILE}"
        then    eval "${DB2CMD}" | change_include > tmphdrfile

                #       Need to translate to target CCSID.

                CMD="CPY OBJ('`pwd`/tmphdrfile') TOOBJ('${DB2DEST}')"
                CMD="${CMD} TOCCSID(${TGTCCSID}) DTAFMT(*TEXT) REPLACE(*YES)"
                system "${CMD}"
        fi
done
