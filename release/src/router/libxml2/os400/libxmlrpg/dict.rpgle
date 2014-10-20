      * Summary: string dictionary
      * Description: dictionary of reusable strings, just used to avoid
      *         allocation and freeing operations.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_DICT_H__)
      /define XML_DICT_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"

      * The dictionary.

     d xmlDictPtr      s               *   based(######typedef######)

      * Initializer

     d xmlInitializeDict...
     d                 pr            10i 0 extproc('xmlInitializeDict')

      * Constructor and destructor.

     d xmlDictCreate   pr                  extproc('xmlDictCreate')
     d                                     like(xmlDictPtr)

     d xmlDictSetLimit...
     d                 pr            10u 0 extproc('xmlDictSetLimit')           size_t
     d  dict                               value like(xmlDictPtr)
     d  limit                        10u 0 value                                size_t

     d xmlDictGetUsage...
     d                 pr            10u 0 extproc('xmlDictGetUsage')           size_t
     d  dict                               value like(xmlDictPtr)

     d xmlDictCreateSub...
     d                 pr                  extproc('xmlDictCreateSub')
     d                                     like(xmlDictPtr)
     d  sub                                value like(xmlDictPtr)

     d xmlDictReference...
     d                 pr            10i 0 extproc('xmlDictGetReference')
     d  dict                               value like(xmlDictPtr)

     d xmlDictFree     pr                  extproc('xmlDictFree')
     d  dict                               value like(xmlDictPtr)

      * Lookup of entry in the dictionary.

     d xmlDictLookup   pr              *   extproc('xmlDictLookup')             const xmlChar *
     d  dict                               value like(xmlDictPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlDictExists   pr              *   extproc('xmlDictExists')             const xmlChar *
     d  dict                               value like(xmlDictPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlDictQLookup  pr              *   extproc('xmlDictQLookup')            const xmlChar *
     d  dict                               value like(xmlDictPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *

     d xmlDictOwns     pr            10i 0 extproc('xmlDictOwns')
     d  dict                               value like(xmlDictPtr)
     d  str                            *   value options(*string)               const xmlChar *

     d xmlDictSize     pr            10i 0 extproc('xmlDictSize')
     d  dict                               value like(xmlDictPtr)

      * Cleanup function

     d xmlDictCleanup  pr                  extproc('xmlDictCleanup')

      /endif                                                                    ! XML_DICT_H__
