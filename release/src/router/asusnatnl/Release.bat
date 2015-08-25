if exist ".\release" (
	REM the release folder exists
)else (
	REM There is no release folder.
	mkdir .\release
)

REM Build multiple platform first, then Build Natnl-SDK for Java-Applet
build-multiple-release.bat