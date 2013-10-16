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

#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>
#include <KDE/KConfigGroup>
#include <KDE/KCrash>
#include <KDE/KLocale>
// #include <KDE/KMenu>
#include <KDE/KMessageBox>
#include <KDE/KProcess>
#include <KDE/KRun>
#include <KDE/KSharedConfig>
#include <KDE/KStandardDirs>
#include <KDE/KUniqueApplication>
#include <KDE/KUriFilterData>
#include <KDE/KWindowInfo>
#include <KDE/KWindowSystem>
#include <KDE/NETWinInfo>

#include <QDesktopWidget>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileSystemWatcher>
#include <QImage>
#include <QMenuBar>
#include <QMouseEvent>
#include <QtDBus>
#include <QTimer>
#include <QStyleOptionProgressBarV2>
#include <QWidgetAction>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "battery.h"
#include "button.h"
#include "claws.h"
#include "clock.h"
#include "desktop.h"
#include "globalmenu.h"
#include "infocenter.h"
#include "label.h"
#include "mediatray.h"
#include "meter.h"
#include "pager.h"
#include "panel.h"
#include "runner.h"
#include "session.h"
#include "systray.h"
#include "tasks.h"
#include "trash.h"
#include "volume.h"

#include "dbus_shell.h"

#ifdef Q_WS_X11
static Atom net_wm_cm;
#endif

static BE::Shell *instance = 0;

BE::Shell::Shell(QObject *parent) : QObject(parent), myStyleWatcher(0L)
{
    if (instance)
    {
        qWarning("One shell at a time!");
        deleteLater();
        return;
    }

#ifdef Q_WS_X11
    Display *dpy = QX11Info::display();
    char string[ 100 ];
    sprintf(string, "_NET_WM_CM_S%d", DefaultScreen( dpy ));
    net_wm_cm = XInternAtom(dpy, string, False);
#endif

    instance = this;
    myContextMenu = 0;
    myContextWindow = 0;
    BE::Plugged::setShell(this);
    BE::Plugged::configMenu()->addSeparator()->setText(i18n("Shell"));
    myThemesMenu = BE::Plugged::configMenu()->addMenu(i18n("Themes"));
    myScreenMenu = BE::Plugged::configMenu()->addMenu(i18n("Screen"));
    connect (BE::Plugged::configMenu(), SIGNAL(aboutToShow()), SLOT(populateScreenMenu()));
    connect (myScreenMenu, SIGNAL(aboutToShow()), SLOT(populateScreenMenu()));
    updateThemeList();

    QFileSystemWatcher *themeWatch = new QFileSystemWatcher(this);
//     themeWatch->addDir(KGlobal::dirs()->locate("data","be.shell/Themes/"));
    themeWatch->addPath(KGlobal::dirs()->locateLocal("data","be.shell/Themes/"));
    connect( themeWatch, SIGNAL(directoryChanged(const QString &)), this, SLOT(updateThemeList()) );

    QMenu *configMenu = BE::Plugged::configMenu()->addMenu(i18n("Config"));
    connect(configMenu->addAction(i18n("Edit...")), SIGNAL(triggered()), this, SLOT(editConfig()));
    connect(configMenu->addAction(i18n("Reload")), SIGNAL(triggered()), this, SLOT(configure()));

    myDesk = new BE::Desk;
    connect (myDesk, SIGNAL(resized()), SIGNAL(desktopResized()));
    myDesk->myName = "BE::Desk";
    KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(myDesk->name());
    myDesk->configure(&grp);
    myPlugs.append(myDesk);

    BE::Plugged::configMenu()->addSeparator()->setText(i18n("Plugins"));

    myWindowList = new QMenu(i18n("Windows"), myDesk);
//     myWindowList->setSeparatorsCollapsible(false);
    connect ( myWindowList, SIGNAL(aboutToShow()), this, SLOT(populateWindowList()) );

    configure();

    myDesk->show();

    new BE::ShellAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/Shell", this);

    new BE::ScreenLockerAdaptor(this);
    QDBusConnection::sessionBus().registerService("org.freedesktop.ScreenSaver");
    QDBusConnection::sessionBus().registerService("org.kde.screensaver");
    QDBusConnection::sessionBus().registerObject("/ScreenSaver", this);

    QMetaObject::invokeMethod(this, "launchRunner");
}

BE::Shell::~Shell()
{
    if (instance == this)
        instance = NULL;
}

BE::Plugged*
BE::Shell::addApplet(const QString &name, Panel *panel)
{
    QString type;
    KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(name);
    if (grp.exists())
        type = grp.readEntry("Type", QString());
    if (type.isEmpty())
        type = name;

    BE::Plugged *p = 0;
    if (!type.compare("GlobalMenu", Qt::CaseInsensitive))
        p = new BE::GMenu(panel);
    else if (!type.compare("MediaTray", Qt::CaseInsensitive))
        p = new BE::MediaTray(panel);
    else if (!type.compare("Panel", Qt::CaseInsensitive))
        p = new BE::Panel(panel);
    else if (!type.compare("SessionButton", Qt::CaseInsensitive))
        p = new BE::Session(panel);
    else if (!type.compare("SysTray", Qt::CaseInsensitive))
        p = new BE::SysTray(panel);
    else if (!type.compare("Clock", Qt::CaseInsensitive))
        p = new BE::Clock(panel);
    else if (!type.compare("Button", Qt::CaseInsensitive))
        p = new BE::Button(panel, name);
    else if (!type.compare("Claws", Qt::CaseInsensitive))
        p = new BE::Claws(panel);
    else if (!type.compare("CpuMeter", Qt::CaseInsensitive))
        p = new BE::CpuMeter(panel);
    else if (!type.compare("RamMeter", Qt::CaseInsensitive))
        p = new BE::RamMeter(panel);
    else if (!type.compare("NetMeter", Qt::CaseInsensitive))
        p = new BE::NetMeter(panel);
    else if (!type.compare("HddMeter", Qt::CaseInsensitive))
        p = new BE::HddMeter(panel);
    else if (!type.compare("TimeMeter", Qt::CaseInsensitive))
        p = new BE::TimeMeter(panel);
    else if (!type.compare("Battery", Qt::CaseInsensitive))
        p = new BE::Battery(panel);
    else if (!type.compare("Label", Qt::CaseInsensitive))
        p = new BE::Label(panel);
    else if (!type.compare("TaskBar", Qt::CaseInsensitive))
        p = new BE::Tasks(panel);
    else if (!type.compare("Pager", Qt::CaseInsensitive))
        p = new BE::Pager(panel);
    else if (!type.compare("InfoCenter", Qt::CaseInsensitive))
        p = new BE::InfoCenter(panel);
    else if (!type.compare("Volume", Qt::CaseInsensitive))
        p = new BE::Volume(panel);
    else if (!type.compare("stretch", Qt::CaseInsensitive))
        p = panel->addStretch(2);
    else if (!type.compare("Trash", Qt::CaseInsensitive))
        p = new BE::Trash(panel);

    if (p)
    {
        p->myName = name;
        p->configure();
    }
    return p;
}

/*
// Exponential blur, Jani Huhtanen, 2006 ==========================
*  expblur(QImage &img, int radius)
*
*  In-place blur of image 'img' with kernel of approximate radius 'radius'.
*  Blurs with two sided exponential impulse response.
*
*  aprec = precision of alpha parameter in fixed-point format 0.aprec
*  zprec = precision of state parameters zR,zG,zB and zA in fp format 8.zprec
*/
#include <cmath>
template<int aprec, int zprec>
static inline void blurinner(unsigned char *bptr, int &zR, int &zG, int &zB, int &zA, int alpha)
{
    int R,G,B,A;
    R = *bptr;
    G = *(bptr+1);
    B = *(bptr+2);
    A = *(bptr+3);

    zR += (alpha * ((R<<zprec)-zR))>>aprec;
    zG += (alpha * ((G<<zprec)-zG))>>aprec;
    zB += (alpha * ((B<<zprec)-zB))>>aprec;
    zA += (alpha * ((A<<zprec)-zA))>>aprec;

    *bptr =     zR>>zprec;
    *(bptr+1) = zG>>zprec;
    *(bptr+2) = zB>>zprec;
    *(bptr+3) = zA>>zprec;
}

template<int aprec,int zprec>
static inline void blurrow( QImage & im, int line, int alpha)
{
    int zR,zG,zB,zA;

    QRgb *ptr = (QRgb *)im.scanLine(line);

    zR = *((unsigned char *)ptr    )<<zprec;
    zG = *((unsigned char *)ptr + 1)<<zprec;
    zB = *((unsigned char *)ptr + 2)<<zprec;
    zA = *((unsigned char *)ptr + 3)<<zprec;

    for(int index=1; index<im.width(); index++)
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);

    for(int index=im.width()-2; index>=0; index--)
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);
}

template<int aprec, int zprec>
static inline void blurcol( QImage & im, int col, int alpha)
{
    int zR,zG,zB,zA;

    QRgb *ptr = (QRgb *)im.bits();
    ptr+=col;

    zR = *((unsigned char *)ptr    )<<zprec;
    zG = *((unsigned char *)ptr + 1)<<zprec;
    zB = *((unsigned char *)ptr + 2)<<zprec;
    zA = *((unsigned char *)ptr + 3)<<zprec;

    for(int index=im.width(); index<(im.height()-1)*im.width(); index+=im.width())
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);

    for(int index=(im.height()-2)*im.width(); index>=0; index-=im.width())
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);
}

void
BE::Shell::blur( QImage &img, int radius )
{
    if(radius<1)
        return;

    static const int aprec = 16; static const int zprec = 7;

    // Calculate the alpha such that 90% of the kernel is within the radius. (Kernel extends to infinity)
    int alpha = (int)((1<<aprec)*(1.0f-expf(-2.3f/(radius+1.f))));

    for(int row=0;row<img.height();row++)
        blurrow<aprec,zprec>(img,row,alpha);

    for(int col=0;col<img.width();col++)
        blurcol<aprec,zprec>(img,col,alpha);
}
// ======================================================


#define LABEL_ERROR "missing \"label\" attribute"
#define MENU_FUNC(_FUNC_) menu ? menu->_FUNC_ : menubar->_FUNC_

void
BE::Shell::rBuildMenu(const QDomElement &node, QWidget *widget)
{
    QMenuBar *menubar = 0;
    QMenu *menu = qobject_cast<QMenu*>(widget);
    if (!menu)
    {
        menubar = qobject_cast<QMenuBar*>(widget);
        if (!menubar)
            return;
    }

    QDomNode kid = node.firstChild();
    while(!kid.isNull())
    {
        QDomElement e = kid.toElement(); // try to convert the node to an element.
        if(!e.isNull())
        {
            if (e.tagName() == "menu")
            {
                QString type = e.attribute("menu");
                if ( type == "windowlist")
                    MENU_FUNC(addMenu(BE::Shell::windowList()));
                else if (type == "BE::Config")
                {
                    QAction *act = MENU_FUNC(addMenu(BE::Plugged::configMenu()));
                    QFont fnt = act->font();
                    fnt.setBold(true);
                    act->setFont(fnt);
                } else {
                    QString preExec;
                    if (!type.isEmpty()) {
                        preExec = e.attribute("preExec");
                        if (preExec.isEmpty())
                            BE::Shell::buildMenu(type, widget, "submenu");
                    }
                    if (type.isEmpty() || !preExec.isEmpty()) {
                        QMenu *newMenu = MENU_FUNC(addMenu(e.attribute("label", LABEL_ERROR)));
                        QString icn = e.attribute("icon");
                        if (!icn.isEmpty())
                            newMenu->setIcon(BE::Plugged::themeIcon(icn));
                        if (preExec.isEmpty()) {
                            rBuildMenu(e, newMenu);
                        } else {
                            newMenu->setProperty("PreExec", preExec);
                            newMenu->setProperty("SubMenu", type);
                            uint preExecTimeout = e.attribute("preExecTimeout").toUInt();
                            if (!preExecTimeout)
                                preExecTimeout = 250;
                            newMenu->setProperty("PreExecTimeout", preExecTimeout);
                            connect(newMenu, SIGNAL(aboutToShow()), SLOT(populateMenu()));
                        }
                    }
                }
            }
            else if (e.tagName() == "action")
            {
                QAction *action = new QAction(widget);
                QString cmd = e.attribute("dbus");
                if (!cmd.isEmpty())
                    connect ( action, SIGNAL(triggered()), SLOT(callFromAction()) );
                else
                {
                    cmd = e.attribute("exec");
                    if (cmd.isEmpty())
                    {
                        cmd = KGlobal::dirs()->locate("services", e.attribute("service") + ".desktop");
                        if (!cmd.isEmpty())
                        {
                            KService kservice(cmd);
                            action->setIcon(BE::Plugged::themeIcon(kservice.icon()));
                            action->setText(kservice.name());
                            cmd = kservice.desktopEntryName();
                        }
                    }
                    if (!cmd.isEmpty())
                        connect ( action, SIGNAL(triggered()), SLOT(runFromAction()) );
                    else
                        qWarning("MainMenu action without effect, add \"dbus\" or \"exec\" attribute!");
                }
                action->setData(cmd);
                if (action->text().isEmpty())
                    action->setText(e.attribute("label", LABEL_ERROR));
                QString icn = e.attribute("icon");
                if (!icn.isEmpty())
                    action->setIcon(BE::Plugged::themeIcon(icn));
                MENU_FUNC(addAction(action));
            }
            else if (e.tagName() == "separator")
                MENU_FUNC(addSeparator());
        }
        kid = kid.nextSibling();
    }
}

void
BE::Shell::getContentsMargins(QWidget *w, int *l, int *t, int *r, int *b)
{
    QStyleOptionProgressBarV2 sopbv2;
    sopbv2.initFrom(w);
    const QRect outer = w->rect();
    const QRect inner = w->style()->subElementRect(QStyle::SE_ProgressBarGroove, &sopbv2, w);
    *l = inner.left() - outer.left();
    *t = inner.top() - outer.top();
    *r = outer.right() - inner.right();
    *b = outer.bottom() - inner.bottom();
}

void
BE::Shell::buildMenu(const QString &name, QWidget *widget, const QString &/*type*/)
{
    if (!instance)
        return;

    QDomDocument menu(name);
    QFile file(KGlobal::dirs()->locate("data", "be.shell/" + name + ".xml"));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning("Menu definition for %s does not exist", name.toLocal8Bit().data());
        return;
    }

    QString error; int row, col;
    if (!menu.setContent(&file, &error, &row, &col))
    {
        qWarning("Failed to build menu %s on %d:%d", name.toLocal8Bit().data(), row, col);
        qWarning("%s\n", error.toLocal8Bit().data());
        file.close();
        return;
    }
    file.close();

    QDomElement element = menu.documentElement();
    if (!element.isNull() /*&& element.tagName() == type*/)
        instance->rBuildMenu(element, widget);
}

void
BE::Shell::callFromAction()
{
    if (QAction *action = qobject_cast<QAction*>(sender()))
        call(action->data().toString());
}

bool
BE::Shell::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::RightButton)
        if (QMenu *menu = qobject_cast<QMenu*>(o))
        if (menu->property("is_window_list").toBool())
        if (QAction *act = menu->actionAt( me->pos()))
        {
            showWindowContextMenu((WId)act->data().toUInt(), me->globalPos());
            return true;
        }
    }
    return false;
}

void
BE::Shell::runFromAction()
{
    if (QAction *action = qobject_cast<QAction*>(sender()))
        run(action->data().toString());
}

int
BE::Shell::shadowPadding(const QString &string)
{
    if (!instance)
        return 0;
    int ret = instance->myShadowPadding.value(string, 0xffffffff).toInt();
    if (ret == 0xffffffff)
        ret = instance->myShadowPadding.value("BE--Panel", 0x80808080).toInt();
    return ret;
}

int
BE::Shell::shadowRadius(const QString &string)
{
    if (!instance)
        return 0;
    int ret = instance->myShadowRadius.value(string, -1).toInt();
    if (ret < 0)
        ret = instance->myShadowRadius.value("BE--Panel", 0).toInt();
    return ret;
}

QPen
BE::Shell::shadowBorder(const QString &string)
{
    if (!instance)
        return QPen();
    QVariant var = instance->myShadowBorder.value(string);
    if (!var.isValid())
        var = instance->myShadowBorder.value("BE--Panel");
    QPen pen = var.value<QPen>();
    return pen;
}


void
BE::Shell::showBeMenu()
{
    BE::GMenu::showMainMenu();
}

#define ADD_WINDOW_ACTION(_STRING_, _ACTION_, _STATE_) \
act = instance->myContextMenu->addAction( _STRING_ ); \
act->setCheckable(_STATE_); \
act->setChecked((info.state() & _STATE_) == _STATE_); \
act->setData(_STATE_); \
act->setEnabled(!_ACTION_ || ((info.allowedActions() & _ACTION_) == _ACTION_))

/// internal implementation
/// a) because i need it and don't want a extra KWindowInfo on a NETWinInfo
/// b) because the KWindowInfo implementaion has issues - see https://git.reviewboard.kde.org/r/108308
bool isMinimized(unsigned long int state, NET::MappingState mappingState)
{
    // NETWM 1.2 compliant WM - uses NET::Hidden for minimized windows
    if(( state & NET::Hidden ) != 0 && ( state & NET::Shaded ) == 0 ) // shaded may have NET::Hidden too
        return true;
    // ICCCM
    if( mappingState != NET::Iconic )
        return false;

    // older WMs use WithdrawnState for other virtual desktops
    // and IconicState only for minimized
    return KWindowSystem::icccmCompliantMappingState() ? false : true;
}

char processState(int pid) {
    if (!pid)
        return 0;
    QFile linux_proc("/proc/" + QString::number(pid) + "/stat");
    if (!linux_proc.exists())
        return 0;
    if (!linux_proc.open(QFile::ReadOnly))
        return 0;
    QString stat = linux_proc.readAll(); // <pid> (<exec>) <state> many numbers
    linux_proc.close();
    int idx = stat.lastIndexOf(')') + 2;
    if (stat.count() <= idx)
        return 0;
    return stat.at(idx).toAscii();
}

void
BE::Shell::showWindowContextMenu(WId id, const QPoint &pos)
{
    if (!instance)
        return;
    if (!(instance->myContextWindow = id))
        return;

    if (!instance->myContextMenu) {
        instance->myContextMenu = new QMenu;
        connect (instance->myContextMenu, SIGNAL(triggered(QAction*)), instance, SLOT(contextWindowAction(QAction*)));
    }
    else
        instance->myContextMenu->clear();
    static const unsigned long props[2] = {NET::WMState|NET::XAWMState|NET::WMDesktop|NET::WMPid, NET::WM2AllowedActions};
    NETWinInfo info(QX11Info::display(), instance->myContextWindow, QX11Info::appRootWindow(), props, 2);
    QAction *act;

    if (const char pstate = processState(info.pid())) {
        act = instance->myContextMenu->addAction( pstate == 'T' ? i18n("{>} Continue process") : i18n("{=} Pause process") );
        act->setCheckable(false);
        act->setData(0xffffffff);
        act->setEnabled(true);
        instance->myContextMenu->addSeparator();
    }

    ADD_WINDOW_ACTION(i18n("Close"), NET::ActionClose, (NET::State)0);
    instance->myContextMenu->addSeparator();

    ADD_WINDOW_ACTION(i18n("On all desktops"), NET::ActionStick, NET::Sticky);
    act->setEnabled(true); //hack
    act->setChecked(info.desktop() == NET::OnAllDesktops);
    QMenu *desktopMenu = instance->myContextMenu->addMenu(i18n("To desktop"));
    connect (desktopMenu, SIGNAL(triggered(QAction*)), instance, SLOT(contextWindowToDesktop(QAction*)));
    act = desktopMenu->addAction(i18n("This desktop"));
    act->setCheckable(true);
    act->setChecked(info.desktop() == NET::OnAllDesktops || info.desktop() == KWindowSystem::currentDesktop());
    act->setData(KWindowSystem::currentDesktop());
    desktopMenu->addSeparator();
    const int n = KWindowSystem::numberOfDesktops() + 1;
    for (int i = 1; i < n; ++i)
    {
        act = desktopMenu->addAction(i18n("%1 desktop %1").arg(i));
        act->setCheckable(true);
        act->setChecked(info.desktop() == NET::OnAllDesktops || info.desktop() == i);
        act->setData(i);
    }
    instance->myContextMenu->addSeparator();
    ADD_WINDOW_ACTION(i18n("FullScreen"), NET::ActionFullScreen, NET::FullScreen);
    ADD_WINDOW_ACTION(i18n("Maximized"), NET::ActionMax, NET::Max);
    ADD_WINDOW_ACTION(i18n("Minimized"), NET::ActionMinimize, NET::Hidden);
    act->setChecked(isMinimized(info.state(), info.mappingState())); //hack
    ADD_WINDOW_ACTION(i18n("Shaded"), NET::ActionShade, NET::Shaded);
    instance->myContextMenu->addSeparator();
    ADD_WINDOW_ACTION(i18n("Above other windows"), (NET::Action)0, NET::KeepAbove);
    ADD_WINDOW_ACTION(i18n("Below other windows"), (NET::Action)0, NET::KeepBelow);
    instance->myContextMenu->addSeparator();
    instance->myContextMenu->exec(pos);

    //     NET::MaxVert
    //     NET::MaxHoriz
    //     NET::ActionMaxVert
    //     NET::ActionMaxHoriz
    //     NET::DemandsAttention
    // NET::ActionMove
    // NET::ActionResize
    //
    //     NET::ActionChangeDesktop
}

bool
BE::Shell::touchMode()
{
    return !instance || instance->iAmTouchy;
}

void
BE::Shell::editConfig()
{
    saveSettings();
    QString file = KGlobal::dirs()->locateLocal("config","be.shell");
    KRun::runUrl( file, "text/plain", 0 );
}

void
BE::Shell::editThemeSheet()
{
    // TODO: create and ensure local copy!
    QString file = KGlobal::dirs()->locateLocal("data","be.shell/Themes/" + myTheme + "/style.css");
    KRun::runUrl( file, "text/plain", 0 );
}

void
BE::Shell::launchRunner()
{
    // startkde sets XCURSOR_* to have it applied on ksmserver, but that breaks runtime theme changes for new processes
    // we fix it for the processes we launch
    unsetenv("XCURSOR_THEME");
    unsetenv("XCURSOR_SIZE");

    BE::Run *beRun = new BE::Run(this);
    beRun->myName = "BE::Run";
    KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(beRun->name());
    beRun->configure(&grp);
    myPlugs.append(beRun);
}

static KProcess *kscreenlocker = 0;
// static KProcess *kscreenlockerconfig = 0;
static bool usedCompositing = false;

static bool runProcess(KProcess **proc, const char *cmd, const char *arg, QObject *parent = 0, bool sync = false)
{
    if (!*proc)
        *proc = new KProcess(parent);

    if ((*proc)->state() != QProcess::NotRunning)
        return false; // don't try to lock twice. ever!

    QString path = KStandardDirs::findExe( cmd );

    if( path.isEmpty())
        return false;

    (*proc)->clearProgram();
    (*proc)->setProgram(path);
    *(*proc) << arg;
    if (sync) // i'm not really a fan of sync running, but otherwise we've no chance
        (*proc)->start(); // to prevent double calls or end event handling :-(
    else
        (*proc)->startDetached();
    (*proc)->waitForStarted();
    return true;
}

#define kwin QDBusInterface("org.kde.kwin", "/KWin", "org.kde.KWin", QDBusConnection::sessionBus())

bool
BE::Shell::compositingActive()
{
#ifdef Q_WS_X11
    return XGetSelectionOwner( QX11Info::display(), net_wm_cm ) != None;
#else
    return true;
#endif
}

void
BE::Shell::lockScreen()
{
    usedCompositing = compositingActive();
    if (runProcess(&kscreenlocker, "kscreenlocker", "--forcelock", this, true))
    {
        if (usedCompositing)
            kwin.call("toggleCompositing");
        connect (kscreenlocker, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(resetCompositing()));
        emit ActiveChanged();
    }
}

void
BE::Shell::resetCompositing()
{
    if (usedCompositing != compositingActive())
        kwin.call("toggleCompositing");
    emit ActiveChanged();
}

void
BE::Shell::configureScreenLocker()
{
    run("kcmshell4 screensaver");
//     runProcess(&kscreenlockerconfig, "kcmshell4", "screensaver", this);
}

void
BE::Shell::configure()
{
    KSharedConfig::Ptr shellConfig = KSharedConfig::openConfig("be.shell");
    KConfigGroup grp = shellConfig->group("BE::Shell");

    QString oldS = myTheme;
    myTheme = grp.readEntry("Theme", "default");
    const bool reloadStyle = myTheme != oldS;

    iAmTouchy = grp.readEntry("Touch", false);

    myDesk->Plugged::configure();

    QStringList panels = grp.readEntry("Panels", QStringList());

    // kick no more used and update formerly used
    QList<Plugged*>::iterator it = myPlugs.begin(), end = myPlugs.end();
    while (it != end)
    {
        BE::Panel *panel = dynamic_cast<BE::Panel*>(*it);
        if (!panel) {
            if (BE::Run *beRun = dynamic_cast<BE::Run*>(*it)) {
                KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(beRun->name());
                beRun->configure(&grp);
            }
            ++it;
            continue;
        }
        if (!panels.contains(panel->name())) {
            delete panel;
            it = myPlugs.erase(it);
            continue;
        }
        else
            panel->Plugged::configure();
        ++it;
    }

    // add new panels & fix stacking order
    foreach (const QString &panelName, panels)
    {
        if (!myPanels.contains(panelName))
        {
            BE::Panel *panel = new BE::Panel(myDesk);
            panel->myName = panelName;
            myPlugs.append(panel);
            myDesk->registrate(panel);
            panel->Plugged::configure();
            panel->raise();
        }
        else
        {
            it = myPlugs.begin(), end = myPlugs.end();
            while (it != end)
            {
                if (BE::Panel *panel = dynamic_cast<BE::Panel*>(*it))
                if (panel->name() == panelName)
                {
                    panel->raise();
                    break;
                }
                ++it;
            }
        }
    }

    myPanels = panels;

    if (reloadStyle)
        QMetaObject::invokeMethod(this, "setTheme", Qt::QueuedConnection, Q_ARG(QString, myTheme));
}

static bool _isContextDesktopChange = false;

void
BE::Shell::contextWindowAction(QAction *act)
{
    if (_isContextDesktopChange)
    {
        _isContextDesktopChange = false;
        return;
    }
    if (!myContextWindow)
        return;

    const NET::State singleState = (NET::State)act->data().toUInt();

    if (!singleState) { // "close"
        NETRootInfo(QX11Info::display(), NET::CloseWindow).closeWindowRequest(myContextWindow);
        return;
    }

    if (singleState == (NET::State)0xffffffff) { // toggle pause
        NETWinInfo info(QX11Info::display(), myContextWindow, QX11Info::appRootWindow(), NET::WMPid);
        const char pstate = processState(info.pid());
        if (!pstate) // error
            return;
        if (pstate == 'T') // paused
            ::kill(info.pid(), SIGCONT);
        else {
            ::kill(info.pid(), SIGSTOP);
            KConfig cfg;
            KConfigGroup cg(&cfg, "Notification Messages");  // Depends on KMessageBox
            if (!cg.readEntry("SIGSTOP_warning", true))
                return;
            KMessageBox::information(0, i18n("<h1>WARNING</h1><h2>Read this first!</h2><h1>Seriously!</h1>"
                                        "<h2>SIGSTOP is not for noobs!</h2>"
                                        "You just stopped a process, what will freeze it.<br>"
                                        "The process can be re-activtated by sending it SIGCONT "
                                        "(contact the kill manpage)<br>"
                                        "What you <b>must understand</b> is that neither the Windowmanager, "
                                        "nor the display server (X11) are stopped.<br>"
                                        "<h3>This has serious implications</h3>"
                                        "All - and I mean <b>all</b> input events are still delivered "
                                        "to the process and will be executed immediately when the "
                                        "process continues!<br>"
                                        "<h3>Example:</h3>"
                                        "If you paused konsole and then happily typed<br>"
                                        "<pre>rm -rf ~/* ~/.*</pre><br>"
                                        "into it, there will be no immediate output or reaction.<br>"
                                        "<b>BUT:</b> The moment the process continues, konsole will "
                                        "<b>delete all your personal data</b> by that command."
                                        "<h2>Sum up</h2>"
                                        "Do not do \"funny\" things with a process because it seems inreactive.<br>"
                                        "Best minimize the window and do not touch it before you continued "
                                        "to play it."
                                        "<h1>You have been warned!</h1>"),
                                        i18n("SIGSTOP is not for fools!"), "SIGSTOP_warning",
                                        KMessageBox::Dangerous);
        }
        return;
    }

    const KWindowInfo info(myContextWindow, NET::WMState|NET::WMDesktop);
    const unsigned long state = info.state();
    if (singleState == NET::Hidden) // minimize
    {
        if (isMinimized(info.state(), info.mappingState()))
            KWindowSystem::unminimizeWindow(myContextWindow);
        else
            KWindowSystem::minimizeWindow(myContextWindow);
    }
    if (singleState == NET::Sticky) // state settign fails
        KWindowSystem::setOnAllDesktops(myContextWindow, !info.onAllDesktops());
    else if (state & singleState)
        KWindowSystem::clearState(myContextWindow, singleState);
    else
        KWindowSystem::setState(myContextWindow, singleState);
}

void
BE::Shell::contextWindowToDesktop(QAction *act)
{
    if (!myContextWindow)
        return;
    _isContextDesktopChange = true;
    KWindowSystem::setOnDesktop(myContextWindow, act->data().toInt());
}

static QWidget *currentThemeChangeHandler = 0;

bool
BE::Shell::callThemeChangeEventFor(BE::Plugged *p)
{
    if ( !currentThemeChangeHandler )
        return false;

    QWidget *w = dynamic_cast<QWidget*>(p);
    if ( currentThemeChangeHandler != p->parent() )
    {
        if ( !w || currentThemeChangeHandler != w->parent() )
            return false;
    }

    QWidget *formerHandler = currentThemeChangeHandler;
    currentThemeChangeHandler = w;
    p->themeChanged();
    currentThemeChangeHandler = formerHandler;

    return true;
}

QRect
BE::Shell::desktopGeometry(int screen)
{
    if (!instance)
        return QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen());
    else if (instance->myDesk->screen() < 0 && screen > -1)
        return QApplication::desktop()->screenGeometry(screen);
    else
        return instance->myDesk->geometry();
}

void
BE::Shell::configure( Plugged *plug )
{
    if (plug)
    {
        KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(plug->name().isEmpty() ? "Plugin" : plug->name());
        plug->configure(&grp);
    }
}


void
BE::Shell::call(const QString &instruction)
{
    QStringList list = instruction.split(';');
    if (list.count() < 5)
    {
        qWarning("invalid dbus chain, must be: \"bus;service;path;interface;method[;arg1;arg2;...]\", bus is \"session\" or \"system\"");
        return;
    }

    QDBusInterface *caller = 0;
    if (list.at(0) == "session")
        caller = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::sessionBus() );
    else if (list.at(0) == "system")
        caller = new QDBusInterface( list.at(1), list.at(2), list.at(3), QDBusConnection::systemBus() );
    else
    {
        qWarning("unknown bus, must be \"session\" or \"system\"");
        return;
    }

    QList<QVariant> args;
    if (list.count() > 5)
    {
        for (int i = 5; i < list.count(); ++i)
        {
            bool ok = false;
            short Short = list.at(i).toShort(&ok);
            if (ok) { args << Short; continue; }
            unsigned short UShort = list.at(i).toUShort(&ok);
            if (ok) { args << UShort; continue; }
            int Int = list.at(i).toInt(&ok);
            if (ok) { args << Int; continue; }
            uint UInt = list.at(i).toUInt(&ok);
            if (ok) { args << UInt; continue; }
            double Double = list.at(i).toDouble(&ok);
            if (ok) { args << Double; continue; }

            args << list.at(i);
        }
    }
    caller->asyncCallWithArgumentList(list.at(4), args);
    delete caller;
}


QString
BE::Shell::executable(WId id)
{
    NETWinInfo info(QX11Info::display(), id, QX11Info::appRootWindow(), NET::WMPid);
    const int pid = info.pid();
    if (!pid)
        return QString();

    // read /proc/${PID}/cmdline
    QFile linux_proc("/proc/" + QString::number(pid) + "/cmdline");
    if (!linux_proc.exists())
        return QString();
    if (!linux_proc.open(QFile::ReadOnly))
        return QString();
    QString exe = linux_proc.readAll();
    linux_proc.close();

    // find executable (w/o path etc.)
//     qDebug() << exe;
    if (!exe.isEmpty())
    {
        if (exe.startsWith("kdeinit4:"))
            exe = exe.mid(9);
        exe = exe.trimmed();
        exe = exe.section('/', -1);
        exe = exe.section(' ', 0, 0);
    }
//     qDebug() << "-->" << exe;
    return exe;
}

bool
BE::Shell::hasFullscreenAction()
{
    if (WId id = KWindowSystem::activeWindow())
    {
        KWindowInfo info(id, NET::WMState);
        return info.hasState(NET::FullScreen);
    }
    return false;
}

bool
BE::Shell::name(BE::Plugged *p, const QString &string)
{
    if (!p->myName.isEmpty())
        return false;
    p->myName = string;
    return true;
}


void
BE::Shell::populateMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    if (!menu)
        return;
    const QString preExec = menu->property("PreExec").toString().replace("$HOME", QDir::home().path());
    if (preExec.isEmpty())
        return;
    const uint preExecTimeout = menu->property("PreExecTimeout").toUInt();
    QProcess proc(this);
    proc.start(preExec);
    proc.waitForFinished(preExecTimeout);
    menu->clear();
    BE::Shell::buildMenu(menu->property("SubMenu").toString(), menu, "submenu");
}

void
BE::Shell::populateScreenMenu()
{
    if (!sender())
        return;
    if (sender() == BE::Plugged::configMenu())
    {
        myScreenMenu->menuAction()->setVisible( QApplication::desktop()->screenCount() > 1 );
        return;
    }
    myScreenMenu->clear();
    bool ok;
    int currentScreen = myScreenMenu->property("CurrentScreen").toInt(&ok);
    if (!ok)
        currentScreen = -2; // no match
    for (int i = -1; i < QApplication::desktop()->screenCount(); ++i)
    {
        QAction *act = myScreenMenu->addAction(i < 0 ? i18n("All screens") : i18n("Screen %1").arg(i+1));
        act->setData(i);
        act->setCheckable(true);
        act->setChecked(currentScreen == i);
    }
}

/*
Window list, sorted by desktops.
*** WARNING: KWindowSystem counts desktops from "1", "0" is the current desktop
*/
void
BE::Shell::populateWindowList(const QList<WId> &windows, QMenu *popup, bool allDesktops)
{
    popup->clear();
    popup->setProperty("is_window_list", true);
    if (instance)
        popup->installEventFilter(instance);

    KWindowInfo info;
    int currentDesktop = KWindowSystem::currentDesktop();
    int n = KWindowSystem::numberOfDesktops();
    QList<WId> *windowTable = new QList<WId>[n];

    if ( QApplication::keyboardModifiers() & Qt::ShiftModifier )
    {
        foreach (WId id, windows)
        {
            info = KWindowInfo(id, NET::WMDesktop, 0);
            if (int d = info.desktop())
            {
                if (info.onAllDesktops())
                    d = currentDesktop;
                windowTable[d-1].append(id);
            }
        }
    }
    else
    {
        static const unsigned long
        supported_types = NET::Unknown | NET::NormalMask | NET::DialogMask | NET::OverrideMask | NET::UtilityMask;
        foreach (WId id, windows)
        {
            foreach ( QWidget *w, QApplication::topLevelWidgets() )
            {
                if ( w->testAttribute(Qt::WA_WState_Created) && w->internalWinId() && w->winId() == id )
                    goto nextWindow;
            }

            info = KWindowInfo(id, NET::WMDesktop | NET::WMWindowType, 0);
            if ( (1<<info.windowType( NET::AllTypesMask )) & supported_types )
            {
                if (int d = info.desktop())
                {
                    if (info.onAllDesktops())
                        d = currentDesktop;
                    windowTable[d-1].append(id);
                }
            }
nextWindow: continue;
        }
    }

    QAction *act(0);
    int d;
    bool needSep = false;
    for (int i = 0; i < n; ++i)
    {
        if (!allDesktops && windowTable[i].isEmpty())
            continue;
        d = i+1;

        if (needSep)
            popup->addSeparator();
        if ( d == KWindowSystem::currentDesktop() )
        {
            //             myWindowList->addSeparator()->setText(KWindowSystem::desktopName(d));
            // because QMenu cannot handle text separators right, patch submitted
            QLabel *dummy = new QLabel(KWindowSystem::desktopName(d), popup);
            QWidgetAction *dummyAction = new QWidgetAction( popup );
            dummyAction->setDefaultWidget( dummy );
            popup->addAction( dummyAction );
            dummy->setAlignment( Qt::AlignCenter );
            QPalette pal = popup->palette();
            pal.setColor( QPalette::WindowText, pal.color(popup->foregroundRole()) );
            dummy->setPalette(pal);
            QFont fnt = popup->font();
            fnt.setPointSize( fnt.pointSize() * 1.4 );
            fnt.setBold(true);
            dummy->setFont(fnt);
            //----------------------
        }
        else
        {
            act = popup->addAction( KWindowSystem::desktopName(d), instance, SLOT(setCurrentDesktop()) );
            act->setData(d);
            if (i < n-1)
                popup->addSeparator();
        }

        needSep = false;
        foreach (WId id, windowTable[i])
        {
            info = KWindowInfo(id, NET::WMVisibleIconName | NET::WMState | NET::XAWMState, 0);
            {
                if (info.hasState(NET::SkipTaskbar))
                    continue;
                QString title = info.visibleIconName();
                if (isMinimized(info.state(), info.mappingState()))
                    title = "( " + title + " )";
                title = "    " + title;
                if (title.length() > 52)
                    title = title.left(22) + "..." + title.right(22);
                act = popup->addAction( KWindowSystem::icon(id, 32, 32, false ), title, instance, SLOT(setActiveWindow()) );
                act->setData((uint)id);
                act->setDisabled(id == KWindowSystem::activeWindow());
                needSep = true;
            }
        }
    }
    delete[] windowTable;
}

void
BE::Shell::populateWindowList()
{
    BE::Shell::populateWindowList( KWindowSystem::windows(), myWindowList, true);
}

void
BE::Shell::run(const QString &command)
{
    KUriFilterData execLineData( command );
    KUriFilter::self()->filterUri( execLineData, QStringList() << "kurisearchfilter" << "kshorturifilter" );
    QString cmd = ( execLineData.uri().isLocalFile() ? execLineData.uri().path() : execLineData.uri().url() );

    if ( cmd.isEmpty() )
        return;

    switch( execLineData.uriType() )
    {
        case KUriFilterData::LocalFile:
        case KUriFilterData::LocalDir:
        case KUriFilterData::NetProtocol:
        case KUriFilterData::Help:
        {
            new KRun( execLineData.uri(), 0 );
            break;
        }
        case KUriFilterData::Executable:
        case KUriFilterData::Shell:
        {
            QString args = cmd;
            if( execLineData.hasArgsAndOptions() )
                cmd += execLineData.argsAndOptions();
            KRun::runCommand( cmd, args, "", 0 );
            break;
        }
        case KUriFilterData::Unknown:
        case KUriFilterData::Error:
        default:
            break;
    }
}

void
BE::Shell::saveSettings( Plugged *plug )
{
    if (plug)
    {
        KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group(plug->name().isEmpty() ? "Plugin" : plug->name());
        plug->saveSettings(&grp);
    }
}

void
BE::Shell::saveSettings()
{
    KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group("BE::Shell");
    grp.writeEntry("Theme", myTheme);
}

int
BE::Shell::screen() { return instance ? instance->myDesk->screen() : -1; }

QMenu*
BE::Shell::screenMenu() { return instance ? instance->myScreenMenu : 0L; }

void
BE::Shell::setActiveWindow()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act) return;
    WId win = act->data().toUInt();
//     KWindowSystem::unminimizeWindow(win); // sometimes activation can fail, at least show.
//     KWindowSystem::activateWindow(win);
    KWindowSystem::forceActiveWindow(win);
//     KWindowSystem::raiseWindow(win);
}

void
BE::Shell::setCurrentDesktop()
{
    if ( QAction *act = qobject_cast<QAction*>(sender()) )
        KWindowSystem::setCurrentDesktop(act->data().toInt());
}

void
BE::Shell::setPanelVisible(const QString &name, char vis)
{
    for (QList<Plugged*>::iterator it = myPlugs.begin(), end = myPlugs.end(); it != end; ++it) {
        BE::Panel *panel = dynamic_cast<BE::Panel*>(*it);
        if (!panel)
            continue;
        if (panel->name() == name) {
            const bool v = vis < 0 ? !panel->isVisible() : bool(vis);
            if (v == panel->isVisible())
                break; // nothing to do
            if (panel->layer() > 1 || !panel->struts())
                panel->slide(v);
            else
                panel->setVisible(v);
            break;
        }
    }
}

void
BE::Shell::setTheme(QAction *action)
{
    if (action)
    {
        QString theme = action->data().toString();
        if (!theme.isEmpty())
            setTheme(theme);
    }
}

void
BE::Shell::setTheme(const QString &t)
{
    myTheme = t;
    delete myStyleWatcher; myStyleWatcher = 0;
    QString file = KGlobal::dirs()->locate("data","be.shell/Themes/" + myTheme + "/style.css");
    qApp->setStyleSheet( QString() );
    if( !file.isEmpty() )
        updateStyleSheet(file);

    foreach (Plugged *p, myPlugs)
    {
        if ((currentThemeChangeHandler = dynamic_cast<QWidget*>(p)))
            p->themeChanged();
    }
    currentThemeChangeHandler = 0L;

    saveSettings();
}

enum CssExtension { ShadowBorder = 0, ShadowRadius, ShadowPadding };

QVariant shadowBorder(const QString &string, bool *ok)
{
    QVariant value;
    QString s(string);
    if (s.contains("rgba(")) {
        int open = s.indexOf("rgba(");
        int close = s.indexOf(")", open);
        QString tmp = s.mid(open, close-open);
        tmp.remove(" ");
        s.replace(s.mid(open, close-open), tmp);
    }
    const QStringList &col(s.split(" ", QString::SkipEmptyParts));
    if (col.count() > 1) {
#define REQUIRE(_OPTS_) _OPTS_; if (!*ok) return QVariant()
        int penWidth = 0;
        QColor penColor;
        foreach (QString opt, col) {
            if (opt.endsWith("px")) {
                REQUIRE(penWidth = qRound(opt.left(opt.length()-2).toFloat(ok)));
            } else if (opt.startsWith("rgba(")) {
                int r, g, b, a;
                QStringList rgba(opt.mid(5, opt.length()-6 ).split(",", QString::SkipEmptyParts));
                if (rgba.count() == 4) {
                    REQUIRE(r = rgba.at(0).toInt(ok));
                    REQUIRE(g = rgba.at(1).toInt(ok));
                    REQUIRE(b = rgba.at(2).toInt(ok));
                    REQUIRE(a = rgba.at(3).toInt(ok));
                } else {
                    return QVariant();
                }
                penColor = QColor(r, g, b, a);
            } else {
                penColor.setAllowX11ColorNames(true);
                penColor.setNamedColor(opt);
                *ok = penColor.isValid();
                if (!*ok)
                    penWidth = opt.toInt(ok);
                // sth. that's neither an integer nor a color nor explicit px size nor rgba...
                if (!*ok)
                    return QVariant();
            }
        }
#undef REQUIRE
        if ( penColor.isValid() && penWidth > 0 )
            value.setValue(QPen(penColor, penWidth));
    }
    return value;
}

void parse(CssExtension ext, QString *sheet, QMap<QString,QVariant> *map) {
    QString property;
    switch (ext) {
    case ShadowBorder:
        property = "shadow-border"; break;
    case ShadowRadius:
        property = "shadow-radius"; break;
    case ShadowPadding:
        property = "shadow-padding"; break;
    default:
        return; // no unknown junk
    }

    static QRegExp nonDig("[^-0123456789\\s]");
    int pi = 0;
    map->clear();
    while ((pi = sheet->indexOf(property, pi)) > -1) {
        int open = sheet->lastIndexOf('{', pi);
        pi += property.length();
        if (open < 0) {
            qWarning("%s cannot be used outside a definition!", property.toLocal8Bit().data());
            continue;
        }
        int close = sheet->lastIndexOf('}', open) + 1;
        QStringList elements = sheet->mid(close, open-close).simplified().split(',', QString::SkipEmptyParts);
        open = sheet->indexOf(':', pi) + 1;
        close = sheet->indexOf(nonDig, open);

        QVariant value;
        bool ok = false;
        switch (ext) {
        case ShadowBorder:
            value = shadowBorder(sheet->mid(open, sheet->indexOf(';', open)-open).simplified(), &ok);
            break;
        case ShadowRadius:
            value = sheet->mid(open, close-open).toInt(&ok);
            break;
        case ShadowPadding: {
            QStringList s = sheet->mid(open, close-open).simplified().split(" ", QString::SkipEmptyParts);
            if (s.count() == 1) {
                value = ((s.at(0).toInt(&ok) + 128) & 0xff) | (0xffffff << 8);
            }
            else if (s.count() == 4) {
                int v = 0;
                for (int i = 0; i < 4; ++i) {
                    v |= ((s.at(i).toInt(&ok) + 128) & 0xff) << (8*i);
                    if (!ok)
                        break;
                }
                value = v;
            }
            break;
        }
        }

        close = sheet->indexOf(';', close);

        if (!ok)
            continue;
        foreach (const QString element, elements) {
            const QString e = element.trimmed();
            if (e.contains(' '))
                continue; // no nested selectors
            if (e.contains("BE--Panel"))
                map->insert("BE--Panel", value);
            else if (e.startsWith("#")) // only #id's supported beyond "BE--Panel"
                map->insert(e.mid(1), value);
        }
        // remove entry no not confuse the css parser
        sheet->replace(pi-property.length(), close-(pi-property.length()), ' ');
    }
}

void
BE::Shell::updateStyleSheet(const QString &filename)
{
    QFile file(filename);
    if ( file.open(QIODevice::ReadOnly) )
    {
        qApp->setStyleSheet( QString() );
        QString sheet = file.readAll().replace("${base}", filename.left(filename.length() - 10).toLocal8Bit());
        int cs = 0, ce;
        // get rid of comments, just override them
        while ((cs = sheet.indexOf("/*", cs)) > -1) {
            if ((ce = sheet.indexOf("*/", cs)) > cs)
                sheet.replace(cs, ce+2-cs, ' ');
            else
                qWarning("INVALID CSS - comment not closed!");
        }
        parse(ShadowRadius, &sheet, &myShadowRadius);
        parse(ShadowPadding, &sheet, &myShadowPadding);
        parse(ShadowBorder, &sheet, &myShadowBorder);

        qApp->setStyleSheet( sheet );
        emit styleSheetChanged();
    }
    delete myStyleWatcher;
    myStyleWatcher = new QFileSystemWatcher(this);
    myStyleWatcher->addPath(filename);
    connect( myStyleWatcher, SIGNAL(fileChanged(const QString &)), this, SLOT(updateStyleSheet(const QString &)) );
}

void
BE::Shell::updateThemeList()
{
    myThemesMenu->clear();
    QStringList themedirs;
    themedirs << KGlobal::dirs()->locate("data","be.shell/Themes/") << KGlobal::dirs()->locateLocal("data","be.shell/Themes/");
    QStringList themes;
    foreach(QString themedir, themedirs)
        themes << QDir(themedir).entryList(QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name);
    themedirs.clear();
    foreach (QString theme, themes)
    {
        if (!themedirs.contains(theme))
            { myThemesMenu->addAction(theme)->setData(theme); themedirs << theme; }
    }
    connect(myThemesMenu, SIGNAL(triggered(QAction*)), this, SLOT(setTheme(QAction*)));
    myThemesMenu->addSeparator();
    connect(myThemesMenu->addAction(i18n("Edit current...")), SIGNAL(triggered()), this, SLOT(editThemeSheet()));
}

QMenu*
BE::Shell::windowList() { return instance ? instance->myWindowList : 0L; }


#if 1
static volatile sig_atomic_t fatal_error_in_progress = 0;
static void handle_crash(int sig)
{
    qDebug() << "crash" << sig;
    if (fatal_error_in_progress)
        raise(sig);
    fatal_error_in_progress = 1;

    const int answer = KMessageBox::warningYesNo(0,
                                                 i18n("Sorry, BE::Shell just crashed<h2>Do you want to restart?</h2>"),
                                                 i18n("Sorry, BE::Shell just crashed"),
                                                 KGuiItem(i18n("Restart")),
                                                 KGuiItem(i18n("Quit")));
    if (answer == KMessageBox::Yes)
        BE::Shell::run("be.shell");
    KCrash::defaultCrashHandler(sig);

    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

int main (int argc, char *argv[])
{
    KAboutData aboutData( "be.shell", 0, ki18n("BE::shell"), "0.1", ki18n("A lightweight Desktop shell for KDE4"),
                          KAboutData::License_GPL, ki18n("(c) 2009"), ki18n("Some about text..."),
                          "http://www.kde.org", "thomas.luebking@web.de");

    QString home = QDir::home().path();
    QString path = "/etc/gtk-2.0/gtkrc:" + home + "/.gtkrc-2.0:" + home + "/.gtkrc-2.0-kde4:" + KGlobal::dirs()->localkdedir() + "/share/config/gtkrc-2.0";
    setenv("GTK2_RC_FILES", path.toLocal8Bit().data(), 0);
    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    options.add("restart", ki18n("Send all your private data to the empire!"));
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if (args->isSet("restart")) {
        QDBusInterface be_shell("org.kde.be.shell", "/MainApplication", "org.kde.KApplication", QDBusConnection::sessionBus());
        be_shell.call(QLatin1String("quit"));
        usleep(500000);
        BE::Shell::run("be.shell");
        return 0;
    }
    args->clear();

    KUniqueApplication app;
    app.disableSessionManagement();

    const QString startupID("workspace desktop");
    QDBusInterface ksmserver( "org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface", QDBusConnection::sessionBus() );
    ksmserver.call(QLatin1String("suspendStartup"), startupID);
    BE::Shell beShell;
    ksmserver.call(QLatin1String("resumeStartup"), startupID);

    KCrash::setCrashHandler(handle_crash);

    return app.exec();
}

