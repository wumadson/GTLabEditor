#include "modernEqGraph.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <cmath>

namespace {
const QColor kAccent("#368FD6");
const QColor kGrid("#343A42");
const QColor kZeroLine("#697684");
const QColor kText("#B5BDC6");
const qreal kMinimumFrequency = 20.0;
const qreal kMaximumFrequency = 20000.0;

QString frequencyLabel(qreal frequencyHz)
{
    if (frequencyHz >= 1000.0) {
        const qreal khz = frequencyHz / 1000.0;
        return qFuzzyCompare(khz, qRound(khz))
            ? QString::number(qRound(khz)) + "k"
            : QString::number(khz, 'f', 1) + "k";
    }
    return QString::number(qRound(frequencyHz));
}
}

ModernEqGraph::ModernEqGraph(QWidget *parent)
    : QWidget(parent),
      eqActive(false),
      lowGainDb(0.0),
      lowMidFrequencyHz(500.0),
      lowMidGainDb(0.0),
      lowMidQ(1.0),
      highMidFrequencyHz(2000.0),
      highMidGainDb(0.0),
      highMidQ(1.0),
      highGainDb(0.0),
      lowCutActive(false),
      lowCutFrequencyHz(kMinimumFrequency),
      highCutActive(false),
      highCutFrequencyHz(kMaximumFrequency)
{
    setObjectName("ModernEqGraph");
    setMinimumHeight(125);
    setMaximumHeight(135);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ModernEqGraph::setEqActive(bool active)
{
    if (eqActive == active)
        return;
    eqActive = active;
    update();
}

void ModernEqGraph::setLowGain(qreal gainDb)
{
    lowGainDb = qBound(-20.0, gainDb, 20.0);
    update();
}

void ModernEqGraph::setLowMid(qreal frequencyHz, qreal gainDb, qreal q)
{
    lowMidFrequencyHz = qBound(kMinimumFrequency, frequencyHz,
                               kMaximumFrequency);
    lowMidGainDb = qBound(-20.0, gainDb, 20.0);
    lowMidQ = qMax(0.01, q);
    update();
}

void ModernEqGraph::setHighMid(qreal frequencyHz, qreal gainDb, qreal q)
{
    highMidFrequencyHz = qBound(kMinimumFrequency, frequencyHz,
                                kMaximumFrequency);
    highMidGainDb = qBound(-20.0, gainDb, 20.0);
    highMidQ = qMax(0.01, q);
    update();
}

void ModernEqGraph::setHighGain(qreal gainDb)
{
    highGainDb = qBound(-20.0, gainDb, 20.0);
    update();
}

void ModernEqGraph::setLowCut(bool active, qreal frequencyHz)
{
    lowCutActive = active;
    lowCutFrequencyHz = qBound(kMinimumFrequency, frequencyHz,
                               kMaximumFrequency);
    update();
}

void ModernEqGraph::setHighCut(bool active, qreal frequencyHz)
{
    highCutActive = active;
    highCutFrequencyHz = qBound(kMinimumFrequency, frequencyHz,
                                kMaximumFrequency);
    update();
}

QSize ModernEqGraph::sizeHint() const
{
    return QSize(920, 130);
}

QSize ModernEqGraph::minimumSizeHint() const
{
    return QSize(640, 125);
}

qreal ModernEqGraph::frequencyPosition(qreal frequencyHz,
                                       const QRectF &plot) const
{
    const qreal clamped = qBound(kMinimumFrequency, frequencyHz,
                                 kMaximumFrequency);
    const qreal minimumLog = std::log10(kMinimumFrequency);
    const qreal maximumLog = std::log10(kMaximumFrequency);
    const qreal normalized = (std::log10(clamped) - minimumLog)
        / (maximumLog - minimumLog);
    return plot.left() + normalized * plot.width();
}

qreal ModernEqGraph::gainPosition(qreal gainDb, const QRectF &plot) const
{
    const qreal normalized = (20.0 - qBound(-20.0, gainDb, 20.0)) / 40.0;
    return plot.top() + normalized * plot.height();
}

qreal ModernEqGraph::normalizedFrequency(qreal frequencyHz) const
{
    const qreal clamped = qBound(kMinimumFrequency, frequencyHz,
                                 kMaximumFrequency);
    const qreal minimumLog = std::log10(kMinimumFrequency);
    const qreal maximumLog = std::log10(kMaximumFrequency);
    return (std::log10(clamped) - minimumLog)
        / (maximumLog - minimumLog);
}

qreal ModernEqGraph::controlCurveGain(qreal position) const
{
    const qreal t = qBound(0.0, position, 1.0);
    auto gaussian = [](qreal distance, qreal width) {
        const qreal ratio = distance / qMax(0.001, width);
        return std::exp(-0.5 * ratio * ratio);
    };
    auto visualBellWidth = [](qreal q) {
        // Visual width only: low Q is broad and high Q is narrow. This is
        // deliberately not a biquad bandwidth calculation.
        return qBound(0.018, 0.16 / std::sqrt(qMax(0.5, q) / 0.5),
                      0.16);
    };
    auto smoothStep = [](qreal edge0, qreal edge1, qreal value) {
        const qreal x = qBound(0.0, (value - edge0) / (edge1 - edge0),
                                1.0);
        return x * x * (3.0 - 2.0 * x);
    };

    qreal gain = lowGainDb * gaussian(t, 0.19)
        + highGainDb * gaussian(1.0 - t, 0.19);

    const qreal lowMidCenter = normalizedFrequency(lowMidFrequencyHz);
    const qreal highMidCenter = normalizedFrequency(highMidFrequencyHz);
    gain += lowMidGainDb * gaussian(t - lowMidCenter,
                                    visualBellWidth(lowMidQ));
    gain += highMidGainDb * gaussian(t - highMidCenter,
                                     visualBellWidth(highMidQ));

    const qreal cutTransitionWidth = 0.055;
    if (lowCutActive) {
        const qreal center = normalizedFrequency(lowCutFrequencyHz);
        gain -= 20.0 * (1.0 - smoothStep(center - cutTransitionWidth,
                                         center + cutTransitionWidth, t));
    }
    if (highCutActive) {
        const qreal center = normalizedFrequency(highCutFrequencyHz);
        gain -= 20.0 * smoothStep(center - cutTransitionWidth,
                                  center + cutTransitionWidth, t);
    }
    return qBound(-20.0, gain, 20.0);
}

void ModernEqGraph::drawBandPoint(QPainter &painter, const QRectF &plot,
                                  qreal x, qreal gainDb,
                                  const QString &label, qreal q,
                                  bool showQ) const
{
    const qreal y = gainPosition(gainDb, plot);
    if (showQ) {
        // This bracket is an abstract control-width indicator. It is not a
        // filter response curve and does not imply a DSP slope.
        const qreal normalizedQ = qBound(0.0,
            std::log10(qMax(0.1, q) / 0.1) / std::log10(160.0), 1.0);
        const qreal halfWidth = 34.0 - normalizedQ * 23.0;
        painter.setPen(QPen(QColor(54, 143, 214, 130), 1.0));
        painter.drawLine(QPointF(x - halfWidth, y),
                         QPointF(x + halfWidth, y));
        painter.drawLine(QPointF(x - halfWidth, y - 3.0),
                         QPointF(x - halfWidth, y + 3.0));
        painter.drawLine(QPointF(x + halfWidth, y - 3.0),
                         QPointF(x + halfWidth, y + 3.0));
    }

    painter.setPen(QPen(QColor("#DDEEFF"), 1.4));
    painter.setBrush(kAccent);
    painter.drawEllipse(QPointF(x, y), 7.0, 7.0);

    QFont labelFont = painter.font();
    labelFont.setPointSizeF(8.5);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor("#E8F3FC"));
    painter.drawText(QRectF(x - 24.0, y - 27.0, 48.0, 16.0),
                     Qt::AlignCenter, label);

    if (showQ) {
        QFont qFont = painter.font();
        qFont.setPointSizeF(8.0);
        qFont.setBold(false);
        painter.setFont(qFont);
        painter.setPen(QColor("#C3CBD3"));
        painter.drawText(QRectF(x - 38.0, y + 8.0, 76.0, 15.0),
                         Qt::AlignCenter,
                         QString("Q %1").arg(q, 0, 'g', 3));
    }
}

void ModernEqGraph::drawCutMarker(QPainter &painter, const QRectF &plot,
                                  qreal frequencyHz,
                                  const QString &label) const
{
    const qreal x = frequencyPosition(frequencyHz, plot);
    painter.setPen(QPen(QColor(54, 143, 214, 150), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));

    QFont font = painter.font();
    font.setPointSizeF(7.5);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor("#BFDDF4"));
    painter.drawText(QRectF(x - 20.0, plot.top() + 4.0, 40.0, 15.0),
                     Qt::AlignCenter, label);
}

void ModernEqGraph::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panel = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor("#242A30"), 1.0));
    painter.setBrush(QColor("#07090B"));
    painter.drawRoundedRect(panel, 4.0, 4.0);

    QFont titleFont = painter.font();
    titleFont.setPointSizeF(9.0);
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    painter.setFont(titleFont);
    painter.setPen(QColor("#D4D7DB"));
    painter.drawText(QRectF(18.0, 10.0, width() - 36.0, 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     "EQ CONTROL GRAPH");

    const qreal topInset = height() < 150 ? 32.0 : 38.0;
    const qreal bottomInset = height() < 150 ? 22.0 : 30.0;
    const QRectF plot = panel.adjusted(54.0, topInset, -22.0,
                                       -bottomInset);
    if (plot.width() < 100.0 || plot.height() < 32.0)
        return;

    static const qreal frequencies[] = {
        20.0, 50.0, 100.0, 200.0, 500.0,
        1000.0, 2000.0, 5000.0, 10000.0, 20000.0
    };
    QFont axisFont = painter.font();
    axisFont.setPointSizeF(7.0);
    axisFont.setBold(false);
    axisFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
    painter.setFont(axisFont);
    for (qreal frequency : frequencies) {
        const qreal x = frequencyPosition(frequency, plot);
        painter.setPen(QPen(kGrid, 1.0));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setPen(kText);
        painter.drawText(QRectF(x - 22.0, plot.bottom() + 7.0, 44.0, 14.0),
                         Qt::AlignCenter, frequencyLabel(frequency));
    }

    static const int gains[] = {20, 10, 0, -10, -20};
    for (int gain : gains) {
        const qreal y = gainPosition(gain, plot);
        painter.setPen(QPen(gain == 0 ? kZeroLine : kGrid,
                            gain == 0 ? 1.3 : 1.0));
        painter.drawLine(QPointF(plot.left(), y),
                         QPointF(plot.right(), y));
        painter.setPen(kText);
        painter.drawText(QRectF(8.0, y - 7.0, 36.0, 14.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         gain > 0 ? QString("+%1").arg(gain)
                                  : QString::number(gain));
    }

    // The coordinate system remains legible while the EQ is off. Only the
    // live control markers are subdued; dimming the complete canvas made the
    // grid effectively disappear against the near-black panel.
    painter.setOpacity(eqActive ? 1.0 : 0.58);

    QPainterPath controlCurve;
    const int samples = qMax(180, qRound(plot.width() / 3.0));
    for (int sample = 0; sample <= samples; ++sample) {
        const qreal normalized = qreal(sample) / qreal(samples);
        const QPointF point(plot.left() + normalized * plot.width(),
                            gainPosition(controlCurveGain(normalized), plot));
        if (sample == 0)
            controlCurve.moveTo(point);
        else
            controlCurve.lineTo(point);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kAccent, 1.65, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(controlCurve);

    if (lowCutActive)
        drawCutMarker(painter, plot, lowCutFrequencyHz, "LC");
    if (highCutActive)
        drawCutMarker(painter, plot, highCutFrequencyHz, "HC");

    // LOW and HIGH use fixed conceptual anchors because the GT-10 exposes no
    // shelf-frequency parameter for those bands.
    drawBandPoint(painter, plot, plot.left() + plot.width() * 0.10,
                  lowGainDb, "L");
    drawBandPoint(painter, plot,
                  frequencyPosition(lowMidFrequencyHz, plot),
                  lowMidGainDb, "LM", lowMidQ, true);
    drawBandPoint(painter, plot,
                  frequencyPosition(highMidFrequencyHz, plot),
                  highMidGainDb, "HM", highMidQ, true);
    drawBandPoint(painter, plot, plot.left() + plot.width() * 0.90,
                  highGainDb, "H");
}
