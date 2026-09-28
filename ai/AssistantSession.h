#ifndef AI_ASSISTANTSESSION_H
#define AI_ASSISTANTSESSION_H

#include "ai/AIProvider.h"

#include <QObject>

namespace Ai {

class AssistantSession : public QObject
{
    Q_OBJECT

public:
    enum State { Idle, Sending, Cancelling, Error };
    Q_ENUM(State)

    explicit AssistantSession(QObject *parent = nullptr);

    void setProvider(AIProvider *provider);
    AIProvider *provider() const;
    QList<AssistantMessage> messages() const;
    State state() const;
    QString contextLabel() const;
    void setResponseLanguage(const QString &language);
    QString responseLanguage() const;
    bool sendMessage(const QString &text, const AIContext &context,
                     const QString &contextLabel);

public slots:
    void cancel();
    void resetConversation();

signals:
    void messageAdded(Ai::AssistantMessage message);
    void conversationReset();
    void stateChanged(Ai::AssistantSession::State state);
    void contextChanged(QString label);
    void providerChanged(QString name);
    void errorChanged(QString error);

private slots:
    void handleResponse(QString requestId, Ai::AIResponse response);
    void handleError(QString requestId, QString error);
    void handleCancelled(QString requestId);

private:
    void appendMessage(AssistantMessage::Role role, const QString &text);
    void setState(State state);
    void clearActiveRequest();

    AIProvider *currentProvider = nullptr;
    QList<AssistantMessage> history;
    State currentState = Idle;
    QString activeRequestId;
    QString currentContextLabel;
    QString currentResponseLanguage = QStringLiteral("en");
    quint64 nextRequestId = 0;
    quint64 nextMessageId = 0;
};

} // namespace Ai

#endif // AI_ASSISTANTSESSION_H
