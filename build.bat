@echo off
set GHOST_TRAP_VERSION=1.3
set INNO_COMPILER=%programfiles(x86)%\Inno Setup 6\ISCC.exe
SETLOCAL ENABLEDELAYEDEXPANSION
set starttime=%time%
set startdir=%cd%
set gsversion=9.27

echo  .-.      ___ _               _  _____                 
echo (o o)    / _ \ ^|__   ___  ___^| ^|/__   \_ __ __ _ _ __  
echo ^| O \   / /_\/ '_ \ / _ \/ __^| __^|/ /\/ '__/ _` ^| '_ \ 
echo  \   \ / /_\\^| ^| ^| ^| (_) \__ \ ^|_/ /  ^| ^| ^| (_^| ^| ^|_) ^|
echo   `~~~'\____/^|_^| ^|_^|\___/^|___/\__\/   ^|_^|  \__,_^| .__/ 
echo                                                 ^|_^|    
echo.

rem Make the current dir the script dir
cd %~dp0

if %errorlevel% NEQ 0 goto builderror

REM #
REM # Verify that our required dependencies exists
REM #

if exist "%~dp0third-party\ghostpdl\bin\gsdll64.dll" goto gsinsok
echo Error: Unable to locate the GhostPDL files
echo Please ensure its at:
echo     %~dp0third-party\ghostpdl\bin\gsdll64.dll
goto builderror
:gsinsok

if exist %~dp0third-party\chromium\src\sandbox goto chromesrcok
echo Error: Unable to locate the chromium source.
echo Please ensure the chromium source is located at:
echo     %~dp0third-party\chromium\src
goto builderror
:chromesrcok

if exist %~dp0src goto ghosttrapsrcok
echo Error: Unable to locate the GhostTrap project source.
echo Please ensure the GhostTrap source is located at:
echo     %~dp0src
goto builderror
:ghosttrapsrcok

if exist %~dp0third-party\ghostpdl\psi\ goto ghostscriptsrcok
echo Error: Unable to locate the GhostPDL project source.
echo Please ensure the GhostPDL source is located at:
echo     %~dp0third-party\ghostpdl
goto builderror
:ghostscriptsrcok

REM #
REM # Get Ghostscript version info
REM #

for /f "usebackq delims=" %%x in (`findstr /B /C:GS_VERSION_M %~dp0third-party\ghostpdl\base\version.mak`) do (set "%%x") 

echo Ghostscript version is %GS_VERSION_MAJOR%.%GS_VERSION_MINOR%
echo.

REM #
REM # Build Ghost Trap
REM #

echo ====  Compiling 64-bit  ====

REM Append extra build info to the BUILD.gn
>nul find "gswin64c" %~dp0third-party\chromium\src\sandbox\win\BUILD.gn && (
  goto buildinfoexists
) || (
  goto buildinfomissing
)

:buildinfomissing
echo.>>%~dp0chromium\src\sandbox\win\BUILD.gn
echo executable("gswin64c-trapped") {>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   sources = [>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo      "ghosttrap/gstrapped.cpp",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo      "ghosttrap/sandbox_procmgmt.cpp",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo      "ghosttrap/sandbox_procmgmt.h",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   ]>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo.>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   include_dirs = [>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo      "ghosttrap/ghostscript/psi",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo      "ghosttrap/ghostscript/base",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   ]>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo.>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   deps = [>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo     ":sandbox",>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo   ]>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo }>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
:buildinfoexists

REM Copy the ghosttrap source code to the Chromium project
call mkdir %~dp0third-party\chromium\src\sandbox\win\ghosttrap > NUL
call xcopy %~dp0src %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ /Y /E /s > NUL

REM Copy the Ghostscript source code to the Chromium project
call mkdir %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ghostscript > NUL
call xcopy %~dp0third-party\ghostpdl %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ghostscript\ /Y /E /s > NUL

REM #
REM # REM Start the build
REM #

call cd third-party\chromium\src\ > NUL
if %errorlevel% NEQ 0 goto builderror

call gn gen out\Default --args="is_debug=false" > NUL
if %errorlevel% NEQ 0 goto builderror

call autoninja -C out\Default sandbox/win:gswin64c-trapped > NUL
if %errorlevel% NEQ 0 goto builderror

REM #
REM # Test GhostTrap
REM #
copy "..\..\ghostpdl\bin\gsdll64.dll" out\Default\ /Y > NUL
call cd out\Default\ > NUL
echo Testing GhostTrap...
call gswin64c-trapped.exe --test-sandbox -sOutputFile="C:\output\outputtest.txt" "C:\input\inputtest.txt"
if %errorlevel% NEQ 0 goto builderror

call cd %startdir% > NUL
if %errorlevel% NEQ 0 goto builderror

REM #
REM # Create target dir mirroring Ghostscript standard install.
REM #
rmdir /s /q "target" > NUL

REM # Small sleep so we don't hit locked files.
ping -n 3 127.0.0.1 > NUL

echo Copying files...

mkdir target > NUL
mkdir target\installfiles > NUL
mkdir target\installfiles\bin > NUL
mkdir target\installfiles\doc > NUL
mkdir target\installfiles\doc\images > NUL
mkdir target\installfiles\doc\pclxps > NUL
mkdir target\installfiles\examples > NUL
mkdir target\installfiles\examples\cjk > NUL
mkdir target\installfiles\iccprofiles > NUL
mkdir target\installfiles\lib > NUL
mkdir target\installfiles\Resource > NUL
mkdir target\installfiles\Resource\CIDFont > NUL
mkdir target\installfiles\Resource\CIDFSubst > NUL
mkdir target\installfiles\Resource\CMap > NUL
mkdir target\installfiles\Resource\ColorSpace > NUL
mkdir target\installfiles\Resource\Decoding > NUL
mkdir target\installfiles\Resource\Encoding > NUL
mkdir target\installfiles\Resource\Font > NUL
mkdir target\installfiles\Resource\IdiomSet > NUL
mkdir target\installfiles\Resource\Init > NUL
mkdir target\installfiles\Resource\SubstCID > NUL

REM # Ghost Trap exe, README and LICENSE files
copy "third-party\chromium\src\out\Default\gswin64c-trapped.exe" target\installfiles\bin\gsc-trapped.exe /Y > NUL
copy "third-party\chromium\src\out\Default\gswin64c-trapped.exe" target\installfiles\bin\gswin32c-trapped.exe /Y > NUL
copy LICENSE* target\installfiles /Y > NUL
copy README* target\installfiles /Y > NUL

REM # Ghostscript files (mirroring standard install structure)
copy "third-party\ghostpdl\bin\gswin64.exe" target\installfiles\bin /Y > NUL
copy "third-party\ghostpdl\bin\gswin64.exe" target\installfiles\bin\gswin32.exe /Y > NUL
copy "third-party\ghostpdl\bin\gswin64c.exe" target\installfiles\bin /Y > NUL
copy "third-party\ghostpdl\bin\gswin64c.exe" target\installfiles\bin\gswin32c.exe /Y > NUL
copy "third-party\ghostpdl\bin\gsdll64.dll" target\installfiles\bin /Y > NUL
copy "third-party\ghostpdl\bin\gpcl6win64.exe" target\installfiles\bin\pcl6.exe /Y > NUL
copy "third-party\ghostpdl\bin\gpcl6dll64.dll" target\installfiles\bin /Y > NUL
copy "third-party\ghostpdl\bin\gxpswin64.exe" target\installfiles\bin\gxps.exe /Y > NUL
copy "third-party\ghostpdl\bin\gxpsdll64.dll" target\installfiles\bin /Y > NUL
copy "third-party\ghostpdl\doc\*.*" target\installfiles\doc /Y > NUL
copy "third-party\ghostpdl\doc\images\*.*" target\installfiles\doc\images /Y > NUL
copy "third-party\ghostpdl\doc\pclxps\*.*" target\installfiles\doc\pclxps /Y > NUL
copy "third-party\ghostpdl\examples\*.*" target\installfiles\examples /Y > NUL
copy "third-party\ghostpdl\examples\cjk\*.*" target\installfiles\examples\cjk /Y > NUL
copy "third-party\ghostpdl\iccprofiles\*.*" target\installfiles\iccprofiles /Y > NUL
copy "third-party\ghostpdl\lib\*.*" target\installfiles\lib /Y > NUL
copy "third-party\ghostpdl\Resource\CIDFont\*.*" target\installfiles\Resource\CIDFont /Y > NUL
copy "third-party\ghostpdl\Resource\CIDFSubst\*.*" target\installfiles\Resource\CIDFSubst /Y > NUL
copy "third-party\ghostpdl\Resource\CMap\*.*" target\installfiles\Resource\CMap /Y > NUL
copy "third-party\ghostpdl\Resource\ColorSpace\*.*" target\installfiles\Resource\ColorSpace /Y > NUL
copy "third-party\ghostpdl\Resource\Decoding\*.*" target\installfiles\Resource\Decoding /Y > NUL
copy "third-party\ghostpdl\Resource\Encoding\*.*" target\installfiles\Resource\Encoding /Y > NUL
copy "third-party\ghostpdl\Resource\Font\*.*" target\installfiles\Resource\Font /Y > NUL
copy "third-party\ghostpdl\Resource\IdiomSet\*.*" target\installfiles\Resource\IdiomSet /Y > NUL
copy "third-party\ghostpdl\Resource\Init\*.*" target\installfiles\Resource\Init /Y > NUL
copy "third-party\ghostpdl\Resource\SubstCID\*.*" target\installfiles\Resource\SubstCID /Y > NUL

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
