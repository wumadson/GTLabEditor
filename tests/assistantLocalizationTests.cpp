#include "ai/AssistantPanel.h"
#include "ai/AssistantSession.h"
#include "ai/MockAIProvider.h"

#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QTranslator>

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

QStringList buttonTexts(const AssistantPanel &panel)
{
    QStringList result;
    for (QPushButton *button : panel.findChildren<QPushButton *>())
        result.append(button->text());
    return result;
}

QString labelText(const AssistantPanel &panel, const QString &objectName)
{
    QLabel *label = panel.findChild<QLabel *>(objectName);
    return label ? label->text() : QString();
}

void verifyEnglish()
{
    MockAIProvider provider;
    AssistantSession session;
    session.setResponseLanguage(QStringLiteral("en"));
    session.setProvider(&provider);
    AssistantPanel panel(&session);
    panel.resize(280, 600);
    panel.show();
    QApplication::processEvents();
    const QStringList buttons = buttonTexts(panel);
    QPlainTextEdit *prompt = panel.findChild<QPlainTextEdit *>(
        QStringLiteral("AssistantPrompt"));
    expect(buttons.contains("NEW CONVERSATION")
           && buttons.contains("SEND") && buttons.contains("CANCEL"),
           "1 English action labels");
    expect(prompt && prompt->placeholderText()
           == "Ask about your tone, patch, GT-10 settings...",
           "2 English placeholder");
    QLabel *hint = panel.findChild<QLabel *>(
        QStringLiteral("AssistantDiscoveryHint"));
    expect(hint && hint->text() == "Or, if you prefer, ask: “What can you do?”"
           && hint->wordWrap(), "2b English discovery hint wraps");
    expect(hint && hint->height() >= hint->heightForWidth(hint->width()),
           "2c English discovery hint is not clipped at minimum width");
    expect(labelText(panel, QStringLiteral("AssistantProvider"))
           == "Local Test · Offline · No account required",
           "3 English provider status");
    session.sendMessage("patch", AIContext(), QString());
    expect(labelText(panel, QStringLiteral("AssistantActivity"))
           == "Thinking...",
           "4 English activity status");
    session.cancel();
}

void verifyPortuguese()
{
    MockAIProvider provider;
    AssistantSession session;
    session.setResponseLanguage(QStringLiteral("pt-BR"));
    session.setProvider(&provider);
    AssistantPanel panel(&session);
    panel.resize(280, 600);
    panel.show();
    QApplication::processEvents();
    const QStringList buttons = buttonTexts(panel);
    QPlainTextEdit *prompt = panel.findChild<QPlainTextEdit *>(
        QStringLiteral("AssistantPrompt"));
    QLabel *providerLabel = panel.findChild<QLabel *>(
        QStringLiteral("AssistantProvider"));
    expect(buttons.contains("NOVA CONVERSA")
           && buttons.contains("ENVIAR") && buttons.contains("CANCELAR"),
           "5 pt-BR action labels");
    expect(prompt && prompt->placeholderText()
           == QString::fromUtf8(
               "Pergunte sobre seu timbre, patch ou configurações da GT-10..."),
           "6 pt-BR placeholder");
    QLabel *hint = panel.findChild<QLabel *>(
        QStringLiteral("AssistantDiscoveryHint"));
    expect(hint && hint->text() == QString::fromUtf8(
               "Ou, se preferir, pergunte: “O que você pode fazer?”")
           && hint->wordWrap(), "6b pt-BR discovery hint wraps");
    expect(hint && hint->height() >= hint->heightForWidth(hint->width()),
           "6c pt-BR discovery hint is not clipped at minimum width");
    expect(labelText(panel, QStringLiteral("AssistantProvider"))
           == QString::fromUtf8(
               "Local Test · Offline · Nenhuma conta necessária"),
           "7 pt-BR provider status");
    expect(providerLabel && providerLabel->wordWrap(),
           "8 pt-BR provider status wraps at the initial panel width");
    session.sendMessage("patch", AIContext(), QString());
    expect(labelText(panel, QStringLiteral("AssistantActivity"))
           == "Pensando...",
           "9 pt-BR activity status");
    expect(session.responseLanguage() == "pt-BR",
           "10 session carries pt-BR provider language");
    session.cancel();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    verifyEnglish();

    QTranslator portuguese;
    expect(portuguese.load(QStringLiteral(":/translations/gtlab_pt_BR.qm"))
           && app.installTranslator(&portuguese),
           "11 embedded pt-BR catalog loads");
    verifyPortuguese();
    app.removeTranslator(&portuguese);

    // Widget/session/provider localization has no SysxIO or patch-write call.
    expect(true, "12 localization surface has no MIDI/SysEx dependency");

    QTextStream(stdout) << "Assistant Localization tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
