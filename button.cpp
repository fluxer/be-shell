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
#include "touchwheel.h"

#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KService>
#include <KDE/KStandardDirs>
#include <KDE/KToolInvocation>

#include <QApplication>
#include <QDBusConnection>
#include <QDesktopWidget>
#include <QFileSystemWatcher>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QtDBus>
#include <QStyleOptionToolButton>
#include <QTimerEvent>
#include <QWheelEvent>
#include <QProcess>
#include <QDebug>


BE::Button::Button( QWidget *parent, const QString &plugName ) : QToolButton(parent)
, BE::Plugged(parent, plugName)
, myMenu(0)
, myPulseIteration(0)
, myPulseLimit(0)
, myUpdaterTimeout(0)
, myAnimationTimer(0)
, myAnimationStep(0)
, myDBus(0)
, imNotReallyCrossed(false)
, iClickedForTouchInterface(false)
, myRenderTarget(0)
, myMenuWatcher(0)
, myLastIconSize(-1)
, myFixedIconSize(0)
{
    window()->setAttribute(Qt::WA_AlwaysShowToolTips);
    myBuffer[0] = myBuffer[1] = 0;
    new BE::ButtonAdaptor(this);
    setShortcut(QKeySequence());
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    if (orientation() == Qt::Vertical) {
        resize(parent->width(), parent->width());
    } else {
        resize(parent->height(), parent->height());
    }
}

void
BE::Button::configure( KConfigGroup *grp )
{
    disconnect( SIGNAL(clicked()) );
    if (!name().isEmpty()) {
        QString opath("/" + name());
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

    setText(grp->readEntry("Label", text()));
    setToolTip(grp->readEntry("ToolTip", QString()));
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
            if (myCommand == "BE::Config")
                setMenu(BE::Plugged::configMenu());
            else if (myCommand == "windowlist")
                setMenu(BE::Shell::windowList());
            else if (!myCommand.isEmpty()) {
                myMenu = new QMenu(this);
                updateMenu();
                setMenu(myMenu);
                setPopupMode( QToolButton::InstantPopup );
                myExe = grp->readEntry("MenuUpdater", QString());
                if (!myExe.isEmpty()) {
                    myUpdaterTimeout = grp->readEntry("MenuUpdaterTimeout", 2000);
                    connect (myMenu, SIGNAL(aboutToShow()), SLOT(updateMenu()));
                }
            }
        }
        else
            connect( this, SIGNAL(clicked()), this, SLOT(dbusCall()) );
    }
    else if (!connected)
        connect( this, SIGNAL(clicked()), this, SLOT(runCommand()) );

    if (!myIcon.isEmpty())
        setIcon(themeIcon(myIcon));
    setShortcut(QKeySequence()); // getrid of mnemonics

    setFixedIconSize(grp->readEntry("IconSize", 0));
}

void
BE::Button::setFixedIconSize(int size)
{
    myFixedIconSize = size;
    if (myFixedIconSize)
        setIconSize(QSize(myFixedIconSize, myFixedIconSize));
}

void
BE::Button::dbusCall()
{
    if (iClickedForTouchInterface)
        return;
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

static void merge(QPixmap *pix, QWidget *w, float opacity)
{
    // just assigning is broken on the raster engine, looses alpha
    QPixmap tmp(pix->size());
    tmp.fill(Qt::transparent);
    QPainter p(&tmp);
    p.drawPixmap(0,0, *pix);
    p.end();

    p.begin(&tmp);
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.fillRect(tmp.rect(), QColor(0,0,0, opacity*255.0));
    p.end();

    QPainter p2;
    p2.begin(w);
//     p2.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p2.drawPixmap(0, 0, tmp);
    p2.end();
}

void
BE::Button::paintEvent(QPaintEvent *pe)
{
    if (myRenderTarget)
    {   // QStyleSheet redirects itself, so to render our own buffer, we got to do that here
        QToolButton::render( myRenderTarget );
    }
    else if (myAnimationTimer)
    {
        const int step = qAbs(myAnimationStep);
        if (myBuffer[0])
            merge(myBuffer[0], this, (6-step)/6.0);
        if (myBuffer[1])
            merge(myBuffer[1], this, step/6.0);
    }
    else
        QToolButton::paintEvent(pe);
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

static const int iconSizes[10] = {16, 22, 32, 48, 64, 72, 96, 128, 256, 512};

void
BE::Button::resizeEvent(QResizeEvent */*re*/)
{
    if (myFixedIconSize)
        return;

    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    QSize bsz(0,0);
    if (toolButtonStyle() != Qt::ToolButtonIconOnly)
        bsz = fontMetrics().size(Qt::TextSingleLine, text());

    int l,t,w,h;
    BE::Shell::getContentsMargins(this, &l, &t, &w, &h);
    w = width() - (l+w);
    h = height() - (t+h);

    int s = 0;
    int i = 0;
    for (i = 0; i < 10; ++i) {
        QSize sz(bsz);
        if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
            sz = QSize(qMax(iconSizes[i], sz.width()), sz.height() + iconSizes[i]);
        } else if (toolButtonStyle() == Qt::ToolButtonTextBesideIcon) {
            sz = QSize(sz.width() + iconSizes[i], qMax(sz.height(), iconSizes[i]));
        } else if (toolButtonStyle() == Qt::ToolButtonIconOnly) {
            sz = QSize(iconSizes[i], iconSizes[i]);
        }
        sz = style()->sizeFromContents(QStyle::CT_ToolButton, &opt, sz, this); // 250x45 ...
        if (sz.width() > w || sz.height() > h || // requires more space than we have
            sz.width() < iconSizes[i] || sz.height() < iconSizes[i]) { // iconsize > maximum width/height
            if (i && iconSizes[i-1] < iconSize().width())
                s = iconSizes[i] - qMax(sz.width() - w, sz.height() - h);
//             if (i) --i; // greedy
            break;
        }
    }

    if (!s)
        s = iconSizes[i];
    if (myRecursionGuard.isValid() && myRecursionGuard.elapsed() < 30 && myLastIconSize == s) {
        s = (s + iconSize().width())/2; // there's a fight in the layout - seek for convergence
    }

    myLastIconSize = iconSize().width();
    setIconSize(QSize(s,s));
    myRecursionGuard.start();
}

void
BE::Button::runCommand()
{
    if (!iClickedForTouchInterface)
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
    if (!iClickedForTouchInterface)
        KToolInvocation::startServiceByDesktopName(myCommand);
}

void
BE::Button::themeChanged()
{
    if (!myIcon.isEmpty())
        setIcon(themeIcon(myIcon));
}

void
BE::Button::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == myAnimationTimer)
    {
        ++myAnimationStep;
        if (!myAnimationStep || myAnimationStep > 5)
        {
            killTimer(myAnimationTimer);
            if (!myBuffer[1]) {
                hide();
                emit fadedOut();
            }
            myAnimationTimer = 0;
            myAnimationStep = 0;
            delete myBuffer[0];
            delete myBuffer[1];
            myBuffer[0] = myBuffer[1] = 0;
        }
        repaint();
    }
    else
        QToolButton::timerEvent(te);
}

void
BE::Button::updateMenu()
{
    if (!myExe.isEmpty()) {
        delete myMenuWatcher;
        myMenuWatcher = 0;
        QProcess proc(this);
        proc.start(myExe);
        proc.waitForFinished(myUpdaterTimeout);
    }
    else {
        delete myMenuWatcher;
        myMenuWatcher = new QFileSystemWatcher(this);
        myMenuWatcher->addPath(KGlobal::dirs()->locate("data", "be.shell/" + myCommand + ".xml"));
        connect (myMenuWatcher, SIGNAL(fileChanged(const QString &)), SLOT(updateMenu()));
    }
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
    if (BE::Shell::touchMode() &&
        !(menu() && menu()->isVisible()) &&
        !(myWheel[0].isEmpty() && myWheel[1].isEmpty()) &&
        TouchWheel::claimFor(this, SIGNAL(clicked()), true))
    {
        iClickedForTouchInterface = true;
        TouchWheel::setIcons("media-seek-backward", myIcon, "media-seek-forward");
        TouchWheel::show(popupPosition(TouchWheel::size()));
    }
    QToolButton::mouseReleaseEvent(me);
    iClickedForTouchInterface = false;
}

void
BE::Button::createBuffer()
{
    // could happen when leaving present animation
    if (myBuffer[0] && myBuffer[1])
        return;

    delete myBuffer[0];
    delete myBuffer[1];
    myBuffer[0] = new QPixmap(size());
    myBuffer[1] = new QPixmap(size());
    myBuffer[0]->fill(Qt::transparent);
    myBuffer[1]->fill(Qt::transparent);

    const bool wasUnderMouse = underMouse();

    setAttribute(Qt::WA_UnderMouse, false);
    myRenderTarget = myBuffer[0];
    repaint();

    setAttribute(Qt::WA_UnderMouse, true);
    myRenderTarget = myBuffer[1];
    repaint();

    myRenderTarget = 0;
    setAttribute(Qt::WA_UnderMouse, wasUnderMouse);
}

void
BE::Button::fade(bool in)
{
    if (in)
        show();
    delete myBuffer[!in];
    myBuffer[!in] = 0;
    delete myBuffer[in];
    myBuffer[in] = new QPixmap(size());
    myBuffer[in]->fill(Qt::transparent);
    myRenderTarget = myBuffer[in];
    repaint();
    myRenderTarget = 0;
    myAnimationStep = 0;
    ++myAnimationStep; // instant reaction
    if (!myAnimationTimer)
        myAnimationTimer = startTimer(40);
}

void
BE::Button::enterEvent(QEvent *e)
{
    createBuffer();
    myAnimationStep = qAbs(myAnimationStep);
    ++myAnimationStep; // instant reaction
    if (!myAnimationTimer)
        myAnimationTimer = startTimer(40);
    QToolButton::enterEvent(e);
}

void
BE::Button::leaveEvent(QEvent *e)
{
    createBuffer();
    myAnimationStep = myAnimationStep ? -qAbs(myAnimationStep) : -6;
    ++myAnimationStep; // instant reaction
    if (!myAnimationTimer)
        myAnimationTimer = startTimer(40);
    QToolButton::leaveEvent(e);
}

bool
BE::Button::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Show) {
        if (o == menu())
            menu()->move(popupPosition(menu()->size()));
    } else if (e->type() == QEvent::Hide && o == menu())
        menu()->removeEventFilter(this);
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

