#include "gt10UsbMidiCodec.h"

namespace {

int statusDataLength(unsigned char status)
{
    if (status < 0x80)
        return -1;
    if (status < 0xF0)
        return ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
    if (status == 0xF1 || status == 0xF3)
        return 1;
    if (status == 0xF2)
        return 2;
    return 0;
}

unsigned char cinForStatus(unsigned char status)
{
    if (status < 0xF0) {
        const unsigned char type = status & 0xF0;
        if (type >= 0x80 && type <= 0xE0)
            return type >> 4;
    }
    if (status == 0xF1 || status == 0xF3)
        return 0x2;
    if (status == 0xF2)
        return 0x3;
    if (status == 0xF6 || status == 0xF7)
        return 0x5;
    return 0xF;
}

void appendEvent(QVector<QByteArray> *events, unsigned char cin,
                 unsigned char b1, unsigned char b2 = 0,
                 unsigned char b3 = 0)
{
    QByteArray event(4, 0);
    event[0] = char(cin & 0x0F);
    event[1] = char(b1);
    event[2] = char(b2);
    event[3] = char(b3);
    events->append(event);
}

int midiLengthFromCin(unsigned char cin)
{
    switch (cin & 0x0F) {
    case 0x2: case 0x6: case 0xC: case 0xD: return 2;
    case 0x3: case 0x4: case 0x7: case 0x8: case 0x9:
    case 0xA: case 0xB: case 0xE: return 3;
    case 0x5: case 0xF: return 1;
    default: return 0;
    }
}

}

bool Gt10UsbMidiCodec::encode(const QByteArray &midi,
                              QVector<UsbEvent> *events,
                              QString *errorMessage)
{
    if (!events)
        return false;
    events->clear();
    QByteArray sysex;
    QByteArray message;
    int needed = 0;
    bool inSysex = false;

    for (int i = 0; i < midi.size(); ++i) {
        const unsigned char byte = static_cast<unsigned char>(midi.at(i));
        if (byte >= 0xF8) {
            appendEvent(events, 0xF, byte);
        } else if (inSysex) {
            sysex.append(char(byte));
            if (byte == 0xF7) {
                const int count = sysex.size();
                appendEvent(events, count == 1 ? 0x5 : count == 2 ? 0x6 : 0x7,
                            static_cast<unsigned char>(sysex.at(0)),
                            count > 1 ? static_cast<unsigned char>(sysex.at(1)) : 0,
                            count > 2 ? static_cast<unsigned char>(sysex.at(2)) : 0);
                sysex.clear();
                inSysex = false;
            } else if (sysex.size() == 3) {
                appendEvent(events, 0x4,
                            static_cast<unsigned char>(sysex.at(0)),
                            static_cast<unsigned char>(sysex.at(1)),
                            static_cast<unsigned char>(sysex.at(2)));
                sysex.clear();
            }
        } else if (byte == 0xF0) {
            message.clear();
            needed = 0;
            sysex.append(char(byte));
            inSysex = true;
        } else if (byte & 0x80) {
            message.clear();
            message.append(char(byte));
            needed = statusDataLength(byte) + 1;
            if (needed == 1) {
                appendEvent(events, cinForStatus(byte), byte);
                message.clear();
            }
        } else if (!message.isEmpty()) {
            message.append(char(byte));
            if (message.size() == needed) {
                appendEvent(events, cinForStatus(static_cast<unsigned char>(message.at(0))),
                            static_cast<unsigned char>(message.at(0)),
                            message.size() > 1 ? static_cast<unsigned char>(message.at(1)) : 0,
                            message.size() > 2 ? static_cast<unsigned char>(message.at(2)) : 0);
                message.clear();
            }
        }
    }

    if (inSysex) {
        if (errorMessage) *errorMessage = QStringLiteral("unterminated SysEx message");
        events->clear();
        return false;
    }
    if (!message.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("incomplete MIDI message");
        events->clear();
        return false;
    }
    return true;
}

bool Gt10UsbMidiCodec::decode(const unsigned char *usb, int length,
                              QByteArray *midi, QString *errorMessage)
{
    if (!midi || !usb || length < 0)
        return false;
    midi->clear();
    if (length % 4) {
        if (errorMessage) *errorMessage = QStringLiteral("USB-MIDI payload is not a multiple of four");
        return false;
    }
    for (int offset = 0; offset < length; offset += 4) {
        const int count = midiLengthFromCin(usb[offset]);
        for (int byte = 0; byte < count; ++byte)
            midi->append(char(usb[offset + 1 + byte]));
    }
    return true;
}
