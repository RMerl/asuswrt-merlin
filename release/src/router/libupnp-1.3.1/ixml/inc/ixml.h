///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _IXML_H_
#define _IXML_H_

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

typedef int BOOL;

#define DOMString   char *


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

/**@name DOM Interfaces 
 * The Document Object Model consists of a set of objects and interfaces
 * for accessing and manipulating documents.  IXML does not implement all
 * the interfaces documented in the DOM2-Core recommendation but defines
 * a subset of the most useful interfaces.  A description of the supported
 * interfaces and methods is presented in this section.
 *
 * For a complete discussion on the object model, the object hierarchy,
 * etc., refer to section 1.1 of the DOM2-Core recommendation.
 */

//@{

/*================================================================
*
*   DOM node type 
*
*
*=================================================================*/
typedef enum
{
    eINVALID_NODE                   = 0,
    eELEMENT_NODE                   = 1,
    eATTRIBUTE_NODE                 = 2,
    eTEXT_NODE                      = 3,
    eCDATA_SECTION_NODE             = 4,
    eENTITY_REFERENCE_NODE          = 5,
    eENTITY_NODE                    = 6,                
    ePROCESSING_INSTRUCTION_NODE    = 7,
    eCOMMENT_NODE                   = 8,
    eDOCUMENT_NODE                  = 9,
    eDOCUMENT_TYPE_NODE             = 10,
    eDOCUMENT_FRAGMENT_NODE         = 11,
    eNOTATION_NODE                  = 12,

}   IXML_NODE_TYPE;

/*================================================================
*
*   error code 
*
*
*=================================================================*/
typedef enum 
{   // see DOM spec
    IXML_INDEX_SIZE_ERR                 = 1,
    IXML_DOMSTRING_SIZE_ERR             = 2,
    IXML_HIERARCHY_REQUEST_ERR          = 3,
    IXML_WRONG_DOCUMENT_ERR             = 4,
    IXML_INVALID_CHARACTER_ERR          = 5,
    IXML_NO_DATA_ALLOWED_ERR            = 6,
    IXML_NO_MODIFICATION_ALLOWED_ERR    = 7,
    IXML_NOT_FOUND_ERR                  = 8,
    IXML_NOT_SUPPORTED_ERR              = 9,
    IXML_INUSE_ATTRIBUTE_ERR            = 10,
    IXML_INVALID_STATE_ERR              = 11,
    IXML_SYNTAX_ERR                     = 12,
    IXML_INVALID_MODIFICATION_ERR       = 13,
    IXML_NAMESPACE_ERR                  = 14,
    IXML_INVALID_ACCESS_ERR             = 15,

    IXML_SUCCESS                        = 0,
    IXML_NO_SUCH_FILE                   = 101,
    IXML_INSUFFICIENT_MEMORY            = 102,
    IXML_FILE_DONE                      = 104,
    IXML_INVALID_PARAMETER              = 105,
    IXML_FAILED                         = 106,
    IXML_INVALID_ITEM_NUMBER            = 107,

} IXML_ERRORCODE;


#define DOCUMENTNODENAME    "#document"
#define TEXTNODENAME        "#text"
#define CDATANODENAME       "#cdata-section"

/*================================================================
*
*   DOM data structures
*
*
*=================================================================*/
typedef struct _IXML_Document *Docptr;

typedef struct _IXML_Node    *Nodeptr;
typedef struct _IXML_Node
{
    DOMString       nodeName;
    DOMString       nodeValue;
    IXML_NODE_TYPE  nodeType;
    DOMString       namespaceURI;
    DOMString       prefix;
    DOMString       localName;
    BOOL            readOnly;

    Nodeptr         parentNode;
    Nodeptr         firstChild;
    Nodeptr         prevSibling;
    Nodeptr         nextSibling;
    Nodeptr         firstAttr;
    Docptr          ownerDocument;

} IXML_Node;

typedef struct _IXML_Document
{
    IXML_Node    n;
} IXML_Document;

typedef struct _IXML_CDATASection
{
    IXML_Node    n;
} IXML_CDATASection;

typedef struct _IXML_Element
{
    IXML_Node   n;
    DOMString   tagName;

} IXML_Element;

typedef struct _IXML_ATTR
{
    IXML_Node   n;
    BOOL        specified;
    IXML_Element *ownerElement;
} IXML_Attr;

typedef struct _IXML_Text
{
    IXML_Node   n;
} IXML_Text;

typedef struct _IXML_NodeList
{
    IXML_Node    *nodeItem;
    struct  _IXML_NodeList *next;
} IXML_NodeList;


typedef struct _IXML_NamedNodeMap
{
    IXML_Node                 *nodeItem;
    struct _IXML_NamedNodeMap *next;
} IXML_NamedNodeMap;

#ifdef __cplusplus
extern "C" {
#endif

/*================================================================
*
*   NODE interfaces
*
*
*=================================================================*/

/**@name Interface {\it Node}
 * The {\bf Node} interface forms the primary datatype for all other DOM 
 * objects.  Every other interface is derived from this interface, inheriting 
 * its functionality.  For more information, refer to DOM2-Core page 34.
 */

//@{

  /** Returns the name of the {\bf Node}, depending on what type of 
   *  {\bf Node} it is, in a read-only string. Refer to the table in the 
   *  DOM2-Core for a description of the node names for various interfaces.
   *
   *  @return [const DOMString] A constant {\bf DOMString} of the node name.
   */

const DOMString
ixmlNode_getNodeName(IXML_Node *nodeptr 
		       /** Pointer to the node to retrieve the name. */
                    );

  /** Returns the value of the {\bf Node} as a string.  Note that this string 
   *  is not a copy and modifying it will modify the value of the {\bf Node}.
   *
   *  @return [DOMString] A {\bf DOMString} of the {\bf Node} value.
   */

DOMString               
ixmlNode_getNodeValue(IXML_Node *nodeptr  
		        /** Pointer to the {\bf Node} to retrieve the value. */
                     );

  /** Assigns a new value to a {\bf Node}.  The {\bf newNodeValue} string is
   *  duplicated and stored in the {\bf Node} so that the original does not
   *  have to persist past this call.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: The {\bf Node*} is not a valid 
   *            pointer.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int                     
ixmlNode_setNodeValue(IXML_Node *nodeptr, 
		        /** The {\bf Node} to which to assign a new value. */
                      char *newNodeValue  
		        /** The new value of the {\bf Node}. */
                  );

  /** Retrieves the type of a {\bf Node}.  The defined {\bf Node} constants 
   *  are:
   *  \begin{itemize}
   *    \item {\tt eATTRIBUTE_NODE} 
   *    \item {\tt eCDATA_SECTION_NODE}
   *    \item {\tt eCOMMENT_NODE}
   *    \item {\tt eDOCUMENT_FRAGMENT_NODE} 
   *    \item {\tt eDOCUMENT_NODE} 
   *    \item {\tt eDOCUMENT_TYPE_NODE} 
   *    \item {\tt eELEMENT_NODE} 
   *    \item {\tt eENTITY_NODE}
   *    \item {\tt eENTITY_REFERENCE_NODE}
   *    \item {\tt eNOTATION_NODE} 
   *    \item {\tt ePROCESSING_INSTRUCTION_NODE}
   *    \item {\tt eTEXT_NODE}
   *  \end{itemize}
   *
   *  @return [const unsigned short] An integer representing the type of the 
   *          {\bf Node}.
   */

unsigned short    
ixmlNode_getNodeType(IXML_Node *nodeptr  
		       /** The {\bf Node} from which to retrieve the type. */
                    );

  /** Retrieves the parent {\bf Node} for a {\bf Node}.
   *
   *  @return [Node*] A pointer to the parent {\bf Node} or {\tt NULL} if the 
   *          {\bf Node} has no parent.
   */

IXML_Node*                   
ixmlNode_getParentNode(IXML_Node *nodeptr  
		         /** The {\bf Node} from which to retrieve the 
			     parent. */ 
                      );

  /** Retrieves the list of children of a {\bf Node} in a {\bf NodeList} 
   *  structure.  If a {\bf Node} has no children, {\bf ixmlNode_getChildNodes} 
   *  returns a {\bf NodeList} structure that contains no {\bf Node}s.
   *
   *  @return [NodeList*] A {\bf NodeList} of the children of the {\bf Node}.
   */

IXML_NodeList*               
ixmlNode_getChildNodes(IXML_Node *nodeptr  
		         /** The {\bf Node} from which to retrieve the 
			     children. */
                   );

  /** Retrieves the first child {\bf Node} of a {\bf Node}.
   *
   *  @return [Node*] A pointer to the first child {\bf Node} or {\tt NULL} 
   *                  if the {\bf Node} does not have any children.
   */

IXML_Node*                   
ixmlNode_getFirstChild(IXML_Node *nodeptr  
		         /** The {\bf Node} from which to retrieve the first 
			     child.  */ 
);

  /** Retrieves the last child {\bf Node} of a {\bf Node}.
   *
   *  @return [Node*] A pointer to the last child {\bf Node} or {\tt NULL} if 
   *                  the {\bf Node} does not have any children.
   */

IXML_Node*                   
ixmlNode_getLastChild(IXML_Node *nodeptr  
		        /** The {\bf Node} from which to retrieve the last 
			    child. */
                  );

  /** Retrieves the sibling {\bf Node} immediately preceding this {\bf Node}.
   *
   *  @return [Node*] A pointer to the previous sibling {\bf Node} or 
   *                  {\tt NULL} if no such {\bf Node} exists.
   */

IXML_Node*                   
ixmlNode_getPreviousSibling(IXML_Node *nodeptr  
		              /** The {\bf Node} for which to retrieve the 
			          previous sibling.  */
                        );

  /** Retrieves the sibling {\bf Node} immediately following this {\bf Node}.
   *
   *  @return [Node*] A pointer to the next sibling {\bf Node} or {\tt NULL} 
   *                  if no such {\bf Node} exists.
   */

IXML_Node*                   
ixmlNode_getNextSibling(IXML_Node *nodeptr  
		          /** The {\bf Node} from which to retrieve the next 
			      sibling. */ 
                    );

  /** Retrieves the attributes of a {\bf Node}, if it is an {\bf Element} node,
   *  in a {\bf NamedNodeMap} structure.
   *
   *  @return [NamedNodeMap*] A {\bf NamedNodeMap} of the attributes or 
   *                          {\tt NULL}.
   */

IXML_NamedNodeMap*           
ixmlNode_getAttributes(IXML_Node *nodeptr  
		         /** The {\bf Node} from which to retrieve the 
			     attributes. */ 
                   );

  /** Retrieves the document object associated with this {\bf Node}.  This 
   *  owner document {\bf Node} allows other {\bf Node}s to be created in the 
   *  context of this document.  Note that {\bf Document} nodes do not have 
   *  an owner document.
   *
   *  @return [Document*] A pointer to the owning {\bf Document} or 
   *                      {\tt NULL}, if the {\bf Node} does not have an owner.
   */

IXML_Document*               
ixmlNode_getOwnerDocument(IXML_Node *nodeptr  
		            /** The {\bf Node} from which to retrieve the 
			        owner document. */
                      );

  /** Retrieves the namespace URI for a {\bf Node} as a {\bf DOMString}.  Only
   *  {\bf Node}s of type {\tt eELEMENT_NODE} or {\tt eATTRIBUTE_NODE} can 
   *  have a namespace URI.  {\bf Node}s created through the {\bf Document} 
   *  interface will only contain a namespace if created using 
   *  {\bf ixmlDocument_createElementNS}.
   *
   *  @return [const DOMString] A {\bf DOMString} representing the URI of the 
   *                            namespace or {\tt NULL}.
   */

const DOMString         
ixmlNode_getNamespaceURI(IXML_Node *nodeptr  
		           /** The {\bf Node} for which to retrieve the 
			       namespace. */
                     );

  /** Retrieves the namespace prefix, if present.  The prefix is the name
   *  used as an alias for the namespace URI for this element.  Only 
   *  {\bf Node}s of type {\tt eELEMENT_NODE} or {\tt eATTRIBUTE_NODE} can have 
   *  a prefix. {\bf Node}s created through the {\bf Document} interface will 
   *  only contain a prefix if created using {\bf ixmlDocument_createElementNS}.
   *
   *  @return [DOMString] A {\bf DOMString} representing the namespace prefix 
   *                      or {\tt NULL}.
   */

DOMString               
ixmlNode_getPrefix(IXML_Node *nodeptr  
		     /** The {\bf Node} from which to retrieve the prefix. */
               );

  /** Retrieves the local name of a {\bf Node}, if present.  The local name is
   *  the tag name without the namespace prefix.  Only {\bf Node}s of type
   *  {\tt eELEMENT_NODE} or {\tt eATTRIBUTE_NODE} can have a local name.
   *  {\Bf Node}s created through the {\bf Document} interface will only 
   *  contain a local name if created using {\bf ixmlDocument_createElementNS}.
   *
   *  @return [const DOMString] A {\bf DOMString} representing the local name 
   *                            of the {\bf Element} or {\tt NULL}.
   */

const DOMString         
ixmlNode_getLocalName(IXML_Node *nodeptr  
		        /** The {\bf Node} from which to retrieve the local 
			    name. */
                  );

  /** Inserts a new child {\bf Node} before the existing child {\bf Node}.  
   *  {\bf refChild} can be {\tt NULL}, which inserts {\bf newChild} at the
   *  end of the list of children.  Note that the {\bf Node} (or {\bf Node}s) 
   *  in {\bf newChild} must already be owned by the owner document (or have no
   *  owner at all) of {\bf nodeptr} for insertion.  If not, the {\bf Node} 
   *  (or {\bf Node}s) must be imported into the document using 
   *  {\bf ixmlDocument_importNode}.  If {\bf newChild} is already in the tree,
   *  it is removed first.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf nodeptr} or 
   *            {\bf newChild} is {\tt NULL}.
   *      \item {\tt IXML_HIERARCHY_REQUEST_ERR}: The type of the {\bf Node} 
   *            does not allow children of the type of {\bf newChild}.
   *      \item {\tt IXML_WRONG_DOCUMENT_ERR}: {\bf newChild} has an owner 
   *            document that does not match the owner of {\bf nodeptr}.
   *      \item {\tt IXML_NO_MODIFICATION_ALLOWED_ERR}: {\bf nodeptr} is 
   *            read-only or the parent of the {\bf Node} being inserted is 
   *            read-only.
   *      \item {\tt IXML_NOT_FOUND_ERR}: {\bf refChild} is not a child of 
   *            {\bf nodeptr}.
   *    \end{itemize}
   */

int     
ixmlNode_insertBefore(IXML_Node *nodeptr,   
		        /** The parent of the {\bf Node} before which to 
			    insert the new child. */
                      IXML_Node* newChild,      
		        /** The {\bf Node} to insert into the tree. */
                      IXML_Node* refChild       
		        /** The reference child where the new {\bf Node} 
			    should be inserted. The new {\bf Node} will
			    appear directly before the reference child. */
                  );

  /** Replaces an existing child {\bf Node} with a new child {\bf Node} in 
   *  the list of children of a {\bf Node}. If {\bf newChild} is already in 
   *  the tree, it will first be removed. {\bf returnNode} will contain the 
   *  {\bf oldChild} {\bf Node}, appropriately removed from the tree (i.e. it 
   *  will no longer have an owner document).
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMTER: Either {\bf nodeptr}, {\bf 
   *            newChild}, or {\bf oldChild} is {\tt NULL}.
   *      \item {\tt IXML_HIERARCHY_REQUEST_ERR}: The {\bf newChild} is not 
   *            a type of {\bf Node} that can be inserted into this tree or 
   *            {\bf newChild} is an ancestor of {\bf nodePtr}.
   *      \item {\tt IXML_WRONG_DOCUMENT_ERR}: {\bf newChild} was created from 
   *            a different document than {\bf nodeptr}.
   *      \item {\tt IXML_NO_MODIFICATION_ALLOWED_ERR}: {\bf nodeptr} or 
   *            its parent is read-only.
   *      \item {\tt IXML_NOT_FOUND_ERR}: {\bf oldChild} is not a child of 
   *            {\bf nodeptr}.
   *    \end{itemize}
   */

int     
ixmlNode_replaceChild(IXML_Node *nodeptr,     
		        /** The parent of the {\bf Node} which contains the 
			    child to replace. */
                      IXML_Node* newChild,        
		        /** The child with which to replace {\bf oldChild}. */
                      IXML_Node* oldChild,        
		        /** The child to replace with {\bf newChild}. */
                      IXML_Node** returnNode      
		        /** Pointer to a {\bf Node} to place the removed {\bf 
			    oldChild} {\bf Node}. */
                  );

  /** Removes a child from the list of children of a {\bf Node}.
   *  {\bf returnNode} will contain the {\bf oldChild} {\bf Node}, 
   *  appropriately removed from the tree (i.e. it will no longer have an 
   *  owner document).
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf nodeptr} or 
   *            {\bf oldChild} is {\tt NULL}.
   *      \item {\tt IXML_NO_MODIFICATION_ALLOWED_ERR}: {\bf nodeptr} or its 
   *            parent is read-only.
   *      \item {\tt IXML_NOT_FOUND_ERR}: {\bf oldChild} is not among the 
   *            children of {\bf nodeptr}.
   *    \end{itemize}
   */

int     
ixmlNode_removeChild(IXML_Node *nodeptr,     
		       /** The parent of the child to remove. */
                     IXML_Node* oldChild,  
		       /** The child {\bf Node} to remove. */
                     IXML_Node **returnNode
		       /** Pointer to a {\bf Node} to place the removed {\bf 
			   oldChild} {\bf Node}. */
                 );

  /** Appends a child {\bf Node} to the list of children of a {\bf Node}.  If
   *  {\bf newChild} is already in the tree, it is removed first.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf nodeptr} or 
   *            {\bf newChild} is {\tt NULL}.
   *      \item {\tt IXML_HIERARCHY_REQUEST_ERR}: {\bf newChild} is of a type 
   *            that cannot be added as a child of {\bf nodeptr} or 
   *            {\bf newChild} is an ancestor of {\bf nodeptr}.
   *      \item {\tt IXML_WRONG_DOCUMENT_ERR}: {\bf newChild} was created from 
   *            a different document than {\bf nodeptr}.
   *      \item {\tt IXML_NO_MODIFICATION_ALLOWED_ERR}: {\bf nodeptr} is a 
   *            read-only {\bf Node}.
   */

int     
ixmlNode_appendChild(IXML_Node *nodeptr,  
		       /** The {\bf Node} in which to append the new child. */
                     IXML_Node* newChild      
		       /** The new child to append. */
                 );

  /** Queries whether or not a {\bf Node} has children.
   *
   *  @return [BOOL] {\tt TRUE} if the {\bf Node} has one or more children 
   *                 otherwise {\tt FALSE}.
   */

BOOL    
ixmlNode_hasChildNodes(IXML_Node *nodeptr  
		         /** The {\bf Node} to query for children. */
                   );

  /** Clones a {\bf Node}.  The new {\bf Node} does not have a parent.  The
   *  {\bf deep} parameter controls whether the subtree of the {\bf Node} is
   *  also cloned.  For details on cloning specific types of {\bf Node}s, 
   *  refer to the DOM2-Core recommendation.
   *
   *  @return [Node*] A clone of {\bf nodeptr} or {\tt NULL}.
   */

IXML_Node*   
ixmlNode_cloneNode(IXML_Node *nodeptr,  
		     /** The {\bf Node} to clone.  */
                   BOOL deep
		     /** {\tt TRUE} to clone the subtree also or {\tt FALSE} 
		         to clone only {\bf nodeptr}. */
                  );

  /** Queries whether this {\bf Node} has attributes.  Note that only 
   *  {\bf Element} nodes have attributes.
   *
   *  @return [BOOL] {\tt TRUE} if the {\bf Node} has attributes otherwise 
   *                 {\tt FALSE}.
   */

BOOL    
ixmlNode_hasAttributes(IXML_Node *node  
		         /** The {\bf Node} to query for attributes. */
                      );

  /** Frees a {\bf Node} and all {\bf Node}s in its subtree.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlNode_free(IXML_Node *IXML_Node  
		/** The {\bf Node} to free. */
             );

//@}

/*================================================================
*
*   Attribute interfaces
*
*
*=================================================================*/

/**@name Interface {\it Attr}
 * The {\bf Attr} interface represents an attribute of an {\bf Element}.
 * The document type definition (DTD) or schema usually dictate the
 * allowable attributes and values for a particular element.  For more 
 * information, refer to the {\it Interface Attr} section in the DOM2-Core.
 */
//@{


  /** Frees an {\bf Attr} node.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlAttr_free(IXML_Attr *attrNode  
		/** The {\bf Attr} node to free.  */
             );

//@}


/*================================================================
*
*   CDATASection interfaces
*
*
*=================================================================*/

/**@name Interface {\it CDATASection}
 * The {\bf CDATASection} is used to escape blocks of text containing
 * characters that would otherwise be regarded as markup. CDATA sections
 * cannot be nested. Their primary purpose is for including material such
 * XML fragments, without needing to escape all the delimiters.  For more 
 * information, refer to the {\it Interface CDATASection} section in the
 * DOM2-Core.
 */
//@{


  /** Initializes a {\bf CDATASection} node.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlCDATASection_init(IXML_CDATASection *nodeptr  
		        /** The {\bf CDATASection} node to initialize.  */
                     );


  /** Frees a {\bf CDATASection} node.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlCDATASection_free(IXML_CDATASection *nodeptr  
		        /** The {\bf CDATASection} node to free. */
                     );

//@}

/*================================================================
*
*   Document interfaces
*
*
*=================================================================*/

/**@name Interface {\it Document}
 * The {\bf Document} interface represents the entire XML document.
 * In essence, it is the root of the document tree and provides the
 * primary interface to the elements of the document.  For more information,
 * refer to the {\it Interface Document} section in the DOM2Core.
 */
//@{

  /** Initializes a {\bf Document} node.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlDocument_init(IXML_Document *nodeptr  
		    /** The {\bf Document} node to initialize.  */
                 );

  /** Creates a new empty {\bf Document} node.  The 
   *  {\bf ixmlDocument_createDocumentEx} API differs from the {\bf
   *  ixmlDocument_createDocument} API in that it returns an error code
   *  describing the reason for the failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int ixmlDocument_createDocumentEx(IXML_Document** doc 
		                    /** Pointer to a {\bf Document} where the 
				        new object will be stored. */
		                  );


  /** Creates a new empty {\bf Document} node.
   *
   *  @return [Document*] A pointer to the new {\bf Document} or {\tt NULL} on 
   *                      failure.
   */

IXML_Document* ixmlDocument_createDocument();

  /** Creates a new {\bf Element} node with the given tag name.  The new
   *  {\bf Element} node has a {\tt nodeName} of {\bf tagName} and
   *  the {\tt localName}, {\tt prefix}, and {\tt namespaceURI} set 
   *  to {\tt NULL}.  To create an {\bf Element} with a namespace, 
   *  see {\bf ixmlDocument_createElementNS}.
   *
   *  The {\bf ixmlDocument_createElementEx} API differs from the {\bf
   *  ixmlDocument_createElement} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc} or 
   *            {\bf tagName} is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlDocument_createElementEx(IXML_Document *doc,  
		               /** The owner {\bf Document} of the new node. */
                             const DOMString tagName,  
			       /** The tag name of the new {\bf Element} 
				   node. */
                             IXML_Element **rtElement
			       /** Pointer to an {\bf Element} where the new 
				   object will be stored. */
                            );

  /** Creates a new {\bf Element} node with the given tag name.  The new
   *  {\bf Element} node has a {\tt nodeName} of {\bf tagName} and
   *  the {\tt localName}, {\tt prefix}, and {\tt namespaceURI} set 
   *  to {\tt NULL}.  To create an {\bf Element} with a namespace, 
   *  see {\bf ixmlDocument_createElementNS}.
   *
   *  @return [Document*] A pointer to the new {\bf Element} or {\tt NULL} on 
   *                      failure.
   */

IXML_Element*
ixmlDocument_createElement(IXML_Document *doc,  
		             /** The owner {\bf Document} of the new node. */
                           const DOMString tagName    
			     /** The tag name of the new {\bf Element} node. */
                           );


  /** Creates a new {\bf Text} node with the given data.  
   *  The {\bf ixmlDocument_createTextNodeEx} API differs from the {\bf
   *  ixmlDocument_createTextNode} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc} or {\bf data} 
   *            is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlDocument_createTextNodeEx(IXML_Document *doc,  
		                /** The owner {\bf Document} of the new node. */
                              const DOMString data,      
			        /** The data to associate with the new {\bf 
				    Text} node. */
                              IXML_Node** textNode 
			        /** A pointer to a {\bf Node} where the new 
				    object will be stored. */
                              );


  /** Creates a new {\bf Text} node with the given data.
   *
   *  @return [Node*] A pointer to the new {\bf Node} or {\tt NULL} on failure.
   */

IXML_Node*
ixmlDocument_createTextNode(IXML_Document *doc,  
		              /** The owner {\bf Document} of the new node. */
                            const DOMString data       
			      /** The data to associate with the new {\bf Text} 
			          node. */
                            );

  /** Creates a new {\bf CDATASection} node with given data.
   *
   *  The {\bf ixmlDocument_createCDATASectionEx} API differs from the {\bf
   *  ixmlDocument_createCDATASection} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc} or {\bd data} 
   *            is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlDocument_createCDATASectionEx(IXML_Document *doc,  
		                    /** The owner {\bf Document} of the new 
				        node. */
                                  DOMString data,      
				    /** The data to associate with the new 
				        {\bf CDATASection} node. */
                                  IXML_CDATASection** cdNode   
				    /** A pointer to a {\bf Node} where the 
				        new object will be stored. */ 
                                 );


  /** Creates a new {\bf CDATASection} node with given data.
   *
   *  @return [CDATASection*] A pointer to the new {\bf CDATASection} or 
   *                          {\tt NULL} on failure.
   */

IXML_CDATASection*
ixmlDocument_createCDATASection(IXML_Document *doc,  
				  /** The owner {\bf Document} of the new 
				      node. */
                                DOMString data  
				  /** The data to associate with the new {\bf 
				      CDATASection} node. */
                               );

  /** Creates a new {\bf Attr} node with the given name.  
   *
   *  @return [Attr*] A pointer to the new {\bf Attr} or {\tt NULL} on failure.
   */

IXML_Attr*
ixmlDocument_createAttribute(IXML_Document *doc,  
		               /** The owner {\bf Document} of the new node. */
                             char *name      
			       /** The name of the new attribute. */
                            );


  /** Creates a new {\bf Attr} node with the given name.  
   *
   *  The {\bf ixmlDocument_createAttributeEx} API differs from the {\bf
   *  ixmlDocument_createAttribute} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc} or {\bf name} 
   *            is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlDocument_createAttributeEx(IXML_Document *doc,  
		                 /** The owner {\bf Document} of the new 
				     node. */
                               char *name,      
			         /** The name of the new attribute. */
                               IXML_Attr** attrNode
			         /** A pointer to a {\bf Attr} where the new 
				     object will be stored. */
                              );


  /** Returns a {\bf NodeList} of all {\bf Elements} that match the given
   *  tag name in the order in which they were encountered in a preorder
   *  traversal of the {\bf Document} tree.  
   *
   *  @return [NodeList*] A pointer to a {\bf NodeList} containing the 
   *                      matching items or {\tt NULL} on an error.
   */

IXML_NodeList*
ixmlDocument_getElementsByTagName(IXML_Document *doc,     
		                    /** The {\bf Document} to search. */
                                  DOMString tagName  
				    /** The tag name to find. */
                                 );

// introduced in DOM level 2

  /** Creates a new {\bf Element} node in the given qualified name and
   *  namespace URI.
   *
   *  The {\bf ixmlDocument_createElementNSEx} API differs from the {\bf
   *  ixmlDocument_createElementNS} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc}, 
   *            {\bf namespaceURI}, or {\bf qualifiedName} is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlDocument_createElementNSEx(IXML_Document *doc,           
		                 /** The owner {\bf Document} of the new 
				     node. */
                               DOMString namespaceURI,  
			         /** The namespace URI for the new {\bf 
				     Element}. */
                               DOMString qualifiedName,  
			         /** The qualified name of the new {\bf 
				     Element}. */
                               IXML_Element** rtElement
			         /** A pointer to an {\bf Element} where the 
				     new object will be stored. */
                              );


  /** Creates a new {\bf Element} node in the given qualified name and
   *  namespace URI.
   *
   *  @return [Element*] A pointer to the new {\bf Element} or {\tt NULL} on 
   *                     failure.
   */

IXML_Element*
ixmlDocument_createElementNS(IXML_Document *doc,           
		               /** The owner {\bf Document} of the new node. */
                             DOMString namespaceURI,  
			       /** The namespace URI for the new {\bf 
				   Element}. */
                             DOMString qualifiedName  
			       /** The qualified name of the new {\bf 
				   Element}. */
                             );

  /** Creates a new {\bf Attr} node with the given qualified name and
   *  namespace URI.
   *
   *  The {\bf ixmlDocument_createAttributeNSEx} API differs from the {\bf
   *  ixmlDocument_createAttributeNS} API in that it returns an error code
   *  describing the reason for failure rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc}, 
   *            {\bf namespaceURI}, or {\bf qualifiedName} is {\tt NULL}.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}   
   */

int
ixmlDocument_createAttributeNSEx(IXML_Document *doc,
		                   /** The owner {\bf Document} of the new 
				       {\bf Attr}. */
                                 DOMString namespaceURI, 
				   /** The namespace URI for the attribute. */
                                 DOMString qualifiedName, 
				   /** The qualified name of the attribute. */
                                 IXML_Attr** attrNode
				   /** A pointer to an {\bf Attr} where the 
				       new object will be stored. */
                                );   

  /** Creates a new {\bf Attr} node with the given qualified name and
   *  namespace URI.
   *
   *  @return [Attr*] A pointer to the new {\bf Attr} or {\tt NULL} on failure.
   */

IXML_Attr*
ixmlDocument_createAttributeNS(IXML_Document *doc, 
		                 /** The owner {\bf Document} of the new 
				     {\bf Attr}. */
                               DOMString namespaceURI, 
			         /** The namespace URI for the attribute. */
                               DOMString qualifiedName 
			         /** The qualified name of the attribute. */
                              );   

  /** Returns a {\bf NodeList} of {\bf Elements} that match the given
   *  local name and namespace URI in the order they are encountered
   *  in a preorder traversal of the {\bf Document} tree.  Either 
   *  {\bf namespaceURI} or {\bf localName} can be the special {\tt "*"}
   *  character, which matches any namespace or any local name respectively.
   *
   *  @return [NodeList*] A pointer to a {\bf NodeList} containing the 
   *                      matching items or {\tt NULL} on an error.
   */

IXML_NodeList*   
ixmlDocument_getElementsByTagNameNS(IXML_Document* doc,          
		                      /** The {\bf Document} to search. */
                                    DOMString namespaceURI, 
				      /** The namespace of the elements to 
                                          find or {\tt "*"} to match any 
                                          namespace. */
                                    DOMString localName     
				      /** The local name of the elements to 
                                          find or {\tt "*"} to match any local 
                                          name.  */
                                    );

  /** Returns the {\bf Element} whose {\tt ID} matches that given id.
   *
   *  @return [Element*] A pointer to the matching {\bf Element} or 
   *                     {\tt NULL} on an error.
   */

IXML_Element*    
ixmlDocument_getElementById(IXML_Document* doc,         
		              /** The owner {\bf Document} of the {\bf 
			          Element}. */
                            DOMString tagName  
			      /** The name of the {\bf Element}.*/
                            );

  /** Frees a {\bf Document} object and all {\bf Node}s associated with it.  
   *  Any {\bf Node}s extracted via any other interface function, e.g. 
   *  {\bf ixmlDocument_GetElementById}, become invalid after this call unless
   *  explicitly cloned.
   *
   *  @return [void] This function does not return a value.
   */

void        
ixmlDocument_free(IXML_Document* doc  
		    /** The {\bf Document} to free.  */
                 );

  /** Imports a {\bf Node} from another {\bf Document} into this 
   *  {\bf Document}.  The new {\bf Node} does not a have parent node: it is a 
   *  clone of the original {\bf Node} with the {\tt ownerDocument} set to 
   *  {\bf doc}.  The {\bf deep} parameter controls whether all the children 
   *  of the {\bf Node} are imported.  Refer to the DOM2-Core recommendation 
   *  for details on importing specific node types.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf doc} or 
   *            {\bf importNode} is not a valid pointer.
   *      \item {\tt IXML_NOT_SUPPORTED_ERR}: {\bf importNode} is a 
   *            {\bf Document}, which cannot be imported.
   *      \item {\tt IXML_FAILED}: The import operation failed because the 
   *            {\bf Node} to be imported could not be cloned.
   *    \end{itemize}
   */

int         
ixmlDocument_importNode(IXML_Document* doc,     
		          /** The {\bf Document} into which to import. */
                        IXML_Node* importNode,  
			  /** The {\bf Node} to import. */
                        BOOL deep,         
			  /** {\tt TRUE} to import all children of {\bf 
			      importNode} or {\tt FALSE} to import only the 
			      root node. */
                        IXML_Node** rtNode      
			  /** A pointer to a new {\bf Node} owned by {\bf 
			      doc}. */
                       );
//@}

/*================================================================
*
*   Element interfaces
*
*
*=================================================================*/

/**@name Interface {\it Element}
 * The {\bf Element} interface represents an element in an XML document.
 * Only {\bf Element}s are allowed to have attributes, which are stored in the
 * {\tt attributes} member of a {\bf Node}.  The {\bf Element} interface
 * extends the {\bf Node} interface and adds more operations to manipulate
 * attributes.
 */
//@{

  /** Initializes a {\bf IXML_Element} node.
   *
   *  @return [void] This function does not return a value.
   */

void ixmlElement_init(IXML_Element *element  
		        /** The {\bf Element} to initialize.*/
                     );


  /** Returns the name of the tag as a constant string.
   *
   *  @return [const DOMString] A {\bf DOMString} representing the name of the 
   *                            {\bf Element}.
   */

const DOMString
ixmlElement_getTagName(IXML_Element* element  
		         /** The {\bf Element} from which to retrieve the 
			     name. */
                      );

  /** Retrieves an attribute of an {\bf Element} by name.  
   *
   *  @return [DOMString] A {\bf DOMString} representing the value of the 
   *                      attribute.
   */

DOMString   
ixmlElement_getAttribute(IXML_Element* element,  
		           /** The {\bf Element} from which to retrieve the 
			       attribute. */
                         DOMString name     
			   /** The name of the attribute to retrieve. */
                        );

  /** Adds a new attribute to an {\bf Element}.  If an attribute with the same
   *  name already exists, the attribute value will be updated with the
   *  new value in {\bf value}.  
   *
   *  @return [int] An integer representing of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element}, 
   *            {\bf name}, or {\bf value} is {\tt NULL}.
   *      \item {\tt IXML_INVALID_CHARACTER_ERR}: {\bf name} contains an 
   *            illegal character.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete the operation.
   *    \end{itemize}
   */

int         
ixmlElement_setAttribute(IXML_Element* element,  
		           /** The {\bf Element} on which to set the 
			       attribute. */
                         DOMString name,    
			   /** The name of the attribute. */
                         DOMString value    
			   /** The value of the attribute.  Note that this is 
			       a non-parsed string and any markup must be 
			       escaped. */
                        );

  /** Removes an attribute by name.  
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element} or 
   *            {\bf name} is {\tt NULL}.
   *    \end{itemize}
   */

int         
ixmlElement_removeAttribute(IXML_Element* element,  
		              /** The {\bf Element} from which to remove the 
			          attribute. */
                            DOMString name     
			      /** The name of the attribute to remove.  */
                           );              

  /** Retrieves an attribute node by name.  See 
   *  {\bf ixmlElement_getAttributeNodeNS} to retrieve an attribute node using
   *  a qualified name or namespace URI.
   *
   *  @return [Attr*] A pointer to the attribute matching {\bf name} or 
   *                  {\tt NULL} on an error.
   */

IXML_Attr*       
ixmlElement_getAttributeNode(IXML_Element* element,  
		               /** The {\bf Element} from which to get the 
				   attribute node.  */
                             DOMString name     
			       /** The name of the attribute node to find. */
                            );

  /** Adds a new attribute node to an {\bf Element}.  If an attribute already
   *  exists with {\bf newAttr} as a name, it will be replaced with the
   *  new one and the old one will be returned in {\bf rtAttr}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element} or 
   *            {\bf newAttr} is {\tt NULL}.
   *      \item {\tt IXML_WRONG_DOCUMENT_ERR}: {\bf newAttr} does not belong 
   *            to the same one as {\bf element}.
   *      \item {\tt IXML_INUSE_ATTRIBUTE_ERR}: {\bf newAttr} is already 
   *            an attribute of another {\bf Element}.
   *    \end{itemize}
   */

int         
ixmlElement_setAttributeNode(IXML_Element* element,  
		               /** The {\bf Element} in which to add the new 
				   attribute. */
                             IXML_Attr* newAttr,     
			       /** The new {\bf Attr} to add. */
                             IXML_Attr** rtAttr      
			       /** A pointer to an {\bf Attr} where the old 
				   {\bf Attr} will be stored.  This will have  
				   a {\tt NULL} if no prior node 
				   existed. */
                            );

  /** Removes the specified attribute node from an {\bf Element}.  
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element} or 
   *            {\bf oldAttr} is {\tt NULL}.
   *      \item {\tt IXML_NOT_FOUND_ERR}: {\bf oldAttr} is not among the list 
   *            attributes of {\bf element}.
   *    \end{itemize}
   */

int         
ixmlElement_removeAttributeNode(IXML_Element* element,  
		                  /** The {\bf Element} from which to remove 
				      the attribute. */
                                IXML_Attr* oldAttr,     
				  /** The attribute to remove from the {\bf 
				      Element}. */
                                IXML_Attr** rtAttr      
				  /** A pointer to an attribute in which to 
				      place the removed attribute. */
                               );

  /** Returns a {\bf NodeList} of all {\it descendant} {\bf Elements} with
   *  a given tag name, in the order in which they are encountered in a
   *  pre-order traversal of this {\bf Element} tree.
   *
   *  @return [NodeList*] A {\bf NodeList} of the matching {\bf Element}s or 
   *                      {\tt NULL} on an error.
   */

IXML_NodeList*   
ixmlElement_getElementsByTagName(IXML_Element* element,  
		                   /** The {\bf Element} from which to start 
				       the search. */
                                 DOMString tagName  
				   /** The name of the tag for which to 
				       search. */
                                );

// introduced in DOM 2

  /** Retrieves an attribute value using the local name and namespace URI.
   *
   *  @return [DOMString] A {\bf DOMString} representing the value of the 
   *                      matching attribute.
   */

DOMString   
ixmlElement_getAttributeNS(IXML_Element* element,       
		             /** The {\bf Element} from which to get the 
			         attribute value. */
                           DOMString namespaceURI, 
			     /** The namespace URI of the attribute. */
                           DOMString localname     
			     /** The local name of the attribute. */
                          );

  /** Adds a new attribute to an {\bf Element} using the local name and 
   *  namespace URI.  If another attribute matches the same local name and 
   *  namespace, the prefix is changed to be the prefix part of the 
   *  {\tt qualifiedName} and the value is changed to {\bf value}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element}, 
   *            {\bf namespaceURI}, {\bf qualifiedName}, or {\bf value} is 
   *            {\tt NULL}.
   *      \item {\tt IXML_INVALID_CHARACTER_ERR}: {\bf qualifiedName} contains 
   *            an invalid character.
   *      \item {\tt IXML_NAMESPACE_ERR}: Either the {\bf qualifiedName} or 
   *            {\bf namespaceURI} is malformed.  Refer to the DOM2-Core for 
   *            possible reasons.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exist 
   *            to complete the operation.
   *      \item {\tt IXML_FAILED}: The operation could not be completed.
   *    \end{itemize}
   */

int         
ixmlElement_setAttributeNS(IXML_Element* element,         
		             /** The {\bf Element} on which to set the 
			         attribute. */
                           DOMString namespaceURI,   
		             /** The namespace URI of the new attribute. */
                           DOMString qualifiedName,  
			     /** The qualified name of the attribute. */
                           DOMString value 
			     /** The new value for the attribute. */
                          );

  /** Removes an attribute using the namespace URI and local name.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element}, 
   *            {\bf namespaceURI}, or {\bf localName} is {\tt NULL}.
   *    \end{itemize}
   */

int         
ixmlElement_removeAttributeNS(IXML_Element* element,        
		                /** The {\bf Element} from which to remove the 
				    the attribute. */
                              DOMString namespaceURI,  
			        /** The namespace URI of the attribute. */
                              DOMString localName      
			        /** The local name of the attribute.*/
                             );

  /** Retrieves an {\bf Attr} node by local name and namespace URI.
   *
   *  @return [Attr*] A pointer to an {\bf Attr} or {\tt NULL} on an error.
   */

IXML_Attr*       
ixmlElement_getAttributeNodeNS(IXML_Element* element,        
		                 /** The {\bf Element} from which to get the 
				     attribute. */
                               DOMString namespaceURI,  
			         /** The namespace URI of the attribute. */
                               DOMString localName      
			         /** The local name of the attribute. */
                              );

  /** Adds a new attribute node.  If an attribute with the same local name
   *  and namespace URI already exists in the {\bf Element}, the existing 
   *  attribute node is replaced with {\bf newAttr} and the old returned in 
   *  {\bf rcAttr}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: Either {\bf element} or 
   *            {\bf newAttr} is {\tt NULL}.
   *      \item {\tt IXML_WRONG_DOCUMENT_ERR}: {\bf newAttr} does not belong 
   *            to the same document as {\bf element}.
   *      \item {\tt IXML_INUSE_ATTRIBUTE_ERR}: {\bf newAttr} already is an 
   *            attribute of another {\bf Element}.
   *    \end{itemize}
   */

int         
ixmlElement_setAttributeNodeNS(IXML_Element* element,  
		                 /** The {\bf Element} in which to add the 
				     attribute node. */
                               IXML_Attr*   newAttr,     
			         /** The new {\bf Attr} to add. */
                               IXML_Attr**  rcAttr      
			         /** A pointer to the replaced {\bf Attr}, if 
				     it exists. */
                              );

  /** Returns a {\bf NodeList} of all {\it descendant} {\bf Elements} with a
   *  given tag name, in the order in which they are encountered in the
   *  pre-order traversal of the {\bf Element} tree.
   *
   *  @return [NodeList*] A {\bf NodeList} of matching {\bf Element}s or 
   *                      {\tt NULL} on an error.
   */

IXML_NodeList*   
ixmlElement_getElementsByTagNameNS(IXML_Element* element,        
		                     /** The {\bf Element} from which to start 
				         the search. */
                                   DOMString namespaceURI,  
				     /** The namespace URI of the {\bf 
				         Element}s to find. */
                                   DOMString localName      
				     /** The local name of the {\bf Element}s 
				         to find. */
                                  );

  /** Queries whether the {\bf Element} has an attribute with the given name
   *  or a default value.
   *
   *  @return [BOOL] {\tt TRUE} if the {\bf Element} has an attribute with 
   *                 this name or has a default value for that attribute, 
   *                 otherwise {\tt FALSE}.
   */

BOOL        
ixmlElement_hasAttribute(IXML_Element* element, 
		           /** The {\bf Element} on which to check for an 
			       attribute. */
                         DOMString name    
			   /** The name of the attribute for which to check. */
                        );

  /** Queries whether the {\bf Element} has an attribute with the given
   *  local name and namespace URI or has a default value for that attribute.
   *
   *  @return [BOOL] {\tt TRUE} if the {\bf Element} has an attribute with 
   *                 the given namespace and local name or has a default 
   *                 value for that attribute, otherwise {\tt FALSE}.
   */

BOOL        
ixmlElement_hasAttributeNS(IXML_Element* element,       
		             /** The {\bf Element} on which to check for the 
			         attribute. */
                           DOMString namespaceURI, 
			     /** The namespace URI of the attribute. */
                           DOMString localName     
			     /** The local name of the attribute. */
                          );

  /** Frees the given {\bf Element} and any subtree of the {\bf Element}.
   *
   *  @return [void] This function does not return a value.
   */

void        
ixmlElement_free(IXML_Element* element  
		   /** The {\bf Element} to free. */
                );

//@}

/*================================================================
*
*   NamedNodeMap interfaces
*
*
*=================================================================*/

/**@name Interface {\it NamedNodeMap}
 * A {\bf NamedNodeMap} object represents a list of objects that can be
 * accessed by name.  A {\bf NamedNodeMap} maintains the objects in 
 * no particular order.  The {\bf Node} interface uses a {\bf NamedNodeMap}
 * to maintain the attributes of a node.
 */
//@{

  /** Returns the number of items contained in this {\bf NamedNodeMap}.
   *
   *  @return [unsigned long] The number of nodes in this map.
   */

unsigned long 
ixmlNamedNodeMap_getLength(IXML_NamedNodeMap *nnMap  
		             /** The {\bf NamedNodeMap} from which to retrieve 
			         the size. */
                          );

  /** Retrieves a {\bf Node} from the {\bf NamedNodeMap} by name.
   *
   *  @return [Node*] A {\bf Node} or {\tt NULL} if there is an error.
   */

IXML_Node*   
ixmlNamedNodeMap_getNamedItem(IXML_NamedNodeMap *nnMap, 
		                /** The {\bf NamedNodeMap} to search. */
                              DOMString name       
			        /** The name of the {\bf Node} to find. */
                             );

  /** Adds a new {\bf Node} to the {\bf NamedNodeMap} using the {\bf Node} 
   *  name attribute.
   *
   *  @return [Node*] The old {\bf Node} if the new {\bf Node} replaces it or 
   *                  {\tt NULL} if the {\bf Node} was not in the 
   *                  {\bf NamedNodeMap} before.
   */

IXML_Node*   
ixmlNamedNodeMap_setNamedItem(IXML_NamedNodeMap *nnMap, 
		                /** The {\bf NamedNodeMap} in which to add the 
				    new {\bf Node}. */
                              IXML_Node *arg            
			        /** The new {\bf Node} to add to the {\bf 
				    NamedNodeMap}. */
                             );

  /** Removes a {\bf Node} from a {\bf NamedNodeMap} specified by name.
   *
   *  @return [Node*] A pointer to the {\bf Node}, if found, or {\tt NULL} if 
   *                  it wasn't.
   */

IXML_Node*   
ixmlNamedNodeMap_removeNamedItem(IXML_NamedNodeMap *nnMap,  
		                   /** The {\bf NamedNodeMap} from which to 
				       remove the item. */
                                 DOMString name        
				   /** The name of the item to remove. */
                                );

  /** Retrieves a {\bf Node} from a {\bf NamedNodeMap} specified by a
   *  numerical index.
   *
   *  @return [Node*] A pointer to the {\bf Node}, if found, or {\tt NULL} if 
   *                  it wasn't.
   */

IXML_Node*   
ixmlNamedNodeMap_item(IXML_NamedNodeMap *nnMap, 
		        /** The {\bf NamedNodeMap} from which to remove the 
			    {\bf Node}. */
                      unsigned long index  
		        /** The index into the map to remove. */
                     );

// introduced in DOM level 2

  /** Retrieves a {\bf Node} from a {\bf NamedNodeMap} specified by
   *  namespace URI and local name.
   *
   *  @return [Node*] A pointer to the {\bf Node}, if found, or {\tt NULL} if 
   *                  it wasn't
   */

IXML_Node*   
ixmlNamedNodeMap_getNamedItemNS(IXML_NamedNodeMap *nnMap,    
		                  /** The {\bf NamedNodeMap} from which to 
				      remove the {\bf Node}. */
                                DOMString *namespaceURI,
				  /** The namespace URI of the {\bf Node} to 
                                      remove. */
                                DOMString localName     
				  /** The local name of the {\bf Node} to 
				      remove. */
                               );

  /** Adds a new {\bf Node} to the {\bf NamedNodeMap} using the {\bf Node} 
   *  local name and namespace URI attributes.
   *
   *  @return [Node*] The old {\bf Node} if the new {\bf Node} replaces it or 
   *                  {\tt NULL} if the {\bf Node} was not in the 
   *                  {\bf NamedNodeMap} before.
   */

IXML_Node*   
ixmlNamedNodeMap_setNamedItemNS(IXML_NamedNodeMap *nnMap, 
		                  /** The {\bf NamedNodeMap} in which to add 
				      the {\bf Node}. */
                                IXML_Node *arg 
				  /** The {\bf Node} to add to the map. */
                               );

  /** Removes a {\bf Node} from a {\bf NamedNodeMap} specified by 
   *  namespace URI and local name.
   *
   *  @return [Node*] A pointer to the {\bf Node}, if found, or {\tt NULL} if 
   *          it wasn't.
   */

IXML_Node*   
ixmlNamedNodeMap_removeNamedItemNS(IXML_NamedNodeMap *nnMap,    
		                     /** The {\bf NamedNodeMap} from which to 
				         remove the {\bf Node}. */
                                   DOMString namespaceURI, 
				     /** The namespace URI of the {\bf Node} 
				         to remove. */
                                   DOMString localName     
				     /** The local name of the {\bf Node} to 
				         remove. */
                                  );

  /** Frees a {\bf NamedNodeMap}.  The {\bf Node}s inside the map are not
   *  freed, just the {\bf NamedNodeMap} object.
   *
   *  @return [void] This function does not return a value.
   */

void    
ixmlNamedNodeMap_free(IXML_NamedNodeMap *nnMap  
		        /** The {\bf NamedNodeMap to free}. */
                     );

//@}

/*================================================================
*
*   NodeList interfaces
*
*
*=================================================================*/

/**@name Interface {\it NodeList}
 * The {\bf NodeList} interface abstracts an ordered collection of
 * nodes.  Note that changes to the underlying nodes will change
 * the nodes contained in a {\bf NodeList}.  The DOM2-Core refers to
 * this as being {\it live}.
 */
//@{

  /** Retrieves a {\bf Node} from a {\bf NodeList} specified by a 
   *  numerical index.
   *
   *  @return [Node*] A pointer to a {\bf Node} or {\tt NULL} if there was an 
   *                  error.
   */

IXML_Node*           
ixmlNodeList_item(IXML_NodeList *nList,     
		    /** The {\bf NodeList} from which to retrieve the {\bf 
		        Node}. */
                  unsigned long index  
		    /** The index into the {\bf NodeList} to retrieve. */
                 );

  /** Returns the number of {\bf Nodes} in a {\bf NodeList}.
   *
   *  @return [unsigned long] The number of {\bf Nodes} in the {\bf NodeList}.
   */

unsigned long   
ixmlNodeList_length(IXML_NodeList *nList  
		      /** The {\bf NodeList} for which to retrieve the 
		          number of {\bf Nodes}. */
                   );

  /** Frees a {\bf NodeList} object.  Since the underlying {\bf Nodes} are
   *  references, they are not freed using this operating.  This only
   *  frees the {\bf NodeList} object.
   *
   *  @return [void] This function does not return a value.
   */

void            
ixmlNodeList_free(IXML_NodeList *nList  
		    /** The {\bf NodeList} to free.  */
                 );

//@} Interface NodeList
//@} DOM Interfaces

/**@name IXML API
 * The IXML API contains utility functions that are not part of the standard
 * DOM interfaces.  They include functions to create a DOM structure from a
 * file or buffer, create an XML file from a DOM structure, and manipulate 
 * DOMString objects.
 */
//@{

/*================================================================
* 
*   ixml interfaces
*
*
*=================================================================*/

#define     ixmlPrintDocument(doc)  ixmlPrintNode((IXML_Node *)doc)

#define ixmlDocumenttoString(doc)	ixmlNodetoString((IXML_Node *)doc)

  /** Renders a {\bf Node} and all sub-elements into an XML text
   *  representation.  The caller is required to free the {\bf DOMString}
   *  returned from this function using {\bf ixmlFreeDOMString} when it
   *  is no longer required.
   *
   *  Note that this function can be used for any {\bf Node}-derived
   *  interface.  A similar {\bf ixmlPrintDocument} function is defined
   *  to avoid casting when printing whole documents. This function
   *  introduces lots of white space to print the {\bf DOMString} in readable
   *  format.
   * 
   *  @return [DOMString] A {\bf DOMString} with the XML text representation 
   *                      of the DOM tree or {\tt NULL} on an error.
   */

DOMString   
ixmlPrintNode(IXML_Node *doc  
                /** The root of the {\bf Node} tree to render to XML text. */
             );

  /** Renders a {\bf Node} and all sub-elements into an XML text
   *  representation.  The caller is required to free the {\bf DOMString}
   *  returned from this function using {\bf ixmlFreeDOMString} when it
   *  is no longer required.
   *
   *  Note that this function can be used for any {\bf Node}-derived
   *  interface.  A similar {\bf ixmlPrintDocument} function is defined
   *  to avoid casting when printing whole documents.
   * 
   *  @return [DOMString] A {\bf DOMString} with the XML text representation 
   *                      of the DOM tree or {\tt NULL} on an error.
   */

DOMString   
ixmlNodetoString(IXML_Node *doc  
		   /** The root of the {\bf Node} tree to render to XML text. */
                );


  /** Makes the XML parser more tolerant to malformed text.
   *       
   * If {\bf errorChar} is 0 (default), the parser is strict about XML 
   * encoding : invalid UTF-8 sequences or "&" entities are rejected, and 
   * the parsing aborts.
   * If {\bf errorChar} is not 0, the parser is relaxed : invalid UTF-8 
   * characters are replaced by the {\bf errorChar}, and invalid "&" entities 
   * are left untranslated. The parsing is then allowed to continue.
   */
void
ixmlRelaxParser(char errorChar);


  /** Parses an XML text buffer converting it into an IXML DOM representation.
   *
   *  @return [Document*] A {\bf Document} if the buffer correctly parses or 
   *                      {\tt NULL} on an error. 
   */
IXML_Document*
ixmlParseBuffer(char *buffer 
		  /** The buffer that contains the XML text to convert to a 
		      {\bf Document}. */
               );


  /** Parses an XML text buffer converting it into an IXML DOM representation.
   *
   *  The {\bf ixmlParseBufferEx} API differs from the {\bf ixmlParseBuffer}
   *  API in that it returns an error code representing the actual failure
   *  rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: The {\bf buffer} is not a valid 
   *            pointer.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int
ixmlParseBufferEx(char *buffer, 
		    /** The buffer that contains the XML text to convert to a 
		        {\bf Document}. */
                  IXML_Document** doc 
		    /** A point to store the {\bf Document} if file correctly 
		        parses or {\bf NULL} on an error. */
                );

  /** Parses an XML text file converting it into an IXML DOM representation.
   *
   *  @return [Document*] A {\bf Document} if the file correctly parses or 
   *                      {\tt NULL} on an error.
   */

IXML_Document*
ixmlLoadDocument(char* xmlFile      
		   /** The filename of the XML text to convert to a {\bf 
		       Document}. */
                );

  /** Parses an XML text file converting it into an IXML DOM representation.
   *
   *  The {\bf ixmlLoadDocumentEx} API differs from the {\bf ixmlLoadDocument}
   *  API in that it returns a an error code representing the actual failure
   *  rather than just {\tt NULL}.
   *
   *  @return [int] An integer representing one of the following:
   *    \begin{itemize}
   *      \item {\tt IXML_SUCCESS}: The operation completed successfully.
   *      \item {\tt IXML_INVALID_PARAMETER}: The {\bf xmlFile} is not a valid 
   *            pointer.
   *      \item {\tt IXML_INSUFFICIENT_MEMORY}: Not enough free memory exists 
   *            to complete this operation.
   *    \end{itemize}
   */

int 
ixmlLoadDocumentEx(char* xmlFile,      
		     /** The filename of the XML text to convert to a {\bf 
		         Document}. */
                   IXML_Document** doc   
		     /** A pointer to the {\bf Document} if file correctly 
		         parses or {\bf NULL} on an error. */
                 );

  /** Clones an existing {\bf DOMString}.
   *
   *  @return [DOMString] A new {\bf DOMString} that is a duplicate of the 
   *                      original or {\tt NULL} if the operation could not 
   *                      be completed.
   */

DOMString   
ixmlCloneDOMString(const DOMString src  
		     /** The source {\bf DOMString} to clone. */
                  );

  /** Frees a {\bf DOMString}.
   *
   *  @return [void] This function does not return a value.
   */

void        
ixmlFreeDOMString(DOMString buf  
		    /** The {\bf DOMString} to free. */
                 );

#ifdef __cplusplus
}
#endif

//@} IXML API

#endif  // _IXML_H_
