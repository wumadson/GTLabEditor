#ifndef EFFECTARTWORKWIDGET_H
#define EFFECTARTWORKWIDGET_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class QPainter;

class EffectArtworkWidget : public QWidget
{
public:
    struct TextOverlay {
        QString id;
        QRectF normalizedRect;
        QString text;
        Qt::Alignment alignment;
        QFont font;
        QColor color;
        qreal relativeFontHeight;
        qreal rotationDegrees = 0.0;
    };

    explicit EffectArtworkWidget(QWidget *parent = nullptr);

    bool setArtwork(const QString &resourcePath);
    void setGenericPedalIdentity(const QString &effectName,
                                 const QColor &nameColor,
                                 const QColor &accentColor);
    void setGenericPedalState(bool available, bool enabled);
    void setGenericPedalVisualIntensity(qreal intensity);
    void setGenericExpressionIdentity(const QString &effectName,
                                      const QColor &nameColor,
                                      const QColor &accentColor);
    void setGenericExpressionState(bool available, bool enabled);
    void setTextOverlay(const QString &id,
                        const QRectF &normalizedRect,
                        const QString &text,
                        const QFont &font,
                        const QColor &color,
                        Qt::Alignment alignment = Qt::AlignCenter,
                        qreal relativeFontHeight = 0.62);
    void setTextOverlayText(const QString &id, const QString &text);
    void setTextOverlayRotation(const QString &id, qreal degrees);
    void removeOverlay(const QString &id);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void paintGenericPedalAccents(QPainter &painter,
                                  const QRectF &artworkRect) const;
    void paintGenericExpressionAccents(QPainter &painter,
                                       const QRectF &artworkRect) const;
    void updateScaledArtwork();

    QPixmap sourceArtwork;
    QPixmap scaledArtwork;
    QList<TextOverlay> textOverlays;
    QColor genericPedalAccent;
    bool genericPedal = false;
    bool genericExpression = false;
    bool genericPedalAvailable = false;
    bool genericPedalEnabled = false;
    qreal genericPedalVisualIntensity = 1.0;
};

#endif
