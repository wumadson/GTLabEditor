#ifndef QUICKSETTINGSERVICE_H
#define QUICKSETTINGSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QElapsedTimer>

#include "quickSettingCodec.h"

class floorBoard;

class QuickSettingService : public QObject
{
    Q_OBJECT

public:
    explicit QuickSettingService(floorBoard *operationGuard,
                                 QObject *parent = nullptr);

    bool isBusy() const;
    void requestPresentation(QuickSettingEffect effect, int slot);
    void requestType(QuickSettingEffect effect, int slot);
    void load(QuickSettingEffect effect, int slot);
    void save(QuickSettingEffect effect, int slot);
    void save(QuickSettingEffect effect, int slot, const QString &name);

signals:
    void busyChanged(bool busy);
    void slotTypeReady(QuickSettingEffect effect, int slot,
                       int typeRaw, bool valid);
    void slotIdentityReady(QuickSettingEffect effect, int slot,
                           int typeRaw, bool typeValid,
                           QString name, bool nameValid);
    void slotPresentationReady(QuickSettingEffect effect, int slot,
                               int typeRaw, bool typeValid,
                               QString name, bool nameValid,
                               bool success);
    void loadFinished(QuickSettingEffect effect, int slot,
                      bool success, QString detail);
    void saveFinished(QuickSettingEffect effect, int slot,
                      bool verified, QString detail);

private slots:
    void handleReply(QString reply);

private:
    enum class Operation {
        Idle,
        ReadPresentationName,
        ReadPresentationFallback,
        ReadType,
        ReadName,
        LoadEffect,
        ApplyTemporary,
        SaveTransmission,
        SaveNameTransmission,
        VerifyEffect,
        VerifyName
    };

    bool begin(Operation operation, QuickSettingEffect effect, int slot,
               QString *error);
    void sendRead(const QByteArray &address, int size);
    void handleSegmentedReply(const QString &reply);
    void resetSegmentedTransfer(bool temporary);
    void sendCurrentSegmentRead();
    bool sendCurrentSegmentWrite(const QVector<QByteArray> &payloads,
                                 QString *error);
    QByteArray activeNameAddress(QString *error = nullptr) const;
    void finish(bool success, const QString &detail);
    void logLoadTiming(const char *stage, const QString &detail = QString()) const;
    QByteArray currentTemporaryPayload(QuickSettingEffect effect,
                                       QString *error) const;

    floorBoard *guard;
    Operation operation;
    QuickSettingEffect activeEffect;
    int activeSlot;
    QByteArray savedEffectPayload;
    QByteArray savedNamePayload;
    QuickSettingTransferPlan segmentPlan;
    QVector<QByteArray> receivedSegments;
    QVector<QByteArray> savedSegments;
    int segmentIndex = 0;
    int pendingTypeRaw = -1;
    QElapsedTimer loadTimer;
    bool loadTimingActive = false;
};

#endif // QUICKSETTINGSERVICE_H
