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
AppName=SelectBridge
AppVersion={#AppVersion}
AppPublisher=SelectBridge
DefaultDirName={localappdata}\Programs\SelectBridge
DefaultGroupName=SelectBridge
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=SelectBridge-{#AppVersion}-windows-x64-setup
SetupIconFile={#ProjectRoot}\resources\icon.ico
UninstallDisplayIcon={app}\SelectBridge.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\SelectBridge.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\selection-hook.node"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\select_bridge_win32_ui.node"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SelectBridge"; Filename: "{app}\SelectBridge.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\SelectBridge.exe"; Description: "启动 SelectBridge"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExistingCommand: String;
  SetupCommand: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  SetupCommand := AddQuotes(ExpandConstant('{app}\SelectBridge.exe')) + ' --silent';
  if RegQueryStringValue(
       HKCU,
       'Software\Microsoft\Windows\CurrentVersion\Run',
       'SelectBridge',
       ExistingCommand) and
     (CompareText(ExistingCommand, SetupCommand) = 0) then
  begin
    RegDeleteValue(
      HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Run',
      'SelectBridge');
  end;
end;
