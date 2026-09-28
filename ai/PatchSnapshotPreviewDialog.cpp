#include "ai/PatchSnapshotPreviewDialog.h"

#include "modernTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QShowEvent>
#include <QStringList>
#include <QVBoxLayout>

namespace Ai {
namespace {

QString safeFileComponent(QString value)
{
    value = value.normalized(QString::NormalizationForm_C).trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                  QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("_+")),
                  QStringLiteral("_"));
    value.remove(QRegularExpression(QStringLiteral("^[._-]+|[._-]+$")));
    return value.left(64);
}

} // namespace

PatchSnapshotPreviewDialog::PatchSnapshotPreviewDialog(
    const QByteArray &snapshotJson, const QByteArray &compactContextJson,
    const QByteArray &detailedContextJson, const QString &displayNumber,
    const QString &patchName, QWidget *parent)
    : QDialog(parent),
      payloads({snapshotJson, compactContextJson, detailedContextJson}),
      exportBaseName(suggestedFileName(displayNumber, patchName))
{
    setWindowTitle(tr("AI Patch Snapshot"));
    setModal(true);
    resize(820, 620);
    setMinimumSize(620, 420);
    setStyleSheet(ModernTheme::applicationStyleSheet());

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 16);
    root->setSpacing(12);

    formatSelector = new QComboBox(this);
    formatSelector->addItem(tr("Patch Snapshot"));
    formatSelector->addItem(tr("AI Context — Compact"));
    formatSelector->addItem(tr("AI Context — Detailed"));
    connect(formatSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PatchSnapshotPreviewDialog::selectPayload);
    root->addWidget(formatSelector);

    preview = new QPlainTextEdit(this);
    preview->setObjectName(QStringLiteral("AiPatchSnapshotPreview"));
    preview->setReadOnly(true);
    preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    preview->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    preview->setPlainText(QString::fromUtf8(currentPayload()));
    root->addWidget(preview, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch();
    auto *copy = new QPushButton(tr("COPY JSON"), this);
    auto *save = new QPushButton(tr("SAVE JSON..."), this);
    auto *close = new QPushButton(tr("CLOSE"), this);
    close->setDefault(true);
    connect(copy, &QPushButton::clicked, this,
            &PatchSnapshotPreviewDialog::copyJson);
    connect(save, &QPushButton::clicked, this,
            &PatchSnapshotPreviewDialog::saveJson);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(copy);
    buttons->addWidget(save);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

QString PatchSnapshotPreviewDialog::suggestedFileName(
    const QString &displayNumber, const QString &patchName)
{
    QStringList parts;
    const QString safeNumber = safeFileComponent(displayNumber);
    const QString safeName = safeFileComponent(patchName);
    if (!safeNumber.isEmpty())
        parts.append(safeNumber);
    if (!safeName.isEmpty())
        parts.append(safeName);
    if (parts.isEmpty())
        parts.append(QStringLiteral("gtlab_patch"));
    return parts.join(QLatin1Char('_')) + QStringLiteral("_snapshot.json");
}

void PatchSnapshotPreviewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    ModernTheme::applyWindowsDarkTitleBar(this);
}

void PatchSnapshotPreviewDialog::copyJson()
{
    QApplication::clipboard()->setText(QString::fromUtf8(currentPayload()));
}

void PatchSnapshotPreviewDialog::saveJson()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save AI Patch Snapshot"), currentFileName(),
        tr("JSON Files (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(currentPayload()) != currentPayload().size()
        || !file.commit()) {
        QMessageBox::critical(this, tr("AI Patch Snapshot"),
                              tr("The JSON snapshot could not be saved."));
    }
}

void PatchSnapshotPreviewDialog::selectPayload(int index)
{
    if (index < 0 || index >= payloads.size())
        return;
    preview->setPlainText(QString::fromUtf8(payloads.at(index)));
}

const QByteArray &PatchSnapshotPreviewDialog::currentPayload() const
{
    int index = formatSelector ? formatSelector->currentIndex() : 0;
    if (index < 0 || index >= payloads.size())
        index = 0;
    return payloads.at(index);
}

QString PatchSnapshotPreviewDialog::currentFileName() const
{
    QString name = exportBaseName;
    name.chop(QStringLiteral("_snapshot.json").size());
    const int index = formatSelector ? formatSelector->currentIndex() : 0;
    if (index == 1)
        return name + QStringLiteral("_ai_context_compact.json");
    if (index == 2)
        return name + QStringLiteral("_ai_context_detailed.json");
    return exportBaseName;
}

} // namespace Ai
