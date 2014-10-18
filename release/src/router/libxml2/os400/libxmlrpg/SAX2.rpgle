      * Summary: SAX2 parser interface used to build the DOM tree
      * Description: those are the default SAX2 interfaces used by
      *              the library when building DOM tree.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_SAX2_H__)
      /define XML_SAX2_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/parser"
      /include "libxmlrpg/xlink"

     d xmlSAX2GetPublicId...
     d                 pr              *   extproc('xmlSAX2getPublicId')        const xmlChar *
     d  ctx                            *   value                                void *

     d xmlSAX2GetSystemId...
     d                 pr              *   extproc('xmlSAX2getSystemId')        const xmlChar *
     d  ctx                            *   value                                void *

     d xmlSAX2SetDocumentLocator...
     d                 pr                  extproc('xmlSAX2SetDocumentLocator')
     d  ctx                            *   value                                void *
     d  loc                                value like(xmlSAXLocatorPtr)

     d xmlSAX2GetLineNumber...
     d                 pr            10i 0 extproc('xmlSAX2GetLineNumber')
     d  ctx                            *   value                                void *

     d xmlSAX2GetColumnNumber...
     d                 pr            10i 0 extproc('xmlSAX2GetColumnNumber')
     d  ctx                            *   value                                void *

     d xmlSAX2IsStandalone...
     d                 pr            10i 0 extproc('xmlSAX2IsStandalone')
     d  ctx                            *   value                                void *

     d xmlSAX2HasInternalSubset...
     d                 pr            10i 0 extproc('xmlSAX2HasInternalSubset')
     d  ctx                            *   value                                void *

     d xmlSAX2HasExternalSubset...
     d                 pr            10i 0 extproc('xmlSAX2HasExternalSubset')
     d  ctx                            *   value                                void *

     d xmlSAX2InternalSubset...
     d                 pr                  extproc('xmlSAX2InternalSubset')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *
     d  SystemID                       *   value options(*string)               const xmlChar *

     d xmlSAX2ExternalSubset...
     d                 pr                  extproc('xmlSAX2ExternalSubset')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *
     d  SystemID                       *   value options(*string)               const xmlChar *

     d xmlSAX2GetEntity...
     d                 pr                  extproc('xmlSAX2GetEntity')
     d                                     like(xmlEntityPtr)
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d xmlSAX2GetParameterEntity...
     d                 pr                  extproc('xmlSAX2GetParameterEntity')
     d                                     like(xmlEntityPtr)
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d xmlSAX2ResolveEntity...
     d                 pr                  extproc('xmlSAX2ResolveEntity')
     d                                     like(xmlParserInputPtr)
     d  ctx                            *   value                                void *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *

     d xmlSAX2EntityDecl...
     d                 pr                  extproc('xmlSAX2EntityDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               xmlChar *

     d xmlSAX2AttributeDecl...
     d                 pr                  extproc('xmlSAX2AttributeDecl')
     d  ctx                            *   value                                void *
     d  elem                           *   value options(*string)               const xmlChar *
     d  fullname                       *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  def                          10i 0 value
     d  defaultValue                   *   value options(*string)               const xmlChar *
     d  tree                               value like(xmlEnumerationPtr)

     d xmlSAX2ElementDecl...
     d                 pr                  extproc('xmlSAX2ElementDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  content                            value like(xmlElementContentPtr)

     d xmlSAX2NotationDecl...
     d                 pr                  extproc('xmlSAX2NotationDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *

     d xmlSAX2UnparsedEntityDecl...
     d                 pr                  extproc('xmlSAX2UnparsedEntityDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *
     d  notationName                   *   value options(*string)               xmlChar *

     d xmlSAX2StartDocument...
     d                 pr                  extproc('xmlSAX2StartDocument')
     d  ctx                            *   value                                void *

     d xmlSAX2EndDocument...
     d                 pr                  extproc('xmlSAX2EndDocument')
     d  ctx                            *   value                                void *

      /undefine XML_TESTVAL
      /if defined(LIBXML_SAX1_ENABLED)
      /define XML_TESTVAL
      /elseif defined(LIBXML_HTML_ENABLED)
      /define XML_TESTVAL
      /elseif defined(LIBXML_WRITER_ENABLED)
      /define XML_TESTVAL
      /elseif defined(LIBXML_DOCB_ENABLED)
      /endif
      /if defined(XML_TESTVAL)
     d xmlSAX2StartElement...
     d                 pr                  extproc('xmlSAX2StartElement')
     d  ctx                            *   value                                void *
     d  fullname                       *   value options(*string)               const xmlChar *
     d  atts                           *                                        const xmlChar *(*)

     d xmlSAX2EndElement...
     d                 pr                  extproc('xmlSAX2EndElement')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

      /undefine XML_TESTVAL
      /endif

     d xmlSAX2StartElementNs...
     d                 pr                  extproc('xmlSAX2StartElementNs')
     d  ctx                            *   value                                void *
     d  localname                      *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *
     d  URI                            *   value options(*string)               const xmlChar *
     d  nb_namespaces                10i 0 value
     d  namespaces                     *   value                                const xmlChar *(*)
     d  nb_attributes                10i 0 value
     d  nb_defaulted                 10i 0 value
     d  attributes                     *                                        const xmlChar *(*)

     d xmlSAX2EndElementNs...
     d                 pr                  extproc('xmlSAX2EndElementNs')
     d  ctx                            *   value                                void *
     d  localname                      *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *
     d  URI                            *   value options(*string)               const xmlChar *

     d xmlSAX2Reference...
     d                 pr                  extproc('xmlSAX2Reference')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d xmlSAX2Characters...
     d                 pr                  extproc('xmlSAX2Characters')
     d  ctx                            *   value                                void *
     d  ch                             *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlSAX2IgnorableWhitespace...
     d                 pr                  extproc('xmlSAX2IgnorableWhitespace')
     d  ctx                            *   value                                void *
     d  ch                             *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlSAX2ProcessingInstruction...
     d                 pr                  extproc(
     d                                      'xmlSAX2ProcessingInstruction')
     d  ctx                            *   value                                void *
     d  target                         *   value options(*string)               const xmlChar *
     d  data                           *   value options(*string)               const xmlChar *

     d xmlSAX2Comment...
     d                 pr                  extproc('xmlSAX2Comment')
     d  ctx                            *   value                                void *
     d  value                          *   value options(*string)               const xmlChar *

     d xmlSAX2CDataBlock...
     d                 pr                  extproc('xmlSAX2CDataBlock')
     d  ctx                            *   value                                void *
     d  value                          *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

      /if defined(LIBXML_SAX1_ENABLED)
     d xmlSAXDefaultVersion...
     d                 pr            10i 0 extproc('xmlSAXDefaultVersion')
     d  version                      10i 0 value
      /endif                                                                    LIBXML_SAX1_ENABLED

     d xmlSAXVersion   pr            10i 0 extproc('xmlSAXVersion')
     d  hdlr                               like(xmlSAXHandler)
     d  version                      10i 0 value

     d xmlSAX2InitDefaultSAXHandler...
     d                 pr                  extproc(
     d                                      'xmlSAX2InitDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandler)
     d  warning                      10i 0 value

      /if defined(LIBXML_HTML_ENABLED)
     d xmlSAX2InitHtmlDefaultSAXHandler...
     d                 pr                  extproc(
     d                                      'xmlSAX2InitHtmlDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandler)

     d htmlDefaultSAXHandlerInit...
     d                 pr                  extproc('htmlDefaultSAXHandlerInit')
      /endif

      /if defined(LIBXML_DOCB_ENABLED)
     d xmlSAX2InitDocbDefaultSAXHandler...
     d                 pr                  extproc(
     d                                      'xmlSAX2InitDocbDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandler)

     d docbDefaultSAXHandlerInit...
     d                 pr                  extproc('docbDefaultSAXHandlerInit')
      /endif

     d xmlDefaultSAXHandlerInit...
     d                 pr                  extproc('xmlDefaultSAXHandlerInit')

      /endif                                                                    XML_SAX2_H__
