#include "quickSettingService.h"

#include "floorBoard.h"
#include "SysxIO.h"
#include "globalVariables.h"

#include <QDebug>

namespace {
QList<QString> hexItems(const QByteArray &payload)
{
    QList<QString> items;
    for (char byte : payload) {
        items.append(QStringLiteral("%1").arg(
            static_cast<quint8>(byte), 2, 16, QChar('0')).toUpper());
    }
    return items;
}

QString effectLabel(QuickSettingEffect effect)
{
    if (effect == QuickSettingEffect::PreampA)
        return QStringLiteral("PREAMP A");
    if (effect == QuickSettingEffect::PreampB)
        return QStringLiteral("PREAMP B");
    if (effect == QuickSettingEffect::Delay)
        return QStringLiteral("DELAY");
    if (effect == QuickSettingEffect::Chorus)
        return QStringLiteral("CHORUS");
    if (effect == QuickSettingEffect::Reverb)
        return QStringLiteral("REVERB");
    if (effect == QuickSettingEffect::Compressor)
        return QStringLiteral("COMP");
    if (effect == QuickSettingEffect::Equalizer)
        return QStringLiteral("EQ");
    if (effect == QuickSettingEffect::Fx1)
        return QStringLiteral("FX-1");
    if (effect == QuickSettingEffect::SendReturn)
        return QStringLiteral("S/R");
    return QStringLiteral("OD/DS");
}
}

QuickSettingService::QuickSettingService(floorBoard *operationGuard,
                                         QObject *parent)
    : QObject(parent),
      guard(operationGuard),
      operation(Operation::Idle),
      activeEffect(QuickSettingEffect::PreampA),
      activeSlot(0)
{
}

bool QuickSettingService::isBusy() const
{
    return operation != Operation::Idle;
}

void QuickSettingService::requestPresentation(QuickSettingEffect effect,
                                               int slot)
{
    const bool readsName = QuickSettingCodec::hasPresentationName(effect);
    const Operation firstOperation = readsName
        ? Operation::ReadPresentationName
        : Operation::ReadPresentationFallback;
    QString error;
    if (!begin(firstOperation, effect, slot, &error)) {
        emit slotPresentationReady(effect, slot, -1, false,
                                   QString(), false, false);
        return;
    }

    SysxIO::Instance()->emitStatusMessage(
        tr("Reading Quick Setting list"));
    if (readsName) {
        sendRead(QuickSettingCodec::effectNameAddress(slot, effect),
                 QuickSettingCodec::NameSize);
        return;
    }
    const QByteArray address =
        QuickSettingCodec::presentationFallbackAddress(slot, effect, &error);
    if (address.isEmpty()) {
        emit slotPresentationReady(effect, slot, -1, false,
                                   QString(), false, false);
        finish(false, error);
        return;
    }
    sendRead(address, 1);
}

bool QuickSettingService::begin(Operation nextOperation,
                                QuickSettingEffect effect, int slot,
                                QString *error)
{
    SysxIO *sysxIO = SysxIO::Instance();
    QString addressError;
    if (QuickSettingCodec::slotBaseAddress(slot, &addressError).isEmpty()) {
        if (error)
            *error = addressError;
        return false;
    }
    if (isBusy() || !guard || !guard->canStartExclusiveMemoryOperation()) {
        if (error)
            *error = tr("Another memory operation is already active.");
        return false;
    }
    if (!sysxIO->isConnected() || !sysxIO->deviceReady()) {
        if (error)
            *error = tr("The GT-10 is not ready.");
        return false;
    }

    guard->setExclusiveMemoryOperation(true);
    operation = nextOperation;
    activeEffect = effect;
    activeSlot = slot;
    connect(sysxIO, SIGNAL(sysxReply(QString)),
            this, SLOT(handleReply(QString)), Qt::UniqueConnection);
    emit busyChanged(true);
    if (nextOperation == Operation::LoadEffect)
        logLoadTiming("T1", QStringLiteral("exclusive operation started"));
    return true;
}

void QuickSettingService::sendRead(const QByteArray &address, int size)
{
    QString error;
    const QString request = QuickSettingCodec::buildReadRequest(
        address, size, &error);
    if (request.isEmpty()) {
        finish(false, error);
        return;
    }
    if (operation == Operation::LoadEffect) {
        logLoadTiming("T2", QStringLiteral("RQ1 PREAMP sent; payload=%1 bytes; address=%2")
            .arg(size).arg(QString::fromLatin1(address.toHex().toUpper())));
    } else if (operation == Operation::ReadType && loadTimingActive) {
        logLoadTiming("TYPE RQ1 start",
                      QStringLiteral("payload=%1 bytes").arg(size));
    }
    SysxIO::Instance()->sendSysx(
        request, size, QString::fromLatin1(address.toHex().toUpper()));
}

void QuickSettingService::requestType(QuickSettingEffect effect, int slot)
{
    QString error;
    if (!begin(Operation::ReadType, effect, slot, &error)) {
        emit slotTypeReady(effect, slot, -1, false);
        return;
    }
    if (effect == QuickSettingEffect::Equalizer) {
        pendingTypeRaw = -1;
        operation = Operation::ReadName;
        SysxIO::Instance()->emitStatusMessage(tr("Reading Quick Setting name"));
        sendRead(QuickSettingCodec::effectNameAddress(slot, effect),
                 QuickSettingCodec::NameSize);
        return;
    }
    if (effect == QuickSettingEffect::Fx1) {
        resetSegmentedTransfer(false);
        if (!segmentPlan.isValid()) {
            finish(false, tr("FX-1 Quick Setting transfer plan is invalid."));
            return;
        }
        SysxIO::Instance()->emitStatusMessage(tr("Reading FX-1 Quick Setting"));
        sendCurrentSegmentRead();
        return;
    }
    SysxIO::Instance()->emitStatusMessage(tr("Reading Quick Setting type"));
    sendRead(QuickSettingCodec::effectAddress(slot, effect),
             QuickSettingCodec::effectPayloadSize(effect));
}

void QuickSettingService::load(QuickSettingEffect effect, int slot)
{
    loadTimer.start();
    loadTimingActive = true;
    logLoadTiming("T0", QStringLiteral("LOAD clicked; effect=%1; slot=U%2")
        .arg(effectLabel(effect))
        .arg(slot, 2, 10, QChar('0')));
    QString error;
    if (!begin(Operation::LoadEffect, effect, slot, &error)) {
        logLoadTiming("T8", QStringLiteral("operation rejected; %1").arg(error));
        loadTimingActive = false;
        emit loadFinished(effect, slot, false, error);
        return;
    }
    SysxIO::Instance()->emitStatusMessage(tr("Loading Quick Setting"));
    if (effect == QuickSettingEffect::Fx1) {
        resetSegmentedTransfer(false);
        if (!segmentPlan.isValid()) {
            finish(false, tr("FX-1 Quick Setting transfer plan is invalid."));
            return;
        }
        sendCurrentSegmentRead();
        return;
    }
    sendRead(QuickSettingCodec::effectAddress(slot, effect),
             QuickSettingCodec::effectPayloadSize(effect));
}

QByteArray QuickSettingService::currentTemporaryPayload(
    QuickSettingEffect effect, QString *error) const
{
    if (effect == QuickSettingEffect::Fx1) {
        const QuickSettingTransferPlan plan =
            QuickSettingCodec::transferPlan(1, effect, true, error);
        if (!plan.isValid())
            return QByteArray();
        QVector<QByteArray> segments;
        for (const QuickSettingSegment &segment : plan.segments) {
            const QString bank = QStringLiteral("%1").arg(
                static_cast<quint8>(segment.address.at(2)), 2, 16,
                QChar('0')).toUpper();
            const int offset = static_cast<quint8>(segment.address.at(3));
            const QList<QString> block = SysxIO::Instance()->getSourceItems(
                QStringLiteral("Structure"), bank, QStringLiteral("00"));
            const int first = 11 + offset;
            if (block.size() < first + segment.size) {
                if (error)
                    *error = tr("The current FX-1 buffer is incomplete.");
                return QByteArray();
            }
            QByteArray bytes;
            for (int index = 0; index < segment.size; ++index) {
                bool ok = false;
                const int value = block.at(first + index).toInt(&ok, 16);
                if (!ok || value < 0 || value > 0x7F) {
                    if (error)
                        *error = tr("The current FX-1 buffer is invalid.");
                    return QByteArray();
                }
                bytes.append(static_cast<char>(value));
            }
            segments.append(bytes);
        }
        return QuickSettingCodec::joinSegments(segments, plan, error);
    }
    const QByteArray temporaryAddress =
        QuickSettingCodec::temporaryEffectAddress(effect);
    const QString hex1 = QStringLiteral("%1").arg(
        static_cast<quint8>(temporaryAddress.at(2)), 2, 16,
        QChar('0')).toUpper();
    const int offset = static_cast<quint8>(temporaryAddress.at(3));
    const int payloadSize = QuickSettingCodec::effectPayloadSize(effect);
    const QList<QString> block = SysxIO::Instance()->getSourceItems(
        QStringLiteral("Structure"), hex1, QStringLiteral("00"));
    const int first = 11 + offset;
    if (block.size() < first + payloadSize) {
        if (error)
            *error = tr("The current effect buffer is incomplete.");
        return QByteArray();
    }
    QByteArray payload;
    for (int index = 0; index < payloadSize; ++index) {
        bool ok = false;
        const int value = block.at(first + index).toInt(&ok, 16);
        if (!ok || value < 0 || value > 0x7F) {
            if (error)
                *error = tr("The current effect buffer is invalid.");
            return QByteArray();
        }
        payload.append(static_cast<char>(value));
    }
    return payload;
}

void QuickSettingService::save(QuickSettingEffect effect, int slot)
{
    save(effect, slot, QString());
}

void QuickSettingService::save(QuickSettingEffect effect, int slot,
                               const QString &name)
{
    QString error;
    savedEffectPayload = currentTemporaryPayload(effect, &error);
    if (savedEffectPayload.isEmpty()) {
        emit saveFinished(effect, slot, false, error);
        return;
    }
    savedNamePayload.clear();
    if (effect == QuickSettingEffect::Compressor
        || effect == QuickSettingEffect::OverdriveDistortion
        || effect == QuickSettingEffect::Delay
        || effect == QuickSettingEffect::Chorus
        || effect == QuickSettingEffect::Reverb
        || effect == QuickSettingEffect::Equalizer
        || effect == QuickSettingEffect::Fx1
        || effect == QuickSettingEffect::SendReturn) {
        savedNamePayload = QuickSettingCodec::encodeName(name, &error);
        if (savedNamePayload.isEmpty()) {
            emit saveFinished(effect, slot, false, error);
            return;
        }
    }
    if (!begin(Operation::SaveTransmission, effect, slot, &error)) {
        emit saveFinished(effect, slot, false, error);
        return;
    }

    if (effect == QuickSettingEffect::Fx1) {
        resetSegmentedTransfer(false);
        savedSegments = QuickSettingCodec::splitPayload(
            savedEffectPayload, segmentPlan, &error);
        pendingTypeRaw = QuickSettingCodec::effectTypeRaw(
            effect, savedEffectPayload, &error);
        if (savedSegments.size() != segmentPlan.segments.size()
            || pendingTypeRaw < 0) {
            finish(false, error);
            return;
        }
        SysxIO::Instance()->emitStatusMessage(tr("Saving FX-1 Quick Setting"));
        if (!sendCurrentSegmentWrite(savedSegments, &error))
            finish(false, error);
        return;
    }

    const QString effectMessage = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(slot, effect), savedEffectPayload, &error);
    if (effectMessage.isEmpty()) {
        finish(false, error);
        return;
    }
    SysxIO::Instance()->emitStatusMessage(tr("Saving Quick Setting"));
    SysxIO::Instance()->setDeviceReady(false);
    SysxIO::Instance()->sendSysx(effectMessage);
}

void QuickSettingService::resetSegmentedTransfer(bool temporary)
{
    QString error;
    segmentPlan = QuickSettingCodec::transferPlan(
        activeSlot, activeEffect, temporary, &error);
    receivedSegments.clear();
    segmentIndex = 0;
}

void QuickSettingService::sendCurrentSegmentRead()
{
    if (!segmentPlan.isValid() || segmentIndex < 0
        || segmentIndex >= segmentPlan.segments.size()) {
        finish(false, tr("FX-1 Quick Setting segment index is invalid."));
        return;
    }
    const QuickSettingSegment &segment = segmentPlan.segments.at(segmentIndex);
    sendRead(segment.address, segment.size);
}

bool QuickSettingService::sendCurrentSegmentWrite(
    const QVector<QByteArray> &payloads, QString *error)
{
    if (!segmentPlan.isValid() || payloads.size() != segmentPlan.segments.size()
        || segmentIndex < 0 || segmentIndex >= segmentPlan.segments.size()
        || payloads.at(segmentIndex).size()
            != segmentPlan.segments.at(segmentIndex).size) {
        if (error)
            *error = tr("FX-1 Quick Setting segment is invalid.");
        return false;
    }
    const QString message = QuickSettingCodec::buildWriteMessage(
        segmentPlan.segments.at(segmentIndex).address,
        payloads.at(segmentIndex), error);
    if (message.isEmpty())
        return false;
    SysxIO::Instance()->setDeviceReady(false);
    SysxIO::Instance()->sendSysx(message);
    return true;
}

QByteArray QuickSettingService::activeNameAddress(QString *error) const
{
    return QuickSettingCodec::effectNameAddress(
        activeSlot, activeEffect, pendingTypeRaw, error);
}

void QuickSettingService::handleSegmentedReply(const QString &reply)
{
    if (operation == Operation::ReadType
        || operation == Operation::LoadEffect
        || operation == Operation::VerifyEffect) {
        if (!segmentPlan.isValid() || segmentIndex < 0
            || segmentIndex >= segmentPlan.segments.size()) {
            finish(false, tr("FX-1 Quick Setting transfer state is invalid."));
            return;
        }
        const QuickSettingSegment &segment = segmentPlan.segments.at(segmentIndex);
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, segment.address, segment.size);
        if (!decoded.valid) {
            receivedSegments.clear();
            finish(false, decoded.error);
            return;
        }
        receivedSegments.append(decoded.payload);
        ++segmentIndex;
        if (segmentIndex < segmentPlan.segments.size()) {
            sendCurrentSegmentRead();
            return;
        }

        QString error;
        const QByteArray logical = QuickSettingCodec::joinSegments(
            receivedSegments, segmentPlan, &error);
        receivedSegments.clear();
        if (logical.size() != QuickSettingCodec::Fx1PayloadSize) {
            finish(false, error);
            return;
        }

        if (operation == Operation::ReadType) {
            pendingTypeRaw = QuickSettingCodec::effectTypeRaw(
                activeEffect, logical, &error);
            if (pendingTypeRaw < 0) {
                emit slotTypeReady(activeEffect, activeSlot, -1, false);
                finish(false, error);
                return;
            }
            const QByteArray nameAddress = activeNameAddress(&error);
            if (nameAddress.isEmpty()) {
                finish(false, error);
                return;
            }
            operation = Operation::ReadName;
            sendRead(nameAddress, QuickSettingCodec::NameSize);
            return;
        }

        if (operation == Operation::LoadEffect) {
            savedEffectPayload = logical;
            resetSegmentedTransfer(true);
            savedSegments = QuickSettingCodec::splitPayload(
                savedEffectPayload, segmentPlan, &error);
            if (savedSegments.size() != segmentPlan.segments.size()) {
                finish(false, error);
                return;
            }
            operation = Operation::ApplyTemporary;
            if (!sendCurrentSegmentWrite(savedSegments, &error))
                finish(false, error);
            return;
        }

        if (!QuickSettingCodec::payloadMatches(savedEffectPayload, logical)) {
            finish(false, tr("FX-1 Quick Setting readback did not match."));
            return;
        }
        operation = Operation::VerifyName;
        const QByteArray nameAddress = activeNameAddress(&error);
        if (nameAddress.isEmpty()) {
            finish(false, error);
            return;
        }
        sendRead(nameAddress, QuickSettingCodec::NameSize);
        return;
    }

    if (operation == Operation::ReadName || operation == Operation::VerifyName) {
        QString error;
        const QByteArray nameAddress = activeNameAddress(&error);
        if (nameAddress.isEmpty()) {
            finish(false, error);
            return;
        }
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, nameAddress, QuickSettingCodec::NameSize);
        if (operation == Operation::ReadName) {
            QString nameError;
            const QString name = decoded.valid
                ? QuickSettingCodec::decodeName(decoded.payload, &nameError)
                : QString();
            const bool valid = decoded.valid && nameError.isEmpty();
            emit slotIdentityReady(activeEffect, activeSlot, pendingTypeRaw,
                                   true, name,
                                   valid && !name.trimmed().isEmpty());
            finish(valid, valid ? QString()
                                : (decoded.valid ? nameError : decoded.error));
            return;
        }
        if (!decoded.valid
            || !QuickSettingCodec::payloadMatches(savedNamePayload,
                                                   decoded.payload)) {
            finish(false, decoded.valid
                ? tr("FX-1 data verified, but the shared TYPE name did not match.")
                : decoded.error);
            return;
        }
        finish(true, tr("FX-1 Quick Setting data and shared TYPE name verified."));
        return;
    }

    if (operation == Operation::SaveTransmission
        || operation == Operation::ApplyTemporary) {
        SysxIO::Instance()->setDeviceReady(true);
        ++segmentIndex;
        if (segmentIndex < segmentPlan.segments.size()) {
            QString error;
            if (!sendCurrentSegmentWrite(savedSegments, &error))
                finish(false, error);
            return;
        }
        if (operation == Operation::ApplyTemporary) {
            finish(true, tr("FX-1 Quick Setting loaded into the Temporary Buffer."));
            return;
        }
        QString error;
        const QByteArray nameAddress = activeNameAddress(&error);
        const QString nameMessage = QuickSettingCodec::buildWriteMessage(
            nameAddress, savedNamePayload, &error);
        if (nameAddress.isEmpty() || nameMessage.isEmpty()) {
            finish(false, error);
            return;
        }
        operation = Operation::SaveNameTransmission;
        SysxIO::Instance()->setDeviceReady(false);
        SysxIO::Instance()->sendSysx(nameMessage);
        return;
    }

    if (operation == Operation::SaveNameTransmission) {
        SysxIO::Instance()->setDeviceReady(true);
        operation = Operation::VerifyEffect;
        resetSegmentedTransfer(false);
        sendCurrentSegmentRead();
        return;
    }

    finish(false, tr("Unexpected FX-1 Quick Setting service reply."));
}

void QuickSettingService::handleReply(QString reply)
{
    if (activeEffect == QuickSettingEffect::Fx1) {
        handleSegmentedReply(reply);
        return;
    }
    if (operation == Operation::ReadPresentationName) {
        const QByteArray address = QuickSettingCodec::effectNameAddress(
            activeSlot, activeEffect);
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, address, QuickSettingCodec::NameSize);
        QString nameError;
        const QString name = decoded.valid
            ? QuickSettingCodec::decodeName(decoded.payload, &nameError)
            : QString();
        const bool nameValid = decoded.valid && nameError.isEmpty()
            && !name.trimmed().isEmpty();
        if (nameValid) {
            emit slotPresentationReady(activeEffect, activeSlot, -1, false,
                                       name.trimmed(), true, true);
            finish(true, QString());
            return;
        }
        if (activeEffect == QuickSettingEffect::Equalizer) {
            const bool emptyButValid = decoded.valid && nameError.isEmpty();
            emit slotPresentationReady(activeEffect, activeSlot, -1, false,
                                       QString(), false, emptyButValid);
            finish(emptyButValid,
                   emptyButValid ? QString()
                                 : (decoded.valid ? nameError : decoded.error));
            return;
        }
        QString error;
        const QByteArray fallback =
            QuickSettingCodec::presentationFallbackAddress(
                activeSlot, activeEffect, &error);
        if (fallback.isEmpty()) {
            emit slotPresentationReady(activeEffect, activeSlot, -1, false,
                                       QString(), false, false);
            finish(false, error);
            return;
        }
        operation = Operation::ReadPresentationFallback;
        sendRead(fallback, 1);
        return;
    }

    if (operation == Operation::ReadPresentationFallback) {
        QString error;
        const QByteArray address =
            QuickSettingCodec::presentationFallbackAddress(
                activeSlot, activeEffect, &error);
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, address, 1);
        const bool valid = decoded.valid && decoded.payload.size() == 1;
        const int typeRaw = valid
            ? static_cast<quint8>(decoded.payload.at(0)) : -1;
        emit slotPresentationReady(activeEffect, activeSlot, typeRaw, valid,
                                   QString(), false, valid);
        finish(valid, valid ? QString()
                            : (decoded.valid ? error : decoded.error));
        return;
    }

    if (operation == Operation::SaveTransmission) {
        SysxIO::Instance()->setDeviceReady(true);
        if (!savedNamePayload.isEmpty()) {
            QString error;
            const QString nameMessage = QuickSettingCodec::buildWriteMessage(
                QuickSettingCodec::effectNameAddress(activeSlot, activeEffect),
                savedNamePayload, &error);
            if (nameMessage.isEmpty()) {
                finish(false, error);
                return;
            }
            operation = Operation::SaveNameTransmission;
            SysxIO::Instance()->setDeviceReady(false);
            SysxIO::Instance()->sendSysx(nameMessage);
            return;
        }
        operation = Operation::VerifyEffect;
        sendRead(QuickSettingCodec::effectAddress(activeSlot, activeEffect),
                 QuickSettingCodec::effectPayloadSize(activeEffect));
        return;
    }

    if (operation == Operation::SaveNameTransmission) {
        SysxIO::Instance()->setDeviceReady(true);
        operation = Operation::VerifyEffect;
        sendRead(QuickSettingCodec::effectAddress(activeSlot, activeEffect),
                 QuickSettingCodec::effectPayloadSize(activeEffect));
        return;
    }

    if (operation == Operation::ApplyTemporary) {
        logLoadTiming("T6", QStringLiteral("Temporary Buffer DT1 transmission completed"));
        finish(true, tr("Quick Setting loaded into the Temporary Buffer."));
        return;
    }

    if (operation == Operation::ReadType) {
        if (loadTimingActive)
            logLoadTiming("TYPE reply", QStringLiteral("reply=%1 bytes").arg(reply.size() / 2));
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, QuickSettingCodec::effectAddress(activeSlot, activeEffect),
            QuickSettingCodec::effectPayloadSize(activeEffect));
        QString typeError;
        const int typeRaw = decoded.valid
            ? QuickSettingCodec::effectTypeRaw(
                  activeEffect, decoded.payload, &typeError) : -1;
        const bool valid = decoded.valid && typeRaw >= 0;
        if (loadTimingActive)
            logLoadTiming("TYPE validation", valid ? QStringLiteral("valid") : QStringLiteral("invalid"));
        if (valid
            && (activeEffect == QuickSettingEffect::Compressor
                || activeEffect == QuickSettingEffect::OverdriveDistortion
                || activeEffect == QuickSettingEffect::Delay
                || activeEffect == QuickSettingEffect::Chorus
                || activeEffect == QuickSettingEffect::Reverb
                || activeEffect == QuickSettingEffect::SendReturn)) {
            pendingTypeRaw = typeRaw;
            operation = Operation::ReadName;
            sendRead(QuickSettingCodec::effectNameAddress(activeSlot,
                                                          activeEffect),
                     QuickSettingCodec::NameSize);
            return;
        }
        emit slotTypeReady(activeEffect, activeSlot, typeRaw, valid);
        finish(valid, valid ? QString()
                            : (decoded.valid ? typeError : decoded.error));
        return;
    }

    if (operation == Operation::ReadName) {
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply,
            QuickSettingCodec::effectNameAddress(activeSlot, activeEffect),
            QuickSettingCodec::NameSize);
        QString nameError;
        const QString name = decoded.valid
            ? QuickSettingCodec::decodeName(decoded.payload, &nameError)
            : QString();
        const bool namePayloadValid = decoded.valid && nameError.isEmpty();
        emit slotIdentityReady(activeEffect, activeSlot, pendingTypeRaw,
                               pendingTypeRaw >= 0,
                               name, namePayloadValid && !name.isEmpty());
        finish(namePayloadValid, namePayloadValid ? QString()
                                                   : (decoded.valid
                                                          ? nameError
                                                          : decoded.error));
        return;
    }

    if (operation == Operation::LoadEffect) {
        logLoadTiming("T3", QStringLiteral("first service reply received; reply=%1 bytes")
            .arg(reply.size() / 2));
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, QuickSettingCodec::effectAddress(activeSlot, activeEffect),
            QuickSettingCodec::effectPayloadSize(activeEffect));
        if (!decoded.valid) {
            finish(false, decoded.error);
            return;
        }
        logLoadTiming("T4", QStringLiteral("effect response complete and validated"));
        operation = Operation::ApplyTemporary;
        SysxIO::Instance()->setDeviceReady(true);
        logLoadTiming("T5", QStringLiteral("applying %1 bytes to Temporary Buffer")
            .arg(decoded.payload.size()));
        const QByteArray temporaryAddress =
            QuickSettingCodec::temporaryEffectAddress(activeEffect);
        SysxIO::Instance()->setFileSource(
            QStringLiteral("Structure"),
            QStringLiteral("%1").arg(
                static_cast<quint8>(temporaryAddress.at(2)), 2, 16,
                QChar('0')).toUpper(),
            QStringLiteral("00"),
            QStringLiteral("%1").arg(
                static_cast<quint8>(temporaryAddress.at(3)), 2, 16,
                QChar('0')).toUpper(),
            hexItems(decoded.payload));
        return;
    }

    if (operation == Operation::VerifyEffect) {
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply, QuickSettingCodec::effectAddress(activeSlot, activeEffect),
            QuickSettingCodec::effectPayloadSize(activeEffect));
        if (!decoded.valid
            || !QuickSettingCodec::payloadMatches(savedEffectPayload,
                                                   decoded.payload)) {
            finish(false, decoded.valid
                ? tr("Quick Setting effect readback did not match.")
                : decoded.error);
            return;
        }
        if (!savedNamePayload.isEmpty()) {
            operation = Operation::VerifyName;
            sendRead(QuickSettingCodec::effectNameAddress(activeSlot,
                                                          activeEffect),
                     QuickSettingCodec::NameSize);
            return;
        }
        finish(true, tr("Quick Setting write verified."));
        return;
    }

    if (operation == Operation::VerifyName) {
        const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
            reply,
            QuickSettingCodec::effectNameAddress(activeSlot, activeEffect),
            QuickSettingCodec::NameSize);
        if (!decoded.valid
            || !QuickSettingCodec::payloadMatches(savedNamePayload,
                                                   decoded.payload)) {
            finish(false, decoded.valid
                ? tr("Quick Setting name readback did not match.")
                : decoded.error);
            return;
        }
        finish(true, tr("Quick Setting effect and name write verified."));
    }
}

void QuickSettingService::finish(bool success, const QString &detail)
{
    const Operation completed = operation;
    const QuickSettingEffect effect = activeEffect;
    const int slot = activeSlot;
    disconnect(SysxIO::Instance(), SIGNAL(sysxReply(QString)),
               this, SLOT(handleReply(QString)));
    SysxIO::Instance()->setDeviceReady(true);
    if (guard)
        guard->setExclusiveMemoryOperation(false);
    operation = Operation::Idle;
    emit busyChanged(false);

    if (completed == Operation::LoadEffect
        || completed == Operation::ApplyTemporary) {
        emit loadFinished(effect, slot, success, detail);
        logLoadTiming("T7", QStringLiteral("Modern UI refresh callback returned"));
    } else if (completed == Operation::SaveTransmission
             || completed == Operation::SaveNameTransmission
             || completed == Operation::VerifyEffect
             || completed == Operation::VerifyName)
        emit saveFinished(effect, slot, success, detail);

    SysxIO::Instance()->emitStatusMessage(success
        ? tr("Ready") : tr("Quick Setting operation failed"));

    if (completed == Operation::LoadEffect
        || completed == Operation::ApplyTemporary) {
        logLoadTiming("T8", QStringLiteral("operation finished; success=%1")
            .arg(success ? QStringLiteral("true") : QStringLiteral("false")));
        loadTimingActive = false;
    }
}

void QuickSettingService::logLoadTiming(const char *stage,
                                        const QString &detail) const
{
    if (!loadTimingActive || !loadTimer.isValid())
        return;
    qInfo().noquote() << QStringLiteral("[QuickSetting LOAD] %1 +%2 ms%3")
        .arg(QString::fromLatin1(stage))
        .arg(loadTimer.elapsed())
        .arg(detail.isEmpty() ? QString() : QStringLiteral(" | ") + detail);
}
