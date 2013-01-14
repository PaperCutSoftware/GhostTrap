@echo off

set GHOST_TRAP_VERSION=1.0

set INNO_COMPILER=%programfiles(x86)%\Inno Setup 5\ISCC.exe
SETLOCAL ENABLEDELAYEDEXPANSION
set starttime=%time%
set startdir=%cd%

rem Make the current dir the script dir
cd %~dp0


REM #
REM # Verify that our 3rd party dependencies exist and are built.
REM #

if exist third-party\chromium\src\sandbox goto chromesrcok
echo Error: Unable to locate the chromium source.
echo Please ensure the chromium source is located at:
echo     %~dp0third-party\chromium\src
goto builderror
:chromesrcok

if exist third-party\chromium\src\build\Release\lib\sandbox*.lib goto chromelibok
echo Error: Unable to locate the chromium sandbox.lib files.
echo Please ensure the chromium sandbox project has been built.
echo The sandbox.lib file should exist at:
echo     %~dp0third-party\chromium\build\Release\lib\sandbox.lib
goto builderror
:chromelibok

if exist third-party\ghostpdl\win32 goto ghostpdlsrcok
echo Error: Unable to locate the GhostPDL project source.
echo Please ensure the GhostPDL source is located at:
echo     %~dp0third-party\ghostpdl
goto builderror
:ghostpdlsrcok

if exist third-party\ghostpdl\gs\bin\gswin32c.exe goto ghostpdllibok
echo Error: Unable to locate Ghostscript binaries.
echo Please ensure that the GhostPDL solution has been built.
echo The gswin32c.exe file should exist at:
echo     %~dp0third-party\ghostpdl\gs\bin\gswin32c.exe
goto builderror
:ghostpdllibok

REM #
REM # Get Ghostscript version info
REM #

for /f "usebackq delims=" %%x in (`findstr /B /C:GS_VERSION_M third-party\ghostpdl\gs\base\version.mak`) do (set "%%x") 

echo Ghostscript version is %GS_VERSION_MAJOR%.%GS_VERSION_MINOR%

REM #
REM # Build Ghost Trap
REM #

@if "%VSINSTALLDIR%"=="" call "%VS100COMNTOOLS%\vsvars32.bat"
echo.

REM Build 32 bit
echo ====  Compiling 32-bit  ====
devenv src\ghost-trap.sln /rebuild "release|Win32"
if %errorlevel% NEQ 0 goto builderror

REM #
REM # Create target dir mirroring Ghostscript standard install.
REM #

rmdir /s /q "target" > NUL

echo Copying files...

mkdir target > NUL
mkdir target\installfiles > NUL
mkdir target\installfiles\bin > NUL
mkdir target\installfiles\doc > NUL
mkdir target\installfiles\examples > NUL
mkdir target\installfiles\lib > NUL
mkdir target\installfiles\zlib > NUL
mkdir target\installfiles\zlib\doc > NUL

REM # Ghost Trap exe, README and LICENSE files
copy src\Release\gswin32c-trapped.exe target\installfiles\bin /Y > NUL

REM # Chrome sandbox wow helper
copy third-party\chromium\src\build\Release\wow_helper.exe target\installfiles\bin /Y > NUL
copy LICENSE* target\installfiles /Y > NUL
copy README* target\installfiles /Y > NUL

REM # Ghostscript files (mirroring standard install structure)
copy third-party\ghostpdl\gs\bin\gswin32*.exe target\installfiles\bin /Y > NUL
copy third-party\ghostpdl\gs\bin\gsdll32*.dll target\installfiles\bin /Y > NUL

copy third-party\ghostpdl\gs\doc\*.* target\installfiles\doc /Y > NUL

copy third-party\ghostpdl\gs\examples\*.* target\installfiles\examples /Y > NUL

copy third-party\ghostpdl\gs\lib\*.* target\installfiles\lib /Y > NUL

copy third-party\ghostpdl\gs\zlib\doc\*.* target\installfiles\zlib\doc /Y > NUL

REM # Also include pcl6.exe (PCL support) and gxps.exe (XPS support) for convenience.
REM # Note: These do not (yet) have trapped varients.

copy third-party\ghostpdl\main\obj\pcl6.exe target\installfiles\bin /Y > NUL
copy third-party\ghostpdl\xps\obj\gxps.exe target\installfiles\bin /Y > NUL

REM #
REM # Run Inno install script to build the installer
REM #

echo Building installer...

"%INNO_COMPILER%" "/dapp_version=%GHOST_TRAP_VERSION%" "/dgs_version=%GS_VERSION_MAJOR%.%GS_VERSION_MINOR%" installer\win\ghost-trap.iss /q
if %errorlevel% NEQ 0 goto builderror


REM We've finished successfully
goto end

:builderror

echo ****************************************************
echo ************* Error running build ******************
echo ****************************************************
cd %startdir%
exit /B 1

:end
rem Change back to the original dir
cd %startdir%

echo Build complete.
echo started at: %starttime%
echo now it is : %time%
