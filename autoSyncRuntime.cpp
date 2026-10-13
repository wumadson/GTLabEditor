#include "autoSyncRuntime.h"
#include "mainWindow.h"
#include "modernFloorBoard.h"
#include "backupCoordinator.h"
#include "quickSettingService.h"
#include "patchTransferCodec.h"
#include "Preferences.h"
#include "midiIO.h"
#include <QApplication>
#ifdef Q_OS_WIN
#include "gt10WinUsbBackend.h"
#endif
#ifdef GTLAB_AUTOSYNC_FOUNDATION_TEST
#include <QShortcut>
#include <QDebug>
#endif

AutoSyncRuntime::AutoSyncRuntime(mainWindow *w) : QObject(w), window(w) {
    coordinator=new AutoSyncCoordinator([this] { return state(); },
        [this](quint64 token) { start(token); },this);
    connect(window->modernFloorBoardWidget,&modernFloorBoard::connectionStateChanged,
        this,[this](bool) { ++session; coordinator->invalidate("SESSION_CHANGED"); });
    connect(window->legacyFloorBoard,&floorBoard::updateSignal,this,
        [this] { coordinator->invalidate("PATCH_REFRESH"); });
    connect(window,&mainWindow::updateSignal,this,
        [this] { coordinator->invalidate("SOURCE_REFRESH"); });
    connect(SysxIO::Instance(),&SysxIO::setStatusSymbol,this,
        [this](int status) { if (status!=1) coordinator->invalidate("PRODUCTION_ACTIVITY"); });
    connect(SysxIO::Instance(),&SysxIO::sysxReply,this,
        [this](const QString &) { coordinator->invalidate("FOREIGN_REPLY"); });
#ifdef GTLAB_AUTOSYNC_FOUNDATION_TEST
    auto *trigger=new QShortcut(QKeySequence("Ctrl+Alt+R"),window);
    connect(trigger,&QShortcut::activated,this,
        [this] { coordinator->requestCandidateRead("manual-test"); });
    connect(coordinator,&AutoSyncCoordinator::resultReady,this,
        [](const AutoSyncCoordinator::Result &r) {
            qInfo() << "AUTO_SYNC_FOUNDATION" << int(r.status) << r.reason
                << r.generation << r.candidate.size() << r.differingBlocks
                << r.differingBytes << r.requestLatencyMs;
        });
#endif
}

AutoSyncRuntime::~AutoSyncRuntime() {
    if (transaction) {
        transaction->disconnect(this); transaction->wait(); delete transaction;
    }
    if (reserved) window->legacyFloorBoard->setExclusiveMemoryOperation(false);
}

AutoSyncCoordinator::State AutoSyncRuntime::state() {
    AutoSyncCoordinator::State s;
    auto *ui=window->modernFloorBoardWidget;
    auto *io=SysxIO::Instance();
    QString error;
    s.source=PatchTransferCodec::comparableBlocks00To0C(io->getFileSource(),&error);
    s.connected=ui->backendIsConnected && io->isConnected(); s.ready=io->deviceReady();
    s.sourceValid=ui->backendHasPatchData && error.isEmpty() && s.source.size()==13;
    s.bank=io->getLoadedBank(); s.patch=io->getLoadedPatch();
    s.identityKnown=s.bank>0 && s.patch>0;
    s.baselineValid=ui->patchChangeGuard.hasBaseline()
        && ui->patchChangeGuard.isCurrentPatch(s.bank,s.patch);
    s.dirty=ui->patchChangeGuard.isDirty(s.source);
    s.operationsBusy=ui->readRequestInFlight || ui->writeRequestInFlight
        || ui->patchManagementInFlight || ui->manualSystemReadInFlight
        || ui->signalChainTransactionActive || ui->quickPrefetchActive
        || !ui->outputSystemDataReady || !ui->tunerSystemDataReady
        || window->backupCoordinator->operation()!=BackupCoordinator::Idle
        || window->quickSettingService->isBusy() || QApplication::activeModalWidget();
    s.exclusiveAvailable=reserved || window->legacyFloorBoard->canStartExclusiveMemoryOperation();
    s.session=session;
    midiInPort=0; midiOutPort=0;
#ifdef Q_OS_WIN
    if (midiIO::usingWinUsbBackend()) {
        const auto &backend=Gt10WinUsbBackend::instance();
        s.transportValid=AutoSyncCoordinator::transportReady(true,backend.wasSelected(),
            backend.isOpen(),backend.hasDeviceLoss(),false);
        s.session=(session<<32) | backend.connectionEpoch();
    } else
#endif
    {
        auto *prefs=Preferences::Instance(); bool inOk=false,outOk=false;
        int in=prefs->getPreferences("Midi","MidiIn","device").toInt(&inOk);
        int out=prefs->getPreferences("Midi","MidiOut","device").toInt(&outOk);
        midiIO ports;
        const auto ins=ports.getMidiInDevices(), outs=ports.getMidiOutDevices();
        if (ins.contains("BOSS GT-10")) { in=ins.indexOf("BOSS GT-10"); inOk=true; }
        if (outs.contains("BOSS GT-10")) { out=outs.indexOf("BOSS GT-10"); outOk=true; }
        midiInPort=in; midiOutPort=out;
        s.transportValid=AutoSyncCoordinator::transportReady(false,false,false,false,
            inOk && outOk && in>=0 && out>=0 && in<ins.size() && out<outs.size()
            && !prefs->getPreferences("Midi","MidiOut","device").isEmpty());
    }
    return s;
}

void AutoSyncRuntime::start(quint64 token) {
    auto *io=SysxIO::Instance();
    transaction=new midiIO(this);
    QObject::disconnect(transaction,nullptr,io,nullptr);
    reserved=true; window->legacyFloorBoard->setExclusiveMemoryOperation(true);
    midiIO *owned=transaction;
    connect(owned,&midiIO::midiFinished,this,[this,owned,token] {
        if (transaction==owned) coordinator->completed(token);
    });
    connect(owned,&midiIO::replyMsg,this,[this,owned,token](const QString &wire) {
        if (transaction==owned) coordinator->reply(token,wire);
    });
    connect(owned,&midiIO::errorSignal,this,[this,owned](QString,QString) {
        if (transaction==owned) coordinator->invalidate("TRANSPORT_ERROR");
    });
    connect(owned,&midiIO::setStatusdBugMessage,this,[this,owned](const QString &message) {
        if (transaction==owned && message==QCoreApplication::translate("midiIO","re-trying data request"))
            coordinator->invalidate("TRANSPORT_RETRY");
    });
    connect(owned,&QThread::finished,this,[this,owned,token] {
        if (transaction!=owned) return;
        transaction=nullptr; owned->deleteLater();
        reserved=false; window->legacyFloorBoard->setExclusiveMemoryOperation(false);
        coordinator->finished(token);
    });
    owned->sendSysxMsg(MidiTable::Instance()->patchRequest(0,0),midiOutPort,midiInPort);
}
