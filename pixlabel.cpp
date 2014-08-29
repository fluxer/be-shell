/**************************************************************************
*   Copyright (C) 2014 by Thomas Luebking                                 *
*   thomas.luebking@gmail.com                                             *
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

#include "pixlabel.h"
#include "be.shell.h"
#include <QResizeEvent>
#include <QTimerEvent>


#include <QtDebug>

PixLabel::PixLabel(QWidget *parent) : QLabel(parent), myScaleDelay(0), iTriedToGrow(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

int PixLabel::heightForWidth(int w) const
{
    int l,t,r,b;
    BE::Shell::getContentsMargins(this, &l, &t, &r, &b);
    return w - (l+r) + t + b;
}

void PixLabel::setIcon(const QIcon &icn)
{
    myIcn = icn;

    int l,t,r,b;
    BE::Shell::getContentsMargins(this, &l, &t, &r, &b);
    const QSize sz(width() - (l+r), height() - (t+b));
    if (sz.isEmpty())
        return;
    // round up to ^2
    int ext = qMin(sz.width(), sz.height());
    --ext;
    for (int i = 0; i < 5; ++i)
        ext |= ext >> (2<<i);
    ++ext;
    setPixmap(icn.pixmap(ext).scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PixLabel::resizeEvent(QResizeEvent *re)
{
    int l,t,r,b;
    BE::Shell::getContentsMargins(this, &l, &t, &r, &b);
    const QSize sz(width() - (l+r), height() - (t+b));
    if (sz.width() == sz.height()) {
        QLabel::resizeEvent(re);
        if (myScaleDelay)
            killTimer(myScaleDelay);
        if (pixmap() && sz != pixmap()->size())
            myScaleDelay = startTimer(150);
    } else if (iTriedToGrow) {
        iTriedToGrow = false;
        const int s = qMin(sz.width(), sz.height());
        resize(s + width() - sz.width(), s + height() - sz.height());
    } else {
        iTriedToGrow = true;
        const int s = qMax(sz.width(), sz.height());
        resize(s + width() - sz.width(), s + height() - sz.height());
    }
}

void PixLabel::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == myScaleDelay) {
        killTimer(myScaleDelay);
        myScaleDelay = 0;
        setIcon(myIcn);
        iTriedToGrow = false;
    } else {
        QLabel::timerEvent(te);
    }
}