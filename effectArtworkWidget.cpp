#include "effectArtworkWidget.h"

#include <QEvent>
#include <QPainter>
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

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (const TextOverlay &overlay : textOverlays) {
        if (overlay.text.isEmpty())
            continue;
        const QRectF area(
            topLeft.x() + overlay.normalizedRect.x() * logicalSize.width(),
            topLeft.y() + overlay.normalizedRect.y() * logicalSize.height(),
            overlay.normalizedRect.width() * logicalSize.width(),
            overlay.normalizedRect.height() * logicalSize.height());
        QFont font = overlay.font;
        font.setPixelSize(qMax(1, qRound(
            area.height() * overlay.relativeFontHeight)));
        painter.setFont(font);
        painter.setPen(overlay.color);
        painter.drawText(area, overlay.alignment, overlay.text);
    }
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
