/*
  the menu hierarchy for SWAT
*/

include("/scripting/menus.js");

swat_menus = new Object();

swat_menus.main = simple_menu(
	"Main Menu",
	"Servers",            session_uri("/?menu=servers"),
	"Installation",       session_uri("/install/"),
	"ESP Tests",          session_uri("/esptest/"));

swat_menus.servers = simple_menu(
	"Servers",
	"LDAP Server",        session_uri("/ldap_server/"),
	"CLDAP Server",       session_uri("/cldap_server/"),
	"RPC Server",         session_uri("/rpc_server/"),
	"NBT Server",         session_uri("/nbt_server/"),
	"WINS Server",        session_uri("/wins_server/"),
	"Kerberos Server",    session_uri("/kdc_server/"));

swat_menus.esptest = simple_menu(
	"ESP Tests",
	"ldb database",          session_uri("/esptest/ldb.esp"),
	"samr calls",            session_uri("/esptest/samr.esp"),
	"html forms",            session_uri("/esptest/formtest.esp"),
	"esp includes",          session_uri("/esptest/include.esp"),
	"session variables",     session_uri("/esptest/session.esp"),
	"loadparm access",       session_uri("/esptest/loadparm.esp"),
	"exception handling",    session_uri("/esptest/exception.esp"),
	"environment variables", session_uri("/esptest/showvars.esp"),
	"qooxdoo",               session_uri("/esptest/qooxdoo.esp"),
	"registry",              session_uri("/esptest/registry.esp"));


swat_menus.install = simple_menu(
	"Installation",
	"Provisioning",       session_uri("/install/provision.esp"),
	"New User",           session_uri("/install/newuser.esp"),
	"Import from Samba3", session_uri("/install/samba3.esp"),
	"Import from Windows", session_uri("/install/vampire.esp"));


swat_menus.nbt_server = simple_menu(
	"NBT Server",
	"Statistics",          session_uri("/nbt_server/index.esp"));


swat_menus.docs = simple_menu(
	"Links",
	"Samba4 development", "http://devel.samba.org/",
	"Recent Checkins", "http://build.samba.org/?tree=samba4;function=Recent+Checkins",
	"Recent Builds", "http://build.samba.org/?tree=samba4;function=Recent+Builds",
	"EJS Information", "http://www.appwebserver.org/products/ejs/ejs.html",
	"ESP Information", "http://www.appwebserver.org/products/esp/esp.html",
	"XHTML Spec", "http://www.w3.org/TR/xhtml1/",
	"JavaScript Spec", "http://www.ecma-international.org/publications/files/ecma-st/ECMA-262.pdf",
	"CSS Specs", "http://www.w3.org/Style/CSS/#specs",
	"CSS1/2 Reference", "http://www.w3schools.com/css/css_reference.asp",
	"qooxdoo info", "http://qooxdoo.sourceforge.net/");

