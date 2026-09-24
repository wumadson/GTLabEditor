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
const char *const kDelayArtwork[] = {
    ":/assets/pedals/delay/00_single.png",
    ":/assets/pedals/delay/01_pan.png",
    ":/assets/pedals/delay/02_stereo.png",
    ":/assets/pedals/delay/03_dual_series.png",
    ":/assets/pedals/delay/04_dual_parallel.png",
    ":/assets/pedals/delay/05_dual_lr.png",
    ":/assets/pedals/delay/06_reverse.png",
    ":/assets/pedals/delay/07_analog.png",
    ":/assets/pedals/delay/08_tape.png",
    ":/assets/pedals/delay/09_warp.png",
    ":/assets/pedals/delay/0a_modulate.png"
};
const char kNoiseSuppressor1Artwork[] =
    ":/assets/pedals/ns/ns1.png";
const char kNoiseSuppressor2Artwork[] =
    ":/assets/pedals/ns/ns2.png";
const char kSendReturnArtwork[] =
    ":/assets/pedals/sr/send_return.png";
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

    if (request.family == PedalArtworkFamily::Delay
        && request.modelRaw >= 0x00 && request.modelRaw <= 0x0A) {
        return QString::fromLatin1(kDelayArtwork[request.modelRaw]);
    }

    if (request.family == PedalArtworkFamily::NoiseSuppressor) {
        if (request.variant == PedalArtworkVariant::Ns1)
            return QString::fromLatin1(kNoiseSuppressor1Artwork);
        if (request.variant == PedalArtworkVariant::Ns2)
            return QString::fromLatin1(kNoiseSuppressor2Artwork);
    }

    if (request.family == PedalArtworkFamily::SendReturn
        && request.variant == PedalArtworkVariant::Default) {
        return QString::fromLatin1(kSendReturnArtwork);
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
