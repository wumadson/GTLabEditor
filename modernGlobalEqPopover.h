#ifndef MODERNGLOBALEQPOPOVER_H
#define MODERNGLOBALEQPOPOVER_H

#include <QFrame>

class ParameterBar;
class QComboBox;
class QKeyEvent;
class QPaintEvent;
class QWidget;

class ModernGlobalEqPopover final : public QFrame
{
    Q_OBJECT

public:
    explicit ModernGlobalEqPopover(QWidget *parent = nullptr);

    void setSystemDataReady(bool ready);
    bool systemDataReady() const;
    void refreshFromBackend();
    void showAnchoredTo(QWidget *anchor);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void gainChanged(int value);
    void selectorChanged(int index);

private:
    ParameterBar *createGain(const QString &label, const QString &address);
    QWidget *createSelector(const QString &label, const QString &address,
                            QComboBox **comboOut);
    void writeValue(const QString &address, int raw);
    QString gainDisplay(const QString &address, int raw) const;

    bool ready = false;
    ParameterBar *lowGain = nullptr;
    ParameterBar *midGain = nullptr;
    ParameterBar *highGain = nullptr;
    QComboBox *midFrequency = nullptr;
    QComboBox *midQ = nullptr;
};

#endif
