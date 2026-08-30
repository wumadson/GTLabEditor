#ifndef MODERNQUICKSETTINGDIALOG_H
#define MODERNQUICKSETTINGDIALOG_H

#include <QDialog>
#include <QHash>

#include "quickSettingCodec.h"

class QLabel;
class QStackedWidget;
class QWidget;

class ModernQuickSettingDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ModernQuickSettingDialog(QWidget *parent = nullptr);

    void addEffectPage(QuickSettingEffect effect, const QString &effectName,
                       QWidget *page);
    void showEffect(QuickSettingEffect effect);

private:
    QLabel *effectLabel = nullptr;
    QStackedWidget *pages = nullptr;
    QHash<int, int> pageIndexes;
    QHash<int, QString> effectNames;
};

#endif
