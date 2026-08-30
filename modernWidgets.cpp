#include "modernWidgets.h"
#include "modernTheme.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QSizePolicy>
#include <QStringList>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <cmath>

namespace {
const qreal kPi = 3.14159265358979323846;

class CompactComboBox final : public QComboBox
{
public:
    explicit CompactComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        connect(this, &QComboBox::currentTextChanged,
                this, [this]() { updateTextTooltip(); });
    }

    void showPopup() override
    {
        int popupWidth = width();
        const QFontMetrics metrics(view()->font());
        for (int index = 0; index < count(); ++index)
            popupWidth = qMax(popupWidth,
                              metrics.horizontalAdvance(itemText(index)) + 36);
        view()->setMinimumWidth(popupWidth);
        QComboBox::showPopup();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);

        QStylePainter painter(this);
        painter.drawComplexControl(QStyle::CC_ComboBox, option);

        const QRect textRect = style()->subControlRect(
            QStyle::CC_ComboBox, &option,
            QStyle::SC_ComboBoxEditField, this);
        const QString fullText = option.currentText;
        option.currentText = fontMetrics().elidedText(
            fullText, Qt::ElideRight, qMax(0, textRect.width() - 4));
        painter.drawControl(QStyle::CE_ComboBoxLabel, option);

        const QString desiredTooltip = option.currentText == fullText
            ? QString() : fullText;
        if (toolTip() != desiredTooltip)
            setToolTip(desiredTooltip);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QComboBox::resizeEvent(event);
        updateTextTooltip();
    }

private:
    void updateTextTooltip()
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect textRect = style()->subControlRect(
            QStyle::CC_ComboBox, &option,
            QStyle::SC_ComboBoxEditField, this);
        const QString elided = fontMetrics().elidedText(
            currentText(), Qt::ElideRight, qMax(0, textRect.width() - 4));
        setToolTip(elided == currentText() ? QString() : currentText());
    }
};

void drawScrew(QPainter &p, const QPointF &c)
{
    QRadialGradient metal(c - QPointF(1.5, 1.5), 6);
    metal.setColorAt(0, QColor("#CDD3D8"));
    metal.setColorAt(.42, QColor("#68727B"));
    metal.setColorAt(1, QColor("#171B20"));
    p.setPen(QPen(QColor("#06080A"), 1)); p.setBrush(metal);
    p.drawEllipse(c, 5, 5);
    p.setPen(QPen(QColor("#242A30"), 1.4));
    p.drawLine(c - QPointF(2.5, 0), c + QPointF(2.5, 0));
}

}

QWidget *createParameterScrollContent(QWidget *content, QWidget *parent)
{
    QWidget *wrapper = new QWidget(parent);
    wrapper->setObjectName("EffectParameterScrollContent");
    QHBoxLayout *layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 12, 0);
    layout->setSpacing(0);
    layout->addWidget(content);
    return wrapper;
}

EffectEditorPanel::EffectEditorPanel(const QString &effectName, QWidget *parent)
    : QFrame(parent), rightPanelWidget(nullptr)
{
    setObjectName("EffectEditorPanel");
    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *artworkPane = new QFrame;
    artworkPane->setObjectName("EffectArtworkPane");
    artworkPane->setMinimumWidth(240);
    artworkPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    artworkLayout = new QVBoxLayout(artworkPane);
    artworkLayout->setContentsMargins(8, 8, 8, 8);
    artworkLayout->setSpacing(6);
    QLabel *title = new QLabel(effectName);
    title->setObjectName("EditorTitle");
    const QString editorAccent = ModernTheme::activeEffectAccent(effectName);
    title->setStyleSheet(QString("color:%1;").arg(editorAccent));
    artworkLayout->addWidget(title);
    currentType = new QLabel(QString::fromUtf8("—"));
    currentType->setObjectName("EffectTypeDisplay");
    currentType->setStyleSheet(QString("color:%1;").arg(editorAccent));
    artworkLayout->addWidget(currentType);
    artwork = new QWidget;
    artwork->setObjectName("EffectArtworkArea");
    artworkLayout->addWidget(artwork, 1);

    QFrame *parameterPane = new QFrame;
    parameterPane->setObjectName("EffectParameterPane");
    parameterPane->setMinimumWidth(420);
    parameterPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *parameterPaneLayout = new QVBoxLayout(parameterPane);
    parameterPaneLayout->setContentsMargins(10, 10, 10, 10);
    parameterPaneLayout->setSpacing(6);
    QLabel *parameterTitle = new QLabel("PARAMETERS");
    parameterTitle->setObjectName("WorkspaceColumnTitle");
    parameterPaneLayout->addWidget(parameterTitle);
    parameters = new QWidget;
    parameters->setObjectName("EffectParameterArea");
    parameters->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QScrollArea *parameterScroll = new QScrollArea;
    parameterScroll->setObjectName("EffectParameterScroll");
    parameterScroll->setWidgetResizable(true);
    parameterScroll->setFrameShape(QFrame::NoFrame);
    parameterScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parameterScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    parameterScroll->setWidget(createParameterScrollContent(parameters));
    parameterPaneLayout->addWidget(parameterScroll, 1);

    QFrame *modelPane = new QFrame;
    modelPane->setObjectName("EffectModelPane");
    modelPane->setMinimumWidth(180);
    modelPane->setMaximumWidth(280);
    modelPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    modelLayout = new QVBoxLayout(modelPane);
    modelLayout->setContentsMargins(12, 12, 12, 12);
    modelLayout->setSpacing(8);
    const QString modelHeading = effectName == "OD/DS"
        ? "OD/DS MODELS" : effectName + " TYPES";
    modelTitle = new QLabel(modelHeading);
    modelTitle->setObjectName("WorkspaceColumnTitle");
    modelLayout->addWidget(modelTitle);
    QFrame *modelRule = new QFrame;
    modelRule->setObjectName("WorkspaceRule");
    modelRule->setFixedHeight(1);
    modelLayout->addWidget(modelRule);
    modelState = new QLabel("MODEL BROWSER\nNOT INTEGRATED");
    modelState->setObjectName("WorkspaceUnavailable");
    modelState->setAlignment(Qt::AlignCenter);
    modelState->setWordWrap(true);
    modelLayout->addWidget(modelState, 1);
    rightPanelWidget = modelState;

    root->addWidget(artworkPane, 25);
    root->addWidget(parameterPane, 56);
    root->addWidget(modelPane, 19);
}

QLabel *EffectEditorPanel::typeLabel() const { return currentType; }
QWidget *EffectEditorPanel::parameterArea() const { return parameters; }
QWidget *EffectEditorPanel::artworkArea() const { return artwork; }
void EffectEditorPanel::setArtworkWidget(QWidget *widget)
{
    if (!widget)
        return;
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(artwork->layout());
    if (!layout) {
        layout = new QVBoxLayout(artwork);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(widget);
}

void EffectEditorPanel::setArtworkControlWidget(QWidget *widget)
{
    if (widget)
        artworkLayout->addWidget(widget, 0, Qt::AlignLeft);
}

void EffectEditorPanel::setModelBrowserWidget(QWidget *widget)
{
    setRightPanelWidget(widget);
}

void EffectEditorPanel::setRightPanelTitle(const QString &title)
{
    if (modelTitle)
        modelTitle->setText(title);
}

void EffectEditorPanel::setRightPanelWidget(QWidget *widget)
{
    if (!widget || !modelLayout || widget == rightPanelWidget)
        return;
    if (rightPanelWidget) {
        modelLayout->removeWidget(rightPanelWidget);
        rightPanelWidget->hide();
        rightPanelWidget->deleteLater();
    }
    if (rightPanelWidget == modelState)
        modelState = nullptr;
    rightPanelWidget = widget;
    modelLayout->addWidget(widget, 1);
}

QSize EffectEditorPanel::minimumSizeHint() const
{
    return QSize(840, qMax(330, parameters->minimumSizeHint().height() + 46));
}

namespace {
class BottomActionRegion final : public QFrame
{
public:
    explicit BottomActionRegion(QWidget *parent = nullptr) : QFrame(parent) {}
    std::function<void()> activated;
protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QFrame::mouseReleaseEvent(event);
        if (rect().contains(event->pos()) && activated)
            activated();
    }
};

class BottomAssignBadge final : public QPushButton
{
public:
    explicit BottomAssignBadge(int number, QWidget *parent = nullptr)
        : QPushButton(parent), assignNumber(number)
    {
        setFixedSize(22, 31);
        setCursor(Qt::PointingHandCursor);
        setFlat(true);
    }

    void setSummaryState(bool summaryAvailable, bool enabled,
                         bool selected)
    {
        available = summaryAvailable;
        assignEnabled = enabled;
        current = selected;
        setEnabled(summaryAvailable);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const bool highlighted = isEnabled() && (underMouse() || current);
        painter.setPen(QPen(highlighted ? QColor("#2A7599")
                                       : QColor("#242A30"), 1));
        painter.setBrush(highlighted ? QColor("#101B23")
                                     : QColor("#090D11"));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

        painter.setPen(available ? QColor("#C8CDD2") : QColor("#59616A"));
        QFont numberFont = font();
        numberFont.setPixelSize(9);
        numberFont.setWeight(QFont::DemiBold);
        painter.setFont(numberFont);
        painter.drawText(QRect(0, 2, width(), 14), Qt::AlignCenter,
                         QString::number(assignNumber));

        painter.setPen(Qt::NoPen);
        painter.setBrush(!available ? QColor("#363D44")
                                    : assignEnabled ? QColor("#00AEEF")
                                                    : QColor("#424A52"));
        painter.drawEllipse(QPointF(width() / 2.0, 23.0), 3.0, 3.0);
    }

    void enterEvent(QEvent *event) override
    {
        QPushButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QPushButton::leaveEvent(event);
        update();
    }

private:
    int assignNumber = 0;
    bool available = false;
    bool assignEnabled = false;
    bool current = false;
};

class BottomSummaryValueLabel final : public QLabel
{
public:
    explicit BottomSummaryValueLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        const QString visibleText = fontMetrics().elidedText(
            text(), Qt::ElideRight, qMax(0, contentsRect().width()));
        painter.drawText(contentsRect(), alignment(), visibleText);
    }
};

class BottomPedalFootWidget final : public QWidget
{
public:
    explicit BottomPedalFootWidget(const QString &text,
                                   QWidget *parent = nullptr)
        : QWidget(parent), label(text)
    {
        setFixedSize(38, 48);
        setToolTip(text);
    }

    void setVisualState(ModernPedalboardModel::LogicalState newState)
    {
        if (state == newState)
            return;
        state = newState;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF cardRect(5.5, 0.5, 27.0, 27.0);
        painter.setPen(QPen(QColor("#293A47"), 1.0));
        painter.setBrush(QColor(14, 30, 42, 205));
        painter.drawRoundedRect(cardRect, 5.0, 5.0);

        QFont labelFont = font();
        labelFont.setPixelSize(8);
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);
        painter.setPen(QColor("#AEB7C0"));
        painter.drawText(QRectF(0, 31, width(), 13),
                         Qt::AlignHCenter | Qt::AlignVCenter, label);

        const QPointF ledCenter(width() / 2.0, 14.0);
        const qreal radius = 3.25;
        painter.setPen(Qt::NoPen);
        if (state == ModernPedalboardModel::LogicalState::Unknown
            || state == ModernPedalboardModel::LogicalState::Momentary) {
            painter.setBrush(QColor("#3A454E"));
            painter.setPen(QPen(QColor("#53616C"), 0.7));
        } else if (state == ModernPedalboardModel::LogicalState::On) {
            painter.setBrush(QColor(255, 34, 40, 55));
            painter.drawEllipse(ledCenter, radius * 2.1, radius * 2.1);
            QRadialGradient led(ledCenter - QPointF(0.7, 0.8), radius * 1.3);
            led.setColorAt(0.0, QColor("#FFE2E2"));
            led.setColorAt(0.25, QColor("#FF4B50"));
            led.setColorAt(1.0, QColor("#8B0B10"));
            painter.setBrush(led);
            painter.setPen(QPen(QColor("#FF7478"), 0.7));
        } else {
            painter.setBrush(QColor("#48181B"));
            painter.setPen(QPen(QColor("#713034"), 0.7));
        }
        painter.drawEllipse(ledCenter, radius, radius);
    }

private:
    QString label;
    ModernPedalboardModel::LogicalState state =
        ModernPedalboardModel::LogicalState::Unknown;
};
}

BottomControlStrip::BottomControlStrip(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("BottomControlStrip");
    setMinimumHeight(115);
    setMaximumHeight(130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QWidget *contentWrapper = new QWidget;
    // Keep the strip close to the natural width of the primary workspace.
    contentWrapper->setMaximumWidth(1680);
    contentWrapper->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
    QHBoxLayout *layout = new QHBoxLayout(contentWrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const QStringList titles = {
        "EXPRESSION", "CONTROL ASSIGN", "PEDALBOARD", "TUNER"
    };
    const int stretches[] = {17, 33, 33, 17};
    for (int i = 0; i < titles.size(); ++i) {
        QFrame *region = (i >= 0 && i <= 2)
            ? static_cast<QFrame *>(new BottomActionRegion)
            : new QFrame;
        region->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        region->setObjectName(i == 0 ? "BottomRegionFirst" : "BottomRegion");
        QVBoxLayout *regionLayout = new QVBoxLayout(region);
        regionLayout->setContentsMargins(14, i == 0 ? 9 : 11,
                                         14, i == 0 ? 13 : 11);
        regionLayout->setSpacing(8);
        QLabel *title = new QLabel(titles.at(i));
        title->setObjectName("BottomRegionTitle");
        if (i >= 0 && i <= 2) {
            QWidget *header = new QWidget;
            header->setFixedHeight(19);
            header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            QHBoxLayout *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(0, 0, 0, 0);
            headerLayout->setSpacing(6);
            headerLayout->addWidget(title);
            headerLayout->addStretch(1);
            QPushButton *editButton = new QPushButton("EDIT");
            editButton->setObjectName(i == 0
                ? "BottomExpressionEditButton" : i == 1
                    ? "BottomAssignEditButton" : "BottomPedalboardEditButton");
            editButton->setFixedSize(38, 19);
            editButton->setCursor(Qt::PointingHandCursor);
            headerLayout->addWidget(editButton, 0, Qt::AlignVCenter);
            regionLayout->addWidget(header);
            BottomActionRegion *actionRegion =
                static_cast<BottomActionRegion *>(region);
            connect(editButton, &QPushButton::clicked, editButton,
                    [actionRegion]() {
                if (actionRegion->activated)
                    actionRegion->activated();
            });
        } else {
            regionLayout->addWidget(title);
        }
        if (titles.at(i) == "TUNER") {
            QVBoxLayout *tunerLayout = new QVBoxLayout;
            tunerLayout->setContentsMargins(0, 0, 0, 0);
            tunerLayout->setSpacing(4);

            QLabel *referenceLabel = new QLabel("REFERENCE");
            referenceLabel->setObjectName("BottomTunerLabel");
            QLabel *outputLabel = new QLabel("OUTPUT");
            outputLabel->setObjectName("BottomTunerLabel");

            tunerReference = new QComboBox;
            tunerReference->setObjectName("BottomTunerCombo");
            for (int raw = 0; raw <= 0x0A; ++raw)
                tunerReference->addItem(QString::number(435 + raw) + " Hz", raw);

            tunerOutput = new QComboBox;
            tunerOutput->setObjectName("BottomTunerCombo");
            tunerOutput->addItem("MUTE", 0x00);
            tunerOutput->addItem("BYPASSED", 0x01);

            const QList<QComboBox *> tunerCombos = {
                tunerReference, tunerOutput
            };
            for (QComboBox *combo : tunerCombos) {
                combo->setEnabled(false);
                combo->setCurrentIndex(-1);
                combo->setFixedHeight(17);
                combo->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
            }

            const QList<QLabel *> tunerLabels = {
                referenceLabel, outputLabel
            };
            for (int row = 0; row < tunerCombos.size(); ++row) {
                QFrame *card = new QFrame;
                card->setObjectName("BottomTunerCard");
                card->setMinimumHeight(34);
                card->setMaximumHeight(36);
                card->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Fixed);
                QVBoxLayout *cardLayout = new QVBoxLayout(card);
                cardLayout->setContentsMargins(8, 3, 6, 3);
                cardLayout->setSpacing(0);
                cardLayout->addWidget(tunerLabels.at(row));
                cardLayout->addWidget(tunerCombos.at(row));
                tunerLayout->addWidget(card);
            }
            regionLayout->addLayout(tunerLayout, 1);

            region->setStyleSheet(
                "QFrame#BottomTunerCard {"
                " background: rgba(14,30,42,205);"
                " border: 1px solid #293A47; border-radius: 5px; }"
                "QFrame#BottomTunerCard:hover {"
                " background: rgba(17,37,51,215); border-color: #354B5A; }"
                "QLabel#BottomTunerLabel {"
                " color: #88949F; font-size: 8px; font-weight: 600;"
                " letter-spacing: 0.4px; }"
                "QComboBox#BottomTunerCombo {"
                " padding: 0 17px 0 0; color: #39B8F3;"
                " background: transparent; border: none;"
                " font-size: 10px; font-weight: 600; }"
                "QComboBox#BottomTunerCombo:hover {"
                " color: #55C8FA; background: transparent; }"
                "QComboBox#BottomTunerCombo:focus {"
                " color: #55C8FA; background: transparent; }"
                "QComboBox#BottomTunerCombo:disabled {"
                " color: #66737E; background: transparent; }"
                "QComboBox#BottomTunerCombo::drop-down {"
                " subcontrol-origin: padding; subcontrol-position:"
                " center right; width: 16px; border: none; }"
                "QComboBox#BottomTunerCombo QAbstractItemView {"
                " color: #ECEFF2; background: #0D0F12;"
                " border: 1px solid #24272C;"
                " selection-background-color: #123347; outline: none; }"
            );
        } else if (i == 0) {
            QGridLayout *summaryLayout = new QGridLayout;
            summaryLayout->setContentsMargins(0, 0, 0, 0);
            summaryLayout->setHorizontalSpacing(4);
            summaryLayout->setVerticalSpacing(1);
            const QStringList names = {"EXP1", "EXP SW", "EXP2"};
            for (int column = 0; column < names.size(); ++column) {
                QFrame *controlCard = new QFrame;
                controlCard->setObjectName("BottomExpressionControlCard");
                controlCard->setMinimumHeight(30);
                controlCard->setMaximumHeight(34);
                controlCard->setSizePolicy(QSizePolicy::Ignored,
                                           QSizePolicy::Fixed);
                QVBoxLayout *cardLayout = new QVBoxLayout(controlCard);
                cardLayout->setContentsMargins(6, 3, 6, 3);
                cardLayout->setSpacing(0);
                QLabel *name = new QLabel(names.at(column));
                name->setObjectName("BottomExpressionLabel");
                name->setAlignment(Qt::AlignCenter);
                name->setMinimumWidth(0);
                name->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Preferred);
                QLabel *value = new BottomSummaryValueLabel;
                value->setObjectName("BottomExpressionValue");
                value->setText(QString::fromUtf8("—"));
                expressionValues.append(value);
                cardLayout->addWidget(name);
                cardLayout->addWidget(value);
                summaryLayout->addWidget(controlCard, 0, column);
                summaryLayout->setColumnMinimumWidth(column, 0);
                summaryLayout->setColumnStretch(column, 1);
            }
            regionLayout->addLayout(summaryLayout);
            QHBoxLayout *assignLayout = new QHBoxLayout;
            assignLayout->setContentsMargins(0, 3, 0, 0);
            assignLayout->setSpacing(3);
            QLabel *assigns = new QLabel("ASSIGNS");
            assigns->setObjectName("BottomExpressionLabel");
            assignLayout->addWidget(assigns);
            assignLayout->addStretch(1);
            for (int index = 0; index < 8; ++index) {
                QPushButton *badge = new QPushButton(QString("A%1").arg(index + 1));
                badge->setObjectName("BottomExpressionAssignBadge");
                badge->setFixedSize(22, 18);
                badge->setVisible(false);
                badge->setCursor(Qt::PointingHandCursor);
                badge->setProperty("assignIndex", index);
                expressionBadges.append(badge);
                assignLayout->addWidget(badge);
            }
            regionLayout->addLayout(assignLayout);
            expressionRegion = region;
            region->setCursor(Qt::PointingHandCursor);
            region->setStyleSheet(
                "QFrame#BottomExpressionControlCard{"
                "background:rgba(14,30,42,205);"
                "border:1px solid #293A47;border-radius:5px;}"
                "QLabel#BottomExpressionLabel{color:#88949F;font-size:8px;"
                "font-weight:600;letter-spacing:0.4px;}"
                "QLabel#BottomExpressionValue{color:#E5E9ED;font-size:9px;"
                "font-weight:600;}"
                "QPushButton#BottomExpressionAssignBadge{color:#39B8F3;"
                "background:#101B23;border:1px solid #27506A;"
                "border-radius:4px;font-size:7px;font-weight:600;padding:0;}"
                "QPushButton#BottomExpressionAssignBadge:hover{"
                "border-color:#39B8F3;background:#132734;}"
                "QPushButton#BottomExpressionEditButton{color:#E6C8C8;"
                "background:#211315;border:1px solid #63363A;"
                "border-radius:4px;font-size:8px;font-weight:600;padding:0;}"
                "QPushButton#BottomExpressionEditButton:hover{color:#F0D8D8;"
                "border-color:#865057;background:#2B171A;}"
                "QPushButton#BottomExpressionEditButton:pressed{"
                "background:#180D0F;border-color:#A65B62;}");
        } else if (i == 1) {
            QGridLayout *summaryLayout = new QGridLayout;
            summaryLayout->setContentsMargins(0, 0, 0, 0);
            summaryLayout->setHorizontalSpacing(4);
            summaryLayout->setVerticalSpacing(1);
            const QStringList controlNames = {"CTL1", "CTL2", "EXP SW"};
            for (int column = 0; column < controlNames.size(); ++column) {
                QFrame *controlCard = new QFrame;
                controlCard->setObjectName("BottomAssignControlCard");
                controlCard->setMinimumHeight(30);
                controlCard->setMaximumHeight(34);
                controlCard->setSizePolicy(QSizePolicy::Ignored,
                                           QSizePolicy::Fixed);
                QVBoxLayout *cardLayout = new QVBoxLayout(controlCard);
                cardLayout->setContentsMargins(6, 3, 6, 3);
                cardLayout->setSpacing(0);
                QLabel *name = new QLabel(controlNames.at(column));
                name->setObjectName("BottomAssignLabel");
                name->setAlignment(Qt::AlignCenter);
                name->setMinimumWidth(0);
                name->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Preferred);
                QLabel *value = new BottomSummaryValueLabel;
                value->setText(QString::fromUtf8("—"));
                value->setObjectName("BottomAssignValue");
                controlAssignValues.append(value);
                cardLayout->addWidget(name);
                cardLayout->addWidget(value);
                summaryLayout->addWidget(controlCard, 0, column);
                summaryLayout->setColumnMinimumWidth(column, 0);
                summaryLayout->setColumnStretch(column, 1);
            }
            regionLayout->addLayout(summaryLayout);

            QHBoxLayout *assignLayout = new QHBoxLayout;
            assignLayout->setContentsMargins(0, 1, 0, 0);
            assignLayout->setSpacing(3);
            QLabel *assigns = new QLabel("ASSIGNS");
            assigns->setObjectName("BottomAssignLabel");
            assignLayout->addWidget(assigns);
            assignLayout->addStretch(1);
            for (int index = 0; index < 8; ++index) {
                BottomAssignBadge *badge = new BottomAssignBadge(index + 1);
                controlAssignBadges.append(badge);
                assignLayout->addWidget(badge);
            }
            regionLayout->addLayout(assignLayout);
            region->setStyleSheet(
                "QFrame#BottomAssignControlCard{"
                "background:rgba(14,30,42,205);"
                "border:1px solid #293A47;border-radius:5px;}"
                "QLabel#BottomAssignLabel{color:#88949F;font-size:8px;"
                "font-weight:600;letter-spacing:0.4px;}"
                "QLabel#BottomAssignValue{color:#E5E9ED;font-size:9px;"
                "font-weight:600;}"
                "QPushButton#BottomAssignEditButton{color:#E6C8C8;"
                "background:#211315;border:1px solid #63363A;"
                "border-radius:4px;font-size:8px;font-weight:600;"
                "padding:0;}"
                "QPushButton#BottomAssignEditButton:hover{color:#F0D8D8;"
                "border-color:#865057;background:#2B171A;}"
                "QPushButton#BottomAssignEditButton:pressed{"
                "background:#180D0F;border-color:#A65B62;}"
                "QPushButton#BottomAssignEditButton:disabled{color:#665357;"
                "background:#110D0E;border-color:#332529;}"
            );
        } else if (i == 2) {
            QGridLayout *footsLayout = new QGridLayout;
            footsLayout->setContentsMargins(0, 0, 0, 0);
            footsLayout->setHorizontalSpacing(4);
            footsLayout->setVerticalSpacing(0);
            footsLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

            const QStringList footLabels = {
                "1", "2", "3", "4", "CTL1", "CTL2", "DN", "UP", "EXP"
            };
            for (int column = 0; column < footLabels.size(); ++column) {
                BottomPedalFootWidget *foot = new BottomPedalFootWidget(
                    footLabels.at(column));
                pedalboardFoots.append(foot);
                footsLayout->addWidget(foot, 0, column,
                                       Qt::AlignHCenter | Qt::AlignVCenter);
                footsLayout->setColumnStretch(column, 1);
            }
            regionLayout->addLayout(footsLayout, 1);
            pedalboardRegion = region;
            region->setCursor(Qt::PointingHandCursor);
            region->setStyleSheet(
                "QPushButton#BottomPedalboardEditButton{color:#E6C8C8;"
                "background:#211315;border:1px solid #63363A;"
                "border-radius:4px;font-size:8px;font-weight:600;padding:0;}"
                "QPushButton#BottomPedalboardEditButton:hover{color:#F0D8D8;"
                "border-color:#865057;background:#2B171A;}"
                "QPushButton#BottomPedalboardEditButton:pressed{"
                "background:#180D0F;border-color:#A65B62;}");
        } else {
            QLabel *state = new QLabel(i == 1
                ? "DIRECT CONTROLS" : "NOT INTEGRATED");
            state->setObjectName("WorkspaceUnavailable");
            state->setAlignment(Qt::AlignCenter);
            regionLayout->addWidget(state, 1);
        }
        if (i == 1) {
            controlAssignRegion = region;
            region->setCursor(Qt::PointingHandCursor);
        }
        layout->addWidget(region, stretches[i]);
    }
    outerLayout->addStretch();
    outerLayout->addWidget(contentWrapper, 1);
    outerLayout->addStretch();
}

QComboBox *BottomControlStrip::tunerReferenceComboBox() const
{
    return tunerReference;
}

QComboBox *BottomControlStrip::tunerOutputComboBox() const
{
    return tunerOutput;
}

void BottomControlStrip::setControlAssignActivated(
    const std::function<void()> &callback)
{
    BottomActionRegion *region =
        dynamic_cast<BottomActionRegion *>(controlAssignRegion);
    if (region)
        region->activated = callback;
}

void BottomControlStrip::setExpressionActivated(
    const std::function<void()> &callback)
{
    BottomActionRegion *region =
        dynamic_cast<BottomActionRegion *>(expressionRegion);
    if (region)
        region->activated = callback;
}

void BottomControlStrip::setExpressionAssignActivated(
    const std::function<void(int)> &callback)
{
    for (QPushButton *badge : expressionBadges) {
        QObject::connect(badge, &QPushButton::clicked, badge,
                         [badge, callback]() {
            if (callback)
                callback(badge->property("assignIndex").toInt());
        });
    }
}

void BottomControlStrip::setExpressionSummary(
    bool available, const QString &exp1, const QString &expSwitch,
    const QString &exp2, const QList<int> &assigns)
{
    const QStringList values = {exp1, expSwitch, exp2};
    for (int index = 0; index < expressionValues.size(); ++index) {
        const QString value = available && index < values.size()
            && !values.at(index).trimmed().isEmpty()
            ? values.at(index) : QString::fromUtf8("—");
        expressionValues.at(index)->setText(value);
        expressionValues.at(index)->setToolTip(
            value == QString::fromUtf8("—") ? QString() : value);
    }
    for (QPushButton *badge : expressionBadges) {
        const int index = badge->property("assignIndex").toInt();
        badge->setVisible(available && assigns.contains(index));
    }
}

void BottomControlStrip::setPedalboardActivated(
    const std::function<void()> &callback)
{
    BottomActionRegion *region =
        dynamic_cast<BottomActionRegion *>(pedalboardRegion);
    if (region)
        region->activated = callback;
}

void BottomControlStrip::setPedalboardSummary(
    const QVector<ModernPedalboardModel::LogicalState> &states)
{
    for (int index = 0; index < pedalboardFoots.size(); ++index) {
        BottomPedalFootWidget *foot =
            static_cast<BottomPedalFootWidget *>(pedalboardFoots.at(index));
        const ModernPedalboardModel::LogicalState state = index < states.size()
            ? states.at(index) : ModernPedalboardModel::LogicalState::Unknown;
        foot->setVisualState(state);
    }
}

void BottomControlStrip::setAssignActivated(
    const std::function<void(int)> &callback)
{
    for (int index = 0; index < controlAssignBadges.size(); ++index) {
        QObject::connect(controlAssignBadges.at(index), &QPushButton::clicked,
                         controlAssignBadges.at(index),
                         [callback, index]() {
            if (callback)
                callback(index);
        });
    }
}

void BottomControlStrip::setControlAssignSummary(
    bool available, const QString &ctl1, const QString &ctl2,
    const QString &expSwitch, const QVector<bool> &assignStates,
    int selectedAssign)
{
    const QStringList values = {ctl1, ctl2, expSwitch};
    for (int index = 0; index < controlAssignValues.size(); ++index) {
        QLabel *label = controlAssignValues.at(index);
        const QString value = available && index < values.size()
            && !values.at(index).trimmed().isEmpty()
            ? values.at(index) : QString::fromUtf8("—");
        label->setText(value);
        label->setToolTip(value == QString::fromUtf8("—")
            ? QString() : value);
    }
    for (int index = 0; index < controlAssignBadges.size(); ++index) {
        BottomAssignBadge *badge = static_cast<BottomAssignBadge *>(
            controlAssignBadges.at(index));
        badge->setSummaryState(available && index < assignStates.size(),
                               index < assignStates.size()
                                   && assignStates.at(index),
                               index == selectedAssign);
    }
}

ResponsiveSectionArea::ResponsiveSectionArea(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ResponsiveSectionArea");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sectionGrid = new QGridLayout(this);
    sectionGrid->setContentsMargins(0, 0, 0, 0);
    sectionGrid->setHorizontalSpacing(10);
    sectionGrid->setVerticalSpacing(6);
}

void ResponsiveSectionArea::addSection(QWidget *section)
{
    sections.append(section);
    setProperty("twoColumnMinimumWidth", QVariant());
    updateSections();
}

void ResponsiveSectionArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateSections();
}

void ResponsiveSectionArea::updateSections()
{
    int twoColumnMinimum = property("twoColumnMinimumWidth").toInt();
    if (twoColumnMinimum <= 0) {
        int firstColumnMinimum = 0;
        int secondColumnMinimum = 0;
        for (int i = 0; i < sections.size(); ++i) {
            QWidget *section = sections.at(i);
            const int sectionMinimum = qMax(
                section->minimumWidth(), section->minimumSizeHint().width());
            if (i % 2 == 0)
                firstColumnMinimum = qMax(firstColumnMinimum, sectionMinimum);
            else
                secondColumnMinimum = qMax(secondColumnMinimum, sectionMinimum);
        }
        twoColumnMinimum = firstColumnMinimum + secondColumnMinimum
            + sectionGrid->horizontalSpacing();
        setProperty("twoColumnMinimumWidth", twoColumnMinimum);
    }
    // Layout rounding can leave the viewport one or two pixels below the
    // calculated minimum even though both columns fit without compression.
    const int layoutRoundingTolerance = 2;
    const int columns = sections.size() > 1
        && width() + layoutRoundingTolerance >= twoColumnMinimum
        ? 2 : 1;
    if (columns == currentColumns && sectionGrid->count() == sections.size())
        return;
    currentColumns = columns;
    while (sectionGrid->count() > 0)
        delete sectionGrid->takeAt(0);
    for (int i = 0; i < sections.size(); ++i)
        sectionGrid->addWidget(sections.at(i), i / columns, i % columns,
                               Qt::AlignLeft | Qt::AlignTop);
    sectionGrid->setColumnStretch(0, 1);
    sectionGrid->setColumnStretch(1, columns == 2 ? 1 : 0);
    updateGeometry();
}

ParameterSection::ParameterSection(const QString &title, int maximumColumns,
                                   QWidget *parent)
    : QWidget(parent),
      fixedColumns(qMax(1, maximumColumns)),
      compactColumns(fixedColumns),
      compactBreakpoint(0),
      mediumColumns(fixedColumns),
      narrowColumns(fixedColumns),
      mediumBreakpoint(0),
      narrowBreakpoint(0),
      threeLevelResponsive(false),
      currentColumns(0)
{
    setObjectName("ParameterSection");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    QLabel *heading = new QLabel(title.toUpper());
    heading->setObjectName("ParameterSectionTitle");
    layout->addWidget(heading);
    controlGrid = new QGridLayout;
    controlGrid->setContentsMargins(0, 0, 0, 0);
    controlGrid->setHorizontalSpacing(10);
    controlGrid->setVerticalSpacing(10);
    layout->addLayout(controlGrid);
}

void ParameterSection::addControl(QWidget *control)
{
    controls.append(control);
    updateGrid();
}

void ParameterSection::setResponsiveColumns(int narrowColumns, int breakpoint)
{
    compactColumns = qBound(1, narrowColumns, fixedColumns);
    compactBreakpoint = qMax(0, breakpoint);
    threeLevelResponsive = false;
    currentColumns = 0;
    updateGrid();
}

void ParameterSection::setResponsiveColumns(int wideColumns,
                                            int mediumColumnsValue,
                                            int narrowColumnsValue,
                                            int mediumBreakpointValue,
                                            int narrowBreakpointValue)
{
    fixedColumns = qMax(1, wideColumns);
    mediumColumns = qBound(1, mediumColumnsValue, fixedColumns);
    narrowColumns = qBound(1, narrowColumnsValue, mediumColumns);
    mediumBreakpoint = qMax(0, mediumBreakpointValue);
    narrowBreakpoint = qBound(0, narrowBreakpointValue,
                              mediumBreakpoint);
    threeLevelResponsive = true;
    currentColumns = 0;
    updateGrid();
}

void ParameterSection::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGrid();
}

void ParameterSection::updateGrid()
{
    int columns = fixedColumns;
    if (threeLevelResponsive) {
        if (width() < narrowBreakpoint)
            columns = narrowColumns;
        else if (width() < mediumBreakpoint)
            columns = mediumColumns;
    } else if (compactBreakpoint > 0 && width() < compactBreakpoint) {
        columns = compactColumns;
    }
    if (columns == currentColumns && controlGrid->count() == controls.size())
        return;
    currentColumns = columns;
    while (controlGrid->count() > 0)
        delete controlGrid->takeAt(0);
    for (int i = 0; i < controls.size(); ++i)
        controlGrid->addWidget(controls.at(i), i / columns,
                               i % columns, Qt::AlignLeft | Qt::AlignTop);
    for (int column = 0; column <= fixedColumns; ++column)
        controlGrid->setColumnStretch(column, 0);
    controlGrid->setColumnStretch(columns, 1);
    updateGeometry();
}

ParameterKnob::ParameterKnob(const QString &label, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ParameterKnob");
    setMinimumWidth(100);
    setMinimumHeight(130);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(0);
    QLabel *title = new QLabel(label.toUpper());
    title->setObjectName("ControlLabel");
    title->setAlignment(Qt::AlignCenter);
    setMinimumWidth(qMax(minimumWidth(), title->sizeHint().width() + 8));
    knob = new AudioGearKnob;
    knob->setNotchesVisible(false);
    knob->setWrapping(false);
    knob->setFixedSize(76, 76);
    value = new QLabel(QString::fromUtf8("—"));
    value->setObjectName("ControlValue");
    value->setAlignment(Qt::AlignCenter);
    value->setMinimumHeight(20);
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(knob, 0, Qt::AlignHCenter);
    layout->addSpacing(8);
    layout->addWidget(value);
}

AudioGearKnob *ParameterKnob::dial() const { return knob; }
QLabel *ParameterKnob::valueLabel() const { return value; }
QSize ParameterKnob::sizeHint() const { return QSize(124, 136); }
QSize ParameterKnob::minimumSizeHint() const { return QSize(108, 130); }

ParameterCombo::ParameterCombo(const QString &label, QWidget *parent)
    : QWidget(parent), title(new QLabel(label.toUpper())),
      labelVisible(true)
{
    setObjectName("ParameterCombo");
    setMinimumSize(130, 64);
    setMaximumWidth(216);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    title->setObjectName("ControlLabel");
    combo = new CompactComboBox;
    layout->addWidget(title);
    layout->addWidget(combo);
}

QComboBox *ParameterCombo::comboBox() const { return combo; }
void ParameterCombo::setLabelVisible(bool visible)
{
    if (labelVisible == visible)
        return;
    labelVisible = visible;
    title->setVisible(visible);
    setMinimumHeight(visible ? 64 : 36);
    updateGeometry();
}
QSize ParameterCombo::sizeHint() const
{ return QSize(166, labelVisible ? 66 : 38); }
QSize ParameterCombo::minimumSizeHint() const
{ return QSize(130, labelVisible ? 64 : 36); }

ParameterToggle::ParameterToggle(const QString &label, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ParameterToggle");
    setMinimumSize(100, 64);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    QLabel *title = new QLabel(label.toUpper());
    title->setObjectName("ControlLabel");
    title->setAlignment(Qt::AlignCenter);
    control = new AudioGearSwitch;
    layout->addWidget(title);
    layout->addWidget(control, 0, Qt::AlignHCenter);
}

AudioGearSwitch *ParameterToggle::toggle() const { return control; }
QSize ParameterToggle::sizeHint() const { return QSize(108, 66); }
QSize ParameterToggle::minimumSizeHint() const { return QSize(100, 64); }

ModernToggleSwitch::ModernToggleSwitch(QWidget *parent)
    : QAbstractButton(parent),
      switchAccent(ModernTheme::color(ModernTheme::EditorAccent)),
      thumbPosition(0.0), thumbAnimation(new QVariantAnimation(this))
{
    setObjectName("ModernToggleSwitch");
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(66, 24);
    thumbAnimation->setDuration(125);
    thumbAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(thumbAnimation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant &value) {
        thumbPosition = value.toReal();
        update();
    });
    connect(this, &QAbstractButton::toggled,
            this, [this](bool checked) { animateThumb(checked); });
}

void ModernToggleSwitch::setAccentColor(const QColor &color)
{
    if (!color.isValid() || switchAccent == color)
        return;
    switchAccent = color;
    update();
}

QColor ModernToggleSwitch::accentColor() const
{
    return switchAccent;
}

void ModernToggleSwitch::setCheckedFromBackend(bool checked)
{
    const QSignalBlocker blocker(this);
    QAbstractButton::setChecked(checked);
    thumbAnimation->stop();
    thumbPosition = checked ? 1.0 : 0.0;
    update();
}

QSize ModernToggleSwitch::sizeHint() const { return QSize(66, 24); }
QSize ModernToggleSwitch::minimumSizeHint() const { return QSize(66, 24); }

void ModernToggleSwitch::animateThumb(bool checked)
{
    thumbAnimation->stop();
    thumbAnimation->setStartValue(thumbPosition);
    thumbAnimation->setEndValue(checked ? 1.0 : 0.0);
    thumbAnimation->start();
}

void ModernToggleSwitch::paintEvent(QPaintEvent *)
{
    // Programmatic refreshes intentionally block signals to avoid writes.
    // Keep the resting thumb position authoritative to the checked state even
    // when the toggled signal (and therefore the animation) was suppressed.
    if (thumbAnimation->state() != QAbstractAnimation::Running)
        thumbPosition = isChecked() ? 1.0 : 0.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track = rect().adjusted(1, 1, -1, -1);
    QColor trackColor;
    if (!isEnabled()) {
        trackColor = QColor(ModernTheme::color(ModernTheme::BorderSubtle));
    } else if (isChecked()) {
        trackColor = switchAccent;
        trackColor.setAlpha(underMouse() ? 235 : 210);
    } else {
        trackColor = QColor(ModernTheme::color(
            underMouse() ? ModernTheme::ElevatedPanel
                         : ModernTheme::ControlBackground));
    }

    painter.setPen(QPen(QColor(ModernTheme::color(
        isChecked() && isEnabled() ? ModernTheme::Border
                                   : ModernTheme::BorderSubtle)), 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2,
                            track.height() / 2);

    const qreal thumbRadius = 8.0;
    const qreal leftCenter = track.left() + thumbRadius + 3.5;
    const qreal rightCenter = track.right() - thumbRadius - 3.5;
    const qreal thumbX = leftCenter
        + thumbPosition * (rightCenter - leftCenter);
    const QPointF thumbCenter(thumbX, track.center().y());

    // State labels are fixed and painted before the thumb. The moving thumb
    // deliberately covers ON at the left end and OFF at the right end.
    QFont stateFont = font();
    stateFont.setPixelSize(8);
    stateFont.setWeight(QFont::Medium);
    painter.setFont(stateFont);
    painter.setPen(QColor(ModernTheme::color(
        isEnabled() ? ModernTheme::PrimaryText
                    : ModernTheme::DisabledText)));
    const QRectF onTextRect(track.left(), track.top(),
                            leftCenter * 2.0 - track.left() * 2.0,
                            track.height());
    const QRectF offTextRect(rightCenter * 2.0 - track.right(), track.top(),
                             track.right() * 2.0 - rightCenter * 2.0,
                             track.height());
    painter.drawText(onTextRect, Qt::AlignCenter, "ON");
    painter.drawText(offTextRect, Qt::AlignCenter, "OFF");

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, isEnabled() ? 82 : 45));
    painter.drawEllipse(thumbCenter + QPointF(0, 1.2),
                        thumbRadius + 0.7, thumbRadius + 0.7);

    QLinearGradient thumbSurface(
        thumbCenter - QPointF(0, thumbRadius),
        thumbCenter + QPointF(0, thumbRadius));
    thumbSurface.setColorAt(0, QColor(isEnabled()
        ? "#F1F3F4" : "#A1A5A9"));
    thumbSurface.setColorAt(1, QColor(isEnabled()
        ? "#BFC4C8" : "#777C81"));
    painter.setPen(QPen(QColor("#08090A"), 0.8));
    painter.setBrush(thumbSurface);
    painter.drawEllipse(thumbCenter, thumbRadius, thumbRadius);

    if (hasFocus()) {
        QColor focus = switchAccent;
        focus.setAlpha(115);
        painter.setPen(QPen(focus, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-0.25, -0.25, 0.25, 0.25),
                                track.height() / 2, track.height() / 2);
    }
}

EffectToggleControl::EffectToggleControl(const QString &label,
                                         QWidget *parent)
    : QWidget(parent), control(new ModernToggleSwitch)
{
    setObjectName("EffectToggleControl");
    setMinimumSize(100, 55);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    QLabel *title = new QLabel(label.toUpper());
    title->setObjectName("ControlLabel");
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(title);
    layout->addWidget(control, 0, Qt::AlignLeft);
}

ModernToggleSwitch *EffectToggleControl::toggle() const { return control; }
QSize EffectToggleControl::sizeHint() const { return QSize(112, 57); }
QSize EffectToggleControl::minimumSizeHint() const { return QSize(100, 55); }

AudioGearPanel::AudioGearPanel(QWidget *parent)
    : QFrame(parent),
      panelAccent(ModernTheme::color(ModernTheme::AccentCyan)),
      panelSelected(false), panelEnabled(true)
{ setAttribute(Qt::WA_StyledBackground, false); }

void AudioGearPanel::setPanelAccent(const QColor &c) { panelAccent = c; update(); }
void AudioGearPanel::setPanelSelected(bool v) { panelSelected = v; update(); }
void AudioGearPanel::setPanelEnabled(bool v) { panelEnabled = v; update(); }

void AudioGearPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 105));
    p.drawRoundedRect(rect().adjusted(5, 7, -3, -1), 5, 5);
    const QRectF body = rect().adjusted(2, 2, -2, -5);
    QLinearGradient face(body.topLeft(), body.bottomRight());
    face.setColorAt(0, panelAccent.darker(panelEnabled ? 300 : 520));
    face.setColorAt(.52, panelAccent.darker(panelEnabled ? 480 : 650));
    face.setColorAt(1, QColor(ModernTheme::color(ModernTheme::ApplicationBackground)));
    p.setPen(QPen(panelSelected
        ? QColor(ModernTheme::color(ModernTheme::AccentCyan))
        : QColor(ModernTheme::color(ModernTheme::Border)),
        panelSelected ? 1.35 : 1));
    p.setBrush(face); p.drawRoundedRect(body, 5, 5);
    QLinearGradient sheen(body.topLeft(), body.topRight());
    sheen.setColorAt(0, QColor(255,255,255,2)); sheen.setColorAt(.5, QColor(255,255,255,14)); sheen.setColorAt(1, QColor(255,255,255,2));
    p.setPen(Qt::NoPen); p.setBrush(sheen);
    p.drawRoundedRect(body.adjusted(2,2,-2,-body.height()*.72), 4, 4);
    QColor accent = panelAccent; accent.setAlpha(panelEnabled ? 175 : 42);
    p.setBrush(accent); p.drawRoundedRect(QRectF(body.left()+13, body.top()+7, body.width()-26, 3), 1.5, 1.5);
    drawScrew(p, body.topLeft()+QPointF(9,9)); drawScrew(p, body.topRight()+QPointF(-9,9));
    drawScrew(p, body.bottomLeft()+QPointF(9,-9)); drawScrew(p, body.bottomRight()+QPointF(-9,-9));
    paintPanelDetails(p, body);
}
void AudioGearPanel::paintPanelDetails(QPainter &, const QRectF &) {}

AudioGearKnob::AudioGearKnob(QWidget *parent) : QDial(parent) {}
void AudioGearKnob::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const qreal side=qMin(width(),height()); const QPointF c(width()/2.0,height()/2.0);
    const qreal ratio=maximum()==minimum()?0.0:qreal(value()-minimum())/qreal(maximum()-minimum());
    const QColor accent=isEnabled()
        ? QColor(ModernTheme::color(ModernTheme::EditorAccent))
        : QColor(ModernTheme::color(ModernTheme::DisabledText));
    for(int i=0;i<=12;++i){ const qreal a=(225.0-270.0*i/12.0)*kPi/180.0;
        p.setPen(QPen(QColor("#43505B"),i%3==0?1.4:.8));
        p.drawLine(QPointF(c.x()+std::cos(a)*side*.445,c.y()-std::sin(a)*side*.445),QPointF(c.x()+std::cos(a)*side*.485,c.y()-std::sin(a)*side*.485)); }
    const QRectF arc(c.x()-side*.41,c.y()-side*.41,side*.82,side*.82);
    p.setPen(QPen(QColor("#26303A"),side*.045,Qt::SolidLine,Qt::RoundCap)); p.drawArc(arc,225*16,-270*16);
    p.setPen(QPen(accent,side*.045,Qt::SolidLine,Qt::RoundCap)); p.drawArc(arc,225*16,int(-270.0*ratio*16));
    const qreal r=side*.305; const QRectF knob(c.x()-r,c.y()-r,r*2,r*2);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0,0,0,150)); p.drawEllipse(knob.translated(0,side*.045));
    QRadialGradient ring(c-QPointF(side*.08,side*.1),r*1.6); ring.setColorAt(0,QColor("#B2B9BF")); ring.setColorAt(.28,QColor("#4D5862")); ring.setColorAt(.38,QColor("#12171C")); ring.setColorAt(1,QColor("#020304"));
    p.setPen(QPen(QColor("#707A83"),1)); p.setBrush(ring); p.drawEllipse(knob);
    QRadialGradient face(c-QPointF(side*.06,side*.08),r); face.setColorAt(0,QColor("#47515A")); face.setColorAt(.5,QColor("#171C21")); face.setColorAt(1,QColor("#040506"));
    p.setPen(QPen(QColor("#050709"),1)); p.setBrush(face); p.drawEllipse(knob.adjusted(side*.065,side*.065,-side*.065,-side*.065));
    const qreal a=(225.0-270.0*ratio)*kPi/180.0; const QPointF marker(c.x()+std::cos(a)*r*.60,c.y()-std::sin(a)*r*.60);
    p.setPen(QPen(accent,side*.035,Qt::SolidLine,Qt::RoundCap)); p.drawLine(c,marker);
    p.setPen(Qt::NoPen); p.setBrush(isEnabled()?QColor("#BCEEFF"):QColor("#59636D")); p.drawEllipse(marker,side*.022,side*.022);
}

AudioGearLed::AudioGearLed(QWidget *parent)
    : QWidget(parent),
      color(ModernTheme::color(ModernTheme::ActiveGreen)), lit(false)
{ setFixedSize(20,20); }
void AudioGearLed::setLedColor(const QColor &c){color=c;update();}
void AudioGearLed::setOn(bool v){lit=v;update();}
void AudioGearLed::paintEvent(QPaintEvent *)
{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QPointF c(width()/2.0,height()/2.0);
    if(lit){QRadialGradient g(c,7);QColor h=color;h.setAlpha(36);g.setColorAt(0,h);g.setColorAt(1,Qt::transparent);p.setPen(Qt::NoPen);p.setBrush(g);p.drawEllipse(c,7,7);}
    QRadialGradient lens(c-QPointF(1.5,2),6);lens.setColorAt(0,lit?color.lighter(180):QColor("#4B545C"));lens.setColorAt(.45,lit?color:QColor("#242A30"));lens.setColorAt(1,QColor("#080A0C"));
    p.setPen(QPen(QColor("#050708"),1));p.setBrush(lens);p.drawEllipse(c,5.5,5.5);
}

AudioGearSwitch::AudioGearSwitch(QWidget *parent):QPushButton(parent),active(false){setFixedSize(58,36);setStyleSheet("background:transparent;border:none;");}
void AudioGearSwitch::setOn(bool v){active=v;update();}
void AudioGearSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QPointF c(width()/2.0,13);
    QRadialGradient ring(c-QPointF(2,2),12);ring.setColorAt(0,QColor("#D0D4D7"));ring.setColorAt(.35,QColor("#737B82"));ring.setColorAt(1,QColor("#14181C"));
    p.setPen(QPen(QColor("#050607"),1));p.setBrush(ring);p.drawEllipse(c,10,10);p.setBrush(active?QColor("#26342C"):QColor("#171B1F"));p.drawEllipse(c,6,6);
    p.setPen(QPen(active
        ? QColor(ModernTheme::color(ModernTheme::ActiveGreen))
        : QColor(ModernTheme::color(ModernTheme::SecondaryText)), 1));p.setFont(QFont("Helvetica Neue",8,QFont::DemiBold));p.drawText(QRectF(0,25,width(),10),Qt::AlignCenter,active?"ON":"OFF");
}

SignalConnector::SignalConnector(Direction d,QWidget *parent):QWidget(parent),connectorDirection(d){setFixedSize(42,76);}
void SignalConnector::setCompactWidth(int w){setFixedSize(w,76);update();}
void SignalConnector::paintEvent(QPaintEvent *)
{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#8D98A5"),1.5));p.setFont(QFont("Helvetica Neue",9,QFont::DemiBold));p.drawText(QRectF(0,4,width(),16),Qt::AlignCenter,connectorDirection==Input?"IN":"OUT");
    const qreal y=height()/2.0;p.setPen(QPen(QColor("#080A0D"),8,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(7,y),QPointF(width()-7,y));p.setPen(QPen(QColor("#78848E"),2,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(7,y),QPointF(width()-7,y));
    p.setBrush(QColor("#11171D"));p.setPen(QPen(QColor("#65717C"),1));const QPointF jack(connectorDirection==Input?width()-7:7,y);p.drawEllipse(jack,5,5);
}

SignalChainPanel::SignalChainPanel(QWidget *parent):QFrame(parent){setObjectName("SignalChain");}
void SignalChainPanel::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
}

SignalChainContent::SignalChainContent(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SignalChainContent");
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void SignalChainContent::setDragHandler(const DragHandler &handler)
{ chainDragHandler = handler; }

void SignalChainContent::setDragLeaveHandler(const DragLeaveHandler &handler)
{ chainDragLeaveHandler = handler; }

void SignalChainContent::setDragFeedback(const QRect &regionRect,
                                         const QLineF &insertionLine,
                                         bool valid)
{
    dragRegionRect = regionRect;
    dragInsertionLine = insertionLine;
    dragFeedbackActive = true;
    dragFeedbackValid = valid;
    update();
}

void SignalChainContent::setParallelCableGeometry(
    QWidget *split, QWidget *merge, qreal pathAY, qreal pathBY)
{
    parallelSplitAnchor = split;
    parallelMergeAnchor = merge;
    parallelPathAY = pathAY;
    parallelPathBY = pathBY;
    update();
}

void SignalChainContent::clearDragFeedback()
{
    dragFeedbackActive = false;
    dragFeedbackValid = false;
    update();
}

void SignalChainContent::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // The drag indicator is repainted across changing regions. Clear the
    // complete opaque surface first so no cable/preview pixels survive from
    // the preceding frame on raster-backed Retina displays.
    p.fillRect(rect(), QColor(ModernTheme::color(
        ModernTheme::ControlBackground)));
    p.setRenderHint(QPainter::Antialiasing);
    const qreal y = height() / 2.0;

    // Region feedback stays behind the topology and is deliberately subtle.
    if (dragFeedbackActive) {
        QColor outline(ModernTheme::color(ModernTheme::AccentCyan));
        outline.setAlpha(dragFeedbackValid ? 54 : 22);
        QColor fill = outline;
        fill.setAlpha(dragFeedbackValid ? 4 : 1);
        p.setPen(QPen(outline, 1.0));
        p.setBrush(fill);
        p.drawRoundedRect(dragRegionRect.adjusted(1, 1, -1, -1), 5, 5);
    }

    const auto drawCable = [&p](const QPointF &from, const QPointF &to,
                                const QBrush &brush) {
        p.setPen(QPen(QColor(0, 0, 0, 175), 7, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(from + QPointF(0, 3), to + QPointF(0, 3));
        p.setPen(QPen(brush, 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(from, to);
    };
    QLinearGradient commonCable(18, y, width() - 18, y);
    commonCable.setColorAt(0, QColor("#394550"));
    commonCable.setColorAt(.5, QColor("#9AA5AE"));
    commonCable.setColorAt(1, QColor("#394550"));
    if (parallelSplitAnchor && parallelMergeAnchor
        && parallelPathAY >= 0.0 && parallelPathBY >= 0.0) {
        // The painter works in SignalChainContent coordinates. Resolve both
        // junction centers into that same space for every paint; QScrollArea
        // may resize this content after a prior layout calculation.
        const qreal splitX = parallelSplitAnchor->mapTo(
            this, parallelSplitAnchor->rect().center()).x();
        const qreal mergeX = parallelMergeAnchor->mapTo(
            this, parallelMergeAnchor->rect().center()).x();
        drawCable(QPointF(24, y), QPointF(splitX, y), QBrush(commonCable));
        drawCable(QPointF(mergeX, y), QPointF(width() - 24, y),
                  QBrush(commonCable));
        QLinearGradient pathCable(splitX, 0, mergeX, 0);
        pathCable.setColorAt(0, QColor("#56636E"));
        pathCable.setColorAt(.5, QColor("#AAB3BA"));
        pathCable.setColorAt(1, QColor("#56636E"));
        const auto drawParallelRoute = [&p, splitX, mergeX, y, &pathCable](
                                           qreal pathY) {
            QPainterPath route;
            route.moveTo(splitX, y);
            route.lineTo(splitX, pathY);
            route.lineTo(mergeX, pathY);
            route.lineTo(mergeX, y);

            QTransform shadowTransform;
            shadowTransform.translate(0, 3);
            p.setPen(QPen(QColor(0, 0, 0, 175), 7, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(shadowTransform.map(route));
            p.setPen(QPen(QBrush(pathCable), 3, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(route);
        };
        drawParallelRoute(parallelPathAY);
        drawParallelRoute(parallelPathBY);
    } else {
        drawCable(QPointF(24, y), QPointF(width() - 24, y),
                  QBrush(commonCable));
    }

    // The insertion marker is the primary destination feedback.
    if (dragFeedbackActive && dragFeedbackValid) {
        QColor insertion(ModernTheme::color(ModernTheme::AccentCyan));
        insertion.setAlpha(235);
        p.setPen(QPen(insertion, 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(dragInsertionLine);
        p.setPen(Qt::NoPen);
        p.setBrush(insertion);
        p.drawEllipse(dragInsertionLine.pointAt(.5), 3.2, 3.2);
    }
}

namespace {
const char *chainModuleMime = "application/x-gtlab-signal-chain-module";

int draggedModuleId(const QMimeData *mime)
{
    if (!mime || !mime->hasFormat(chainModuleMime))
        return -1;
    bool ok = false;
    const int id = QString::fromLatin1(mime->data(chainModuleMime)).toInt(&ok);
    return ok ? id : -1;
}
}

void SignalChainContent::dragEnterEvent(QDragEnterEvent *event)
{
    if (draggedModuleId(event->mimeData()) >= 0)
        event->acceptProposedAction();
    else
        event->ignore();
}

void SignalChainContent::dragMoveEvent(QDragMoveEvent *event)
{
    const int moduleId = draggedModuleId(event->mimeData());
    const bool accepted = moduleId >= 0 && chainDragHandler
        && chainDragHandler(moduleId, event->pos(), false);
    if (accepted)
        event->acceptProposedAction();
    else
        event->ignore();
}

void SignalChainContent::dragLeaveEvent(QDragLeaveEvent *event)
{
    clearDragFeedback();
    if (chainDragLeaveHandler)
        chainDragLeaveHandler();
    event->accept();
}

void SignalChainContent::dropEvent(QDropEvent *event)
{
    const int moduleId = draggedModuleId(event->mimeData());
    const bool accepted = moduleId >= 0 && chainDragHandler
        && chainDragHandler(moduleId, event->pos(), true);
    clearDragFeedback();
    if (chainDragLeaveHandler)
        chainDragLeaveHandler();
    if (accepted)
        event->acceptProposedAction();
    else
        event->ignore();
}

SignalJunction::SignalJunction(Kind kind, QWidget *parent)
    : QPushButton(parent), junctionSelected(false), junctionPathOffset(46.0)
{
    Q_UNUSED(kind);
    setFixedSize(54, 168);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet("background:transparent;border:none;");
}
void SignalJunction::setCompactWidth(int w){setFixedWidth(w);update();}
void SignalJunction::setChainGeometry(int h, qreal pathOffset)
{
    setFixedHeight(qMax(1, h));
    junctionPathOffset = qMax<qreal>(1.0, pathOffset);
    update();
}
void SignalJunction::setSelected(bool selected)
{
    junctionSelected = selected;
    update();
}

void SignalJunction::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    update();
}

void SignalJunction::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    update();
}

void SignalJunction::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal x = width() / 2.0;
    const qreal centerY = height() / 2.0;

    QColor nodeOutline("#17C7E8");
    nodeOutline.setAlpha(junctionSelected ? 245
        : isDown() ? 235 : underMouse() ? 205 : 150);
    QColor nodeFill("#101820");
    if (underMouse())
        nodeFill = nodeFill.lighter(isDown() ? 132 : 118);
    p.setPen(QPen(nodeOutline,
                  junctionSelected || isDown() ? 2.0 : 1.5));
    p.setBrush(nodeFill);
    p.drawEllipse(QPointF(x, centerY),
                  junctionSelected || underMouse() ? 6.5 : 6.0,
                  junctionSelected || underMouse() ? 6.5 : 6.0);

}

SignalChainModule::SignalChainModule(const QString &name, const QColor &accent,
                                     const QColor &faceColor,
                                     QWidget *parent)
    : QPushButton(parent), moduleName(name), moduleAccent(accent),
      moduleFaceColor(faceColor),
      stateAvailable(false), stateOn(false), structuralModule(false),
      moduleSelected(false),
      moduleNavigable(false), moduleMovable(false), modulePending(false),
      dragStarted(false), stableModuleId(-1)
{
    setFixedSize(96, 78);
    setCursor(Qt::ArrowCursor);
    setStyleSheet("background:transparent;border:none;");
    setFocusPolicy(Qt::NoFocus);
}

void SignalChainModule::setEffectState(bool available, bool on)
{
    stateAvailable = available;
    stateOn = available && on;
    update();
}

void SignalChainModule::setStructural(bool structural)
{
    structuralModule = structural;
    update();
}

void SignalChainModule::setSelected(bool selected)
{
    moduleSelected = selected;
    update();
}

void SignalChainModule::setNavigable(bool navigable)
{
    moduleNavigable = navigable;
    setEnabled(moduleNavigable || moduleMovable);
    setCursor(moduleMovable ? Qt::OpenHandCursor
                            : navigable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void SignalChainModule::setMovable(bool movable, int moduleId)
{
    moduleMovable = movable;
    stableModuleId = moduleId;
    setEnabled(moduleNavigable || moduleMovable);
    setCursor(moduleMovable ? Qt::OpenHandCursor
                            : moduleNavigable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void SignalChainModule::setPending(bool pending)
{ modulePending = pending; update(); }

void SignalChainModule::setCompactWidth(int w)
{
    setFixedSize(w, qBound(58, int(w * .78), 78));
    update();
}

void SignalChainModule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (modulePending)
        p.setOpacity(.66);
    const QRectF body = rect().adjusted(2, 2, -2, -3);

    if (moduleSelected) {
        QColor edgeLight(ModernTheme::color(ModernTheme::AccentCyanDim));
        edgeLight.setAlpha(24);
        p.setPen(QPen(edgeLight, 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body.adjusted(1, 1, -1, -1), 7, 7);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRoundedRect(body.translated(0, 2), 6, 6);

    QLinearGradient surface(body.topLeft(), body.bottomLeft());
    const bool visuallyPresent = stateAvailable || structuralModule;
    QColor faceTop = moduleFaceColor;
    QColor faceMiddle = moduleFaceColor.darker(
        stateOn ? 108 : visuallyPresent ? 125 : 145);
    QColor faceBottom = moduleFaceColor.darker(
        stateOn ? 145 : visuallyPresent ? 165 : 185);
    if (stateOn) faceTop = faceTop.lighter(112);
    surface.setColorAt(0, faceTop);
    surface.setColorAt(.52, faceMiddle);
    surface.setColorAt(1, faceBottom);
    QColor categoryOutline = moduleAccent.darker(185);
    categoryOutline.setAlpha(visuallyPresent ? 145 : 90);
    QColor outline = moduleSelected
        ? QColor(ModernTheme::color(ModernTheme::AccentCyan))
        : categoryOutline;
    p.setPen(QPen(outline, moduleSelected ? 1.25 : 1.0));
    p.setBrush(surface);
    p.drawRoundedRect(body, 6, 6);

    QColor accent = moduleAccent;
    accent.setAlpha(moduleSelected ? 235 : stateOn ? 220
        : visuallyPresent ? 165 : 130);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawRoundedRect(QRectF(body.left() + 8, body.top() + 5,
                             body.width() - 16, 2.5), 1.25, 1.25);

    QColor detail = accent;
    detail.setAlpha(qMax(70, accent.alpha() - 35));
    p.setBrush(detail);
    p.drawRoundedRect(QRectF(body.left() + 5, body.top() + 13, 2, 13), 1, 1);
    p.drawRoundedRect(QRectF(body.right() - 7, body.top() + 13, 2, 13), 1, 1);

    const int namePointSize = width() < 62 ? 7 : width() < 78 ? 8 : 9;
    p.setFont(QFont("Helvetica Neue", namePointSize, QFont::DemiBold));
    QColor nameColor(ModernTheme::color(ModernTheme::PrimaryText));
    nameColor.setAlpha(moduleSelected ? 255 : stateOn ? 250
        : visuallyPresent ? 225 : 190);
    p.setPen(nameColor);
    p.drawText(QRectF(body.left() + 4, body.top() + 11,
                      body.width() - 8, 15), Qt::AlignCenter, moduleName);

    if (structuralModule) {
        const qreal arrowY = body.bottom() - 22;
        const qreal arrowLeft = body.left() + qMax<qreal>(11, body.width() * .20);
        const qreal arrowRight = body.right() - qMax<qreal>(11, body.width() * .20);
        QColor arrowColor = moduleAccent;
        arrowColor.setAlpha(185);
        p.setPen(QPen(arrowColor, 1.7, Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(arrowLeft, arrowY),
                   QPointF(arrowRight, arrowY));
        p.drawLine(QPointF(arrowRight - 5, arrowY - 4),
                   QPointF(arrowRight, arrowY));
        p.drawLine(QPointF(arrowRight - 5, arrowY + 4),
                   QPointF(arrowRight, arrowY));

        p.setFont(QFont("Helvetica Neue", 7, QFont::DemiBold));
        QColor roleColor(ModernTheme::color(ModernTheme::SecondaryText));
        roleColor.setAlpha(175);
        p.setPen(roleColor);
        p.drawText(QRectF(body.left(), body.bottom() - 13,
                          body.width(), 10),
                   Qt::AlignCenter, "DIGITAL");
        if (modulePending) {
            p.setOpacity(1.0);
            QColor pendingColor(ModernTheme::color(ModernTheme::AccentCyan));
            pendingColor.setAlpha(220);
            p.setPen(Qt::NoPen);
            p.setBrush(pendingColor);
            p.drawEllipse(QPointF(body.right() - 8, body.top() + 8), 2.5, 2.5);
        }
        return;
    }

    const QPointF ledCenter(body.center().x(), body.bottom() - 23);
    if (stateOn) {
        QRadialGradient glow(ledCenter, 7);
        QColor glowColor(ModernTheme::color(ModernTheme::ActiveGreen));
        glowColor.setAlpha(30);
        glow.setColorAt(0, glowColor);
        glowColor.setAlpha(0);
        glow.setColorAt(1, glowColor);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(ledCenter, 7, 7);
    }
    QRadialGradient lens(ledCenter - QPointF(1, 1), 5);
    lens.setColorAt(0, stateOn ? QColor("#9BFFD0") : QColor("#555E66"));
    lens.setColorAt(.45, stateOn
        ? QColor(ModernTheme::color(ModernTheme::ActiveGreen))
        : QColor("#252B30"));
    lens.setColorAt(1, QColor("#080A0C"));
    p.setPen(QPen(QColor("#050708"), 1));
    p.setBrush(lens);
    p.drawEllipse(ledCenter, 4.5, 4.5);

    p.setFont(QFont("Helvetica Neue", 8, QFont::DemiBold));
    p.setPen(stateOn
        ? QColor(ModernTheme::color(ModernTheme::ActiveGreen))
        : QColor(ModernTheme::color(ModernTheme::DisabledText)));
    const QString stateText = stateAvailable ? (stateOn ? "ON" : "OFF")
                                               : QString::fromUtf8("—");
    p.drawText(QRectF(body.left(), body.bottom() - 14,
                      body.width(), 12), Qt::AlignCenter, stateText);

    if (modulePending) {
        p.setOpacity(1.0);
        QColor pendingColor(ModernTheme::color(ModernTheme::AccentCyan));
        pendingColor.setAlpha(220);
        p.setPen(Qt::NoPen);
        p.setBrush(pendingColor);
        p.drawEllipse(QPointF(body.right() - 8, body.top() + 8), 2.5, 2.5);
    }
}

void SignalChainModule::mousePressEvent(QMouseEvent *event)
{
    dragStarted = false;
    dragPressPosition = event->pos();
    QPushButton::mousePressEvent(event);
}

void SignalChainModule::mouseMoveEvent(QMouseEvent *event)
{
    if (!moduleMovable || !(event->buttons() & Qt::LeftButton)
        || (event->pos() - dragPressPosition).manhattanLength()
            < QApplication::startDragDistance()) {
        QPushButton::mouseMoveEvent(event);
        return;
    }

    dragStarted = true;
    setDown(false);
    QDrag *drag = new QDrag(this);
    QMimeData *mime = new QMimeData;
    mime->setData(chainModuleMime, QByteArray::number(stableModuleId));
    drag->setMimeData(mime);

    QPixmap ghost(86, 44);
    ghost.fill(Qt::transparent);
    QPainter painter(&ghost);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor face = moduleFaceColor;
    face.setAlpha(225);
    painter.setPen(QPen(moduleAccent, 1.2));
    painter.setBrush(face);
    painter.drawRoundedRect(QRectF(1, 1, 83, 40), 6, 6);
    painter.setPen(QColor(ModernTheme::color(ModernTheme::PrimaryText)));
    painter.setFont(QFont("Helvetica Neue", 9, QFont::DemiBold));
    painter.drawText(ghost.rect(), Qt::AlignCenter, moduleName);
    drag->setPixmap(ghost);
    drag->setHotSpot(QPoint(ghost.width() / 2, ghost.height() / 2));
    drag->exec(Qt::MoveAction);
}

void SignalChainModule::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragStarted) {
        dragStarted = false;
        setDown(false);
        event->accept();
        return;
    }
    QPushButton::mouseReleaseEvent(event);
}

StatusBadge::StatusBadge(QWidget *parent):QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setTextFormat(Qt::RichText);
    setConnected(false);
}
void StatusBadge::setConnected(bool v)
{
    const QString indicator = ModernTheme::color(
        v ? ModernTheme::ActiveGreen : ModernTheme::DangerRed);
    const QString label = v ? "GT-10 CONNECTED" : "NOT CONNECTED";
    setText(QString(
        "<span style='color:%1'>●</span>&nbsp;&nbsp;"
        "<span style='color:%2'>%3</span>")
        .arg(indicator,
             ModernTheme::color(ModernTheme::PrimaryText), label));
    setStyleSheet(
        "background:transparent;border:none;padding:2px 3px;"
        "font-size:11px;font-weight:600;");
}

EffectModule::EffectModule(const QString &name,const QString &accent,bool available,VisualKind kind,QWidget *parent)
    :AudioGearPanel(parent),effectName(name),accentColor(accent),effectAvailable(available),effectOn(false),valuesValid(false),leftValue(0),rightValue(0),visualKind(kind)
{
    setObjectName("EffectModule");setPanelAccent(QColor(accent));setPanelEnabled(available);
    const int minimumWidth = effectName == "PREAMP" ? 122
        : (kind == Equalizer ? 116 : ((effectName == "FX-1" || effectName == "FX-2") ? 100 : 106));
    setMinimumWidth(minimumWidth);setMaximumWidth(minimumWidth + 38);setFixedHeight(172);
    QVBoxLayout *layout=new QVBoxLayout(this);layout->setContentsMargins(12,15,12,9);layout->setSpacing(3);
    nameLabel=new QLabel(name);nameLabel->setAlignment(Qt::AlignCenter);typeLabel=new QLabel(available?QString::fromUtf8("—"):"Unavailable");typeLabel->setAlignment(Qt::AlignCenter);
    QHBoxLayout *labels=new QHBoxLayout;leftLabel=new QLabel;rightLabel=new QLabel;leftLabel->setAlignment(Qt::AlignCenter);rightLabel->setAlignment(Qt::AlignCenter);labels->addWidget(leftLabel);labels->addWidget(rightLabel);
    led=new AudioGearLed;button=new AudioGearSwitch;button->setEnabled(available);
    layout->addWidget(nameLabel);layout->addWidget(typeLabel);layout->addSpacing(48);layout->addLayout(labels);layout->addStretch();layout->addWidget(led,0,Qt::AlignHCenter);layout->addWidget(button,0,Qt::AlignHCenter);
    configureControlLabels();updateAppearance();
}
void EffectModule::configureControlLabels(){QString l,r;if(effectName=="COMP"){l="SUSTAIN";r="LEVEL";}else if(effectName=="OD/DS"){l="DRIVE";r="LEVEL";}else if(effectName=="PREAMP"){l="GAIN";r="LEVEL";}else if(effectName=="FX-1"||effectName=="FX-2"){l="RATE";r="DEPTH";}else if(effectName=="DELAY"){l="TIME";r="FEEDBACK";}else if(effectName=="REVERB"){l="EFFECT";r="DIRECT";}leftLabel->setText(l);rightLabel->setText(r);}
QPushButton *EffectModule::actionButton()const{return button;}
void EffectModule::setEffectState(bool available,bool on){effectAvailable=available;effectOn=on;setPanelEnabled(available);button->setEnabled(available);button->setOn(on);led->setOn(available&&on);if(!available){typeLabel->setText(QString::fromUtf8("—"));valuesValid=false;}updateAppearance();}
void EffectModule::setSelected(bool v){setPanelSelected(v);}
void EffectModule::setTypeText(const QString &v){typeLabel->setText(v.isEmpty()?QString::fromUtf8("—"):v);}
void EffectModule::setControlValues(int l,int r,bool valid){leftValue=l;rightValue=r;valuesValid=valid;update();}
void EffectModule::setCompact(bool compact){if(!compact)return;setMinimumWidth(92);setMaximumWidth(108);setFixedHeight(126);typeLabel->hide();leftLabel->hide();rightLabel->hide();button->setFixedSize(50,31);QLayout *l=layout();if(l)l->setContentsMargins(10,13,10,7);}
void EffectModule::paintPanelDetails(QPainter &p,const QRectF &body){const QRectF area(body.left()+14,body.top()+48,body.width()-28,48);if(visualKind==Equalizer)paintEqualizer(p,area);else{paintMiniKnob(p,QPointF(area.left()+area.width()*.27,area.center().y()),17,leftValue);paintMiniKnob(p,QPointF(area.left()+area.width()*.73,area.center().y()),17,rightValue);}}
void EffectModule::paintMiniKnob(QPainter &p,const QPointF &c,qreal r,int value)const{const bool valid=effectAvailable&&valuesValid;const qreal ratio=qBound(0,value,100)/100.0;p.setPen(QPen(QColor(valid?accentColor:"#303942"),2.4,Qt::SolidLine,Qt::RoundCap));p.drawArc(QRectF(c.x()-r-3,c.y()-r-3,(r+3)*2,(r+3)*2),225*16,int(-270.0*(valid?ratio:0)*16));QRadialGradient face(c-QPointF(4,5),r*1.4);face.setColorAt(0,valid?QColor("#66717A"):QColor("#343B42"));face.setColorAt(.42,QColor("#22282E"));face.setColorAt(1,QColor("#050709"));p.setPen(QPen(QColor("#68727B"),1));p.setBrush(face);p.drawEllipse(c,r,r);const qreal a=(225.0-270.0*(valid?ratio:.5))*kPi/180.0;p.setPen(QPen(valid?QColor("#BCEEFF"):QColor("#59636D"),2,Qt::SolidLine,Qt::RoundCap));p.drawLine(c,QPointF(c.x()+std::cos(a)*r*.62,c.y()-std::sin(a)*r*.62));}
void EffectModule::paintEqualizer(QPainter &p,const QRectF &area)const{for(int i=0;i<5;++i){const qreal x=area.left()+(i+1)*area.width()/6.0;p.setPen(QPen(QColor("#06080A"),3));p.drawLine(QPointF(x,area.top()+5),QPointF(x,area.bottom()-5));p.setPen(QPen(QColor(effectAvailable?"#52606B":"#323A42"),1));p.setBrush(QColor("#151B21"));p.drawRoundedRect(QRectF(x-4,area.center().y()-3,8,7),2,2);}}
void EffectModule::updateAppearance(){nameLabel->setStyleSheet(QString("background:transparent;color:%1;font-size:13px;font-weight:700;").arg(effectAvailable?accentColor:ModernTheme::color(ModernTheme::DisabledText)));typeLabel->setStyleSheet(QString("background:transparent;color:%1;font-size:9px;").arg(ModernTheme::color(ModernTheme::SecondaryText)));const QString s=QString("background:transparent;color:%1;font-size:8px;font-weight:600;").arg(ModernTheme::color(ModernTheme::SecondaryText));leftLabel->setStyleSheet(s);rightLabel->setStyleSheet(s);led->setLedColor(QColor(ModernTheme::color(ModernTheme::ActiveGreen)));update();}
