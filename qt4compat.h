#pragma once

/*
 * Qt4 -> Qt5 compatibility header for GT-10 FxFloorBoard.
 *
 * qmake also injects QMAKE_CXXFLAGS while generating moc_predefs.h,
 * but that special compiler invocation has no Qt include paths.
 *
 * Therefore only load the compatibility includes when Qt headers
 * are actually available.
 */

#if __has_include(<QApplication>)

#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QWidget>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>

#include <QGroupBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QScrollArea>

#include <QLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

#include <QProgressBar>
#include <QSlider>
#include <QDial>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintPreviewDialog>

#endif
