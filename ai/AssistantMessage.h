#ifndef AI_ASSISTANTMESSAGE_H
#define AI_ASSISTANTMESSAGE_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace Ai {

struct AssistantMessage
{
    enum Role { System, User, Assistant };

    QString id;
    Role role = System;
    QString text;
    QDateTime timestamp;
};

} // namespace Ai

Q_DECLARE_METATYPE(Ai::AssistantMessage)

#endif // AI_ASSISTANTMESSAGE_H
