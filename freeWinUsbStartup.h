#ifndef FREEWINUSBSTARTUP_H
#define FREEWINUSBSTARTUP_H
#include <QtGlobal>

namespace FreeWinUsbStartup {
inline bool shouldConnect(bool freeEdition, bool windows11,
                          bool midiConfigured, bool functionalWinUsb)
{
    return midiConfigured || (freeEdition && windows11 && functionalWinUsb);
}
inline bool validBinding(bool exactHardware, bool activeInterface,
                         bool winUsbService, bool started, unsigned long problem)
{
    return exactHardware && activeInterface && winUsbService && started && problem == 0;
}
}

#if defined(Q_OS_WIN) && defined(GTLAB_FREE_EDITION)
#include "productEdition.h"
#include "gt10WinUsbBackend.h"
#include <QOperatingSystemVersion>
#include <QString>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <vector>
namespace FreeWinUsbStartup {
inline bool isWindows11()
{
    const auto version = QOperatingSystemVersion::current();
    return version.majorVersion() == 10 && version.microVersion() >= 22000;
}
// Read-only startup eligibility. Opening, identity handshake and error handling
// remain in the existing backend; this check never installs or changes a driver.
inline bool functionalBindingAvailable()
{
    const GUID &guid = Gt10WinUsbBackend::interfaceGuid();
    HDEVINFO info = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return false;
    unsigned int matches = 0, interfaces = 0;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA iface = {};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &guid, index, &iface)) break;
        ++interfaces;
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &iface, nullptr, 0, &required, nullptr);
        if (!required) continue;
        std::vector<unsigned char> storage(required);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA device = {};
        device.cbSize = sizeof(device);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &iface, detail, required, nullptr, &device)) continue;
        wchar_t hardware[1024] = {}, service[128] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(info, &device, SPDRP_HARDWAREID, nullptr,
                reinterpret_cast<PBYTE>(hardware), sizeof(hardware), nullptr) ||
            !SetupDiGetDeviceRegistryPropertyW(info, &device, SPDRP_SERVICE, nullptr,
                reinterpret_cast<PBYTE>(service), sizeof(service), nullptr)) continue;
        bool exactHardware = false;
        // Leave room for a terminator even if a malformed property filled the buffer.
        hardware[1023] = L'\0'; service[127] = L'\0';
        for (const wchar_t *id = hardware; id < hardware + 1023 && *id; id += wcslen(id) + 1)
            exactHardware |= QString::fromWCharArray(id).compare(
                QStringLiteral("USB\\VID_0582&PID_00DA"), Qt::CaseInsensitive) == 0;
        ULONG status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, device.DevInst, 0) != CR_SUCCESS) continue;
        if (validBinding(exactHardware, (iface.Flags & SPINT_ACTIVE) != 0,
                QString::fromWCharArray(service).compare(QStringLiteral("WINUSB"), Qt::CaseInsensitive) == 0,
                (status & DN_STARTED) != 0, problem)) ++matches;
    }
    SetupDiDestroyDeviceInfoList(info);
    return matches == 1 && interfaces == 1;
}
}
#endif
#endif