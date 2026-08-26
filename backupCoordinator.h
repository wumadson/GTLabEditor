#ifndef BACKUPCOORDINATOR_H
#define BACKUPCOORDINATOR_H

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>

#include "patchTransferCodec.h"

class floorBoard;

class BackupCoordinator : public QObject
{
    Q_OBJECT
public:
    enum Operation { Idle, UserBackup, UserRestore };
    enum Result { Complete, Canceled, Failed };

    explicit BackupCoordinator(floorBoard *backend, QObject *parent = nullptr);
    Operation operation() const { return currentOperation; }
    bool startUserBackup(const QString &filePath);
    bool startUserRestore(const QVector<DecodedPatch> &patches);

public slots:
    void cancel();

signals:
    void progressChanged(QString patchNumber, int completed, int total,
                         QString stage, QString status);
    void patchVerified(int bank, int patch, QString name);
    void finished(int result, QString title, QString detail);

private slots:
    void replyReceived(QString reply);
    void operationTimedOut();

private:
    enum Stage { Reading, Writing, Verifying };
    bool begin(Operation operation);
    void issueCurrentStep();
    void issueBackupRead();
    void issueRestoreWrite();
    void issueRestoreVerify();
    void handleFailure(const QString &reason);
    void advancePatch();
    void finish(Result result, const QString &title, const QString &detail);
    QString currentPatchNumber() const;
    int currentBank() const;
    int currentPatch() const;

    floorBoard *backend;
    Operation currentOperation = Idle;
    Stage currentStage = Reading;
    QTimer timeoutTimer;
    QElapsedTimer elapsed;
    QVector<DecodedPatch> patches;
    QString backupFilePath;
    int patchIndex = 0;
    int attempt = 0;
    bool cancelRequested = false;
    bool retryPending = false;
    static const int PatchCount = 200;
    static const int MaximumAttempts = 3;
};

#endif // BACKUPCOORDINATOR_H
