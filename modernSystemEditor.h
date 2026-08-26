#ifndef MODERNSYSTEMEDITOR_H
#define MODERNSYSTEMEDITOR_H

#include <QList>
#include <QWidget>

class QLabel;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class QButtonGroup;
class QComboBox;
class QLineEdit;
class ParameterBar;

class ModernSystemEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit ModernSystemEditor(QWidget *parent = nullptr);
    void refresh(bool backendConnected, bool systemDataReady);

private:
    enum class FieldKind { Selector, Bar };
    struct Field {
        FieldKind kind = FieldKind::Selector;
        QWidget *control = nullptr;
        QComboBox *combo = nullptr;
        ParameterBar *bar = nullptr;
        QString bank;
        QString page;
        QString address;
        bool catalogAvailable = true;
    };
    struct CategoryField {
        QLineEdit *editor = nullptr;
        QString address;
    };
    struct ActiveRangePair {
        ParameterBar *low = nullptr;
        ParameterBar *high = nullptr;
        int minimum = 0;
        int maximum = 127;
    };
    QWidget *createPage(const QString &title);
    QWidget *createSection(QWidget *page, QVBoxLayout *pageLayout,
                           const QString &title);
    void addSelector(QWidget *section, const QString &label,
                     const QString &bank, const QString &page,
                     const QString &address);
    ParameterBar *addBar(QWidget *section, const QString &label,
                         const QString &bank, const QString &page,
                         const QString &address);
    void addControllerSection(QWidget *page, QVBoxLayout *layout,
                              const QString &name,
                              const QString &scopePage,
                              const QString &scopeAddress,
                              const QString &detailBase);
    QWidget *createPlayOptionPage();
    QWidget *createInputPage();
    QWidget *createGlobalEqPage();
    QWidget *createOutputPage();
    QWidget *createControllersPage();
    QWidget *createPhraseLoopPage();
    QWidget *createLcdPage();
    QWidget *createUsbPage();
    QWidget *createCategoryNamesPage();
    bool containsValue(const QString &bank, const QString &page,
                       const QString &address) const;
    int rawValue(const QString &bank, const QString &page,
                 const QString &address) const;
    QString displayForRaw(const QString &bank, const QString &page,
                          const QString &address, int raw) const;
    QString categoryName(const QString &address) const;
    void writeValue(const QString &bank, const QString &page,
                    const QString &address, int raw);
    void commitCategoryName(int index);
    void updateActiveRangeConstraints();
    void selectInputProfile(int profile);
    void refreshInputPage();

    QListWidget *navigation = nullptr;
    QStackedWidget *pages = nullptr;
    QLabel *availability = nullptr;
    QList<Field> fields;
    QList<CategoryField> categories;
    QList<ActiveRangePair> activeRanges;
    QButtonGroup *inputProfileGroup = nullptr;
    ParameterBar *inputLevel = nullptr;
    ParameterBar *inputPresence = nullptr;
    int currentInputProfile = 0;
    bool ready = false;
};

#endif
