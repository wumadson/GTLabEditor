#ifndef GT10WINUSBBACKEND_H
#define GT10WINUSBBACKEND_H

#ifdef Q_OS_WIN

#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>
#include <functional>
#include <thread>
#include <atomic>
#include <windows.h>
#include <winusb.h>

class Gt10WinUsbBackend
{
public:
    typedef std::function<void (const QByteArray &)> ReceiveCallback;

    static Gt10WinUsbBackend &instance();
    static bool isAvailable();
    static const GUID &interfaceGuid();
    bool wasSelected() const { return selected; }
    bool hasDeviceLoss() const { return deviceLost; }
    unsigned long connectionEpoch() const { return lossEpoch; }
    bool prepareReconnect();
    void confirmDeviceRemoval();
    // Callback must only queue work; never close/join from the reader thread.
    void setDeviceLostCallback(const std::function<void ()> &callback);

    bool open(QString *errorMessage = 0);
    void close();
    bool isOpen() const;
    bool send(const QByteArray &midi, QString *errorMessage = 0);
    QByteArray transact(const QByteArray &midi, int timeoutMs, int expectedResponseBytes,
                        QString *errorMessage = 0);
    void setPersistentCallback(const ReceiveCallback &callback);
    QString deviceName() const;

private:
    Gt10WinUsbBackend();
    ~Gt10WinUsbBackend();
    Gt10WinUsbBackend(const Gt10WinUsbBackend &);
    Gt10WinUsbBackend &operator=(const Gt10WinUsbBackend &);

    bool findMidiInterface(QString *errorMessage);
    bool configure(QString *errorMessage);
    bool writeEvent(const QByteArray &event, QString *errorMessage);
    void receiveLoop();
    void handleDeviceError(DWORD error);
    void markDeviceLost();
    void processMidiBytes(const QByteArray &bytes);

    // Serializes handle lifecycle with callers; receiveLoop never takes it.
    mutable QMutex ioMutex;
    QMutex callbackMutex;
    std::function<void ()> deviceLostCallback;
    std::atomic<bool> selected{false};
    std::atomic<bool> closing{true};
    std::atomic<bool> deviceLost{false};
    std::atomic<unsigned long> lossEpoch{0};
    mutable QMutex mutex;
    QWaitCondition responseReady;
    QList<QByteArray> responses;
    QByteArray sysexAssembly;
    ReceiveCallback persistentCallback;
    HANDLE device;
    WINUSB_INTERFACE_HANDLE defaultInterface;
    WINUSB_INTERFACE_HANDLE midiInterface;
    bool midiIsDefault;
    HANDLE stopEvent;
    std::thread receiveThread;
    std::atomic<bool> receiving;
};

#endif
#endif
