      * Summary: Old SAX version 1 handler, deprecated
      * Description: DEPRECATED set of SAX version 1 interfaces used to
      *              build the DOM tree.
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_SAX_H__)
      /define XML_SAX_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/parser"
      /include "libxmlrpg/xlink"

      /if defined(LIBXML_LEGACY_ENABLED)

     d getPublicId     pr              *   extproc('getPublicId')               const xmlChar *
     d  ctx                            *   value                                void *

     d getSystemId     pr              *   extproc('getSystemId')               const xmlChar *
     d  ctx                            *   value                                void *

     d setDocumentLocator...
     d                 pr                  extproc('setDocumentLocator')
     d  ctx                            *   value                                void *
     d  loc                                value like(xmlSAXLocatorPtr)

     d getLineNumber   pr            10i 0 extproc('getLineNumber')
     d  ctx                            *   value                                void *

     d getColumnNumber...
     d                 pr            10i 0 extproc('getColumnNumber')
     d  ctx                            *   value                                void *

     d isStandalone    pr            10i 0 extproc('isStandalone')
     d  ctx                            *   value                                void *

     d hasInternalSubset...
     d                 pr            10i 0 extproc('hasInternalSubset')
     d  ctx                            *   value                                void *

     d hasExternalSubset...
     d                 pr            10i 0 extproc('hasExternalSubset')
     d  ctx                            *   value                                void *

     d internalSubset  pr                  extproc('internalSubset')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *
     d  SystemID                       *   value options(*string)               const xmlChar *

     d externalSubset  pr                  extproc('externalSubset')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  ExternalID                     *   value options(*string)               const xmlChar *
     d  SystemID                       *   value options(*string)               const xmlChar *

     d getEntity       pr                  extproc('getEntity')
     d                                     like(xmlEntityPtr)
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d getParameterEntity...
     d                 pr                  extproc('getParameterEntity')
     d                                     like(xmlEntityPtr)
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d resolveEntity   pr                  extproc('resolveEntity')
     d                                     like(xmlParserInputPtr)
     d  ctx                            *   value                                void *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *

     d entityDecl      pr                  extproc('entityDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               xmlChar *

     d attributeDecl   pr                  extproc('attributeDecl')
     d  ctx                            *   value                                void *
     d  elem                           *   value options(*string)               const xmlChar *
     d  fullname                       *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  def                          10i 0 value
     d  defaultValue                   *   value options(*string)               const xmlChar *
     d  tree                               value like(xmlEnumerationPtr)

     d elementDecl     pr                  extproc('elementDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  type                         10i 0 value
     d  content                            value like(xmlElementContentPtr)

     d notationDecl    pr                  extproc('notationDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *

     d unparsedEntityDecl...
     d                 pr                  extproc('unparsedEntityDecl')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *
     d  publicId                       *   value options(*string)               const xmlChar *
     d  systemId                       *   value options(*string)               const xmlChar *
     d  notationName                   *   value options(*string)               const xmlChar *

     d startDocument   pr                  extproc('startDocument')
     d  ctx                            *   value                                void *

     d endDocument     pr                  extproc('endDocument')
     d  ctx                            *   value                                void *

     d attribute       pr                  extproc('attribute')
     d  ctx                            *   value                                void *
     d  fullname                       *   value options(*string)               const xmlChar *
     d  value                          *   value options(*string)               const xmlChar *

     d startElement    pr                  extproc('startElement')
     d  ctx                            *   value                                void *
     d  fullname                       *   value options(*string)               const xmlChar *
     d  atts                           *                                        const xmlChar *(*)

     d endElement      pr                  extproc('endElement')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d reference       pr                  extproc('reference')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d characters      pr                  extproc('characters')
     d  ctx                            *   value                                void *
     d  ch                             *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d ignorableWhitespace...
     d                 pr                  extproc('ignorableWhitespace')
     d  ctx                            *   value                                void *
     d  ch                             *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d processingInstruction...
     d                 pr                  extproc('processingInstruction')
     d  ctx                            *   value                                void *
     d  target                         *   value options(*string)               const xmlChar *
     d  data                           *   value options(*string)               const xmlChar *

     d globalNamespace...
     d                 pr                  extproc('globalNamespace')
     d  ctx                            *   value                                void *
     d  href                           *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *

     d setNamespace    pr                  extproc('setNamespace')
     d  ctx                            *   value                                void *
     d  name                           *   value options(*string)               const xmlChar *

     d getNamespace    pr                  extproc('getNamespace')
     d                                     like(xmlNsPtr)
     d  ctx                            *   value                                void *

     d checkNamespace  pr            10i 0 extproc('checkNamespace')
     d  ctx                            *   value                                void *
     d  nameSpace                      *   value options(*string)               xmlChar *

     d namespaceDecl   pr                  extproc('namespaceDecl')
     d  ctx                            *   value                                void *
     d  href                           *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *

     d comment         pr                  extproc('comment')
     d  ctx                            *   value                                void *
     d  value                          *   value options(*string)               const xmlChar *

     d cdataBlock      pr                  extproc('cdataBlock')
     d  ctx                            *   value                                void *
     d  value                          *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

      /if defined(LIBXML_SAX1_ENABLED)
     d initxmlDefaultSAXHandler...
     d                 pr                  extproc('initxmlDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandlerV1)
     d  warning                      10i 0 value

      /if defined(LIBXML_HTML_ENABLED)
     d inithtmlDefaultSAXHandler...
     d                 pr                  extproc('inithtmlDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandlerV1)
      /endif

      /if defined(LIBXML_DOCB_ENABLED)
     d initdocbDefaultSAXHandler...
     d                 pr                  extproc('initdocbDefaultSAXHandler')
     d  hdlr                               like(xmlSAXHandlerV1)
      /endif
      /endif                                                                    LIBXML_SAX1_ENABLED

      /endif                                                                    LIBXML_LEGACY_ENABLD

      /endif                                                                    XML_SAX_H__
