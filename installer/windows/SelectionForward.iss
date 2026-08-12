#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef ProjectRoot
  #define ProjectRoot "."
#endif
#ifndef SourceDir
  #define SourceDir "."
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{8A6DCE91-54BC-4E4A-A09D-6ADFD7F41430}
AppName=Selection Forward
AppVersion={#AppVersion}
AppPublisher=Selection Forward
DefaultDirName={localappdata}\Programs\Selection Forward
DefaultGroupName=Selection Forward
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=SelectionForward-{#AppVersion}-windows-x64-setup
SetupIconFile={#ProjectRoot}\resources\icon.ico
UninstallDisplayIcon={app}\SelectionForward.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\SelectionForward.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\selection-hook.node"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\selection_forward_win32_ui.node"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Selection Forward"; Filename: "{app}\SelectionForward.exe"; WorkingDir: "{app}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "SelectionForward"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\SelectionForward.exe"; Description: "启动 Selection Forward"; Flags: nowait postinstall skipifsilent
