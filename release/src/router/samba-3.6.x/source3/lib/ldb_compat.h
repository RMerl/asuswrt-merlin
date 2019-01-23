/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

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

#ifndef __LDB_COMPAT_H__
#define __LDB_COMPAT_H__

char *ldb_binary_encode(void *mem_ctx, struct ldb_val val);
struct ldb_parse_tree *ldb_parse_tree(void *mem_ctx, const char *s);

/*
   structures for ldb_parse_tree handling code
*/
enum ldb_parse_op { LDB_OP_AND=1, LDB_OP_OR=2, LDB_OP_NOT=3,
		    LDB_OP_EQUALITY=4, LDB_OP_SUBSTRING=5,
		    LDB_OP_GREATER=6, LDB_OP_LESS=7, LDB_OP_PRESENT=8,
		    LDB_OP_APPROX=9, LDB_OP_EXTENDED=10 };

struct ldb_parse_tree {
	enum ldb_parse_op operation;
	union {
		struct {
			struct ldb_parse_tree *child;
		} isnot;
		struct {
			const char *attr;
			struct ldb_val value;
		} equality;
		struct {
			const char *attr;
			int start_with_wildcard;
			int end_with_wildcard;
			struct ldb_val **chunks;
		} substring;
		struct {
			const char *attr;
		} present;
		struct {
			const char *attr;
			struct ldb_val value;
		} comparison;
		struct {
			const char *attr;
			int dnAttributes;
			char *rule_id;
			struct ldb_val value;
		} extended;
		struct {
			unsigned int num_elements;
			struct ldb_parse_tree **elements;
		} list;
	} u;
};

struct ldb_message_element {
	unsigned int flags;
	const char *name;
	unsigned int num_values;
	struct ldb_val *values;
};

struct ldb_control {
	const char *oid;
	int critical;
	void *data;
};

#endif
