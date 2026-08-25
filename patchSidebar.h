#ifndef PATCHSIDEBAR_H
#define PATCHSIDEBAR_H

#include <QFrame>
#include <QMap>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QContextMenuEvent;
class ModernPatchListModel;
class PatchBankSection;

class PatchListItem : public QFrame
{
    Q_OBJECT
public:
    PatchListItem(int bank, int patch, const QString &number, QWidget *parent = nullptr);
    void setPatchName(const QString &name);
    void setCurrent(bool current);
    void setPending(bool pending);
    QString patchName() const;
signals:
    void activated(int bank, int patch, QString name);
    void contextMenuRequested(int bank, int patch, QString name,
                              QPoint globalPosition);
protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
private:
    void refreshStyle();
    int patchBank;
    int patchIndex;
    QLabel *numberLabel;
    QLabel *nameLabel;
    bool current = false;
    bool pending = false;
};

class PatchBankSection : public QWidget
{
    Q_OBJECT
public:
    PatchBankSection(int bank, const QString &label, QWidget *parent = nullptr);
    void addPatch(PatchListItem *item);
    void setExpanded(bool expanded);
    bool matchesSearch(const QString &text);
signals:
    void expanded(int bank);
private slots:
    void toggleExpanded();
private:
    int bankNumber;
    QPushButton *header;
    QWidget *content;
    QVBoxLayout *contentLayout;
    QList<PatchListItem *> patchItems;
};

class PatchSidebar : public QFrame
{
    Q_OBJECT
public:
    explicit PatchSidebar(ModernPatchListModel *model, QWidget *parent = nullptr);
    QSize sizeHint() const override;
signals:
    void patchActivated(int bank, int patch, QString name);
    void bankExpanded(int bank);
    void renamePatchRequested(int bank, int patch);
    void pastePatchRequested(int sourceBank, int sourcePatch,
                             QString sourceNumber, QString sourceName,
                             int targetBank, int targetPatch,
                             QString targetName);
public slots:
    void updatePatch(int bank, int patch);
    void setCurrentPatch(int bank, int patch);
private slots:
    void applyFilter(const QString &text);
    void activatePatch(int bank, int patch, QString name);
    void expandBank(int bank);
    void showPatchContextMenu(int bank, int patch, QString name,
                              QPoint globalPosition);
private:
    ModernPatchListModel *patchModel;
    QMap<QString, PatchListItem *> items;
    QMap<int, PatchBankSection *> bankSections;
    QList<PatchBankSection *> banks;
    int copiedBank = 0;
    int copiedPatch = 0;
    QString copiedNumber;
    QString copiedName;
};

#endif
