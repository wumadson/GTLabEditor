# GT Lab Editor v1.1.0

Preparação da release. Os artefatos finais macOS e Windows ainda devem passar
pela regressão final antes da criação da tag e da publicação. Mudanças abaixo
comparadas à v1.0.0.

## Novidades

- Ação explícita **READ SYSTEM / LER SYSTEM** para reler os parâmetros SYSTEM
  da GT-10 sem reiniciar ou reconectar o Editor. O READ principal continua
  dedicado ao patch.

## Correções

- Leitura e escrita multibyte dos System Controllers usando os caminhos
  existentes do backend, incluindo CTL3/CTL4 e valores de destino.
- EXP2 com Function = Patch Level: Max exibe 0–200 a partir de raw 0–100.
  Min e Foot Volume permanecem 0–100, sem scaling, nas telas SYSTEM e EXPRESSION.
- Melhor adaptação da janela à área disponível do monitor e acesso ao conteúdo
  em telas 1360/1366×768 por meio de rolagem.

## Interface

- ParameterBars mais compactas: altura de 48 px, track de 8 px e raio de 4 px,
  preservando interação, ranges e formatação dos valores.
- Diagrama PREAMP A/B com junção contínua, contraste e hierarquia refinados.
- Expression, FV e P.FX usam o mesmo artwork de pedal de expressão.
- Fundo, título e indicadores dos cards inferiores não abrem mais os editores.
  EDITAR continua abrindo cada editor; os atalhos dos badges Assign permanecem.
- Traduções PT-BR das mensagens de READ SYSTEM e da versão no About.

## Compatibilidade

- Melhorias de layout para 768p. A regressão final inclui Windows em 1366×768,
  escala 100% e taskbar visível, além de resize e geometria salva.
- Nenhuma mudança de protocolo MIDI/SysEx ou dos backends de transporte nesta
  rodada.

## Windows

- Mantido o modelo universal Windows 10/11 da v1.0.0. O installer público será
  gerado explicitamente com `GTLabEditor-Windows.iss`, não com o app-only do CI.
- Windows 10: driver BOSS/Roland e WinMM/RtMidi.
- Windows 11: WinUSB opcional, mediante consentimento explícito. Payload e
  helpers históricos são reutilizados sem mudanças; não há nova assinatura
  pública de driver nesta preparação.

## macOS

- Apple Silicon ARM64, Qt privado 5.15.19 sem OpenGL, target macOS 11.0.
- GT-10 USB Bridge v1.1.2 continua obrigatório e não recebe alteração de versão.
- Bundle planejado: versão 1.1.0, build 2.

## Limitações conhecidas

- A GT-10 pode ignorar Program Change externo em telas físicas de edição;
  utilize a tela PLAY para trocar patches pelo Editor.
- A reconexão automática no fluxo do Bridge macOS permanece limitada;
  recuperação manual pode ser necessária.
- Com o binding WinUSB ativo no Windows 11, USB Audio/ASIO da GT-10 ficam
  indisponíveis. Use uma interface de áudio externa para gravação.
- O app macOS usa assinatura ad-hoc e não é notarizado.

## Créditos

Derivado de GT-10 FxFloorBoard / FXFloorBoard, de Colin Willcocks e Uco Mesdag.
Modernização por Wumadson Cardoso / GT LAB. Créditos, copyright notices e licença
GPL-2.0-or-later permanecem preservados; componentes terceiros mantêm suas
próprias licenças e notices.

## Artefatos e SHA-256

Planejados para publicação após validação:

- `GTLabEditor-1.1.0-Windows-x64-Setup.exe` (universal)
- `GTLabEditor-1.1.0-macOS-arm64.dmg`
- `GT-10_USB_Bridge_v1.1.2_Installer.pkg`
- `GT-LAB-v1.1.0-SHA256SUMS.txt`

Hashes finais serão calculados sobre os artefatos efetivamente publicados.
O portable e o installer app-only não serão publicados nesta release.
