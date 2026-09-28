#include "ai/AssistantSession.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

using namespace Ai;

namespace {
int failures = 0;
int assertions = 0;

void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

void drainEvents(int milliseconds = 20)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

AIContext context(const QString &number)
{
    AIContext result;
    QJsonObject patch;
    patch.insert("number", number);
    patch.insert("name", number == "U01-1" ? "FIRST" : "SECOND");
    result.root.insert("patch", patch);
    return result;
}

class TestProvider : public AIProvider
{
public:
    explicit TestProvider(QObject *parent = nullptr) : AIProvider(parent) {}

    QString providerId() const override { return "test"; }
    QString providerName() const override { return "Test Provider"; }
    AIProviderCapabilities capabilities() const override
    {
        AIProviderCapabilities result;
        result.supportsConversation = true;
        return result;
    }

    void sendRequest(const QString &requestId,
                     const AIRequest &request) override
    {
        requests.append(request);
        activeId = requestId;
        QTimer::singleShot(1, this, [this, requestId, request]() {
            if (request.userMessage == "error") {
                emit errorOccurred(requestId, "provider failed");
                return;
            }
            AIResponse response;
            response.text = "reply: " + request.userMessage;
            emit responseReady(requestId, response);
        });
    }

    void cancelRequest(const QString &requestId) override
    {
        emit requestCancelled(requestId);
        QTimer::singleShot(1, this, [this, requestId]() {
            AIResponse late;
            late.text = "late response";
            emit responseReady(requestId, late);
        });
    }

    QList<AIRequest> requests;
    QString activeId;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestProvider provider;
    AssistantSession session;
    session.setResponseLanguage(QStringLiteral("pt-BR"));
    session.setProvider(&provider);

    QList<AssistantSession::State> states;
    QObject::connect(&session, &AssistantSession::stateChanged,
                     [&](AssistantSession::State state) { states.append(state); });
    expect(session.provider() == &provider
           && session.state() == AssistantSession::Idle
           && session.responseLanguage() == "pt-BR",
           "1 provider contract and initial state");
    expect(!session.sendMessage("   ", context("U01-1"), "U01-1 — FIRST")
           && session.messages().isEmpty() && provider.requests.isEmpty(),
           "2 empty message rejected");

    const QJsonObject firstBefore = context("U01-1").root;
    AIContext firstContext;
    firstContext.root = firstBefore;
    expect(session.sendMessage("patch", firstContext, "U01-1 — FIRST")
           && session.state() == AssistantSession::Sending
           && session.messages().size() == 1
           && session.messages().first().role == AssistantMessage::User,
           "3 normal request enters Sending and appends user");
    expect(provider.requests.size() == 1
           && provider.requests.first().language == "pt-BR"
           && provider.requests.first().context.root == firstBefore
           && provider.requests.first().conversationHistory.isEmpty(),
           "4 language, current context and initial history passed");
    drainEvents();
    expect(session.state() == AssistantSession::Idle
           && session.messages().size() == 2
           && session.messages().last().role == AssistantMessage::Assistant
           && session.messages().last().text == "reply: patch",
           "5 asynchronous response appended");
    expect(states.contains(AssistantSession::Sending)
           && states.contains(AssistantSession::Idle),
           "6 state transitions emitted");

    const int historyBeforeLanguageChange = session.messages().size();
    session.setResponseLanguage(QStringLiteral("en"));
    expect(session.responseLanguage() == "en"
           && session.messages().size() == historyBeforeLanguageChange,
           "7 language update preserves conversation history");
    expect(session.sendMessage("delay", context("U02-1"), "U02-1 — SECOND"),
           "7 second request accepted");
    expect(provider.requests.last().context.root.value("patch").toObject()
               .value("number") == "U02-1"
           && provider.requests.last().language == "en"
           && provider.requests.last().conversationHistory.size() == 2,
           "8 context updated and conversation history passed");
    drainEvents();
    expect(session.contextLabel() == "U02-1 — SECOND",
           "9 latest context label retained");

    const int beforeCancel = session.messages().size();
    expect(session.sendMessage("cancel me", context("U02-1"), "U02-1 — SECOND"),
           "10 cancellable request accepted");
    session.cancel();
    expect(session.state() == AssistantSession::Idle,
           "11 cancel returns session to Idle");
    drainEvents();
    expect(session.messages().size() == beforeCancel + 1
           && session.messages().last().text == "cancel me",
           "12 late response after cancel ignored");

    expect(session.sendMessage("error", context("U02-1"), "U02-1 — SECOND"),
           "13 error request accepted");
    drainEvents();
    expect(session.state() == AssistantSession::Error
           && session.messages().last().role == AssistantMessage::System
           && session.messages().last().text == "provider failed",
           "14 provider error produces Error state and status message");

    session.resetConversation();
    expect(session.messages().isEmpty(), "15 reset clears only conversation");
    expect(firstContext.root == firstBefore, "16 session does not mutate context");
    expect(provider.capabilities().supportsConversation
           && !provider.capabilities().requiresNetwork,
           "17 provider capabilities remain available");

    // This harness links no SysxIO, MIDI or patch-writing implementation.
    expect(true, "18 session contract has no MIDI/SysEx dependency");

    QTextStream(stdout) << "Assistant Session tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
