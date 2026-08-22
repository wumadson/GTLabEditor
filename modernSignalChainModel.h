#ifndef MODERNSIGNALCHAINMODEL_H
#define MODERNSIGNALCHAINMODEL_H

#include <QList>
#include <QString>

class modernSignalChainModel
{
public:
    enum Path {
        Common,
        PathA,
        PathB
    };

    enum class ChainRegion {
        CommonPrefix,
        PathA,
        PathB,
        CommonSuffix
    };

    struct ChainDestination {
        ChainRegion region = ChainRegion::CommonPrefix;
        int index = -1;
        bool valid = false;
    };

    enum ChannelMode {
        Single,
        DualMono,
        DualLR,
        Dynamic,
        UnknownMode
    };

    struct Entry {
        QString rawValue;
        QString originalRawValue;
        int moduleId = -1;
        QString name;
        int originalPosition = -1;
        Path path = Common;
        ChainRegion region = ChainRegion::CommonPrefix;
        bool movable = false;
        bool isSplit = false;
        bool isMerge = false;
        bool isPreampA = false;
        bool isPreampB = false;
    };

    struct ChainSnapshot {
        QList<Entry> commonPrefix;
        Entry split;
        QList<Entry> pathA;
        QList<Entry> pathB;
        Entry merge;
        QList<Entry> commonSuffix;
        quint64 revision = 0;
        QString patchIdentity;
        ChannelMode channelMode = UnknownMode;
        int channelSelect = -1;
    };

    bool refreshFromLegacyBackend();
    bool replaceSnapshot(const ChainSnapshot &snapshot, QString *error = nullptr);
    void clear();
    void logInterpretedChain() const;

    bool isValid() const;
    QString errorString() const;
    ChannelMode channelMode() const;
    int channelSelect() const;
    ChainSnapshot snapshot() const;
    QList<Entry> entries() const;
    QList<Entry> commonPrefix() const;
    QList<Entry> pathA() const;
    QList<Entry> pathB() const;
    QList<Entry> commonSuffix() const;

    static bool parseRawBytes(const QList<QString> &rawBytes,
                              ChainSnapshot *snapshot,
                              QString *error = nullptr,
                              quint64 revision = 0,
                              const QString &patchIdentity = QString(),
                              ChannelMode channelMode = UnknownMode,
                              int channelSelect = -1);
    static QList<Entry> flattenedEntries(const ChainSnapshot &snapshot);
    static bool isMovableModule(int moduleId);

    static QString channelModeName(ChannelMode mode);
    static QString displayName(const Entry &entry);

private:
    void applySnapshot(const ChainSnapshot &snapshot);

    bool valid = false;
    QString error;
    ChannelMode mode = UnknownMode;
    int selectedChannel = -1;
    QList<Entry> chainEntries;
    QList<Entry> prefixEntries;
    QList<Entry> pathAEntries;
    QList<Entry> pathBEntries;
    QList<Entry> suffixEntries;
    ChainSnapshot confirmedSnapshot;
};

#endif
