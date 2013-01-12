@echo // Copyright (C) 1996-2011 Markus F.X.J. Oberhumer
@echo //
@echo //   Windows 16-bit
@echo //   Digital Mars C/C++ (using WINIO)
@echo //
@call b\prepare.bat
@if "%BECHO%"=="n" echo off


set CC=dmc -ml -2 -W
set CF=-o -w- %CFI%
set LF=%BLIB% libw.lib commdlg.lib lwindos.lib /L/map/stack:8096

%CC% %CF% -c @b\src.rsp
@if errorlevel 1 goto error
lib %BLIB% /b /c /n /noi @b\dos16\bc.rsp
@if errorlevel 1 goto error

%CC% %CF% examples\dict.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\lzopack.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\precomp.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\precomp2.c %LF%
@if errorlevel 1 goto error
%CC% %CF% examples\simple.c %LF%
@if errorlevel 1 goto error

%CC% %CF% lzotest\lzotest.c %LF%
@if errorlevel 1 goto error

set LF=libw.lib commdlg.lib lwindos.lib /L/map/stack:8096
%CC% %CF% -Iinclude\lzo minilzo\testmini.c minilzo\minilzo.c %LF%
@if errorlevel 1 goto error


@call b\done.bat
@goto end
:error
@echo ERROR during build!
:end
@call b\unset.bat
