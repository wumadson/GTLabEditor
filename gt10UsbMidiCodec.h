#ifndef GT10USBMIDICODEC_H
#define GT10USBMIDICODEC_H

#include <QByteArray>
#include <QVector>

namespace Gt10UsbMidiCodec {

typedef QByteArray UsbEvent;

bool encode(const QByteArray &midi, QVector<UsbEvent> *events,
            QString *errorMessage = 0);
bool decode(const unsigned char *usb, int length, QByteArray *midi,
            QString *errorMessage = 0);

}

#endif
