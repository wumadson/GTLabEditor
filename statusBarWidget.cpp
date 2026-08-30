/****************************************************************************
**
** Copyright (C) 2007~2010 Colin Willcocks.
** Copyright (C) 2005~2007 Uco Mesdag. 
** All rights reserved.
** This file is part of "GT-10 Fx FloorBoard".
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**
****************************************************************************/

#include <QtGui>
#include <QStackedLayout>
#include "statusBarWidget.h"
#include "modernTheme.h"

statusBarWidget::statusBarWidget(QWidget *parent)
    : QWidget(parent)
{
	setObjectName("ModernStatusWidget");
	this->progressBar = new QProgressBar(this);
	this->progressBar->setObjectName("StatusProgress");
	this->progressBar->setTextVisible(false);
	this->progressBar->setFixedSize(72, 4);
	this->progressBar->setRange(0, 100);
	this->progressBar->setValue(0);
	this->progressBar->hide();

	this->symbol = new QLabel(QString::fromUtf8("●"), this);
	this->symbol->setObjectName("StatusSymbol");
	this->symbol->setProperty("state", "idle");

	this->label = new QLabel(this);
	this->label->setObjectName("StatusMessage");
	this->label->setText("");

	this->dBuglabel = new QLabel(this);
	this->dBuglabel->setObjectName("StatusDebug");
	this->dBuglabel->setText("");

	this->connectionDot = new QLabel(QString::fromUtf8("●"), this);
	this->connectionDot->setObjectName("ConnectionDot");
	this->connectionLabel = new QLabel(this);
	this->connectionLabel->setObjectName("ConnectionLabel");
	setConnectionState(false);

	QWidget *operationalGroup = new QWidget(this);
	QHBoxLayout *operationalLayout = new QHBoxLayout(operationalGroup);
	operationalLayout->setContentsMargins(0, 0, 0, 0);
	operationalLayout->setSpacing(4);
	operationalLayout->addWidget(this->symbol, 0, Qt::AlignVCenter);
	operationalLayout->addWidget(this->label, 0, Qt::AlignVCenter);
	operationalLayout->addWidget(this->progressBar, 0, Qt::AlignVCenter);

	QWidget *connectionGroup = new QWidget(this);
	QHBoxLayout *connectionLayout = new QHBoxLayout(connectionGroup);
	connectionLayout->setContentsMargins(0, 0, 8, 0);
	connectionLayout->setSpacing(4);
	connectionLayout->addWidget(this->connectionDot, 0, Qt::AlignVCenter);
	connectionLayout->addWidget(this->connectionLabel, 0, Qt::AlignVCenter);

	QWidget *statusRow = new QWidget(this);
	QHBoxLayout *statusRowLayout = new QHBoxLayout(statusRow);
	statusRowLayout->setContentsMargins(0, 0, 0, 0);
	statusRowLayout->setSpacing(7);
	statusRowLayout->addWidget(operationalGroup, 0, Qt::AlignVCenter);
	statusRowLayout->addStretch(1);
	statusRowLayout->addWidget(connectionGroup, 0, Qt::AlignVCenter);

	QWidget *activityLayer = new QWidget(this);
	activityLayer->setAttribute(Qt::WA_TransparentForMouseEvents);
	QHBoxLayout *activityLayout = new QHBoxLayout(activityLayer);
	activityLayout->setContentsMargins(0, 0, 0, 0);
	activityLayout->addStretch(1);
	activityLayout->addWidget(this->dBuglabel, 0, Qt::AlignCenter);
	activityLayout->addStretch(1);

	QStackedLayout *widgetLayout = new QStackedLayout;
	widgetLayout->setContentsMargins(10, 0, 10, 0);
	widgetLayout->setStackingMode(QStackedLayout::StackAll);
	widgetLayout->addWidget(statusRow);
	widgetLayout->addWidget(activityLayer);

	this->setLayout(widgetLayout);
};

void statusBarWidget::setStatusMessage(QString message)
{
	this->label->setText(message.toUpper());
};

void statusBarWidget::setStatusdBugMessage(QString dBug)
{
	this->dBuglabel->setText(dBug.toUpper());
};

void statusBarWidget::setStatusProgress(int value)
{
	this->progressBar->setValue(value);
	this->progressBar->setVisible(value > 0 && value < 100);
};

void statusBarWidget::setStatusSymbol(int value)
{
	const char *state = value == 1 ? "ready" : value == 2 ? "busy" : "idle";
	this->symbol->setProperty("state", state);
	this->symbol->style()->unpolish(this->symbol);
	this->symbol->style()->polish(this->symbol);
	this->symbol->update();
};

void statusBarWidget::setConnectionState(bool connected)
{
	this->connectionLabel->setText(connected ? "GT-10 CONNECTED"
	                                         : "GT-10 DISCONNECTED");
	const char *state = connected ? "connected" : "disconnected";
	this->connectionDot->setProperty("state", state);
	this->connectionLabel->setProperty("state", state);
	this->connectionDot->style()->unpolish(this->connectionDot);
	this->connectionDot->style()->polish(this->connectionDot);
	this->connectionLabel->style()->unpolish(this->connectionLabel);
	this->connectionLabel->style()->polish(this->connectionLabel);
};


