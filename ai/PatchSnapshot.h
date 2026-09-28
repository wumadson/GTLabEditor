#ifndef AI_PATCHSNAPSHOT_H
#define AI_PATCHSNAPSHOT_H

#include <QList>
#include <QString>

namespace Ai {

struct ParameterValue
{
    QString id;
    QString address;
    bool available = true;
    QString unavailableReason;
    bool hasRaw = false;
    int raw = 0;
    QString rawHex;
    QString display;
    QString unit;
    QString enumText;
};

struct EffectSnapshot
{
    QString id;
    bool hasEnabled = false;
    bool enabled = false;
    ParameterValue type;
    QList<ParameterValue> parameters;
};

struct SignalChainModule
{
    QString id;
    QString name;
    QString raw;
    QString normalized;
    int position = -1;
    bool hasEnabled = false;
    bool enabled = false;
};

struct SignalChainSnapshot
{
    QList<SignalChainModule> commonPrefix;
    SignalChainModule split;
    QList<SignalChainModule> pathA;
    QList<SignalChainModule> pathB;
    SignalChainModule merge;
    QList<SignalChainModule> commonSuffix;
    QString channelMode;
    int channelSelect = -1;
    QString sourceReliability;
};

struct PatchMetadata
{
    QString name;
    int bank = -1;
    int slot = -1;
    QString displayNumber;
    QString location;
    bool dirty = false;
};

struct DeviceMetadata
{
    QString model;
    bool connected = false;
    bool confirmed = false;
};

struct SourceMetadata
{
    QString provenance;
    QString reliability;
};

struct ControllersMetadata
{
    bool included = false;
    QString reason = "deferred_from_schema_v1";
};

struct PatchSnapshot
{
    int schemaVersion = 1;
    DeviceMetadata device;
    SourceMetadata source;
    PatchMetadata patch;
    QList<ParameterValue> patchParameters;
    SignalChainSnapshot signalChain;
    QList<ParameterValue> routing;
    QList<EffectSnapshot> effects;
    ControllersMetadata controllers;
};

} // namespace Ai

#endif // AI_PATCHSNAPSHOT_H
