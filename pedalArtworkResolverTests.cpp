#include "effectArtworkWidget.h"
#include "pedalArtworkResolver.h"

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical() << "FAILED:" << message;
    return condition;
}

QImage renderArtwork(EffectArtworkWidget &widget)
{
    widget.resize(360, 360);
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    bool ok = true;

    PedalArtworkRequest request;
    request.family = PedalArtworkFamily::OverdriveDistortion;
    request.modelRaw = 0x7F;
    request.secondaryRaw = 0x7E;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/pedal_generic.png",
                 "unknown OD/DS raw uses the generic fallback");

    request.family = PedalArtworkFamily::Preamp;
    request.variant = PedalArtworkVariant::ChannelA;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/amp_a.png",
                 "PREAMP A keeps its existing artwork");
    request.variant = PedalArtworkVariant::ChannelB;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/amp_b.png",
                 "PREAMP B keeps its existing artwork");

    request.family = PedalArtworkFamily::PedalFx;
    request.variant = PedalArtworkVariant::FootVolume;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/expression_pedal.png",
                 "P.FX/FV keeps the expression pedal artwork");

    EffectArtworkWidget widget;
    ok &= expect(widget.setArtworkWithFallback(
                     ":/assets/effects/amp_a.png",
                     ":/missing/fallback.png"),
                 "valid specific resource loads");
    const QImage specificImage = renderArtwork(widget);

    ok &= expect(widget.setArtworkWithFallback(
                     ":/missing/specific.png",
                     ":/assets/effects/pedal_generic.png"),
                 "valid fallback loads when the specific resource is missing");
    const QImage fallbackImage = renderArtwork(widget);
    ok &= expect(fallbackImage != specificImage,
                 "fallback visibly replaces the valid specific resource");

    ok &= expect(!widget.setArtworkWithFallback(
                     ":/missing/specific.png",
                     ":/missing/fallback.png"),
                 "two invalid resources report failure");
    ok &= expect(renderArtwork(widget) == fallbackImage,
                 "two invalid resources preserve the previous artwork");

    return ok ? 0 : 1;
}
