#ifndef AI_ASSISTANTPANEL_H
#define AI_ASSISTANTPANEL_H

#include "ai/AssistantSession.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace Ai {

class AssistantPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AssistantPanel(AssistantSession *session,
                            QWidget *parent = nullptr);
    void setContextStatus(const QString &label, bool available);

signals:
    void promptSubmitted(QString text);
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void submitPrompt();
    void appendMessage(Ai::AssistantMessage message);
    void clearMessages();
    void updateState(Ai::AssistantSession::State state);
    void updateProvider(QString name);
    void updateError(QString error);

private:
    QWidget *messageCard(const AssistantMessage &message) const;
    void scrollToBottom();

    AssistantSession *assistantSession = nullptr;
    QLabel *providerNameLabel = nullptr;
    QLabel *contextLabel = nullptr;
    QLabel *activityLabel = nullptr;
    QScrollArea *conversationScroll = nullptr;
    QWidget *conversationContent = nullptr;
    QVBoxLayout *conversationLayout = nullptr;
    QPlainTextEdit *promptInput = nullptr;
    QPushButton *sendButton = nullptr;
    QPushButton *cancelButton = nullptr;
};

} // namespace Ai

#endif // AI_ASSISTANTPANEL_H
