#ifndef AUTOSYNCCOORDINATOR_H
#define AUTOSYNCCOORDINATOR_H
#include <QObject>
#include <QMap>
#include <QStringList>
#include <QElapsedTimer>
#include <functional>

class AutoSyncCoordinator : public QObject {
    Q_OBJECT
public:
    using Blocks = QMap<QString, QString>;
    struct State {
        Blocks source;
        bool connected=false, ready=false, sourceValid=false, baselineValid=false;
        bool dirty=false, operationsBusy=false, exclusiveAvailable=false;
        bool transportValid=false, identityKnown=false;
        int bank=0, patch=0;
        quint64 session=0;
    };
    enum Status { Unchanged, Changed, Refused, Error };
    Q_ENUM(Status)
    struct Result {
        Status status=Refused;
        QString reason;
        Blocks candidate;
        QStringList differingBlocks;
        int differingBytes=0;
        qint64 requestLatencyMs=0;
        quint64 generation=0, session=0;
    };
    using StateReader=std::function<State()>;
    using ReadStarter=std::function<void(quint64)>;
    AutoSyncCoordinator(StateReader state, ReadStarter start, QObject *parent=nullptr);
    bool requestCandidateRead(const QString &reason);
    bool isBusy() const { return active; }
    const Result &lastResult() const { return last; }
    static bool transportReady(bool winUsb, bool selected, bool open, bool lost, bool portsValid);
    static QString admission(const State &state, bool busy);
    // The adapter supplies callbacks only from its owned transaction.
    void completed(quint64 token);
    void reply(quint64 token, const QString &wire);
    void finished(quint64 token);
    void invalidate(const QString &reason);
signals:
    void resultReady(const AutoSyncCoordinator::Result &result);
private:
    bool owns(quint64 token) const { return active && token==generation; }
    StateReader readState;
    ReadStarter startRead;
    State before;
    Result last, pending;
    QElapsedTimer clock;
    quint64 generation=0;
    bool active=false, completion=false, replied=false;
    QString invalidation;
};
#endif
