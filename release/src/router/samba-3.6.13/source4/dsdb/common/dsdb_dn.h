enum dsdb_dn_format {
	DSDB_NORMAL_DN,
	DSDB_BINARY_DN,
	DSDB_STRING_DN,
	DSDB_INVALID_DN
};

struct dsdb_dn {
	struct ldb_dn *dn;
	DATA_BLOB extra_part;
	enum dsdb_dn_format dn_format;
	const char *oid;
};

#define DSDB_SYNTAX_BINARY_DN	"1.2.840.113556.1.4.903"
#define DSDB_SYNTAX_STRING_DN	"1.2.840.113556.1.4.904"
#define DSDB_SYNTAX_OR_NAME	"1.2.840.113556.1.4.1221"


/* RMD_FLAGS component in a DN */
#define DSDB_RMD_FLAG_DELETED     1
#define DSDB_RMD_FLAG_INVISIBLE   2
