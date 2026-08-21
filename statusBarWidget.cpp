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
	this->label->setMinimumWidth(100);
	this->label->setText("");

	this->dBuglabel = new QLabel(this);
	this->dBuglabel->setObjectName("StatusDebug");
	this->dBuglabel->setText("");

	QHBoxLayout *widgetLayout = new QHBoxLayout;
	widgetLayout->setContentsMargins(10, 0, 10, 0);
	widgetLayout->setSpacing(7);
	widgetLayout->addWidget(this->symbol, Qt::AlignCenter);
	widgetLayout->addWidget(this->label, Qt::AlignCenter);
	widgetLayout->addWidget(this->progressBar, Qt::AlignCenter);
	widgetLayout->addWidget(this->dBuglabel, 1, Qt::AlignCenter);

	this->setLayout(widgetLayout);
};

void statusBarWidget::setStatusMessage(QString message)
{
	this->label->setText(message);
};

void statusBarWidget::setStatusdBugMessage(QString dBug)
{
	this->dBuglabel->setText(dBug);
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


