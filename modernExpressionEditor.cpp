#include "modernExpressionEditor.h"

#include "SysxIO.h"
#include "MidiTable.h"
#include "effectArtworkWidget.h"
#include "modernWidgets.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString rawHex(int raw)
{
    return QString::number(raw, 16).rightJustified(2, '0').toUpper();
}

QLabel *sectionValue()
{
    QLabel *label = new QLabel(QString::fromUtf8("—"));
    label->setObjectName("ExpressionValue");
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
}

ModernExpressionEditor::ModernExpressionEditor(QObject *parent)
    : QObject(parent)
{
    buildEditor();
}

EffectEditorPanel *ModernExpressionEditor::widget() const
{
    return editor;
}

void ModernExpressionEditor::buildEditor()
{
    editor = new EffectEditorPanel(tr("EXPRESSION"));
    editor->typeLabel()->hide();

    EffectArtworkWidget *artwork = new EffectArtworkWidget;
    artwork->setArtwork(":/assets/pedals/expression_pedal.png");
    editor->setArtworkWidget(artwork);

    QWidget *content = editor->parameterArea();
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(createSection("EXP1 PEDAL", {
        {"SCOPE", "00:7A"}, {"SETTING", "01:10"}, {"SYSTEM ROLE", "01:11"},
        {"MIN", "01:13"}, {"MAX", "01:15"}, {"SOURCE MODE", "01:17"},
        {"ACTIVE RANGE LOW", "01:18"}, {"ACTIVE RANGE HIGH", "01:19"}
    }, "System", "00"));

    QFrame *switchSection = static_cast<QFrame *>(createSection(
        "EXP SWITCH", {{"SYSTEM SCOPE", "00:7B"},
        {"SYSTEM FUNCTION", "01:21"}}, "System", "00"));
    QVBoxLayout *switchLayout = qobject_cast<QVBoxLayout *>(switchSection->layout());
    ValueRow patchFunction;
    patchFunction.value = sectionValue();
    patchFunction.area = "Structure";
    patchFunction.bank = "0A";
    patchFunction.middle = "00";
    patchFunction.address = "46";
    rows.append(patchFunction);
    QHBoxLayout *patchRow = new QHBoxLayout;
    QLabel *patchLabel = new QLabel(tr("PATCH FUNCTION"));
    patchLabel->setObjectName("ExpressionFieldLabel");
    patchRow->addWidget(patchLabel);
    patchRow->addStretch(1);
    patchRow->addWidget(patchFunction.value);
    switchLayout->insertLayout(1, patchRow);
    QPushButton *assignLink = new QPushButton(tr("EDIT IN CONTROL ASSIGN"));
    assignLink->setObjectName("ExpressionLinkButton");
    connect(assignLink, &QPushButton::clicked, this,
            [this]() { emit openControlAssignRequested(-1); });
    switchLayout->addWidget(assignLink, 0, Qt::AlignLeft);
    layout->addWidget(switchSection);

    layout->addWidget(createSection("EXP2 PEDAL", {
        {"SCOPE", "00:7E"}, {"SETTING", "01:50"}, {"SYSTEM ROLE", "01:51"},
        {"MIN", "01:53"}, {"MAX", "01:55"}, {"SOURCE MODE", "01:57"},
        {"ACTIVE RANGE LOW", "01:58"}, {"ACTIVE RANGE HIGH", "01:59"}
    }, "System", "00"));

    QFrame *pedal = new QFrame;
    pedal->setObjectName("ExpressionSection");
    QVBoxLayout *pedalLayout = new QVBoxLayout(pedal);
    pedalLayout->setContentsMargins(12, 10, 12, 10);
    pedalLayout->setSpacing(6);
    QLabel *pedalTitle = new QLabel(tr("PEDAL FX / FOOT VOLUME"));
    pedalTitle->setObjectName("ExpressionSectionTitle");
    pedalLayout->addWidget(pedalTitle);
    pedalState = sectionValue();
    pedalMode = sectionValue();
    pedalDetails = sectionValue();
    pedalDetails->setWordWrap(true);
    const QList<QPair<QString, QLabel *> > pedalRows = {
        {"STATE", pedalState}, {"PATCH PEDAL FX", pedalMode},
        {"MODE LIMITS", pedalDetails}
    };
    for (const auto &row : pedalRows) {
        QHBoxLayout *line = new QHBoxLayout;
        QLabel *name = new QLabel(row.first);
        name->setObjectName("ExpressionFieldLabel");
        line->addWidget(name);
        line->addStretch(1);
        line->addWidget(row.second);
        pedalLayout->addLayout(line);
    }
    QPushButton *pedalLink = new QPushButton(tr("EDIT P.FX/FV"));
    pedalLink->setObjectName("ExpressionLinkButton");
    connect(pedalLink, &QPushButton::clicked, this,
            &ModernExpressionEditor::openPedalFxRequested);
    pedalLayout->addWidget(pedalLink, 0, Qt::AlignLeft);
    layout->addWidget(pedal);
    layout->addStretch(1);

    editor->setRightPanelTitle("ASSIGNS USING EXP");
    assignList = new QWidget;
    QVBoxLayout *assignLayout = new QVBoxLayout(assignList);
    assignLayout->setContentsMargins(0, 0, 0, 0);
    assignLayout->setSpacing(6);
    assignEmpty = new QLabel(tr("NO EXP ASSIGNS"));
    assignEmpty->setObjectName("WorkspaceUnavailable");
    assignEmpty->setAlignment(Qt::AlignCenter);
    assignLayout->addWidget(assignEmpty, 1);
    editor->setRightPanelWidget(assignList);

    editor->setStyleSheet(editor->styleSheet() +
        "QFrame#ExpressionSection{background:#0B1117;border:1px solid #27313A;"
        "border-radius:7px;}"
        "QLabel#ExpressionSectionTitle{color:#F2F4F6;font-size:11px;"
        "font-weight:600;letter-spacing:0.7px;}"
        "QLabel#ExpressionFieldLabel{color:#8995A1;font-size:9px;"
        "font-weight:600;letter-spacing:0.5px;}"
        "QLabel#ExpressionValue{color:#DDE4E9;font-size:10px;font-weight:600;}"
        "QPushButton#ExpressionLinkButton{color:#39B8F3;background:#101B23;"
        "border:1px solid #27506A;border-radius:5px;padding:5px 9px;"
        "font-size:9px;font-weight:600;}"
        "QPushButton#ExpressionLinkButton:hover{background:#132734;"
        "border-color:#39B8F3;}"
        "QPushButton#ExpressionAssignButton{text-align:left;color:#DDE4E9;"
        "background:#0B1117;border:1px solid #27313A;border-radius:5px;"
        "padding:7px;font-size:9px;}"
        "QPushButton#ExpressionAssignButton:hover{border-color:#39B8F3;}");
    setUnavailable();
}

QWidget *ModernExpressionEditor::createSection(
    const QString &title, const QList<QPair<QString, QString> > &fields,
    const QString &area, const QString &bank)
{
    QFrame *section = new QFrame;
    section->setObjectName("ExpressionSection");
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    QLabel *heading = new QLabel(title);
    heading->setObjectName("ExpressionSectionTitle");
    layout->addWidget(heading);
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(5);
    for (int index = 0; index < fields.size(); ++index) {
        QLabel *name = new QLabel(fields.at(index).first);
        name->setObjectName("ExpressionFieldLabel");
        QLabel *value = sectionValue();
        grid->addWidget(name, index / 2 * 2, index % 2);
        grid->addWidget(value, index / 2 * 2 + 1, index % 2);
        ValueRow row;
        row.value = value;
        row.area = area;
        row.bank = bank;
        const QStringList addressParts = fields.at(index).second.split(':');
        row.middle = addressParts.size() == 2 ? addressParts.at(0) : "00";
        row.address = addressParts.size() == 2
            ? addressParts.at(1) : fields.at(index).second;
        rows.append(row);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    return section;
}

bool ModernExpressionEditor::bufferContains(
    const QString &area, const QString &bank, const QString &middle,
    const QString &address) const
{
    bool ok = false;
    const int offset = address.toInt(&ok, 16);
    if (!ok)
        return false;
    const SysxData source = area == "System"
        ? SysxIO::Instance()->getSystemSource()
        : SysxIO::Instance()->getFileSource();
    const QString block = bank + middle;
    const int blockIndex = source.address.indexOf(block);
    return blockIndex >= 0 && blockIndex < source.hex.size()
        && source.hex.at(blockIndex).size() > 2 + offset;
}

QString ModernExpressionEditor::displayValue(
    const QString &area, const QString &bank, const QString &middle,
    const QString &address) const
{
    if (!bufferContains(area, bank, middle, address))
        return QString::fromUtf8("—");
    const int raw = SysxIO::Instance()->getSourceValue(
        area, bank, middle, address);
    const QString display = MidiTable::Instance()->getValue(
        area, bank, middle, address, rawHex(raw)).trimmed();
    return display.isEmpty() ? QString::fromUtf8("—") : display;
}

void ModernExpressionEditor::refresh(bool backendConnected,
                                     bool backendHasPatchData)
{
    const bool systemReady = backendConnected
        && bufferContains("System", "00", "00", "7A")
        && bufferContains("System", "00", "01", "59");
    const bool patchReady = backendConnected && backendHasPatchData
        && bufferContains("Structure", "0A", "00", "40")
        && bufferContains("Structure", "0A", "00", "5D");
    available = systemReady || patchReady;

    for (const ValueRow &row : rows) {
        const bool rowReady = row.area == "System" ? systemReady : patchReady;
        row.value->setText(rowReady
            ? displayValue(row.area, row.bank, row.middle, row.address)
            : QString::fromUtf8("—"));
    }
    currentExp1 = systemReady ? displayValue("System", "00", "01", "11")
                              : QString::fromUtf8("—");
    currentExpSwitch = patchReady
        ? displayValue("Structure", "0A", "00", "46") : QString::fromUtf8("—");
    currentExp2 = systemReady ? displayValue("System", "00", "01", "51")
                              : QString::fromUtf8("—");
    refreshPedalSummary(patchReady);
    refreshAssigns(backendConnected && backendHasPatchData);
    emit summaryChanged();
}

void ModernExpressionEditor::refreshPedalSummary(bool ready)
{
    if (!ready) {
        pedalState->setText(QString::fromUtf8("—"));
        pedalMode->setText(QString::fromUtf8("—"));
        pedalDetails->setText(QString::fromUtf8("—"));
        return;
    }
    pedalState->setText(displayValue("Structure", "0A", "00", "40"));
    pedalMode->setText(displayValue("Structure", "0A", "00", "45"));
    const int mode = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", "45");
    QStringList details;
    if (mode == 1 || mode == 4 || mode == 5) {
        details << tr("FV %1 / %2 · %3")
            .arg(displayValue("Structure", "0A", "00", "5B"),
                 displayValue("Structure", "0A", "00", "5C"),
                 displayValue("Structure", "0A", "00", "5D"));
    }
    if (mode == 3 || mode == 5) {
        details << tr("WAH %1 / %2")
            .arg(displayValue("Structure", "0A", "00", "4B"),
                 displayValue("Structure", "0A", "00", "4C"));
    }
    if (mode == 2 || mode == 4) {
        details << tr("PB %1 / %2")
            .arg(displayValue("Structure", "0A", "00", "54"),
                 displayValue("Structure", "0A", "00", "55"));
    }
    pedalDetails->setText(details.isEmpty() ? QString::fromUtf8("—")
                                             : details.join("   "));
}

void ModernExpressionEditor::refreshAssigns(bool ready)
{
    assignModel.refresh(ready, ready);
    currentAssigns.clear();
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(assignList->layout());
    if (!layout)
        return;
    while (layout->count() > 1) {
        QLayoutItem *item = layout->takeAt(1);
        delete item->widget();
        delete item;
    }
    if (ready && assignModel.isAvailable()) {
        for (int index = 0; index < assignModel.count(); ++index) {
            const ModernAssignModel::Record &record = assignModel.record(index);
            if (!record.valid || (record.source != 0x00
                && record.source != 0x03 && record.source != 0x04))
                continue;
            currentAssigns.append(index);
            QPushButton *button = new QPushButton(
            tr("A%1   %2  →  %3")
                    .arg(index + 1).arg(record.sourceDisplay,
                                        record.targetName));
            button->setObjectName("ExpressionAssignButton");
            button->setCursor(Qt::PointingHandCursor);
            connect(button, &QPushButton::clicked, this, [this, index]() {
                emit openControlAssignRequested(index);
            });
            layout->addWidget(button);
        }
    }
    assignEmpty->setVisible(currentAssigns.isEmpty());
    if (!currentAssigns.isEmpty())
        layout->addStretch(1);
}

void ModernExpressionEditor::setUnavailable()
{
    available = false;
    currentExp1 = currentExpSwitch = currentExp2 = QString::fromUtf8("—");
    for (const ValueRow &row : rows)
        row.value->setText(QString::fromUtf8("—"));
    refreshPedalSummary(false);
}

bool ModernExpressionEditor::summaryAvailable() const { return available; }
QString ModernExpressionEditor::exp1Summary() const { return currentExp1; }
QString ModernExpressionEditor::expSwitchSummary() const { return currentExpSwitch; }
QString ModernExpressionEditor::exp2Summary() const { return currentExp2; }
QList<int> ModernExpressionEditor::expressionAssigns() const
{
    return currentAssigns;
}
