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

#include "systray.h"
#include "flowlayout.h"

#include <unistd.h>

#define LABEL true
#if LABEL
#define ICON_BASE QLabel
#include <QLabel>
#else
#define ICON_BASE QToolButton
#include <QToolButton>
#endif

#include <QAction>
#include <QDesktopWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QX11EmbedWidget>
#include <QX11Info>

#include <QtDebug>

#include <KDE/KApplication>
#include <KDE/KConfigGroup>
#include <kwindowsystem.h>
#include <netwm.h>

#include <X11/Xlib.h>
// #include <X11/extensions/XTest.h>
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

Atom net_opcode_atom;
Atom net_selection_atom;
Atom net_manager_atom;
Atom net_message_data_atom;

namespace BE {

class X11EmbedContainer : public QX11EmbedContainer {
public:
    X11EmbedContainer(QWidget *parent) : QX11EmbedContainer(parent) {}
    void embed(WId clientId)
    {
        Display *display = QX11Info::display();
        XWindowAttributes attr;
        if (!XGetWindowAttributes(display, clientId, &attr))
            return;

        XSetWindowAttributes sAttr;
        sAttr.background_pixel = BlackPixel(display, DefaultScreen(display));
        sAttr.border_pixel = BlackPixel(display, DefaultScreen(display));
        sAttr.colormap = attr.colormap;

        WId parentId = parentWidget() ? parentWidget()->winId() : DefaultRootWindow(display);
        Window winId = XCreateWindow(display, parentId, 0, 0, attr.width, attr.height,
                                        0, attr.depth, InputOutput, attr.visual,
                                        CWBackPixel | CWBorderPixel | CWColormap, &sAttr);

        if (!XGetWindowAttributes(display, winId, &attr))
            return;

        create(winId);

        // repeat everything from QX11EmbedContainer's ctor that might be relevant
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAcceptDrops(true);
        setEnabled(false);

        XSelectInput(display, winId,
                    KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask | ButtonMotionMask |
                    KeymapStateMask |
                    PointerMotionMask |
                    EnterWindowMask | LeaveWindowMask |
                    FocusChangeMask |
                    ExposureMask |
                    StructureNotifyMask |
                    SubstructureNotifyMask);

//         XFlush(display);
        XSync(display, False);

        embedClient(clientId);
    }
};

class SysTrayIcon : public ICON_BASE
{
public:
    SysTrayIcon(WId id, SysTray *parent) : ICON_BASE(parent), nasty(false), fallback(false)
    {
        setContentsMargins(0, 0, 0, 0);
#if LABEL
        setScaledContents(true);
#endif
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMinimumSize(16,16);
        setFocusPolicy(Qt::ClickFocus);
        
        iName = KWindowInfo(id, NET::WMName).name();

        bed = new X11EmbedContainer( this );
        bed->setAttribute(Qt::WA_TranslucentBackground, false);
        bed->setAttribute(Qt::WA_DontCreateNativeAncestors, false);
        bed->setAttribute(Qt::WA_NativeWindow, true);

        hide();
        connect(bed, SIGNAL(clientIsEmbedded()), parent, SLOT(requestShowIcon()));
        connect(bed, SIGNAL(clientClosed()), this, SLOT(deleteLater())); // this? leeds to problems with e.g. KMail
        bed->embed(id);
        bed->setParent(this);
//         bed->show();
        bed->setFixedSize(0,0);
//         bed->setUpdatesEnabled(false);
//         bed->installEventFilter(this);
        setFocusProxy(bed);

//         XResizeWindow(QX11Info::display(), cId(), width(), height() );
//         XMapRaised(QX11Info::display(), cId());

        wmPix = KWindowSystem::icon(id, 128, 128, false );
        updateIcon();
    }
//     ~SysTrayIcon() {
//         if (mouseGrabber() == this)
//             releaseMouse();
//     }
    inline WId cId() { return bed->clientWinId(); }
    inline bool isOutdated() { return !bed->clientWinId(); }
    const QString &name() { return iName; }
    void setFallBack( bool on )
    {
        fallback = on;
        fallback ? bed->setFixedSize(size()) : bed->setFixedSize(0,0);
    }
    inline static void setSize(int s) { ourSize = s; };
    void updateIcon()
    {
        QIcon icn = BE::Plugged::themeIcon(iName, false);
#if LABEL
        setPixmap(icn.isNull() ? wmPix : icn.pixmap(128));
#else
        setIcon(icn.isNull() ? wmPix : icn);
#endif
    }
    bool nasty, fallback;
protected:
    void enterEvent( QEvent *ev )
    {
        ICON_BASE::enterEvent( ev );
        XCrossingEvent e; xCrossEvent(&e); e.type = EnterNotify;
        sendXEvent(EnterWindowMask, (XEvent*)&e);
    }
//     bool eventFilter(QObject *o, QEvent *e)
//     {
//         if (o == bed && e->type() == QEvent::FocusOut)
//         {
//             qWarning("bed lost focus");
//             XTestFakeKeyEvent(QX11Info::display(), XK_Escape, true, 0);
//             XTestFakeKeyEvent(QX11Info::display(), XK_Escape, false, CurrentTime);
//         }
//         return false;
//     }
    void leaveEvent( QEvent *ev )
    {
        ICON_BASE::leaveEvent( ev );
        XCrossingEvent e; xCrossEvent(&e);
        e.type = LeaveNotify;
        sendXEvent(LeaveWindowMask, &e);
    }
    void mousePressEvent( QMouseEvent *me)
    {
        // NOTICE: this does not work because oc. the popup still needs the mouse.
//         if (mouseGrabber() == this)
//             releaseMouse();
//         else if (me->button() == Qt::RightButton) // let's assume this toggles a popup
//             grabMouse();
        me->accept();
#if ! LABEL
        ICON_BASE::mousePressEvent( me );
#endif
        XButtonEvent e;
        xButtonEvent( &e, xButton(me->button()), me->globalPos() );
        e.type = ButtonPress;
        sendXEvent(ButtonPressMask, &e);
    }
    void mouseReleaseEvent( QMouseEvent *me)
    {
        bed->setFocus(Qt::MouseFocusReason);
        me->accept();
#if ! LABEL
        ICON_BASE::mouseReleaseEvent( me );
#endif
        XButtonEvent e; xButtonEvent( &e, xButton(me->button()), me->globalPos() ); e.type = ButtonRelease;
        sendXEvent(ButtonReleaseMask, &e);
    }

    void resizeEvent( QResizeEvent *re )
    {
        if ( width() == height() )
        {
            ICON_BASE::resizeEvent(re);
//             XResizeWindow(QX11Info::display(), cId(), width(), height() );
//             XMapRaised(QX11Info::display(), cId());
            if (fallback)
                bed->setFixedSize(size());
        }
        else if (width() > height())
            setMaximumWidth(height());
        else
            setMaximumHeight(width());
    }

    void wheelEvent ( QWheelEvent *we )
    {
        we->accept();
        bool h = we->orientation() == Qt::Horizontal;
        XButtonEvent e; xButtonEvent( &e, (we->delta() > 0 ? 4 : 5) + h, we->globalPos() );
        e.type = ButtonPress;
        XSendEvent(QX11Info::display(), cId(), True, ButtonPressMask, (XEvent*)&e);
        usleep(1000);
        e.type = ButtonRelease;
        sendXEvent(ButtonReleaseMask, &e);
    }
private:
    void sendXEvent( int mask, void *ev)
    {
        XSendEvent(QX11Info::display(), cId(), True, mask, (XEvent*)ev);
        XSync(QX11Info::display(), True);
    }
    int xButton (Qt::MouseButton btn)
    {
        switch (btn)
        {
            default:
            case Qt::NoButton: return AnyButton;
            case Qt::LeftButton: return Button1;
            case Qt::RightButton: return Button3;
            case Qt::MidButton: return Button2;
            case Qt::XButton1: return Button4;
            case Qt::XButton2: return Button5;
        }
    }
    void xButtonEvent( XButtonEvent *xe, int button, const QPoint &gPos  )
    {
        xe->button = button;
        xe->display = QX11Info::display();
        xe->state = 0;
        xe->window = cId();
//         xe->serial = 0;
//         xe->xkey.state;
        xe->send_event = True;
        xe->same_screen = True;
        xe->root = QX11Info::appRootWindow();
        xe->subwindow = None;
        xe->time = QX11Info::appTime();
        xe->x_root = gPos.x();
        xe->y_root = gPos.y();
        xe->x = 1;
        xe->y = 1;
    }
    void xCrossEvent( XCrossingEvent *xe )
    {
        xe->display = QX11Info::display();
        xe->window = cId();
        xe->root = QX11Info::appRootWindow();
        xe->subwindow = None;
        xe->time = QX11Info::appTime();
        xe->x = 1;
        xe->y = 1;
        QPoint pt = mapToGlobal(QPoint(1,1));
        xe->x_root = pt.x();
        xe->y_root = pt.y();
        xe->mode = NotifyNormal;
        xe->detail = NotifyNonlinear;
        xe->same_screen = True;
        xe->focus = True;
        xe->state = 0;
        xe->send_event = True;
    }
protected:
    static int ourSize;
    QString iName;
    X11EmbedContainer *bed;
    QPixmap wmPix;
    bool iGrabTheMouse;
};

}

BE::SysTray::SysTray(QWidget *parent) : QFrame(parent), BE::Plugged(parent), nastyOnesAreVisible(false)
{
    setObjectName("SystemTray");
    myConfigMenu = configMenu()->addMenu("SystemTray");
    QAction *act = myConfigMenu->addAction( "Show nasty ones" );
    act->setCheckable(true);
    connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleNastyOnes(bool)));
    myConfigMenu->addAction( "Configure...", this, SLOT(configureIcons()) );
    healthTimer = new QTimer(this);
//     connect (healthTimer, SIGNAL(timeout()), this, SLOT(selfCheck()));

    FlowLayout *l = new FlowLayout(this);
    l->setContentsMargins(3, 1, 3, 1);
    l->setSpacing(2);
    QTimer::singleShot(0, this, SLOT(init()));
}

BE::SysTray::~SysTray() {
    delete myConfigMenu;
}

void
BE::SysTray::init()
{
     // Freedesktop.org system tray support
    char name[20] = {0};
    qsnprintf(name, 20, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(QX11Info::display()));
    net_selection_atom = XInternAtom(QX11Info::display(), name, FALSE);
    net_opcode_atom = XInternAtom(QX11Info::display(), "_NET_SYSTEM_TRAY_OPCODE", FALSE);
    net_manager_atom = XInternAtom(QX11Info::display(), "MANAGER", FALSE);
    net_message_data_atom = XInternAtom(QX11Info::display(), "_NET_SYSTEM_TRAY_MESSAGE_DATA", FALSE);

    XSetSelectionOwner(QX11Info::display(), net_selection_atom, winId(), CurrentTime);

    if (XGetSelectionOwner(QX11Info::display(), net_selection_atom) == winId())
    {
        XClientMessageEvent xev;

        xev.type = ClientMessage;
        xev.window = QApplication::desktop()->winId();
        xev.message_type = net_manager_atom;
        xev.format = 32;
        xev.data.l[0] = CurrentTime;
        xev.data.l[1] = net_selection_atom;
        xev.data.l[2] = winId();
        xev.data.l[3] = 0;        /* manager specific data */
        xev.data.l[4] = 0;        /* manager specific data */

        XSendEvent(QX11Info::display(), QApplication::desktop()->winId(), False, StructureNotifyMask, (XEvent *)&xev);
    }
//     healthTimer->start(40000);
}

void
BE::SysTray::configure( KConfigGroup *grp )
{
    int spacing = grp->readEntry("Spacing", 2);
    if (spacing < 0)
        spacing = 2;
    static_cast<FlowLayout*>(layout())->setSpacing(spacing);
    QStringList oldOnes = nastyOnes;
    nastyOnes = grp->readEntry("NastyIcons", QStringList());
    if (nastyOnes != oldOnes)
        toggleNastyOnes(nastyOnesAreVisible);
    oldOnes = fallbackOnes;
    fallbackOnes = grp->readEntry("FallbackIcons", QStringList());
    if (fallbackOnes != oldOnes)
        updateFallbacks();
}

void
BE::SysTray::requestShowIcon()
{
    BE::SysTrayIcon *icon = dynamic_cast<BE::SysTrayIcon*>(sender());
    if (!icon)
        return;
    if (icon->isOutdated())
    {
        icon->deleteLater();
        return;
    }
    icon->nasty = nastyOnes.contains(icon->name());
    if (nastyOnesAreVisible || !icon->nasty)
        icon->show();
}

void
BE::SysTray::selfCheck()
{
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon)
        {
            i = myIcons.erase(i);
            continue;
        }
        else if (icon->isOutdated())
        {
            qDebug() << "dead" << icon->name();
            icon->hide();
            icon->deleteLater();
            i = myIcons.erase(i);
            continue;
        }
        else
            icon->updateIcon();
        ++i;
    }
}

void
BE::SysTray::themeChanged()
{
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon)
        {
            i = myIcons.erase(i);
            continue;
        }
        else if (icon->isOutdated())
        {
            icon->hide();
            icon->deleteLater();
        }
        else
            icon->updateIcon();
        ++i;
    }
}

void
BE::SysTray::toggleItem( QTreeWidgetItem * item, int column )
{
    if (item && column)
        item->setCheckState(column, item->checkState(column) == Qt::Checked ? Qt::Unchecked :  Qt::Checked );
}

void
BE::SysTray::configureIcons()
{
    QDialog *d = new QDialog;
    new QVBoxLayout(d);
    QTreeWidget *tree = new QTreeWidget(d);
    QPalette pal = tree->palette();
    pal.setColor(QPalette::Base, QColor(140,140,140));
    pal.setColor(QPalette::Text, Qt::black);
    tree->setPalette(pal);
    tree->setColumnCount(3);
    tree->setRootIsDecorated(false);
    tree->header()->setStretchLastSection(false);
    tree->header()->setResizeMode(0, QHeaderView::Stretch);
    tree->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setResizeMode(2, QHeaderView::ResizeToContents);
    tree->setHeaderHidden(true);

    QMap<QString,int> icons;
    QList< QPointer<SysTrayIcon> >::iterator it = myIcons.begin();
    while (it != myIcons.end())
    {
        BE::SysTrayIcon *icon = *it;
        if (!icon)
            { it = myIcons.erase(it); continue; }
        icons[icon->name()] = 0;
        ++it;
    }

    foreach (QString s, fallbackOnes)
        icons[s] |= 1;

    foreach (QString s, nastyOnes)
        icons[s] |= 2;

    for (QMap<QString, int>::const_iterator it = icons.constBegin(), end = icons.constEnd(); it != end; ++it)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem( QStringList() << it.key() << "Hidden" << "Fallback");
        item->setCheckState(2, it.value() & 1 ? Qt::Checked : Qt::Unchecked );
        item->setCheckState(1, it.value() & 2 ? Qt::Checked : Qt::Unchecked );
        item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
        item->setIcon(0, BE::Plugged::themeIcon(it.key(), true));
        tree->addTopLevelItem(item);
    }

    tree->sortItems( 0, Qt::AscendingOrder );
    d->layout()->addWidget(tree);

    QDialogButtonBox *dbb = new QDialogButtonBox( QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, d );
    d->layout()->addWidget(dbb);
    connect (dbb, SIGNAL(accepted()), d, SLOT(accept()));
    connect (dbb, SIGNAL(rejected()), d, SLOT(reject()));
    connect (tree, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(toggleItem(QTreeWidgetItem*,int)));
    d->exec();
    d->hide();

    if (d->result() == QDialog::Accepted)
    {
        nastyOnes.clear();
        fallbackOnes.clear();
        const int n = tree->topLevelItemCount();
        for (int i = 0; i < n; ++i)
        {
            QTreeWidgetItem *item = tree->topLevelItem(i);
            if (item->checkState(1) == Qt::Checked)
                nastyOnes << item->text(0);
            if (item->checkState(2) == Qt::Checked)
                fallbackOnes << item->text(0);
        }
        toggleNastyOnes(nastyOnesAreVisible);
        updateFallbacks();
        Plugged::saveSettings();
    }
    delete d;
}

void
BE::SysTray::mousePressEvent(QMouseEvent *ev)
{
    if( ev->button() == Qt::RightButton )
    {
        myConfigMenu->exec(QCursor::pos());
        ev->accept();
    }
    else
        QFrame::mousePressEvent(ev);
}

void
BE::SysTray::saveSettings( KConfigGroup *grp )
{
    grp->writeEntry( "NastyIcons", nastyOnes);
    grp->writeEntry( "FallbackIcons", fallbackOnes);
}


void
BE::SysTray::toggleNastyOnes(bool on)
{
    nastyOnesAreVisible = on;
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon)
            { i = myIcons.erase(i); continue; }

        icon->setVisible(!(icon->nasty = nastyOnes.contains(icon->name())) || on);
        ++i;
    }
}

void
BE::SysTray::updateFallbacks()
{
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon) { i = myIcons.erase(i); continue; }
        
        icon->setFallBack(fallbackOnes.contains(icon->name()));
        ++i;
    }
}


// void BE::SysTray::resizeEvent( QResizeEvent * )
// {
//     QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
//     while (i != myIcons.end())
//     {
//         BE::SysTrayIcon *icon = *i;
//         if (!icon) { i = myIcons.erase(i); continue; }
//         
//         icon->setFixedSize( height(), height() );
//         ++i;
//     }
// }

bool BE::SysTray::x11Event(XEvent *event)
{
    if (event->type == ClientMessage)
    {
        if (event->xclient.message_type == net_opcode_atom && event->xclient.data.l[1] == SYSTEM_TRAY_REQUEST_DOCK)
        {
            WId winId = event->xclient.data.l[2];
            SysTrayIcon *icon = new SysTrayIcon( winId, this );
            myIcons.append( icon );
            layout()->addWidget( icon );
            icon->setVisible(!(icon->nasty = nastyOnes.contains(icon->name())) || nastyOnesAreVisible);
            icon->setFallBack(fallbackOnes.contains(icon->name()));
            QTimer::singleShot( 2000, this, SLOT(selfCheck()) );
        }
        else if(event->xclient.message_type == net_opcode_atom && event->xclient.data.l[1] == SYSTEM_TRAY_BEGIN_MESSAGE)
        {

        }
        else if(event->xclient.message_type == net_opcode_atom && event->xclient.data.l[1] == SYSTEM_TRAY_CANCEL_MESSAGE)
        {

        }
        else if(event->xclient.data.l[1] == net_message_data_atom)
        {

        }
        return true;
    }
    return false;
}

