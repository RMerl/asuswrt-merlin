s|po2tbl\.sed\.in|po2tblsed.in|g
s|gdb\.c++|gdb.cxx|g
/ac_rel_source/s|ln -s|cp -p|
s|\.gdbinit|gdb.ini|g

# Edit Makefiles.  This should go near the beginning of
# the substitutions script, before the branch command that
# skips any lines without @...@ in them.
# Any commands that can match again after substitution must
# do a conditional branch to next cycle (;t), or else Sed might hang.
/(echo[ 	]*':t/a\
  s,\\([yp*]\\)\\.tab,\\1_tab,g\
  s,\\$@\\.tmp,\\$@_tmp,g\
  s,\\$@\\.new,\\$@_new,g\
  /^	@rm -f/s,\\$@-\\[0-9\\]\\[0-9\\],& *.i[1-9] *.i[1-9][0-9],;t\
  s,standards\\.info\\*,standard*.inf*,\
  s,configure\\.info\\*,configur*.inf*,\
  s,\\.info\\*,.inf* *.i[1-9] *.i[1-9][0-9],\
  s,\\.gdbinit,gdb.ini,g\
  s,@PATH_SEPARATOR@,";",\
  /TEXINPUTS=/s,:,";",g\
  /VPATH *=/s,\\([^A-z]\\):,\\1;,g\
  /\\$\\$file-\\[0-9\\]/s,echo,& *.i[1-9] *.i[1-9][0-9],;t\
  /\\$\\$file-\\[0-9\\]/s,rm -f \\$\\$file,& \\${PACKAGE}.i[1-9] \\${PACKAGE}.i[1-9][0-9],;t\
  s,config\\.h\\.in,config.h-in,g;t t\
  s,po2tbl\\.sed\\.in,po2tblsed.in,g;t t

# Prevent splitting of config.status substitutions, because that
# might break multi-line sed commands.
/ac_max_sed_lines=[0-9]/s,=.*$,=`sed -n "$=" $tmp/subs.sed`,

/^ac_given_srcdir=/,/^CEOF/ {
  /^s%@TOPLEVEL_CONFIGURE_ARGUMENTS@%/a\
  /@test ! -f /s,\\(.\\)\$, export am_cv_exeext=.exe; export lt_cv_sys_max_cmd_len=12288; \\1,\
  /@test -f stage_last /s,\\(.\\)\$, export am_cv_exeext=.exe; export lt_cv_sys_max_cmd_len=12288; \\1,

}

/^CONFIG_FILES=/,/^EOF/ {
  s|po/Makefile.in\([^-:a-z]\)|po/Makefile.in:po/Makefile.in-in\1|
}

/^ *# *Handling of arguments/,/^done/ {
  s| config.h"| config.h:config.h-in"|
  s|config.h\([^-:"a-z]\)|config.h:config.h-in\1|
}

/^[ 	]*\/\*)/s,/\*,/*|[A-z]:/*,
/\$]\*) INSTALL=/s,\[/\$\]\*,&|[A-z]:/*,
/\$]\*) ac_rel_source=/s,\[/\$\]\*,&|[A-z]:/*,
/ac_file_inputs=/s,\( -e "s%\^%\$ac_given_srcdir/%"\)\( -e "s%:% $ac_given_srcdir/%g"\),\2\1,
/^[ 	]*if test "x`echo /s,sed 's@/,sed -e 's@^[A-z]:@@' -e 's@/,
/^ *ac_config_headers=/s, config.h", config.h:config.h-in",
