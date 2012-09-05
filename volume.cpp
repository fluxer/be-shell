/**************************************************************************
*   Copyright (C) 2009 by Thomas Luebking                                  *
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

#include "volume.h"
#include "be.shell.h"
#include "touchwheel.h"

#include <KDE/KConfigGroup>
#include <KDE/KAction>
#include <KDE/KLocale>

#include <QApplication>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>

#include <QDebug>

static const int OSD_BRICK_WIDTH = 5;
static const int OSD_FRAME_WIDTH = 2;
static const int OSD_GAP_WIDTH = 3;

BE::Volume::OSD::OSD( QWidget *parent ) : QWidget(parent, Qt::Window|Qt::X11BypassWindowManagerHint|Qt::SplashScreen)
, myProgress(-1)
, myTarget(-1)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoChildEventsForParent);
    setWindowOpacity( 0.8 );
    hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    connect (hideTimer, SIGNAL(timeout()), this, SLOT(hide()));
}

void
BE::Volume::OSD::paintEvent(QPaintEvent *ev)
{
    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawRect(rect());
    p.setBrush(Qt::white);
    int x = OSD_FRAME_WIDTH, y, h = 11;
    y = height() - (h + OSD_FRAME_WIDTH);
    for (int i=0; i<myProgress; ++i)
    {
        p.drawRect(x,y,OSD_BRICK_WIDTH,h);
        x += OSD_BRICK_WIDTH + 2*OSD_FRAME_WIDTH + OSD_GAP_WIDTH;
        y -= 2;
        h += 2;
    }
    p.end();
}

void
BE::Volume::OSD::show(int total, int done)
{
    if (BE::Shell::hasFullscreenAction())
        return;

    TouchWheel::blockClose(true);
    if (myTarget != total)
    {
        myTarget = total;
        resize((total*(OSD_BRICK_WIDTH+2*OSD_FRAME_WIDTH))+((total-1)*OSD_GAP_WIDTH), 11+2*OSD_FRAME_WIDTH+2*total);
        QRect r = QApplication::desktop()->availableGeometry();
        move( r.x() + (r.width() - width())/2, r.y() + (r.height() - height())/2 );
//         setGeometry( r.x() + (r.width() - width())/2, r.y() + (r.height() - height())/2, width(), height() );

        QRegion mask;
        int x = 0, y, h = 11+2*OSD_FRAME_WIDTH;
        y = height() - h;
        for (int i=0; i<total; ++i)
        {
            mask |= QRect(x,y,OSD_BRICK_WIDTH + 2*OSD_FRAME_WIDTH,h);
            x += OSD_BRICK_WIDTH + 2*OSD_FRAME_WIDTH + OSD_GAP_WIDTH;
            y -= 2;
            h += 2;
        }
        setMask(mask);
    }
    QWidget::show();
    if (myProgress != done)
    {
        myProgress = done;
        update();
    }
    hideTimer->start(750);
    TouchWheel::blockClose(false);
}

BE::Volume::Volume( QWidget *parent ) : QLabel(parent), BE::Plugged( parent )
, myValue(-1)
, myUnmutedValue(-1)
{
    setObjectName("Volume");
    window()->setAttribute(Qt::WA_AlwaysShowToolTips);

    KAction *action = new KAction(this);
    action->setObjectName("BE::Volume::up");
    action->setGlobalShortcut(KShortcut(Qt::Key_VolumeUp));
    connect ( action, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(up()) );
    action = new KAction(this);
    action->setObjectName("BE::Volume::down");
    action->setGlobalShortcut(KShortcut(Qt::Key_VolumeDown));
    connect ( action, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(down()) );
    action = new KAction(this);
    action->setObjectName("BE::Volume::toggleMute");
    action->setGlobalShortcut(KShortcut(Qt::Key_VolumeMute));
    connect ( action, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(toggleMute()) );

    syncTimer = new QTimer(this);
    syncTimer->setSingleShot(true);
    connect ( syncTimer, SIGNAL(timeout()), this, SLOT(sync()) );

    myOSD = new OSD(desktop());

    QTimer::singleShot(0, this, SLOT(sync()));
}

void
BE::Volume::configure( KConfigGroup *grp )
{
    myChannel = grp->readEntry("Channel", "Master");
    myMixerCommand = grp->readEntry("MixerCommand", "kmix");
    if ((myStep = grp->readEntry("Step", 5)) < 1)
        myStep = 5; // no idiotic settings please...
    myPollInterval = grp->readEntry("PollInterval", 5000);
    if (myPollInterval)
        syncTimer->start(myPollInterval);
}

void
BE::Volume::mousePressEvent(QMouseEvent *ev)
{
    if (BE::Shell::touchMode())
    {
        if (TouchWheel::claimFor(this, SLOT(toggleMute()), true)) {
            TouchWheel::setIcons("audio-volume-low", "audio-volume-muted", "audio-volume-high");
            TouchWheel::show(popupPosition(TouchWheel::size()));
            return;
        }
    }
    if ( ev->button() == Qt::LeftButton )
        toggleMute();
    else if ( ev->button() == Qt::RightButton )
        launchMixer();
}


void BE::Volume::down()
{
    if (iAmMuted)
    {
        int oldValue = myUnmutedValue;
        myUnmutedValue = qMax(0, myUnmutedValue-myStep);
        if (oldValue != myUnmutedValue)
            updateValue();
    }
    else
        set(myStep, '-');
    if (sender())
        myOSD->show(100/myStep, myValue/myStep);
}

void BE::Volume::launchMixer()
{
    BE::Shell::run(myMixerCommand);
}


void BE::Volume::read(QProcess &amixer)
{
    int oldValue = myValue;

    const QStringList reply = QString(amixer.readAll()).split('[', QString::KeepEmptyParts);

    if ( reply.count() < 2 )
        return;

    int i = -1;
    QStringList::const_iterator it = reply.constBegin();
    while (++it != reply.constEnd())
    {
        if ((i = it->indexOf("%]")) > -1)
            break;
    }

    if (i<0)
        return;

    myValue = it->left(i).toInt();

    if (myValue > 0)
        iAmMuted = false;

    if (oldValue != myValue)
        updateValue();

    if (myPollInterval)
        syncTimer->start(myPollInterval);
}

void BE::Volume::set(int value, QChar dir)
{
    QProcess amixer;
    amixer.start("amixer", QStringList() << "sset" << myChannel << QString::number(value).append(dir) );
    if (amixer.waitForFinished())
        read(amixer);
}

void BE::Volume::sync()
{
    QProcess amixer;
    int oldValue = myValue;
    amixer.start("amixer", QStringList() << "get" << myChannel);
    if (amixer.waitForFinished())
        read(amixer);
    if (oldValue != myValue)
        updateValue();
}

void
BE::Volume::themeChanged()
{
    updateValue();
}

void
BE::Volume::toggleMute()
{
    bool wasMuted = iAmMuted;

    if (myValue > 0)
    {
        myUnmutedValue = myValue;
        set(0);
        if (myValue < 1)
            iAmMuted = true;
    }
    else
        set(myUnmutedValue);

    if (iAmMuted != wasMuted)
        updateValue();

    if (sender())
        myOSD->show(100/myStep, myValue/myStep);
}

void BE::Volume::updateValue()
{
    QIcon icn;
    int v = iAmMuted?myUnmutedValue:myValue;
    setToolTip(QString("%1%").arg(v));
    if (iAmMuted || v < 1)
        icn = themeIcon("audio-volume-muted");
    else if (v < 34)
        icn = themeIcon("audio-volume-low");
    else if (v < 67)
        icn = themeIcon("audio-volume-medium");
    else
        icn = themeIcon("audio-volume-high");
    int s = contentsRect().height();
    setPixmap(icn.pixmap(s, s));
}

void BE::Volume::up()
{
    if (iAmMuted && myUnmutedValue > 0)
        set(myUnmutedValue);
    else
        set(myStep, '+');
    if (sender())
        myOSD->show(100/myStep, myValue/myStep);
}

void
BE::Volume::wheelEvent(QWheelEvent *ev)
{
    if ( ev->delta() > 0 )
        up();
    else
        down();
    QToolTip::showText(QCursor::pos(), toolTip(), this);
}
