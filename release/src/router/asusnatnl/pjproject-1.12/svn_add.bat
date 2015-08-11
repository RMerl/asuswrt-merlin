@echo off

if "%*" EQU "" (
	echo Usage: svn_add.bat FILE1 ...
	goto end
)

svn add %*

svn_pset.bat %*

:end
