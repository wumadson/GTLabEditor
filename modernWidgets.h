#ifndef MODERNWIDGETS_H
#define MODERNWIDGETS_H

#include <QAbstractButton>
#include <QColor>
#include <QDial>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QPushButton>

class QPainter;
class QComboBox;
class QGridLayout;
class QVBoxLayout;
class QResizeEvent;
class QVariantAnimation;
class AudioGearKnob;
class AudioGearSwitch;
class ModernToggleSwitch;

class ResponsiveSectionArea : public QWidget
{
public:
    explicit ResponsiveSectionArea(QWidget *parent = nullptr);
    void addSection(QWidget *section);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateSections();
    QGridLayout *sectionGrid;
    QList<QWidget *> sections;
    int currentColumns = 0;
};

class EffectEditorPanel : public QFrame
{
public:
    EffectEditorPanel(const QString &effectName, QWidget *parent = nullptr);
    QLabel *typeLabel() const;
    QWidget *parameterArea() const;
    QWidget *artworkArea() const;
    void setArtworkWidget(QWidget *widget);
    void setArtworkControlWidget(QWidget *widget);
    void setModelBrowserWidget(QWidget *widget);
    QSize minimumSizeHint() const override;
private:
    QLabel *currentType;
    QLabel *modelState;
    QWidget *parameters;
    QWidget *artwork;
    QVBoxLayout *artworkLayout;
    QVBoxLayout *modelLayout;
};

class BottomControlStrip : public QFrame
{
public:
    explicit BottomControlStrip(QWidget *parent = nullptr);
};

class ParameterSection : public QWidget
{
public:
    ParameterSection(const QString &title, int maximumColumns,
                     QWidget *parent = nullptr);
    void addControl(QWidget *control);
    void setResponsiveColumns(int narrowColumns, int breakpoint);
    void setResponsiveColumns(int wideColumns, int mediumColumns,
                              int narrowColumns, int mediumBreakpoint,
                              int narrowBreakpoint);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateGrid();
    int fixedColumns;
    int compactColumns;
    int compactBreakpoint;
    int mediumColumns;
    int narrowColumns;
    int mediumBreakpoint;
    int narrowBreakpoint;
    bool threeLevelResponsive;
    int currentColumns;
    QGridLayout *controlGrid;
    QList<QWidget *> controls;
};

class ParameterKnob : public QWidget
{
public:
    ParameterKnob(const QString &label, QWidget *parent = nullptr);
    AudioGearKnob *dial() const;
    QLabel *valueLabel() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
private:
    AudioGearKnob *knob;
    QLabel *value;
};

class ParameterCombo : public QWidget
{
public:
    ParameterCombo(const QString &label, QWidget *parent = nullptr);
    QComboBox *comboBox() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
private:
    QComboBox *combo;
};

class ParameterToggle : public QWidget
{
public:
    ParameterToggle(const QString &label, QWidget *parent = nullptr);
    AudioGearSwitch *toggle() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
private:
    AudioGearSwitch *control;
};

class ModernToggleSwitch : public QAbstractButton
{
public:
    explicit ModernToggleSwitch(QWidget *parent = nullptr);
    void setAccentColor(const QColor &color);
    QColor accentColor() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void animateThumb(bool checked);
    QColor switchAccent;
    qreal thumbPosition;
    QVariantAnimation *thumbAnimation;
};

class EffectToggleControl : public QWidget
{
public:
    explicit EffectToggleControl(const QString &label,
                                 QWidget *parent = nullptr);
    ModernToggleSwitch *toggle() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
private:
    ModernToggleSwitch *control;
};

class AudioGearPanel : public QFrame
{
public:
    explicit AudioGearPanel(QWidget *parent = nullptr);
    void setPanelAccent(const QColor &color);
    void setPanelSelected(bool selected);
    void setPanelEnabled(bool enabled);
protected:
    void paintEvent(QPaintEvent *event) override;
    virtual void paintPanelDetails(QPainter &painter, const QRectF &body);
    QColor panelAccent;
    bool panelSelected;
    bool panelEnabled;
};

class AudioGearKnob : public QDial
{
public:
    explicit AudioGearKnob(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};

class AudioGearLed : public QWidget
{
public:
    explicit AudioGearLed(QWidget *parent = nullptr);
    void setLedColor(const QColor &color);
    void setOn(bool on);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QColor color;
    bool lit;
};

class AudioGearSwitch : public QPushButton
{
public:
    explicit AudioGearSwitch(QWidget *parent = nullptr);
    void setOn(bool on);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    bool active;
};

class SignalConnector : public QWidget
{
public:
    enum Direction { Input, Output };
    explicit SignalConnector(Direction direction, QWidget *parent = nullptr);
    void setCompactWidth(int width);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    Direction connectorDirection;
};

class SignalChainPanel : public QFrame
{
public:
    explicit SignalChainPanel(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};

class SignalJunction : public QPushButton
{
public:
    enum Kind { Split, Merge };
    explicit SignalJunction(Kind kind, QWidget *parent = nullptr);
    void setCompactWidth(int width);
    void setSelected(bool selected);
protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    Kind junctionKind;
    bool junctionSelected;
};

class SignalChainModule : public QPushButton
{
public:
    SignalChainModule(const QString &name, const QColor &accent,
                      const QColor &faceColor,
                      QWidget *parent = nullptr);
    void setEffectState(bool available, bool on);
    void setSelected(bool selected);
    void setNavigable(bool navigable);
    void setCompactWidth(int width);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString moduleName;
    QColor moduleAccent;
    QColor moduleFaceColor;
    bool stateAvailable;
    bool stateOn;
    bool moduleSelected;
    bool moduleNavigable;
};

class StatusBadge : public QLabel
{
public:
    explicit StatusBadge(QWidget *parent = nullptr);
    void setConnected(bool connected);
};

class EffectModule : public AudioGearPanel
{
public:
    enum VisualKind { DualKnob, Equalizer };
    EffectModule(const QString &name, const QString &accent, bool available,
                 VisualKind kind = DualKnob, QWidget *parent = nullptr);
    QPushButton *actionButton() const;
    void setEffectState(bool available, bool on);
    void setSelected(bool selected);
    void setTypeText(const QString &type);
    void setControlValues(int left, int right, bool valid);
    void setCompact(bool compact);
protected:
    void paintPanelDetails(QPainter &painter, const QRectF &body) override;
private:
    void configureControlLabels();
    void updateAppearance();
    void paintMiniKnob(QPainter &painter, const QPointF &center, qreal radius, int value) const;
    void paintEqualizer(QPainter &painter, const QRectF &area) const;
    QString effectName;
    QString accentColor;
    bool effectAvailable;
    bool effectOn;
    bool valuesValid;
    int leftValue;
    int rightValue;
    VisualKind visualKind;
    QLabel *nameLabel;
    QLabel *typeLabel;
    QLabel *leftLabel;
    QLabel *rightLabel;
    AudioGearLed *led;
    AudioGearSwitch *button;
};

#endif
