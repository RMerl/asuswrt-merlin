/* unused.  Stub to make the pidl generated NDR parsers compile */

/*
  this is used to find pointers to calls
*/
struct dcerpc_interface_call {
        const char *name;
        size_t struct_size;
        ndr_push_flags_fn_t ndr_push;
        ndr_pull_flags_fn_t ndr_pull;
        ndr_print_function_t ndr_print;
        BOOL async;
};

struct dcerpc_endpoint_list {
        uint32_t count;
        const char * const *names;
};

struct dcerpc_authservice_list {
        uint32_t count;
        const char * const *names;
};

struct dcerpc_interface_table {
        const char *name;
        struct dcerpc_syntax_id syntax_id;
        const char *helpstring;
        uint32_t num_calls;
        const struct dcerpc_interface_call *calls;
        const struct dcerpc_endpoint_list *endpoints;
        const struct dcerpc_authservice_list *authservices;
};

struct dcerpc_interface_list {
        struct dcerpc_interface_list *prev, *next;
        const struct dcerpc_interface_table *table;
};


