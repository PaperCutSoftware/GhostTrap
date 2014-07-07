; Copyright (c) 2012-2014 PaperCut Software International Pty. Ltd.
; Author: Chris Dance <chris.dance@papercut.com>
; 
; License: GNU GPL v3 - See project LICENSE file.
;                                                        z
; $Id: $
;
; Ghost Trap Inno Setup based installer script.
;

#define app_name "Ghost Trap"
#define app_name_no_space "GhostTrap"

#ifndef app_version
  #define app_version "0.1"
#endif

#define gs_name "GPL Ghostscript"

#ifndef gs_version
  #define gs_version "9.06"
#endif

#define gs_c_exe "gswin32c.exe"
#define gs_dll "gsdll32.dll"


[Setup]
AppName={#app_name}
AppVerName="{#app_name} {#app_version}.{#gs_version}"
AppPublisher="PaperCut Software Int. Pty. Ltd."
AppPublisherURL=https://github.com/PaperCutSoftware/GhostTrap
AppSupportURL=https://github.com/PaperCutSoftware/GhostTrap/issues
AppUpdatesURL=https://github.com/PaperCutSoftware/GhostTrap
DefaultDirName={pf}\{#app_name_no_space}

VersionInfoVersion={#app_version}.{#gs_version}

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
;WizardSmallImageFile=setup-logo.bmp

[Messages]
BeveledLabel={#app_name} {#app_version}


[Files]
Source: *; DestDir: {app}; Flags: ignoreversion recursesubdirs createallsubdirs


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


  
  
  

