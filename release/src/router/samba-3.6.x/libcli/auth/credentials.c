/* 
   Unix SMB/CIFS implementation.

   code to manipulate domain credentials

   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/time.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "../libcli/security/dom_sid.h"

static void netlogon_creds_step_crypt(struct netlogon_creds_CredentialState *creds,
				      const struct netr_Credential *in,
				      struct netr_Credential *out)
{
	des_crypt112(out->data, in->data, creds->session_key, 1);
}

/*
  initialise the credentials state for old-style 64 bit session keys

  this call is made after the netr_ServerReqChallenge call
*/
static void netlogon_creds_init_64bit(struct netlogon_creds_CredentialState *creds,
				      const struct netr_Credential *client_challenge,
				      const struct netr_Credential *server_challenge,
				      const struct samr_Password *machine_password)
{
	uint32_t sum[2];
	uint8_t sum2[8];

	sum[0] = IVAL(client_challenge->data, 0) + IVAL(server_challenge->data, 0);
	sum[1] = IVAL(client_challenge->data, 4) + IVAL(server_challenge->data, 4);

	SIVAL(sum2,0,sum[0]);
	SIVAL(sum2,4,sum[1]);

	ZERO_STRUCT(creds->session_key);

	des_crypt128(creds->session_key, sum2, machine_password->hash);
}

/*
  initialise the credentials state for ADS-style 128 bit session keys

  this call is made after the netr_ServerReqChallenge call
*/
static void netlogon_creds_init_128bit(struct netlogon_creds_CredentialState *creds,
				       const struct netr_Credential *client_challenge,
				       const struct netr_Credential *server_challenge,
				       const struct samr_Password *machine_password)
{
	unsigned char zero[4], tmp[16];
	HMACMD5Context ctx;
	MD5_CTX md5;

	ZERO_STRUCT(creds->session_key);

	memset(zero, 0, sizeof(zero));

	hmac_md5_init_rfc2104(machine_password->hash, sizeof(machine_password->hash), &ctx);	
	MD5Init(&md5);
	MD5Update(&md5, zero, sizeof(zero));
	MD5Update(&md5, client_challenge->data, 8);
	MD5Update(&md5, server_challenge->data, 8);
	MD5Final(tmp, &md5);
	hmac_md5_update(tmp, sizeof(tmp), &ctx);
	hmac_md5_final(creds->session_key, &ctx);
}

static void netlogon_creds_first_step(struct netlogon_creds_CredentialState *creds,
				      const struct netr_Credential *client_challenge,
				      const struct netr_Credential *server_challenge)
{
	netlogon_creds_step_crypt(creds, client_challenge, &creds->client);

	netlogon_creds_step_crypt(creds, server_challenge, &creds->server);

	creds->seed = creds->client;
}

/*
  step the credentials to the next element in the chain, updating the
  current client and server credentials and the seed
*/
static void netlogon_creds_step(struct netlogon_creds_CredentialState *creds)
{
	struct netr_Credential time_cred;

	DEBUG(5,("\tseed        %08x:%08x\n", 
		 IVAL(creds->seed.data, 0), IVAL(creds->seed.data, 4)));

	SIVAL(time_cred.data, 0, IVAL(creds->seed.data, 0) + creds->sequence);
	SIVAL(time_cred.data, 4, IVAL(creds->seed.data, 4));

	DEBUG(5,("\tseed+time   %08x:%08x\n", IVAL(time_cred.data, 0), IVAL(time_cred.data, 4)));

	netlogon_creds_step_crypt(creds, &time_cred, &creds->client);

	DEBUG(5,("\tCLIENT      %08x:%08x\n", 
		 IVAL(creds->client.data, 0), IVAL(creds->client.data, 4)));

	SIVAL(time_cred.data, 0, IVAL(creds->seed.data, 0) + creds->sequence + 1);
	SIVAL(time_cred.data, 4, IVAL(creds->seed.data, 4));

	DEBUG(5,("\tseed+time+1 %08x:%08x\n", 
		 IVAL(time_cred.data, 0), IVAL(time_cred.data, 4)));

	netlogon_creds_step_crypt(creds, &time_cred, &creds->server);

	DEBUG(5,("\tSERVER      %08x:%08x\n", 
		 IVAL(creds->server.data, 0), IVAL(creds->server.data, 4)));

	creds->seed = time_cred;
}


/*
  DES encrypt a 8 byte LMSessionKey buffer using the Netlogon session key
*/
void netlogon_creds_des_encrypt_LMKey(struct netlogon_creds_CredentialState *creds, struct netr_LMSessionKey *key)
{
	struct netr_LMSessionKey tmp;
	des_crypt56(tmp.key, key->key, creds->session_key, 1);
	*key = tmp;
}

/*
  DES decrypt a 8 byte LMSessionKey buffer using the Netlogon session key
*/
void netlogon_creds_des_decrypt_LMKey(struct netlogon_creds_CredentialState *creds, struct netr_LMSessionKey *key)
{
	struct netr_LMSessionKey tmp;
	des_crypt56(tmp.key, key->key, creds->session_key, 0);
	*key = tmp;
}

/*
  DES encrypt a 16 byte password buffer using the session key
*/
void netlogon_creds_des_encrypt(struct netlogon_creds_CredentialState *creds, struct samr_Password *pass)
{
	struct samr_Password tmp;
	des_crypt112_16(tmp.hash, pass->hash, creds->session_key, 1);
	*pass = tmp;
}

/*
  DES decrypt a 16 byte password buffer using the session key
*/
void netlogon_creds_des_decrypt(struct netlogon_creds_CredentialState *creds, struct samr_Password *pass)
{
	struct samr_Password tmp;
	des_crypt112_16(tmp.hash, pass->hash, creds->session_key, 0);
	*pass = tmp;
}

/*
  ARCFOUR encrypt/decrypt a password buffer using the session key
*/
void netlogon_creds_arcfour_crypt(struct netlogon_creds_CredentialState *creds, uint8_t *data, size_t len)
{
	DATA_BLOB session_key = data_blob(creds->session_key, 16);

	arcfour_crypt_blob(data, len, &session_key);

	data_blob_free(&session_key);
}

/*****************************************************************
The above functions are common to the client and server interface
next comes the client specific functions
******************************************************************/

/*
  initialise the credentials chain and return the first client
  credentials
*/
 
struct netlogon_creds_CredentialState *netlogon_creds_client_init(TALLOC_CTX *mem_ctx, 
								  const char *client_account,
								  const char *client_computer_name, 
								  const struct netr_Credential *client_challenge,
								  const struct netr_Credential *server_challenge,
								  const struct samr_Password *machine_password,
								  struct netr_Credential *initial_credential,
								  uint32_t negotiate_flags)
{
	struct netlogon_creds_CredentialState *creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);
	
	if (!creds) {
		return NULL;
	}
	
	creds->sequence = time(NULL);
	creds->negotiate_flags = negotiate_flags;

	creds->computer_name = talloc_strdup(creds, client_computer_name);
	if (!creds->computer_name) {
		talloc_free(creds);
		return NULL;
	}
	creds->account_name = talloc_strdup(creds, client_account);
	if (!creds->account_name) {
		talloc_free(creds);
		return NULL;
	}

	dump_data_pw("Client chall", client_challenge->data, sizeof(client_challenge->data));
	dump_data_pw("Server chall", server_challenge->data, sizeof(server_challenge->data));
	dump_data_pw("Machine Pass", machine_password->hash, sizeof(machine_password->hash));

	if (negotiate_flags & NETLOGON_NEG_128BIT) {
		netlogon_creds_init_128bit(creds, client_challenge, server_challenge, machine_password);
	} else {
		netlogon_creds_init_64bit(creds, client_challenge, server_challenge, machine_password);
	}

	netlogon_creds_first_step(creds, client_challenge, server_challenge);

	dump_data_pw("Session key", creds->session_key, 16);
	dump_data_pw("Credential ", creds->client.data, 8);

	*initial_credential = creds->client;
	return creds;
}

/*
  initialise the credentials structure with only a session key.  The caller better know what they are doing!
 */

struct netlogon_creds_CredentialState *netlogon_creds_client_init_session_key(TALLOC_CTX *mem_ctx, 
									      const uint8_t session_key[16])
{
	struct netlogon_creds_CredentialState *creds;

	creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);
	if (!creds) {
		return NULL;
	}
	
	memcpy(creds->session_key, session_key, 16);

	return creds;
}

/*
  step the credentials to the next element in the chain, updating the
  current client and server credentials and the seed

  produce the next authenticator in the sequence ready to send to 
  the server
*/
void netlogon_creds_client_authenticator(struct netlogon_creds_CredentialState *creds,
				struct netr_Authenticator *next)
{	
	creds->sequence += 2;
	netlogon_creds_step(creds);

	next->cred = creds->client;
	next->timestamp = creds->sequence;
}

/*
  check that a credentials reply from a server is correct
*/
bool netlogon_creds_client_check(struct netlogon_creds_CredentialState *creds,
			const struct netr_Credential *received_credentials)
{
	if (!received_credentials || 
	    memcmp(received_credentials->data, creds->server.data, 8) != 0) {
		DEBUG(2,("credentials check failed\n"));
		return false;
	}
	return true;
}


/*****************************************************************
The above functions are common to the client and server interface
next comes the server specific functions
******************************************************************/

/*
  check that a credentials reply from a server is correct
*/
static bool netlogon_creds_server_check_internal(const struct netlogon_creds_CredentialState *creds,
						 const struct netr_Credential *received_credentials)
{
	if (memcmp(received_credentials->data, creds->client.data, 8) != 0) {
		DEBUG(2,("credentials check failed\n"));
		dump_data_pw("client creds", creds->client.data, 8);
		dump_data_pw("calc   creds", received_credentials->data, 8);
		return false;
	}
	return true;
}

/*
  initialise the credentials chain and return the first server
  credentials
*/
struct netlogon_creds_CredentialState *netlogon_creds_server_init(TALLOC_CTX *mem_ctx, 
								  const char *client_account,
								  const char *client_computer_name, 
								  uint16_t secure_channel_type,
								  const struct netr_Credential *client_challenge,
								  const struct netr_Credential *server_challenge,
								  const struct samr_Password *machine_password,
								  struct netr_Credential *credentials_in,
								  struct netr_Credential *credentials_out,
								  uint32_t negotiate_flags)
{
	
	struct netlogon_creds_CredentialState *creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);
	
	if (!creds) {
		return NULL;
	}
	
	creds->negotiate_flags = negotiate_flags;
	creds->secure_channel_type = secure_channel_type;

	creds->computer_name = talloc_strdup(creds, client_computer_name);
	if (!creds->computer_name) {
		talloc_free(creds);
		return NULL;
	}
	creds->account_name = talloc_strdup(creds, client_account);
	if (!creds->account_name) {
		talloc_free(creds);
		return NULL;
	}

	if (negotiate_flags & NETLOGON_NEG_128BIT) {
		netlogon_creds_init_128bit(creds, client_challenge, server_challenge, 
					   machine_password);
	} else {
		netlogon_creds_init_64bit(creds, client_challenge, server_challenge, 
					  machine_password);
	}

	netlogon_creds_first_step(creds, client_challenge, server_challenge);

	/* And before we leak information about the machine account
	 * password, check that they got the first go right */
	if (!netlogon_creds_server_check_internal(creds, credentials_in)) {
		talloc_free(creds);
		return NULL;
	}

	*credentials_out = creds->server;

	return creds;
}

NTSTATUS netlogon_creds_server_step_check(struct netlogon_creds_CredentialState *creds,
				 struct netr_Authenticator *received_authenticator,
				 struct netr_Authenticator *return_authenticator) 
{
	if (!received_authenticator || !return_authenticator) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!creds) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* TODO: this may allow the a replay attack on a non-signed
	   connection. Should we check that this is increasing? */
	creds->sequence = received_authenticator->timestamp;
	netlogon_creds_step(creds);
	if (netlogon_creds_server_check_internal(creds, &received_authenticator->cred)) {
		return_authenticator->cred = creds->server;
		return_authenticator->timestamp = creds->sequence;
		return NT_STATUS_OK;
	} else {
		ZERO_STRUCTP(return_authenticator);
		return NT_STATUS_ACCESS_DENIED;
	}
}

void netlogon_creds_decrypt_samlogon(struct netlogon_creds_CredentialState *creds,
			    uint16_t validation_level,
			    union netr_Validation *validation) 
{
	static const char zeros[16];

	struct netr_SamBaseInfo *base = NULL;
	switch (validation_level) {
	case 2:
		if (validation->sam2) {
			base = &validation->sam2->base;
		}
		break;
	case 3:
		if (validation->sam3) {
			base = &validation->sam3->base;
		}
		break;
	case 6:
		if (validation->sam6) {
			base = &validation->sam6->base;
		}
		break;
	default:
		/* If we can't find it, we can't very well decrypt it */
		return;
	}

	if (!base) {
		return;
	}

	/* find and decyrpt the session keys, return in parameters above */
	if (validation_level == 6) {
		/* they aren't encrypted! */
	} else if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
		if (memcmp(base->key.key, zeros,  
			   sizeof(base->key.key)) != 0) {
			netlogon_creds_arcfour_crypt(creds, 
					    base->key.key, 
					    sizeof(base->key.key));
		}
			
		if (memcmp(base->LMSessKey.key, zeros,  
			   sizeof(base->LMSessKey.key)) != 0) {
			netlogon_creds_arcfour_crypt(creds, 
					    base->LMSessKey.key, 
					    sizeof(base->LMSessKey.key));
		}
	} else {
		if (memcmp(base->LMSessKey.key, zeros,  
			   sizeof(base->LMSessKey.key)) != 0) {
			netlogon_creds_des_decrypt_LMKey(creds, 
						&base->LMSessKey);
		}
	}
}	

/*
  copy a netlogon_creds_CredentialState struct
*/

struct netlogon_creds_CredentialState *netlogon_creds_copy(TALLOC_CTX *mem_ctx,
							   struct netlogon_creds_CredentialState *creds_in)
{
	struct netlogon_creds_CredentialState *creds = talloc_zero(mem_ctx, struct netlogon_creds_CredentialState);

	if (!creds) {
		return NULL;
	}

	creds->sequence			= creds_in->sequence;
	creds->negotiate_flags		= creds_in->negotiate_flags;
	creds->secure_channel_type	= creds_in->secure_channel_type;

	creds->computer_name = talloc_strdup(creds, creds_in->computer_name);
	if (!creds->computer_name) {
		talloc_free(creds);
		return NULL;
	}
	creds->account_name = talloc_strdup(creds, creds_in->account_name);
	if (!creds->account_name) {
		talloc_free(creds);
		return NULL;
	}

	if (creds_in->sid) {
		creds->sid = dom_sid_dup(creds, creds_in->sid);
		if (!creds->sid) {
			talloc_free(creds);
			return NULL;
		}
	}

	memcpy(creds->session_key, creds_in->session_key, sizeof(creds->session_key));
	memcpy(creds->seed.data, creds_in->seed.data, sizeof(creds->seed.data));
	memcpy(creds->client.data, creds_in->client.data, sizeof(creds->client.data));
	memcpy(creds->server.data, creds_in->server.data, sizeof(creds->server.data));

	return creds;
}
