/*
 * table_generic.c
 *
 *    Generic table API framework
 */

/** @defgroup table_generic generic_table_API
 *  General requirements for a table helper.
 *  @ingroup table
 *
 * A given table helper need not implement the whole of this API,
 *   and may need to adjust the prototype of certain routines.
 * But this description provides a suitable standard design framework.
 *   
 * @{
 */

/* =======================================================
 * 
 *  Table Maintenance:
 *      create/delete table
 *      create/copy/clone/delete row
 *      add/replace/remove row
 *
 * ======================================================= */

/** @defgroup table_maintenance table_maintenance
 *
 * Routines for maintaining the contents of a table.
 * This would typically be part of implementing an SNMP MIB,
 *   but could potentially also be used for a standalone table.
 *
 * This section of the generic API is primarily relevant to
 *   table helpers where the representation of the table is 
 *   constructed and maintained within the helper itself.
 * "External" tables will typically look after such aspects
 *   directly, although this section of the abstract API 
 *   framework could also help direct the design of such
 *   table-specific implementations.
 *
 * @{
 */

/** Create a structure to represent the table.
  *
  * This could be as simple as the head of a linked
  *   list, or a more complex container structure.
  * The 'name' field would typically be used to
  *  distinguish between several tables implemented
  *  using the same table helper.  The 'flags' field
  *  would be used to control various (helper-specific)
  *  aspects of table behaviour.
  *
  * The table structure returned should typically be
  *  regarded as an opaque, private structure. All
  *  operations on the content of the table should
  *  ideally use the appropriate routines from this API.
  */
void *
netsnmp_generic_create_table( const char *name, int flags ) {
}

/** Release the structure representing a table.
  * Any rows still contained within the table
  *   should also be removed and deleted.
  */
void
netsnmp_generic_delete_table( void *table ) {
}

/** Create a new row structure suitable for this style of table.
  * Note that this would typically be a 'standalone' row, and
  *   would not automatically be inserted into an actual table.
  */
void *
netsnmp_generic_create_row( void ) {
}

/** Create a new copy of the specified row.
  */
void *
netsnmp_generic_clone_row( void *row ) {
}

/** Copy the contents of one row into another.
  * The destination row structure should be
  *   created before this routine is called.
  */
int
netsnmp_generic_copy_row( void *dst_row, void *src_row ) {
}

/** Delete a row data structure.
  * The row should be removed from any relevant
  *   table(s) before this routine is called.
  */
void
netsnmp_generic_delete_row( void *row ) {
}

/** Add a row to the table.
  */
int
netsnmp_generic_add_row( void *table, void *row ) {
}

/** Replace one row with another in the table.
  * This will typically (but not necessarily) involve
  *   two rows sharing the same index information (e.g.
  *   to implement update/restore-style SET behaviour).
  */
int
netsnmp_generic_replace_row( void *table, void *old_row, void *new_row ) {
}

/** Remove a row from the table.
  * The data structure for the row should not be released,
  *   and would be the return value of this routine.
  */
void *
netsnmp_generic_remove_row( void *table, void *row ) {
}

/** Remove and delete a row from the table.
  */
void
netsnmp_generic_remove_delete_row( void *table, void *row ) {
}

/** @} end of table_maintenance */

/* =======================================================
 * 
 *  MIB Maintenance:
 *      create a handler registration
 *      register/unregister table
 *      extract table from request
 *      extract/insert row
 *
 * ======================================================= */

/** @defgroup mib_maintenance mib_maintenance
 *
 * Routines for maintaining a MIB table.
 *
 * @{
 */

/** Create a MIB handler structure.
  * This will typically be invoked within the corresponding
  *   'netsnmp_generic_register' routine (or the registration
  *   code of a sub-helper based on this helper).
  *
  * Alternatively, it might be called from the initialisation
  *   code of a particular MIB table implementation.
  */
netsnmp_mib_handler *
netsnmp_generic_get_handler(void /* table specific */ ) {

}

/** Free a MIB handler structure, releasing any related resources.
  * Possibly called automatically by 'netsnmp_unregister_handler' ?
  */
netsnmp_generic_free_handler( netsnmp_mib_handler *handler ) {

}

/** Register a MIB table with the SNMP agent.
  */
int
netsnmp_generic_register(netsnmp_handler_registration    *reginfo,
                         void                            *table,
                         netsnmp_table_registration_info *table_info) {
}

/** Unregister a MIB table from the SNMP agent.
  * This should also release the internal representation of the table.
  * ?? Is a table-specific version of this needed, or would
  *    'netsnmp_unregister_handler' + 'netsnmp_generic_free_handler' do?
  */
int
netsnmp_generic_unregister(netsnmp_handler_registration    *reginfo) {
}

/** Extract the table relating to a requested varbind.
  */
void
netsnmp_generic_extract_table( netsnmp_request_info *request ) {
}

/** Extract the row relating to a requested varbind.
  */
void
netsnmp_generic_extract_row( netsnmp_request_info *request ) {
}

/** Associate a (new) row with the requested varbind.
  * The row should also be associated with any other
  *   varbinds that refer to the same index values.
  */
void
netsnmp_generic_insert_row( netsnmp_request_info *request, void *row ) {
}

/** @} end of mib_maintenance */

/* =======================================================
 * 
 *  Row Operations
 *      get first/this/next row
 *      get row/next row by index
 *      get row/next row by OID
 *      number of rows
 *
 * ======================================================= */

/** @defgroup table_rows table_rows
 *
 * Routines for working with the rows of a table.
 *
 * @{
 */

/** Retrieve the first row of the table.
  */
void *
netsnmp_generic_row_first( void *table ) {
}

/** Retrieve the given row from the table.
  * This could either be the same data pointer,
  *   passed in, or a separate row structure
  *   sharing the same index values (or NULL).
  *
  * This routine also provides a means to tell
  *   whether a given row is present in the table.
  */
void *
netsnmp_generic_row_get( void *table, void *row ) {
}

/** Retrieve the following row from the table.
  * If the specified row is not present, this
  *   routine should return the entry next after
  *   the position this row would have occupied.
  */
void *
netsnmp_generic_row_next( void *table, void *row ) {
}

/** Retrieve the row with the specified index values.
  */
void *
netsnmp_generic_row_get_byidx(  void *table,
                                netsnmp_variable_list *indexes ) {
}

/** Retrieve the next row after the specified index values.
  */
void *
netsnmp_generic_row_next_byidx( void *table,
                                netsnmp_variable_list *indexes ) {

}

/** Retrieve the row with the specified instance OIDs.
  */
void *
netsnmp_generic_row_get_byoid(  void *table, oid *instance, size_t len ) {
}

/** Retrieve the next row after the specified instance OIDs.
  */
void *
netsnmp_generic_row_next_byoid( void *table, oid *instance, size_t len ) {
}

/** Report the number of rows in the table.
  */
int
netsnmp_generic_row_count( void *table ) {
}

/** @} end of table_rows */

/* =======================================================
 * 
 *  Index Operations
 *      get table index structure
 *      get row index values/OIDs
 *      compare row with index/OIDs
 *      subtree comparisons (index/OIDs)
 *
 * ======================================================= */

/** @defgroup table_indexes table_indexes
 *
 * Routines for working with row indexes.
 *
 * @{
 */

/** Retrieve the indexing structure of the table.
  */
netsnmp_variable_list *
netsnmp_generic_idx( void *table ) {
}

/** Report the index values for a row.
  */
netsnmp_variable_list *
netsnmp_generic_row_idx( void *row ) {
}

/** Report the instance OIDs for a row.
  */
size_t
netsnmp_generic_row_oid( void *row, oid *instances ) {
}

/** Compare a row against the specified index values.
  */
int
netsnmp_generic_compare_idx( void *row, netsnmp_variable_list *index ) {
}

/** Compare a row against the specified instance OIDs.
  */
int
netsnmp_generic_compare_oid( void *row, oid *instances, size_t len ) {
}

/** Check if a row lies within a subtree of index values.
  */
int
netsnmp_generic_compare_subtree_idx( void *row, netsnmp_variable_list *index ) {
}

/** Check if a row lies within a subtree of instance OIDs.
  */
int
netsnmp_generic_compare_subtree_oid( void *row, oid *instances, size_t len ) {
}

/** @} end of table_indexes */
/** @} end of table_generic */
