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

[Run]
Filename: "{app}\SelectionForward.exe"; Description: "启动 Selection Forward"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExistingCommand: String;
  SetupCommand: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  SetupCommand := AddQuotes(ExpandConstant('{app}\SelectionForward.exe')) + ' --silent';
  if RegQueryStringValue(
       HKCU,
       'Software\Microsoft\Windows\CurrentVersion\Run',
       'SelectionForward',
       ExistingCommand) and
     (CompareText(ExistingCommand, SetupCommand) = 0) then
  begin
    RegDeleteValue(
      HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Run',
      'SelectionForward');
  end;
end;
