#ifndef PARAMETERBAR_H
#define PARAMETERBAR_H

#include <QAbstractSlider>
#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class ParameterBar : public QAbstractSlider
{
    Q_OBJECT

public:
    explicit ParameterBar(const QString &label, QWidget *parent = nullptr);

    void setDisplayText(const QString &text);
    QString displayText() const;
    void setAccentColor(const QColor &color);
    QColor accentColor() const;
    void setCenterValue(int rawCenter);
    void clearCenterValue();
    bool hasCenterValue() const;
    int centerValue() const;
    void setSegmentedMapping(int continuousMaximum,
                             const QVector<int> &discreteRawValues,
                             qreal splitRatio = 0.5);
    void clearSegmentedMapping();
    bool hasSegmentedMapping() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRectF trackRect() const;
    qreal positionForValue(int rawValue) const;
    int valueForPosition(qreal position) const;
    int adjacentValue(int direction) const;
    void updateFromPosition(qreal position);

    QString parameterLabel;
    QString presentedValue;
    QColor parameterAccent;
    bool centerEnabled;
    int rawCenterValue;
    bool segmentedMappingEnabled;
    int segmentedContinuousMaximum;
    QVector<int> segmentedDiscreteValues;
    qreal segmentedSplitRatio;
    bool dragging;
};

#endif
