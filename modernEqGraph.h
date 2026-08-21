#ifndef MODERNEQGRAPH_H
#define MODERNEQGRAPH_H

#include <QWidget>

class QPainter;
class QRectF;

class ModernEqGraph : public QWidget
{
public:
    explicit ModernEqGraph(QWidget *parent = nullptr);

    void setEqActive(bool active);
    void setLowGain(qreal gainDb);
    void setLowMid(qreal frequencyHz, qreal gainDb, qreal q);
    void setHighMid(qreal frequencyHz, qreal gainDb, qreal q);
    void setHighGain(qreal gainDb);
    void setLowCut(bool active, qreal frequencyHz = 20.0);
    void setHighCut(bool active, qreal frequencyHz = 20000.0);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal frequencyPosition(qreal frequencyHz, const QRectF &plot) const;
    qreal gainPosition(qreal gainDb, const QRectF &plot) const;
    qreal normalizedFrequency(qreal frequencyHz) const;
    qreal controlCurveGain(qreal normalizedPosition) const;
    void drawBandPoint(QPainter &painter, const QRectF &plot,
                       qreal x, qreal gainDb, const QString &label,
                       qreal q = 0.0, bool showQ = false) const;
    void drawCutMarker(QPainter &painter, const QRectF &plot,
                       qreal frequencyHz, const QString &label) const;

    bool eqActive;
    qreal lowGainDb;
    qreal lowMidFrequencyHz;
    qreal lowMidGainDb;
    qreal lowMidQ;
    qreal highMidFrequencyHz;
    qreal highMidGainDb;
    qreal highMidQ;
    qreal highGainDb;
    bool lowCutActive;
    qreal lowCutFrequencyHz;
    bool highCutActive;
    qreal highCutFrequencyHz;
};

#endif
