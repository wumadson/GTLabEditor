#include "modernTheme.h"

#include <QColor>
#include <QtMath>

namespace {
qreal linearChannel(qreal channel)
{
    channel /= 255.0;
    return channel <= 0.03928
        ? channel / 12.92
        : qPow((channel + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QColor &color)
{
    return 0.2126 * linearChannel(color.red())
        + 0.7152 * linearChannel(color.green())
        + 0.0722 * linearChannel(color.blue());
}
}

QString ModernTheme::color(ColorRole role)
{
    switch (role) {
    case ApplicationBackground: return "#030405";
    case MainSurface: return "#060708";
    case Panel: return "#090A0C";
    case ElevatedPanel: return "#0D0F12";
    case ControlBackground: return "#050607";
    case Border: return "#24272C";
    case BorderSubtle: return "#171A1E";
    case PrimaryText: return "#ECEFF2";
    case SecondaryText: return "#B3B7BC";
    case DisabledText: return "#666B72";
    case AccentCyan: return "#258DB5";
    case AccentCyanHover: return "#36A1C8";
    case AccentCyanDim: return "#18536A";
    case EditorAccent: return "#B4383E";
    case EditorAccentHover: return "#D04A50";
    case EditorAccentDim: return "#3A1518";
    case ActiveGreen: return "#27C768";
    case ActiveGreenDim: return "#105B35";
    case WarningOrange: return "#D98B35";
    case DangerRed: return "#D85C62";
    }
    return QString();
}

int ModernTheme::radius(RadiusRole role)
{
    switch (role) {
    case SmallRadius: return 4;
    case ControlRadius: return 4;
    case PanelRadius: return 4;
    case ContainerRadius: return 5;
    }
    return 0;
}

QString ModernTheme::applicationStyleSheet()
{
    return QStringLiteral(R"(
        QWidget#ModernFloorBoard {
            background: #030405;
            color: #ECEFF2;
            font-family: -apple-system, "SF Pro Display", "Helvetica Neue";
        }
        QFrame#AppHeader {
            background: #050607;
            border-bottom: 1px solid #202329;
        }
        QLabel#BrandTitle { color: #ECEFF2; font-size: 20px; font-weight: 700; }
        QLabel#BrandSubtitle { color: #8C9198; font-size: 10px; font-weight: 600; }
        QLabel#PatchCaption { color: #8C9198; font-size: 9px; font-weight: 700; }
        QLabel#PatchNumber { color: #258DB5; font-size: 15px; font-weight: 700; }
        QLabel#PatchName { color: #ECEFF2; font-size: 15px; font-weight: 600; }
        QLabel#SectionTitle { color: #8C9198; font-size: 11px; font-weight: 600; }
        QFrame#PatchSidebar {
            background: #050607;
            border: none;
            border-right: 1px solid #171A1E;
        }
        QLabel#PatchLibraryTitle {
            color: #ECEFF2;
            font-size: 12px;
            font-weight: 600;
            letter-spacing: 1px;
        }
        QLabel#PatchGroupTitle {
            color: #8C9198;
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 1px;
            padding: 5px 4px 2px 4px;
        }
        QLineEdit#PatchSearch {
            min-height: 34px;
            padding: 0 9px;
            color: #ECEFF2;
            background: #050607;
            border: 1px solid #24272C;
            border-radius: 4px;
            selection-background-color: #16313C;
        }
        QLineEdit#PatchSearch:focus { border-color: #258DB5; }
        QLineEdit#PatchSearch:disabled { color: #666B72; }
        QPushButton#PatchBankHeader {
            min-height: 28px;
            padding: 0 7px;
            text-align: left;
            color: #8C9198;
            background: transparent;
            border: none;
            border-radius: 4px;
            font-size: 9px;
            font-weight: 600;
            letter-spacing: 0.8px;
        }
        QPushButton#PatchBankHeader:hover { background: #0D0F12; color: #ECEFF2; }
        QPushButton#PatchBankHeader[expanded="true"] {
            color: #ECEFF2;
            background: #090A0C;
            border-left: 2px solid #258DB5;
        }
        PatchListItem {
            background: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 4px;
        }
        PatchListItem:hover { background: #0D0F12; }
        PatchListItem[current="true"] {
            background: #16313C;
            border-left-color: #258DB5;
        }
        PatchListItem[pending="true"] {
            background: #090A0C;
            border-left-color: #36A1C8;
        }
        PatchListItem QLabel#PatchItemNumber {
            color: #36A1C8;
            font-size: 9px;
            font-weight: 600;
        }
        PatchListItem QLabel#PatchItemName {
            color: #B3B7BC;
            font-size: 10px;
            font-weight: 400;
        }
        PatchListItem[current="true"] QLabel#PatchItemName {
            color: #ECEFF2;
            font-weight: 600;
        }
        PatchListItem[pending="true"] QLabel#PatchItemName { color: #ECEFF2; }
        QScrollArea#PatchScroll { background: transparent; border: none; }
        QScrollArea#PatchScroll > QWidget > QWidget { background: transparent; }
        QScrollArea#PatchScroll QScrollBar:vertical {
            width: 7px; background: transparent; margin: 0;
        }
        QScrollArea#PatchScroll QScrollBar:horizontal {
            height: 7px; background: transparent; margin: 0;
        }
        QScrollArea#PatchScroll QScrollBar::handle:vertical,
        QScrollArea#PatchScroll QScrollBar::handle:horizontal {
            background: #292D32; border: none; border-radius: 2px; min-height: 28px; min-width: 28px;
        }
        QScrollArea#PatchScroll QScrollBar::handle:hover { background: #3A3F45; }
        QScrollArea#PatchScroll QScrollBar::handle:pressed { background: #4A5057; }
        QScrollArea#PatchScroll QScrollBar::add-line,
        QScrollArea#PatchScroll QScrollBar::sub-line { width: 0; height: 0; background: transparent; border: none; }
        QScrollArea#PatchScroll QScrollBar::add-page,
        QScrollArea#PatchScroll QScrollBar::sub-page { background: transparent; }
        QFrame#PatchSidebar QPushButton:disabled {
            color: #666B72;
            background: #050607;
            border: 1px solid #24272C;
            border-radius: 4px;
        }
        QLabel#EditorTitle { color: #ECEFF2; font-size: 14px; font-weight: 700; letter-spacing: 1px; }
        QFrame#SignalChain {
            background: #050607;
            border: 1px solid #24272C;
            border-radius: 4px;
        }
        QFrame#EffectEditorPanel {
            background: #08090B;
            border: 1px solid #24272C;
            border-radius: 4px;
        }
        QFrame#EffectArtworkPane,
        QFrame#EffectParameterPane,
        QFrame#EffectModelPane {
            background: #0A0B0D;
            border: none;
        }
        QFrame#EffectParameterPane,
        QFrame#EffectModelPane {
            border-left: 1px solid #24272C;
        }
        QLabel#WorkspaceColumnTitle,
        QLabel#BottomRegionTitle {
            color: #D4D7DB;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#WorkspaceUnavailable {
            color: #666B72;
            font-size: 9px;
            font-weight: 600;
            letter-spacing: 1px;
        }
        QWidget#EffectModelBrowser,
        QListWidget#EffectModelList,
        QListWidget#EffectModelList::item {
            background: transparent;
            border: none;
            outline: none;
        }
        QListWidget#EffectModelList::item:selected,
        QListWidget#EffectModelList::item:hover {
            background: transparent;
            border: none;
        }
        QScrollBar#EffectModelScroll:vertical {
            width: 7px;
            background: transparent;
            margin: 0;
        }
        QScrollBar#EffectModelScroll::handle:vertical {
            background: #292D32;
            border: none;
            border-radius: 2px;
            min-height: 28px;
        }
        QScrollBar#EffectModelScroll::handle:vertical:hover {
            background: #3A3F45;
        }
        QScrollBar#EffectModelScroll::add-line:vertical,
        QScrollBar#EffectModelScroll::sub-line:vertical {
            height: 0;
            background: transparent;
            border: none;
        }
        QScrollBar#EffectModelScroll::up-arrow:vertical,
        QScrollBar#EffectModelScroll::down-arrow:vertical {
            width: 0;
            height: 0;
            background: transparent;
            border: none;
        }
        QScrollBar#EffectModelScroll::add-page:vertical,
        QScrollBar#EffectModelScroll::sub-page:vertical {
            background: transparent;
        }
        QFrame#WorkspaceRule {
            background: #24272C;
            border: none;
        }
        QFrame#BottomControlStrip {
            background: #08090B;
            border: 1px solid #24272C;
            border-radius: 4px;
        }
        QFrame#BottomRegionFirst,
        QFrame#BottomRegion {
            background: transparent;
            border: none;
        }
        QFrame#BottomRegion {
            border-left: 1px solid #24272C;
        }
        QFrame#EditorDivider {
            background: #24272C;
            border: none;
        }
        QFrame#EditorColumnDivider {
            background: #171A1E;
            border: none;
        }
        QWidget#EffectParameterArea,
        QWidget#EffectArtworkArea,
        QWidget#ResponsiveSectionArea,
        QWidget#ParameterSection,
        QWidget#ParameterKnob,
        QWidget#ParameterCombo {
            background: transparent;
            border: none;
        }
        QScrollArea#EffectParameterScroll {
            background: transparent;
            border: none;
        }
        QScrollArea#EffectParameterScroll > QWidget > QWidget { background: transparent; }
        QScrollArea#EffectParameterScroll QScrollBar:vertical {
            width: 7px;
            background: transparent;
            margin: 0;
        }
        QScrollArea#EffectParameterScroll QScrollBar::handle:vertical {
            background: #292D32;
            border: none;
            border-radius: 2px;
            min-height: 28px;
        }
        QScrollArea#EffectParameterScroll QScrollBar::handle:vertical:hover { background: #3A3F45; }
        QScrollArea#EffectParameterScroll QScrollBar::add-line:vertical,
        QScrollArea#EffectParameterScroll QScrollBar::sub-line:vertical {
            height: 0;
            background: transparent;
            border: none;
        }
        QScrollArea#EffectParameterScroll QScrollBar::add-page:vertical,
        QScrollArea#EffectParameterScroll QScrollBar::sub-page:vertical { background: transparent; }
        QLabel#ParameterSectionTitle {
            color: #8C9198;
            font-size: 11px;
            font-weight: 600;
            padding-bottom: 4px;
        }
        QLabel#ControlLabel { color: #D4D7DB; font-size: 10px; font-weight: 600; }
        QLabel#ControlValue { color: #D04A50; font-size: 13px; font-weight: 650; }
        QLabel#EffectTypeDisplay {
            color: #D04A50;
            font-size: 15px;
            font-weight: 650;
        }
        QFrame#EffectEditorPanel QComboBox {
            min-height: 32px;
            padding: 0 8px;
            color: #ECEFF2;
            background: #050607;
            border: 1px solid #24272C;
            border-radius: 4px;
        }
        QFrame#EffectEditorPanel QComboBox:hover { background: #0D0F12; border-color: #34383E; }
        QFrame#EffectEditorPanel QComboBox:focus { border-color: #B4383E; }
        QFrame#EffectEditorPanel QComboBox:disabled { color: #666B72; background: #050607; border-color: #24272C; }
        QFrame#EffectEditorPanel QComboBox QAbstractItemView {
            background: #0D0F12;
            color: #ECEFF2;
            border: 1px solid #24272C;
            selection-background-color: #3A1518;
            outline: none;
        }
        QStatusBar {
            min-height: 24px;
            max-height: 24px;
            background: #050607;
            color: #8C9198;
            border-top: 1px solid #171A1E;
        }
        QWidget#ModernStatusWidget { background: transparent; color: #8C9198; }
        QLabel#StatusMessage { color: #8C9198; font-size: 10px; }
        QLabel#StatusDebug { color: #666B72; font-size: 10px; }
        QLabel#StatusSymbol { color: #666B72; font-size: 10px; }
        QLabel#StatusSymbol[state="ready"] { color: #27C768; }
        QLabel#StatusSymbol[state="busy"] { color: #D98B35; }
        QLabel#ConnectionDot { color: #56606B; font-size: 9px; }
        QLabel#ConnectionDot[state="connected"] { color: #39C66D; }
        QLabel#ConnectionLabel { color: #59636E; font-size: 9px; font-weight: 600; }
        QLabel#ConnectionLabel[state="connected"] { color: #8D99A5; }
        QProgressBar#StatusProgress { background: #050607; border: none; border-radius: 2px; }
        QProgressBar#StatusProgress::chunk { background: #258DB5; border-radius: 2px; }
    )");
}

QString ModernTheme::effectColor(const QString &effectName)
{
    if (effectName == "COMP") return "#39B779";
    if (effectName == "OD/DS") return "#B6A33B";
    if (effectName.startsWith("PREAMP")) return "#E14D4D";
    if (effectName == "EQ") return "#368FD6";
    if (effectName == "FX-1" || effectName == "FX-2") return "#9364C7";
    if (effectName == "DELAY") return "#348BD4";
    if (effectName == "REVERB") return "#18B8AA";
    if (effectName == "CHORUS") return "#537FC7";
    if (effectName == "PEDAL FX") return "#8D63BE";
    if (effectName.startsWith("NS-")) return "#708493";
    if (effectName == "SEND/RETURN") return "#A96F4B";
    if (effectName == "FOOT VOLUME") return "#7B858D";
    if (effectName == "DIGITAL OUT") return "#69747D";
    return "#66717C";
}

QString ModernTheme::activeEffectAccent(const QString &effectName)
{
    const QColor candidate(effectColor(effectName));
    const QColor background(color(ApplicationBackground));
    if (!candidate.isValid() || !background.isValid())
        return color(PrimaryText);

    const qreal candidateLuminance = relativeLuminance(candidate);
    const qreal backgroundLuminance = relativeLuminance(background);
    const qreal lighter = qMax(candidateLuminance, backgroundLuminance);
    const qreal darker = qMin(candidateLuminance, backgroundLuminance);
    const qreal contrastRatio = (lighter + 0.05) / (darker + 0.05);
    return contrastRatio >= 4.5
        ? candidate.name(QColor::HexRgb).toUpper()
        : color(PrimaryText);
}

QString ModernTheme::effectFaceColor(const QString &effectName)
{
    if (effectName == "COMP") return "#12352B";
    if (effectName == "OD/DS") return "#403512";
    if (effectName.startsWith("PREAMP")) return "#421A1D";
    if (effectName == "EQ") return "#12324D";
    if (effectName == "FX-1" || effectName == "FX-2") return "#332047";
    if (effectName == "PEDAL FX") return "#20203A";
    if (effectName == "CHORUS") return "#102A3A";
    if (effectName == "DELAY") return "#10283D";
    if (effectName == "REVERB") return "#0E302B";
    if (effectName.startsWith("NS-")) return "#1A222A";
    if (effectName == "SEND/RETURN") return "#332417";
    if (effectName == "FOOT VOLUME") return "#20252A";
    if (effectName == "DIGITAL OUT") return "#1A2025";
    return "#1B2228";
}
