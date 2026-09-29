#ifndef AI_TONE_TONEINTENTPARSER_H
#define AI_TONE_TONEINTENTPARSER_H

#include "ai/tone/ToneModels.h"

#include <QString>

namespace Ai {
namespace Tone {

class ToneIntentParser
{
public:
    ToneRequest parse(const QString &text) const;
    static QString normalize(const QString &text);
};

} // namespace Tone
} // namespace Ai

#endif
