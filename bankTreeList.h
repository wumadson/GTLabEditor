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

#ifndef BANKTREELIST_H
#define BANKTREELIST_H

#include <QWidget>

class midiIO;
#include <QMap>
#include <QList>
#include <QTreeWidget>

class bankTreeList : public QWidget
{
	Q_OBJECT

public:
	bankTreeList(QWidget *parent = 0);
	~bankTreeList();

public slots:
	void updateSize(QRect newrect);
	void updatePatchNames(QString replyMsg);
	void updatePatch(QString patchName);
	void setClosedItems(QTreeWidgetItem *item);
	void setOpenItems(QTreeWidgetItem *item);
	void connectedSignal();
	void disconnectedSignal();
	void requestPatch();
	void requestPatch(int bank, int patch);
	void reloadCurrentPatch();
	void requestSelectedPatchReadback();
	void requestPatchNamesForBank(int bank);
	void selectPatch(int bank, int patch, const QString &name);
	void shortMidiMessageReceived(int status, int data1, int data2);
	void setExclusiveMemoryOperation(bool active);
	void setItemClicked(QTreeWidgetItem *item, int column);
	void setItemDoubleClicked(QTreeWidgetItem *item, int column);

signals:
	void itemExpanded(QTreeWidgetItem *item);
	void itemCollapsed(QTreeWidgetItem *item);
	void itemClicked(QTreeWidgetItem *item, int column);
	void itemDoubleClicked(QTreeWidgetItem *item, int column);
	void patchSelectSignal(int bank, int patch);
	void patchLoadSignal(int bank, int patch);
	void patchNameResolved(int bank, int patch, QString name);
	void updateSignal();

	void setStatusSymbol(int value);
	void setStatusProgress(int value);
    void setStatusMessage(QString message);

	void notConnectedSignal();

private:
	void requestPhysicalPatchReadback(int bank, int patch);
	int configuredTransmitChannel() const;
	bool exclusiveMemoryOperationActive = false;
	void updateTree(QTreeWidgetItem *item);
	void closeChildren(QTreeWidgetItem *item);
	QTreeWidget* newTreeList();
	QList<QTreeWidgetItem*> openBankTreeItems;
	QList<QTreeWidgetItem*> openPatchTreeItems;
	QList<QTreeWidgetItem*> currentPatchTreeItems;
	midiIO *patchChangeListener;
	int lastBankMsb;
	int lastBankLsb;
	int queuedPhysicalBank;
	int queuedPhysicalPatch;
	bool localPatchChangePending;
	int localPatchChangeBank;
	int localPatchChangePatch;
	bool preserveLoadedPatchOnNextRead;
	int preservedLoadedBank;
	int preservedLoadedPatch;
	QTreeWidget* treeList;
	QMap<int, QTreeWidgetItem*> patchBankItems;
	int itemIndex;
	int listIndex;
};

#endif // BANKTREELIST_H
