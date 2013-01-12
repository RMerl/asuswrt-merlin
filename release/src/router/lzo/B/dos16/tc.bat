@echo // Copyright (C) 1996-2011 Markus F.X.J. Oberhumer
@echo //
@echo //   DOS 16-bit
@echo //   Turbo C/C++
@echo //
@call b\prepare.bat
@if "%BECHO%"=="n" echo off


set CC=tcc -ml -f-
set CF=-O -G -w -w-rch -w-sig %CFI% -Iinclude\lzo
set LF=%BLIB%

%CC% %CF% -Isrc -c src\*.c
@if errorlevel 1 goto error
tlib %BLIB% @b\dos16\bc.rsp
@if errorlevel 1 goto error

%CC% %CF% -f -Iexamples examples\dict.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\lzopack.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\precomp.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\precomp2.c %LF%
@if errorlevel 1 goto error
%CC% %CF% -Iexamples examples\simple.c %LF%
@if errorlevel 1 goto error

%CC% %CF% -f -Ilzotest lzotest\lzotest.c %LF%
@if errorlevel 1 goto error


@call b\done.bat
@goto end
:error
@echo ERROR during build!
:end
@call b\unset.bat
