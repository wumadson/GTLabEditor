#ifndef AI_MOCKAIPROVIDER_H
#define AI_MOCKAIPROVIDER_H

#include "ai/AIProvider.h"

#include <QTimer>

namespace Ai {

class MockAIProvider : public AIProvider
{
    Q_OBJECT

public:
    explicit MockAIProvider(QObject *parent = nullptr);

    QString providerId() const override;
    QString providerName() const override;
    AIProviderCapabilities capabilities() const override;
    void sendRequest(const QString &requestId,
                     const AIRequest &request) override;
    void cancelRequest(const QString &requestId) override;

private slots:
    void finishRequest();

private:
    QString responseFor(const AIRequest &request) const;

    QTimer responseTimer;
    QString activeRequestId;
    AIRequest activeRequest;
};

} // namespace Ai

#endif // AI_MOCKAIPROVIDER_H
