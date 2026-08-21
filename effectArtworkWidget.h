#ifndef EFFECTARTWORKWIDGET_H
#define EFFECTARTWORKWIDGET_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

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
    };

    explicit EffectArtworkWidget(QWidget *parent = nullptr);

    bool setArtwork(const QString &resourcePath);
    void setTextOverlay(const QString &id,
                        const QRectF &normalizedRect,
                        const QString &text,
                        const QFont &font,
                        const QColor &color,
                        Qt::Alignment alignment = Qt::AlignCenter,
                        qreal relativeFontHeight = 0.62);
    void setTextOverlayText(const QString &id, const QString &text);
    void removeOverlay(const QString &id);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScaledArtwork();

    QPixmap sourceArtwork;
    QPixmap scaledArtwork;
    QList<TextOverlay> textOverlays;
};

#endif
