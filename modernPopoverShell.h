#ifndef MODERNPOPOVERSHELL_H
#define MODERNPOPOVERSHELL_H

#include "modernTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QWidget>

namespace ModernPopoverShell {

inline void paint(QWidget *widget, qreal radius = 9.0)
{
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = QRectF(widget->rect())
        .adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, radius, radius);
    painter.fillPath(panelPath, QColor(ModernTheme::color(ModernTheme::Panel)));
    painter.setPen(QPen(QColor(ModernTheme::color(ModernTheme::Border)), 1.0));
    painter.drawPath(panelPath);
}

}

#endif // MODERNPOPOVERSHELL_H
