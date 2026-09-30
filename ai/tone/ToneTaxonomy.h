#ifndef AI_TONE_TONETAXONOMY_H
#define AI_TONE_TONETAXONOMY_H

#include <QList>
#include <QStringList>

namespace Ai { namespace Tone {

enum class ToneFamily { Clean, EdgeOfBreakup, Crunch, HighGain, Ambient, Lead };
enum class ToneProfile { Neutral, Worship, Forro, Rock, Metal, NeoSoul, Pop };
enum class ToneModifier {
    Brighter, Darker, Warmer, MoreDefined, LessMuddy, MoreDynamic,
    MoreCompressed, MoreAmbient, LessAmbient, Wider, MoreFocused,
    MoreAggressive, MoreSmooth, MoreSustain, LessGain, MoreGain
};
enum class ToneSpatialGoal { None, Big, Stereo, Modern };
enum class ToneUsageRole { Neutral, Rhythm, Lead, Arpeggio, Ambient };
enum class ToneProblem { None, Muddy, Harsh, Thin, Noisy, NotDefined, NeedsSustain, NeedsWidth };

struct ToneStyleRequest {
    bool recognized = false;
    bool needsClarification = false;
    bool explicitFamily = false;
    bool explicitProfile = false;
    bool wantsValues = false;
    bool wantsDetail = false;
    bool usePatchContext = false;
    bool preserveBody = false;
    ToneFamily family = ToneFamily::Clean;
    ToneProfile profile = ToneProfile::Neutral;
    QList<ToneModifier> modifiers;
    ToneSpatialGoal spatialGoal = ToneSpatialGoal::None;
    ToneUsageRole usageRole = ToneUsageRole::Neutral;
    ToneProblem toneProblem = ToneProblem::None;
    QString focusBlock;
    QString guitarContext;
    QString usageContext;
};

// Closed semantic vocabulary. Technical effect names remain in MidiTable.
class ToneTaxonomy {
public:
    static ToneStyleRequest parse(const QString &text);
    static QString id(ToneFamily value);
    static QString id(ToneProfile value);
    static QString id(ToneModifier value);
    static QString id(ToneUsageRole value);
    static QString id(ToneProblem value);
    static QStringList families();
    static QStringList profiles();
    static QStringList modifiers();
};

} }
#endif
