#define AppName "GT Lab Editor Free"
#define AppVersion "1.2.0"
#define AppPublisher "GT LAB"
#define AppExeName "GTLabEditorFree.exe"
#define SourceRoot "..\.."
#ifndef StagingDir
#define StagingDir SourceRoot + "\dist\staging\windows\GTLabEditorFree"
#endif
#ifndef OutputDir
#define OutputDir SourceRoot + "\dist\windows"
#endif

[Setup]
AppId={{235BA6CC-C72D-4EB4-B476-AF62F10BD03D}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/wumadson/GTLabEditor
AppSupportURL=https://github.com/wumadson/GTLabEditor/issues
AppUpdatesURL=https://github.com/wumadson/GTLabEditor/releases
DefaultDirName={autopf}\GT LAB\GT Lab Editor Free
DefaultGroupName=GT Lab Editor Free
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=GTLabEditor-Free-1.2.0-Windows-x64-Setup
SetupIconFile={#SourceRoot}\GTLabEditor.ico
UninstallDisplayIcon={app}\{#AppExeName}
LicenseFile={#SourceRoot}\LICENSE.GPL-2.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion=1.2.0.0
VersionInfoCompany=GT LAB
VersionInfoDescription=GT Lab Editor Free Setup
VersionInfoProductName=GT Lab Editor Free
VersionInfoProductVersion=1.2.0
VersionInfoCopyright=See bundled notices and license.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\GT Lab Editor Free"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\GT Lab Editor Free"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch GT Lab Editor Free"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}\imageformats"
Type: dirifempty; Name: "{app}\platforms"
Type: dirifempty; Name: "{app}\printsupport"
Type: dirifempty; Name: "{app}\styles"
Type: dirifempty; Name: "{app}"

