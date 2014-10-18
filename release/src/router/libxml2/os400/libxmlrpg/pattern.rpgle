      * Summary: pattern expression handling
      * Description: allows to compile and test pattern expressions for nodes
      *              either in a tree or based on a parser state.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_PATTERN_H__)
      /define XML_PATTERN_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"
      /include "libxmlrpg/dict"

      /if defined(LIBXML_PATTERN_ENABLED)

      * xmlPattern:
      *
      * A compiled (XPath based) pattern to select nodes

     d xmlPatternPtr...
     d                 s               *   based(######typedef######)

      * xmlPatternFlags:
      *
      * This is the set of options affecting the behaviour of pattern
      * matching with this module

     d xmlPatternFlags...
     d                 s             10i 0 based(######typedef######)           enum
     d  XML_PATTERN_DEFAULT...                                                  Simple pattern match
     d                 c                   X'0000'
     d  XML_PATTERN_XPATH...                                                    Std XPath pattern
     d                 c                   X'0001'
     d  XML_PATTERN_XSSEL...                                                    Schm sel XPth subset
     d                 c                   X'0002'
     d  XML_PATTERN_XSFIELD...                                                  Schm fld XPth subset
     d                 c                   X'0004'

     d xmlFreePattern  pr                  extproc('xmlFreePattern')
     d  comp                               value like(xmlPatternPtr)

     d xmlFreePatternList...
     d                 pr                  extproc('xmlFreePatternList')
     d  comp                               value like(xmlPatternPtr)

     d xmlPatterncompile...
     d                 pr                  extproc('xmlPatterncompile')
     d                                     like(xmlPatternPtr)
     d  pattern                        *   value options(*string)               const xmlChar *
     d  dict                           *   value                                xmlDict *
     d  flags                        10i 0 value
     d  namespaces                     *                                        const xmlChar *(*)

     d xmlPatternMatch...
     d                 pr            10i 0 extproc('xmlPatternMatch')
     d  comp                               value like(xmlPatternPtr)
     d  node                               value like(xmlNodePtr)

      * streaming interfaces

     d xmlStreamCtxtPtr...
     d                 s               *   based(######typedef######)

     d xmlPatternStreamable...
     d                 pr            10i 0 extproc('xmlPatternStreamable')
     d  comp                               value like(xmlPatternPtr)

     d xmlPatternMaxDepth...
     d                 pr            10i 0 extproc('xmlPatternMaxDepth')
     d  comp                               value like(xmlPatternPtr)

     d xmlPatternMinDepth...
     d                 pr            10i 0 extproc('xmlPatternMinDepth')
     d  comp                               value like(xmlPatternPtr)

     d xmlPatternFromRoot...
     d                 pr            10i 0 extproc('xmlPatternFromRoot')
     d  comp                               value like(xmlPatternPtr)

     d xmlPatternGetStreamCtxt...
     d                 pr                  extproc('xmlPatternGetStreamCtxt')
     d                                     like(xmlStreamCtxtPtr)
     d  comp                               value like(xmlPatternPtr)

     d xmlFreeStreamCtxt...
     d                 pr                  extproc('xmlFreeStreamCtxt')
     d  stream                             value like(xmlStreamCtxtPtr)

     d xmlStreamPushNode...
     d                 pr            10i 0 extproc('xmlStreamPushNode')
     d  stream                             value like(xmlStreamCtxtPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *
     d  nodeType                     10i 0 value

     d xmlStreamPush   pr            10i 0 extproc('xmlStreamPush')
     d  stream                             value like(xmlStreamCtxtPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *

     d xmlStreamPushAttr...
     d                 pr            10i 0 extproc('xmlStreamPushAttr')
     d  stream                             value like(xmlStreamCtxtPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *

     d xmlStreamPop    pr            10i 0 extproc('xmlStreamPop')
     d  stream                             value like(xmlStreamCtxtPtr)

     d xmlStreamWantsAnyNode...
     d                 pr            10i 0 extproc('xmlStreamWantsAnyNode')
     d  stream                             value like(xmlStreamCtxtPtr)

      /endif                                                                    LIBXML_PATTERN_ENBLD
      /endif                                                                    XML_PATTERN_H__
