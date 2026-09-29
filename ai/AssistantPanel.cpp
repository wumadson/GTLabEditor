#include "ai/AssistantPanel.h"

#include "modernTheme.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace Ai {

AssistantPanel::AssistantPanel(AssistantSession *session, QWidget *parent)
    : QWidget(parent), assistantSession(session)
{
    setObjectName(QStringLiteral("AssistantPanel"));
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setStyleSheet(ModernTheme::applicationStyleSheet());

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("AssistantHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(10, 8, 8, 8);
    headerLayout->setSpacing(8);
    auto *titleColumn = new QVBoxLayout;
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(2);
    auto *title = new QLabel(tr("GT LAB ASSISTANT"), header);
    title->setObjectName(QStringLiteral("AssistantTitle"));
    providerNameLabel = new QLabel(tr("Local GT-10 · Offline · No account required"),
                                   header);
    providerNameLabel->setObjectName(QStringLiteral("AssistantProvider"));
    providerNameLabel->setWordWrap(true);
    titleColumn->addWidget(title);
    titleColumn->addWidget(providerNameLabel);
    headerLayout->addLayout(titleColumn, 1);
    auto *close = new QPushButton(QStringLiteral("×"), header);
    close->setObjectName(QStringLiteral("AssistantCloseButton"));
    close->setToolTip(tr("Close GT Lab Assistant"));
    close->setFixedSize(26, 26);
    connect(close, &QPushButton::clicked,
            this, &AssistantPanel::closeRequested);
    headerLayout->addWidget(close, 0, Qt::AlignTop);
    root->addWidget(header);

    auto *contextFrame = new QFrame(this);
    contextFrame->setObjectName(QStringLiteral("AssistantContext"));
    auto *contextLayout = new QVBoxLayout(contextFrame);
    contextLayout->setContentsMargins(10, 7, 10, 7);
    contextLayout->setSpacing(2);
    auto *contextCaption = new QLabel(tr("CONTEXT"), contextFrame);
    contextCaption->setObjectName(QStringLiteral("AssistantContextCaption"));
    contextLabel = new QLabel(tr("Patch context unavailable"), contextFrame);
    contextLabel->setObjectName(QStringLiteral("AssistantContextValue"));
    contextLabel->setWordWrap(true);
    contextLayout->addWidget(contextCaption);
    contextLayout->addWidget(contextLabel);
    root->addWidget(contextFrame);

    conversationScroll = new QScrollArea(this);
    conversationScroll->setObjectName(QStringLiteral("AssistantConversation"));
    conversationScroll->setWidgetResizable(true);
    conversationScroll->setFrameShape(QFrame::NoFrame);
    conversationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    conversationContent = new QWidget;
    conversationContent->setObjectName(QStringLiteral("AssistantConversationContent"));
    conversationLayout = new QVBoxLayout(conversationContent);
    conversationLayout->setContentsMargins(4, 4, 4, 4);
    conversationLayout->setSpacing(8);
    conversationLayout->addStretch(1);
    conversationScroll->setWidget(conversationContent);
    root->addWidget(conversationScroll, 1);

    activityLabel = new QLabel(this);
    activityLabel->setObjectName(QStringLiteral("AssistantActivity"));
    activityLabel->hide();
    root->addWidget(activityLabel);

    promptInput = new QPlainTextEdit(this);
    promptInput->setObjectName(QStringLiteral("AssistantPrompt"));
    promptInput->setPlaceholderText(
        tr("Ask about your tone, patch, GT-10 settings..."));
    promptInput->setMinimumHeight(76);
    promptInput->setMaximumHeight(132);
    promptInput->installEventFilter(this);
    root->addWidget(promptInput);

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(8);
    auto *clear = new QPushButton(tr("NEW CONVERSATION"), this);
    clear->setObjectName(QStringLiteral("AssistantSecondaryButton"));
    connect(clear, &QPushButton::clicked,
            assistantSession, &AssistantSession::resetConversation);
    actions->addWidget(clear);
    actions->addStretch(1);
    cancelButton = new QPushButton(tr("CANCEL"), this);
    cancelButton->setObjectName(QStringLiteral("AssistantSecondaryButton"));
    cancelButton->hide();
    connect(cancelButton, &QPushButton::clicked,
            assistantSession, &AssistantSession::cancel);
    actions->addWidget(cancelButton);
    sendButton = new QPushButton(tr("SEND"), this);
    sendButton->setObjectName(QStringLiteral("AssistantPrimaryButton"));
    connect(sendButton, &QPushButton::clicked,
            this, &AssistantPanel::submitPrompt);
    actions->addWidget(sendButton);
    root->addLayout(actions);

    connect(assistantSession, &AssistantSession::messageAdded,
            this, &AssistantPanel::appendMessage);
    connect(assistantSession, &AssistantSession::conversationReset,
            this, &AssistantPanel::clearMessages);
    connect(assistantSession, &AssistantSession::stateChanged,
            this, &AssistantPanel::updateState);
    connect(assistantSession, &AssistantSession::providerChanged,
            this, &AssistantPanel::updateProvider);
    connect(assistantSession, &AssistantSession::contextChanged,
            this, [this](const QString &label) {
        setContextStatus(label, label != tr("Patch context unavailable"));
    });
    connect(assistantSession, &AssistantSession::errorChanged,
            this, &AssistantPanel::updateError);
    updateState(assistantSession->state());
    updateProvider(assistantSession->provider()
        ? assistantSession->provider()->providerName() : QString());
}

void AssistantPanel::setContextStatus(const QString &label, bool available)
{
    contextLabel->setText(available && !label.trimmed().isEmpty()
        ? label : tr("Patch context unavailable"));
    contextLabel->setProperty("available", available);
    contextLabel->style()->unpolish(contextLabel);
    contextLabel->style()->polish(contextLabel);
}

bool AssistantPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == promptInput && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && !(key->modifiers() & Qt::ShiftModifier)) {
            submitPrompt();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AssistantPanel::submitPrompt()
{
    const QString text = promptInput->toPlainText().trimmed();
    if (text.isEmpty() || assistantSession->state() == AssistantSession::Sending
        || assistantSession->state() == AssistantSession::Cancelling)
        return;
    promptInput->clear();
    emit promptSubmitted(text);
}

void AssistantPanel::appendMessage(AssistantMessage message)
{
    conversationLayout->insertWidget(conversationLayout->count() - 1,
                                     messageCard(message));
    scrollToBottom();
}

void AssistantPanel::clearMessages()
{
    while (conversationLayout->count() > 1) {
        QLayoutItem *item = conversationLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void AssistantPanel::updateState(AssistantSession::State state)
{
    const bool busy = state == AssistantSession::Sending
        || state == AssistantSession::Cancelling;
    promptInput->setEnabled(!busy);
    sendButton->setEnabled(!busy);
    cancelButton->setVisible(busy);
    cancelButton->setEnabled(state == AssistantSession::Sending);
    if (state == AssistantSession::Sending) {
        activityLabel->setText(tr("Thinking..."));
        activityLabel->show();
    } else if (state == AssistantSession::Cancelling) {
        activityLabel->setText(tr("Cancelling..."));
        activityLabel->show();
    } else if (state != AssistantSession::Error) {
        activityLabel->hide();
        activityLabel->clear();
    }
    if (!busy)
        promptInput->setFocus(Qt::OtherFocusReason);
}

void AssistantPanel::updateProvider(QString name)
{
    providerNameLabel->setText(name.isEmpty()
        ? tr("No provider")
        : tr("%1 · Offline · No account required").arg(name));
}

void AssistantPanel::updateError(QString error)
{
    if (error.isEmpty()) {
        if (assistantSession->state() != AssistantSession::Sending
            && assistantSession->state() != AssistantSession::Cancelling)
            activityLabel->hide();
        return;
    }
    activityLabel->setText(error);
    activityLabel->show();
}

QWidget *AssistantPanel::messageCard(const AssistantMessage &message) const
{
    auto *card = new QFrame(conversationContent);
    card->setObjectName(QStringLiteral("AssistantMessageCard"));
    QString role;
    QString caption;
    if (message.role == AssistantMessage::User) {
        role = QStringLiteral("user");
        caption = tr("YOU");
    } else if (message.role == AssistantMessage::Assistant) {
        role = QStringLiteral("assistant");
        caption = tr("GT LAB ASSISTANT");
    } else {
        role = QStringLiteral("system");
        caption = tr("STATUS");
    }
    card->setProperty("role", role);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 9);
    layout->setSpacing(4);
    auto *roleLabel = new QLabel(caption, card);
    roleLabel->setObjectName(QStringLiteral("AssistantMessageRole"));
    auto *text = new QLabel(message.text, card);
    text->setObjectName(QStringLiteral("AssistantMessageText"));
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setWordWrap(true);
    layout->addWidget(roleLabel);
    layout->addWidget(text);
    return card;
}

void AssistantPanel::scrollToBottom()
{
    QTimer::singleShot(0, this, [this]() {
        conversationScroll->verticalScrollBar()->setValue(
            conversationScroll->verticalScrollBar()->maximum());
    });
}

} // namespace Ai
