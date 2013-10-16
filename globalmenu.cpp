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

#include "globalmenu.h"
#include "dbus_gmenu.h"

#include "hmenubar.h"
#include "vmenubar.h"

#include "be.shell.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QBoxLayout>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDesktopWidget>
#include <QDomElement>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QtConcurrentRun>
#include <QTimer>
#include <QX11Info>

#include <KDE/KConfigGroup>
#include <KDE/KIcon>
#include <KDE/KStandardDirs>
#include <kwindowsystem.h>
#include <kwindowinfo.h>

#include <QtDebug>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>



static BE::GMenu *instance = NULL;

QTimer BE::GMenu::ourBodyCleaner;

static Atom ggmContext;
static Atom ggmEvent;
static QAbstractEventDispatcher::EventFilter formerX11EventFilter = 0;

static void setBold(QAction *act, bool bold)
{
    if (!act)
        return;
    QFont fnt = act->font(); fnt.setBold(bold); act->setFont(fnt);
}

static void ggmSetLocalMenus(bool on)
{
    return;
    // there's somehoe a stray character or the gconf thing is broken - I get random
    // "Key file contains line '<...>' which is not a key-value pair, group, or comment"
    // So right now this only breaks the value as eg. set by a startup script :-(
    Atom ggmSettings = XInternAtom( QX11Info::display(), "_NET_GLOBALMENU_SETTINGS", false );
    XTextProperty text;
    QString string = QString("\n[GlobalMenu:Client]\nshow-local-menu=%1\nshow-menu-icons=true\nchanged-notify-timeout=500\n").arg(on?"true":"false");
    QByteArray ba = string.toLatin1();
    ba.append("\0");
    char *data = ba.data();
    XStringListToTextProperty( &data, 1, &text );
    XSetTextProperty( QX11Info::display(), QX11Info::appRootWindow(), &text, ggmSettings );
}

BE::GMenu::GMenu(QWidget *parent) : QWidget(parent), BE::Plugged(parent)
{
    if (instance)
    {
        QMessageBox::warning ( 0, "Multiple XBar requests", "XBar shall be unique dummy text");
        qWarning("XBar, Do NOT load XBar more than once!");
        deleteLater();
        return;
    }

    instance = this;

    myMainMenu = 0;
    myMainMenuDefWatcher = 0;
    myCurrentBar = 0; // important!
    iAmGnome2 = true; // "fixed" with ::configure
    ggmLastId = 0;
    ggmContext = XInternAtom( QX11Info::display(), "_NET_GLOBALMENU_MENU_CONTEXT", false );
    ggmEvent = XInternAtom( QX11Info::display(), "_NET_GLOBALMENU_MENU_EVENT", false );

    if (!formerX11EventFilter)
        formerX11EventFilter = QAbstractEventDispatcher::instance()->setEventFilter(globalX11EventFilter);

    setLayout(new QBoxLayout(QBoxLayout::LeftToRight, this));
    layout()->setAlignment(Qt::AlignCenter);
    layout()->setContentsMargins(0, 0, 0, 0);

    setFocusPolicy( Qt::ClickFocus );

    myOrientation = Plugged::orientation();

    repopulateMainMenu();

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    new GMenuAdaptor(this);

    connect (parent, SIGNAL(orientationChanged( Qt::Orientation )),
             this, SLOT(orientationChanged(Qt::Orientation)));
}

BE::GMenu::~GMenu()
{
    if (instance == this) {
        byeMenus();
        instance = NULL;
    }
    QDBusConnection::sessionBus().unregisterService("org.kde.XBar");
}

void BE::GMenu::configure( KConfigGroup *grp )
{
    bool wasGnome2 = iAmGnome2;
    iAmGnome2 = !grp->readEntry("WindowMenus", true);
    if (wasGnome2 == iAmGnome2)
        return;
    if (iAmGnome2) {
        disconnect (this, SIGNAL(destroyed()), this, SLOT(byeMenus()));
        disconnect (qApp, SIGNAL(aboutToQuit()), this, SLOT(byeMenus()));
        disconnect( KWindowSystem::self(), SIGNAL( activeWindowChanged(WId) ), this, SLOT( ggmWindowActivated(WId) ) );
        disconnect( KWindowSystem::self(), SIGNAL( windowAdded(WId) ), this, SLOT( ggmWindowAdded(WId) ) );
        disconnect( KWindowSystem::self(), SIGNAL( windowRemoved(WId) ), this, SLOT( ggmWindowRemoved(WId) ) );
        disconnect (&ourBodyCleaner, SIGNAL(timeout()), this, SLOT(cleanBodies()));
        ggmSetLocalMenus(true);
        show(myMainMenu);
        foreach ( WId id, ggmMenus )
            ggmWindowRemoved( id );
        byeMenus();
        for (MenuMap::iterator i = myMenus.begin(), end = myMenus.end(); i != end; ++i)
            delete i.value();
        myMenus.clear();
        QDBusConnection::sessionBus().unregisterService("org.kde.XBar");
    } else {
        QDBusConnection::sessionBus().registerService("org.kde.XBar");
        QDBusConnection::sessionBus().registerObject("/XBar", this);
        connect (this, SIGNAL(destroyed()), SLOT(byeMenus()));
        connect (qApp, SIGNAL(aboutToQuit()), SLOT(byeMenus()));
        connect( KWindowSystem::self(), SIGNAL( activeWindowChanged(WId) ), SLOT( ggmWindowActivated(WId) ) );
        connect( KWindowSystem::self(), SIGNAL( windowAdded(WId) ), SLOT( ggmWindowAdded(WId) ) );
        connect( KWindowSystem::self(), SIGNAL( windowRemoved(WId) ), SLOT( ggmWindowRemoved(WId) ) );
        connect (&ourBodyCleaner, SIGNAL(timeout()), SLOT(cleanBodies()));
        ourBodyCleaner.start(30000); // every 5 minutes - it's just to clean menus from crashed windows, so users won't constantly scroll them
        QMetaObject::invokeMethod(this, "callMenus", Qt::QueuedConnection);
    }
}

void
BE::GMenu::byeMenus()
{
    QDBusConnectionInterface *session = QDBusConnection::sessionBus().interface();
    QStringList services = session->registeredServiceNames();
    foreach ( QString service, services ) 
    {
        if ( service.startsWith("org.kde.XBar-") )
        {
            QDBusInterface interface( service, "/XBarClient", "org.kde.XBarClient" );
            if ( interface.isValid() )
                interface.call( "deactivate" );
        }
    }
    ggmSetLocalMenus(true);
}

void
BE::GMenu::unlockUpdates()
{
    blockUpdates(false);
}

void
BE::GMenu::blockUpdates(bool on)
{
    on = !on;
    parentWidget() ? parentWidget()->setUpdatesEnabled(on) : setUpdatesEnabled(on);
}

static void callDBusMenus()
{
    QDBusConnectionInterface *session = QDBusConnection::sessionBus().interface();
    QStringList services = session->registeredServiceNames();

    foreach (QString service, services)
    {
        if (service.startsWith("org.kde.XBar-"))
        {
            QDBusInterface interface( service, "/XBarClient", "org.kde.XBarClient" );
            if (interface.isValid())
                interface.call("activate");
        }
    }
}

void
BE::GMenu::callMenus()
{
    QtConcurrent::run(&callDBusMenus);
    ggmSetLocalMenus(false);
    // and read them
    foreach (WId id, KWindowSystem::windows())
        ggmWindowAdded( id );
}

void
BE::GMenu::changeEntry(qlonglong key, int idx, const QString &entry, bool add)
{
    MenuMap::iterator i = myMenus.find( key );
    if (i == myMenus.end())
        return;

    QWidget *bar = i.value();

    if (entry.isNull())
    {   // delete

        if (idx < 0 || idx >= bar->actions().count())
            return;

        delete bar->actions().takeAt(idx);

        if (bar->actions().isEmpty())
            return;

        if (!idx)
        if (QAction *act = bar->actions().at(0))
        {
            setBold(act, true);
            act->setText(title(bar));
        }
    }
    else if (add)
    {
        bool titleItem = !idx;
        QAction *before = 0;
        if (bar->actions().isEmpty())
            titleItem = titleItem || idx < 0;
        else if (idx > -1 && idx < bar->actions().count())
            before = bar->actions().at(idx);

        QAction *act = new QAction(entry, bar);
        if (entry == "<XBAR_SEPARATOR/>")
            act->setSeparator(true);
        else
            act->setData(entry);
        if (titleItem)
        {
            setBold(act, true);
            act->setText(title(bar));
            if (before)
            {
                before->setText(before->data().toString());
                setBold(before, false);
            }
        }
        act->setVisible(!act->text().isEmpty());
        bar->insertAction(before, act);
    }
    else
    {
        if (idx < 0 || idx >= bar->actions().count())
            return;
        if (QAction *act = bar->actions().at(idx))
        {
            if (entry == "<XBAR_SEPARATOR/>")
                act->setSeparator(true);
            else
                act->setData(entry);
            if (idx && !act->isSeparator())
                act->setText(entry);
            act->setVisible(!act->text().isEmpty());
        }
    }
}

void
BE::GMenu::cleanBodies()
{
    QDBusConnectionInterface *session = QDBusConnection::sessionBus().interface();
    QStringList services = session->registeredServiceNames();
    services = services.filter(QRegExp("^org\\.kde\\.XBar-"));
    MenuMap::iterator i = myMenus.begin();
    QWidget *bar = 0;
    while (i != myMenus.end())
    {
        if (ggmMenus.contains(i.key()) || services.contains(service(i.value())))
            ++i;
        else
        {
            bar = i.value();
            if ( bar == myCurrentBar )
            {
                hide(bar);
                show(myMainMenu);
            }
            i = myMenus.erase(i);
            delete bar;
        }
    }
}

BE::HMenuBar *BE::GMenu::currentHBar() const
{
    return qobject_cast<BE::HMenuBar*>(myCurrentBar);
}

BE::VMenuBar *BE::GMenu::currentVBar() const
{
    return qobject_cast<BE::VMenuBar*>(myCurrentBar);
}

bool
BE::GMenu::dbusAction(const QObject *o, int idx, const QString &cmd)
{
    QAction *act = 0; qlonglong key = 0; QString service; QPoint pt;
    if (const BE::HMenuBar *bar = qobject_cast<const BE::HMenuBar*>(o))
    {
        act = bar->action(idx); service = bar->service(); key = bar->key();
        pt = bar->actionGeometry(act).bottomLeft(); // needs to be smarter..., i.e. panel dependend
    }
    else if (const BE::VMenuBar *bar = qobject_cast<const BE::VMenuBar*>(o))
    {
        act = bar->action(idx); service = bar->service(); key = bar->key();
        pt = bar->actionGeometry(act).topRight();
    }
    else
        return false; // that's not our business!
    const QWidget *w = static_cast<const QWidget*>(o);
    if (!act || act->menu())
        return false; // that's not our business!
    
    pt = w->mapToGlobal(pt);

    QDBusInterface interface( service, "/XBarClient", "org.kde.XBarClient" );
    if (interface.isValid())
    {
        if (idx < 0)
            interface.call(cmd, key);
        else
            interface.call(cmd, key, idx, pt.x(), pt.y());
    }
    return true;
}

void
BE::GMenu::focusOutEvent(QFocusEvent *event)
{
    if (BE::HMenuBar *bar = currentHBar())
        bar->popDown();
    else if (BE::VMenuBar *bar = currentVBar())
        bar->popDown();

    QWidget::focusOutEvent(event);
}

void
BE::GMenu::hide(QWidget *item)
{
    item->hide();
}

void
BE::GMenu::hover(int idx)
{
    dbusAction(sender(), idx, "hover");
}

void
BE::GMenu::raiseCurrentWindow()
{
    QWidget *w = currentHBar();
    if (!w) w = currentVBar();
    if (!w || w == myMainMenu)
        return; // nothing to raise...
    dbusAction(w, -1, "raise");
}

void
BE::GMenu::registerMenu(const QString &service, qlonglong key, const QString &title, const QStringList &entries)
{
    QWidget *newBar;
    if (myOrientation == Qt::Horizontal)
        newBar = new HMenuBar(service, key, this);
    else
        newBar = new VMenuBar(service, key, this);
    BE::GMenu::title(newBar) = title;
    newBar->setPalette(palette());
    connect (newBar, SIGNAL(hovered(int)), this, SLOT(hover(int)));
    connect (newBar, SIGNAL(triggered(int)), this, SLOT(trigger(int)));

    // the demanded menu entries
    QAction *act;
    foreach (QString entry, entries)
    {
        act = new QAction(entry, newBar);
        if (entry == "<XBAR_SEPARATOR/>")
            act->setSeparator(true);
        else
            act->setData(entry);
        newBar->addAction(act);
    }
    if (newBar->actions().count())
    if ((act = newBar->actions().at(0)))
    {
        setBold(act, true);
        act->setText(title);
    }

    // replace older versions - in case
    QWidget *oldBar = myMenus.take( key );
    myMenus.insert( key, newBar );
    layout()->addWidget(newBar);
    if (oldBar == myCurrentBar) {
        myCurrentBar = newBar;
        newBar->show();
    } else {
        // add hidden
        newBar->hide();
    }
    delete oldBar;
}

void
BE::GMenu::releaseFocus(qlonglong key)
{
    blockUpdates(true);
    int n = 0;
    for (MenuMap::iterator i = myMenus.begin(); i != myMenus.end(); ++i)
    {
        if (i.key() == key)
            hide(i.value());
        else
            n += i.value()->isVisible();
    }
    if (!n)
        show(myMainMenu);
    // there might come an immediate request afterwards
//     blockUpdates(false);
    QTimer::singleShot(100, this, SLOT(unlockUpdates()));
}

void
BE::GMenu::reparent(qlonglong oldKey, qlonglong newKey)
{
    MenuMap::iterator i = myMenus.find( oldKey );
    if (i == myMenus.end())
        return;
    QWidget *bar = i.value();
    myMenus.erase(i);
    myMenus.insert(newKey, bar);
}

void
BE::GMenu::repopulateMainMenu()
{
    if (myCurrentBar == myMainMenu)
        myCurrentBar = 0;
    delete myMainMenu;
    if (myOrientation == Qt::Horizontal)
        myMainMenu = new QMenuBar(this);
    else
        myMainMenu = new QMenu(this);
    
    delete myMainMenuDefWatcher;

    BE::Shell::buildMenu("MainMenu", myMainMenu, "menubar");
    
    myMainMenuDefWatcher = new QFileSystemWatcher(this);
    myMainMenuDefWatcher->addPath(KGlobal::dirs()->locate("data", "be.shell/MainMenu.xml"));
    connect( myMainMenuDefWatcher, SIGNAL(fileChanged(const QString &)), this, SLOT(repopulateMainMenu()) );

    if (myCurrentBar)
        myMainMenu->hide();
    else
        myCurrentBar = myMainMenu;
    layout()->addWidget(myMainMenu);
}

void
BE::GMenu::requestFocus(qlonglong key)
{
    blockUpdates(true);
    for (MenuMap::iterator i = myMenus.begin(); i != myMenus.end(); ++i)
    {
        if (i.key() == key)
        {
            hide(myMainMenu);
            if ( !i.value()->isEnabled() && ggmMenus.contains( key ) )
            {   // invalidated
                delete i.value();
                i.value() = ggmCreate( key );
                if (!i.value())
                {
                    myMenus.erase( i );
                    ggmMenus.removeAll( key );
                    show(myMainMenu); // invalid attempt
                    return;
                }
            }
            show(i.value());
        }
        else
            hide(i.value());
    }
    blockUpdates(false);
}

void
BE::GMenu::setOpenPopup(int idx)
{
    if (BE::HMenuBar *bar = currentHBar())
        bar->setOpenPopup(idx);
    else if (BE::VMenuBar *bar = currentVBar())
        bar->setOpenPopup(idx);
    if (myCurrentBar)
        myCurrentBar->update();
}

void
BE::GMenu::show(QWidget *item)
{
    myCurrentBar = item;
    item->show();
}

void 
BE::GMenu::showMainMenu()
{
    if ( instance && instance->myCurrentBar != instance->myMainMenu )
    {
        instance->hide(instance->myCurrentBar);
        instance->show(instance->myMainMenu);
    }
}

void
BE::GMenu::trigger(int idx)
{
    dbusAction(sender(), idx, "popup");
}

void
BE::GMenu::unregisterMenu(qlonglong key)
{
    releaseFocus(key);
    delete myMenus.take( key );
}

void
BE::GMenu::unregisterCurrentMenu()
{
    if (myCurrentBar && myCurrentBar != myMainMenu)
    {
        qlonglong key = myMenus.key(myCurrentBar, 0);
        if (key)
        {
            QDBusInterface interface( service(myCurrentBar), "/XBarClient", "org.kde.XBarClient" );
            if (interface.isValid())
                interface.call("deactivate");
            unregisterMenu(key);
        }
    }
}

void
BE::GMenu::mousePressEvent( QMouseEvent *ev )
{
    ev->accept();
}

void
BE::GMenu::orientationChanged(Qt::Orientation o)
{
    myOrientation = o;
    repopulateMainMenu();
    static_cast<QBoxLayout*>(layout())->setDirection(o == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom );
    MenuMap oldMenus = myMenus;
    MenuMap::const_iterator i = oldMenus.constBegin();
    QWidget *w; QStringList entries;
    while (i != oldMenus.constEnd())
    {
        entries.clear();
        w = i.value();
        foreach (QAction *act, w->actions())
            entries << act->data().toString();
        registerMenu(service(w), i.key(), title(w), entries);
        ++i;
    }
}

static QString nullString;

const QString &
BE::GMenu::service(QWidget *w)
{
    if (HMenuBar *bar = qobject_cast<HMenuBar*>(w))
        return bar->service();
    if (VMenuBar *bar = qobject_cast<VMenuBar*>(w))
        return bar->service();
    return nullString;
}

QString &
BE::GMenu::title(QWidget *w)
{
    if (HMenuBar *bar = qobject_cast<HMenuBar*>(w))
        return bar->title;
    if (VMenuBar *bar = qobject_cast<VMenuBar*>(w))
        return bar->title;
    return nullString;
}

void
BE::GMenu::wheelEvent(QWheelEvent *ev)
{
    if (myMenus.isEmpty())
        return;
    
    MenuMap::iterator n;
    
    if (myCurrentBar == myMainMenu)
    {
        hide(myMainMenu);
        if (ev->delta() < 0)
            n = myMenus.begin();
        else
            { n = myMenus.end(); --n; }
    }
    else {
        n = myMenus.end();
        MenuMap::iterator i = myMenus.end();
        for (i = myMenus.begin(); i != myMenus.end(); ++i)
        {
            hide(i.value());
            if (i.value() == myCurrentBar)
            {
                if (ev->delta() < 0)
                    n = i+1;
                else if (i == myMenus.begin())
                    n = myMenus.end();
                else
                    n = i-1;
            }
        }
    }
    
    while ( n != myMenus.end() && !n.value()->isEnabled() && ggmMenus.contains( n.key() ) )
    {   // update invalidated menus - we might have lost them as well...
        delete n.value();
        n.value() = ggmCreate( n.key() );
        if (n.value())
            break;
        else
        {
            ggmMenus.removeAll( n.key() );
            n = myMenus.erase( n );
        }
    }

    if (n == myMenus.end())
        show(myMainMenu);
    else
        show( n.value() );
}

// ================= GGM support implementation ===============


static QHash<QString,QString> ggmMapIcons()
{
    QHash<QString,QString> hash;
    hash.insert( "gtk-about", "start-here-kde" );
    hash.insert( "gtk-add", "list-add.png" );
    hash.insert( "gtk-apply", "dialog-ok-apply.png" );
    hash.insert( "gtk-bold", "format-text-bold.png" );
    hash.insert( "gtk-cancel", "dialog-cancel.png" );
    hash.insert( "gtk-cdrom", "media-optical.png" );
    hash.insert( "gtk-clear", "edit-clear.png" );
    hash.insert( "gtk-close", "window-close.png" );
    hash.insert( "gtk-color-picker", "color-picker.png" );
    hash.insert( "gtk-connect", "network-connect.png" );
    hash.insert( "gtk-convert", "document-export.png" );
    hash.insert( "gtk-copy", "edit-copy.png" );
    hash.insert( "gtk-cut", "edit-cut.png" );
    hash.insert( "gtk-delete", "edit-delete.png" );
    hash.insert( "gtk-dialog-authentication", "status/dialog-password.png" );
    hash.insert( "gtk-dialog-error", "status/dialog-error.png" );
    hash.insert( "gtk-dialog-info", "status/dialog-information.png" );
    hash.insert( "gtk-dialog-question", "status/dialog-information.png" );
    hash.insert( "gtk-dialog-warning", "status/dialog-warning.png" );
    hash.insert( "gtk-directory", "folder.png" );
    hash.insert( "gtk-disconnect", "network-disconnect.png" );
    hash.insert( "gtk-dnd", "application-x-zerosize.png" );
    hash.insert( "gtk-dnd-multiple", "document-multiple.png" );
    hash.insert( "gtk-edit", "document-properties.png" );
    hash.insert( "gtk-execute", "fork.png" );
    hash.insert( "gtk-file", "application-x-zerosize.png" );
    hash.insert( "gtk-find", "edit-find.png" );
    hash.insert( "gtk-find-and-replace", "edit-find-replace.png" );
    hash.insert( "gtk-floppy", "media-floppy.png" );
    hash.insert( "gtk-fullscreen", "view-fullscreen.png" );
    hash.insert( "gtk-goto-bottom", "go-bottom.png" );
    hash.insert( "gtk-goto-first", "go-first.png" );
    hash.insert( "gtk-goto-last", "go-last.png" );
    hash.insert( "gtk-goto-top", "go-top.png" );
    hash.insert( "gtk-go-back", "go-previous.png" );
    hash.insert( "gtk-go-back-ltr", "go-previous.png" );
    hash.insert( "gtk-go-back-rtl", "go-next.png" );
    hash.insert( "gtk-go-down", "go-down.png" );
    hash.insert( "gtk-go-forward", "go-next.png" );
    hash.insert( "gtk-go-forward-ltr", "go-next.png" );
    hash.insert( "gtk-go-forward-rtl", "go-previous.png" );
    hash.insert( "gtk-go-up", "go-up.png" );
    hash.insert( "gtk-harddisk", "drive-harddisk.png" );
    hash.insert( "gtk-help", "help-contents.png" );
    hash.insert( "gtk-home", "go-home.png" );
    hash.insert( "gtk-indent", "format-indent-more.png" );
    hash.insert( "gtk-index", "help-contents.png" );
    hash.insert( "gtk-info", "help-about.png" );
    hash.insert( "gtk-italic", "format-text-italic.png" );
    hash.insert( "gtk-jump-to", "go-jump.png" );
    hash.insert( "gtk-justify-center", "format-justify-center.png" );
    hash.insert( "gtk-justify-fill", "format-justify-fill.png" );
    hash.insert( "gtk-justify-left", "format-justify-left.png" );
    hash.insert( "gtk-justify-right", "format-justify-right.png" );
    hash.insert( "gtk-leave-fullscreen", "view-restore.png" );
    hash.insert( "gtk-media-forward", "media-seek-forward.png" );
    hash.insert( "gtk-media-next", "media-skip-forward.png" );
    hash.insert( "gtk-media-pause", "media-playback-pause.png" );
    hash.insert( "gtk-media-play", "media-playback-start.png" );
    hash.insert( "gtk-media-previous", "media-skip-backward.png" );
    hash.insert( "gtk-media-record", "media-record.png" );
    hash.insert( "gtk-media-rewind", "media-seek-backward.png" );
    hash.insert( "gtk-media-stop", "media-playback-stop.png" );
    hash.insert( "gtk-missing-image", "unknown.png" );
    hash.insert( "gtk-network", "network-server.png" );
    hash.insert( "gtk-new", "document-new.png" );
    hash.insert( "gtk-no", "edit-delete.png" );
    hash.insert( "gtk-ok", "dialog-ok.png" );
    hash.insert( "gtk-open", "document-open.png" );
    hash.insert( "gtk-paste", "edit-paste.png" );
    hash.insert( "gtk-preferences", "configure.png" );
    hash.insert( "gtk-print", "document-print.png" );
    hash.insert( "gtk-print-preview", "document-print-preview.png" );
    hash.insert( "gtk-properties", "document-properties.png" );
    hash.insert( "gtk-quit", "application-exit.png" );
    hash.insert( "gtk-redo", "edit-redo.png" );
    hash.insert( "gtk-refresh", "view-refresh.png" );
    hash.insert( "gtk-remove", "edit-delete.png" );
    hash.insert( "gtk-revert-to-saved", "document-revert.png" );
    hash.insert( "gtk-save", "document-save.png" );
    hash.insert( "gtk-save-as", "document-save-as.png" );
    hash.insert( "gtk-select-all", "edit-select-all.png" );
    hash.insert( "gtk-select-color", "color-picker.png" );
    hash.insert( "gtk-select-font", "preferences-desktop-font.png" );
    hash.insert( "gtk-sort-ascending", "view-sort-ascending.png" );
    hash.insert( "gtk-sort-descending", "view-sort-descending.png" );
    hash.insert( "gtk-spell-check", "tools-check-spelling.png" );
    hash.insert( "gtk-stop", "process-stop.png" );
    hash.insert( "gtk-strikethrough", "format-text-strikethrough.png" );
    hash.insert( "gtk-undelete", "edit-undo.png" );
    hash.insert( "gtk-underline", "format-text-underline.png" );
    hash.insert( "gtk-undo", "edit-undo.png" );
    hash.insert( "gtk-unindent", "format-indent-less.png" );
    hash.insert( "gtk-yes", "dialog-ok.png" );
    hash.insert( "gtk-zoom-100", "zoom-original.png" );
    hash.insert( "gtk-zoom-fit", "zoom-fit-best.png" );
    hash.insert( "gtk-zoom-in", "zoom-in.png" );
    hash.insert( "gtk-zoom-out", "zoom-out.png" );
    return hash;
}

static QHash<QString,QString> ggmIconMap = ggmMapIcons();

bool 
BE::GMenu::globalX11EventFilter( void *msg )
{
    XEvent *ev = static_cast<XEvent*>(msg);
    if (instance && ev && ev->type == PropertyNotify) 
    {
        if (ev->xproperty.atom == ggmContext)
            instance->ggmUpdate( ev->xproperty.window );
//         else if (ev->xproperty.atom == mEvtAtom)
//             qDebug() << "Evt:" << QString::number(ev->xproperty.window) << XGetAtomName(QX11Info::display(),ev->xproperty.atom);
    }
    return formerX11EventFilter && (formerX11EventFilter)( msg );
}

#define MENU_FUNC(_FUNC_) menu ? menu->_FUNC_ : menubar->_FUNC_

static inline QString accelMappedLabel( const QDomElement &node )
{
    // escape "&", replace "_" accels by "&"
    return node.attribute("label").replace('&', "&&").replace(QRegExp("_([^_])"), "&\\1");
}

void
ggmRecursive(const QDomElement &node, QWidget *widget, const QString &prefix )
{
    if ( node.isNull() )
        return;
    
    QMenuBar *menubar = 0;
    QMenu *menu = qobject_cast<QMenu*>(widget);
    if (!menu)
    {
        menubar = qobject_cast<QMenuBar*>(widget);
        if (!menubar)
            return;
    }

    QDomElement e = node.firstChildElement("item");
    while ( !e.isNull() )
    {
        if (e.attribute("visible") != "0" )
        {
            if (e.attribute("type") == "s")
                MENU_FUNC( addSeparator() );
            else
            {
                QDomElement menuNode = e.firstChildElement("menu");
                if ( !menuNode.isNull() )
                {   // submenu
                    QMenu *newMenu = MENU_FUNC( addMenu( accelMappedLabel(e) ) );
                    ggmRecursive(menuNode, newMenu, prefix + "/" + e.attribute("id") );
                }
                else if ( !e.attribute("label").isEmpty() )
                {   // real action item
                    QAction *action = new QAction(widget);
                    action->setText( accelMappedLabel(e) );
                    action->setData( prefix + "/" + e.attribute("id") );
                    action->setEnabled( e.attribute("sensible") != "0" );
                    QString icon = e.attribute("icon");
                    if ( !icon.isEmpty() ) 
                        icon = ggmIconMap.value( icon, QString() );
                    if ( !icon.isEmpty() ) 
                        action->setIcon( KIcon( icon ) );
                    QString state = e.attribute("state");
                    int iState = 0;
                    if ( state == "0" )
                        iState = 1;
                    else if ( state == "1" )
                        iState = 2;
                    action->setCheckable( iState > 0 );
                    action->setChecked( (iState == 2) );
                    QObject::connect ( action, SIGNAL(triggered()), instance, SLOT(runGgmAction()) );
                    MENU_FUNC( addAction(action) );
                }
            }
        }
        e = e.nextSiblingElement("item");
    }
}

QWidget *
BE::GMenu::ggmCreate( WId id )
{
    QWidget *bar = 0;
    int nItems;
    char **list;
    XTextProperty string;
    
    if ( XGetTextProperty(QX11Info::display(), id, &string, ggmContext) && XTextPropertyToStringList(&string, &list, &nItems) )
    {
        if (nItems)
        {
            QString xml = QString::fromUtf8( list[0] );
            if (myOrientation == Qt::Horizontal)
                bar = new QMenuBar(this);
            else
                bar = new QMenu(this);
            QDomDocument doc;
            doc.setContent( xml, false );
            QDomElement root = doc.firstChildElement();
            ggmRecursive( root, bar, QString::number(id) );
            if ( !bar->actions().isEmpty() )
            {
                QAction *action = bar->actions().at(0);
                QFont fnt = action->font();
                fnt.setBold(true);
                action->setFont(fnt);
                KWindowInfo info(id, 0, NET::WM2WindowClass);
                action->setText( info.windowClassClass() );
            }
            layout()->addWidget(bar);
            bar->hide();
        }
        XFreeStringList(list);
    }
    return bar;
}

void 
BE::GMenu::ggmWindowActivated( WId id )
{
    while ( id && !ggmMenus.contains( id ) )
    {
        if ( KWindowInfo( id, NET::WMState ).state() & NET::Modal )
            id = 0;
        else
            id = KWindowSystem::transientFor(id);
    }
    
    if ( id && ggmMenus.contains( id ) )
    {
        ggmLastId = id;
        requestFocus( id );
    }
    else if ( ggmLastId )
    {
        releaseFocus( ggmLastId );
        ggmLastId = 0;
    }
}

void 
BE::GMenu::ggmUpdate( WId id )
{
    bool added = false, wasVisible = false;

    MenuMap::iterator it = myMenus.find( id );
    if ( it == myMenus.end() )
    {
        if ( QWidget *bar = ggmCreate(id) )
            myMenus.insert( id, bar );
        else
            return; // there's no menu for us
    }
    else 
    {
        wasVisible = it.value()->isVisible();
        it.value()->setEnabled(false); // invalidate
    }

    if ( (added = !ggmMenus.contains( id )) )
        ggmMenus.append( id );
    if ( wasVisible || (added && KWindowSystem::activeWindow() == id) )
        requestFocus( id );
}

static const unsigned long supported_types = NET::NormalMask | NET::DialogMask | NET::OverrideMask | NET::UtilityMask;

void 
BE::GMenu::ggmWindowAdded( WId id )
{
    KWindowInfo info( id, NET::WMWindowType );
    NET::WindowType type = info.windowType( supported_types );
    if ( type == NET::Unknown ) // everything that's not a supported_type
        return;

    foreach ( QWidget *w, QApplication::topLevelWidgets() ) {
        if ( w->testAttribute(Qt::WA_WState_Created) && w->internalWinId() && w->winId() == id )
            return;
    }

    XSelectInput( QX11Info::display(), id, PropertyChangeMask );
    qApp->syncX();
    ggmUpdate( id );
}

void 
BE::GMenu::ggmWindowRemoved( WId id )
{
    int idx = ggmMenus.indexOf( id );
    if ( idx > -1 )
    {
        releaseFocus( id );
        delete myMenus.take( id );
        ggmMenus.removeAt( idx );
    }
}

void
BE::GMenu::runGgmAction()
{
    if ( QAction *act = qobject_cast<QAction*>(sender()) )
    {
        QString string = act->data().toString();
        int slash = string.indexOf('/');
        WId id = string.left(slash).toULongLong();
        string = string.mid(slash);
//         qDebug() << id << string;
        char *data = string.toUtf8().append("\0").data();
        XTextProperty text;
        XStringListToTextProperty( &data, 1, &text );
        XSetTextProperty( QX11Info::display(), id, &text, ggmEvent );
    }
}
