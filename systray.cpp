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

#include <QAction>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
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
#include <KDE/KLocale>
#include <kwindowsystem.h>
#include <netwm.h>

#include <X11/Xlib.h>
// #include <X11/extensions/XTest.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

Atom net_opcode_atom;
Atom net_selection_atom;
Atom net_manager_atom;
Atom net_message_data_atom;
Atom net_tray_visual_atom;

namespace BE {

class X11EmbedContainer : public QX11EmbedContainer {
public:
    enum Mode { Plain, Backgrounded, Redirected };
    X11EmbedContainer(QWidget *parent) : QX11EmbedContainer(parent)
    , m_paintingBlocked(false)
    , m_picture(0)
    , m_bufferX(0)
    , m_mode(Plain)
    , m_damage(0) {

    }
    const QPixmap &buffer() { return m_buffer; }
    Mode mode() { return m_mode; }
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

        // attempt to redirect the window
        XRenderPictFormat *format = XRenderFindVisualFormat(display, attr.visual);
        if (format && format->type == PictTypeDirect && format->direct.alphaMask)
        {
            // Redirect ARGB windows to offscreen storage so we can composite them ourselves
            XRenderPictureAttributes attr;
            attr.subwindow_mode = IncludeInferiors;

            m_picture = XRenderCreatePicture(display, clientId, format, CPSubwindowMode, &attr);
            XCompositeRedirectSubwindows(display, winId, CompositeRedirectManual);
            m_damage = XDamageCreate(display, clientId, XDamageReportNonEmpty);
            m_mode = Redirected;
        }

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

        XFlush(display);

        embedClient(clientId);
    }

    ~X11EmbedContainer() {
        XDamageDestroy(QX11Info::display(), m_damage);
    }
    void syncToRedirected() {
        if (!m_picture)
            return;

        if (!m_bufferX) {
            m_bufferX = XCreatePixmap(QX11Info::display(), QX11Info::appRootWindow(), width(), height(), 32);
            m_buffer = QPixmap::fromX11Pixmap(m_bufferX, QPixmap::ExplicitlyShared);
        }
        m_buffer.fill(Qt::transparent);
        XRenderComposite(QX11Info::display(), PictOpSrc, m_picture, None, m_buffer.x11PictureHandle(),
                                                                    0, 0, 0, 0, 0, 0, width(), height());
    }
    void updateClientBackground() {
        if (m_picture || m_paintingBlocked || !isVisible() || size().isEmpty())
            return;

        m_paintingBlocked = true;

        QList<QWidget*> stack;
        QWidget *win = parentWidget()->parentWidget();
        while (win->parentWidget()) {
            win = win->parentWidget();
            stack << win;
        }

        XWindowAttributes attr;
        if (XGetWindowAttributes(QX11Info::display(), clientWinId(), &attr)) {
            m_mode = Backgrounded;
            Pixmap bufferX = XCreatePixmap(QX11Info::display(), QX11Info::appRootWindow(), width(), height(), attr.depth);
            QPixmap buffer = QPixmap::fromX11Pixmap(bufferX, QPixmap::ExplicitlyShared);
            for (int i = stack.count()-1; i > -1; --i)
                stack.at(i)->render(&m_buffer, QPoint(), rect().translated(mapTo(stack.at(i), QPoint(0,0))), i == stack.count()-1 ? DrawWindowBackground : RenderFlag(0));
            XSetWindowBackgroundPixmap(QX11Info::display(), clientWinId(), bufferX);
            XFreePixmap(QX11Info::display(), bufferX);
            XClearArea(QX11Info::display(), clientWinId(), 0, 0, 0, 0, True);
        }

        m_paintingBlocked = false;
    }
protected:
    void moveEvent(QMoveEvent *me) {
        QX11EmbedContainer::moveEvent(me);
        updateClientBackground();
    }
    void paintEvent(QPaintEvent *pe) {
        if (!m_paintingBlocked) {
            QWidget::paintEvent(pe);
//             if (!m_buffer.isNull()) {
//                 QPainter p(this);
//                 p.drawPixmap(0, 0, m_buffer);
//                 p.end();
//             }
        }
    }
    void resizeEvent(QResizeEvent *re) {
        QX11EmbedContainer::resizeEvent(re);
        if (m_bufferX)
            XFreePixmap(QX11Info::display(), m_bufferX);
        m_bufferX = 0;
        m_buffer = QPixmap();
        updateClientBackground();
    }
    void showEvent(QShowEvent *se) {
        QX11EmbedContainer::showEvent(se);
        updateClientBackground();
    }
private:
    bool m_paintingBlocked;
    Picture m_picture;
    QPixmap m_buffer;
    Pixmap m_bufferX;
    Mode m_mode;
    Damage m_damage;
};

class SysTrayIcon : public QWidget
{
public:
    enum Feature { Custom = 1<<0, InputShape = 1<<1, Curtain = 1<<2 };
    SysTrayIcon(WId id, SysTray *parent) : QWidget(parent), nasty(false), fallback(false)
    {
        setContentsMargins(0, 0, 0, 0);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMinimumSize(16,16);
        setFocusPolicy(Qt::ClickFocus);

        iName = KWindowInfo(id, NET::WMName).name();

        curtain = new QLabel( this );
        curtain->setScaledContents(true);
        curtain->setAttribute(Qt::WA_TranslucentBackground, false);
        curtain->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
        curtain->setAttribute(Qt::WA_NativeWindow, true);
        curtain->setAttribute(Qt::WA_NoSystemBackground, true);
        curtain->setAttribute(Qt::WA_TransparentForMouseEvents);
        XShapeCombineRectangles(QX11Info::display(), curtain->winId(), ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
        XReparentWindow(QX11Info::display(), curtain->winId(), winId(), 0, 0);
        curtain->installEventFilter(this);

        bed = new X11EmbedContainer( this );
        bed->setAttribute(Qt::WA_TranslucentBackground, false);
        bed->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
        bed->setAttribute(Qt::WA_NativeWindow, true);
        bed->setAttribute(Qt::WA_NoSystemBackground, true);

        hide();
        connect(bed, SIGNAL(clientIsEmbedded()), parent, SLOT(requestShowIcon()));
        connect(bed, SIGNAL(clientClosed()), this, SLOT(deleteLater())); // this? leeds to problems with e.g. KMail
        bed->embed(id);
        bed->setFixedSize(size());
        curtain->setFixedSize(size());
        curtain->setVisible(fallback || bed->mode() == X11EmbedContainer::Redirected);
        XRaiseWindow(QX11Info::display(), curtain->winId());

        setFocusProxy(bed);

        wmPix = KWindowSystem::icon(id, 128, 128, false );
        updateIcon();
    }

    void fixStack() {
        XRaiseWindow(QX11Info::display(), curtain->winId());
    }
    inline WId cId() { return bed->clientWinId(); }
    inline bool isOutdated() { return !bed->clientWinId(); }
    const QString &name() { return iName; }
    void release() {
        XReparentWindow(QX11Info::display(), cId(), QX11Info::appRootWindow(), 0, 0);
    }
    void setFallBack( bool on )
    {
        if (fallback == on)
            return;
        fallback = on;
        curtain->setVisible(!fallback || bed->mode() == X11EmbedContainer::Redirected);
        fixStack();
        curtain->update();  // switch between icon and redirection buffer
    }
    inline static void setSize(int s) { ourSize = s; };
    void syncToRedirected() { if (!bed) return; bed->syncToRedirected(); curtain->repaint(); }
    void updateIcon()
    {
        QIcon icn = BE::Plugged::themeIcon(iName, parentWidget(), false);
        curtain->setPixmap(icn.isNull() ? wmPix : icn.pixmap(128));
    }
    bool nasty, fallback;
protected:

    bool eventFilter(QObject *o, QEvent *e)
    {
        if (e->type() == QEvent::Paint && o == curtain && fallback && !bed->buffer().isNull()) {
            QPainter p(curtain);
            p.drawPixmap(0, 0, bed->buffer());
            p.end();
            return true;
        }
        return false;
    }

    void resizeEvent( QResizeEvent *re )
    {
        if ( width() == height() )
        {
            QWidget::resizeEvent(re);
//             XResizeWindow(QX11Info::display(), cId(), width(), height() );
//             XMapRaised(QX11Info::display(), cId());
            bed->setFixedSize(size());
            curtain->setFixedSize(size());
        }
        else if (width() > height())
            setMaximumWidth(height());
        else
            setMaximumHeight(width());
    }

protected:
    static int ourSize;
    QString iName;
    X11EmbedContainer *bed;
    QPixmap wmPix;
    QLabel *curtain;
};

}

static int damageEventBase = 0;
static QCoreApplication::EventFilter formerX11EventFilter = 0;
static BE::SysTray *s_instance = 0;

BE::SysTray::SysTray(QWidget *parent) : QFrame(parent), BE::Plugged(parent), nastyOnesAreVisible(false)
{
    if (s_instance) {
        deleteLater();
        return;
    }
    s_instance = this;
    setObjectName("SystemTray");
    myConfigMenu = configMenu()->addMenu(i18n("SystemTray"));
    QAction *act = myConfigMenu->addAction(i18n("Show nasty ones"));
    act->setCheckable(true);
    connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleNastyOnes(bool)));
    myConfigMenu->addAction(i18n("Configure..."), this, SLOT(configureIcons()) );
    healthTimer = new QTimer(this);
    healthTimer->setSingleShot(true);
    connect (healthTimer, SIGNAL(timeout()), this, SLOT(selfCheck()));
    connect (qApp, SIGNAL(aboutToQuit()), SLOT(releaseIcons()));

    FlowLayout *l = new FlowLayout(this);
    l->setContentsMargins(3, 1, 3, 1);
    l->setSpacing(2);
    QTimer::singleShot(0, this, SLOT(init()));
    int errorBase;
    XDamageQueryExtension(QX11Info::display(), &damageEventBase, &errorBase);
}

BE::SysTray::~SysTray() {
    if (s_instance == this) {
        s_instance = 0;
    }
    delete myConfigMenu;
    if (formerX11EventFilter)
        QCoreApplication::instance()->setEventFilter(formerX11EventFilter);
}

namespace BE {

bool x11EventFilter(void *message, long int *result) {
    Q_ASSERT(s_instance);
    XEvent *event = reinterpret_cast<XEvent*>(message);
    if (event->xany.type == damageEventBase + XDamageNotify) {
        s_instance->damageEvent(message);
    }
    return formerX11EventFilter ? formerX11EventFilter(message, result) : false;
}

}

void
BE::SysTray::init()
{
    Display *dpy = QX11Info::display();
    const WId desktop = QApplication::desktop()->winId();
     // Freedesktop.org system tray support
    char name[20] = {0};
    qsnprintf(name, 20, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(dpy));
    net_selection_atom = XInternAtom(dpy, name, False);
    net_opcode_atom = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    net_manager_atom = XInternAtom(dpy, "MANAGER", False);
    net_message_data_atom = XInternAtom(dpy, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
    net_tray_visual_atom = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);

//     formerX11EventFilter = QCoreApplication::instance()->setEventFilter(((QCoreApplication::EventFilter)&BE::SysTray::x11EventFilter));
    formerX11EventFilter = QCoreApplication::instance()->setEventFilter(x11EventFilter);

    XSetSelectionOwner(dpy, net_selection_atom, winId(), CurrentTime);

    if (XGetSelectionOwner(dpy, net_selection_atom) != winId()) {
        qWarning("There's already some systray - bye'");
        hide();
        return;
    }

    // Prefer the ARGB32 visual if available
    int nVi;
    VisualID visual = XVisualIDFromVisual((Visual*)QX11Info::appVisual());
    XVisualInfo templ;
    templ.visualid = visual;
    XVisualInfo *xvi = XGetVisualInfo(dpy, VisualIDMask, &templ, &nVi);
    if (xvi && xvi[0].depth > 16) {
        templ.screen  = xvi[0].screen;
        templ.depth   = 32;
        templ.c_class = TrueColor;
        XFree(xvi);
        xvi = XGetVisualInfo(dpy, VisualScreenMask|VisualDepthMask|VisualClassMask, &templ, &nVi);
        for (int i = 0; i < nVi; ++i) {
            XRenderPictFormat *format = XRenderFindVisualFormat(dpy, xvi[i].visual);
            if (format && format->type == PictTypeDirect && format->direct.alphaMask) {
                visual = xvi[i].visualid;
                break;
            }
        }
        XFree(xvi);
    }
    XChangeProperty(dpy, winId(), net_tray_visual_atom, XA_VISUALID, 32, PropModeReplace, (const uchar*)&visual, 1);
    
    XClientMessageEvent xev;

    xev.type = ClientMessage;
    xev.window = desktop;
    xev.message_type = net_manager_atom;
    xev.format = 32;
    xev.data.l[0] = CurrentTime;
    xev.data.l[1] = net_selection_atom;
    xev.data.l[2] = winId();
    xev.data.l[3] = 0;        /* manager specific data */
    xev.data.l[4] = 0;        /* manager specific data */

    XSendEvent(dpy, desktop, False, StructureNotifyMask, (XEvent *)&xev);
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
    oldOnes = unthemedOnes;
    unthemedOnes = grp->readEntry("FallbackIcons", QStringList());
    if (unthemedOnes != oldOnes)
        updateUnthemed();
}

void
BE::SysTray::requestShowIcon()
{
    if (!sender())
        return;

    BE::SysTrayIcon *icon = dynamic_cast<BE::SysTrayIcon*>(sender()->parent());
    if (!icon)
        return;

    if (icon->isOutdated())
    {
        icon->deleteLater();
        return;
    }

    icon->fixStack();
    icon->nasty = nastyOnes.contains(icon->name());
    if (nastyOnesAreVisible || !icon->nasty) {
        icon->show();
    }
    icon->setFallBack(unthemedOnes.contains(icon->name()));
}

void
BE::SysTray::releaseIcons() {
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end()) {
        BE::SysTrayIcon *icon = *i;
        if (icon)
            icon->release();
        ++i;
    }
    XSync(QX11Info::display(), false);
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

    foreach (QString s, unthemedOnes)
        icons[s] |= 1;

    foreach (QString s, nastyOnes)
        icons[s] |= 2;

    for (QMap<QString, int>::const_iterator it = icons.constBegin(), end = icons.constEnd(); it != end; ++it)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem( QStringList() << it.key() << i18n("Nasty") << i18n("Unthemed"));
        item->setCheckState(2, it.value() & 1 ? Qt::Checked : Qt::Unchecked );
        item->setCheckState(1, it.value() & 2 ? Qt::Checked : Qt::Unchecked );
        item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
        item->setIcon(0, themeIcon(it.key(), true));
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
        unthemedOnes.clear();
        const int n = tree->topLevelItemCount();
        for (int i = 0; i < n; ++i)
        {
            QTreeWidgetItem *item = tree->topLevelItem(i);
            if (item->checkState(1) == Qt::Checked)
                nastyOnes << item->text(0);
            if (item->checkState(2) == Qt::Checked)
                unthemedOnes << item->text(0);
        }
        toggleNastyOnes(nastyOnesAreVisible);
        updateUnthemed();
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
    grp->writeEntry( "FallbackIcons", unthemedOnes);
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
BE::SysTray::updateUnthemed()
{
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon) { i = myIcons.erase(i); continue; }
        
        icon->setFallBack(unthemedOnes.contains(icon->name()));
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
            icon->hide();
            healthTimer->start(2000);
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

void BE::SysTray::damageEvent(void *event)
{
    XDamageNotifyEvent *de = static_cast<XDamageNotifyEvent*>(event);
    QList< QPointer<SysTrayIcon> >::iterator i = myIcons.begin();
    while (i != myIcons.end())
    {
        BE::SysTrayIcon *icon = *i;
        if (!icon)
        {
            i = myIcons.erase(i);
            continue;
        }
        else if (de->drawable == icon->cId()) {
            XserverRegion region = XFixesCreateRegion(de->display, 0, 0);
            XDamageSubtract(de->display, de->damage, None, region);
            XFixesDestroyRegion(de->display, region);
            icon->syncToRedirected();
        }
        ++i;
    }
}