      * Summary: the XML document serializer
      * Description: API to save document or subtree of document
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_XMLSAVE_H__)
      /define XML_XMLSAVE_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"
      /include "libxmlrpg/encoding"
      /include "libxmlrpg/xmlIO"

      /if defined(LIBXML_OUTPUT_ENABLED)

      * xmlSaveOption:
      *
      * This is the set of XML save options that can be passed down
      * to the xmlSaveToFd() and similar calls.

     d xmlSaveOption   s             10i 0 based(######typedef######)           enum
     d  XML_SAVE_FORMAT...                                                      Format save output
     d                 c                   X'0001'
     d  XML_SAVE_NO_DECL...                                                     Drop xml declaration
     d                 c                   X'0002'
     d  XML_SAVE_NO_EMPTY...                                                    No empty tags
     d                 c                   X'0004'
     d  XML_SAVE_NO_XHTML...                                                    No XHTML1 specific
     d                 c                   X'0008'
     d  XML_SAVE_XHTML...                                                       Frce XHTML1 specific
     d                 c                   X'0010'
     d  XML_SAVE_AS_XML...                                                      Frce XML on HTML doc
     d                 c                   X'0020'
     d  XML_SAVE_AS_HTML...                                                     Frce HTML on XML doc
     d                 c                   X'0040'
     d  XML_SAVE_WSNONSIG...                                                    Fmt w/ non-sig space
     d                 c                   X'0080'

     d xmlSaveCtxtPtr  s               *   based(######typedef######)

     d xmlSaveToFd     pr                  extproc('xmlSaveToFd')
     d                                     like(xmlSaveCtxtPtr)
     d  fd                           10i 0 value
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d xmlSaveToFilename...
     d                 pr                  extproc('xmlSaveToFilename')
     d                                     like(xmlSaveCtxtPtr)
     d  filename                       *   value options(*string)               const char *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d xmlSaveToBuffer...
     d                 pr                  extproc('xmlSaveToBuffer')
     d                                     like(xmlSaveCtxtPtr)
     d  buffer                             value like(xmlBufferPtr)
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d xmlSaveToIO     pr                  extproc('xmlSaveToIO')
     d                                     like(xmlSaveCtxtPtr)
     d  iowrite                            value like(xmlOutputWriteCallback)
     d  ioclose                            value like(xmlOutputCloseCallback)
     d  ioctx                          *   value                                void *
     d  encoding                       *   value options(*string)               const char *
     d  options                      10i 0 value

     d xmlSaveDoc      pr            20i 0 extproc('xmlSaveDoc')
     d  ctxt                               value like(xmlSaveCtxtPtr)
     d  doc                                value like(xmlDocPtr)

     d xmlSaveTree     pr            20i 0 extproc('xmlSaveTree')
     d  ctxt                               value like(xmlSaveCtxtPtr)
     d  node                               value like(xmlNodePtr)

     d xmlSaveFlush    pr            10i 0 extproc('xmlSaveFlush')
     d  ctxt                               value like(xmlSaveCtxtPtr)

     d xmlSaveClose    pr            10i 0 extproc('xmlSaveClose')
     d  ctxt                               value like(xmlSaveCtxtPtr)

     d xmlSaveSetEscape...
     d                 pr            10i 0 extproc('xmlSaveSetEscape')
     d  ctxt                               value like(xmlSaveCtxtPtr)
     d  escape                             value like(xmlCharEncodingOutputFunc)

     d xmlSaveSetAttrEscape...
     d                 pr            10i 0 extproc('xmlSaveSetAttrEscape')
     d  ctxt                               value like(xmlSaveCtxtPtr)
     d  escape                             value like(xmlCharEncodingOutputFunc)

      /endif                                                                    LIBXML_OUTPUT_ENABLD
      /endif                                                                    XML_XMLSAVE_H__
