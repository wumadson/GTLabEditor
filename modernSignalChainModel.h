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

    enum ChannelMode {
        Single,
        DualMono,
        DualLR,
        Dynamic,
        UnknownMode
    };

    struct Entry {
        QString rawValue;
        int moduleId = -1;
        QString name;
        int originalPosition = -1;
        Path path = Common;
        bool movable = false;
        bool isSplit = false;
        bool isMerge = false;
        bool isPreampA = false;
        bool isPreampB = false;
    };

    bool refreshFromLegacyBackend();
    void clear();
    void logInterpretedChain() const;

    bool isValid() const;
    QString errorString() const;
    ChannelMode channelMode() const;
    int channelSelect() const;
    QList<Entry> entries() const;
    QList<Entry> commonPrefix() const;
    QList<Entry> pathA() const;
    QList<Entry> pathB() const;
    QList<Entry> commonSuffix() const;

    static QString channelModeName(ChannelMode mode);
    static QString displayName(const Entry &entry);

private:
    bool valid = false;
    QString error;
    ChannelMode mode = UnknownMode;
    int selectedChannel = -1;
    QList<Entry> chainEntries;
    QList<Entry> prefixEntries;
    QList<Entry> pathAEntries;
    QList<Entry> pathBEntries;
    QList<Entry> suffixEntries;
};

#endif
