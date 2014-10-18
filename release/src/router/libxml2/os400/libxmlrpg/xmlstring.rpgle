      * Summary: set of routines to process strings
      * Description: type and interfaces needed for the internal string
      *              handling of the library, especially UTF8 processing.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_STRING_H__)
      /define XML_STRING_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/xmlstdarg"

      * xmlChar:
      *
      * This is a basic byte in an UTF-8 encoded string.
      * It's unsigned allowing to pinpoint case where char * are assigned
      * to xmlChar * (possibly making serialization back impossible).

     d xmlChar         s              3u 0 based(######typedef######)

      * xmlChar handling

     d xmlStrdup       pr              *   extproc('xmlStrdup')                 xmlChar *
     d  cur                            *   value options(*string)               const xmlChar *

     d xmlStrndup      pr              *   extproc('xmlStrndup')                xmlChar *
     d  cur                            *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlCharStrndup  pr              *   extproc('xmlCharStrndup')            xmlChar *
     d  cur                            *   value options(*string)               const char *
     d  len                          10i 0 value

     d xmlCharStrdup   pr              *   extproc('xmlCharStrdup')             xmlChar *
     d  cur                            *   value options(*string)               const char *

     d xmlStrsub       pr              *   extproc('xmlStrsub')                 const xmlChar *
     d  str                            *   value options(*string)               const xmlChar *
     d  start                        10i 0 value
     d  len                          10i 0 value

     d xmlStrchr       pr              *   extproc('xmlStrchr')                 const xmlChar *
     d  str                            *   value options(*string)               const xmlChar *
     d  val                                value like(xmlChar)

     d xmlStrstr       pr              *   extproc('xmlStrstr')                 const xmlChar *
     d  str                            *   value options(*string)               const xmlChar *
     d  val                            *   value options(*string)               const xmlChar *

     d xmlStrcasestr   pr              *   extproc('xmlStrcasestr')             const xmlChar *
     d  str                            *   value options(*string)               const xmlChar *
     d  val                            *   value options(*string)               const xmlChar *

     d xmlStrcmp       pr            10i 0 extproc('xmlStrcmp')
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *

     d xmlStrncmp      pr            10i 0 extproc('xmlStrncmp')
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlStrcasecmp   pr            10i 0 extproc('xmlStrcasecmp')
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *

     d xmlStrncasecmp  pr            10i 0 extproc('xmlStrncasecmp')
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlStrEqual     pr            10i 0 extproc('xmlStrEqual')
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *

     d xmlStrQEqual    pr            10i 0 extproc('xmlStrQEqual')
     d  pref                           *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  stre                           *   value options(*string)               const xmlChar *

     d xmlStrlen       pr            10i 0 extproc('xmlStrlen')
     d  str                            *   value options(*string)               const xmlChar *

     d xmlStrcat       pr              *   extproc('xmlStrcat')                 xmlChar *
     d  cur                            *   value options(*string)               xmlChar *
     d  add                            *   value options(*string)               const xmlChar *

     d xmlStrncat      pr              *   extproc('xmlStrncat')                xmlChar *
     d  cur                            *   value options(*string)               xmlChar *
     d  add                            *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlStrncatNew   pr              *   extproc('xmlStrncatNew')             xmlChar *
     d  str1                           *   value options(*string)               const xmlChar *
     d  str2                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

      * xmlStrPrintf() is a vararg function.
      * The following prototype supports up to 8 pointer arguments.
      * Other argument signature can be achieved by defining alternate
      *   prototypes redirected to the same function.

     d xmlStrPrintf    pr            10i 0 extproc('xmlStrPrintf')
     d  buf                            *   value options(*string)               xmlChar *
     d  len                          10i 0 value
     d  msg                            *   value options(*string)               const xmlChar *
     d  arg1                           *   value options(*string: *nopass)
     d  arg2                           *   value options(*string: *nopass)
     d  arg3                           *   value options(*string: *nopass)
     d  arg4                           *   value options(*string: *nopass)
     d  arg5                           *   value options(*string: *nopass)
     d  arg6                           *   value options(*string: *nopass)
     d  arg7                           *   value options(*string: *nopass)
     d  arg8                           *   value options(*string: *nopass)

     d xmlStrVPrintf   pr            10i 0 extproc('xmlStrVPrintf')
     d  buf                            *   value options(*string)               xmlChar *
     d  len                          10i 0 value
     d  msg                            *   value options(*string)               const xmlChar *
     d  ap                                 likeds(xmlVaList)

     d xmlGetUTF8Char  pr            10i 0 extproc('xmlGetUTF8Char')
     d  utf                            *   value options(*string)               const uns. char *
     d  len                          10i 0

     d xmlCheckUTF8    pr            10i 0 extproc('xmlCheckUTF8')
     d  utf                            *   value options(*string)               const uns. char *

     d xmlUTF8Strsize  pr            10i 0 extproc('xmlUTF8Strsize')
     d  utf                            *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlUTF8Strndup  pr              *   extproc('xmlUTF8Strndup')            xmlChar *
     d  utf                            *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlUTF8Strpos   pr              *   extproc('xmlUTF8Strpos')             const xmlChar *
     d  utf                            *   value options(*string)               const xmlChar *
     d  pos                          10i 0 value

     d xmlUTF8Strloc   pr            10i 0 extproc('xmlUTF8Strloc')
     d  utf                            *   value options(*string)               const xmlChar *
     d  utfchar                        *   value options(*string)               const xmlChar *

     d xmlUTF8Strsub   pr              *   extproc('xmlUTF8Strsub')             xmlChar *
     d  utf                            *   value options(*string)               const xmlChar *
     d  start                        10i 0 value
     d  len                          10i 0 value

     d xmlUTF8Strlen   pr            10i 0 extproc('xmlUTF8Strlen')
     d  utf                            *   value options(*string)               const xmlChar *

     d xmlUTF8Size     pr            10i 0 extproc('xmlUTF8Size')
     d  utf                            *   value options(*string)               const xmlChar *

     d xmlUTF8Charcmp  pr            10i 0 extproc('xmlUTF8Charcmp')
     d  utf1                           *   value options(*string)               const xmlChar *
     d  utf2                           *   value options(*string)               const xmlChar *

      /endif                                                                    XML_STRING_H__
