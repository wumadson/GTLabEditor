#ifndef MODERNABOUTDIALOG_H
#define MODERNABOUTDIALOG_H

#include <QDialog>

class QListWidget;
class QStackedWidget;

class ModernAboutDialog final : public QDialog
{
    Q_OBJECT

public:
    enum Page { AboutPage, CreditsPage, LicensePage, ThirdPartyPage,
                SourceCodePage };

    explicit ModernAboutDialog(Page initialPage = AboutPage,
                               QWidget *parent = nullptr);

    void setCurrentPage(Page page);

private:
    QWidget *createAboutPage();
    QWidget *createCreditsPage();
    QWidget *createLicensePage();
    QWidget *createThirdPartyPage();
    QWidget *createSourceCodePage();
    QWidget *createTextPage(const QString &title, const QString &html);

    QListWidget *navigation;
    QStackedWidget *pages;
};

#endif
