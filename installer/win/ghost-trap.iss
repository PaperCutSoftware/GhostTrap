; Copyright (c) 2012-2024 PaperCut Software Pty Ltd
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
  #define gs_version "10.02.1"
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
ArchitecturesAllowed=x64
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

const
; Expected minimum version is 14.38.33130.00, for Visual C++ 2015 redistributable packs
; Update the values here if our requirements change over time
  MinMajor = 14;
  { only applies if actual major version is 14, do not apply if actual major version is higher }
  MinMinor = 38;
  { only applies if actual major is 14 and actual minor 38, do not apply if previous versions are higher }
  MinBld = 33130;

; Used to store value of whether we have acceptable Visual C++ Runtime components
var
  HasRequiredVCRuntimeVersion : Boolean;
  VCRuntimeMissingOptionsPage : TInputOptionWizardPage;
  IsInstallationFinishedNormally : Boolean;

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

function IsRequiredVCRuntimeVersionInstalled() : Boolean;
var
  cMajor: Cardinal;
  cMinor: Cardinal;
  cBld: Cardinal;
  cRbld: Cardinal;
  sKey: String;
begin
  Result := False;
  ; for more info see https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170#install-the-redistributable-packages
  sKey := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  if RegQueryDWordValue(HKEY_LOCAL_MACHINE, sKey, 'Major', cMajor) then begin
    if RegQueryDWordValue(HKEY_LOCAL_MACHINE, sKey, 'Minor', cMinor) then begin
      if RegQueryDWordValue(HKEY_LOCAL_MACHINE, sKey, 'Bld', cBld) then begin
        if RegQueryDWordValue(HKEY_LOCAL_MACHINE, sKey, 'RBld', cRbld) then begin
          if cMajor > MinMajor then begin
            Result := True;
            Exit;
          end;

          if cMajor < MinMajor then begin
            Exit;
          end;

          if cMinor > MinMinor then begin
            Result := True;
            Exit;
          end;

          if cMinor < MinMinor then begin
            Exit;
          end;

          if (cBld >= MinBld) and (cRbld >= 0) then begin
            Result := True;
          end;
        end;
      end;
    end;
  end;
end;

procedure InitializeWizard();
begin
  HasRequiredVCRuntimeVersion := IsRequiredVCRuntimeVersionInstalled();
; If the OS is missing required dependencies, the installation process will not go the full length and will be aborted early
  if not HasRequiredVCRuntimeVersion then begin
    IsInstallationFinishedNormally := False;
    VCRuntimeMissingOptionsPage := CreateInputOptionPage(wpWelcome,
      'Visual C++ Runtime Required',
      'You are seeing this page because required dependencies for GhostTrap are missing on your operating system.',
      'GhostTrap of version 1.4.10.02 and above requires reasonably up-to-date Visual C++ Runtimes to function properly. You must install required Microsoft Redistributables before installing GhostTrap.' + #13#10 + #13#10 + 'Either option will terminate the current installation process.' + #13#10 + 'Read more about this: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#visual-studio-2015-2017-2019-and-2022',
      False, False);

    VCRuntimeMissingOptionsPage.Add('Install the dependencies from Microsoft now. (Recommended)');
    VCRuntimeMissingOptionsPage.Add('I will find the required dependencies myself later.');
  end;
; If the OS has the required dependencies, completion of the installation is treated as normal
  else
  begin
    IsInstallationFinishedNormally := True;
  end;
end;

procedure CurStepChanged(CurStep : TSetupStep);
var
  ErrorCode : Integer;
begin
  if (curStep = ssPostInstall) and not ((WizardForm.Canceled) or (IsInstallationFinishedNormally)) then begin
    case VCRuntimeMissingOptionsPage.Values[0] of
    0: begin
      { install dependencies now, using Microsoft's perma-link }
      ShellExec('', 'https://aka.ms/vs/17/release/vc_redist.x64.exe', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
      Abort;
    end;
    1: begin
      { do it later themselves }
      MsgBox('Please install required VC++ dependencies first and try again.', mbInformation, MB_OK);
      Abort;
    end;
  end;
end;