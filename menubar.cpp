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

#include <QAction>
#include <QBasicTimer>
#include <QDBusInterface>
#include <QMouseEvent>
#include <QStyleOptionMenuItem>
#include <QTime>
#include <QTimerEvent>

#include <QtDebug>


static QBasicTimer mousePoll;
static QPoint lastMousePos;
static QTime lastTriggerTime;
static QAction *lastTriggeredAction = 0;

BE::_CLASS_::_CLASS_( const QString &service, qlonglong key, QWidget *parent) : _BASE_(parent)
{
    setWindowFlags(Qt::Widget);
    myService = service;
    myKey = key;
    setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );

    lastTriggerTime = QTime::currentTime();

    connect (this, SIGNAL(triggered(QAction*)), this, SLOT(trigger(QAction*)));
//    setObjectName( "XBarMenubar" );
}

QAction *
BE::_CLASS_::action(int idx) const
{
    if (idx > -1 && idx < actions().count())
        return actions().at(idx);
    return NULL;
}

void
BE::_CLASS_::clear()
{
    while (actions().isEmpty())
        delete actions().takeFirst();
    _BASE_::clear();
}


void
BE::_CLASS_::hide()
{
    popDown();
    _BASE_::hide();
}

void
BE::_CLASS_::hover(QAction *act)
{
//     if (act->menu())
//         return;
    emit hovered(index(act));
}


int
BE::_CLASS_::index(const QAction *act) const
{
    for (int i = 0; i < actions().count(); ++i)
        if ( actions().at(i) == act )
            return i;

   return -1;
}

void
BE::_CLASS_::leaveEvent(QEvent *event)
{
#ifdef _VERTICAL_
    if (!mousePoll.isActive())
    {
        _BASE_::leaveEvent(event);
        setActiveAction(0);
    }
#endif
}

void
BE::_CLASS_::popDown()
{
    QDBusInterface interface( myService, "/XBarClient", "org.kde.XBarClient" );
    if (interface.isValid())
        interface.call(QDBus::NoBlock, "popDown", myKey);

//     foreach (QAction *action, actions())
//     {
//         if (action->menu())
//             action->menu()->close();
//     }

   // to stop timer in case!
    setOpenPopup(-1);
}

void
BE::_CLASS_::setOpenPopup(int popup)
{
    disconnect (this, SIGNAL(hovered(QAction*)), this, SLOT(hover(QAction*)));
    if (popup < 0)
    {
        mousePoll.stop();
        // fake a mouseclick to reset the menubar (activeAction() == 0, d->popupMode == false, setActiveAction() does NOT work)
        QMouseEvent mev( QEvent::MouseButtonPress, QPoint(-1,-1), Qt::LeftButton, Qt::LeftButton,  Qt::NoModifier);
        mousePressEvent(&mev);
        mev = QMouseEvent( QEvent::MouseButtonRelease, QPoint(-1,-1), Qt::LeftButton, Qt::LeftButton,  Qt::NoModifier);
        mouseReleaseEvent(&mev);
    }
    else
    {
        connect (this, SIGNAL(hovered(QAction*)), this, SLOT(hover(QAction*)));
        mousePoll.start(50, this);
    }
}

void
BE::_CLASS_::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != mousePoll.timerId())
        return;

    QPoint pos = mapFromGlobal(QCursor::pos());
    if (pos != lastMousePos && rect().contains(pos))
    {
        QAction *act = actionAt(pos);
        if (activeAction() != act)
            setActiveAction( act );
    }
    lastMousePos = pos;
}

void
BE::_CLASS_::trigger(QAction *act)
{
//     if (act->menu())
//         return;
    // this is kinda workaround. seems after changing the desktop the trigger gets called twice
    // we catch it and ignore fast double triggers
    if (lastTriggeredAction != act || lastTriggerTime.msecsTo(QTime::currentTime()) > 60) // nobody clicks that fast...
        emit triggered(index(act));
    lastTriggerTime = QTime::currentTime();
    lastTriggeredAction = act;
}

