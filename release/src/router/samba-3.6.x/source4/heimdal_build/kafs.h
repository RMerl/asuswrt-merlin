int k_hasafs (void) {
	return 0;
};

int krb_afslog (const char *cell, const char *realm) {
	return 0;
};
int k_unlog (void) {
	return 0;
};
int k_setpag (void) {
	return 0;
};
krb5_error_code krb5_afslog (krb5_context context,
				 krb5_ccache id, 
				 const char *cell,
			     krb5_const_realm realm) {
	return 0;
};
