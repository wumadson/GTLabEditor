#define AppName "GT Lab Editor"
#define AppVersion "1.0.0"
#define AppPublisher "GT LAB"
#define AppExeName "GTLabEditor.exe"
#define SourceRoot "..\.."
#define StagingDir SourceRoot + "\dist\staging\windows\GTLabEditor"
#define OutputDir SourceRoot + "\dist\windows"
#define WinUsbPayload "winusb-lab"

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
InfoBeforeFile=LICENSE-PTBR.txt
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
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Messages]
WizardInfoBefore=Licença do GT Lab Editor
InfoBeforeLabel=Informações sobre a licença do GT Lab Editor.
InfoBeforeClickLabel=Quando estiver pronto para continuar, clique em Avançar.

[CustomMessages]
brazilianportuguese.WinUsbPageTitle=Suporte à BOSS GT-10 no Windows 11
brazilianportuguese.WinUsbPageDescription=Leia as informações antes de escolher se deseja configurar o suporte WinUSB.
brazilianportuguese.WinUsbHeading=O GT Lab Editor pode configurar a GT-10 para utilizar o driver WinUSB nativo do Windows, mantendo a Integridade da Memória (HVCI) ativada.
brazilianportuguese.WinUsbBody=O driver oficial legado da BOSS/Roland não é compatível com a Integridade da Memória nas versões atuais do Windows 11.%n%nPara configurar o WinUSB, será instalado no computador o certificado público e autoassinado GT LAB WinUSB Laboratory. Esse certificado é utilizado para confiar localmente no pacote WinUSB auditado do projeto para USB\VID_0582&PID_00DA. Nenhuma chave privada está incluída.%n%nIMPORTANTE: ao utilizar o WinUSB, o ÁUDIO USB/ASIO nativo da GT-10 não ficará disponível. Para gravação no computador, utilize uma interface de áudio externa.%n%nA configuração poderá ser removida posteriormente. O instalador não altera a Integridade da Memória, o Secure Boot nem a política de assinatura de drivers.%n%nPara verificar a associação do driver, conecte e ligue a GT-10 durante a instalação.
brazilianportuguese.WinUsbConsent=Li as informações acima e concordo em instalar o suporte WinUSB para a BOSS GT-10.
brazilianportuguese.WinUsbNotSelected=O suporte WinUSB da GT-10 não foi instalado. O GT Lab Editor foi instalado sem alterar drivers de dispositivo ou certificados.
brazilianportuguese.WinUsbFailed=O GT Lab Editor foi instalado, mas a configuração WinUSB falhou com o código %1. Consulte %2. Nenhuma política de segurança foi alterada.
brazilianportuguese.WinUsbFailedFinished=O GT Lab Editor foi instalado, mas a configuração WinUSB não foi concluída. Consulte o log em %1. Nenhuma política de segurança foi alterada.
brazilianportuguese.WinUsbStartFailed=O GT Lab Editor foi instalado, mas o auxiliar de configuração WinUSB não pôde ser iniciado. Nenhum driver ou política de segurança foi alterado.
brazilianportuguese.WinUsbRemovePrompt=Deseja remover também o suporte GT LAB WinUSB da BOSS GT-10?%n%nSim remove somente o pacote de driver GT LAB exato e o certificado de laboratório. O driver Roland não é removido e a Integridade da Memória não é alterada.%n%nNão mantém o suporte WinUSB instalado.
brazilianportuguese.WinUsbUninstallStartFailed=O GT Lab Editor foi removido, mas o auxiliar de remoção WinUSB não pôde ser iniciado. Consulte o log em %1.
brazilianportuguese.WinUsbUninstallFailed=O GT Lab Editor foi removido, mas a remoção WinUSB falhou com o código %1. Consulte o log em %2.
brazilianportuguese.WinUsbLogLabel=Log do WinUSB:

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#WinUsbPayload}\GT-LAB-WinUSB-Lab.cer"; Flags: dontcopy
Source: "{#WinUsbPayload}\gt10-winusb.inf"; Flags: dontcopy
Source: "{#WinUsbPayload}\gt10-winusb.cat"; Flags: dontcopy
Source: "{#WinUsbPayload}\install-winusb-universal.ps1"; Flags: dontcopy
Source: "{#WinUsbPayload}\uninstall-winusb-universal.ps1"; DestDir: "{app}\WinUSB-Support"; Flags: ignoreversion; Check: IsWindows11

[Icons]
Name: "{group}\GT Lab Editor"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\GT Lab Editor"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}\WinUSB-Support"
Type: dirifempty; Name: "{app}\iconengines"
Type: dirifempty; Name: "{app}\imageformats"
Type: dirifempty; Name: "{app}\platforms"
Type: dirifempty; Name: "{app}\printsupport"
Type: dirifempty; Name: "{app}\styles"
Type: dirifempty; Name: "{app}"

[Code]
var
  WinUsbPage: TWizardPage;
  WinUsbText: TNewMemo;
  WinUsbConsent: TNewCheckBox;
  WinUsbRestartRequired: Boolean;
  WinUsbAttempted: Boolean;
  WinUsbSucceeded: Boolean;
  RemoveWinUsbOnUninstall: Boolean;
  WinUsbUninstallInvoked: Boolean;

procedure WriteUniversalLog(MessageText: String);
var
  LogDirectory: String;
  LogPath: String;
begin
  LogDirectory := ExpandConstant('{commonappdata}\GT LAB\Logs');
  ForceDirectories(LogDirectory);
  LogPath := LogDirectory + '\Universal-RC6-Install.log';
  SaveStringToFile(LogPath, GetDateTimeString('yyyy-mm-dd hh:nn:ss', '-', ':') + ' ' + MessageText + #13#10, True);
end;

procedure WriteUniversalUninstallLog(MessageText: String);
var
  LogDirectory: String;
  LogPath: String;
begin
  LogDirectory := ExpandConstant('{commonappdata}\GT LAB\Logs');
  ForceDirectories(LogDirectory);
  LogPath := LogDirectory + '\Universal-RC6-Uninstall.log';
  SaveStringToFile(LogPath, GetDateTimeString('yyyy-mm-dd hh:nn:ss', '-', ':') + ' ' + MessageText + #13#10, True);
end;

function IsWindows11: Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  Result := Version.Build >= 22000;
end;

procedure InitializeWizard;
var
  Version: TWindowsVersion;
  Windows11Text: String;
begin
  GetWindowsVersionEx(Version);
  if Version.Build >= 22000 then
    Windows11Text := 'true'
  else
    Windows11Text := 'false';
  WriteUniversalLog(Format('Detecção do sistema: build=%d; Windows11=%s', [Version.Build, Windows11Text]));

  WinUsbPage := CreateCustomPage(wpSelectTasks,
    ExpandConstant('{cm:WinUsbPageTitle}'),
    ExpandConstant('{cm:WinUsbPageDescription}'));

  WinUsbText := TNewMemo.Create(WinUsbPage);
  WinUsbText.Parent := WinUsbPage.Surface;
  WinUsbText.Left := 0;
  WinUsbText.Top := 0;
  WinUsbText.Width := WinUsbPage.SurfaceWidth;
  WinUsbText.Height := ScaleY(220);
  WinUsbText.ReadOnly := True;
  WinUsbText.ScrollBars := ssVertical;
  WinUsbText.Text := ExpandConstant('{cm:WinUsbHeading}') + #13#10 + #13#10 +
    ExpandConstant('{cm:WinUsbBody}');

  WinUsbConsent := TNewCheckBox.Create(WinUsbPage);
  WinUsbConsent.Parent := WinUsbPage.Surface;
  WinUsbConsent.Left := 0;
  WinUsbConsent.Top := WinUsbText.Top + WinUsbText.Height + ScaleY(12);
  WinUsbConsent.Width := WinUsbPage.SurfaceWidth;
  WinUsbConsent.Height := ScaleY(42);
  WinUsbConsent.Caption := ExpandConstant('{cm:WinUsbConsent}');
  WinUsbConsent.Checked := False;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = WinUsbPage.ID) and (not IsWindows11);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  PowerShellPath: String;
  Parameters: String;
  ResultCode: Integer;
  LogPath: String;
begin
  Result := '';
  WinUsbAttempted := False;
  WinUsbSucceeded := False;

  if not IsWindows11 then
  begin
    WriteUniversalLog('Caminho Windows 10 selecionado: nenhuma extração de certificado, validação de CAT, instalação de INF ou alteração de driver.');
    Exit;
  end;

  if not WinUsbConsent.Checked then
  begin
    WriteUniversalLog('Caminho Windows 11 selecionado; o usuário recusou o suporte WinUSB. A instalação somente do aplicativo continua.');
    Exit;
  end;

  WinUsbAttempted := True;
  WriteUniversalLog('Caminho Windows 11 selecionado; consentimento explícito para WinUSB registrado. Iniciando o auxiliar auditado.');
  ExtractTemporaryFile('GT-LAB-WinUSB-Lab.cer');
  ExtractTemporaryFile('gt10-winusb.cat');
  ExtractTemporaryFile('gt10-winusb.inf');
  ExtractTemporaryFile('install-winusb-universal.ps1');

  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  LogPath := ExpandConstant('{commonappdata}\GT LAB\Logs\WinUSB-Install.log');
  Parameters :=
    '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    ExpandConstant('{tmp}\install-winusb-universal.ps1') + '" -CertificatePath "' +
    ExpandConstant('{tmp}\GT-LAB-WinUSB-Lab.cer') + '" -CatalogPath "' +
    ExpandConstant('{tmp}\gt10-winusb.cat') + '" -InfPath "' +
    ExpandConstant('{tmp}\gt10-winusb.inf') + '" -LogPath "' + LogPath + '"';

  WriteUniversalLog('HELPER_PATH=' + ExpandConstant('{tmp}\install-winusb-universal.ps1'));
  WriteUniversalLog('POWERSHELL_PATH=' + PowerShellPath);
  WriteUniversalLog('POWERSHELL_ARGUMENTS=' + Parameters);

  if not Exec(PowerShellPath, Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    WriteUniversalLog('O auxiliar WinUSB não pôde ser iniciado. A instalação do aplicativo continua.');
    SuppressibleMsgBox(ExpandConstant('{cm:WinUsbStartFailed}'), mbError, MB_OK, IDOK);
    Exit;
  end;

  WriteUniversalLog(Format('HELPER_EXIT_CODE=%d', [ResultCode]));

  if ResultCode = 3010 then
  begin
    WriteUniversalLog('O auxiliar WinUSB foi concluído e solicitou reinicialização (3010).');
    WinUsbSucceeded := True;
    WinUsbRestartRequired := True;
    NeedsRestart := True;
  end
  else if ResultCode = 0 then
  begin
    WriteUniversalLog('O auxiliar WinUSB foi concluído com sucesso.');
    WinUsbSucceeded := True
  end
  else
  begin
    WriteUniversalLog(Format('O auxiliar WinUSB falhou com o código %d. A instalação do aplicativo continua.', [ResultCode]));
    SuppressibleMsgBox(
      FmtMessage(ExpandConstant('{cm:WinUsbFailed}'), [IntToStr(ResultCode), LogPath]),
      mbError, MB_OK, IDOK);
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  Guidance: String;
begin
  if CurPageID = wpFinished then
  begin
    if IsWindows11 and (not WinUsbConsent.Checked) then
      Guidance := ExpandConstant('{cm:WinUsbNotSelected}')
    else if WinUsbAttempted and (not WinUsbSucceeded) then
      Guidance := FmtMessage(ExpandConstant('{cm:WinUsbFailedFinished}'), [ExpandConstant('{commonappdata}\GT LAB\Logs\WinUSB-Install.log')])
    else
      Guidance := '';

    if Guidance <> '' then
      WizardForm.FinishedLabel.Caption := WizardForm.FinishedLabel.Caption + #13#10 + #13#10 + Guidance;
  end;
end;

function NeedRestart: Boolean;
begin
  Result := WinUsbRestartRequired;
end;

function InitializeUninstall: Boolean;
begin
  Result := True;
  RemoveWinUsbOnUninstall := False;
  WinUsbUninstallInvoked := False;
  if IsWindows11 and (not UninstallSilent) and FileExists(ExpandConstant('{app}\WinUSB-Support\uninstall-winusb-universal.ps1')) then
    RemoveWinUsbOnUninstall := MsgBox(ExpandConstant('{cm:WinUsbRemovePrompt}'), mbConfirmation, MB_YESNO) = IDYES;
  if RemoveWinUsbOnUninstall then
    WriteUniversalUninstallLog('UserChoiceRemoveWinUSB=YES')
  else
    WriteUniversalUninstallLog('UserChoiceRemoveWinUSB=NO');
end;

procedure RunWinUsbUninstall;
var
  PowerShellPath: String;
  HelperPath: String;
  LogPath: String;
  Parameters: String;
  ResultCode: Integer;
begin
  if WinUsbUninstallInvoked then
    Exit;
  WinUsbUninstallInvoked := True;

  HelperPath := ExpandConstant('{app}\WinUSB-Support\uninstall-winusb-universal.ps1');
  LogPath := ExpandConstant('{commonappdata}\GT LAB\Logs\WinUSB-Uninstall.log');
  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  Parameters :=
    '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    HelperPath + '" -LogPath "' + LogPath + '"';

  WriteUniversalUninstallLog('HELPER_PATH=' + HelperPath);
  if FileExists(HelperPath) then
    WriteUniversalUninstallLog('HELPER_EXISTS=True')
  else
    WriteUniversalUninstallLog('HELPER_EXISTS=False');
  WriteUniversalUninstallLog('POWERSHELL_PATH=' + PowerShellPath);
  WriteUniversalUninstallLog('POWERSHELL_ARGUMENTS=' + Parameters);

  if not FileExists(HelperPath) then
  begin
    WriteUniversalUninstallLog('HELPER_INVOKED=NO; FinalResult=HELPER_NOT_FOUND');
    if not UninstallSilent then
      MsgBox(FmtMessage(ExpandConstant('{cm:WinUsbUninstallStartFailed}'), [LogPath]), mbError, MB_OK);
    Exit;
  end;

  if not Exec(PowerShellPath, Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    WriteUniversalUninstallLog('HELPER_INVOKED=NO; FinalResult=PROCESS_START_FAILED');
    if not UninstallSilent then
      MsgBox(FmtMessage(ExpandConstant('{cm:WinUsbUninstallStartFailed}'), [LogPath]), mbError, MB_OK);
    Exit;
  end;

  WriteUniversalUninstallLog('HELPER_INVOKED=YES');
  WriteUniversalUninstallLog(Format('HELPER_EXIT_CODE=%d', [ResultCode]));
  if ResultCode = 0 then
    WriteUniversalUninstallLog('FinalResult=PASS')
  else
  begin
    WriteUniversalUninstallLog('FinalResult=HELPER_FAILED');
    if not UninstallSilent then
      MsgBox(FmtMessage(ExpandConstant('{cm:WinUsbUninstallFailed}'), [IntToStr(ResultCode), LogPath]), mbError, MB_OK);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    if RemoveWinUsbOnUninstall then
      RunWinUsbUninstall
    else
      WriteUniversalUninstallLog('WinUSBRemoval=SKIPPED; WinUSBAndCertificates=PRESERVED; FinalResult=APP_ONLY');
  end;
end;
