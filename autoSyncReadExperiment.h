#ifndef AUTOSYNCREADEXPERIMENT_H
#define AUTOSYNCREADEXPERIMENT_H
#ifdef GTLAB_AUTOSYNC_READ_EXPERIMENT
#include <QObject>
#include <QLocalServer>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QMap>
#include <QPointer>
#include "SysxIO.h"
class mainWindow;
class midiIO;
class QLocalSocket;
class AutoSyncReadExperiment : public QObject {
public:
    explicit AutoSyncReadExperiment(mainWindow *window);
    ~AutoSyncReadExperiment() override;
    static bool owns(const void *active, const void *sender, int current, int token) {
        return active && active == sender && current == token;
    }
    static QString checkGates(const QJsonObject &g) {
        if (g["dirty"].toBool()) return "REFUSED_DIRTY";
        for (const char *key : {"connected", "ready", "patch_valid", "baseline_valid",
                                "system_ready", "exclusive_available"})
            if (!g[key].toBool()) return "REFUSED_GATES";
        for (const char *key : {"read", "write", "management", "system_read", "chain",
                                "backup", "quick", "prefetch", "modal", "experiment_active"})
            if (g[key].toBool()) return "REFUSED_GATES";
        return QString();
    }
private:
    using Blocks = QMap<QString, QString>;
    QJsonObject gates() const;
    void command(const QByteArray &command, QLocalSocket *client);
    void finish();
    void observe(const QString &reason);
    static QString fingerprint(const Blocks &blocks);
    mainWindow *window;
    QLocalServer server;
    QPointer<QLocalSocket> client;
    midiIO *transaction = nullptr;
    QElapsedTimer clock, lastEnd;
    Blocks before;
    SysxData sourceBefore;
    bool syncBefore = false;
    QString nameBefore;
    QString uiBefore;
    QJsonObject result;
    QString violation;
    int bankBefore = 0, patchBefore = 0;
    int generation = 0, baselineCount = 0, physicalCount = 0, rq1Count = 0;
    int refreshCount = 0, genericReplyCount = 0;
    bool stopped = false, replied = false, completion = false;
};
#endif
#endif
