#define RPC_AUTH_GSS_KRB5       390003
#define RPC_AUTH_GSS_KRB5I      390004
#define RPC_AUTH_GSS_KRB5P      390005
#define RPC_AUTH_GSS_LKEY       390006
#define RPC_AUTH_GSS_LKEYI      390007
#define RPC_AUTH_GSS_LKEYP      390008
#define RPC_AUTH_GSS_SPKM       390009
#define RPC_AUTH_GSS_SPKMI      390010
#define RPC_AUTH_GSS_SPKMP      390011

struct flav_info {
	char    *flavour;
	int     fnum;
};

extern struct flav_info flav_map[];
extern const int flav_map_size;
