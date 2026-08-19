#ifndef MODERNFLOORBOARD_H
#define MODERNFLOORBOARD_H

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QFrame;

class modernFloorBoard : public QWidget
{
    Q_OBJECT

public:
    explicit modernFloorBoard(QWidget *parent = nullptr);

private slots:
    void toggleReverb();
    void refreshBackendStatus();

private:
    QFrame *createEffectBlock(const QString &name,
                              const QString &subtitle,
                              const QString &color);

    QLabel *patchNumber;
    QLabel *patchName;
    QLabel *connectionStatus;
    QListWidget *presetList;

    QPushButton *reverbButton = nullptr;
    QLabel *reverbLed = nullptr;
};

#endif
