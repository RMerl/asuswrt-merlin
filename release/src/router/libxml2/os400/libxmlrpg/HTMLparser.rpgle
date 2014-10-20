      * Summary: interface for an HTML 4.0 non-verifying parser
      * Description: this module implements an HTML 4.0 non-verifying parser
      *              with API compatible with the XML parser ones. It should
      *              be able to parse "real world" HTML, even if severely
      *              broken from a specification point of view.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(HTML_PARSER_H__)
      /define HTML_PARSER_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/parser"

      /if defined(LIBXML_HTML_ENABLED)

      * Most of the back-end structures from XML and HTML are shared.

     d htmlParserCtxtPtr...
     d                 s                   based(######typedef######)
     d                                     like(xmlParserCtxtPtr)

     d htmlParserCtxt  ds                  based(htmlParserCtxtPtr)
     d                                     likeds(xmlParserCtxt)

     d htmlParserNodeInfoPtr...
     d                 s                   based(######typedef######)
     d                                     like(xmlParserNodeInfoPtr)

     d htmlParserNodeInfo...
     d                 ds                  based(htmlParserNodeInfoPtr)
     d                                     likeds(xmlParserNodeInfo)

     d htmlSAXHandlerPtr...
     d                 s                   based(######typedef######)
     d                                     like(xmlSAXHandlerPtr)

     d htmlSAXHandler  ds                  based(htmlSAXHandlerPtr)
     d                                     likeds(xmlSAXHandler)

     d htmlParserInputPtr...
     d                 s                   based(######typedef######)
     d                                     like(xmlParserInputPtr)

     d htmlParserInput...
     d                 ds                  based(htmlParserInputPtr)
     d                                     likeds(xmlParserInput)

     d htmlDocPtr      s                   based(######typedef######)
     d                                     like(xmlDocPtr)

     d htmlNodePtr     s                   based(######typedef######)
     d                                     like(xmlNodePtr)

      * Internal description of an HTML element, representing HTML 4.01
      * and XHTML 1.0 (which share the same structure).

     d htmlElemDescPtr...
     d                 s               *   based(######typedef######)

     d htmlElemDesc    ds                  based(htmlElemDescPtr)
     d                                     align qualified
     d  name                           *                                        const char *
     d  startTag                      3u 0                                      Start tag implied ?
     d  endTag                        3u 0                                      End tag implied ?
     d  saveEndTag                    3u 0                                      Save end tag ?
     d  empty                         3u 0                                      Empty element ?
     d  depr                          3u 0                                      Deprecated element ?
     d  dtd                           3u 0                                      Loose DTD/Frameset
     d  isinline                      3u 0                                      Block 0/inline elem?
     d  desc                           *                                        const char *
      *
      * New fields encapsulating HTML structure
      *
      * Bugs:
      *      This is a very limited representation.  It fails to tell us when
      *      an element *requires* subelements (we only have whether they're
      *      allowed or not), and it doesn't tell us where CDATA and PCDATA
      *      are allowed.  Some element relationships are not fully represented:
      *      these are flagged with the word MODIFIER
      *
     d  subelts                        *                                        const char * *
     d  defaultsubelt                  *                                        const char *
     d  attrs_opt                      *                                        const char * *
     d  attrs_depr                     *                                        const char * *
     d  attrs_req                      *                                        const char * *

      * Internal description of an HTML entity.

     d htmlEntityDescPtr...
     d                 s               *   based(######typedef######)

     d htmlEntityDesc...
     d                 ds                  based(htmlEntityDescPtr)
     d                                     align qualified
     d  value                        10u 0                                      Unicode char value
     d  name                           *                                        const char *
     d  desc                           *                                        const char *

      * There is only few public functions.

     d htmlTagLookup   pr                  extproc('htmlTagLookup')
     d                                     like(htmlElemDescPtr)                const
     d  tag                            *   value options(*string)               const xmlChar *

     d htmlEntityLookup...
     d                 pr                  extproc('htmlEntityLookup')
     d                                     like(htmlEntityDescPtr)              const
     d  name                           *   value options(*string)               const xmlChar *

     d htmlEntityValueLookup...
     d                 pr                  extproc('htmlEntityValueLookup')
     d                                     like(htmlEntityDescPtr)              const
     d  value                        10u 0 value

     d htmlIsAutoClosed...
     d                 pr            10i 0 extproc('htmlIsAutoClosed')
     d  doc                                value like(htmlDocPtr)
     d  elem                               value like(htmlNodePtr)

     d htmlAutoCloseTag...
     d                 pr            10i 0 extproc('htmlAutoCloseTag')
     d  doc                                value like(htmlDocPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  elem                               value like(htmlNodePtr)

     d htmlParseEntityRef...
     d                 pr                  extproc('htmlParseEntityRef')
     d                                     like(htmlEntityDescPtr)              const
     d  ctxt                               value like(htmlParserCtxtPtr)
     d  str                            *                                        const xmlChar *(*)

     d htmlParseCharRef...
     d                 pr            10i 0 extproc('htmlParseCharRef')
     d  ctxt                               value like(htmlParserCtxtPtr)

     d htmlParseElement...
     d                 pr                  extproc('htmlParseElement')
     d  ctxt                               value like(htmlParserCtxtPtr)

     d htmlNewParserCtxt...
     d                 pr                  extproc('htmlNewParserCtxt')
     d                                     like(htmlParserCtxtPtr)

     d htmlCreateMemoryParserCtxt...
     d                 pr                  extproc('htmlCreateMemoryParserCtxt')
     d                                     like(htmlParserCtxtPtr)
     d  buffer                         *   value options(*string)               const char *
     d  size                         10i 0 value

     d htmlParseDocument...
     d                 pr            10i 0 extproc('htmlParseDocument')
     d  ctxt                               value like(htmlParserCtxtPtr)

     d htmlSAXParseDoc...
     d                 pr                  extproc('htmlSAXParseDoc')
     d                                     like(htmlDocPtr)
     d  cur                            *   value options(*string)               xmlChar *
     d  encoding                       *   value options(*string)               const char *
     d  sax                                value like(htmlSAXHandlerPtr)
     d  userData                       *   value                                void *

     d htmlParseDoc    pr                  extproc('htmlParseDoc')
     d                                     like(htmlDocPtr)
     d  cur                            *   value options(*string)               xmlChar *
     d  encoding                       *   value options(*string)               const char *

     d htmlSAXParseFile...
     d                 pr                  extproc('htmlSAXParseFile')
     d                                     like(htmlDocPtr)
     d  filename                       *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  sax                                value like(htmlSAXHandlerPtr)
     d  userData                       *   value                                void *

     d htmlParseFile   pr                  extproc('htmlParseFile')
     d                                     like(htmlDocPtr)
     d  filename                       *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *

     d UTF8ToHtml      pr            10i 0 extproc('UTF8ToHtml')
     d  out                       65535    options(*varsize)                    unsigned char []
     d  outlen                       10i 0
     d  in                             *   value options(*string)               const unsigned char*
     d  inlen                        10i 0

     d htmlEncodeEntities...
     d                 pr            10i 0 extproc('htmlEncodeEntities')
     d  out                       65535    options(*varsize)                    unsigned char []
     d  outlen                       10i 0
     d  in                             *   value options(*string)               const unsigned char*
     d  inlen                        10i 0
     d  quoteChar                    10i 0 value

     d htmlIsScriptAttribute...
     d                 pr            10i 0 extproc('htmlIsScriptAttribute')
     d  name                           *   value options(*string)               const xmlChar *

     d htmlHandleOmittedElem...
     d                 pr            10i 0 extproc('htmlHandleOmittedElem')
     d  val                          10i 0 value

      /if defined(LIBXML_PUSH_ENABLED)

      * Interfaces for the Push mode.

     d htmlCreatePushParserCtxt...
     d                 pr                  extproc('htmlCreatePushParserCtxt')
     d                                     like(htmlParserCtxtPtr)
     d  sax                                value like(htmlSAXHandlerPtr)
     d  user_data                      *   value                                void *
     d  chunk                          *   value options(*string)               const char *
     d  size                         10i 0 value
     d  filename                       *   value options(*string)               const char *
     d  enc                                value like(xmlCharEncoding)

     d htmlParseChunk  pr            10i 0 extproc('htmlParseChunk')
     d  ctxt                               value like(htmlParserCtxtPtr)
     d  chunk                          *   value options(*string)               const char *
     d  size                         10i 0 value
     d  terminate                    10i 0 value
      /endif                                                                    LIBXML_PUSH_ENABLED

     d htmlFreeParserCtxt...
     d                 pr                  extproc('htmlFreeParserCtxt')
     d  ctxt                               value like(htmlParserCtxtPtr)

      * New set of simpler/more flexible APIs

      * xmlParserOption:
      *
      * This is the set of XML parser options that can be passed down
      * to the xmlReadDoc() and similar calls.

     d htmlParserOption...
     d                 s             10i 0 based(######typedef######)           enum
     d  HTML_PARSE_RECOVER...                                                   Relaxed parsing
     d                 c                   X'00000001'
     d  HTML_PARSE_NODEFDTD...                                                  No default doctype
     d                 c                   X'00000004'
     d  HTML_PARSE_NOERROR...                                                   No error reports
     d                 c                   X'00000020'
     d  HTML_PARSE_NOWARNING...                                                 No warning reports
     d                 c                   X'00000040'
     d  HTML_PARSE_PEDANTIC...                                                  Pedantic err reports
     d                 c                   X'00000080'
     d  HTML_PARSE_NOBLANKS...                                                  Remove blank nodes
     d                 c                   X'00000100'
     d  HTML_PARSE_NONET...                                                     Forbid net access
     d                 c                   X'00000800'
     d  HTML_PARSE_NOIMPLIED...                                                 No implied html/body
     d                 c                   X'00002000'
     d  HTML_PARSE_COMPACT...                                                   compact small txtnod
     d                 c                   X'00010000'
     d  HTML_PARSE_IGNORE_ENC...                                                Ignore encoding hint
     d                 c                   X'00200000'

     d htmlCtxtReset   pr                  extproc('htmlCtxtReset')
     d ctxt                                value like(htmlParserCtxtPtr)

     d htmlCtxtUseOptions...
     d                 pr            10i 0 extproc('htmlCtxtUseOptions')
     d ctxt                                value like(htmlParserCtxtPtr)
     d options                       10i 0 value

     d htmlReadDoc     pr                  extproc('htmlReadDoc')
     d                                     like(htmlDocPtr)
     d  cur                            *   value options(*string)               const xmlChar *
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlReadFile    pr                  extproc('htmlReadFile')
     d                                     like(htmlDocPtr)
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlReadMemory  pr                  extproc('htmlReadMemory')
     d                                     like(htmlDocPtr)
     d  buffer                         *   value options(*string)               const char *
     d  size                         10i 0 value
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlReadFd      pr                  extproc('htmlReadFd')
     d                                     like(htmlDocPtr)
     d  fd                           10i 0 value
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlReadIO      pr                  extproc('htmlReadIO')
     d                                     like(htmlDocPtr)
     d  ioread                             value like(xmlInputReadCallback)
     d  ioclose                            value like(xmlInputCloseCallback)
     d  ioctx                          *   value                                void *
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlCtxtReadDoc...
     d                 pr                  extproc('htmlCtxtReadDoc')
     d                                     like(htmlDocPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  cur                            *   value options(*string)               const xmlChar *
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlCtxtReadFile...
     d                 pr                  extproc('htmlCtxtReadFile')
     d                                     like(htmlDocPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  filename                       *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlCtxtReadMemory...
     d                 pr                  extproc('htmlCtxtReadMemory')
     d                                     like(htmlDocPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  buffer                         *   value options(*string)               const char *
     d  size                         10i 0 value
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlCtxtReadFd  pr                  extproc('htmlCtxtReadFd')
     d                                     like(htmlDocPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  fd                           10i 0 value
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d htmlCtxtReadIO  pr                  extproc('htmlCtxtReadIO')
     d                                     like(htmlDocPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  ioread                             value like(xmlInputReadCallback)
     d  ioclose                            value like(xmlInputCloseCallback)
     d  ioctx                          *   value                                void *
     d  URL                            *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

      * Further knowledge of HTML structure

     d htmlStatus      s             10i 0 based(######typedef######)           enum
     d  HTML_NA        c                   X'0000'                              No check at all
     d  HTML_INVALID   c                   X'0001'
     d  HTML_DEPRECATED...
     d                 c                   X'0002'
     d  HTML_VALID     c                   X'0004'
     d  HTML_REQUIRED  c                   X'000C'                              HTML_VALID ored-in

      * Using htmlElemDesc rather than name here, to emphasise the fact
      *  that otherwise there's a lookup overhead

     d htmlAttrAllowed...
     d                 pr                  extproc('htmlAttrAllowed')
     d                                     like(htmlStatus)
     d  #param1                            value like(htmlElemDescPtr)          const
     d  #param2                        *   value options(*string)               const xmlChar *
     d  #param3                      10i 0 value

     d htmlElementAllowedHere...
     d                 pr            10i 0 extproc('htmlElementAllowedHere')
     d  #param1                            value like(htmlElemDescPtr)          const
     d  #param2                        *   value options(*string)               const xmlChar *

     d htmlElementStatusHere...
     d                 pr                  extproc('htmlElementStatusHere')
     d                                     like(htmlStatus)
     d  #param1                            value like(htmlElemDescPtr)          const
     d  #param2                            value like(htmlElemDescPtr)          const

     d htmlNodeStatus  pr                  extproc('htmlNodeStatus')
     d                                     like(htmlStatus)
     d  #param1                            value like(htmlNodePtr)
     d  #param2                      10i 0 value

      * C macros implemented as procedures for ILE/RPG support.

     d htmlDefaultSubelement...
     d                 pr              *   extproc('__htmlDefaultSubelement')   const char *
     d  elt                            *   value                                const htmlElemDesc *

     d htmlElementAllowedHereDesc...
     d                 pr            10i 0 extproc(
     d                                     '__htmlElementAllowedHereDesc')
     d  parent                         *   value                                const htmlElemDesc *
     d  elt                            *   value                                const htmlElemDesc *

     d htmlRequiredAttrs...
     d                 pr              *   extproc('__htmlRequiredAttrs')        const char * *
     d  elt                            *   value                                const htmlElemDesc *

      /endif                                                                    LIBXML_HTML_ENABLED
      /endif                                                                    HTML_PARSER_H__
