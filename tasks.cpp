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

#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTime>
#include <QTimer>
#include <KConfigGroup>
#include <KGlobalSettings>
#include <KIconLoader>
#include <KLocale>
#include <KWindowSystem>
#include <KWindowInfo>

#include "be.shell.h"
#include "tasks.h"

#include <QtDebug>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <QX11Info>
#include <netwm.h>

static Atom kdeWindowHighlight = 0;
static Atom netIconGeometry = 0;
QLabel *BE::Task::ourToolTip = 0;

#endif


static void highlightWindows(WId controller, const QList<WId> &ids)
{
#ifdef Q_WS_X11
    if (!ids.count())
    {
        XDeleteProperty(QX11Info::display(), controller, kdeWindowHighlight);
        return;
    }

    QVector<WId> wIDs = ids.toVector();
    if (wIDs.size())
        XChangeProperty(QX11Info::display(), controller, kdeWindowHighlight, kdeWindowHighlight, 32,
                        PropModeReplace, reinterpret_cast<unsigned char *>(wIDs.data()), wIDs.size());
#endif
}

static bool isOccluded(WId id)
{
    KWindowInfo i( id, NET::WMGeometry|NET::WMState|NET::XAWMState|NET::WMDesktop|NET::WMFrameExtents );
    const QList<WId> order = KWindowSystem::stackingOrder();
    QList<WId>::const_iterator it = order.constEnd();
    while (--it != order.constBegin())
    {
        if (*it == id)
            return false;

        KWindowInfo j( *it, NET::WMGeometry|NET::WMState|NET::XAWMState|NET::WMDesktop|NET::WMFrameExtents );
        const bool sameDesk =   j.desktop() == i.desktop() ||
                                j.desktop() == NET::OnAllDesktops || i.desktop() == NET::OnAllDesktops;
        if ( j.isMinimized() || !sameDesk || !i.frameGeometry().intersects(j.frameGeometry()) )
            continue;
        // now it's gonna be tricky - the windows intersect, but if one is keepAbove and the other
        // isn't, this will never change - same holds for the KeepBelow state
        // rest is weird logics
        bool occluded = true;
        if (j.hasState(NET::StaysOnTop))
            occluded = i.hasState(NET::StaysOnTop);
        if (i.hasState(NET::KeepBelow))
            occluded = j.hasState(NET::KeepBelow);
        return occluded;
    }
    return false;
}


BE::Task::Task(QWidget *parent, WId id, bool sticky, const QString &name) : BE::Button(parent, name)
{
    mySizeHintIsDirty = true;
    if (!kdeWindowHighlight)
        kdeWindowHighlight = XInternAtom(QX11Info::display(), "_KDE_WINDOW_HIGHLIGHT", False);
    if (!netIconGeometry)
        netIconGeometry = XInternAtom(QX11Info::display(), "_NET_WM_ICON_GEOMETRY", False);
    if (!ourToolTip) {
        ourToolTip = new QLabel(0, Qt::ToolTip);
        ourToolTip->setObjectName("TaskTip");
    }

    setSizePolicy(QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed ));
    setCheckable(true);
    setFont( KGlobalSettings::taskbarFont() );
    setPopupMode(QToolButton::DelayedPopup);

    if (id) {
        myWindows << id;
        setObjectName( "OneTask" );
        iAmMinimized = KWindowInfo(id, NET::XAWMState|NET::WMState).isMinimized();
    } else {
        setObjectName( "NoTask" );
        iAmMinimized = false;
    }

    iStick = sticky;
    iAmImportant = false;
    iAmDirty = false;
    const unsigned long props[2] = {NET::WMVisibleIconName|NET::WMIcon, NET::WM2WindowClass};
    update(props);
}

void
BE::Task::add(WId id) {
    if (myWindows.contains(id))
        return;
    bool wereFew = count() < 2;
    myWindows << id;
    if (count() > 1 && !menu())
    {
        setMenu(new QMenu(this));
        connect( menu(), SIGNAL(hovered(QAction*)), SLOT(highlightWindow(QAction*)) );
        connect( menu(), SIGNAL(aboutToHide()), SLOT(highlightAllOrNone()) );
    }
    myText = myGroup;
    setObjectName( count() > 1 ? "ManyTasks" : "OneTask" );
    const unsigned long props[2] = {NET::WMState|NET::XAWMState, 0};
    update(props);
    if (wereFew) {
        mySizeHintIsDirty = true;
        repolish();
    }
}

void
BE::Task::configure( KConfigGroup *grp )
{
    BE::Button::configure(grp);
    myText = myLabel = text();
}

void
BE::Task::enterEvent(QEvent *e)
{
    BE::Button::enterEvent(e);
    if (isSyntheticCrossing())
        return;

    if (true /*TODO add option*/)
    {
        if (count() < 1)
            ourToolTip->setText(myLabel);
        else if (count() < 2)
            ourToolTip->setText(myText);
        else
            ourToolTip->setText(QString(myLabel.isEmpty() ? myGroup : myLabel) + " (" + QString::number(count()) + ')');

        ourToolTip->adjustSize();
        ourToolTip->move(popupPosition(ourToolTip->size()));
        ourToolTip->show();
        ourToolTip->raise();
    }

    if (isRelevant())
        highlightWindows(window()->winId(), QList<WId>(myWindows) << ourToolTip->winId());
}

void
BE::Task::highlightAllOrNone()
{
    if (rect().contains(mapFromGlobal(QCursor::pos())))
        highlightWindows(window()->winId(), QList<WId>(myWindows) << ourToolTip->winId());
    else
        highlightWindows(window()->winId(), QList<WId>());
}

void
BE::Task::highlightWindow(QAction *a)
{
    if (a)
        highlightWindows(window()->winId(), QList<WId>() << (WId)a->data().toUInt() << menu()->winId() );
}

void
BE::Task::leaveEvent(QEvent *e)
{
    BE::Button::leaveEvent(e);
    if (isSyntheticCrossing())
        return;
    ourToolTip->hide();
    highlightWindows(window()->winId(), QList<WId>());
}

static QTime mouseDownTime;
void
BE::Task::mousePressEvent(QMouseEvent *me)
{
    ourToolTip->hide();
    switch (me->button())
    {
        case Qt::LeftButton:
            setDown(true);
            mouseDownTime.start();
//             BE::Button::mousePressEvent(me);
            break;
        case Qt::RightButton:
            if (count() > 1)
                showWindowList();
            else if (count())
                BE::Shell::showWindowContextMenu(myWindows.last(), me->globalPos());
            break;
        default:
            break;
    }
}

void
BE::Task::mouseReleaseEvent(QMouseEvent *me)
{
    int delay = mouseDownTime.elapsed();
//     qDebug() << iAmImportant;
    if (me->button() == Qt::LeftButton) {
        setDown(false);
        if ( iStick && (isEmpty() || delay > 200) ) {
            if ( rect().contains(me->pos()) )
                emit clicked();
        } else if (iAmImportant) {
//             qDebug() << "check for urgent window";
            foreach (WId id, myWindows) {
                if (KWindowInfo(id, NET::WMState).hasState(NET::DemandsAttention)) {
//                     qDebug() << "found" << id;
                    toggleState(id);
                    return;
                }
            }
        }
        else if (menu())
            showWindowList();
        else if (count())
            toggleState(myWindows.at(0));
    }
    else
        BE::Button::mouseReleaseEvent(me);
}

void
BE::Task::moveEvent(QMoveEvent *me)
{
    BE::Button::moveEvent(me);
    publishGeometry(QRect(mapToGlobal(QPoint(0,0)), size()));
}


void
BE::Task::publishGeometry(const QRect &r)
{
    if (!myWindows.count())
        return;
#ifdef Q_WS_X11
    const int32_t rect[4] = { r.x(), r.y(), r.width(), r.height() };
    foreach (WId id, myWindows)
        XChangeProperty(QX11Info::display(), id, netIconGeometry, XA_CARDINAL, 32, PropModeReplace, (uchar*)rect, 4);
#endif
}

bool
BE::Task::isOnCurrentDesktop() const
{
    for (QList<WId>::const_iterator it = myWindows.constBegin(), end = myWindows.constEnd(); it != end; ++it)
        if ( KWindowInfo(*it, NET::WMDesktop).isOnCurrentDesktop() )
            return true;
    return false;
}

WId
BE::Task::firstRelevant() const
{
    for (QList<WId>::const_iterator it = myWindows.constBegin(),  end = myWindows.constEnd(); it != end; ++it)
    {
        KWindowInfo info( *it, NET::WMDesktop | NET::WMState | NET::XAWMState );
        if (info.isOnCurrentDesktop() && !info.isMinimized())
            return *it;
    }
    return 0;
}

WId
BE::Task::lastRelevant()  const
{
    for (QList<WId>::const_iterator it = myWindows.constEnd(),  first = myWindows.constBegin(); it != first;)
    {
        --it;
        KWindowInfo info( *it, NET::WMDesktop | NET::WMState | NET::XAWMState );
        if (info.isOnCurrentDesktop() && !info.isMinimized())
            return *it;
    }
    return 0;
}

bool
BE::Task::remove(WId id)
{
    if (!count())
        return false;

    bool change = myWindows.removeAll(id);
    if (!change)
        return change;
    if (count() < 2 && menu() ) {
        delete menu();
        setMenu(0);
    }
    if (count()) {
        if (count() == 1) {
            setObjectName( "OneTask" );
            myText = resortedText(KWindowInfo(myWindows.at(0), NET::WMVisibleIconName).visibleIconName());
            mySizeHintIsDirty = true;
            repolish();
        }
        const unsigned long props[2] = {NET::WMState|NET::XAWMState, 0};
        update(props);
    } else {
        myText = myLabel;
        mySizeHintIsDirty = true;
        setObjectName( "NoTask" );
        setActive(false);
        setProperty("windowMinimized", false);
        setProperty("needsAttention", false);
        repolish();
    }
    return change;
}

void
BE::Task::repolish()
{
    if (!iAmDirty)
        QMetaObject::invokeMethod(this, "_repolish", Qt::QueuedConnection);
    iAmDirty = true;
}

void
BE::Task::_repolish()
{
    iAmDirty = false;
    style()->unpolish(this);
    style()->polish(this);
}

void
BE::Task::resizeEvent(QResizeEvent *re)
{
    BE::Button::resizeEvent(re);
    if (toolButtonStyle() == Qt::ToolButtonIconOnly || toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
        int s = qMin(width(), height());
        if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon)
            s -= fontMetrics().height() + 4;
        setIconSize(QSize(s,s));
        mySizeHintIsDirty = true;
    } else {
//         int w = fontMetrics().width(text());
//         if (w > width() || w + 16 < width())
            setText(squeezedText(myText));
    }
    publishGeometry(QRect(mapToGlobal(QPoint(0,0)), size()));
}

void
BE::Task::wheelEvent(QWheelEvent *ev)
{
    ourToolTip->hide();
    int i = myWindows.indexOf(KWindowSystem::activeWindow());
    if (i > -1)
    {
        if (ev->delta() > 0)
            ++i;
        else
        {
            if (i == 0)
                i = myWindows.count();
            else
                --i;
        }
        if (i < myWindows.count())
        {
            WId id = myWindows.at(i);
            KWindowSystem::forceActiveWindow(id);
            highlightWindows(window()->winId(), QList<WId>() << id);
            ev->accept();
            return;
        }
    }
    ev->ignore(); // pass on to taskbar
}


void
BE::Task::setSticky(bool stick)
{
#if 0
    if (!myContextWindow || stick == iStick)
        return;
    if ((iStick = stick))
        setCommand(BE::Shell::executable(myContextWindow));
    else
    {
        // TODO: remove from config
    }
#endif
}

void
BE::Task::setToolButtonStyle(Qt::ToolButtonStyle tbs)
{
    BE::Button::setToolButtonStyle(tbs);
    int s;
    if (tbs == Qt::ToolButtonIconOnly)
        s = qMin(width(), height());
    else
        s = style()->pixelMetric( QStyle::PM_ToolBarIconSize, 0L, this);
    setIconSize(QSize(s,s));
    mySizeHintIsDirty = true;
}


QSize
BE::Task::sizeHint() const
{
    Task *that = const_cast<BE::Task*>(this);
    if (mySizeHintIsDirty) {
        if (toolButtonStyle() == Qt::ToolButtonIconOnly)
            that->mySizeHint = iconSize();
        else {
            that->mySizeHint = fontMetrics().size(Qt::TextSingleLine|Qt::TextShowMnemonic, myText);
            if (toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
                that->mySizeHint.setWidth(qMax(mySizeHint.width(), iconSize().width()) + 4);
                that->mySizeHint.setHeight(mySizeHint.height() + iconSize().height() + 4);
            }
            else if (toolButtonStyle() == Qt::ToolButtonTextBesideIcon) {
                that->mySizeHint.setWidth(mySizeHint.width() + iconSize().width() + 12);
                that->mySizeHint.setHeight(qMax(mySizeHint.height(), iconSize().height()) + 4);
            }
        }
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.text = myText;
        that->mySizeHint = style()->sizeFromContents(QStyle::CT_ToolButton, &opt, mySizeHint, this);
        that->mySizeHint.setWidth(qMin(qMax(100,mySizeHint.width()),250));
        that->mySizeHintIsDirty = false;
    }
    return mySizeHint;
}

void
BE::Task::showWindowList()
{
    if (!menu())
        return;
    BE::Shell::populateWindowList( myWindows,  menu(), false );
    menu()->adjustSize();
    menu()->exec(popupPosition(menu()->size()));
}

// trimming adapted from my bespin kwin client

static bool
isBrowser(const QString &s)
{
    return
    s.contains("konqueror", Qt::CaseInsensitive) ||
    s.contains("opera", Qt::CaseInsensitive) ||
    s.contains("rekonq", Qt::CaseInsensitive) ||
    s.contains("qupzilla", Qt::CaseInsensitive) ||
    s.contains("arora", Qt::CaseInsensitive) ||
    s.contains("firefox", Qt::CaseInsensitive) ||
    s.contains("leechcraft", Qt::CaseInsensitive) ||
    s.contains("mozilla", Qt::CaseInsensitive) ||
    s.contains("chromium", Qt::CaseInsensitive) ||
    s.contains("safari", Qt::CaseInsensitive); // just in case ;)
}



QString
BE::Task::resortedText(const QString &text)
{
    QString resortedText = text;

    if ( myWindows.count() == 1) {
        static QStringList kwin_seps;
        if (kwin_seps.isEmpty())
            kwin_seps << " - " << QString(" %1 ").arg(QChar(0x2013)) << // recently used utf-8 dash
                                   QString(" %1 ").arg(QChar(0x2014)); // trojitá uses an em dash ...

        QString appName;
        foreach (const QString &s, kwin_seps) {
            if (resortedText.contains(s)) {
                QStringList tokens = resortedText.split(s, QString::SkipEmptyParts);
                appName = tokens.last() + s;
                tokens.removeLast();
                resortedText = tokens.join(s);
                break;
            }
        }

        if (isBrowser(resortedText)) {
            int n = qMin(3, resortedText.count(" - "));
            if (n--) // select last three if 4 or more sects, prelast two otherwise
                resortedText = resortedText.section(" - ", -3, n-3, QString::SectionSkipEmpty);
        }

        if (resortedText.contains(": "))
            resortedText = resortedText.section(": ", 1, -1, QString::SectionSkipEmpty );

        if (resortedText.contains("http://"))
            resortedText = resortedText.remove("http://", Qt::CaseInsensitive)/*.section()*/;

        resortedText = appName + resortedText;

        // in general, remove leading and ending blanks...
        resortedText = resortedText.trimmed();

        if (resortedText.isEmpty()) // were too aggressive ...
            resortedText = text.trimmed();
    }

    return resortedText;
}

QString
BE::Task::squeezedText(const QString &text)
{
    int buttonWidth = contentsRect().width();
    if (toolButtonStyle() == Qt::ToolButtonTextBesideIcon) {
        buttonWidth -= 4 + iconSize().width(); // TODO: "4+2" is a guess
    }

    QFontMetrics fm(fontMetrics());
    if (fm.width(text) > buttonWidth)
        return fm.elidedText(text, Qt::ElideRight, buttonWidth - 2 );
    return text;
}


void
BE::Task::toggleState(WId id)
{
    if (id != KWindowSystem::activeWindow())
        KWindowSystem::forceActiveWindow(id);
    else if (isOccluded(id))
        KWindowSystem::raiseWindow(id);
    else
    {
        highlightWindows(window()->winId(), QList<WId>());
        KWindowSystem::minimizeWindow(id);
    }
}

void
BE::Task::toggleSticky()
{
    setSticky(!iStick);
}

//NOTICE that we actually only get WMName state changes what seems a BUG that we WORKAROUND
static const unsigned long s_nameProps = NET::WMVisibleIconName|NET::WMVisibleName|
                                           NET::WMIconName|NET::WMName;
static const unsigned long s_relT1Props = s_nameProps|NET::WMIcon|NET::XAWMState|
                                            NET::WMState|NET::DemandsAttention;

void
BE::Task::update(const unsigned long *properties)
{
    //     WM2UserTime
    //     WM2StartupId
    //     WM2TransientFor
    //     WM2GroupLeader
    //     WM2AllowedActions
    //     WMDesktop

    const unsigned long props[2] = { properties[0] & s_relT1Props, properties[1] & NET::WM2WindowClass };
    if (!(props[0] || props[1]))
        return;

    const WId id = isEmpty() ? 0 : myWindows.last();
    if (id)
    {
        NETWinInfo info(QX11Info::display(), id, QX11Info::appRootWindow(), props, 2);
        if (props[1] & NET::WM2WindowClass) {
            myGroup = info.windowClassName();
            if (myGroup.isEmpty())
                myGroup = info.windowClassClass();
        }

        if (props[0] & s_nameProps) { // NOTICE info.visibleIconName() is an empty bytearray!
            myText = count() > 1 ? myGroup :
                            resortedText(KWindowInfo(id, NET::WMVisibleIconName).visibleIconName());
            mySizeHintIsDirty = true;
            const int oldWidth = width();
            setText(myText);
            if (width() == oldWidth && oldWidth == 250) // didn't trigger resize cause we're capped
                setText(squeezedText(myText));
        }

        if (props[0] & (NET::WMState|NET::XAWMState)) {
            const bool wasImportant = iAmImportant;
            if (count()) {
                const bool wasMinimized = iAmMinimized;
                iAmMinimized = !iAmMinimized;
                iAmImportant = false;
                foreach (WId id, myWindows) {
                    KWindowInfo info(id, NET::XAWMState|NET::WMState);
                    if (info.hasState(NET::DemandsAttention))
                        iAmImportant = true;
                    if (info.isMinimized() == wasMinimized)
                        iAmMinimized = wasMinimized;
                }
                if (iAmMinimized != wasMinimized) {
                    setProperty("windowMinimized", iAmMinimized);
                    repolish();
                }
                if (wasImportant != iAmImportant) {
                    setProperty("needsAttention", iAmImportant);
                    repolish();
                }
                if (iAmImportant && !wasImportant)
                    requestAttention(60);
                else if (!iAmImportant && wasImportant)
                    requestAttention(0);
            }
        }
    }
    else
        setText(myLabel);

    if (props[0] & NET::WMIcon)
    {
        QIcon icn = BE::Plugged::themeIcon(exe().isEmpty() ? myGroup : exe(), false);
        if (icn.isNull() && id)
            icn.addPixmap( KWindowSystem::icon(id) );
        if (!icn.isNull())
            setIcon( icn );
    }
}

// -------------------------------

BE::Tasks::Tasks(QWidget *parent) : QFrame(parent), BE::Plugged(parent)
{
    iStackNow = iStack = true;
    setLayout(new QBoxLayout(QBoxLayout::LeftToRight, this));
    layout()->setAlignment(Qt::AlignCenter);
    layout()->setContentsMargins(0, 0, 0, 0);

    setFocusPolicy( Qt::ClickFocus );

    myOrientation = Plugged::orientation();

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect( parent, SIGNAL(orientationChanged(Qt::Orientation)), SLOT(orientationChanged(Qt::Orientation)) );
    connect( KWindowSystem::self(), SIGNAL(windowAdded(WId)), SLOT( addWindow(WId)) );
    connect( KWindowSystem::self(), SIGNAL(activeWindowChanged(WId)), SLOT( windowActivated(WId)) );
    connect( KWindowSystem::self(), SIGNAL(windowRemoved(WId)), SLOT( removeWindow(WId)) );
    connect( KWindowSystem::self(), SIGNAL(windowChanged(WId,const unsigned long*)), SLOT(updateWindowProperties(WId,const unsigned long*)) );
    connect( KWindowSystem::self(), SIGNAL(currentDesktopChanged(int)), SLOT(setCurrentDesktop(int)) );

    QTimer *sanityTimer = new QTimer(this);
    connect( sanityTimer, SIGNAL(timeout()), SLOT(checkSanity()) );
    sanityTimer->start(10000);

}

static const unsigned long
supported_types = NET::Unknown | NET::NormalMask | NET::DialogMask | NET::UtilityMask;

BE::Task*
BE::Tasks::addWindow( WId id )
{
    foreach ( QWidget *w, QApplication::topLevelWidgets() )
        if ( w->testAttribute(Qt::WA_WState_Created) && w->internalWinId() && w->winId() == id )
            return 0;

    KWindowInfo info( id, NET::WMWindowType|NET::WMState, NET::WM2WindowClass );
    NET::WindowType type = info.windowType( NET::AllTypesMask );
    if ( !((1<<type) & supported_types) ) // everything that's not a supported_type
        return 0;
    if (info.state() & NET::SkipTaskbar)
        return 0;

    if (!myWindows.contains(id))
        myWindows << id;
    if (iStackNow || hasStickies)
    {
        QString newClass = info.windowClassName();
        if (newClass.isEmpty())
            newClass = info.windowClassClass();
        const QString newExe = BE::Shell::executable(id);
        foreach (Task *t, myTasks) {
            if (!(iStackNow || t->isSticky()))
                continue;
            if (newClass.compare(t->group(), Qt::CaseInsensitive) && newExe.compare(t->exe(), Qt::CaseInsensitive) &&
                newClass.compare(t->exe(), Qt::CaseInsensitive) && newExe.compare(t->group(), Qt::CaseInsensitive))
                continue;
            // found one, add and outa here
            t->add(id);
            updateVisibility(t);
//             t->setToolButtonStyle(myButtonMode);
            return 0;
        }
    }

    Task *newTask = new Task(this, id);
    newTask->setToolButtonStyle(myButtonMode);
    layout()->addWidget(newTask);
    myTasks << newTask;
    updateVisibility(newTask);
    return newTask;
}

void
BE::Tasks::configure( KConfigGroup *grp )
{
//     foreach (Task *task, myTasks)
//         delete task;
    qDeleteAll(myTasks);
    myTasks.clear();
    myWindows.clear();

    iSeparateDesktops = grp->readEntry("SeparateDesktops", false);
    myButtonMode = (Qt::ToolButtonStyle)grp->readEntry("ButtonMode", (int)Qt::ToolButtonTextOnly);

    KConfigGroup siblings = grp->parent();
    hasStickies = false;
    if (siblings.isValid())
    {
        QStringList sticks = grp->readEntry("Buttons", QStringList());
        int n = 0;
        foreach (QString sticky, sticks)
        {
            QString type;
            KConfigGroup config = siblings.group(sticky);
            if (config.exists())
                type = config.readEntry("Type", QString());
            if (!type.compare("Button", Qt::CaseInsensitive))
            {
                Task *task = new Task(this, 0, true, sticky);
                task->configure(&config);
                task->setObjectName("NoTask");
//                 task->setToolButtonStyle(Qt::ToolButtonIconOnly);
                myTasks << task;
                static_cast<QBoxLayout*>(layout())->insertWidget(n++, task, 1);
            }
        }
        hasStickies = n;
    }

    iStackNow = iStack = grp->readEntry("AlwaysGroup", false);
    iIgnoreVisible = grp->readEntry("OnlyMinimized", false);
    iSeparateDesktops = grp->readEntry("OnlyCurrentDesk", false);
    iSeparateScreens = !hasStickies && grp->readEntry("OnlyCurrentScreen", false);
    static_cast<QBoxLayout*>(layout())->setSpacing(grp->readEntry("Spacing", 2));

    foreach (WId id, KWindowSystem::windows())
        addWindow(id);
}

void
BE::Tasks::orientationChanged(Qt::Orientation o)
{
    myOrientation = o;
    static_cast<QBoxLayout*>(layout())->setDirection(o == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom );
}

void
BE::Tasks::removeWindow( WId id )
{
    myWindows.removeOne(id); // "removeAll" -- addWindow checks for doublettes
    TaskList::iterator it = myTasks.begin();
    while (it != myTasks.end())
    {
        if ((*it)->remove(id))
        {
            if ((*it)->isEmpty())
            {
//                 if ((*it)->isSticky())
//                     (*it)->setToolButtonStyle(Qt::ToolButtonIconOnly);
//                 else
                if (!(*it)->isSticky())
                {
                    delete *it;
                    myTasks.erase(it);
                }
            }
            else
                updateVisibility(*it);
            return;
        }
        ++it;
    }
}


void
BE::Tasks::setCurrentDesktop(int /*desktop*/)
{
    if (iSeparateDesktops)
    {
        foreach (Task *t, myTasks)
            updateVisibility(t);
    }
}

void
BE::Tasks::setStyle()
{

}

void
BE::Tasks::updateWindowProperties(WId id, const unsigned long *properties)
{
    if (properties[1] & NET::WM2WindowClass || properties[0] & NET::WMPid)
    {
        removeWindow( id );
        addWindow( id );
        return;
    }

    foreach (Task *t, myTasks)
        if (t->contains(id)) {
            const unsigned long props[2] = { properties[0] & s_relT1Props, properties[1] & 0 };
            t->update(props);
            if (properties[0] & (NET::WMDesktop|NET::WMState|NET::XAWMState))
                updateVisibility(t);
            break;
        }
}

void
BE::Tasks::leaveEvent(QEvent */*e*/)
{
    highlightWindows(window()->winId(), QList<WId>());
}

void
BE::Tasks::wheelEvent( QWheelEvent *we )
{
    if (myTasks.isEmpty())
        return;

    WId activeId = KWindowSystem::activeWindow();
    QList<Task*>::const_iterator it = myTasks.constBegin();
    while (it != myTasks.constEnd() && !(*it)->contains(activeId) /*isActive() */)
        ++it;
    if (it == myTasks.constEnd())
    {
        if (we->delta() > 0 )
            it = myTasks.constBegin();
        else
            --it;
    }

    QList<Task*>::const_iterator active = it;
    if ( (*active)->count() > 1 &&
         ((we->delta() > 0 && activeId != (*active)->lastRelevant()) ||
         (we->delta() < 0 && activeId != (*active)->firstRelevant())) )
    {
        QCoreApplication::sendEvent(*active, we);
        return;
    }

    while (true)
    {
        if (we->delta() > 0)
        {
            ++it;
            if (it == myTasks.constEnd())
                it = myTasks.constBegin();
        }
        else
        {
            if (it == myTasks.constBegin())
                it = myTasks.constEnd();
            --it;
        }
        if (it == active)
            return; // none found

        if ((*it)->isRelevant())
            break;
    }
    if ((activeId = (we->delta() > 0) ? (*it)->firstRelevant() : (*it)->lastRelevant()))
    {
        KWindowSystem::forceActiveWindow(activeId);
        highlightWindows(window()->winId(), QList<WId>() << activeId);
    }
}

void
BE::Tasks::updateVisibility(Task *t)
{
    if (t->isSticky())
        return; // stickies are always visible

    bool vis = true;
    if (iSeparateDesktops)
        vis = t->isOnCurrentDesktop();
    if (vis && iIgnoreVisible)
        vis = t->isMinimized();
    t->setVisible(vis);
}

void
BE::Tasks::windowActivated( WId id )
{
    foreach (Task *t, myTasks)
        t->setActive(t->contains(id));
}

void
BE::Tasks::checkSanity()
{
    const QList<WId> windows = KWindowSystem::windows();
    foreach (WId id, myWindows)
        if (!windows.contains(id))
            removeWindow(id);
    foreach (WId id, windows)
        if (!myWindows.contains(id))
            addWindow(id);
}
