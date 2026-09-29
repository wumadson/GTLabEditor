#include "ai/knowledge/GT10CapabilityService.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace Ai::Knowledge;

namespace {
int failures = 0;
int assertions = 0;
void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    GT10CapabilityService service;

    expect(service.deviceModel() == "GT-10", "1 device identity");
    expect(service.hasBlock("delay"), "2 existing block");
    expect(!service.hasBlock("xyz123"), "3 absent block");
    expect(service.hasEffectType("fx1", "Tremolo"), "4 existing effect type");
    expect(!service.hasEffectType("fx1", "XYZ123"), "5 absent effect type");
    expect(service.parameterCapability("reverb", "Spring", "Reverb Time").available,
           "6 existing parameter");
    expect(!service.parameterCapability("reverb", "Spring", "XYZ123").available,
           "7 absent parameter");
    const ParameterCapability time = service.parameterCapability(
        "reverb", "Spring", "Reverb Time");
    expect(!time.maximum.isEmpty() || !time.enumValues.isEmpty(),
           "8 range or enum consultable");
    expect(service.hasBlock("preampA") && service.hasBlock("preampB"),
           "9 PREAMP A/B");
    bool dualDelay = false;
    for (const QString &type : service.effectTypesForBlock("delay"))
        dualDelay = dualDelay || service.normalize(type).contains("dual");
    expect(dualDelay, "10 Delay Dual");
    expect(service.hasEffectType("reverb", "Spring"), "11 Reverb Spring");
    expect(service.hasEffectType("fx1", "Tremolo"), "12 FX1 Tremolo");
    expect(service.hasEffectType("fx2", "Harmonist"), "13 FX2 Harmonist");
    expect(service.assignCount() == 8, "14 Assign count");
    expect(service.controller("CTL1").scope == DataScope::Patch,
           "15 Assign/controller patch scope");
    expect(service.supportsAssignTarget("Delay Feedback"),
           "16 target existence");
    expect(service.supportsAssignSource("CTL1"), "17 source existence");
    expect(!service.parameterCapability("delay", "Single", "Feedback").name.isEmpty(),
           "18 Target Min/Max value knowledge source");
    expect(!service.assignSourceModes().isEmpty(), "19 Source Mode");
    expect(!service.controller("CTL1").id.isEmpty(), "20 CTL1");
    expect(!service.controller("CTL2").id.isEmpty(), "21 CTL2");
    expect(!service.controller("EXP").id.isEmpty(), "22 EXP");
    expect(!service.controller("EXP SW").id.isEmpty(), "23 EXP SW");
    expect(!service.controller("FOOT 1").id.isEmpty()
           && !service.controller("FOOT 4").id.isEmpty(), "24 Foot 1-4");
    expect(!service.controller("BANK UP").id.isEmpty()
           && !service.controller("BANK DOWN").id.isEmpty(), "25 Bank Up/Down");
    expect(service.controller("CTL1").scope == DataScope::Patch
           && service.controller("FOOT 1").scope == DataScope::System,
           "26 Patch/System separation");
    expect(service.blocksForEffectType("Harmonist").contains("fx2"),
           "27 unqualified effect lookup");
    expect(!service.controller("CTL3").id.isEmpty()
           && !service.controller("CTL4").id.isEmpty()
           && !service.controller("EXP2").id.isEmpty(),
           "28 confirmed external assign sources");

    QTextStream(stdout) << "GT-10 Capability Service tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
