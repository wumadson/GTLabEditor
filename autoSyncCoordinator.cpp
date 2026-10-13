#include "autoSyncCoordinator.h"
#include "patchTransferCodec.h"
#include "quickSettingCodec.h"
#include <QRegularExpression>
#include <utility>

AutoSyncCoordinator::AutoSyncCoordinator(StateReader state, ReadStarter start, QObject *parent)
    : QObject(parent), readState(std::move(state)), startRead(std::move(start)) {}

bool AutoSyncCoordinator::transportReady(bool winUsb, bool selected, bool open, bool lost, bool portsValid) {
    return winUsb ? selected && open && !lost : portsValid;
}

QString AutoSyncCoordinator::admission(const State &s, bool busy) {
    if (busy) return "SINGLE_FLIGHT";
    if (s.dirty) return "DIRTY";
    if (!s.connected || !s.ready) return "NOT_READY";
    if (!s.sourceValid || s.source.size()!=13) return "SOURCE";
    if (!s.baselineValid) return "BASELINE";
    if (s.operationsBusy || !s.exclusiveAvailable) return "BUSY";
    if (!s.transportValid) return "TRANSPORT";
    return {};
}

bool AutoSyncCoordinator::requestCandidateRead(const QString &reason) {
    const State s=readState();
    const QString refusal=admission(s,active);
    if (!refusal.isEmpty()) {
        last=Result(); last.reason=refusal; last.generation=generation;
        emit resultReady(last); return false;
    }
    before=s; pending=Result(); pending.reason=reason;
    pending.generation=++generation; pending.session=s.session;
    active=true; completion=false; replied=false; invalidation.clear(); clock.start();
    startRead(generation); return true;
}

void AutoSyncCoordinator::invalidate(const QString &reason) {
    if (active && invalidation.isEmpty()) invalidation=reason;
}

void AutoSyncCoordinator::completed(quint64 token) {
    if (!owns(token)) return;
    if (completion) { invalidate("DUPLICATE_COMPLETION"); return; }
    completion=true;
}

void AutoSyncCoordinator::reply(quint64 token, const QString &raw) {
    if (!owns(token)) return;
    if (replied || !completion) { invalidate("REPLY_ORDER_OR_DUPLICATE"); return; }
    replied=true; pending.requestLatencyMs=clock.elapsed();
    QString wire=raw.simplified().remove(' ').toUpper();
    bool valid=QRegularExpression("^[0-9A-F]{3568}$").match(wire).hasMatch();
    const QByteArray bytes=QByteArray::fromHex(wire.toLatin1());
    int cursor=0;
    while (valid && cursor<bytes.size()) {
        const int end=bytes.indexOf(char(0xF7),cursor);
        if (end<cursor) { valid=false; break; }
        const QByteArray frame=bytes.mid(cursor,end-cursor+1);
        valid=frame.size()>=13 && frame.at(0)==char(0xF0)
            && frame.mid(7,2)==QByteArray::fromHex("6000");
        if (valid) valid=QuickSettingCodec::decodeReply(
            QString::fromLatin1(frame.toHex()),frame.mid(7,4),frame.size()-13).valid;
        cursor=end+1;
    }
    const auto decoded=PatchTransferCodec::decodePatchReply(wire);
    valid=valid && cursor==bytes.size() && decoded.valid && !decoded.quickFx
        && decoded.logicalBlocks00To0C.keys()==before.source.keys();
    if (valid) {
        for (auto it=before.source.cbegin();it!=before.source.cend();++it) {
            const QString value=decoded.logicalBlocks00To0C.value(it.key());
            if (value.size()!=it.value().size()) { valid=false; break; }
            if (value!=it.value()) {
                pending.differingBlocks.append(it.key());
                for (int i=0;i<value.size();i+=2)
                    if (value.mid(i,2)!=it.value().mid(i,2)) ++pending.differingBytes;
            }
        }
    }
    if (!valid) { invalidate("INVALID_REPLY"); return; }
    pending.candidate=decoded.logicalBlocks00To0C;
    pending.status=pending.differingBytes ? Changed : Unchanged;
}

void AutoSyncCoordinator::finished(quint64 token) {
    if (!owns(token)) return;
    const State after=readState();
    if (!completion || !replied) invalidate("MISSING_REPLY");
    if (!admission(after,false).isEmpty()) invalidate("GATES_CHANGED");
    if (after.source!=before.source || after.session!=before.session
        || after.identityKnown!=before.identityKnown
        || after.bank!=before.bank || after.patch!=before.patch) invalidate("STATE_CHANGED");
    if (!invalidation.isEmpty()) {
        pending.status=Error; pending.reason=invalidation; pending.candidate.clear();
        pending.differingBlocks.clear(); pending.differingBytes=0;
    }
    last=pending; active=false; emit resultReady(last);
}
