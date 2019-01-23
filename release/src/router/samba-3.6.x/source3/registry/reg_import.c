/*
 * Samba Unix/Linux SMB client library
 * Adapter to use reg_parse with the registry api
 *
 * Copyright (C) Gregor Beck 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "reg_parse.h"
#include "reg_import.h"
#include "registry.h"
#include "registry/reg_objects.h"
#include <assert.h>

/* Debuglevel for tracing */
static const int TL = 2;

struct reg_import
{
	struct reg_parse_callback reg_parse_callback;
	struct reg_import_callback call;
	void* open_key;
};

static int
reg_parse_callback_key(struct reg_import* cb_private,
		       const char* key[], size_t n,
		       bool del);

static int
reg_parse_callback_val(struct reg_import* cb_private,
		       const char* name, uint32_t type,
		       const uint8_t* data, uint32_t len);

static int
reg_parse_callback_val_registry_value(struct reg_import* cb_private,
				      const char* name, uint32_t type,
				      const uint8_t* data, uint32_t len);

static int
reg_parse_callback_val_regval_blob(struct reg_import* cb_private,
				   const char* name, uint32_t type,
				   const uint8_t* data, uint32_t len);

static int
reg_parse_callback_val_del(struct reg_import* cb_private,
			   const char* name);

static int
reg_parse_callback_comment(struct reg_import* cb_private,
			   const char* txt);


/*******************************************************************************/

int reg_parse_callback_key(struct reg_import* p,
			   const char* key[], size_t n, bool del)
{
	WERROR werr = WERR_OK;

	DEBUG(TL, ("%s: %s\n", __FUNCTION__, key[0]));

	if (p->open_key != NULL ) {
		werr = p->call.closekey(p->call.data, p->open_key);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0, ("closekey failed: %s\n", win_errstr(werr)));
		}
	}

	if (del) {
		werr = p->call.deletekey(p->call.data, NULL, key[0]);
		if (W_ERROR_EQUAL(werr, WERR_BADFILE)) {
			/* the key didn't exist, treat as success */
			werr = WERR_OK;
		}
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0, ("deletekey %s failed: %s\n",
				  key[0], win_errstr(werr)));
		}
	}
	else {
		bool existing;
		werr = p->call.createkey(p->call.data, NULL, key[0],
					 &p->open_key, &existing);
		if (W_ERROR_IS_OK(werr)) {
			DEBUG(TL, ("createkey %s %s\n",
				  existing ? "opened" : "created", key[0]));
		} else {
			DEBUG(0, ("createkey %s failed: %s\n",
				  key[0], win_errstr(werr)));
		}
	}

	return W_ERROR_IS_OK(werr) ? 0 : -1;
}

#define DEBUG_ADD_HEX(LEV, PTR, LEN)					\
	do {								\
		int i;							\
		const unsigned char* ptr = (const unsigned char*)PTR;	\
		for (i=0; i<LEN; i++) {					\
			DEBUGADD(LEV, ("'%c'(%02x)%s",			\
				       isprint(ptr[i]) ? ptr[i] : '.',	\
				       (unsigned)ptr[i],		\
				       ((i+1 < LEN) && (i+1)%8)		\
				       ? ",  " : "\n"));		\
		}							\
	} while(0)

/*----------------------------------------------------------------------------*/
int reg_parse_callback_val(struct reg_import* p,
			   const char* name, uint32_t type,
			   const uint8_t* data, uint32_t len)
{
	WERROR werr = WERR_OK;

	DEBUG(TL, ("%s(%x): >%s< = [%x]\n",  __FUNCTION__, type, name, len));
	DEBUG_ADD_HEX(TL, data, len);

	werr = p->call.setval.blob(p->call.data, p->open_key, name, type,
				   data, len);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("setval %s failed: %s\n",
			  name, win_errstr(werr)));
	}

	return W_ERROR_IS_OK(werr) ? 0 : -1;
}

/*----------------------------------------------------------------------------*/
int reg_parse_callback_val_registry_value(struct reg_import* p,
					  const char* name, uint32_t type,
					  const uint8_t* data, uint32_t len)
{
	WERROR werr = WERR_OK;
	struct registry_value val = {
		.type = type,
		.data = data_blob_talloc(p, data, len),
	};

	DEBUG(TL, ("%s(%x): >%s< = [%x]\n", __FUNCTION__, type, name, len));
	DEBUG_ADD_HEX(TL, data, len);

	werr = p->call.setval.registry_value(p->call.data, p->open_key,
					     name, &val);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("setval %s failed: %s\n",
			  name, win_errstr(werr)));
	}

	data_blob_free(&val.data);
	return W_ERROR_IS_OK(werr) ? 0 : -1;
}

/*----------------------------------------------------------------------------*/
int reg_parse_callback_val_regval_blob(struct reg_import* p,
				       const char* name, uint32_t type,
				       const uint8_t* data, uint32_t len)
{
	WERROR werr = WERR_OK;
	void* mem_ctx = talloc_new(p);
	struct regval_blob* v = NULL;

	DEBUG(TL, ("%s(%x): >%s< = [%x]\n", __FUNCTION__, type, name, len));
	DEBUG_ADD_HEX(TL, data, len);

	v = regval_compose(mem_ctx, name, type, data, len);
	if (v == NULL) {
		DEBUG(0, ("regval_compose %s failed\n", name));
		werr = WERR_NOMEM;
		goto done;
	}

	werr = p->call.setval.regval_blob(p->call.data, p->open_key, v);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("setval %s failed: %s\n",
			  name, win_errstr(werr)));
	}

done:
	talloc_free(mem_ctx);

	return W_ERROR_IS_OK(werr) ? 0 : -1;
}


/*----------------------------------------------------------------------------*/

int reg_parse_callback_val_del(struct reg_import* p,
			       const char* name)
{
	WERROR werr = WERR_OK;

	DEBUG(TL, ("%s: %s\n", __FUNCTION__, name));

	werr = p->call.deleteval(p->call.data, p->open_key, name);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("deleteval %s failed: %s\n",
			  name, win_errstr(werr)));
	}

	return W_ERROR_IS_OK(werr) ? 0 : -1;
}


int reg_parse_callback_comment(struct reg_import* cb_private,
			       const char* txt)
{
	DEBUG(TL, ("%s: %s\n", __FUNCTION__, txt));
	return 0;
}

/******************************************************************************/
static int nop(void* data)
{
	return 0;
}


struct reg_parse_callback* reg_import_adapter(const void* talloc_ctx,
					      struct reg_import_callback cb)
{
	struct reg_parse_callback* ret;
	struct reg_import* p = talloc_zero(talloc_ctx, struct reg_import);
	if (p == NULL) {
		goto fail;
	}
	if (cb.openkey == NULL ) {
		cb.openkey = (reg_import_callback_openkey_t)&nop;
	}
	if (cb.closekey == NULL ) {
		cb.closekey = (reg_import_callback_closekey_t)&nop;
	}
	if (cb.createkey == NULL ) {
		cb.createkey = (reg_import_callback_createkey_t)&nop;
	}
	if (cb.deletekey == NULL ) {
		cb.deletekey = (reg_import_callback_deletekey_t)&nop;
	}
	if (cb.deleteval == NULL ) {
		cb.deleteval = (reg_import_callback_deleteval_t)&nop;
	}

	p->call = cb;

	ret = &p->reg_parse_callback;
	ret->key     = (reg_parse_callback_key_t)     &reg_parse_callback_key;
	ret->val_del = (reg_parse_callback_val_del_t) &reg_parse_callback_val_del;
	ret->comment = (reg_parse_callback_comment_t) &reg_parse_callback_comment;
	ret->data = p;

	switch (cb.setval_type) {
	case BLOB:
		assert(cb.setval.blob != NULL);
		ret->val = (reg_parse_callback_val_t) &reg_parse_callback_val;
		break;
	case REGISTRY_VALUE:
		assert(cb.setval.registry_value != NULL);
		ret->val = (reg_parse_callback_val_t) &reg_parse_callback_val_registry_value;
		break;
	case REGVAL_BLOB:
		assert(cb.setval.regval_blob != NULL);
		ret->val = (reg_parse_callback_val_t) &reg_parse_callback_val_regval_blob;
		break;
	case NONE:
		ret->val = NULL;
		break;
	default:
		assert(false);
	}

	assert((struct reg_parse_callback*)p == ret);
	return ret;
fail:
	talloc_free(p);
	return NULL;
}
