#ifndef MODERNPATCHLISTMODEL_H
#define MODERNPATCHLISTMODEL_H

#include <QObject>
#include <QVector>

class ModernPatchListModel : public QObject
{
    Q_OBJECT
public:
    enum Category { User, Preset, Temp, QuickFxUser, QuickFxPreset };
    struct Patch {
        Category category;
        int bank;
        int patch;
        QString number;
        QString name;
        bool nameResolved;
    };

    explicit ModernPatchListModel(QObject *parent = nullptr);
    const QVector<Patch> &patches() const { return items; }
    QString patchNumber(int bank, int patch) const;

public slots:
    void setPatchName(int bank, int patch, const QString &name);
    void setCurrentPatch(int bank, int patch);

signals:
    void patchUpdated(int bank, int patch);
    void currentPatchChanged(int bank, int patch);

private:
    Patch *find(int bank, int patch);
    QVector<Patch> items;
};

#endif
