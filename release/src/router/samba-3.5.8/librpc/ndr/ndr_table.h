#ifndef _NDR_TABLE_PROTO_H_
#define _NDR_TABLE_PROTO_H_

NTSTATUS ndr_table_register(const struct ndr_interface_table *table);
const char *ndr_interface_name(const struct GUID *uuid, uint32_t if_version);
int ndr_interface_num_calls(const struct GUID *uuid, uint32_t if_version);
const struct ndr_interface_table *ndr_table_by_name(const char *name);
const struct ndr_interface_table *ndr_table_by_uuid(const struct GUID *uuid);
const struct ndr_interface_list *ndr_table_list(void);
NTSTATUS ndr_table_init(void);
NTSTATUS ndr_table_register_builtin_tables(void);

#endif /* _NDR_TABLE_PROTO_H_ */

