#include "modernWidgets.h"
#include "productEdition.h"
#include "freeWinUsbStartup.h"
#include "mainWindow.h"
#include "modernFloorBoard.h"
#include "modernControlAssignEditor.h"
#include "modernQuickSettingDialog.h"
#include "modernAboutDialog.h"
#include "customControlListMenu.h"
#include "patchSidebar.h"
#include "Preferences.h"
#include "customKnob.h"
#include "patchTransferCodec.h"
#include "patchBackupCodec.h"
#include "globalVariables.h"
#include "SysxIO.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QFile>
#include <QLabel>
#include <QTranslator>
#include <QDebug>

namespace {
int failures = 0, premiumDialogs = 0, fileDialogs = 0;
void check(bool passed, const QString &name)
{
    qInfo().noquote() << (passed ? "PASS" : "FAIL") << name;
    failures += !passed;
}
QString prefix() { return ProductEdition::isFree() ? "FREE_" : "FULL_"; }
void invoke(QObject *object, const char *slot, const QString &name)
{
    const int premiumBefore = premiumDialogs;
    const int fileBefore = fileDialogs;
    check(QMetaObject::invokeMethod(object, slot, Qt::DirectConnection),
          name + " slot exists");
    check(ProductEdition::isFree()
        ? premiumDialogs == premiumBefore + 1 && fileDialogs == fileBefore
        : premiumDialogs == premiumBefore && fileDialogs == fileBefore + 1,
        prefix() + name + (ProductEdition::isFree() ? "_BLOCKED" : "_ENABLED"));
}
}

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    using namespace FreeWinUsbStartup;
    check(shouldConnect(true, true, false, validBinding(true, true, true, true, 0)),
          "FREE_WIN11_WINUSB_EMPTY_MIDI_SETTINGS_CONNECTS");
    check(!shouldConnect(true, true, false, validBinding(false, false, false, false, 0)),
          "FREE_WIN11_NO_GT10_DOES_NOT_CONNECT");
    check(!shouldConnect(true, true, false, validBinding(true, true, true, false, 31))
          && !validBinding(true, true, false, true, 0)
          && !validBinding(false, true, true, true, 0),
          "FREE_WIN11_BROKEN_WINUSB_DOES_NOT_CONNECT");
    check(!shouldConnect(false, true, false, true) && shouldConnect(false, true, true, false),
          "FULL_STARTUP_BEHAVIOR_UNCHANGED");
    check(!shouldConnect(true, false, false, true) && shouldConnect(true, false, true, false),
          "WIN10_ROLAND_MIDI_PATH_UNCHANGED");
#if defined(Q_OS_WIN) && defined(GTLAB_FREE_EDITION)
    if (functionalBindingAvailable())
        qFatal("Disconnect GT-10 before running the offline edition harness");
    check(!functionalBindingAvailable(), "FREE_REAL_NO_GT10_BINDING_REJECTED");
#endif
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("GT Lab Edition Offline Tests");
    Preferences *preferences = Preferences::Instance();
    if (!preferences->getPreferences("Midi", "MidiIn", "device").isEmpty()
        || !preferences->getPreferences("Midi", "MidiOut", "device").isEmpty())
        qFatal("Offline test profile must have no MIDI devices selected");
    QTimer closer;
    QObject::connect(&closer, &QTimer::timeout, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible()) continue;
            if (widget->objectName() == "PremiumFeatureDialog") {
                if (premiumDialogs == 0)
                    widget->grab().save("/tmp/gtlab-free-premium.png");
                ++premiumDialogs;
                static_cast<QMessageBox *>(widget)->accept();
            } else if (auto *dialog = qobject_cast<QFileDialog *>(widget)) {
                ++fileDialogs;
                dialog->reject();
            }
        }
    });
    closer.start(10);
    // Timeout is a failure, not a hang if an unexpected modal dialog appears.
    QTimer::singleShot(60000, &app, [] { qFatal("Edition test timed out"); });
    mainWindow window;
    QTranslator translation;
    check(translation.load(":/translations/gtlab_pt_BR.qm")
        && translation.translate("ProductEdition", "This feature is available in the full GT Lab Editor.")
            == QString::fromUtf8("Esta funcionalidade está disponível no GT Lab Editor completo."),
          prefix() + "PREMIUM_PT_BR_RESOURCE");
    SysxIO *sysx = SysxIO::Instance();
    sysx->setConnected(false);
    check(premiumDialogs == 0, "No startup premium popup");
    window.resize(1440, 900);
    window.show();
    app.processEvents();
    check(window.findChild<QWidget *>("PatchSidebar"), prefix() + "LIBRARY_WORKS UI present");
    auto *sidebar = window.findChild<PatchSidebar *>();
    if (sidebar) {
        const int n = premiumDialogs;
        check(QMetaObject::invokeMethod(sidebar, "expandBank", Qt::DirectConnection,
                                       Q_ARG(int, 1)) && premiumDialogs == n,
              prefix() + "LIBRARY_BROWSE_NOT_GATED");
    }
    check(window.findChild<QWidget *>("BottomTunerCard"), prefix() + "TUNER_WORKS UI present");
    check(window.findChild<QWidget *>("SignalChainContent"), prefix() + "SIGNAL_CHAIN_WORKS UI present");
    window.grab().save(ProductEdition::isFree() ? "/tmp/gtlab-free-ui.png" : "/tmp/gtlab-full-ui.png");
    ModernAboutDialog about;
    bool hasEditionName = false;
    for (QLabel *label : about.findChildren<QLabel *>("AboutSubtitle"))
        hasEditionName |= label->text() == ProductEdition::applicationName();
    check(hasEditionName, prefix() + "ABOUT_IDENTITY");
    about.resize(760, 540);
    about.show();
    app.processEvents();
    about.grab().save(ProductEdition::isFree() ? "/tmp/gtlab-free-about.png" : "/tmp/gtlab-full-about.png");
    about.hide();
    const SysxData before = sysx->getFileSource();
    invoke(&window, "open", "IMPORT");
    invoke(&window, "saveAs", "EXPORT");
    invoke(&window, "backupUserPatches", "BACKUP");
    invoke(&window, "restoreUserPatches", "RESTORE");
    if (ProductEdition::isFree()) {
        for (const char *slot : {"save", "importSMF", "exportSMF", "openGXG",
                                "saveGXG", "systemLoad", "systemSave",
                                "bulkLoad", "bulkSave"}) {
            const int count = premiumDialogs;
            check(QMetaObject::invokeMethod(&window, slot, Qt::DirectConnection)
                  && premiumDialogs == count + 1,
                  QString("FREE_indirect_%1_BLOCKED").arg(slot));
        }
    }
    check(sysx->getFileSource().hex == before.hex,
          "File/backup attempts do not mutate offline patch");
    auto *board = window.findChild<modernFloorBoard *>();
    check(board, "Modern UI exists");
    if (board) {
        const int count = premiumDialogs;
        check(QMetaObject::invokeMethod(board, "showControlAssignEditor", Qt::DirectConnection),
              "Assign entry exists");
        check(ProductEdition::isFree() ? premiumDialogs == count + 1
                                      : premiumDialogs == count,
              prefix() + "ASSIGN_EDIT_" + (ProductEdition::isFree() ? "BLOCKED" : "ENABLED"));
        if (ProductEdition::isFree()) {
            for (const char *slot : {"showQuickSettings", "loadQuickSetting", "saveQuickSetting"}) {
                const int n = premiumDialogs;
                check(QMetaObject::invokeMethod(board, slot, Qt::DirectConnection)
                      && premiumDialogs == n + 1, QString("FREE_QUICK_SETTINGS_%1_BLOCKED").arg(slot));
            }
        } else {
            auto *quick = board->findChild<ModernQuickSettingDialog *>();
            check(quick, "Full Quick Settings dialog exists");
            if (quick) {
                quick->showEffect(QuickSettingEffect::Reverb);
                check(quick->isVisible() && premiumDialogs == count, "FULL_QUICK_SETTINGS_ENABLED UI");
                quick->close();
            }
        }
    }

    // Real loaded patch, legacy parameter widget, existing serializer; no MIDI.
    QFile fixture("gt-10 temp patch_pathA.syx");
    check(fixture.open(QIODevice::ReadOnly), "Patch fixture available");
    const QByteArray patch = fixture.readAll();
    const DecodedPatch decoded = PatchTransferCodec::decodePatchReply(
        QString::fromLatin1(patch.toHex()));
    check(decoded.valid, "RQ1 fixture decoded using existing codec");
    if (!decoded.valid) return 1;
    SysxData loaded = before; // Keep all existing synthetic fallback blocks intact.
    for (int block = 0; block <= 0x0C; ++block) {
        const QString address = QString("%1").arg(block, 2, 16, QChar('0')).toUpper() + "00";
        const int index = loaded.address.indexOf(address);
        check(index >= 0, "Existing logical block " + address);
        if (index < 0) return 1;
        const QString message = decoded.messages00To0C.at(block);
        QList<QString> bytes;
        for (int offset = 0; offset < message.size(); offset += 2)
            bytes.append(message.mid(offset, 2));
        loaded.hex[index] = bytes;
    }
    sysx->setFileSource("Structure", loaded);
    sysx->setFileSource("Structure", "0B", "00", "20", "01");
    sysx->setFileSource("Structure", "0B", "00", "27", "03");
    QString error;
    const auto original = PatchTransferCodec::comparableBlocks00To0C(sysx->getFileSource(), &error);
    check(error.isEmpty() && original.contains("0B00"), "Configured Assign fixture loaded");
    if (ProductEdition::isFree()) {
        customControlListMenu source(nullptr, "0B", "00", "27");
        const int n = premiumDialogs;
        source.dialogUpdateSignal();
        check(premiumDialogs == n && sysx->getSourceValue("Structure", "0B", "00", "27") == 3,
              "FREE_ASSIGN_REFRESH_READ_ONLY_NO_POPUP");
    }
    customKnob assign(nullptr, "0B", "00", "20");
    const int count = premiumDialogs;
    assign.valueChanged(0, "0B", "00", "20");
    check(ProductEdition::isFree()
        ? premiumDialogs == count + 1 && sysx->getSourceValue("Structure", "0B", "00", "20") == 1
        : premiumDialogs == count && sysx->getSourceValue("Structure", "0B", "00", "20") == 0,
        prefix() + "LEGACY_ASSIGN_EDIT_GUARD");
    // Restore the fixture for the independent preservation test.
    sysx->setFileSource("Structure", "0B", "00", "20", "01");
    customKnob reverb(nullptr, "0A", "00", "32");
    reverb.valueChanged(25, "0A", "00", "32");
    check(sysx->getSourceValue("Structure", "0A", "00", "32") == 25,
          prefix() + "PATCH_EDIT_WORKS real widget");
    sysx->setFileSource("Structure", "00", "00", "00", "46");
    check(sysx->getSourceValue("Structure", "00", "00", "00") == 0x46,
          prefix() + "PATCH_RENAME_WORKS buffer");
    const QString write = PatchTransferCodec::buildUserWriteMessage(sysx->getFileSource(), 29, 1, &error);
    QMap<QString, QString> serialized;
    const QByteArray stream = QByteArray::fromHex(write.toLatin1());
    bool valid = !stream.isEmpty();
    int offset = 0;
    while (offset < stream.size()) {
        const int end = stream.indexOf(char(0xF7), offset);
        if (end < 0) { valid = false; break; }
        QByteArray frame = stream.mid(offset, end - offset + 1);
        // The existing encoder relocates addresses; midiIO finalizes checksums
        // at send time. Validate using the same existing checksum helper,
        // without starting that transport or touching a MIDI device.
        int sum = 0;
        for (int i = checksumOffset; i < frame.size() - 2; ++i)
            sum += static_cast<unsigned char>(frame.at(i));
        frame[frame.size() - 2] = static_cast<char>(sysx->getCheckSum(sum).toInt(nullptr, 16));
        valid &= PatchBackupCodec::validateMessageChecksum(frame);
        const QString key = QString::fromLatin1(
            frame.mid(sysxAddressOffset + 2, 2).toHex()).toUpper();
        serialized.insert(key, QString::fromLatin1(frame.mid(
            sysxDataOffset, frame.size() - sysxDataOffset - 2).toHex()).toUpper());
        offset = end + 1;
    }
    check(error.isEmpty() && valid
          && serialized.size() == sysx->getFileSource().address.size(),
          prefix() + "NORMAL_WRITE_PATH_PRESERVED offline codec");
    check(serialized.value("0B00").mid(0x20 * 2)
              == original.value("0B00").mid(0x20 * 2)
          && serialized.value("0C00") == original.value("0C00"),
          prefix() + "ASSIGN_DATA_PRESERVED after widget edit/rename/serialization");
    check(!sysx->isConnected(), "Offline tests never connect to hardware");
    qInfo() << "Edition tests failures=" << failures;
    return failures ? 1 : 0;
}
