#include "parameterBar.h"

#include "modernTheme.h"

#include <QColor>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

ParameterBar::ParameterBar(const QString &label, QWidget *parent)
    : QAbstractSlider(parent),
      parameterLabel(label.trimmed().toUpper()),
      presentedValue(QString::fromUtf8("—")),
      parameterAccent(ModernTheme::color(ModernTheme::EditorAccent)),
      centerEnabled(false),
      rawCenterValue(0),
      segmentedMappingEnabled(false),
      segmentedContinuousMaximum(0),
      segmentedSplitRatio(0.5),
      dragging(false)
{
    setObjectName("ParameterBar");
    setOrientation(Qt::Horizontal);
    setSingleStep(1);
    setPageStep(10);
    setTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(220);
    setMinimumHeight(52);
}

void ParameterBar::setDisplayText(const QString &text)
{
    if (presentedValue == text)
        return;
    presentedValue = text;
    update();
}

QString ParameterBar::displayText() const
{
    return presentedValue;
}

void ParameterBar::setAccentColor(const QColor &color)
{
    if (!color.isValid() || parameterAccent == color)
        return;
    parameterAccent = color;
    update();
}

QColor ParameterBar::accentColor() const
{
    return parameterAccent;
}

void ParameterBar::setCenterValue(int rawCenter)
{
    centerEnabled = true;
    rawCenterValue = qBound(minimum(), rawCenter, maximum());
    update();
}

void ParameterBar::clearCenterValue()
{
    if (!centerEnabled)
        return;
    centerEnabled = false;
    update();
}

bool ParameterBar::hasCenterValue() const
{
    return centerEnabled;
}

int ParameterBar::centerValue() const
{
    return rawCenterValue;
}

void ParameterBar::setSegmentedMapping(
    int continuousMaximum, const QVector<int> &discreteRawValues,
    qreal splitRatio)
{
    QVector<int> normalizedValues;
    normalizedValues.reserve(discreteRawValues.size());
    for (int rawValue : discreteRawValues) {
        if (rawValue > continuousMaximum
            && rawValue >= minimum() && rawValue <= maximum()
            && !normalizedValues.contains(rawValue))
            normalizedValues.append(rawValue);
    }

    if (continuousMaximum < minimum()
        || continuousMaximum >= maximum()
        || normalizedValues.isEmpty()
        || splitRatio <= 0.0 || splitRatio >= 1.0) {
        clearSegmentedMapping();
        return;
    }

    segmentedMappingEnabled = true;
    segmentedContinuousMaximum = continuousMaximum;
    segmentedDiscreteValues = normalizedValues;
    segmentedSplitRatio = splitRatio;
    update();
}

void ParameterBar::clearSegmentedMapping()
{
    if (!segmentedMappingEnabled && segmentedDiscreteValues.isEmpty())
        return;
    segmentedMappingEnabled = false;
    segmentedContinuousMaximum = 0;
    segmentedDiscreteValues.clear();
    segmentedSplitRatio = 0.5;
    update();
}

bool ParameterBar::hasSegmentedMapping() const
{
    return segmentedMappingEnabled;
}

QSize ParameterBar::sizeHint() const
{
    return QSize(520, 56);
}

QSize ParameterBar::minimumSizeHint() const
{
    return QSize(220, 52);
}

QRectF ParameterBar::trackRect() const
{
    return QRectF(4.5, 33.5, qMax(1.0, width() - 9.0), 7.0);
}

qreal ParameterBar::positionForValue(int rawValue) const
{
    const QRectF track = trackRect();
    if (maximum() == minimum())
        return track.left();

    if (segmentedMappingEnabled) {
        if (rawValue <= segmentedContinuousMaximum) {
            const int boundedValue = qBound(
                minimum(), rawValue, segmentedContinuousMaximum);
            const qreal continuousRange = qMax(
                1, segmentedContinuousMaximum - minimum());
            const qreal ratio = qreal(boundedValue - minimum())
                / continuousRange;
            return track.left()
                + ratio * segmentedSplitRatio * track.width();
        }

        int discreteIndex = segmentedDiscreteValues.indexOf(rawValue);
        if (discreteIndex < 0) {
            discreteIndex = 0;
            int nearestDistance = qAbs(
                rawValue - segmentedDiscreteValues.first());
            for (int index = 1;
                 index < segmentedDiscreteValues.size(); ++index) {
                const int distance = qAbs(
                    rawValue - segmentedDiscreteValues.at(index));
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    discreteIndex = index;
                }
            }
        }

        const qreal discreteRatio = segmentedDiscreteValues.size() == 1
            ? 0.0
            : qreal(discreteIndex)
                / qreal(segmentedDiscreteValues.size() - 1);
        const qreal visualRatio = segmentedSplitRatio
            + discreteRatio * (1.0 - segmentedSplitRatio);
        return track.left() + visualRatio * track.width();
    }

    const qreal ratio = qreal(qBound(minimum(), rawValue, maximum()) - minimum())
        / qreal(maximum() - minimum());
    return track.left() + ratio * track.width();
}

int ParameterBar::valueForPosition(qreal position) const
{
    const QRectF track = trackRect();
    const qreal ratio = qBound(0.0,
        (position - track.left()) / qMax(1.0, track.width()), 1.0);

    if (segmentedMappingEnabled) {
        if (ratio <= segmentedSplitRatio) {
            const qreal continuousRatio = ratio
                / segmentedSplitRatio;
            const qreal unsnapped = minimum()
                + continuousRatio
                    * (segmentedContinuousMaximum - minimum());
            const int step = qMax(1, singleStep());
            const int stepped = minimum()
                + qRound((unsnapped - minimum()) / step) * step;
            return qBound(minimum(), stepped,
                          segmentedContinuousMaximum);
        }

        const qreal discreteRatio = (ratio - segmentedSplitRatio)
            / (1.0 - segmentedSplitRatio);
        const int discreteIndex = qBound(
            0,
            qRound(discreteRatio
                * (segmentedDiscreteValues.size() - 1)),
            segmentedDiscreteValues.size() - 1);
        return segmentedDiscreteValues.at(discreteIndex);
    }

    const qreal unsnapped = minimum() + ratio * (maximum() - minimum());
    const int step = qMax(1, singleStep());
    const int stepped = minimum()
        + qRound((unsnapped - minimum()) / step) * step;
    return qBound(minimum(), stepped, maximum());
}

int ParameterBar::adjacentValue(int direction) const
{
    const int step = qMax(1, singleStep());
    if (!segmentedMappingEnabled)
        return value() + direction * step;

    if (value() <= segmentedContinuousMaximum) {
        if (direction < 0)
            return qMax(minimum(), value() - step);
        if (value() < segmentedContinuousMaximum)
            return qMin(segmentedContinuousMaximum, value() + step);
        return segmentedDiscreteValues.first();
    }

    int index = segmentedDiscreteValues.indexOf(value());
    if (index < 0) {
        index = 0;
        int nearestDistance = qAbs(
            value() - segmentedDiscreteValues.first());
        for (int candidate = 1;
             candidate < segmentedDiscreteValues.size(); ++candidate) {
            const int distance = qAbs(
                value() - segmentedDiscreteValues.at(candidate));
            if (distance < nearestDistance) {
                nearestDistance = distance;
                index = candidate;
            }
        }
        index += direction;
    } else {
        index += direction;
    }

    if (index < 0)
        return segmentedContinuousMaximum;
    if (index >= segmentedDiscreteValues.size())
        return segmentedDiscreteValues.last();
    return segmentedDiscreteValues.at(index);
}

void ParameterBar::updateFromPosition(qreal position)
{
    setValue(valueForPosition(position));
}

void ParameterBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool enabled = isEnabled();
    QFont labelFont = font();
    labelFont.setPointSizeF(qMax(9.0, labelFont.pointSizeF() - 1.0));
    labelFont.setWeight(QFont::DemiBold);
    painter.setFont(labelFont);
    painter.setPen(QColor(ModernTheme::color(
        enabled ? ModernTheme::PrimaryText : ModernTheme::DisabledText)));
    painter.drawText(QRectF(4, 1, width() - 72, 20),
                     Qt::AlignLeft | Qt::AlignVCenter, parameterLabel);

    QFont valueFont = labelFont;
    valueFont.setWeight(QFont::Bold);
    painter.setFont(valueFont);
    painter.setPen(enabled ? parameterAccent.lighter(116)
        : QColor(ModernTheme::color(ModernTheme::DisabledText)));
    painter.drawText(QRectF(width() - 84, 1, 80, 20),
                     Qt::AlignRight | Qt::AlignVCenter, presentedValue);

    const QRectF track = trackRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(ModernTheme::color(
        ModernTheme::BorderSubtle)));
    painter.drawRoundedRect(track, 3.5, 3.5);

    const qreal valuePosition = positionForValue(value());
    const qreal startPosition = centerEnabled
        ? positionForValue(rawCenterValue) : track.left();
    QRectF fill(qMin(startPosition, valuePosition), track.top(),
                qAbs(valuePosition - startPosition), track.height());
    if (fill.width() > 0.5) {
        painter.setBrush(enabled ? parameterAccent
            : QColor(ModernTheme::color(ModernTheme::DisabledText)));
        painter.drawRoundedRect(fill, 3.5, 3.5);
    }

    if (centerEnabled) {
        const qreal centerPosition = positionForValue(rawCenterValue);
        QColor centerColor(ModernTheme::color(
            enabled ? ModernTheme::SecondaryText
                    : ModernTheme::DisabledText));
        centerColor.setAlpha(enabled ? 210 : 120);
        painter.setPen(QPen(centerColor, 1));
        painter.drawLine(QPointF(centerPosition, track.top() - 3),
                         QPointF(centerPosition, track.bottom() + 3));
    }

    if (segmentedMappingEnabled) {
        const qreal splitPosition = track.left()
            + segmentedSplitRatio * track.width();
        QColor splitColor(ModernTheme::color(
            enabled ? ModernTheme::SecondaryText
                    : ModernTheme::DisabledText));
        splitColor.setAlpha(enabled ? 125 : 80);
        painter.setPen(QPen(splitColor, 1));
        painter.drawLine(QPointF(splitPosition, track.top() - 2),
                         QPointF(splitPosition, track.bottom() + 2));
    }

    QColor indicatorColor(ModernTheme::color(
        enabled ? ModernTheme::PrimaryText : ModernTheme::DisabledText));
    painter.setPen(QPen(indicatorColor, hasFocus() ? 2.0 : 1.4,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(valuePosition, track.top() - 2.5),
                     QPointF(valuePosition, track.bottom() + 2.5));
}

void ParameterBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isEnabled()) {
        QAbstractSlider::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    dragging = true;
    setSliderDown(true);
    updateFromPosition(event->localPos().x());
    event->accept();
}

void ParameterBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging || !(event->buttons() & Qt::LeftButton)) {
        QAbstractSlider::mouseMoveEvent(event);
        return;
    }
    updateFromPosition(event->localPos().x());
    event->accept();
}

void ParameterBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && dragging) {
        updateFromPosition(event->localPos().x());
        dragging = false;
        setSliderDown(false);
        event->accept();
        return;
    }
    QAbstractSlider::mouseReleaseEvent(event);
}

void ParameterBar::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}

void ParameterBar::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        setValue(adjacentValue(-1));
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        setValue(adjacentValue(1));
        break;
    case Qt::Key_Home:
        setValue(minimum());
        break;
    case Qt::Key_End:
        setValue(segmentedMappingEnabled
            ? segmentedDiscreteValues.last() : maximum());
        break;
    default:
        QAbstractSlider::keyPressEvent(event);
        return;
    }
    event->accept();
}
