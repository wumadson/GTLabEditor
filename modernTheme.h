#ifndef MODERNTHEME_H
#define MODERNTHEME_H

#include <QString>

class QWidget;

class ModernTheme
{
public:
    enum ColorRole {
        ApplicationBackground, MainSurface, Panel, ElevatedPanel,
        ControlBackground, Border, BorderSubtle, PrimaryText,
        SecondaryText, DisabledText, AccentCyan, AccentCyanHover,
        AccentCyanDim, EditorAccent, EditorAccentHover, EditorAccentDim,
        ActiveGreen, ActiveGreenDim, WarningOrange, DangerRed,
        BorderSelected, HoverSurface, PressedSurface, SelectedBackground,
        PendingBackground, BankDivider, PrimaryButton, SecondaryButton,
        ChainSurface, ChainBorder, ChainModuleSurface, ChainModuleOff,
        ChainSelected, ChainConnector, ChainConnectorActive,
        ChainOnIndicator, ChainOffIndicator
    };
    enum RadiusRole { SmallRadius, ControlRadius, PanelRadius, ContainerRadius };

    static QString applicationStyleSheet();
    static QString color(ColorRole role);
    static QString activeEffectAccent(const QString &effectName);
    static int radius(RadiusRole role);
    static QString effectColor(const QString &effectName);
    static QString effectFaceColor(const QString &effectName);
    static void applyWindowsDarkTitleBar(QWidget *window);
    static void refreshWindowsDarkTitleBar(QWidget *window);
};

#endif
