      * Summary: specific APIs to process HTML tree, especially serialization
      * Description: this module implements a few function needed to process
      *              tree in an HTML specific way.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(HTML_TREE_H__)
      /define HTML_TREE_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"
      /include "libxmlrpg/HTMLparser"

      /if defined(LIBXML_HTML_ENABLED)

      * HTML_TEXT_NODE:
      *
      * Macro. A text node in a HTML document is really implemented
      * the same way as a text node in an XML document.

     d HTML_TEXT_NODE  c                   3

      * HTML_ENTITY_REF_NODE:
      *
      * Macro. An entity reference in a HTML document is really implemented
      * the same way as an entity reference in an XML document.

     d HTML_ENTITY_REF_NODE...
     d                 c                   5

      * HTML_COMMENT_NODE:
      *
      * Macro. A comment in a HTML document is really implemented
      * the same way as a comment in an XML document.

     d HTML_COMMENT_NODE...
     d                 c                   8

      * HTML_PRESERVE_NODE:
      *
      * Macro. A preserved node in a HTML document is really implemented
      * the same way as a CDATA section in an XML document.

     d HTML_PRESERVE_NODE...
     d                 c                   4

      * HTML_PI_NODE:
      *
      * Macro. A processing instruction in a HTML document is really implemented
      * the same way as a processing instruction in an XML document.

     d HTML_PI_NODE    c                   7

     d htmlNewDoc      pr                  extproc('htmlNewDoc')
     d                                     like(htmlDocPtr)
     d  URI                            *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *

     d htmlNewDocNoDtD...
     d                 pr                  extproc('htmlNewDocNoDtD')
     d                                     like(htmlDocPtr)
     d  URI                            *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *

     d htmlGetMetaEncoding...
     d                 pr              *   extproc('htmlGetMetaEncoding')       const xmlChar *
     d  doc                                value like(htmlDocPtr)

     d htmlSetMetaEncoding...
     d                 pr            10i 0 extproc('htmlSetMetaEncoding')
     d  doc                                value like(htmlDocPtr)
     d  encoding                       *   value options(*string)               const xmlChar *

      /if defined(LIBXML_OUTPUT_ENABLED)
     d htmlDocDumpMemory...
     d                 pr                  extproc('htmlDocDumpMemory')
     d  cur                                value like(xmlDocPtr)
     d  mem                            *   value                                xmlChar * *
     d  size                         10i 0

     d htmlDocDumpMemoryFormat...
     d                 pr                  extproc('htmlDocDumpMemoryFormat')
     d  cur                                value like(xmlDocPtr)
     d  mem                            *   value                                xmlChar * *
     d  size                         10i 0
     d  format                       10i 0 value

     d htmlDocDump     pr            10i 0 extproc('htmlDocDump')
     d  f                              *   value                                FILE *
     d  cur                                value like(xmlDocPtr)

     d htmlSaveFile    pr            10i 0 extproc('htmlSaveFile')
     d  filename                       *   value options(*string)               const char *
     d  cur                                value like(xmlDocPtr)

     d htmlNodeDump    pr            10i 0 extproc('htmlNodeDump')
     d  buf                                value like(xmlBufferPtr)
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlNodePtr)

     d htmlNodeDumpFile...
     d                 pr                  extproc('htmlNodeDumpFile')
     d  out                            *   value                                FILE *
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlNodePtr)

     d htmlNodeDumpFileFormat...
     d                 pr            10i 0 extproc('htmlNodeDumpFileFormat')
     d  out                            *   value                                FILE *
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlNodePtr)
     d  encoding                       *   value options(*string)               const char *
     d  format                       10i 0 value

     d htmlSaveFileEnc...
     d                 pr            10i 0 extproc('htmlSaveFileEnc')
     d  filename                       *   value options(*string)               const char *
     d  cur                                value like(xmlDocPtr)
     d  encoding                       *   value options(*string)               const char *

     d htmlSaveFileFormat...
     d                 pr            10i 0 extproc('htmlSaveFileFormat')
     d  filename                       *   value options(*string)               const char *
     d  cur                                value like(xmlDocPtr)
     d  encoding                       *   value options(*string)               const char *
     d  format                       10i 0 value

     d htmlNodeDumpFormatOutput...
     d                 pr                  extproc('htmlNodeDumpFormatOutput')
     d  buf                                value like(xmlOutputBufferPtr)
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlNodePtr)
     d  encoding                       *   value options(*string)               const char *
     d  format                       10i 0 value

     d htmlDocContentDumpOutput...
     d                 pr                  extproc('htmlDocContentDumpOutput')
     d  buf                                value like(xmlOutputBufferPtr)
     d  cur                                value like(xmlDocPtr)
     d  encoding                       *   value options(*string)               const char *

     d htmlDocContentDumpFormatOutput...
     d                 pr                  extproc(
     d                                     'htmlDocContentDumpFormatOutput')
     d  buf                                value like(xmlOutputBufferPtr)
     d  cur                                value like(xmlDocPtr)
     d  encoding                       *   value options(*string)               const char *
     d  format                       10i 0 value

     d htmlNodeDumpOutput...
     d                 pr                  extproc('htmlNodeDumpOutput')
     d  buf                                value like(xmlOutputBufferPtr)
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlNodePtr)
     d  encoding                       *   value options(*string)               const char *

      /endif                                                                    LIBXML_OUTPUT_ENABLD

     d htmlIsBooleanAttr...
     d                 pr            10i 0 extproc('htmlIsBooleanAttr')
     d  name                           *   value options(*string)               const xmlChar *

      /endif                                                                    LIBXML_HTML_ENABLED
      /endif                                                                    HTML_TREE_H__
