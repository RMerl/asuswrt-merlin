#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "err.h"
#include "query.h"

static const query_field_t* find_field(const char* name, 
				       const query_field_t* fields);
static int arith_query(query_node_t* query, void* target);
static int string_query(query_node_t* query, void* target);

static query_node_t*	match_specifier(const char* query,
					const char** cursor,
					const query_field_t* fields);
static query_node_t*	group_match(const char* query,
				    const char** cursor,
				    const query_field_t* fields);
static query_node_t*	single_match(const char* query,
				     const char** cursor,
				     const query_field_t* fields);
static int		get_field_name(const char** pcursor,
				       const char* query, 
				       char* name, 
				       int len);
static int		get_opcode(const char** pcursor,
				   const char* query, 
				   char* name, 
				   int len);
static query_node_t*	match_number(const query_field_t* field, 
				     char not, char opcode,
				     const char** pcursor, 
				     const char* query);
static query_node_t*	match_string(const query_field_t* field, 
				     char not, char opcode,
				     const char** pcursor, 
				     const char* query);
char*			query_unescape(const char* query);

query_node_t* query_build(const char* query, const query_field_t* fields)
{
    query_node_t*	left = 0;
    char*		raw = query_unescape(query);
    const char*		cursor = raw;
    query_node_t*	right = 0;
    query_type_t	join;

    if(0 == (left = match_specifier(query, &cursor, fields)))
	goto error;

    while(*cursor)
    {
	query_node_t*	con;

	switch(*cursor)
	{
	case '+':
	case ' ':	join = qot_and;		break;
	case ',':	join = qot_or;		break;
	default:
	    DPRINTF(E_LOG,L_QRY, "Illegal character '%c' (0%o) at index %d: %s\n",
		    *cursor, *cursor, cursor - raw, raw);
	    goto error;
	}

	cursor++;

	if(0 == (right = match_specifier(raw, &cursor, fields)))
	    goto error;

	con = (query_node_t*) calloc(1, sizeof(*con));
	con->type = join;
	con->left.node = left;
	con->right.node = right;

	left = con;
    }

    if(query != raw)
	free(raw);

    return left;

 error:
    if(left != 0)
	query_free(left);
    if(raw != query)
	free(raw);

    return NULL;
}

static query_node_t*	match_specifier(const char* query,
					const char** cursor,
					const query_field_t* fields)
{
    switch(**cursor)
    {
    case '\'':		return single_match(query, cursor, fields);
    case '(':		return group_match(query, cursor, fields);
    }

    DPRINTF(E_LOG,L_QRY,"Illegal character '%c' (0%o) at index %d: %s\n",
	    **cursor, **cursor, *cursor - query, query);
    return NULL;
}

static query_node_t*	group_match(const char* query,
				    const char** pcursor,
				    const query_field_t* fields)
{
    query_node_t*	left = 0;
    query_node_t*	right = 0;
    query_node_t*	join = 0;
    query_type_t	opcode;
    const char*		cursor = *pcursor;

    /* skip the opening ')' */
    ++cursor;

    if(0 == (left = single_match(query, &cursor, fields)))
	return NULL;

    switch(*cursor)
    {
    case '+':
    case ' ':
	opcode = qot_and;
	break;

    case ',':
	opcode = qot_or;
	break;

    default:
	DPRINTF(E_LOG,L_QRY,"Illegal character '%c' (0%o) at index %d: %s\n",
		*cursor, *cursor, cursor - query, query);
	goto error;
    }

    if(0 == (right = single_match(query, &cursor, fields)))
	goto error;

    if(*cursor != ')')
    {
	DPRINTF(E_LOG,L_QRY,"Illegal character '%c' (0%o) at index %d: %s\n",
		*cursor, *cursor, cursor - query, query);
	goto error;
    }
	
    *pcursor = cursor + 1;

    join = (query_node_t*) calloc(1, sizeof(*join));
    join->type = opcode;
    join->left.node = left;
    join->right.node = right;

    return join;

 error:
    if(0 != left)
	query_free(left);
    if(0 != right)
	query_free(right);

    return 0;
}

static query_node_t*	single_match(const char* query,
				     const char** pcursor,
				     const query_field_t* fields)
{
    char			fname[64];
    const query_field_t*	field;
    char			not = 0;
    char			op = 0;
    query_node_t*		node = 0;

    /* skip opening ' */
    (*pcursor)++;

    /* collect the field name */
    if(!get_field_name(pcursor, query, fname, sizeof(fname)))
	return NULL;

    if(**pcursor == '!')
    {
	not = '!';
	++(*pcursor);
    }

    if(strchr(":+-", **pcursor))
    {
	op = **pcursor;
	++(*pcursor);
    }
    else
    {
	DPRINTF(E_LOG,L_QRY,"Illegal Operator: %c (0%o) at index %d: %s\n",
		**pcursor, **pcursor, *pcursor - query, query);
	return NULL;
    }

    if(0 == (field = find_field(fname, fields)))
    {
	DPRINTF(E_LOG,L_QRY,"Unknown field: %s\n", fname);
	return NULL;
    }

    switch(field->type)
    {
    case qft_i32:
    case qft_i64:
	node = match_number(field, not, op, pcursor, query);
	break;

    case qft_string:
	node = match_string(field, not, op, pcursor, query);
	break;

    default:
	DPRINTF(E_LOG,L_QRY,"Invalid field type: %d\n", field->type);
	break;
    }

    if(**pcursor != '\'')
    {
	DPRINTF(E_LOG,L_QRY,"Illegal Character: %c (0%o) index %d: %s\n",
		**pcursor, **pcursor, *pcursor - query, query);
	query_free(node);
	node = 0;
    }
    else
	++(*pcursor);

    return node;
}

static int		get_field_name(const char** pcursor,
				       const char* query, 
				       char* name, 
				       int len)
{
    const char*	cursor = *pcursor;

    if(!isalpha(*cursor))
	return 0;

    while(isalpha(*cursor) || *cursor == '.')
    {
	if(--len <= 0)
	{
	    DPRINTF(E_LOG,L_QRY,"token length exceeded at offset %d: %s\n",
		    cursor - query, query);
	    return 0;
	}

	*name++ = *cursor++;
    }

    *pcursor = cursor;

    *name = 0;

    return 1;
}

static query_node_t*	match_number(const query_field_t* field, 
				     char not, char opcode,
				     const char** pcursor, 
				     const char* query)
{
    query_node_t*	node = (query_node_t*) calloc(1, sizeof(*node));

    switch(opcode)
    {
    case ':':
	node->type = not ? qot_ne : qot_eq;
	break;
    case '+':
    case ' ':
	node->type = not ? qot_le : qot_gt;
	break;
    case '-':
	node->type = not ? qot_ge : qot_lt;
	break;
    }

    node->left.field = field;

    switch(field->type)
    {
    case qft_i32:
	node->right.i32 = strtol(*pcursor, (char**) pcursor, 10);
	break;
    case qft_i64:
	node->right.i64 = strtoll(*pcursor, (char**) pcursor, 10);
	break;
    }

    if(**pcursor != '\'')
    {
	DPRINTF(E_LOG,L_QRY,"Illegal char in number '%c' (0%o) at index %d: %s\n",
		**pcursor, **pcursor, *pcursor - query, query);
	free(node);
	return 0;
    }

    return node;
}

static query_node_t*	match_string(const query_field_t* field, 
				     char not, char opcode,
				     const char** pcursor, 
				     const char* query)
{
    char		match[64];
    char*		dst = match;
    int			left = sizeof(match);
    const char*		cursor = *pcursor;
    query_type_t	op = qot_is;
    query_node_t*	node;

    if(opcode != ':')
    {
	DPRINTF(E_LOG,L_QRY,"Illegal operation on string: %c at index %d: %s\n",
		opcode, cursor - query - 1);
	return NULL;
    }

    if(*cursor == '*')
    {
	op = qot_ends;
	cursor++;
    }

    while(*cursor && *cursor != '\'')
    {
	if(--left == 0)
	{
	    DPRINTF(E_LOG,L_QRY,"string too long at index %d: %s\n",
		    cursor - query, query);
	    return NULL;
	}

	if(*cursor == '\\')
	{
	    switch(*++cursor)
	    {
	    case '*':
	    case '\'':
	    case '\\':
		*dst++ = *cursor++;
		break;
	    default:
		DPRINTF(E_LOG,L_QRY,"Illegal escape: %c (0%o) at index %d: %s\n",
			*cursor, *cursor, cursor - query, query);
		return NULL;
	    }
	}
	else
	    *dst++ = *cursor++;
    }

    if(dst[-1] == '*')
    {
	op = (op == qot_is) ? qot_begins : qot_contains;
	dst--;
    }

    *dst = 0;

    node = (query_node_t*) calloc(1, sizeof(*node));
    node->type = op;
    node->left.field = field;
    node->right.str = strdup(match);

    *pcursor = cursor;

    return node;
}

int query_test(query_node_t* query, void* target)
{
    switch(query->type)
    {
	/* conjunction */
    case qot_and:
	return (query_test(query->left.node, target) &&
		query_test(query->right.node, target));

    case qot_or:
	return (query_test(query->left.node, target) ||
		query_test(query->right.node, target));

	/* negation */
    case qot_not:	
	return !query_test(query->left.node, target);

	/* arithmetic */
    case qot_eq:
    case qot_ne:
    case qot_le:
    case qot_lt:
    case qot_ge:
    case qot_gt:
	return arith_query(query, target);

	/* string */
    case qot_is:
    case qot_begins:
    case qot_ends:
    case qot_contains:
	return string_query(query, target);
	break;

	/* constants */
    case qot_const:
	return query->left.constant;
    }
}

void query_free(query_node_t* query)
{
    if(0 != query)
    {
	switch(query->type)
	{
	    /* conjunction */
	case qot_and:
	case qot_or:
	    query_free(query->left.node);
	    query_free(query->right.node);
	    break;

	    /* negation */
	case qot_not:
	    query_free(query->left.node);
	    break;

	    /* arithmetic */
	case qot_eq:
	case qot_ne:
	case qot_le:
	case qot_lt:
	case qot_ge:
	case qot_gt:
	    break;

	    /* string */
	case qot_is:
	case qot_begins:
	case qot_ends:
	case qot_contains:
	    free(query->right.str);
	    break;

	    /* constants */
	case qot_const:
	    break;
	    
	default:
	    DPRINTF(E_LOG,L_QRY,"Illegal query type: %d\n", query->type);
	    break;
	}

	free(query);
    }
}

static const query_field_t* find_field(const char* name, const query_field_t* fields)
{
    while(fields->name && strcasecmp(fields->name, name))
	fields++;

    if(fields->name == 0)
    {
	DPRINTF(E_LOG,L_QRY,"Illegal query field: %s\n", name);
	return NULL;
    }

    return fields;
}

static int arith_query(query_node_t* query, void* target)
{
    const query_field_t*	field = query->left.field;

    switch(field->type)
    {
    case qft_i32:
	{
	    int		tv = * (int*) ((size_t) target + field->offset);

	    tv -= query->right.i32;

	    switch(query->type)
	    {
	    case qot_eq:	return tv == 0;
	    case qot_ne:	return tv != 0;
	    case qot_le:	return tv <= 0;
	    case qot_lt:	return tv < 0;
	    case qot_ge:	return tv >= 0;
	    case qot_gt:	return tv > 0;
	    default:
		DPRINTF(E_LOG,L_QRY,"illegal query type: %d\n", query->type);
		break;
	    }
	}
	break;

    case qft_i64:
	{
	    long long tv = * (long long*) ((size_t) target + field->offset);

	    tv -= query->right.i32;

	    switch(query->type)
	    {
	    case qot_eq:	return tv == 0;
	    case qot_ne:	return tv != 0;
	    case qot_le:	return tv <= 0;
	    case qot_lt:	return tv < 0;
	    case qot_ge:	return tv >= 0;
	    case qot_gt:	return tv > 0;
	    default:
		DPRINTF(E_LOG,L_QRY,"illegal query type: %d\n", query->type);
		break;
	    }
	}
	break;

    default:
	DPRINTF(E_LOG,L_QRY,"illegal field type: %d\n", field->type);
	break;
    }
    
    return 0;
}

static int string_query(query_node_t* query, void* target)
{
    const query_field_t*	field = query->left.field;
    const char*		ts;

    if(field->type != qft_string)
    {
	DPRINTF(E_LOG,L_QRY,"illegal field type: %d\n", field->type);
	return 0;
    }

    ts = * (const char**) ((size_t) target + field->offset);

    if(0 == ts)
	return strlen(query->right.str) == 0;
	
    switch(query->type)
    {
    case qot_is:
	return !strcasecmp(query->right.str, ts);

    case qot_begins:
	return !strncasecmp(query->right.str, ts, strlen(query->right.str));

    case qot_ends:
	{
	    int start = strlen(ts) - strlen(query->right.str);

	    if(start < 0)
		return 0;

	    return !strcasecmp(query->right.str, ts + start);
	}
	
    case qot_contains:
	return (int) strcasestr(ts, query->right.str); /* returns null if not found */

    default:
	DPRINTF(E_LOG,L_QRY,"Illegal query type: %d\n", query->type);
	break;
    }

    return 0;
}

void query_dump(FILE* fp, query_node_t* query, int depth)
{
    static const char* labels[] = {
	"NOP",
	"and",
	"or",
	"not",
	"==",
	"!=",
	"<=",
	"<",
	">=",
	">",
	"eq",
	"beginswith",
	"endwith",
	"contains",
	"constant"
    };

#ifndef DEBUG
    return;
#endif

    switch(query->type)
    {
    case qot_and:
    case qot_or:
	fprintf(fp, "%*s(%s\n", depth, "", labels[query->type]);
	query_dump(fp, query->left.node, depth + 4);
	query_dump(fp, query->right.node, depth + 4);
	fprintf(fp, "%*s)\n", depth, "");
	break;

    case qot_not:
	fprintf(fp, "%*s(not\n", depth, "");
	query_dump(fp, query->left.node, depth + 4);
	fprintf(fp, "%*s)\n", depth, "");
	break;

	/* arithmetic */
    case qot_eq:
    case qot_ne:
    case qot_le:
    case qot_lt:
    case qot_ge:
    case qot_gt:
	if(query->left.field->type == qft_i32)
	    fprintf(fp, "%*s(%s %s %d)\n",
		    depth, "", labels[query->type],
		    query->left.field->name, query->right.i32);
	else
	    fprintf(fp, "%*s(%s %s %q)\n",
		    depth, "", labels[query->type],
		    query->left.field->name, query->right.i64);
	break;

	/* string */
    case qot_is:
    case qot_begins:
    case qot_ends:
    case qot_contains:
	fprintf(fp, "%*s(%s %s \"%s\")\n",
		depth, "", labels[query->type],
		query->left.field->name, query->right.str);
	break;

	/* constants */
    case qot_const:
	fprintf(fp, "%*s(%s)\n", depth, "", query->left.constant ? "true" : "false");
	break;
    }
    
}

char* query_unescape(const char* src)
{
    char*	copy = malloc(strlen(src) + 1);
    char*	dst = copy;

    while(*src)
    {
	if(*src == '%')
	{
	    int		val = 0;

	    if(*++src)
	    {
		if(isdigit(*src))
		    val = val * 16 + *src - '0';
		else
		    val = val * 16 + tolower(*src) - 'a' + 10;
	    }

	    if(*++src)
	    {
		if(isdigit(*src))
		    val = val * 16 + *src - '0';
		else
		    val = val * 16 + tolower(*src) - 'a' + 10;
	    }

	    src++;
	    *dst++ = val;
	}
	else
	    *dst++ = *src++;
    }

    *dst++ = 0;

    return copy;
}
