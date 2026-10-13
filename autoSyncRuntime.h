#ifndef AUTOSYNCRUNTIME_H
#define AUTOSYNCRUNTIME_H
#include "autoSyncCoordinator.h"
class mainWindow;
class midiIO;
// Binds existing application gates/transport to the compare-only coordinator.
class AutoSyncRuntime : public QObject {
public:
    explicit AutoSyncRuntime(mainWindow *window);
    ~AutoSyncRuntime() override;
private:
    AutoSyncCoordinator::State state();
    void start(quint64 token);
    mainWindow *window;
    AutoSyncCoordinator *coordinator;
    midiIO *transaction=nullptr;
    quint64 session=0;
    int midiInPort=0, midiOutPort=0;
    bool reserved=false;
};
#endif
