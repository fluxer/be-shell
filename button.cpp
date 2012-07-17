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

#include "button.h"
#include "dbus_button.h"
#include "be.shell.h"

#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KService>
#include <KDE/KStandardDirs>
#include <KDE/KToolInvocation>

#include <QApplication>
#include <QDBusConnection>
#include <QDesktopWidget>
#include <QMenu>
#include <QtDBus>
#include <QWheelEvent>


BE::Button::Button( QWidget *parent, const QString &plugName ) : QToolButton(parent)
, BE::Plugged(parent, plugName)
, myMenu(0)
, myPulseIteration(0)
, myDBus(0)
, imNotReallyCrossed(false)
{
    new BE::ButtonAdaptor(this);
    setShortcut(QKeySequence());
}

void
BE::Button::configure( KConfigGroup *grp )
{
    disconnect( SIGNAL(clicked()) );
    if (!name().isEmpty()) {
        QString opath("/" % name());
        opath.replace( QRegExp("[^A-Za-z0-9_/]"), "_" );
        opath.replace( QRegExp("/+"), "/" );
        QObject *clash = QDBusConnection::sessionBus().objectRegisteredAt(opath);
        if (clash && clash != this)
            qDebug() << "WARNING: dbus object \"" << opath << "\" is already taken by " << clash << " and cannot be registered by " << this;
        else
            QDBusConnection::sessionBus().registerObject(opath, this);
    }
    else
        qDebug() << "BUG: some button ain't no name" << this << QToolButton::parent();
    delete myMenu;
    myMenu = 0;
    bool connected = false;

    setText(grp->readEntry("Label", QString()));
    myIcon = grp->readEntry("Icon", QString());
    setToolButtonStyle((Qt::ToolButtonStyle)grp->readEntry("Mode", text().isEmpty() ? (int)Qt::ToolButtonIconOnly : (int)Qt::ToolButtonTextOnly));
    setPopupMode(InstantPopup);

    myCommand = grp->readEntry("Exec", QString());
    if (!myCommand.isEmpty())
    {
        myExe = myCommand.section(' ', 0, 0);
        myExe = myExe.section('/', -1);
        QString env = grp->readEntry("Env", QString());
        if (!env.isEmpty())
            myCommand = "env " + env + " " + myCommand;
    }
    myWheel[0] = grp->readEntry("WheelUp", QString());
    myWheel[1] = grp->readEntry("WheelDown", QString());

    QString service = grp->readEntry("Service", QString());
    if (!service.isEmpty())
    {
        QString s = service;
        service = KGlobal::dirs()->locate("xdgdata-apps", s + ".desktop");
        if (service.isEmpty())
            service = KGlobal::dirs()->locate("services", s + ".desktop");
        if (!service.isEmpty())
        {
            KService kservice(service);
            if (myIcon.isEmpty())
                myIcon = kservice.icon();
            if (myCommand.isEmpty())
            {
                myCommand = kservice.desktopEntryName();
                connect( this, SIGNAL(clicked()), this, SLOT(startService()) );
                connected = true;
                myExe = kservice.exec();
                myExe = myExe.section(' ', 0, 0);
                myExe = myExe.section('/', -1);
            }
            if (text().isEmpty())
                setText(kservice.name());
        }
    }


    if (myCommand.isEmpty())
    {
        myCommand = grp->readEntry("DBus", QString());
        if (myCommand.isEmpty())
        {
            myCommand = grp->readEntry("Menu", QString());
            if (myCommand.isEmpty())
                return;
            myMenu = new QMenu(this);
            BE::Shell::buildMenu(myCommand, myMenu, "menu");
            setMenu(myMenu);
            setPopupMode( QToolButton::InstantPopup );
            myExe = grp->readEntry("MenuUpdater", QString());
            if (!myExe.isEmpty()) {
                myUpdaterTimeout = grp->readEntry("MenuUpdaterTimeout", 2000);
                connect (myMenu, SIGNAL(aboutToShow()), SLOT(updateMenu()));
            }
        }
        else
            connect( this, SIGNAL(clicked()), this, SLOT(dbusCall()) );
    }
    else if (!connected)
        connect( this, SIGNAL(clicked()), this, SLOT(runCommand()) );

    setIcon(themeIcon(myIcon));
    setShortcut(QKeySequence()); // getrid of mnemonics
}

void
BE::Button::dbusCall()
{
    QStringList list = myCommand.split(';');
    // geometry substitution
    if (list.count() > 5)
    {
        int x = 0, y = 0;
        QRect r = QApplication::desktop()->availableGeometry();
        QRect gr(mapToGlobal(rect().topLeft()), size());

        if (orientation() == Qt::Horizontal)
        {
            x = gr.left();
            if (gr.bottom() < r.height()/3) y = gr.bottom();
            else if (gr.top() > 2*r.height()/3) y = gr.bottom();
            else y = gr.center().y();
        }
        else
        {
            y = gr.top();
            if (gr.right() < r.width()/3) x = gr.right();
            else if (gr.left() > 2*r.width()/3) x = gr.left();
            else x = gr.center().x();
        }

        for (int i = 5; i < list.count(); ++i)
        {
            if (list.at(i) == "$x")
                list[i] = QString::number(x);
            else if (list.at(i) == "$y")
                list[i] = QString::number(y);
        }
    }
    BE::Shell::call(list.join(";"));
}

void
BE::Button::pulse()
{
    ++myPulseIteration;
    if ( myPulseIteration > myPulseLimit )
    {
        myPulseIteration = 0;
        if (rect().contains(mapFromGlobal(QCursor::pos())))
        {
            QEvent e(QEvent::Enter);
            QCoreApplication::sendEvent(this, &e);
        }
        return;
    }
    imNotReallyCrossed = true;
    QEvent e(bool(myPulseIteration % 2) ? QEvent::Enter : QEvent::Leave);
    QCoreApplication::sendEvent(this, &e);
    imNotReallyCrossed = false;
    QTimer::singleShot(1000, this, SLOT(pulse()));
}

void
BE::Button::requestAttention(int count)
{
    myPulseLimit = count;
    if (!myPulseIteration)
        QTimer::singleShot(1000, this, SLOT(pulse()));
    myPulseIteration %= 2; // reset
}

void
BE::Button::resizeEvent(QResizeEvent */*re*/)
{
    int s = qMin(width(),height());
    setIconSize(QSize(s,s));
}

void
BE::Button::runCommand()
{
    BE::Shell::run(myCommand);
}

void
BE::Button::setCommand( QString cmd )
{
    disconnect( this, SIGNAL(clicked()), this, 0 );
    myExe = myCommand = cmd;
    myExe = myExe.section(' ', 0, 0);
    myExe = myExe.section('/', -1);
    connect( this, SIGNAL(clicked()), SLOT(runCommand()) );
}

void
BE::Button::startService()
{
    KToolInvocation::startServiceByDesktopName(myCommand);
}

void
BE::Button::themeChanged()
{
    setIcon(themeIcon(myIcon));
}

void
BE::Button::updateMenu()
{
    QProcess proc(this);
    proc.start(myExe);
    proc.waitForFinished(myUpdaterTimeout);
    myMenu->clear();
    BE::Shell::buildMenu(myCommand, myMenu, "menu");
}


void
BE::Button::mousePressEvent(QMouseEvent *me)
{
    if (menu())
        menu()->installEventFilter(this);
    QToolButton::mousePressEvent(me);
}
void
BE::Button::mouseReleaseEvent(QMouseEvent *me)
{
    if (menu())
        menu()->removeEventFilter(this);
    QToolButton::mouseReleaseEvent(me);
}

bool
BE::Button::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Show && o == menu())
        menu()->move(popupPosition(menu()->size()));
    return false;
}

void
BE::Button::wheelEvent(QWheelEvent *ev)
{
    int i = ev->delta() > 0 ? 0 : 1;
    if ( !myWheel[i].isEmpty() )
    {
        if ( myWheel[i].startsWith("dbus/") )
            BE::Shell::call(myWheel[i].right( myWheel[i].length()-5 ));
        else
            BE::Shell::run(myWheel[i]);
    }
}

