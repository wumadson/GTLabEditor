#include "ai/knowledge/KnowledgeGuides.h"

namespace Ai {
namespace Knowledge {

QList<KnowledgeGuide> builtInGuides()
{
    return {
        {"assign_basics", "Configuração básica de Assign",
         "Assign basics",
         "Escolha um dos Assigns 1–8, ative-o, defina Target, Target Min e Target Max, escolha Source e ajuste Source Mode e a faixa ativa quando aplicável.",
         "Choose one of Assigns 1–8, enable it, set Target, Target Min and Target Max, choose Source, then adjust Source Mode and the active range when applicable.",
         {"assign"}},
        {"expression_basics", "Pedal de expressão e EXP SW",
         "Expression pedal and EXP SW",
         "Use EXP como Source de um Assign para controlar um Target contínuo existente no catálogo. EXP SW é um controle separado e também pode acionar Assigns.",
         "Use EXP as an Assign Source to control a continuous Target present in the catalog. EXP SW is a separate control and can also trigger Assigns.",
         {"expression", "assign"}},
        {"controller_scope", "Escopo dos controladores",
         "Controller scope",
         "CTL1, CTL2 e EXP SW têm funções diretas por patch. FOOT 1–4 e BANK UP/DOWN são configurações System no modelo atual do Editor.",
         "CTL1, CTL2 and EXP SW have per-patch direct functions. FOOT 1–4 and BANK UP/DOWN are System settings in the current Editor model.",
         {"controllers", "patch_vs_system"}},
        {"signal_chain_basics", "Signal Chain",
         "Signal Chain",
         "A cadeia possui regiões comuns e caminhos A/B delimitados por SPLIT e MERGE. A ordem exibida deve vir do patch atual, não de uma cadeia presumida.",
         "The chain has common regions and A/B paths bounded by SPLIT and MERGE. The displayed order must come from the current patch, not an assumed chain.",
         {"signal_chain"}},
        {"dual_preamp_basics", "PREAMP A/B",
         "PREAMP A/B",
         "A GT-10 possui PREAMP A e PREAMP B. Channel Mode e Channel Select determinam como os dois caminhos são usados; os valores atuais devem ser lidos do contexto do patch.",
         "The GT-10 provides PREAMP A and PREAMP B. Channel Mode and Channel Select determine how both paths are used; current values must come from patch context.",
         {"preamp", "routing"}},
        {"routing_basics", "Routing",
         "Routing",
         "Routing combina Channel Mode, Channel Select e a topologia real da Signal Chain. Patch Output Select só é usado quando Output Select Mode está em Patch; caso contrário vale System Output Select.",
         "Routing combines Channel Mode, Channel Select and the actual Signal Chain topology. Patch Output Select is used only when Output Select Mode is Patch; otherwise System Output Select applies.",
         {"routing", "patch_vs_system"}}
    };
}

const KnowledgeGuide *findGuide(const QList<KnowledgeGuide> &guides,
                                const QString &id)
{
    for (const KnowledgeGuide &guide : guides) {
        if (guide.id == id)
            return &guide;
    }
    return nullptr;
}

} // namespace Knowledge
} // namespace Ai
