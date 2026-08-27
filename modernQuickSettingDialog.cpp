#include "modernQuickSettingDialog.h"

#include "modernTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

ModernQuickSettingDialog::ModernQuickSettingDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("QUICK SETTINGS"));
    setModal(false);
    setMinimumWidth(440);
    setMaximumWidth(520);
    setSizeGripEnabled(false);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel#QuickSettingDialogTitle { color: %2; font-size: 12px; "
        "font-weight: 700; letter-spacing: 1px; }"
        "QLabel#QuickSettingDialogEffect { color: %3; font-size: 11px; "
        "font-weight: 600; letter-spacing: 0.8px; }"
        "QComboBox { min-height: 26px; color: %2; background: %4; "
        "border: 1px solid %5; border-radius: 5px; padding: 0 9px; }"
        "QComboBox:hover, QComboBox:focus { border-color: %6; }"
        "QComboBox QAbstractItemView { color: %2; background: %4; "
        "border: 1px solid %5; selection-background-color: %7; "
        "selection-color: #FFFFFF; outline: 0; }"
        "QPushButton { min-width: 68px; min-height: 26px; color: %2; "
        "background: %4; border: 1px solid %5; border-radius: 5px; "
        "font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { border-color: %6; }"
        "QPushButton:pressed { background: #090D11; }"
        "QPushButton:disabled { color: %8; border-color: %9; }")
        .arg(ModernTheme::color(ModernTheme::ElevatedPanel),
             ModernTheme::color(ModernTheme::PrimaryText),
             ModernTheme::color(ModernTheme::SecondaryText),
             ModernTheme::color(ModernTheme::Panel),
             ModernTheme::color(ModernTheme::Border),
             ModernTheme::color(ModernTheme::AccentCyan),
             ModernTheme::color(ModernTheme::AccentCyanDim),
             ModernTheme::color(ModernTheme::DisabledText),
             ModernTheme::color(ModernTheme::BorderSubtle)));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 20);
    root->setSpacing(10);

    auto *heading = new QHBoxLayout;
    heading->setSpacing(8);
    auto *title = new QLabel(tr("QUICK SETTINGS"), this);
    title->setObjectName("QuickSettingDialogTitle");
    heading->addWidget(title);
    heading->addStretch(1);
    auto *closeButton = new QPushButton(tr("CLOSE"), this);
    closeButton->setFixedSize(62, 28);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    heading->addWidget(closeButton);
    root->addLayout(heading);

    effectLabel = new QLabel(this);
    effectLabel->setObjectName("QuickSettingDialogEffect");
    root->addWidget(effectLabel);

    auto *rule = new QFrame(this);
    rule->setFixedHeight(1);
    rule->setStyleSheet("background: "
                        + ModernTheme::color(ModernTheme::BorderSubtle)
                        + "; border: none;");
    root->addWidget(rule);

    pages = new QStackedWidget(this);
    pages->setMinimumHeight(72);
    root->addWidget(pages);
}

void ModernQuickSettingDialog::addEffectPage(QuickSettingEffect effect,
                                              const QString &effectName,
                                              QWidget *page)
{
    if (!page || pageIndexes.contains(static_cast<int>(effect)))
        return;
    pageIndexes.insert(static_cast<int>(effect), pages->addWidget(page));
    effectNames.insert(static_cast<int>(effect), effectName);
}

void ModernQuickSettingDialog::showEffect(QuickSettingEffect effect)
{
    const int key = static_cast<int>(effect);
    if (!pageIndexes.contains(key))
        return;
    pages->setCurrentIndex(pageIndexes.value(key));
    effectLabel->setText(effectNames.value(key));
    adjustSize();
    show();
    raise();
    activateWindow();
}
