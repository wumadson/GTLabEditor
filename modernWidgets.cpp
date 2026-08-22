#include "modernWidgets.h"
#include "modernTheme.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QGridLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <cmath>

namespace {
const qreal kPi = 3.14159265358979323846;

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
    parameterScroll->setWidget(parameters);
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

BottomControlStrip::BottomControlStrip(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("BottomControlStrip");
    setMinimumHeight(115);
    setMaximumHeight(130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const QStringList titles = {
        "EXPRESSION", "CONTROL ASSIGN", "PEDALBOARD", "TUNER"
    };
    const int stretches[] = {17, 28, 39, 16};
    for (int i = 0; i < titles.size(); ++i) {
        QFrame *region = new QFrame;
        region->setObjectName(i == 0 ? "BottomRegionFirst" : "BottomRegion");
        QVBoxLayout *regionLayout = new QVBoxLayout(region);
        regionLayout->setContentsMargins(14, 11, 14, 11);
        regionLayout->setSpacing(8);
        QLabel *title = new QLabel(titles.at(i));
        title->setObjectName("BottomRegionTitle");
        regionLayout->addWidget(title);
        QLabel *state = new QLabel("NOT INTEGRATED");
        state->setObjectName("WorkspaceUnavailable");
        state->setAlignment(Qt::AlignCenter);
        regionLayout->addWidget(state, 1);
        layout->addWidget(region, stretches[i]);
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
    : QWidget(parent)
{
    setObjectName("ParameterCombo");
    setMinimumSize(150, 64);
    setMaximumWidth(240);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    QLabel *title = new QLabel(label.toUpper());
    title->setObjectName("ControlLabel");
    combo = new QComboBox;
    layout->addWidget(title);
    layout->addWidget(combo);
}

QComboBox *ParameterCombo::comboBox() const { return combo; }
QSize ParameterCombo::sizeHint() const { return QSize(190, 66); }
QSize ParameterCombo::minimumSizeHint() const { return QSize(150, 64); }

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
    setFixedSize(88, 30);
    thumbAnimation->setDuration(145);
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

QSize ModernToggleSwitch::sizeHint() const { return QSize(88, 30); }
QSize ModernToggleSwitch::minimumSizeHint() const { return QSize(88, 30); }

void ModernToggleSwitch::animateThumb(bool checked)
{
    thumbAnimation->stop();
    thumbAnimation->setStartValue(thumbPosition);
    thumbAnimation->setEndValue(checked ? 1.0 : 0.0);
    thumbAnimation->start();
}

void ModernToggleSwitch::paintEvent(QPaintEvent *)
{
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

    const qreal thumbRadius = 10.5;
    const qreal leftCenter = track.left() + 4 + thumbRadius;
    const qreal rightCenter = track.right() - 4 - thumbRadius;
    const qreal thumbX = leftCenter
        + thumbPosition * (rightCenter - leftCenter);
    const QPointF thumbCenter(thumbX, track.center().y());

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

    QFont stateFont = font();
    stateFont.setPointSizeF(qMax(8.0, stateFont.pointSizeF() - 1.0));
    stateFont.setWeight(QFont::DemiBold);
    painter.setFont(stateFont);
    painter.setPen(QColor(ModernTheme::color(
        isEnabled() ? ModernTheme::PrimaryText
                    : ModernTheme::DisabledText)));
    const QRectF stateRect = isChecked()
        ? QRectF(track.left() + 7, track.top(),
                 rightCenter - thumbRadius - track.left() - 8,
                 track.height())
        : QRectF(leftCenter + thumbRadius + 5, track.top(),
                 track.right() - leftCenter - thumbRadius - 9,
                 track.height());
    painter.drawText(stateRect, Qt::AlignCenter,
                     isChecked() ? "ON" : "OFF");

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
    const qreal y=42;p.setPen(QPen(QColor("#080A0D"),8,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(7,y),QPointF(width()-7,y));p.setPen(QPen(QColor("#78848E"),2,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(7,y),QPointF(width()-7,y));
    p.setBrush(QColor("#11171D"));p.setPen(QPen(QColor("#65717C"),1));const QPointF jack(connectorDirection==Input?width()-7:7,y);p.drawEllipse(jack,5,5);
}

SignalChainPanel::SignalChainPanel(QWidget *parent):QFrame(parent){setObjectName("SignalChain");}
void SignalChainPanel::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const qreal y=height()/2.0+5;
    p.setPen(QPen(QColor(0,0,0,175),7,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(24,y+3),QPointF(width()-24,y+3));
    QLinearGradient cable(18,y,width()-18,y);cable.setColorAt(0,QColor("#394550"));cable.setColorAt(.5,QColor("#9AA5AE"));cable.setColorAt(1,QColor("#394550"));p.setPen(QPen(QBrush(cable),3,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(24,y),QPointF(width()-24,y));
}

SignalJunction::SignalJunction(Kind kind, QWidget *parent)
    : QPushButton(parent), junctionKind(kind), junctionSelected(false)
{
    setFixedSize(54, 176);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet("background:transparent;border:none;");
}
void SignalJunction::setCompactWidth(int w){setFixedSize(w,176);update();}
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
    const qreal pathAY = 42;
    const qreal pathBY = height() - 42;
    const QColor cable("#7E8A94");

    const qreal commonStart = junctionKind == Split ? 0 : x;
    const qreal commonEnd = junctionKind == Split ? x : width();
    const qreal branchStart = junctionKind == Split ? x : 0;
    const qreal branchEnd = junctionKind == Split ? width() : x;
    p.setPen(QPen(QColor(0, 0, 0, 180), 7, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(commonStart, centerY + 3), QPointF(commonEnd, centerY + 3));
    p.drawLine(QPointF(x, pathAY + 3), QPointF(x, pathBY + 3));
    p.drawLine(QPointF(branchStart, pathAY + 3), QPointF(branchEnd, pathAY + 3));
    p.drawLine(QPointF(branchStart, pathBY + 3), QPointF(branchEnd, pathBY + 3));
    p.setPen(QPen(cable, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(commonStart, centerY), QPointF(commonEnd, centerY));
    p.drawLine(QPointF(x, pathAY), QPointF(x, pathBY));
    p.drawLine(QPointF(branchStart, pathAY), QPointF(branchEnd, pathAY));
    p.drawLine(QPointF(branchStart, pathBY), QPointF(branchEnd, pathBY));

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
      moduleNavigable(false)
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
    setEnabled(navigable);
    setCursor(navigable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void SignalChainModule::setCompactWidth(int w)
{
    setFixedSize(w, qBound(58, int(w * .78), 78));
    update();
}

void SignalChainModule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
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
