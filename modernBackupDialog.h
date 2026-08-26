#ifndef MODERNBACKUPDIALOG_H
#define MODERNBACKUPDIALOG_H

#include <QDialog>

class BackupCoordinator;
class QLabel;
class QProgressBar;
class QPushButton;

class ModernBackupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ModernBackupDialog(BackupCoordinator *coordinator,
                                const QString &title,
                                QWidget *parent = nullptr);

private slots:
    void updateProgress(QString patchNumber, int completed, int total,
                        QString stage, QString status);
    void operationFinished(int result, QString title, QString detail);

private:
    BackupCoordinator *coordinator;
    QLabel *patchValue;
    QLabel *countValue;
    QLabel *stageValue;
    QLabel *statusValue;
    QProgressBar *progressBar;
    QPushButton *cancelButton;
};

#endif // MODERNBACKUPDIALOG_H
