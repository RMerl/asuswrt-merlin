# NOTE! this whole m4 file is disabled in configure.in for now

#################################################
# KRB5 support
KRB5_CFLAGS=""
KRB5_CPPFLAGS=""
KRB5_LDFLAGS=""
KRB5_LIBS=""
with_krb5_support=auto
krb5_withval=auto
AC_MSG_CHECKING([for KRB5 support])

# Do no harm to the values of CFLAGS and LIBS while testing for
# Kerberos support.
AC_ARG_WITH(krb5,
[  --with-krb5=base-dir    Locate Kerberos 5 support (default=auto)],
    	[ case "$withval" in
		no)
        		with_krb5_support=no
        		AC_MSG_RESULT(no)
        		krb5_withval=no
			;;
		yes)
      			with_krb5_support=yes
        		AC_MSG_RESULT(yes)
        		krb5_withval=yes
			;;
		auto)
      			with_krb5_support=auto
        		AC_MSG_RESULT(auto)
        		krb5_withval=auto
			;;
		*)
			with_krb5_support=yes
			AC_MSG_RESULT(yes)
			krb5_withval=$withval
			KRB5CONFIG="$krb5_withval/bin/krb5-config"
        		;;
	esac ],
	AC_MSG_RESULT($with_krb5_support)
)

if test x$with_krb5_support != x"no"; then
	FOUND_KRB5=no
	FOUND_KRB5_VIA_CONFIG=no

	#################################################
	# check for krb5-config from recent MIT and Heimdal kerberos 5
	AC_MSG_CHECKING(for working specified location for krb5-config)
	if test x$KRB5CONFIG != "x"; then
		if test -x "$KRB5CONFIG"; then
			ac_save_CFLAGS=$CFLAGS
			CFLAGS="";export CFLAGS
			ac_save_LDFLAGS=$LDFLAGS
			LDFLAGS="";export LDFLAGS
			KRB5_LIBS="`$KRB5CONFIG --libs gssapi`"
			KRB5_CFLAGS="`$KRB5CONFIG --cflags | sed s/@INCLUDE_des@//`" 
			KRB5_CPPFLAGS="`$KRB5CONFIG --cflags | sed s/@INCLUDE_des@//`"
			CFLAGS=$ac_save_CFLAGS;export CFLAGS
			LDFLAGS=$ac_save_LDFLAGS;export LDFLAGS
			FOUND_KRB5=yes
			FOUND_KRB5_VIA_CONFIG=yes
			AC_MSG_RESULT(yes. Found $KRB5CONFIG)
		else 
			AC_MSG_RESULT(no. Fallback to specified directory)
		fi
	else
		AC_MSG_RESULT(no. Fallback to finding krb5-config in path)
		#################################################
		# check for krb5-config from recent MIT and Heimdal kerberos 5
		AC_PATH_PROG(KRB5CONFIG, krb5-config)
		AC_MSG_CHECKING(for working krb5-config in path)
		if test -x "$KRB5CONFIG"; then
			ac_save_CFLAGS=$CFLAGS
			CFLAGS="";export CFLAGS
			ac_save_LDFLAGS=$LDFLAGS
			LDFLAGS="";export LDFLAGS
			KRB5_LIBS="`$KRB5CONFIG --libs gssapi`"
			KRB5_CFLAGS="`$KRB5CONFIG --cflags | sed s/@INCLUDE_des@//`" 
			KRB5_CPPFLAGS="`$KRB5CONFIG --cflags | sed s/@INCLUDE_des@//`"
			CFLAGS=$ac_save_CFLAGS;export CFLAGS
			LDFLAGS=$ac_save_LDFLAGS;export LDFLAGS
			FOUND_KRB5=yes
			FOUND_KRB5_VIA_CONFIG=yes
			AC_MSG_RESULT(yes. Found $KRB5CONFIG)
		else
			AC_MSG_RESULT(no. Fallback to previous krb5 detection strategy)
		fi
	fi
  
	if test x$FOUND_KRB5 != x"yes"; then
		#################################################
		# check for location of Kerberos 5 install
		AC_MSG_CHECKING(for kerberos 5 install path)
		case "$krb5_withval" in
			no)
				AC_MSG_RESULT(no krb5-path given)
				;;
			yes)
				AC_MSG_RESULT(/usr)
				FOUND_KRB5=yes
				;;
			*)
				AC_MSG_RESULT($krb5_withval)
				KRB5_CFLAGS="-I$krb5_withval/include"
				KRB5_CPPFLAGS="-I$krb5_withval/include"
				KRB5_LDFLAGS="-L$krb5_withval/lib"
				FOUND_KRB5=yes
				;;
		esac
	fi

	if test x$FOUND_KRB5 != x"yes"; then
		#################################################
		# see if this box has the SuSE location for the heimdal krb implementation
		AC_MSG_CHECKING(for /usr/include/heimdal)
		if test -d /usr/include/heimdal; then
			if test -f /usr/lib/heimdal/lib/libkrb5.a; then
				KRB5_CFLAGS="-I/usr/include/heimdal"
				KRB5_CPPFLAGS="-I/usr/include/heimdal"
				KRB5_LDFLAGS="-L/usr/lib/heimdal/lib"
				AC_MSG_RESULT(yes)
			else
				KRB5_CFLAGS="-I/usr/include/heimdal"
				KRB5_CPPFLAGS="-I/usr/include/heimdal"
				AC_MSG_RESULT(yes)
			fi
		else
			AC_MSG_RESULT(no)
		fi
	fi

	if test x$FOUND_KRB5 != x"yes"; then
		#################################################
		# see if this box has the RedHat location for kerberos
		AC_MSG_CHECKING(for /usr/kerberos)
		if test -d /usr/kerberos -a -f /usr/kerberos/lib/libkrb5.a; then
			KRB5_LDFLAGS="-L/usr/kerberos/lib"
			KRB5_CFLAGS="-I/usr/kerberos/include"
			KRB5_CPPFLAGS="-I/usr/kerberos/include"
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi
	fi

	ac_save_CFLAGS=$CFLAGS
	ac_save_CPPFLAGS=$CPPFLAGS
	ac_save_LDFLAGS=$LDFLAGS

	#MIT needs this, to let us see 'internal' parts of the headers we use
	KRB5_CFLAGS="${KRB5_CFLAGS} -DKRB5_PRIVATE -DKRB5_DEPRECATED"

	#Heimdal needs this
	#TODO: we need to parse KRB5_LIBS for -L path
	#      and set -Wl,-rpath -Wl,path

	CFLAGS="$CFLAGS $KRB5_CFLAGS"
	CPPFLAGS="$CPPFLAGS $KRB5_CPPFLAGS"
	LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"

	KRB5_LIBS="$KRB5_LDFLAGS $KRB5_LIBS"

	# now check for krb5.h. Some systems have the libraries without the headers!
	# note that this check is done here to allow for different kerberos
	# include paths
	AC_CHECK_HEADERS(krb5.h)

	if test x"$ac_cv_header_krb5_h" = x"no"; then
		# Give a warning if KRB5 support was not explicitly requested,
		# i.e with_krb5_support = auto, otherwise die with an error.
		if test x"$with_krb5_support" = x"yes"; then
			AC_MSG_ERROR([KRB5 cannot be supported without krb5.h])
		else
			AC_MSG_WARN([KRB5 cannot be supported without krb5.h])
		fi
		# Turn off AD support and restore CFLAGS and LIBS variables
		with_krb5_support="no"
	fi

	CFLAGS=$ac_save_CFLAGS
	CPPFLAGS=$ac_save_CPPFLAGS
	LDFLAGS=$ac_save_LDFLAGS
fi

# Now we have determined whether we really want KRB5 support

if test x"$with_krb5_support" != x"no"; then
	ac_save_CFLAGS=$CFLAGS
	ac_save_CPPFLAGS=$CPPFLAGS
	ac_save_LDFLAGS=$LDFLAGS
	ac_save_LIBS=$LIBS

	CFLAGS="$CFLAGS $KRB5_CFLAGS"
	CPPFLAGS="$CPPFLAGS $KRB5_CPPFLAGS"
	LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"

	# now check for gssapi headers.  This is also done here to allow for
	# different kerberos include paths
	AC_CHECK_HEADERS(gssapi.h gssapi_krb5.h gssapi/gssapi.h gssapi/gssapi_generic.h gssapi/gssapi_krb5.h com_err.h)


	# Heimdal checks.
	# But only if we didn't have a krb5-config to tell us this already
	if test x"$FOUND_KRB5_VIA_CONFIG" != x"yes"; then
		##################################################################
		# we might need the k5crypto and com_err libraries on some systems
 		AC_CHECK_LIB_EXT(com_err, KRB5_LIBS, _et_list)
		AC_CHECK_LIB_EXT(k5crypto, KRB5_LIBS, krb5_encrypt_data)

		AC_CHECK_LIB_EXT(crypto, KRB5_LIBS, des_set_key)
		AC_CHECK_LIB_EXT(asn1, KRB5_LIBS, copy_Authenticator)
		AC_CHECK_LIB_EXT(roken, KRB5_LIBS, roken_getaddrinfo_hostspec)
	fi

	# Heimdal checks. On static Heimdal gssapi must be linked before krb5.
	AC_CHECK_LIB_EXT(gssapi, KRB5_LIBS, gss_display_status,[],[],
				AC_DEFINE(HAVE_GSSAPI,1,[Whether GSSAPI is available]))

	########################################################
	# now see if we can find the krb5 libs in standard paths
	# or as specified above
	AC_CHECK_LIB_EXT(krb5, KRB5_LIBS, krb5_mk_req_extended)
	AC_CHECK_LIB_EXT(krb5, KRB5_LIBS, krb5_kt_compare)

	########################################################
	# now see if we can find the gssapi libs in standard paths
	if test x"$ac_cv_lib_ext_gssapi_gss_display_status" != x"yes"; then
	   AC_CHECK_LIB_EXT(gssapi_krb5, KRB5_LIBS,gss_display_status,[],[],
		AC_DEFINE(HAVE_GSSAPI,1,[Whether GSSAPI is available]))
        fi

	AC_CHECK_FUNC_EXT(krb5_set_real_time, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_set_default_in_tkt_etypes, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_set_default_tgs_ktypes, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_principal2salt, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_use_enctype, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_string_to_key, $KRB5_LIBS) 
	AC_CHECK_FUNC_EXT(krb5_get_pw_salt, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_string_to_key_salt, $KRB5_LIBS) 
	AC_CHECK_FUNC_EXT(krb5_auth_con_setkey, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_auth_con_setuseruserkey, $KRB5_LIBS) 
	AC_CHECK_FUNC_EXT(krb5_locate_kdc, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_get_permitted_enctypes, $KRB5_LIBS) 
	AC_CHECK_FUNC_EXT(krb5_get_default_in_tkt_etypes, $KRB5_LIBS) 
	AC_CHECK_FUNC_EXT(krb5_free_ktypes, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_free_data_contents, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_principal_get_comp_string, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_free_unparsed_name, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_free_keytab_entry_contents, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_kt_free_entry, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_krbhst_get_addrinfo, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_verify_checksum, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_c_verify_checksum, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_ticket_get_authorization_data_type, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_krbhst_get_addrinfo, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_c_enctype_compare, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_enctypes_compatible_keys, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_get_error_string, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_free_error_string, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_get_error_message, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_free_error_message, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_initlog, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_addlog_func, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(krb5_set_warn_dest, $KRB5_LIBS)

	LIBS="$LIBS $KRB5_LIBS"
  
	AC_CACHE_CHECK([for krb5_log_facility type],
                samba_cv_HAVE_KRB5_LOG_FACILITY,[
	AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_log_facility block;],
		samba_cv_HAVE_KRB5_LOG_FACILITY=yes,
		samba_cv_HAVE_KRB5_LOG_FACILITY=no)])

	if test x"$samba_cv_HAVE_KRB5_LOG_FACILITY" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_LOG_FACILITY,1,
		[Whether the type krb5_log_facility exists])
	fi

	AC_CACHE_CHECK([for krb5_encrypt_block type],
                samba_cv_HAVE_KRB5_ENCRYPT_BLOCK,[
	AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_encrypt_block block;],
		samba_cv_HAVE_KRB5_ENCRYPT_BLOCK=yes,
		samba_cv_HAVE_KRB5_ENCRYPT_BLOCK=no)])

	if test x"$samba_cv_HAVE_KRB5_ENCRYPT_BLOCK" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_ENCRYPT_BLOCK,1,
		[Whether the type krb5_encrypt_block exists])
	fi

	AC_CACHE_CHECK([for addrtype in krb5_address],
		samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_address kaddr; kaddr.addrtype = ADDRTYPE_INET;],
		samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS=yes,
		samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS=no)])
	if test x"$samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS" = x"yes"; then
		AC_DEFINE(HAVE_ADDRTYPE_IN_KRB5_ADDRESS,1,
		[Whether the krb5_address struct has a addrtype property])
	fi

	AC_CACHE_CHECK([for addr_type in krb5_address],
		samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_address kaddr; kaddr.addr_type = KRB5_ADDRESS_INET;],
		samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS=yes,
		samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS=no)])
	if test x"$samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS" = x"yes"; then
		AC_DEFINE(HAVE_ADDR_TYPE_IN_KRB5_ADDRESS,1,
		[Whether the krb5_address struct has a addr_type property])
	fi

	AC_CACHE_CHECK([for enc_part2 in krb5_ticket], 
		samba_cv_HAVE_KRB5_TKT_ENC_PART2,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_ticket tkt; tkt.enc_part2->authorization_data[0]->contents = NULL;],
		samba_cv_HAVE_KRB5_TKT_ENC_PART2=yes,
		samba_cv_HAVE_KRB5_TKT_ENC_PART2=no)])
	if test x"$samba_cv_HAVE_KRB5_TKT_ENC_PART2" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_TKT_ENC_PART2,1,
		[Whether the krb5_ticket struct has a enc_part2 property])
	fi

	AC_CACHE_CHECK([for keyblock in krb5_creds],
                 samba_cv_HAVE_KRB5_KEYBLOCK_IN_CREDS,[
	AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_creds creds; krb5_keyblock kb; creds.keyblock = kb;],
		samba_cv_HAVE_KRB5_KEYBLOCK_IN_CREDS=yes,
		samba_cv_HAVE_KRB5_KEYBLOCK_IN_CREDS=no)])

	if test x"$samba_cv_HAVE_KRB5_KEYBLOCK_IN_CREDS" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_KEYBLOCK_IN_CREDS,1,
		[Whether the krb5_creds struct has a keyblock property])
	fi

	AC_CACHE_CHECK([for session in krb5_creds],
                 samba_cv_HAVE_KRB5_SESSION_IN_CREDS,[
	AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_creds creds; krb5_keyblock kb; creds.session = kb;],
		samba_cv_HAVE_KRB5_SESSION_IN_CREDS=yes,
		samba_cv_HAVE_KRB5_SESSION_IN_CREDS=no)])

	if test x"$samba_cv_HAVE_KRB5_SESSION_IN_CREDS" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_SESSION_IN_CREDS,1,
		[Whether the krb5_creds struct has a session property])
	fi

	AC_CACHE_CHECK([for keyvalue in krb5_keyblock],
		samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_keyblock key; key.keyvalue.data = NULL;],
		samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE=yes,
		samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE=no)])
	if test x"$samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_KEYBLOCK_KEYVALUE,1,
		[Whether the krb5_keyblock struct has a keyvalue property])
	fi

	AC_CACHE_CHECK([for ENCTYPE_ARCFOUR_HMAC_MD5],
		samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_enctype enctype; enctype = ENCTYPE_ARCFOUR_HMAC_MD5;],
		samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5=yes,
		samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5=no)])
	AC_CACHE_CHECK([for KEYTYPE_ARCFOUR_56],
                 samba_cv_HAVE_KEYTYPE_ARCFOUR_56,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_keytype keytype; keytype = KEYTYPE_ARCFOUR_56;],
		samba_cv_HAVE_KEYTYPE_ARCFOUR_56=yes,
		samba_cv_HAVE_KEYTYPE_ARCFOUR_56=no)])
	# Heimdals with KEYTYPE_ARCFOUR but not KEYTYPE_ARCFOUR_56 are broken
	# w.r.t. arcfour and windows, so we must not enable it here
	if test x"$samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5" = x"yes" -a\
	   x"$samba_cv_HAVE_KEYTYPE_ARCFOUR_56" = x"yes"; then
		AC_DEFINE(HAVE_ENCTYPE_ARCFOUR_HMAC_MD5,1,
		[Whether the ENCTYPE_ARCFOUR_HMAC_MD5 key type is available])
	fi

	AC_CACHE_CHECK([for AP_OPTS_USE_SUBKEY],
		samba_cv_HAVE_AP_OPTS_USE_SUBKEY,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_flags ap_options; ap_options = AP_OPTS_USE_SUBKEY;],
		samba_cv_HAVE_AP_OPTS_USE_SUBKEY=yes,
		samba_cv_HAVE_AP_OPTS_USE_SUBKEY=no)])
	if test x"$samba_cv_HAVE_AP_OPTS_USE_SUBKEY" = x"yes"; then
		AC_DEFINE(HAVE_AP_OPTS_USE_SUBKEY,1,
		[Whether the AP_OPTS_USE_SUBKEY ap option is available])
	fi

	AC_CACHE_CHECK([for KV5M_KEYTAB],
		samba_cv_HAVE_KV5M_KEYTAB,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_keytab_entry entry; entry.magic = KV5M_KEYTAB;],
		samba_cv_HAVE_KV5M_KEYTAB=yes,
		samba_cv_HAVE_KV5M_KEYTAB=no)])
	if test x"$samba_cv_HAVE_KV5M_KEYTAB" = x"yes"; then
		AC_DEFINE(HAVE_KV5M_KEYTAB,1,
		[Whether the KV5M_KEYTAB option is available])
	fi

	AC_CACHE_CHECK([for the krb5_princ_component macro],
		samba_cv_HAVE_KRB5_PRINC_COMPONENT,[
		AC_TRY_LINK([#include <krb5.h>],
		[const krb5_data *pkdata; krb5_context context; krb5_principal principal; 
			pkdata = krb5_princ_component(context, principal, 0);],
		samba_cv_HAVE_KRB5_PRINC_COMPONENT=yes,
		samba_cv_HAVE_KRB5_PRINC_COMPONENT=no)])
	if test x"$samba_cv_HAVE_KRB5_PRINC_COMPONENT" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_PRINC_COMPONENT,1,
               [Whether krb5_princ_component is available])
	fi

	AC_CACHE_CHECK([for key in krb5_keytab_entry],
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_keytab_entry entry; krb5_keyblock e; entry.key = e;],
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY=yes,
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY=no)])
	if test x"$samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_KEYTAB_ENTRY_KEY,1,
		[Whether krb5_keytab_entry has key member])
	fi

	AC_CACHE_CHECK([for keyblock in krb5_keytab_entry],
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_keytab_entry entry; entry.keyblock.keytype = 0;],
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK=yes,
		samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK=no)])
	if test x"$samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK" = x"yes"; then
		AC_DEFINE(HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK,1,
		[Whether krb5_keytab_entry has keyblock member])
	fi

	AC_CACHE_CHECK([for WRFILE: keytab support],
                samba_cv_HAVE_WRFILE_KEYTAB,[
		AC_TRY_RUN([
		#include<krb5.h>
		main()
		{
			krb5_context context;
			krb5_keytab keytab;
			krb5_init_context(&context);
			return krb5_kt_resolve(context, "WRFILE:api", &keytab);
		}],
		samba_cv_HAVE_WRFILE_KEYTAB=yes,
		samba_cv_HAVE_WRFILE_KEYTAB=no)])
	if test x"$samba_cv_HAVE_WRFILE_KEYTAB" = x"yes"; then
		AC_DEFINE(HAVE_WRFILE_KEYTAB,1,
		[Whether the WRFILE:-keytab is supported])
	fi

	AC_CACHE_CHECK([for krb5_princ_realm returns krb5_realm or krb5_data],
    		samba_cv_KRB5_PRINC_REALM_RETURNS_REALM,[
		AC_TRY_COMPILE([#include <krb5.h>],
		[krb5_context context;krb5_principal principal;krb5_realm realm;
			realm = *krb5_princ_realm(context, principal);],
		samba_cv_KRB5_PRINC_REALM_RETURNS_REALM=yes,
		samba_cv_KRB5_PRINC_REALM_RETURNS_REALM=no)])
	if test x"$samba_cv_KRB5_PRINC_REALM_RETURNS_REALM" = x"yes"; then
		AC_DEFINE(KRB5_PRINC_REALM_RETURNS_REALM,1,
		[Whether krb5_princ_realm returns krb5_realm or krb5_data])
	fi

	# TODO: check all gssapi headers for this
	AC_CACHE_CHECK([for GSS_C_DCE_STYLE in gssapi.h],
    		samba_cv_GSS_C_DCE_STYLE,[
		AC_TRY_COMPILE([#include <gssapi.h>],
		[int flags = GSS_C_DCE_STYLE;],
		samba_cv_GSS_C_DCE_STYLE=yes,
		samba_cv_GSS_C_DCE_STYLE=no)])

	AC_CHECK_FUNC_EXT(gsskrb5_get_initiator_subkey, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(gsskrb5_extract_authz_data_from_sec_context, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(gsskrb5_register_acceptor_identity, $KRB5_LIBS)
	AC_CHECK_FUNC_EXT(gss_krb5_ccache_name, $KRB5_LIBS)
	if test x"$ac_cv_lib_ext_krb5_krb5_mk_req_extended" = x"yes"; then
		AC_DEFINE(HAVE_KRB5,1,[Whether to have KRB5 support])
		AC_MSG_CHECKING(whether KRB5 support is used)
		SMB_ENABLE(KRB5,YES)
		AC_MSG_RESULT(yes)
		echo "KRB5_CFLAGS:   ${KRB5_CFLAGS}"
		echo "KRB5_CPPFLAGS: ${KRB5_CPPFLAGS}"
		echo "KRB5_LDFLAGS:  ${KRB5_LDFLAGS}"
		echo "KRB5_LIBS:     ${KRB5_LIBS}"
	else
		if test x"$with_krb5_support" = x"yes"; then
			AC_MSG_ERROR(a working krb5 library is needed for KRB5 support)
		else
			AC_MSG_WARN(a working krb5 library is needed for KRB5 support)
		fi
		KRB5_CFLAGS=""
		KRB5_CPPFLAGS=""
		KRB5_LDFLAGS=""
		KRB5_LIBS=""
		with_krb5_support=no 
	fi

	# checks if we have access to a libkdc
	# and can use it for our builtin kdc server_service
	KDC_CFLAGS=""
	KDC_CPPFLAGS=""
	KDC_DLFLAGS=""
	KDC_LIBS=""
	AC_CHECK_HEADERS(kdc.h)
	AC_CHECK_LIB_EXT(kdc, KDC_LIBS, krb5_kdc_default_config)
	AC_CHECK_LIB_EXT(hdb, KDC_LIBS, hdb_generate_key_set_password)

	AC_MSG_CHECKING(whether libkdc is used)
	if test x"$ac_cv_header_kdc_h" = x"yes"; then
		if test x"$ac_cv_lib_ext_kdc_krb5_kdc_default_config" = x"yes"; then
	   		if test x"$ac_cv_lib_ext_hdb_hdb_generate_key_set_password" = x"yes"; then
				SMB_ENABLE(KDC,YES)
				AC_MSG_RESULT(yes)
				echo "KDC_LIBS:     ${KDC_LIBS}"
			else
				AC_MSG_RESULT(no)
			fi
		else
			AC_MSG_RESULT(no)
		fi
	else
		AC_MSG_RESULT(no)
	fi

	CFLAGS=$ac_save_CFLAGS
	CPPFLAGS=$ac_save_CPPFLAGS
	LDFLAGS=$ac_save_LDFLAGS
	LIBS="$ac_save_LIBS"

	# as a nasty hack add the krb5 stuff to the global vars,
	# at some point this should not be needed anymore when the build system
	# can handle that alone
	CFLAGS="$CFLAGS $KRB5_CFLAGS"
	CPPFLAGS="$CPPFLAGS $KRB5_CPPFLAGS"
	LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"
fi

SMB_EXT_LIB(KRB5,[${KRB5_LIBS}],[${KRB5_CFLAGS}],[${KRB5_CPPFLAGS}],[${KRB5_LDFLAGS}])
SMB_EXT_LIB(KDC,[${KDC_LIBS}],[${KDC_CFLAGS}],[${KDC_CPPFLAGS}],[${KDC_LDFLAGS}])
