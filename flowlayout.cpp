/**************************************************************************
*   Copyright (C) 2009 by Thomas Lübking                                  *
*   thomas.luebking@web.de                                                *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "flowlayout.h"
#include "be.shell.h"
#include <QWidget>

BE::FlowLayout::FlowLayout(QWidget *parent) : QGridLayout(parent), internalInvalidation(false) {}

void
BE::FlowLayout::addItem(QLayoutItem *item)
{
    for (int r = 0; r < rowCount(); ++r)
    for (int c = 0; c < columnCount(); ++c) {
        if (!itemAtPosition(r,c)) {
            QGridLayout::addItem(item, r, c);
            return;
        }
    }
    QGridLayout::addItem(item, 0, columnCount(), 1, 1, Qt::AlignCenter );
}


void
BE::FlowLayout::invalidate()
{
    QGridLayout::invalidate();
    if (internalInvalidation) {
        internalInvalidation = false;
        return;
    }

    QWidget *pw = parentWidget();

    if (!(pw && pw->isVisible()))
        return;

    QLayoutItem *item;
    int left, right, top, bottom;
    BE::Shell::getContentsMargins(pw, &left, &top, &right, &bottom);
    const int pww = pw->maximumWidth()-(left+right);

    QList<QLayoutItem*> items;
    QList<QLayoutItem*> inItems;
    int r = 0, c = 0;
    for (r = 0; r < rowCount(); ++r)
    for (c = 0; c < columnCount(); ++c)
        if ((item = itemAtPosition(r, c))) {
            if (item->widget() && !item->widget()->isVisibleTo(pw))
                inItems << item;
            else
                items << item;
            internalInvalidation = true; removeItem(item);
        }

    r = 0; c = 0; int w = 0;
    while (!items.isEmpty()) {
        item = items.takeFirst();
        if (w + item->minimumSize().width() > pww)
            { ++r; w = 0; c = 0; }
        internalInvalidation = true; QGridLayout::addItem(item, r, c);
        w += item->minimumSize().width() + spacing();
        ++c;
    }
    while (!inItems.isEmpty()) {
        internalInvalidation = true; QGridLayout::addItem(inItems.takeFirst(), r, ++c);
    }

    internalInvalidation = false;
}
