@echo off
set GHOST_TRAP_VERSION=1.5
set INNO_COMPILER=%programfiles(x86)%\Inno Setup 6\ISCC.exe
SETLOCAL ENABLEDELAYEDEXPANSION
set starttime=%time%
set startdir=%cd%
set gsversion=10.04.0

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
echo Error: Unable to locate the Ghost Trap project source.
echo Please ensure the Ghost Trap source is located at:
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
>nul find "gsc-trapped" %~dp0third-party\chromium\src\sandbox\win\BUILD.gn && (
  goto buildinfoexists
) || (
  goto buildinfomissing
)

:buildinfomissing
echo.>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
echo executable("gsc-trapped") {>>%~dp0third-party\chromium\src\sandbox\win\BUILD.gn
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

REM Copy the Ghost Trap source code to the Chromium project
call mkdir %~dp0third-party\chromium\src\sandbox\win\ghosttrap > NUL
call xcopy %~dp0src %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ /Y /E /S > NUL

REM Copy the Ghostscript source code to the Chromium project
call mkdir %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ghostscript > NUL
call xcopy %~dp0third-party\ghostpdl %~dp0third-party\chromium\src\sandbox\win\ghosttrap\ghostscript\ /Y /E /S > NUL

REM #
REM # REM Start the build
REM #

call cd "%~dp0third-party\chromium\src\" > NUL
if %errorlevel% NEQ 0 goto builderror
echo Checked out into Chromium's repository.

call gn gen out\Default --args="is_debug=false" > NUL
if %errorlevel% NEQ 0 goto builderror
echo GN generation successful.

call autoninja -C out\Default sandbox/win:gsc-trapped > NUL
if %errorlevel% NEQ 0 goto builderror
echo autoninja build successful.

REM #
REM # Test Ghost Trap
REM #
echo Testing Ghost Trap...
echo Copying gsdll64.dll to output directory...
copy "%~dp0third-party\ghostpdl\bin\gsdll64.dll" "%~dp0third-party\chromium\src\out\Default\" /Y > NUL
if %errorlevel% NEQ 0 goto builderror
echo gsdll64.dll copied successfully

REM #
REM # !!! Do not add multi-lined conditional blocks after calling gsc-trapped. Keep the "if" line strictly single-lined.
REM # CMD gets tripped up for no particular good reason and complains about unexpected symbol "."
REM #
call "%~dp0third-party\chromium\src\out\Default\gsc-trapped.exe" --test-sandbox -sOutputFile="C:\output\outputtest.txt" "C:\input\inputtest.txt"
if %errorlevel% NEQ 0 goto builderror
echo gsc-trapped executed for testing successfully.

REM #
REM # Create target dir mirroring Ghostscript standard install.
REM #
rmdir /s /q "%~dp0target" > NUL

REM # Small sleep so we don't hit locked files.
ping -n 3 127.0.0.1 > NUL

echo Copying files...

mkdir "%~dp0target" > NUL
mkdir "%~dp0target\installfiles" > NUL
mkdir "%~dp0target\installfiles\bin" > NUL

REM # Ghost Trap exe, README and LICENSE files
copy "%~dp0third-party\chromium\src\out\Default\gsc-trapped.exe" "%~dp0target\installfiles\bin\gsc-trapped.exe" /Y > NUL
copy "%~dp0LICENSE*" "%~dp0target\installfiles" /Y > NUL
copy "%~dp0README*" "%~dp0target\installfiles" /Y > NUL

REM # Ghostscript files (mirroring standard install structure)
echo copying files from GhostPDL project into GhostTrap paths for bundling installer etc...
copy "%~dp0third-party\ghostpdl\bin\gswin64.exe" "%~dp0target\installfiles\bin\gs.exe" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gswin64c.exe" "%~dp0target\installfiles\bin\gsc.exe" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gsdll64.dll" "%~dp0target\installfiles\bin\" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gpcl6win64.exe" "%~dp0target\installfiles\bin\pcl6.exe" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gpcl6dll64.dll" "%~dp0target\installfiles\bin\" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gxpswin64.exe" "%~dp0target\installfiles\bin\gxps.exe" /Y > NUL
copy "%~dp0third-party\ghostpdl\bin\gxpsdll64.dll" "%~dp0target\installfiles\bin\" /Y > NUL
xcopy "%~dp0third-party\ghostpdl\doc" "%~dp0target\installfiles\doc\" /Y /S > NUL
xcopy "%~dp0third-party\ghostpdl\examples" "%~dp0target\installfiles\examples\" /Y /S > NUL
xcopy "%~dp0third-party\ghostpdl\iccprofiles" "%~dp0target\installfiles\iccprofiles\" /Y /S > NUL
xcopy "%~dp0third-party\ghostpdl\lib" "%~dp0target\installfiles\lib\" /Y /S > NUL
xcopy "%~dp0third-party\ghostpdl\Resource" "%~dp0target\installfiles\Resource\" /Y /S > NUL

REM #
REM # Run Inno install script to build the installer
REM #

echo Building installer...
"%INNO_COMPILER%" "/dapp_version=%GHOST_TRAP_VERSION%" "/dgs_version=%GS_VERSION_MAJOR%.%GS_VERSION_MINOR%" "%~dp0installer\win\ghost-trap.iss" /q
if %errorlevel% NEQ 0 (
    echo Error building GhostTrap installer with ghost-trap.iss !!
    goto builderror
)
echo GhostTrap installer generated successfully! All done!


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
