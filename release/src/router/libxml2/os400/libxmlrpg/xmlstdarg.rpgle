      * Summary: va_list support for ILE/RPG.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_STDARG_H__)
      /define XML_STDARG_H__

      /include "libxmlrpg/xmlversion"

      * The va_list object.

     d xmlVaList       ds                  based(######typedef######)
     d                                     align qualified
     d  current                        *
     d  next                           *

      * Procedures.

     d xmlVaStart      pr                  extproc('__xmlVaStart')
     d  list                               like(xmlVaList)
     d  lastargaddr                    *   value
     d  lastargsize                  10u 0 value

     d xmlVaArg        pr              *   extproc('__xmlVaArg')
     d  list                               like(xmlVaList)
     d  dest                           *   value
     d  argsize                      10i 0 value

     d xmlVaEnd        pr                  extproc('__xmlVaEnd')
     d  list                               like(xmlVaList)

      /endif                                                                    XML_STDARG_H__
