# Building GT Lab Editor 1.2.0

Documentação técnica das edições Full e Free. Para instalação pelo usuário,
consulte [README.md](README.md). Estes passos não modificam drivers e não
publicam releases automaticamente.

## Escolha o source correto

A default branch `modern-ui` ainda contém source, metadata e scripts Windows
rotulados como 1.1.0. Ela não é, por si só, o source das distribuições 1.2.0.
Use as tags imutáveis em checkouts/worktrees separados:

| Edição | Ref de release | Comportamento do build |
|---|---|---|
| Full 1.2.0 | `v1.2.0` | padrão, sem flag Free |
| Free 1.2.0 | `v1.2.0-free` | `CONFIG+=gtlab_free` |

A branch `free/1.2.0` acompanha o desenvolvimento Free; para reproduzir a
release, prefira sua tag. Não copie installers ou scripts entre as edições
e não mescle Free na Full para compilar. O refresh documental não atualiza
código ou pipelines da default branch.

O build normal na tag Free continua Full por padrão; a flag Free define
`GTLAB_FREE_EDITION`, identidade própria e guards somente para Import,
Export, configuração Assign, Backup/Restore e Quick Settings.

## macOS Apple Silicon / ARM64

Toolchain local validada: Apple clang, Qt **5.15.19**, qmake 3.1 e SDK macOS.
A distribuição ARM64 tem mínimo macOS 11.0 no bundle. Confirme a arquitetura
e o deployment do Qt utilizado; não presuma que qualquer build Qt serve.

Exemplos de shadow build, executados no respectivo checkout da tag:

```bash
# Full — source v1.2.0
mkdir -p build/macos-full
cd build/macos-full
/opt/homebrew/opt/qt@5/bin/qmake ../../GT-10FxFloorBoard.pro CONFIG+=release
make -j1
```

```bash
# Free — source v1.2.0-free
mkdir -p build/macos-free
cd build/macos-free
/opt/homebrew/opt/qt@5/bin/qmake ../../GT-10FxFloorBoard.pro CONFIG+=release CONFIG+=gtlab_free
make -j1
```

Saídas relativas ao shadow build:

- Full: `packager/GT Lab Editor.app`, executável `GTLabEditor`.
- Free: `packager/GT Lab Editor Free.app`, executável `GTLabEditorFree`.

As tags 1.2.0 usam `resources_big`, mecanismo nativo Qt/qmake que evita
compilar o resource monolítico como um enorme array C++.
Não remova essa configuração. Use paralelismo 1 e monitore RSS, memória/swap.
Não execute testes junto com o build; interrompa diante de pressão anormal.

### Deployment e DMG

Use `macdeployqt` da mesma instalação Qt, em uma cópia de staging da app:

```bash
/opt/homebrew/opt/qt@5/bin/macdeployqt "GT Lab Editor Free.app" -verbose=2 -codesign=-
codesign --verify --deep --strict "GT Lab Editor Free.app"
```

Antes de gerar o DMG, audite todos os Mach-O com `file`, `lipo -archs` e
`otool -L`: executável, frameworks Qt, plugins e dylibs devem ser ARM64.
Dependências não fornecidas pelo sistema devem estar dentro da app, com
paths internos resolvíveis; não pode restar dependência necessária do
Homebrew, checkout ou diretório de build.

Na preparação da Free, `macdeployqt` omitiu a dependência WebP transitiva
`libsharpyuv.0.dylib`. Foi incorporada somente no staging ARM64, com caminhos
internos ajustados e reassinatura ad-hoc. Verifique dependências transitivas,
não apenas Qt; não altere o source para compensar falhas de deployment.

Gere o DMG com `hdiutil create`, contendo somente a app da edição e, se usado,
o atalho Applications. Monte-o read-only, copie a app para um diretório
temporário, verifique assinatura/arquitetura e teste abertura, About e recursos.
Não substitua a Full instalada para validar a Free.

Artefato público Free: `GTLabEditor-Free-1.2.0-macOS-arm64.dmg`.
Use saída separada, por exemplo `dist/free/macos/1.2.0/`, e registre SHA-256.
O Bridge ARM64 v1.1.2 é instalado/distribuído separadamente; não o reconstrua
ou inclua dentro do DMG do Editor.

## Windows x64

Toolchain do pipeline versionado e validado:

| Componente | Versão |
|---|---|
| Qt | 5.15.2 `msvc2019_64` |
| qmake | 3.1 |
| MSVC | v142, `cl.exe` 19.29.x, x64 |
| Windows SDK | 10.0.26100.x |
| Inno Setup | 6.7.3 |
| Configuração | Release x64 |

Visual Studio 2022 pode hospedar o toolset v142. Os scripts versionados
verificam essas versões; não use apenas o nome da instalação VS como prova
do compilador ativo. Use o ambiente x64 do toolset correto e o Qt correspondente.

### Full

Na tag `v1.2.0`, os scripts `scripts/windows/build-release.ps1` e
`stage-release.ps1` oferecem build/staging do executável Full.
Defina `QT_ROOT_DIR` e `INNO_SETUP_ROOT`; informe `VsInstallPath` se necessário.

O **Universal Installer Full** é `installer/windows/GTLabEditor-Windows.iss`.
Ele usa os inputs locais auditados de `installer/windows/winusb-lab/`, com
consentimento para WinUSB no Windows 11. Esses inputs não estão no Git público.
Não gere novos INF/CAT/certificados nem altere binding ao compilar a aplicação.

O `GTLabEditor.iss` usado pelo pipeline automático é **app-only Full**,
não o Universal. Separe as saídas: ambos os definitions Full podem produzir
o mesmo nome de Setup. Registre source, definition, toolchain e hash final.

```powershell
# Após build e staging Full, a partir do checkout v1.2.0:
& "$env:INNO_SETUP_ROOT\ISCC.exe" .\installer\windows\GTLabEditor-Windows.iss
```

Inputs históricos auditados, reutilizados sem regeneração:

| Input | SHA-256 |
|---|---|
| `gt10-winusb.inf` | `57fc8c322a7ea96bdab5db98e770aca4a2bd6828fcde79a857971bd1b38ba14e` |
| `gt10-winusb.cat` | `5ff62b27a436df0ab2d649458cc93839e24d57e3b944dcb35e0fb068a59df85e` |
| `GT-LAB-WinUSB-Lab.cer` | `39ce00a770ab28c9de39dfd92eedecb1ba342a7ba21ae9a78e7d242934af150e` |

### Free

Use a tag `v1.2.0-free`, com `CONFIG+=gtlab_free`.
Ela seleciona `GTLabEditorFree.exe` e o resource Windows próprio.
Os scripts Full existentes não recebem automaticamente essa flag e procuram
`GTLabEditor.exe`; não execute `build-all.ps1` esperando um pacote Free.

Em um shadow build limpo e no ambiente MSVC x64 adequado:

```powershell
# Executar a partir de build\windows-free no checkout Free:
& "$env:QT_ROOT_DIR\bin\qmake.exe" ..\..\GT-10FxFloorBoard.pro -spec win32-msvc CONFIG+=release CONFIG+=gtlab_free
nmake
```

Algumas instalações Qt/MSVC exigem informar `QMAKE_MSC_VER` e
`QMAKE_MSC_FULL_VER` antes de carregar o projeto, como faz o script Full.
Derive os valores do `cl.exe` ativo; não copie um valor de patch do compilador
de outra máquina.

Prepare staging separado em `dist/staging/windows/GTLabEditorFree/`.
Use o `windeployqt --release` correspondente e o CRT VC142 app-local completo;
copie licenças/notices e valide DLLs, plugins e manifest de hashes.
Então, a partir do checkout Free:

```powershell
& "$env:INNO_SETUP_ROOT\ISCC.exe" .\installer\windows\GTLabEditor-Free.iss
```

Saída: `GTLabEditor-Free-1.2.0-Windows-x64-Setup.exe`.
O installer Free é APP-ONLY: AppId, diretório, atalhos e executável próprios,
sem instalação/remoção de drivers ou certificados. Não reutilize o AppId Full.

### Transporte e validação

Windows 10: driver BOSS/Roland instalado independentemente, WinMM/RtMidi.
Windows 11 Full: WinUSB opcional pelo Universal Installer, específico da GT-10.
Windows 11 Free: requer binding WinUSB funcional prévio; o startup verifica
disponibilidade e reutiliza o backend, sem instalar drivers.
Áudio USB/ASIO do driver oficial pode ficar indisponível nesse binding.
Não desabilite HVCI, Memory Integrity, Secure Boot ou driver-signature enforcement.

## Testes e gates

Execute suítes sequencialmente, depois do build: Signal Chain, Patch Change
Guard, Backup, Decoder, Quick Settings e Artwork. Na tag Free, execute também
`CONFIG+=productedition_tests` em Full e Free. Os harnesses substituem o entry
point somente com configuração explícita; não os inclua no pacote final.
Use perfis offline sem portas MIDI para testes automatizados.

Gate de distribuição: versão/identidade, arquitetura, recursos, dependências,
assinatura aplicável, abertura de uma cópia do pacote, licenças/notices e hashes.
Testes offline não comprovam comportamento físico USB/MIDI/áudio.

## macOS Intel e CI

O workflow experimental `.github/workflows/macos-intel-build.yml` está na
default branch, mas recebe um `source_ref` imutável separado (padrão Full 1.2.0).
Não compila implicitamente a HEAD do workflow. Usa runner Intel, Qt 5.15.2,
deployment target 11.0 e gates de arquitetura/dependências, testes e DMG.

Editor x86_64 e protótipo Bridge x86_64 passaram em build/auditoria; a
validação física de instalação, GT-10, MIDI, áudio/HAL e READY segue pendente.
Não tratar artifacts internos como downloads estáveis. O source do Bridge
pertence a outro projeto e não é alterado pelo build do Editor.

O workflow Windows `.github/workflows/windows-build.yml` continua como
pipeline unsigned app-only, disparado por dispatch/push em `modern-ui`.
Na default branch, nomes e caminhos de artifacts ainda são 1.1.0.
Não equivale ao pacote Free 1.2.0 nem ao Universal Full 1.2.0.
Esta revisão não corrige scripts/workflows: isso exige outra autorização.

## Licenças, assinatura e reprodução

Preserve licenças, créditos e notices; veja [README.md](README.md),
[ASSET-PROVENANCE.md](ASSET-PROVENANCE.md) e [SIGNING.md](SIGNING.md).
A questão de licenciamento do Inno Setup registrada no audit anterior e a
geração de SBOM permanecem pendentes de confirmação, não resolvidas por este texto.

O pipeline registra comandos, versões e hashes para rastreabilidade, mas
não garante reprodução bit-for-bit: timestamps PE/COFF, Inno/ZIP, imagem do
runner e ordering de deployment podem variar.
Nenhum segredo ou credencial deve entrar no source, logs ou pacotes.
