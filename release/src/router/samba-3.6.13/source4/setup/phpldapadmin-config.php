<?php
/**
 * The phpLDAPadmin config file, customised for use with Samba4
 * This overrides phpLDAPadmin defaults
 * that are defined in config_default.php.
 *
 * DONT change config_default.php, you changes will be lost by the next release
 * of PLA. Instead change this file - as it will NOT be replaced by a new
 * version of phpLDAPadmin.
 */

/*********************************************/
/* Useful important configuration overrides  */
/*********************************************/

/* phpLDAPadmin can encrypt the content of sensitive cookies if you set this
   to a big random string. */

$i=0;
$ldapservers = new LDAPServers;

/* A convenient name that will appear in the tree viewer and throughout
   phpLDAPadmin to identify this LDAP server to users. */
$ldapservers->SetValue($i,'server','name','Samba4 LDAP Server');
$ldapservers->SetValue($i,'server','host','${S4_LDAPI_URI}');
$ldapservers->SetValue($i,'server','auth_type','session');
$ldapservers->SetValue($i,'login','attr','dn');
?>
