@echo // Copyright (C) 1996-2011 Markus F.X.J. Oberhumer
@echo //
@echo //   DOS 16-bit
@echo //   Microsoft C/C++
@echo //
@call b\prepare.bat
@if "%BECHO%"=="n" echo off


set CC=cl -nologo -AL
set CF=-O -Gf -W3 %CFI%
set LF=/map

@REM %CC% %CF% -c src\*.c
for %%f in (src\*.c) do %CC% %CF% -c %%f
@if errorlevel 1 goto error
lib /nologo %BLIB% @b\dos16\bc.rsp;
@if errorlevel 1 goto error

%CC% %CF% -c examples\dict.c
@if errorlevel 1 goto error
link %LF% dict.obj,,,%BLIB%;
@if errorlevel 1 goto error
%CC% %CF% -c examples\lzopack.c
@if errorlevel 1 goto error
link %LF% lzopack.obj,,,%BLIB%;
@if errorlevel 1 goto error
%CC% %CF% -c examples\precomp.c
@if errorlevel 1 goto error
link %LF% precomp.obj,,,%BLIB%;
@if errorlevel 1 goto error
%CC% %CF% -c examples\precomp2.c
@if errorlevel 1 goto error
link %LF% precomp2.obj,,,%BLIB%;
@if errorlevel 1 goto error
%CC% %CF% -c examples\simple.c
@if errorlevel 1 goto error
link %LF% simple.obj,,,%BLIB%;
@if errorlevel 1 goto error

%CC% %CF% -c lzotest\lzotest.c
@if errorlevel 1 goto error
link %LF% lzotest.obj,,,%BLIB%;
@if errorlevel 1 goto error


@call b\done.bat
@goto end
:error
@echo ERROR during build!
:end
@call b\unset.bat
