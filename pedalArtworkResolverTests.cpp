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

QRect visibleBounds(const QImage &source, int alphaThreshold = 16)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(
            image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) < alphaThreshold)
                continue;
            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }
    if (maxX < minX || maxY < minY)
        return QRect();
    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    bool ok = true;

    PedalArtworkRequest request;
    request.family = PedalArtworkFamily::Compressor;
    request.modelRaw = 0x00;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/comp/00_compressor.png",
                 "compressor raw 00 uses its specific artwork");
    request.modelRaw = 0x01;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/comp/01_limiter.png",
                 "compressor raw 01 uses its specific artwork");
    request.modelRaw = 0x7F;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/pedal_generic.png",
                 "unknown compressor raw uses the generic fallback");

    request.family = PedalArtworkFamily::Reverb;
    request.modelRaw = 0x00;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/00_ambience.png",
                 "reverb raw 00 uses the ambience artwork");
    request.modelRaw = 0x01;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/01_room.png",
                 "reverb raw 01 uses the room artwork");
    request.modelRaw = 0x02;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/02_hall_1.png",
                 "reverb raw 02 uses the hall 1 artwork");
    request.modelRaw = 0x03;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/03_hall_2.png",
                 "reverb raw 03 uses the hall 2 artwork");
    request.modelRaw = 0x04;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/04_plate.png",
                 "reverb raw 04 uses the plate artwork");
    request.modelRaw = 0x05;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/05_spring.png",
                 "reverb raw 05 uses the spring artwork");
    request.modelRaw = 0x06;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/reverb/06_modulate.png",
                 "reverb raw 06 uses the modulate artwork");
    request.modelRaw = 0x7F;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/pedal_generic.png",
                 "unknown reverb raw uses the generic fallback");
    for (int raw = 0x00; raw <= 0x06; ++raw) {
        request.modelRaw = raw;
        ok &= expect(!QImage(PedalArtworkResolver::resolve(request)).isNull(),
                     "reverb specific resource loads through QRC");
    }

    request.family = PedalArtworkFamily::Chorus;
    request.modelRaw = 0x00;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/chorus/00_mono.png",
                 "chorus raw 00 uses the mono artwork");
    request.modelRaw = 0x01;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/chorus/01_stereo_1.png",
                 "chorus raw 01 uses the stereo 1 artwork");
    request.modelRaw = 0x02;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/pedals/chorus/02_stereo_2.png",
                 "chorus raw 02 uses the stereo 2 artwork");
    request.modelRaw = 0x7F;
    ok &= expect(PedalArtworkResolver::resolve(request)
                     == ":/assets/effects/pedal_generic.png",
                 "unknown chorus raw uses the generic fallback");
    for (int raw = 0x00; raw <= 0x02; ++raw) {
        request.modelRaw = raw;
        ok &= expect(!QImage(PedalArtworkResolver::resolve(request)).isNull(),
                     "chorus specific resource loads through QRC");
    }

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
    bool usedFallback = true;
    ok &= expect(widget.setArtworkWithFallback(
                     ":/assets/pedals/comp/00_compressor.png",
                     ":/assets/effects/pedal_generic.png", &usedFallback,
                     true),
                 "compressor specific resource loads");
    ok &= expect(!usedFallback,
                 "compressor does not use the generic fallback");
    const QImage compressorImage = renderArtwork(widget);
    const QRect compressorBounds = visibleBounds(compressorImage);

    usedFallback = true;
    ok &= expect(widget.setArtworkWithFallback(
                     ":/assets/pedals/comp/01_limiter.png",
                     ":/assets/effects/pedal_generic.png", &usedFallback,
                     true),
                 "limiter specific resource loads");
    ok &= expect(!usedFallback,
                 "limiter does not use the generic fallback");
    const QImage limiterImage = renderArtwork(widget);
    const QRect limiterBounds = visibleBounds(limiterImage);
    ok &= expect(limiterImage != compressorImage,
                 "compressor and limiter render distinct artworks");
    ok &= expect(qAbs(compressorBounds.height() - limiterBounds.height()) <= 1,
                 "specific stompboxes have matching visible heights");

    usedFallback = false;
    ok &= expect(widget.setArtworkWithFallback(
                     ":/missing/specific.png",
                     ":/assets/effects/pedal_generic.png", &usedFallback,
                     true),
                 "valid fallback loads when the specific resource is missing");
    ok &= expect(usedFallback,
                 "fallback load is reported to the caller");
    const QImage fallbackImage = renderArtwork(widget);
    ok &= expect(fallbackImage != limiterImage,
                 "fallback visibly replaces the valid specific resource");

    EffectArtworkWidget genericWidget;
    ok &= expect(genericWidget.setArtwork(
                     ":/assets/effects/pedal_generic.png"),
                 "generic artwork loads through the legacy path");
    ok &= expect(fallbackImage == renderArtwork(genericWidget),
                 "specific-art normalization leaves fallback rendering unchanged");

    ok &= expect(!widget.setArtworkWithFallback(
                     ":/missing/specific.png",
                     ":/missing/fallback.png"),
                 "two invalid resources report failure");
    ok &= expect(renderArtwork(widget) == fallbackImage,
                 "two invalid resources preserve the previous artwork");

    return ok ? 0 : 1;
}
