/* 
   ldb database library

   Copyright (C) Simo Sorce 2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: oLschema2ldif
 *
 *  Description: utility to convert an OpenLDAP schema into AD LDIF
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb.h"
#include "tools/cmdline.h"
#include "dsdb/samdb/samdb.h"

#define SCHEMA_UNKNOWN 0
#define SCHEMA_NAME 1
#define SCHEMA_SUP 2
#define SCHEMA_STRUCTURAL 3
#define SCHEMA_ABSTRACT 4
#define SCHEMA_AUXILIARY 5
#define SCHEMA_MUST 6
#define SCHEMA_MAY 7
#define SCHEMA_SINGLE_VALUE 8
#define SCHEMA_EQUALITY 9
#define SCHEMA_ORDERING 10
#define SCHEMA_SUBSTR 11
#define SCHEMA_SYNTAX 12
#define SCHEMA_DESC 13

struct schema_conv {
	int count;
	int failures;
};

struct schema_token {
	int type;
	char *value;
};

struct ldb_context *ldb_ctx;
struct ldb_dn *basedn;

static int check_braces(const char *string)
{
	int b;
	char *c;

	b = 0;
	if ((c = strchr(string, '(')) == NULL) {
		return -1;
	}
	b++;
	c++;
	while (b) {
		c = strpbrk(c, "()");
		if (c == NULL) return 1;
		if (*c == '(') b++;
		if (*c == ')') b--;
		c++;
	}
	return 0;
}

static char *skip_spaces(char *string) {
	return (string + strspn(string, " \t\n"));
}

static int add_multi_string(struct ldb_message *msg, const char *attr, char *values)
{
	char *c;
	char *s;
	int n;

	c = skip_spaces(values);
	while (*c) {
		n = strcspn(c, " \t$");
		s = talloc_strndup(msg, c, n);
		if (ldb_msg_add_string(msg, attr, s) != 0) {
			return -1;
		}
		c += n;
		c += strspn(c, " \t$");
	}

	return 0;
}

#define MSG_ADD_STRING(a, v) do { if (ldb_msg_add_string(msg, a, v) != 0) goto failed; } while(0)
#define MSG_ADD_M_STRING(a, v) do { if (add_multi_string(msg, a, v) != 0) goto failed; } while(0)

static char *get_def_value(TALLOC_CTX *ctx, char **string)
{
	char *c = *string;
	char *value;
	int n;

	if (*c == '\'') {
		c++;
		n = strcspn(c, "\'");
		value = talloc_strndup(ctx, c, n);
		c += n;
		c++; /* skip closing \' */
	} else {
		n = strcspn(c, " \t\n");
		value = talloc_strndup(ctx, c, n);
		c += n;
	}
	*string = c;

	return value;
}

static struct schema_token *get_next_schema_token(TALLOC_CTX *ctx, char **string)
{
	char *c = skip_spaces(*string);
	char *type;
	struct schema_token *token;
	int n;

	token = talloc(ctx, struct schema_token);

	n = strcspn(c, " \t\n");
	type = talloc_strndup(token, c, n);
	c += n;
	c = skip_spaces(c);

	if (strcasecmp("NAME", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_NAME;
		/* we do not support aliases so we get only the first name given and skip others */
		if (*c == '(') {
			char *s = strchr(c, ')');
			if (s == NULL) return NULL;
			s = skip_spaces(s);
			*string = s;

			c++;
			c = skip_spaces(c);
		}

		token->value = get_def_value(ctx, &c);

		if (*string < c) { /* single name */
			c = skip_spaces(c);
			*string = c;
		}
		return token;
	}
	if (strcasecmp("SUP", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_SUP;

		if (*c == '(') {
			c++;
			n = strcspn(c, ")");
			token->value = talloc_strndup(ctx, c, n);
			c += n;
			c++;
		} else {
			token->value = get_def_value(ctx, &c);
		}

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("STRUCTURAL", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_STRUCTURAL;
		*string = c;
		return token;
	}

	if (strcasecmp("ABSTRACT", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_ABSTRACT;
		*string = c;
		return token;
	}

	if (strcasecmp("AUXILIARY", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_AUXILIARY;
		*string = c;
		return token;
	}

	if (strcasecmp("MUST", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_MUST;

		if (*c == '(') {
			c++;
			n = strcspn(c, ")");
			token->value = talloc_strndup(ctx, c, n);
			c += n;
			c++;
		} else {
			token->value = get_def_value(ctx, &c);
		}

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("MAY", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_MAY;

		if (*c == '(') {
			c++;
			n = strcspn(c, ")");
			token->value = talloc_strndup(ctx, c, n);
			c += n;
			c++;
		} else {
			token->value = get_def_value(ctx, &c);
		}

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("SINGLE-VALUE", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_SINGLE_VALUE;
		*string = c;
		return token;
	}

	if (strcasecmp("EQUALITY", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_EQUALITY;

		token->value = get_def_value(ctx, &c);

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("ORDERING", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_ORDERING;

		token->value = get_def_value(ctx, &c);

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("SUBSTR", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_SUBSTR;

		token->value = get_def_value(ctx, &c);

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("SYNTAX", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_SYNTAX;

		token->value = get_def_value(ctx, &c);

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	if (strcasecmp("DESC", type) == 0) {
		talloc_free(type);
		token->type = SCHEMA_DESC;

		token->value = get_def_value(ctx, &c);

		c = skip_spaces(c);
		*string = c;
		return token;
	}

	token->type = SCHEMA_UNKNOWN;
	token->value = type;
	if (*c == ')') {
		*string = c;
		return token;
	}
	if (*c == '\'') {
		c = strchr(++c, '\'');
		c++;
	} else {
		c += strcspn(c, " \t\n");
	}
	c = skip_spaces(c);
	*string = c;

	return token;
}

static struct ldb_message *process_entry(TALLOC_CTX *mem_ctx, const char *entry)
{
	TALLOC_CTX *ctx;
	struct ldb_message *msg;
	struct schema_token *token;
        char *c, *s;
        int n;

	ctx = talloc_new(mem_ctx);
	msg = ldb_msg_new(ctx);

	ldb_msg_add_string(msg, "objectClass", "top");

	c = talloc_strdup(ctx, entry);
	if (!c) return NULL;

	c = skip_spaces(c);

	switch (*c) {
	case 'a':
		if (strncmp(c, "attributetype", 13) == 0) {
			c += 13;
			MSG_ADD_STRING("objectClass", "attributeSchema");
			break;
		}
		goto failed;
	case 'o':
		if (strncmp(c, "objectclass", 11) == 0) {
			c += 11;
			MSG_ADD_STRING("objectClass", "classSchema");
			break;
		}
		goto failed;
	default:
		goto failed;
	}

	c = strchr(c, '(');
	if (c == NULL) goto failed;
	c++;

	c = skip_spaces(c);

	/* get attributeID */
	n = strcspn(c, " \t");
	s = talloc_strndup(msg, c, n);
	MSG_ADD_STRING("attributeID", s);
	c += n;
	c = skip_spaces(c);	

	while (*c != ')') {
		token = get_next_schema_token(msg, &c);
		if (!token) goto failed;

		switch (token->type) {
		case SCHEMA_NAME:
			MSG_ADD_STRING("cn", token->value);
			MSG_ADD_STRING("name", token->value);
			MSG_ADD_STRING("lDAPDisplayName", token->value);
			msg->dn = ldb_dn_copy(msg, basedn);
			ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=Schema,CN=Configuration", token->value);
			break;

		case SCHEMA_SUP:
			MSG_ADD_M_STRING("subClassOf", token->value);
			break;

		case SCHEMA_STRUCTURAL:
			MSG_ADD_STRING("objectClassCategory", "1");
			break;

		case SCHEMA_ABSTRACT:
			MSG_ADD_STRING("objectClassCategory", "2");
			break;

		case SCHEMA_AUXILIARY:
			MSG_ADD_STRING("objectClassCategory", "3");
			break;

		case SCHEMA_MUST:
			MSG_ADD_M_STRING("mustContain", token->value);
			break;

		case SCHEMA_MAY:
			MSG_ADD_M_STRING("mayContain", token->value);
			break;

		case SCHEMA_SINGLE_VALUE:
			MSG_ADD_STRING("isSingleValued", "TRUE");
			break;

		case SCHEMA_EQUALITY:
			/* TODO */
			break;

		case SCHEMA_ORDERING:
			/* TODO */
			break;

		case SCHEMA_SUBSTR:
			/* TODO */
			break;

		case SCHEMA_SYNTAX:
		{
			const struct dsdb_syntax *map = 
				find_syntax_map_by_standard_oid(token->value);
			if (!map) {
				break;
			}
			MSG_ADD_STRING("attributeSyntax", map->attributeSyntax_oid);
			break;
		}
		case SCHEMA_DESC:
			MSG_ADD_STRING("description", token->value);
			break;

		default:
			fprintf(stderr, "Unknown Definition: %s\n", token->value);
		}
	}

	talloc_steal(mem_ctx, msg);
	talloc_free(ctx);
	return msg;

failed:
	talloc_free(ctx);
	return NULL;
}

static struct schema_conv process_file(FILE *in, FILE *out)
{
	TALLOC_CTX *ctx;
	struct schema_conv ret;
	char *entry;
	int c, t, line;
	struct ldb_ldif ldif;

	ldif.changetype = LDB_CHANGETYPE_NONE;

	ctx = talloc_new(NULL);

	ret.count = 0;
	ret.failures = 0;
	line = 0;

	while ((c = fgetc(in)) != EOF) {
		line++;
		/* fprintf(stderr, "Parsing line %d\n", line); */
		if (c == '#') {
			do {
				c = fgetc(in);
			} while (c != EOF && c != '\n');
			continue;
		}
		if (c == '\n') {
			continue;
		}

		t = 0;
		entry = talloc_array(ctx, char, 1024);
		if (entry == NULL) exit(-1);

		do { 
			if (c == '\n') {
				entry[t] = '\0';	
				if (check_braces(entry) == 0) {
					ret.count++;
					ldif.msg = process_entry(ctx, entry);
					if (ldif.msg == NULL) {
						ret.failures++;
						fprintf(stderr, "No valid msg from entry \n[%s]\n at line %d\n", entry, line);
						break;
					}
					ldb_ldif_write_file(ldb_ctx, out, &ldif);
					break;
				}
				line++;
			} else {
				entry[t] = c;
				t++;
			}
			if ((t % 1023) == 0) {
				entry = talloc_realloc(ctx, entry, char, t + 1024);
				if (entry == NULL) exit(-1);
			}
		} while ((c = fgetc(in)) != EOF); 

		if (c != '\n') {
			entry[t] = '\0';
			if (check_braces(entry) == 0) {
				ret.count++;
				ldif.msg = process_entry(ctx, entry);
				if (ldif.msg == NULL) {
					ret.failures++;
					fprintf(stderr, "No valid msg from entry \n[%s]\n at line %d\n", entry, line);
					break;
				}
				ldb_ldif_write_file(ldb_ctx, out, &ldif);
			} else {
				fprintf(stderr, "malformed entry on line %d\n", line);
				ret.failures++;
			}
		}
	
		if (c == EOF) break;
	}

	return ret;
}

static void usage(void)
{
	printf("Usage: oLschema2ldif -H NONE <options>\n");
	printf("\nConvert OpenLDAP schema to AD-like LDIF format\n\n");
	printf("Options:\n");
	printf("  -I inputfile     inputfile of OpenLDAP style schema otherwise STDIN\n");
	printf("  -O outputfile    outputfile otherwise STDOUT\n");
	printf("  -o options       pass options like modules to activate\n");
	printf("              e.g: -o modules:timestamps\n");
	printf("\n");
	printf("Converts records from an openLdap formatted schema to an ldif schema\n\n");
	exit(1);
}

 int main(int argc, const char **argv)
{
	TALLOC_CTX *ctx;
	struct schema_conv ret;
	struct ldb_cmdline *options;
	FILE *in = stdin;
	FILE *out = stdout;
	ctx = talloc_new(NULL);
	ldb_ctx = ldb_init(ctx, NULL);

	setenv("LDB_URL", "NONE", 1);
	options = ldb_cmdline_process(ldb_ctx, argc, argv, usage);

	if (options->basedn == NULL) {
		perror("Base DN not specified");
		exit(1);
	} else {
		basedn = ldb_dn_new(ctx, ldb_ctx, options->basedn);
		if ( ! ldb_dn_validate(basedn)) {
			perror("Malformed Base DN");
			exit(1);
		}
	}

	if (options->input) {
		in = fopen(options->input, "r");
		if (!in) {
			perror(options->input);
			exit(1);
		}
	}
	if (options->output) {
		out = fopen(options->output, "w");
		if (!out) {
			perror(options->output);
			exit(1);
		}
	}

	ret = process_file(in, out);

	fclose(in);
	fclose(out);

	printf("Converted %d records with %d failures\n", ret.count, ret.failures);

	return 0;
}
