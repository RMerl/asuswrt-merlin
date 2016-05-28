/*
 * Prototypes for ads
 */

void ads_msgfree(ADS_STRUCT *ads, LDAPMessage *msg);
char *ads_get_dn(ADS_STRUCT *ads, LDAPMessage *msg);
char *ads_get_dn_canonical(ADS_STRUCT *ads, LDAPMessage *msg);

char *ads_pull_string(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, LDAPMessage *msg,
		      const char *field);
char **ads_pull_strings(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			LDAPMessage *msg, const char *field,
			size_t *num_values);
char **ads_pull_strings_range(ADS_STRUCT *ads, 
			      TALLOC_CTX *mem_ctx,
			      LDAPMessage *msg, const char *field,
			      char **current_strings,
			      const char **next_attribute,
			      size_t *num_strings,
			      BOOL *more_strings);
BOOL ads_pull_uint32(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		     uint32 *v);
BOOL ads_pull_guid(ADS_STRUCT *ads, LDAPMessage *msg, struct GUID *guid);
BOOL ads_pull_sid(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		  DOM_SID *sid);
int ads_pull_sids(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		  LDAPMessage *msg, const char *field, DOM_SID **sids);
BOOL ads_pull_sd(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		 LDAPMessage *msg, const char *field, SEC_DESC **sd);
char *ads_pull_username(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			LDAPMessage *msg);
int ads_pull_sids_from_extendeddn(ADS_STRUCT *ads, 
				  TALLOC_CTX *mem_ctx, 
				  LDAPMessage *msg, 
				  const char *field,
				  enum ads_extended_dn_flags flags,
				  DOM_SID **sids);

ADS_STATUS ads_find_machine_acct(ADS_STRUCT *ads, LDAPMessage **res,
				 const char *machine);
ADS_STATUS ads_find_printer_on_server(ADS_STRUCT *ads, LDAPMessage **res,
				      const char *printer,
				      const char *servername);
ADS_STATUS ads_find_printers(ADS_STRUCT *ads, LDAPMessage **res);
ADS_STATUS ads_find_user_acct(ADS_STRUCT *ads, LDAPMessage **res,
			      const char *user);

ADS_STATUS ads_do_search(ADS_STRUCT *ads, const char *bind_path, int scope, 
			 const char *expr,
			 const char **attrs, LDAPMessage **res);
ADS_STATUS ads_search(ADS_STRUCT *ads, LDAPMessage **res, 
		      const char *expr, const char **attrs);
ADS_STATUS ads_search_dn(ADS_STRUCT *ads, LDAPMessage **res, 
			 const char *dn, const char **attrs);
ADS_STATUS ads_do_search_all_args(ADS_STRUCT *ads, const char *bind_path,
				  int scope, const char *expr,
				  const char **attrs, void *args,
				  LDAPMessage **res);
ADS_STATUS ads_do_search_all(ADS_STRUCT *ads, const char *bind_path,
			     int scope, const char *expr,
			     const char **attrs, LDAPMessage **res);
ADS_STATUS ads_do_search_retry(ADS_STRUCT *ads, const char *bind_path,
			       int scope, 
			       const char *expr,
			       const char **attrs, LDAPMessage **res);
ADS_STATUS ads_do_search_retry_args(ADS_STRUCT *ads, const char *bind_path,
				    int scope, const char *expr,
				    const char **attrs, void *args,
				    LDAPMessage **res);
ADS_STATUS ads_search_retry(ADS_STRUCT *ads, LDAPMessage **res, 
			    const char *expr, const char **attrs);
ADS_STATUS ads_search_retry_dn(ADS_STRUCT *ads, LDAPMessage **res, 
			       const char *dn, 
			       const char **attrs);
ADS_STATUS ads_search_retry_extended_dn(ADS_STRUCT *ads, LDAPMessage **res, 
					const char *dn, 
					const char **attrs,
					enum ads_extended_dn_flags flags);
ADS_STATUS ads_search_retry_sid(ADS_STRUCT *ads, LDAPMessage **res, 
				const DOM_SID *sid,
				const char **attrs);


LDAPMessage *ads_first_entry(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_next_entry(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_first_message(ADS_STRUCT *ads, LDAPMessage *res);
LDAPMessage *ads_next_message(ADS_STRUCT *ads, LDAPMessage *res);
void ads_process_results(ADS_STRUCT *ads, LDAPMessage *res,
			 BOOL(*fn)(char *, void **, void *),
			 void *data_area);
void ads_dump(ADS_STRUCT *ads, LDAPMessage *res);

ADS_STATUS ads_parse_gpo(ADS_STRUCT *ads,
			 TALLOC_CTX *mem_ctx,
			 LDAPMessage *res,
			 const char *gpo_dn,
			 struct GROUP_POLICY_OBJECT *gpo);
