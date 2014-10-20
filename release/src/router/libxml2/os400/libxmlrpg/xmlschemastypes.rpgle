      * Summary: implementation of XML Schema Datatypes
      * Description: module providing the XML Schema Datatypes implementation
      *              both definition and validity checking
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_SCHEMA_TYPES_H__)
      /define XML_SCHEMA_TYPES_H__

      /include "libxmlrpg/xmlversion"

      /if defined(LIBXML_SCHEMAS_ENABLED)

      /include "libxmlrpg/schemasInternals"
      /include "libxmlrpg/xmlschemas"

     d xmlSchemaWhitespaceValueType...
     d                 s             10i 0 based(######typedef######)           enum
     d  XML_SCHEMA_WHITESPACE_UNKNOWN...
     d                 c                   0
     d  XML_SCHEMA_WHITESPACE_PRESERVE...
     d                 c                   1
     d  XML_SCHEMA_WHITESPACE_REPLACE...
     d                 c                   2
     d  XML_SCHEMA_WHITESPACE_COLLAPSE...
     d                 c                   3

     d xmlSchemaInitTypes...
     d                 pr                  extproc('xmlSchemaInitTypes')

     d xmlSchemaCleanupTypes...
     d                 pr                  extproc('xmlSchemaCleanupTypes')

     d xmlSchemaGetPredefinedType...
     d                 pr                  extproc('xmlSchemaGetPredefinedType')
     d                                     like(xmlSchemaTypePtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *

     d xmlSchemaValidatePredefinedType...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaValidatePredefinedType')
     d  type                               value like(xmlSchemaTypePtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                            *   value                                xmlSchemaValPtr *

     d xmlSchemaValPredefTypeNode...
     d                 pr            10i 0 extproc('xmlSchemaValPredefTypeNode')
     d  type                               value like(xmlSchemaTypePtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                            *   value                                xmlSchemaValPtr *
     d  node                               value like(xmlNodePtr)

     d xmlSchemaValidateFacet...
     d                 pr            10i 0 extproc('xmlSchemaValidateFacet')
     d  base                               value like(xmlSchemaTypePtr)
     d  facet                              value like(xmlSchemaFacetPtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                                value like(xmlSchemaValPtr)

     d xmlSchemaValidateFacetWhtsp...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaValidateFacetWhtsp')
     d  facet                              value like(xmlSchemaFacetPtr)
     d  fws                                value
     d                                     like(xmlSchemaWhitespaceValueType)
     d  valType                            value like(xmlSchemaValType)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                                value like(xmlSchemaValPtr)
     d  ws                                 value
     d                                     like(xmlSchemaWhitespaceValueType)

     d xmlSchemaFreeValue...
     d                 pr                  extproc('xmlSchemaFreeValue')
     d  val                                value like(xmlSchemaValPtr)

     d xmlSchemaNewFacet...
     d                 pr                  extproc('xmlSchemaNewFacet')
     d                                     like(xmlSchemaFacetPtr)

     d xmlSchemaCheckFacet...
     d                 pr            10i 0 extproc('xmlSchemaCheckFacet')
     d  facet                              value like(xmlSchemaFacetPtr)
     d  typeDecl                           value like(xmlSchemaTypePtr)
     d  ctxt                               value like(xmlSchemaParserCtxtPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlSchemaFreeFacet...
     d                 pr                  extproc('xmlSchemaFreeFacet')
     d  facet                              value like(xmlSchemaFacetPtr)

     d xmlSchemaCompareValues...
     d                 pr            10i 0 extproc('xmlSchemaCompareValues')
     d  x                                  value like(xmlSchemaValPtr)
     d  y                                  value like(xmlSchemaValPtr)

     d xmlSchemaGetBuiltInListSimpleTypeItemType...
     d                 pr                  extproc('xmlSchemaGetBuiltInListSimp-
     d                                     leTypeItemType')
     d                                     like(xmlSchemaTypePtr)
     d  type                               value like(xmlSchemaTypePtr)

     d xmlSchemaValidateListSimpleTypeFacet...
     d                 pr            10i 0 extproc('xmlSchemaValidateListSimple-
     d                                     TypeFacet')
     d  facet                              value like(xmlSchemaFacetPtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  actualLen                    20u 0 value
     d  expectedLen                    *   value                                unsigned long *

     d xmlSchemaGetBuiltInType...
     d                 pr                  extproc('xmlSchemaGetBuiltInType')
     d                                     like(xmlSchemaTypePtr)
     d  type                               value like(xmlSchemaValType)

     d xmlSchemaIsBuiltInTypeFacet...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaIsBuiltInTypeFacet')
     d  type                               value like(xmlSchemaTypePtr)
     d  facetType                    10i 0 value

     d xmlSchemaCollapseString...
     d                 pr              *   extproc('xmlSchemaCollapseString')   xmlChar *
     d  value                          *   value options(*string)               const xmlChar *

     d xmlSchemaWhiteSpaceReplace...
     d                 pr              *   extproc('xmlSchemaWhiteSpaceReplace')xmlChar *
     d  value                          *   value options(*string)               const xmlChar *

     d xmlSchemaGetFacetValueAsULong...
     d                 pr            20u 0 extproc(
     d                                     'xmlSchemaGetFacetValueAsULong')
     d  facet                              value like(xmlSchemaFacetPtr)

     d xmlSchemaValidateLengthFacet...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaValidateLengthFacet')
     d  type                               value like(xmlSchemaTypePtr)
     d  facet                              value like(xmlSchemaFacetPtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                                value like(xmlSchemaValPtr)
     d  length                       20u 0

     d xmlSchemaValidateLengthFacetWhtsp...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaValidateLengthFacetWhtsp')
     d  facet                              value like(xmlSchemaFacetPtr)
     d  valType                            value like(xmlSchemaValType)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                                value like(xmlSchemaValPtr)
     d  length                       20u 0
     d  ws                                 value
     d                                     like(xmlSchemaWhitespaceValueType)

     d xmlSchemaValPredefTypeNodeNoNorm...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaValPredefTypeNodeNoNorm')
     d  type                               value like(xmlSchemaTypePtr)
     d  value                          *   value options(*string)               const xmlChar *
     d  val                                like(xmlSchemaValPtr)
     d  node                               value like(xmlNodePtr)

     d xmlSchemaGetCanonValue...
     d                 pr            10i 0 extproc('xmlSchemaGetCanonValue')
     d  val                                value like(xmlSchemaValPtr)
     d  retValue                       *   value                                const xmlChar * *

     d xmlSchemaGetCanonValueWhtsp...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaGetCanonValueWhtsp')
     d  val                                value like(xmlSchemaValPtr)
     d  retValue                       *   value                                const xmlChar * *
     d  ws                                 value
     d                                     like(xmlSchemaWhitespaceValueType)

     d xmlSchemaValueAppend...
     d                 pr            10i 0 extproc('xmlSchemaValueAppend')
     d  prev                               value like(xmlSchemaValPtr)
     d  cur                                value like(xmlSchemaValPtr)

     d xmlSchemaValueGetNext...
     d                 pr                  extproc('xmlSchemaValueGetNext')
     d                                     like(xmlSchemaValPtr)
     d  cur                                value like(xmlSchemaValPtr)

     d xmlSchemaValueGetAsString...
     d                 pr              *   extproc('xmlSchemaValueGetAsString') const xmlChar *
     d  val                                value like(xmlSchemaValPtr)

     d xmlSchemaValueGetAsBoolean...
     d                 pr            10i 0 extproc('xmlSchemaValueGetAsBoolean')
     d  val                                value like(xmlSchemaValPtr)

     d xmlSchemaNewStringValue...
     d                 pr                  extproc('xmlSchemaNewStringValue')
     d                                     like(xmlSchemaValPtr)
     d  type                               value like(xmlSchemaValType)
     d  value                          *   value options(*string)               const xmlChar *

     d xmlSchemaNewNOTATIONValue...
     d                 pr                  extproc('xmlSchemaNewNOTATIONValue')
     d                                     like(xmlSchemaValPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns                             *   value options(*string)               const xmlChar *

     d xmlSchemaNewQNameValue...
     d                 pr                  extproc('xmlSchemaNewQNameValue')
     d                                     like(xmlSchemaValPtr)
     d  namespaceName                  *   value options(*string)               const xmlChar *
     d  localName                      *   value options(*string)               const xmlChar *

     d xmlSchemaCompareValuesWhtsp...
     d                 pr            10i 0 extproc(
     d                                     'xmlSchemaCompareValuesWhtsp')
     d  x                                  value like(xmlSchemaValPtr)
     d  xws                                value
     d                                     like(xmlSchemaWhitespaceValueType)
     d  y                                  value like(xmlSchemaValPtr)
     d  yws                                value
     d                                     like(xmlSchemaWhitespaceValueType)

     d xmlSchemaCopyValue...
     d                 pr                  extproc('xmlSchemaCopyValue')
     d                                     like(xmlSchemaValPtr)
     d  val                                value like(xmlSchemaValPtr)

     d xmlSchemaGetValType...
     d                 pr                  extproc('xmlSchemaGetValType')
     d                                     like(xmlSchemaValType)
     d  val                                value like(xmlSchemaValPtr)

      /endif                                                                    LIBXML_SCHEMAS_ENBLD
      /endif                                                                    XML_SCHEMA_TYPES_H__
