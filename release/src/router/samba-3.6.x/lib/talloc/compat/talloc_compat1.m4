TALLOC_COMPAT1_MK=""
AC_SUBST(TALLOC_COMPAT1_MK)

AC_ARG_ENABLE(talloc-compat1,
	[AS_HELP_STRING([--enable-talloc-compat1],
		[Build talloc 1.x.x compat library [default=no]])],
	[ enable_talloc_compat1=$enableval ],
	[ enable_talloc_compat1=no ]
)

if test "x$enable_talloc_compat1" = x"yes"; then
	TALLOC_COMPAT1_MK='include $(tallocdir)/compat/talloc_compat1.mk'
fi

