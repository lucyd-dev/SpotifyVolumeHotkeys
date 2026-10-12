; Inno Setup script for Spotify Volume Hotkeys.
;
; Build-time defines (release.yml passes these):
;   /DAppVersion=0.3.0
;   /DStageDir=<absolute path to the staged exe + dlls>
;   /DOutputDir=<absolute path for the built installer>

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\staging"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#define AppName "Spotify Volume Hotkeys"
#define AppPublisher "Lucyd"
#define AppURL "https://github.com/lucyd-dev/SpotifyVolumeHotkeys"
#define AppExeName "SpotifyVolumeHotkey.exe"
; AppId must stay in sync with UNINSTALL_KEY in src/core/Updater.cpp.
#define AppId "{{B7D2E9C4-3F1A-4E7B-9C2D-5A6F8E0B1C3D}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={localappdata}\Programs\SpotifyVolumeHotkeys
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=SpotifyVolumeHotkeys-Setup-{#AppVersion}
SetupIconFile=..\assets\svh.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // The app owns this HKCU Run entry; remove it so uninstalling does not
    // leave a dangling autostart pointing at a deleted executable.
    RegDeleteValue(HKEY_CURRENT_USER,
      'Software\Microsoft\Windows\CurrentVersion\Run', 'SpotifyVolumeHotkeys');
  end;
end;
