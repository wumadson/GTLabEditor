#ifndef QUICKSETTINGCODEC_H
#define QUICKSETTINGCODEC_H

#include <QByteArray>
#include <QString>
#include <QMetaType>
#include <QVector>

enum class QuickSettingEffect
{
    PreampA,
    PreampB,
    OverdriveDistortion,
    Delay,
    Chorus,
    Reverb,
    Compressor,
    Equalizer,
    Fx1,
    SendReturn
};

Q_DECLARE_METATYPE(QuickSettingEffect)

struct QuickSettingReply
{
    bool valid = false;
    QString error;
    QByteArray address;
    QByteArray payload;
};

struct QuickSettingSegment
{
    QByteArray address;
    int size = 0;
};

struct QuickSettingTransferPlan
{
    QVector<QuickSettingSegment> segments;
    int totalLogicalSize = 0;

    bool isValid() const;
};

class QuickSettingCodec
{
public:
    static constexpr int PreampPayloadSize = 29;
    static constexpr int OddsPayloadSize = 14;
    static constexpr int DelayPayloadSize = 32;
    static constexpr int ChorusPayloadSize = 16;
    static constexpr int ReverbPayloadSize = 16;
    static constexpr int CompressorPayloadSize = 16;
    static constexpr int EqualizerPayloadSize = 16;
    static constexpr int Fx1PayloadSize = 470;
    static constexpr int SendReturnPayloadSize = 4;
    static constexpr int NameSize = 12;

    static QByteArray slotBaseAddress(int slot, QString *error = nullptr);
    static QByteArray effectAddress(int slot, QuickSettingEffect effect,
                                    QString *error = nullptr);
    static QByteArray nameAddress(int slot, QString *error = nullptr);
    static QByteArray effectNameAddress(int slot, QuickSettingEffect effect,
                                        QString *error = nullptr);
    static QByteArray effectNameAddress(int slot, QuickSettingEffect effect,
                                        int typeRaw,
                                        QString *error = nullptr);
    static bool hasPresentationName(QuickSettingEffect effect);
    static QByteArray presentationFallbackAddress(
        int slot, QuickSettingEffect effect, QString *error = nullptr);
    static QByteArray temporaryEffectAddress(QuickSettingEffect effect);
    static QuickSettingTransferPlan transferPlan(
        int slot, QuickSettingEffect effect, bool temporary,
        QString *error = nullptr);
    static QByteArray joinSegments(const QVector<QByteArray> &segments,
                                   const QuickSettingTransferPlan &plan,
                                   QString *error = nullptr);
    static QVector<QByteArray> splitPayload(
        const QByteArray &payload, const QuickSettingTransferPlan &plan,
        QString *error = nullptr);
    static int effectPayloadSize(QuickSettingEffect effect);
    static int effectTypeRaw(QuickSettingEffect effect,
                             const QByteArray &payload,
                             QString *error = nullptr);

    static QString buildReadRequest(const QByteArray &address, int size,
                                    QString *error = nullptr);
    static QString buildWriteMessage(const QByteArray &address,
                                     const QByteArray &payload,
                                     QString *error = nullptr);
    static QuickSettingReply decodeReply(const QString &reply,
                                         const QByteArray &expectedAddress,
                                         int expectedSize);
    static int preampTypeRaw(const QByteArray &payload,
                             QString *error = nullptr);

    static QByteArray encodeName(const QString &name,
                                 QString *error = nullptr);
    static QString decodeName(const QByteArray &encoded,
                              QString *error = nullptr);
    static bool payloadMatches(const QByteArray &expected,
                               const QByteArray &actual);
};

#endif // QUICKSETTINGCODEC_H
