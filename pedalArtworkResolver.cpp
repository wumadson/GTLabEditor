#include "pedalArtworkResolver.h"

namespace {
const char kGenericPedalArtwork[] =
    ":/assets/effects/pedal_generic.png";
const char kPreampAArtwork[] = ":/assets/effects/amp_a.png";
const char kPreampBArtwork[] = ":/assets/effects/amp_b.png";
const char kExpressionPedalArtwork[] =
    ":/assets/pedals/expression_pedal.png";
}

QString PedalArtworkResolver::resolve(const PedalArtworkRequest &request)
{
    // Phase 1 deliberately resolves every request to its existing fallback.
    // modelRaw and secondaryRaw remain part of the request so model-specific
    // artwork can be introduced later without changing editor call sites.
    return fallback(request);
}

QString PedalArtworkResolver::fallback(const PedalArtworkRequest &request)
{
    if (request.family == PedalArtworkFamily::Preamp) {
        if (request.variant == PedalArtworkVariant::ChannelA)
            return QString::fromLatin1(kPreampAArtwork);
        if (request.variant == PedalArtworkVariant::ChannelB)
            return QString::fromLatin1(kPreampBArtwork);
    }

    if (request.family == PedalArtworkFamily::PedalFx)
        return QString::fromLatin1(kExpressionPedalArtwork);

    return QString::fromLatin1(kGenericPedalArtwork);
}
