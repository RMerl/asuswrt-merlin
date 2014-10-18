      * Summary: The DTD validation
      * Description: API for the DTD handling and the validity checking
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_VALID_H__)
      /define XML_VALID_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/xmlerror"
      /include "libxmlrpg/tree"
      /include "libxmlrpg/list"
      /include "libxmlrpg/xmlautomata"
      /include "libxmlrpg/xmlregexp"

      * Validation state added for non-determinist content model.

     d xmlValidStatePtr...
     d                 s               *   based(######typedef######)

      * xmlValidityErrorFunc:
      * @ctx:  usually an xmlValidCtxtPtr to a validity error context,
      *        but comes from ctxt->userData (which normally contains such
      *        a pointer); ctxt->userData can be changed by the user.
      * @msg:  the string to format *printf like vararg
      * @...:  remaining arguments to the format
      *
      * Callback called when a validity error is found. This is a message
      * oriented function similar to an *printf function.

     d xmlValidityErrorFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * xmlValidityWarningFunc:
      * @ctx:  usually an xmlValidCtxtPtr to a validity error context,
      *        but comes from ctxt->userData (which normally contains such
      *        a pointer); ctxt->userData can be changed by the user.
      * @msg:  the string to format *printf like vararg
      * @...:  remaining arguments to the format
      *
      * Callback called when a validity warning is found. This is a message
      * oriented function similar to an *printf function.

     d xmlValidityWarningFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * xmlValidCtxt:
      * An xmlValidCtxt is used for error reporting when validating.

     d xmlValidCtxtPtr...
     d                 s               *   based(######typedef######)

     d xmlValidCtxt    ds                  based(xmlValidCtxtPtr)
     d                                     align qualified
     d  userdata                       *                                        void *
     d  error                              like(xmlValidityErrorFunc)           Error callback
     d  warning                            like(xmlValidityWarningFunc)         Warning callback
      *
      * Node analysis stack used when validating within entities
      *
     d  node                               like(xmlNodePtr)                     Current parsed Node
     d  nodeNr                       10i 0                                      Parsing stack depth
     d  nodeMax                      10i 0                                      Max stack depth
     d  nodeTab                        *                                        xmlNodePtr *
      *
     d  finishDtd                    10u 0                                      Finish validtng DTD?
     d  doc                                like(xmlDocPtr)                      The document
     d  valid                        10i 0                                      Temp check result
      *
      * state state used for non-determinist content validation
      *
     d  vstate                         *                                        xmlValidState *
     d  vstateNr                     10i 0                                      Validat. stack depth
     d  vstateMax                    10i 0                                      Max stack depth
     d  vstateTab                      *                                        xmlValidState *
      *
      /if defined(LIBXML_REGEXP_ENABLED)
     d  am                                 like(xmlAutomataPtr)                 The automata
     d  state                              like(xmlAutomataStatePtr)            Automata build state
      /else
     d  am                             *
     d  state                          *
      /endif

      * ALL notation declarations are stored in a table.
      * There is one table per DTD.

     d xmlNotationTablePtr...
     d                 s               *   based(######typedef######)

      * ALL element declarations are stored in a table.
      * There is one table per DTD.

     d xmlElementTablePtr...
     d                 s               *   based(######typedef######)

      * ALL attribute declarations are stored in a table.
      * There is one table per DTD.

     d xmlAttributeTablePtr...
     d                 s               *   based(######typedef######)

      * ALL IDs attributes are stored in a table.
      * There is one table per document.

     d xmlIDTablePtr   s               *   based(######typedef######)

      * ALL Refs attributes are stored in a table.
      * There is one table per document.

     d xmlRefTablePtr  s               *   based(######typedef######)

      * Notation

     d xmlAddNotationDecl...
     d                 pr                  extproc('xmlAddNotationDecl')
     d                                     like(xmlNotationPtr)
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  PublicID                       *   value options(*string)               const xmlChar *
     d  SystemID                       *   value options(*string)               const xmlChar *

      /if defined(LIBXML_TREE_ENABLED)
     d xmlCopyNotationTable...
     d                 pr                  extproc('xmlCopyNotationTable')
     d                                     like(xmlNotationPtr)
     d  table                              value like(xmlNotationTablePtr)
      /endif                                                                    LIBXML_TREE_ENABLED

     d xmlFreeNotationTable...
     d                 pr                  extproc('xmlFreeNotationTable')
     d  table                              value like(xmlNotationTablePtr)

      /if defined(LIBXML_OUTPUT_ENABLED)
     d xmlDumpNotationDecl...
     d                 pr                  extproc('xmlDumpNotationDecl')
     d  buf                                value like(xmlBufferPtr)
     d  nota                               value like(xmlNotationPtr)

     d xmlDumpNotationTable...
     d                 pr                  extproc('xmlDumpNotationTable')
     d  buf                                value like(xmlBufferPtr)
     d  table                              value like(xmlNotationTablePtr)
      /endif                                                                    LIBXML_OUTPUT_ENABLD

      * Element Content
      * the non Doc version are being deprecated

     d xmlNewElementContent...
     d                 pr                  extproc('xmlNewElementContent')
     d                                     like(xmlElementContentPtr)
     d  name                           *   value                                const xmlChar *
     d  type                               value like(xmlElementContentType)

     d xmlCopyElementContent...
     d                 pr                  extproc('xmlCopyElementContent')
     d                                     like(xmlElementContentPtr)
     d  content                            value like(xmlElementContentPtr)

     d xmlFreeElementContent...
     d                 pr                  extproc('xmlFreeElementContent')
     d  cur                                value like(xmlElementContentPtr)

      * the new versions with doc argument

     d xmlNewDocElementContent...
     d                 pr                  extproc('xmlNewDocElementContent')
     d                                     like(xmlElementContentPtr)
     d  doc                                value like(xmlDocPtr)
     d  name                           *   value                                const xmlChar *
     d  type                               value like(xmlElementContentType)

     d xmlCopyDocElementContent...
     d                 pr                  extproc('xmlCopyDocElementContent')
     d                                     like(xmlElementContentPtr)
     d  doc                                value like(xmlDocPtr)
     d  content                            value like(xmlElementContentPtr)

     d xmlFreeDocElementContent...
     d                 pr                  extproc('xmlFreeDocElementContent')
     d  doc                                value like(xmlDocPtr)
     d  cur                                value like(xmlElementContentPtr)

     d xmlSnprintfElementContent...
     d                 pr                  extproc('xmlSnprintfElementContent')
     d  buf                       65535    options(*varsize)
     d  size                         10i 0 value
     d  content                            value like(xmlElementContentPtr)
     d  englob                       10i 0 value

      /if defined(LIBXML_OUTPUT_ENABLED)
      * DEPRECATED
     d xmlSprintfElementContent...
     d                 pr                  extproc('xmlSprintfElementContent')
     d  buf                       65535    options(*varsize)
     d  content                            value like(xmlElementContentPtr)
     d  englob                       10i 0 value
      /endif                                                                    LIBXML_OUTPUT_ENABLD

      * DEPRECATED

      * Element

     d xmlAddElementDecl...
     d                 pr                  extproc('xmlAddElementDecl')
     d                                     like(xmlElementPtr)
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  type                               value like(xmlElementTypeVal)
     d  content                            value like(xmlElementContentPtr)

      /if defined(LIBXML_TREE_ENABLED)
     d xmlCopyElementTable...
     d                 pr                  extproc('xmlCopyElementTable')
     d                                     like(xmlElementTablePtr)
     d  table                              value like(xmlElementTablePtr)
      /endif                                                                    LIBXML_TREE_ENABLED

     d xmlFreeElementTable...
     d                 pr                  extproc('xmlFreeElementTable')
     d  table                              value like(xmlElementTablePtr)

      /if defined(LIBXML_OUTPUT_ENABLED)
     d xmlDumpElementTable...
     d                 pr                  extproc('xmlDumpElementTable')
     d  buf                                value like(xmlBufferPtr)
     d  table                              value like(xmlElementTablePtr)

     d xmlDumpElementDecl...
     d                 pr                  extproc('xmlDumpElementDecl')
     d  buf                                value like(xmlBufferPtr)
     d  elem                               value like(xmlElementPtr)
      /endif                                                                    LIBXML_OUTPUT_ENABLD

      * Enumeration

     d xmlCreateEnumeration...
     d                 pr                  extproc('xmlCreateEnumeration')
     d                                     like(xmlEnumerationPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlFreeEnumeration...
     d                 pr                  extproc('xmlFreeEnumeration')
     d  cur                                value like(xmlEnumerationPtr)

      /if defined(LIBXML_TREE_ENABLED)
     d xmlCopyEnumeration...
     d                 pr                  extproc('xmlCopyEnumeration')
     d                                     like(xmlEnumerationPtr)
     d  cur                                value like(xmlEnumerationPtr)
      /endif                                                                    LIBXML_TREE_ENABLED

      * Attribute

     d xmlAddAttributeDecl...
     d                 pr                  extproc('xmlAddAttributeDecl')
     d                                     like(xmlAttributePtr)
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  elem                           *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *
     d  type                               value like(xmlAttributeType)
     d  def                                value like(xmlAttributeDefault)
     d  defaultValue                   *   value options(*string)               const xmlChar *
     d  tree                               value like(xmlEnumerationPtr)

      /if defined(LIBXML_TREE_ENABLED)
     d xmlCopyAttributeTable...
     d                 pr                  extproc('xmlCopyAttributeTable')
     d                                     like(xmlAttributeTablePtr)
     d  table                              value like(xmlAttributeTablePtr)
      /endif                                                                    LIBXML_TREE_ENABLED

     d xmlFreeAttributeTable...
     d                 pr                  extproc('xmlFreeAttributeTable')
     d  table                              value like(xmlAttributeTablePtr)

      /if defined(LIBXML_OUTPUT_ENABLED)
     d xmlDumpAttributeTable...
     d                 pr                  extproc('xmlDumpAttributeTable')
     d  buf                                value like(xmlBufferPtr)
     d  table                              value like(xmlAttributeTablePtr)

     d xmlDumpAttributeDecl...
     d                 pr                  extproc('xmlDumpAttributeDecl')
     d  buf                                value like(xmlBufferPtr)
     d  attr                               value like(xmlAttributePtr)
      /endif                                                                    LIBXML_OUTPUT_ENABLD

      * IDs

     d xmlAddID        pr                  extproc('xmlAddID')
     d                                     like(xmlIDPtr)
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  attr                               value like(xmlAttrPtr)

     d xmlFreeIdTable  pr                  extproc('xmlFreeIDTable')
     d  table                              value like(xmlIDTablePtr)

     d xmlGetID        pr                  extproc('xmlGetID')
     d                                     like(xmlAttrPtr)
     d  doc                                value like(xmlDocPtr)
     d  ID                             *   value options(*string)               const xmlChar *

     d xmlIsID         pr            10i 0 extproc('xmlIsID')
     d  doc                                value like(xmlDocPtr)
     d  node                               value like(xmlNodePtr)
     d  attr                               value like(xmlAttrPtr)

     d xmlRemoveID     pr            10i 0 extproc('xmlRemoveID')
     d  doc                                value like(xmlDocPtr)
     d  attr                               value like(xmlAttrPtr)

      * IDREFs

     d xmlAddRef       pr                  extproc('xmlAddRef')
     d                                     like(xmlRefPtr)
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  attr                               value like(xmlAttrPtr)

     d xmlFreeRefTable...
     d                 pr                  extproc('xmlFreeRefTable')
     d  table                              value like(xmlRefTablePtr)

     d xmlIsRef        pr            10i 0 extproc('xmlIsRef')
     d  doc                                value like(xmlDocPtr)
     d  node                               value like(xmlNodePtr)
     d  attr                               value like(xmlAttrPtr)

     d xmlRemoveRef    pr            10i 0 extproc('xmlRemoveRef')
     d  doc                                value like(xmlDocPtr)
     d  attr                               value like(xmlAttrPtr)

     d xmlGetRefs      pr                  extproc('xmlGetRefs')
     d                                     like(xmlListPtr)
     d  doc                                value like(xmlDocPtr)
     d  ID                             *   value options(*string)               const xmlChar *

      * The public function calls related to validity checking.

      /if defined(LIBXML_VALID_ENABLED)
      * Allocate/Release Validation Contexts

     d xmlNewValidCtxt...
     d                 pr                  extproc('xmlNewValidCtxt')
     d                                     like(xmlValidCtxtPtr)

     d xmlFreeValidCtxt...
     d                 pr                  extproc('xmlFreeValidCtxt')
     d  ctxt                               value like(xmlValidCtxtPtr)

     d xmlValidateRoot...
     d                 pr            10i 0 extproc('xmlValidateRoot')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)

     d xmlValidateElementDecl...
     d                 pr            10i 0 extproc('xmlValidateElementDecl')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlElementPtr)

     d xmlValidNormalizeAttributeValue...
     d                 pr              *   extproc(                             xmlChar *
     d                                      'xmlValidNormalizeAttributeValue')
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidCtxtNormalizeAttributeValue...
     d                 pr              *   extproc('xmlValidCtxt+               xmlChar *
     d                                     NormalizeAttributeValue')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateAttributeDecl...
     d                 pr            10i 0 extproc('xmlValidateAttributeDecl')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  attr                               value like(xmlAttributePtr)

     d xmlValidateAttributeValue...
     d                 pr            10i 0 extproc('xmlValidateAttributeValue')
     d  type                               value like(xmlAttributeType)
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateNotationDecl...
     d                 pr            10i 0 extproc('xmlValidateNotationDecl')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  nota                               value like(xmlNotationPtr)

     d xmlValidateDtd  pr            10i 0 extproc('xmlValidateDtd')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  dtd                                value like(xmlDtdPtr)

     d xmlValidateDtdFinal...
     d                 pr            10i 0 extproc('xmlValidateDtdFinal')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)

     d xmlValidateDocument...
     d                 pr            10i 0 extproc('xmlValidateDocument')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)

     d xmlValidateElement...
     d                 pr            10i 0 extproc('xmlValidateElement')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)

     d xmlValidateOneElement...
     d                 pr            10i 0 extproc('xmlValidateOneElement')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)

     d xmlValidateOneAttribute...
     d                 pr            10i 0 extproc('xmlValidateOneAttribute')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  attr                               value like(xmlAttrPtr)
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateOneNamespace...
     d                 pr            10i 0 extproc('xmlValidateOneNamespace')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  ns                                 value like(xmlNsPtr)
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateDocumentFinal...
     d                 pr            10i 0 extproc('xmlValidateDocumentFinal')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
      /endif                                                                    LIBXML_VALID_ENABLED

      /undefine XML_TESTVAL
      /if defined(LIBXML_VALID_ENABLED)
      /define XML_TESTVAL
      /elseif defined(LIBXML_SCHEMAS_ENABLED)
      /define XML_TESTVAL
      /endif
      /if defined(XML_TESTVAL)
     d xmlValidateNotationUse...
     d                 pr            10i 0 extproc('xmlValidateNotationUse')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  notationName                   *   value options(*string)               const xmlChar *

      /undefine XML_TESTVAL
      /endif

     d xmlIsMixedElement...
     d                 pr            10i 0 extproc('xmlIsMixedElement')
     d  doc                                value like(xmlDocPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlGetDtdAttrDesc...
     d                 pr                  extproc('xmlGetDtdAttrDesc')
     d                                     like(xmlAttributePtr)
     d  dtd                                value like(xmlDtdPtr)
     d  elem                           *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *

     d xmlGetDtdQAttrDesc...
     d                 pr                  extproc('xmlGetDtdQAttrDesc')
     d                                     like(xmlAttributePtr)
     d  dtd                                value like(xmlDtdPtr)
     d  elem                           *   value options(*string)               const xmlChar *
     d  name                           *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *

     d xmlGetDtdNotationDesc...
     d                 pr                  extproc('xmlGetDtdNotationDesc')
     d                                     like(xmlNotationPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlGetDtdQElementDesc...
     d                 pr                  extproc('xmlGetDtdQElementDesc')
     d                                     like(xmlElementPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  prefix                         *   value options(*string)               const xmlChar *

     d xmlGetDtdElementDesc...
     d                 pr                  extproc('xmlGetDtdElementDesc')
     d                                     like(xmlElementPtr)
     d  dtd                                value like(xmlDtdPtr)
     d  name                           *   value options(*string)               const xmlChar *

      /if defined(LIBXML_VALID_ENABLED)
     d xmlValidGetPotentialChildren...
     d                 pr            10i 0 extproc(
     d                                      'xmlValidGetPotentialChildren')
     d  ctree                          *   value                                xmlElementContent *
     d  names                          *                                        const xmlChar *(*)
     d  len                          10i 0
     d  max                          10i 0 value

     d xmlValidGetValidElements...
     d                 pr            10i 0 extproc('xmlValidGetValidElements')
     d  prev                               like(xmlNodePtr)
     d  next                               like(xmlNodePtr)
     d  names                          *                                        const xmlChar *(*)
     d  max                          10i 0 value

     d xmlValidateNameValue...
     d                 pr            10i 0 extproc('xmlValidateNameValue')
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateNamesValue...
     d                 pr            10i 0 extproc('xmlValidateNamesValue')
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateNmtokenValue...
     d                 pr            10i 0 extproc('xmlValidateNmtokenValue')
     d  value                          *   value options(*string)               const xmlChar *

     d xmlValidateNmtokensValue...
     d                 pr            10i 0 extproc('xmlValidateNmtokensValue')
     d  value                          *   value options(*string)               const xmlChar *

      /if defined(LIBXML_REGEXP_ENABLED)
      * Validation based on the regexp support

     d xmlValidBuildContentModel...
     d                 pr            10i 0 extproc('xmlValidBuildContentModel')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  elem                               value like(xmlElementPtr)

     d xmlValidatePushElement...
     d                 pr            10i 0 extproc('xmlValidatePushElement')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  qname                          *   value options(*string)               const xmlChar *

     d xmlValidatePushCData...
     d                 pr            10i 0 extproc('xmlValidatePushCData')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  data                           *   value options(*string)               const xmlChar *
     d  len                          10i 0 value

     d xmlValidatePopElement...
     d                 pr            10i 0 extproc('xmlValidatePopElement')
     d  ctxt                               value like(xmlValidCtxtPtr)
     d  doc                                value like(xmlDocPtr)
     d  elem                               value like(xmlNodePtr)
     d  qname                          *   value options(*string)               const xmlChar *

      /endif                                                                    LIBXML_REGEXP_ENABLD
      /endif                                                                    LIBXML_VALID_ENABLED
      /endif                                                                    XML_VALID_H__
