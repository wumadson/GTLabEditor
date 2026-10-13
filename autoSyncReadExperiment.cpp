#include "autoSyncReadExperiment.h"
#ifdef GTLAB_AUTOSYNC_READ_EXPERIMENT
#include "mainWindow.h"
#include "modernFloorBoard.h"
#include "floorBoard.h"
#include "backupCoordinator.h"
#include "quickSettingService.h"
#include "quickSettingCodec.h"
#include "patchTransferCodec.h"
#include "SysxIO.h"
#include "MidiTable.h"
#include "Preferences.h"
#include "midiIO.h"
#include <QApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QMetaProperty>
#include <mach/mach.h>

namespace {
QString widgetFingerprint(QWidget *root) {
    QByteArray values;
    const auto widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        const auto property = widget->metaObject()->userProperty();
        if (property.isValid())
            values += QByteArray::number(quintptr(widget), 16) + ':'
                + property.read(widget).toString().toUtf8() + '\n';
    }
    return QString::fromLatin1(QCryptographicHash::hash(values, QCryptographicHash::Sha256).toHex());
}
QJsonObject memorySample() {
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    QJsonObject sample;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        sample["rss_bytes"] = double(info.resident_size);
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t size = 0;
    if (task_threads(mach_task_self(), &threads, &size) == KERN_SUCCESS) {
        sample["threads"] = int(size);
        for (unsigned i = 0; i < size; ++i) mach_port_deallocate(mach_task_self(), threads[i]);
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), size * sizeof(thread_t));
    }
    return sample;
}
void sendResult(QLocalSocket *socket, const QJsonObject &result) {
    const QByteArray bytes = QJsonDocument(result).toJson(QJsonDocument::Compact);
    qInfo().noquote() << "READ_EXPERIMENT" << bytes;
    QFile log(QString("/tmp/gtlab-read-experiment-%1.jsonl")
              .arg(QCoreApplication::applicationPid()));
    if (log.open(QIODevice::Append)) log.write(bytes + '\n');
    if (socket) { socket->write(bytes + '\n'); socket->flush(); socket->disconnectFromServer(); }
}
}

AutoSyncReadExperiment::AutoSyncReadExperiment(mainWindow *w)
    : QObject(w), window(w) {
    server.setSocketOptions(QLocalServer::UserAccessOption);
    const QString path = QString("/tmp/gtlab-read-experiment-%1.sock")
        .arg(QCoreApplication::applicationPid());
    if (!server.listen(path)) { stopped = true; qWarning() << server.errorString(); }
    else qInfo() << "READ_EXPERIMENT_SOCKET" << path;
    connect(&server, &QLocalServer::newConnection, this, [this] {
        while (server.hasPendingConnections()) {
            QLocalSocket *socket = server.nextPendingConnection();
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            const auto consume = [this, socket] {
                if (socket->canReadLine()) command(socket->readLine().trimmed(), socket);
            };
            connect(socket, &QLocalSocket::readyRead, this, consume);
            if (socket->canReadLine()) consume();
        }
    });
    connect(window->legacyFloorBoard, &floorBoard::updateSignal, this,
            [this] { if (transaction) { ++refreshCount; observe("normal_refresh"); } });
    connect(window, &mainWindow::updateSignal, this,
            [this] { if (transaction) { ++refreshCount; observe("window_refresh"); } });
    connect(SysxIO::Instance(), &SysxIO::sysxReply, this,
            [this](QString) { if (transaction) { ++genericReplyCount; observe("foreign_reply"); } });
    connect(SysxIO::Instance(), &SysxIO::setStatusSymbol, this,
            [this](int status) { if (transaction && status != 1) observe("foreign_activity"); });
}

QString AutoSyncReadExperiment::fingerprint(const Blocks &blocks) {
    QByteArray bytes;
    for (auto it = blocks.cbegin(); it != blocks.cend(); ++it)
        bytes += it.key().toUtf8() + ':' + it.value().toUtf8() + '\n';
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

AutoSyncReadExperiment::~AutoSyncReadExperiment() {
    if (transaction) {
        transaction->disconnect(this);
        transaction->wait();
        delete transaction;
    }
}

QJsonObject AutoSyncReadExperiment::gates() const {
    auto *ui = window->modernFloorBoardWidget;
    auto *io = SysxIO::Instance();
    QString error;
    const Blocks source = PatchTransferCodec::comparableBlocks00To0C(io->getFileSource(), &error);
    QJsonObject g;
    g["connected"] = ui->backendIsConnected && io->isConnected();
    g["ready"] = io->deviceReady();
    g["patch_valid"] = ui->backendHasPatchData && error.isEmpty() && source.size() == 13;
    g["baseline_valid"] = ui->patchChangeGuard.hasBaseline()
        && ui->patchChangeGuard.isCurrentPatch(io->getLoadedBank(), io->getLoadedPatch());
    g["read"] = ui->readRequestInFlight;
    g["write"] = ui->writeRequestInFlight;
    g["management"] = ui->patchManagementInFlight;
    g["system_read"] = ui->manualSystemReadInFlight;
    g["system_ready"] = ui->outputSystemDataReady && ui->tunerSystemDataReady;
    g["chain"] = ui->signalChainTransactionActive;
    g["backup"] = window->backupCoordinator->operation() != BackupCoordinator::Idle;
    g["quick"] = window->quickSettingService->isBusy();
    g["prefetch"] = ui->quickPrefetchActive;
    g["exclusive_available"] = window->legacyFloorBoard->canStartExclusiveMemoryOperation();
    g["modal"] = QApplication::activeModalWidget() != nullptr;
    g["dirty"] = ui->patchChangeGuard.isDirty(source);
    g["experiment_active"] = transaction != nullptr;
    g["source_fingerprint"] = fingerprint(source);
    return g;
}

void AutoSyncReadExperiment::observe(const QString &reason) {
    if (violation.isEmpty()) violation = reason;
    stopped = true;
}

void AutoSyncReadExperiment::command(const QByteArray &cmd, QLocalSocket *socket) {
    QJsonObject g = gates();
    g["rq1_count"] = rq1Count;
    if (cmd == "status") { sendResult(socket, g); return; }
    QString refusal;
    if (cmd != "baseline" && cmd != "physical") refusal = "REFUSED_COMMAND";
    else if (transaction) refusal = "REFUSED_SINGLE_FLIGHT";
    else if (stopped) refusal = "BLOCKED_PREVIOUS_VIOLATION";
    else if (!checkGates(g).isEmpty()) refusal = checkGates(g);
    else if (lastEnd.isValid() && lastEnd.elapsed() < 3000) refusal = "REFUSED_SETTLE";
    else if ((cmd == "baseline" && baselineCount >= 5)
        || (cmd == "physical" && (baselineCount != 5 || physicalCount != 0))) refusal = "REFUSED_LIMIT";
    if (!refusal.isEmpty()) { g["result"] = refusal; sendResult(socket, g); return; }

    auto *io = SysxIO::Instance();
    sourceBefore = io->getFileSource(); syncBefore = io->getSyncStatus();
    before = PatchTransferCodec::comparableBlocks00To0C(sourceBefore);
    bankBefore = io->getLoadedBank(); patchBefore = io->getLoadedPatch();
    nameBefore = io->getCurrentPatchName();
    uiBefore = widgetFingerprint(window);
    const QString request = MidiTable::Instance()->patchRequest(0, 0);
    // Assert request kind/address; never construct a new SysEx mapping here.
    if (request.size() != 34 || request.mid(12, 2) != "11"
        || request.mid(14, 8) != "60000000" || request.mid(22, 8) != "00000D63") {
        stopped = true; g["result"] = "BLOCKED_REQUEST"; sendResult(socket, g); return;
    }
    transaction = new midiIO(this);
    // Only this experimental object's default forwarding is removed. Production
    // objects and SysxIO observers are untouched; no generic reply/refresh leaks.
    QObject::disconnect(transaction, nullptr, io, nullptr);
    const auto ins = transaction->getMidiInDevices();
    const auto outs = transaction->getMidiOutDevices();
    bool inOk = false, outOk = false;
    auto *prefs = Preferences::Instance();
    int in = prefs->getPreferences("Midi", "MidiIn", "device").toInt(&inOk);
    int out = prefs->getPreferences("Midi", "MidiOut", "device").toInt(&outOk);
    if (ins.contains("BOSS GT-10")) { in = ins.indexOf("BOSS GT-10"); inOk = true; }
    if (outs.contains("BOSS GT-10")) { out = outs.indexOf("BOSS GT-10"); outOk = true; }
    if (!inOk || !outOk || in < 0 || out < 0 || in >= ins.size() || out >= outs.size()) {
        delete transaction; transaction = nullptr;
        g["result"] = "REFUSED_PORTS"; sendResult(socket, g); return;
    }
    result = {{"gates", g}, {"generation", ++generation}, {"kind", QString::fromLatin1(cmd)},
              {"dt1_count", 0}, {"write_count", 0}};
    result["memory_before"] = memorySample();
    result["counter_scope"] = "experimental executor only; foreign activity blocks experiment";
    client = socket; violation.clear(); replied = false; completion = false;
    refreshCount = genericReplyCount = 0;
    midiIO *owned = transaction;
    const int token = generation;
    connect(owned, &midiIO::midiFinished, this, [this, owned, token] {
        if (!owns(transaction, owned, generation, token) || completion) { observe("completion_ownership"); return; }
        completion = true; result["completion_ms"] = clock.elapsed();
    });
    connect(owned, &midiIO::setStatusdBugMessage, this, [this](const QString &message) {
        if (message == QCoreApplication::translate("midiIO", "re-trying data request")) {
            ++rq1Count; observe("transport_retry");
        }
    });
    connect(owned, &midiIO::errorSignal, this, [this](QString, QString) { observe("transport_error"); });
    connect(owned, &midiIO::replyMsg, this, [this, owned, token](QString reply) {
        if (!owns(transaction, owned, generation, token) || replied || !completion) { observe("reply_ownership"); return; }
        replied = true;
        const qint64 t1 = clock.elapsed();
        result["request_to_reply_ms"] = t1;
        reply = reply.simplified().remove(' ').toUpper();
        result["reply_bytes"] = reply.size() / 2;
        QByteArray wire = QByteArray::fromHex(reply.toLatin1());
        bool wireValid = QRegularExpression("^[0-9A-F]{3568}$").match(reply).hasMatch();
        int cursor = 0;
        while (wireValid && cursor < wire.size()) {
            const int end = wire.indexOf(char(0xF7), cursor);
            if (end < cursor) { wireValid = false; break; }
            const QByteArray frame = wire.mid(cursor, end - cursor + 1);
            if (frame.size() < 13 || frame.at(0) != char(0xF0)
                || frame.mid(7, 2) != QByteArray::fromHex("6000")) { wireValid = false; break; }
            const auto parsed = QuickSettingCodec::decodeReply(
                QString::fromLatin1(frame.toHex()), frame.mid(7, 4), frame.size() - 13);
            wireValid = parsed.valid;
            cursor = end + 1;
        }
        const DecodedPatch candidate = PatchTransferCodec::decodePatchReply(reply);
        bool valid = wireValid && cursor == wire.size() && candidate.valid
            && !candidate.quickFx && candidate.logicalBlocks00To0C.size() == 13;
        QJsonArray differences;
        int byteCount = 0;
        for (auto it = before.cbegin(); it != before.cend(); ++it) {
            const QString payload = candidate.logicalBlocks00To0C.value(it.key());
            valid = valid && payload.size() == it.value().size();
            if (payload != it.value()) {
                differences.append(it.key());
                for (int i = 0; i < it.value().size(); i += 2)
                    if (it.value().mid(i, 2) != payload.mid(i, 2)) ++byteCount;
            }
        }
        result["decoded_blocks"] = candidate.logicalBlocks00To0C.size();
        result["different_blocks"] = differences;
        result["different_bytes"] = byteCount;
        result["candidate_result"] = valid ? (byteCount ? "DIFFERENT" : "EQUAL") : "INVALID";
        result["decode_compare_ms"] = clock.elapsed() - t1;
        if (!valid) observe("invalid_candidate");
    });
    connect(owned, &QThread::finished, this, [this, owned, token] {
        if (!owns(transaction, owned, generation, token)) { observe("thread_ownership"); return; }
        finish();
    });
    ++rq1Count; clock.start();
    qInfo() << "READ_EXPERIMENT_REQUEST_STARTED" << token;
    owned->sendSysxMsg(request, out, in);
}

void AutoSyncReadExperiment::finish() {
    auto *io = SysxIO::Instance();
    const Blocks after = PatchTransferCodec::comparableBlocks00To0C(io->getFileSource());
    auto *ui = window->modernFloorBoardWidget;
    const bool sourceSame = before == after && bankBefore == io->getLoadedBank()
        && patchBefore == io->getLoadedPatch() && nameBefore == io->getCurrentPatchName()
        && sourceBefore.address == io->getFileSource().address
        && sourceBefore.hex == io->getFileSource().hex
        && syncBefore == io->getSyncStatus() && io->isConnected() && io->deviceReady();
    // Clean gate proves baseline == before. Check it still equals that same map;
    // no private baseline accessor or mutation of PatchChangeGuard is required.
    const bool baselineSame = ui->patchChangeGuard.isCurrentPatch(bankBefore, patchBefore)
        && !ui->patchChangeGuard.isDirty(before);
    result["source_before"] = fingerprint(before); result["source_after"] = fingerprint(after);
    result["source_unchanged"] = sourceSame; result["baseline_unchanged"] = baselineSame;
    result["dirty_after"] = ui->patchChangeGuard.isDirty(after);
    result["ui_unchanged"] = uiBefore == widgetFingerprint(window);
    result["refresh_count"] = refreshCount; result["generic_reply_count"] = genericReplyCount;
    result["rq1_count"] = rq1Count; result["total_ms"] = clock.elapsed();
    result["memory_after"] = memorySample();
    if (!sourceSame || !baselineSame || result["dirty_after"].toBool()) observe("state_changed");
    if (!result["ui_unchanged"].toBool()) observe("ui_values_changed");
    if (!replied || !completion) observe("missing_completion");
    if (result["kind"] == "baseline" && result["candidate_result"] != "EQUAL") observe("baseline_difference");
    if (result["kind"] == "physical" && result["candidate_result"] != "DIFFERENT") observe("physical_no_difference");
    result["result"] = violation.isEmpty() ? "PASS_ONE_SHOT" : "BLOCKED";
    result["violation"] = violation;
    if (violation.isEmpty()) {
        if (result["kind"] == "baseline") ++baselineCount; else ++physicalCount;
    }
    transaction->deleteLater(); transaction = nullptr; lastEnd.start();
    sendResult(client, result); client = nullptr;
}
#endif
