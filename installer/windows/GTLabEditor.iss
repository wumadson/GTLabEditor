#define AppName "GT Lab Editor"
#define AppVersion "1.0.0"
#define AppPublisher "GT LAB"
#define AppExeName "GTLabEditor.exe"
#define SourceRoot "..\.."
#define StagingDir SourceRoot + "\dist\staging\windows\GTLabEditor"
#define OutputDir SourceRoot + "\dist\windows"

[Setup]
AppId={{A3A86BF7-C1FB-4D16-A7B3-AB672F8B52A1}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/wumadson/GTLabEditor
AppSupportURL=https://github.com/wumadson/GTLabEditor/issues
AppUpdatesURL=https://github.com/wumadson/GTLabEditor/releases
DefaultDirName={autopf}\GT LAB\GT Lab Editor
DefaultGroupName=GT Lab Editor
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=GTLabEditor-1.0.0-Windows-x64-Setup
SetupIconFile={#SourceRoot}\GTLabEditor.ico
UninstallDisplayIcon={app}\{#AppExeName}
LicenseFile={#SourceRoot}\LICENSE.GPL-2.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion=1.0.0.0
VersionInfoCompany=GT LAB
VersionInfoDescription=GT Lab Editor Setup
VersionInfoProductName=GT Lab Editor
VersionInfoProductVersion=1.0.0
VersionInfoCopyright=See bundled notices and license.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\GT Lab Editor"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\GT Lab Editor"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch GT Lab Editor"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}\imageformats"
Type: dirifempty; Name: "{app}\platforms"
Type: dirifempty; Name: "{app}\printsupport"
Type: dirifempty; Name: "{app}\styles"
Type: dirifempty; Name: "{app}"
