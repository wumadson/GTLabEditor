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
    void processMidiBytes(const QByteArray &bytes);

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
