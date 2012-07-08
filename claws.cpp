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

#include "claws.h"
#include "dbus_claws.h"
#include "be.shell.h"

// #include <QWheelEvent>
#include <QDBusConnection>
#include <QProcess>
#include <QTimer>
#include <KDE/KToolInvocation>
#include <KDE/KWindowInfo>
#include <KDE/KWindowSystem>


BE::Claws::Claws( QWidget *parent ) : QToolButton(parent), BE::Plugged(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIcon(themeIcon("claws-mail"));
    QDBusConnection::sessionBus().registerObject("/Claws", this);
    new BE::ClawsAdaptor(this);
    connect( this, SIGNAL(clicked()), this, SLOT(toggleClaws()) );
}


void
BE::Claws::resizeEvent(QResizeEvent */*re*/)
{
    int s = qMin(width(),height());
    setIconSize(QSize(s,s));
}

#define EQUALS(_STR1_,_STR2_) !_STR1_.compare(_STR2_, Qt::CaseInsensitive)

void
BE::Claws::toggleClaws()
{
    const QString cmd = "claws-mail";
    if (WId id = KWindowSystem::activeWindow())
    {
        KWindowInfo info(id, 0, NET::WM2WindowClass);
        if ( EQUALS(cmd, info.windowClassName()) )
        {
            BE::Shell::run(cmd + " --exit");
            return;
        }
    }
    KToolInvocation::startServiceByDesktopName(cmd);
    QTimer::singleShot(5000, this, SLOT(updateNewMails()));
}

void
BE::Claws::themeChanged()
{
    setIcon(themeIcon("claws-mail"));
}

void
BE::Claws::updateNewMails()
{
    QProcess proc;
    proc.start( "claws-mail --status" );
    proc.waitForFinished();
    QString ret = proc.readAllStandardOutput();
    bool ok;
    int n = ret.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt(&ok);
    if ( ok )
        setText( QString::number(n) );
}

// void
// BE::Claws::wheelEvent(QWheelEvent *ev)
// {
//     int i = ev->delta() > 0 ? 0 : 1;
//     if ( !myWheel[i].isEmpty() )
//     {
//         if ( myWheel[i].startsWith("dbus/") )
//             BE::Shell::call(myWheel[i].right( myWheel[i].length()-5 ));
//         else
//             BE::Shell::run(myWheel[i]);
//     }
// }

