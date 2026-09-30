#include "ai/tone/ToneIntentParser.h"
#include "ai/tone/ToneTaxonomy.h"

#include <QRegularExpression>

namespace Ai {
namespace Tone {
namespace {

bool containsAny(const QString &text, std::initializer_list<const char *> values,
                 QString *matched = nullptr)
{
    for (const char *value : values) {
        const QString candidate = QString::fromLatin1(value);
        if (text.contains(candidate)) {
            if (matched)
                *matched = candidate;
            return true;
        }
    }
    return false;
}

} // namespace

QString ToneIntentParser::normalize(const QString &text)
{
    QString result = text.normalized(QString::NormalizationForm_D).toLower();
    result.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9/]+")),
                   QStringLiteral(" "));
    return result.simplified();
}

ToneRequest ToneIntentParser::parse(const QString &text) const
{
    ToneRequest request;
    const QString normalized = normalize(text);
    if (normalized.isEmpty())
        return request;

    if (containsAny(normalized, {"mateus asato", "andy timmons", "sound like an artist",
                                 "clone artist", "clonar artista"})) {
        request.kind = ToneRequestKind::ArtistProfileUnavailable;
        return request;
    }

    if (containsAny(normalized, {"shimmer", "cloud reverb",
                                 "neural capture"})) {
        request.kind = ToneRequestKind::UnsupportedCapability;
        return request;
    }

    if (ToneTaxonomy::parse(text).recognized) {
        request.kind = ToneRequestKind::BuildTone;
        return request;
    }

    if (containsAny(normalized, {
            "me explique esse preset", "explique esse preset",
            "explique esse timbre", "como esse preset tende a soar",
            "como esse timbre tende a soar", "qual e a proposta desse preset",
            "qual a proposta desse timbre", "what does this patch sound like",
            "explain this tone", "explain this patch",
            "por que esse preset soa", "por que meu timbre soa",
            "por que meu reverb soa", "por que o reverb soa",
            "por que meu delay", "por que o delay soa",
            "o que esta deixando esse preset"})) {
        request.kind = ToneRequestKind::ExplainTone;
        return request;
    }

    struct Mapping { ToneDirection direction; QStringList terms; };
    const Mapping mappings[] = {
        {ToneDirection::LessMuddy, {"menos embolado", "less muddy", "less boomy"}},
        {ToneDirection::MoreDefined, {"mais definicao", "more definition", "more defined", "more clarity"}},
        {ToneDirection::MoreCrunch, {"mais crunch", "more crunch", "mais drive", "more drive"}},
        {ToneDirection::MoreClean, {"mais limpo", "mais clean", "cleaner", "more clean"}},
        {ToneDirection::MoreAmbient, {"mais ambiente", "mais ambiencia", "mais ambient", "more ambient", "more ambience"}},
        {ToneDirection::MoreDeep, {"mais profundo", "mais profundidade", "more depth", "deeper"}},
        {ToneDirection::MoreBright, {"mais brilhante", "mais brilho", "brighter", "more bright"}},
        {ToneDirection::MoreDark, {"mais escuro", "darker", "more dark"}},
        {ToneDirection::MoreWide, {"mais aberto", "mais amplo", "more open", "wider", "more wide"}},
        {ToneDirection::MoreFocused, {"mais focado", "more focused", "narrower"}},
        {ToneDirection::MoreAggressive, {"mais agressivo", "more aggressive"}},
        {ToneDirection::MoreSmooth, {"mais suave", "smoother", "more smooth"}},
        {ToneDirection::MoreDynamic, {"mais dinamico", "more dynamic"}},
        {ToneDirection::MoreCompressed, {"mais comprimido", "more compressed"}}
    };
    for (const Mapping &mapping : mappings) {
        QString matched;
        bool found = false;
        for (const QString &term : mapping.terms) {
            if (normalized.contains(term)) {
                matched = term;
                found = true;
                break;
            }
        }
        if (!found)
            continue;
        request.kind = ToneRequestKind::AdjustTone;
        request.adjustment.direction = mapping.direction;
        request.adjustment.matchedText = matched;
        if (containsAny(normalized, {"muito", "bem mais", "much more", "a lot"}))
            request.adjustment.magnitude = ChangeMagnitude::Moderate;
        else
            request.adjustment.magnitude = ChangeMagnitude::Small;
        return request;
    }
    return request;
}

} // namespace Tone
} // namespace Ai
