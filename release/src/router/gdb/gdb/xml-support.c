/* Helper routines for parsing XML using Expat.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "gdbcmd.h"
#include "exceptions.h"
#include "xml-support.h"

#include "gdb_string.h"
#include "safe-ctype.h"

/* Debugging flag.  */
static int debug_xml;

/* The contents of this file are only useful if XML support is
   available.  */
#ifdef HAVE_LIBEXPAT

#include "gdb_expat.h"

/* The maximum depth of <xi:include> nesting.  No need to be miserly,
   we just want to avoid running out of stack on loops.  */
#define MAX_XINCLUDE_DEPTH 30

/* Simplified XML parser infrastructure.  */

/* A parsing level -- used to keep track of the current element
   nesting.  */
struct scope_level
{
  /* Elements we allow at this level.  */
  const struct gdb_xml_element *elements;

  /* The element which we are within.  */
  const struct gdb_xml_element *element;

  /* Mask of which elements we've seen at this level (used for
     optional and repeatable checking).  */
  unsigned int seen;

  /* Body text accumulation.  */
  struct obstack *body;
};
typedef struct scope_level scope_level_s;
DEF_VEC_O(scope_level_s);

/* The parser itself, and our additional state.  */
struct gdb_xml_parser
{
  XML_Parser expat_parser;	/* The underlying expat parser.  */

  const char *name;		/* Name of this parser.  */
  void *user_data;		/* The user's callback data, for handlers.  */

  VEC(scope_level_s) *scopes;	/* Scoping stack.  */

  struct gdb_exception error;	/* A thrown error, if any.  */
  int last_line;		/* The line of the thrown error, or 0.  */

  const char *dtd_name;		/* The name of the expected / default DTD,
				   if specified.  */
  int is_xinclude;		/* Are we the special <xi:include> parser?  */
};

/* Process some body text.  We accumulate the text for later use; it's
   wrong to do anything with it immediately, because a single block of
   text might be broken up into multiple calls to this function.  */

static void
gdb_xml_body_text (void *data, const XML_Char *text, int length)
{
  struct gdb_xml_parser *parser = data;
  struct scope_level *scope = VEC_last (scope_level_s, parser->scopes);

  if (parser->error.reason < 0)
    return;

  if (scope->body == NULL)
    {
      scope->body = XZALLOC (struct obstack);
      obstack_init (scope->body);
    }

  obstack_grow (scope->body, text, length);
}

/* Issue a debugging message from one of PARSER's handlers.  */

void
gdb_xml_debug (struct gdb_xml_parser *parser, const char *format, ...)
{
  int line = XML_GetCurrentLineNumber (parser->expat_parser);
  va_list ap;
  char *message;

  if (!debug_xml)
    return;

  va_start (ap, format);
  message = xstrvprintf (format, ap);
  if (line)
    fprintf_unfiltered (gdb_stderr, "%s (line %d): %s\n",
			parser->name, line, message);
  else
    fprintf_unfiltered (gdb_stderr, "%s: %s\n",
			parser->name, message);
  xfree (message);
}

/* Issue an error message from one of PARSER's handlers, and stop
   parsing.  */

void
gdb_xml_error (struct gdb_xml_parser *parser, const char *format, ...)
{
  int line = XML_GetCurrentLineNumber (parser->expat_parser);
  va_list ap;

  parser->last_line = line;
  va_start (ap, format);
  throw_verror (XML_PARSE_ERROR, format, ap);
}

/* Clean up a vector of parsed attribute values.  */

static void
gdb_xml_values_cleanup (void *data)
{
  VEC(gdb_xml_value_s) **values = data;
  struct gdb_xml_value *value;
  int ix;

  for (ix = 0; VEC_iterate (gdb_xml_value_s, *values, ix, value); ix++)
    xfree (value->value);
  VEC_free (gdb_xml_value_s, *values);
}

/* Handle the start of an element.  DATA is our local XML parser, NAME
   is the element, and ATTRS are the names and values of this
   element's attributes.  */

static void
gdb_xml_start_element (void *data, const XML_Char *name,
		       const XML_Char **attrs)
{
  struct gdb_xml_parser *parser = data;
  struct scope_level *scope;
  struct scope_level new_scope;
  const struct gdb_xml_element *element;
  const struct gdb_xml_attribute *attribute;
  VEC(gdb_xml_value_s) *attributes = NULL;
  unsigned int seen;
  struct cleanup *back_to;

  /* Push an error scope.  If we return or throw an exception before
     filling this in, it will tell us to ignore children of this
     element.  */
  VEC_reserve (scope_level_s, parser->scopes, 1);
  scope = VEC_last (scope_level_s, parser->scopes);
  memset (&new_scope, 0, sizeof (new_scope));
  VEC_quick_push (scope_level_s, parser->scopes, &new_scope);

  gdb_xml_debug (parser, _("Entering element <%s>"), name);

  /* Find this element in the list of the current scope's allowed
     children.  Record that we've seen it.  */

  seen = 1;
  for (element = scope->elements; element && element->name;
       element++, seen <<= 1)
    if (strcmp (element->name, name) == 0)
      break;

  if (element == NULL || element->name == NULL)
    {
      /* If we're working on XInclude, <xi:include> can be the child
	 of absolutely anything.  Copy the previous scope's element
	 list into the new scope even if there was no match.  */
      if (parser->is_xinclude)
	{
	  struct scope_level *unknown_scope;

	  XML_DefaultCurrent (parser->expat_parser);

	  unknown_scope = VEC_last (scope_level_s, parser->scopes);
	  unknown_scope->elements = scope->elements;
	  return;
	}

      gdb_xml_debug (parser, _("Element <%s> unknown"), name);
      return;
    }

  if (!(element->flags & GDB_XML_EF_REPEATABLE) && (seen & scope->seen))
    gdb_xml_error (parser, _("Element <%s> only expected once"), name);

  scope->seen |= seen;

  back_to = make_cleanup (gdb_xml_values_cleanup, &attributes);

  for (attribute = element->attributes;
       attribute != NULL && attribute->name != NULL;
       attribute++)
    {
      const char *val = NULL;
      const XML_Char **p;
      void *parsed_value;
      struct gdb_xml_value new_value;

      for (p = attrs; *p != NULL; p += 2)
	if (!strcmp (attribute->name, p[0]))
	  {
	    val = p[1];
	    break;
	  }

      if (*p != NULL && val == NULL)
	{
	  gdb_xml_debug (parser, _("Attribute \"%s\" missing a value"),
			 attribute->name);
	  continue;
	}

      if (*p == NULL && !(attribute->flags & GDB_XML_AF_OPTIONAL))
	{
	  gdb_xml_error (parser, _("Required attribute \"%s\" of "
				   "<%s> not specified"),
			 attribute->name, element->name);
	  continue;
	}

      if (*p == NULL)
	continue;

      gdb_xml_debug (parser, _("Parsing attribute %s=\"%s\""),
		     attribute->name, val);

      if (attribute->handler)
	parsed_value = attribute->handler (parser, attribute, val);
      else
	parsed_value = xstrdup (val);

      new_value.name = attribute->name;
      new_value.value = parsed_value;
      VEC_safe_push (gdb_xml_value_s, attributes, &new_value);
    }

  /* Check for unrecognized attributes.  */
  if (debug_xml)
    {
      const XML_Char **p;

      for (p = attrs; *p != NULL; p += 2)
	{
	  for (attribute = element->attributes;
	       attribute != NULL && attribute->name != NULL;
	       attribute++)
	    if (strcmp (attribute->name, *p) == 0)
	      break;

	  if (attribute == NULL || attribute->name == NULL)
	    gdb_xml_debug (parser, _("Ignoring unknown attribute %s"), *p);
	}
    }

  /* Call the element handler if there is one.  */
  if (element->start_handler)
    element->start_handler (parser, element, parser->user_data, attributes);

  /* Fill in a new scope level.  */
  scope = VEC_last (scope_level_s, parser->scopes);
  scope->element = element;
  scope->elements = element->children;

  do_cleanups (back_to);
}

/* Wrapper for gdb_xml_start_element, to prevent throwing exceptions
   through expat.  */

static void
gdb_xml_start_element_wrapper (void *data, const XML_Char *name,
			       const XML_Char **attrs)
{
  struct gdb_xml_parser *parser = data;
  volatile struct gdb_exception ex;

  if (parser->error.reason < 0)
    return;

  TRY_CATCH (ex, RETURN_MASK_ALL)
    {
      gdb_xml_start_element (data, name, attrs);
    }
  if (ex.reason < 0)
    {
      parser->error = ex;
#ifdef HAVE_XML_STOPPARSER
      XML_StopParser (parser->expat_parser, XML_FALSE);
#endif
    }
}

/* Handle the end of an element.  DATA is our local XML parser, and
   NAME is the current element.  */

static void
gdb_xml_end_element (void *data, const XML_Char *name)
{
  struct gdb_xml_parser *parser = data;
  struct scope_level *scope = VEC_last (scope_level_s, parser->scopes);
  const struct gdb_xml_element *element;
  unsigned int seen;

  gdb_xml_debug (parser, _("Leaving element <%s>"), name);

  for (element = scope->elements, seen = 1;
       element != NULL && element->name != NULL;
       element++, seen <<= 1)
    if ((scope->seen & seen) == 0
	&& (element->flags & GDB_XML_EF_OPTIONAL) == 0)
      gdb_xml_error (parser, _("Required element <%s> is missing"),
		     element->name);

  /* Call the element processor. */
  if (scope->element != NULL && scope->element->end_handler)
    {
      char *body;

      if (scope->body == NULL)
	body = "";
      else
	{
	  int length;

	  length = obstack_object_size (scope->body);
	  obstack_1grow (scope->body, '\0');
	  body = obstack_finish (scope->body);

	  /* Strip leading and trailing whitespace.  */
	  while (length > 0 && ISSPACE (body[length-1]))
	    body[--length] = '\0';
	  while (*body && ISSPACE (*body))
	    body++;
	}

      scope->element->end_handler (parser, scope->element, parser->user_data,
				   body);
    }
  else if (scope->element == NULL)
    XML_DefaultCurrent (parser->expat_parser);

  /* Pop the scope level.  */
  if (scope->body)
    {
      obstack_free (scope->body, NULL);
      xfree (scope->body);
    }
  VEC_pop (scope_level_s, parser->scopes);
}

/* Wrapper for gdb_xml_end_element, to prevent throwing exceptions
   through expat.  */

static void
gdb_xml_end_element_wrapper (void *data, const XML_Char *name)
{
  struct gdb_xml_parser *parser = data;
  volatile struct gdb_exception ex;

  if (parser->error.reason < 0)
    return;

  TRY_CATCH (ex, RETURN_MASK_ALL)
    {
      gdb_xml_end_element (data, name);
    }
  if (ex.reason < 0)
    {
      parser->error = ex;
#ifdef HAVE_XML_STOPPARSER
      XML_StopParser (parser->expat_parser, XML_FALSE);
#endif
    }
}

/* Free a parser and all its associated state.  */

static void
gdb_xml_cleanup (void *arg)
{
  struct gdb_xml_parser *parser = arg;
  struct scope_level *scope;
  int ix;

  XML_ParserFree (parser->expat_parser);

  /* Clean up the scopes.  */
  for (ix = 0; VEC_iterate (scope_level_s, parser->scopes, ix, scope); ix++)
    if (scope->body)
      {
	obstack_free (scope->body, NULL);
	xfree (scope->body);
      }
  VEC_free (scope_level_s, parser->scopes);

  xfree (parser);
}

/* Initialize and return a parser.  Register a cleanup to destroy the
   parser.  */

struct gdb_xml_parser *
gdb_xml_create_parser_and_cleanup (const char *name,
				   const struct gdb_xml_element *elements,
				   void *user_data)
{
  struct gdb_xml_parser *parser;
  struct scope_level start_scope;

  /* Initialize the parser.  */
  parser = XZALLOC (struct gdb_xml_parser);
  parser->expat_parser = XML_ParserCreateNS (NULL, '!');
  if (parser->expat_parser == NULL)
    {
      xfree (parser);
      nomem (0);
    }

  parser->name = name;

  parser->user_data = user_data;
  XML_SetUserData (parser->expat_parser, parser);

  /* Set the callbacks.  */
  XML_SetElementHandler (parser->expat_parser, gdb_xml_start_element_wrapper,
			 gdb_xml_end_element_wrapper);
  XML_SetCharacterDataHandler (parser->expat_parser, gdb_xml_body_text);

  /* Initialize the outer scope.  */
  memset (&start_scope, 0, sizeof (start_scope));
  start_scope.elements = elements;
  VEC_safe_push (scope_level_s, parser->scopes, &start_scope);

  make_cleanup (gdb_xml_cleanup, parser);

  return parser;
}

/* External entity handler.  The only external entities we support
   are those compiled into GDB (we do not fetch entities from the
   target).  */

static int XMLCALL
gdb_xml_fetch_external_entity (XML_Parser expat_parser,
			       const XML_Char *context,
			       const XML_Char *base,
			       const XML_Char *systemId,
			       const XML_Char *publicId)
{
  struct gdb_xml_parser *parser = XML_GetUserData (expat_parser);
  XML_Parser entity_parser;
  const char *text;
  enum XML_Status status;

  if (systemId == NULL)
    {
      text = fetch_xml_builtin (parser->dtd_name);
      if (text == NULL)
	internal_error (__FILE__, __LINE__, "could not locate built-in DTD %s",
			parser->dtd_name);
    }
  else
    {
      text = fetch_xml_builtin (systemId);
      if (text == NULL)
	return XML_STATUS_ERROR;
    }

  entity_parser = XML_ExternalEntityParserCreate (expat_parser, context, NULL);

  /* Don't use our handlers for the contents of the DTD.  Just let expat
     process it.  */
  XML_SetElementHandler (entity_parser, NULL, NULL);
  XML_SetDoctypeDeclHandler (entity_parser, NULL, NULL);
  XML_SetXmlDeclHandler (entity_parser, NULL);
  XML_SetDefaultHandler (entity_parser, NULL);
  XML_SetUserData (entity_parser, NULL);

  status = XML_Parse (entity_parser, text, strlen (text), 1);

  XML_ParserFree (entity_parser);
  return status;
}

/* Associate DTD_NAME, which must be the name of a compiled-in DTD,
   with PARSER.  */

void
gdb_xml_use_dtd (struct gdb_xml_parser *parser, const char *dtd_name)
{
  enum XML_Error err;

  parser->dtd_name = dtd_name;

  XML_SetParamEntityParsing (parser->expat_parser,
			     XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  XML_SetExternalEntityRefHandler (parser->expat_parser,
				   gdb_xml_fetch_external_entity);

  /* Even if no DTD is provided, use the built-in DTD anyway.  */
  err = XML_UseForeignDTD (parser->expat_parser, XML_TRUE);
  if (err != XML_ERROR_NONE)
    internal_error (__FILE__, __LINE__,
		    "XML_UseForeignDTD failed: %s", XML_ErrorString (err));
}

/* Invoke PARSER on BUFFER.  BUFFER is the data to parse, which
   should be NUL-terminated.

   The return value is 0 for success or -1 for error.  It may throw,
   but only if something unexpected goes wrong during parsing; parse
   errors will be caught, warned about, and reported as failure.  */

int
gdb_xml_parse (struct gdb_xml_parser *parser, const char *buffer)
{
  enum XML_Status status;
  const char *error_string;

  gdb_xml_debug (parser, _("Starting:\n%s"), buffer);

  status = XML_Parse (parser->expat_parser, buffer, strlen (buffer), 1);

  if (status == XML_STATUS_OK && parser->error.reason == 0)
    return 0;

  if (parser->error.reason == RETURN_ERROR
      && parser->error.error == XML_PARSE_ERROR)
    {
      gdb_assert (parser->error.message != NULL);
      error_string = parser->error.message;
    }
  else if (status == XML_STATUS_ERROR)
    {
      enum XML_Error err = XML_GetErrorCode (parser->expat_parser);
      error_string = XML_ErrorString (err);
    }
  else
    {
      gdb_assert (parser->error.reason < 0);
      throw_exception (parser->error);
    }

  if (parser->last_line != 0)
    warning (_("while parsing %s (at line %d): %s"), parser->name,
	     parser->last_line, error_string);
  else
    warning (_("while parsing %s: %s"), parser->name, error_string);

  return -1;
}

/* Parse a field VALSTR that we expect to contain an integer value.
   The integer is returned in *VALP.  The string is parsed with an
   equivalent to strtoul.

   Returns 0 for success, -1 for error.  */

static int
xml_parse_unsigned_integer (const char *valstr, ULONGEST *valp)
{
  const char *endptr;
  ULONGEST result;

  if (*valstr == '\0')
    return -1;

  result = strtoulst (valstr, &endptr, 0);
  if (*endptr != '\0')
    return -1;

  *valp = result;
  return 0;
}

/* Parse an integer string into a ULONGEST and return it, or call
   gdb_xml_error if it could not be parsed.  */

ULONGEST
gdb_xml_parse_ulongest (struct gdb_xml_parser *parser, const char *value)
{
  ULONGEST result;

  if (xml_parse_unsigned_integer (value, &result) != 0)
    gdb_xml_error (parser, _("Can't convert \"%s\" to an integer"), value);

  return result;
}

/* Parse an integer attribute into a ULONGEST.  */

void *
gdb_xml_parse_attr_ulongest (struct gdb_xml_parser *parser,
			     const struct gdb_xml_attribute *attribute,
			     const char *value)
{
  ULONGEST result;
  void *ret;

  if (xml_parse_unsigned_integer (value, &result) != 0)
    gdb_xml_error (parser, _("Can't convert %s=\"%s\" to an integer"),
		   attribute->name, value);

  ret = xmalloc (sizeof (result));
  memcpy (ret, &result, sizeof (result));
  return ret;
}

/* A handler_data for yes/no boolean values.  */

const struct gdb_xml_enum gdb_xml_enums_boolean[] = {
  { "yes", 1 },
  { "no", 0 },
  { NULL, 0 }
};

/* Map NAME to VALUE.  A struct gdb_xml_enum * should be saved as the
   value of handler_data when using gdb_xml_parse_attr_enum to parse a
   fixed list of possible strings.  The list is terminated by an entry
   with NAME == NULL.  */

void *
gdb_xml_parse_attr_enum (struct gdb_xml_parser *parser,
			 const struct gdb_xml_attribute *attribute,
			 const char *value)
{
  const struct gdb_xml_enum *enums = attribute->handler_data;
  void *ret;

  for (enums = attribute->handler_data; enums->name != NULL; enums++)
    if (strcasecmp (enums->name, value) == 0)
      break;

  if (enums->name == NULL)
    gdb_xml_error (parser, _("Unknown attribute value %s=\"%s\""),
		 attribute->name, value);

  ret = xmalloc (sizeof (enums->value));
  memcpy (ret, &enums->value, sizeof (enums->value));
  return ret;
}


/* XInclude processing.  This is done as a separate step from actually
   parsing the document, so that we can produce a single combined XML
   document - e.g. to hand to a front end or to simplify comparing two
   documents.  We make extensive use of XML_DefaultCurrent, to pass
   input text directly into the output without reformatting or
   requoting it.

   We output the DOCTYPE declaration for the first document unchanged,
   if present, and discard DOCTYPEs from included documents.  Only the
   one we pass through here is used when we feed the result back to
   expat.  The XInclude standard explicitly does not discuss
   validation of the result; we choose to apply the same DTD applied
   to the outermost document.

   We can not simply include the external DTD subset in the document
   as an internal subset, because <!IGNORE> and <!INCLUDE> are valid
   only in external subsets.  But if we do not pass the DTD into the
   output at all, default values will not be filled in.

   We don't pass through any <?xml> declaration because we generate
   UTF-8, not whatever the input encoding was.  */

struct xinclude_parsing_data
{
  /* The obstack to build the output in.  */
  struct obstack obstack;

  /* A count indicating whether we are in an element whose
     children should not be copied to the output, and if so,
     how deep we are nested.  This is used for anything inside
     an xi:include, and for the DTD.  */
  int skip_depth;

  /* The number of <xi:include> elements currently being processed,
     to detect loops.  */
  int include_depth;

  /* A function to call to obtain additional features, and its
     baton.  */
  xml_fetch_another fetcher;
  void *fetcher_baton;
};

static void
xinclude_start_include (struct gdb_xml_parser *parser,
			const struct gdb_xml_element *element,
			void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  struct xinclude_parsing_data *data = user_data;
  char *href = VEC_index (gdb_xml_value_s, attributes, 0)->value;
  struct cleanup *back_to;
  char *text, *output;
  int ret;

  gdb_xml_debug (parser, _("Processing XInclude of \"%s\""), href);

  if (data->include_depth > MAX_XINCLUDE_DEPTH)
    gdb_xml_error (parser, _("Maximum XInclude depth (%d) exceeded"),
		   MAX_XINCLUDE_DEPTH);

  text = data->fetcher (href, data->fetcher_baton);
  if (text == NULL)
    gdb_xml_error (parser, _("Could not load XML document \"%s\""), href);
  back_to = make_cleanup (xfree, text);

  output = xml_process_xincludes (parser->name, text, data->fetcher,
				  data->fetcher_baton,
				  data->include_depth + 1);
  if (output == NULL)
    gdb_xml_error (parser, _("Parsing \"%s\" failed"), href);

  obstack_grow (&data->obstack, output, strlen (output));
  xfree (output);

  do_cleanups (back_to);

  data->skip_depth++;
}

static void
xinclude_end_include (struct gdb_xml_parser *parser,
		      const struct gdb_xml_element *element,
		      void *user_data, const char *body_text)
{
  struct xinclude_parsing_data *data = user_data;

  data->skip_depth--;
}

static void XMLCALL
xml_xinclude_default (void *data_, const XML_Char *s, int len)
{
  struct gdb_xml_parser *parser = data_;
  struct xinclude_parsing_data *data = parser->user_data;

  /* If we are inside of e.g. xi:include or the DTD, don't save this
     string.  */
  if (data->skip_depth)
    return;

  /* Otherwise just add it to the end of the document we're building
     up.  */
  obstack_grow (&data->obstack, s, len);
}

static void XMLCALL
xml_xinclude_start_doctype (void *data_, const XML_Char *doctypeName,
			    const XML_Char *sysid, const XML_Char *pubid,
			    int has_internal_subset)
{
  struct gdb_xml_parser *parser = data_;
  struct xinclude_parsing_data *data = parser->user_data;

  /* Don't print out the doctype, or the contents of the DTD internal
     subset, if any.  */
  data->skip_depth++;
}

static void XMLCALL
xml_xinclude_end_doctype (void *data_)
{
  struct gdb_xml_parser *parser = data_;
  struct xinclude_parsing_data *data = parser->user_data;

  data->skip_depth--;
}

static void XMLCALL
xml_xinclude_xml_decl (void *data_, const XML_Char *version,
		       const XML_Char *encoding, int standalone)
{
  /* Do nothing - this function prevents the default handler from
     being called, thus suppressing the XML declaration from the
     output.  */
}

static void
xml_xinclude_cleanup (void *data_)
{
  struct xinclude_parsing_data *data = data_;

  obstack_free (&data->obstack, NULL);
  xfree (data);
}

const struct gdb_xml_attribute xinclude_attributes[] = {
  { "href", GDB_XML_AF_NONE, NULL, NULL },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element xinclude_elements[] = {
  { "http://www.w3.org/2001/XInclude!include", xinclude_attributes, NULL,
    GDB_XML_EF_OPTIONAL | GDB_XML_EF_REPEATABLE,
    xinclude_start_include, xinclude_end_include },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

/* The main entry point for <xi:include> processing.  */

char *
xml_process_xincludes (const char *name, const char *text,
		       xml_fetch_another fetcher, void *fetcher_baton,
		       int depth)
{
  enum XML_Error err;
  struct gdb_xml_parser *parser;
  struct xinclude_parsing_data *data;
  struct cleanup *back_to;
  char *result = NULL;

  data = XZALLOC (struct xinclude_parsing_data);
  obstack_init (&data->obstack);
  back_to = make_cleanup (xml_xinclude_cleanup, data);

  parser = gdb_xml_create_parser_and_cleanup (name, xinclude_elements, data);
  parser->is_xinclude = 1;

  data->include_depth = depth;
  data->fetcher = fetcher;
  data->fetcher_baton = fetcher_baton;

  XML_SetCharacterDataHandler (parser->expat_parser, NULL);
  XML_SetDefaultHandler (parser->expat_parser, xml_xinclude_default);

  /* Always discard the XML version declarations; the only important
     thing this provides is encoding, and our result will have been
     converted to UTF-8.  */
  XML_SetXmlDeclHandler (parser->expat_parser, xml_xinclude_xml_decl);

  if (depth > 0)
    /* Discard the doctype for included documents.  */
    XML_SetDoctypeDeclHandler (parser->expat_parser,
			       xml_xinclude_start_doctype,
			       xml_xinclude_end_doctype);

  gdb_xml_use_dtd (parser, "xinclude.dtd");

  if (gdb_xml_parse (parser, text) == 0)
    {
      obstack_1grow (&data->obstack, '\0');
      result = xstrdup (obstack_finish (&data->obstack));

      if (depth == 0)
	gdb_xml_debug (parser, _("XInclude processing succeeded."));
    }
  else
    result = NULL;

  do_cleanups (back_to);
  return result;
}
#endif /* HAVE_LIBEXPAT */


/* Return an XML document which was compiled into GDB, from
   the given FILENAME, or NULL if the file was not compiled in.  */

const char *
fetch_xml_builtin (const char *filename)
{
  const char *(*p)[2];

  for (p = xml_builtin; (*p)[0]; p++)
    if (strcmp ((*p)[0], filename) == 0)
      return (*p)[1];

  return NULL;
}

/* A to_xfer_partial helper function which reads XML files which were
   compiled into GDB.  The target may call this function from its own
   to_xfer_partial handler, after converting object and annex to the
   appropriate filename.  */

LONGEST
xml_builtin_xfer_partial (const char *filename,
			  gdb_byte *readbuf, const gdb_byte *writebuf,
			  ULONGEST offset, LONGEST len)
{
  const char *buf;
  LONGEST len_avail;

  gdb_assert (readbuf != NULL && writebuf == NULL);
  gdb_assert (filename != NULL);

  buf = fetch_xml_builtin (filename);
  if (buf == NULL)
    return -1;

  len_avail = strlen (buf);
  if (offset >= len_avail)
    return 0;

  if (len > len_avail - offset)
    len = len_avail - offset;
  memcpy (readbuf, buf + offset, len);
  return len;
}


static void
show_debug_xml (struct ui_file *file, int from_tty,
		struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("XML debugging is %s.\n"), value);
}

/* Return a malloc allocated string with special characters from TEXT
   replaced by entity references.  */

char *
xml_escape_text (const char *text)
{
  char *result;
  int i, special;

  /* Compute the length of the result.  */
  for (i = 0, special = 0; text[i] != '\0'; i++)
    switch (text[i])
      {
      case '\'':
      case '\"':
	special += 5;
	break;
      case '&':
	special += 4;
	break;
      case '<':
      case '>':
	special += 3;
	break;
      default:
	break;
      }

  /* Expand the result.  */
  result = xmalloc (i + special + 1);
  for (i = 0, special = 0; text[i] != '\0'; i++)
    switch (text[i])
      {
      case '\'':
	strcpy (result + i + special, "&apos;");
	special += 5;
	break;
      case '\"':
	strcpy (result + i + special, "&quot;");
	special += 5;
	break;
      case '&':
	strcpy (result + i + special, "&amp;");
	special += 4;
	break;
      case '<':
	strcpy (result + i + special, "&lt;");
	special += 3;
	break;
      case '>':
	strcpy (result + i + special, "&gt;");
	special += 3;
	break;
      default:
	result[i + special] = text[i];
	break;
      }
  result[i + special] = '\0';

  return result;
}

void _initialize_xml_support (void);

void
_initialize_xml_support (void)
{
  add_setshow_boolean_cmd ("xml", class_maintenance, &debug_xml,
			   _("Set XML parser debugging."),
			   _("Show XML parser debugging."),
			   _("When set, debugging messages for XML parsers "
			     "are displayed."),
			   NULL, show_debug_xml,
			   &setdebuglist, &showdebuglist);
}
