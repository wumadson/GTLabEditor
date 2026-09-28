#ifndef AI_PATCHSNAPSHOTPREVIEWDIALOG_H
#define AI_PATCHSNAPSHOTPREVIEWDIALOG_H

#include <QByteArray>
#include <QDialog>
#include <QList>
#include <QString>

class QShowEvent;
class QComboBox;
class QPlainTextEdit;

namespace Ai {

class PatchSnapshotPreviewDialog final : public QDialog
{
    Q_OBJECT

public:
    PatchSnapshotPreviewDialog(const QByteArray &snapshotJson,
                               const QByteArray &compactContextJson,
                               const QByteArray &detailedContextJson,
                               const QString &displayNumber,
                               const QString &patchName,
                               QWidget *parent = nullptr);

    static QString suggestedFileName(const QString &displayNumber,
                                     const QString &patchName);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void copyJson();
    void saveJson();
    void selectPayload(int index);
    const QByteArray &currentPayload() const;
    QString currentFileName() const;

    QList<QByteArray> payloads;
    QString exportBaseName;
    QComboBox *formatSelector = nullptr;
    QPlainTextEdit *preview = nullptr;
};

} // namespace Ai

#endif // AI_PATCHSNAPSHOTPREVIEWDIALOG_H
