#include "modernAboutDialog.h"

#include "modernTheme.h"

#include <QApplication>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
QString paragraph(const QString &text)
{
    return QString("<p>%1</p>").arg(text);
}

QString section(const QString &title, const QString &body)
{
    return QString("<h3>%1</h3>%2").arg(title, paragraph(body));
}
}

ModernAboutDialog::ModernAboutDialog(Page initialPage, QWidget *parent)
    : QDialog(parent), navigation(new QListWidget(this)),
      pages(new QStackedWidget(this))
{
    setWindowTitle(tr("About GT Lab Editor"));
    setModal(true);
    resize(760, 540);
    setMinimumSize(680, 480);
    setStyleSheet(QStringLiteral(
        "QDialog{background:%1;color:%2;}"
        "QFrame#AboutSurface{background:%3;border:1px solid %4;border-radius:6px;}"
        "QListWidget#AboutNavigation{background:%5;border:none;border-right:1px solid %4;"
        "padding:12px 8px;outline:none;color:%6;font-size:10px;font-weight:600;}"
        "QListWidget#AboutNavigation::item{height:34px;padding:0 10px;border-radius:4px;}"
        "QListWidget#AboutNavigation::item:hover{background:%3;color:%2;}"
        "QListWidget#AboutNavigation::item:selected{background:%7;color:%2;}"
        "QLabel#AboutProduct{color:%2;font-size:25px;font-weight:700;}"
        "QLabel#AboutSubtitle{color:%8;font-size:12px;}"
        "QLabel#AboutBuild{color:%9;font-size:10px;font-weight:600;}"
        "QLabel#AboutPageTitle{color:%2;font-size:15px;font-weight:700;letter-spacing:1px;}"
        "QTextBrowser#AboutText{background:transparent;border:none;color:%8;font-size:12px;}"
        "QTextBrowser#AboutText a{color:%10;}"
        "QPushButton{min-width:72px;min-height:28px;padding:0 12px;color:%2;"
        "background:%5;border:1px solid %4;border-radius:4px;font-size:10px;font-weight:600;}"
        "QPushButton:hover{border-color:%10;background:%3;}"
        "QPushButton:pressed{background:%1;}")
        .arg(ModernTheme::color(ModernTheme::ApplicationBackground))
        .arg(ModernTheme::color(ModernTheme::PrimaryText))
        .arg(ModernTheme::color(ModernTheme::ElevatedPanel))
        .arg(ModernTheme::color(ModernTheme::Border))
        .arg(ModernTheme::color(ModernTheme::ControlBackground))
        .arg(ModernTheme::color(ModernTheme::SecondaryText))
        .arg(ModernTheme::color(ModernTheme::AccentCyanDim))
        .arg(ModernTheme::color(ModernTheme::SecondaryText))
        .arg(ModernTheme::color(ModernTheme::DisabledText))
        .arg(ModernTheme::color(ModernTheme::AccentCyan)));

    navigation->setObjectName("AboutNavigation");
    navigation->setFixedWidth(150);
    navigation->addItems({tr("ABOUT"), tr("CREDITS"), tr("LICENSE"),
                          tr("THIRD-PARTY"), tr("SOURCE CODE")});

    pages->addWidget(createAboutPage());
    pages->addWidget(createCreditsPage());
    pages->addWidget(createLicensePage());
    pages->addWidget(createThirdPartyPage());
    pages->addWidget(createSourceCodePage());

    auto *surface = new QFrame(this);
    surface->setObjectName("AboutSurface");
    auto *surfaceLayout = new QHBoxLayout(surface);
    surfaceLayout->setContentsMargins(0, 0, 0, 0);
    surfaceLayout->setSpacing(0);
    surfaceLayout->addWidget(navigation);
    surfaceLayout->addWidget(pages, 1);

    auto *closeButton = new QPushButton(tr("CLOSE"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 14);
    root->setSpacing(12);
    root->addWidget(surface, 1);
    root->addLayout(buttonRow);

    connect(navigation, &QListWidget::currentRowChanged,
            pages, &QStackedWidget::setCurrentIndex);
    setCurrentPage(initialPage);
}

void ModernAboutDialog::setCurrentPage(Page page)
{
    const int index = qBound(0, static_cast<int>(page), pages->count() - 1);
    navigation->setCurrentRow(index);
    pages->setCurrentIndex(index);
}

QWidget *ModernAboutDialog::createAboutPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 25, 28, 24);
    layout->setSpacing(6);

    auto *product = new QLabel("GT Lab Editor", page);
    product->setObjectName("AboutProduct");
    auto *subtitle = new QLabel(tr("Modern editor for the BOSS GT-10"), page);
    subtitle->setObjectName("AboutSubtitle");
    const QString applicationVersion = qApp->applicationVersion().trimmed();
    auto *build = new QLabel(applicationVersion.isEmpty()
                                 ? tr("Development Build")
                                 : tr("Version %1").arg(applicationVersion), page);
    build->setObjectName("AboutBuild");

    auto *text = new QTextBrowser(page);
    text->setObjectName("AboutText");
    text->setOpenExternalLinks(true);
    text->setHtml(
        paragraph(tr("GT Lab Editor is derived from the open-source "
                     "GT-10 FxFloorBoard / FxFloorBoard project.")) +
        section(tr("ORIGINAL GT-10 FXFLOORBOARD"),
                tr("Colin Willcocks<br>Copyright 2007–2010")) +
        section(tr("ORIGINAL FXFLOORBOARD"),
                tr("Uco Mesdag<br>Copyright 2005–2007")) +
        section(tr("GT LAB MODERNIZATION"), tr("Wumadson Cardoso")) +
        section(tr("LICENSE"),
                tr("GNU GPL family / licensing information. License information "
                   "is being consolidated; see License and Third-Party Software.")) +
        // TODO: Review trademark wording before a public release.
        paragraph(tr("GT Lab Editor is an independent community project and is "
                     "not affiliated with or endorsed by BOSS Corporation or "
                     "Roland Corporation.")));

    auto *links = new QHBoxLayout;
    const QList<QPair<QString, Page>> buttons = {
        {tr("LICENSE"), LicensePage}, {tr("CREDITS"), CreditsPage},
        {tr("THIRD-PARTY"), ThirdPartyPage}, {tr("SOURCE CODE"), SourceCodePage}};
    for (const auto &entry : buttons) {
        auto *button = new QPushButton(entry.first, page);
        connect(button, &QPushButton::clicked, this,
                [this, entry]() { setCurrentPage(entry.second); });
        links->addWidget(button);
    }
    links->addStretch();

    layout->addWidget(product);
    layout->addWidget(subtitle);
    layout->addWidget(build);
    layout->addSpacing(10);
    layout->addWidget(text, 1);
    layout->addLayout(links);
    return page;
}

QWidget *ModernAboutDialog::createCreditsPage()
{
    const QString html =
        section(tr("ORIGINAL GT-10 FXFLOORBOARD"), tr("Colin Willcocks")) +
        section(tr("ORIGINAL FXFLOORBOARD"), tr("Uco Mesdag")) +
        section(tr("RTMIDI"), tr("Gary P. Scavone")) +
        section(tr("OSDAB XMLWRITER"), tr("Fabrizio Angius")) +
        section(tr("GT LAB MODERNIZATION"), tr("Wumadson Cardoso")) +
        section(tr("ADDITIONAL CONTRIBUTORS"), tr("See source history."));
    return createTextPage(tr("CREDITS"), html);
}

QWidget *ModernAboutDialog::createLicensePage()
{
    QFile file(":license.txt");
    QString licenseText;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        licenseText = QString::fromUtf8(file.readAll());
    else
        licenseText = tr("The bundled license text could not be loaded.");

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);
    auto *title = new QLabel(tr("LICENSE"), page);
    title->setObjectName("AboutPageTitle");
    auto *notice = new QLabel(
        tr("License information is being consolidated. Source files declare "
           "GPL v2 or later; bundled components may have additional licensing terms."),
        page);
    notice->setObjectName("AboutSubtitle");
    notice->setWordWrap(true);
    auto *viewer = new QTextBrowser(page);
    viewer->setObjectName("AboutText");
    viewer->setPlainText(licenseText);
    layout->addWidget(title);
    layout->addWidget(notice);
    layout->addWidget(viewer, 1);
    return page;
}

QWidget *ModernAboutDialog::createThirdPartyPage()
{
    const QString qtVersion = QString::fromLatin1(qVersion());
    const QString html =
        section(tr("QT %1").arg(qtVersion),
                tr("This application is built with Qt. Distribution licensing "
                   "details must accompany the deployment selected for each release; "
                   "this build does not assert LGPL, GPL, or commercial licensing.")) +
        section(tr("RTMIDI 1.0.11"),
                tr("Realtime MIDI I/O C++ classes by Gary P. Scavone. "
                   "Copyright 2003–2010. The copyright and permission notice "
                   "contained in the source must be preserved.")) +
        section(tr("OSDAB XMLWRITER"),
                tr("XML generation component by Fabrizio Angius. Its component "
                   "header declares GNU GPL version 2.")) +
        paragraph(tr("Project licensing information is being consolidated. The "
                     "bundled license text and component-specific notices remain "
                     "available and have not been reclassified."));
    return createTextPage(tr("THIRD-PARTY SOFTWARE"), html);
}

QWidget *ModernAboutDialog::createSourceCodePage()
{
    const QString html =
        section(tr("MODERN SOURCE"),
                QStringLiteral("<a href=\"https://github.com/wumadson/"
                               "GT10-FXFloorBoard-Modern\">"
                               "github.com/wumadson/GT10-FXFloorBoard-Modern</a>")) +
        section(tr("ORIGINAL PROJECT"),
                QStringLiteral("<a href=\"https://git.code.sf.net/p/"
                               "fxfloorboard/fxfloorboard\">"
                               "git.code.sf.net/p/fxfloorboard/fxfloorboard</a>")) +
        paragraph(tr("The original project remains part of the GT Lab Editor "
                     "history and attribution."));
    return createTextPage(tr("SOURCE CODE"), html);
}

QWidget *ModernAboutDialog::createTextPage(const QString &title,
                                           const QString &html)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);
    auto *heading = new QLabel(title, page);
    heading->setObjectName("AboutPageTitle");
    auto *text = new QTextBrowser(page);
    text->setObjectName("AboutText");
    text->setOpenExternalLinks(true);
    text->setHtml(html);
    layout->addWidget(heading);
    layout->addWidget(text, 1);
    return page;
}
