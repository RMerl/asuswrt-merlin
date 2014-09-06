dnl
dnl Store information for displaying later.
dnl
AC_DEFUN([AC_MSG_CACHE_INIT],[
  rm -f configure-summary
])

AC_DEFUN([AC_MSG_CACHE_ADD],[
  cat >> configure-summary << EOF
  $1
EOF
])

AC_DEFUN([AC_MSG_CACHE_DISPLAY],[
  echo ""
  echo "---------------------------------------------------------"
  echo "            Net-SNMP configuration summary:"
  echo "---------------------------------------------------------"
  echo ""
  cat configure-summary
  echo ""
  echo "---------------------------------------------------------"
  echo ""
])
