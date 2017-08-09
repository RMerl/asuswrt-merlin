/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mech_locl.h"

static int
get_option_def(int def, gss_const_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    return def;
}


int
_gss_mo_get_option_1(gss_const_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    return get_option_def(1, mech, mo, value);
}

int
_gss_mo_get_option_0(gss_const_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    return get_option_def(0, mech, mo, value);
}

int
_gss_mo_get_ctx_as_string(gss_const_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    if (value) {
	value->value = strdup((char *)mo->ctx);
	if (value->value == NULL)
	    return 1;
	value->length = strlen((char *)mo->ctx);
    }
    return 0;
}

GSSAPI_LIB_FUNCTION int GSSAPI_LIB_CALL
gss_mo_set(gss_const_OID mech, gss_const_OID option,
	   int enable, gss_buffer_t value)
{
    gssapi_mech_interface m;
    size_t n;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return GSS_S_BAD_MECH;

    for (n = 0; n < m->gm_mo_num; n++)
	if (gss_oid_equal(option, m->gm_mo[n].option) && m->gm_mo[n].set)
	    return m->gm_mo[n].set(mech, &m->gm_mo[n], enable, value);
    return 0;
}

GSSAPI_LIB_FUNCTION int GSSAPI_LIB_CALL
gss_mo_get(gss_const_OID mech, gss_const_OID option, gss_buffer_t value)
{
    gssapi_mech_interface m;
    size_t n;

    _mg_buffer_zero(value);

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return 0;

    for (n = 0; n < m->gm_mo_num; n++)
	if (gss_oid_equal(option, m->gm_mo[n].option) && m->gm_mo[n].get)
	    return m->gm_mo[n].get(mech, &m->gm_mo[n], value);

    return 0;
}

static void
add_all_mo(gssapi_mech_interface m, gss_OID_set *options, OM_uint32 mask)
{
    OM_uint32 minor;
    size_t n;

    for (n = 0; n < m->gm_mo_num; n++)
	if ((m->gm_mo[n].flags & mask) == mask)
	    gss_add_oid_set_member(&minor, m->gm_mo[n].option, options);
}

GSSAPI_LIB_FUNCTION void GSSAPI_LIB_CALL
gss_mo_list(gss_const_OID mech, gss_OID_set *options)
{
    gssapi_mech_interface m;
    OM_uint32 major, minor;

    if (options == NULL)
	return;

    *options = GSS_C_NO_OID_SET;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return;

    major = gss_create_empty_oid_set(&minor, options);
    if (major != GSS_S_COMPLETE)
	return;

    add_all_mo(m, options, 0);
}

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_mo_name(gss_const_OID mech, gss_const_OID option, gss_buffer_t name)
{
    gssapi_mech_interface m;
    size_t n;

    if (name == NULL)
	return GSS_S_BAD_NAME;

    if ((m = __gss_get_mechanism(mech)) == NULL)
	return GSS_S_BAD_MECH;

    for (n = 0; n < m->gm_mo_num; n++) {
	if (gss_oid_equal(option, m->gm_mo[n].option)) {
	    /*
	     * If ther is no name, its because its a GSS_C_MA and there is already a table for that.
	     */
	    if (m->gm_mo[n].name) {
		name->value = strdup(m->gm_mo[n].name);
		if (name->value == NULL)
		    return GSS_S_BAD_NAME;
		name->length = strlen(m->gm_mo[n].name);
		return GSS_S_COMPLETE;
	    } else {
		OM_uint32 junk;
		return gss_display_mech_attr(&junk, option,
					     NULL, name, NULL);
	    }
	}
    }
    return GSS_S_BAD_NAME;
}

/*
 * Helper function to allow NULL name
 */

static OM_uint32
mo_value(const gss_const_OID mech, gss_const_OID option, gss_buffer_t name)
{
    if (name == NULL)
	return GSS_S_COMPLETE;

    if (gss_mo_get(mech, option, name) != 0 && name->length == 0)
	return GSS_S_FAILURE;

    return GSS_S_COMPLETE;
}

/**
 * Returns differnt protocol names and description of the mechanism.
 *
 * @param minor_status minor status code
 * @param desired_mech mech list query
 * @param sasl_mech_name SASL GS2 protocol name
 * @param mech_name gssapi protocol name
 * @param mech_description description of gssapi mech
 *
 * @return returns GSS_S_COMPLETE or a error code.
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_inquire_saslname_for_mech(OM_uint32 *minor_status,
			      const gss_OID desired_mech,
			      gss_buffer_t sasl_mech_name,
			      gss_buffer_t mech_name,
			      gss_buffer_t mech_description)
{
    OM_uint32 major;

    _mg_buffer_zero(sasl_mech_name);
    _mg_buffer_zero(mech_name);
    _mg_buffer_zero(mech_description);

    if (minor_status)
	*minor_status = 0;

    if (desired_mech == NULL)
	return GSS_S_BAD_MECH;

    major = mo_value(desired_mech, GSS_C_MA_SASL_MECH_NAME, sasl_mech_name);
    if (major) return major;

    major = mo_value(desired_mech, GSS_C_MA_MECH_NAME, mech_name);
    if (major) return major;

    major = mo_value(desired_mech, GSS_C_MA_MECH_DESCRIPTION, mech_description);
    if (major) return major;

    return GSS_S_COMPLETE;
}

/**
 * Find a mech for a sasl name
 *
 * @param minor_status minor status code
 * @param sasl_mech_name
 * @param mech_type
 *
 * @return returns GSS_S_COMPLETE or an error code.
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_inquire_mech_for_saslname(OM_uint32 *minor_status,
			      const gss_buffer_t sasl_mech_name,
			      gss_OID *mech_type)
{
    struct _gss_mech_switch *m;
    gss_buffer_desc name;
    OM_uint32 major;

    _gss_load_mech();

    *mech_type = NULL;

    HEIM_SLIST_FOREACH(m, &_gss_mechs, gm_link) {

	major = mo_value(&m->gm_mech_oid, GSS_C_MA_SASL_MECH_NAME, &name);
	if (major)
	    continue;
	if (name.length == sasl_mech_name->length &&
	    memcmp(name.value, sasl_mech_name->value, name.length) == 0) {
	    gss_release_buffer(&major, &name);
	    *mech_type = &m->gm_mech_oid;
	    return 0;
	}
	gss_release_buffer(&major, &name);
    }

    return GSS_S_BAD_MECH;
}

/**
 * Return set of mechanism that fullfill the criteria
 *
 * @param minor_status minor status code
 * @param desired_mech_attrs
 * @param except_mech_attrs
 * @param critical_mech_attrs
 * @param mechs returned mechs, free with gss_release_oid_set().
 *
 * @return returns GSS_S_COMPLETE or an error code.
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_indicate_mechs_by_attrs(OM_uint32 * minor_status,
			    gss_const_OID_set desired_mech_attrs,
			    gss_const_OID_set except_mech_attrs,
			    gss_const_OID_set critical_mech_attrs,
			    gss_OID_set *mechs)
{
    struct _gss_mech_switch *ms;
    OM_uint32 major;
    size_t n, m;

    major = gss_create_empty_oid_set(minor_status, mechs);
    if (major)
	return major;

    _gss_load_mech();

    HEIM_SLIST_FOREACH(ms, &_gss_mechs, gm_link) {
	gssapi_mech_interface mi = &ms->gm_mech;

	if (desired_mech_attrs) {
	    for (n = 0; n < desired_mech_attrs->count; n++) {
		for (m = 0; m < mi->gm_mo_num; m++)
		    if (gss_oid_equal(mi->gm_mo[m].option, &desired_mech_attrs->elements[n]))
			break;
		if (m == mi->gm_mo_num)
		    goto next;
	    }
	}

	if (except_mech_attrs) {
	    for (n = 0; n < desired_mech_attrs->count; n++) {
		for (m = 0; m < mi->gm_mo_num; m++) {
		    if (gss_oid_equal(mi->gm_mo[m].option, &desired_mech_attrs->elements[n]))
			goto next;
		}
	    }
	}

	if (critical_mech_attrs) {
	    for (n = 0; n < desired_mech_attrs->count; n++) {
		for (m = 0; m < mi->gm_mo_num; m++) {
		    if (mi->gm_mo[m].flags & GSS_MO_MA_CRITICAL)
			continue;
		    if (gss_oid_equal(mi->gm_mo[m].option, &desired_mech_attrs->elements[n]))
			break;
		}
		if (m == mi->gm_mo_num)
		    goto next;
	    }
	}


    next:
	do { } while(0);
    }


    return GSS_S_FAILURE;
}

/**
 * List support attributes for a mech and/or all mechanisms.
 *
 * @param minor_status minor status code
 * @param mech given together with mech_attr will return the list of
 *        attributes for mechanism, can optionally be GSS_C_NO_OID.
 * @param mech_attr see mech parameter, can optionally be NULL,
 *        release with gss_release_oid_set().
 * @param known_mech_attrs all attributes for mechanisms supported,
 *        release with gss_release_oid_set().
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_inquire_attrs_for_mech(OM_uint32 * minor_status,
			   gss_const_OID mech,
			   gss_OID_set *mech_attr,
			   gss_OID_set *known_mech_attrs)
{
    OM_uint32 major, junk;

    if (mech_attr && mech) {
	gssapi_mech_interface m;

	if ((m = __gss_get_mechanism(mech)) == NULL) {
	    *minor_status = 0;
	    return GSS_S_BAD_MECH;
	}

	major = gss_create_empty_oid_set(minor_status, mech_attr);
	if (major != GSS_S_COMPLETE)
	    return major;

	add_all_mo(m, mech_attr, GSS_MO_MA);
    }    

    if (known_mech_attrs) {
	struct _gss_mech_switch *m;

	major = gss_create_empty_oid_set(minor_status, known_mech_attrs);
	if (major) {
	    if (mech_attr)
		gss_release_oid_set(&junk, mech_attr);
	    return major;
	}

	_gss_load_mech();

	HEIM_SLIST_FOREACH(m, &_gss_mechs, gm_link)
	    add_all_mo(&m->gm_mech, known_mech_attrs, GSS_MO_MA);
    }


    return GSS_S_COMPLETE;
}

/**
 * Return names and descriptions of mech attributes
 *
 * @param minor_status minor status code
 * @param mech_attr
 * @param name
 * @param short_desc
 * @param long_desc
 *
 * @return returns GSS_S_COMPLETE or an error code.
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_display_mech_attr(OM_uint32 * minor_status,
		      gss_const_OID mech_attr,
		      gss_buffer_t name,
		      gss_buffer_t short_desc,
		      gss_buffer_t long_desc)
{
    struct _gss_oid_name_table *ma = NULL;
    OM_uint32 major;
    size_t n;

    _mg_buffer_zero(name);
    _mg_buffer_zero(short_desc);
    _mg_buffer_zero(long_desc);

    if (minor_status)
	*minor_status = 0;

    for (n = 0; ma == NULL && _gss_ont_ma[n].oid; n++)
	if (gss_oid_equal(mech_attr, _gss_ont_ma[n].oid))
	    ma = &_gss_ont_ma[n];

    if (ma == NULL)
	return GSS_S_BAD_MECH_ATTR;

    if (name) {
	gss_buffer_desc n;
	n.value = rk_UNCONST(ma->name);
	n.length = strlen(ma->name);
	major = _gss_copy_buffer(minor_status, &n, name);
	if (major != GSS_S_COMPLETE)
	    return major;
    }

    if (short_desc) {
	gss_buffer_desc n;
	n.value = rk_UNCONST(ma->short_desc);
	n.length = strlen(ma->short_desc);
	major = _gss_copy_buffer(minor_status, &n, short_desc);
	if (major != GSS_S_COMPLETE)
	    return major;
    }

    if (long_desc) {
	gss_buffer_desc n;
	n.value = rk_UNCONST(ma->long_desc);
	n.length = strlen(ma->long_desc);
	major = _gss_copy_buffer(minor_status, &n, long_desc);
	if (major != GSS_S_COMPLETE)
	    return major;
    }

    return GSS_S_COMPLETE;
}
