
bool test_SetupCredentials2(struct dcerpc_pipe *p, struct torture_context *tctx,
			    uint32_t negotiate_flags,
			    struct cli_credentials *machine_credentials,
			    int sec_chan_type,
			    struct netlogon_creds_CredentialState **creds_out);
