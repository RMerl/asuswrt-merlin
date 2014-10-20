      * Summary: text writing API for XML
      * Description: text writing API for XML
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_XMLWRITER_H__)
      /define XML_XMLWRITER_H__

      /include "libxmlrpg/xmlversion"

      /if defined(LIBXML_WRITER_ENABLED)

      /include "libxmlrpg/xmlstdarg"
      /include "libxmlrpg/xmlIO"
      /include "libxmlrpg/list"
      /include "libxmlrpg/xmlstring"

     d xmlTextWriterPtr...
     d                 s               *   based(######typedef######)

      * Constructors & Destructor

     d xmlNewTextWriter...
     d                 pr                  extproc('xmlNewTextWriter')
     d                                     like(xmlTextWriterPtr)
     d  out                                value like(xmlOutputBufferPtr)

     d xmlNewTextWriterFilename...
     d                 pr                  extproc('xmlNewTextWriterFilename')
     d                                     like(xmlTextWriterPtr)
     d  uri                            *   value options(*string)               const char *
     d  compression                  10i 0 value

     d xmlNewTextWriterMemory...
     d                 pr                  extproc('xmlNewTextWriterMemory')
     d                                     like(xmlTextWriterPtr)
     d  buf                                value like(xmlBufferPtr)
     d  compression                  10i 0 value

     d xmlNewTextWriterPushParser...
     d                 pr                  extproc('xmlNewTextWriterPushParser')
     d                                     like(xmlTextWriterPtr)
     d  ctxt                               value like(xmlParserCtxtPtr)
     d  compression                  10i 0 value

     d xmlNewTextWriterDoc...
     d                 pr                  extproc('xmlNewTextWriterDoc')
     d                                     like(xmlTextWriterPtr)
     d  doc                                like(xmlDocPtr)
     d  compression                  10i 0 value

     d xmlNewTextWriterTree...
     d                 pr                  extproc('xmlNewTextWriterTree')
     d                                     like(xmlTextWriterPtr)
     d  doc                                value like(xmlDocPtr)
     d  node                               value like(xmlNodePtr)
     d  compression                  10i 0 value

     d xmlFreeTextWriter...
     d                 pr                  extproc('xmlFreeTextWriter')
     d  writer                             value like(xmlTextWriterPtr)

      * Functions

      * Document

     d xmlTextWriterStartDocument...
     d                 pr            10i 0 extproc('xmlTextWriterStartDocument')
     d  writer                             value like(xmlTextWriterPtr)
     d  version                        *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  standalone                     *   value options(*string)               const char *

     d xmlTextWriterEndDocument...
     d                 pr            10i 0 extproc('xmlTextWriterEndDocument')
     d  writer                             value like(xmlTextWriterPtr)

      * Comments

     d xmlTextWriterStartComment...
     d                 pr            10i 0 extproc('xmlTextWriterStartComment')
     d  writer                             value like(xmlTextWriterPtr)

     d xmlTextWriterEndComment...
     d                 pr            10i 0 extproc('xmlTextWriterEndComment')
     d  writer                             value like(xmlTextWriterPtr)

     d xmlTextWriterWriteFormatComment...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatComment')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string: *nopass)      const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatComment...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatComment')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteComment...
     d                 pr            10i 0 extproc('xmlTextWriterWriteComment')
     d  writer                             value like(xmlTextWriterPtr)
     d  content                        *   value options(*string)               const xmlChar *

      * Elements

     d xmlTextWriterStartElement...
     d                 pr            10i 0 extproc('xmlTextWriterStartElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlTextWriterStartElementNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartElementNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *

     d xmlTextWriterEndElement...
     d                 pr            10i 0 extproc('xmlTextWriterEndElement')
     d  writer                             value like(xmlTextWriterPtr)

     d xmlTextWriterFullEndElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterFullEndElement')
     d  writer                             value like(xmlTextWriterPtr)

      * Elements conveniency functions

     d xmlTextWriterWriteFormatElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteElement...
     d                 pr            10i 0 extproc('xmlTextWriterWriteElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteFormatElementNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatElementNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatElementNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatElementNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteElementNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteElementNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * Text

     d xmlTextWriterWriteFormatRaw...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatRaw')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatRaw...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatRaw')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteRawLen...
     d                 pr            10i 0 extproc('xmlTextWriterWriteRawLen')
     d  writer                             value like(xmlTextWriterPtr)
     d  content                        *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlTextWriterWriteRaw...
     d                 pr            10i 0 extproc('xmlTextWriterWriteRaw')
     d  writer                             value like(xmlTextWriterPtr)
     d  content                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteFormatString...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatString')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatString...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatString')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteString...
     d                 pr            10i 0 extproc('xmlTextWriterWriteString')
     d  writer                             value like(xmlTextWriterPtr)
     d  content                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteBase64...
     d                 pr            10i 0 extproc('xmlTextWriterWriteBase64')
     d  writer                             value like(xmlTextWriterPtr)
     d  data                           *   value options(*string)               const char *
     d  start                        10i 0 value
     d  len                          10i 0 value

     d xmlTextWriterWriteBinHex...
     d                 pr            10i 0 extproc('xmlTextWriterWriteBinHex')
     d  writer                             value like(xmlTextWriterPtr)
     d  data                           *   value options(*string)               const char *
     d  start                        10i 0 value
     d  len                          10i 0 value

      * Attributes

     d xmlTextWriterStartAttribute...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartAttribute')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlTextWriterStartAttributeNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartAttributeNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *

     d xmlTextWriterEndAttribute...
     d                 pr            10i 0 extproc('xmlTextWriterEndAttribute')
     d  writer                             value like(xmlTextWriterPtr)

      * Attributes conveniency functions

     d xmlTextWriterWriteFormatAttribute...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatAttribute')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatAttribute...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatAttribute')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteAttribute...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteAttribute')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteFormatAttributeNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatAttributeNS'
     d                                     )
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatAttributeNS...
     d                 pr            10i 0 extproc('xmlTextWriterWriteVFormatAt-
     d                                     tributeNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteAttributeNS...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteAttributeNS')
     d  writer                             value like(xmlTextWriterPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  namespaceURI                   *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * PI's

     d xmlTextWriterStartPI...
     d                 pr            10i 0 extproc('xmlTextWriterStartPI')
     d  writer                             value like(xmlTextWriterPtr)
     d  target                         *   value options(*string)               const xmlChar *

     d xmlTextWriterEndPI...
     d                 pr            10i 0 extproc('xmlTextWriterEndPI')
     d  writer                             value like(xmlTextWriterPtr)

      * PI conveniency functions

     d xmlTextWriterWriteFormatPI...
     d                 pr            10i 0 extproc('xmlTextWriterWriteFormatPI')
     d  writer                             value like(xmlTextWriterPtr)
     d  target                         *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatPI...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatPI')
     d  writer                             value like(xmlTextWriterPtr)
     d  target                         *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWritePI...
     d                 pr            10i 0 extproc('xmlTextWriterWritePI')
     d  writer                             value like(xmlTextWriterPtr)
     d  target                         *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * xmlTextWriterWriteProcessingInstruction:
      *
      * This macro maps to xmlTextWriterWritePI

     d xmlTextWriterWriteProcessingInstruction...
     d                 pr            10i 0 extproc('xmlTextWriterWritePI')
     d  writer                             value like(xmlTextWriterPtr)
     d  target                         *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * CDATA

     d xmlTextWriterStartCDATA...
     d                 pr            10i 0 extproc('xmlTextWriterStartCDATA')
     d  writer                             value like(xmlTextWriterPtr)

     d xmlTextWriterEndCDATA...
     d                 pr            10i 0 extproc('xmlTextWriterEndCDATA')
     d  writer                             value like(xmlTextWriterPtr)

      * CDATA conveniency functions

     d xmlTextWriterWriteFormatCDATA...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatCDATA')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatCDATA...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatCDATA')
     d  writer                             value like(xmlTextWriterPtr)
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteCDATA...
     d                 pr            10i 0 extproc('xmlTextWriterWriteCDATA')
     d  writer                             value like(xmlTextWriterPtr)
     d  content                        *   value options(*string)               const xmlChar *

      * DTD

     d xmlTextWriterStartDTD...
     d                 pr            10i 0 extproc('xmlTextWriterStartDTD')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *

     d xmlTextWriterEndDTD...
     d                 pr            10i 0 extproc('xmlTextWriterEndDTD')
     d  writer                             value like(xmlTextWriterPtr)

      * DTD conveniency functions

     d xmlTextWriterWriteFormatDTD...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatDTD')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatDTD...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatDTD')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteDTD...
     d                 pr            10i 0 extproc('xmlTextWriterWriteDTD')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  subset                         *   value options(*string)               const xmlChar *

      * xmlTextWriterWriteDocType:
      *
      * this macro maps to xmlTextWriterWriteDTD

     d xmlTextWriterWriteDocType...
     d                 pr            10i 0 extproc('xmlTextWriterWriteDTD')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  subset                         *   value options(*string)               const xmlChar *

      * DTD element definition

     d xmlTextWriterStartDTDElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartDTDElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlTextWriterEndDTDElement...
     d                 pr            10i 0 extproc('xmlTextWriterEndDTDElement')
     d  writer                             value like(xmlTextWriterPtr)

      * DTD element definition conveniency functions

     d xmlTextWriterWriteFormatDTDElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatDTDElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatDTDElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatDTDElement'
     d                                     )
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteDTDElement...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDElement')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * DTD attribute list definition

     d xmlTextWriterStartDTDAttlist...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartDTDAttlist')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlTextWriterEndDTDAttlist...
     d                 pr            10i 0 extproc('xmlTextWriterEndDTDAttlist')
     d  writer                             value like(xmlTextWriterPtr)

      * DTD attribute list definition conveniency functions

     d xmlTextWriterWriteFormatDTDAttlist...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteFormatDTDAttlist')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatDTDAttlist...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteVFormatDTDAttlist'
     d                                     )
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteDTDAttlist...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDAttlist')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * DTD entity definition

     d xmlTextWriterStartDTDEntity...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterStartDTDEntity')
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *

     d xmlTextWriterEndDTDEntity...
     d                 pr            10i 0 extproc('xmlTextWriterEndDTDEntity')
     d  writer                             value like(xmlTextWriterPtr)

      * DTD entity definition conveniency functions

     d xmlTextWriterWriteFormatDTDInternalEntity...
     d                 pr            10i 0 extproc('xmlTextWriterWriteFormatDTD-
     d                                     InternalEntity')
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  #vararg1                       *   value options(*string: *nopass)      void *
     d  #vararg2                       *   value options(*string: *nopass)      void *
     d  #vararg3                       *   value options(*string: *nopass)      void *
     d  #vararg4                       *   value options(*string: *nopass)      void *
     d  #vararg5                       *   value options(*string: *nopass)      void *
     d  #vararg6                       *   value options(*string: *nopass)      void *
     d  #vararg7                       *   value options(*string: *nopass)      void *
     d  #vararg8                       *   value options(*string: *nopass)      void *

     d xmlTextWriterWriteVFormatDTDInternalEntity...
     d                 pr            10i 0 extproc('xmlTextWriterWriteVFormatDT-
     d                                     DInternalEntity')
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *
     d  format                         *   value options(*string)               const char *
     d  argptr                             likeds(xmlVaList)

     d xmlTextWriterWriteDTDInternalEntity...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDInternalEntity'
     d                                     )
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteDTDExternalEntity...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDExternalEntity'
     d                                     )
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  ndataid                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteDTDExternalEntityContents...
     d                 pr            10i 0 extproc('xmlTextWriterWriteDTDExtern-
     d                                     alEntityContents')
     d  writer                             value like(xmlTextWriterPtr)
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  ndataid                        *   value options(*string)               const xmlChar *

     d xmlTextWriterWriteDTDEntity...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDEntity')
     d  writer                             value like(xmlTextWriterPtr)
     d  pe                           10i 0 value
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *
     d  ndataid                        *   value options(*string)               const xmlChar *
     d  content                        *   value options(*string)               const xmlChar *

      * DTD notation definition

     d xmlTextWriterWriteDTDNotation...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterWriteDTDNotation')
     d  writer                             value like(xmlTextWriterPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  pubid                          *   value options(*string)               const xmlChar *
     d  sysid                          *   value options(*string)               const xmlChar *

      * Indentation

     d xmlTextWriterSetIndent...
     d                 pr            10i 0 extproc('xmlTextWriterSetIndent')
     d  writer                             value like(xmlTextWriterPtr)
     d  indent                       10i 0 value

     d xmlTextWriterSetIndentString...
     d                 pr            10i 0 extproc(
     d                                     'xmlTextWriterSetIndentString')
     d  writer                             value like(xmlTextWriterPtr)
     d  str                            *   value options(*string)               const xmlChar *

     d xmlTextWriterSetQuoteChar...
     d                 pr            10i 0 extproc('xmlTextWriterSetQuoteChar')
     d  writer                             value like(xmlTextWriterPtr)
     d  quotechar                          value like(xmlChar)

      * misc

     d xmlTextWriterFlush...
     d                 pr            10i 0 extproc('xmlTextWriterFlush')
     d  writer                             value like(xmlTextWriterPtr)

      /endif                                                                    LIBXML_WRITER_ENABLD
      /endif                                                                    XML_XMLWRITER_H__
