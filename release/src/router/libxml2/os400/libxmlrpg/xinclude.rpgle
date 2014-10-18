      * Summary: implementation of XInclude
      * Description: API to handle XInclude processing,
      * implements the
      * World Wide Web Consortium Last Call Working Draft 10 November 2003
      * http://www.w3.org/TR/2003/WD-xinclude-20031110
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_XINCLUDE_H__)
      /define XML_XINCLUDE_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"

      /if defined(LIBXML_XINCLUDE_ENABLED)

      * XINCLUDE_NS:
      *
      * Macro defining the Xinclude namespace: http://www.w3.org/2003/XInclude

     d XINCLUDE_NS     c                   'http://www.w3.org/2003/XInclude'


      * XINCLUDE_OLD_NS:
      *
      * Define the draft Xinclude namespace: http://www.w3.org/2001/XInclude

     d XINCLUDE_OLD_NS...
     d                 c                   'http://www.w3.org/2001/XInclude'

      * XINCLUDE_NODE:
      *
      * Macro defining "include"

     d XINCLUDE_NODE   c                   'include'

      * XINCLUDE_FALLBACK:
      *
      * Macro defining "fallback"

     d XINCLUDE_FALLBACK...
     d                 c                   'fallback'

      * XINCLUDE_HREF:
      *
      * Macro defining "href"

     d XINCLUDE_HREF   c                   'href'

      * XINCLUDE_PARSE:
      *
      * Macro defining "parse"

     d XINCLUDE_PARSE  c                   'parse'

      * XINCLUDE_PARSE_XML:
      *
      * Macro defining "xml"

     d XINCLUDE_PARSE_XML...
     d                 c                   'xml'

      * XINCLUDE_PARSE_TEXT:
      *
      * Macro defining "text"

     d XINCLUDE_PARSE_TEXT...
     d                 c                   'text'

      * XINCLUDE_PARSE_ENCODING:
      *
      * Macro defining "encoding"

     d XINCLUDE_PARSE_ENCODING...
     d                 c                   'encoding'

      * XINCLUDE_PARSE_XPOINTER:
      *
      * Macro defining "xpointer"

     d XINCLUDE_PARSE_XPOINTER...
     d                 c                   'xpointer'

     d xmlXIncludeCtxtPtr...
     d                 s               *   based(######typedef######)

      * standalone processing

     d xmlXIncludeProcess...
     d                 pr            10i 0 extproc('xmlXIncludeProcess')
     d  doc                                value like(xmlDocPtr)

     d xmlXIncludeProcessFlags...
     d                 pr            10i 0 extproc('xmlXIncludeProcessFlags')
     d  doc                                value like(xmlDocPtr)
     d  flags                        10i 0 value

     d xmlXIncludeProcessFlagsData...
     d                 pr            10i 0 extproc(
     d                                     'xmlXIncludeProcessFlagsData')
     d  doc                                value like(xmlDocPtr)
     d  flags                        10i 0 value
     d  data                           *   value                                void *

     d xmlXIncludeProcessTreeFlagsData...
     d                 pr            10i 0 extproc(
     d                                     'xmlXIncludeProcessTreeFlagsData')
     d  tree                               value like(xmlNodePtr)
     d  flags                        10i 0 value
     d  data                           *   value                                void *

     d xmlXIncludeProcessTree...
     d                 pr            10i 0 extproc('xmlXIncludeProcessTree')
     d  tree                               value like(xmlNodePtr)

     d xmlXIncludeProcessTreeFlags...
     d                 pr            10i 0 extproc(
     d                                     'xmlXIncludeProcessTreeFlags')
     d  tree                               value like(xmlNodePtr)
     d  flags                        10i 0 value


      * contextual processing

     d xmlXIncludeNewContext...
     d                 pr                  extproc('xmlXIncludeNewContext')
     d                                     like(xmlXIncludeCtxtPtr)
     d  doc                                value like(xmlDocPtr)

     d xmlXIncludeSetFlags...
     d                 pr            10i 0 extproc('xmlXIncludeSetFlags')
     d  ctxt                               value like(xmlXIncludeCtxtPtr)
     d  flags                        10i 0 value

     d xmlXIncludeFreeContext...
     d                 pr                  extproc('xmlXIncludeFreeContext')
     d  ctxt                               value like(xmlXIncludeCtxtPtr)

     d xmlXIncludeProcessNode...
     d                 pr            10i 0 extproc('xmlXIncludeProcessNode')
     d  ctxt                               value like(xmlXIncludeCtxtPtr)
     d  tree                               value like(xmlNodePtr)

      /endif                                                                    XINCLUDE_ENABLED
      /endif                                                                    XML_XINCLUDE_H__
