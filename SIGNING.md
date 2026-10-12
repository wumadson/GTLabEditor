# Code signing

Os instaladores públicos atuais não possuem certificado oficial Authenticode
ou Developer ID / notarização Apple. O bundle macOS usa assinatura ad-hoc.
SmartScreen e Gatekeeper podem apresentar avisos; assinatura ad-hoc não é
equivalente a Developer ID nem a aprovação da Apple.

## Proposta anterior SignPath

A documentação anterior registrava uma intenção de candidatura ao SignPath
Foundation, não uma aprovação ou serviço ativo. Não há integração SignPath,
segredos de assinatura ou fluxo de aprovação implementados nos workflows
auditados. Opções futuras continuam em avaliação.

O fluxo proposto era: compilar um EXE verificável, obter aprovação manual da
assinatura, montar o installer com esse EXE e obter uma segunda aprovação
para o installer. Esta sequência é contexto histórico, não uma promessa de
implementação ou instrução para assinar os artifacts atuais.

## Condições para uma futura integração

- Confirmar elegibilidade, serviço, política e responsáveis reais antes de
  habilitar a integração; os antigos placeholders de reviewers/approvers não
  representam pessoas ou permissões aprovadas.
- Definir proteção do repositório, MFA e aprovação manual de cada release.
- Preservar rastreabilidade source-to-artifact e acesso ao source GPL.
- Não assinar Qt, ANGLE, CRT Microsoft ou outros binários upstream como se
  fossem código autoral do GT LAB; preservar suas licenças e identidades.
- Não confundir assinatura do aplicativo com assinatura oficial de driver.
  O certificado autoassinado do pacote WinUSB de laboratório é outro assunto.
- Nunca armazenar chaves privadas, senhas ou tokens no repositório.

Mantenedor: Wumadson Cardoso / GT LAB. Esta documentação não nomeia novos
reviewers/approvers e não autoriza mudanças em CI, certificados ou releases.
