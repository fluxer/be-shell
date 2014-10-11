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

#include "be.shell.h"

#include <KDE/KConfigGroup>

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QFile>
#include <QMovie>
#include <QProcess>
#include <QtDBus>
#include <QTimer>
#include <QTimerEvent>
#include <QUrl>
#include <QWheelEvent>

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
, myToolTip(0)
, myToolTipTimer(0)
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
    delete movie(); setMovie(NULL);
    setCursor(Qt::ArrowCursor);
    myPollInterval = grp->readEntry("PollInterval", 1000);
    myLines = grp->readEntry("Lines", -1);
    myCommand = grp->readEntry("Exec", QString());
    setWordWrap(grp->readEntry("Wrap", false));
    myPermittedCommands = grp->readEntry("PermittedCommands", QStringList());
    bool isActive = grp->readEntry("Active", false);
    setOpenExternalLinks(isActive && myPermittedCommands.isEmpty());
    setTextInteractionFlags(isActive ? Qt::TextSelectableByKeyboard|Qt::LinksAccessibleByMouse : Qt::LinksAccessibleByMouse);
    disconnect (this, SIGNAL(linkActivated(const QString &)), this, SLOT(protectedExec(const QString &)));
    if (isActive && !myPermittedCommands.isEmpty())
        connect(this, SIGNAL(linkActivated(const QString &)), SLOT(protectedExec(const QString &)));

    QString _home, _user;
    char *env;
    if ((env = getenv("HOME")))
        _home = QString::fromLocal8Bit(env);
    if ((env = getenv("USER")))
        _user = QString::fromLocal8Bit(env);
    QString movie = grp->readEntry("Anicon", QString());
    QString image = grp->readEntry("Image", QString());
    if (!movie.isEmpty()) {
        movie.replace("$HOME", _home).replace("$USER", _user);
        myCommand = QString();
        QMovie *M = new QMovie(movie, QByteArray(), this); // see Fritz Lang's "M"!
        if (M->isValid()) {
            iStartMovieOnShow = grp->readEntry("AniconStartOnShow", false);
            M->setCacheMode(QMovie::CacheAll);
            setMovie(M);
            setCursor(Qt::PointingHandCursor);
            myMovieLoops = grp->readEntry("AniconLoops", -2);
            if (myMovieLoops != 0) { // no hand start
                QMetaObject::invokeMethod(this, "startMovie", Qt::QueuedConnection);
            }
        } else {
            delete M;
            if (QFile::exists(movie))
                qWarning() << movie << " is NOT a supported animation format!\nSupported formats:\n" << QMovie::supportedFormats();
            else
                qWarning() << "The path " << movie << " does not exist...";
        }
    } else if (!image.isEmpty()) {
        image.replace("$HOME", _home).replace("$USER", _user);
        myCommand = QString();
        setPixmap(QPixmap(image));
    } else if (!myCommand.isEmpty()) {
        myCommand.replace("$HOME", _home).replace("$USER", _user);
        myProcess = new QProcess(this);
        if ( myPollInterval )
            connect( myProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(updateContents()) );
        else
            connect( myProcess, SIGNAL(readyReadStandardOutput()), SLOT(updateContents()) );
    } else {
        QString file = grp->readEntry("FiFo", QString());
        if (!file.isEmpty()) {
            file.replace("$HOME", _home).replace("$USER", _user);
            if ((QFile::exists(file) && isFiFo(file)) ||
                (mkfifo(CHAR(file), S_IRUSR|S_IWUSR) == 0)) {
                if (int fd = open(CHAR(file), O_RDWR|O_ASYNC|O_NONBLOCK)) {
                    myFiFo = new QFile(this);
                    if (myFiFo->open(fd, QIODevice::ReadWrite, QFile::AutoCloseHandle)) {
                        QSocketNotifier *snr = new QSocketNotifier(myFiFo->handle(), QSocketNotifier::Read, myFiFo);
                        connect (snr, SIGNAL(activated(int)), SLOT(readFiFo()));
                    } else {
                        delete myFiFo; myFiFo = 0;
                    }
                }
            }
            else {
                qWarning("*** BE::Shell Label *** \"%s\": the FiFo %s does not exist nor could be created!", CHAR(name()), CHAR(file));
            }
        } else {
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

    QString toolTip = grp->readEntry("TipLabel", QString());
    if (toolTip.isEmpty() || (myToolTip && myToolTip->objectName() != toolTip)) {
        delete myToolTip;
        myToolTip = 0;
    }
    if (!toolTip.isEmpty() && !myToolTip) {
        myToolTip = new Label(this);
        myToolTip->setWindowFlags(Qt::ToolTip);

        if (!BE::Shell::name(myToolTip, toolTip)) {
            qWarning("failed to name a tooltip label, this should not have happened!");
            delete myToolTip;
            myToolTip = 0;
        }
    }
    if (myToolTip)
        myToolTip->Plugged::configure();

    if (myTimer)
        killTimer(myTimer);
    if (!myFiFo && !myCommand.isEmpty()) {
        if (myPollInterval)
            myTimer = startTimer(myPollInterval);
        poll();
    }
}

void
BE::Label::protectedExec(const QString &cmd)
{
    QUrl url(cmd);
    if (url.scheme() != "exec") // not for us
        return;
    if (url.path() == "%poll") {
        poll();
        return;
    }
    if (!myPermittedCommands.contains(url.path())) {
        qWarning() << "***WARNING*** Execution of unpermitted path requested\nPath: " << url.path() << 
                      "The path *must* be in the \"PermittedCommands\" list of the sender" << sender();;
        return;
    }
    BE::Shell::run(url.path());
}

void
BE::Label::enterEvent(QEvent *e)
{
    QLabel::enterEvent(e);
    if (!myToolTip)
        return;
    myToolTip->adjustSize();
    myToolTip->move(popupPosition(myToolTip->size()));
    if (!myToolTipTimer) {
        myToolTipTimer = new QTimer(this);
        myToolTipTimer->setSingleShot(true);
    }
    else
        myToolTipTimer->stop();
    if (myToolTip->isVisible())
        return;
    connect (myToolTipTimer, SIGNAL(timeout()), myToolTip, SLOT(show()));
    connect (myToolTipTimer, SIGNAL(timeout()), myToolTip, SLOT(raise()));
    myToolTipTimer->start(128);
}

void
BE::Label::leaveEvent(QEvent *e)
{
    QLabel::leaveEvent(e);
    if (!myToolTip)
        return;
    if (!myToolTipTimer) {
        myToolTipTimer = new QTimer(this);
        myToolTipTimer->setSingleShot(true);
    }
    else
        myToolTipTimer->stop();
    if (!myToolTip->isVisible())
        return;
    connect (myToolTipTimer, SIGNAL(timeout()), myToolTip, SLOT(hide()));
    myToolTipTimer->start(512);
}

void
BE::Label::hideEvent(QHideEvent *he)
{
    if (movie() && movie()->state() == QMovie::Running) {
        movie()->setPaused(true);
    }
    QLabel::hideEvent(he);
}

void
BE::Label::mousePressEvent(QMouseEvent *me)
{
    if (movie() && me->button() == Qt::LeftButton) {
        if (movie()->state() == QMovie::NotRunning) {
            startMovie();
        } else {
            movie()->setPaused(movie()->state() != QMovie::Paused);
        }
    } else {
        QLabel::mousePressEvent(me);
    }
}

void
BE::Label::showEvent(QShowEvent *se)
{
    if (QMovie *M = movie()) { // see it everyday!
        if (iStartMovieOnShow) {
            stopMovie();
            startMovie();
        } else if (M->state() == QMovie::Paused) {
            M->setPaused(false);
        }
    }
    QLabel::showEvent(se);
}

void
BE::Label::startMovie()
{
    if (QMovie *M = movie()) { // see Fritz Lang's "M" one more time!
        if (M->state() != QMovie::NotRunning)
            return;
        if (myMovieLoops > -2 && M->loopCount() != -1) { // make infinite
            connect(M, SIGNAL(finished()), M, SLOT(start()));
        }
        M->start();
        if (!isVisible())
            M->setPaused(true);
        if (myMovieLoops > 0) { // fixed playback time
            M->jumpToNextFrame();
            int timeout = M->nextFrameDelay();
            timeout = myMovieLoops * M->frameCount() * timeout - timeout/2;
            M->jumpToFrame(0);
            QTimer::singleShot(timeout, this, SLOT(stopMovie()));
        }
    }
}

void
BE::Label::stopMovie()
{
    if (QMovie *M = movie()) { // see Fritz Lang's "M" even another time!
        disconnect(M, SIGNAL(finished()), M, SLOT(start()));
        M->stop();
    }
}

void
BE::Label::timerEvent(QTimerEvent *te)
{
    if ( te->timerId() != myTimer ) {
        QLabel::timerEvent(te);
        return;
    }
    if ( !myReplyIsPending )
        poll();
}

void
BE::Label::wheelEvent(QWheelEvent *we)
{
    QLabel::wheelEvent(we);
    QSize hint = sizeHint();
    int l,t,r,b;
    getContentsMargins(&l, &t, &r, &b);
    int cth = height() - (qMax(t,0) + qMax(b,0));
    int ctw =  width() - (qMax(l,0) + qMax(r,0));
    if (hint.height() > cth || hint.height() > ctw) {
        we->accept();
        int delta = we->delta() > 0 ? 32 : -32;
        hint -= QSize(l+r,t+b);
        if (we->orientation() == Qt::Horizontal || hint.height() <= cth)
            l = qMax(ctw - hint.width(), qMin(0, l + delta));
        else
            t = qMax(cth - hint.height(), qMin(0, t + delta));
        setContentsMargins(l, t, r, b);
    }
}

void
BE::Label::poll()
{
    if ( myDBus )
    {
        myReplyIsPending = true;
        QDBusPendingCallWatcher *watcher;
        if ( myDBusArgs )
            watcher = new QDBusPendingCallWatcher(myDBus->asyncCallWithArgumentList( myCommand, *myDBusArgs ), this);
        else
            watcher = new QDBusPendingCallWatcher(myDBus->asyncCall( myCommand ), this);
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
        QString s = text() + '\n' + ns;
        const QStringList l = s.split('\n');
        s.clear();
        int last = l.count(), lines = 0;
        while (lines < myLines && last > 0) {
            if (!l.at(--last).isEmpty())
                ++lines;
        }
        while (lines > -1 && last < l.count()) {
            if (!l.at(last).isEmpty()) {
                --lines;
                s += l.at(last);
                if (lines)
                    s+= '\n';
            }
            ++last;
        }
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

