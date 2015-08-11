@echo off

if "%*" EQU "" (
	echo Usage: svn_pset.bat FILE1 ...
	goto end
)

svn pset svn:keywords id %*
svn pset svn:eol-style native %*

:end
