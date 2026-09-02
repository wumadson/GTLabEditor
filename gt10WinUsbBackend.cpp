#include "gt10WinUsbBackend.h"

#ifdef Q_OS_WIN

#include "gt10UsbMidiCodec.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <setupapi.h>
#include <usb.h>
#include <vector>

namespace {

const GUID kGt10WinUsbGuid = {
    0x6bac1d7f, 0x8f71, 0x42a9,
    {0xba, 0x81, 0xa8, 0x2a, 0x15, 0xc1, 0x0f, 0x5b}
};
const UCHAR kInterfaceNumber = 2;
const UCHAR kAlternateSetting = 0;
const UCHAR kOutPipe = 0x03;
const UCHAR kInPipe = 0x84;

QString windowsError(const QString &operation, DWORD code = GetLastError())
{
    return QStringLiteral("%1 failed (%2)").arg(operation).arg(code);
}

int midiMessageLength(unsigned char status)
{
    if (status < 0xF0)
        return ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 2 : 3;
    if (status == 0xF1 || status == 0xF3)
        return 2;
    if (status == 0xF2)
        return 3;
    return 1;
}

QStringList devicePaths()
{
    QStringList paths;
    HDEVINFO info = SetupDiGetClassDevsW(&kGt10WinUsbGuid, 0, 0,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return paths;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA data = {};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInterfaces(info, 0, &kGt10WinUsbGuid, index, &data))
            break;
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &data, 0, 0, &required, 0);
        if (!required)
            continue;
        std::vector<unsigned char> storage(required);
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail =
            reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (SetupDiGetDeviceInterfaceDetailW(info, &data, detail, required, 0, 0))
            paths.append(QString::fromWCharArray(detail->DevicePath));
    }
    SetupDiDestroyDeviceInfoList(info);
    return paths;
}

}

Gt10WinUsbBackend &Gt10WinUsbBackend::instance()
{
    static Gt10WinUsbBackend backend;
    return backend;
}

bool Gt10WinUsbBackend::isAvailable()
{
    return !devicePaths().isEmpty();
}

Gt10WinUsbBackend::Gt10WinUsbBackend()
    : device(INVALID_HANDLE_VALUE), defaultInterface(0), midiInterface(0),
      midiIsDefault(false), stopEvent(0), receiving(false)
{
}

Gt10WinUsbBackend::~Gt10WinUsbBackend()
{
    close();
}

bool Gt10WinUsbBackend::open(QString *errorMessage)
{
    QMutexLocker locker(&mutex);
    if (device != INVALID_HANDLE_VALUE && receiving)
        return true;
    locker.unlock();
    close();

    const QStringList paths = devicePaths();
    if (paths.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("GT-10 WinUSB interface not found");
        return false;
    }
    device = CreateFileW(reinterpret_cast<LPCWSTR>(paths.first().utf16()),
                         GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
    if (device == INVALID_HANDLE_VALUE) {
        if (errorMessage) *errorMessage = windowsError(QStringLiteral("CreateFile"));
        return false;
    }
    if (!WinUsb_Initialize(device, &defaultInterface) ||
        !findMidiInterface(errorMessage) || !configure(errorMessage)) {
        close();
        return false;
    }
    stopEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (!stopEvent) {
        if (errorMessage) *errorMessage = windowsError(QStringLiteral("CreateEvent"));
        close();
        return false;
    }
    receiving = true;
    receiveThread = std::thread(&Gt10WinUsbBackend::receiveLoop, this);
    return true;
}

bool Gt10WinUsbBackend::findMidiInterface(QString *errorMessage)
{
    USB_INTERFACE_DESCRIPTOR descriptor = {};
    if (!WinUsb_QueryInterfaceSettings(defaultInterface, 0, &descriptor)) {
        if (errorMessage) *errorMessage = windowsError(QStringLiteral("WinUsb_QueryInterfaceSettings"));
        return false;
    }
    if (descriptor.bInterfaceNumber == kInterfaceNumber) {
        midiInterface = defaultInterface;
        midiIsDefault = true;
        return true;
    }
    for (UCHAR index = 0;; ++index) {
        WINUSB_INTERFACE_HANDLE candidate = 0;
        if (!WinUsb_GetAssociatedInterface(defaultInterface, index, &candidate))
            break;
        USB_INTERFACE_DESCRIPTOR candidateDescriptor = {};
        if (WinUsb_QueryInterfaceSettings(candidate, 0, &candidateDescriptor) &&
            candidateDescriptor.bInterfaceNumber == kInterfaceNumber) {
            midiInterface = candidate;
            return true;
        }
        WinUsb_Free(candidate);
    }
    if (errorMessage) *errorMessage = QStringLiteral("WinUSB interface 2 not found");
    return false;
}

bool Gt10WinUsbBackend::configure(QString *errorMessage)
{
    if (!WinUsb_SetCurrentAlternateSetting(midiInterface, kAlternateSetting)) {
        if (errorMessage) *errorMessage = windowsError(QStringLiteral("WinUsb_SetCurrentAlternateSetting"));
        return false;
    }
    USB_INTERFACE_DESCRIPTOR descriptor = {};
    if (!WinUsb_QueryInterfaceSettings(midiInterface, kAlternateSetting, &descriptor))
        return false;
    bool outFound = false;
    bool inFound = false;
    for (UCHAR index = 0; index < descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe = {};
        if (!WinUsb_QueryPipe(midiInterface, kAlternateSetting, index, &pipe))
            return false;
        if (pipe.PipeType == UsbdPipeTypeBulk && pipe.MaximumPacketSize == 32) {
            outFound = outFound || pipe.PipeId == kOutPipe;
            inFound = inFound || pipe.PipeId == kInPipe;
        }
    }
    if (!outFound || !inFound) {
        if (errorMessage) *errorMessage = QStringLiteral("required Bulk pipes 0x03/0x84 MPS 32 not found");
        return false;
    }
    return true;
}

bool Gt10WinUsbBackend::isOpen() const
{
    return device != INVALID_HANDLE_VALUE && receiving;
}

bool Gt10WinUsbBackend::writeEvent(const QByteArray &event, QString *errorMessage)
{
    OVERLAPPED operation = {};
    operation.hEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (!operation.hEvent)
        return false;
    BOOL started = WinUsb_WritePipe(midiInterface, kOutPipe,
        reinterpret_cast<PUCHAR>(const_cast<char *>(event.constData())), 4, 0, &operation);
    if (!started && GetLastError() != ERROR_IO_PENDING) {
        if (errorMessage) *errorMessage = windowsError(QStringLiteral("WinUsb_WritePipe"));
        CloseHandle(operation.hEvent);
        return false;
    }
    const DWORD wait = WaitForSingleObject(operation.hEvent, 1000);
    ULONG transferred = 0;
    const bool ok = wait == WAIT_OBJECT_0 &&
        WinUsb_GetOverlappedResult(midiInterface, &operation, &transferred, FALSE) &&
        transferred == 4;
    if (!ok) {
        CancelIoEx(device, &operation);
        if (errorMessage && errorMessage->isEmpty())
            *errorMessage = QStringLiteral("WinUSB write failed or timed out");
    }
    CloseHandle(operation.hEvent);
    return ok;
}

bool Gt10WinUsbBackend::send(const QByteArray &midi, QString *errorMessage)
{
    if (!isOpen() && !open(errorMessage))
        return false;
    QVector<QByteArray> events;
    if (!Gt10UsbMidiCodec::encode(midi, &events, errorMessage))
        return false;
    for (int i = 0; i < events.size(); ++i) {
        if (!writeEvent(events.at(i), errorMessage))
            return false;
    }
    return true;
}

QByteArray Gt10WinUsbBackend::transact(const QByteArray &midi, int timeoutMs,
                                       int expectedResponseBytes,
                                       QString *errorMessage)
{
    {
        QMutexLocker locker(&mutex);
        responses.clear();
    }
    if (!send(midi, errorMessage))
        return QByteArray();
    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    QMutexLocker locker(&mutex);
    while (expectedResponseBytes <= 0 || result.size() < expectedResponseBytes) {
        while (!responses.isEmpty()) {
            result.append(responses.takeFirst());
            if (expectedResponseBytes <= 0 || result.size() >= expectedResponseBytes)
                break;
        }
        if ((expectedResponseBytes <= 0 && !result.isEmpty()) ||
            (expectedResponseBytes > 0 && result.size() >= expectedResponseBytes) ||
            !receiving)
            break;
        const int remaining = timeoutMs - int(timer.elapsed());
        if (remaining <= 0 || !responseReady.wait(&mutex, remaining))
            break;
    }
    return result;
}

void Gt10WinUsbBackend::processMidiBytes(const QByteArray &bytes)
{
    for (int index = 0; index < bytes.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(bytes.at(index));
        if (!sysexAssembly.isEmpty() || byte == 0xF0) {
            sysexAssembly.append(char(byte));
            if (byte == 0xF7) {
                QMutexLocker locker(&mutex);
                responses.append(sysexAssembly);
                sysexAssembly.clear();
                responseReady.wakeAll();
            }
            continue;
        }
        if (byte & 0x80) {
            int length = midiMessageLength(byte);
            if (length > 0 && index + length <= bytes.size()) {
                ReceiveCallback callback;
                {
                    QMutexLocker locker(&mutex);
                    callback = persistentCallback;
                }
                if (callback)
                    callback(bytes.mid(index, length));
                index += length - 1;
            }
        }
    }
}

void Gt10WinUsbBackend::receiveLoop()
{
    unsigned char buffer[256] = {};
    while (receiving) {
        OVERLAPPED operation = {};
        operation.hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (!operation.hEvent)
            break;
        BOOL started = WinUsb_ReadPipe(midiInterface, kInPipe, buffer,
                                       sizeof(buffer), 0, &operation);
        if (!started && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(operation.hEvent);
            break;
        }
        HANDLE waits[2] = { stopEvent, operation.hEvent };
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            CancelIoEx(device, &operation);
            WaitForSingleObject(operation.hEvent, INFINITE);
            CloseHandle(operation.hEvent);
            break;
        }
        ULONG transferred = 0;
        if (wait != WAIT_OBJECT_0 + 1 ||
            !WinUsb_GetOverlappedResult(midiInterface, &operation, &transferred, FALSE)) {
            CloseHandle(operation.hEvent);
            break;
        }
        CloseHandle(operation.hEvent);
        QByteArray midiBytes;
        if (Gt10UsbMidiCodec::decode(buffer, int(transferred), &midiBytes)) {
            processMidiBytes(midiBytes);
        }
    }
    receiving = false;
    responseReady.wakeAll();
}

void Gt10WinUsbBackend::setPersistentCallback(const ReceiveCallback &callback)
{
    QMutexLocker locker(&mutex);
    persistentCallback = callback;
}

QString Gt10WinUsbBackend::deviceName() const
{
    return QStringLiteral("GT-10 WinUSB");
}

void Gt10WinUsbBackend::close()
{
    receiving = false;
    if (stopEvent) SetEvent(stopEvent);
    if (device != INVALID_HANDLE_VALUE) CancelIoEx(device, 0);
    if (receiveThread.joinable()) receiveThread.join();
    if (stopEvent) { CloseHandle(stopEvent); stopEvent = 0; }
    if (midiInterface && !midiIsDefault) WinUsb_Free(midiInterface);
    midiInterface = 0;
    midiIsDefault = false;
    if (defaultInterface) { WinUsb_Free(defaultInterface); defaultInterface = 0; }
    if (device != INVALID_HANDLE_VALUE) { CloseHandle(device); device = INVALID_HANDLE_VALUE; }
    QMutexLocker locker(&mutex);
    responses.clear();
    sysexAssembly.clear();
}

#endif
