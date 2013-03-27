/* 
   Wrapper interface to XML parser
   Copyright (C) 1999-2007, 2009, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "ne_internal.h"

#include "ne_alloc.h"
#include "ne_xml.h"
#include "ne_utils.h"
#include "ne_string.h"

#if defined(HAVE_EXPAT)
/* expat support: */
#ifdef HAVE_XMLPARSE_H
#include "xmlparse.h"
#else
#include <expat.h>
#endif
typedef XML_Char ne_xml_char;

#if !defined(XML_MAJOR_VERSION)
#define NEED_BOM_HANDLING
#elif XML_MAJOR_VERSION < 2 && XML_MINOR_VERSION == 95 && XML_MICRO_VERSION < 2
#define NEED_BOM_HANDLING
#endif

#elif defined(HAVE_LIBXML)
/* libxml2 support: */
#include <libxml/xmlversion.h>
#include <libxml/parser.h>
typedef xmlChar ne_xml_char;

#if LIBXML_VERSION < 20619
/* 2.6.19 and earlier have broken BOM handling */
#define NEED_BOM_HANDLING
#endif
#else /* not HAVE_LIBXML */
#  error need an XML parser
#endif /* not HAVE_EXPAT */

/* Approx. one screen of text: */
#define ERR_SIZE (2048)

struct handler {
    ne_xml_startelm_cb *startelm_cb; /* start-element callback */
    ne_xml_endelm_cb *endelm_cb; /* end-element callback */
    ne_xml_cdata_cb *cdata_cb; /* character-data callback. */
    void *userdata; /* userdata for the above. */
    struct handler *next; /* next handler in stack. */
};

#ifdef HAVE_LIBXML
static void sax_error(void *ctx, const char *msg, ...);
#endif

struct element {
    const ne_xml_char *nspace;
    ne_xml_char *name;

    int state; /* opaque state integer */
    
    /* Namespaces declared in this element */
    ne_xml_char *default_ns; /* A default namespace */
    struct namespace *nspaces; /* List of other namespace scopes */

    struct handler *handler; /* Handler for this element */

    struct element *parent; /* parent element, or NULL */    
};

/* We pass around a ne_xml_parser as the userdata in the parsing
 * library.  This maintains the current state of the parse and various
 * other bits and bobs. Within the parse, we store the current branch
 * of the tree, i.e., the current element and all its parents, up to
 * the root, but nothing other than that.  */
struct ne_xml_parser_s {
    struct element *root; /* the root of the document */
    struct element *current; /* current element in the branch */
    struct handler *top_handlers; /* always points at the 
					   * handler on top of the stack. */
    int failure; /* zero whilst parse should continue */
    int prune; /* if non-zero, depth within a dead branch */

#ifdef NEED_BOM_HANDLING
    int bom_pos;
#endif

#ifdef HAVE_EXPAT
    XML_Parser parser;
    char *encoding;
#else
    xmlParserCtxtPtr parser;
#endif
    char error[ERR_SIZE];
};

/* The callback handlers */
static void start_element(void *userdata, const ne_xml_char *name, const ne_xml_char **atts);
static void end_element(void *userdata, const ne_xml_char *name);
static void char_data(void *userdata, const ne_xml_char *cdata, int len);
static const char *resolve_nspace(const struct element *elm, 
                                  const char *prefix, size_t pfxlen);

/* Linked list of namespace scopes */
struct namespace {
    ne_xml_char *name;
    ne_xml_char *uri;
    struct namespace *next;
};

#ifdef HAVE_LIBXML

/* Could be const as far as we care, but libxml doesn't want that */
static xmlSAXHandler sax_handler = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    start_element, /* startElement */
    end_element, /* endElement */
    NULL, /* reference */
    char_data, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    NULL, /* xmlParserWarning */
    sax_error, /* xmlParserError */
    sax_error, /* fatal error (never called by libxml2?) */
    NULL, /* getParameterEntity */
    char_data /* cdataBlock */
};

/* empty attributes array to mimic expat behaviour */
static const char *const empty_atts[] = {NULL, NULL};

/* macro for determining the attributes array to pass */
#define PASS_ATTS(atts) (atts ? (const char **)(atts) : empty_atts)

#else

#define PASS_ATTS(atts) ((const char **)(atts))

/* XML declaration callback for expat. */
static void decl_handler(void *userdata,
			 const XML_Char *version, const XML_Char *encoding, 
			 int standalone)
{
    ne_xml_parser *p = userdata;
    if (encoding) p->encoding = ne_strdup(encoding);    
}

#endif /* HAVE_LIBXML */

int ne_xml_currentline(ne_xml_parser *p) 
{
#ifdef HAVE_EXPAT
    return XML_GetCurrentLineNumber(p->parser);
#else
    return p->parser->input->line;
#endif
}

const char *ne_xml_doc_encoding(const ne_xml_parser *p)
{
#ifdef HAVE_LIBXML
    return p->parser->encoding;
#else
    return p->encoding;
#endif
}

/* The first character of the REC-xml-names "NCName" rule excludes
 * "Digit | '.' | '-' | '_' | CombiningChar | Extender"; the XML
 * parser will not enforce this rule in a namespace declaration since
 * it treats the entire attribute name as a REC-xml "Name" rule.  It's
 * too hard to check for all of CombiningChar | Digit | Extender here,
 * but the valid_ncname_ch1 macro catches some of the rest. */

/* Return non-zero if 'ch' is an invalid start character for an NCName: */
#define invalid_ncname_ch1(ch) ((ch) == '\0' || strchr("-.0123456789", (ch)) != NULL)

/* Subversion repositories have been deployed which use property names
 * marshalled as NCNames including a colon character; these should
 * also be rejected but will be allowed for the time being. */
#define invalid_ncname(xn) (invalid_ncname_ch1((xn)[0]))

/* Extract the namespace prefix declarations from 'atts'. */
static int declare_nspaces(ne_xml_parser *p, struct element *elm,
                           const ne_xml_char **atts)
{
    int n;
    
    for (n = 0; atts && atts[n]; n += 2) {
        if (strcmp(atts[n], "xmlns") == 0) {
            /* New default namespace */
            elm->default_ns = ne_strdup(atts[n+1]);
        } else if (strncmp(atts[n], "xmlns:", 6) == 0) {
            struct namespace *ns;
            
            /* Reject some invalid NCNames as namespace prefix, and an
             * empty URI as the namespace URI */
            if (invalid_ncname(atts[n] + 6) || atts[n+1][0] == '\0') {
                ne_snprintf(p->error, ERR_SIZE, 
                            ("XML parse error at line %d: invalid namespace "
                             "declaration"), ne_xml_currentline(p));
                return -1;
            }

            /* New namespace scope */
            ns = ne_calloc(sizeof(*ns));
            ns->next = elm->nspaces;
            elm->nspaces = ns;
            ns->name = ne_strdup(atts[n]+6); /* skip the xmlns= */
            ns->uri = ne_strdup(atts[n+1]);
        }
    }
    
    return 0;
}

/* Expand an XML qualified name, which may include a namespace prefix
 * as well as the local part. */
static int expand_qname(ne_xml_parser *p, struct element *elm,
                        const ne_xml_char *qname)
{
    const ne_xml_char *pfx;

    pfx = strchr(qname, ':');
    if (pfx == NULL) {
        struct element *e = elm;

        /* Find default namespace; guaranteed to terminate as the root
         * element always has default_ns="". */
        while (e->default_ns == NULL)
            e = e->parent;
        
        elm->name = ne_strdup(qname);
        elm->nspace = e->default_ns;
    } else if (invalid_ncname(pfx + 1) || qname == pfx) {
        ne_snprintf(p->error, ERR_SIZE, 
                    _("XML parse error at line %d: invalid element name"), 
                    ne_xml_currentline(p));
        return -1;
    } else {
        const char *uri = resolve_nspace(elm, qname, pfx-qname);

	if (uri) {
	    elm->name = ne_strdup(pfx+1);
            elm->nspace = uri;
	} else {
	    ne_snprintf(p->error, ERR_SIZE, 
                        ("XML parse error at line %d: undeclared namespace prefix"),
                        ne_xml_currentline(p));
	    return -1;
	}
    }
    return 0;
}

/* Called with the start of a new element. */
static void start_element(void *userdata, const ne_xml_char *name,
			  const ne_xml_char **atts) 
{
    ne_xml_parser *p = userdata;
    struct element *elm;
    struct handler *hand;
    int state = NE_XML_DECLINE;

    if (p->failure) return;
    
    if (p->prune) {
        p->prune++;
        return;
    }

    /* Create a new element */
    elm = ne_calloc(sizeof *elm);
    elm->parent = p->current;
    p->current = elm;

    if (declare_nspaces(p, elm, atts) || expand_qname(p, elm, name)) {
        p->failure = 1;
        return;
    }

    /* Find a handler which will accept this element (or abort the parse) */
    for (hand = elm->parent->handler; hand && state == NE_XML_DECLINE;
         hand = hand->next) {
        elm->handler = hand;
        state = hand->startelm_cb(hand->userdata, elm->parent->state,
                                  elm->nspace, elm->name, PASS_ATTS(atts));
    }

    NE_DEBUG(NE_DBG_XML, "XML: start-element (%d, {%s, %s}) => %d\n", 
             elm->parent->state, elm->nspace, elm->name, state);             
    
    if (state > 0)
        elm->state = state;
    else if (state == NE_XML_DECLINE)
        /* prune this branch. */
        p->prune++;
    else /* state < 0 => abort parse  */
        p->failure = state;
}

/* Destroys an element structure. */
static void destroy_element(struct element *elm) 
{
    struct namespace *this_ns, *next_ns;
    ne_free(elm->name);
    /* Free the namespaces */
    this_ns = elm->nspaces;
    while (this_ns != NULL) {
	next_ns = this_ns->next;
	ne_free(this_ns->name);
	ne_free(this_ns->uri);
	ne_free(this_ns);
	this_ns = next_ns;
    }
    if (elm->default_ns)
        ne_free(elm->default_ns);
    ne_free(elm);
}

/* cdata SAX callback */
static void char_data(void *userdata, const ne_xml_char *data, int len) 
{
    ne_xml_parser *p = userdata;
    struct element *elm = p->current;

    if (p->failure || p->prune) return;
    
    if (elm->handler->cdata_cb) {
        p->failure = elm->handler->cdata_cb(elm->handler->userdata, elm->state, data, len);
        NE_DEBUG(NE_DBG_XML, "XML: char-data (%d) returns %d\n", 
                 elm->state, p->failure);
    }        
}

/* Called with the end of an element */
static void end_element(void *userdata, const ne_xml_char *name) 
{
    ne_xml_parser *p = userdata;
    struct element *elm = p->current;

    if (p->failure) return;
	
    if (p->prune) {
        if (p->prune-- > 1) return;
    } else if (elm->handler->endelm_cb) {
        p->failure = elm->handler->endelm_cb(elm->handler->userdata, elm->state,
                                             elm->nspace, elm->name);
        if (p->failure) {
            NE_DEBUG(NE_DBG_XML, "XML: end-element for %d failed with %d.\n", 
                     elm->state, p->failure);
        }
    }
    
    NE_DEBUG(NE_DBG_XML, "XML: end-element (%d, {%s, %s})\n",
             elm->state, elm->nspace, elm->name);

    /* move back up the tree */
    p->current = elm->parent;
    p->prune = 0;

    destroy_element(elm);
}

#if defined(HAVE_EXPAT) && XML_MAJOR_VERSION > 1
/* Stop the parser if an entity declaration is hit. */
static void entity_declaration(void *userData, const XML_Char *entityName,
                              int is_parameter_entity, const XML_Char *value,
                              int value_length, const XML_Char *base,
                              const XML_Char *systemId, const XML_Char *publicId,
                              const XML_Char *notationName)
{
    ne_xml_parser *parser = userData;
    
    NE_DEBUG(NE_DBG_XMLPARSE, "XML: entity declaration [%s]. Failing.\n",
             entityName);

    XML_StopParser(parser->parser, XML_FALSE);
}
#elif defined(HAVE_EXPAT)
/* A noop default_handler. */
static void default_handler(void *userData, const XML_Char *s, int len)
{
}
#endif

/* Find a namespace definition for 'prefix' in given element, where
 * length of prefix is 'pfxlen'.  Returns the URI or NULL. */
static const char *resolve_nspace(const struct element *elm, 
                                  const char *prefix, size_t pfxlen)
{
    const struct element *s;

    /* Search up the tree. */
    for (s = elm; s != NULL; s = s->parent) {
	const struct namespace *ns;
	/* Iterate over defined spaces on this node. */
	for (ns = s->nspaces; ns != NULL; ns = ns->next) {
	    if (strlen(ns->name) == pfxlen && 
		memcmp(ns->name, prefix, pfxlen) == 0)
		return ns->uri;
	}
    }

    return NULL;
}

const char *ne_xml_resolve_nspace(ne_xml_parser *parser, 
                                  const char *prefix, size_t length)
{
    if (prefix) {
        return resolve_nspace(parser->current, prefix, length);
    }
    else {
        struct element *e = parser->current;

        while (e->default_ns == NULL)
            e = e->parent;

        return e->default_ns;
    }
}

ne_xml_parser *ne_xml_create(void) 
{
    ne_xml_parser *p = ne_calloc(sizeof *p);
    /* Placeholder for the root element */
    p->current = p->root = ne_calloc(sizeof *p->root);
    p->root->default_ns = "";
    p->root->state = 0;
    strcpy(p->error, _("Unknown error"));
#ifdef HAVE_EXPAT
    p->parser = XML_ParserCreate(NULL);
    if (p->parser == NULL) {
	abort();
    }
    XML_SetElementHandler(p->parser, start_element, end_element);
    XML_SetCharacterDataHandler(p->parser, char_data);
    XML_SetUserData(p->parser, (void *) p);
    XML_SetXmlDeclHandler(p->parser, decl_handler);

    /* Prevent the "billion laughs" attack against expat by disabling
     * internal entity expansion.  With 2.x, forcibly stop the parser
     * if an entity is declared - this is safer and a more obvious
     * failure mode.  With older versions, installing a noop
     * DefaultHandler means that internal entities will be expanded as
     * the empty string, which is also sufficient to prevent the
     * attack. */
#if XML_MAJOR_VERSION > 1
    XML_SetEntityDeclHandler(p->parser, entity_declaration);
#else
    XML_SetDefaultHandler(p->parser, default_handler);
#endif

#else /* HAVE_LIBXML */
    p->parser = xmlCreatePushParserCtxt(&sax_handler, 
					(void *)p, NULL, 0, NULL);
    if (p->parser == NULL) {
	abort();
    }
#if LIBXML_VERSION < 20602
    p->parser->replaceEntities = 1;
#else
    /* Enable expansion of entities, and disable network access. */
    xmlCtxtUseOptions(p->parser, XML_PARSE_NOENT | XML_PARSE_NONET);
#endif

#endif /* HAVE_LIBXML || HAVE_EXPAT */
    return p;
}

void ne_xml_push_handler(ne_xml_parser *p,
			 ne_xml_startelm_cb *startelm_cb, 
			 ne_xml_cdata_cb *cdata_cb, 
			 ne_xml_endelm_cb *endelm_cb,
			 void *userdata)
{
    struct handler *hand = ne_calloc(sizeof(struct handler));

    hand->startelm_cb = startelm_cb;
    hand->cdata_cb = cdata_cb;
    hand->endelm_cb = endelm_cb;
    hand->userdata = userdata;

    /* If this is the first handler registered, update the
     * base pointer too. */
    if (p->top_handlers == NULL) {
	p->root->handler = hand;
	p->top_handlers = hand;
    } else {
	p->top_handlers->next = hand;
	p->top_handlers = hand;
    }
}

int ne_xml_parse_v(void *userdata, const char *block, size_t len) 
{
    ne_xml_parser *p = userdata;
    return ne_xml_parse(p, (const ne_xml_char *)block, len);
}

#define BOM_UTF8 "\xEF\xBB\xBF" /* UTF-8 BOM */

int ne_xml_parse(ne_xml_parser *p, const char *block, size_t len) 
{
    int ret, flag;
    /* duck out if it's broken */
    if (p->failure) {
	NE_DEBUG(NE_DBG_XMLPARSE, "XML: Failed; ignoring %" NE_FMT_SIZE_T 
                 " bytes.\n", len);
	return p->failure;
    }
    if (len == 0) {
	flag = -1;
	block = "";
	NE_DEBUG(NE_DBG_XMLPARSE, "XML: End of document.\n");
    } else {	
	NE_DEBUG(NE_DBG_XMLPARSE, "XML: Parsing %" NE_FMT_SIZE_T " bytes.\n", len);
	flag = 0;
    }

#ifdef NEED_BOM_HANDLING
    if (p->bom_pos < 3) {
        NE_DEBUG(NE_DBG_XMLPARSE, "Checking for UTF-8 BOM.\n");
        while (len > 0 && p->bom_pos < 3 && 
               block[0] == BOM_UTF8[p->bom_pos]) {
            block++;
            len--;
            p->bom_pos++;
        }
        if (len == 0)
            return 0;
        if (p->bom_pos == 0) {
            p->bom_pos = 3; /* no BOM */
        } else if (p->bom_pos > 0 && p->bom_pos < 3) {
            strcpy(p->error, _("Invalid Byte Order Mark"));
            return p->failure = 1;
        }
    }
#endif

    /* Note, don't write a parser error if p->failure, since an error
     * will already have been written in that case. */
#ifdef HAVE_EXPAT
    ret = XML_Parse(p->parser, block, len, flag);
    NE_DEBUG(NE_DBG_XMLPARSE, "XML: XML_Parse returned %d\n", ret);
    if (ret == 0 && p->failure == 0) {
	ne_snprintf(p->error, ERR_SIZE,
		    "XML parse error at line %" NE_FMT_XML_SIZE ": %s", 
		    XML_GetCurrentLineNumber(p->parser),
		    XML_ErrorString(XML_GetErrorCode(p->parser)));
	p->failure = 1;
        NE_DEBUG(NE_DBG_XMLPARSE, "XML: Parse error: %s\n", p->error);
    }
#else
    ret = xmlParseChunk(p->parser, block, len, flag);
    NE_DEBUG(NE_DBG_XMLPARSE, "XML: xmlParseChunk returned %d\n", ret);
    /* Parse errors are normally caught by the sax_error() callback,
     * which clears p->valid. */
    if (p->parser->errNo && p->failure == 0) {
	ne_snprintf(p->error, ERR_SIZE, "XML parse error at line %d", 
		    ne_xml_currentline(p));
	p->failure = 1;
        NE_DEBUG(NE_DBG_XMLPARSE, "XML: Parse error: %s\n", p->error);
    }
#endif
    return p->failure;
}

int ne_xml_failed(ne_xml_parser *p)
{
    return p->failure;
}

void ne_xml_destroy(ne_xml_parser *p) 
{
    struct element *elm, *parent;
    struct handler *hand, *next;

    /* Free up the handlers on the stack: the root element has the
     * pointer to the base of the handler stack. */
    for (hand = p->root->handler; hand!=NULL; hand=next) {
	next = hand->next;
	ne_free(hand);
    }

    /* Clean up remaining elements */
    for (elm = p->current; elm != p->root; elm = parent) {
	parent = elm->parent;
	destroy_element(elm);
    }

    /* free root element */
    ne_free(p->root);

#ifdef HAVE_EXPAT
    XML_ParserFree(p->parser);
    if (p->encoding) ne_free(p->encoding);
#else
    xmlFreeParserCtxt(p->parser);
#endif

    ne_free(p);
}

void ne_xml_set_error(ne_xml_parser *p, const char *msg)
{
    ne_snprintf(p->error, ERR_SIZE, "%s", msg);
}

#ifdef HAVE_LIBXML
static void sax_error(void *ctx, const char *msg, ...)
{
    ne_xml_parser *p = ctx;
    va_list ap;
    char buf[1024];

    va_start(ap, msg);
    ne_vsnprintf(buf, 1024, msg, ap);
    va_end(ap);

    if (p->failure == 0) {
        ne_snprintf(p->error, ERR_SIZE, 
                    _("XML parse error at line %d: %s"),
                    p->parser->input->line, buf);
        p->failure = 1;
    }
}
#endif

const char *ne_xml_get_error(ne_xml_parser *p)
{
    return p->error;
}

const char *
ne_xml_get_attr(ne_xml_parser *p, const char **attrs, 
		const char *nspace, const char *name)
{
    int n;

    for (n = 0; attrs[n] != NULL; n += 2) {
	char *pnt = strchr(attrs[n], ':');

	if (!nspace && !pnt && strcmp(attrs[n], name) == 0) {
	    return attrs[n+1];
	} else if (nspace && pnt) {
	    /* If a namespace is given, and the local part matches,
	     * then resolve the namespace and compare that too. */
	    if (strcmp(pnt + 1, name) == 0) {
		const char *uri = resolve_nspace(p->current, 
						 attrs[n], pnt - attrs[n]);
		if (uri && strcmp(uri, nspace) == 0)
		    return attrs[n+1];
	    }
	}
    }

    return NULL;
}

int ne_xml_mapid(const struct ne_xml_idmap map[], size_t maplen,
                 const char *nspace, const char *name)
{
    size_t n;
    
    for (n = 0; n < maplen; n++)
        if (strcmp(name, map[n].name) == 0 &&
            strcmp(nspace, map[n].nspace) == 0)
            return map[n].id;
    
    return 0;
}
