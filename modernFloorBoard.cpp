#include "modernFloorBoard.h"
#include "SysxIO.h"
#include "MidiTable.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "effectArtworkWidget.h"
#include "effectModelBrowser.h"
#include "modernEqGraph.h"
#include "modernFxEditor.h"
#include "modernPedalFxEditor.h"
#include "modernNoiseSuppressorEditor.h"
#include "modernSendReturnEditor.h"
#include "modernSignalChainMutationController.h"
#include "modernSignalChainSerializer.h"
#include "parameterBar.h"
#include "patchSidebar.h"
#include "signalChainHardwareValidation.h"

#include <QComboBox>
#include <QButtonGroup>
#include <QDebug>
#include <QDial>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QIconEngine>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QResizeEvent>
#include <QEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QSignalBlocker>
#include <QSet>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include "globalVariables.h"

#include <algorithm>

namespace {
class GtLabBrandWidget final : public QWidget
{
public:
    explicit GtLabBrandWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(132, 25);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont brandFont = font();
        brandFont.setPixelSize(20);
        brandFont.setWeight(QFont::DemiBold);
        brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);

        QFont editorFont = font();
        editorFont.setPixelSize(19);
        editorFont.setWeight(QFont::Normal);

        const QFontMetricsF brandMetrics(brandFont);
        const qreal baseline = (height() - brandMetrics.height()) / 2.0
            + brandMetrics.ascent();
        const qreal editorX = brandMetrics.horizontalAdvance("GT Lab") + 7.0;

        QPainterPath brandPath;
        brandPath.addText(QPointF(0.5, baseline), brandFont, "GT Lab");
        QLinearGradient steel(0.0, brandPath.boundingRect().top(),
                              0.0, brandPath.boundingRect().bottom());
        steel.setColorAt(0.00, QColor("#87929C"));
        steel.setColorAt(0.32, QColor("#E8EDF1"));
        steel.setColorAt(0.58, QColor("#B8C1C9"));
        steel.setColorAt(1.00, QColor("#75818B"));
        painter.fillPath(brandPath, steel);

        painter.setPen(QPen(QColor(55, 159, 218, 145), 1.0,
                            Qt::SolidLine, Qt::RoundCap));
        const qreal accentY = qMin<qreal>(height() - 1.5, baseline + 3.0);
        painter.drawLine(QPointF(1.0, accentY),
                         QPointF(editorX - 9.0, accentY));

        painter.setFont(editorFont);
        painter.setPen(QColor("#9AA5AF"));
        painter.drawText(QPointF(editorX, baseline), "Editor");
    }
};

class OutputSelectIconEngine final : public QIconEngine
{
public:
    explicit OutputSelectIconEngine(int rawValue)
        : raw(rawValue)
    {
    }

    QIconEngine *clone() const override
    {
        return new OutputSelectIconEngine(raw);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                   QIcon::State state) override
    {
        return renderPixmap(size, mode, state, 1.0);
    }

    void virtual_hook(int id, void *data) override
    {
        if (id == QIconEngine::ScaledPixmapHook) {
            auto *argument = static_cast<QIconEngine::ScaledPixmapArgument *>(data);
            argument->pixmap = renderPixmap(argument->size,
                                            argument->mode,
                                            argument->state,
                                            argument->scale);
            return;
        }
        QIconEngine::virtual_hook(id, data);
    }

private:
    QPixmap renderPixmap(const QSize &size, QIcon::Mode mode,
                         QIcon::State state, qreal scale)
    {
        const QSize pixelSize(qMax(1, qRound(size.width() * scale)),
                              qMax(1, qRound(size.height() * scale)));
        QPixmap result(pixelSize);
        result.setDevicePixelRatio(scale);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return result;
    }

public:

    void paint(QPainter *painter, const QRect &rect,
               QIcon::Mode mode, QIcon::State) override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const qreal side = qMin(rect.width(), rect.height());
        const QRectF target(rect.center().x() - side / 2.0,
                            rect.center().y() - side / 2.0,
                            side, side);
        painter->translate(target.topLeft());
        painter->scale(target.width() / 18.0, target.height() / 18.0);

        const QColor color = mode == QIcon::Disabled
            ? QColor("#59636e")
            : mode == QIcon::Selected
                ? QColor("#eef5fb")
                : QColor("#aab6c1");
        QPen pen(color, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        if (raw == 7) {
            QPainterPath headphones;
            headphones.moveTo(3.0, 10.0);
            headphones.cubicTo(3.0, 2.8, 15.0, 2.8, 15.0, 10.0);
            painter->drawPath(headphones);
            painter->drawRoundedRect(QRectF(2.1, 9.2, 3.0, 5.4), 1.0, 1.0);
            painter->drawRoundedRect(QRectF(12.9, 9.2, 3.0, 5.4), 1.0, 1.0);
            return painter->restore();
        }

        const bool isReturn = raw >= 4 && raw <= 6;
        const bool isStack = raw == 3 || raw == 6;
        const bool isSmall = raw == 1;
        const bool isTwinCombo = raw == 0 || raw == 4;
        const qreal left = isReturn ? 5.0 : (isSmall ? 4.0 : 2.0);
        const qreal width = isReturn ? 11.0 : (isSmall ? 10.0 : 14.0);

        if (isStack) {
            painter->drawRoundedRect(QRectF(left, 2.4, width, 4.0), 0.8, 0.8);
            painter->drawRoundedRect(QRectF(left, 7.5, width, 8.2), 1.0, 1.0);
            painter->drawEllipse(QPointF(left + width * 0.32, 11.6), 1.9, 1.9);
            painter->drawEllipse(QPointF(left + width * 0.68, 11.6), 1.9, 1.9);
            painter->drawPoint(QPointF(left + width - 2.0, 4.4));
        } else {
            const qreal top = isSmall ? 5.0 : 3.2;
            const qreal height = isSmall ? 10.0 : 12.5;
            painter->drawRoundedRect(QRectF(left, top, width, height), 1.3, 1.3);
            painter->drawLine(QPointF(left + 1.2, top + 3.0),
                              QPointF(left + width - 1.2, top + 3.0));
            if (isTwinCombo) {
                painter->drawEllipse(QPointF(left + width * 0.34, top + 8.2),
                                     1.8, 1.8);
                painter->drawEllipse(QPointF(left + width * 0.68, top + 8.2),
                                     1.8, 1.8);
            } else {
                painter->drawEllipse(QPointF(left + width / 2.0, top + 8.0),
                                     isSmall ? 2.2 : 2.8,
                                     isSmall ? 2.2 : 2.8);
            }
            painter->drawPoint(QPointF(left + width - 2.0, top + 1.5));
        }

        if (isReturn) {
            painter->drawLine(QPointF(0.8, 9.0), QPointF(5.1, 9.0));
            painter->drawLine(QPointF(3.1, 7.1), QPointF(5.1, 9.0));
            painter->drawLine(QPointF(3.1, 10.9), QPointF(5.1, 9.0));
        }

        painter->restore();
    }

private:
    int raw;
};

QIcon outputSelectIcon(int raw)
{
    return QIcon(new OutputSelectIconEngine(raw));
}

class OutputSelectComboBox final : public QComboBox
{
public:
    explicit OutputSelectComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        setFrame(false);
        setMinimumWidth(142);
        setMaximumWidth(184);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    QSize sizeHint() const override
    {
        QSize hint = QComboBox::sizeHint();
        hint.setWidth(168);
        return hint;
    }

protected:
    void enterEvent(QEvent *event) override
    {
        QComboBox::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QComboBox::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const bool active = isEnabled();
        const QColor textColor(active ? "#d8e1ea" : "#59636e");
        const QColor borderColor(!active ? "#27313a"
                                       : underMouse() ? "#4d92bd"
                                                      : "#33404d");

        const qreal borderY = height() - 1.0;
        painter.setPen(QPen(borderColor, 1.0));
        painter.drawLine(QPointF(0.5, borderY),
                         QPointF(width() - 0.5, borderY));

        const int iconSide = 18;
        const QRect iconRect(1, (height() - iconSide) / 2,
                             iconSide, iconSide);
        const QIcon icon = itemIcon(currentIndex());
        if (!icon.isNull())
            icon.paint(&painter, iconRect, Qt::AlignCenter,
                       active ? QIcon::Normal : QIcon::Disabled);

        QFont valueFont = font();
        valueFont.setPointSizeF(11.0);
        valueFont.setWeight(QFont::DemiBold);
        painter.setFont(valueFont);
        painter.setPen(textColor);
        const int textLeft = icon.isNull() ? 1 : 25;
        const QRect textRect(textLeft, 0,
                             qMax(0, width() - textLeft - 22), height() - 2);
        const QString visibleText = QFontMetrics(valueFont).elidedText(
            currentText(), Qt::ElideRight, textRect.width());
        painter.drawText(textRect,
                         Qt::AlignVCenter | Qt::AlignLeft,
                         visibleText);

        const qreal arrowX = width() - 10.0;
        const qreal arrowY = height() / 2.0 + 0.5;
        painter.setPen(QPen(active ? QColor("#919da9")
                                   : QColor("#59636e"),
                            1.45, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        QPainterPath chevron;
        chevron.moveTo(arrowX - 3.5, arrowY - 1.8);
        chevron.lineTo(arrowX, arrowY + 1.8);
        chevron.lineTo(arrowX + 3.5, arrowY - 1.8);
        painter.drawPath(chevron);
    }
};

QString oddsArtworkType(QString type)
{
    if (type.startsWith('(')) {
        const int categoryEnd = type.indexOf(") ");
        if (categoryEnd >= 0)
            type = type.mid(categoryEnd + 2);
    }
    return type.toUpper();
}

QString delayArtworkType(QString type)
{
    type = type.trimmed().toUpper();
    if (type.startsWith("DUAL") && type.size() > 4 && type.at(4) != ' ')
        type.insert(4, ' ');
    return type;
}

QVector<int> rhythmicDivisionRawValues(const Midi &parameter)
{
    struct RhythmicEntry {
        int raw;
        QString display;
    };

    QVector<RhythmicEntry> entries;
    for (const Midi &highByte : parameter.level) {
        if (highByte.value == "range")
            continue;

        if (highByte.level.isEmpty()) {
            bool recognized = false;
            const QString display = FxPresentation::formatRhythmicDivision(
                highByte.name, &recognized);
            bool rawOk = false;
            const int raw = highByte.value.toInt(&rawOk, 16);
            if (recognized && rawOk)
                entries.append({raw, display});
            continue;
        }

        bool highByteOk = false;
        const int high = highByte.value.toInt(&highByteOk, 16);
        if (!highByteOk)
            continue;

        for (const Midi &entry : highByte.level) {
            if (entry.value == "range")
                continue;

            bool recognized = false;
            const QString display = FxPresentation::formatRhythmicDivision(
                entry.name, &recognized);
            if (!recognized)
                continue;

            bool lowByteOk = false;
            const int low = entry.value.toInt(&lowByteOk, 16);
            if (lowByteOk)
                entries.append({high * 128 + low, display});
        }
    }

    static const QStringList visualOrder = {
        "1/1", "1/1T",
        "1/2", "1/2D", "1/2T",
        "1/4", "1/4D", "1/4T",
        "1/8", "1/8D", "1/8T",
        "1/16", "1/16D"
    };

    QVector<int> rawValues;
    rawValues.reserve(entries.size());
    for (const QString &display : visualOrder) {
        for (const RhythmicEntry &entry : entries) {
            if (entry.display == display) {
                rawValues.append(entry.raw);
                break;
            }
        }
    }
    return rawValues;
}

bool centerValueFromMapping(const Midi &parameter, int *rawCenter)
{
    if (!rawCenter || parameter.level.isEmpty())
        return false;
    const Midi range = parameter.level.last();
    if (range.value != "range")
        return false;

    const QStringList parts = range.name.split('/');
    if (parts.size() < 4)
        return false;

    bool rawMinimumOk = false;
    bool rawMaximumOk = false;
    bool displayMinimumOk = false;
    bool displayMaximumOk = false;
    const int rawMinimum = parts.at(0).toInt(&rawMinimumOk, 16);
    const int rawMaximum = parts.at(1).toInt(&rawMaximumOk, 16);
    const qreal displayMinimum = parts.at(2).toDouble(&displayMinimumOk);
    const qreal displayMaximum = parts.at(3).toDouble(&displayMaximumOk);
    if (!rawMinimumOk || !rawMaximumOk || !displayMinimumOk
        || !displayMaximumOk || rawMaximum <= rawMinimum
        || displayMinimum >= 0.0 || displayMaximum <= 0.0)
        return false;

    const qreal ratio = -displayMinimum
        / (displayMaximum - displayMinimum);
    *rawCenter = qRound(rawMinimum
        + ratio * (rawMaximum - rawMinimum));
    return true;
}

QString formatEqDisplay(QString value, const QString &address, int rawValue)
{
    static const QSet<QString> gainAddresses = {
        "72", "75", "78", "79", "7B"
    };
    if (!gainAddresses.contains(address))
        return value;

    QString numeric = value.trimmed();
    if (numeric.endsWith("dB", Qt::CaseInsensitive)) {
        numeric.chop(2);
        numeric = numeric.trimmed();
    }
    if (rawValue > 0x14 && !numeric.startsWith('+'))
        numeric.prepend('+');
    return numeric + " dB";
}

qreal numericPresentationValue(QString text, bool *ok = nullptr)
{
    text = text.trimmed();
    QString numeric;
    for (const QChar character : text) {
        if (character.isDigit() || character == '.' || character == ','
            || ((character == '+' || character == '-') && numeric.isEmpty())) {
            numeric.append(character == ',' ? '.' : character);
        }
    }
    bool converted = false;
    const qreal value = numeric.toDouble(&converted);
    if (ok)
        *ok = converted;
    return converted ? value : 0.0;
}

qreal frequencyPresentationValue(const QString &text, bool *ok = nullptr)
{
    bool converted = false;
    qreal frequency = numericPresentationValue(text, &converted);
    if (converted && text.contains("k", Qt::CaseInsensitive))
        frequency *= 1000.0;
    if (ok)
        *ok = converted;
    return frequency;
}

bool preampBrightAvailable(int type, int customType)
{
    static const QSet<int> brightTypes = {
        0x00, 0x01, 0x02, 0x04, 0x05, 0x06,
        0x08, 0x09, 0x0B, 0x12, 0x13, 0x14
    };
    static const QSet<int> brightCustomTypes = {
        0x00, 0x01, 0x02, 0x04
    };
    return brightTypes.contains(type)
        || (type == 0x27 && brightCustomTypes.contains(customType));
}

QString preampDisplayText(const QString &value, int offset)
{
    if (offset != 0x0E || value.compare("Center", Qt::CaseInsensitive) == 0
        || value.contains("cm", Qt::CaseInsensitive))
        return value;

    bool numeric = false;
    value.trimmed().toInt(&numeric);
    return numeric ? value.trimmed() + " cm" : value;
}

class EqBandArea : public QWidget
{
public:
    explicit EqBandArea(QWidget *parent = nullptr)
        : QWidget(parent), currentColumns(0)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
    }

    void addBand(QWidget *band)
    {
        if (!band)
            return;
        band->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        bands.append(band);
        updateBands();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        updateBands();
    }

private:
    void updateBands()
    {
        int fourColumnMinimum = grid->horizontalSpacing() * 3;
        for (QWidget *band : bands)
            fourColumnMinimum += qMax(220, band->minimumSizeHint().width());
        const int columns = width() >= fourColumnMinimum ? 4 : 2;
        if (columns == currentColumns && grid->count() == bands.size())
            return;
        currentColumns = columns;
        while (grid->count() > 0)
            delete grid->takeAt(0);
        for (int index = 0; index < bands.size(); ++index)
            grid->addWidget(bands.at(index), index / columns,
                            index % columns);
        for (int column = 0; column < 4; ++column)
            grid->setColumnStretch(column, column < columns ? 1 : 0);
        updateGeometry();
    }

    QGridLayout *grid;
    QList<QWidget *> bands;
    int currentColumns;
};

class EqBandColumn : public QWidget
{
public:
    explicit EqBandColumn(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);
        QLabel *heading = new QLabel(title.toUpper());
        heading->setObjectName("ParameterSectionTitle");
        heading->setMinimumHeight(18);
        column->addWidget(heading);
    }

    void addControl(QWidget *control)
    {
        if (control)
            column->addWidget(control);
    }

private:
    QVBoxLayout *column;
};

class ChannelRoutingDiagram : public QWidget
{
public:
    explicit ChannelRoutingDiagram(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(250, 190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setProperty("routingMode", 0);
        setProperty("routingChannel", 0);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int mode = property("routingMode").toInt();
        const int channel = property("routingChannel").toInt();
        const bool valid = mode >= 0 && mode <= 3;
        const QRectF area = rect().adjusted(20, 20, -20, -20);
        const qreal nodeX = area.left() + area.width() * 0.38;
        const qreal endX = area.right() - 18;
        const qreal centerY = area.center().y();
        const qreal pathAY = centerY - 45;
        const qreal pathBY = centerY + 45;
        const QColor active(ModernTheme::color(ModernTheme::AccentCyan));
        QColor inactive(ModernTheme::color(ModernTheme::Border));
        inactive.setAlpha(135);
        const bool aActive = valid && (mode != 0 || channel == 0);
        const bool bActive = valid && (mode != 0 || channel == 1);

        auto drawCable = [&painter](const QLineF &line,
                                    const QColor &color) {
            painter.setPen(QPen(QColor(0, 0, 0, 175), 7,
                                Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(line.translated(0, 3));
            painter.setPen(QPen(color, 2.5, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(line);
        };

        drawCable(QLineF(area.left(), centerY, nodeX, centerY), active);
        drawCable(QLineF(nodeX, pathAY, nodeX, pathBY), active);
        drawCable(QLineF(nodeX, pathAY, endX, pathAY),
                  aActive ? active : inactive);
        drawCable(QLineF(nodeX, pathBY, endX, pathBY),
                  bActive ? active : inactive);

        painter.setPen(QPen(active, 2));
        painter.setBrush(QColor(ModernTheme::color(
            ModernTheme::ControlBackground)));
        painter.drawEllipse(QPointF(nodeX, centerY), 7, 7);

        QFont labelFont("Helvetica Neue", 12, QFont::DemiBold);
        painter.setFont(labelFont);
        painter.setPen(aActive ? active : inactive);
        painter.drawText(QRectF(endX - 8, pathAY - 24, 34, 20),
                         Qt::AlignCenter, "A");
        painter.setPen(bActive ? active : inactive);
        painter.drawText(QRectF(endX - 8, pathBY + 5, 34, 20),
                         Qt::AlignCenter, "B");

        if (!valid) {
            painter.setPen(QColor(ModernTheme::color(
                ModernTheme::SecondaryText)));
            painter.setFont(QFont("Helvetica Neue", 9, QFont::DemiBold));
            painter.drawText(QRectF(area.left(), area.bottom() + 10,
                                    area.width(), 18),
                             Qt::AlignCenter, "NO PATCH DATA");
        }
    }
};
}

modernFloorBoard::modernFloorBoard(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ModernFloorBoard");
    setMinimumSize(1280, 800);

    setStyleSheet(ModernTheme::applicationStyleSheet());

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // HEADER
    QFrame *header = new QFrame;
    header->setObjectName("AppHeader");
    header->setFixedHeight(60);

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 6, 16, 6);

    QVBoxLayout *brandLayout = new QVBoxLayout;

    GtLabBrandWidget *title = new GtLabBrandWidget;

    QLabel *subtitle = new QLabel("BOSS GT-10");
    subtitle->setObjectName("BrandSubtitle");

    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();

    QVBoxLayout *patchNumberLayout = new QVBoxLayout;
    patchNumberLayout->setSpacing(1);
    QLabel *patchCaption = new QLabel("PATCH");
    patchCaption->setObjectName("PatchCaption");
    patchNumber = new QLabel(QString::fromUtf8("—"));
    patchNumber->setObjectName("PatchNumber");
    patchNumberLayout->addWidget(patchCaption);
    patchNumberLayout->addWidget(patchNumber);

    patchName = new QLabel("NO PATCH DATA");
    patchName->setObjectName("PatchName");

    QWidget *outputSelectHeader = new QWidget;
    outputSelectHeader->setObjectName("OutputSelectHeader");
    QVBoxLayout *outputSelectLayout = new QVBoxLayout(outputSelectHeader);
    outputSelectLayout->setContentsMargins(0, 0, 0, 0);
    outputSelectLayout->setSpacing(1);

    QLabel *outputSelectCaption = new QLabel("OUTPUT SELECT");
    outputSelectCaption->setObjectName("OutputSelectCaption");
    outputSelectCombo = new OutputSelectComboBox;
    outputSelectCombo->setObjectName("OutputSelectCombo");
    outputSelectCombo->setMinimumHeight(27);
    outputSelectCombo->setIconSize(QSize(18, 18));

    outputSelectCombo->addItem(QString::fromUtf8("—"), -1);
    if (QStandardItemModel *model =
            qobject_cast<QStandardItemModel *>(outputSelectCombo->model())) {
        if (QStandardItem *placeholder = model->item(0))
            placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsEnabled);
    }

    const Midi outputMap = MidiTable::Instance()->getMidiMap(
        "Structure", "00", "00", "11");
    for (const Midi &item : outputMap.level) {
        if (item.value == "range")
            continue;
        bool rawOk = false;
        const int raw = item.value.toInt(&rawOk, 16);
        if (!rawOk)
            continue;
        outputSelectCombo->addItem(outputSelectIcon(raw),
                                   item.name.trimmed().toUpper(), raw);
    }
    outputSelectCombo->setCurrentIndex(0);
    outputSelectCombo->setEnabled(false);
    outputSelectHeader->setStyleSheet(
        "QLabel#OutputSelectCaption { color: #78828e; font-size: 9px; "
        "font-weight: 700; letter-spacing: 1px; }"
        "QComboBox#OutputSelectCombo QAbstractItemView { color: #d8e1ea; "
        "background: #11171d; border: 1px solid #35414d; selection-background-color: #244b66; "
        "selection-color: #ffffff; outline: 0; padding: 4px; }"
        "QComboBox#OutputSelectCombo QAbstractItemView::item { "
        "min-height: 26px; padding: 2px 6px; border: 0; }"
    );
    outputSelectLayout->addWidget(outputSelectCaption);
    outputSelectLayout->addWidget(outputSelectCombo);
    connect(outputSelectCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &modernFloorBoard::outputSelectChanged);

    headerLayout->addLayout(patchNumberLayout);
    headerLayout->addSpacing(14);
    headerLayout->addWidget(patchName);
    headerLayout->addSpacing(24);
    headerLayout->addWidget(outputSelectHeader);

    root->addWidget(header);

    // BODY
    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(0);

    patchSidebar = new PatchSidebar(&patchListModel);
    connect(patchSidebar, SIGNAL(bankExpanded(int)),
            this, SIGNAL(requestPatchNames(int)));
    connect(patchSidebar, SIGNAL(patchActivated(int,int,QString)),
            this, SIGNAL(selectPatchRequested(int,int,QString)));
    body->addWidget(patchSidebar);

    // MAIN AREA
    QWidget *mainArea = new QWidget;

    QVBoxLayout *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(7);

    QLabel *chainTitle = new QLabel("SIGNAL CHAIN");
    chainTitle->setObjectName("SectionTitle");

    mainLayout->addWidget(chainTitle);

    signalChainPanel = new SignalChainPanel;
    signalChainScroll = new QScrollArea(signalChainPanel);
    signalChainScroll->setWidgetResizable(true);
    signalChainScroll->setFrameShape(QFrame::NoFrame);
    signalChainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    signalChainScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(signalChainScroll->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        if (value != 0 && signalChainScroll)
            signalChainScroll->verticalScrollBar()->setValue(0);
    });
    signalChainScroll->setStyleSheet(QString(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:horizontal{height:8px;background:%1;}"
        "QScrollBar::handle:horizontal{background:%2;border-radius:2px;min-width:40px;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::Border)));
    signalChainScroll->viewport()->installEventFilter(this);
    signalChainAutoScrollTimer = new QTimer(this);
    signalChainAutoScrollTimer->setInterval(40);
    connect(signalChainAutoScrollTimer, &QTimer::timeout, this, [this]() {
        if (!signalChainScroll || signalChainAutoScrollDirection == 0)
            return;
        QScrollBar *bar = signalChainScroll->horizontalScrollBar();
        bar->setValue(bar->value() + signalChainAutoScrollDirection * 12);
    });
    QVBoxLayout *chainPanelLayout = new QVBoxLayout(signalChainPanel);
    chainPanelLayout->setContentsMargins(6, 6, 6, 6);
    chainPanelLayout->addWidget(signalChainScroll);
    signalChainPanel->setFixedHeight(185);
    mainLayout->addWidget(signalChainPanel);
    rebuildSignalChainView();

    effectEditorStack = new QStackedWidget;

    reverbEditor = new EffectEditorPanel("REVERB");
    reverbTypeDisplay = reverbEditor->typeLabel();
    reverbTypeDisplay->hide();
    reverbModelBrowser = new EffectModelBrowser;
    reverbModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    reverbEditor->setModelBrowserWidget(reverbModelBrowser);
    connect(reverbModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::reverbModelSelected);
    reverbArtwork = new EffectArtworkWidget;
    reverbArtwork->setArtwork(":/assets/effects/reverb.png");
    QFont reverbDisplayFont;
    reverbDisplayFont.setFamily("Menlo");
    reverbDisplayFont.setBold(true);
    reverbDisplayFont.setStretch(QFont::Condensed);
    reverbArtwork->setTextOverlay(
        "type", QRectF(0.30, 0.244, 0.40, 0.064), QString(),
        reverbDisplayFont, QColor("#35F238"), Qt::AlignCenter, 0.66);
    reverbEditor->setArtworkWidget(reverbArtwork);
    QVBoxLayout *parameterLayout = new QVBoxLayout(reverbEditor->parameterArea());
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(8);

    QWidget *reverbPrimaryControls = new QWidget;
    QHBoxLayout *reverbPrimaryLayout =
        new QHBoxLayout(reverbPrimaryControls);
    reverbPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    reverbPrimaryLayout->setSpacing(0);
    EffectToggleControl *reverbToggle = new EffectToggleControl("State");
    reverbOnOff = reverbToggle->toggle();
    reverbOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    connect(reverbOnOff, SIGNAL(clicked()), this, SLOT(toggleReverb()));
    reverbPrimaryLayout->addWidget(reverbToggle, 0, Qt::AlignTop);
    reverbPrimaryLayout->addStretch(1);
    parameterLayout->addWidget(reverbPrimaryControls);

    QWidget *reverbTypeControl = createReverbCombo("Type", "31");
    reverbTypeControl->setParent(reverbEditor->parameterArea());
    reverbTypeControl->hide();

    QLabel *spaceTitle = new QLabel("SPACE");
    spaceTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(spaceTitle);
    parameterLayout->addWidget(createReverbBar("Time", "32"));
    parameterLayout->addWidget(createReverbBar("Pre Delay", "3A", true));
    parameterLayout->addWidget(createReverbBar("Density", "36"));
    parameterLayout->addWidget(
        createReverbBar("Spring Sensitivity", "39"));

    QLabel *filterTitle = new QLabel("FILTER");
    filterTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(filterTitle);
    parameterLayout->addWidget(createReverbCombo("Low Cut", "34"));
    parameterLayout->addWidget(createReverbCombo("High Cut", "35"));

    QLabel *mixTitle = new QLabel("MIX");
    mixTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(mixTitle);
    parameterLayout->addWidget(createReverbBar("Effect Level", "37"));
    parameterLayout->addWidget(createReverbBar("Direct Level", "38"));
    parameterLayout->addStretch(1);
    effectEditorStack->addWidget(reverbEditor);

    compEditor = new EffectEditorPanel("COMP");
    compTypeDisplay = compEditor->typeLabel();
    compTypeDisplay->hide();
    compModelBrowser = new EffectModelBrowser;
    compModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    compEditor->setModelBrowserWidget(compModelBrowser);
    connect(compModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::compModelSelected);
    EffectArtworkWidget *compArtwork = new EffectArtworkWidget;
    compArtwork->setArtwork(":/assets/effects/comp.png");
    compEditor->setArtworkWidget(compArtwork);

    QVBoxLayout *compParameterLayout = new QVBoxLayout(compEditor->parameterArea());
    compParameterLayout->setContentsMargins(0, 0, 0, 0);
    compParameterLayout->setSpacing(8);

    QWidget *compPrimaryControls = new QWidget;
    QHBoxLayout *compPrimaryLayout = new QHBoxLayout(compPrimaryControls);
    compPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    compPrimaryLayout->setSpacing(0);
    EffectToggleControl *compToggle = new EffectToggleControl("State");
    compOnOff = compToggle->toggle();
    compOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    connect(compOnOff, SIGNAL(clicked()), this, SLOT(toggleComp()));
    compPrimaryLayout->addWidget(compToggle, 0, Qt::AlignTop);
    compPrimaryLayout->addStretch(1);
    compParameterLayout->addWidget(compPrimaryControls);
    QWidget *compTypeControl = createCompCombo("Type", "41");
    compTypeControl->setParent(compEditor->parameterArea());
    compTypeControl->hide();

    compModeStack = new QStackedWidget;
    QWidget *compressorPage = new QWidget;
    QVBoxLayout *compressorLayout = new QVBoxLayout(compressorPage);
    compressorLayout->setContentsMargins(0, 0, 0, 0);
    compressorLayout->setSpacing(7);
    QLabel *compressorTitle = new QLabel("COMPRESSOR");
    compressorTitle->setObjectName("ParameterSectionTitle");
    compressorLayout->addWidget(compressorTitle);
    compressorLayout->addWidget(createCompBar("Sustain", "42"));
    compressorLayout->addWidget(createCompBar("Attack", "43"));
    compressorLayout->addWidget(createCompBar("Tone", "46"));
    compressorLayout->addWidget(createCompBar("Level", "47"));
    compressorLayout->addStretch(1);

    QWidget *limiterPage = new QWidget;
    QVBoxLayout *limiterLayout = new QVBoxLayout(limiterPage);
    limiterLayout->setContentsMargins(0, 0, 0, 0);
    limiterLayout->setSpacing(7);
    QLabel *limiterTitle = new QLabel("LIMITER");
    limiterTitle->setObjectName("ParameterSectionTitle");
    limiterLayout->addWidget(limiterTitle);
    limiterLayout->addWidget(createCompBar("Threshold", "44"));
    limiterLayout->addWidget(createCompBar("Release", "45"));
    limiterLayout->addWidget(createCompBar("Tone", "46"));
    limiterLayout->addWidget(createCompBar("Level", "47"));
    limiterLayout->addStretch(1);

    compModeStack->addWidget(compressorPage);
    compModeStack->addWidget(limiterPage);
    compParameterLayout->addWidget(compModeStack, 1);
    effectEditorStack->addWidget(compEditor);

    oddsEditor = new EffectEditorPanel("OD/DS");
    oddsEditor->typeLabel()->hide();
    oddsModelBrowser = new EffectModelBrowser;
    oddsModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsModelBrowser->setCategoriesCollapsible(true);
    oddsEditor->setModelBrowserWidget(oddsModelBrowser);
    connect(oddsModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::oddsModelSelected);
    oddsArtwork = new EffectArtworkWidget;
    oddsArtwork->setArtwork(":/assets/effects/od_ds.png");
    QFont oddsDisplayFont;
    oddsDisplayFont.setFamily("Menlo");
    oddsDisplayFont.setBold(true);
    oddsDisplayFont.setStretch(QFont::Condensed);
    oddsArtwork->setTextOverlay(
        "type", QRectF(0.27, 0.245, 0.46, 0.058), QString(),
        oddsDisplayFont, QColor("#FFC21A"), Qt::AlignCenter, 0.52);
    oddsEditor->setArtworkWidget(oddsArtwork);

    QVBoxLayout *oddsParameterLayout =
        new QVBoxLayout(oddsEditor->parameterArea());
    oddsParameterLayout->setContentsMargins(0, 0, 0, 0);
    oddsParameterLayout->setSpacing(8);

    QWidget *oddsPrimaryControls = new QWidget;
    QHBoxLayout *oddsPrimaryLayout = new QHBoxLayout(oddsPrimaryControls);
    oddsPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    oddsPrimaryLayout->setSpacing(0);
    EffectToggleControl *oddsToggle = new EffectToggleControl("State");
    oddsOnOff = oddsToggle->toggle();
    oddsOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsOnOff->setProperty("address", "70");
    connect(oddsOnOff, SIGNAL(clicked()), this, SLOT(oddsToggleChanged()));
    oddsPrimaryLayout->addWidget(oddsToggle, 0, Qt::AlignTop);
    oddsPrimaryLayout->addStretch(1);
    oddsParameterLayout->addWidget(oddsPrimaryControls);

    QWidget *oddsTypeControl = createOddsCombo("Type", "71");
    oddsTypeControl->setParent(oddsEditor->parameterArea());
    oddsTypeControl->hide();

    QLabel *oddsCommonTitle = new QLabel("DRIVE / MIX");
    oddsCommonTitle->setObjectName("ParameterSectionTitle");
    oddsParameterLayout->addWidget(oddsCommonTitle);
    oddsParameterLayout->addWidget(createOddsBar("Drive", "72"));
    oddsParameterLayout->addWidget(createOddsBar("Bottom", "73"));
    oddsParameterLayout->addWidget(createOddsBar("Tone", "74"));
    oddsParameterLayout->addWidget(createOddsBar("Effect", "75"));
    oddsParameterLayout->addWidget(createOddsBar("Direct", "76"));
    oddsParameterLayout->addWidget(createOddsBar("Solo Level", "78"));

    QLabel *oddsSoloTitle = new QLabel("SOLO");
    oddsSoloTitle->setObjectName("ParameterSectionTitle");
    oddsParameterLayout->addWidget(oddsSoloTitle);
    EffectToggleControl *oddsSolo = new EffectToggleControl("Solo Switch");
    oddsSoloSwitch = oddsSolo->toggle();
    oddsSoloSwitch->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsSoloSwitch->setProperty("address", "77");
    connect(oddsSoloSwitch, SIGNAL(clicked()),
            this, SLOT(oddsToggleChanged()));
    oddsParameterLayout->addWidget(oddsSolo, 0, Qt::AlignLeft);

    QWidget *customSection = new QWidget;
    QVBoxLayout *customLayout = new QVBoxLayout(customSection);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(7);
    QLabel *customTitle = new QLabel("CUSTOM");
    customTitle->setObjectName("ParameterSectionTitle");
    customLayout->addWidget(customTitle);
    customLayout->addWidget(createOddsCombo("Custom Type", "79"));
    customLayout->addWidget(createOddsBar("Bottom", "7A"));
    customLayout->addWidget(createOddsBar("Top", "7B"));
    customLayout->addWidget(createOddsBar("Low", "7C"));
    customLayout->addWidget(createOddsBar("High", "7D"));
    oddsCustomSection = customSection;
    oddsCustomSection->hide();
    oddsParameterLayout->addWidget(oddsCustomSection);
    oddsParameterLayout->addStretch(1);
    effectEditorStack->addWidget(oddsEditor);

    delayEditor = new EffectEditorPanel("DELAY");
    delayEditor->typeLabel()->hide();
    delayModelBrowser = new EffectModelBrowser;
    delayModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayEditor->setModelBrowserWidget(delayModelBrowser);
    connect(delayModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::delayModelSelected);
    delayArtwork = new EffectArtworkWidget;
    delayArtwork->setArtwork(":/assets/effects/delay.png");
    QFont delayDisplayFont;
    delayDisplayFont.setFamily("Menlo");
    delayDisplayFont.setBold(true);
    delayDisplayFont.setStretch(QFont::Condensed);
    delayArtwork->setTextOverlay(
        "type", QRectF(0.255, 0.242, 0.49, 0.048), QString(),
        delayDisplayFont, QColor("#35D8FF"), Qt::AlignCenter, 0.50);
    delayEditor->setArtworkWidget(delayArtwork);

    QVBoxLayout *delayParameterLayout =
        new QVBoxLayout(delayEditor->parameterArea());
    delayParameterLayout->setContentsMargins(0, 0, 0, 0);
    delayParameterLayout->setSpacing(8);

    QWidget *delayPrimaryControls = new QWidget;
    QHBoxLayout *delayPrimaryLayout = new QHBoxLayout(delayPrimaryControls);
    delayPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    delayPrimaryLayout->setSpacing(0);
    EffectToggleControl *delayToggle = new EffectToggleControl("State");
    delayOnOff = delayToggle->toggle();
    delayOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayOnOff->setProperty("address", "00");
    connect(delayOnOff, SIGNAL(clicked()), this, SLOT(delayToggleChanged()));
    delayPrimaryLayout->addWidget(delayToggle, 0, Qt::AlignTop);
    delayPrimaryLayout->addStretch(1);
    delayParameterLayout->addWidget(delayPrimaryControls);

    QWidget *delayTypeControl = createDelayCombo("Type", "01");
    delayTypeControl->setParent(delayEditor->parameterArea());
    delayTypeControl->hide();

    delayPageStack = new QStackedWidget;
    QWidget *delayStandardPage = new QWidget;
    QVBoxLayout *delayStandardLayout = new QVBoxLayout(delayStandardPage);
    delayStandardLayout->setContentsMargins(0, 0, 0, 0);
    delayStandardLayout->setSpacing(8);
    QLabel *delayCommonTitle = new QLabel("DELAY");
    delayCommonTitle->setObjectName("ParameterSectionTitle");
    delayStandardLayout->addWidget(delayCommonTitle);
    delayStandardLayout->addWidget(createDelayBar("Time", "02", true));
    delayStandardLayout->addWidget(createDelayBar("Feedback", "05"));
    delayStandardLayout->addWidget(createDelayCombo("High Cut", "06"));
    delayStandardLayout->addWidget(createDelayBar("Effect", "17"));
    delayStandardLayout->addWidget(createDelayBar("Direct", "18"));

    delayExtraStack = new QStackedWidget;
    delayExtraStack->addWidget(new QWidget);
    QWidget *delayPanSection = new QWidget;
    QVBoxLayout *delayPanLayout = new QVBoxLayout(delayPanSection);
    delayPanLayout->setContentsMargins(0, 0, 0, 0);
    delayPanLayout->setSpacing(7);
    QLabel *delayPanTitle = new QLabel("PAN");
    delayPanTitle->setObjectName("ParameterSectionTitle");
    delayPanLayout->addWidget(delayPanTitle);
    delayPanLayout->addWidget(createDelayBar("Tap Time", "04"));
    delayExtraStack->addWidget(delayPanSection);

    QWidget *delayWarpSection = new QWidget;
    QVBoxLayout *delayWarpLayout = new QVBoxLayout(delayWarpSection);
    delayWarpLayout->setContentsMargins(0, 0, 0, 0);
    delayWarpLayout->setSpacing(7);
    QLabel *delayWarpTitle = new QLabel("WARP");
    delayWarpTitle->setObjectName("ParameterSectionTitle");
    delayWarpLayout->addWidget(delayWarpTitle);
    EffectToggleControl *delayWarpToggle =
        new EffectToggleControl("Warp Switch");
    delayWarpSwitch = delayWarpToggle->toggle();
    delayWarpSwitch->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayWarpSwitch->setProperty("address", "11");
    connect(delayWarpSwitch, SIGNAL(clicked()),
            this, SLOT(delayToggleChanged()));
    delayWarpLayout->addWidget(delayWarpToggle, 0, Qt::AlignLeft);
    delayWarpLayout->addWidget(createDelayBar("Rise Time", "12"));
    delayWarpLayout->addWidget(createDelayBar("Feedback Depth", "13"));
    delayWarpLayout->addWidget(createDelayBar("Depth Level", "14"));
    delayExtraStack->addWidget(delayWarpSection);

    QWidget *delayModSection = new QWidget;
    QVBoxLayout *delayModLayout = new QVBoxLayout(delayModSection);
    delayModLayout->setContentsMargins(0, 0, 0, 0);
    delayModLayout->setSpacing(7);
    QLabel *delayModTitle = new QLabel("MODULATION");
    delayModTitle->setObjectName("ParameterSectionTitle");
    delayModLayout->addWidget(delayModTitle);
    delayModLayout->addWidget(createDelayBar("Rate", "15"));
    delayModLayout->addWidget(createDelayBar("Depth", "16"));
    delayExtraStack->addWidget(delayModSection);
    delayStandardLayout->addWidget(delayExtraStack);
    delayStandardLayout->addStretch(1);
    delayPageStack->addWidget(delayStandardPage);

    QWidget *delayDualPage = new QWidget;
    QVBoxLayout *delayDualLayout = new QVBoxLayout(delayDualPage);
    delayDualLayout->setContentsMargins(0, 0, 0, 0);
    delayDualLayout->setSpacing(8);
    ResponsiveSectionArea *delayDualArea = new ResponsiveSectionArea;
    QWidget *delayOneSection = new QWidget;
    QVBoxLayout *delayOneLayout = new QVBoxLayout(delayOneSection);
    delayOneLayout->setContentsMargins(0, 0, 0, 0);
    delayOneLayout->setSpacing(7);
    QLabel *delayOneTitle = new QLabel("DELAY 1");
    delayOneTitle->setObjectName("ParameterSectionTitle");
    delayOneLayout->addWidget(delayOneTitle);
    delayOneLayout->addWidget(createDelayBar("Time", "07", true));
    delayOneLayout->addWidget(createDelayBar("Feedback", "09"));
    delayOneLayout->addWidget(createDelayCombo("High Cut", "0A"));
    delayOneLayout->addWidget(createDelayBar("Effect", "0B"));

    QWidget *delayTwoSection = new QWidget;
    QVBoxLayout *delayTwoLayout = new QVBoxLayout(delayTwoSection);
    delayTwoLayout->setContentsMargins(0, 0, 0, 0);
    delayTwoLayout->setSpacing(7);
    QLabel *delayTwoTitle = new QLabel("DELAY 2");
    delayTwoTitle->setObjectName("ParameterSectionTitle");
    delayTwoLayout->addWidget(delayTwoTitle);
    delayTwoLayout->addWidget(createDelayBar("Time", "0C", true));
    delayTwoLayout->addWidget(createDelayBar("Feedback", "0E"));
    delayTwoLayout->addWidget(createDelayCombo("High Cut", "0F"));
    delayTwoLayout->addWidget(createDelayBar("Effect", "10"));
    delayDualArea->addSection(delayOneSection);
    delayDualArea->addSection(delayTwoSection);
    delayDualLayout->addWidget(delayDualArea);
    QLabel *delayDualCommonTitle = new QLabel("COMMON");
    delayDualCommonTitle->setObjectName("ParameterSectionTitle");
    delayDualLayout->addWidget(delayDualCommonTitle);
    delayDualLayout->addWidget(createDelayBar("Direct", "18"));
    delayDualLayout->addStretch(1);
    delayPageStack->addWidget(delayDualPage);
    delayParameterLayout->addWidget(delayPageStack);
    delayParameterLayout->addStretch(1);
    effectEditorStack->addWidget(delayEditor);

    chorusEditor = new EffectEditorPanel("CHORUS");
    chorusEditor->typeLabel()->hide();
    chorusModeBrowser = new EffectModelBrowser;
    chorusModeBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("CHORUS")));
    chorusEditor->setRightPanelTitle("CHORUS MODES");
    chorusEditor->setModelBrowserWidget(chorusModeBrowser);
    connect(chorusModeBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::chorusModeSelected);

    QWidget *chorusModeControl = createChorusCombo("Mode", "21");
    chorusModeControl->setParent(chorusEditor->parameterArea());
    chorusModeControl->hide();
    QStringList chorusModes;
    if (chorusMode) {
        for (int index = 0; index < chorusMode->count(); ++index)
            chorusModes.append(chorusMode->itemText(index));
    }
    chorusModeBrowser->setModels(chorusModes);

    chorusArtwork = new EffectArtworkWidget;
    chorusArtwork->setArtwork(":/assets/effects/chorus.png");
    QFont chorusDisplayFont;
    chorusDisplayFont.setFamily("Menlo");
    chorusDisplayFont.setBold(true);
    chorusDisplayFont.setStretch(QFont::Condensed);
    chorusArtwork->setTextOverlay(
        "mode", QRectF(0.29, 0.229, 0.42, 0.057), QString(),
        chorusDisplayFont, QColor("#35D8FF"), Qt::AlignCenter, 0.50);
    chorusEditor->setArtworkWidget(chorusArtwork);

    QVBoxLayout *chorusParameterLayout =
        new QVBoxLayout(chorusEditor->parameterArea());
    chorusParameterLayout->setContentsMargins(0, 0, 0, 0);
    chorusParameterLayout->setSpacing(7);

    QWidget *chorusPrimaryControls = new QWidget;
    QHBoxLayout *chorusPrimaryLayout =
        new QHBoxLayout(chorusPrimaryControls);
    chorusPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    chorusPrimaryLayout->setSpacing(0);
    EffectToggleControl *chorusToggle = new EffectToggleControl("State");
    chorusOnOff = chorusToggle->toggle();
    chorusOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("CHORUS")));
    connect(chorusOnOff, SIGNAL(clicked()), this, SLOT(toggleChorus()));
    chorusPrimaryLayout->addWidget(chorusToggle, 0, Qt::AlignTop);
    chorusPrimaryLayout->addStretch(1);
    chorusParameterLayout->addWidget(chorusPrimaryControls);

    QLabel *chorusModulationTitle = new QLabel("MODULATION");
    chorusModulationTitle->setObjectName("ParameterSectionTitle");
    chorusParameterLayout->addWidget(chorusModulationTitle);
    chorusParameterLayout->addWidget(createChorusBar("Rate", "22"));
    chorusParameterLayout->addWidget(createChorusBar("Depth", "23"));

    QLabel *chorusTimingTitle = new QLabel("TIMING");
    chorusTimingTitle->setObjectName("ParameterSectionTitle");
    chorusParameterLayout->addWidget(chorusTimingTitle);
    chorusParameterLayout->addWidget(createChorusBar("Pre Delay", "24"));

    QLabel *chorusFilterTitle = new QLabel("FILTER");
    chorusFilterTitle->setObjectName("ParameterSectionTitle");
    chorusParameterLayout->addWidget(chorusFilterTitle);
    QWidget *chorusFilterControls = new QWidget;
    QHBoxLayout *chorusFilterLayout = new QHBoxLayout(chorusFilterControls);
    chorusFilterLayout->setContentsMargins(0, 0, 0, 0);
    chorusFilterLayout->setSpacing(10);
    chorusFilterLayout->addWidget(createChorusCombo("Low Cut", "25"));
    chorusFilterLayout->addWidget(createChorusCombo("High Cut", "26"));
    chorusFilterLayout->addStretch(1);
    chorusParameterLayout->addWidget(chorusFilterControls);

    QLabel *chorusOutputTitle = new QLabel("OUTPUT");
    chorusOutputTitle->setObjectName("ParameterSectionTitle");
    chorusParameterLayout->addWidget(chorusOutputTitle);
    chorusParameterLayout->addWidget(createChorusBar("Effect Level", "27"));
    chorusParameterLayout->addStretch(1);
    effectEditorStack->addWidget(chorusEditor);

    QFrame *eqFullWidthEditor = new QFrame;
    eqFullWidthEditor->setObjectName("EffectEditorPanel");
    eqEditor = eqFullWidthEditor;
    QVBoxLayout *eqLayout = new QVBoxLayout(eqFullWidthEditor);
    eqLayout->setContentsMargins(10, 8, 10, 8);
    eqLayout->setSpacing(5);

    QWidget *eqHeader = new QWidget;
    QHBoxLayout *eqHeaderLayout = new QHBoxLayout(eqHeader);
    eqHeaderLayout->setContentsMargins(0, 0, 0, 0);
    eqHeaderLayout->setSpacing(12);
    QLabel *eqTitle = new QLabel("EQ");
    eqTitle->setObjectName("EditorTitle");
    eqTitle->setStyleSheet(QString("color:%1;").arg(
        ModernTheme::activeEffectAccent("EQ")));
    EffectToggleControl *eqToggle = new EffectToggleControl("State");
    eqOnOff = eqToggle->toggle();
    eqOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("EQ")));
    connect(eqOnOff, SIGNAL(clicked()), this, SLOT(toggleEq()));
    eqHeaderLayout->addWidget(eqToggle, 0, Qt::AlignTop);
    eqHeaderLayout->addStretch(1);
    eqHeaderLayout->addWidget(eqTitle);
    eqLayout->addWidget(eqHeader);

    eqGraph = new ModernEqGraph;
    eqLayout->addWidget(eqGraph, 1);

    QWidget *eqControlsContent = new QWidget;
    QVBoxLayout *eqControlsLayout = new QVBoxLayout(eqControlsContent);
    eqControlsLayout->setContentsMargins(0, 0, 0, 0);
    eqControlsLayout->setSpacing(4);

    EqBandArea *eqBandArea = new EqBandArea;

    EqBandColumn *eqLow = new EqBandColumn("LOW");
    eqLow->addControl(createEqCombo("Low Cut", "71"));
    eqLow->addControl(createEqBar("Low Gain", "72"));
    eqBandArea->addBand(eqLow);

    EqBandColumn *eqLowMid = new EqBandColumn("LOW-MID");
    eqLowMid->addControl(createEqCombo("Frequency", "73"));
    eqLowMid->addControl(createEqCombo("Q", "74"));
    eqLowMid->addControl(createEqBar("Gain", "75"));
    eqBandArea->addBand(eqLowMid);

    EqBandColumn *eqHighMid = new EqBandColumn("HIGH-MID");
    eqHighMid->addControl(createEqCombo("Frequency", "76"));
    eqHighMid->addControl(createEqCombo("Q", "77"));
    eqHighMid->addControl(createEqBar("Gain", "78"));
    eqBandArea->addBand(eqHighMid);

    EqBandColumn *eqHigh = new EqBandColumn("HIGH");
    eqHigh->addControl(createEqCombo("High Cut", "7A"));
    eqHigh->addControl(createEqBar("High Gain", "79"));
    eqBandArea->addBand(eqHigh);
    eqControlsLayout->addWidget(eqBandArea);

    EqBandColumn *eqOutput = new EqBandColumn("OUTPUT");
    eqOutput->addControl(createEqBar("Level", "7B"));
    QWidget *eqOutputRow = new QWidget;
    QHBoxLayout *eqOutputLayout = new QHBoxLayout(eqOutputRow);
    eqOutputLayout->setContentsMargins(0, 0, 0, 0);
    eqOutputLayout->setSpacing(0);
    eqOutputLayout->addStretch(1);
    eqOutputLayout->addWidget(eqOutput, 4);
    eqOutputLayout->addStretch(1);
    eqControlsLayout->addWidget(eqOutputRow);

    QScrollArea *eqControlsScroll = new QScrollArea;
    eqControlsScroll->setObjectName("EffectParameterScroll");
    eqControlsScroll->setWidgetResizable(true);
    eqControlsScroll->setFrameShape(QFrame::NoFrame);
    eqControlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    eqControlsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    eqControlsScroll->setWidget(
        createParameterScrollContent(eqControlsContent));
    eqLayout->addWidget(eqControlsScroll, 1);
    effectEditorStack->addWidget(eqEditor);

    effectEditorStack->addWidget(createPreampEditor(PreampChannel::A));
    effectEditorStack->addWidget(createPreampEditor(PreampChannel::B));
    channelRoutingEditor = createChannelRoutingEditor();
    effectEditorStack->addWidget(channelRoutingEditor);
    fx1Editor = new ModernFxEditor(FxSlot::FX1, this);
    effectEditorStack->addWidget(fx1Editor->widget());
    connect(fx1Editor, &ModernFxEditor::stateChanged,
            this, &modernFloorBoard::fx1StateChanged);
    fx2Editor = new ModernFxEditor(FxSlot::FX2, this);
    effectEditorStack->addWidget(fx2Editor->widget());
    connect(fx2Editor, &ModernFxEditor::stateChanged,
            this, &modernFloorBoard::fx2StateChanged);
    pedalFxEditor = new ModernPedalFxEditor(this);
    effectEditorStack->addWidget(pedalFxEditor->widget());
    connect(pedalFxEditor, &ModernPedalFxEditor::activityChanged,
            this, &modernFloorBoard::pedalFxActivityChanged);
    ns1Editor = new ModernNoiseSuppressorEditor(
        NoiseSuppressorSlot::NS1, this);
    effectEditorStack->addWidget(ns1Editor->widget());
    connect(ns1Editor, &ModernNoiseSuppressorEditor::stateChanged,
            this, [this](bool available, bool on) {
        if (ns1Card)
            ns1Card->setEffectState(available, on);
    });
    ns2Editor = new ModernNoiseSuppressorEditor(
        NoiseSuppressorSlot::NS2, this);
    effectEditorStack->addWidget(ns2Editor->widget());
    connect(ns2Editor, &ModernNoiseSuppressorEditor::stateChanged,
            this, [this](bool available, bool on) {
        if (ns2Card)
            ns2Card->setEffectState(available, on);
    });
    sendReturnEditor = new ModernSendReturnEditor(this);
    effectEditorStack->addWidget(sendReturnEditor->widget());
    connect(sendReturnEditor, &ModernSendReturnEditor::stateChanged,
            this, [this](bool available, bool on) {
        if (sendReturnCard)
            sendReturnCard->setEffectState(available, on);
    });

    mainLayout->addWidget(effectEditorStack, 1);

    BottomControlStrip *bottomControlStrip = new BottomControlStrip;
    tunerReferenceCombo = bottomControlStrip->tunerReferenceComboBox();
    tunerOutputCombo = bottomControlStrip->tunerOutputComboBox();
    connect(tunerReferenceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &modernFloorBoard::tunerReferenceChanged);
    connect(tunerOutputCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &modernFloorBoard::tunerOutputChanged);
    bottomControlStrip->setFixedHeight(122);
    mainLayout->addWidget(bottomControlStrip);

    body->addWidget(mainArea, 1);

    root->addLayout(body, 1);

    backendDisconnected();
}

modernFloorBoard::PreampEditorState &modernFloorBoard::preampState(
    PreampChannel channel)
{
    return channel == PreampChannel::A ? preampA : preampB;
}

const modernFloorBoard::PreampEditorState &modernFloorBoard::preampState(
    PreampChannel channel) const
{
    return channel == PreampChannel::A ? preampA : preampB;
}

QString modernFloorBoard::preampAddress(PreampChannel channel,
                                         int offset) const
{
    const int base = channel == PreampChannel::A ? 0x10 : 0x30;
    return QString("%1").arg(base + offset, 2, 16, QChar('0')).toUpper();
}

EffectEditorPanel *modernFloorBoard::createPreampEditor(
    PreampChannel channel)
{
    PreampEditorState &state = preampState(channel);
    const QString name = channel == PreampChannel::A
        ? "PREAMP A" : "PREAMP B";
    const int channelValue = channel == PreampChannel::A ? 0 : 1;
    const QColor accent(ModernTheme::activeEffectAccent(name));

    state.editor = new EffectEditorPanel(name);
    state.editor->typeLabel()->hide();
    state.browser = new EffectModelBrowser;
    state.browser->setAccentColor(accent);
    state.browser->setProperty("preampChannel", channelValue);
    state.editor->setModelBrowserWidget(state.browser);
    connect(state.browser, SIGNAL(modelSelected(int)),
            this, SLOT(preampModelSelected(int)));

    state.artwork = new EffectArtworkWidget;
    state.artwork->setArtwork(channel == PreampChannel::A
        ? ":/assets/effects/preamp_a.png"
        : ":/assets/effects/preamp_b.png");
    state.editor->setArtworkWidget(state.artwork);

    QVBoxLayout *layout = new QVBoxLayout(state.editor->parameterArea());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QWidget *primary = new QWidget;
    QHBoxLayout *primaryLayout = new QHBoxLayout(primary);
    primaryLayout->setContentsMargins(0, 0, 0, 0);
    EffectToggleControl *globalControl =
        new EffectToggleControl("State (A/B)");
    state.globalState = globalControl->toggle();
    state.globalState->setAccentColor(accent);
    state.globalState->setProperty("preampChannel", channelValue);
    state.globalState->setProperty("preampGlobalState", true);
    state.toggles.append(state.globalState);
    connect(state.globalState, SIGNAL(clicked()),
            this, SLOT(preampToggleChanged()));
    primaryLayout->addWidget(globalControl, 0, Qt::AlignTop);
    primaryLayout->addStretch(1);
    layout->addWidget(primary);

    QWidget *typeControl = createPreampCombo(channel, "Type", 0x00);
    typeControl->setParent(state.editor->parameterArea());
    typeControl->hide();

    auto addTitle = [layout](const QString &text) {
        QLabel *title = new QLabel(text);
        title->setObjectName("ParameterSectionTitle");
        layout->addWidget(title);
    };

    addTitle("AMP");
    layout->addWidget(createPreampBar(channel, "Gain", 0x01));
    layout->addWidget(createPreampBar(channel, "Level", 0x06));
    layout->addWidget(createPreampCombo(channel, "Gain Switch", 0x08));
    state.brightControl = createPreampToggle(
        channel, "Bright", 0x07, &state.bright);
    layout->addWidget(state.brightControl);

    addTitle("EQ");
    layout->addWidget(createPreampBar(channel, "Bass", 0x02));
    layout->addWidget(createPreampBar(channel, "Middle", 0x03));
    layout->addWidget(createPreampBar(channel, "Treble", 0x04));
    layout->addWidget(createPreampBar(channel, "Presence", 0x05));

    addTitle("SOLO");
    layout->addWidget(createPreampToggle(
        channel, "Solo", 0x09, &state.solo));
    layout->addWidget(createPreampBar(channel, "Solo Level", 0x0A));

    addTitle("SPEAKER");
    layout->addWidget(createPreampCombo(
        channel, "Speaker Type", 0x0B));

    state.customSpeakerSection = new QWidget;
    QVBoxLayout *customSpeaker =
        new QVBoxLayout(state.customSpeakerSection);
    customSpeaker->setContentsMargins(0, 0, 0, 0);
    customSpeaker->setSpacing(8);
    QLabel *customSpeakerTitle = new QLabel("CUSTOM SPEAKER");
    customSpeakerTitle->setObjectName("ParameterSectionTitle");
    customSpeaker->addWidget(customSpeakerTitle);
    customSpeaker->addWidget(createPreampBar(channel, "Size", 0x18));
    customSpeaker->addWidget(createPreampBar(
        channel, "Color Low", 0x19));
    customSpeaker->addWidget(createPreampBar(
        channel, "Color High", 0x1A));
    customSpeaker->addWidget(createPreampCombo(
        channel, "Speaker Number", 0x1B));
    customSpeaker->addWidget(createPreampCombo(
        channel, "Cabinet Back", 0x1C));
    layout->addWidget(state.customSpeakerSection);

    addTitle("MIC / MIX");
    layout->addWidget(createPreampCombo(channel, "Mic Type", 0x0C));
    layout->addWidget(createPreampCombo(
        channel, "Mic Distance", 0x0D));
    layout->addWidget(createPreampBar(
        channel, "Mic Position", 0x0E));
    layout->addWidget(createPreampBar(channel, "Mic Level", 0x0F));
    layout->addWidget(createPreampBar(channel, "Direct Level", 0x10));

    state.customPreampSection = new QWidget;
    QVBoxLayout *customPreamp =
        new QVBoxLayout(state.customPreampSection);
    customPreamp->setContentsMargins(0, 0, 0, 0);
    customPreamp->setSpacing(8);
    QLabel *customPreampTitle = new QLabel("CUSTOM PREAMP");
    customPreampTitle->setObjectName("ParameterSectionTitle");
    customPreamp->addWidget(customPreampTitle);
    customPreamp->addWidget(createPreampCombo(
        channel, "Custom Preamp Type", 0x11));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Bottom", 0x12));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Edge", 0x13));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Bass Frequency", 0x14));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Treble Frequency", 0x15));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Pre Low", 0x16));
    customPreamp->addWidget(createPreampBar(
        channel, "Custom Pre High", 0x17));
    layout->addWidget(state.customPreampSection);
    layout->addStretch(1);

    state.customPreampSection->hide();
    state.customSpeakerSection->hide();
    state.brightControl->hide();
    return state.editor;
}

QWidget *modernFloorBoard::createPreampCombo(PreampChannel channel,
                                              const QString &label,
                                              int offset)
{
    PreampEditorState &state = preampState(channel);
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("preampChannel",
                       channel == PreampChannel::A ? 0 : 1);
    combo->setProperty("preampOffset", offset);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "01", "00", preampAddress(channel, offset));
    QStringList labels;
    for (const Midi &item : parameter.level) {
        if (item.value == "range")
            continue;
        const QString text = !item.customdesc.isEmpty()
            ? item.customdesc
            : (!item.desc.isEmpty() ? item.desc : item.name);
        combo->addItem(text);
        labels.append(text);
    }
    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(preampComboChanged(int)));
    state.combos.append(combo);
    if (offset == 0x00) {
        state.type = combo;
        if (state.browser)
            state.browser->setModels(labels);
    } else if (offset == 0x0B) {
        state.speakerType = combo;
    } else if (offset == 0x11) {
        state.customType = combo;
    }
    return container;
}

QWidget *modernFloorBoard::createPreampBar(PreampChannel channel,
                                            const QString &label,
                                            int offset)
{
    PreampEditorState &state = preampState(channel);
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(ModernTheme::activeEffectAccent(
        channel == PreampChannel::A ? "PREAMP A" : "PREAMP B")));
    bar->setProperty("preampChannel",
                     channel == PreampChannel::A ? 0 : 1);
    bar->setProperty("preampOffset", offset);
    const QString address = preampAddress(channel, offset);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "01", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "01", "00", address),
                  midiTable->getRange(
                      "Structure", "01", "00", address));

    if (offset >= 0x12 && offset <= 0x17)
        bar->setCenterValue(5);
    else if (offset == 0x19 || offset == 0x1A)
        bar->setCenterValue(10);
    else {
        int rawCenter = 0;
        if (centerValueFromMapping(parameter, &rawCenter))
            bar->setCenterValue(rawCenter);
    }
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::preampBarChanged);
    state.bars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createPreampToggle(
    PreampChannel channel, const QString &label, int offset,
    ModernToggleSwitch **target)
{
    PreampEditorState &state = preampState(channel);
    EffectToggleControl *control = new EffectToggleControl(label);
    ModernToggleSwitch *toggle = control->toggle();
    toggle->setAccentColor(QColor(ModernTheme::activeEffectAccent(
        channel == PreampChannel::A ? "PREAMP A" : "PREAMP B")));
    toggle->setProperty("preampChannel",
                        channel == PreampChannel::A ? 0 : 1);
    toggle->setProperty("preampOffset", offset);
    connect(toggle, SIGNAL(clicked()), this, SLOT(preampToggleChanged()));
    state.toggles.append(toggle);
    if (target)
        *target = toggle;
    return control;
}

QWidget *modernFloorBoard::createChannelRoutingEditor()
{
    QFrame *editor = new QFrame;
    editor->setObjectName("EffectEditorPanel");
    QHBoxLayout *root = new QHBoxLayout(editor);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *diagramPane = new QFrame;
    diagramPane->setObjectName("EffectArtworkPane");
    diagramPane->setMinimumWidth(280);
    QVBoxLayout *diagramLayout = new QVBoxLayout(diagramPane);
    diagramLayout->setContentsMargins(12, 12, 12, 12);
    diagramLayout->setSpacing(5);
    QLabel *title = new QLabel("CHANNEL ROUTING");
    title->setObjectName("EditorTitle");
    title->setStyleSheet(QString("color:%1;").arg(
        ModernTheme::color(ModernTheme::AccentCyan)));
    QLabel *subtitle = new QLabel("PREAMP A/B");
    subtitle->setObjectName("EffectTypeDisplay");
    diagramLayout->addWidget(title);
    diagramLayout->addWidget(subtitle);
    channelRoutingDiagram = new ChannelRoutingDiagram;
    diagramLayout->addWidget(channelRoutingDiagram, 1);

    QFrame *parameterPane = new QFrame;
    parameterPane->setObjectName("EffectParameterPane");
    parameterPane->setMinimumWidth(520);
    QVBoxLayout *parameterPaneLayout = new QVBoxLayout(parameterPane);
    parameterPaneLayout->setContentsMargins(10, 10, 10, 10);
    parameterPaneLayout->setSpacing(6);
    QWidget *parameters = new QWidget;
    parameters->setObjectName("EffectParameterArea");
    QVBoxLayout *layout = new QVBoxLayout(parameters);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);
    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName("EffectParameterScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(createParameterScrollContent(parameters));
    parameterPaneLayout->addWidget(scroll, 1);

    ParameterCombo *modeControl = new ParameterCombo("Mode");
    channelMode = modeControl->comboBox();
    const Midi modeParameter = MidiTable::Instance()->getMidiMap(
        "Structure", "01", "00", "01");
    for (int index = 0; index < qMin(4, modeParameter.level.size()); ++index) {
        const Midi &item = modeParameter.level.at(index);
        channelMode->addItem(item.desc.isEmpty() ? item.name : item.desc);
    }
    connect(channelMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(channelModeChanged(int)));
    modeControl->hide();

    QLabel *modeLabel = new QLabel("CHANNEL MODE");
    modeLabel->setObjectName("ParameterLabel");
    layout->addWidget(modeLabel);
    QWidget *modeSelector = new QWidget;
    QHBoxLayout *modeSelectorLayout = new QHBoxLayout(modeSelector);
    modeSelectorLayout->setContentsMargins(0, 0, 0, 0);
    modeSelectorLayout->setSpacing(1);
    const QString modeSelectorStyle = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %3;"
        "border-radius:4px;font-size:10px;font-weight:600;padding:0 8px;}"
        "QPushButton:hover{border-color:%4;color:%5;}"
        "QPushButton:checked{background:%6;color:%5;border-color:%4;}"
        "QPushButton:disabled{color:%7;border-color:%3;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::SecondaryText),
             ModernTheme::color(ModernTheme::BorderSubtle),
             ModernTheme::color(ModernTheme::AccentCyan),
             ModernTheme::color(ModernTheme::PrimaryText),
             ModernTheme::color(ModernTheme::ElevatedPanel),
             ModernTheme::color(ModernTheme::DisabledText));
    QButtonGroup *modeGroup = new QButtonGroup(modeSelector);
    modeGroup->setExclusive(true);
    const QStringList modeNames = {
        "SINGLE", "DUAL MONO", "DUAL L/R", "DYNAMIC"
    };
    for (int raw = 0; raw < modeNames.size(); ++raw) {
        QPushButton *button = new QPushButton(modeNames.at(raw));
        button->setCheckable(true);
        button->setMinimumHeight(32);
        button->setStyleSheet(modeSelectorStyle);
        modeGroup->addButton(button, raw);
        connect(button, &QPushButton::clicked,
                this, [this, raw]() { setChannelMode(raw); });
        modeSelectorLayout->addWidget(button, 1);
        channelModeButtons.append(button);
    }
    layout->addWidget(modeSelector);

    channelRoutingStack = new QStackedWidget;

    QWidget *singlePage = new QWidget;
    QVBoxLayout *singleLayout = new QVBoxLayout(singlePage);
    singleLayout->setContentsMargins(0, 0, 0, 0);
    singleLayout->setSpacing(9);
    QLabel *channelLabel = new QLabel("CHANNEL");
    channelLabel->setObjectName("ParameterLabel");
    singleLayout->addWidget(channelLabel);
    QWidget *selector = new QWidget;
    QHBoxLayout *selectorLayout = new QHBoxLayout(selector);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->setSpacing(4);
    channelAButton = new QPushButton("A");
    channelBButton = new QPushButton("B");
    channelAButton->setCheckable(true);
    channelBButton->setCheckable(true);
    channelAButton->setFixedSize(72, 32);
    channelBButton->setFixedSize(72, 32);
    const QString selectorStyle = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %3;"
        "border-radius:4px;font-weight:700;}"
        "QPushButton:hover{border-color:%4;}"
        "QPushButton:checked{background:%5;color:%6;border-color:%4;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::SecondaryText),
             ModernTheme::color(ModernTheme::BorderSubtle),
             ModernTheme::color(ModernTheme::AccentCyan),
             ModernTheme::color(ModernTheme::ElevatedPanel),
             ModernTheme::color(ModernTheme::PrimaryText));
    channelAButton->setStyleSheet(selectorStyle);
    channelBButton->setStyleSheet(selectorStyle);
    QButtonGroup *channelGroup = new QButtonGroup(selector);
    channelGroup->setExclusive(true);
    channelGroup->addButton(channelAButton, 0);
    channelGroup->addButton(channelBButton, 1);
    connect(channelAButton, &QPushButton::clicked,
            this, [this]() { setChannelSelect(0); });
    connect(channelBButton, &QPushButton::clicked,
            this, [this]() { setChannelSelect(1); });
    selectorLayout->addWidget(channelAButton);
    selectorLayout->addWidget(channelBButton);
    selectorLayout->addStretch(1);
    singleLayout->addWidget(selector);
    singleLayout->addStretch(1);
    channelRoutingStack->addWidget(singlePage);

    QWidget *dualPage = new QWidget;
    QVBoxLayout *dualLayout = new QVBoxLayout(dualPage);
    dualLayout->setContentsMargins(0, 0, 0, 0);
    dualLayout->setSpacing(9);
    channelDelay = new ParameterBar("Channel Delay");
    channelDelay->setRange(0, 100);
    channelDelay->setProperty("channelRoutingAddress", "03");
    channelDelay->setAccentColor(QColor(
        ModernTheme::color(ModernTheme::AccentCyan)));
    connect(channelDelay, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::channelRoutingBarChanged);
    dualLayout->addWidget(channelDelay);
    dualLayout->addStretch(1);
    channelRoutingStack->addWidget(dualPage);

    QWidget *dynamicPage = new QWidget;
    QVBoxLayout *dynamicLayout = new QVBoxLayout(dynamicPage);
    dynamicLayout->setContentsMargins(0, 0, 0, 0);
    dynamicLayout->setSpacing(9);
    dynamicSense = new ParameterBar("Dynamic Sense");
    dynamicSense->setRange(0, 100);
    dynamicSense->setProperty("channelRoutingAddress", "04");
    dynamicSense->setAccentColor(QColor(
        ModernTheme::color(ModernTheme::AccentCyan)));
    connect(dynamicSense, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::channelRoutingBarChanged);
    dynamicLayout->addWidget(dynamicSense);
    dynamicLayout->addStretch(1);
    channelRoutingStack->addWidget(dynamicPage);

    layout->addWidget(channelRoutingStack, 1);
    layout->addStretch(1);
    root->addWidget(diagramPane, 34);
    root->addWidget(parameterPane, 66);
    return editor;
}

QWidget *modernFloorBoard::createReverbCombo(const QString &label,
                                              const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "0A", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(reverbComboChanged(int)));
    if (address == "31") {
        reverbType = combo;
        if (reverbModelBrowser)
            reverbModelBrowser->setModels(labels);
    } else if (address == "34") reverbLowCut = combo;
    else if (address == "35") reverbHighCut = combo;
    return container;
}

QWidget *modernFloorBoard::createReverbBar(const QString &label,
                                            const QString &address,
                                            bool twoByte)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    bar->setProperty("address", address);
    bar->setProperty("twoByte", twoByte);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "0A", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "0A", "00", address),
                  midiTable->getRange(
                      "Structure", "0A", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::reverbBarChanged);
    if (address == "39")
        reverbSpringSensitivity = bar;
    reverbBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createCompCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "00", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(compTypeChanged(int)));
    compType = combo;
    if (compModelBrowser)
        compModelBrowser->setModels(labels);
    return container;
}

QWidget *modernFloorBoard::createCompBar(const QString &label,
                                          const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    bar->setProperty("address", address);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "00", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "00", "00", address),
                  midiTable->getRange(
                      "Structure", "00", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::compBarChanged);
    compBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createOddsCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "00", "00", address);
    QStringList browserLabels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        bool rawOk = false;
        const int raw = item.value.toInt(&rawOk, 16);
        browserLabels.append(address == "71" && rawOk && raw == 0x19
            ? QString("(CUSTOM) %1").arg(text) : text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(oddsComboChanged(int)));
    if (address == "71") {
        oddsType = combo;
        if (oddsModelBrowser)
            oddsModelBrowser->setModels(browserLabels);
    } else if (address == "79") {
        oddsCustomType = combo;
    }
    return container;
}

QWidget *modernFloorBoard::createOddsBar(const QString &label,
                                          const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    bar->setProperty("address", address);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "00", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "00", "00", address),
                  midiTable->getRange(
                      "Structure", "00", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::oddsBarChanged);
    oddsBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createDelayCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "0A", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(delayComboChanged(int)));
    delayCombos.append(combo);
    if (address == "01") {
        delayType = combo;
        if (delayModelBrowser)
            delayModelBrowser->setModels(labels);
    }
    return container;
}

QWidget *modernFloorBoard::createDelayBar(const QString &label,
                                          const QString &address,
                                          bool twoByte)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    bar->setProperty("address", address);
    bar->setProperty("twoByte", twoByte);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "0A", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "0A", "00", address),
                  midiTable->getRange(
                      "Structure", "0A", "00", address));
    if (twoByte) {
        const QVector<int> rhythmicRawValues =
            rhythmicDivisionRawValues(parameter);
        if (!rhythmicRawValues.isEmpty()) {
            const int firstRhythmicRaw = *std::min_element(
                rhythmicRawValues.constBegin(),
                rhythmicRawValues.constEnd());
            bar->setSegmentedMapping(
                firstRhythmicRaw - 1,
                rhythmicRawValues, 0.5);
        }
    }
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::delayBarChanged);
    delayBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createChorusCombo(const QString &label,
                                             const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "0A", "00", address);
    for (const Midi &item : parameter.level) {
        if (item.value == "range")
            continue;

        bool rawOk = false;
        const int raw = item.value.toInt(&rawOk, 16);
        if (!rawOk)
            continue;

        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text, raw);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(chorusComboChanged(int)));
    chorusCombos.append(combo);
    if (address == "21")
        chorusMode = combo;
    return container;
}

QWidget *modernFloorBoard::createChorusBar(const QString &label,
                                           const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("CHORUS")));
    bar->setProperty("address", address);

    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "0A", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "0A", "00", address),
                  midiTable->getRange(
                      "Structure", "0A", "00", address));
    if (address == "22") {
        const QVector<int> rhythmicRawValues =
            rhythmicDivisionRawValues(parameter);
        if (!rhythmicRawValues.isEmpty()) {
            const int firstRhythmicRaw = *std::min_element(
                rhythmicRawValues.constBegin(),
                rhythmicRawValues.constEnd());
            bar->setSegmentedMapping(
                firstRhythmicRaw - 1, rhythmicRawValues, 0.5);
        }
    }

    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::chorusBarChanged);
    chorusBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createEqCombo(const QString &label,
                                         const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "01", "00", address);
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(eqComboChanged(int)));
    eqCombos.append(combo);
    return container;
}

QWidget *modernFloorBoard::createEqBar(const QString &label,
                                       const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("EQ")));
    bar->setProperty("address", address);
    MidiTable *midiTable = MidiTable::Instance();
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "01", "00", address),
                  midiTable->getRange(
                      "Structure", "01", "00", address));
    bar->setCenterValue(0x14);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::eqBarChanged);
    eqBars.append(bar);
    return bar;
}

EffectModule *modernFloorBoard::createEffectBlock(const QString &name,
                                                   bool available)
{
    const EffectModule::VisualKind kind = name == "EQ"
        ? EffectModule::Equalizer : EffectModule::DualKnob;
    return new EffectModule(name, ModernTheme::effectColor(name), available, kind);
}


bool modernFloorBoard::hasValidReverbBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0A00");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x3B;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidCompBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0000");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x47;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidOddsBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0000");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x7D;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidDelayBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0A00");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x18;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidChorusParameter(
    const QString &address) const
{
    bool addressOk = false;
    const int offset = address.toInt(&addressOk, 16);
    if (!addressOk)
        return false;

    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0A00");
    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0
        || addressIndex >= source.hex.size())
        return false;

    const int valueIndex = sysxDataOffset + offset;
    return valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidChorusBuffer() const
{
    return hasValidChorusParameter("20")
        && hasValidChorusParameter("21");
}

bool modernFloorBoard::hasValidEqBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0100");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x7B;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidPreampBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0100");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x4C;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

void modernFloorBoard::setReverbUnavailable()
{
    if (reverbCard)
        reverbCard->setEffectState(false, false);
    updateReverbParameterControls(false);
}

void modernFloorBoard::setCompUnavailable()
{
    if (compCard)
        compCard->setEffectState(false, false);
    updateCompParameterControls(false);
}

void modernFloorBoard::setOddsUnavailable()
{
    if (oddsCard)
        oddsCard->setEffectState(false, false);
    updateOddsParameterControls(false);
}

void modernFloorBoard::setDelayUnavailable()
{
    if (delayCard)
        delayCard->setEffectState(false, false);
    updateDelayParameterControls(false);
}

void modernFloorBoard::setChorusUnavailable()
{
    if (chorusCard)
        chorusCard->setEffectState(false, false);
    updateChorusParameterControls(false);
}

void modernFloorBoard::setEqUnavailable()
{
    if (eqCard)
        eqCard->setEffectState(false, false);
    updateEqParameterControls(false);
}

void modernFloorBoard::setPreampUnavailable()
{
    if (preampACard)
        preampACard->setEffectState(false, false);
    if (preampBCard)
        preampBCard->setEffectState(false, false);
    updatePreampParameterControls(PreampChannel::A, false);
    updatePreampParameterControls(PreampChannel::B, false);
}

bool modernFloorBoard::hasSourceValue(const QString &area,
                                      const QString &hex1,
                                      const QString &hex2,
                                      const QString &hex3) const
{
    bool offsetOk = false;
    const int offset = hex3.toInt(&offsetOk, 16);
    if (!offsetOk)
        return false;

    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = area == "System"
        ? sysxIO->getSystemSource()
        : sysxIO->getFileSource();
    const int blockIndex = source.address.indexOf(hex1 + hex2);
    if (blockIndex < 0 || blockIndex >= source.hex.size())
        return false;

    return source.hex.at(blockIndex).size() > sysxDataOffset + offset;
}

void modernFloorBoard::refreshOutputSelectHeader()
{
    if (!outputSelectCombo)
        return;

    const QSignalBlocker blocker(outputSelectCombo);
    outputSelectCombo->setEnabled(false);
    outputSelectCombo->setCurrentIndex(0);
    outputSelectCombo->setToolTip(QString());

    SysxIO *sysxIO = SysxIO::Instance();
    if (!backendIsConnected || !sysxIO->isConnected()
            || !outputSystemDataReady
            || !hasSourceValue("System", "00", "00", "4E"))
        return;

    const int outputMode = sysxIO->getSourceValue(
        "System", "00", "00", "4E");
    const bool patchScope = outputMode == 0;
    const bool systemScope = outputMode == 1;
    if (!patchScope && !systemScope)
        return;

    const QString area = patchScope ? "Structure" : "System";
    const QString address = patchScope ? "11" : "4F";
    if (!hasSourceValue(area, "00", "00", address))
        return;

    const int raw = sysxIO->getSourceValue(area, "00", "00", address);
    const int comboIndex = outputSelectCombo->findData(raw);
    if (comboIndex < 1)
        return;

    outputSelectCombo->setCurrentIndex(comboIndex);
    outputSelectCombo->setToolTip(QString("%1\n%2")
        .arg(patchScope ? "Patch Output Select" : "System Output Select",
             outputSelectCombo->currentText()));
    outputSelectCombo->setEnabled(true);
}

void modernFloorBoard::outputSelectChanged(int index)
{
    if (!outputSelectCombo || index < 1 || !outputSelectCombo->isEnabled())
        return;

    bool rawOk = false;
    const int raw = outputSelectCombo->itemData(index).toInt(&rawOk);
    SysxIO *sysxIO = SysxIO::Instance();
    if (!rawOk || !backendIsConnected || !sysxIO->isConnected()
            || !hasSourceValue("System", "00", "00", "4E"))
        return;

    const int outputMode = sysxIO->getSourceValue(
        "System", "00", "00", "4E");
    const QString rawHex = QString("%1").arg(raw, 2, 16, QChar('0')).toUpper();
    if (outputMode == 0
            && hasSourceValue("Structure", "00", "00", "11")) {
        sysxIO->setFileSource("Structure", "00", "00", "11", rawHex);
    } else if (outputMode == 1
               && hasSourceValue("System", "00", "00", "4F")) {
        sysxIO->setFileSource("System", "00", "00", "4F", rawHex);
    } else {
        refreshOutputSelectHeader();
    }
}

void modernFloorBoard::refreshTunerSettings()
{
    if (!tunerReferenceCombo || !tunerOutputCombo)
        return;

    const QSignalBlocker referenceBlocker(tunerReferenceCombo);
    const QSignalBlocker outputBlocker(tunerOutputCombo);
    tunerReferenceCombo->setEnabled(false);
    tunerOutputCombo->setEnabled(false);
    tunerReferenceCombo->setCurrentIndex(-1);
    tunerOutputCombo->setCurrentIndex(-1);

    SysxIO *sysxIO = SysxIO::Instance();
    if (!backendIsConnected || !sysxIO->isConnected()
            || !tunerSystemDataReady
            || !hasSourceValue("System", "00", "00", "30")
            || !hasSourceValue("System", "00", "00", "31"))
        return;

    const int referenceRaw = sysxIO->getSourceValue(
        "System", "00", "00", "30");
    const int outputRaw = sysxIO->getSourceValue(
        "System", "00", "00", "31");
    const int referenceIndex = tunerReferenceCombo->findData(referenceRaw);
    const int outputIndex = tunerOutputCombo->findData(outputRaw);
    if (referenceIndex < 0 || outputIndex < 0)
        return;

    tunerReferenceCombo->setCurrentIndex(referenceIndex);
    tunerOutputCombo->setCurrentIndex(outputIndex);
    tunerReferenceCombo->setEnabled(true);
    tunerOutputCombo->setEnabled(true);
}

void modernFloorBoard::tunerReferenceChanged(int index)
{
    if (!tunerReferenceCombo || index < 0
            || !tunerReferenceCombo->isEnabled())
        return;

    bool rawOk = false;
    const int raw = tunerReferenceCombo->itemData(index).toInt(&rawOk);
    SysxIO *sysxIO = SysxIO::Instance();
    if (!rawOk || raw < 0x00 || raw > 0x0A
            || !backendIsConnected || !sysxIO->isConnected()
            || !tunerSystemDataReady
            || !hasSourceValue("System", "00", "00", "30")) {
        refreshTunerSettings();
        return;
    }

    const QString rawHex = QString("%1").arg(
        raw, 2, 16, QChar('0')).toUpper();
    sysxIO->setFileSource("System", "00", "00", "30", rawHex);
}

void modernFloorBoard::tunerOutputChanged(int index)
{
    if (!tunerOutputCombo || index < 0 || !tunerOutputCombo->isEnabled())
        return;

    bool rawOk = false;
    const int raw = tunerOutputCombo->itemData(index).toInt(&rawOk);
    SysxIO *sysxIO = SysxIO::Instance();
    if (!rawOk || raw < 0x00 || raw > 0x01
            || !backendIsConnected || !sysxIO->isConnected()
            || !tunerSystemDataReady
            || !hasSourceValue("System", "00", "00", "31")) {
        refreshTunerSettings();
        return;
    }

    const QString rawHex = QString("%1").arg(
        raw, 2, 16, QChar('0')).toUpper();
    sysxIO->setFileSource("System", "00", "00", "31", rawHex);
}

void modernFloorBoard::requestOutputSystemData()
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (outputSystemDataRequested || !backendIsConnected
            || !sysxIO->isConnected())
        return;

    if (!backendHasPatchData || !sysxIO->deviceReady()) {
        QTimer::singleShot(200, this,
                           &modernFloorBoard::requestOutputSystemData);
        return;
    }

    outputSystemDataRequested = true;
    outputSystemDataReady = false;
    tunerSystemDataReady = false;
    sysxIO->systemDataRequest();
    QTimer::singleShot(200, this, &modernFloorBoard::pollOutputSystemData);
}

void modernFloorBoard::pollOutputSystemData()
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (!backendIsConnected || !sysxIO->isConnected())
        return;

    if (sysxIO->deviceReady()) {
        outputSystemDataReady =
            hasSourceValue("System", "00", "00", "4E")
            && hasSourceValue("System", "00", "00", "4F");
        tunerSystemDataReady =
            hasSourceValue("System", "00", "00", "30")
            && hasSourceValue("System", "00", "00", "31");
        refreshOutputSelectHeader();
        refreshTunerSettings();
        return;
    }

    QTimer::singleShot(200, this, &modernFloorBoard::pollOutputSystemData);
}

void modernFloorBoard::backendConnected()
{
    backendIsConnected = true;
    backendHasPatchData = false;
    outputSystemDataRequested = false;
    outputSystemDataReady = false;
    tunerSystemDataReady = false;
    emit connectionStateChanged(true);
    refreshOutputSelectHeader();
    refreshTunerSettings();
    requestOutputSystemData();
    setReverbUnavailable();
    setCompUnavailable();
    setOddsUnavailable();
    setDelayUnavailable();
    setChorusUnavailable();
    setEqUnavailable();
    setPreampUnavailable();
    updateChannelRoutingControls(false);
    refreshFx(FxSlot::FX1);
    refreshFx(FxSlot::FX2);
    refreshPedalFx();
    refreshNoiseSuppressors();
    refreshSendReturn();
}

void modernFloorBoard::backendDisconnected()
{
    backendIsConnected = false;
    backendHasPatchData = false;
    outputSystemDataRequested = false;
    outputSystemDataReady = false;
    tunerSystemDataReady = false;
    emit connectionStateChanged(false);
    refreshOutputSelectHeader();
    refreshTunerSettings();
    signalChainModel.clear();
    rebuildSignalChainView();
    setReverbUnavailable();
    setCompUnavailable();
    setOddsUnavailable();
    setDelayUnavailable();
    setChorusUnavailable();
    setEqUnavailable();
    setPreampUnavailable();
    updateChannelRoutingControls(false);
    refreshFx(FxSlot::FX1);
    refreshFx(FxSlot::FX2);
    refreshPedalFx();
    refreshNoiseSuppressors();
    refreshSendReturn();
    patchNumber->setText(QString::fromUtf8("—"));
    patchName->setText("NO PATCH DATA");
    patchListModel.setCurrentPatch(0, 0, QString());
}

void modernFloorBoard::patchNameResolved(int bank, int patch, QString name)
{
    patchListModel.setPatchName(bank, patch, name);
}

void modernFloorBoard::refreshReverbState()
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (backendIsConnected && sysxIO->isConnected() && sysxIO->isDevice())
        backendHasPatchData = true;

    if (backendHasPatchData) {
        const int bank = sysxIO->getLoadedBank();
        const int patch = sysxIO->getLoadedPatch();
        const QString name = sysxIO->getCurrentPatchName().trimmed();
        patchNumber->setText(patchListModel.patchNumber(bank, patch));
        patchName->setText(name.isEmpty() ? QString::fromUtf8("—") : name);
        patchListModel.setCurrentPatch(bank, patch, name);
    }

    requestOutputSystemData();
    refreshOutputSelectHeader();
    refreshTunerSettings();

    refreshSignalChainModel();

    if (!hasValidReverbBuffer()) {
        setReverbUnavailable();
        refreshCompState();
        refreshOddsState();
        refreshDelayState();
        refreshChorus();
        refreshEq();
        refreshPreamp(PreampChannel::A);
        refreshPreamp(PreampChannel::B);
        refreshPreampGlobalState();
        refreshChannelRouting();
        refreshFx(FxSlot::FX1);
        refreshFx(FxSlot::FX2);
        refreshPedalFx();
        refreshNoiseSuppressors();
        refreshSendReturn();
        return;
    }

    const int value = sysxIO->getSourceValue(
        "Structure", "0A", "00", "30"
    );
    const bool on = (value == 1);

    reverbCard->setEffectState(true, on);
    updateReverbParameterControls(true);
    refreshCompState();
    refreshOddsState();
    refreshDelayState();
    refreshChorus();
    refreshEq();
    refreshPreamp(PreampChannel::A);
    refreshPreamp(PreampChannel::B);
    refreshPreampGlobalState();
    refreshChannelRouting();
    refreshFx(FxSlot::FX1);
    refreshFx(FxSlot::FX2);
    refreshPedalFx();
    refreshNoiseSuppressors();
    refreshSendReturn();
}

void modernFloorBoard::refreshCompState()
{
    if (!hasValidCompBuffer()) {
        setCompUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", "40") == 1;
    if (compCard)
        compCard->setEffectState(true, on);
    updateCompParameterControls(true);
}

void modernFloorBoard::refreshOddsState()
{
    if (!hasValidOddsBuffer()) {
        setOddsUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", "70") == 1;
    if (oddsCard)
        oddsCard->setEffectState(true, on);
    updateOddsParameterControls(true);
}

void modernFloorBoard::refreshDelayState()
{
    if (!hasValidDelayBuffer()) {
        setDelayUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", "00") == 1;
    if (delayCard)
        delayCard->setEffectState(true, on);
    updateDelayParameterControls(true);
}

void modernFloorBoard::refreshChorus()
{
    if (!hasValidChorusBuffer()) {
        setChorusUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", "20") == 1;
    if (chorusCard)
        chorusCard->setEffectState(true, on);
    updateChorusParameterControls(true);
}

void modernFloorBoard::refreshEq()
{
    if (!hasValidEqBuffer()) {
        setEqUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "01", "00", "70") == 1;
    if (eqCard)
        eqCard->setEffectState(true, on);
    updateEqParameterControls(true);
}

void modernFloorBoard::refreshSignalChainModel()
{
    if (signalChainTransactionActive)
        return;

    const bool wasValid = signalChainModel.isValid();
    const modernSignalChainModel::ChainSnapshot previousSnapshot =
        signalChainModel.snapshot();

    const auto structuralSignature = [](
        const modernSignalChainModel::ChainSnapshot &snapshot) {
        QStringList signature;
        const auto appendRegion = [&signature](
            const QList<modernSignalChainModel::Entry> &entries,
            const QString &separator) {
            signature.append(separator);
            for (const modernSignalChainModel::Entry &entry : entries)
                signature.append(entry.rawValue.toUpper());
        };
        appendRegion(snapshot.commonPrefix, "PREFIX");
        signature.append("SPLIT:" + snapshot.split.rawValue.toUpper());
        appendRegion(snapshot.pathA, "PATH-A");
        appendRegion(snapshot.pathB, "PATH-B");
        signature.append("MERGE:" + snapshot.merge.rawValue.toUpper());
        appendRegion(snapshot.commonSuffix, "SUFFIX");
        return signature;
    };

    if (!backendIsConnected || !backendHasPatchData) {
        signalChainModel.clear();
        if (wasValid || !signalChainContent)
            rebuildSignalChainView();
        return;
    }

    signalChainModel.refreshFromLegacyBackend();
    signalChainModel.logInterpretedChain();
    const bool isValid = signalChainModel.isValid();
    const bool structureChanged = wasValid != isValid
        || (wasValid && isValid
            && structuralSignature(previousSnapshot)
                != structuralSignature(signalChainModel.snapshot()));
    if (structureChanged || !signalChainContent)
        rebuildSignalChainView();
}

SignalChainModule *modernFloorBoard::createSignalChainModule(
    const modernSignalChainModel::Entry &entry)
{
    const QString fullName = modernSignalChainModel::displayName(entry);
    QString name = fullName;
    if (name == "SEND/RETURN") name = "S/R";
    else if (name == "FOOT VOLUME") name = "FV";
    else if (name == "PEDAL FX") name = "P.FX";
    else if (name == "DIGITAL OUT") name = "D.OUT";
    else if (name == "NS-1") name = "NS1";
    else if (name == "NS-2") name = "NS2";
    const bool isReverb = entry.moduleId == 0x09;
    const bool isComp = entry.moduleId == 0x00;
    const bool isOdds = entry.moduleId == 0x01;
    const bool isDelay = entry.moduleId == 0x07;
    const bool isChorus = entry.moduleId == 0x08;
    const bool isEq = entry.moduleId == 0x04;
    const bool isFx1 = entry.moduleId == 0x05;
    const bool isFx2 = entry.moduleId == 0x06;
    const bool isPreampA = entry.moduleId == 0x02;
    const bool isPreampB = entry.moduleId == 0x03;
    const bool isPedalFx = entry.moduleId == 0x0A;
    const bool isFootVolume = entry.moduleId == 0x0B;
    const bool isNs1 = entry.moduleId == 0x0C;
    const bool isNs2 = entry.moduleId == 0x0D;
    const bool isSendReturn = entry.moduleId == 0x0E;
    const bool isDigitalOut = entry.moduleId == 0x0F;
    SignalChainModule *module = new SignalChainModule(
        name, QColor(ModernTheme::effectColor(fullName)),
        QColor(ModernTheme::effectFaceColor(fullName)));
    module->setEffectState(false, false);
    module->setStructural(isDigitalOut);
    module->setNavigable(isComp || isReverb || isOdds || isDelay || isChorus
                         || isEq || isFx1 || isFx2
                         || isPreampA || isPreampB
                         || isPedalFx || isFootVolume
                         || isNs1 || isNs2 || isSendReturn);
    module->setMovable(entry.movable && !signalChainTransactionActive,
                       entry.moduleId);
    module->setProperty("chainPosition", entry.originalPosition);
    module->setProperty("rawValue", entry.rawValue);
    module->setProperty("signalPath", entry.path);
    module->setProperty("moduleId", entry.moduleId);
    module->setProperty("chainRegion", int(entry.region));
    signalChainModules.append(module);

    if (isReverb) {
        reverbCard = module;
        reverbCard->setSelected(selectedEditor == "REVERB");
        connect(module, SIGNAL(clicked()), this, SLOT(showReverbEditor()));
    }
    if (isComp) {
        compCard = module;
        compCard->setSelected(selectedEditor == "COMP");
        connect(module, SIGNAL(clicked()), this, SLOT(showCompEditor()));
    }
    if (isOdds) {
        oddsCard = module;
        oddsCard->setSelected(selectedEditor == "OD/DS");
        connect(module, SIGNAL(clicked()), this, SLOT(showOddsEditor()));
    }
    if (isDelay) {
        delayCard = module;
        delayCard->setSelected(selectedEditor == "DELAY");
        connect(module, SIGNAL(clicked()), this, SLOT(showDelayEditor()));
    }
    if (isChorus) {
        chorusCard = module;
        chorusCard->setSelected(selectedEditor == "CHORUS");
        connect(module, SIGNAL(clicked()), this, SLOT(showChorusEditor()));
    }
    if (isEq) {
        eqCard = module;
        eqCard->setSelected(selectedEditor == "EQ");
        connect(module, SIGNAL(clicked()), this, SLOT(showEqEditor()));
    }
    if (isFx1) {
        fx1Card = module;
        fx1Card->setSelected(selectedEditor == "FX-1");
        connect(module, SIGNAL(clicked()), this, SLOT(showFx1Editor()));
    }
    if (isFx2) {
        fx2Card = module;
        fx2Card->setSelected(selectedEditor == "FX-2");
        connect(module, SIGNAL(clicked()), this, SLOT(showFx2Editor()));
    }
    if (isPreampA) {
        preampACard = module;
        preampACard->setSelected(selectedEditor == "PREAMP A");
        connect(module, SIGNAL(clicked()),
                this, SLOT(showPreampAEditor()));
    }
    if (isPreampB) {
        preampBCard = module;
        preampBCard->setSelected(selectedEditor == "PREAMP B");
        connect(module, SIGNAL(clicked()),
                this, SLOT(showPreampBEditor()));
    }
    if (isPedalFx) {
        pedalFxCard = module;
        pedalFxCard->setSelected(selectedEditor == "P.FX");
        connect(module, SIGNAL(clicked()),
                this, SLOT(showPedalFxEditor()));
    }
    if (isFootVolume) {
        footVolumeCard = module;
        footVolumeCard->setSelected(selectedEditor == "FV");
        connect(module, SIGNAL(clicked()),
                this, SLOT(showFootVolumeEditor()));
    }
    if (isNs1) {
        ns1Card = module;
        ns1Card->setSelected(selectedEditor == "NS-1");
        connect(module, SIGNAL(clicked()), this, SLOT(showNs1Editor()));
    }
    if (isNs2) {
        ns2Card = module;
        ns2Card->setSelected(selectedEditor == "NS-2");
        connect(module, SIGNAL(clicked()), this, SLOT(showNs2Editor()));
    }
    if (isSendReturn) {
        sendReturnCard = module;
        sendReturnCard->setSelected(selectedEditor == "SEND/RETURN");
        connect(module, SIGNAL(clicked()),
                this, SLOT(showSendReturnEditor()));
    }
    return module;
}

void modernFloorBoard::rebuildSignalChainView()
{
    reverbCard = nullptr;
    compCard = nullptr;
    delayCard = nullptr;
    chorusCard = nullptr;
    eqCard = nullptr;
    fx1Card = nullptr;
    fx2Card = nullptr;
    preampACard = nullptr;
    preampBCard = nullptr;
    pedalFxCard = nullptr;
    footVolumeCard = nullptr;
    ns1Card = nullptr;
    ns2Card = nullptr;
    sendReturnCard = nullptr;
    splitJunction = nullptr;
    signalChainModules.clear();
    signalChainJunctions.clear();
    signalChainConnectors.clear();
    signalFlowLayout = nullptr;
    signalPathsLayout = nullptr;
    signalParallelPaths = nullptr;
    signalChainContent = nullptr;

    SignalChainContent *content = new SignalChainContent;
    signalChainContent = content;
    content->setDragHandler([this](int moduleId, const QPoint &position,
                                   bool commit) {
        return handleSignalChainDrag(moduleId, position, commit);
    });
    content->setDragLeaveHandler([this]() {
        clearSignalChainDragFeedback();
    });
    content->setStyleSheet("QWidget#SignalChainContent{background:transparent;}");
    signalFlowLayout = new QHBoxLayout(content);
    signalFlowLayout->setContentsMargins(4, 0, 4, 0);
    signalFlowLayout->setSpacing(5);
    SignalConnector *inputConnector = new SignalConnector(SignalConnector::Input);
    signalChainConnectors.append(inputConnector);
    signalFlowLayout->addWidget(inputConnector, 0, Qt::AlignVCenter);

    if (!signalChainModel.isValid()) {
        QLabel *placeholder = new QLabel("Load a GT-10 patch to display its real signal chain");
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QString(
            "color:%1;font-size:12px;padding:30px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalFlowLayout->addWidget(placeholder, 1);
        SignalConnector *outputConnector = new SignalConnector(SignalConnector::Output);
        signalChainConnectors.append(outputConnector);
        signalFlowLayout->addWidget(outputConnector, 0, Qt::AlignVCenter);
        signalChainScroll->setUpdatesEnabled(false);
        signalChainScroll->setWidget(content);
        signalChainScroll->setUpdatesEnabled(true);
        signalChainScroll->viewport()->update();
        return;
    }

    int regionIndex = 0;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.commonPrefix()) {
        SignalChainModule *module = createSignalChainModule(entry);
        module->setProperty("regionIndex", regionIndex++);
        signalFlowLayout->addWidget(module, 0, Qt::AlignVCenter);
    }

    splitJunction = new SignalJunction(SignalJunction::Split);
    splitJunction->setSelected(selectedEditor == "CHANNEL ROUTING");
    connect(splitJunction, SIGNAL(clicked()),
            this, SLOT(showChannelRoutingEditor()));
    signalChainJunctions.append(splitJunction);
    signalFlowLayout->addWidget(splitJunction, 0, Qt::AlignVCenter);

    QWidget *parallelPaths = new QWidget;
    signalParallelPaths = parallelPaths;
    parallelPaths->setObjectName("ParallelPaths");
    // The parallel container is layout-only. A decorative frame here merges
    // visually with the two signal cables and reads as a third rectangular
    // route, so topology is expressed exclusively by SPLIT/Path A/Path B/MERGE.
    parallelPaths->setStyleSheet(
        "QWidget#ParallelPaths{background:transparent;border:none;}");
    signalPathsLayout = new QGridLayout(parallelPaths);
    signalPathsLayout->setContentsMargins(5, 5, 5, 5);
    signalPathsLayout->setHorizontalSpacing(5);
    signalPathsLayout->setVerticalSpacing(4);
    int column = 0;
    regionIndex = 0;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.pathA()) {
        SignalChainModule *module = createSignalChainModule(entry);
        module->setProperty("regionIndex", regionIndex++);
        signalPathsLayout->addWidget(module, 0, column++, Qt::AlignCenter);
    }
    if (column == 0) {
        QLabel *empty = new QLabel("EMPTY PATH");
        empty->setStyleSheet(QString(
            "color:%1;font-size:9px;padding:20px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalPathsLayout->addWidget(empty, 0, column);
    }

    column = 0;
    regionIndex = 0;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.pathB()) {
        SignalChainModule *module = createSignalChainModule(entry);
        module->setProperty("regionIndex", regionIndex++);
        signalPathsLayout->addWidget(module, 1, column++, Qt::AlignCenter);
    }
    if (column == 0) {
        QLabel *empty = new QLabel("EMPTY PATH");
        empty->setStyleSheet(QString(
            "color:%1;font-size:9px;padding:20px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalPathsLayout->addWidget(empty, 1, column);
    }
    signalFlowLayout->addWidget(parallelPaths, 0, Qt::AlignVCenter);
    SignalJunction *merge = new SignalJunction(SignalJunction::Merge);
    signalChainJunctions.append(merge);
    signalFlowLayout->addWidget(merge, 0, Qt::AlignVCenter);

    regionIndex = 0;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.commonSuffix()) {
        SignalChainModule *module = createSignalChainModule(entry);
        module->setProperty("regionIndex", regionIndex++);
        signalFlowLayout->addWidget(module, 0, Qt::AlignVCenter);
    }
    SignalConnector *outputConnector = new SignalConnector(SignalConnector::Output);
    signalChainConnectors.append(outputConnector);
    signalFlowLayout->addWidget(outputConnector, 0, Qt::AlignVCenter);

    // The 18 structure bytes carry topology only. Recreated cards must read
    // their presentation state from the existing parameter backends before
    // the new view becomes visible.
    refreshSignalChainPresentation();

    // Installing a freshly-created chain at its constructor defaults (96x78)
    // and compacting it on the next event-loop pass exposed an intermediate
    // geometry for one or more frames. Keep the scroll area frozen until the
    // responsive geometry has been applied synchronously.
    signalChainScroll->setUpdatesEnabled(false);
    signalChainScroll->setWidget(content);
    applyResponsiveSignalChainLayout();
    signalChainScroll->setUpdatesEnabled(true);
    signalChainScroll->viewport()->update();
}

void modernFloorBoard::refreshSignalChainPresentation()
{
    if (!backendIsConnected || !backendHasPatchData)
        return;

    if (reverbCard) {
        if (hasValidReverbBuffer()) {
            const bool on = SysxIO::Instance()->getSourceValue(
                "Structure", "0A", "00", "30") == 1;
            reverbCard->setEffectState(true, on);
        } else {
            reverbCard->setEffectState(false, false);
        }
    }
    refreshCompState();
    refreshOddsState();
    refreshDelayState();
    refreshChorus();
    refreshEq();
    refreshPreampGlobalState();
    refreshFx(FxSlot::FX1);
    refreshFx(FxSlot::FX2);
    refreshPedalFx();
    refreshNoiseSuppressors();
    refreshSendReturn();
}

modernSignalChainModel::ChainDestination
modernFloorBoard::resolveSignalChainDestination(
    const QPoint &contentPosition) const
{
    using Model = modernSignalChainModel;
    Model::ChainDestination destination;
    if (!signalChainContent || !signalChainModel.isValid()
        || !signalChainContent->rect().contains(contentPosition))
        return destination;

    for (SignalChainModule *module : signalChainModules) {
        const QRect moduleRect(module->mapTo(signalChainContent, QPoint(0, 0)),
                               module->size());
        if (!moduleRect.contains(contentPosition))
            continue;
        destination.region = static_cast<Model::ChainRegion>(
            module->property("chainRegion").toInt());
        destination.index = module->property("regionIndex").toInt()
            + (contentPosition.x() >= moduleRect.center().x() ? 1 : 0);
        destination.valid = true;
        return destination;
    }

    const int splitX = signalChainJunctions.isEmpty() ? 0
        : signalChainJunctions.first()->mapTo(
              signalChainContent, signalChainJunctions.first()->rect().center()).x();
    const int mergeX = signalChainJunctions.size() < 2 ? signalChainContent->width()
        : signalChainJunctions.at(1)->mapTo(
              signalChainContent, signalChainJunctions.at(1)->rect().center()).x();

    if (contentPosition.x() < splitX) {
        destination.region = Model::ChainRegion::CommonPrefix;
    } else if (contentPosition.x() > mergeX) {
        destination.region = Model::ChainRegion::CommonSuffix;
    } else {
        int centerY = signalChainContent->height() / 2;
        if (signalParallelPaths) {
            const QRect paths(signalParallelPaths->mapTo(signalChainContent,
                                                           QPoint(0, 0)),
                              signalParallelPaths->size());
            centerY = paths.center().y();
        }
        destination.region = contentPosition.y() < centerY
            ? Model::ChainRegion::PathA : Model::ChainRegion::PathB;
    }

    QList<SignalChainModule *> regionModules;
    for (SignalChainModule *module : signalChainModules) {
        if (module->property("chainRegion").toInt() == int(destination.region))
            regionModules.append(module);
    }
    std::sort(regionModules.begin(), regionModules.end(),
              [this](SignalChainModule *left, SignalChainModule *right) {
        return left->mapTo(signalChainContent, left->rect().center()).x()
            < right->mapTo(signalChainContent, right->rect().center()).x();
    });
    destination.index = 0;
    for (SignalChainModule *module : regionModules) {
        const int centerX = module->mapTo(
            signalChainContent, module->rect().center()).x();
        if (contentPosition.x() >= centerX)
            ++destination.index;
    }
    destination.valid = true;
    return destination;
}

bool modernFloorBoard::handleSignalChainDrag(
    int moduleId, const QPoint &contentPosition, bool commit)
{
    using Model = modernSignalChainModel;
    if (signalChainTransactionActive || !Model::isMovableModule(moduleId)
        || !signalChainContent)
        return false;

    const Model::ChainDestination destination =
        resolveSignalChainDestination(contentPosition);
    if (!destination.valid)
        return false;

    Model previewModel;
    QString previewError;
    if (!previewModel.replaceSnapshot(signalChainModel.snapshot(), &previewError))
        return false;
    modernSignalChainMutationController previewController(&previewModel);
    const ChainMoveResult preview = previewController.moveModule(
        moduleId, destination.region, destination.index);
    QList<QString> beforeBytes;
    modernSignalChainSerializer::serialize(preview.before, &beforeBytes, nullptr);
    const bool accepted = preview.accepted
        && preview.serializedBytes != beforeBytes;

    QRect regionRect;
    if (destination.region == Model::ChainRegion::PathA
        || destination.region == Model::ChainRegion::PathB) {
        regionRect = signalParallelPaths
            ? QRect(signalParallelPaths->mapTo(signalChainContent, QPoint(0, 0)),
                    signalParallelPaths->size()) : signalChainContent->rect();
        if (destination.region == Model::ChainRegion::PathA)
            regionRect.setBottom(regionRect.center().y());
        else
            regionRect.setTop(regionRect.center().y() + 1);
    } else {
        const int splitX = signalChainJunctions.isEmpty() ? 0
            : signalChainJunctions.first()->mapTo(
                  signalChainContent,
                  signalChainJunctions.first()->rect().center()).x();
        const int mergeX = signalChainJunctions.size() < 2
            ? signalChainContent->width()
            : signalChainJunctions.at(1)->mapTo(
                  signalChainContent,
                  signalChainJunctions.at(1)->rect().center()).x();
        regionRect = destination.region == Model::ChainRegion::CommonPrefix
            ? QRect(0, 0, splitX, signalChainContent->height())
            : QRect(mergeX, 0, signalChainContent->width() - mergeX,
                    signalChainContent->height());
    }

    QList<SignalChainModule *> regionModules;
    for (SignalChainModule *module : signalChainModules) {
        if (module->property("chainRegion").toInt() == int(destination.region))
            regionModules.append(module);
    }
    std::sort(regionModules.begin(), regionModules.end(),
              [this](SignalChainModule *left, SignalChainModule *right) {
        return left->mapTo(signalChainContent, left->rect().center()).x()
            < right->mapTo(signalChainContent, right->rect().center()).x();
    });
    int lineX = regionRect.center().x();
    if (!regionModules.isEmpty()) {
        if (destination.index <= 0) {
            lineX = regionModules.first()->mapTo(signalChainContent,
                                                  QPoint(0, 0)).x() - 3;
        } else if (destination.index >= regionModules.size()) {
            SignalChainModule *last = regionModules.last();
            lineX = last->mapTo(signalChainContent,
                                QPoint(last->width(), 0)).x() + 3;
        } else {
            SignalChainModule *left = regionModules.at(destination.index - 1);
            SignalChainModule *right = regionModules.at(destination.index);
            const int leftEdge = left->mapTo(signalChainContent,
                                              QPoint(left->width(), 0)).x();
            const int rightEdge = right->mapTo(signalChainContent,
                                                QPoint(0, 0)).x();
            lineX = (leftEdge + rightEdge) / 2;
        }
    }
    const int lineHalfHeight = destination.region == Model::ChainRegion::PathA
        || destination.region == Model::ChainRegion::PathB ? 33 : 39;
    const int lineCenterY = regionRect.center().y();
    signalChainContent->setDragFeedback(
        regionRect, QLineF(lineX, lineCenterY - lineHalfHeight,
                           lineX, lineCenterY + lineHalfHeight), accepted);

    const QPoint viewportPosition = signalChainContent->mapTo(
        signalChainScroll->viewport(), contentPosition);
    signalChainAutoScrollDirection = viewportPosition.x() < 36 ? -1
        : viewportPosition.x() > signalChainScroll->viewport()->width() - 36
            ? 1 : 0;
    if (signalChainAutoScrollDirection != 0)
        signalChainAutoScrollTimer->start();
    else
        signalChainAutoScrollTimer->stop();

    if (!commit || !accepted)
        return accepted;

    // Defer mutation/rebuild until QDropEvent has returned. Rebuilding here
    // would delete the SignalChainContent that is still dispatching the drop.
    QTimer::singleShot(0, this, [this, moduleId, destination]() {
        if (signalChainTransactionActive || !signalChainModel.isValid())
            return;
        modernSignalChainMutationController controller(&signalChainModel);
        const ChainMoveResult move = controller.moveModule(
            moduleId, destination.region, destination.index);
        if (!move.accepted)
            return;
        QList<QString> actualBefore;
        modernSignalChainSerializer::serialize(move.before, &actualBefore,
                                               nullptr);
        if (move.serializedBytes == actualBefore) {
            signalChainModel.replaceSnapshot(move.before, nullptr);
            return;
        }
        signalChainTransactionActive = true;
        pendingSignalChainModuleId = moduleId;
        rebuildSignalChainView();
        QTimer::singleShot(0, this, [this, moduleId, move]() {
            performSignalChainTransaction(moduleId, move.before, move.after,
                                          move.serializedBytes);
        });
    });
    return true;
}

void modernFloorBoard::clearSignalChainDragFeedback()
{
    signalChainAutoScrollDirection = 0;
    if (signalChainAutoScrollTimer)
        signalChainAutoScrollTimer->stop();
    if (signalChainContent)
        signalChainContent->clearDragFeedback();
}

void modernFloorBoard::syncSignalChainCache(const QList<QString> &bytes)
{
    if (bytes.size() != 18)
        return;
    SysxIO *io = SysxIO::Instance();
    SysxData source = io->getFileSource();
    const int addressIndex = source.address.indexOf("0B00");
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return;
    QList<QString> block = source.hex.at(addressIndex);
    if (block.size() < sysxDataOffset + bytes.size())
        return;
    for (int i = 0; i < bytes.size(); ++i)
        block[sysxDataOffset + i] = bytes.at(i);
    source.hex[addressIndex] = block;
    io->setFileSource("Structure", source);
}

void modernFloorBoard::performSignalChainTransaction(
    int moduleId, const modernSignalChainModel::ChainSnapshot &before,
    const modernSignalChainModel::ChainSnapshot &after,
    const QList<QString> &serializedBytes)
{
    QList<QString> beforeBytes;
    modernSignalChainSerializer::serialize(before, &beforeBytes, nullptr);
    const QString currentPatchIdentity = QString("%1:%2")
        .arg(SysxIO::Instance()->getLoadedBank())
        .arg(SysxIO::Instance()->getLoadedPatch());
    if (before.patchIdentity.contains(':')
        && currentPatchIdentity != before.patchIdentity) {
        signalChainTransactionActive = false;
        pendingSignalChainModuleId = -1;
        qWarning().noquote()
            << "Signal Chain move cancelled before write: patch changed from"
            << before.patchIdentity << "to" << currentPatchIdentity;
        refreshSignalChainModel();
        return;
    }
    SignalChainHardwareValidation hardware;
    ChainLiveTransactionResult result;
    QString error;
    const bool confirmed = hardware.executeLiveTransaction(
        beforeBytes, serializedBytes, &result, &error);

    qInfo().noquote() << "SIGNAL CHAIN TRANSACTION module=" << moduleId;
    qInfo().noquote() << "BEFORE=" << beforeBytes.join(" ");
    qInfo().noquote() << "REQUESTED=" << serializedBytes.join(" ");
    qInfo().noquote() << "READBACK=" << result.readback.join(" ");
    qInfo().noquote() << "MATCH=" << (result.match ? "true" : "false");
    if (result.rollbackAttempted) {
        qInfo().noquote() << "ROLLBACK READBACK="
                          << result.rollbackReadback.join(" ");
        qInfo().noquote() << "ROLLBACK MATCH="
                          << (result.rollbackMatch ? "true" : "false");
    }

    signalChainTransactionActive = false;
    pendingSignalChainModuleId = -1;
    if (confirmed) {
        syncSignalChainCache(serializedBytes);
        modernSignalChainModel::ChainSnapshot confirmedSnapshot = after;
        confirmedSnapshot.patchIdentity = before.patchIdentity;
        signalChainModel.replaceSnapshot(confirmedSnapshot, nullptr);
    } else if (result.patchChanged) {
        refreshSignalChainModel();
    } else if (result.rollbackMatch || result.readback.isEmpty()) {
        signalChainModel.replaceSnapshot(before, nullptr);
        if (result.rollbackMatch)
            syncSignalChainCache(beforeBytes);
    } else if (result.readback.size() == 18) {
        modernSignalChainModel::ChainSnapshot physical;
        if (modernSignalChainModel::parseRawBytes(
                result.readback, &physical, nullptr, after.revision + 1,
                result.patchIdentity, before.channelMode,
                before.channelSelect)) {
            signalChainModel.replaceSnapshot(physical, nullptr);
            syncSignalChainCache(result.readback);
        } else {
            signalChainModel.replaceSnapshot(before, nullptr);
        }
    } else {
        signalChainModel.replaceSnapshot(before, nullptr);
    }
    if (!confirmed)
        qWarning().noquote() << "Signal Chain move not confirmed:" << error;
    rebuildSignalChainView();
}

void modernFloorBoard::applyResponsiveSignalChainLayout()
{
    if (!signalChainModel.isValid() || !signalChainScroll || !signalFlowLayout)
        return;

    const int available = signalChainScroll->viewport()->width();
    const int viewportHeight = signalChainScroll->viewport()->height();
    if (available <= 0 || viewportHeight <= 0)
        return;

    QWidget *content = signalChainScroll->widget();
    if (!content)
        return;
    content->setMinimumHeight(viewportHeight);
    content->setMaximumHeight(viewportHeight);

    const int slotCount = signalChainModel.commonPrefix().size()
        + qMax(signalChainModel.pathA().size(), signalChainModel.pathB().size())
        + signalChainModel.commonSuffix().size();
    if (slotCount <= 0)
        return;

    const int gap = qBound(2, available / 240, 6);
    const int connectorWidth = qBound(24, available / 44, 32);
    const int junctionWidth = qBound(18, available / 55, 28);
    const int outerItemCount = signalChainModel.commonPrefix().size()
        + signalChainModel.commonSuffix().size() + 5;
    const int totalGapWidth = qMax(0, outerItemCount - 1) * gap
        + qMax(signalChainModel.pathA().size(), signalChainModel.pathB().size()) * gap;
    const int fixedWidth = connectorWidth * 2 + junctionWidth * 2
        + 16 + totalGapWidth;
    const int moduleWidth = qBound(52, (available - fixedWidth) / slotCount, 96);
    const int moduleHeight = qBound(58, int(moduleWidth * .78), 78);

    signalFlowLayout->setContentsMargins(2, 0, 2, 0);
    signalFlowLayout->setSpacing(gap);
    if (signalPathsLayout) {
        const int pathMargin = moduleWidth < 66 ? 2 : 4;
        signalPathsLayout->setContentsMargins(pathMargin, 4, pathMargin, 4);
        signalPathsLayout->setHorizontalSpacing(gap);
        signalPathsLayout->setRowMinimumHeight(0, moduleHeight);
        signalPathsLayout->setRowMinimumHeight(1, moduleHeight);
    }
    for (SignalChainModule *module : signalChainModules)
        module->setCompactWidth(moduleWidth);
    for (SignalJunction *junction : signalChainJunctions)
        junction->setCompactWidth(junctionWidth);
    for (SignalConnector *connector : signalChainConnectors)
        connector->setCompactWidth(connectorWidth);

    signalFlowLayout->invalidate();
    signalFlowLayout->activate();
    if (signalPathsLayout)
        signalPathsLayout->activate();

    qreal pathOffset = (moduleHeight
        + (signalPathsLayout ? signalPathsLayout->verticalSpacing() : 0)) / 2.0;
    if (signalPathsLayout && signalParallelPaths) {
        const QRect rowA = signalPathsLayout->cellRect(0, 0);
        const QRect rowB = signalPathsLayout->cellRect(1, 0);
        if (rowA.isValid() && rowB.isValid())
            pathOffset = qAbs(rowB.center().y() - rowA.center().y()) / 2.0;
    }
    for (SignalJunction *junction : signalChainJunctions)
        junction->setChainGeometry(viewportHeight, pathOffset);

    signalFlowLayout->invalidate();
    signalFlowLayout->activate();
    if (signalPathsLayout)
        signalPathsLayout->activate();

    // Register the live junction widgets after their geometry is established.
    // SignalChainContent maps their centers into paint coordinates on demand.
    SignalJunction *mergeJunction = signalChainJunctions.size() > 1
        ? signalChainJunctions.at(1) : nullptr;
    if (signalPathsLayout && signalParallelPaths && signalChainContent
        && splitJunction && mergeJunction) {
        const QRect rowA = signalPathsLayout->cellRect(0, 0);
        const QRect rowB = signalPathsLayout->cellRect(1, 0);
        if (rowA.isValid() && rowB.isValid()) {
            const QPoint panelOrigin = signalParallelPaths->mapTo(
                signalChainContent, QPoint(0, 0));
            const qreal pathAY = panelOrigin.y() + rowA.center().y();
            const qreal pathBY = panelOrigin.y() + rowB.center().y();
            signalChainContent->setParallelCableGeometry(
                splitJunction, mergeJunction, pathAY, pathBY);
        }
    }
    signalChainScroll->verticalScrollBar()->setValue(0);

#ifndef QT_NO_DEBUG
    const qreal chainCenterY = viewportHeight / 2.0;
    for (SignalChainModule *module : signalChainModules) {
        if (module->parentWidget() != content)
            continue;
        const qreal moduleCenterY = module->mapTo(content, module->rect().center()).y();
        Q_ASSERT_X(qAbs(moduleCenterY - chainCenterY) <= 1.0,
                   "Signal Chain geometry", "common module is off the chain axis");
    }
    for (SignalJunction *junction : signalChainJunctions) {
        const qreal junctionCenterY =
            junction->mapTo(content, junction->rect().center()).y();
        Q_ASSERT_X(qAbs(junctionCenterY - chainCenterY) <= 1.0,
                   "Signal Chain geometry", "junction is off the chain axis");
    }
    if (signalPathsLayout) {
        const QRect rowA = signalPathsLayout->cellRect(0, 0);
        const QRect rowB = signalPathsLayout->cellRect(1, 0);
        Q_ASSERT_X(qAbs((rowA.center().y() + rowB.center().y())
                       / 2.0 - signalParallelPaths->height() / 2.0) <= 1.0,
                   "Signal Chain geometry", "parallel paths are not symmetric");
    }
#endif
}

void modernFloorBoard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyResponsiveSignalChainLayout();
}

bool modernFloorBoard::eventFilter(QObject *watched, QEvent *event)
{
    if (signalChainScroll && watched == signalChainScroll->viewport()) {
        if (event->type() == QEvent::Resize) {
            QTimer::singleShot(0, this, [this]() {
                applyResponsiveSignalChainLayout();
            });
        } else if (event->type() == QEvent::Wheel) {
            QTimer::singleShot(0, this, [this]() {
                if (signalChainScroll)
                    signalChainScroll->verticalScrollBar()->setValue(0);
            });
        }
    }

    return QWidget::eventFilter(watched, event);
}

void modernFloorBoard::updateReverbParameterControls(bool available)
{
    if (reverbModelBrowser)
        reverbModelBrowser->setEnabled(available);

    const QList<QComboBox *> combos = {reverbType, reverbLowCut, reverbHighCut};

    for (QComboBox *combo : combos)
        if (combo) combo->setEnabled(available);
    if (reverbOnOff) {
        reverbOnOff->setEnabled(available);
        reverbOnOff->setVisible(available);
        if (!available)
            reverbOnOff->setChecked(false);
    }
    for (ParameterBar *bar : reverbBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available)
    {
        for (QComboBox *combo : combos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (reverbTypeDisplay) reverbTypeDisplay->setText(QString::fromUtf8("—"));
        if (reverbArtwork) reverbArtwork->setTextOverlayText("type", QString());
        if (reverbModelBrowser)
            reverbModelBrowser->setCurrentIndex(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    if (reverbOnOff)
        reverbOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "30") == 1);
    for (QComboBox *combo : combos) {
        if (!combo) continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "0A", "00", address));
    }

    if (reverbTypeDisplay && reverbType) {
        reverbTypeDisplay->setText(reverbType->currentText());
    }
    if (reverbArtwork && reverbType)
        reverbArtwork->setTextOverlayText(
            "type", reverbType->currentText().toUpper());
    if (reverbModelBrowser && reverbType)
        reverbModelBrowser->setCurrentIndex(reverbType->currentIndex());

    for (ParameterBar *bar : reverbBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "0A", "00", address,
            QString::number(value, 16).toUpper()));
    }

    if (reverbSpringSensitivity && reverbType)
        reverbSpringSensitivity->setEnabled(
            reverbType->currentIndex() == 5);
}

void modernFloorBoard::setReverbValue(const QString &address,
                                      int value,
                                      bool twoByte)
{
    if (!hasValidReverbBuffer())
        return;

    SysxIO *sysxIO = SysxIO::Instance();
    if (twoByte) {
        const QString high = QString("%1").arg(value / 128, 2, 16, QChar('0')).toUpper();
        const QString low = QString("%1").arg(value % 128, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource("Structure", "0A", "00", address, high, low);
    } else {
        const QString hex = QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource("Structure", "0A", "00", address, hex);
    }
}

void modernFloorBoard::reverbComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == reverbType) {
        setReverbType(value);
        return;
    }
    setReverbValue(combo->property("address").toString(), value, false);
}

void modernFloorBoard::setReverbType(int index)
{
    if (!reverbType || index < 0 || index >= reverbType->count()
        || !hasValidReverbBuffer())
        return;

    {
        const QSignalBlocker blocker(reverbType);
        reverbType->setCurrentIndex(index);
    }
    setReverbValue("31", index, false);
    if (reverbModelBrowser)
        reverbModelBrowser->setCurrentIndex(index);
    if (reverbTypeDisplay)
        reverbTypeDisplay->setText(reverbType->itemText(index));
    if (reverbArtwork)
        reverbArtwork->setTextOverlayText(
            "type", reverbType->itemText(index).toUpper());
    if (reverbSpringSensitivity)
        reverbSpringSensitivity->setEnabled(index == 5);
}

void modernFloorBoard::reverbModelSelected(int index)
{
    setReverbType(index);
}

void modernFloorBoard::reverbBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;

    const QString address = bar->property("address").toString();
    setReverbValue(address, value, bar->property("twoByte").toBool());
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "0A", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : reverbBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::showCompEditor()
{
    selectedEditor = "COMP";
    if (effectEditorStack && compEditor)
        effectEditorStack->setCurrentWidget(compEditor);
    if (compCard)
        compCard->setSelected(true);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showReverbEditor()
{
    selectedEditor = "REVERB";
    if (effectEditorStack && reverbEditor)
        effectEditorStack->setCurrentWidget(reverbEditor);
    if (reverbCard)
        reverbCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showOddsEditor()
{
    selectedEditor = "OD/DS";
    if (effectEditorStack && oddsEditor)
        effectEditorStack->setCurrentWidget(oddsEditor);
    if (oddsCard)
        oddsCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showDelayEditor()
{
    selectedEditor = "DELAY";
    if (effectEditorStack && delayEditor)
        effectEditorStack->setCurrentWidget(delayEditor);
    if (delayCard)
        delayCard->setSelected(true);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showChorusEditor()
{
    refreshChorus();
    selectedEditor = "CHORUS";
    if (effectEditorStack && chorusEditor)
        effectEditorStack->setCurrentWidget(chorusEditor);
    if (chorusCard)
        chorusCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showEqEditor()
{
    selectedEditor = "EQ";
    if (effectEditorStack && eqEditor)
        effectEditorStack->setCurrentWidget(eqEditor);
    if (eqCard)
        eqCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showPreampAEditor()
{
    selectedEditor = "PREAMP A";
    if (effectEditorStack && preampA.editor)
        effectEditorStack->setCurrentWidget(preampA.editor);
    if (preampACard)
        preampACard->setSelected(true);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showPreampBEditor()
{
    selectedEditor = "PREAMP B";
    if (effectEditorStack && preampB.editor)
        effectEditorStack->setCurrentWidget(preampB.editor);
    if (preampBCard)
        preampBCard->setSelected(true);
    if (preampACard)
        preampACard->setSelected(false);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::showFx1Editor()
{
    showFxEditor(FxSlot::FX1);
}

void modernFloorBoard::showFx2Editor()
{
    showFxEditor(FxSlot::FX2);
}

void modernFloorBoard::showFxEditor(FxSlot slot)
{
    ModernFxEditor *targetEditor = slot == FxSlot::FX1
        ? fx1Editor : fx2Editor;
    SignalChainModule *targetCard = slot == FxSlot::FX1
        ? fx1Card : fx2Card;
    if (!targetEditor)
        return;

    selectedEditor = slot == FxSlot::FX1 ? "FX-1" : "FX-2";
    if (effectEditorStack)
        effectEditorStack->setCurrentWidget(targetEditor->widget());
    if (fx1Card)
        fx1Card->setSelected(targetCard == fx1Card);
    if (fx2Card)
        fx2Card->setSelected(targetCard == fx2Card);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::refreshFx(FxSlot slot)
{
    ModernFxEditor *targetEditor = slot == FxSlot::FX1
        ? fx1Editor : fx2Editor;
    if (targetEditor)
        targetEditor->refreshFx(backendIsConnected, backendHasPatchData);
}

void modernFloorBoard::fx1StateChanged(bool available, bool on)
{
    if (fx1Card)
        fx1Card->setEffectState(available, on);
}

void modernFloorBoard::fx2StateChanged(bool available, bool on)
{
    if (fx2Card)
        fx2Card->setEffectState(available, on);
}

void modernFloorBoard::showPedalFxEditor()
{
    showPedalEditor(false);
}

void modernFloorBoard::showFootVolumeEditor()
{
    showPedalEditor(true);
}

void modernFloorBoard::showPedalEditor(bool footVolumeContext)
{
    if (!pedalFxEditor)
        return;

    selectedEditor = footVolumeContext ? "FV" : "P.FX";
    pedalFxEditor->setContext(footVolumeContext
        ? PedalEditorContext::FootVolume
        : PedalEditorContext::General);
    pedalFxEditor->refreshPedalFx(backendIsConnected,
                                  backendHasPatchData);
    if (effectEditorStack)
        effectEditorStack->setCurrentWidget(pedalFxEditor->widget());

    if (pedalFxCard)
        pedalFxCard->setSelected(!footVolumeContext);
    if (footVolumeCard)
        footVolumeCard->setSelected(footVolumeContext);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::refreshPedalFx()
{
    if (pedalFxEditor)
        pedalFxEditor->refreshPedalFx(backendIsConnected,
                                      backendHasPatchData);
}

void modernFloorBoard::pedalFxActivityChanged(
    bool available, bool pedalFxActive, bool footVolumeActive)
{
    if (pedalFxCard)
        pedalFxCard->setEffectState(available, pedalFxActive);
    if (footVolumeCard)
        footVolumeCard->setEffectState(available, footVolumeActive);
}

void modernFloorBoard::clearPedalSelection()
{
    if (pedalFxCard)
        pedalFxCard->setSelected(false);
    if (footVolumeCard)
        footVolumeCard->setSelected(false);
}

void modernFloorBoard::showNs1Editor()
{
    showNoiseSuppressorEditor(NoiseSuppressorSlot::NS1);
}

void modernFloorBoard::showNs2Editor()
{
    showNoiseSuppressorEditor(NoiseSuppressorSlot::NS2);
}

void modernFloorBoard::showNoiseSuppressorEditor(NoiseSuppressorSlot slot)
{
    ModernNoiseSuppressorEditor *targetEditor =
        slot == NoiseSuppressorSlot::NS1 ? ns1Editor : ns2Editor;
    if (!targetEditor)
        return;

    selectedEditor = slot == NoiseSuppressorSlot::NS1 ? "NS-1" : "NS-2";
    targetEditor->refreshNoiseSuppressor(backendIsConnected,
                                         backendHasPatchData);
    if (effectEditorStack)
        effectEditorStack->setCurrentWidget(targetEditor->widget());

    if (ns1Card)
        ns1Card->setSelected(slot == NoiseSuppressorSlot::NS1);
    if (ns2Card)
        ns2Card->setSelected(slot == NoiseSuppressorSlot::NS2);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearSendReturnSelection();
}

void modernFloorBoard::refreshNoiseSuppressors()
{
    if (ns1Editor)
        ns1Editor->refreshNoiseSuppressor(backendIsConnected,
                                           backendHasPatchData);
    if (ns2Editor)
        ns2Editor->refreshNoiseSuppressor(backendIsConnected,
                                           backendHasPatchData);
}

void modernFloorBoard::clearNoiseSuppressorSelection()
{
    if (ns1Card)
        ns1Card->setSelected(false);
    if (ns2Card)
        ns2Card->setSelected(false);
    clearSendReturnSelection();
}

void modernFloorBoard::showSendReturnEditor()
{
    if (!sendReturnEditor)
        return;

    selectedEditor = "SEND/RETURN";
    sendReturnEditor->refreshSendReturn(backendIsConnected,
                                        backendHasPatchData);
    if (effectEditorStack)
        effectEditorStack->setCurrentWidget(sendReturnEditor->widget());

    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    if (splitJunction)
        splitJunction->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
    if (sendReturnCard)
        sendReturnCard->setSelected(true);
}

void modernFloorBoard::refreshSendReturn()
{
    if (sendReturnEditor)
        sendReturnEditor->refreshSendReturn(backendIsConnected,
                                             backendHasPatchData);
}

void modernFloorBoard::clearSendReturnSelection()
{
    if (sendReturnCard)
        sendReturnCard->setSelected(false);
}

void modernFloorBoard::showChannelRoutingEditor()
{
    refreshChannelRouting();
    selectedEditor = "CHANNEL ROUTING";
    if (effectEditorStack && channelRoutingEditor)
        effectEditorStack->setCurrentWidget(channelRoutingEditor);
    if (splitJunction)
        splitJunction->setSelected(true);
    if (preampACard)
        preampACard->setSelected(false);
    if (preampBCard)
        preampBCard->setSelected(false);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
    if (chorusCard)
        chorusCard->setSelected(false);
    if (eqCard)
        eqCard->setSelected(false);
    if (fx1Card)
        fx1Card->setSelected(false);
    if (fx2Card)
        fx2Card->setSelected(false);
    clearPedalSelection();
    clearNoiseSuppressorSelection();
}

void modernFloorBoard::updateChannelRoutingPage(int mode)
{
    if (channelRoutingStack) {
        const int page = mode == 0 ? 0
            : (mode == 1 || mode == 2) ? 1 : 2;
        channelRoutingStack->setCurrentIndex(page);
    }
    if (channelRoutingDiagram) {
        channelRoutingDiagram->setProperty("routingMode", mode);
        const int channel = channelBButton && channelBButton->isChecked()
            ? 1 : 0;
        channelRoutingDiagram->setProperty("routingChannel", channel);
        channelRoutingDiagram->update();
    }
}

void modernFloorBoard::updateChannelRoutingControls(bool available)
{
    if (channelMode)
        channelMode->setEnabled(available);
    for (QPushButton *button : channelModeButtons)
        button->setEnabled(available);
    if (channelAButton)
        channelAButton->setEnabled(available);
    if (channelBButton)
        channelBButton->setEnabled(available);
    if (channelDelay)
        channelDelay->setEnabled(available);
    if (dynamicSense)
        dynamicSense->setEnabled(available);

    if (available)
        return;
    if (channelMode) {
        const QSignalBlocker blocker(channelMode);
        channelMode->setCurrentIndex(-1);
    }
    for (QPushButton *button : channelModeButtons) {
        const QSignalBlocker blocker(button);
        button->setChecked(false);
    }
    if (channelAButton) {
        const QSignalBlocker blocker(channelAButton);
        channelAButton->setChecked(false);
    }
    if (channelBButton) {
        const QSignalBlocker blocker(channelBButton);
        channelBButton->setChecked(false);
    }
    if (channelDelay)
        channelDelay->setDisplayText(QString::fromUtf8("—"));
    if (dynamicSense)
        dynamicSense->setDisplayText(QString::fromUtf8("—"));
    if (channelRoutingDiagram) {
        channelRoutingDiagram->setProperty("routingMode", -1);
        channelRoutingDiagram->setProperty("routingChannel", -1);
        channelRoutingDiagram->update();
    }
}

void modernFloorBoard::refreshChannelRouting()
{
    if (!hasValidPreampBuffer()) {
        updateChannelRoutingControls(false);
        return;
    }

    updateChannelRoutingControls(true);
    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    const int mode = sysxIO->getSourceValue(
        "Structure", "01", "00", "01");
    const int channel = sysxIO->getSourceValue(
        "Structure", "01", "00", "02");
    const int delay = sysxIO->getSourceValue(
        "Structure", "01", "00", "03");
    const int sense = sysxIO->getSourceValue(
        "Structure", "01", "00", "04");

    if (channelMode) {
        const QSignalBlocker blocker(channelMode);
        channelMode->setCurrentIndex(mode);
    }
    for (int raw = 0; raw < channelModeButtons.size(); ++raw) {
        const QSignalBlocker blocker(channelModeButtons.at(raw));
        channelModeButtons.at(raw)->setChecked(raw == mode);
    }
    if (channelAButton) {
        const QSignalBlocker blocker(channelAButton);
        channelAButton->setChecked(channel == 0);
    }
    if (channelBButton) {
        const QSignalBlocker blocker(channelBButton);
        channelBButton->setChecked(channel == 1);
    }
    if (channelDelay) {
        const QSignalBlocker blocker(channelDelay);
        channelDelay->setValue(delay);
        channelDelay->setDisplayText(midiTable->getValue(
            "Structure", "01", "00", "03",
            QString::number(delay, 16).toUpper()));
    }
    if (dynamicSense) {
        const QSignalBlocker blocker(dynamicSense);
        dynamicSense->setValue(sense);
        dynamicSense->setDisplayText(midiTable->getValue(
            "Structure", "01", "00", "04",
            QString::number(sense, 16).toUpper()));
    }
    updateChannelRoutingPage(mode);
}

void modernFloorBoard::setChannelRoutingValue(const QString &address,
                                               int value)
{
    if (!hasValidPreampBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "01", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setChannelMode(int index)
{
    if (!channelMode || index < 0 || index > 3
        || !hasValidPreampBuffer())
        return;
    {
        const QSignalBlocker blocker(channelMode);
        channelMode->setCurrentIndex(index);
    }
    for (int raw = 0; raw < channelModeButtons.size(); ++raw) {
        const QSignalBlocker blocker(channelModeButtons.at(raw));
        channelModeButtons.at(raw)->setChecked(raw == index);
    }
    // The GT-10 raw value is the UI index. midi.xml contains duplicated
    // value attributes for these labels, so Midi.value is intentionally
    // not used here.
    setChannelRoutingValue("01", index);
    updateChannelRoutingPage(index);
}

void modernFloorBoard::setChannelSelect(int index)
{
    if (index < 0 || index > 1 || !hasValidPreampBuffer())
        return;
    setChannelRoutingValue("02", index);
    if (channelAButton)
        channelAButton->setChecked(index == 0);
    if (channelBButton)
        channelBButton->setChecked(index == 1);
    if (channelRoutingDiagram) {
        channelRoutingDiagram->setProperty("routingChannel", index);
        channelRoutingDiagram->update();
    }
}

void modernFloorBoard::setChannelDelay(int value)
{
    setChannelRoutingValue("03", value);
}

void modernFloorBoard::setDynamicSense(int value)
{
    setChannelRoutingValue("04", value);
}

void modernFloorBoard::channelModeChanged(int index)
{
    setChannelMode(index);
}

void modernFloorBoard::channelRoutingBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property(
        "channelRoutingAddress").toString();
    if (address == "03")
        setChannelDelay(value);
    else if (address == "04")
        setDynamicSense(value);
    bar->setDisplayText(MidiTable::Instance()->getValue(
        "Structure", "01", "00", address,
        QString::number(value, 16).toUpper()));
}

void modernFloorBoard::updatePreampConditionalSections(
    PreampChannel channel)
{
    PreampEditorState &state = preampState(channel);
    const int type = state.type ? state.type->currentIndex() : -1;
    const int customType = state.customType
        ? state.customType->currentIndex() : -1;
    const int speakerType = state.speakerType
        ? state.speakerType->currentIndex() : -1;

    if (state.customPreampSection)
        state.customPreampSection->setVisible(type == 0x27);
    if (state.customSpeakerSection)
        state.customSpeakerSection->setVisible(speakerType == 0x09);
    if (state.brightControl)
        state.brightControl->setVisible(
            preampBrightAvailable(type, customType));
}

void modernFloorBoard::updatePreampParameterControls(
    PreampChannel channel, bool available)
{
    PreampEditorState &state = preampState(channel);
    if (state.browser)
        state.browser->setEnabled(available);

    for (QComboBox *combo : state.combos) {
        if (combo)
            combo->setEnabled(available);
    }
    for (ModernToggleSwitch *toggle : state.toggles) {
        if (!toggle)
            continue;
        toggle->setEnabled(available);
        if (!available)
            toggle->setChecked(false);
    }
    for (ParameterBar *bar : state.bars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : state.combos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (state.browser)
            state.browser->setCurrentIndex(-1);
        if (state.brightControl)
            state.brightControl->hide();
        if (state.customPreampSection)
            state.customPreampSection->hide();
        if (state.customSpeakerSection)
            state.customSpeakerSection->hide();
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    for (QComboBox *combo : state.combos) {
        if (!combo)
            continue;
        const int offset = combo->property("preampOffset").toInt();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "01", "00", preampAddress(channel, offset)));
    }

    if (state.browser && state.type)
        state.browser->setCurrentIndex(state.type->currentIndex());

    for (ModernToggleSwitch *toggle : state.toggles) {
        if (!toggle || toggle->property("preampGlobalState").toBool())
            continue;
        const int offset = toggle->property("preampOffset").toInt();
        toggle->setChecked(sysxIO->getSourceValue(
            "Structure", "01", "00", preampAddress(channel, offset)) == 1);
    }

    for (ParameterBar *bar : state.bars) {
        if (!bar)
            continue;
        const int offset = bar->property("preampOffset").toInt();
        const QString address = preampAddress(channel, offset);
        const int value = sysxIO->getSourceValue(
            "Structure", "01", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(preampDisplayText(
            midiTable->getValue(
                "Structure", "01", "00", address,
                QString::number(value, 16).toUpper()),
            offset));
    }
    updatePreampConditionalSections(channel);
}

void modernFloorBoard::refreshPreamp(PreampChannel channel)
{
    if (!hasValidPreampBuffer()) {
        updatePreampParameterControls(channel, false);
        return;
    }
    updatePreampParameterControls(channel, true);
}

void modernFloorBoard::refreshPreampGlobalState()
{
    if (!hasValidPreampBuffer()) {
        setPreampUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "01", "00", "00") == 1;
    if (preampA.globalState)
        preampA.globalState->setChecked(on);
    if (preampB.globalState)
        preampB.globalState->setChecked(on);
    if (preampACard)
        preampACard->setEffectState(true, on);
    if (preampBCard)
        preampBCard->setEffectState(true, on);
}

void modernFloorBoard::setPreampValue(PreampChannel channel,
                                       int offset, int value)
{
    if (!hasValidPreampBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "01", "00", preampAddress(channel, offset),
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setPreampGlobalState(bool on)
{
    if (!hasValidPreampBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "01", "00", "00", on ? "01" : "00");
    refreshPreampGlobalState();
}

void modernFloorBoard::setPreampType(PreampChannel channel, int index)
{
    PreampEditorState &state = preampState(channel);
    if (!state.type || index < 0 || index >= state.type->count()
        || !hasValidPreampBuffer())
        return;

    {
        const QSignalBlocker blocker(state.type);
        state.type->setCurrentIndex(index);
    }
    setPreampValue(channel, 0x00, index);
    if (state.browser)
        state.browser->setCurrentIndex(index);
    updatePreampConditionalSections(channel);
}

void modernFloorBoard::preampModelSelected(int index)
{
    QObject *browser = sender();
    if (!browser)
        return;
    const PreampChannel channel = browser->property("preampChannel").toInt()
        == 0 ? PreampChannel::A : PreampChannel::B;
    setPreampType(channel, index);
}

void modernFloorBoard::preampComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    const PreampChannel channel = combo->property("preampChannel").toInt()
        == 0 ? PreampChannel::A : PreampChannel::B;
    const int offset = combo->property("preampOffset").toInt();
    if (offset == 0x00) {
        setPreampType(channel, value);
        return;
    }
    setPreampValue(channel, offset, value);
    if (offset == 0x0B || offset == 0x11)
        updatePreampConditionalSections(channel);
}

void modernFloorBoard::preampBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const PreampChannel channel = bar->property("preampChannel").toInt()
        == 0 ? PreampChannel::A : PreampChannel::B;
    const int offset = bar->property("preampOffset").toInt();
    const QString address = preampAddress(channel, offset);
    setPreampValue(channel, offset, value);
    bar->setDisplayText(preampDisplayText(
        MidiTable::Instance()->getValue(
            "Structure", "01", "00", address,
            QString::number(value, 16).toUpper()),
        offset));
}

void modernFloorBoard::preampToggleChanged()
{
    ModernToggleSwitch *toggle =
        static_cast<ModernToggleSwitch *>(sender());
    if (!toggle || !hasValidPreampBuffer())
        return;
    if (toggle->property("preampGlobalState").toBool()) {
        setPreampGlobalState(toggle->isChecked());
        return;
    }
    const PreampChannel channel = toggle->property("preampChannel").toInt()
        == 0 ? PreampChannel::A : PreampChannel::B;
    setPreampValue(channel, toggle->property("preampOffset").toInt(),
                   toggle->isChecked() ? 1 : 0);
}

void modernFloorBoard::updateCompParameterControls(bool available)
{
    if (compModelBrowser)
        compModelBrowser->setEnabled(available);
    if (compType)
        compType->setEnabled(available);
    if (compOnOff) {
        compOnOff->setEnabled(available);
        compOnOff->setVisible(available);
        if (!available)
            compOnOff->setChecked(false);
    }
    for (ParameterBar *bar : compBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        if (compType) {
            const QSignalBlocker blocker(compType);
            compType->setCurrentIndex(-1);
        }
        if (compTypeDisplay)
            compTypeDisplay->setText(QString::fromUtf8("—"));
        if (compModelBrowser)
            compModelBrowser->setCurrentIndex(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    const int type = sysxIO->getSourceValue(
        "Structure", "00", "00", "41");
    if (compType) {
        const QSignalBlocker blocker(compType);
        compType->setCurrentIndex(type);
    }
    if (compModeStack)
        compModeStack->setCurrentIndex(type == 1 ? 1 : 0);
    if (compModelBrowser)
        compModelBrowser->setCurrentIndex(type);
    if (compTypeDisplay && compType)
        compTypeDisplay->setText(compType->currentText());
    if (compOnOff)
        compOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "40") == 1);

    for (ParameterBar *bar : compBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "00", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "00", "00", address,
            QString::number(value, 16).toUpper()));
    }
}

void modernFloorBoard::setCompValue(const QString &address, int value)
{
    if (!hasValidCompBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "00", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setCompType(int index)
{
    if (!compType || index < 0 || index >= compType->count()
        || !hasValidCompBuffer())
        return;

    {
        const QSignalBlocker blocker(compType);
        compType->setCurrentIndex(index);
    }
    setCompValue(compType->property("address").toString(), index);
    if (compModelBrowser)
        compModelBrowser->setCurrentIndex(index);
    if (compModeStack)
        compModeStack->setCurrentIndex(index == 1 ? 1 : 0);
    if (compTypeDisplay)
        compTypeDisplay->setText(compType->itemText(index));
}

void modernFloorBoard::compTypeChanged(int value)
{
    if (!compType || sender() != compType)
        return;
    setCompType(value);
}

void modernFloorBoard::compModelSelected(int index)
{
    setCompType(index);
}

void modernFloorBoard::compBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setCompValue(address, value);
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "00", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : compBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::updateOddsParameterControls(bool available)
{
    if (oddsModelBrowser)
        oddsModelBrowser->setEnabled(available);

    const QList<QComboBox *> combos = {oddsType, oddsCustomType};
    for (QComboBox *combo : combos) {
        if (combo)
            combo->setEnabled(available);
    }

    const QList<ModernToggleSwitch *> toggles = {
        oddsOnOff, oddsSoloSwitch
    };
    for (ModernToggleSwitch *toggle : toggles) {
        if (!toggle)
            continue;
        toggle->setEnabled(available);
        toggle->setVisible(available);
        if (!available)
            toggle->setChecked(false);
    }

    for (ParameterBar *bar : oddsBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : combos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (oddsArtwork)
            oddsArtwork->setTextOverlayText("type", QString());
        if (oddsModelBrowser)
            oddsModelBrowser->setCurrentIndex(-1);
        if (oddsCustomSection)
            oddsCustomSection->hide();
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    for (QComboBox *combo : combos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "00", "00", address));
    }

    if (oddsOnOff)
        oddsOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "70") == 1);
    if (oddsSoloSwitch)
        oddsSoloSwitch->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "77") == 1);
    if (oddsCustomSection && oddsType)
        oddsCustomSection->setVisible(oddsType->currentIndex() == 0x19);
    if (oddsModelBrowser && oddsType)
        oddsModelBrowser->setCurrentIndex(oddsType->currentIndex());
    if (oddsArtwork && oddsType)
        oddsArtwork->setTextOverlayText(
            "type", oddsArtworkType(oddsType->currentText()));

    for (ParameterBar *bar : oddsBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "00", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "00", "00", address,
            QString::number(value, 16).toUpper()));
    }
}

void modernFloorBoard::setOddsValue(const QString &address, int value)
{
    if (!hasValidOddsBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "00", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setOddsType(int index)
{
    if (!oddsType || index < 0 || index >= oddsType->count()
        || !hasValidOddsBuffer())
        return;

    {
        const QSignalBlocker blocker(oddsType);
        oddsType->setCurrentIndex(index);
    }

    setOddsValue(oddsType->property("address").toString(), index);
    if (oddsModelBrowser)
        oddsModelBrowser->setCurrentIndex(index);
    if (oddsCustomSection)
        oddsCustomSection->setVisible(index == 0x19);
    if (oddsArtwork)
        oddsArtwork->setTextOverlayText(
            "type", oddsArtworkType(oddsType->itemText(index)));
}

void modernFloorBoard::oddsComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == oddsType) {
        setOddsType(value);
        return;
    }
    setOddsValue(combo->property("address").toString(), value);
}

void modernFloorBoard::oddsModelSelected(int index)
{
    setOddsType(index);
}

void modernFloorBoard::oddsBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setOddsValue(address, value);
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "00", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : oddsBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::oddsToggleChanged()
{
    QObject *toggle = sender();
    if (!toggle || !hasValidOddsBuffer())
        return;
    const QString address = toggle->property("address").toString();
    const bool newState = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", address) != 1;
    setOddsValue(address, newState ? 1 : 0);
    refreshOddsState();
}

void modernFloorBoard::updateDelayPageForType(int type)
{
    const bool dual = type >= 3 && type <= 5;
    if (delayPageStack)
        delayPageStack->setCurrentIndex(dual ? 1 : 0);

    int extraPage = 0;
    if (type == 1)
        extraPage = 1;
    else if (type == 9)
        extraPage = 2;
    else if (type == 10)
        extraPage = 3;
    if (delayExtraStack) {
        delayExtraStack->setCurrentIndex(extraPage);
        delayExtraStack->setVisible(extraPage != 0);
    }
}

void modernFloorBoard::updateDelayParameterControls(bool available)
{
    if (delayModelBrowser)
        delayModelBrowser->setEnabled(available);

    for (QComboBox *combo : delayCombos) {
        if (combo)
            combo->setEnabled(available);
    }

    const QList<ModernToggleSwitch *> toggles = {
        delayOnOff, delayWarpSwitch
    };
    for (ModernToggleSwitch *toggle : toggles) {
        if (!toggle)
            continue;
        toggle->setEnabled(available);
        toggle->setVisible(available);
        if (!available)
            toggle->setChecked(false);
    }

    for (ParameterBar *bar : delayBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : delayCombos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (delayArtwork)
            delayArtwork->setTextOverlayText("type", QString());
        if (delayModelBrowser)
            delayModelBrowser->setCurrentIndex(-1);
        updateDelayPageForType(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    for (QComboBox *combo : delayCombos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "0A", "00", address));
    }

    const int type = delayType ? delayType->currentIndex() : -1;
    updateDelayPageForType(type);
    if (delayArtwork && delayType)
        delayArtwork->setTextOverlayText(
            "type", delayArtworkType(delayType->currentText()));
    if (delayModelBrowser && delayType)
        delayModelBrowser->setCurrentIndex(delayType->currentIndex());
    if (delayOnOff)
        delayOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "00") == 1);
    if (delayWarpSwitch)
        delayWarpSwitch->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "11") == 1);

    for (ParameterBar *bar : delayBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(FxPresentation::formatRhythmicDivision(
            midiTable->getValue(
                "Structure", "0A", "00", address,
                QString::number(value, 16).toUpper())));
    }
}

void modernFloorBoard::setDelayValue(const QString &address, int value,
                                     bool twoByte)
{
    if (!hasValidDelayBuffer())
        return;

    if (twoByte) {
        const QString high = QString("%1").arg(
            value / 128, 2, 16, QChar('0')).toUpper();
        const QString low = QString("%1").arg(
            value % 128, 2, 16, QChar('0')).toUpper();
        SysxIO::Instance()->setFileSource(
            "Structure", "0A", "00", address, high, low);
    } else {
        SysxIO::Instance()->setFileSource(
            "Structure", "0A", "00", address,
            QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    }
}

void modernFloorBoard::delayComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == delayType) {
        setDelayType(value);
        return;
    }
    setDelayValue(combo->property("address").toString(), value);
}

void modernFloorBoard::setDelayType(int index)
{
    if (!delayType || index < 0 || index >= delayType->count()
        || !hasValidDelayBuffer())
        return;

    {
        const QSignalBlocker blocker(delayType);
        delayType->setCurrentIndex(index);
    }
    setDelayValue("01", index);
    if (delayModelBrowser)
        delayModelBrowser->setCurrentIndex(index);
    updateDelayPageForType(index);
    if (delayArtwork)
        delayArtwork->setTextOverlayText(
            "type", delayArtworkType(delayType->itemText(index)));
}

void modernFloorBoard::delayModelSelected(int index)
{
    setDelayType(index);
}

void modernFloorBoard::delayBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setDelayValue(address, value, bar->property("twoByte").toBool());
    const QString display = FxPresentation::formatRhythmicDivision(
        MidiTable::Instance()->getValue(
            "Structure", "0A", "00", address,
            QString::number(value, 16).toUpper()));

    // Direct Level is presented on both the standard and Dual pages.
    // Keep duplicate views of the same backend address synchronized.
    for (ParameterBar *peer : delayBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::updateChorusParameterControls(bool available)
{
    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();

    if (chorusModeBrowser)
        chorusModeBrowser->setEnabled(available);

    if (chorusOnOff) {
        const QSignalBlocker blocker(chorusOnOff);
        chorusOnOff->setEnabled(available);
        chorusOnOff->setVisible(true);
        chorusOnOff->setChecked(available && sysxIO->getSourceValue(
            "Structure", "0A", "00", "20") == 1);
    }

    for (QComboBox *combo : chorusCombos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const bool parameterAvailable = available
            && hasValidChorusParameter(address);
        const QSignalBlocker blocker(combo);
        combo->setEnabled(parameterAvailable);
        if (!parameterAvailable) {
            combo->setCurrentIndex(-1);
            continue;
        }

        const int raw = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        combo->setCurrentIndex(combo->findData(raw));
    }

    if (chorusModeBrowser) {
        const QSignalBlocker blocker(chorusModeBrowser);
        chorusModeBrowser->setCurrentIndex(
            available && chorusMode ? chorusMode->currentIndex() : -1);
    }
    if (chorusArtwork) {
        chorusArtwork->setTextOverlayText(
            "mode", available && chorusMode
                ? chorusMode->currentText().toUpper()
                : QString());
    }

    for (ParameterBar *bar : chorusBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const bool parameterAvailable = available
            && hasValidChorusParameter(address);
        bar->setEnabled(parameterAvailable);
        if (!parameterAvailable) {
            bar->setDisplayText(QString::fromUtf8("—"));
            continue;
        }

        const int value = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(FxPresentation::formatRhythmicDivision(
            midiTable->getValue(
                "Structure", "0A", "00", address,
                QString::number(value, 16).toUpper())));
    }
}

void modernFloorBoard::setChorusValue(const QString &address, int value)
{
    if (!hasValidChorusBuffer() || !hasValidChorusParameter(address))
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "0A", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setChorusMode(int index)
{
    if (!chorusMode || index < 0 || index >= chorusMode->count()
        || !hasValidChorusBuffer())
        return;

    bool rawOk = false;
    const int raw = chorusMode->itemData(index).toInt(&rawOk);
    if (!rawOk)
        return;

    {
        const QSignalBlocker blocker(chorusMode);
        chorusMode->setCurrentIndex(index);
    }
    setChorusValue("21", raw);
    if (chorusModeBrowser)
        chorusModeBrowser->setCurrentIndex(index);
    if (chorusArtwork)
        chorusArtwork->setTextOverlayText(
            "mode", chorusMode->itemText(index).toUpper());
}

void modernFloorBoard::chorusComboChanged(int index)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo || index < 0)
        return;

    if (combo == chorusMode) {
        setChorusMode(index);
        return;
    }

    bool rawOk = false;
    const int raw = combo->itemData(index).toInt(&rawOk);
    if (!rawOk)
        return;
    setChorusValue(combo->property("address").toString(), raw);
}

void modernFloorBoard::chorusModeSelected(int index)
{
    setChorusMode(index);
}

void modernFloorBoard::chorusBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setChorusValue(address, value);
    bar->setDisplayText(FxPresentation::formatRhythmicDivision(
        MidiTable::Instance()->getValue(
            "Structure", "0A", "00", address,
            QString::number(value, 16).toUpper())));
}

void modernFloorBoard::toggleChorus()
{
    if (!chorusOnOff || !hasValidChorusBuffer())
        return;
    setChorusValue("20", chorusOnOff->isChecked() ? 1 : 0);
    refreshChorus();
}

void modernFloorBoard::updateEqParameterControls(bool available)
{
    for (QComboBox *combo : eqCombos) {
        if (combo)
            combo->setEnabled(available);
    }
    if (eqOnOff) {
        eqOnOff->setEnabled(available);
        eqOnOff->setVisible(available);
        if (!available) {
            const QSignalBlocker blocker(eqOnOff);
            eqOnOff->setChecked(false);
        }
    }
    for (ParameterBar *bar : eqBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : eqCombos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        updateEqGraph();
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    if (eqOnOff) {
        const QSignalBlocker blocker(eqOnOff);
        eqOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "01", "00", "70") == 1);
    }
    for (QComboBox *combo : eqCombos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "01", "00", address));
    }
    for (ParameterBar *bar : eqBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "01", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(formatEqDisplay(
            midiTable->getValue(
                "Structure", "01", "00", address,
                QString::number(value, 16).toUpper()),
            address, value));
    }
    updateEqGraph();
}

void modernFloorBoard::updateEqGraph()
{
    if (!eqGraph)
        return;

    auto comboForAddress = [this](const QString &address) -> QComboBox * {
        for (QComboBox *combo : eqCombos) {
            if (combo && combo->property("address").toString() == address)
                return combo;
        }
        return nullptr;
    };
    auto barForAddress = [this](const QString &address) -> ParameterBar * {
        for (ParameterBar *bar : eqBars) {
            if (bar && bar->property("address").toString() == address)
                return bar;
        }
        return nullptr;
    };
    auto gainForAddress = [&barForAddress](const QString &address) -> qreal {
        ParameterBar *bar = barForAddress(address);
        bool valid = false;
        const qreal displayed = bar
            ? numericPresentationValue(bar->displayText(), &valid) : 0.0;
        return valid ? displayed : 0.0;
    };
    auto comboNumber = [&comboForAddress](const QString &address,
                                          qreal fallback) -> qreal {
        QComboBox *combo = comboForAddress(address);
        bool valid = false;
        const qreal value = combo
            ? numericPresentationValue(combo->currentText(), &valid) : 0.0;
        return valid ? value : fallback;
    };
    auto comboFrequency = [&comboForAddress](const QString &address,
                                             qreal fallback) -> qreal {
        QComboBox *combo = comboForAddress(address);
        bool valid = false;
        const qreal value = combo
            ? frequencyPresentationValue(combo->currentText(), &valid) : 0.0;
        return valid ? value : fallback;
    };

    eqGraph->setEqActive(eqOnOff && eqOnOff->isEnabled()
                         && eqOnOff->isChecked());
    eqGraph->setLowGain(gainForAddress("72"));
    eqGraph->setLowMid(comboFrequency("73", 500.0),
                       gainForAddress("75"),
                       comboNumber("74", 1.0));
    eqGraph->setHighMid(comboFrequency("76", 2000.0),
                        gainForAddress("78"),
                        comboNumber("77", 1.0));
    eqGraph->setHighGain(gainForAddress("79"));

    QComboBox *lowCut = comboForAddress("71");
    QComboBox *highCut = comboForAddress("7A");
    const bool lowCutEnabled = lowCut && lowCut->currentIndex() >= 0
        && lowCut->currentText().compare("FLAT", Qt::CaseInsensitive) != 0;
    const bool highCutEnabled = highCut && highCut->currentIndex() >= 0
        && highCut->currentText().compare("FLAT", Qt::CaseInsensitive) != 0;
    eqGraph->setLowCut(lowCutEnabled,
                       comboFrequency("71", 20.0));
    eqGraph->setHighCut(highCutEnabled,
                        comboFrequency("7A", 20000.0));
}

void modernFloorBoard::setEqValue(const QString &address, int value)
{
    if (!hasValidEqBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "01", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::eqComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo || value < 0)
        return;
    setEqValue(combo->property("address").toString(), value);
    updateEqGraph();
}

void modernFloorBoard::eqBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setEqValue(address, value);
    bar->setDisplayText(formatEqDisplay(
        MidiTable::Instance()->getValue(
            "Structure", "01", "00", address,
            QString::number(value, 16).toUpper()),
        address, value));
    updateEqGraph();
}

void modernFloorBoard::toggleEq()
{
    if (!hasValidEqBuffer())
        return;
    const bool newState = SysxIO::Instance()->getSourceValue(
        "Structure", "01", "00", "70") != 1;
    setEqValue("70", newState ? 1 : 0);
    refreshEq();
}

void modernFloorBoard::delayToggleChanged()
{
    QObject *toggle = sender();
    if (!toggle || !hasValidDelayBuffer())
        return;
    const QString address = toggle->property("address").toString();
    const bool newState = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", address) != 1;
    setDelayValue(address, newState ? 1 : 0);
    refreshDelayState();
}

void modernFloorBoard::toggleComp()
{
    if (!hasValidCompBuffer())
        return;
    SysxIO *sysxIO = SysxIO::Instance();
    const bool newState = sysxIO->getSourceValue(
        "Structure", "00", "00", "40") != 1;
    setCompValue("40", newState ? 1 : 0);
    refreshCompState();
}

void modernFloorBoard::toggleReverb()
{
    SysxIO *sysxIO = SysxIO::Instance();

    if (!hasValidReverbBuffer())
        return;

    int current = sysxIO->getSourceValue(
        "Structure",
        "0A",
        "00",
        "30"
    );

    bool newState = (current != 1);

    sysxIO->setFileSource(
        "Structure",
        "0A",
        "00",
        "30",
        newState ? "01" : "00"
    );

    refreshReverbState();
}
