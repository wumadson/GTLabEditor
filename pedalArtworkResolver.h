#ifndef PEDALARTWORKRESOLVER_H
#define PEDALARTWORKRESOLVER_H

#include <QString>

enum class PedalArtworkFamily
{
    Compressor,
    OverdriveDistortion,
    Preamp,
    Fx,
    PedalFx,
    SendReturn,
    Chorus,
    Delay,
    Reverb,
    NoiseSuppressor
};

enum class PedalArtworkVariant
{
    Default,
    ChannelA,
    ChannelB,
    Fx1,
    Fx2,
    Ns1,
    Ns2,
    FootVolume
};

struct PedalArtworkRequest
{
    PedalArtworkFamily family = PedalArtworkFamily::Compressor;
    int modelRaw = -1;
    int secondaryRaw = -1;
    PedalArtworkVariant variant = PedalArtworkVariant::Default;
};

class PedalArtworkResolver
{
public:
    static QString resolve(const PedalArtworkRequest &request);
    static QString fallback(const PedalArtworkRequest &request);
};

#endif
