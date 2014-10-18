      * Summary: XML Path Language implementation
      * Description: API for the XML Path Language implementation
      *
      * XML Path Language implementation
      * XPath is a language for addressing parts of an XML document,
      * designed to be used by both XSLT and XPointer
      *     http://www.w3.org/TR/xpath
      *
      * Implements
      * W3C Recommendation 16 November 1999
      *     http://www.w3.org/TR/1999/REC-xpath-19991116
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_XPATH_H__)
      /define XML_XPATH_H__

      /include "libxmlrpg/xmlversion"

      /if defined(LIBXML_XPATH_ENABLED)

      /include "libxmlrpg/xmlerror"
      /include "libxmlrpg/tree"
      /include "libxmlrpg/hash"
      /endif                                                                    LIBXML_XPATH_ENABLED

      /if defined(LIBXML_XPATH_ENABLED)

     d xmlXPathContextPtr...
     d                 s               *   based(######typedef######)

     d xmlXPathParserContextPtr...
     d                 s               *   based(######typedef######)

      * The set of XPath error codes.

     d xmlXPathError   s             10i 0 based(######typedef######)           enum
     d  XPATH_EXPRESSION_OK...
     d                 c                   0
     d  XPATH_NUMBER_ERROR...
     d                 c                   1
     d  XPATH_UNFINISHED_LITERAL_ERROR...
     d                 c                   2
     d  XPATH_START_LITERAL_ERROR...
     d                 c                   3
     d  XPATH_VARIABLE_REF_ERROR...
     d                 c                   4
     d  XPATH_UNDEF_VARIABLE_ERROR...
     d                 c                   5
     d  XPATH_INVALID_PREDICATE_ERROR...
     d                 c                   6
     d  XPATH_EXPR_ERROR...
     d                 c                   7
     d  XPATH_UNCLOSED_ERROR...
     d                 c                   8
     d  XPATH_UNKNOWN_FUNC_ERROR...
     d                 c                   9
     d  XPATH_INVALID_OPERAND...
     d                 c                   10
     d  XPATH_INVALID_TYPE...
     d                 c                   11
     d  XPATH_INVALID_ARITY...
     d                 c                   12
     d  XPATH_INVALID_CTXT_SIZE...
     d                 c                   13
     d  XPATH_INVALID_CTXT_POSITION...
     d                 c                   14
     d  XPATH_MEMORY_ERROR...
     d                 c                   15
     d  XPTR_SYNTAX_ERROR...
     d                 c                   16
     d  XPTR_RESOURCE_ERROR...
     d                 c                   17
     d  XPTR_SUB_RESOURCE_ERROR...
     d                 c                   18
     d  XPATH_UNDEF_PREFIX_ERROR...
     d                 c                   19
     d  XPATH_ENCODING_ERROR...
     d                 c                   20
     d  XPATH_INVALID_CHAR_ERROR...
     d                 c                   21
     d  XPATH_INVALID_CTXT...
     d                 c                   22
     d  XPATH_STACK_ERROR...
     d                 c                   23
     d  XPATH_FORBID_VARIABLE_ERROR...
     d                 c                   24

      * A node-set (an unordered collection of nodes without duplicates).

     d xmlNodeSetPtr   s               *   based(######typedef######)

     d xmlNodeSet      ds                  based(xmlNodeSetPtr)
     d                                     align qualified
     d  nodeNr                       10i 0                                      Set node count
     d  nodeMax                      10i 0                                      Max # nodes in set
     d  nodeTab                        *                                        xmlNodePtr *

      * An expression is evaluated to yield an object, which
      * has one of the following four basic types:
      *   - node-set
      *   - boolean
      *   - number
      *   - string
      *
      * @@ XPointer will add more types !

     d xmlXPathObjectType...
     d                 s             10i 0 based(######typedef######)           enum
     d  XPATH_UNDEFINED...
     d                 c                   0
     d  XPATH_NODESET  c                   1
     d  XPATH_BOOLEAN  c                   2
     d  XPATH_NUMBER   c                   3
     d  XPATH_STRING   c                   4
     d  XPATH_POINT    c                   5
     d  XPATH_RANGE    c                   6
     d  XPATH_LOCATIONSET...
     d                 c                   7
     d  XPATH_USERS    c                   8
     d  XPATH_XSLT_TREE...                                                      R/O XSLT value tree
     d                 c                   9

     d xmlXPathObjectPtr...
     d                 s               *   based(######typedef######)

     d xmlXPathObject  ds                  based(xmlXPathObjectPtr)
     d                                     align qualified
     d  type                               like(xmlXPathObjectType)
     d  nodesetval                         like(xmlNodeSetPtr)
     d  boolval                      10i 0
     d  floatval                      8f
     d  stringval                      *                                        xmlChar *
     d  user                           *                                        void *
     d  index                        10i 0
     d  user2                          *                                        void *
     d  index2                       10i 0

      * xmlXPathConvertFunc:
      * @obj:  an XPath object
      * @type:  the number of the target type
      *
      * A conversion function is associated to a type and used to cast
      * the new type to primitive values.
      *
      * Returns -1 in case of error, 0 otherwise

     d xmlXPathConvertFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * Extra type: a name and a conversion function.

     d xmlXPathTypePtr...
     d                 s               *   based(######typedef######)

     d xmlXPathType    ds                  based(xmlXPathTypePtr)
     d                                     align qualified
     d  name                           *                                        The type name
     d  func                               like(xmlXPathConvertFunc)            Conversion function

      * Extra variable: a name and a value.

     d xmlXPathVariablePtr...
     d                 s               *   based(######typedef######)

     d xmlXPathVariable...
     d                 ds                  based(xmlXPathVariablePtr)
     d                                     align qualified
     d  name                           *                                        The variable name
     d  value                              like(xmlXPathObjectPtr)              The value

      * xmlXPathEvalFunc:
      * @ctxt: an XPath parser context
      * @nargs: the number of arguments passed to the function
      *
      * An XPath evaluation function, the parameters are on the XPath
      *   context stack.

     d xmlXPathEvalFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * Extra function: a name and an evaluation function.

     d xmlXPathFuncPtr...
     d                 s               *   based(######typedef######)

     d xmlXPathFunct   ds                  based(xmlXPathFuncPtr)
     d                                     align qualified
     d  name                           *                                        The function name
     d  func                               like(xmlXPathEvalFunc)               Evaluation function

      * xmlXPathAxisFunc:
      * @ctxt:  the XPath interpreter context
      * @cur:  the previous node being explored on that axis
      *
      * An axis traversal function. To traverse an axis, the engine calls
      * the first time with cur == NULL and repeat until the function returns
      * NULL indicating the end of the axis traversal.
      *
      * Returns the next node in that axis or NULL if at the end of the axis.

     d xmlXPathAxisFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * Extra axis: a name and an axis function.

     d xmlXPathAxisPtr...
     d                 s               *   based(######typedef######)

     d xmlXPathAxis    ds                  based(xmlXPathAxisPtr)
     d                                     align qualified
     d  name                           *                                        The axis name
     d  func                               like(xmlXPathAxisFunc)               The search function

      * xmlXPathFunction:
      * @ctxt:  the XPath interprestation context
      * @nargs:  the number of arguments
      *
      * An XPath function.
      * The arguments (if any) are popped out from the context stack
      * and the result is pushed on the stack.

     d xmlXPathFunction...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * Function and Variable Lookup.

      * xmlXPathVariableLookupFunc:
      * @ctxt:  an XPath context
      * @name:  name of the variable
      * @ns_uri:  the namespace name hosting this variable
      *
      * Prototype for callbacks used to plug variable lookup in the XPath
      * engine.
      *
      * Returns the XPath object value or NULL if not found.

     d xmlXPathVariableLookupFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * xmlXPathFuncLookupFunc:
      * @ctxt:  an XPath context
      * @name:  name of the function
      * @ns_uri:  the namespace name hosting this function
      *
      * Prototype for callbacks used to plug function lookup in the XPath
      * engine.
      *
      * Returns the XPath function or NULL if not found.

     d xmlXPathFuncLookupFunc...
     d                 s               *   based(######typedef######)
     d                                     procptr

      * xmlXPathFlags:
      * Flags for XPath engine compilation and runtime

      * XML_XPATH_CHECKNS:
      *
      * check namespaces at compilation

     d  XML_XPATH_CHECKNS...
     d                 c                   X'0001'

      * XML_XPATH_NOVAR:
      *
      * forbid variables in expression

     d  XML_XPATH_NOVAR...
     d                 c                   X'0002'

      * xmlXPathContext:
      *
      * Expression evaluation occurs with respect to a context.
      * he context consists of:
      *    - a node (the context node)
      *    - a node list (the context node list)
      *    - a set of variable bindings
      *    - a function library
      *    - the set of namespace declarations in scope for the expression
      * Following the switch to hash tables, this need to be trimmed up at
      * the next binary incompatible release.
      * The node may be modified when the context is passed to libxml2
      * for an XPath evaluation so you may need to initialize it again
      * before the next call.

     d xmlXPathContext...
     d                 ds                  based(xmlXPathContextPtr)
     d                                     align qualified
     d  doc                                like(xmlDocPtr)                      Current document
     d  node                               like(xmlNodePtr)                     Current node
      *
     d  nb_variables_unused...                                                  Unused (hash table)
     d                               10i 0
     d  max_variables_unused...                                                  Unused (hash table)
     d                               10i 0
     d  varHash                            like(xmlHashTablePtr)                 Defined variables
      *
     d  nb_types                     10i 0                                       # of defined types
     d  max_types                    10i 0                                       Max number of types
     d  types                              like(xmlXPathTypePtr)                 Defined types array
      *
     d  nb_funcs_unused...                                                      Unused (hash table)
     d                               10i 0
     d  max_funcs_unused...                                                      Unused (hash table)
     d                               10i 0
     d  funcHash                           like(xmlHashTablePtr)                 Defined functions
      *
     d  nb_axis                      10i 0                                       # of defined axis
     d  max_axis                     10i 0                                       Max number of axis
     d  axis                               like(xmlXPathAxisPtr)                 Defined axis array
      *
      * the namespace nodes of the context node
      *
     d  namespaces                     *                                         xmlNsPtr *
     d  nsNr                         10i 0                                       # scope namespaces
     d  user                           *   procptr                               Function to free
      *
      * extra variables
      *
     d  contextSize                  10i 0                                       The context size
     d  proximityPosition...
     d                               10i 0
      *
      * extra stuff for XPointer
      *
     d  xptr                         10i 0                                       XPointer context ?
     d  here                               like(xmlNodePtr)                      For here()
     d  origin                             like(xmlNodePtr)                      For origin()
      *
      * the set of namespace declarations in scope for the expression
      *
     d  nsHash                             like(xmlHashTablePtr)                 Namespace hashtable
     d  varLookupFunc                      like(xmlXPathVariableLookupFunc)      Var lookup function
     d  varLookupData                  *                                         void *
      *
      * Possibility to link in an extra item
      *
     d  extra                          *                                         void *
      *
      * The function name and URI when calling a function
      *
     d  function                       *                                         const xmlChar *
     d  functionURI                    *                                         const xmlChar *
      *
      * function lookup function and data
      *
     d  funcLookupFunc...                                                        Func lookup func
     d                                     like(xmlXPathVariableLookupFunc)
     d  funcLookupData...                                                        void *
     d                                 *
      *
      * temporary namespace lists kept for walking the namespace axis
      *
     d  tmpNsList                      *                                        xmlNsPtr *
     d  tmpNsNr                      10i 0                                      # scope namespaces
      *
      * error reporting mechanism
      *
     d  userData                       *                                        void *
     d  error                              like(xmlStructuredErrorFunc)         Error callback
     d  lastError                          like(xmlError)                       The last error
     d  debugNode                          like(xmlNodePtr)                     XSLT source node
      *
      * dictionary
      *
     d  dict                               like(xmlDictPtr)                     Dictionary if any
      *
     d  flags                        10i 0                                      Compilation control
      *
      * Cache for reusal of XPath objects
      *
     d  cache                          *                                        void *

      * The structure of a compiled expression form is not public.

     d xmlXPathCompExprPtr...
     d                 s               *   based(######typedef######)

      * xmlXPathParserContext:
      *
      * An XPath parser context. It contains pure parsing informations,
      * an xmlXPathContext, and the stack of objects.

     d xmlXPathParserContext...
     d                 ds                  based(xmlXPathParserContextPtr)
     d                                     align qualified
     d  cur                            *                                        const xmlChar *
     d  base                           *                                        const xmlChar *
      *
     d  error                        10i 0                                      Error code
      *
     d  context                            like(xmlXPathContextPtr)             Evaluation context
     d  value                              like(xmlXPathObjectPtr)              The current value
     d  valueNr                      10i 0                                      Value stack depth
     d  valueMax                     10i 0                                      Max stack depth
     d  valueTab                       *                                        xmlXPathObjectPtr *
      *
     d  comp                               like(xmlXPathCompExprPtr)            Precompiled expr.
     d  xptr                         10i 0                                      XPointer expression?
     d  ancestor                           like(xmlNodePtr)                     To walk prec. axis
      *
     d  valueFrame                   10i 0                                      Limit stack pop

      **************************************************************************
      *                                                                        *
      *                             Public API                                 *
      *                                                                        *
      **************************************************************************

      * Objects and Nodesets handling

     d xmlXPathNAN     s              8f   import('xmlXPathNAN')
     d xmlXPathPINF    s              8f   import('xmlXPathPINF')
     d xmlXPathNINF    s              8f   import('xmlXPathNINF')

     d xmlXPathFreeObject...
     d                 pr                  extproc('xmlXPathFreeObject')
     d obj                                 value like(xmlXPathObjectPtr)

     d xmlXPathNodeSetCreate...
     d                 pr                  extproc('xmlXPathNodeSetCreate')
     d                                     like(xmlNodeSetPtr)
     d val                                 value like(xmlNodePtr)

     d xmlXPathFreeNodeSetList...
     d                 pr                  extproc('xmlXPathFreeNodeSetList')
     d obj                                 value like(xmlXPathObjectPtr)

     d xmlXPathFreeNodeSet...
     d                 pr                  extproc('xmlXPathFreeNodeSet')
     d obj                                 value like(xmlNodeSetPtr)

     d xmlXPathObjectCopy...
     d                 pr                  extproc('xmlXPathObjectCopy')
     d                                     like(xmlXPathObjectPtr)
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathCmpNodes...
     d                 pr            10i 0 extproc('xmlXPathCmpNodes')
     d node1                               value like(xmlNodePtr)
     d node2                               value like(xmlNodePtr)

      * Conversion functions to basic types.

     d xmlXPathCastNumberToBoolean...
     d                 pr            10i 0 extproc(
     d                                      'xmlXPathCastNumberToBoolean')
     d val                            8f   value

     d xmlXPathCastStringToBoolean...
     d                 pr            10i 0 extproc(
     d                                      'xmlXPathCastStringToBoolean')
     d val                             *   value options(*string)               const xmlChar *

     d xmlXPathCastNodeSetToBoolean...
     d                 pr            10i 0 extproc(
     d                                     'xmlXPathCastNodeSetToBoolean')
     d ns                                  value like(xmlNodeSetPtr)

     d xmlXPathCastToBoolean...
     d                 pr            10i 0 extproc('xmlXPathCastToBoolean')
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathCastBooleanToNumber...
     d                 pr                  extproc(
     d                                      'xmlXPathCastBooleanToNumber')
     d                                8f
     d val                           10i 0 value

     d xmlXPathCastStringToNumber...
     d                 pr             8f   extproc('xmlXPathCastStringToNumber')
     d val                             *   value options(*string)               const xmlChar *

     d xmlXPathCastNodeToNumber...
     d                 pr             8f   extproc('xmlXPathCastNodeToNumber')
     d node                                value like(xmlNodePtr)

     d xmlXPathCastNodeSetToNumber...
     d                 pr             8f   extproc(
     d                                      'xmlXPathCastNodeSetToNumber')
     d ns                                  value like(xmlNodeSetPtr)

     d xmlXPathCastToNumber...
     d                 pr             8f   extproc('xmlXPathCastToNumber')
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathCastBooleanToString...
     d                 pr              *   extproc(                             xmlChar *
     d                                      'xmlXPathCastBooleanToString')
     d val                           10i 0 value

     d xmlXPathCastNumberToString...
     d                 pr              *   extproc('xmlXPathCastNumberToString')xmlChar *
     d val                            8f   value

     d xmlXPathCastNodeToString...
     d                 pr              *   extproc('xmlXPathCastNodeToString')  xmlChar *
     d node                                value like(xmlNodePtr)

     d xmlXPathCastNodeSetToString...
     d                 pr              *   extproc('xmlXPathCastNodeSetToString'xmlChar *
     d                                     )
     d ns                                  value like(xmlNodeSetPtr)

     d xmlXPathCastToString...
     d                 pr              *   extproc('xmlXPathCastToString')      xmlChar *
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathConvertBoolean...
     d                 pr                  extproc('xmlXPathConvertBoolean')
     d                                     like(xmlXPathObjectPtr)
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathConvertNumber...
     d                 pr                  extproc('xmlXPathConvertNumber')
     d                                     like(xmlXPathObjectPtr)
     d val                                 value like(xmlXPathObjectPtr)

     d xmlXPathConvertString...
     d                 pr                  extproc('xmlXPathConvertString')
     d                                     like(xmlXPathObjectPtr)
     d val                                 value like(xmlXPathObjectPtr)

      * Context handling.

     d xmlXPathNewContext...
     d                 pr                  extproc('xmlXPathNewContext')
     d                                     like(xmlXPathContextPtr)
     d doc                                 value like(xmlDocPtr)

     d xmlXPathFreeContext...
     d                 pr                  extproc('xmlXPathFreeContext')
     d ctxt                                value like(xmlXPathContextPtr)

     d xmlXPathContextSetCache...
     d                 pr            10i 0 extproc('xmlXPathContextSetCache')
     d ctxt                                value like(xmlXPathContextPtr)
     d active                        10i 0 value
     d value                         10i 0 value
     d options                       10i 0 value

      * Evaluation functions.

     d xmlXPathOrderDocElems...
     d                 pr            20i 0 extproc('xmlXPathOrderDocElems')
     d doc                                 value like(xmlDocPtr)

     d xmlXPathSetContextNode...
     d                 pr            10i 0 extproc('xmlXPathSetContextNode')
     d node                                value like(xmlNodePtr)
     d ctx                                 value like(xmlXPathContextPtr)

     d xmlXPathNodeEval...
     d                 pr                  extproc('xmlXPathNodeEval')
     d                                     like(xmlXPathObjectPtr)
     d node                                value like(xmlNodePtr)
     d str                             *   value options(*string)               const xmlChar *
     d ctx                                 value like(xmlXPathContextPtr)

     d xmlXPathEval    pr                  extproc('xmlXPathEval')
     d                                     like(xmlXPathObjectPtr)
     d str                             *   value options(*string)               const xmlChar *
     d ctx                                 value like(xmlXPathContextPtr)

     d xmlXPathEvalExpression...
     d                 pr                  extproc('xmlXPathEvalExpression')
     d                                     like(xmlXPathObjectPtr)
     d str                             *   value options(*string)               const xmlChar *
     d ctxt                                value like(xmlXPathContextPtr)

     d xmlXPathEvalPredicate...
     d                 pr            10i 0 extproc('xmlXPathEvalPredicate')
     d ctxt                                value like(xmlXPathContextPtr)
     d res                                 value like(xmlXPathObjectPtr)

      * Separate compilation/evaluation entry points.

     d xmlXPathCompile...
     d                 pr                  extproc('xmlXPathCompile')
     d                                     like(xmlXPathCompExprPtr)
     d str                             *   value options(*string)               const xmlChar *

     d xmlXPathCtxtCompile...
     d                 pr                  extproc('xmlXPathCtxtCompile')
     d                                     like(xmlXPathCompExprPtr)
     d ctxt                                value like(xmlXPathContextPtr)
     d str                             *   value options(*string)               const xmlChar *

     d xmlXPathCompiledEval...
     d                 pr                  extproc('xmlXPathCompiledEval')
     d                                     like(xmlXPathObjectPtr)
     d comp                                value like(xmlXPathCompExprPtr)
     d ctx                                 value like(xmlXPathContextPtr)

     d xmlXPathCompiledEvalToBoolean...
     d                 pr            10i 0 extproc(
     d                                     'xmlXPathCompiledEvalToBoolean')
     d comp                                value like(xmlXPathCompExprPtr)
     d ctxt                                value like(xmlXPathContextPtr)

     d xmlXPathFreeCompExpr...
     d                 pr                  extproc('xmlXPathFreeCompExpr')
     d comp                                value like(xmlXPathCompExprPtr)
      /endif                                                                    LIBXML_XPATH_ENABLED

      /undefine XML_TESTVAL
      /if defined(LIBXML_XPATH_ENABLED)
      /define XML_TESTVAL
      /elseif defined(LIBXML_SCHEMAS_ENABLED)
      /define XML_TESTVAL
      /endif
      /if defined(XML_TESTVAL)
     d xmlXPathInit    pr                  extproc('xmlXPathInit')

     d xmlXPathIsNaN   pr            10i 0 extproc('xmlXPathIsNaN')
     d val                            8f   value

     d xmlXPathIsInf   pr            10i 0 extproc('xmlXPathIsInf')
     d val                            8f   value

      /undefine XML_TESTVAL
      /endif

      * C macros implemented as procedures for ILE/RPG support.

      /if defined(LIBXML_XPATH_ENABLED)
     d xmlXPathNodeSetGetLength...
     d                 pr            10i 0 extproc('__xmlXPathNodeSetGetLength')
     d  ns                                 value like(xmlNodeSetPtr)

     d xmlXPathNodeSetItem...
     d                 pr                  extproc('__xmlXPathNodeSetItem')
     d                                     like(xmlNodePtr)
     d  ns                                 value like(xmlNodeSetPtr)
     d  index                        10i 0 value

     d xmlXPathNodeSetIsEmpty...
     d                 pr            10i 0 extproc('__xmlXPathNodeSetIsEmpty')
     d  ns                                 value like(xmlNodeSetPtr)
      /endif                                                                    LIBXML_XPATH_ENABLED
      /endif                                                                    XML_XPATH_H__
