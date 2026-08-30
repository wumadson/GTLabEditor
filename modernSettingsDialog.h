#ifndef MODERNSETTINGSDIALOG_H
#define MODERNSETTINGSDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

class ModernSettingsDialog : public QDialog
{
public:
    explicit ModernSettingsDialog(QWidget *parent = 0);

protected:
    void showEvent(QShowEvent *event) override;

private:
    QWidget *createGeneralPage();
    QWidget *createMidiPage();
    QWidget *createWindowPage();
    QWidget *createLanguagePage();
    void choosePatchFolder();
    void resetPatchFolder();
    void refreshMidiDevices();
    void saveSettings();
    void updateDirtyState();
    bool validatePatchFolder(QString *errorMessage) const;
    void populateMidiSelector(QComboBox *selector,
                              const QStringList &devices,
                              int selectedDevice);

    QListWidget *navigation;
    QStackedWidget *pages;
    QLineEdit *patchFolderField;
    QLabel *pathError;
    QPushButton *saveButton;
    QComboBox *midiInputSelector;
    QComboBox *midiOutputSelector;
    QLabel *midiStatus;
    QCheckBox *debugModeCheckBox;
    QSpinBox *midiTimingSpinBox;
    QSpinBox *midiDelaySpinBox;
    QCheckBox *restoreWindowCheckBox;
    QCheckBox *splashScreenCheckBox;
    QComboBox *languageSelector;
    QString initialPatchFolder;
    QString pendingPatchFolder;
    int initialMidiInput;
    int initialMidiOutput;
    bool initialDebugMode;
    int initialMidiTiming;
    int initialMidiDelay;
    bool initialRestoreWindow;
    bool initialSplashScreen;
    int initialLanguage;
};

#endif // MODERNSETTINGSDIALOG_H
