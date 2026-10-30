; The Windows installer (Inno Setup 6): TunedChaos-<version>-setup-x64.exe.
; Built in CI from the same folder as the portable ZIP:
;   iscc /DAppVersion=1.0 /DSourceDir=<full path of dist\TunedChaos> /DOutputDir=<full path> release\windows\TunedChaos.iss
; (Relative paths are read from this file's folder.)
; Installs for the current user (no administrator prompt) into
; %LOCALAPPDATA%\Programs\TunedChaos, with a Start menu entry carrying the
; app's Windows id (which the song-change toasts and media controls need).
; Uninstalling keeps your settings and library (%LOCALAPPDATA%\TunedChaos).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\TunedChaos"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\installer"
#endif

[Setup]
; (The id Windows knows the installed app by: never change it)
AppId={{5DCAD0ED-7FCC-42EE-95A0-8C212AF127B2}
AppName=Tuned Chaos
AppVersion={#AppVersion}
AppVerName=Tuned Chaos {#AppVersion}
AppPublisher=Chandan Das
AppPublisherURL=https://github.com/ChandanDasGIT/TunedChaos
AppSupportURL=https://github.com/ChandanDasGIT/TunedChaos/issues
DefaultDirName={autopf}\TunedChaos
DefaultGroupName=Tuned Chaos
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
LicenseFile={#SourceDir}\LICENSE
SetupIconFile=tunedchaos.ico
UninstallDisplayIcon={app}\TunedChaos.exe
UninstallDisplayName=Tuned Chaos
OutputDir={#OutputDir}
OutputBaseFilename=TunedChaos-{#AppVersion}-setup-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; A running copy is closed first (its files are in use), and started again after an update
CloseApplications=yes
RestartApplications=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Tuned Chaos"; Filename: "{app}\TunedChaos.exe"; AppUserModelID: "ChandanDas.TunedChaos"
Name: "{autodesktop}\Tuned Chaos"; Filename: "{app}\TunedChaos.exe"; AppUserModelID: "ChandanDas.TunedChaos"; Tasks: desktopicon

[Run]
Filename: "{app}\TunedChaos.exe"; Description: "{cm:LaunchProgram,Tuned Chaos}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The folder itself, once empty (yt-dlp's updates and the user's data live elsewhere)
Type: dirifempty; Name: "{app}"
