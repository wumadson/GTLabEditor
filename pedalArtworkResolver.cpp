#include "pedalArtworkResolver.h"

namespace {
const char kGenericPedalArtwork[] =
    ":/assets/effects/pedal_generic.png";
const char kPreampAArtwork[] = ":/assets/effects/amp_a.png";
const char kPreampBArtwork[] = ":/assets/effects/amp_b.png";
const char kExpressionPedalArtwork[] =
    ":/assets/pedals/expression_pedal.png";
const char kCompressorArtwork[] =
    ":/assets/pedals/comp/00_compressor.png";
const char kLimiterArtwork[] =
    ":/assets/pedals/comp/01_limiter.png";
const char *const kReverbArtwork[] = {
    ":/assets/pedals/reverb/00_ambience.png",
    ":/assets/pedals/reverb/01_room.png",
    ":/assets/pedals/reverb/02_hall_1.png",
    ":/assets/pedals/reverb/03_hall_2.png",
    ":/assets/pedals/reverb/04_plate.png",
    ":/assets/pedals/reverb/05_spring.png",
    ":/assets/pedals/reverb/06_modulate.png"
};
const char *const kChorusArtwork[] = {
    ":/assets/pedals/chorus/00_mono.png",
    ":/assets/pedals/chorus/01_stereo_1.png",
    ":/assets/pedals/chorus/02_stereo_2.png"
};
}

QString PedalArtworkResolver::resolve(const PedalArtworkRequest &request)
{
    if (request.family == PedalArtworkFamily::Compressor) {
        if (request.modelRaw == 0x00)
            return QString::fromLatin1(kCompressorArtwork);
        if (request.modelRaw == 0x01)
            return QString::fromLatin1(kLimiterArtwork);
    }

    if (request.family == PedalArtworkFamily::Reverb
        && request.modelRaw >= 0x00 && request.modelRaw <= 0x06) {
        return QString::fromLatin1(kReverbArtwork[request.modelRaw]);
    }

    if (request.family == PedalArtworkFamily::Chorus
        && request.modelRaw >= 0x00 && request.modelRaw <= 0x02) {
        return QString::fromLatin1(kChorusArtwork[request.modelRaw]);
    }

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
