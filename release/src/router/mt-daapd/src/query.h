#ifndef __query__
#define __query__

typedef enum 
{
    /* node opcodes */
    qot_empty,

    /* conjunctions */
    qot_and,
    qot_or,

    /* negation */
    qot_not,

    /* arithmetic */
    qot_eq,
    qot_ne,
    qot_le,
    qot_lt,
    qot_ge,
    qot_gt,

    /* string */
    qot_is,
    qot_begins,
    qot_ends,
    qot_contains,

    /* constant opcode */
    qot_const,

    /* field types */
    qft_i32,
    qft_i64,
    qft_string

} query_type_t;

typedef struct query_field_ query_field_t;
struct query_field_
{
    query_type_t	type;
    const char*		name;
    int			offset;
};

typedef struct query_node_ query_node_t;
struct query_node_
{
    query_type_t		type;
    union {
	query_node_t*		node;
	const query_field_t*	field;
	int			constant;
    }				left;
    union {
	query_node_t*		node;
	int			i32;
	long long		i64;
	char*			str;
    }				right;
};

query_node_t*		query_build(const char* query, 
				    const query_field_t* fields);
int			query_test(query_node_t* query, void* target);
void			query_free(query_node_t* query);
void			query_dump(FILE* fp, query_node_t* query, int depth);

#endif
