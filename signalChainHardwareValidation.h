#ifndef SIGNALCHAINHARDWAREVALIDATION_H
#define SIGNALCHAINHARDWAREVALIDATION_H

#include "modernSignalChainModel.h"

#include <QList>
#include <QString>

struct ChainHardwareTestResult
{
    QString name;
    QString patchIdentity;
    QList<QString> before;
    QList<QString> requested;
    QList<QString> readback;
    bool match = false;
    QList<QString> rollbackRequested;
    QList<QString> rollbackReadback;
    bool rollbackMatch = false;
    QString dt1;
    QString error;
};

struct ChainLiveTransactionResult
{
    QList<QString> before;
    QList<QString> requested;
    QList<QString> readback;
    QList<QString> rollbackReadback;
    QString patchIdentity;
    QString error;
    bool match = false;
    bool rollbackAttempted = false;
    bool rollbackMatch = false;
    bool patchChanged = false;
};

class SignalChainHardwareValidation
{
public:
    SignalChainHardwareValidation();

    bool captureOriginal(QString *error);
    bool runApprovedTests(QString *error);
    bool executeLiveTransaction(const QList<QString> &before,
                                const QList<QString> &requested,
                                ChainLiveTransactionResult *result,
                                QString *error);

    QString backupPath() const;
    QString logPath() const;
    QString patchIdentity() const;
    QString patchName() const;
    QList<QString> originalStructure() const;
    QList<ChainHardwareTestResult> results() const;

private:
    struct PatchDump {
        QString rawHex;
        QList<QString> structure;
        QString identity;
        QString name;
    };

    bool requestTemporaryPatch(PatchDump *dump, QString *error);
    bool parseTemporaryPatch(const QString &reply, PatchDump *dump,
                             QString *error) const;
    bool sendAndWait(const QString &sysx, QString *reply, int timeoutMs,
                     QString *error) const;
    bool performChainWriteTest(const QString &name,
                               const modernSignalChainModel::ChainSnapshot &after,
                               QString *error);
    bool restoreOriginal(const PatchDump *currentReadback,
                         ChainHardwareTestResult *result,
                         QString *error);
    bool buildMove(int moduleId, modernSignalChainModel::ChainRegion region,
                   int index, modernSignalChainModel::ChainSnapshot *after,
                   QString *error) const;
    bool chooseCommonModule(int *moduleId,
                            modernSignalChainModel::ChainRegion *region,
                            int *index, QString *error) const;
    bool choosePathBModule(int *moduleId, QString *error) const;
    QString buildStructureDt1(const QList<QString> &bytes,
                              QString *error) const;
    void appendLog(const QString &line) const;
    void logResult(const ChainHardwareTestResult &result) const;

    PatchDump originalDump;
    modernSignalChainModel::ChainSnapshot originalSnapshot;
    QString savedBackupPath;
    QString savedLogPath;
    QList<ChainHardwareTestResult> testResults;
    bool transactionActive = false;
};

#endif
