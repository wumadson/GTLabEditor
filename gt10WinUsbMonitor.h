#ifndef GT10WINUSBMONITOR_H
#define GT10WINUSBMONITOR_H
#ifdef Q_OS_WIN

#include "gt10WinUsbBackend.h"
#include <QObject>
#include <QMetaObject>
#include <cfgmgr32.h>

// PnP callbacks only enqueue work. Cleanup and existing session/UI callbacks
// run on the owning UI thread, never on the WinUSB reader or PnP callback.
class Gt10WinUsbMonitor : public QObject
{
public:
    Gt10WinUsbMonitor(QObject *parent, const std::function<void ()> &lost,
                     const std::function<void ()> &arrived)
        : QObject(parent), onLost(lost), onArrived(arrived)
    {
        Gt10WinUsbBackend::instance().setDeviceLostCallback([this] {
            QMetaObject::invokeMethod(this, [this] { processLoss(); }, Qt::QueuedConnection);
        });
        CM_NOTIFY_FILTER filter = {};
        filter.cbSize = sizeof(filter);
        filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        filter.u.DeviceInterface.ClassGuid = Gt10WinUsbBackend::interfaceGuid();
        CM_Register_Notification(&filter, this, &notify, &notification);
    }

    ~Gt10WinUsbMonitor()
    {
        if (notification) CM_Unregister_Notification(notification);
        Gt10WinUsbBackend::instance().setDeviceLostCallback(std::function<void ()>());
    }

private:
    static DWORD CALLBACK notify(HCMNOTIFICATION, PVOID context, CM_NOTIFY_ACTION action,
                                 PCM_NOTIFY_EVENT_DATA, DWORD)
    {
        auto *self = static_cast<Gt10WinUsbMonitor *>(context);
        if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL
            || action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
            QMetaObject::invokeMethod(self, [self, action] {
                auto &backend = Gt10WinUsbBackend::instance();
                if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
                    backend.confirmDeviceRemoval();
                    self->processLoss();
                } else {
                    self->processLoss();
                    if (backend.prepareReconnect()) self->onArrived();
                }
            }, Qt::QueuedConnection);
        }
        return ERROR_SUCCESS;
    }

    void processLoss()
    {
        auto &backend = Gt10WinUsbBackend::instance();
        if (!backend.hasDeviceLoss() || handledEpoch == backend.connectionEpoch())
            return;
        handledEpoch = backend.connectionEpoch();
        onLost();
        backend.close();
    }

    std::function<void ()> onLost;
    std::function<void ()> onArrived;
    HCMNOTIFICATION notification = nullptr;
    unsigned long handledEpoch = 0;
};

#endif
#endif
