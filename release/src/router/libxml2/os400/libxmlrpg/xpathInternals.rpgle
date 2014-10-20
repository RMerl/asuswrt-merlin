      * Summary: internal interfaces for XML Path Language implementation
      * Description: internal interfaces for XML Path Language implementation
      *              used to build new modules on top of XPath like XPointer and
      *              XSLT
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_XPATH_INTERNALS_H__)
      /define XML_XPATH_INTERNALS_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/xpath"

      /if defined(LIBXML_XPATH_ENABLED)

      ************************************************************************
      *                                                                      *
      *                            Helpers                                   *
      *                                                                      *
      ************************************************************************

      * Many of these macros may later turn into functions. They
      * shouldn't be used in #ifdef's preprocessor instructions.

     d xmlXPathPopBoolean...
     d                 pr            10i 0 extproc('xmlXPathPopBoolean')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathPopNumber...
     d                 pr             8f   extproc('xmlXPathPopNumber')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathPopString...
     d                 pr              *   extproc('xmlXPathPopString')         xmlChar *
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathPopNodeSet...
     d                 pr                  extproc('xmlXPathPopNodeSet')
     d                                     like(xmlNodeSetPtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathPopExternal...
     d                 pr              *   extproc('xmlXPathPopExternal')       void *
     d  ctxt                               value like(xmlXPathParserContextPtr)

      * Variable Lookup forwarding.

     d xmlXPathRegisterVariableLookup...
     d                 pr                  extproc(
     d                                     'xmlXPathRegisterVariableLookup')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  f                                  value
     d                                     like(xmlXPathVariableLookupFunc)
     d  data                           *   value                                void *

      * Function Lookup forwarding.

     d xmlXPathRegisterFuncLookup...
     d                 pr                  extproc('xmlXPathRegisterFuncLookup')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  f                                  value like(xmlXPathFuncLookupFunc)
     d  funcCtxt                       *   value                                void *

      * Error reporting.
      * Note this procedure is renamed in RPG to avoid character case clash with
      *   data type xmlXPathError.

     d xmlXPathReportError...
     d                 pr                  extproc('xmlXPatherror')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  file                           *   value options(*string)               const char *
     d  line                         10i 0 value
     d  no                           10i 0 value

     d xmlXPathErr     pr                  extproc('xmlXPathErr')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  error                        10i 0 value

      /if defined(LIBXML_DEBUG_ENABLED)
     d xmlXPathDebugDumpObject...
     d                 pr                  extproc('xmlXPathDebugDumpObject')
     d  output                         *   value                                FILE *
     d  cur                                value like(xmlXPathObjectPtr)
     d  depth                        10i 0 value

     d xmlXPathDebugDumpCompExpr...
     d                 pr                  extproc('xmlXPathDebugDumpCompExpr')
     d  output                         *   value                                FILE *
     d  comp                               value like(xmlXPathCompExprPtr)
     d  depth                        10i 0 value
      /endif

      * NodeSet handling.

     d xmlXPathNodeSetContains...
     d                 pr            10i 0 extproc('xmlXPathNodeSetContains')
     d  cur                                value like(xmlNodeSetPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathDifference...
     d                 pr                  extproc('xmlXPathDifference')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathIntersection...
     d                 pr                  extproc('xmlXPathIntersection')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathDistinctSorted...
     d                 pr                  extproc('xmlXPathDistinctSorted')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)

     d xmlXPathDistinct...
     d                 pr                  extproc('xmlXPathDistinct')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)

     d xmlXPathHasSameNodes...
     d                 pr            10i 0 extproc('xmlXPathHasSameNodes')
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathNodeLeadingSorted...
     d                 pr                  extproc('xmlXPathNodeLeadingSorted')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)
     d  node                               value like(xmlNodePtr)

     d xmlXPathLeadingSorted...
     d                 pr                  extproc('xmlXPathLeadingSorted')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathNodeLeading...
     d                 pr                  extproc('xmlXPathNodeLeading')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)
     d  node                               value like(xmlNodePtr)

     d xmlXPathLeading...
     d                 pr                  extproc('xmlXPathLeading')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathNodeTrailingSorted...
     d                 pr                  extproc('xmlXPathNodeTrailingSorted')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)
     d  node                               value like(xmlNodePtr)

     d xmlXPathTrailingSorted...
     d                 pr                  extproc('xmlXPathTrailingSorted')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

     d xmlXPathNodeTrailing...
     d                 pr                  extproc('xmlXPathNodeTrailing')
     d                                     like(xmlNodeSetPtr)
     d  nodes                              value like(xmlNodeSetPtr)
     d  node                               value like(xmlNodePtr)

     d xmlXPathTrailing...
     d                 pr                  extproc('xmlXPathTrailing')
     d                                     like(xmlNodeSetPtr)
     d  nodes1                             value like(xmlNodeSetPtr)
     d  nodes2                             value like(xmlNodeSetPtr)

      * Extending a context.

     d xmlXPathRegisterNs...
     d                 pr            10i 0 extproc('xmlXPathRegisterNs')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  prefix                         *   value options(*string)               const xmlChar *
     d  ns_uri                         *   value options(*string)               const xmlChar *

     d xmlXPathNsLookup...
     d                 pr              *   extproc('xmlXPathNsLookup')          const xmlChar *
     d  ctxt                               value like(xmlXPathContextPtr)
     d  prefix                         *   value options(*string)               const xmlChar *

     d xmlXPathRegisteredNsCleanup...
     d                 pr                  extproc(
     d                                     'xmlXPathRegisteredNsCleanup')
     d  ctxt                               value like(xmlXPathContextPtr)

     d xmlXPathRegisterFunc...
     d                 pr            10i 0 extproc('xmlXPathRegisterFunc')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  f                                  value like(xmlXPathFunction)

     d xmlXPathRegisterFuncNS...
     d                 pr            10i 0 extproc('xmlXPathRegisterFuncNS')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns_uri                         *   value options(*string)               const xmlChar *
     d  f                                  value like(xmlXPathFunction)

     d xmlXPathRegisterVariable...
     d                 pr            10i 0 extproc('xmlXPathRegisterVariable')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  value                              value like(xmlXPathObjectPtr)

     d xmlXPathRegisterVariableNS...
     d                 pr            10i 0 extproc('xmlXPathRegisterVariableNS')
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns_uri                         *   value options(*string)               const xmlChar *
     d  value                              value like(xmlXPathObjectPtr)

     d xmlXPathFunctionLookup...
     d                 pr                  extproc('xmlXPathFunctionLookup')
     d                                     like(xmlXPathFunction)
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlXPathFunctionLookupNS...
     d                 pr                  extproc('xmlXPathFunctionLookupNS')
     d                                     like(xmlXPathFunction)
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns_uri                         *   value options(*string)               const xmlChar *

     d xmlXPathRegisteredFuncsCleanup...
     d                 pr                  extproc(
     d                                     'xmlXPathRegisteredFuncsCleanup')
     d  ctxt                               value like(xmlXPathContextPtr)

     d xmlXPathVariableLookup...
     d                 pr                  extproc('xmlXPathVariableLookup')
     d                                     like(xmlXPathObjectPtr)
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *

     d xmlXPathVariableLookupNS...
     d                 pr                  extproc('xmlXPathVariableLookupNS')
     d                                     like(xmlXPathObjectPtr)
     d  ctxt                               value like(xmlXPathContextPtr)
     d  name                           *   value options(*string)               const xmlChar *
     d  ns_uri                         *   value options(*string)               const xmlChar *

     d xmlXPathRegisteredVariablesCleanup...
     d                 pr                  extproc(
     d                                     'xmlXPathRegisteredVariablesCleanup')
     d  ctxt                               value like(xmlXPathContextPtr)

      * Utilities to extend XPath.

     d xmlXPathNewParserContext...
     d                 pr                  extproc('xmlXPathNewParserContext')
     d                                     like(xmlXPathParserContextPtr)
     d  str                            *   value options(*string)               const xmlChar *
     d  ctxt                               value like(xmlXPathContextPtr)

     d xmlXPathFreeParserContext...
     d                 pr                  extproc('xmlXPathFreeParserContext')
     d  ctxt                               value like(xmlXPathParserContextPtr)


      * TODO: remap to xmlXPathValuePop and Push.

     d valuePop        pr                  extproc('valuePop')
     d                                     like(xmlXPathObjectPtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d valuePush       pr            10i 0 extproc('valuePush')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  value                              value like(xmlXPathObjectPtr)

     d xmlXPathNewString...
     d                 pr                  extproc('xmlXPathNewString')
     d                                     like(xmlXPathObjectPtr)
     d  val                            *   value options(*string)               const xmlChar *

     d xmlXPathNewCString...
     d                 pr                  extproc('xmlXPathNewCString')
     d                                     like(xmlXPathObjectPtr)
     d  val                            *   value options(*string)               const char *

     d xmlXPathWrapString...
     d                 pr                  extproc('xmlXPathWrapString')
     d                                     like(xmlXPathObjectPtr)
     d  val                            *   value options(*string)               xmlChar *

     d xmlXPathWrapCString...
     d                 pr                  extproc('xmlXPathWrapCString')
     d                                     like(xmlXPathObjectPtr)
     d  val                            *   value options(*string)               char *

     d xmlXPathNewFloat...
     d                 pr                  extproc('xmlXPathNewFloat')
     d                                     like(xmlXPathObjectPtr)
     d  val                           8f   value

     d xmlXPathNewBoolean...
     d                 pr                  extproc('xmlXPathNewBoolean')
     d                                     like(xmlXPathObjectPtr)
     d  val                          10i 0 value

     d xmlXPathNewNodeSet...
     d                 pr                  extproc('xmlXPathNewNodeSet')
     d                                     like(xmlXPathObjectPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathNewValueTree...
     d                 pr                  extproc('xmlXPathNewValueTree')
     d                                     like(xmlXPathObjectPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathNodeSetAdd...
     d                 pr            10i 0 extproc('xmlXPathNodeSetAdd')
     d  cur                                value like(xmlNodeSetPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathNodeSetAddUnique...
     d                 pr            10i 0 extproc('xmlXPathNodeSetAddUnique')
     d  cur                                value like(xmlNodeSetPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathNodeSetAddNs...
     d                 pr            10i 0 extproc('xmlXPathNodeSetAddNs')
     d  cur                                value like(xmlNodeSetPtr)
     d  node                               value like(xmlNodePtr)
     d  ns                                 value like(xmlNsPtr)

     d xmlXPathNodeSetSort...
     d                 pr                  extproc('xmlXPathNodeSetSort')
     d  set                                value like(xmlNodeSetPtr)

     d xmlXPathRoot    pr                  extproc('xmlXPathRoot')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathEvalExpr...
     d                 pr                  extproc('xmlXPathEvalExpr')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathParseName...
     d                 pr              *   extproc('xmlXPathParseName')         xmlChar *
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathParseNCName...
     d                 pr              *   extproc('xmlXPathParseNCName')       xmlChar *
     d  ctxt                               value like(xmlXPathParserContextPtr)

      * Existing functions.

     d xmlXPathStringEvalNumber...
     d                 pr             8f   extproc('xmlXPathStringEvalNumber')
     d  str                            *   value options(*string)               const xmlChar *

     d xmlXPathEvaluatePredicateResult...
     d                 pr            10i 0 extproc(
     d                                     'xmlXPathEvaluatePredicateResult')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  res                                value like(xmlXPathObjectPtr)

     d xmlXPathRegisterAllFunctions...
     d                 pr                  extproc(
     d                                     'xmlXPathRegisterAllFunctions')
     d  ctxt                               value like(xmlXPathContextPtr)

     d xmlXPathNodeSetMerge...
     d                 pr                  extproc('xmlXPathNodeSetMerge')
     d                                     like(xmlNodeSetPtr)
     d  val1                               value like(xmlNodeSetPtr)
     d  val2                               value like(xmlNodeSetPtr)

     d xmlXPathNodeSetDel...
     d                 pr                  extproc('xmlXPathNodeSetDel')
     d  cur                                value like(xmlNodeSetPtr)
     d  val                                value like(xmlNodePtr)

     d xmlXPathNodeSetRemove...
     d                 pr                  extproc('xmlXPathNodeSetRemove')
     d  cur                                value like(xmlNodeSetPtr)
     d  val                          10i 0 value

     d xmlXPathNewNodeSetList...
     d                 pr                  extproc('xmlXPathNewNodeSetList')
     d                                     like(xmlXPathObjectPtr)
     d  val                                value like(xmlNodeSetPtr)

     d xmlXPathWrapNodeSet...
     d                 pr                  extproc('xmlXPathWrapNodeSet')
     d                                     like(xmlXPathObjectPtr)
     d  val                                value like(xmlNodeSetPtr)

     d xmlXPathWrapExternal...
     d                 pr                  extproc('xmlXPathWrapExternal')
     d                                     like(xmlXPathObjectPtr)
     d  val                            *   value                                void *

     d xmlXPathEqualValues...
     d                 pr            10i 0 extproc('xmlXPathEqualValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathNotEqualValues...
     d                 pr            10i 0 extproc('xmlXPathNotEqualValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathCompareValues...
     d                 pr            10i 0 extproc('xmlXPathCompareValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  inf                          10i 0 value
     d  strict                       10i 0 value

     d xmlXPathValueFlipSign...
     d                 pr                  extproc('xmlXPathValueFlipSign')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathAddValues...
     d                 pr                  extproc('xmlXPathAddValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathSubValues...
     d                 pr                  extproc('xmlXPathSubValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathMultValues...
     d                 pr                  extproc('xmlXPathMultValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathDivValues...
     d                 pr                  extproc('xmlXPathDivValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathModValues...
     d                 pr                  extproc('xmlXPathModValues')
     d  ctxt                               value like(xmlXPathParserContextPtr)

     d xmlXPathIsNodeType...
     d                 pr            10i 0 extproc('xmlXPathIsNodeType')
     d  name                           *   value options(*string)               const xmlChar *

      * Some of the axis navigation routines.

     d xmlXPathNextSelf...
     d                 pr                  extproc('xmlXPathNextSelf')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextChild...
     d                 pr                  extproc('xmlXPathNextChild')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextDescendant...
     d                 pr                  extproc('xmlXPathNextDescendant')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextDescendantOrSelf...
     d                 pr                  extproc(
     d                                     'xmlXPathNextDescendantOrSelf')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextParent...
     d                 pr                  extproc('xmlXPathNextParent')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextAncestorOrSelf...
     d                 pr                  extproc('xmlXPathNextAncestorOrSelf')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextFollowingSibling...
     d                 pr                  extproc(
     d                                     'xmlXPathNextFollowingSibling')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextFollowing...
     d                 pr                  extproc('xmlXPathNextFollowing')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextNamespace...
     d                 pr                  extproc('xmlXPathNextNamespace')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextAttribute...
     d                 pr                  extproc('xmlXPathNextAttribute')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextPreceding...
     d                 pr                  extproc('xmlXPathNextPreceding')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextAncestor...
     d                 pr                  extproc('xmlXPathNextAncestor')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

     d xmlXPathNextPrecedingSibling...
     d                 pr                  extproc(
     d                                     'xmlXPathNextPrecedingSibling')
     d                                     like(xmlNodePtr)
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  cur                                value like(xmlNodePtr)

      * The official core of XPath functions.

     d xmlXPathLastFunction...
     d                 pr                  extproc('xmlXPathLastFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathPositionFunction...
     d                 pr                  extproc('xmlXPathPositionFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathCountFunction...
     d                 pr                  extproc('xmlXPathCountFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathIdFunction...
     d                 pr                  extproc('xmlXPathIdFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathLocalNameFunction...
     d                 pr                  extproc('xmlXPathLocalNameFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathNamespaceURIFunction...
     d                 pr                  extproc(
     d                                     'xmlXPathNamespaceURIFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathStringFunction...
     d                 pr                  extproc('xmlXPathStringFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathStringLengthFunction...
     d                 pr                  extproc(
     d                                     'xmlXPathStringLengthFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathConcatFunction...
     d                 pr                  extproc('xmlXPathConcatFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathContainsFunction...
     d                 pr                  extproc('xmlXPathContainsFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathStartsWithFunction...
     d                 pr                  extproc('xmlXPathStartsWithFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathSubstringFunction...
     d                 pr                  extproc('xmlXPathSubstringFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathSubstringBeforeFunction...
     d                 pr                  extproc(
     d                                     'xmlXPathSubstringBeforeFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathSubstringAfterFunction...
     d                 pr                  extproc(
     d                                     'xmlXPathSubstringAfterFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value


     d xmlXPathNormalizeFunction...
     d                 pr                  extproc('xmlXPathNormalizeFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathTranslateFunction...
     d                 pr                  extproc('xmlXPathTranslateFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathNotFunction...
     d                 pr                  extproc('xmlXPathNotFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathTrueFunction...
     d                 pr                  extproc('xmlXPathTrueFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathFalseFunction...
     d                 pr                  extproc('xmlXPathFalseFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathLangFunction...
     d                 pr                  extproc('xmlXPathLangFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathNumberFunction...
     d                 pr                  extproc('xmlXPathNumberFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathSumFunction...
     d                 pr                  extproc('xmlXPathSumFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathFloorFunction...
     d                 pr                  extproc('xmlXPathFloorFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathCeilingFunction...
     d                 pr                  extproc('xmlXPathCeilingFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathRoundFunction...
     d                 pr                  extproc('xmlXPathRoundFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

     d xmlXPathBooleanFunction...
     d                 pr                  extproc('xmlXPathBooleanFunction')
     d  ctxt                               value like(xmlXPathParserContextPtr)
     d  nargs                        10i 0 value

      * Really internal functions

     d xmlXPathNodeSetFreeNs...
     d                 pr                  extproc('xmlXPathNodeSetFreeNs')
     d  ns                                 value like(xmlNsPtr)

      /endif                                                                    LIBXML_XPATH_ENABLED
      /endif                                                                    XPATH_INTERNALS_H__
