/**************************************************************************
 *   Copyright (C) 2011 by Thomas Lübking                                  *
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

#include "label.h"

#include <KDE/KConfigGroup>

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QFile>
#include <QProcess>
#include <QtDBus>
#include <QTimerEvent>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define CHAR(_S_) _S_.toLocal8Bit().data()

BE::Label::Label( QWidget *parent ) : QLabel(parent)
, BE::Plugged(parent)
, myTimer(0)
, myProcess(0)
, myDBus(0)
, myDBusArgs(0)
, myReplyIsPending(false)
, myFiFo(0)
{
}

static bool isFiFo(const QString &path)
{
    struct stat fileStatus;
    stat(CHAR(path), &fileStatus);
    return S_ISFIFO (fileStatus.st_mode);
}

void
BE::Label::configure( KConfigGroup *grp )
{
    if (myProcess) disconnect( myProcess, 0, this, 0 );
    delete myProcess; myProcess = 0;
    delete myDBus; myDBus = 0;
    delete myDBusArgs; myDBusArgs = 0;
    delete myFiFo; myFiFo = 0;
    myPollInterval = grp->readEntry("PollInterval", 1000);
    myLines = grp->readEntry("Lines", -1);
    myCommand = grp->readEntry("Exec", QString());
    if (!myCommand.isEmpty())
    {
        myProcess = new QProcess(this);
        if ( myPollInterval )
            connect( myProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(updateContents()) );
        else
            connect( myProcess, SIGNAL(readyReadStandardOutput()), SLOT(updateContents()) );
    }
    else
    {
        const QString file = grp->readEntry("FiFo", QString());
        if (!file.isEmpty())
        {
            if (QFile::exists(file) && isFiFo(file)) {
                if (int fd = open(CHAR(file), O_RDWR|O_ASYNC|O_NONBLOCK))
                {
                    myFiFo = new QFile(this);
                    if (myFiFo->open(fd, QIODevice::ReadWrite, QFile::AutoCloseHandle))
                    {
                        QSocketNotifier *snr = new QSocketNotifier(myFiFo->handle(), QSocketNotifier::Read, myFiFo);
                        connect (snr, SIGNAL(activated(int)), SLOT(readFiFo()));
                    }
                    else
                    {
                        delete myFiFo; myFiFo = 0;
                    }
                }
            }
            else
                qWarning("*** BE::Shell Label *** \"%s\": the FiFo %s does not exist!", CHAR(name()), CHAR(file) );
        }
        else
        {
            myCommand = grp->readEntry("DBus", QString());
            QStringList list = myCommand.split(';');
            if (list.count() < 5)
                qWarning("invalid dbus chain, must be: \"bus;service;path;interface;method[;arg1;arg2;...]\", bus is \"session\" or \"system\"");
            else
            {
                if (list.at(0) == "session")
                    myDBus = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::sessionBus() );
                else if (list.at(0) == "system")
                    myDBus = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::systemBus() );
                else
                    qWarning("unknown bus, must be \"session\" or \"system\"");
                if (myDBus)
                {
                    myCommand = list.at(4);
                    if (list.count() > 5)
                    {
                        myDBusArgs = new QList<QVariant>;
                        for (int i = 5; i < list.count(); ++i)
                            *myDBusArgs << QVariant(list.at(i));
                    }
                }
            }
        }
    }

    if (myTimer)
        killTimer(myTimer);
    if (!myFiFo && !myCommand.isEmpty())
    {
        if (myPollInterval)
            myTimer = startTimer(myPollInterval);
        poll();
    }
}




void
BE::Label::timerEvent(QTimerEvent *te)
{
    if ( !te->timerId() == myTimer )
    {
        QLabel::timerEvent(te);
        return;
    }
    if ( !myReplyIsPending )
        poll();
}

void
BE::Label::poll()
{
    if ( myDBus )
    {
        myReplyIsPending = true;
        QDBusPendingCall *pending(0);
        if ( myDBusArgs )
            *pending = myDBus->asyncCallWithArgumentList( myCommand, *myDBusArgs );
        else
            *pending = myDBus->asyncCall( myCommand );
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(*pending, this);
        connect( watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(updateContents()) );
        return;
    }
    if ( myCommand.isEmpty() )
        return;

    myReplyIsPending = true;
    myProcess->start( myCommand, QIODevice::ReadOnly );
}


void
BE::Label::readFiFo()
{
    Q_ASSERT(myFiFo);
    static_cast<QSocketNotifier*>(sender())->setEnabled(false);
    const QString ns = QString::fromLocal8Bit(myFiFo->readAll());
    if (!ns.isEmpty())
    {
        QString s = text() + ns;
        const QStringList l = s.split('\n');
        s.clear();
        for (int i = qMax(0, l.count()-myLines); i < l.count() - 1; ++i)
            s += l.at(i) + '\n';
        s += l.at(l.count()-1);
        // we want the last myLines of this appended to the existing text()and use the
        setText(s);
    }
//     myFiFo->flush();
//     myFiFo->reset();
    static_cast<QSocketNotifier*>(sender())->setEnabled(true);
}

void
BE::Label::updateContents()
{
    myReplyIsPending = false;
    QString s;
    if (QProcess *proc = qobject_cast<QProcess*>(sender()))
        s = QString::fromLocal8Bit(proc->readAllStandardOutput());
    else if (QDBusPendingCallWatcher *call = qobject_cast<QDBusPendingCallWatcher*>(sender()))
    {
        foreach (QVariant v, call->reply().arguments())
            s += " " + v.toString();
        call->deleteLater();
    }
    if (!myPollInterval)
        s = text() + '\n' + s;
    if (!myLines)
        s.replace('\n', ' ');
    else if (myLines > 0)
        s = s.section('\n', 0, myLines-1, QString::SectionSkipEmpty); // head myLines
    else
        s = s.section('\n', myLines, -1, QString::SectionSkipEmpty); // tail myLines
    setText(s);
}

