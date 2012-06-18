struct syntax_map {
	const char *Standard_OID;
	const char *AD_OID;
	const char *equality;
	const char *substring;
	const char *comment;
};

const struct syntax_map *find_syntax_map_by_ad_oid(const char *ad_oid); 
const struct syntax_map *find_syntax_map_by_standard_oid(const char *standard_oid);
