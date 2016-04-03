dnl Boilerplate
AC_INIT([ipset], [6.29], [kadlec@blackhole.kfki.hu])
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects tar-pax])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ENABLE_STATIC
LT_INIT([dlopen])
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT([nonrecursive])

PKG_PROG_PKG_CONFIG

dnl Shortcut: Linux supported alone
case "$host" in
*-*-linux* | *-*-uclinux*) ;;
*) AC_MSG_ERROR([Linux systems supported exclusively!]);;
esac

dnl Optionnally disable building the kernel module
AC_ARG_WITH([kmod],
            AS_HELP_STRING([--with-kmod=yes/no],
                           [Build the kernel module (default: yes)]),
            [BUILDKMOD="$withval";],
            [BUILDKMOD="yes";])
AM_CONDITIONAL(WITH_KMOD, test "$BUILDKMOD" == "yes")

dnl Additional arguments
dnl Kernel build directory or source tree
AC_ARG_WITH([kbuild],
            AS_HELP_STRING([--with-kbuild=PATH],
                           [Path to kernel build directory]),
            [KBUILDDIR="$withval";])
AC_ARG_WITH([ksource],
            AS_HELP_STRING([--with-ksource=PATH],
                           [Path to kernel source directory, if not the same as the kernel build directory]),
            [KSOURCEDIR="$withval";])
AM_CONDITIONAL(WITH_KBUILDDIR, test "$KBUILDDIR" != "")
AC_SUBST(KBUILDDIR)

dnl ipset bash compspec - bash shell programmable completion
AC_ARG_ENABLE([bashcompl],
	AS_HELP_STRING([--enable-bashcompl],
	[Install bash completion for ipset]),
	[enable_bashcompl="$enableval"], [enable_bashcompl="no"])

# backward compatibility with older pkg-config
m4_ifndef([PKG_CHECK_VAR], [
# PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
# [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -------------------------------------------
# Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])# PKG_CHECK_VAR
])

if test "x$enable_bashcompl" = "xyes"; then
	PKG_CHECK_VAR(bashcompdir, [bash-completion], [completionsdir], , bashcompdir="${sysconfdir}/bash_completion.d")
	AC_SUBST(bashcompdir)
fi

if test "$BUILDKMOD" == "yes"
then
dnl Sigh: check kernel version dependencies
if test "$KBUILDDIR" != ""
then
	kbuilddir="$KBUILDDIR"
else
	kbuilddir="/lib/modules/`uname -r`/build"
fi

if test -n "$KSOURCEDIR"; then
	ksourcedir="$KSOURCEDIR"
elif test -e "$kbuilddir/include/linux/netfilter/nfnetlink.h"; then
	ksourcedir="$kbuilddir"
else
	ksourcedir="/lib/modules/$(uname -r)/source"
fi
if test ! -e "$ksourcedir/include/linux/netfilter/nfnetlink.h"
then
	AC_MSG_ERROR([Invalid kernel source directory $ksourcedir])
fi

if test ! -e "$kbuilddir/.config"
then
	AC_MSG_ERROR([The kernel build directory $kbuilddir is not configured])
fi

AC_PROG_GREP
AC_PROG_AWK

if ! $GREP -q "NFNL_SUBSYS_IPSET" "$ksourcedir/include/linux/netfilter/nfnetlink.h" && \
   ! $GREP -q "NFNL_SUBSYS_IPSET" "$ksourcedir/include/uapi/linux/netfilter/nfnetlink.h";
then
	AC_MSG_ERROR([The kernel source directory $ksourcedir is not patched with netlink.patch to support ipset])
fi
fi

dnl Maximal number of sets supported by the kernel, default 256
AC_ARG_WITH([maxsets],
	    AS_HELP_STRING([--with-maxsets=256],
	    		   [Maximal numer of sets supported by the kernel]),
	    [MAXSETS="$withval";])
AM_CONDITIONAL(WITH_MAXSETS, test "$MAXSETS" != "")
AC_SUBST(MAXSETS)

dnl Verbose compiling
AC_ARG_ENABLE([verbose],
	      AS_HELP_STRING([--enable-verbose],
	                     [Enable verbose mode at compiling/linking.]),
	      [case "${enableval}" in
	       yes)	enable_verbose=yes ;;
	       no)	enable_verbose=no ;;
	       *) AC_MSG_ERROR([bad value ${enableval} for --enable-verbose]) ;;
	       esac], [enable_verbose=no])
	      
AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
	[], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
	AC_DEFINE(ENABLE_DEBUG, [1], [Debug messages.])
])
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = xyes])

dnl Enable type modules
AC_ARG_ENABLE([settype_modules],
	          AS_HELP_STRING([--enable-settype-modules],
	          [Enable set type modules support]),
	          [enable_settype_modules="$enableval"],
	          [enable_settype_modules="no"])

AC_ARG_WITH([settype_modules_list],
	        AS_HELP_STRING([--with-settype-modules-list="mod1 mod2 ..."],
	                       [List of dynamic loading modules, ignored if settype-modules is disabled. It could be "all" to build all available settypes as modules]),
	                       [SETTYPE_MODLIST_RAW="$withval";])

SETTYPE_MODLIST=
if test "x$enable_settype_modules" = "xyes"; then
	for mod in $SETTYPE_MODLIST_RAW; do
		if echo $mod | grep "all"; then
			m="${mod}"
		else
			if echo $mod | grep "ipset_"; then
				m="${mod}.c"
			else
				m="ipset_${mod}.c"
			fi
		fi

		SETTYPE_MODLIST="${SETTYPE_MODLIST} $m"
	done

	AC_MSG_RESULT([checking for configuration with dynamic loading modules... $SETTYPE_MODLIST_RAW])
fi
AC_SUBST(SETTYPE_MODLIST)

AM_CONDITIONAL([ENABLE_SETTYPE_MODULES], [test "x$enable_settype_modules" = xyes])

AM_CONDITIONAL([ENABLE_STATIC], [test "x$enable_static" = xyes])
AM_CONDITIONAL([ENABLE_SHARED], [test "x$enable_shared" = xyes])

dnl Checks for programs
: ${CFLAGS=""}

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries
PKG_CHECK_MODULES([libmnl], [libmnl >= 1])

dnl Checks for header files

dnl Checks for declarations
AC_CHECK_DECLS([NLA_F_NESTED, NLA_F_NET_BYTEORDER, NLA_TYPE_MASK],,
		[AC_MSG_ERROR([System kernel header files are older than 2.6.24, use CFLAGS for non-default location])],
		[#include <sys/socket.h>
#include <linux/netlink.h>])

dnl Checks for typedefs, structures
AC_CHECK_TYPES([union nf_inet_addr],,,[#include <linux/types.h>
#include <netinet/in.h>
#include <linux/netfilter.h>])

dnl Checks for functions
AC_CHECK_FUNCS(gethostbyname2)

if test "$BUILDKMOD" == "yes"
then
dnl Check kernel incompatibilities... Ugly like hell
AC_MSG_CHECKING([kernel source for struct xt_action_param])
if test -f $ksourcedir/include/linux/netfilter/x_tables.h && \
   $GREP -q 'struct xt_action_param' $ksourcedir/include/linux/netfilter/x_tables.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_STRUCT_XT_ACTION_PARAM, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_STRUCT_XT_ACTION_PARAM, undef)
fi

AC_MSG_CHECKING([kernel source for vzalloc])
if test -f $ksourcedir/include/linux/vmalloc.h && \
   $GREP -q 'vzalloc' $ksourcedir/include/linux/vmalloc.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_VZALLOC, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_VZALLOC, undef)
fi

AC_MSG_CHECKING([kernel source for ether_addr_equal])
if test -f $ksourcedir/include/linux/etherdevice.h && \
   $GREP -q 'ether_addr_equal' $ksourcedir/include/linux/etherdevice.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_ETHER_ADDR_EQUAL, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_ETHER_ADDR_EQUAL, undef)
fi

AC_MSG_CHECKING([kernel source for nla_put_be16])
if test -f $ksourcedir/include/net/netlink.h && \
   $GREP -q 'nla_put_be16' $ksourcedir/include/net/netlink.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NLA_PUT_BE16, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NLA_PUT_BE16, undef)
fi

AC_MSG_CHECKING([kernel source for nla_put_be64])
if test -f $ksourcedir/include/net/netlink.h && \
   $GREP -q 'nla_put_be64' $ksourcedir/include/net/netlink.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NLA_PUT_BE64, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NLA_PUT_BE64, undef)
fi

AC_MSG_CHECKING([kernel source for portid in nl_info])
if test -f $ksourcedir/include/linux/netlink.h && \
   $AWK '/^struct netlink_skb_parms/ {for(i=1; i<=5; i++) {getline; print}}' $ksourcedir/include/linux/netlink.h | $GREP -q 'portid;'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NL_INFO_PORTID, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NL_INFO_PORTID, undef)
fi

AC_MSG_CHECKING([kernel source for netlink_dump_start args])
if test -f $ksourcedir/include/linux/netlink.h && \
   $AWK '/netlink_dump_start/ {for(i=1; i<=4; i++) {getline; print}}' $ksourcedir/include/linux/netlink.h | $GREP -q 'done.*;'; then
	AC_MSG_RESULT(5 args)
	AC_SUBST(HAVE_NETLINK_DUMP_START_ARGS, 5)
elif test -f $ksourcedir/include/linux/netlink.h && \
   $AWK '/netlink_dump_start/ {for(i=1; i<=4; i++) {getline; print}}' $ksourcedir/include/linux/netlink.h | $GREP -q 'min_dump_alloc.*;'; then
	AC_MSG_RESULT(6 args)
	AC_SUBST(HAVE_NETLINK_DUMP_START_ARGS, 6)
else
	AC_MSG_RESULT(4 args)
	AC_SUBST(HAVE_NETLINK_DUMP_START_ARGS, 4)
fi

AC_MSG_CHECKING([kernel source for ns_capable])
if test -f $ksourcedir/include/linux/capability.h && \
   $GREP -q 'ns_capable' $ksourcedir/include/linux/capability.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NS_CAPABLE, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NS_CAPABLE, undef)
fi

AC_MSG_CHECKING([kernel source for nfnl_lock per subsys])
if test -f $ksourcedir/include/linux/netfilter/nfnetlink.h && \
   $GREP -q 'nfnl_lock.* subsys_id' $ksourcedir/include/linux/netfilter/nfnetlink.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NFNL_LOCK_SUBSYS, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NFNL_LOCK_SUBSYS, undef)
fi

AC_MSG_CHECKING([kernel source for export.h])
if test -f $ksourcedir/include/linux/export.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_EXPORT_H, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_EXPORT_H, undef)
fi

AC_MSG_CHECKING([kernel source for ipv6_skip_exthdr args])
if test -f $ksourcedir/include/net/ipv6.h && \
   $AWK '/ipv6_skip_exthdr/ {getline; print}' $ksourcedir/include/net/ipv6.h | $GREP -q 'frag_offp'; then
	AC_MSG_RESULT(4 args)
	AC_SUBST(HAVE_IPV6_SKIP_EXTHDR_ARGS, 4)
else
	AC_MSG_RESULT(3 args)
	AC_SUBST(HAVE_IPV6_SKIP_EXTHDR_ARGS, 3)
fi

AC_MSG_CHECKING([kernel source for bool checkentry function prototype])
if test -f $ksourcedir/include/linux/netfilter/x_tables.h && \
   $GREP -q 'bool .\*checkentry.' $ksourcedir/include/linux/netfilter/x_tables.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_CHECKENTRY_BOOL, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_CHECKENTRY_BOOL, undef)
fi

AC_MSG_CHECKING([kernel source for old struct xt_target_param])
if test -f $ksourcedir/include/linux/netfilter/x_tables.h && \
   $GREP -q '^struct xt_target_param ' $ksourcedir/include/linux/netfilter/x_tables.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_XT_TARGET_PARAM, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_XT_TARGET_PARAM, undef)
fi

AC_MSG_CHECKING([kernel source for id in struct pernet_operations])
if test -f $ksourcedir/include/net/net_namespace.h && \
   $AWK '/struct pernet_operations/ {for(i=1; i<=6; i++) {getline; print}}' $ksourcedir/include/net/net_namespace.h | $GREP -q 'int \*id;'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NET_OPS_ID, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NET_OPS_ID, undef)
fi

AC_MSG_CHECKING([kernel source for user_ns in struct net])
if test -f $ksourcedir/include/net/net_namespace.h && \
   $AWK '/^struct net \{/ {for(i=1; i<=20; i++) {getline; print}}' $ksourcedir/include/net/net_namespace.h | $GREP -q 'user_ns'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_USER_NS_IN_STRUCT_NET, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_USER_NS_IN_STRUCT_NET, undef)
fi

AC_MSG_CHECKING([kernel source for rbtree_postorder_for_each_entry_safe])
if test -f $ksourcedir/include/linux/rbtree.h && \
   $GREP -q 'rbtree_postorder_for_each_entry_safe' $ksourcedir/include/linux/rbtree.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_RBTREE_POSTORDER_FOR_EACH_ENTRY_SAFE, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_RBTREE_POSTORDER_FOR_EACH_ENTRY_SAFE, undef)
fi

AC_MSG_CHECKING([kernel source for kvfree])
if test -f $ksourcedir/include/linux/mm.h && \
   $GREP -q 'kvfree' $ksourcedir/include/linux/mm.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_KVFREE, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_KVFREE, undef)
fi

AC_MSG_CHECKING([kernel source for struct net in struct xt_mtchk_param])
if test -f $ksourcedir/include/linux/netfilter/x_tables.h && \
   $AWK '/^struct xt_mtchk_param / {for(i=1; i<=5; i++) {getline; print}}' $ksourcedir/include/linux/netfilter/x_tables.h | \
   $GREP -q 'struct net '; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_XT_MTCHK_PARAM_STRUCT_NET, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_XT_MTCHK_PARAM_STRUCT_NET, undef)
fi

AC_MSG_CHECKING([kernel source for struct net in the change function of tcf_ematch_ops])
if test -f $ksourcedir/include/net/pkt_cls.h && \
   $AWK '/^struct tcf_ematch_ops / {for(i=1; i<=5; i++) {getline; print}}' $ksourcedir/include/net/pkt_cls.h | \
   $GREP -q '\*change..struct net \*net'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_TCF_EMATCH_OPS_CHANGE_ARG_NET, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_TCF_EMATCH_OPS_CHANGE_ARG_NET, undef)
fi

AC_MSG_CHECKING([kernel source for struct net in struct tcf_ematch])
if test -f $ksourcedir/include/net/pkt_cls.h && \
   $AWK '/^struct tcf_ematch / {for(i=1; i<=7; i++) {getline; print}}' $ksourcedir/include/net/pkt_cls.h | \
   $GREP -q 'struct net'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_TCF_EMATCH_STRUCT_NET, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_TCF_EMATCH_STRUCT_NET, undef)
fi

AC_MSG_CHECKING([kernel source for list_last_entry])
if test -f $ksourcedir/include/linux/list.h && \
   $GREP -q 'list_last_entry' $ksourcedir/include/linux/list.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_LIST_LAST_ENTRY, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_LIST_LAST_ENTRY, undef)
fi

AC_MSG_CHECKING([kernel source for list_next_entry])
if test -f $ksourcedir/include/linux/list.h && \
   $GREP -q 'list_next_entry' $ksourcedir/include/linux/list.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_LIST_NEXT_ENTRY, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_LIST_NEXT_ENTRY, undef)
fi

AC_MSG_CHECKING([kernel source for ether_addr_copy])
if test -f $ksourcedir/include/linux/etherdevice.h && \
   $GREP -q 'ether_addr_copy' $ksourcedir/include/linux/etherdevice.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_ETHER_ADDR_COPY, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_ETHER_ADDR_COPY, undef)
fi

AC_MSG_CHECKING([kernel source for nf_bridge_get_physindev])
if test -f $ksourcedir/include/linux/netfilter_bridge.h && \
   $GREP -q 'nf_bridge_get_physindev' $ksourcedir/include/linux/netfilter_bridge.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NF_BRIDGE_GET_PHYSDEV, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NF_BRIDGE_GET_PHYSDEV, undef)
fi

AC_MSG_CHECKING([kernel source for nla_put_in_addr])
if test -f $ksourcedir/include/net/netlink.h && \
   $GREP -q 'nla_put_in_add' $ksourcedir/include/net/netlink.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NLA_PUT_IN_ADDR, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NLA_PUT_IN_ADDR, undef)
fi

AC_MSG_CHECKING([kernel source for struct net in struct nfnl_callback])
if test -f $ksourcedir/include/linux/netfilter/nfnetlink.h && \
   $AWK '/^struct nfnl_callback / {for(i=1; i<=3; i++) {getline; print}}' $ksourcedir/include/linux/netfilter/nfnetlink.h | \
   $GREP -q 'struct net'; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_NET_IN_NFNL_CALLBACK_FN, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_NET_IN_NFNL_CALLBACK_FN, undef)
fi

AC_MSG_CHECKING([kernel source for EXPORT_SYMBOL_GPL in module.h])
if test -f $ksourcedir/include/linux/module.h && \
   $GREP -q 'EXPORT_SYMBOL_GPL' $ksourcedir/include/linux/module.h; then
	AC_MSG_RESULT(yes)
	AC_SUBST(HAVE_EXPORT_SYMBOL_GPL_IN_MODULE_H, define)
else
	AC_MSG_RESULT(no)
	AC_SUBST(HAVE_EXPORT_SYMBOL_GPL_IN_MODULE_H, undef)
fi

AC_MSG_CHECKING([kernel source for struct net_generic])
if test -f $ksourcedir/include/net/netns/generic.h && \
   $GREP -q 'struct net_generic' $ksourcedir/include/net/netns/generic.h; then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([Netns support is required in the Linux kernel tree])
fi
fi

dnl Checks for compiler characteristics.
dnl Check extra warning flags except
dnl	-Wconversion		-> we need it
dnl	-Wunreachable-code	-> fails with ntoh*
dnl	-Wpointer-arith		-> limbnl uses it
dnl	-Wsign-conversion	-> libmnl
if test "x$enable_debug" = "xyes"
then
AX_CFLAGS_GCC_OPTION(-Waggregate-return)
AX_CFLAGS_GCC_OPTION(-Wbad-function-cast)
AX_CFLAGS_GCC_OPTION(-Wcast-align)
AX_CFLAGS_GCC_OPTION(-Wcast-qual)
AX_CFLAGS_GCC_OPTION(-Werror)
AX_CFLAGS_GCC_OPTION(-Wextra)
AX_CFLAGS_GCC_OPTION(-Wfloat-equal)
AX_CFLAGS_GCC_OPTION(-Wformat=2)
AX_CFLAGS_GCC_OPTION(-Wjump-misses-init)
AX_CFLAGS_GCC_OPTION(-Winit-self)
AX_CFLAGS_GCC_OPTION(-Winline)
AX_CFLAGS_GCC_OPTION(-Wlogical-op)
AX_CFLAGS_GCC_OPTION(-Wmissing-declarations)
AX_CFLAGS_GCC_OPTION(-Wmissing-format-attribute)
AX_CFLAGS_GCC_OPTION(-Wmissing-prototypes)
AX_CFLAGS_GCC_OPTION(-Wnested-externs)
AX_CFLAGS_GCC_OPTION(-Wno-missing-field-initializers)
AX_CFLAGS_GCC_OPTION(-Wold-style-definition)
AX_CFLAGS_GCC_OPTION(-Woverlength-strings)
AX_CFLAGS_GCC_OPTION(-Wpacked)
AX_CFLAGS_GCC_OPTION(-Wredundant-decls)
AX_CFLAGS_GCC_OPTION(-Wrwrite-strings)
AX_CFLAGS_GCC_OPTION(-Wshadow)
AX_CFLAGS_GCC_OPTION(-Wsign-compare)
AX_CFLAGS_GCC_OPTION(-Wstrict-prototypes)
AX_CFLAGS_GCC_OPTION(-Wswitch-default)
AX_CFLAGS_GCC_OPTION(-Wundef)
AX_CFLAGS_GCC_OPTION(-Wuninitialized)
AX_CFLAGS_GCC_OPTION(-Wunused)
AX_CFLAGS_GCC_OPTION(-Wvla)
AX_CFLAGS_GCC_OPTION(-Wwrite-strings)
fi
dnl Checks for library functions.

dnl Generate output
AC_CONFIG_FILES([Makefile include/libipset/Makefile
	lib/Makefile lib/libipset.pc src/Makefile utils/Makefile
	kernel/include/linux/netfilter/ipset/ip_set_compat.h])
AC_OUTPUT

dnl Summary
AC_MSG_RESULT([])
AC_MSG_RESULT([$PACKAGE userspace tool configuration:])
if test "x$bashcompdir" != "x"; then
	AC_MSG_RESULT([    Bash completion directory: ${bashcompdir}])
fi
if test "x$enable_settype_modules" != "xyes"; then
	AC_MSG_RESULT([    Dynamic module loading: disabled])
else
	AC_MSG_RESULT([    Dynamic module loading: enabled])
fi
IPSET_ALL_MODULES="`ls ${srcdir}/lib/ipset_*.c|sed -e 's/^.*lib\///' -e 's/\.c$//'`"
AC_MSG_RESULT([    Static modules:])
if test "x$SETTYPE_MODLIST" = "x"; then
	for mod in $IPSET_ALL_MODULES; do
		AC_MSG_RESULT([        ${mod}])
	done
	AC_MSG_RESULT([    Dynamic modules:])
elif echo $SETTYPE_MODLIST | grep "all" >/dev/null; then
	AC_MSG_RESULT([    Dynamic modules:])
	for mod in $IPSET_ALL_MODULES; do
		AC_MSG_RESULT([        ${mod}])
	done
else
	for mod in $IPSET_ALL_MODULES; do
		if echo $SETTYPE_MODLIST | grep $mod >/dev/null; then
			:
		else
			AC_MSG_RESULT([        ${mod}])
		fi
	done
	AC_MSG_RESULT([    Dynamic modules:])
	for mod in $IPSET_ALL_MODULES; do
		if echo $SETTYPE_MODLIST | grep $mod >/dev/null; then
			AC_MSG_RESULT([        ${mod}])
		fi
	done
fi
