#ifndef AI_PATCHPARAMETERCATALOG_H
#define AI_PATCHPARAMETERCATALOG_H

#include <QList>
#include <QMap>
#include <QString>
#include <QVector>

namespace Ai {

enum class AvailabilityRule {
    Always,
    ControllerEquals,
    ControllerIn,
    ControllerNotEquals,
    PreampBright,
    CustomWah
};

struct ParameterAddress
{
    QString block;
    QString offset;
    bool twoByte = false;

    QString text() const;
};

struct ParameterDescriptor
{
    QString id;
    ParameterAddress address;
    AvailabilityRule rule = AvailabilityRule::Always;
    ParameterAddress controller;
    QVector<int> controllerValues;
    QString unavailableReason;
};

struct EffectDescriptor
{
    QString id;
    ParameterAddress state;
    ParameterAddress type;
    QList<ParameterDescriptor> parameters;
};

class PatchParameterCatalog
{
public:
    static QList<ParameterDescriptor> patchParameters();
    static QList<ParameterDescriptor> routingParameters();
    static QList<EffectDescriptor> effects(const QMap<QString, QString> &blocks);

    static bool preampBrightAvailable(int type, int customType);
    static bool isAvailable(const ParameterDescriptor &descriptor,
                            const QMap<QString, QString> &blocks);
    static QString canonicalId(const QString &source, const QString &fallback);

private:
    static QList<ParameterDescriptor> fxParameters(
        const QString &baseBlock, int algorithmRaw);
};

} // namespace Ai

#endif // AI_PATCHPARAMETERCATALOG_H
