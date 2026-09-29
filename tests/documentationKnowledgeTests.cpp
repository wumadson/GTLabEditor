#include "ai/knowledge/DocumentationKnowledgeSource.h"
#include "ai/knowledge/LocalKnowledgeEngine.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace Ai::Knowledge;

namespace {
int assertions = 0;
int failures = 0;

void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

DocumentationEntry entry(const QString &id, const QString &title,
                         const QString &language, const QString &body,
                         const QStringList &aliases,
                         const QStringList &topics,
                         const QStringList &blocks = {})
{
    DocumentationEntry value;
    value.id = id;
    value.title = title;
    value.language = language;
    value.body = body;
    value.aliases = aliases;
    value.relatedTopics = topics;
    value.relatedBlocks = blocks;
    value.section = QStringLiteral("Synthetic Test Section");
    value.sourceDocument = QStringLiteral("Synthetic Official Documentation Fixture");
    return value;
}

QList<DocumentationEntry> fixtures()
{
    QList<DocumentationEntry> values;
    values << entry("assign", "Assign", "en",
                    "Synthetic fixture: Assign procedure, Target, Target Min, Target Max, Source, Toggle and Momentary.",
                    {"assignment", "how to configure an assign", "how many assigns"},
                    {"assign"});
    values << entry("assign", "Assign", "pt-BR",
                    "Fixture sintética: procedimento de Assign, Target, Target Min, Target Max, Source, Toggle e Momentary.",
                    {"atribuição", "para que serve assign", "como configurar um assign", "quantos assigns"},
                    {"assign"});
    values << entry("expression", "Expression Pedal", "en",
                    "Synthetic fixture for EXP, EXP SW and Assign usage.",
                    {"expression pedal", "how to use exp", "exp sw"},
                    {"expression", "controllers"});
    values << entry("expression", "Pedal de expressão", "pt-BR",
                    "Fixture sintética para EXP, EXP SW e uso com Assign.",
                    {"pedal de expressão", "como usar exp", "exp sw"},
                    {"expression", "controllers"});
    values << entry("controllers", "Controllers", "en",
                    "Synthetic fixture for CTL1, CTL2, Foot and Bank controllers.",
                    {"ctl1", "ctl2", "foot controller", "bank up", "bank down"},
                    {"controllers"});
    values << entry("signal_chain", "Signal Chain", "en",
                    "Synthetic fixture for signal-chain Split and Merge concepts.",
                    {"signal chain", "split", "merge"}, {"signal_chain"});
    values << entry("routing", "Routing", "en",
                    "Synthetic fixture for Dual L/R, Channel Select and A/B routing.",
                    {"dual l/r", "channel select", "a/b routing"}, {"routing"});
    values << entry("reverb_time", "Reverb Time", "en",
                    "Synthetic fixture for the meaning of Reverb Time and Density.",
                    {"what does reverb time do", "density"}, {"reverb"});
    values << entry("warp_delay", "Warp Delay", "en",
                    "Synthetic fixture for the meaning of Warp Delay.",
                    {"what is warp delay"}, {"delay"});
    return values;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const LocalDocumentationIndex empty;
    const LocalDocumentationIndex index(fixtures());

    expect(index.isAvailable()
           && index.documentNames().contains("Synthetic Official Documentation Fixture"),
           "1 approved logical document found");
    expect(!empty.isAvailable() && empty.search("Assign", "en").isEmpty(),
           "2 nonexistent document remains unavailable");

    DocumentationEntry found;
    expect(index.topic("assign", "pt-BR", &found)
           && found.language == "pt-BR", "3 topic lookup");
    expect(index.findByAlias("ATRIBUICAO", "pt-BR", &found)
           && found.id == "assign", "4 accent-insensitive alias lookup");
    expect(index.answer("para que serve assign", "pt-BR").text.contains("sintética"),
           "5 pt-BR document response");
    expect(index.answer("how to configure an assign", "en").text.contains("Synthetic"),
           "6 English document response");
    expect(index.answer("how to configure an assign", "en").answered,
           "7 Assign documentation");
    expect(index.answer("how to use exp", "en").answered,
           "8 EXP documentation");
    expect(index.answer("ctl1", "en").answered,
           "9 CTL documentation");
    expect(index.answer("signal chain", "en").answered,
           "10 Signal Chain documentation");
    expect(index.answer("dual l r", "en").answered,
           "11 routing documentation");
    expect(index.answer("what does reverb time do", "en").answered,
           "12 parameter meaning");

    const LocalKnowledgeAnswer traced = index.answer("signal chain", "en");
    const QJsonObject trace = traced.metadata.value("documentationTrace").toObject();
    expect(traced.sourceKind == SourceKind::OfficialDocumentation
           && trace.value("sourceDocument").toString()
                  == "Synthetic Official Documentation Fixture"
           && trace.value("section").toString() == "Synthetic Test Section",
           "13 source traceability");

    QList<DocumentationEntry> conflicting = fixtures();
    conflicting[0].claims.insert("assign.count", 4);
    const LocalDocumentationIndex conflictIndex(conflicting);
    const LocalKnowledgeEngine conflictEngine(&conflictIndex);
    const LocalKnowledgeAnswer conflict = conflictEngine.answer(
        "How many Assigns does the GT-10 have?", "en");
    expect(conflict.answered && conflict.text.contains("8 Assigns")
           && conflict.metadata.value("documentationConflict").toBool(),
           "14 structural truth wins a documented conflict");

    const LocalKnowledgeEngine documentedEngine(&index);
    const LocalKnowledgeAnswer unknown = documentedEngine.answer(
        "How do I configure Wi-Fi on the GT-10?", "en");
    expect(!unknown.answered && unknown.text.contains("documentation"),
           "15 documentation-specific unknown fallback");

    const QList<DocumentationSearchResult> first = index.search("assign target", "en");
    const QList<DocumentationSearchResult> second = index.search("assign target", "en");
    expect(!first.isEmpty() && first.size() == second.size()
           && first.first().entry.id == second.first().entry.id
           && first.first().score == second.first().score,
           "16 deterministic weighted search");

    DocumentationEntry unsafe = entry("unsafe", "Unsafe", "en", "unsafe",
                                      {"unsafe"}, {"system"});
    unsafe.sourceDocument = "/Users/private/manual.pdf";
    DocumentationEntry remote = unsafe;
    remote.id = "remote";
    remote.sourceDocument = "https://example.invalid/manual";
    const LocalDocumentationIndex rejected({unsafe, remote});
    expect(!rejected.isAvailable()
           && !traced.text.contains("/Users/")
           && !trace.value("sourceDocument").toString().contains('/'),
           "17 filesystem privacy");
    expect(!rejected.isAvailable(), "18 remote sources are never loaded");

    const QMap<QString, DocumentationCoverage> coverage = index.coverageReport();
    expect(coverage.value("assign") == DocumentationCoverage::Covered
           && coverage.value("midi") == DocumentationCoverage::Unavailable,
           "19 coverage report distinguishes covered and unavailable topics");
    const LocalKnowledgeAnswer otherLanguage = index.answer(
        "what is warp delay", "pt-BR");
    expect(!otherLanguage.answered
           && otherLanguage.metadata.value("availableLanguage").toString() == "en",
           "20 no automatic free translation of English-only content");

    const LocalDocumentationIndex official = officialGT10DocumentationIndex();
    expect(official.isAvailable()
           && official.documentNames().size() == 3
           && official.documentNames().contains("GT-10 Owner's Manual")
           && official.documentNames().contains("GT-10 Preset Patch List")
           && official.documentNames().contains("GT-10 MIDI Implementation"),
           "21 three official indexes load from resources");

    const LocalKnowledgeAnswer assignPt = official.answer(
        "Pra que serve o Assign?", "pt-BR");
    const QJsonObject assignTrace = assignPt.metadata.value(
        "documentationTrace").toObject();
    expect(assignPt.answered
           && assignPt.sourceKind
                  == SourceKind::DerivedFromOfficialDocumentation
           && assignPt.text.contains("oito Assigns")
           && assignTrace.value("documentId").toString() == "owners_manual"
           && assignTrace.value("pageStart").toInt() == 50
           && assignTrace.value("pageEnd").toInt() == 54
           && assignTrace.value("sourceLanguage").toString() == "en",
           "22 pt-BR answer is derived and traces English pages");

    expect(official.answer("O que significa Target Min?", "pt-BR").answered
           && official.answer("Como calibrar EXP?", "pt-BR").text.contains(
                  "PDL CALIBRATION")
           && official.answer("O que faz Density?", "pt-BR").text.contains(
                  "densidade")
           && official.answer("O que é Warp Delay?", "pt-BR").text.contains(
                  "Warp Delay"),
           "23 official manual semantic aliases");

    const LocalKnowledgeAnswer patch = official.answer("P01-1", "en");
    const QJsonObject patchTrace = patch.metadata.value(
        "documentationTrace").toObject();
    expect(patch.answered
           && patch.sourceKind == SourceKind::OfficialPatchReference
           && patch.text.contains("STACK LEAD")
           && patchTrace.value("documentId").toString() == "patch_list"
           && patchTrace.value("pageStart").toInt() == 1,
           "24 factory patch reference is semantically separate");

    const LocalKnowledgeAnswer sysex = official.answer("RQ1 DT1 SysEx", "en");
    const QJsonObject sysexTrace = sysex.metadata.value(
        "documentationTrace").toObject();
    expect(sysex.answered && sysex.text.contains("RQ1")
           && sysexTrace.value("documentId").toString()
                  == "midi_implementation"
           && sysexTrace.value("pageStart").toInt() == 2,
           "25 MIDI implementation query and page trace");

    const QMap<QString, DocumentationCoverage> officialCoverage =
        official.coverageReport();
    expect(officialCoverage.value("assign") == DocumentationCoverage::Covered
           && officialCoverage.value("expression") == DocumentationCoverage::Covered
           && officialCoverage.value("controllers") == DocumentationCoverage::Covered
           && officialCoverage.value("signal_chain") == DocumentationCoverage::Covered
           && officialCoverage.value("routing") == DocumentationCoverage::Covered
           && officialCoverage.value("delay") == DocumentationCoverage::Covered
           && officialCoverage.value("reverb") == DocumentationCoverage::Covered
           && officialCoverage.value("midi") == DocumentationCoverage::Covered
           && officialCoverage.value("system") == DocumentationCoverage::Covered,
           "26 coverage is derived from real indexed chunks");

    expect(!official.answer("How do I configure Wi-Fi?", "en").answered
           && !official.answer("Como configurar Bluetooth?", "pt-BR").answered
           && !official.answer("Does the manual say there are 17 reverbs?", "en").answered,
           "27 official index does not fabricate unsupported features");

    QTextStream(stdout) << "Documentation Knowledge tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
