#include "effectArtworkWidget.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QtMath>

namespace {
const QSizeF kMaximumArtworkViewport(400.0, 400.0);

QSizeF responsiveArtworkViewport(const QSizeF &availableSize)
{
    if (availableSize.isEmpty())
        return QSizeF();

    const qreal scale = qMin(
        1.0,
        qMin(availableSize.width() / kMaximumArtworkViewport.width(),
             availableSize.height() / kMaximumArtworkViewport.height()));
    return kMaximumArtworkViewport * qMax(0.0, scale);
}
}

EffectArtworkWidget::EffectArtworkWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

bool EffectArtworkWidget::setArtwork(const QString &resourcePath)
{
    const QPixmap artwork(resourcePath);
    if (artwork.isNull()) {
        sourceArtwork = QPixmap();
        scaledArtwork = QPixmap();
        update();
        return false;
    }

    sourceArtwork = artwork;
    updateScaledArtwork();
    return true;
}

void EffectArtworkWidget::setGenericPedalIdentity(
    const QString &effectName, const QColor &nameColor,
    const QColor &accentColor)
{
    genericPedal = true;
    genericPedalAccent = accentColor;
    QFont nameFont = font();
    nameFont.setWeight(QFont::DemiBold);
    nameFont.setLetterSpacing(QFont::PercentageSpacing, 106.0);
    setTextOverlay(
        "name", QRectF(0.225, 0.046, 0.55, 0.070), effectName.toUpper(),
        nameFont, nameColor, Qt::AlignCenter, 0.42);

    QFont typeFont = font();
    typeFont.setWeight(QFont::DemiBold);
    typeFont.setStretch(QFont::Condensed);
    setTextOverlay(
        "type", QRectF(0.245, 0.184, 0.51, 0.068), QString(),
        typeFont, accentColor, Qt::AlignCenter, 0.48);
}

void EffectArtworkWidget::setGenericPedalState(bool available, bool enabled)
{
    if (genericPedalAvailable == available && genericPedalEnabled == enabled)
        return;
    genericPedalAvailable = available;
    genericPedalEnabled = enabled;
    update();
}

void EffectArtworkWidget::setGenericPedalVisualIntensity(qreal intensity)
{
    const qreal bounded = qBound(0.0, intensity, 1.0);
    if (qFuzzyCompare(genericPedalVisualIntensity, bounded))
        return;
    genericPedalVisualIntensity = bounded;
    update();
}

void EffectArtworkWidget::setGenericExpressionIdentity(
    const QString &effectName, const QColor &nameColor,
    const QColor &accentColor)
{
    genericExpression = true;
    genericPedalAccent = accentColor;
    QFont nameFont = font();
    nameFont.setWeight(QFont::DemiBold);
    nameFont.setLetterSpacing(QFont::PercentageSpacing, 104.0);
    setTextOverlay(
        "name", QRectF(0.125, 0.055, 0.205, 0.055), effectName.toUpper(),
        nameFont, nameColor, Qt::AlignCenter, 0.34);

    QFont typeFont = font();
    typeFont.setWeight(QFont::DemiBold);
    typeFont.setStretch(QFont::Condensed);
    setTextOverlay(
        "type", QRectF(0.170, 0.285, 0.120, 0.110), QString(),
        typeFont, accentColor, Qt::AlignCenter, 0.18);
}

void EffectArtworkWidget::setGenericExpressionState(bool available,
                                                      bool enabled)
{
    setGenericPedalState(available, enabled);
}

void EffectArtworkWidget::setTextOverlay(const QString &id,
                                         const QRectF &normalizedRect,
                                         const QString &text,
                                         const QFont &font,
                                         const QColor &color,
                                         Qt::Alignment alignment,
                                         qreal relativeFontHeight)
{
    for (TextOverlay &overlay : textOverlays) {
        if (overlay.id != id)
            continue;
        overlay.normalizedRect = normalizedRect;
        overlay.text = text;
        overlay.alignment = alignment;
        overlay.font = font;
        overlay.color = color;
        overlay.relativeFontHeight = relativeFontHeight;
        update();
        return;
    }

    TextOverlay overlay;
    overlay.id = id;
    overlay.normalizedRect = normalizedRect;
    overlay.text = text;
    overlay.alignment = alignment;
    overlay.font = font;
    overlay.color = color;
    overlay.relativeFontHeight = relativeFontHeight;
    textOverlays.append(overlay);
    update();
}

void EffectArtworkWidget::setTextOverlayText(const QString &id,
                                             const QString &text)
{
    for (TextOverlay &overlay : textOverlays) {
        if (overlay.id == id) {
            overlay.text = text;
            update();
            return;
        }
    }
}

void EffectArtworkWidget::setTextOverlayRotation(const QString &id,
                                                  qreal degrees)
{
    for (TextOverlay &overlay : textOverlays) {
        if (overlay.id == id) {
            overlay.rotationDegrees = degrees;
            update();
            return;
        }
    }
}

void EffectArtworkWidget::removeOverlay(const QString &id)
{
    for (int i = textOverlays.size() - 1; i >= 0; --i) {
        if (textOverlays.at(i).id == id)
            textOverlays.removeAt(i);
    }
    update();
}

QSize EffectArtworkWidget::sizeHint() const
{
    return QSize(360, 360);
}

QSize EffectArtworkWidget::minimumSizeHint() const
{
    return QSize(200, 200);
}

bool EffectArtworkWidget::event(QEvent *event)
{
    const bool screenChanged = event->type() == QEvent::ScreenChangeInternal;
    const bool handled = QWidget::event(event);

    if (screenChanged)
        updateScaledArtwork();

    return handled;
}

void EffectArtworkWidget::paintEvent(QPaintEvent *)
{
    if (scaledArtwork.isNull())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const qreal dpr = scaledArtwork.devicePixelRatio();
    const QSizeF logicalSize(scaledArtwork.width() / dpr,
                             scaledArtwork.height() / dpr);
    const QPointF topLeft((width() - logicalSize.width()) / 2.0,
                          (height() - logicalSize.height()) / 2.0);
    painter.drawPixmap(topLeft, scaledArtwork);

    const QRectF artworkRect(topLeft, logicalSize);
    if (genericPedal)
        paintGenericPedalAccents(painter, artworkRect);
    if (genericExpression)
        paintGenericExpressionAccents(painter, artworkRect);

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (const TextOverlay &overlay : textOverlays) {
        if (overlay.text.isEmpty())
            continue;
        const QRectF area(
            topLeft.x() + overlay.normalizedRect.x() * logicalSize.width(),
            topLeft.y() + overlay.normalizedRect.y() * logicalSize.height(),
            overlay.normalizedRect.width() * logicalSize.width(),
            overlay.normalizedRect.height() * logicalSize.height());
        painter.save();
        QRectF drawArea = area;
        if (!qFuzzyIsNull(overlay.rotationDegrees)) {
            painter.translate(area.center());
            painter.rotate(overlay.rotationDegrees);
            drawArea = QRectF(-area.height() / 2.0, -area.width() / 2.0,
                              area.height(), area.width());
        }
        QFont font = overlay.font;
        font.setPixelSize(qMax(1, qRound(
            area.height() * overlay.relativeFontHeight)));
        painter.setFont(font);
        painter.setPen(overlay.color);
        painter.drawText(drawArea, overlay.alignment, overlay.text);
        painter.restore();
    }
}

void EffectArtworkWidget::paintGenericExpressionAccents(
    QPainter &painter, const QRectF &artworkRect) const
{
    if (!genericPedalAccent.isValid() || artworkRect.isEmpty())
        return;

    const qreal intensity = (!genericPedalAvailable
        ? 0.12 : (genericPedalEnabled ? 1.0 : 0.28))
        * genericPedalVisualIntensity;
    const qreal width = artworkRect.width();
    const auto point = [&artworkRect](qreal x, qreal y) {
        return QPointF(artworkRect.left() + x * artworkRect.width(),
                       artworkRect.top() + y * artworkRect.height());
    };

    QPainterPath displayFrame;
    displayFrame.moveTo(point(0.190, 0.270));
    displayFrame.lineTo(point(0.273, 0.270));
    displayFrame.lineTo(point(0.292, 0.283));
    displayFrame.lineTo(point(0.292, 0.407));
    displayFrame.lineTo(point(0.273, 0.420));
    displayFrame.lineTo(point(0.190, 0.420));
    displayFrame.lineTo(point(0.171, 0.407));
    displayFrame.lineTo(point(0.171, 0.283));
    displayFrame.closeSubpath();

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor glow = genericPedalAccent;
    glow.setAlphaF(qBound(0.0, 0.42 * intensity, 1.0));
    const qreal strokeScale = 0.65
        + 0.35 * genericPedalVisualIntensity;
    painter.setPen(QPen(glow, qMax(1.5, width * 0.024 * strokeScale),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(displayFrame);
    QColor core = genericPedalAccent;
    core.setAlphaF(qBound(0.0, intensity, 1.0));
    painter.setPen(QPen(core, qMax(1.0, width * 0.008 * strokeScale),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(displayFrame);

    const QPointF ledCenter = point(0.229, 0.166);
    const qreal ledRadius = width * 0.021;
    QColor ledGlow = genericPedalAccent;
    ledGlow.setAlphaF(qBound(0.0, 0.52 * intensity, 1.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(ledGlow);
    const qreal glowScale = 1.55
        + 0.60 * genericPedalVisualIntensity;
    painter.drawEllipse(ledCenter, ledRadius * glowScale,
                        ledRadius * glowScale);
    QColor ledCore = genericPedalAccent;
    ledCore.setAlphaF(qBound(0.0, intensity, 1.0));
    painter.setBrush(ledCore);
    painter.drawEllipse(ledCenter, ledRadius, ledRadius);
    painter.restore();
}

void EffectArtworkWidget::paintGenericPedalAccents(
    QPainter &painter, const QRectF &artworkRect) const
{
    if (!genericPedalAccent.isValid() || artworkRect.isEmpty())
        return;

    const qreal intensity = (!genericPedalAvailable
        ? 0.12 : (genericPedalEnabled ? 1.0 : 0.28))
        * genericPedalVisualIntensity;
    const qreal width = artworkRect.width();
    const auto point = [&artworkRect](qreal x, qreal y) {
        return QPointF(artworkRect.left() + x * artworkRect.width(),
                       artworkRect.top() + y * artworkRect.height());
    };
    const auto linePath = [&point](std::initializer_list<QPointF> points) {
        QPainterPath path;
        bool first = true;
        for (const QPointF &normalized : points) {
            const QPointF mapped = point(normalized.x(), normalized.y());
            if (first) {
                path.moveTo(mapped);
                first = false;
            } else {
                path.lineTo(mapped);
            }
        }
        return path;
    };

    QList<QPainterPath> paths;
    QPainterPath typeFrame = linePath({
        QPointF(0.264, 0.180), QPointF(0.736, 0.180),
        QPointF(0.758, 0.196), QPointF(0.758, 0.237),
        QPointF(0.736, 0.253), QPointF(0.264, 0.253),
        QPointF(0.242, 0.237), QPointF(0.242, 0.196),
        QPointF(0.264, 0.180)});
    paths << typeFrame;
    paths << linePath({QPointF(0.108, 0.286), QPointF(0.108, 0.178),
                       QPointF(0.160, 0.137)});
    paths << linePath({QPointF(0.892, 0.286), QPointF(0.892, 0.178),
                       QPointF(0.840, 0.137)});
    paths << linePath({QPointF(0.327, 0.164), QPointF(0.390, 0.164)});
    paths << linePath({QPointF(0.610, 0.164), QPointF(0.673, 0.164)});
    paths << linePath({QPointF(0.107, 0.363), QPointF(0.151, 0.401),
                       QPointF(0.202, 0.600)});
    paths << linePath({QPointF(0.893, 0.363), QPointF(0.849, 0.401),
                       QPointF(0.798, 0.600)});
    paths << linePath({QPointF(0.103, 0.718), QPointF(0.103, 0.858)});
    paths << linePath({QPointF(0.897, 0.718), QPointF(0.897, 0.858)});
    paths << linePath({QPointF(0.157, 0.758), QPointF(0.268, 0.807)});
    paths << linePath({QPointF(0.843, 0.758), QPointF(0.732, 0.807)});

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor glow = genericPedalAccent;
    glow.setAlphaF(qBound(0.0, 0.42 * intensity, 1.0));
    const qreal strokeScale = 0.65
        + 0.35 * genericPedalVisualIntensity;
    painter.setPen(QPen(glow, qMax(1.5, width * 0.024 * strokeScale),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QPainterPath &path : paths)
        painter.drawPath(path);

    QColor core = genericPedalAccent;
    core.setAlphaF(qBound(0.0, 1.0 * intensity, 1.0));
    painter.setPen(QPen(core, qMax(1.0, width * 0.008 * strokeScale),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QPainterPath &path : paths)
        painter.drawPath(path);

    const QPointF ledCenter = point(0.5, 0.668);
    const qreal ledRadius = width * 0.024;
    QColor ledGlow = genericPedalAccent;
    ledGlow.setAlphaF(qBound(0.0, 0.52 * intensity, 1.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(ledGlow);
    const qreal glowScale = 1.55
        + 0.60 * genericPedalVisualIntensity;
    painter.drawEllipse(ledCenter, ledRadius * glowScale,
                        ledRadius * glowScale);
    QColor ledCore = genericPedalAccent;
    ledCore.setAlphaF(qBound(0.0, 1.0 * intensity, 1.0));
    painter.setBrush(ledCore);
    painter.drawEllipse(ledCenter, ledRadius, ledRadius);
    QColor highlight(242, 244, 246);
    highlight.setAlphaF(qBound(0.0, 0.92 * intensity, 1.0));
    painter.setBrush(highlight);
    painter.drawEllipse(point(0.496, 0.664), ledRadius * 0.34,
                        ledRadius * 0.34);
    painter.restore();
}

void EffectArtworkWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScaledArtwork();
}

void EffectArtworkWidget::updateScaledArtwork()
{
    if (sourceArtwork.isNull() || size().isEmpty()) {
        scaledArtwork = QPixmap();
    } else {
        const qreal dpr = devicePixelRatioF();
        const QSizeF logicalViewport = responsiveArtworkViewport(size());
        const QSizeF physicalViewport = logicalViewport * dpr;
        const qreal sourceScale = qMin(
            physicalViewport.width() / sourceArtwork.width(),
            physicalViewport.height() / sourceArtwork.height());
        const QSize physicalSize(
            qMax(1, qRound(sourceArtwork.width() * sourceScale)),
            qMax(1, qRound(sourceArtwork.height() * sourceScale)));
        scaledArtwork = sourceArtwork.scaled(
            physicalSize, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        scaledArtwork.setDevicePixelRatio(dpr);
    }
    update();
}
