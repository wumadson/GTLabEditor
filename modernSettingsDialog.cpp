#include "modernSettingsDialog.h"

#include "Preferences.h"
#include "midiIO.h"
#include "modernTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

int storedDeviceIndex(const QString &value)
{
    if (value.isEmpty())
        return -1;
    bool ok = false;
    const int index = value.toInt(&ok, 10);
    return ok ? index : -1;
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setObjectName("settingsFieldLabel");
    return label;
}

}

ModernSettingsDialog::ModernSettingsDialog(QWidget *parent)
    : QDialog(parent),
      navigation(new QListWidget(this)),
      pages(new QStackedWidget(this)),
      patchFolderField(0),
      pathError(0),
      saveButton(new QPushButton(tr("SAVE"), this)),
      midiInputSelector(0),
      midiOutputSelector(0),
      midiStatus(0),
      debugModeCheckBox(0),
      midiTimingSpinBox(0),
      midiDelaySpinBox(0),
      restoreWindowCheckBox(0),
      splashScreenCheckBox(0),
      languageSelector(0)
{
    setWindowTitle(tr("GT Lab Editor — Settings"));
    resize(820, 540);
    setMinimumSize(700, 480);
    setModal(true);

    Preferences *preferences = Preferences::Instance();
    initialPatchFolder = preferences->getPreferences("General", "Files", "dir");
    pendingPatchFolder = initialPatchFolder;
    initialMidiInput = storedDeviceIndex(
        preferences->getPreferences("Midi", "MidiIn", "device"));
    initialMidiOutput = storedDeviceIndex(
        preferences->getPreferences("Midi", "MidiOut", "device"));
    initialDebugMode = preferences->getPreferences("Midi", "DBug", "bool") == "true";
    initialMidiTiming = qBound(
        1, preferences->getPreferences("Midi", "Time", "set").toInt(), 99);
    initialMidiDelay = qBound(
        1, preferences->getPreferences("Midi", "Delay", "set").toInt(), 20);
    initialRestoreWindow = preferences->getPreferences("Window", "Restore", "window") == "true";
    initialSplashScreen = preferences->getPreferences("Window", "Splash", "bool") == "true";
    initialLanguage = preferences->getPreferences("Language", "Locale", "select").toInt();

    navigation->setObjectName("settingsNavigation");
    navigation->setFixedWidth(172);
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setFocusPolicy(Qt::NoFocus);
    navigation->addItem(tr("GENERAL"));
    navigation->addItem(tr("USB / MIDI"));
    navigation->addItem(tr("WINDOW"));
    navigation->addItem(tr("LANGUAGE"));

    pages->addWidget(createGeneralPage());
    pages->addWidget(createMidiPage());
    pages->addWidget(createWindowPage());
    pages->addWidget(createLanguagePage());

    connect(navigation, &QListWidget::currentRowChanged,
            pages, &QStackedWidget::setCurrentIndex);

    QPushButton *cancelButton = new QPushButton(tr("CANCEL"), this);
    cancelButton->setObjectName("settingsSecondaryButton");
    saveButton->setObjectName("settingsPrimaryButton");
    saveButton->setEnabled(false);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked,
            this, &ModernSettingsDialog::saveSettings);

    QHBoxLayout *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addStretch();
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(saveButton);

    QVBoxLayout *contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(24, 22, 24, 20);
    contentLayout->setSpacing(16);
    contentLayout->addWidget(pages, 1);
    contentLayout->addLayout(buttonRow);

    QWidget *content = new QWidget(this);
    content->setObjectName("settingsContent");
    content->setLayout(contentLayout);

    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(navigation);
    rootLayout->addWidget(content, 1);

    const QString background = ModernTheme::color(ModernTheme::ApplicationBackground);
    const QString panel = ModernTheme::color(ModernTheme::Panel);
    const QString elevated = ModernTheme::color(ModernTheme::ElevatedPanel);
    const QString control = ModernTheme::color(ModernTheme::ControlBackground);
    const QString border = ModernTheme::color(ModernTheme::Border);
    const QString primary = ModernTheme::color(ModernTheme::PrimaryText);
    const QString secondary = ModernTheme::color(ModernTheme::SecondaryText);
    const QString accent = ModernTheme::color(ModernTheme::AccentCyan);
    const QString danger = ModernTheme::color(ModernTheme::DangerRed);

    setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QWidget#settingsContent { background: %1; }"
        "QListWidget#settingsNavigation { background: %3; border: none;"
        " border-right: 1px solid %4; padding: 18px 10px; outline: none; }"
        "QListWidget#settingsNavigation::item { color: %5; height: 38px;"
        " padding-left: 12px; border-radius: 7px; font-size: 11px; font-weight: 600; }"
        "QListWidget#settingsNavigation::item:hover { background: %6; color: %2; }"
        "QListWidget#settingsNavigation::item:selected { background: %7; color: %2; }"
        "QLabel#settingsPageTitle { color: %2; font-size: 20px; font-weight: 600; }"
        "QLabel#settingsSectionTitle { color: %5; font-size: 10px; font-weight: 600; }"
        "QLabel#settingsFieldLabel { color: %2; font-size: 12px; font-weight: 600; }"
        "QLabel#settingsDescription { color: %5; font-size: 11px; }"
        "QLabel#settingsStatus { color: %5; font-size: 10px; }"
        "QLabel#settingsError { color: %9; font-size: 11px; }"
        "QWidget#settingsSection { background: %3; border: 1px solid %4; border-radius: 10px; }"
        "QLineEdit { background: %10; color: %2; border: 1px solid %4;"
        " border-radius: 7px; padding: 0 11px; min-height: 34px; }"
        "QComboBox, QSpinBox { background: %10; color: %2; border: 1px solid %4;"
        " border-radius: 7px; padding: 0 10px; min-height: 34px; }"
        "QComboBox:focus, QSpinBox:focus { border-color: %8; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background: %3; color: %2; border: 1px solid %4;"
        " selection-background-color: %7; selection-color: %2; outline: none; }"
        "QCheckBox { color: %2; spacing: 9px; font-size: 11px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %4;"
        " border-radius: 4px; background: %10; }"
        "QCheckBox::indicator:checked { background: %8; border-color: %8; }"
        "QToolButton#settingsDisclosure { color: %2; background: transparent; border: none;"
        " min-height: 28px; padding: 0; font-size: 10px; font-weight: 600; text-align: left; }"
        "QPushButton { min-height: 32px; padding: 0 14px; border-radius: 7px;"
        " background: %6; color: %2; border: 1px solid %4; font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { border-color: %8; }"
        "QPushButton:pressed { background: %7; }"
        "QPushButton#settingsPrimaryButton { background: %8; color: %1; border-color: %8; }"
        "QPushButton#settingsPrimaryButton:disabled { background: %6; color: %5; border-color: %4; }"
        "QPushButton#settingsSecondaryButton { background: transparent; }"
    ).arg(background)
     .arg(primary)
     .arg(panel)
     .arg(border)
     .arg(secondary)
     .arg(elevated)
     .arg(ModernTheme::color(ModernTheme::AccentCyanDim))
     .arg(accent)
     .arg(danger)
     .arg(control));

    navigation->setCurrentRow(0);
    refreshMidiDevices();
    updateDirtyState();
}

void ModernSettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    ModernTheme::applyWindowsDarkTitleBar(this);
}

QWidget *ModernSettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(18);

    QLabel *title = new QLabel(tr("GENERAL"), page);
    title->setObjectName("settingsPageTitle");
    pageLayout->addWidget(title);

    QWidget *section = new QWidget(page);
    section->setObjectName("settingsSection");
    QVBoxLayout *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(18, 16, 18, 18);
    sectionLayout->setSpacing(10);

    QLabel *sectionTitle = new QLabel(tr("FILES"), section);
    sectionTitle->setObjectName("settingsSectionTitle");
    QLabel *fieldLabel = new QLabel(tr("Default Patch Folder"), section);
    fieldLabel->setObjectName("settingsFieldLabel");
    QLabel *description = new QLabel(
        tr("Folder initially used when opening or saving patch files."), section);
    description->setObjectName("settingsDescription");

    patchFolderField = new QLineEdit(section);
    patchFolderField->setReadOnly(true);
    patchFolderField->setText(pendingPatchFolder);
    patchFolderField->setPlaceholderText(tr("Application default"));

    QPushButton *chooseButton = new QPushButton(tr("CHOOSE…"), section);
    QPushButton *resetButton = new QPushButton(tr("RESET TO DEFAULT"), section);
    connect(chooseButton, &QPushButton::clicked,
            this, &ModernSettingsDialog::choosePatchFolder);
    connect(resetButton, &QPushButton::clicked,
            this, &ModernSettingsDialog::resetPatchFolder);

    QHBoxLayout *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(8);
    pathRow->addWidget(patchFolderField, 1);
    pathRow->addWidget(chooseButton);

    QHBoxLayout *resetRow = new QHBoxLayout;
    resetRow->setContentsMargins(0, 0, 0, 0);
    resetRow->addStretch();
    resetRow->addWidget(resetButton);

    pathError = new QLabel(section);
    pathError->setObjectName("settingsError");
    pathError->setWordWrap(true);
    pathError->hide();

    sectionLayout->addWidget(sectionTitle);
    sectionLayout->addWidget(fieldLabel);
    sectionLayout->addWidget(description);
    sectionLayout->addLayout(pathRow);
    sectionLayout->addWidget(pathError);
    sectionLayout->addLayout(resetRow);

    pageLayout->addWidget(section);
    pageLayout->addStretch();
    return page;
}

QWidget *ModernSettingsDialog::createMidiPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(14);

    QLabel *title = new QLabel(tr("USB / MIDI"), page);
    title->setObjectName("settingsPageTitle");
    pageLayout->addWidget(title);

    QWidget *connectionSection = new QWidget(page);
    connectionSection->setObjectName("settingsSection");
    QVBoxLayout *connectionLayout = new QVBoxLayout(connectionSection);
    connectionLayout->setContentsMargins(18, 16, 18, 18);
    connectionLayout->setSpacing(9);

    QLabel *sectionTitle = new QLabel(tr("MIDI CONNECTION"), connectionSection);
    sectionTitle->setObjectName("settingsSectionTitle");
    midiInputSelector = new QComboBox(connectionSection);
    midiOutputSelector = new QComboBox(connectionSection);

    connectionLayout->addWidget(sectionTitle);
    connectionLayout->addWidget(fieldLabel(tr("MIDI INPUT"), connectionSection));
    connectionLayout->addWidget(midiInputSelector);
    connectionLayout->addWidget(fieldLabel(tr("MIDI OUTPUT"), connectionSection));
    connectionLayout->addWidget(midiOutputSelector);

    QPushButton *refreshButton = new QPushButton(tr("REFRESH DEVICES"), connectionSection);
    QHBoxLayout *refreshRow = new QHBoxLayout;
    refreshRow->setContentsMargins(0, 2, 0, 0);
    refreshRow->addWidget(refreshButton);
    refreshRow->addStretch();
    connectionLayout->addLayout(refreshRow);

    midiStatus = new QLabel(connectionSection);
    midiStatus->setObjectName("settingsStatus");
    midiStatus->setWordWrap(true);
    connectionLayout->addWidget(midiStatus);

    QLabel *indexWarning = new QLabel(
        tr("Ports are stored by CoreMIDI index; device order can change between sessions."),
        connectionSection);
    indexWarning->setObjectName("settingsDescription");
    indexWarning->setWordWrap(true);
    connectionLayout->addWidget(indexWarning);
    QLabel *restartNotice = new QLabel(
        tr("Saved MIDI port changes take effect through the existing startup connection flow."),
        connectionSection);
    restartNotice->setObjectName("settingsDescription");
    restartNotice->setWordWrap(true);
    connectionLayout->addWidget(restartNotice);

    QToolButton *advancedButton = new QToolButton(page);
    advancedButton->setObjectName("settingsDisclosure");
    advancedButton->setText(tr("ADVANCED"));
    advancedButton->setCheckable(true);
    advancedButton->setChecked(false);
    advancedButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advancedButton->setArrowType(Qt::RightArrow);

    QWidget *advancedSection = new QWidget(page);
    advancedSection->setObjectName("settingsSection");
    QGridLayout *advancedLayout = new QGridLayout(advancedSection);
    advancedLayout->setContentsMargins(18, 16, 18, 18);
    advancedLayout->setHorizontalSpacing(18);
    advancedLayout->setVerticalSpacing(10);

    debugModeCheckBox = new QCheckBox(tr("Enable diagnostic MIDI logging"), advancedSection);
    debugModeCheckBox->setChecked(initialDebugMode);
    debugModeCheckBox->setToolTip(tr("Legacy preference: Midi:DBug:bool"));

    midiTimingSpinBox = new QSpinBox(advancedSection);
    midiTimingSpinBox->setRange(1, 99);
    midiTimingSpinBox->setValue(initialMidiTiming);
    midiTimingSpinBox->setSuffix(tr(" × 10 ms"));
    midiTimingSpinBox->setToolTip(tr("Legacy preference: Midi:Time:set"));

    midiDelaySpinBox = new QSpinBox(advancedSection);
    midiDelaySpinBox->setRange(1, 20);
    midiDelaySpinBox->setValue(initialMidiDelay);
    midiDelaySpinBox->setSuffix(tr(" times/second"));
    midiDelaySpinBox->setToolTip(tr("Legacy preference: Midi:Delay:set"));

    advancedLayout->addWidget(fieldLabel(tr("DEBUG MODE"), advancedSection), 0, 0);
    advancedLayout->addWidget(debugModeCheckBox, 1, 0, 1, 2);
    advancedLayout->addWidget(fieldLabel(tr("TIMING"), advancedSection), 2, 0);
    advancedLayout->addWidget(fieldLabel(tr("RECEIVE DELAY / TIMEOUT"), advancedSection), 2, 1);
    advancedLayout->addWidget(midiTimingSpinBox, 3, 0);
    advancedLayout->addWidget(midiDelaySpinBox, 3, 1);
    advancedLayout->setColumnStretch(0, 1);
    advancedLayout->setColumnStretch(1, 1);
    advancedSection->setVisible(false);

    connect(advancedButton, &QToolButton::toggled,
            this, [advancedButton, advancedSection](bool expanded) {
        advancedButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        advancedSection->setVisible(expanded);
    });
    connect(refreshButton, &QPushButton::clicked,
            this, &ModernSettingsDialog::refreshMidiDevices);
    connect(midiInputSelector,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateDirtyState(); });
    connect(midiOutputSelector,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateDirtyState(); });
    connect(debugModeCheckBox, &QCheckBox::toggled,
            this, [this](bool) { updateDirtyState(); });
    connect(midiTimingSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [this](int) { updateDirtyState(); });
    connect(midiDelaySpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [this](int) { updateDirtyState(); });

    pageLayout->addWidget(connectionSection);
    pageLayout->addWidget(advancedButton);
    pageLayout->addWidget(advancedSection);
    pageLayout->addStretch();
    return page;
}

QWidget *ModernSettingsDialog::createWindowPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(18);

    QLabel *title = new QLabel(tr("WINDOW"), page);
    title->setObjectName("settingsPageTitle");
    pageLayout->addWidget(title);

    QWidget *section = new QWidget(page);
    section->setObjectName("settingsSection");
    QVBoxLayout *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(18, 16, 18, 18);
    sectionLayout->setSpacing(12);

    QLabel *sectionTitle = new QLabel(tr("STARTUP"), section);
    sectionTitle->setObjectName("settingsSectionTitle");
    restoreWindowCheckBox = new QCheckBox(
        tr("Restore window position and size"), section);
    splashScreenCheckBox = new QCheckBox(tr("Show splash screen"), section);
    restoreWindowCheckBox->setChecked(initialRestoreWindow);
    splashScreenCheckBox->setChecked(initialSplashScreen);

    connect(restoreWindowCheckBox, &QCheckBox::toggled,
            this, [this](bool) { updateDirtyState(); });
    connect(splashScreenCheckBox, &QCheckBox::toggled,
            this, [this](bool) { updateDirtyState(); });

    sectionLayout->addWidget(sectionTitle);
    sectionLayout->addWidget(restoreWindowCheckBox);
    sectionLayout->addWidget(splashScreenCheckBox);
    pageLayout->addWidget(section);
    pageLayout->addStretch();
    return page;
}

QWidget *ModernSettingsDialog::createLanguagePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(18);

    QLabel *title = new QLabel(tr("LANGUAGE"), page);
    title->setObjectName("settingsPageTitle");
    pageLayout->addWidget(title);

    QWidget *section = new QWidget(page);
    section->setObjectName("settingsSection");
    QVBoxLayout *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(18, 16, 18, 18);
    sectionLayout->setSpacing(10);

    QLabel *sectionTitle = new QLabel(tr("APPLICATION LANGUAGE"), section);
    sectionTitle->setObjectName("settingsSectionTitle");
    languageSelector = new QComboBox(section);
    languageSelector->addItem(tr("English"), 0);
    languageSelector->addItem(tr("French"), 1);
    languageSelector->addItem(tr("German"), 2);
    languageSelector->addItem(tr("Chinese Simplified"), 3);
    languageSelector->setCurrentIndex(qBound(0, initialLanguage, 3));

    QLabel *restartNotice = new QLabel(
        tr("Language changes take effect after restarting GT Lab Editor."), section);
    restartNotice->setObjectName("settingsDescription");
    QLabel *coverageNotice = new QLabel(
        tr("Some modern interface text may not yet be translated."), section);
    coverageNotice->setObjectName("settingsDescription");

    connect(languageSelector,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateDirtyState(); });

    sectionLayout->addWidget(sectionTitle);
    sectionLayout->addWidget(languageSelector);
    sectionLayout->addWidget(restartNotice);
    sectionLayout->addWidget(coverageNotice);
    pageLayout->addWidget(section);
    pageLayout->addStretch();
    return page;
}

void ModernSettingsDialog::populateMidiSelector(QComboBox *selector,
                                                 const QStringList &devices,
                                                 int selectedDevice)
{
    const QSignalBlocker blocker(selector);
    selector->clear();
    selector->addItem(tr("Select MIDI device"), -1);
    for (int index = 0; index < devices.size(); ++index)
        selector->addItem(devices.at(index), index);

    int comboIndex = selector->findData(selectedDevice);
    if (selectedDevice >= 0 && comboIndex < 0) {
        selector->addItem(
            tr("Unavailable — saved device index %1").arg(selectedDevice),
            selectedDevice);
        comboIndex = selector->count() - 1;
    }
    selector->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
}

void ModernSettingsDialog::refreshMidiDevices()
{
    const int selectedInput = midiInputSelector->count()
        ? midiInputSelector->currentData().toInt() : initialMidiInput;
    const int selectedOutput = midiOutputSelector->count()
        ? midiOutputSelector->currentData().toInt() : initialMidiOutput;

    midiIO enumerator;
    const QStringList inputs = enumerator.getMidiInDevices();
    const QStringList outputs = enumerator.getMidiOutDevices();
    populateMidiSelector(midiInputSelector, inputs, selectedInput);
    populateMidiSelector(midiOutputSelector, outputs, selectedOutput);

    midiStatus->setText(tr("%1 input device(s) · %2 output device(s). Refresh does not change the active session.")
                        .arg(inputs.size()).arg(outputs.size()));
    updateDirtyState();
}

void ModernSettingsDialog::choosePatchFolder()
{
    QString startDirectory = pendingPatchFolder;
    if (startDirectory.isEmpty() || !QFileInfo(startDirectory).isDir())
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDirectory.isEmpty())
        startDirectory = QDir::homePath();

    const QString selected = QFileDialog::getExistingDirectory(
        this, tr("Choose Default Patch Folder"), startDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selected.isEmpty())
        return;

    pendingPatchFolder = QDir::cleanPath(selected);
    patchFolderField->setText(pendingPatchFolder);
    pathError->hide();
    updateDirtyState();
}

void ModernSettingsDialog::resetPatchFolder()
{
    pendingPatchFolder.clear();
    patchFolderField->clear();
    pathError->hide();
    updateDirtyState();
}

void ModernSettingsDialog::saveSettings()
{
    QString errorMessage;
    if (!validatePatchFolder(&errorMessage)) {
        pathError->setText(errorMessage);
        pathError->show();
        return;
    }

    Preferences *preferences = Preferences::Instance();
    if (pendingPatchFolder != initialPatchFolder)
        preferences->setPreferences("General", "Files", "dir", pendingPatchFolder);

    const int midiInput = midiInputSelector->currentData().toInt();
    const int midiOutput = midiOutputSelector->currentData().toInt();
    if (midiInput != initialMidiInput)
        preferences->setPreferences("Midi", "MidiIn", "device",
                                    midiInput < 0 ? QString() : QString::number(midiInput));
    if (midiOutput != initialMidiOutput)
        preferences->setPreferences("Midi", "MidiOut", "device",
                                    midiOutput < 0 ? QString() : QString::number(midiOutput));
    if (debugModeCheckBox->isChecked() != initialDebugMode)
        preferences->setPreferences("Midi", "DBug", "bool",
                                    debugModeCheckBox->isChecked() ? "true" : "false");
    if (midiTimingSpinBox->value() != initialMidiTiming)
        preferences->setPreferences("Midi", "Time", "set",
                                    QString::number(midiTimingSpinBox->value()));
    if (midiDelaySpinBox->value() != initialMidiDelay)
        preferences->setPreferences("Midi", "Delay", "set",
                                    QString::number(midiDelaySpinBox->value()));
    if (restoreWindowCheckBox->isChecked() != initialRestoreWindow)
        preferences->setPreferences("Window", "Restore", "window",
                                    restoreWindowCheckBox->isChecked() ? "true" : "false");
    if (splashScreenCheckBox->isChecked() != initialSplashScreen)
        preferences->setPreferences("Window", "Splash", "bool",
                                    splashScreenCheckBox->isChecked() ? "true" : "false");
    if (languageSelector->currentData().toInt() != initialLanguage)
        preferences->setPreferences("Language", "Locale", "select",
                                    QString::number(languageSelector->currentData().toInt()));
    preferences->savePreferences();
    accept();
}

void ModernSettingsDialog::updateDirtyState()
{
    if (!midiInputSelector || !midiOutputSelector || !debugModeCheckBox ||
        !midiTimingSpinBox || !midiDelaySpinBox || !restoreWindowCheckBox ||
        !splashScreenCheckBox || !languageSelector) {
        saveButton->setEnabled(pendingPatchFolder != initialPatchFolder);
        return;
    }

    const bool dirty = pendingPatchFolder != initialPatchFolder ||
        midiInputSelector->currentData().toInt() != initialMidiInput ||
        midiOutputSelector->currentData().toInt() != initialMidiOutput ||
        debugModeCheckBox->isChecked() != initialDebugMode ||
        midiTimingSpinBox->value() != initialMidiTiming ||
        midiDelaySpinBox->value() != initialMidiDelay ||
        restoreWindowCheckBox->isChecked() != initialRestoreWindow ||
        splashScreenCheckBox->isChecked() != initialSplashScreen ||
        languageSelector->currentData().toInt() != initialLanguage;
    saveButton->setEnabled(dirty);
}

bool ModernSettingsDialog::validatePatchFolder(QString *errorMessage) const
{
    if (pendingPatchFolder.isEmpty())
        return true;

    const QFileInfo info(pendingPatchFolder);
    if (!info.exists()) {
        *errorMessage = tr("The selected folder does not exist.");
        return false;
    }
    if (!info.isDir()) {
        *errorMessage = tr("The selected path is not a folder.");
        return false;
    }
    if (!info.isWritable()) {
        *errorMessage = tr("The selected folder is not writable.");
        return false;
    }
    return true;
}
