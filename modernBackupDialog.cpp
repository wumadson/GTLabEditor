#include "modernBackupDialog.h"

#include "backupCoordinator.h"
#include "modernTheme.h"

#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ModernBackupDialog::ModernBackupDialog(BackupCoordinator *coordinator,
                                       const QString &titleText,
                                       QWidget *parent)
    : QDialog(parent), coordinator(coordinator)
{
    setWindowTitle(titleText);
    setModal(true);
    setFixedSize(470, 290);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    QLabel *title = new QLabel(titleText, this);
    title->setObjectName("operationTitle");
    patchValue = new QLabel(QString::fromUtf8("—"), this);
    countValue = new QLabel("0 / 200", this);
    stageValue = new QLabel(tr("PREPARING"), this);
    statusValue = new QLabel(tr("Preparing operation…"), this);
    statusValue->setWordWrap(true);

    QGridLayout *details = new QGridLayout;
    details->setHorizontalSpacing(18);
    details->setVerticalSpacing(8);
    details->addWidget(new QLabel(tr("PATCH"), this), 0, 0);
    details->addWidget(patchValue, 0, 1);
    details->addWidget(new QLabel(tr("PROGRESS"), this), 1, 0);
    details->addWidget(countValue, 1, 1);
    details->addWidget(new QLabel(tr("STAGE"), this), 2, 0);
    details->addWidget(stageValue, 2, 1);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 200);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);

    cancelButton = new QPushButton(tr("CANCEL"), this);
    connect(cancelButton, &QPushButton::clicked,
            coordinator, &BackupCoordinator::cancel);
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        cancelButton->setEnabled(false);
        statusValue->setText(tr("Waiting for the current transfer to finish safely…"));
    });
    connect(coordinator, &BackupCoordinator::progressChanged,
            this, &ModernBackupDialog::updateProgress);
    connect(coordinator, &BackupCoordinator::finished,
            this, &ModernBackupDialog::operationFinished);

    QHBoxLayout *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(cancelButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);
    layout->addWidget(title);
    layout->addLayout(details);
    layout->addWidget(progressBar);
    layout->addWidget(statusValue);
    layout->addStretch();
    layout->addLayout(buttonRow);

    const QString background = ModernTheme::color(ModernTheme::ApplicationBackground);
    const QString panel = ModernTheme::color(ModernTheme::Panel);
    const QString border = ModernTheme::color(ModernTheme::Border);
    const QString primary = ModernTheme::color(ModernTheme::PrimaryText);
    const QString secondary = ModernTheme::color(ModernTheme::SecondaryText);
    const QString accent = ModernTheme::color(ModernTheme::AccentCyan);
    setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %4; font-size: 10px; font-weight: 600; }"
        "QLabel#operationTitle { color: %2; font-size: 18px; font-weight: 600; }"
        "QProgressBar { background: %3; border: 1px solid %5; border-radius: 5px; height: 10px; }"
        "QProgressBar::chunk { background: %6; border-radius: 4px; }"
        "QPushButton { min-width: 90px; min-height: 32px; background: %3; color: %2;"
        " border: 1px solid %5; border-radius: 7px; font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { border-color: %6; }"
        "QPushButton:disabled { color: %4; }")
        .arg(background).arg(primary).arg(panel).arg(secondary).arg(border).arg(accent));
}

void ModernBackupDialog::updateProgress(QString patchNumber, int completed,
                                        int total, QString stage, QString status)
{
    patchValue->setText(patchNumber);
    countValue->setText(QString("%1 / %2").arg(completed).arg(total));
    stageValue->setText(stage);
    statusValue->setText(status);
    progressBar->setMaximum(total);
    progressBar->setValue(completed);
}

void ModernBackupDialog::operationFinished(int result, QString title,
                                           QString detail)
{
    const QMessageBox::Icon icon = result == BackupCoordinator::Complete
        ? QMessageBox::Information : QMessageBox::Warning;
    QMessageBox message(icon, title, detail, QMessageBox::Ok, this);
    message.exec();
    accept();
}
