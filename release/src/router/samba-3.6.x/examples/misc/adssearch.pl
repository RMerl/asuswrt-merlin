#!/usr/bin/perl -w
# 
# adssearch.pl 	- query an Active Directory server and
#		  display objects in a human readable format
#
# Copyright (C) Guenther Deschner <gd@samba.org> 2003-2008
#
# TODO: add range retrieval
#	write sddl-converter, decode userParameters
#	apparently only win2k3 allows simple-binds with machine-accounts.
#	make sasl support independent from Authen::SASL::Cyrus v >0.11
use strict;

use Net::LDAP;
use Net::LDAP::Control;
use Net::LDAP::Constant qw(LDAP_REFERRAL);
use Convert::ASN1;
use Time::Local;
use POSIX qw(strftime);
use Getopt::Long;

my $have_sasl;
my $works_sasl;
my $pref_version;
BEGIN {
	my $class = 'Authen::SASL';
	$pref_version = "0.32";
        if ( eval "require $class;" ) {
                $have_sasl = 1;
        }
        if ( eval "Net::LDAP->VERSION($pref_version);" ) {
                $works_sasl = 1;
        }
}

# users may set defaults here
my $base 	= "";
my $binddn 	= "";
my $password 	= "";
my $server 	= "";
my $rebind_url;


my $tdbdump	= "/usr/bin/tdbdump";
my $testparm	= "/usr/bin/testparm";
my $net		= "/usr/bin/net";
my $dig		= "/usr/bin/dig";
my $nmblookup	= "/usr/bin/nmblookup";
my $secrets_tdb = "/etc/samba/secrets.tdb";
my $klist	= "/usr/bin/klist";
my $kinit	= "/usr/bin/kinit";
my $workgroup	= "";
my $machine	= "";
my $realm	= "";

# parse input
my (
	$opt_asq,
	$opt_base, 
	$opt_binddn,
	$opt_debug,
	$opt_display_extendeddn,
	$opt_display_metadata,
	$opt_display_raw,
	$opt_domain_scope,
	$opt_dump_rootdse,
	$opt_dump_schema,
	$opt_dump_wknguid,
	$opt_fastbind,
	$opt_help, 
	$opt_host, 
	$opt_machine,
	$opt_notify, 
	$opt_notify_nodiffs,
	$opt_paging,
	$opt_password,
	$opt_port,
	$opt_realm,
	$opt_saslmech,
	$opt_search_opt,
	$opt_scope, 
	$opt_simpleauth,
	$opt_starttls,
	$opt_user,
	$opt_verbose,
	$opt_workgroup,
);

GetOptions(
	'asq=s'		=> \$opt_asq,
	'base|b=s'	=> \$opt_base,
	'D|DN=s'	=> \$opt_binddn,
	'debug=i'	=> \$opt_debug,
	'domain_scope'	=> \$opt_domain_scope,
	'extendeddn|e:i'	=> \$opt_display_extendeddn,
	'fastbind'	=> \$opt_fastbind,
	'help'		=> \$opt_help,
	'host|h=s'	=> \$opt_host,
	'machine|P'	=> \$opt_machine,
	'metadata|m'	=> \$opt_display_metadata,
	'nodiffs'	=> \$opt_notify_nodiffs,
	'notify|n'	=> \$opt_notify,
	'paging:i'	=> \$opt_paging,
	'password|w=s'	=> \$opt_password,
	'port=i'	=> \$opt_port,
	'rawdisplay'	=> \$opt_display_raw,
	'realm|R=s'	=> \$opt_realm,
	'rootDSE'	=> \$opt_dump_rootdse,
	'saslmech|Y=s'	=> \$opt_saslmech,
	'schema|c'	=> \$opt_dump_schema,
	'scope|s=s'	=> \$opt_scope,
	'searchopt:i'	=> \$opt_search_opt,
	'simpleauth|x'	=> \$opt_simpleauth,
	'tls|Z'		=> \$opt_starttls,
	'user|U=s'	=> \$opt_user,
	'verbose|v'	=> \$opt_verbose,
	'wknguid'	=> \$opt_dump_wknguid,
	'workgroup|k=s'	=> \$opt_workgroup,
	);


if (!@ARGV && !$opt_dump_schema && !$opt_dump_rootdse && !$opt_notify || $opt_help) {
	usage();
	exit 1;
}

if ($opt_fastbind && !$opt_simpleauth) {
	printf("LDAP fast bind can only be performed with simple binds\n");
	exit 1;
}

if ($opt_notify) {
	$opt_paging = undef;
}

# get the query
my $query 	= shift;
my @attrs	= @ARGV;

# some global vars
my $filter = "";
my ($dse, $uri);
my ($attr, $value);
my (@ctrls, @ctrls_s);
my ($ctl_paged, $cookie);
my ($page_count, $total_entry_count);
my ($sasl_hd, $async_ldap_hd, $sync_ldap_hd);
my ($mesg, $usn);
my (%entry_store);
my $async_search;

# fixed values and vars
my $set   	= "X";
my $unset 	= "-";
my $tabsize 	= "\t\t\t";

# get defaults
my $scope 	= $opt_scope 	|| "sub"; 
my $port 	= $opt_port;

my %ads_controls = (
"LDAP_SERVER_NOTIFICATION_OID"	 	=> "1.2.840.113556.1.4.528",
"LDAP_SERVER_EXTENDED_DN_OID" 		=> "1.2.840.113556.1.4.529",
"LDAP_PAGED_RESULT_OID_STRING"		=> "1.2.840.113556.1.4.319",
"LDAP_SERVER_SD_FLAGS_OID"		=> "1.2.840.113556.1.4.801",
"LDAP_SERVER_SORT_OID"			=> "1.2.840.113556.1.4.473",
"LDAP_SERVER_RESP_SORT_OID"		=> "1.2.840.113556.1.4.474",
"LDAP_CONTROL_VLVREQUEST"		=> "2.16.840.1.113730.3.4.9",
"LDAP_CONTROL_VLVRESPONSE"		=> "2.16.840.1.113730.3.4.10",
"LDAP_SERVER_RANGE_RETRIEVAL"		=> "1.2.840.113556.1.4.802", #unsure
"LDAP_SERVER_SHOW_DELETED_OID"		=> "1.2.840.113556.1.4.417",
"LDAP_SERVER_CROSSDOM_MOVE_TARGET_OID" 	=> "1.2.840.113556.1.4.521",
"LDAP_SERVER_LAZY_COMMIT_OID"		=> "1.2.840.113556.1.4.619",
"LDAP_SERVER_TREE_DELETE_OID"		=> "1.2.840.113556.1.4.805",
"LDAP_SERVER_DIRSYNC_OID"		=> "1.2.840.113556.1.4.841",
"LDAP_SERVER_VERIFY_NAME_OID"		=> "1.2.840.113556.1.4.1338",
"LDAP_SERVER_DOMAIN_SCOPE_OID"		=> "1.2.840.113556.1.4.1339",
"LDAP_SERVER_SEARCH_OPTIONS_OID"	=> "1.2.840.113556.1.4.1340",
"LDAP_SERVER_PERMISSIVE_MODIFY_OID" 	=> "1.2.840.113556.1.4.1413",
"LDAP_SERVER_ASQ_OID"			=> "1.2.840.113556.1.4.1504",
"NONE (Get stats control)"		=> "1.2.840.113556.1.4.970",
"LDAP_SERVER_QUOTA_CONTROL_OID"		=> "1.2.840.113556.1.4.1852",
"LDAP_SERVER_SHUTDOWN_NOTIFY_OID"	=> "1.2.840.113556.1.4.1907",
);

my %ads_capabilities = (
"LDAP_CAP_ACTIVE_DIRECTORY_OID"		=> "1.2.840.113556.1.4.800",
"LDAP_CAP_ACTIVE_DIRECTORY_V51_OID" 	=> "1.2.840.113556.1.4.1670",
"LDAP_CAP_ACTIVE_DIRECTORY_LDAP_INTEG_OID" => "1.2.840.113556.1.4.1791",
);

my %ads_extensions = (
"LDAP_START_TLS_OID"			=> "1.3.6.1.4.1.1466.20037",
"LDAP_TTL_EXTENDED_OP_OID"		=> "1.3.6.1.4.1.1466.101.119.1",
"LDAP_SERVER_FAST_BIND_OID"		=> "1.2.840.113556.1.4.1781", 
"NONE (TTL refresh extended op)" 	=> "1.3.6.1.4.1.1466.101.119.1",
);

my %ads_matching_rules = (
"LDAP_MATCHING_RULE_BIT_AND"		=> "1.2.840.113556.1.4.803",
"LDAP_MATCHING_RULE_BIT_OR"		=> "1.2.840.113556.1.4.804",
);

my %wknguids = (
"WELL_KNOWN_GUID_COMPUTERS"		=> "AA312825768811D1ADED00C04FD8D5CD",
"WELL_KNOWN_GUID_DOMAIN_CONTROLLERS"	=> "A361B2FFFFD211D1AA4B00C04FD7D83A",
"WELL_KNOWN_GUID_SYSTEM"		=> "AB1D30F3768811D1ADED00C04FD8D5CD",
"WELL_KNOWN_GUID_USERS"			=> "A9D1CA15768811D1ADED00C04FD8D5CD",
);

my %ads_systemflags = (
"FLAG_DONT_REPLICATE"			=> 0x00000001,	# 1
"FLAG_REPLICATE_TO_GC"			=> 0x00000002,	# 2
"FLAG_ATTRIBUTE_CONSTRUCT"		=> 0x00000004,	# 4
"FLAG_CATEGORY_1_OBJECT"		=> 0x00000010,	# 16
"FLAG_DELETE_WITHOUT_TOMBSTONE"		=> 0x02000000,	# 33554432
"FLAG_DOMAIN_DISALLOW_MOVE"		=> 0x04000000,	# 67108864
"FLAG_DOMAIN_DISALLOW_RENAME"		=> 0x08000000,	# 134217728
#"FLAG_CONFIG_CAN_MOVE_RESTRICTED"	=> 0x10000000,	# 268435456	# only setable on creation
#"FLAG_CONFIG_CAN_MOVE"			=> 0x20000000,	# 536870912	# only setable on creation
#"FLAG_CONFIG_CAN_RENAME"		=> 0x40000000,	# 1073741824	# only setable on creation
"FLAG_DISALLOW_DELETE"			=> 0x80000000,	# 2147483648
);

my %ads_mixed_domain = (
"NATIVE_LEVEL_DOMAIN"			=> 0,
"MIXED_LEVEL_DOMAIN"			=> 1,
);

my %ads_ds_func = (
"DS_BEHAVIOR_WIN2000"			=> 0,	# untested
"DS_BEHAVIOR_WIN2003"			=> 2,
"DS_BEHAVIOR_WIN2008"			=> 3,
);

my %ads_instance_type = (
"DS_INSTANCETYPE_IS_NC_HEAD"		=> 0x1,
"IT_WRITE"				=> 0x4,
"IT_NC_ABOVE"				=> 0x8,
);

my %ads_uacc = (
	"ACCOUNT_NEVER_EXPIRES"		=> 0x000000, # 0 
	"ACCOUNT_OK"			=> 0x800000, # 8388608
	"ACCOUNT_LOCKED_OUT"		=> 0x800010, # 8388624
);

my %ads_enctypes = (
	"DES-CBC-CRC"				=> 0x01,
	"DES-CBC-MD5"				=> 0x02,
	"RC4_HMAC_MD5"				=> 0x04,
	"AES128_CTS_HMAC_SHA1_96"		=> 0x08,
	"AES128_CTS_HMAC_SHA1_128"		=> 0x10,
);

my %ads_gpoptions = (
	"GPOPTIONS_INHERIT"		=> 0,
	"GPOPTIONS_BLOCK_INHERITANCE"	=> 1,
);

my %ads_gplink_opts = (
	"GPLINK_OPT_NONE"		=> 0,
	"GPLINK_OPT_DISABLED"		=> 1,
	"GPLINK_OPT_ENFORCED"		=> 2,
);

my %ads_gpflags = (
	"GPFLAGS_ALL_ENABLED"			=> 0,
	"GPFLAGS_USER_SETTINGS_DISABLED"	=> 1,
	"GPFLAGS_MACHINE_SETTINGS_DISABLED"	=> 2,
	"GPFLAGS_ALL_DISABLED"			=> 3,
);

my %ads_serverstate = (
	"SERVER_ENABLED"		=> 1,
	"SERVER_DISABLED"		=> 2,
);

my %ads_sdeffective = (
	"OWNER_SECURITY_INFORMATION"	=> 0x01,
	"DACL_SECURITY_INFORMATION"	=> 0x04,
	"SACL_SECURITY_INFORMATION"	=> 0x10,
);

my %ads_trustattrs = (
	"TRUST_ATTRIBUTE_NON_TRANSITIVE"	=> 1,
	"TRUST_ATTRIBUTE_TREE_PARENT"		=> 2,
	"TRUST_ATTRIBUTE_TREE_ROOT"		=> 3,
	"TRUST_ATTRIBUTE_UPLEVEL_ONLY"		=> 4,
);

my %ads_trustdirection = (
	"TRUST_DIRECTION_INBOUND"		=> 1,
	"TRUST_DIRECTION_OUTBOUND"		=> 2,
	"TRUST_DIRECTION_BIDIRECTIONAL"		=> 3,
);

my %ads_trusttype = (
	"TRUST_TYPE_DOWNLEVEL"			=> 1,
	"TRUST_TYPE_UPLEVEL"			=> 2,
	"TRUST_TYPE_KERBEROS"			=> 3,
	"TRUST_TYPE_DCE"			=> 4,
);

my %ads_pwdproperties = (
	"DOMAIN_PASSWORD_COMPLEX"		=> 1, 
	"DOMAIN_PASSWORD_NO_ANON_CHANGE" 	=> 2, 
	"DOMAIN_PASSWORD_NO_CLEAR_CHANGE"	=> 4, 
	"DOMAIN_LOCKOUT_ADMINS"			=> 8, 
	"DOMAIN_PASSWORD_STORE_CLEARTEXT"	=> 16, 
	"DOMAIN_REFUSE_PASSWORD_CHANGE"		=> 32,
);

my %ads_uascompat = (
	"LANMAN_USER_ACCOUNT_SYSTEM_NOLIMITS"	=> 0,
	"LANMAN_USER_ACCOUNT_SYSTEM_COMPAT"	=> 1,
);

my %ads_frstypes = (
	"REPLICA_SET_SYSVOL"		=> 2,	# unsure
	"REPLICA_SET_DFS"		=> 1,	# unsure
	"REPLICA_SET_OTHER"		=> 0,	# unsure
);

my %ads_gp_cse_extensions = (
"Administrative Templates Extension"	=> "35378EAC-683F-11D2-A89A-00C04FBBCFA2",
"Disk Quotas"				=> "3610EDA5-77EF-11D2-8DC5-00C04FA31A66",
"EFS Recovery"				=> "B1BE8D72-6EAC-11D2-A4EA-00C04F79F83A",
"Folder Redirection"			=> "25537BA6-77A8-11D2-9B6C-0000F8080861",
"IP Security"				=> "E437BC1C-AA7D-11D2-A382-00C04F991E27",
"Internet Explorer Maintenance"		=> "A2E30F80-D7DE-11d2-BBDE-00C04F86AE3B",
"QoS Packet Scheduler"			=> "426031c0-0b47-4852-b0ca-ac3d37bfcb39",
"Scripts"				=> "42B5FAAE-6536-11D2-AE5A-0000F87571E3",
"Security"				=> "827D319E-6EAC-11D2-A4EA-00C04F79F83A",
"Software Installation"			=> "C6DC5466-785A-11D2-84D0-00C04FB169F7",
);

# guess work
my %ads_gpcextensions = (
"Administrative Templates"		=> "0F6B957D-509E-11D1-A7CC-0000F87571E3",
"Certificates"				=> "53D6AB1D-2488-11D1-A28C-00C04FB94F17",
"EFS recovery policy processing"	=> "B1BE8D72-6EAC-11D2-A4EA-00C04F79F83A",
"Folder Redirection policy processing"	=> "25537BA6-77A8-11D2-9B6C-0000F8080861",
"Folder Redirection"			=> "88E729D6-BDC1-11D1-BD2A-00C04FB9603F",
"Registry policy processing"		=> "35378EAC-683F-11D2-A89A-00C04FBBCFA2",
"Remote Installation Services"		=> "3060E8CE-7020-11D2-842D-00C04FA372D4",
"Security Settings"			=> "803E14A0-B4FB-11D0-A0D0-00A0C90F574B",
"Security policy processing"		=> "827D319E-6EAC-11D2-A4EA-00C04F79F83A",
"unknown"				=> "3060E8D0-7020-11D2-842D-00C04FA372D4",
"unknown2"				=> "53D6AB1B-2488-11D1-A28C-00C04FB94F17",
);

my %ads_gpo_default_guids = (
"Default Domain Policy"			=> "31B2F340-016D-11D2-945F-00C04FB984F9",
"Default Domain Controllers Policy"	=> "6AC1786C-016F-11D2-945F-00C04fB984F9",
"mist"					=> "61718096-3D3F-4398-8318-203A48976F9E",
);

my %ads_uf = (
	"UF_SCRIPT"				=> 0x00000001,
	"UF_ACCOUNTDISABLE"			=> 0x00000002,
#	"UF_UNUSED_1"				=> 0x00000004,
	"UF_HOMEDIR_REQUIRED"			=> 0x00000008,
	"UF_LOCKOUT"				=> 0x00000010,
	"UF_PASSWD_NOTREQD"			=> 0x00000020,
	"UF_PASSWD_CANT_CHANGE"			=> 0x00000040,
	"UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED"	=> 0x00000080,
	"UF_TEMP_DUPLICATE_ACCOUNT"		=> 0x00000100,
	"UF_NORMAL_ACCOUNT"			=> 0x00000200,
#	"UF_UNUSED_2"				=> 0x00000400,
	"UF_INTERDOMAIN_TRUST_ACCOUNT"		=> 0x00000800,
	"UF_WORKSTATION_TRUST_ACCOUNT"		=> 0x00001000,
	"UF_SERVER_TRUST_ACCOUNT"		=> 0x00002000,
#	"UF_UNUSED_3"				=> 0x00004000,
#	"UF_UNUSED_4"				=> 0x00008000,
	"UF_DONT_EXPIRE_PASSWD"			=> 0x00010000,
	"UF_MNS_LOGON_ACCOUNT"			=> 0x00020000,
	"UF_SMARTCARD_REQUIRED"			=> 0x00040000,
	"UF_TRUSTED_FOR_DELEGATION"		=> 0x00080000,
	"UF_NOT_DELEGATED"			=> 0x00100000,
	"UF_USE_DES_KEY_ONLY"			=> 0x00200000,
	"UF_DONT_REQUIRE_PREAUTH"		=> 0x00400000,
	"UF_PASSWORD_EXPIRED"			=> 0x00800000,
	"UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION" => 0x01000000,
	"UF_NO_AUTH_DATA_REQUIRED"		=> 0x02000000,
#	"UF_UNUSED_8"				=> 0x04000000,
#	"UF_UNUSED_9"				=> 0x08000000,
#	"UF_UNUSED_10"				=> 0x10000000,
#	"UF_UNUSED_11"				=> 0x20000000,
#	"UF_UNUSED_12"				=> 0x40000000,
#	"UF_UNUSED_13"				=> 0x80000000,
);

my %ads_grouptype = (
	"GROUP_TYPE_BUILTIN_LOCAL_GROUP"	=> 0x00000001,
	"GROUP_TYPE_ACCOUNT_GROUP"		=> 0x00000002,
	"GROUP_TYPE_RESOURCE_GROUP"		=> 0x00000004,
	"GROUP_TYPE_UNIVERSAL_GROUP"		=> 0x00000008,
	"GROUP_TYPE_APP_BASIC_GROUP"		=> 0x00000010,
	"GROUP_TYPE_APP_QUERY_GROUP"		=> 0x00000020,
	"GROUP_TYPE_SECURITY_ENABLED"		=> 0x80000000,
);

my %ads_atype = (
	"ATYPE_NORMAL_ACCOUNT"			=> 0x30000000,
	"ATYPE_WORKSTATION_TRUST"		=> 0x30000001,
	"ATYPE_INTERDOMAIN_TRUST"		=> 0x30000002,
	"ATYPE_SECURITY_GLOBAL_GROUP"		=> 0x10000000,
	"ATYPE_DISTRIBUTION_GLOBAL_GROUP"	=> 0x10000001,
	"ATYPE_DISTRIBUTION_UNIVERSAL_GROUP"	=> 0x10000001, # ATYPE_DISTRIBUTION_GLOBAL_GROUP
	"ATYPE_SECURITY_LOCAL_GROUP"		=> 0x20000000,
	"ATYPE_DISTRIBUTION_LOCAL_GROUP"	=> 0x20000001,
	"ATYPE_ACCOUNT"				=> 0x30000000, # ATYPE_NORMAL_ACCOUNT
	"ATYPE_GLOBAL_GROUP"			=> 0x10000000, # ATYPE_SECURITY_GLOBAL_GROUP
	"ATYPE_LOCAL_GROUP"			=> 0x20000000, # ATYPE_SECURITY_LOCAL_GROUP
);

my %ads_gtype = (
	"GTYPE_SECURITY_BUILTIN_LOCAL_GROUP"	=> 0x80000005,
	"GTYPE_SECURITY_DOMAIN_LOCAL_GROUP"	=> 0x80000004,
	"GTYPE_SECURITY_GLOBAL_GROUP"		=> 0x80000002,
	"GTYPE_SECURITY_UNIVERSAL_GROUP"	=> 0x80000008,
	"GTYPE_DISTRIBUTION_GLOBAL_GROUP"	=> 0x00000002,
	"GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP"	=> 0x00000004,
	"GTYPE_DISTRIBUTION_UNIVERSAL_GROUP"	=> 0x00000008,
);

my %munged_dial = (
	"CtxCfgPresent"		=> \&dump_int,
	"CtxCfgFlags1"		=> \&dump_int,
	"CtxCallback"		=> \&dump_string,
	"CtxShadow"		=> \&dump_string,
	"CtxMaxConnectionTime"	=> \&dump_int,
	"CtxMaxDisconnectionTime"=> \&dump_int,
	"CtxMaxIdleTime"	=> \&dump_int,
	"CtxKeyboardLayout"	=> \&dump_int,
	"CtxMinEncryptionLevel"	=> \&dump_int,
	"CtxWorkDirectory"	=> \&dump_string,
	"CtxNWLogonServer"	=> \&dump_string,
	"CtxWFHomeDir"		=> \&dump_string,
	"CtxWFHomeDirDrive"	=> \&dump_string,
	"CtxWFProfilePath"	=> \&dump_string,
	"CtxInitialProgram"	=> \&dump_string,
	"CtxCallbackNumber"	=> \&dump_string,
);

$SIG{__WARN__} = sub {
	use Carp;
	Carp::cluck (shift);
};

# if there is data missing, we try to autodetect with samba-tools (if installed)
# this might fill up workgroup, machine, realm
get_samba_info();

# get a workgroup
$workgroup	= $workgroup || $opt_workgroup || "";

# get the server
$server 	= process_servername($opt_host) || 
		  detect_server($workgroup,$opt_realm) || 
		  die "please define server to query with -h host\n";


# get the base
$base 		= defined($opt_base)? $opt_base : "" || 
	 	  get_base_from_rootdse($server,$dse);

# get the realm
$realm		= $opt_realm || 
		  get_realm_from_rootdse($server,$dse);

# get sasl mechs
my @sasl_mechs	= get_sasl_mechs_from_rootdse($server,$dse);
my $sasl_mech	= "GSSAPI";
if ($opt_saslmech) {
	$sasl_mech = sprintf("%s", (check_sasl_mech($opt_saslmech) == 0)?uc($opt_saslmech):"");
}

# set bind type
my $sasl_bind = 1 if (!$opt_simpleauth);

# get username
my $user 	= check_user($opt_user) || $ENV{'USER'} || "";

# gen upn
my $upn		= sprintf("%s", gen_upn($opt_machine ? "$machine\$" : $user, $realm));

# get binddn
$binddn		= $opt_binddn || $upn;

# get the password
$password 	= $password || $opt_password;
if (!$password) {
	$password = $opt_machine ? get_machine_password($workgroup) : get_password();
}

my %attr_handler = (
	"Token-Groups-No-GC-Acceptable" => \&dump_sid,	#wrong name
	"accountExpires"		=> \&dump_nttime,
	"attributeSecurityGUID"		=> \&dump_guid,
	"badPasswordTime"		=> \&dump_nttime,			
	"creationTime"			=> \&dump_nttime,
	"currentTime"			=> \&dump_timestr,
	"domainControllerFunctionality" => \&dump_ds_func,
	"domainFunctionality" 		=> \&dump_ds_func,
	"fRSReplicaSetGUID"		=> \&dump_guid,
	"fRSReplicaSetType"		=> \&dump_frstype,
	"fRSVersionGUID"		=> \&dump_guid,
	"flags"				=> \&dump_gpflags,	# fixme: possibly only on gpos!
	"forceLogoff"			=> \&dump_nttime_abs,
	"forestFunctionality" 		=> \&dump_ds_func,
#	"gPCMachineExtensionNames"	=> \&dump_gpcextensions,
#	"gPCUserExtensionNames"		=> \&dump_gpcextensions,
	"gPLink"			=> \&dump_gplink,
	"gPOptions"			=> \&dump_gpoptions,
	"groupType"			=> \&dump_gtype,
	"instanceType"			=> \&dump_instance_type,
	"lastLogon"			=> \&dump_nttime,
	"lastLogonTimestamp"		=> \&dump_nttime,
	"lastSetTime"			=> \&dump_nttime,
	"lockOutObservationWindow"	=> \&dump_nttime_abs,
	"lockoutDuration"		=> \&dump_nttime_abs,
	"lockoutTime"			=> \&dump_nttime,
#	"logonHours"			=> \&dump_logonhours,
	"maxPwdAge"			=> \&dump_nttime_abs,
	"minPwdAge"			=> \&dump_nttime_abs,
	"modifyTimeStamp"		=> \&dump_timestr,
	"msDS-Behavior-Version"		=> \&dump_ds_func,	#unsure
	"msDS-User-Account-Control-Computed" => \&dump_uacc,
	"msDS-SupportedEncryptionTypes"	=> \&dump_enctypes,
	"mS-DS-CreatorSID"		=> \&dump_sid,
#	"msRADIUSFramedIPAddress"	=> \&dump_ipaddr,
#	"msRASSavedFramedIPAddress" 	=> \&dump_ipaddr,
	"netbootGUID"			=> \&dump_guid,
	"nTMixedDomain"			=> \&dump_mixed_domain,
	"nTSecurityDescriptor"		=> \&dump_secdesc,
	"objectGUID"			=> \&dump_guid,
	"objectSid"			=> \&dump_sid,
	"pKT"				=> \&dump_pkt,
	"pKTGuid"			=> \&dump_guid,
	"priorSetTime"			=> \&dump_nttime,
	"pwdLastSet"			=> \&dump_nttime,
	"pwdProperties"			=> \&dump_pwdproperties,
	"sAMAccountType"		=> \&dump_atype,
	"schemaIDGUID"			=> \&dump_guid,
	"sDRightsEffective"		=> \&dump_sdeffective,
	"securityIdentifier"		=> \&dump_sid,
	"serverState"			=> \&dump_serverstate,
	"supportedCapabilities",	=> \&dump_capabilities,
	"supportedControl",		=> \&dump_controls,
	"supportedExtension",		=> \&dump_extension,
	"systemFlags"			=> \&dump_systemflags,
	"tokenGroups",			=> \&dump_sid,
	"tokenGroupsGlobalAndUniversal" => \&dump_sid,
	"tokenGroupsNoGCAcceptable"	=> \&dump_sid,
	"trustAttributes"		=> \&dump_trustattr,
	"trustDirection"		=> \&dump_trustdirection,
	"trustType"			=> \&dump_trusttype,
	"uASCompat"			=> \&dump_uascompat,
	"userAccountControl"		=> \&dump_uac,
	"userCertificate"		=> \&dump_cert,
	"userFlags"			=> \&dump_uf,
	"userParameters"		=> \&dump_munged_dial,
	"whenChanged"			=> \&dump_timestr,
	"whenCreated"			=> \&dump_timestr,
#	"dSCorePropagationData"		=> \&dump_timestr,
);



################
# subfunctions #
################

sub usage {
	print "usage: $0 [--asq] [--base|-b base] [--debug level] [--debug level] [--DN|-D binddn] [--extendeddn|-e] [--help] [--host|-h host] [--machine|-P] [--metadata|-m] [--nodiffs] [--notify|-n] [--password|-w password] [--port port] [--rawdisplay] [--realm|-R realm] [--rootdse] [--saslmech|-Y saslmech] [--schema|-c] [--scope|-s scope] [--simpleauth|-x] [--starttls|-Z] [--user|-U user] [--wknguid] [--workgroup|-k workgroup] filter [attrs]\n";
	print "\t--asq [attribute]\n\t\tAttribute to use for a attribute scoped query (LDAP_SERVER_ASQ_OID)\n";
	print "\t--base|-b [base]\n\t\tUse base [base]\n";
	print "\t--debug [level]\n\t\tUse debuglevel (for Net::LDAP)\n";
	print "\t--domain_scope\n\t\tLimit LDAP search to local domain (LDAP_SERVER_DOMAIN_SCOPE_OID)\n";
	print "\t--DN|-D [binddn]\n\t\tUse binddn or principal\n";
	print "\t--extendeddn|-e [value]\n\t\tDisplay extended dn (LDAP_SERVER_EXTENDED_DN_OID)\n";
	print "\t--fastbind\n\t\tDo LDAP fast bind using LDAP_SERVER_FAST_BIND_OID extension\n";
	print "\t--help\n\t\tDisplay help page\n";
	print "\t--host|-h [host]\n\t\tQuery Host [host] (either a hostname or an LDAP uri)\n";
	print "\t--machine|-P\n\t\tUse samba3 machine account stored in $secrets_tdb (needs root access)\n";
	print "\t--metdata|-m\n\t\tDisplay replication metadata\n";
	print "\t--nodiffs\n\t\tDisplay no diffs but full entry dump when running in notify mode\n";
	print "\t--notify|-n\n\t\tActivate asynchronous change notification (LDAP_SERVER_NOTIFICATION_OID)\n";
	print "\t--paging [pagesize]\n\t\tUse paged results when searching\n";
	print "\t--password|-w [password]\n\t\tUse [password] for binddn\n";
	print "\t--port [port]\n\t\tUse [port] when connecting ADS\n";
	print "\t--rawdisplay\n\t\tDo not interpret values\n";
	print "\t--realm|-R [realm]\n\t\tUse [realm] when trying to construct bind-principal\n";
	print "\t--rootdse\n\t\tDisplay RootDSE (anonymously)\n";
	print "\t--saslmech|-Y [saslmech]\n\t\tUse SASL Mechanism [saslmech] when binding\n";
	print "\t--schema|-c\n\t\tDisplay DSE-Schema\n";
	print "\t--scope|-s [scope]\n\t\tUse scope [scope] (sub, base, one)\n";
	print "\t--simpleauth|-x\n\t\tUse simple bind (otherwise SASL binds are performed)\n";
	print "\t--starttls|-Z\n\t\tUse Start TLS extended operation to secure LDAP traffic\n";
	print "\t--user|-U [user]\n\t\tUse [user]\n";
	print "\t--wknguid\n\t\tDisplay well known guids\n";
	print "\t--workgroup|-k [workgroup]\n\t\tWhen LDAP-Server is not known try to find a Domain-Controller for [workgroup]\n";
}

sub write_ads_list {
	my ($mod,$attr,$value) = @_;
	my $ofh = select(STDOUT);
	$~ = "ADS_LIST";
	select($ofh);
	write();

format ADS_LIST =
@<<<< @>>>>>>>>>>>>>>>>>>>>>>>: @*
$mod, $attr, $value
.
}

sub write_ads {
	my ($mod,$attr,$value) = @_;
	my $ofh = select(STDOUT);
	$~ = "ADS";
	select($ofh);
	write();

format ADS =
@<<<< @>>>>>>>>>>>>>>>>>>>>>>>: ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$mod, $attr, $value
~~				^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
				$value
.
}

sub detect_server {

	my $workgroup = shift;
	my $realm = shift;
	my $result;
	my $found;

	# try net cache (nbt records)
	if ( -x $net && $workgroup ) {
		my $key = sprintf("NBT/%s#1C", uc($workgroup));
		chomp($result = `$net cache search $key 2>&1 /dev/null`);
		$result =~ s/^.*Value: //;
		$result =~ s/:.*//;
		return $result if $result;
		printf("%10s query failed for [%s]\n", "net cache", $key);
	}

	# try dns SRV entries
	if ( -x $dig && $realm ) {
		my $key = sprintf("_ldap._tcp.%s", lc($realm));
		chomp($result = `$dig $key SRV +short +search | egrep "^[0-9]" | head -1`);
		$result =~ s/.* //g;
		$result =~ s/.$//g;
		return $result if $result;
		printf("%10s query failed for [%s]\n", "dns", $key);
	}

	# try netbios broadcast query
	if ( -x $nmblookup && $workgroup ) {
		my $key = sprintf("%s#1C", uc($workgroup));
		my $pattern = sprintf("%s<1c>", uc($workgroup));
		chomp($result = `$nmblookup $key -d 0 | grep '$pattern'`);
		$result =~ s/\s.*//;
		return $result if $result;
		printf("%10s query failed for [%s]\n", "nmblookup", $key);
	}

	return "";
}

sub get_samba_info {

	if (! -x $testparm) { return -1; }
	
	my $tmp;
	open(TESTPARM, "$testparm -s -v 2> /dev/null |");
	while (my $line = <TESTPARM>) {
		chomp($line);
		if ($line =~ /netbios name/) {
			($tmp, $machine) = split(/=/, $line);
			$machine =~ s/\s+|\t+//g;
		}
		if ($line =~ /realm/) {
			($tmp, $realm) = split(/=/, $line);
			$realm =~ s/\s+|\t+//g;
		}
		if ($line =~ /workgroup/) {
			($tmp, $workgroup) = split(/=/, $line);
			$workgroup =~ s/\s+|\t+//g;
		}
	}
	close(TESTPARM);
	return 0;
}

sub gen_upn {
	my $machine = shift;
	my $realm = shift;
	if ($machine && $realm) {
		return sprintf("%s\@%s", lc($machine), uc($realm));
	};
	return undef;
}

sub get_password {
	if (!$password && $opt_simpleauth && check_ticket($user)) {
		return prompt_password($user);
	}
	return "";
}

sub get_user {
	my $user = shift || prompt_user();
	return $user;
}

sub get_machine_password {

	my $workgroup = shift || "";
	$workgroup = uc($workgroup);

	my ($found, $tmp);
	-x $tdbdump || die "tdbdump is not installed. cannot proceed autodetection\n";
	-r $secrets_tdb || die "cannot read $secrets_tdb. cannot proceed autodetection\n";

	# get machine-password
	my $key = sprintf("SECRETS/MACHINE_PASSWORD/%s", $workgroup);
	open(SECRETS,"$tdbdump $secrets_tdb |");
	while(my $line = <SECRETS>) {
		chomp($line);
		if ($found) {
			$line =~ s/\\00//;
			($line,$password) = split(/"/, $line);
			last;
		}
		if ($line =~ /\"$key\"/) {
			$found = 1;
		}
	}
	close(SECRETS);

	if ($found) {
		print "Successfully autodetected machine password for workgroup: $workgroup\n";
		return $password;
	} else {
		warn "No machine password available for $workgroup\n";
	}
	return "";
}

sub prompt_password {

	my $acct = shift || "";
	if ($acct =~ /\%/) {
		($acct, $password) = split(/\%/, $acct);
		return $password;
	}
	system "stty -echo";
	print "Enter password for $acct:";
	my $password = <STDIN>;
	chomp($password);
	print "\n";
	system "stty echo";
	return $password;
}

sub prompt_user {

	print "Enter Username:";
	my $user = <STDIN>;
	chomp($user);
	print "\n";
	return $user;
}

sub check_ticket {
	return 0;
	# works only for heimdal
	return system("$klist -t");
}

sub get_ticket {

	my $KRB5_CONFIG = "/tmp/.krb5.conf.telads-$<";

	open(KRB5CONF, "> $KRB5_CONFIG") || die "cannot write $KRB5_CONFIG";
	printf  KRB5CONF "# autogenerated by $0\n";
	printf  KRB5CONF "[libdefaults]\n\tdefault_realm = %s\n\tclockskew = %d\n", uc($realm), 60*60;
	printf  KRB5CONF "[realms]\n\t%s = {\n\t\tkdc = %s\n\t}\n", uc($realm), $server;
	close(KRB5CONF);

	if ( system("KRB5_CONFIG=$KRB5_CONFIG $kinit $user") != 0) {
		return -1;
	}

	return 0;
}

sub check_user {
	my $acct = shift || "";
	if ($acct =~ /\%/) {
		($acct, $password) = split(/\%/, $acct);
	}
	return $acct;
}

sub check_root_dse($$$@) {

	# bogus function??
	my $server = shift || "";
	$dse = shift || get_dse($server) || return -1;
	my $attr = shift || die "unknown query";
	my @array = @_;

	my $dse_list = $dse->get_value($attr, asref => '1');
	my @dse_array = @$dse_list;

	foreach my $i (@array) {
		# we could use -> supported_control but this 
		# is only available in newer versions of perl-ldap
#		if ( ! $dse->supported_control( $i ) ) {
		if ( grep(/$i->type()/, @dse_array) ) { 
			printf("required \"$attr\": %s is not supported by ADS-server.\n", $i->type());
			return -1;
		}
	}
	return 0;
}

sub check_ctrls ($$@) {
	my $server = shift;
	my $dse = shift;
	my @array = @_;
	return check_root_dse($server, $dse, "supportedControl", @array);
}

sub check_exts ($$@) {
	my $server = shift;
	my $dse = shift;
	my @array = @_;
	return check_root_dse($server, $dse, "supportedExtension", @array);
}

sub get_base_from_rootdse {

	my $server = shift || "";
	$dse = shift || get_dse($server,$async_ldap_hd) || return -1;
	return $dse->get_value($opt_dump_schema ? 'schemaNamingContext':
						  'defaultNamingContext');
}

sub get_realm_from_rootdse {

	my $server = shift || "";
	$dse = shift || get_dse($server,$async_ldap_hd) || return -1;
	my $service = $dse->get_value('ldapServiceName') || "";
	if ($service) {
		my ($t,$realm) = split(/\@/, $service);
		return $realm;
	} else {
		die "very odd: could not get realm";
	}
}

sub get_sasl_mechs_from_rootdse {

	my $server = shift || "";
	$dse = shift || get_dse($server,$async_ldap_hd) || return -1;
	my $mechs = $dse->get_value('supportedSASLMechanisms', asref => 1);
	return @$mechs;
}

sub get_dse {

	my $server = shift || return undef;
	$async_ldap_hd = shift || get_ldap_hd($server,1);
	if (!$async_ldap_hd) {
		print "oh, no connection\n";
		return undef;
	}
	my $mesg = $async_ldap_hd->bind() || die "cannot bind\n";
	if ($mesg->code) { die "failed to bind\n"; };
	my $dse = $async_ldap_hd->root_dse( attrs => ['*', "supportedExtension", "supportedFeatures" ] );

	return $dse;
}

sub process_servername {

	my $name = shift || return "";
	if ($name =~ /^ldaps:\/\//i ) {
		$name =~ s#^ldaps://##i;
		$uri = sprintf("%s://%s", "ldaps", $name);
	} else {
		$name =~ s#^ldap://##i;
		$uri = sprintf("%s://%s", "ldap", $name);
	}
	return $name;
}

sub get_ldap_hd {

	my $server = shift || return undef;
	my $async = shift || "0";
	my $hd;
	die "uri unavailable" if (!$uri);
	if ($uri =~ /^ldaps:\/\//i ) {
		$port = $port || 636;
		use Net::LDAPS;
		$hd = Net::LDAPS->new( $server, async => $async, port => $port ) || 
			die "host $server not available: $!";
	} else {
		$port = $port || 389;
		$hd = Net::LDAP->new( $server, async => $async, port => $port ) || 
			die "host $server not available: $!";
	}
	$hd->debug($opt_debug);
	if ($opt_starttls) {
		$hd->start_tls( verify => 'none' );
	}

	return $hd;
}

sub get_sasl_hd {

	if (!$have_sasl) {
		print "no sasl support\n";
		return undef;
	}

	my $hd;
	if ($sasl_mech && $sasl_mech eq "GSSAPI") {
		my $user = sprintf("%s\@%s", $user, uc($realm));
		if (check_ticket($user) != 0 && get_ticket($user) != 0) {
			print "Could not get Kerberos ticket for user [$user]\n";
			return undef;
		}

		$hd = Authen::SASL->new( mechanism => 'GSSAPI' ) || die "nope";
		my $conn = $hd->client_new("ldap", $server);
		
		if ($conn->code == -1) {
		        printf "%s\n", $conn->error();
			return undef;
		};

	} elsif ($sasl_mech) {

		# here comes generic sasl code
		$hd = Authen::SASL->new( mechanism => $sasl_mech, 
			callback  => { 
				user => \&get_user, 
				pass => \&get_password 
			} 
		) || die "nope";
	} else {
		$sasl_bind = 0;
		print "no supported SASL mechanism found (@sasl_mechs).\n";
		print "falling back to simple bind.\n";
		return undef;
	}

	return $hd;
}

sub check_sasl_mech {
	my $mech_check = shift;
	my $have_mech = 0;
	foreach my $mech (@sasl_mechs) {
		$have_mech = 1 if ($mech eq uc($mech_check));
	}
	if (!$have_mech) {
		return -1;
	}
	return 0;
}

sub store_result ($) {

	my $entry = shift;
	return if (!$entry);
	$entry_store{$entry->dn} = $entry;
}

sub display_result_diff ($) {

	my $entry_new = shift;
	return if ( !$entry_new);

	if ( !exists $entry_store{$entry_new->dn}) {
		print "can't display any differences yet. full dump...\n";
		display_entry_generic($entry_new);
		return;
	}

	my $entry_old = $entry_store{$entry_new->dn};

	foreach my $attr (sort $entry_new->attributes) {
		if ( $entry_new->exists($attr) && ! $entry_old->exists($attr)) {
			display_attr_generic("add:\t", $entry_new, $attr);
			next;
		}
	}

	foreach my $attr (sort $entry_old->attributes) {
		if (! $entry_new->exists($attr) && $entry_old->exists($attr)) {
			display_attr_generic("del:\t", $entry_old, $attr);
			next;
		}

		# now check for all values if they have changed, display changes
		my ($old_vals, $new_vals, @old_vals, @new_vals, %old, %new);

		$old_vals = $entry_old->get_value($attr, asref => 1);
		@old_vals = @$old_vals;
		$new_vals = $entry_new->get_value($attr, asref => 1);
		@new_vals = @$new_vals;

		if (scalar(@old_vals) == 1 && scalar(@new_vals) == 1) {
			if ($old_vals[0] ne $new_vals[0]) {
				display_attr_generic("old:\t", $entry_old, $attr);
				display_attr_generic("new:\t", $entry_new, $attr);
			}
			next;
		}

		# handle multivalued diffs
		foreach my $val (@old_vals) { $old{$val} = "dummy"; };
		foreach my $val (@new_vals) { $new{$val} = "dummy"; };
		foreach my $val (sort keys %new) {
			if (!exists $old{$val}) {
				display_value_generic("add:\t", $attr, $val);
			}
		}
		foreach my $val (sort keys %old) {
			if (!exists $new{$val}) {
				display_value_generic("del:\t", $attr, $val);
			}
		}
	}
	print "\n";

}

sub sid2string ($) {

	# Fix from Michael James <michael@james.st>
	my $binary_sid = shift;
	my($sid_rev, $num_auths, $id1, $id2, @ids) = unpack("H2 H2 n N V*", $binary_sid);
	my $sid_string = join("-", "S", hex($sid_rev), ($id1<<32)+$id2, @ids);
	return $sid_string;
			
}

sub string_to_guid {
	my $string = shift;
	return undef unless $string =~ /([0-9,a-z]{8})-([0-9,a-z]{4})-([0-9,a-z]{4})-([0-9,a-z]{2})([0-9,a-z]{2})-([0-9,a-z]{2})([0-9,a-z]{2})([0-9,a-z]{2})([0-9,a-z]{2})([0-9,a-z]{2})([0-9,a-z]{2})/i;
	
	return 	pack("I", hex $1) . 
		pack("S", hex $2) . 
		pack("S", hex $3) . 
		pack("C", hex $4) . 
		pack("C", hex $5) .
		pack("C", hex $6) . 
		pack("C", hex $7) . 
		pack("C", hex $8) . 
		pack("C", hex $9) . 
		pack("C", hex $10) . 
		pack("C", hex $11);

#	print "$1\n$2\n$3\n$4\n$5\n$6\n$7\n$8\n$9\n$10\n$11\n";
}
							       
sub bindstring_to_guid {
	my $str = shift;
	return pack("H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2 H2", 
		substr($str,0,2),
		substr($str,2,2),
		substr($str,4,2),
		substr($str,6,2),
		substr($str,8,2),
		substr($str,10,2),
		substr($str,12,2),
		substr($str,14,2),
		substr($str,16,2),
		substr($str,18,2),
		substr($str,20,2),
		substr($str,22,2),
		substr($str,24,2),
		substr($str,26,2),
		substr($str,28,2),
		substr($str,30,2));
}

sub guid_to_string {
	my $guid = shift;
	my $string = sprintf "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", 
		unpack("I", $guid), 
		unpack("S", substr($guid, 4, 2)), 
		unpack("S", substr($guid, 6, 2)),
		unpack("C", substr($guid, 8, 1)),
		unpack("C", substr($guid, 9, 1)),
		unpack("C", substr($guid, 10, 1)),
		unpack("C", substr($guid, 11, 1)),
		unpack("C", substr($guid, 12, 1)),
		unpack("C", substr($guid, 13, 1)),
		unpack("C", substr($guid, 14, 1)),
		unpack("C", substr($guid, 15, 1)); 
	return lc($string);
}

sub guid_to_bindstring {
	my $guid = shift;
	return unpack("H" . 2 * length($guid), $guid),
}

sub dump_wknguid {
	print "Dumping Well known GUIDs:\n";
	foreach my $wknguid (keys %wknguids) {

		my $guid = bindstring_to_guid($wknguids{$wknguid});
		my $str  = guid_to_string($guid);
		my $back = guid_to_bindstring($guid);

		printf "wkguid:             %s\n", $wknguid;
		printf "bind_string format: %s\n", $wknguids{$wknguid};
		printf "string format:      %s\n", guid_to_string($guid);
		printf "back to bind_string:%s\n", guid_to_bindstring($guid);
				
		printf "use base: \"<WKGUID=%s,%s>\"\n", guid_to_bindstring($guid), $base;
	}
}

sub gen_bitmask_string_format($%) {
	my $mod = shift;
        my (%tmp) = @_;
	my @list;
	foreach my $key (sort keys %tmp) {
		push(@list, sprintf("%s %s", $tmp{$key}, $key));
	}
	return join("\n$mod$tabsize",@list);
}


sub nt_to_unixtime ($) {
	# the number of 100 nanosecond intervals since jan. 1. 1601 (utc)
	my $t64 = shift;
	$t64 =~ s/(.+).{7,7}/$1/;
	$t64 -= 11644473600;
	return $t64;
}

sub dump_equal {
	my $val = shift;
	my $mod = shift || die "no mod";
        my (%header) = @_;
	my %tmp;
	my $found = 0;
	foreach my $key (keys %header) {
		if ($header{$key} eq $val) {
			$tmp{"($val)"} = $key;
			$found = 1;
			last;
		}
	}
	if (!$found) { $tmp{$val} = ""; };
	return gen_bitmask_string_format($mod,%tmp);
}

sub dump_bitmask {
	my $op = shift || die "no op";
	my $val = shift; 
	my $mod = shift || die "no mod";
        my (%header) = @_;
	my %tmp;
	$tmp{""} = sprintf("%s (0x%08x)", $val, $val);
	foreach my $key (sort keys %header) {	# sort by val !
		my $val_hex = sprintf("0x%08x", $header{$key});
		if ($op eq "&") {
			$tmp{"$key ($val_hex)"} = ( $val & $header{$key} ) ? $set:$unset; 
		} elsif ($op eq "==") {
			$tmp{"$key ($val_hex)"} = ( $val == $header{$key} ) ? $set:$unset; 
		} else {
			print "unknown operator: $op\n";
			return;
		}
	}
	return gen_bitmask_string_format($mod,%tmp);
}

sub dump_bitmask_and {
	return dump_bitmask("&",@_);
}

sub dump_bitmask_equal {
	return dump_bitmask("==",@_);
}

sub dump_uac {
	return dump_bitmask_and(@_,%ads_uf); # ads_uf ?
}

sub dump_uascompat {
	return dump_bitmask_equal(@_,%ads_uascompat);
}

sub dump_gpoptions {
	return dump_bitmask_equal(@_,%ads_gpoptions);
}

sub dump_gpflags {
	return dump_bitmask_equal(@_,%ads_gpflags);
}

sub dump_uacc {
	return dump_bitmask_equal(@_,%ads_uacc); 
}

sub dump_enctypes {
	return dump_bitmask_and(@_,%ads_enctypes);
}

sub dump_uf {
	return dump_bitmask_and(@_,%ads_uf);
}

sub dump_gtype {
	my $ret = dump_bitmask_and(@_,%ads_grouptype);
	$ret .= "\n$tabsize\t";
	$ret .= dump_bitmask_equal(@_,%ads_gtype);
	return $ret;
}

sub dump_atype {
	return dump_bitmask_equal(@_,%ads_atype);
}

sub dump_controls {
	return dump_equal(@_,%ads_controls);
}

sub dump_capabilities {
	return dump_equal(@_,%ads_capabilities);
}

sub dump_extension {
	return dump_equal(@_,%ads_extensions);
}

sub dump_systemflags {
	return dump_bitmask_and(@_,%ads_systemflags);
}

sub dump_instance_type {
	return dump_bitmask_and(@_,%ads_instance_type);
}

sub dump_ds_func {
	return dump_bitmask_equal(@_,%ads_ds_func);
}

sub dump_serverstate {
	return dump_bitmask_equal(@_,%ads_serverstate);
}

sub dump_sdeffective {
	return dump_bitmask_and(@_,%ads_sdeffective);
}

sub dump_trustattr {
	return dump_bitmask_equal(@_,%ads_trustattrs);
}

sub dump_trusttype {
	return dump_bitmask_equal(@_,%ads_trusttype);
}

sub dump_trustdirection {
	return dump_bitmask_equal(@_,%ads_trustdirection);
}

sub dump_pwdproperties {
	return dump_bitmask_and(@_,%ads_pwdproperties);
}

sub dump_frstype {
	return dump_bitmask_equal(@_,%ads_frstypes)
}

sub dump_mixed_domain {
	return dump_bitmask_equal(@_,%ads_mixed_domain);
}

sub dump_sid {
	my $bin_sid = shift;
	return sid2string($bin_sid);
}

sub dump_guid {
	my $guid = shift;
	return guid_to_string($guid);
}

sub dump_secdesc {
	my $val = shift;
	return "FIXME: write sddl-converter!";
}

sub dump_nttime {
	my $nttime = shift;
	if ($nttime == 0) {
		return sprintf("%s (%s)", "never", $nttime);
	}
	my $localtime = localtime(nt_to_unixtime($nttime));
	return sprintf("%s (%s)", $localtime, $nttime);
}

sub dump_nttime_abs {
	if ($_[0] == 9223372036854775807) {
		return sprintf("%s (%s)", "now", $_[0]);
	}
	if ($_[0] == -9223372036854775808 || $_[0] == 0) { # 0x7FFFFFFFFFFFFFFF
		return sprintf("%s (%s)", "never", $_[0]);
	}
	# FIXME: actually *do* abs time !
	return dump_nttime($_[0]);
}

sub dump_timestr {
	my $time = shift;
	if ($time eq "16010101000010.0Z") {
		return sprintf("%s (%s)", "never", $time);
	}
	my ($year,$mon,$mday,$hour,$min,$sec,$zone) = 
		unpack('a4 a2 a2 a2 a2 a2 a4', $time);
	$mon -= 1;
	my $localtime = localtime(timegm($sec,$min,$hour,$mday,$mon,$year));
	return sprintf("%s (%s)", $localtime, $time);
}

sub dump_string {
	return $_[0];
}

sub dump_int {
	return sprintf("%d", $_[0]);
}

sub dump_munged_dial {
	my $dial = shift;
	return "FIXME! decode this";
}

sub dump_cert {
 
	my $cert = shift;
	open(OPENSSL, "| /usr/bin/openssl x509 -text -inform der");
	print OPENSSL $cert;
	close(OPENSSL);
	return "";
}

sub dump_pkt {
	my $pkt = shift;
	return "not yet";
	printf("%s: ", $pkt);
	printf("%02X", $pkt);
	
}

sub dump_gplink {

	my $gplink = shift;
	my $gplink_mod = $gplink;
	my @links = split("\\]\\[", $gplink_mod);
	foreach my $link (@links) {
		$link =~ s/^\[|\]$//g;
		my ($ldap_link, $opt) = split(";", $link);
		my @array = ( "$opt", "\t" );
		printf("%slink: %s, opt: %s\n", $tabsize, $ldap_link, dump_bitmask_and(@array, %ads_gplink_opts));
	}
	return $gplink;
}

sub construct_filter {

	my $tmp = shift;

	if (!$tmp || $opt_notify) {
		return "(objectclass=*)";
	}

	if ($tmp && $tmp !~ /^.*\=/) {
		return "(ANR=$tmp)";
	}

	return $tmp;
	# (&(objectCategory=person)
	# (userAccountControl:$ads_matching_rules{LDAP_MATCHING_RULE_BIT_AND}:=2))
}

sub construct_attrs {
	
	my @attrs = @_;
	
	if (!@attrs) {
		push(@attrs,"*");
	}

	if ($opt_notify) {
		push(@attrs,"uSNChanged");
	}

	if ($opt_display_metadata) {
		push(@attrs,"msDS-ReplAttributeMetaData");
		push(@attrs,"replPropertyMetaData");
	}

	if ($opt_verbose) {
		push(@attrs,"nTSecurityDescriptor");
		push(@attrs,"msDS-KeyVersionNumber");
		push(@attrs,"msDS-User-Account-Control-Computed");
		push(@attrs,"modifyTimeStamp");
	}

	return sort @attrs;
}

sub print_header {

	print "\n";
	printf "%10s: %s\n", "uri", $uri;
	printf "%10s: %s\n", "port", $port;
	printf "%10s: %s\n", "base", $base;
	printf "%10s: %s\n", $sasl_bind ? "principal" : "binddn", $sasl_bind ? $upn : $binddn;
	printf "%10s: %s\n", "password", $password;
	printf "%10s: %s\n", "bind-type", $sasl_bind ? "SASL" : "simple";
	printf "%10s: %s\n", "sasl-mech", $sasl_mech if ($sasl_mech);
	printf "%10s: %s\n", "filter", $filter;
	printf "%10s: %s\n", "scope", $scope;
	printf "%10s: %s\n", "attrs", join(", ", @attrs);
	printf "%10s: %s\n", "controls", join(", ", @ctrls_s);
	printf "%10s: %s\n", "page_size", $opt_paging if ($opt_paging);
	printf "%10s: %s\n", "start_tls", $opt_starttls ? "yes" : "no";
	printf "\n";
}

sub gen_controls {

	# setup attribute-scoped query control
	my $asq_asn = Convert::ASN1->new;
	$asq_asn->prepare(
		q<	asq ::=	SEQUENCE {
				sourceAttribute   OCTET_STRING
		  	}
		>
	);
	my $ctl_asq_val = $asq_asn->encode( sourceAttribute => $opt_asq);
	my $ctl_asq = Net::LDAP::Control->new(
		type => $ads_controls{'LDAP_SERVER_ASQ_OID'},
		critical => 'true',
		value => $ctl_asq_val);


	# setup extended dn control
	my $asn_extended_dn = Convert::ASN1->new;
	$asn_extended_dn->prepare(
		q<	ExtendedDn ::= SEQUENCE {
				mode     INTEGER
			}
		>
	);

	# only w2k3 accepts '1' and needs the ctl_val, w2k does not accept a ctl_val
	my $ctl_extended_dn_val = $asn_extended_dn->encode( mode => $opt_display_extendeddn);
	my $ctl_extended_dn = Net::LDAP::Control->new( 
			type => $ads_controls{'LDAP_SERVER_EXTENDED_DN_OID'},
			critical => 'true',
			value => $opt_display_extendeddn ? $ctl_extended_dn_val : "");

	# setup search options
	my $search_opt = Convert::ASN1->new;
	$search_opt->prepare(
		q<	searchopt ::= SEQUENCE {
				flags     INTEGER
			}
		>
	);

	my $tmp = $search_opt->encode( flags => $opt_search_opt);
	my $ctl_search_opt = Net::LDAP::Control->new( 
		type => $ads_controls{'LDAP_SERVER_SEARCH_OPTIONS_OID'},
		critical => 'true',
		value => $tmp);

	# setup notify control
	my $ctl_notification = Net::LDAP::Control->new( 
		type => $ads_controls{'LDAP_SERVER_NOTIFICATION_OID'},
		critical => 'true');


	# setup paging control
	$ctl_paged = Net::LDAP::Control->new( 
		type => $ads_controls{'LDAP_PAGED_RESULT_OID_STRING'},
		critical => 'true',
		size => $opt_paging ? $opt_paging : 1000);

	# setup domscope control
	my $ctl_domscope = Net::LDAP::Control->new( 
		type => $ads_controls{'LDAP_SERVER_DOMAIN_SCOPE_OID'},
		critical => 'true',
		value => "");

	if (defined($opt_paging) || $opt_dump_schema) {
		push(@ctrls, $ctl_paged);
		push(@ctrls_s, "LDAP_PAGED_RESULT_OID_STRING" );
	}

	if (defined($opt_display_extendeddn)) {
		push(@ctrls, $ctl_extended_dn);
		push(@ctrls_s, "LDAP_SERVER_EXTENDED_DN_OID");
	} 
	if ($opt_notify) {
		push(@ctrls, $ctl_notification);
		push(@ctrls_s, "LDAP_SERVER_NOTIFICATION_OID");
	}
	
	if ($opt_asq) {
		push(@ctrls, $ctl_asq);
		push(@ctrls_s, "LDAP_SERVER_ASQ_OID");
	}

	if ($opt_domain_scope) {
		push(@ctrls, $ctl_domscope);
		push(@ctrls_s, "LDAP_SERVER_DOMAIN_SCOPE_OID");
	}

	if ($opt_search_opt) {
		push(@ctrls, $ctl_search_opt);
		push(@ctrls_s, "LDAP_SERVER_SEARCH_OPTIONS_OID");
	}

	return @ctrls;
}

sub display_value_generic ($$$) {

	my ($mod,$attr,$value) = @_;
	return unless (defined($value) and defined($attr));

	if ( ! $opt_display_raw && exists $attr_handler{$attr} ) {
		$value = $attr_handler{$attr}($value,$mod);
		write_ads_list($mod,$attr,$value);
		return;
	}
	write_ads($mod,$attr,$value);
}

sub display_attr_generic ($$$) {

	my ($mod,$entry,$attr) = @_;
	return if (!$attr || !$entry);

	my $ref = $entry->get_value($attr, asref => 1);
	my @values = @$ref;

	foreach my $value ( sort @values) {
		display_value_generic($mod,$attr,$value);
	}
}

sub display_entry_generic ($) {

	my $entry = shift;
	return if (!$entry);

	foreach my $attr ( sort $entry->attributes) {
		display_attr_generic("\t",$entry,$attr);	
	}
}

sub display_ldap_err ($) {

	my $msg = shift;
	print_header();
	my ($package, $filename, $line, $subroutine) = caller(0);

	print "got error from ADS:\n";
	printf("%s(%d): ERROR (%s): %s\n", 
		$subroutine, $line, $msg->code, $msg->error);

}

sub process_result {

	my ($msg,$entry) = @_;

	if (!defined($msg)) { 
		return; 
	}

	if ($msg->code) {
		display_ldap_err($msg);
		exit 1;
	}

	if (!defined($entry)) {
		return;
	}

	if ($entry->isa('Net::LDAP::Reference')) {
		foreach my $ref ($entry->references) {
			print "\ngot Reference: [$ref]\n";
		}
		return;
	}

	print "\nfound entry: ".$entry->dn."\n".("-" x 80)."\n";

	display_entry_generic($entry);
}

sub default_callback {

	my ($res,$obj) = @_;

	if (!$opt_notify && $res->code == LDAP_REFERRAL) {
		return;
	}

	if (!$opt_notify) {
		return process_result($res,$obj);
	} 
}

sub error_callback {

	my ($msg,$entry) = @_;

	if (!$msg) { 
		return; 
	}

	if ($msg->code) {
		display_ldap_err($msg);
		exit(1);
	}
	return;
}

sub notify_callback {

	my ($msg, $obj) = @_;

	if (! defined($obj)) {
		return;
	}

	if ($obj->isa('Net::LDAP::Reference')) {
		foreach my $ref ($obj->references) {
			print "\ngot Reference: [$ref]\n";
		}
		return;
	}

	while (1) {

		my $async_entry = $async_search->pop_entry;

		if (!$async_entry->dn) {
			print "very weird. entry has no dn\n";
			next;
		}
	
		printf("\ngot changenotify for dn: [%s]\n%s\n", $async_entry->dn, ("-" x 80));

		my $new_usn = $async_entry->get_value('uSNChanged');
		if (!$new_usn) {
			die "odd, no usnChanged attribute???";
		}
		if (!$usn || $new_usn > $usn) {
			$usn = $new_usn;
			if ($opt_notify_nodiffs) {
				display_entry_generic($async_entry);
			} else {
				display_result_diff($async_entry);
			}
		} 
		store_result($async_entry);
	}
}

sub do_bind($$) {

	my $async_ldap_hd = shift || return undef;
	my $sasl_bind = shift;

	if ($sasl_bind) {
		if (!$works_sasl) {
			print "this version of Net::LDAP does not have proper SASL support.\n";
			print "Need at least perl-ldap V. $pref_version\n";
		}
		$sasl_hd = get_sasl_hd();
		if (!$sasl_hd) {
			print "failed to create SASL handle\n";
			return -1;
		}
		$mesg = $async_ldap_hd->bind( 
			sasl => $sasl_hd, 
			callback => \&error_callback 
		) || die "doesnt work"; 
	} else {
		$sasl_mech = "";
		$mesg = $async_ldap_hd->bind( 
			$binddn, 
			password => $password, 
			callback => $opt_fastbind ? undef : \&error_callback
		) || die "doesnt work";
	};
	if ($mesg->code) { 
		display_ldap_err($mesg) if (!$opt_fastbind);
		return -1;
	}
	return 0;
}

sub do_fast_bind() {

	do_unbind();

	my $hd = get_ldap_hd($server,1);

	my $mesg = $hd->extension(
		name => $ads_extensions{'LDAP_SERVER_FAST_BIND_OID'});
	
	if ($mesg->code) { 
		display_ldap_err($mesg);
		return -1;
	}

	my $ret = do_bind($hd, undef);
	$hd->unbind;

	printf("bind using 'LDAP_SERVER_FAST_BIND_OID' %s\n", 
		$ret == 0 ? "succeeded" : "failed");

	return $ret;
}

sub do_unbind() {
	if ($async_ldap_hd) {
		$async_ldap_hd->unbind;
	}
	if ($sync_ldap_hd) {
		$sync_ldap_hd->unbind;
	}
}

###############################################################################

sub main () {

	if ($opt_fastbind) {

		if (check_exts($server,$dse,"LDAP_SERVER_FAST_BIND_OID") == -1) {
			print "LDAP_SERVER_FAST_BIND_OID not supported by this server\n";
			exit 1;
		}

		# fast bind can only bind, not search
		exit do_fast_bind();
	}

	$filter = construct_filter($query);
	@attrs  = construct_attrs(@attrs);
	@ctrls  = gen_controls();

	if (check_ctrls($server,$dse,@ctrls) == -1) {
		print "not all required LDAP Controls are supported by this server\n";
		exit 1;
	}

	if ($opt_dump_rootdse) {
		print "Dumping Root-DSE:\n";
		display_entry_generic($dse);
		exit 0;
	}

	if ($opt_dump_wknguid) {
		dump_wknguid();
		exit 0;
	}

	if (do_bind($async_ldap_hd, $sasl_bind) == -1) {
		exit 1;
	}

	print_header();

	if ($opt_dump_schema) {
		print "Dumping Schema:\n";
#		my $ads_schema = $async_ldap_hd->schema;
#		$ads_schema->dump;
#		exit 0;
	}

	while (1) {

		$async_search = $async_ldap_hd->search( 
			base => $base, 
			filter => $filter, 
			attrs => [ @attrs ],
			control => [ @ctrls ], 
			callback => \&default_callback,
			scope => $scope,
				) || die "cannot search";

		if (!$opt_notify && ($async_search->code == LDAP_REFERRAL)) {
			foreach my $ref ($async_search->referrals) {
				print "\ngot Referral: [$ref]\n";
				my ($prot, $host, $base) = split(/\/+/, $ref);
				$async_ldap_hd->unbind();
				$async_ldap_hd = get_ldap_hd($host, 1);
				if (do_bind($async_ldap_hd, $sasl_bind) == -1) {
					$async_ldap_hd->unbind();
					next;
				}
				print "\nsuccessful rebind to: [$ref]\n";
				last;
			}
			next; # repeat the query
		}

		if ($opt_notify) {

			print "Base [$base] is registered now for change-notify\n";
			print "\nWaiting for change-notify...\n";

			my $sync_ldap_hd = get_ldap_hd($server,0);
			if (do_bind($sync_ldap_hd, $sasl_bind) == -1) {
				exit 2;
			}

			my $sync_search = $sync_ldap_hd->search( 
				base => $base, 
				filter => $filter, 
				attrs => [ @attrs ],
				callback => \&notify_callback,
				scope => $scope,
				) || die "cannot search";

		}

		$total_entry_count += $async_search->count;
		++$page_count;
		print "-" x 80 . "\n";
		printf("Got %d Entries in Page %d \n\n", 
			$async_search->count, $page_count);
		my ($resp) = $async_search->control( 
			$ads_controls{'LDAP_PAGED_RESULT_OID_STRING'} ) or last;
		last unless ref $resp && $ctl_paged->cookie($resp->cookie);
	}

	if ($async_search->entries == 0) {
		print "No entries found with filter $filter\n";
	} else {
		print "Got $total_entry_count Entries in $page_count Pages\n" if ($opt_paging);
	}

	do_unbind()
}

main();

exit 0;
