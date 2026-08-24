#ifndef ASSIGNTARGETBROWSER_H
#define ASSIGNTARGETBROWSER_H

#include <QVector>
#include <QWidget>

#include <functional>

class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class AssignTargetBrowser : public QWidget
{
public:
    explicit AssignTargetBrowser(QWidget *parent = nullptr);
    ~AssignTargetBrowser() override;

    void setCurrentTarget(int targetId, const QString &minimum,
                          const QString &maximum, int assignIndex,
                          int loadedBank, int loadedPatch);
    void cancelPreview();

    std::function<void(int, int, int, int, int, int, int)> targetApplied;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Target {
        int id = -1;
        QString name;
        QString category;
        QString bank;
        QString address;
        QString domain;
        QString type;
        int minimum = 0;
        int maximum = 0;
        bool rangeValid = false;
    };

    void loadCatalog();
    QString categoryFor(const Target &target) const;
    QString displayNameFor(const Target &target) const;
    bool isAction(const Target &target) const;
    QString rangeFor(const Target &target) const;
    void openBrowser();
    void rebuildCategories();
    void rebuildResults();
    void previewItem(QListWidgetItem *item);
    void showDetails(const Target &target);
    void selectCurrentTarget();
    void updateActions();
    void applyPreview();

    QPushButton *field = nullptr;
    QFrame *browser = nullptr;
    QLineEdit *search = nullptr;
    QListWidget *categories = nullptr;
    QListWidget *results = nullptr;
    QLabel *idValue = nullptr;
    QLabel *categoryValue = nullptr;
    QLabel *typeValue = nullptr;
    QLabel *rangeValue = nullptr;
    QLabel *minimumValue = nullptr;
    QLabel *maximumValue = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QVector<Target> targets;
    QStringList categoryOrder;
    int currentTargetId = -1;
    int previewTargetId = -1;
    QString currentMinimum;
    QString currentMaximum;
    int contextAssign = -1;
    int contextBank = -1;
    int contextPatch = -1;
    int openedAssign = -1;
    int openedBank = -1;
    int openedPatch = -1;
};

#endif
