# GT Lab Editor

**Modern editor for the BOSS GT-10.**

O GT Lab Editor é um editor independente para a BOSS GT-10, baseado no
**GT-10 FxFloorBoard / FXFloorBoard** e mantido por **Wumadson Cardoso / GT LAB**.
A versão atual do produto é **1.2.0**, nas edições **Free** e **Full**.
A interface está disponível em português brasileiro e inglês.

## Free e Full

A Free continua sendo um editor utilizável: permite editar timbres, renomear
patches e usar a leitura e a escrita normal já suportadas pelo Editor.
Somente os cinco grupos avançados abaixo são restritos à Full. Assigns já
existentes no patch são preservados; a Free não permite reconfigurá-los.

| Recurso | Free | Full |
|---|:---:|:---:|
| Edição de patches/parâmetros e rename | ✅ | ✅ |
| PREAMP / OD/DS / COMP / EQ / FX1 / FX2 | ✅ | ✅ |
| Delay / Reverb / Chorus | ✅ | ✅ |
| Signal Chain | ✅ | ✅ |
| Tuner | ✅ | ✅ |
| Pedalboard | ✅ | ✅ |
| Library | ✅ | ✅ |
| Importar Preset | 🔒 | ✅ |
| Exportar Preset | 🔒 | ✅ |
| Configuração de Assign | 🔒 | ✅ |
| Backup / Restore | 🔒 | ✅ |
| Quick Settings | 🔒 | ✅ |

## Download

**[GT Lab Editor Free v1.2.0 — instaladores públicos](https://github.com/wumadson/GTLabEditor/releases/tag/v1.2.0-free)**

A release inclui o instalador Windows x64, o DMG macOS ARM64, o GT-10 USB
Bridge ARM64 v1.1.2 e o arquivo de hashes SHA-256. Confira o hash do download
antes de instalar.

**GT Lab Editor Full é distribuído pelos canais oficiais do GT LAB.**
Seu código-fonte permanece disponível sob a licença GPL aplicável.
A Free é a única edição com instaladores públicos atualmente.

## Compatibilidade

É necessário ter uma BOSS GT-10 e conexão USB. Os estados abaixo refletem
as validações registradas pelo mantenedor, não uma garantia para todas as
configurações de computador.

| Plataforma | Estado |
|---|---|
| Windows 10 x64 | ✅ Validado |
| Windows 11 x64 | ✅ Validado |
| macOS Apple Silicon / ARM64 | ✅ Validado |
| macOS Intel / x86_64 | 🧪 Build validada; hardware em validação |

## Instalação no Windows

### Windows 10

Use o **driver oficial BOSS/Roland da GT-10**, instalado separadamente.
O projeto não distribui esse driver; obtenha-o em uma fonte oficial e siga
as instruções do fabricante.

1. Instale o Editor e conecte/ligue a GT-10.
2. Abra o Editor e vá em `Tools > USB/MIDI`.
3. Selecione a porta BOSS GT-10 em **MIDI IN** e **MIDI OUT**.
4. Confirme a configuração e aguarde a conexão.

WinUSB não é o caminho recomendado para Windows 10.

### Windows 11

A Full possui um **Universal Installer** que pode configurar o suporte
GT LAB WinUSB, mediante consentimento explícito. O suporte é específico da
GT-10 (`VID_0582 / PID_00DA`), não um driver genérico para outras interfaces.
O instalador informa a alteração de binding e a confiança local no
certificado autoassinado do pacote de laboratório.

O mantenedor validou binding inicial, repair, fresh binding, reconexão e
persistência após reboot. A Integridade da Memória/HVCI permaneceu habilitada;
uma Scarlett conectada no teste não sofreu interferência.

O instalador **Free é APP-ONLY**: não instala drivers nem certificados.
Ele requer uma infraestrutura GT-10/WinUSB funcional previamente configurada.
Nessa condição, a Free pode detectar a GT-10 automaticamente no Windows 11.

### Áudio USB e ASIO

Com a GT-10 vinculada ao WinUSB, o caminho oficial Roland/BOSS não está ativo
para esse binding. O Editor dispõe da comunicação necessária, mas recursos
dependentes do driver oficial, incluindo áudio USB/ASIO da GT-10, podem ficar
indisponíveis. Use uma interface de áudio externa quando necessário.

**Não desabilite HVCI, Integridade da Memória, Secure Boot ou a imposição de
assinatura de drivers para usar o Editor.**

## Instalação no macOS Apple Silicon / ARM64

O Editor precisa do **GT-10 USB Bridge**, distribuído separadamente:

- `GT-10_USB_Bridge_v1.1.2_Installer.pkg`
- `GTLabEditor-Free-1.2.0-macOS-arm64.dmg`

1. Instale primeiro o Bridge.
2. Abra o DMG e copie **GT Lab Editor Free.app** para **Applications**.
3. Conecte e ligue a GT-10.
4. Abra o Editor e aguarde **CONNECTED / READY**.
5. Se necessário, selecione as portas do Bridge em `Tools > USB/MIDI`.

A Full no macOS utiliza o mesmo Bridge. O Editor não substitui o Bridge e
não comunica com a GT-10 sozinho por esse caminho.

## macOS Intel

O suporte Intel é **experimental; a validação física ainda está pendente**.
Já foram registrados build x86_64 do Editor, Qt/dependências, seis suítes core
e geração de DMG. O protótipo separado do Bridge Intel também passou no build
e na auditoria de package dos seus cinco componentes.

Ainda não foram confirmados instalação física em Mac Intel, conexão GT-10,
MIDI, áudio/HAL ou READY reais. Os artifacts internos de CI não são downloads
oficiais nem uma release estável para Intel.

## Coexistência e desinstalação

Free e Full têm app/executável, identidade de instalação e preferências
separados; sua coexistência foi validada no macOS ARM64 e Windows.
A instalação Free não substitui a Full.

No Windows, remova a edição desejada em **Aplicativos instalados / Apps &
Features**. A Free remove somente a aplicação, sem alterar drivers.
O Universal Installer Full oferece a escolha de remover o suporte GT LAB
WinUSB; isso não equivale a excluir pacotes Roland do Driver Store.

No macOS, a app do Editor pode ser removida de Applications independentemente
da outra edição. O Bridge é um componente separado. Preferências podem ser
mantidas para uma reinstalação.

## Código-fonte, créditos e licença

- **Free 1.2.0:** [source da tag v1.2.0-free](https://github.com/wumadson/GTLabEditor/tree/v1.2.0-free)
  e [branch free/1.2.0](https://github.com/wumadson/GTLabEditor/tree/free/1.2.0).
- **Full 1.2.0:** [source da tag v1.2.0](https://github.com/wumadson/GTLabEditor/tree/v1.2.0).
- **Builds:** [BUILDING.md](BUILDING.md).

Desenvolvimento original: **Colin Willcocks** e **Uco Mesdag**.
Modernização e desenvolvimento adicional: **Wumadson Cardoso / GT LAB**.
O [projeto original no SourceForge](https://sourceforge.net/p/fxfloorboard/fxfloorboard/ci/gt-10/tree/),
seus créditos, copyrights e histórico permanecem preservados.

O projeto é distribuído sob a **GNU GPL versão 2 ou, à sua escolha, qualquer
versão posterior (`GPL-2.0-or-later`)**, conforme os headers e notices.
Veja [LICENSE.GPL-2.0](LICENSE.GPL-2.0).
O [license.txt](license.txt) histórico contém a GPLv3; isso não muda a
declaração do projeto para GPL-3.0-only.

Os instaladores oficiais Full são distribuídos pelos canais oficiais do
GT LAB. A distribuição comercial não elimina os direitos dos destinatários
sob a GPL, incluindo os requisitos de acesso ao código-fonte correspondente
e aos scripts de build aplicáveis.

Componentes terceiros mantêm suas próprias licenças. Veja
[Third-Party Notices](installer/THIRD-PARTY-NOTICES.txt) e
[Asset Provenance](ASSET-PROVENANCE.md). A versão do Qt depende da plataforma
e do pacote; consulte a documentação de build e o About da distribuição.

## Privacidade

O Editor não contém telemetry, analytics ou upload automático.
Links externos de Help, Manual, Source Code, Web e Donate são abertos por
ação do usuário, não automaticamente. A operação USB/MIDI usa os caminhos
locais do Editor e da GT-10.

## Assinatura dos instaladores

Os instaladores públicos ainda não têm certificado oficial Authenticode /
Developer ID ou notarização Apple. O Windows pode mostrar avisos do
SmartScreen; o macOS pode mostrar avisos do Gatekeeper.
O bundle macOS usa assinatura ad-hoc, que não é Developer ID.

Opções futuras de assinatura estão em avaliação; veja [SIGNING.md](SIGNING.md).

## Aviso de marcas

BOSS e GT-10 são marcas de seus respectivos proprietários.
GT Lab Editor é um projeto independente, sem afiliação, endosso ou patrocínio
da Roland Corporation ou BOSS.
