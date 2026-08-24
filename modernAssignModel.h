#ifndef MODERNASSIGNMODEL_H
#define MODERNASSIGNMODEL_H

#include <QString>
#include <QVector>

class ModernAssignModel
{
public:
    enum class SourceKind {
        Normal,
        InternalPedal,
        WavePedal,
        InputLevel
    };

    struct Record {
        int number = 0;
        QString bank;
        QString baseAddress;
        bool valid = false;
        bool enabled = false;
        int targetId = 0;
        QString targetName;
        int targetMin = 0;
        int targetMax = 0;
        QString targetMinDisplay;
        QString targetMaxDisplay;
        int source = 0;
        QString sourceDisplay;
        QString sourceModeDisplay;
        int activeRangeLow = 0;
        int activeRangeHigh = 0;
        SourceKind sourceKind = SourceKind::Normal;
        QString internalTriggerDisplay;
        QString internalTimeDisplay;
        QString internalCurveDisplay;
        QString waveRateDisplay;
        QString waveFormDisplay;
        QString inputSensitivityDisplay;
    };

    void refresh(bool backendConnected, bool backendHasPatchData);
    bool isAvailable() const;
    int count() const;
    const Record &record(int index) const;

private:
    struct Address {
        QString bank;
        int offset = 0;
    };

    Address baseForIndex(int index) const;
    QString hexAddress(int value) const;
    bool bufferContains(const QString &bank, int offset,
                        int byteCount = 1) const;
    int readValue(const QString &bank, int offset) const;
    QString displayValue(const QString &bank, int offset, int raw) const;
    QString targetValueDisplay(const QString &targetBank,
                               const QString &targetAddress,
                               int raw) const;
    Record readRecord(int index) const;

    QVector<Record> records;
    bool available = false;
};

#endif
