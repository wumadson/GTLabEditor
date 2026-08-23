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
    void setCategoriesCollapsible(bool enabled);
    QColor accentColor() const;
    int modelCount() const;

signals:
    void modelSelected(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void itemClicked(QListWidgetItem *item);

private:
    void setCategoryExpanded(QListWidgetItem *category, bool expanded);

    QListWidget *modelList;
    QList<QListWidgetItem *> modelItems;
    QList<QListWidgetItem *> modelCategoryItems;
    QColor browserAccent;
    bool categoriesCollapsible;
};

#endif
