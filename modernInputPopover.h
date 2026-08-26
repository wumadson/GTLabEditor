#ifndef MODERNINPUTPOPOVER_H
#define MODERNINPUTPOPOVER_H

#include <QFrame>

class ParameterBar;
class QButtonGroup;
class QKeyEvent;
class QPaintEvent;
class QPushButton;
class QWidget;

class ModernInputPopover final : public QFrame
{
    Q_OBJECT

public:
    explicit ModernInputPopover(QWidget *parent = nullptr);

    void setSystemDataReady(bool ready);
    bool systemDataReady() const;
    void refreshFromBackend();
    void showAnchoredTo(QWidget *anchor);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void profileSelected(int profile);
    void parameterChanged(int raw);

private:
    QString parameterAddress(bool presence) const;
    QString displayFor(const QString &address, int raw) const;
    void refreshParameters();
    void writeValue(const QString &address, int raw);

    bool ready = false;
    int currentProfile = 0;
    QButtonGroup *profileGroup = nullptr;
    ParameterBar *levelBar = nullptr;
    ParameterBar *presBar = nullptr;
};

#endif
