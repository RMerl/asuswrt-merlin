AC_DEFUN([AC_MSG_MODULE_DBG],
[
  if test $module_debug = 1; then
    echo $1 $2 $3 $4
  fi
]
)
