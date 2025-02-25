; Copyright (c) 2012-2025 PaperCut Software Pty Ltd
; Author: Chris Dance <chris.dance@papercut.com>
;
; License: GNU Affero GPL v3 - See project LICENSE file.
;
; Ghost Trap Inno Setup based installer script.
;

[Setup]
#define app_name "Ghost Trap"
#define app_name_no_space "GhostTrap"

#ifndef app_version
  #define app_version "0.1"
#endif

#define gs_name "GPL Ghostscript"

#ifndef gs_version
  #define gs_version "10.03.1"
#endif

#define gs_c_exe "gsc.exe"
#define gs_dll "gsdll64.dll"

AppName={#app_name}
AppVerName="{#app_name} {#app_version}.{#gs_version}"
AppPublisher="PaperCut Software Int. Pty. Ltd."
AppPublisherURL=https://github.com/PaperCutSoftware/GhostTrap
AppSupportURL=https://github.com/PaperCutSoftware/GhostTrap/issues
AppUpdatesURL=https://github.com/PaperCutSoftware/GhostTrap
DefaultDirName={commonpf}\{#app_name_no_space}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64

LicenseFile=..\..\LICENSE.rtf
InfoBeforeFile=..\..\installer\win\install-info.rtf

SourceDir=..\..\target\installfiles
OutputDir=..\..\target
OutputBaseFilename=ghost-trap-installer-{#app_version}.{#gs_version}

SolidCompression=yes
Compression=lzma

ShowLanguageDialog=auto

DisableProgramGroupPage=yes
PrivilegesRequired=admin


WizardImageFile=..\..\installer\win\installer-logo-large.bmp

[Messages]
BeveledLabel={#app_name} {#app_version}


[Files]
Source: *; DestDir: {app}; Flags: ignoreversion recursesubdirs createallsubdirs
; Copy (64-bit) binaries to legacy 32-bit filenames for backwards compatibility (of path references, not of 32-bit OS)
Source: bin\gsc-trapped.exe; DestDir: {app}\bin\; DestName: gswin32c-trapped.exe;
Source: bin\gs.exe; DestDir: {app}\bin\; DestName: gswin32.exe;
Source: bin\gsc.exe; DestDir: {app}\bin\; DestName: gswin32c.exe;
{ This is the Microsoft's redistributable binaries for VC++ dependency, we should provide the exact same build as it is
 is used by GhostScript. As of 25/3/25, the build number is 30153, with a full version number of 14.29.30153. Manually
 update this file whenever necessary. }
Source: ..\..\installer\redist\vc_redist.x64.exe; DestDir: {app}\redist; DestName: vc_redist.x64.exe; AfterInstall: InstallVCRedist


[Registry]
; Add some keys that might help us in the future
Root: HKLM; Subkey: "Software\{#app_name_no_space}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#app_name_no_space}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\{#app_name_no_space}"; ValueType: string; ValueName: "Version"; ValueData: "{#= app_version}.{#gs_version}"; Flags: uninsdeletevalue

; Mirror Ghostscript default keys
Root: HKLM; Subkey: "Software\{#gs_name}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#gs_name}\{#gs_version}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#gs_name}\{#gs_version}"; ValueType: string; ValueName: "GS_LIB"; ValueData: "{app}\bin;{app}\lib;{app}\fonts"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\{#gs_name}\{#gs_version}"; ValueType: string; ValueName: "GS_DLL"; ValueData: "{app}\bin\{#gs_dll}"; Flags: uninsdeletevalue


[Run]
; Generate Windows font map (mirror Ghostscript installer)
Filename: {app}\bin\{#gs_c_exe}; Parameters: "-q -dBATCH ""-sFONTDIR={code:FontsDirWithForwardSlashes}"" ""-sCIDFMAP={app}\lib\cidfmap"" ""{app}\lib\mkcidfm.ps"""; Description: "Generating font map for Windows TrueType fonts..."; Flags: runhidden;

[UninstallDelete]
Type: filesandordirs; Name: {app}\lib\cidfmap;

[Code]
function FontsDirWithForwardSlashes(Param: String): String;
begin
  { The system's Fonts directory in forward slash format }
  Result := ExpandConstant('{fonts}');
  StringChangeEx(Result, '\', '/', True);
end;

function GetUninstallString: string;
var
  sUnInstPath: string;
  sUnInstallString: String;
begin
  Result := '';
  sUnInstPath := ExpandConstant('Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Ghost Trap_is1');
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  iResultCode: Integer;
  sUnInstallString: string;
begin
  if RegValueExists(HKEY_LOCAL_MACHINE,'Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Ghost Trap_is1', 'UninstallString') then  { Your App GUID/ID }
  begin
    sUnInstallString := GetUninstallString();
    sUnInstallString :=  RemoveQuotes(sUnInstallString);
    Exec(ExpandConstant(sUnInstallString), '/VERYSILENT', '', SW_SHOW, ewWaitUntilTerminated, iResultCode);
  end;
end;

{ The same version of VC++ dependency that is used by GhostScript will be automatically installed by GhostTrap }
{ Requires manual updating if the version used by GhostScript changes in order to mirror the dependency updates }
procedure InstallVCRedist();
var
  ErrorCode: Integer;
  VC_Redist: String;
begin
  VC_Redist := ExpandConstant('{app}\redist\vc_redist.x64.exe');
  Log(Format('The VC++ dependency to install will be: %s', [VC_Redist]));
  if Exec(VC_Redist, '/norestart /install /quiet', ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ErrorCode) then
    Log('VC dependencies successfully installed.')
  else
    Log(Format('Failed to launch VC++ Redistributable installer. Error Code: %d', [ErrorCode]));

  if ErrorCode <> 0 then
  begin
    Log(Format('VC++ Redistributable installation failed with exit code: %d', [ErrorCode]));
  end;
end;