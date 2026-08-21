#ifndef EFFECTMODELBROWSER_H
#define EFFECTMODELBROWSER_H

#include <QColor>
#include <QList>
#include <QStringList>
#include <QWidget>

class QListWidget;
class QListWidgetItem;

class EffectModelBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit EffectModelBrowser(QWidget *parent = nullptr);

    void setModels(const QStringList &labels);
    void setCurrentIndex(int index);
    void setAccentColor(const QColor &color);
    QColor accentColor() const;
    int modelCount() const;

signals:
    void modelSelected(int index);

private slots:
    void itemClicked(QListWidgetItem *item);

private:
    QListWidget *modelList;
    QList<QListWidgetItem *> modelItems;
    QList<QListWidgetItem *> modelCategoryItems;
    QColor browserAccent;
};

#endif
